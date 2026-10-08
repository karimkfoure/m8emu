#!/usr/bin/env python3
"""Translate bounded Thumb functions to C11. Unknown operations remain in Unicorn."""
from __future__ import annotations
import argparse
from dataclasses import dataclass
import json
import hashlib
import re
from pathlib import Path
import capstone as cs
from capstone import arm as A

HERE = Path(__file__).resolve().parent
DEFAULT_FIRMWARE = HERE.parent / 'build/firmware/M8_V6_5_2C_HEADLESS.bin'
ITCM_OFFSET = 0x1809E4
ITCM_SIZE = 0x3D5D8

# A public sequencer hook returns to this finite original wrapper. Continue
# within the same FP environment, then exit before its unknown service call.
HOOK_CONTINUATIONS = {0x25510: (0x23b76, 0x23b50)}

# Bounded kernels reject before effects and touch only these state fields.
# Keep their bridge independent of ordinary transitive callee summaries.
KERNEL_REGISTERS = {
    'm8_envelope_left': (
        {('r',i) for i in (0,1,4,7,12,15)} | {('s',12)},
        {('r',i) for i in (0,1,12,15)} | {('s',i) for i in (11,13,15)}),
    'm8_envelope_right': (
        {('r',i) for i in (0,1,4,5,12,15)} | {('s',12)},
        {('r',i) for i in (0,1,12,15)} | {('s',i) for i in (11,13,15)}),
    'm8_meter_scale': (
        {('r',i) for i in (1,2,3,6,15)} | {('s',i) for i in (16,17)},
        {('r',i) for i in (2,3,15)} | {('s',i) for i in (14,15,16,17)}),
    'm8_meter_add': (
        {('r',i) for i in (2,3,4,5,6,7,15)} | {('s',i) for i in (12,13)},
        {('r',i) for i in (2,4,5,7,15)} | {('s',i) for i in range(10,16)}),
    'm8_fm_pitch_kernel': (
        {('r',i) for i in (4,5,14,15)} | {('s',i) for i in (14,15)},
        {('r',i) for i in (1,2,3,14,15)} | {('s',i) for i in (0,*range(7,18))}),
    'm8_effect_phase_kernel': (
        {('r',i) for i in (4,5,8,15)} | {('s',i) for i in (10,12,13,14,15,16,18)},
        {('r',i) for i in (0,1,2,3,11,12,14,15)} | {('s',i) for i in range(6,16)}),
    'm8_fm_batch_kernel': (
        {('r',i) for i in (1,2,5,6,8,9,10,11,13,15)} |
        {('s',i) for i in (14,15,17,18,19,20,21,22,25)},
        {('r',i) for i in (0,1,2,3,4,6,7,14,15)} |
        {('s',i) for i in (*range(17),23,24)}),
    'm8_stereo_mix_kernel': (
        {('r',i) for i in (1,2,3,4,5,6,15)},
        {('r',i) for i in (1,2,4,5,15)} | {('s',i) for i in (11,14,15)}),
    'm8_fm_modulation_tail_kernel': (
        {('r',i) for i in (4,13,15)},
        {('r',i) for i in (*range(8),12,13,14,15)} | {('s',16),('s',17)}),
    'm8_braids_filter_kernel': (
        {('r',i) for i in (3,5,6,7,8,9,10,13,15)} | {('s',i) for i in range(17,22)},
        {('r',i) for i in (0,1,2,3,4,6,7,8,14,15)} |
        {('s',i) for i in (0,1,22)} | {('s',i) for i in range(4,17)}),
    'm8_braids_mode0_kernel': (
        {('r',i) for i in (3,5,6,7,8,9,10,13,14,15)} |
        {('s',i) for i in range(17,22)},
        {('r',i) for i in (0,1,2,3,4,6,7,8,14,15)} |
        {('s',i) for i in (0,1,22,*range(5,17))}),
}

class Firmware:
    def __init__(self, path):
        path = Path(path)
        self.data = self.read_hex(path) if path.suffix.lower() == '.hex' else path.read_bytes()
        self.md = cs.Cs(cs.CS_ARCH_ARM, cs.CS_MODE_THUMB | cs.CS_MODE_MCLASS)
        self.md.detail = True
    @staticmethod
    def read_hex(path):
        """Load flash records while checking lengths, checksums, and overlaps."""
        base = 0
        records = []
        ended = False
        for line_no, text in enumerate(path.read_text().splitlines(), 1):
            if not text:
                continue
            if ended or not text.startswith(':'):
                raise ValueError(f'Invalid HEX record at line {line_no}')
            record = bytes.fromhex(text[1:])
            if len(record) < 5 or len(record) != record[0] + 5 or sum(record) & 255:
                raise ValueError(f'Invalid HEX length/checksum at line {line_no}')
            address = int.from_bytes(record[1:3], 'big')
            kind, payload = record[3], record[4:-1]
            if kind == 0:
                offset = base + address - 0x60000000
                if offset < 0 or offset + len(payload) > 8 * 1024 * 1024:
                    raise ValueError(f'HEX record outside Teensy flash at line {line_no}')
                records.append((offset, payload))
            elif kind == 1 and not payload:
                ended = True
            elif kind == 4 and len(payload) == 2:
                base = int.from_bytes(payload, 'big') << 16
            elif kind == 5 and len(payload) == 4:
                pass  # Firmware starts from the IVT, not this metadata.
            else:
                raise ValueError(f'Unsupported HEX record at line {line_no}')
        if not ended or not records:
            raise ValueError('HEX firmware is incomplete')
        data = bytearray(max(offset + len(payload) for offset, payload in records))
        previous_end = 0
        for offset, payload in sorted(records):
            if offset < previous_end:
                raise ValueError('HEX firmware contains overlapping records')
            data[offset:offset + len(payload)] = payload
            previous_end = offset + len(payload)
        return bytes(data)
    def code(self, pc, size=4):
        off = ITCM_OFFSET + pc if pc < ITCM_SIZE else pc - 0x60000000
        if off < 0 or off + size > len(self.data):
            return b''
        return self.data[off:off + size]
    def decode(self, pc):
        return next(self.md.disasm(self.code(pc), pc, count=1), None)

class Context:
    def __init__(self, insn):
        self.insn = insn
        # IDs avoid condition suffixes supplied by Capstone for an IT member.
        self.name = insn.insn_name()
        if self.name.endswith('.w') or self.name.endswith('.n'):
            self.name = self.name[:-2]
    def regno(self, reg):
        name = self.insn.reg_name(reg)
        aliases = {'sp':13,'lr':14,'pc':15,'ip':12,'fp':11,'sl':10,'sb':9}
        if name in aliases:
            return aliases[name]
        if name.startswith('r') and name[1:].isdigit():
            return int(name[1:])
        raise ValueError(f'not an ARM register: {name}')
    def r(self, reg):
        n = self.regno(reg)
        return f'0x{self.insn.address + 4:x}u' if n == 15 else f'cpu->r[{n}]'
    def set(self, reg, expr):
        n = self.regno(reg)
        if n == 15:
            return [f'cpu->r[15] = ({expr}) & ~1u;', 'goto redirect;']
        return [f'cpu->r[{n}] = (uint32_t)({expr});']
    def op(self, op):
        if op.type == A.ARM_OP_IMM:
            return f'0x{op.imm & 0xffffffff:x}u'
        if op.type != A.ARM_OP_REG:
            raise ValueError('expected register or immediate')
        expr = self.r(op.reg)
        if op.shift.type:
            amount = self.r(op.shift.value) if op.shift.type >= A.ARM_SFT_ASR_REG else str(op.shift.value)
            expr = f'm8_shift({expr}, {op.shift.type}, {amount}, NULL)'
        return expr

@dataclass
class Unit:
    pc: int
    instructions: list
    next_pc: int
    it: bool = False
    table_targets: list | None = None

