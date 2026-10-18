"""Check fixed scalar accesses before forming their host pointers.

The existing public wrapper must first qualify the complete fixed map. Keep
span caches, multi-access preflight and service/DWT branches unchanged. Only
ordinary scalar guards with a pure local address expression are eligible.
"""
from __future__ import annotations

import re

PROFILE = ('defined(M8_AOT_VALIDITY_FIRST) && defined(M8_AOT_FAST_MEMORY) && '
           'defined(M8_AOT_FIXED_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && '
           '!defined(M8_AOT_GUEST_COUNT)')
ADDRESS = r'(?:addr|fp_addr)(?:\s*\+\s*\d+u?)?'
ACCESS = re.compile(
    r'(?P<declare>uint8_t \*access_host=M8_FAST_POINTER\(&ram, '
    rf'(?P<address>{ADDRESS}), (?P<size>[124])\);)\n'
    r'(?P<failure>if\(!access_host\) \{cpu->r\[15\]=0x[0-9a-f]+u; '
    r'goto memory_fallback;\})')


def specialize(source: str) -> str:
    """Keep the old source under the inverse release-profile guard.

    The address temporary retains the pointer helper's uint32_t conversion
    and single evaluation. It cannot contain a call, volatile read, increment
    or other effect. Invalid addresses leave before pointer formation. IT
    stores still use the original journal and rollback path.
    """
    if '#if '+PROFILE in source:
        return source

    def replace(match: re.Match[str]) -> str:
        failure = match['failure'].replace(
            '!access_host',
            f'!m8_canonical_offset_valid(access_addr,{match["size"]})')
        return '\n'.join([
            '#if '+PROFILE,
            f'uint32_t access_addr=(uint32_t)({match["address"]});',
            failure,
            'uint8_t *access_host=(uint8_t *)((uintptr_t)ram.itcm.host+'
            '(uintptr_t)access_addr);',
            '#else', match['declare'], match['failure'], '#endif'])

    return ACCESS.sub(replace, source)
