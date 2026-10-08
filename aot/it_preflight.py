"""Qualify all RAM accesses of one IT unit before its first guest effect.

Analyze original instruction bodies, before fast-memory or scalar rewrites.
Plan.wrap accepts instruction/transaction rows. The translator must append the
unchanged next-unit jump after the guarded rows, then scalarize the function.
Use an explicit translator option; this module does not enable itself.
"""
from dataclasses import dataclass
import re

from capstone import arm as A
from memory_spans import ACCESS, affine

GUARD = ('defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) '
         '&& !defined(M8_AOT_GUEST_COUNT)')
DECLARATION = re.compile(r'uint32_t (base|addr|fp_addr) = (.*);')
LOCAL_WRITE = re.compile(r'\b(base|addr|fp_addr)\s*(?:[+\-*/|&^]|<<|>>)?=(?!=)')
CONTROL = re.compile(r'\b(?:goto|return)\b|\bfn_[0-9a-f]+\s*\(|\bm8_tail\s*\(')
CALLS = re.compile(r'\b([A-Za-z_]\w*)\s*\(')
REG_WRITE = re.compile(r'cpu->r\[(\d+)\]\s*(?:(?:[+\-*/%|&^]|<<|>>)?=(?!=)|\+\+|--)')
REG_PREFIX_WRITE = re.compile(r'(?:\+\+|--)\s*cpu->r\[(\d+)\]')
# These emitter helpers modify only flags/FP registers, or return a value.
# Adding another opaque helper requires checking its state and exit contract.
HELPERS = set('''if sizeof UINT32_C UINT64_C INT32_C INT64_C
    m8_read m8_write m8_shift m8_signed m8_s32 m8_ror m8_clz m8_rbit
    m8_logic_flags m8_nz m8_add_flags m8_sub_flags m8_parallel m8_qresult
    m8_sat_signed m8_sat_unsigned m8_sel m8_usad8
    m8_fp_clear m8_fp_cmp m8_fp_configure m8_fp_convert m8_fp_eval32
    m8_fp_from_int m8_fp_get64 m8_fp_op64 m8_fp_rint m8_fp_set64
    m8_fp_sync m8_fp_to_int'''.split())


@dataclass(frozen=True)
class Access:
    pc: int
    line: str
    operation: str
    base: int | None
    offset: int
    size: int
    value: str


@dataclass
class Plan:
    unit: object
    bodies: list
    groups: list
    accesses: list
    replacements: dict

    def prelude(self, memory='&ram'):
        """Emit pure address checks; reject before any CPU, FP or RAM effect."""
        rows = []
        fail = f'{{cpu->r[15]=0x{self.unit.pc:x}u; goto done;}}'
        for index, group in enumerate(self.groups):
            name = f'it_span_{self.unit.pc:x}_{index}'
            base = '0' if group['base'] is None else f'cpu->r[{group["base"]}]'
            size = group['high'] - group['low']
            rows += [f'int64_t {name}_start=(int64_t)(uint64_t)({base})+({group["low"]});',
                     f'if({name}_start<0 || {name}_start+{size}>INT64_C(0x100000000)) {fail}',
                     f'uint8_t *{name}=M8_FAST_SPAN_POINTER({memory},(uint32_t){name}_start,{size}u);',
                     f'if(!{name}) {fail}']
        return rows

    def replace(self, pc, body):
        """Keep original effect order and preserve host-buffer alias behavior."""
        rows = []
        for line in body:
            match = ACCESS.fullmatch(line)
            if match is None:
                if 'm8_read(' in line or 'm8_write(' in line:
                    raise ValueError(f'{pc:#x}: access differs from the qualified body: {line}')
                rows.append(line)
                continue
            index, offset = self.replacements[(pc, line)]
            host = f'(it_span_{self.unit.pc:x}_{index}+{offset}u)'
            operation, _address, size, value = match.groups()
            if operation == 'read':
                rows.append(f'{value.removeprefix("&")}=m8_le_read({host},{size});')
            else:
                rows.append(f'm8_le_write({host},{size},{value});')
        return rows

    def emit(self, memory='&ram'):
        """Emit the complete fast unit, without a snapshot or store journal.

        A unit with one common false predicate needs no checks or effects.
        Otherwise qualify all accesses, including members that a predicate
        can suppress. Rejection may return an earlier safe prefix. Conditions
        still read current flags before each member, as in the original unit.
        """
        conditions = {i.cc for i in self.unit.instructions}
        common = len(conditions) == 1 and next(iter(conditions)) not in (A.ARM_CC_INVALID, A.ARM_CC_AL)
        rows = ['{']
        if common:
            rows.append(f'if(m8_condition(cpu->xpsr,{next(iter(conditions))})) {{')
        rows.extend(self.prelude(memory))
        for insn, body in zip(self.unit.instructions, self.bodies):
            conditional = insn.cc not in (A.ARM_CC_INVALID, A.ARM_CC_AL)
            rows += ['{', f'/* {insn.address:08x}: {insn.mnemonic} {insn.op_str} */']
            if conditional:
                rows.append(f'if(m8_condition(cpu->xpsr,{insn.cc})) {{')
            rows.extend(self.replace(insn.address, body))
            if conditional:
                rows.append('}')
            rows.append('}')
        if common:
            rows.append('}')
        return rows + ['}']

    def wrap(self, original_rows, memory='&ram'):
        """Guard unit effects, excluding its label and outgoing jump.

        Include original snapshot, instruction and commit rows in the fallback.
        The caller keeps the next-unit jump common to both paths.
        """
        return [f'#if {GUARD}', *self.emit(memory), '#else',
                *original_rows, '#endif']

    def stats(self):
        return {'pc': self.unit.pc, 'spans': self.groups,
                'accesses': len(self.accesses),
                'reads': sum(a.operation == 'read' for a in self.accesses),
                'writes': sum(a.operation == 'write' for a in self.accesses)}