class Function:
    def __init__(self, fw, entry, end):
        self.fw, self.entry, self.end = fw, entry, end
        self.units = {}
        self.calls = set()
        self.resumes = {entry}
        self.barriers = {}
        self.discover()
    def table(self, insn):
        index = insn.operands[0].mem.index
        bound = None
        # GCC emits CMP index,#n followed by BHI before a table branch.
        for unit in sorted(self.units.values(), key=lambda u:u.pc, reverse=True):
            if not insn.address - 32 <= unit.pc < insn.address:
                continue
            for prior in reversed(unit.instructions):
                if prior.id == A.ARM_INS_CMP and len(prior.operands)==2:
                    a,b=prior.operands
                    if a.type==A.ARM_OP_REG and a.reg==index and b.type==A.ARM_OP_IMM:
                        bound=b.imm
                        break
            if bound is not None:
                break
        if bound is None or not 0 <= bound <= 255:
            return None
        mem = insn.operands[0].mem
        if mem.base != A.ARM_REG_PC:
            return None
        width = 2 if insn.id == A.ARM_INS_TBH else 1
        raw = self.fw.code(insn.address+4, width*(bound+1))
        return [insn.address+4+2*int.from_bytes(raw[i*width:(i+1)*width],'little') for i in range(bound+1)]
    def discover(self):
        todo=[self.entry]
        while todo:
            pc=todo.pop()
            if pc in self.units or not self.entry <= pc < self.end:
                continue
            insn=self.fw.decode(pc)
            if not insn:
                self.barriers[pc]='decode failed'
                continue
            unit=Unit(pc,[insn],pc+insn.size)
            if insn.id==A.ARM_INS_IT:
                mask=insn.bytes[0] & 15
                count=4-((mask & -mask).bit_length()-1)
                decoded=list(self.fw.md.disasm(self.fw.code(pc,20),pc,count=count+1))
                if len(decoded)!=count+1:
                    self.barriers[pc]='truncated IT block'
                    continue
                unit.instructions=decoded[1:]
                unit.next_pc=decoded[-1].address+decoded[-1].size
                unit.it=True
            self.units[pc]=unit
            last=unit.instructions[-1]
            if last.id in (A.ARM_INS_B,A.ARM_INS_CBZ,A.ARM_INS_CBNZ):
                target=last.operands[-1].imm & ~1
                todo.append(target)
                if last.id!=A.ARM_INS_B or last.cc not in (A.ARM_CC_AL,A.ARM_CC_INVALID):
                    todo.append(unit.next_pc)
            elif last.id==A.ARM_INS_BL:
                self.calls.add(last.operands[0].imm & ~1)
                self.resumes.add(unit.next_pc)
                todo.append(unit.next_pc)
            elif last.id in (A.ARM_INS_TBB,A.ARM_INS_TBH):
                unit.table_targets=self.table(last)
                if unit.table_targets:
                    todo.extend(unit.table_targets)
            elif last.id in (A.ARM_INS_BX,A.ARM_INS_BLX):
                if last.id==A.ARM_INS_BLX:
                    self.resumes.add(unit.next_pc)
                    todo.append(unit.next_pc)
                elif last.cc not in (A.ARM_CC_AL,A.ARM_CC_INVALID):
                    todo.append(unit.next_pc)
            elif any(o.type==A.ARM_OP_REG and o.reg==A.ARM_REG_PC for o in last.operands[:1]) or (last.id in (A.ARM_INS_POP,A.ARM_INS_LDM) and any(o.type==A.ARM_OP_REG and o.reg==A.ARM_REG_PC for o in last.operands)):
                if last.cc not in (A.ARM_CC_AL,A.ARM_CC_INVALID):
                    todo.append(unit.next_pc)
            else:
                todo.append(unit.next_pc)

