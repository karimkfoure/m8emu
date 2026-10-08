"""Qualify affine memory spans in a bounded call-free control-flow region.

The caller selects an entrance and candidate bases. The analysis stops before
any base write, call, unsupported instruction or computed exit. A failed span
check enters the original scalar code before effects. Qualified clones keep
instruction order and every check for indexed or unknown memory accesses.
"""
from __future__ import annotations
from dataclasses import dataclass
import re
from capstone import arm as A
from memory_spans import ACCESS, ASSIGN, affine

DECL=re.compile(r'uint32_t (\w+)\s*=\s*([^;]+);')
POINTER=re.compile(r'M8_FAST_POINTER\(&ram,\s*([^;\n]+)\)')
CONDITION=('#if defined(M8_AOT_REGION_SPANS) && defined(M8_AOT_FAST_MEMORY) && '
           '!defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)')
# This entrance follows the port-pointer load. R4 and R5 stay unchanged until
# the next callback or base definition, including both captured steady paths.
DEPLOYMENTS={0x60017d9c:((0x60017f58,(4,5)),),
             0x168e8:((0x169b4,(6,),(0x16a74,0x17418)),),
             # Stop before mode dispatch instead of cloning cold callback
             # paths and retaining an additional stack pointer guard.
             0x1991c:((0x199c8,(4,),(0x19a4c,)),)}
CALLS=(A.ARM_INS_BL,A.ARM_INS_BLX,A.ARM_INS_BX,A.ARM_INS_TBB,A.ARM_INS_TBH)


def successors(unit):
    last=unit.instructions[-1]
    if last.id in CALLS:return ()
    if last.id in (A.ARM_INS_B,A.ARM_INS_CBZ,A.ARM_INS_CBNZ):
        target=last.operands[-1].imm & ~1
        if last.id==A.ARM_INS_B and last.cc in (A.ARM_CC_AL,A.ARM_CC_INVALID):return (target,)
        return target,unit.next_pc
    return (unit.next_pc,)


@dataclass
class Plan:
    entry: int
    units: set
    bounds: dict
    accesses: int

    def stats(self):
        return dict(entry=self.entry,units=len(self.units),accesses=self.accesses,
                    spans=[dict(base=base,low=lo,high=hi) for base,(lo,hi) in sorted(self.bounds.items())])


def analyze(function,bodies,entry,bases,min_accesses=8,max_units=160,stops=()):
    """Prove each selected base unchanged on every path of the clone."""
    bases=set(bases)
    if not bases or not bases<=set(range(15)):return None
    stops=set(stops)
    pending=[entry];selected=set();uses={}
    while pending:
        pc=pending.pop()
        if pc in selected or pc in stops:continue
        unit=function.units.get(pc)
        if unit is None or any(body is None for body in bodies[pc]):continue
        if any(insn.id in CALLS for insn in unit.instructions):continue
        text='\n'.join(line for body in bodies[pc] for line in body)
        # PC writes include translated returns and external direct branches.
        if any(int(reg) in bases|{15} for reg in ASSIGN.findall(text)):continue
        selected.add(pc)
        if len(selected)>max_units:return None
        for body in bodies[pc]:
            variables={}
            for line in body:
                declaration=DECL.fullmatch(line)
                if declaration:variables[declaration[1]]=affine(declaration[2],variables)
                access=ACCESS.fullmatch(line)
                if access is None:continue
                value=affine(access[2],variables)
                if value is None or value[0] not in bases:continue
                base,offset=value;offset &= 0xffffffff
                if offset>=0x80000000:offset-=0x100000000
                uses.setdefault(base,[]).append((offset,offset+int(access[3])))
        pending.extend(successors(unit))
    if entry not in selected:return None
    bounds={base:(min(lo for lo,_ in spans),max(hi for _,hi in spans))
            for base,spans in uses.items() if len(spans)>=2}
    if not bounds or any(hi-lo>0xffffffff for lo,hi in bounds.values()):return None
    count=sum(len(uses[base]) for base in bounds)
    return Plan(entry,selected,bounds,count) if count>=min_accesses else None


def apply(plan,source):
    """Clone qualified pointer paths without changing the fallback blocks."""
    if f'L_region_{plan.entry:x}_{plan.entry:x}:\n' in source:return source
    labels=list(re.finditer(r'^L_([0-9a-f]+):\n',source,re.M));chunks={}
    for index,match in enumerate(labels):
        pc=int(match[1],16)
        if pc not in plan.units:continue
        end=labels[index+1].start() if index+1<len(labels) else source.index('\nredirect:',match.end())
        chunks[pc]=source[match.start():end]
    if set(chunks)!=plan.units:return source
    # Codegen can insert bounded native kernels around an original opcode.
    # Their effects are not in the instruction summaries, so do not reuse a
    # span across an inserted native call, even when the original unit is safe.
    if any(re.search(r'\b(?:fn_[0-9a-f]+|m8_tail|m8_\w+_kernel)\s*\(',chunk)
           for chunk in chunks.values()):return source
    clones=[];rewritten=0;prefix=f'region_{plan.entry:x}'
    for pc in sorted(chunks):
        variables={};lines=[]
        for line in chunks[pc].splitlines():
            declaration=DECL.search(line)
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
                return f'({prefix}_host_{base}+{offset-lo}u)'
            lines.append(POINTER.sub(replace,line))
        chunk='\n'.join(lines)+'\n'
        chunk=re.sub(r'\bL_([0-9a-f]+)\b',
                     lambda m:f'L_{prefix}_'+m[1] if int(m[1],16) in plan.units else m[0],chunk)
        clones.append(chunk)
    if rewritten<8:return source
    declarations=[CONDITION]
    guard=[CONDITION,'{']
    for base,(lo,hi) in sorted(plan.bounds.items()):
        name=f'{prefix}_host_{base}'
        declarations.append(f'uint8_t *{name};')
        guard += [f'{name}=NULL;',
                  f'int64_t {prefix}_start_{base}=(int64_t)(uint64_t)cpu->r[{base}]+({lo});',
                  f'int64_t {prefix}_end_{base}=(int64_t)(uint64_t)cpu->r[{base}]+({hi});',
                  f'if({prefix}_start_{base}>=0 && {prefix}_end_{base}<=INT64_C(0x100000000))',
                  f'{name}=M8_FAST_SPAN_POINTER(&ram,(uint32_t){prefix}_start_{base},{hi-lo}u);']
    declarations.append('#endif')
    guard += ['if('+' && '.join(f'{prefix}_host_{base}' for base in sorted(plan.bounds))+') '
              f'goto L_{prefix}_{plan.entry:x};','}','#endif']
    source=source.replace('goto dispatch;','\n'.join(declarations)+'\ngoto dispatch;',1)
    source=source.replace(f'L_{plan.entry:x}:\n',f'L_{plan.entry:x}:\n'+'\n'.join(guard)+'\n',1)
    return source.replace('\nredirect:','\n'+CONDITION+'\n'+''.join(clones)+'#endif\nredirect:',1)


def specialize(function,bodies,source,starts=None):
    """Apply an explicit bounded deployment; release activation is optional."""
    for deployment in DEPLOYMENTS.get(function.entry,()) if starts is None else starts:
        entry,bases,*rest=deployment
        plan=analyze(function,bodies,entry,bases,stops=rest[0] if rest else ())
        if plan is not None:source=apply(plan,source)
    return source
