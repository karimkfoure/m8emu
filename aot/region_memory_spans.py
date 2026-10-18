"""Hoist proven guest-memory range checks to native region entry.

A region page reuses guest registers as address cursors, so whole-page base
invariance almost never holds. This module proves two narrow facts at
generation time, both checked once per region entry at runtime:

* Constant addresses: the accessed window is known at generation time. One
  runtime pointer check at region entry covers every constant access in
  that window; a failed check exits to the exact handler before any effect.
* Register bases that are never assigned in the page: one runtime check at
  region entry covers every access off that base, with a plain NULL test
  at each use. The entry guard dominates every use and the base never
  changes, so no per-access base comparison is needed. Only reads qualify,
  so a surprising target never declines a whole region entry.

Replaced accesses keep their order and effects: a proven access takes the
same fast path the inline region helper would take, and every other access
(including all MMIO and unknown ranges) keeps the unchanged helper call.
Writes qualify only for constant DTCM/OCRAM windows proven at generation
time; the entry check then accepts exactly that proven window.
"""
from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass, field
import re

from memory_spans import ACCESS, ASSIGN, affine

DECL = re.compile(r'uint32_t (base|addr|fp_addr) = (.*);')
CONDITION = ('#if defined(M8_NATIVE_REGION_MEMORY_SPANS) && '
             'M8_NATIVE_REGION_MEMORY_SPANS')

MAX_SPAN = 0x80000
WINDOWS = ((0x0, 0x80000), (0x20000000, 0x20080000),
           (0x20200000, 0x20280000), (0x60000000, 0x60800000))
RAM_WINDOWS = ((0x20000000, 0x20080000), (0x20200000, 0x20280000))


@dataclass
class Use:
    pc: int
    index: int  # insn position inside the unit, for emission ordering
    seq: int  # line order inside the insn body, for emission ordering
    op: str  # 'read' or 'write'
    size: int
    value: str  # result variable (read, without &) or value expression (write)
    offset: int  # guest offset of this access relative to the span low


@dataclass
class Group:
    base: int | None  # guest r-register, or None for constants
    low: int
    high: int
    ram: bool  # any write: DTCM/OCRAM predicate instead of the canonical one
    uses: list = field(default_factory=list)  # Uses in emission order


@dataclass
class Plan:
    groups: list  # Groups in emission order
    claims: dict  # (pc, insn index, seq) -> (group_index, use)

    def stats(self):
        kinds = {}
        for group in self.groups:
            kind = 'const' if group.base is None else 'page'
            row = kinds.setdefault(kind, {'groups': 0, 'accesses': 0})
            row['groups'] += 1
            row['accesses'] += len(group.uses)
        return {'groups': len(self.groups),
                'accesses': sum(len(group.uses) for group in self.groups),
                'by_kind': kinds}


def window_of(addr, size):
    """Generation-time canonical window containing [addr, addr+size), if any."""
    if size <= 0 or addr + size > 0x100000000:
        return None
    for low, high in WINDOWS:
        if low <= addr and addr + size <= high:
            return (low, high)
    return None


def normalize(offset):
    offset &= 0xffffffff
    if offset >= 0x80000000:
        offset -= 0x100000000
    return offset


def collect_events(supported, originals):
    """Map pc -> ordered [(index, seq, op, base, offset, size, value)].

    base is a guest register, None for constant addresses, and the
    'unknown' marker when the address is not affine in one base.
    """
    found = {}
    for pc in sorted(supported):
        events = []
        for index, body in enumerate(originals[pc]):
            variables, seq = {}, 0
            for line in body:
                text = line.strip()
                declaration = DECL.fullmatch(text)
                if declaration:
                    variables[declaration[1]] = affine(declaration[2], variables)
                    continue
                match = ACCESS.fullmatch(text)
                if match is None:
                    continue
                parsed = affine(match[2], variables)
                if parsed is None:
                    events.append((index, seq, match[1], 'unknown', 0,
                                   int(match[3]), match[4]))
                else:
                    base, offset = parsed
                    events.append((index, seq, match[1], base,
                                   normalize(offset), int(match[3]), match[4]))
                seq += 1
        found[pc] = events
    return found


def modified_in(originals, pcs):
    modified = set()
    for pc in pcs:
        for body in originals[pc]:
            modified.update(int(n) for n in ASSIGN.findall('\n'.join(body)))
    return modified


def strip_ref(op, value):
    if op == 'read' and value.startswith('&'):
        return value[1:]
    return value


