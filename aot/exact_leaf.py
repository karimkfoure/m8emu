"""Emit exact-entry paths for small straight-line register-only leaves.

Keep the ordinary continuation function. Use this path only after an exact
native entry lookup and inside the public wrapper's FP environment. The path
uses the existing instruction emitters, and therefore retains their FP and
integer flag rules. It has no memory access or guest call that can observe a
private CPU copy. Register accesses can instead use the complete caller CPU.
"""
from __future__ import annotations

from dataclasses import dataclass
import re

from capstone import arm as A

# These existing helpers use only xpsr/fpscr from their CPU argument. Unknown
# helpers are rejected instead of assuming that they have this footprint.
CPU_HELPERS = frozenset((
    'm8_nz', 'm8_logic_flags', 'm8_add_flags', 'm8_sub_flags',
    'm8_sat_signed', 'm8_sat_unsigned', 'm8_qresult', 'm8_parallel',
    'm8_fp_eval32', 'm8_fp_op32', 'm8_fp_cmp32', 'm8_fp_sync',
))
VALUE_HELPERS = frozenset((
    'm8_s32', 'm8_signed', 'm8_shift', 'm8_ror', 'm8_clz', 'm8_rbit',
    'm8_sel', 'm8_usad8',
))


@dataclass(frozen=True)
class Leaf:
    entry: int
    pcs: tuple[int, ...]
    bodies: tuple[tuple[str, ...], ...]
    registers: frozenset[tuple[str, int]]
    modified: frozenset[tuple[str, int]]

    @property
    def name(self):
        return f'entry_{self.entry:x}'

    def emit(self):
        """Return an ABI-compatible exact native callee, after fn_ENTRY.

        Non-entry PC, IT state and non-Thumb returns leave all state unchanged.
        Diagnostic and counted builds use the existing continuation function.
        The generated body intentionally shares the existing FP helpers. This
        changes state storage and dispatch, not the arithmetic contract.
        """
        rows = [f'static int {self.name}(m8_cpu *restrict cpu,m8_mem *restrict mem) {{',
                f'if(cpu->r[15]!=0x{self.entry:x}u || (cpu->xpsr & 0x0600fc00u) ||',
                '   !(cpu->xpsr & 0x01000000u) || !(cpu->r[14] & 1u) ||',
                '   (cpu->r[14] & 0xfffffff0u)==0xfffffff0u) return M8_AOT_EXIT;',
                '#if defined(M8_AOT_DIAGNOSTIC) || defined(M8_AOT_GUEST_COUNT)',
                f'return fn_{self.entry:x}(cpu,mem);', '#else', '(void)mem;']
        for body in self.bodies:
            rows += ['{', *body, '}']
        rows += ['return M8_AOT_EXIT;', '#endif', '}']
        return '\n'.join(rows) + '\n'


def analyze(function, translator, unit_bodies=None, max_instructions=16):
    """Return a Leaf or None. Follow one complete, unpredicated entry path.

    Reject calls, memory, IT blocks, indirect branches, PC writes and unknown
    helpers. A terminal BX LR is the only control operation. Limit the path
    length so this optional path does not duplicate large function bodies.
    """
    pc = function.entry
    pcs, bodies = [], []
    registers, modified = set(), set()
    while len(pcs) < max_instructions:
        unit = function.units.get(pc)
        if unit is None or unit.it or len(unit.instructions) != 1:
            return None
        insn = unit.instructions[0]
        if insn.cc not in (A.ARM_CC_AL, A.ARM_CC_INVALID):
            return None
        if insn.id == A.ARM_INS_BX:
            if len(insn.operands) != 1 or insn.operands[0].reg != A.ARM_REG_LR:
                return None
            pcs.append(pc)
            bodies.append(('cpu->r[15]=cpu->r[14] & ~1u;',))
            registers.update((('r', 14), ('r', 15)))
            modified.add(('r', 15))
            return Leaf(function.entry, tuple(pcs), tuple(bodies),
                        frozenset(registers), frozenset(modified))
        if any(op.type == A.ARM_OP_MEM for op in insn.operands):
            return None
        body = (translator.instruction(function, insn) if unit_bodies is None
                else unit_bodies[pc][0])
        if body is None:
            return None
        source = '\n'.join(body)
        if re.search(r'\b(goto|return|mem|slow|pc|tx)\b|cpu->r\[15\]', source):
            return None
        # The pre-effect return guard qualifies the incoming LR. A leaf that
        # rewrites LR could introduce a later return fault. SP writes also
        # require architectural alignment handling beyond this narrow path.
        if re.search(r'cpu->r\[(?:13|14)\]\s*(?:=|\+=|-=|\|=|&=|\^=)', source):
            return None
        helpers = set(re.findall(r'\b(m8_\w+)\s*\(', source))
        if helpers - CPU_HELPERS - VALUE_HELPERS:
            return None
        # Do not permit a new helper to expose other CPU fields implicitly.
        cpu_helpers = set(re.findall(r'\b(m8_\w+)\s*\(cpu\s*(?:,|\))', source))
        if cpu_helpers - CPU_HELPERS:
            return None
        registers.update((k, int(i)) for k, i in re.findall(r'cpu->([rs])\[(\d+)\]', source))
        modified.update((k, int(i)) for k, i in re.findall(
            r'cpu->([rs])\[(\d+)\]\s*(?:=|\+=|-=|\|=|&=|\^=)', source))
        pcs.append(pc)
        bodies.append(tuple(body))
        pc = unit.next_pc
    return None