class Translator:
    def __init__(self, fw, functions, hooks=None, entry_hooks=None, dead_nzcv=False, memory_spans=False, local_nzcv=False, it_preflight=False, call_dispatch=False, exact_entry_leaves=False):
        from emit_int import emit as integer
        from emit_fp import emit as floating
        self.integer,self.floating=integer,floating
        self.fw=fw
        self.hooks=hooks
        self.entry_hooks=entry_hooks
        self.dead_nzcv=dead_nzcv
        self.nzcv_stats={}
        self.memory_spans=memory_spans
        self.local_nzcv=local_nzcv
        self.it_preflight=it_preflight
        self.call_dispatch=call_dispatch
        self.exact_entry_leaves=exact_entry_leaves
        self.it_stats={}
        self.span_stats={}
        self.functions=[Function(fw,entry,end) for entry,end in functions]
        self.by_entry={f.entry:f for f in self.functions}
        self.safe={}
        for f in self.functions:
            for pc in f.units:
                if pc in self.safe:
                    raise ValueError(f'overlapping functions at {pc:#x}')
                self.safe[pc]=f.entry
        # Share the emitted instruction bodies with register footprints and
        # exact-leaf qualification. Keep original fn names until scalar calls
        # have their normal state bridges; continuation targets stay fn names.
        self.unit_bodies={f.entry:{pc:[self.instruction(f,insn) for insn in unit.instructions]
                                  for pc,unit in f.units.items()} for f in self.functions}
        # A known native callee needs only the register fields its emitted
        # instructions or descendants reference. Flags always cross calls.
        # Computed dispatch remains conservative and carries the complete CPU.
        all_registers={(kind,i) for kind,count in (('r',16),('s',32)) for i in range(count)}
        self.call_registers={};self.modified_registers={};dependencies={}
        for f in self.functions:
            registers={('r',15)};modified={('r',15)};children=set()
            for pc,unit in f.units.items():
                for insn,body in zip(unit.instructions,self.unit_bodies[f.entry][pc]):
                    if body is None: continue
                    source='\n'.join(body)
                    registers.update((k,int(n)) for k,n in re.findall(r'cpu->([rs])\[(\d+)\]',source))
                    modified.update((k,int(n)) for k,n in re.findall(r'cpu->([rs])\[(\d+)\]\s*(?:=|\+=|-=|\|=|&=|\^=)',source))
                    for n in re.findall(r'm8_fp_(?:get|set)64\(cpu,\s*(\d+)',source):
                        registers.update((('s',2*int(n)),('s',2*int(n)+1)))
                    for n in re.findall(r'm8_fp_set64\(cpu,\s*(\d+)',source):
                        modified.update((('s',2*int(n)),('s',2*int(n)+1)))
                    children.update(int(n,16) for n in re.findall(r'fn_([0-9a-f]+)\(cpu,mem\)',source))
                    if 'm8_tail(cpu,mem)' in source or 'M8_CALL(callee)' in source or 'goto redirect;' in source:
                        registers.update(all_registers)
                        modified.update(all_registers)
            self.call_registers[f'fn_{f.entry:x}']=registers
            self.modified_registers[f'fn_{f.entry:x}']=modified
            dependencies[f'fn_{f.entry:x}']={f'fn_{entry:x}' for entry in children}
        changed=True
        while changed:
            changed=False
            for name,children in dependencies.items():
                before=len(self.call_registers[name])
                for child in children:
                    self.call_registers[name].update(self.call_registers.get(child,all_registers))
                    previous=len(self.modified_registers[name])
                    self.modified_registers[name].update(self.modified_registers.get(child,all_registers))
                    changed |= previous!=len(self.modified_registers[name])
                changed |= before!=len(self.call_registers[name])
        if self.memory_spans:
            from memory_spans import base_writes
            self.span_base_writes=base_writes(self)
        self.exact_leaves={}
        if self.exact_entry_leaves:
            from exact_leaf import analyze
            self.exact_leaves={f.entry:leaf for f in self.functions
                               if (leaf:=analyze(f,self,self.unit_bodies[f.entry])) is not None}
    def jump(self, f, pc):
        if pc in f.units:
            return [f'goto L_{pc:x};']
        if pc in self.safe:
            return [f'cpu->r[15]=0x{pc:x}u;', f'return fn_{self.safe[pc]:x}(cpu,mem);']
        return [f'cpu->r[15]=0x{pc:x}u;', 'goto done;']
    def instruction(self,f,insn,suppress_nzcv=False):
        ctx=Context(insn)
        ctx.suppress_nzcv=suppress_nzcv
        ops=insn.operands
        if insn.id==A.ARM_INS_B:
            return self.jump(f,ops[0].imm & ~1)
        if insn.id in (A.ARM_INS_CBZ,A.ARM_INS_CBNZ):
            cond='==' if insn.id==A.ARM_INS_CBZ else '!='
            return [f'if ({ctx.r(ops[0].reg)} {cond} 0) {{']+self.jump(f,ops[1].imm & ~1)+['}']
        if insn.id==A.ARM_INS_BL:
            target=ops[0].imm & ~1
            if target not in self.by_entry:
                return None
            nextpc=insn.address+insn.size
            return [f'cpu->r[14]=0x{nextpc|1:x}u;',f'cpu->r[15]=0x{target:x}u;',f'int call_result=fn_{target:x}(cpu,mem);',f'if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x{nextpc:x}u) return call_result;']
        if insn.id==A.ARM_INS_BLX and ops[0].type==A.ARM_OP_REG:
            nextpc=insn.address+insn.size
            if self.call_dispatch:
                from call_dispatch import blx_body
                return blx_body(ctx.r(ops[0].reg),nextpc)
            return [f'uint32_t raw_target=({ctx.r(ops[0].reg)});',
                    'uint32_t target=raw_target & ~1u;',
                    'if (!(raw_target & 1u) || !m8_callable(target)) {M8_COUNT_REWIND(); cpu->r[15]=pc; goto done;}',
                    f'cpu->r[14]=0x{nextpc|1:x}u;', 'cpu->r[15]=target;',
                    'int call_result=m8_tail(cpu,mem);',
                    f'if(call_result!=M8_AOT_EXIT || cpu->r[15]!=0x{nextpc:x}u) return call_result;']
        if insn.id==A.ARM_INS_BX:
            # Includes architectural function returns. A local computed branch
            # can resume only at a decoded instruction boundary.
            return [f'cpu->r[15]=({ctx.r(ops[0].reg)}) & ~1u;', 'goto done;' if ops[0].reg==A.ARM_REG_LR else 'goto redirect;']
        if insn.id in (A.ARM_INS_TBB,A.ARM_INS_TBH):
            mem=ops[0].mem
            width=2 if insn.id==A.ARM_INS_TBH else 1
            index=ctx.r(mem.index)
            return [f'uint32_t table_offset;',f'if(m8_read(mem,({ctx.r(mem.base)}) + ({index})*{width}u,{width},&table_offset,&slow)) goto fault;',f'cpu->r[15]=0x{insn.address+4:x}u+2u*table_offset;','goto redirect;']
        if insn.id in (A.ARM_INS_NOP,) or insn.mnemonic.split('.')[0]=='nop':
            return []
        body=self.floating(insn,ctx)
        if body is None:
            body=self.integer(insn,ctx)
        if body is not None and insn.id==A.ARM_INS_POP and any(o.type==A.ARM_OP_REG and o.reg==A.ARM_REG_PC for o in ops):
            # A stack return publishes the architectural return boundary to
            # its native caller. It does not need global tail dispatch.
            body=[line.replace('goto redirect;','goto done;') for line in body]
        return body
    def emit_function(self,f):
        rows=[f'static int fn_{f.entry:x}(m8_cpu *restrict cpu,m8_mem *restrict mem) {{', 'uint32_t pc=cpu->r[15]; int slow=0; m8_it_tx tx; int it_active=0; M8_COUNT_LOCAL;', 'goto dispatch;']
        supported=[]
        self.it_stats[f.entry]=[]
        unit_bodies={}
        for pc,unit in f.units.items():
            bodies=[]
            for insn,body in zip(unit.instructions,self.unit_bodies[f.entry][pc]):
                # Calls in IT blocks remain in Unicorn, as nested native calls
                # could need callbacks before the journal can commit.
                if unit.it and insn.id in (A.ARM_INS_BL,A.ARM_INS_BLX):
                    body=None
                bodies.append(body)
            unit_bodies[pc]=bodies
        span_plan=None
        if self.memory_spans:
            from memory_spans import analyze as analyze_spans
            span_plan=analyze_spans(f,unit_bodies,self.span_base_writes[f.entry])
            self.span_stats[f.entry]=span_plan.stats()
        omitted=set()
        if self.dead_nzcv:
            from nzcv import analyze
            omitted,self.nzcv_stats[f.entry]=analyze(f,unit_bodies)
        for pc,unit in sorted(f.units.items()):
            bodies=unit_bodies[pc]
            unsupported=next((i for i,b in zip(unit.instructions,bodies) if b is None),None)
            rows += [f'L_{pc:x}:', '#ifdef M8_AOT_DIAGNOSTIC', f'pc=0x{pc:x}u;', 'if(m8_stop_pc==pc || m8_steps==m8_stop_after) {cpu->r[15]=pc; goto done;}', 'm8_steps++;', '#endif']
            if f.entry==0x68b8 and pc==0x6f08:
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)',
                         'M8_SAVE(cpu);', 'if(m8_biquad_kernel(cpu,mem)) {M8_LOAD(cpu); goto L_6ffa;}', '#endif']
            if f.entry==0x20628 and pc==0x20850:
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)',
                         'M8_SAVE(cpu);', 'if(m8_oscillator_kernel(cpu,mem)) {M8_LOAD(cpu); goto L_208c2;}', '#endif']
            if f.entry==0x60015a48 and pc==0x60015b5c:
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)',
                         'M8_SAVE(cpu);', 'if(m8_braids_loop_kernel(cpu,mem)) {M8_LOAD(cpu); goto L_60015c3c;}', '#endif']
            if f.entry==0x13fd8 and pc==0x1417c:
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)',
                         'cpu->r[15]=0x1417cu; M8_SAVE(cpu);',
                         'if(m8_fm_renderer_kernel(cpu,mem)) {M8_LOAD(cpu); goto L_14302;}', '#endif']
            if f.entry==0x13fd8 and pc==0x1409c:
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && !defined(M8_AOT_DISABLE_FM_BATCH)',
                         'cpu->r[15]=0x1409cu; M8_KERNEL_SAVE(m8_fm_batch_kernel,cpu);',
                         'if(m8_fm_batch_kernel(cpu,mem)) {',
                         'M8_KERNEL_LOAD(m8_fm_batch_kernel,cpu);',
                         'if(cpu->r[15]==0x143eau) goto L_143ea;',
                         'if(cpu->r[15]==0x145bcu) goto L_145bc;',
                         'goto L_1409c;', '}', '#endif']
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && !defined(M8_AOT_DISABLE_FM_QUANTUM)',
                         'cpu->r[15]=0x1409cu; M8_SAVE(cpu);',
                         'if(m8_fm_quantum_kernel(cpu,mem)) {M8_LOAD(cpu); goto L_14302;}', '#endif']
            if f.entry==0x905c and pc==0x955e:
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && !defined(M8_AOT_DISABLE_STEREO_MIX)',
                         'if(cpu->r[3]-cpu->r[5]==256u) {',
                         'cpu->r[15]=0x955eu; M8_KERNEL_SAVE(m8_stereo_mix_kernel,cpu);',
                         'if(m8_stereo_mix_kernel(cpu,mem)) {',
                         'M8_KERNEL_LOAD(m8_stereo_mix_kernel,cpu); goto L_958a;',
                         '}', '}', '#endif']
            if f.entry==0xbd28 and pc==0xbea8:
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && !defined(M8_AOT_DISABLE_EFFECT_PHASE)',
                         'cpu->r[15]=0xbea8u; M8_KERNEL_SAVE(m8_effect_phase_kernel,cpu);',
                         'if(m8_effect_phase_kernel(cpu,mem)) {',
                         'M8_KERNEL_LOAD(m8_effect_phase_kernel,cpu); goto L_bfb6;',
                         '}', '#endif']
            if f.entry==0x95b0 and pc in (0x9a1c,0x9c08):
                scale=pc==0x9a1c
                bridge='m8_meter_scale' if scale else 'm8_meter_add'
                end,begin,continuation=(1,3,0x9a54) if scale else (3,7,0x9c48)
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && !defined(M8_AOT_DISABLE_METER_MIX)',
                         f'if(cpu->r[{end}]-cpu->r[{begin}]==256u) {{',
                         f'cpu->r[15]=0x{pc:x}u; M8_KERNEL_SAVE({bridge},cpu);',
                         'if(m8_meter_mix_kernel(cpu,mem)) {',
                         f'M8_KERNEL_LOAD({bridge},cpu); goto L_{continuation:x};',
                         '}', '}', '#endif']
            if f.entry==0x13480 and pc==0x136d6:
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && !defined(M8_AOT_DISABLE_FM_PITCH)',
                         'cpu->r[15]=0x136d6u; M8_KERNEL_SAVE(m8_fm_pitch_kernel,cpu);',
                         'if(m8_fm_pitch_kernel(cpu,mem)) {',
                         'M8_KERNEL_LOAD(m8_fm_pitch_kernel,cpu); goto L_139ee;',
                         '}', '#endif']
            if f.entry==0x73bc and pc in (0x7f7c,0x80b6):
                bridge='m8_envelope_left' if pc==0x7f7c else 'm8_envelope_right'
                continuation=0x80a6 if pc==0x7f7c else 0x83b4
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && !defined(M8_AOT_DISABLE_ENVELOPE_MIX)',
                         'if(cpu->r[12]==0u && !(cpu->s[12]&0x7fffffffu)) {',
                         f'cpu->r[15]=0x{pc:x}u; M8_KERNEL_SAVE({bridge},cpu);',
                         'if(m8_envelope_mix_kernel(cpu,mem)) {',
                         f'M8_KERNEL_LOAD({bridge},cpu); goto L_{continuation:x};',
                         '}', '}', '#endif']
            if f.entry==0x13480 and pc==0x13a46:
                rows += ['#if defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && !defined(M8_AOT_DISABLE_FM_MODULATION_TAIL)',
                         'cpu->r[15]=0x13a46u; M8_KERNEL_SAVE(m8_fm_modulation_tail_kernel,cpu);',
                         'if(m8_fm_modulation_tail_kernel(cpu,mem)) {',
                         'M8_KERNEL_LOAD(m8_fm_modulation_tail_kernel,cpu);',
                         'return M8_AOT_EXIT;', '}', '#endif']
            if f.entry==0x1991c and pc==0x19b64:
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && defined(M8_AOT_BRAIDS_MODE0) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)',
                         'cpu->r[15]=0x19b64u; M8_KERNEL_SAVE(m8_braids_mode0_kernel,cpu);',
                         'if(m8_braids_mode0_kernel(cpu,mem)) {',
                         'M8_KERNEL_LOAD(m8_braids_mode0_kernel,cpu);',
                         'if(cpu->r[15]==0x19bd2u) goto L_19bd2;',
                         'goto L_19b94;', '}', '#endif']
                rows += ['#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && !defined(M8_AOT_DISABLE_BRAIDS_FILTER)',
                         'cpu->r[15]=0x19b64u; M8_KERNEL_SAVE(m8_braids_filter_kernel,cpu);',
                         'if(m8_braids_filter_kernel(cpu,mem)) {',
                         'M8_KERNEL_LOAD(m8_braids_filter_kernel,cpu);',
                         'if(cpu->r[15]==0x19bd2u) goto L_19bd2;',
                         'goto L_19b94;', '}', '#endif']
            if unsupported is not None:
                f.barriers[pc]=f'{unsupported.address:#x}: {unsupported.mnemonic} {unsupported.op_str}'
                rows += [f'cpu->r[15]=0x{pc:x}u; goto done;']
                continue
            rows += ['M8_COUNT_BEGIN();']
            supported.extend(i.address for i in unit.instructions)
            if unit.it:
                supported.append(pc)
                rows += ['M8_COUNT_ONE();']
            effects_start=len(rows)
            it_plan=None
            if self.it_preflight:
                from it_preflight import analyze as analyze_it
                it_plan=analyze_it(unit,bodies)
            memory_names=('ldr','str','ldm','stm','push','pop','vldr','vstr','vldm','vstm','vpush','vpop','tbb','tbh')
            has_memory=any(Context(i).name.startswith(memory_names) for i in unit.instructions)
            multi_names=('ldrd','strd','ldm','stm','push','pop','vldm','vstm','vpush','vpop')
            multi_memory=any(Context(i).name.startswith(multi_names) or (i.mnemonic.startswith(('vldr','vstr')) and '.64' in i.mnemonic) for i in unit.instructions)
            transaction=(unit.it and has_memory) or multi_memory
            uninterrupted_it=span_plan is not None and span_plan.uninterrupted_it(unit,bodies)
            if uninterrupted_it:
                self.span_stats[f.entry]['uninterrupted_it_units'].append(pc)
            original_transaction='#if !defined(M8_AOT_FAST_MEMORY) || defined(M8_AOT_DIAGNOSTIC)'
            if transaction and unit.it:
                begin=f'pc=0x{pc:x}u; cpu->r[15]=pc; m8_it_begin(&tx,cpu); it_active=1;'
                conditions={i.cc for i in unit.instructions}
                if len(conditions)==1 and next(iter(conditions)) not in (A.ARM_CC_INVALID,A.ARM_CC_AL):
                    begin=f'if(m8_condition(cpu->xpsr,{next(iter(conditions))})) {{'+begin+'}'
                rows += ([original_transaction,begin,'#endif'] if uninterrupted_it else [begin])
            for insn,body in zip(unit.instructions,bodies):
                if insn.address in omitted:
                    body=(['#ifdef M8_AOT_DIAGNOSTIC']+body+['#else']
                          +self.instruction(f,insn,suppress_nzcv=True)+['#endif'])
                rows += ['{',f'/* {insn.address:08x}: {insn.mnemonic} {insn.op_str} */']
                if not unit.it:
                    rows.append('M8_COUNT_ONE();')
                if insn.cc not in (A.ARM_CC_INVALID,A.ARM_CC_AL):
                    rows.append(f'if(m8_condition(cpu->xpsr,{insn.cc})) {{')
                if unit.it:
                    rows.append('M8_COUNT_ONE();')
                if unit.it and insn.id == A.ARM_INS_B:
                    rows += ['m8_it_commit(); it_active=0;']
                # Ordinary instructions do not publish PC or poll slow state.
                # A fault writes the constant boundary only on the cold edge.
                span_body=span_plan.replace(insn.address,body,unit.it and not uninterrupted_it) if span_plan is not None else None
                if multi_memory and not unit.it:
                    body=self.preflight(body,pc)
                dwt=((f.entry==0x31680 and pc in (0x31688,0x31694,0x3169a,0x316b8))
                     or (f.entry==0x34848 and pc==0x348b2))
                body=self.fast_memory(body,pc,unit.it,dwt,
                                      not unit.it and not multi_memory and f.entry in (0x2ac10,0x2adbc,0x2542c,0x1bc58,0x6001dbf8))
                if span_body is not None:
                    body=(['#if defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC)']
                          +span_body+['#else']+body+['#endif'])
                body=[line.replace('goto fault;',f'{{pc=0x{pc:x}u; goto fault;}}').replace('cpu->r[15]=pc; goto done;',f'cpu->r[15]=0x{pc:x}u; goto done;') for line in body]
                if not transaction:
                    body=[line.replace('m8_write(mem,','m8_write_plain(mem,') for line in body]
                rows.extend(body)
                if insn.cc not in (A.ARM_CC_INVALID,A.ARM_CC_AL):
                    rows.append('}')
                rows.append('}')
                if not unit.it:
                    if transaction:
                        rows += ['m8_it_commit(); it_active=0;']
                    if has_memory:
                        rows += [f'if(slow) {{cpu->r[15]=0x{unit.next_pc:x}u; goto done;}}']
            if transaction:
                commit='m8_it_commit(); it_active=0;'
                rows += ([original_transaction,commit,'#endif'] if uninterrupted_it else [commit])
            if it_plan is not None:
                rows[effects_start:]=it_plan.wrap(rows[effects_start:])
                self.it_stats[f.entry].append(it_plan.stats())
            rows += self.jump(f,unit.next_pc)
        rows += ['redirect:', 'if(it_active) {m8_it_commit(); it_active=0;}','if(slow) goto done;', 'pc=cpu->r[15];', 'switch(pc) {']
        rows += [f'case 0x{pc:x}u: goto L_{pc:x};' for pc in sorted(f.units)]
        rows += ['default: cpu->r[15]=pc; return m8_tail(cpu,mem);', '}', 'dispatch:', 'switch(pc) {']
        rows += [f'case 0x{pc:x}u: goto L_{pc:x};' for pc in sorted(f.units)]
        rows += ['default: cpu->r[15]=pc; goto done;', '}',
                 'memory_fallback:', 'M8_COUNT_REWIND();', 'if(it_active) {m8_it_abort(cpu);}', 'return M8_AOT_EXIT;',
                 'fault:', 'M8_COUNT_REWIND();', 'if(it_active) {int io=tx.saw_slow; m8_it_abort(cpu); return io ? M8_AOT_FAULT : M8_AOT_EXIT;}','cpu->r[15]=pc; return m8_slow_fallback ? M8_AOT_EXIT : M8_AOT_FAULT;', 'done:', 'if(it_active) m8_it_commit();','return M8_AOT_EXIT;', '}']
        body='\n'.join(rows)
        from loop_spans import specialize
        body=specialize(f,unit_bodies,body)
        from region_spans import specialize as specialize_regions
        body=specialize_regions(f,unit_bodies,body)
        from pointee_spans import specialize as specialize_pointees
        body=specialize_pointees(f,unit_bodies,body)
        from validity_first import specialize as specialize_validity_first
        body=specialize_validity_first(body)
        prelude=('#if defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_GUEST_COUNT)\nif(m8_memcpy_words(output,mem)) return M8_AOT_EXIT;\n#endif\n'
                 if f.entry==0x326d4 else '')
        if span_plan is not None and span_plan.groups:
            prelude=span_plan.prelude()+prelude
        if f.entry==0x12174:
            prelude=('#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)\n'
                     'if(output->r[15]==0x12174u && m8_fm_operator_kernel(output,mem)) return M8_AOT_EXIT;\n#endif\n')+prelude
        if f.entry in (0x36da0,0x36e0c):
            kernel='m8_buffer_gain_kernel' if f.entry==0x36da0 else 'm8_buffer_add_kernel'
            prelude=('#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)\n'
                     f'if(output->r[15]==0x{f.entry:x}u && {kernel}(output,mem)) return M8_AOT_EXIT;\n#endif\n')+prelude
        if f.entry==0x168e8 and 0x15da4 in self.by_entry:
            prelude=('#if defined(M8_AOT_HYPER_VOICE) && defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && defined(M8_AOT_FIXED_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)\n'
                     'if(output->r[15]==0x168e8u && output->r[3]>1u) {\n'
                     'int status=m8_hyper_voice_kernel(output,mem,fn_15da4);\n'
                     'if(status>=0) return status;\n}\n#endif\n')+prelude
        if f.entry==0x1caa8 and all(entry in self.by_entry for entry in (0x1c414,0x1b938)):
            prelude=('#if defined(M8_AOT_SAMPLER_VOICE) && defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && defined(M8_AOT_FIXED_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)\n'
                     'if(output->r[15]==0x1caa8u && output->r[3]>1u) {\n'
                     'int status=m8_sampler_voice_kernel(output,mem,fn_1c414,fn_1b938);\n'
                     'if(status>=0) return status;\n}\n#endif\n')+prelude
        modified=self.modified_registers[f'fn_{f.entry:x}']
        result='#ifdef M8_AOT_NATIVE_FP\n'+self.scalarize(body,True,prelude,self.call_registers,modified,self.local_nzcv)+'\n#else\n'+self.scalarize(body,False,prelude,self.call_registers,modified,self.local_nzcv)+'\n#endif'
        if self.exact_leaves:
            # Only BL produces call_result with a static fn target. Other fn
            # expressions can resume inside a function and must keep dispatch.
            result=re.sub(r'\bcall_result=fn_([0-9a-f]+)\(cpu,mem\);',
                          lambda m:('call_result='+self.exact_leaves[int(m[1],16)].name+'(cpu,mem);'
                                    if int(m[1],16) in self.exact_leaves else m[0]),result)
        return result, supported

    @staticmethod
    def fast_memory(body,pc,it,dwt=False,cooperative=False):
        """Avoid escaped load temporaries and callback joins on the RAM path.

        Fast-memory builds return before a non-region access. Unicorn then owns
        that instruction and its IRQ boundary. An IT store still journals its
        earlier effects because a later IT member can require fallback.
        """
        result=[]
        for line in body:
            read=re.search(r'if \(m8_read\(mem, (.*), (\d+), &(\w+), &slow\)\) goto fault;',line)
            write=re.search(r'if \(m8_write\(mem, (.*), (\d+), (.*), &slow\)\) goto fault;',line)
            match=read or write
            if not match:
                result.append(line)
                continue
            prefix=line[:match.start()]
            pointer=f'M8_FAST_POINTER(&ram, {match[1]}, {match[2]})'
            fast=['#ifdef M8_AOT_FAST_MEMORY',prefix+'{']
            fast += [f'uint8_t *access_host={pointer};']
            if cooperative:
                operation='read' if read else 'write'
                value=f'&{match[3]}' if read else match[3]
                fallback=f'cpu->r[15]=0x{pc:x}u; goto memory_fallback;'
                ram_operation=f'{match[3]}=m8_le_read(access_host,{match[2]});' if read else f'm8_le_write(access_host,{match[2]},{match[3]});'
                fast += [f'if(access_host) {{ {ram_operation} }}', '#ifdef M8_AOT_COOPERATIVE_SCS',
                         f'else if(!m8_slow_fallback && m8_nvic_word({match[1]},{match[2]}) && mem->{operation}) {{',
                         f'if(m8_{operation}_cooperative(mem,{match[1]},{match[2]},{value},&slow)) goto fault;', '}', '#endif',
                         f'else {{ {fallback} }}']
                result += fast+['}', '#else',line,'#endif']
                continue
            if read:
                if dwt:
                    fast += [f'if(access_host) {match[3]}=m8_le_read(access_host,{match[2]});',
                             f'else if(!m8_read_dwt_ram(mem,{match[1]},{match[2]},&{match[3]})) {{cpu->r[15]=0x{pc:x}u; goto memory_fallback;}}']
                else:
                    fast += [f'if(!access_host) {{cpu->r[15]=0x{pc:x}u; goto memory_fallback;}}',f'{match[3]}=m8_le_read(access_host,{match[2]});']
            elif it:
                fast += [f'if(!access_host) {{cpu->r[15]=0x{pc:x}u; goto memory_fallback;}}',f'if(m8_write_it(access_host,{match[2]},{match[3]})) goto fault;']
            else:
                fast += [f'if(!access_host) {{cpu->r[15]=0x{pc:x}u; goto memory_fallback;}}',f'm8_le_write(access_host,{match[2]},{match[3]});']
            result += fast+['}', '#else',line,'#endif']
        return result

    @staticmethod
    def preflight(body,pc):
        """A contiguous RAM transfer has no intermediate exit or rollback.

        Keep the exact ordered, journaled path only when the complete span is
        outside one region. This also preserves partial effects on callback faults.
        """
        accesses=[line for line in body if 'm8_read(mem,' in line or 'm8_write(mem,' in line]
        if not accesses:
            return body
        address='fp_addr' if 'fp_addr' in accesses[0] else 'addr'
        span=len(accesses)*4
        result=[]
        for line in body:
            result.append(line)
            if line.startswith(f'uint32_t {address} ='):
                result += ['#ifdef M8_AOT_FAST_MEMORY',f'uint8_t *block_host=M8_FAST_POINTER(&ram,{address},{span});',
                           f'if(!block_host) {{cpu->r[15]=0x{pc:x}u; goto memory_fallback;}}', '#else',
                           f'uint8_t *block_host=m8_pointer(mem,{address},{span});',
                           f'if(!block_host) {{pc=0x{pc:x}u; cpu->r[15]=pc; m8_it_begin(&tx,cpu); it_active=1; tx.forbid_slow=m8_slow_fallback;}}']
                result += ['#endif']
            if 'm8_read(mem,' in line:
                match=re.search(r'm8_read\(mem, (.*), 4, &(\w+), &slow\)',line)
                if match:
                    result[-1]=f'if(block_host) {match[2]}=m8_le_read(block_host+((uint32_t)({match[1]})-{address}),4); else '+line
            elif 'm8_write(mem,' in line:
                match=re.search(r'm8_write\(mem, (.*), 4, (.*), &slow\)',line)
                if match:
                    result[-1]=f'if(block_host) m8_le_write(block_host+((uint32_t)({match[1]})-{address}),4,{match[2]}); else '+line
        return result

    @staticmethod
    def scalarize(body,float_locals=False,prelude='',call_registers=None,modified_registers=None,local_nzcv=False):
        """Keep register values independent of byte-addressable guest memory.

        Only the flags helper object can escape to arithmetic helpers. Register
        locals cross a direct call or transactional fallback through an explicit
        state bridge; the normal arithmetic and RAM path never takes their address.
        """
        # Double register helpers otherwise make the whole CPU object escape.
        body=re.sub(r'm8_fp_get64\(cpu, (\d+)\)',lambda m:f'((uint64_t)cpu->s[{2*int(m[1])}] | ((uint64_t)cpu->s[{2*int(m[1])+1}] << 32))',body)
        body=re.sub(r'm8_fp_set64\(cpu, (\d+), ([^\n]+)\);',lambda m:'{uint64_t pair_value='+m[2]+f'; cpu->s[{2*int(m[1])}]=(uint32_t)pair_value; cpu->s[{2*int(m[1])+1}]=(uint32_t)(pair_value >> 32);}}',body)
        # Bridge calls before replacing architectural array accesses.
        body=re.sub(r'(?:fn_[0-9a-f]+|m8_tail)\(cpu,mem\)',lambda m:'M8_CALL('+m[0].split('(')[0]+')',body)
        body=body.replace('m8_it_begin(&tx,cpu);','M8_SAVE(cpu); m8_it_begin(&tx,cpu);')
        body=body.replace('m8_it_abort(cpu);','m8_it_abort(cpu); M8_LOAD(cpu);')
        if float_locals:
            body=re.sub(r'cpu->s\[(\d+)\]\s*=\s*([^;]+);',lambda m:f's_{m[1]}=m8_fp_float({m[2]});',body)
            body=re.sub(r'cpu->s\[(\d+)\]',lambda m:f'm8_fp_bits32(s_{m[1]})',body)
        body=re.sub(r'cpu->([rs])\[(\d+)\]',lambda m:f'{m[1]}_{m[2]}',body)
        # Evaluate call returns before committing the scalar state.
        body=re.sub(r'\breturn ([^;]+);',r'{int returned=\1; M8_PUBLISH(output); return returned;}',body)
        if local_nzcv:
            from local_flags import transform
            body=transform(body)
        names=[(kind,i) for kind,count in [('r',16),('s',32)] for i in range(count)]
        save=' '.join(f'(dst)->{k}[{i}]='+ (f'm8_fp_bits32(s_{i})' if k=='s' and float_locals else f'{k}_{i}')+';' for k,i in names)+' (dst)->xpsr=cpu->xpsr; (dst)->fpscr=cpu->fpscr;'
        publish=' '.join(f'(dst)->{k}[{i}]='+ (f'm8_fp_bits32(s_{i})' if k=='s' and float_locals else f'{k}_{i}')+';' for k,i in names if modified_registers is None or (k,i) in modified_registers)+' (dst)->xpsr=cpu->xpsr; (dst)->fpscr=cpu->fpscr;'
        load=' '.join(f'{k}_{i}='+ (f'm8_fp_float((src)->s[{i}])' if k=='s' and float_locals else f'(src)->{k}[{i}]')+';' for k,i in names)+' cpu->xpsr=(src)->xpsr; cpu->fpscr=(src)->fpscr;'
        declarations='\n'.join(f'float s_{i}=m8_fp_float(output->s[{i}]);' if k=='s' and float_locals else f'uint32_t {k}_{i}=output->{k}[{i}];' for k,i in names)
        if local_nzcv:
            from local_flags import DECLARATIONS
            declarations=DECLARATIONS+declarations
            save=save.replace('(dst)->xpsr=cpu->xpsr;', '(dst)->xpsr=M8_FLAGS_WORD;')
            publish=publish.replace('(dst)->xpsr=cpu->xpsr;', '(dst)->xpsr=M8_FLAGS_WORD;')
            load+=' M8_FLAGS_GET();'
        # M8_CALL uses an ordinary helper to retain portable C11 expression
        # syntax. Unpack immediately after each call statement/return.
        def selected_bridge(variable,name,selected):
            if selected==set(names):
                return f'M8_SAVE(cpu); int {variable}={name}(cpu,mem); M8_LOAD(cpu);'
            narrow_save=' '.join(f'cpu->{k}[{i}]='+ (f'm8_fp_bits32(s_{i})' if k=='s' and float_locals else f'{k}_{i}')+';' for k,i in sorted(selected))
            narrow_load=' '.join(f'{k}_{i}='+ (f'm8_fp_float(cpu->s[{i}])' if k=='s' and float_locals else f'cpu->{k}[{i}]')+';' for k,i in sorted(selected))
            if local_nzcv:
                narrow_save+=' cpu->xpsr=M8_FLAGS_WORD;'
                narrow_load+=' M8_FLAGS_GET();'
            return '\n'.join([f'int {variable};','#ifdef M8_AOT_NARROW_CALLS',narrow_save,'#else','M8_SAVE(cpu);','#endif',f'{variable}={name}(cpu,mem);','#ifdef M8_AOT_NARROW_CALLS',narrow_load,'#else','M8_LOAD(cpu);','#endif',''])
        def bridge(match):
            variable,name=match[1],match[2]
            return selected_bridge(variable,name,(call_registers or {}).get(name,set(names)))
        body=re.sub(r'int (call_result|returned)=M8_CALL\(([^)]+)\);',bridge,body)
        header='m8_cpu *restrict cpu,m8_mem *restrict mem) {'
        # Arithmetic helpers use only xpsr/fpscr. Every operation that reads
        # a complete CPU (calls, kernels, IT snapshots) has M8_SAVE first.
        # Zeroing all 200 bytes at each small callee is therefore unnecessary.
        # Narrow calls retain an initialized backing CPU. Unreferenced fields
        # can be stale, but a callee's unused scalar copies must never read
        # indeterminate C values. Only referenced fields return to the caller.
        replacement='m8_cpu *restrict output,m8_mem *restrict mem) {\n'+prelude+'#ifdef M8_AOT_NARROW_CALLS\nm8_cpu flags=*output;\n#else\nm8_cpu flags;\n#endif\nm8_cpu *cpu=&flags;\n#ifdef M8_AOT_FAST_MEMORY\nm8_mem ram=*mem;\n#endif\n'+declarations+'\ncpu->xpsr=output->xpsr; cpu->fpscr=output->fpscr;\n'
        body=body.replace(header,replacement)
        publication='#define M8_PUBLISH(dst) do { '+publish+' } while(0)'
        result='#define M8_SAVE(dst) do { '+save+' } while(0)\n#define M8_LOAD(src) do { '+load+' } while(0)\n#ifdef M8_AOT_NARROW_CALLS\n'+publication+'\n#else\n#define M8_PUBLISH(dst) M8_SAVE(dst)\n#endif\n'+body+'\n#undef M8_SAVE\n#undef M8_LOAD\n#undef M8_PUBLISH'
        if local_nzcv:
            from local_flags import MACROS,UNDEF
            result=MACROS+result+UNDEF
        kernels=set(re.findall(r'M8_KERNEL_(?:SAVE|LOAD)\((\w+),',body))
        if kernels:
            macros=['#define M8_KERNEL_SAVE(kernel,dst) M8_KSAVE_##kernel(dst)',
                    '#define M8_KERNEL_LOAD(kernel,src) M8_KLOAD_##kernel(src)']
            undefs=['#undef M8_KERNEL_SAVE','#undef M8_KERNEL_LOAD']
            for kernel in sorted(kernels):
                inputs,outputs=KERNEL_REGISTERS[kernel]
                selected_save=' '.join(f'(dst)->{k}[{i}]='+
                    (f'm8_fp_bits32(s_{i})' if k=='s' and float_locals else f'{k}_{i}')+';'
                    for k,i in sorted(inputs))
                selected_save+=' (dst)->xpsr='+('M8_FLAGS_WORD' if local_nzcv else 'cpu->xpsr')+'; (dst)->fpscr=cpu->fpscr;'
                selected_load=' '.join(f'{k}_{i}='+
                    (f'm8_fp_float((src)->s[{i}])' if k=='s' and float_locals else f'(src)->{k}[{i}]')+';'
                    for k,i in sorted(outputs))
                selected_load+=' cpu->xpsr=(src)->xpsr; cpu->fpscr=(src)->fpscr;'
                if local_nzcv:
                    selected_load+=' M8_FLAGS_GET();'
                macros += [f'#define M8_KSAVE_{kernel}(dst) do {{ {selected_save} }} while(0)',
                           f'#define M8_KLOAD_{kernel}(src) do {{ {selected_load} }} while(0)']
                undefs += [f'#undef M8_KSAVE_{kernel}',f'#undef M8_KLOAD_{kernel}']
            result='\n'.join(macros)+'\n'+result+'\n'+'\n'.join(undefs)
        return result
    def emit(self):
        rows=['/* Generated by translate.py; regenerate from the original firmware. */','#include "runtime.h"','#include "integer.h"','#include "fp.h"','#include "memory_spans.h"','#include "biquad.h"','#include "oscillator.h"','#include "braids_loop.h"','#if defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)','#include "fm_modulation_tail.h"','#endif','#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)', '#include "fm_operator.h"', '#include "fm_renderer.h"', '#include "fm_quantum.h"', '#include "fm_batch.h"', '#include "fm_pitch.h"', '#include "stereo_mix.h"', '#include "meter_mix.h"', '#include "envelope_mix.h"', '#include "effect_phase.h"', '#include "buffer_kernels.h"', '#include "braids_filter.h"', '#if defined(M8_AOT_BRAIDS_MODE0)', '#include "braids_mode0.h"', '#endif', '#endif','#include "memcpy_kernel.h"','static _Thread_local uint32_t m8_stop_pc=0xffffffffu;', 'static _Thread_local uint64_t m8_steps=0, m8_stop_after=UINT64_MAX;', 'void m8_aot_set_stop_pc(uint32_t pc) {m8_stop_pc=pc;}', 'void m8_aot_set_stop_after(uint64_t n) {m8_steps=0; m8_stop_after=n;}', 'uint64_t m8_aot_get_steps(void) {return m8_steps;}', 'void m8_aot_set_slow_fallback(int enabled) {m8_slow_fallback=enabled;}']
        rows += ['#if defined(M8_AOT_FIXED_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)',
                 '#include "canonical_memory.h"',
                 '#define M8_FAST_POINTER(mem,addr,size) m8_canonical_offset_pointer((uintptr_t)(mem)->itcm.host,addr,size)',
                 '#define M8_FAST_SPAN_POINTER(mem,addr,size) M8_FAST_POINTER(mem,addr,size)', '#else',
                 '#define M8_FAST_POINTER(mem,addr,size) m8_pointer(mem, addr, size)',
                 '#define M8_FAST_SPAN_POINTER(mem,addr,size) m8_span_pointer(mem, addr, size)', '#endif']
        if self.local_nzcv:
            rows.insert(3,'#include "local_flags.h"')
        rows += ['#if defined(M8_AOT_HYPER_VOICE) && defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && defined(M8_AOT_FIXED_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)',
                 '#include "hyper_voice.h"', '#endif']
        rows += ['#if defined(M8_AOT_SAMPLER_VOICE) && defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && defined(M8_AOT_FIXED_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)',
                 '#include "sampler_voice.h"', '#endif']
        rows += ['#ifdef M8_AOT_GUEST_COUNT',
                 'static _Thread_local uint64_t m8_guest_count;',
                 '#define M8_COUNT_LOCAL uint64_t count_before=0',
                 '#define M8_COUNT_BEGIN() (count_before=m8_guest_count)',
                 '#define M8_COUNT_ONE() (++m8_guest_count)',
                 '#define M8_COUNT_REWIND() (m8_guest_count=count_before)',
                 'uint32_t m8_aot_guest_count_enabled(void) {return 1;}',
                 'uint64_t m8_aot_guest_instructions(void) {return m8_guest_count;}',
                 'void m8_aot_reset_guest_instructions(void) {m8_guest_count=0;}',
                 '#else', '#define M8_COUNT_LOCAL', '#define M8_COUNT_BEGIN() ((void)0)',
                 '#define M8_COUNT_ONE() ((void)0)', '#define M8_COUNT_REWIND() ((void)0)',
                 'uint32_t m8_aot_guest_count_enabled(void) {return 0;}',
                 'uint64_t m8_aot_guest_instructions(void) {return 0;}',
                 'void m8_aot_reset_guest_instructions(void) {}', '#endif']
        rows += [f'static int fn_{f.entry:x}(m8_cpu*,m8_mem*);' for f in self.functions]
        rows += [f'static int {leaf.name}(m8_cpu*,m8_mem*);' for leaf in self.exact_leaves.values()]
        rows += ['uint32_t m8_aot_memory_services(void) {', '#ifdef M8_AOT_COOPERATIVE_SCS',
                 'return M8_AOT_SERVICE_NVIC;', '#else', 'return 0;', '#endif', '}']
        rows += ['uint32_t m8_aot_fixed_memory_required(void) {',
                 '#if defined(M8_AOT_FIXED_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)',
                 'return 1;', '#else', 'return 0;', '#endif', '}']
        if self.call_dispatch:
            from call_dispatch import Layout
            rows.append(Layout.build(f.entry for f in self.functions).emit(
                {entry:leaf.name for entry,leaf in self.exact_leaves.items()}))
        else:
            rows += ['static int m8_callable(uint32_t pc) {switch(pc) {']
            rows += [f'case 0x{f.entry:x}u:' for f in self.functions]
            rows += ['return 1; default: return 0; }}']
            rows += ['static int m8_tail(m8_cpu *restrict cpu,m8_mem *restrict mem) {switch(cpu->r[15]) {']
            rows += [f'case 0x{f.entry:x}u: return '+
                     (self.exact_leaves[f.entry].name if f.entry in self.exact_leaves else f'fn_{f.entry:x}')+
                     '(cpu,mem);' for f in self.functions]
            rows += ['default: return M8_AOT_EXIT;', '}}']
        manifest={'firmware_sha256':hashlib.sha256(self.fw.data).hexdigest(),'functions':[]}
        for f in self.functions:
            body,supported=self.emit_function(f)
            rows.append(body)
            if f.entry in self.exact_leaves:
                rows.append(self.exact_leaves[f.entry].emit())
            rows += [f'static int wrap_{f.entry:x}(m8_cpu *restrict cpu,m8_mem *restrict mem) {{',
                     '#if defined(M8_AOT_FIXED_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)',
                     'if(!m8_canonical_memory_offset(mem)) return M8_AOT_EXIT;',
                     '#endif',
                     '#ifdef M8_AOT_NATIVE_FP', 'if(!m8_fp_native_supported(cpu)) return M8_AOT_EXIT;', '#endif',
                     'm8_fp_env env=m8_fp_enter(cpu);',f'int result=fn_{f.entry:x}(cpu,mem);']
            continuation=HOOK_CONTINUATIONS.get(f.entry)
            if continuation is not None and continuation[1] in self.by_entry:
                target,owner=continuation
                rows += [f'if(result==M8_AOT_EXIT && cpu->r[15]==0x{target:x}u)',
                         f'result=fn_{owner:x}(cpu,mem);']
            rows += ['m8_fp_leave(env); return result;','}']
            manifest['functions'].append({'entry':f.entry,'end':f.end,'instructions':sorted(set(supported)),'barriers':{hex(pc):s for pc,s in f.barriers.items()},'calls':sorted(f.calls),'resumes':sorted(f.resumes), 'units':[{'pc':u.pc,'members':[i.address for i in u.instructions],'it':u.it} for u in sorted(f.units.values(),key=lambda u:u.pc)], 'nzcv':self.nzcv_stats.get(f.entry), 'memory_spans':self.span_stats.get(f.entry)})
            if self.it_preflight:
                manifest['functions'][-1]['it_preflight']=self.it_stats[f.entry]
        if self.exact_entry_leaves:
            manifest['exact_entry_leaves']={
                'entries':sorted(self.exact_leaves),
                'declined_entries':[f.entry for f in self.functions if f.entry not in self.exact_leaves]}
        rows += ['int m8_aot_verify_memory(m8_mem *mem,uint32_t *bad_pc) {', 'static const struct {uint32_t pc; uint8_t size; uint8_t bytes[4];} code[]={']
        for pc,fentry in sorted(self.safe.items()):
            unit=self.by_entry[fentry].units[pc]
            checks=[self.fw.decode(pc)] if unit.it else []
            checks+=unit.instructions
            for insn in checks:
                raw=','.join(str(b) for b in insn.bytes)
                rows += [f'{{0x{insn.address:x}u,{insn.size},{{{raw}}}}},']
        rows += ['};', 'for(size_t i=0;i<sizeof(code)/sizeof(code[0]);i++) {', 'uint8_t *p=m8_pointer(mem,code[i].pc,code[i].size);', 'if(!p || memcmp(p,code[i].bytes,code[i].size)) {*bad_pc=code[i].pc; return 1;}', '}', 'return 0;', '}']
        rows += ['m8_aot_fn m8_aot_lookup(uint32_t pc) {switch(pc & ~1u) {']
        for pc,entry in sorted(self.safe.items()):
            rows += [f'case 0x{pc:x}u: return wrap_{entry:x};']
        rows += ['default: return NULL;','}}']
        entries=sorted({pc for f in self.functions if self.hooks is None or f.entry in self.hooks for pc in f.resumes if pc in self.safe})
        if self.entry_hooks is not None:
            missing=self.entry_hooks-set(entries)
            if missing:
                raise ValueError(f'entry hooks are not selected safe roots/continuations: {sorted(missing)}')
            entries=sorted(self.entry_hooks)
        manifest['hook_entries']=entries
        rows += ['uint32_t m8_aot_entries(const uint32_t **pcs) {','static const uint32_t entries[]={'+','.join(f'0x{pc:x}u' for pc in entries)+'};','*pcs=entries; return sizeof(entries)/sizeof(entries[0]);','}']
        return '\n'.join(rows)+'\n',manifest

