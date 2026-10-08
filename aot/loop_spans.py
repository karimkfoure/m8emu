"""Qualify stable object and stack spans at a bounded DSP loop entrance."""
from __future__ import annotations
import re
from capstone import arm as A
from memory_spans import ACCESS, ASSIGN, affine

DECL = re.compile(r'uint32_t (\w+)\s*=\s*([^;]+);')
POINTER = re.compile(r'M8_FAST_POINTER\(&ram,\s*([^;\n]+)\)')


def specialize(function, bodies, source):
    """Clone the delay loop after qualifying R4 and SP once.

    These registers do not change in the cloned region. Calls and computed
    branches leave it. Indexed ring and output accesses keep their full checks.
    A failed entrance check takes the unchanged generated path before effects.
    """
    if function.entry != 0xcb04 or 0xd2f6 not in function.units:
        return source
    pending = [0xd2f6]
    selected = set()
    bounds = {}
    while pending:
        pc = pending.pop()
        if pc in selected or pc == 0xd7d2:
            continue
        unit = function.units.get(pc)
        if unit is None or any(body is None for body in bodies[pc]):
            continue
        text = '\n'.join(line for body in bodies[pc] for line in body)
        last = unit.instructions[-1]
        if last.id in (A.ARM_INS_BL, A.ARM_INS_BLX, A.ARM_INS_BX,
                       A.ARM_INS_TBB, A.ARM_INS_TBH):
            continue
        if any(int(reg) in (4, 13, 15) for reg in ASSIGN.findall(text)):
            continue
        selected.add(pc)
        variables = {}
        for body in bodies[pc]:
            for line in body:
                declaration = DECL.search(line)
                if declaration:
                    variables[declaration[1]] = affine(declaration[2], variables)
                access = ACCESS.fullmatch(line)
                if access:
                    value = affine(access[2], variables)
                    if value is not None and value[0] in (4, 13):
                        base, offset = value
                        width = int(access[3])
                        lo, hi = bounds.get(base, (offset, offset + width))
                        bounds[base] = min(lo, offset), max(hi, offset + width)
        if last.id in (A.ARM_INS_B, A.ARM_INS_CBZ, A.ARM_INS_CBNZ):
            pending.append(last.operands[-1].imm & ~1)
            if last.id != A.ARM_INS_B or last.cc not in (A.ARM_CC_AL, A.ARM_CC_INVALID):
                pending.append(unit.next_pc)
        else:
            pending.append(unit.next_pc)
    if not bounds or any(lo < 0 or hi > 0x10000 for lo, hi in bounds.values()):
        return source
    labels = list(re.finditer(r'^L_([0-9a-f]+):\n', source, re.M))
    chunks = {}
    for index, match in enumerate(labels):
        pc = int(match[1], 16)
        if pc in selected:
            end = labels[index + 1].start() if index + 1 < len(labels) else source.index('\nredirect:', match.end())
            chunks[pc] = source[match.start():end]
    rewritten = 0
    clones = []
    for pc in sorted(chunks):
        variables = {}
        lines = []
        for line in chunks[pc].splitlines():
            declaration = DECL.search(line)
            if declaration:
                variables[declaration[1]] = affine(declaration[2], variables)
            def replace(match):
                nonlocal rewritten
                parts = match[1].rsplit(',', 1)
                if len(parts) != 2:
                    return match[0]
                value = affine(parts[0].strip(), variables)
                width = affine(parts[1].strip(), {})
                if value is None or width is None or value[0] not in bounds or width[0] is not None:
                    return match[0]
                base, offset = value
                lo, hi = bounds[base]
                if offset < lo or offset + width[1] > hi:
                    return match[0]
                rewritten += 1
                return f'(loop_host_{base}+{offset-lo}u)'
            lines.append(POINTER.sub(replace, line))
        chunk = '\n'.join(lines) + '\n'
        chunk = re.sub(r'\bL_([0-9a-f]+)\b',
                       lambda m: 'L_loop_'+m[1] if int(m[1], 16) in selected else m[0], chunk)
        clones.append(chunk)
    if not rewritten:
        return source
    condition = '#if defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && !defined(M8_AOT_DISABLE_LOOP_SPANS)'
    locals_ = [condition]
    guard = [condition, '{']
    for base, (lo, hi) in sorted(bounds.items()):
        locals_.append(f'uint8_t *loop_host_{base};')
        guard += [f'loop_host_{base}=NULL;',
                  f'if(cpu->r[{base}]<=UINT32_MAX-{hi}u)',
                  f'loop_host_{base}=M8_FAST_SPAN_POINTER(mem,cpu->r[{base}]+{lo}u,{hi-lo}u);']
    locals_.append('#endif')
    guard += ['if('+' && '.join(f'loop_host_{base}' for base in sorted(bounds))+') goto L_loop_d2f6;', '}', '#endif']
    source = source.replace('goto dispatch;', '\n'.join(locals_)+'\ngoto dispatch;', 1)
    # Place the clone before redirect, and enter it only after the span checks.
    source = source.replace('L_d2f6:\n', 'L_d2f6:\n'+'\n'.join(guard)+'\n', 1)
    return source.replace('\nredirect:', '\n'+condition+'\n'+''.join(clones)+'#endif\nredirect:', 1)
