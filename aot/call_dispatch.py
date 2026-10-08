"""Emit a bounded, exact native function-entry lookup for computed calls.

This optional generator does not change public hook lookup. A table row
contains the full guest entry and its native pointer.
Every lookup checks that tag before returning the pointer, including unknown
targets that hash to an occupied row. The table has no mutable or cached state.
"""
from __future__ import annotations

from dataclasses import dataclass
import re

MASK = 0xffffffff
BUCKET_MULTIPLIER = 0x9e3779b1
SLOT_MULTIPLIER = 0xc2b2ae35
SEED_MULTIPLIER = 0x85ebca6b


def index(value, multiplier, bits):
    return ((value * multiplier) & MASK) >> (32 - bits)


@dataclass(frozen=True)
class Layout:
    bucket_bits: int
    slot_bits: int
    seeds: tuple[int, ...]
    slots: tuple[int | None, ...]

    @classmethod
    def build(cls, entries):
        entries = tuple(sorted(entries))
        if len(set(entries)) != len(entries):
            raise ValueError('Native entries must be unique')
        if any(not 0 <= pc <= MASK or pc & 1 for pc in entries):
            raise ValueError('Native entries must be even uint32 addresses')
        bucket_bits = max(1, (max(1, len(entries) // 3) - 1).bit_length())
        buckets = [[] for _ in range(1 << bucket_bits)]
        for pc in entries:
            buckets[index(pc, BUCKET_MULTIPLIER, bucket_bits)].append(pc)
        slot_bits = max(1, (max(2, len(entries) * 3 // 2) - 1).bit_length())
        while slot_bits <= 20:
            slots = [None] * (1 << slot_bits)
            seeds = [0] * len(buckets)
            for bucket in sorted(range(len(buckets)), key=lambda b: (-len(buckets[b]), b)):
                values = buckets[bucket]
                if not values:
                    continue
                for seed in range(65536):
                    mixed = (seed * SEED_MULTIPLIER) & MASK
                    positions = [index(pc ^ mixed, SLOT_MULTIPLIER, slot_bits) for pc in values]
                    if len(set(positions)) == len(positions) and all(slots[p] is None for p in positions):
                        seeds[bucket] = mixed
                        for position, pc in zip(positions, values):
                            slots[position] = pc
                        break
                else:
                    break
            else:
                return cls(bucket_bits, slot_bits, tuple(seeds), tuple(slots))
            slot_bits += 1
        raise ValueError('Cannot construct bounded native entry table')

    def resolve(self, pc):
        bucket = index(pc, BUCKET_MULTIPLIER, self.bucket_bits)
        slot = index(pc ^ self.seeds[bucket], SLOT_MULTIPLIER, self.slot_bits)
        return self.slots[slot] if self.slots[slot] == pc else None

    def emit(self, callee_names=None):
        """Emit after the translator has declared every static fn_ENTRY."""
        callee_names=callee_names or {}
        if set(callee_names)-{pc for pc in self.slots if pc is not None} or any(
                not isinstance(name,str) or re.fullmatch(r'[A-Za-z_]\w*',name) is None
                for name in callee_names.values()):
            raise ValueError('Native callee aliases must name declared exact entries')
        rows = [
            'typedef int (*m8_native_callee)(m8_cpu *restrict,m8_mem *restrict);',
            'static const uint32_t m8_call_seeds[]={',
            ','.join(f'0x{x:x}u' for x in self.seeds), '};',
            'static const struct {uint32_t entry; m8_native_callee function;} m8_call_slots[]={',
        ]
        rows += ['{0,NULL},' if pc is None else f'{{0x{pc:x}u,{callee_names.get(pc,f"fn_{pc:x}")}}},' for pc in self.slots]
        rows += [
            '};',
            'static inline m8_native_callee m8_resolve_entry(uint32_t pc) {',
            f'uint32_t bucket=(pc*0x{BUCKET_MULTIPLIER:x}u)>>{32-self.bucket_bits};',
            f'uint32_t slot=((pc^m8_call_seeds[bucket])*0x{SLOT_MULTIPLIER:x}u)>>{32-self.slot_bits};',
            'return m8_call_slots[slot].entry==pc ? m8_call_slots[slot].function : NULL;',
            '}',
            'static int m8_tail(m8_cpu *restrict cpu,m8_mem *restrict mem) {',
            'm8_native_callee callee=m8_resolve_entry(cpu->r[15]);',
            'return callee ? callee(cpu,mem) : M8_AOT_EXIT;',
            '}',
        ]
        return '\n'.join(rows) + '\n'


def blx_body(target, next_pc):
    """Original BLX effects with one lookup and an explicit scalar bridge.

    `target` is the generated register expression; `next_pc` is the even return
    boundary. A BLX register target must have its Thumb bit set. An even target
    exits before the instruction so Unicorn can handle its original fault.
    M8_CALL(callee)
    must expand to full M8_SAVE/M8_LOAD, never an inferred static callee mask.
    The translator's transitive register scan must mark this as a computed call.
    Calls inside IT remain unsupported. The count rewind and early unknown exit
    stay before LR/PC effects. Redirect tails use the same exact tagged lookup.
    """
    if next_pc & 1:
        raise ValueError('BLX return boundary must be even')
    return [
        f'uint32_t raw_target=({target});',
        'uint32_t target=raw_target & ~1u;',
        'm8_native_callee callee=(raw_target & 1u) ? m8_resolve_entry(target) : NULL;',
        'if (!callee) {M8_COUNT_REWIND(); cpu->r[15]=pc; goto done;}',
        f'cpu->r[14]=0x{next_pc|1:x}u;',
        'cpu->r[15]=target;',
        'int call_result=M8_CALL(callee);',
        f'if(call_result!=M8_AOT_EXIT || cpu->r[15]!=0x{next_pc:x}u) return call_result;',
    ]
