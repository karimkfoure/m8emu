"""Reuse existing proved pointee spans on the compact release path.

Run on guarded function groups after pointee_spans.specialize_auto_late.
The original pass owns the control-flow proof, cache qualification and resets.
This pass adds no entrance, region or memory operation. A cache miss executes
the original pointer helper and the original first-fault exit.
"""
from __future__ import annotations

import re

PROFILE = (
    'defined(M8_AOT_COMPACT_TABLE) && M8_AOT_COMPACT_TABLE == 1 && defined(M8_AOT_COMPACT_MEMORY) && '
    'defined(M8_AOT_FAST_MEMORY) && defined(M8_AOT_POINTEE_SPANS) && '
    '!defined(M8_AOT_DIAGNOSTIC) && '
    '!defined(M8_AOT_GUEST_COUNT)'
)
DIRECTIVE = re.compile(r'^#(if|ifdef|ifndef|elif|else|endif)\b([^\n]*)$', re.M)
CACHE = re.compile(
    r'uint8_t \*access_host=M8_AUTO_POINTEE_POINTER\('
    r'(?P<host>pointee_[0-9a-f]+_host_\d+),(?P<offset>\d+)u,'
    r'\(uint8_t \*\)\(\(uintptr_t\)ram\.itcm\.host\+\(uintptr_t\)access_addr\)\);'
)
FALLBACK = re.compile(
    r'uint8_t \*access_host=(?P<pointer>(?:M8_FAST_POINTER|m8_aot_guard_data_pointer)'
    r'\(&ram,[^\n;]+,\s*(?P<size>[124])\));\n'
    r'(?P<fault>if\(!access_host\) \{(?:cpu->r\[15\]|r_15)=0x[0-9a-f]+u; '
    r'goto memory_fallback;\})\n'
)


def scopes(source):
    """Locate balanced preprocessor alternatives without parsing C effects."""
    stack = []
    found = []
    for match in DIRECTIVE.finditer(source):
        kind, condition = match.groups()
        if kind in ('if', 'ifdef', 'ifndef'):
            stack.append([match, []])
        elif kind in ('elif', 'else'):
            if not stack:
                return []
            stack[-1][1].append(match)
        else:
            if not stack:
                return []
            first, alternatives = stack.pop()
            found.append((first, alternatives, match))
    return [] if stack else found


def specialize(source, *, enabled=False, stats=None):
    """Keep source identical by default; reuse only already-proved caches."""
    if not enabled:
        return source
    replacements = []
    for first, alternatives, last in scopes(source):
        if 'defined(M8_AOT_FIXED_MEMORY)' not in first[2]:
            continue
        # Only the maintained canonical scalar lowering has this exact shape.
        if len(alternatives) != 1 or alternatives[0][1] != 'else':
            continue
        alternative = alternatives[0]
        native = source[first.end()+1:alternative.start()]
        caches = list(CACHE.finditer(native))
        if len(caches) != 1:
            continue
        fallback = source[alternative.end()+1:last.start()]
        original = FALLBACK.fullmatch(fallback)
        if original is None:
            continue
        cache = caches[0]
        host, offset = cache['host'], int(cache['offset'])
        # This is a reuse pass, not an independent span prover. Require the
        # original proof's declaration, complete qualification and epoch reset.
        if f'uint8_t *{host}=NULL;' not in source:
            continue
        qualification = re.search(
            re.escape(host) + r'=(?:M8_FAST_SPAN_POINTER|m8_aot_guard_data_pointer)'
            r'\(&ram,\(uint32_t\)pointee_[0-9a-f]+_start_\d+,(\d+)u\);', source)
        if qualification is None or source.count(f'{host}=NULL;') < 3:
            continue
        if offset + int(original['size']) > int(qualification[1]):
            continue
        replacement = (
            f'#elif {PROFILE}\n'
            f'uint8_t *access_host=M8_AUTO_POINTEE_POINTER({host},{offset}u,'
            f'{original["pointer"]});\n{original["fault"]}\n#else'
        )
        replacements.append((alternative.start(), alternative.end(), replacement))
    for start, end, replacement in reversed(sorted(replacements)):
        source = source[:start] + replacement + source[end:]
    if stats is not None:
        stats['compact_cached_accesses'] = len(replacements)
    return source
