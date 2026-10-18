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

AUTO_CONDITION=('#if defined(M8_AOT_FAST_MEMORY) && '
                '!defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)')
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
AUTO_MACRO=MACRO.replace(CONDITION,AUTO_CONDITION).replace('M8_POINTEE_POINTER','M8_AUTO_POINTEE_POINTER')


def apply(function,plan,source):
    condition=CONDITION
    pointer_macro='M8_POINTEE_POINTER'
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
                return f'{pointer_macro}({prefix}_host_{base},{offset-lo}u,{match[0]})'
            rewritten_line=region_spans.POINTER.sub(replace,line)
            rows.append(rewritten_line)
        rewritten_chunk='\n'.join(rows)+'\n'
        source=source.replace(chunk,rewritten_chunk,1)
    if rewritten<8:return original
    guard_open='{'
    declarations=[condition];guard=[condition,guard_open]
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
    clear='\n'.join([condition,*[f'{prefix}_host_{base}=NULL;' for base in sorted(plan.bounds)],'#endif'])+'\n'
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


def auto_candidates(function,bodies,max_units=160,min_accesses=8,max_candidates=32):
    """Rank a small set of loop heads and repeated-base definition exits.

    The linear score only limits discovery cost. The control-flow analyzer
    proves each selected region separately before the source can change.
    """
    from bisect import bisect_left
    pcs=sorted(function.units)
    positions={pc:index for index,pc in enumerate(pcs)}
    uses={};writes={};barriers=[];heads=set()
    for index,pc in enumerate(pcs):
        unit=function.units[pc];rows=bodies[pc]
        if any(body is None for body in rows) or any(insn.id in region_spans.CALLS for insn in unit.instructions):
            barriers.append(index)
        text='\n'.join(line for body in rows if body for line in body)
        for base in {int(n) for n in region_spans.ASSIGN.findall(text)}:
            writes.setdefault(base,[]).append(index)
        for target in region_spans.successors(unit):
            if target<=pc and target in positions:heads.add(target)
        for body in rows:
            variables={}
            for line in body or ():
                declaration=region_spans.DECL.fullmatch(line)
                if declaration:variables[declaration[1]]=affine(declaration[2],variables)
                access=region_spans.ACCESS.fullmatch(line)
                if access is None:continue
                value=affine(access[2],variables)
                if value is not None and value[0] is not None and value[0]<15:
                    uses.setdefault(value[0],[]).append(index)
    candidates=[]
    for base,accesses in uses.items():
        if len(accesses)<min_accesses:continue
        entrances=heads|{function.entry}
        entrances |= {function.units[pcs[index]].next_pc for index in writes.get(base,())}
        stops=sorted(set(barriers+writes.get(base,[])+writes.get(15,[])))
        for entry in entrances:
            index=positions.get(entry)
            if index is None:continue
            stop_index=bisect_left(stops,index)
            end=min(index+max_units,stops[stop_index] if stop_index<len(stops) else len(pcs))
            count=bisect_left(accesses,end)-bisect_left(accesses,index)
            if count>=min_accesses:candidates.append((entry,base,count,entry in heads))
    candidates.sort(key=lambda row:(not row[3],-row[2],row[0],row[1]))
    return [(entry,base) for entry,base,_,_ in candidates[:max_candidates]]


def auto_plans(function,bodies,max_units=160,min_accesses=8,max_candidates=32):
    """Return bounded single-base proofs, without applying source changes."""
    result=[]
    for entry,base in auto_candidates(function,bodies,max_units,min_accesses,max_candidates):
        plan=region_spans.analyze(function,bodies,entry,(base,),
                                  max_units=max_units,min_accesses=min_accesses)
        if plan is not None:result.append(plan)
    return sorted(result,key=lambda plan:(-plan.accesses,len(plan.units),plan.entry,sorted(plan.bounds)))


