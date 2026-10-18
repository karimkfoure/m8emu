"""Prove a bounded IT unit's CPU writes before narrowing its rollback copy.

Analyze original emitted bodies, before register scalarization or fast-memory
rewrites. The store journal, conditions, member order and fallback edges stay
unchanged. The caller must retain its normal scalar-to-CPU save bridge before
begin_call(). Diagnostic and instruction-count builds retain full snapshots.
"""
from dataclasses import dataclass
import re

from capstone import CsError, arm as A
from codegen_v2 import code_mask

ORIGINAL = 'm8_it_begin(&tx,cpu);'
CALL = re.compile(r'\b([A-Za-z_]\w*)\s*\(')
FIELD = re.compile(r'\bcpu\s*->\s*(?:([rs])\s*\[\s*(\d+)\s*\]|(xpsr|fpscr))')
WRITE = re.compile(r'\s*(?:=(?!=)|(?:<<|>>|[+\-*/%|&^])=|\+\+|--)')
# These helpers can modify only xpsr/fpscr, or return a value. read/write cannot
# invoke a callback: an IT transaction refuses the slow path before the call.
INTEGER = set('''if sizeof UINT32_C UINT64_C INT32_C INT64_C
    m8_read m8_write m8_shift m8_signed m8_s32 m8_asr m8_ror m8_clz
    m8_rbit m8_logic_flags m8_nz m8_add_flags m8_sub_flags m8_parallel
    m8_qresult m8_sat_signed m8_sat_unsigned m8_sel m8_usad8'''.split())
FP = set('''m8_fp_clear m8_fp_cmp32 m8_fp_cmp64 m8_fp_configure
    m8_fp_convert m8_fp_eval32 m8_fp_from_int m8_fp_op32 m8_fp_op64
    m8_fp_rint m8_fp_sync m8_fp_to_int'''.split())
PAIRS = {'m8_fp_get64', 'm8_fp_set64'}
CPU_HELPERS = (INTEGER - {'if', 'sizeof', 'UINT32_C', 'UINT64_C', 'INT32_C',
                         'INT64_C', 'm8_read', 'm8_write', 'm8_shift',
                         'm8_signed', 'm8_s32', 'm8_asr', 'm8_ror', 'm8_clz',
                         'm8_rbit', 'm8_sel', 'm8_usad8'}) | (FP - {'m8_fp_clear'}) | PAIRS
SYSTEM_IDS = {getattr(A, 'ARM_INS_' + name) for name in
              ('MSR', 'MRS', 'VMRS', 'VMSR', 'CPS', 'SETEND', 'WFI', 'WFE',
               'SEV', 'SVC', 'BKPT')}
ALIASES = {'sp': 13, 'lr': 14, 'pc': 15, 'ip': 12, 'fp': 11,
           'sl': 10, 'sb': 9}
FLAGS = {'apsr', 'apsr_nzcv', 'cpsr', 'fpscr', 'fpscr_nzcv'}
TYPES = {'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t', 'int8_t', 'int16_t',
         'int32_t', 'int64_t', 'int', 'unsigned'}
TOKENS = TYPES | INTEGER | FP | PAIRS | {'else', 'goto', 'fault', 'NULL',
    'UINT32_MAX', 'UINT64_MAX', 'INT32_MAX', 'INT32_MIN', 'INT64_MAX', 'INT64_MIN', 'slow'}


@dataclass(frozen=True)
class Plan:
    r_mask: int
    s_mask: int
    save_fp: bool

    @property
    def cpu_words(self):
        return self.r_mask.bit_count() + self.s_mask.bit_count() + 2

    def begin_call(self):
        return (f'm8_it_begin_partial(&tx,cpu,0x{self.r_mask:x}u,'
                f'0x{self.s_mask:x}u,{int(self.save_fp)});')