def analyze_detail(unit, bodies):
    """Return (plan, rejection reason); unknown address forms remain original."""
    if not unit.it:
        return None, 'not an IT unit'
    if len(bodies) != len(unit.instructions) or not 1 <= len(bodies) <= 4:
        return None, 'incomplete IT unit'
    if any(body is None for body in bodies):
        return None, 'unsupported instruction'
    modified = set()
    accesses = []
    for insn, body in zip(unit.instructions, bodies):
        variables = {}
        for line in body:
            if set(CALLS.findall(line)) - HELPERS or '&cpu->r[' in line:
                return None, 'opaque helper or escaped integer register'
            match = ACCESS.fullmatch(line)
            if match is not None:
                operation, address, size, value = match.groups()
                size = int(size)
                parsed = affine(address, variables)
                if parsed is None or size not in (1, 2, 4):
                    return None, 'unknown memory address or width'
                base, offset = parsed
                if base is not None and not 0 <= base < 15:
                    return None, 'unsupported guest base'
                offset &= 0xffffffff
                if base is not None and offset >= 0x80000000:
                    offset -= 0x100000000
                if base is None and offset + size > 0x100000000:
                    return None, 'literal access wraps'
                if operation == 'read' and not re.fullmatch(r'&[A-Za-z_]\w*', value):
                    return None, 'unknown load destination'
                accesses.append(Access(insn.address, line, operation, base, offset, size, value))
                continue
            if 'm8_read(' in line or 'm8_write(' in line:
                return None, 'unhandled memory access'
            if CONTROL.search(line) or 'cpu->r[15]' in line:
                return None, 'call or control exit'
            modified.update(int(n) for n in REG_WRITE.findall(line))
            modified.update(int(n) for n in REG_PREFIX_WRITE.findall(line))
            declaration = DECLARATION.fullmatch(line)
            if declaration:
                variables[declaration[1]] = affine(declaration[2], variables)
            else:
                for name in LOCAL_WRITE.findall(line):
                    variables[name] = None
    if not accesses:
        return None, 'no memory accesses'
    if any(a.base in modified for a in accesses if a.base is not None):
        return None, 'guest base changes in the IT unit'
    groups = []
    for base in sorted({a.base for a in accesses}, key=lambda b: -1 if b is None else b):
        ranges = sorted((a.offset, a.offset + a.size) for a in accesses if a.base == base)
        for low, high in ranges:
            if groups and groups[-1]['base'] == base and low <= groups[-1]['high']:
                groups[-1]['high'] = max(groups[-1]['high'], high)
            else:
                groups.append({'base': base, 'low': low, 'high': high})
    if any(group['high'] - group['low'] > 0xffffffff for group in groups):
        return None, 'span is too large'
    replacements = {}
    for access in accesses:
        index = next(i for i, group in enumerate(groups)
                     if group['base'] == access.base and group['low'] <= access.offset
                     and access.offset + access.size <= group['high'])
        replacements[(access.pc, access.line)] = (index, access.offset - groups[index]['low'])
    return Plan(unit, bodies, groups, accesses, replacements), ''


def analyze(unit, bodies):
    return analyze_detail(unit, bodies)[0]