def auto_source_safe(function,plan,source,bodies,*,late=False):
    """Reject inserted calls, services and existing caches before effects."""
    labels=list(re.finditer(r'^L_([0-9a-f]+):\n',source,re.M))
    chunks={}
    for index,match in enumerate(labels):
        pc=int(match[1],16)
        if pc not in plan.units:continue
        end=labels[index+1].start() if index+1<len(labels) else source.find('\nredirect:',match.end())
        if end<0:return False
        chunks[pc]=source[match.start():end]
    if set(chunks)!=plan.units:return False
    # These inserted operations are absent from the guest ISA summaries.
    # In particular, a service callback can change CPU or mapping state.
    unknown=re.compile(r'\b(?:fn_[0-9a-f]+|m8_tail|m8_\w+_kernel|M8_(?:KERNEL_)?(?:SAVE|LOAD)|'
                       r'm8_\w*(?:cooperative|service)\w*|M8_(?:AUTO_)?POINTEE_POINTER)\s*\(')
    checked={pc:(chunk.replace('M8_SAVE(cpu); m8_it_begin(&tx,cpu);','m8_it_begin(&tx,cpu);') if late else chunk)
             for pc,chunk in chunks.items()}
    if any(unknown.search(chunk) for chunk in checked.values()):return False
    calls=re.compile(r'\b([A-Za-z_]\w*)\s*\(')
    known={'if','for','while','switch','sizeof','defined','UINT32_C','INT64_C',
           'M8_COUNT_BEGIN','M8_COUNT_ONE','M8_COUNT_REWIND',
           'M8_FAST_POINTER','M8_FAST_SPAN_POINTER','m8_pointer','m8_span_pointer',
           'm8_le_read','m8_le_write','m8_write_plain','m8_write_it',
           'm8_it_begin','m8_it_commit','m8_it_abort','m8_condition'}
    if late:
        known.update({'m8_canonical_offset_valid','m8_aot_guard_offset_data_valid',
                      'm8_aot_guard_data_pointer','m8_aot_guard_offset_data_pointer',
                      'm8_aot_guard_write','m8_aot_guard_write_plain',
                      'm8_flags_nz','m8_flags_logic','m8_flags_add','m8_flags_sub','m8_flags_condition',
                      'M8_FLAGS_SET','M8_FLAGS_GET','m8_fp_float','m8_fp_bits32',
                      'm8_fp_double','m8_fp_bits64'})
    for pc in plan.units:
        for body in bodies[pc]:
            if body:known.update(calls.findall('\n'.join(body)))
    # Only the original instruction helpers and pure memory/IT expansion
    # helpers have a known contract here. Reject any other inserted call.
    exits={target for pc in plan.units for target in region_spans.successors(function.units[pc])}
    for chunk in checked.values():
        if any(int(base) in plan.bounds for base in region_spans.ASSIGN.findall(chunk)):return False
        for target in re.findall(r'\bgoto\s+(L_\w+)',chunk):
            # Older clone paths have separate edges and are not in this proof.
            match=re.fullmatch(r'L_([0-9a-f]+)',target)
            if match is None or int(match[1],16) not in plan.units|exits:return False
        text=re.sub(r'/\*.*?\*/|//[^\n]*','',chunk,flags=re.S)
        if set(calls.findall(text))-known:return False
    return True


LATE_ACCESS=re.compile(
    r'uint32_t access_addr=\(uint32_t\)\((?P<address>[^;\n]+)\);\n'
    r'(?P<guard>if\(!(?P<valid>m8_canonical_offset_valid|m8_aot_guard_offset_data_valid)'
    r'\(access_addr,(?P<width>[124])\)\) \{cpu->r\[15\]=0x[0-9a-f]+u; goto memory_fallback;\})\n'
    r'uint8_t \*access_host=(?P<pointer>\(uint8_t \*\)\(\(uintptr_t\)ram.itcm.host\+\(uintptr_t\)access_addr\));')