def analyze(unit, bodies):
    """Return a complete conservative write set, or None for unknown input.

    Union decoded writes with emitted writes. Both independent descriptions
    must be understood; a new emitter/helper does not silently gain support.
    Pair helpers require a constant in-range D-register number. CPU pointers,
    dynamic indices, register addresses and non-fault control exits decline.
    """
    if not unit.it or not 1 <= len(unit.instructions) <= 4 or len(bodies) != len(unit.instructions):
        return None
    r_mask, s_mask, save_fp = 0x8000, 0, False
    for insn, body in zip(unit.instructions, bodies):
        if not isinstance(body, (tuple, list)) or not all(isinstance(line, str) for line in body):
            return None
        if insn.id in SYSTEM_IDS:
            return None
        try:
            _reads, writes = insn.regs_access()
            for register in writes:
                name = insn.reg_name(register)
                if not isinstance(name, str):
                    return None
                if name in FLAGS:
                    continue
                if name in ALIASES:
                    r_mask |= 1 << ALIASES[name]
                elif re.fullmatch(r'r(?:[0-9]|1[0-5])', name):
                    r_mask |= 1 << int(name[1:])
                elif re.fullmatch(r's(?:[0-9]|[12][0-9]|3[01])', name):
                    s_mask |= 1 << int(name[1:])
                elif re.fullmatch(r'd(?:[0-9]|1[0-5])', name):
                    s_mask |= 3 << (2 * int(name[1:]))
                else:
                    return None
        except (AttributeError, ValueError, CsError):
            return None
        source = code_mask('\n'.join(body))
        if (re.search(r'\b(?:return|float|double|tx|m8_current_it|m8_slow_fallback)\b|\bgoto\s+(?!fault\b)', source)
                or re.search(r'\b\d+(?:\.|[eE][+\-]?\d)|\.\d+', source)
                or re.search(r'\b(?:u?int\d+_t|void|m8_\w+)\s*\*|(?:^|[=({;,])\s*\*', source)
                or re.search(r'^\s*#(?!\s*(?:ifn?def\s+M8_AOT_NATIVE_FP\b|else\b|endif\b))', source, re.M)):
            return None
        helpers = {match[1] for match in CALL.finditer(source)}
        if helpers - INTEGER - FP - PAIRS:
            return None
        save_fp |= bool(helpers & FP)
        # First consume recognized fields, retaining source offsets for writes.
        fields = list(FIELD.finditer(source))
        for match in fields:
            prefix, suffix = source[:match.start()].rstrip(), source[match.end():]
            if (prefix.endswith('&') or prefix.endswith('++') or prefix.endswith('--')
                    or re.match(r'\s*\)\s*(?:=(?!=)|[+\-*/%|&^]=|<<=|>>=|\+\+|--)', suffix)):
                return None
            if match[1]:
                index = int(match[2])
                if index >= (16 if match[1] == 'r' else 32):
                    return None
                if WRITE.match(suffix):
                    if match[1] == 'r':
                        r_mask |= 1 << index
                    else:
                        s_mask |= 1 << index
        for match in reversed(fields):
            source = source[:match.start()] + '0' + ' ' * (match.end() - match.start() - 1) + source[match.end():]
        if '[' in source or '->' in source:
            return None
        # Pair helper state is explicit, even when Capstone omits a destination.
        for helper in PAIRS:
            for match in re.finditer(r'\b' + helper + r'\s*\(', source):
                pair = re.match(r'\s*cpu\s*,\s*(\d+)\s*(?=,|\))', source[match.end():])
                if pair is None or int(pair[1]) >= 16:
                    return None
                if helper == 'm8_fp_set64':
                    s_mask |= 3 << (2 * int(pair[1]))
        for helper in CPU_HELPERS:
            source = re.sub(r'(\b' + helper + r'\s*\(\s*)cpu\b', r'\g<1>0', source)
        source = re.sub(r'(\bm8_(?:read|write)\s*\(\s*)mem\b', r'\g<1>0', source)
        if re.search(r'\b(?:cpu|mem)\b', source):
            return None
        source = re.sub(r'^\s*#.*$', '', source, flags=re.M)
        locals_ = set(re.findall(r'\b(?:' + '|'.join(sorted(TYPES)) + r')\s+(\w+)', source))
        tokens = set(re.findall(r'\b[A-Za-z_]\w*\b', source))
        if any(token not in TOKENS | locals_ and not re.fullmatch(r'M8_FP_[A-Z0-9_]+', token)
               for token in tokens):
            return None
    return Plan(r_mask, s_mask, save_fp)


def begin_call(unit, bodies):
    """Use the original transaction whenever the complete proof declines."""
    plan = analyze(unit, bodies)
    return plan.begin_call() if plan is not None else ORIGINAL