def read_functions(path):
    functions=[]
    for line in Path(path).read_text().splitlines():
        fields=line.split('#',1)[0].split()
        if fields:
            functions.append(tuple(int(x,16) for x in fields[:2]))
    return functions

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware',type=Path,default=DEFAULT_FIRMWARE)
    parser.add_argument('--functions',type=Path,default=HERE/'functions.txt')
    parser.add_argument('--function-fragment',type=Path,action='append',default=[],help='Append original function bounds from an additional catalog')
    parser.add_argument('--hooks',type=Path,default=HERE/'enabled.txt')
    parser.add_argument('--entry-hooks',type=Path,help='Install only these selected safe root/continuation PCs')
    parser.add_argument('--dead-nzcv',action='store_true',help='Omit provably dead whole NZCV writes; diagnostics keep every write')
    parser.add_argument('--memory-spans',action='store_true',help='Qualify complete spans through invariant bases; diagnostics keep original accesses')
    parser.add_argument('--local-nzcv',action='store_true',help='Keep NZCV values in locals and pack only at state boundaries')
    parser.add_argument('--it-preflight',action='store_true',help='Qualify invariant IT-unit spans before effects; diagnostics retain rollback')
    parser.add_argument('--call-dispatch',action='store_true',help='Use one exact tagged lookup for computed native calls')
    parser.add_argument('--exact-entry-leaves',action='store_true',help='Use experimental exact-entry paths for small register-only native leaves')
    parser.add_argument('--output',type=Path,default=HERE/'generated.c')
    parser.add_argument('--manifest',type=Path,default=HERE/'manifest.json')
    args=parser.parse_args()
    def addresses(path):
        return {int(line.split('#')[0].strip(),0) for line in path.read_text().splitlines() if line.split('#')[0].strip()}
    functions=read_functions(args.functions)
    for fragment in args.function_fragment:
        functions.extend(read_functions(fragment))
    code,manifest=Translator(Firmware(args.firmware),functions,addresses(args.hooks),
                             addresses(args.entry_hooks) if args.entry_hooks else None,
                             dead_nzcv=args.dead_nzcv,memory_spans=args.memory_spans,local_nzcv=args.local_nzcv,
                             it_preflight=args.it_preflight,call_dispatch=args.call_dispatch,
                             exact_entry_leaves=args.exact_entry_leaves).emit()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(code)
    args.manifest.write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'Generated {len(manifest["functions"])} functions; {sum(len(f["instructions"]) for f in manifest["functions"])} supported instructions')
    for f in manifest['functions']:
        print(f'{f["entry"]:#x}..{f["end"]:#x}: {len(f["instructions"])} instructions, {len(f["barriers"])} barriers')
if __name__=='__main__':
    main()
