"""Reuse bounded pointee spans without cloning the scalar control flow.

Each entrance qualifies spans through bases that stay unchanged in its region.
Failed qualifications retain each original access and its partial-fault prefix.
Clear cached hosts at every outgoing boundary, so another entrance cannot use
a pointer qualified with an earlier base value. Indexed accesses keep checks.
"""
from __future__ import annotations
import re
import region_spans
from memory_spans import affine

CONDITION=('#if defined(M8_AOT_POINTEE_SPANS) && defined(M8_AOT_FAST_MEMORY) && '
           '!defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)')
DEPLOYMENTS={0x168e8:((0x16a90,(5,7),(0x16c30,)),
                     (0x16c30,(4,5),(0x16cd8,)),
                     (0x16cdc,(3,4,5),(0x16e4c,)))}
MACRO='''/* Bounded pointee spans retain the scalar access on rejection. */
%s
#define M8_POINTEE_POINTER(host,offset,fallback) ((host)?(host)+(offset):(fallback))
#else
#define M8_POINTEE_POINTER(host,offset,fallback) (fallback)
#endif
''' % CONDITION


def apply(function,plan,source):
    original=source
    prefix=f'pointee_{plan.entry:x}'
    if prefix+'_host_' in source:return source
    labels=list(re.finditer(r'^L_([0-9a-f]+):\n',source,re.M));chunks={}
    for index,match in enumerate(labels):
        pc=int(match[1],16)
        if pc in plan.units:
            end=labels[index+1].start() if index+1<len(labels) else source.index('\nredirect:',match.end())
            chunks[pc]=source[match.start():end]
    if set(chunks)!=plan.units:return source
    # Native insertions can change bases without an original ISA definition.
    if any(re.search(r'\b(?:fn_[0-9a-f]+|m8_tail|m8_\w+_kernel|M8_KERNEL_SAVE|M8_KERNEL_LOAD)\s*\(',chunk)
           for chunk in chunks.values()):return source
    rewritten=0
    for pc,chunk in chunks.items():
        variables={};rows=[]
        for line in chunk.splitlines():
            declaration=region_spans.DECL.search(line)
            if declaration:variables[declaration[1]]=affine(declaration[2],variables)
            def replace(match):
                nonlocal rewritten
                parts=match[1].rsplit(',',1)
                if len(parts)!=2:return match[0]
                value=affine(parts[0].strip(),variables);width=affine(parts[1].strip(),{})
                if value is None or width is None or value[0] not in plan.bounds or width[0] is not None:return match[0]
                base,offset=value;offset &= 0xffffffff
                if offset>=0x80000000:offset-=0x100000000
                lo,hi=plan.bounds[base]
                if width[1]<=0 or offset<lo or offset+width[1]>hi:return match[0]
                rewritten+=1
                return f'M8_POINTEE_POINTER({prefix}_host_{base},{offset-lo}u,{match[0]})'
            rows.append(region_spans.POINTER.sub(replace,line))
        source=source.replace(chunk,'\n'.join(rows)+'\n',1)
    if rewritten<8:return original
    declarations=[CONDITION];guard=[CONDITION,'{']
    for base,(lo,hi) in sorted(plan.bounds.items()):
        name=f'{prefix}_host_{base}'
        declarations.append(f'uint8_t *{name}=NULL;')
        guard += [f'{name}=NULL;',
                  f'int64_t {prefix}_start_{base}=(int64_t)(uint64_t)cpu->r[{base}]+({lo});',
                  f'int64_t {prefix}_end_{base}=(int64_t)(uint64_t)cpu->r[{base}]+({hi});',
                  f'if({prefix}_start_{base}>=0 && {prefix}_end_{base}<=INT64_C(0x100000000))',
                  f'{name}=M8_FAST_SPAN_POINTER(&ram,(uint32_t){prefix}_start_{base},{hi-lo}u);']
    declarations.append('#endif');guard += ['}','#endif']
    source=source.replace('goto dispatch;','\n'.join(declarations)+'\ngoto dispatch;',1)
    source=source.replace(f'L_{plan.entry:x}:\n',f'L_{plan.entry:x}:\n'+'\n'.join(guard)+'\n',1)
    outgoing={target for pc in plan.units for target in region_spans.successors(function.units[pc])
              if target not in plan.units}
    clear='\n'.join([CONDITION,*[f'{prefix}_host_{base}=NULL;' for base in sorted(plan.bounds)],'#endif'])+'\n'
    for target in sorted(outgoing):
        source=source.replace(f'L_{target:x}:\n',f'L_{target:x}:\n'+clear,1)
    return source


def specialize(function,bodies,source,starts=None,strategy='cached'):
    """Apply explicit call-free deployments; activation stays default off."""
    if strategy not in ('cached','clone'):raise ValueError(strategy)
    for entry,bases,stops in DEPLOYMENTS.get(function.entry,()) if starts is None else starts:
        plan=region_spans.analyze(function,bodies,entry,bases,stops=stops)
        if plan is not None:
            source=(apply(function,plan,source) if strategy=='cached' else region_spans.apply(plan,source))
    if strategy=='cached' and 'M8_POINTEE_POINTER(' in source and '#define M8_POINTEE_POINTER' not in source:
        source=MACRO+source
    return source