def specialize_frozen(function, bodies, source, stats=None):
    """Apply the same proof to pinned emitted code before the V2 rewrite.

    Require the original unit label, explicit transaction PC, normal state
    save bridge and exact member comments. Decline unknown calls, extra state
    writes and escaped register fields. The caller owns source/firmware pins
    and retains a separate unchanged source for inverse entry bodies.
    Only the begin identifier and constant arguments change; lines survive.
    """
    mask = code_mask(source)
    calls = list(re.finditer(r'\bm8_it_begin\s*\(', mask))
    report = {'begin_calls': len(calls), 'replaced_calls': 0, 'plans': []}
    labels = list(re.finditer(r'^\s*L_([0-9a-f]+):', mask, re.M))
    prelude = re.compile(r'\bpc\s*=\s*0x([0-9a-f]+)u\s*;\s*'
        r'(?:r_15|cpu\s*->\s*r\s*\[\s*15\s*\])\s*=\s*pc\s*;\s*'
        r'M8_SAVE\s*\(\s*cpu\s*\)\s*;\s*'
        r'(?P<name>m8_it_begin)\s*\(\s*&tx\s*,\s*cpu\s*(?P<close>\))\s*;\s*'
        r'it_active\s*=\s*1\s*;')
    emitted_helpers = INTEGER | FP | PAIRS | set('''m8_it_begin m8_it_commit
        M8_SAVE M8_COUNT_BEGIN M8_COUNT_ONE m8_condition m8_flags_condition
        m8_flags_logic m8_flags_nz m8_flags_add m8_flags_sub m8_flags_word
        m8_fp_float m8_fp_bits32 m8_fp_double m8_fp_bits64 m8_le_read
        m8_le_write m8_write_it m8_pointer M8_FAST_POINTER M8_FAST_SPAN_POINTER
        m8_canonical_offset_valid m8_canonical_offset_data_valid
        m8_aot_guard_write m8_aot_guard_read m8_aot_guard_data_pointer
        m8_aot_guard_offset_valid m8_aot_guard_offset_data_valid'''.split())
    edits = []
    for index, label in enumerate(labels):
        pc = int(label[1], 16)
        unit = function.units.get(pc)
        if unit is None or pc not in bodies:
            continue
        plan = analyze(unit, bodies[pc])
        if plan is None:
            continue
        end = labels[index + 1].start() if index + 1 < len(labels) else len(source)
        block, code = source[label.end():end], mask[label.end():end]
        starts = list(prelude.finditer(code))
        if (len(starts) != 1 or int(starts[0][1], 16) != pc
                or len(re.findall(r'\bm8_it_begin\s*\(', code)) != 1):
            continue
        members = re.findall(r'/\*\s*([0-9a-f]{8}):', block)
        if set(map(lambda value: int(value, 16), members)) != {insn.address for insn in unit.instructions}:
            continue
        # Guarded reads/writes inherit the transaction's pre-callback slow
        # refusal. Their admission checks modify no CPU word or FP state.
        calls_in_code = {match[1] for match in CALL.finditer(re.sub(r'^\s*#.*$', '', code, flags=re.M))}
        if calls_in_code - emitted_helpers:
            continue
        if not plan.save_fp and calls_in_code & FP:
            continue
        state = re.compile(r'\b(?:cpu\s*->\s*([rs])\s*\[\s*(\d+)\s*\]|([rs])_(\d+))')
        valid = True
        for field in state.finditer(code):
            kind, number = (field[1], field[2]) if field[1] else (field[3], field[4])
            number = int(number)
            if (number >= (16 if kind == 'r' else 32)
                    or code[:field.start()].rstrip().endswith(('&', '++', '--'))
                    or re.match(r'\s*\)\s*(?:=(?!=)|[+\-*/%|&^]=|<<=|>>=|\+\+|--)', code[field.end():])
                    or WRITE.match(code[field.end():]) and not ((plan.r_mask if kind == 'r' else plan.s_mask) & (1 << number))):
                valid = False
                break
        if not valid or re.search(r'\bcpu\s*->', FIELD.sub('0', code)):
            continue
        for helper in PAIRS:
            for call in re.finditer(r'\b' + helper + r'\s*\(', code):
                pair = re.match(r'\s*cpu\s*,\s*(\d+)\s*(?=,|\))', code[call.end():])
                if pair is None or int(pair[1]) >= 16:
                    valid = False
                elif helper == 'm8_fp_set64':
                    bits = 3 << (2 * int(pair[1]))
                    valid &= plan.s_mask & bits == bits
        if not valid:
            continue
        start = starts[0]
        offset = label.end()
        edits += [(offset + start.start('name'), offset + start.end('name'), 'm8_it_begin_partial'),
                  (offset + start.start('close'), offset + start.start('close'),
                   f',0x{plan.r_mask:x}u,0x{plan.s_mask:x}u,{int(plan.save_fp)}')]
        report['plans'].append({'pc': pc, 'r_mask': plan.r_mask, 's_mask': plan.s_mask,
                               'save_fp': plan.save_fp, 'cpu_words': plan.cpu_words})
    for start, end, replacement in sorted(edits, reverse=True):
        source = source[:start] + replacement + source[end:]
    report['replaced_calls'] = len(report['plans'])
    if stats is not None:
        stats.update(report)
    return source