def apply_auto_late(function,plan,source):
    """Reuse a proven span beside expanded canonical scalar guards.

    This accepts shared-register source before entry splitting. Cache misses
    execute the original validity predicate and pointer expression. A guarded
    function that contains stores admits every cache through data-only bounds.
    """
    prefix=f'pointee_{plan.entry:x}'
    if prefix+'_host_' in source:return source,0
    labels=list(re.finditer(r'^L_([0-9a-f]+):\n',source,re.M));chunks={}
    for index,match in enumerate(labels):
        pc=int(match[1],16)
        if pc not in plan.units:continue
        end=labels[index+1].start() if index+1<len(labels) else source.find('\nredirect:',match.end())
        if end<0:return source,0
        chunks[pc]=source[match.start():end]
    if set(chunks)!=plan.units or source.count('goto dispatch;')!=1:return source,0
    original=source;rewritten=0
    for pc,chunk in chunks.items():
        variables={};rows=chunk.splitlines(keepends=True);output=[];index=0
        while index<len(rows):
            line=rows[index]
            match=LATE_ACCESS.fullmatch(''.join(rows[index:index+3]).rstrip('\n'))
            if match is not None:
                value=affine(match['address'],variables)
                if value is not None and value[0] in plan.bounds:
                    base,offset=value;offset &= 0xffffffff
                    if offset>=0x80000000:offset-=0x100000000
                    low,high=plan.bounds[base];width=int(match['width'])
                    if low<=offset and offset+width<=high:
                        host=f'{prefix}_host_{base}'
                        guard=match['guard'].replace('if(!',f'if(!{host} && !',1)
                        pointer=f'M8_AUTO_POINTEE_POINTER({host},{offset-low}u,{match["pointer"]})'
                        output += [line,AUTO_CONDITION+'\n',guard+'\n',
                                   f'uint8_t *access_host={pointer};\n','#else\n',
                                   rows[index+1],rows[index+2],'#endif\n']
                        rewritten+=1;index+=3;continue
            declaration=region_spans.DECL.search(line)
            if declaration:variables[declaration[1]]=affine(declaration[2],variables)
            output.append(line);index+=1
        source=source.replace(chunk,''.join(output),1)
    if rewritten<8:return original,0
    stores=any(re.search(r'\bm8_(?:le_write|write_it)\s*\(',chunk) for chunk in chunks.values())
    guarded=bool(re.search(r'\bm8_aot_guard_\w+',original))
    helper='m8_aot_guard_data_pointer' if stores and guarded else 'M8_FAST_SPAN_POINTER'
    declarations=[AUTO_CONDITION];guard=[AUTO_CONDITION,
        'if('+' || '.join(f'!{prefix}_host_{base}' for base in sorted(plan.bounds))+') {']
    for base,(low,high) in sorted(plan.bounds.items()):
        host=f'{prefix}_host_{base}';start=f'{prefix}_start_{base}';end=f'{prefix}_end_{base}'
        declarations.append(f'uint8_t *{host}=NULL;')
        guard += [f'{host}=NULL;',f'int64_t {start}=(int64_t)(uint64_t)cpu->r[{base}]+({low});',
                  f'int64_t {end}=(int64_t)(uint64_t)cpu->r[{base}]+({high});',
                  f'if({start}>=0 && {end}<=INT64_C(0x100000000))',
                  f'{host}={helper}(&ram,(uint32_t){start},{high-low}u);']
    declarations.append('#endif');guard += ['}','#endif']
    source=source.replace('goto dispatch;','\n'.join(declarations)+'\ngoto dispatch;',1)
    source=source.replace(f'L_{plan.entry:x}:\n',f'L_{plan.entry:x}:\n'+'\n'.join(guard)+'\n',1)
    outgoing={target for pc in plan.units for target in region_spans.successors(function.units[pc])
              if target not in plan.units}
    clear='\n'.join([AUTO_CONDITION,*[f'{prefix}_host_{base}=NULL;' for base in sorted(plan.bounds)],'#endif'])+'\n'
    for target in sorted(outgoing):
        source=source.replace(f'L_{target:x}:\n',f'L_{target:x}:\n'+clear,1)
    return source,rewritten


def specialize_auto_late(function,bodies,source,*,max_units=160,min_accesses=8,max_plans=4,stats=None):
    """Apply automatic caches to known expanded canonical release accesses."""
    if max_units<=0 or min_accesses<8 or max_plans<0:raise ValueError('Invalid automatic span limits')
    if not max_plans or '#define M8_AUTO_POINTEE_POINTER' in source:return source
    selected=set();applied=0;accesses=0
    for plan in auto_plans(function,bodies,max_units,min_accesses):
        if plan.units & selected or not auto_source_safe(function,plan,source,bodies,late=True):continue
        candidate,count=apply_auto_late(function,plan,source)
        if not count:continue
        source=candidate;selected.update(plan.units);applied+=1;accesses+=count
        if applied>=max_plans:break
    if applied:source=AUTO_MACRO+source
    if stats is not None:stats.update(plans=applied,canonical_guards=accesses)
    return source
