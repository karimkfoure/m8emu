"""Conservative NZCV liveness for bounded translated control-flow graphs."""
from dataclasses import dataclass
from capstone import arm as A

N, Z, C, V = 8, 4, 2, 1
ALL = N | Z | C | V
CONDITIONS = {1: Z, 2: Z, 3: C, 4: C, 5: N, 6: N, 7: V, 8: V,
              9: C | Z, 10: C | Z, 11: N | V, 12: N | V,
              13: N | Z | V, 14: N | Z | V}


@dataclass(frozen=True)
class Effect:
    reads: int
    writes: int
    observable: bool
    eligible: bool
    conditional: bool


def effect(insn, body):
    """Classify the emitted helpers, so Capstone's S-bit errors cannot kill flags."""
    reads = CONDITIONS.get(insn.cc, 0)
    if body is None:
        return Effect(ALL, 0, True, False, False)
    source = '\n'.join(body)
    writes, eligible = 0, False
    if 'm8_add_flags(' in source or 'm8_sub_flags(' in source:
        writes, eligible = ALL, True
    elif 'm8_logic_flags(' in source:
        writes, eligible = N | Z | C, True
    elif 'm8_nz(' in source:
        writes, eligible = N | Z, True
    elif 'cpu->xpsr =' in source:
        # VMRS APSR_NZCV,FPSCR is the only supported direct APSR assignment.
        # Preserve its synchronization helper and its own write.
        writes = ALL
    if 'cpu->xpsr >> 29' in source:
        reads |= C
    if insn.id in (A.ARM_INS_ADC, A.ARM_INS_SBC, A.ARM_INS_RSC, A.ARM_INS_RRX):
        reads |= C
    if any(op.shift.type in (A.ARM_SFT_RRX, A.ARM_SFT_RRX_REG) for op in insn.operands):
        reads |= C
    if ('cpu->xpsr' in source and 'cpu->xpsr >> 29' not in source
            and 'cpu->xpsr =' not in source and 'm8_sel(cpu->xpsr,' not in source):
        reads |= ALL
    observable = (insn.id in (A.ARM_INS_BL, A.ARM_INS_BLX, A.ARM_INS_BX)
                  or 'm8_read(' in source or 'm8_write(' in source
                  or 'goto done;' in source or 'goto redirect;' in source
                  or 'return ' in source)
    return Effect(reads, writes, observable, eligible,
                  insn.cc not in (A.ARM_CC_INVALID, A.ARM_CC_AL))


def successors(unit):
    last = unit.instructions[-1]
    if last.id in (A.ARM_INS_B, A.ARM_INS_CBZ, A.ARM_INS_CBNZ):
        result = [last.operands[-1].imm & ~1]
        if last.id != A.ARM_INS_B or last.cc not in (A.ARM_CC_INVALID, A.ARM_CC_AL):
            result.append(unit.next_pc)
        return result
    if last.id in (A.ARM_INS_TBB, A.ARM_INS_TBH):
        return unit.table_targets or [None]
    if last.id == A.ARM_INS_BX:
        return [None]
    # Calls keep the continuation edge, but both sides of the call observe
    # all flags. Other computed PC writes are marked observable by their body.
    return [unit.next_pc]


def analyze(function, bodies):
    """Return whole flag writes whose outputs are dead on every possible path.

    Memory, calls and unknown exits observe all flags before and after the
    instruction. IT members retain all writes; predicates cannot kill a flag
    because the false path preserves it. Q and GE are outside this analysis.
    """
    effects = {pc: [effect(i, body) for i, body in zip(unit.instructions, bodies[pc])]
               for pc, unit in function.units.items()}
    incoming = {pc: 0 for pc in function.units}
    outgoing = dict(incoming)
    def transfer(unit, live, collect=None):
        if any(e.observable for e in effects[unit.pc]):
            live |= ALL
        for insn, e in reversed(list(zip(unit.instructions, effects[unit.pc]))):
            if e.observable:
                live |= ALL
            if (collect is not None and not unit.it and e.eligible
                    and not (e.writes & live)):
                collect.add(insn.address)
            kill = e.writes if not (unit.it or e.conditional) else 0
            live = e.reads | (live & ~kill)
            if e.observable:
                live |= ALL
        return live
    changed = True
    while changed:
        changed = False
        for pc, unit in sorted(function.units.items(), reverse=True):
            live = 0
            for target in successors(unit):
                live |= incoming.get(target, ALL)
            outgoing[pc] = live
            value = transfer(unit, live)
            if value != incoming[pc]:
                incoming[pc], changed = value, True
    omitted = set()
    for pc, unit in function.units.items():
        transfer(unit, outgoing[pc], omitted)
    return omitted, {'writers': sum(bool(e.writes) for es in effects.values() for e in es),
                     'omitted_writes': len(omitted), 'omitted_pcs': sorted(omitted)}
