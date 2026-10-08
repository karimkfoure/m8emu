"""Exact region-local services for the synchronous native playback model.

This module does not change the maintained AOT translator. Its caller must
verify immutable code and retain every runtime hook and device-service exit.
Mask-changing instructions deliberately remain in the exact step handler.
"""
from capstone import arm as A


def instruction(insn, *, ram_only=False):
    raw = bytes(insn.bytes)
    h1 = int.from_bytes(raw[:2], 'little')
    h2 = int.from_bytes(raw[2:], 'little') if len(raw) == 4 else 0
    word = (h1 << 16) | h2
    if not ram_only and word in (0xf3bf8f4f, 0xf3bf8f5f, 0xf3bf8f6f):
        # RAM and device effects complete on this thread. Code writes already
        # leave before effects; no deferred cache observer crosses this point.
        return ['__asm__ volatile("" ::: "memory");']
    if not ram_only and insn.id in (A.ARM_INS_PLD, A.ARM_INS_PLDW):
        return []
    if (h1 & 0xfff0) == 0xe850 and (h2 & 0x0f00) == 0x0f00:
        rn, rt, offset = h1 & 15, h2 >> 12, (h2 & 255) * 4
        if rn == 15 or rt >= 13:
            return None
        return [f'uint32_t exclusive_addr=cpu->r[{rn}]+{offset}u;',
                'uint8_t *exclusive_ptr=m8_native_region_exclusive_pointer(mem,exclusive_addr);',
                'if(!exclusive_ptr) goto services_unhandled;',
                'if(exclusive_addr & 3u) goto fault;',
                'uint32_t exclusive_value=m8_le_read(exclusive_ptr,4);',
                f'cpu->r[{rt}]=exclusive_value;',
                'arch->exclusive_address=exclusive_addr;',
                'arch->exclusive_value=exclusive_value;', 'arch->exclusive_valid=1;']
    if (h1 & 0xfff0) == 0xe840:
        rn, rt, rd, offset = h1 & 15, h2 >> 12, (h2 >> 8) & 15, (h2 & 255) * 4
        if rn == 15 or rt >= 13 or rd >= 13 or rd in (rn, rt):
            return None
        return [f'uint32_t exclusive_addr=cpu->r[{rn}]+{offset}u;',
                'uint8_t *exclusive_ptr=m8_native_region_exclusive_pointer(mem,exclusive_addr);',
                'if(!exclusive_ptr) goto services_unhandled;',
                'uint32_t exclusive_failed=!arch->exclusive_valid ||',
                'arch->exclusive_address!=exclusive_addr;',
                'if(!exclusive_failed) {',
                'if(exclusive_addr & 3u) goto fault;',
                'exclusive_failed=arch->exclusive_value!=m8_le_read(exclusive_ptr,4);',
                f'if(!exclusive_failed) m8_le_write(exclusive_ptr,4,cpu->r[{rt}]);', '}',
                f'cpu->r[{rd}]=exclusive_failed;', 'arch->exclusive_valid=0;']
    return None


def boundary(insn, *, ram_only=False):
    # Retain budget leaders after formerly unsupported instructions. This
    # also preserves leaders used by the measured computed-target whitelist.
    return instruction(insn, ram_only=ram_only) is not None