def analyze(supported, originals):
    """Prove page-level constant and invariant-base spans for one region page."""
    events = collect_events(supported, originals)
    groups = []
    # Constant windows: one runtime check covers every constant access in
    # the window. Windows containing writes must be RAM.
    consts = defaultdict(list)
    for pc in sorted(supported):
        for index, seq, op, base, offset, size, value in events[pc]:
            if base != 'unknown' and base is None:
                addr = offset & 0xffffffff
                window = window_of(addr, size)
                if window is not None:
                    consts[window].append((pc, index, seq, op, size, value, addr))
    for window in sorted(consts):
        rows = consts[window]
        low = min(addr for _, _, _, _, _, _, addr in rows)
        high = max(addr + size for _, _, _, _, size, _, addr in rows)
        if high - low > MAX_SPAN:
            continue
        ram = any(op == 'write' for _, _, _, op, _, _, _ in rows)
        if ram and window not in RAM_WINDOWS:
            continue
        group = Group(None, low, high, ram)
        for pc, index, seq, op, size, value, addr in rows:
            group.uses.append(Use(pc, index, seq, op, size,
                                   strip_ref(op, value), addr - low))
        groups.append(group)
    # Invariant register bases. Reads only: a surprising write target must
    # never decline a whole region entry.
    page_modified = modified_in(originals, sorted(supported))
    reg_uses = defaultdict(list)
    for pc in sorted(supported):
        for index, seq, op, base, offset, size, value in events[pc]:
            if base == 'unknown' or base is None or base not in range(15):
                continue
            if op == 'write':
                continue
            reg_uses[base].append((pc, index, seq, op, size, value, offset))
    for base in sorted(reg_uses):
        rows = reg_uses[base]
        if base in page_modified or len(rows) < 2:
            continue
        low = min(offset for _, _, _, _, _, _, offset in rows)
        high = max(offset + size for _, _, _, _, size, _, offset in rows)
        if high - low > MAX_SPAN:
            continue
        group = Group(base, low, high, False)
        for pc, index, seq, op, size, value, offset in rows:
            group.uses.append(Use(pc, index, seq, op, size,
                                   strip_ref(op, value), offset - low))
        groups.append(group)
    claims = {}
    for group_index, group in enumerate(groups):
        for use in group.uses:
            claims[(use.pc, use.index, use.seq)] = (group_index, use)
    return Plan(groups, claims)


def match_access(line):
    """Match a baseline memory-helper line before region renaming."""
    return ACCESS.fullmatch(line.strip())


def host_name(index):
    return f'm8_region_span_host_{index}'


def declarations(plan):
    rows = []
    for index in range(len(plan.groups)):
        rows += [CONDITION, f'uint8_t *{host_name(index)}=NULL;', '#endif']
    return rows


def page_prologue(plan, metadata):
    """Function-top checks for page-level groups, before the entry switch."""
    rows = []
    for index, group in enumerate(plan.groups):
        host = host_name(index)
        if group.base is None:
            length = f'{group.high - group.low}u'
            rows += [CONDITION,
                     f'{host}=M8_NATIVE_REGION_POINTER(mem,0x{group.low:x}u,{length});',
                     f'if(!{host}) {{cpu->r[15]=pc;']
            if metadata:
                rows += ['exit_source=pc; exit_reason=5u; exit_edge=0;']
            rows += ['goto done;', '}', '#endif']
            continue
        start, end = f'{host}_start', f'{host}_end'
        addr, length_var = f'{host}_addr', f'{host}_len'
        # The function-top guard dominates every use and the base is never
        # assigned in the page, so a NULL check at each use is sufficient.
        rows += [CONDITION, '{',
                 f'int64_t {start}=(int64_t)(uint64_t)(cpu->r[{group.base}])+({group.low});',
                 f'int64_t {end}=(int64_t)(uint64_t)(cpu->r[{group.base}])+({group.high});',
                 f'if({start}>=0 && {end}<=INT64_C(0x100000000)) {{',
                 f'uint32_t {addr}=(uint32_t){start};',
                 f'uint32_t {length_var}=(uint32_t)({end}-{start});',
                 f'{host}=M8_NATIVE_REGION_POINTER(mem,{addr},{length_var});',
                 '}', '}', '#endif']
    return rows


def fast_line(group_index, use):
    host = f'{host_name(group_index)}+{use.offset}u'
    if use.op == 'read':
        return f'{use.value}=m8_le_read({host},{use.size});'
    return f'm8_le_write({host},{use.size},{use.value});'


def replace_line(plan, group, group_index, use, fallback):
    """Guarded direct access with the exact helper as the fallback."""
    rows = [CONDITION]
    host = host_name(group_index)
    if group.base is None:
        rows += [fast_line(group_index, use)]
    else:
        rows += [f'if({host}) {{{fast_line(group_index, use)}}}',
                 f'else {fallback}']
    rows += ['#else', fallback, '#endif']
    return rows
