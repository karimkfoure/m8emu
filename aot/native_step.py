#!/usr/bin/env python3
"""Generate default-off, firmware-specific single-instruction native fallback.

The existing AOT emitter remains unchanged. Each generated case validates its
live code bytes, executes one instruction, and publishes the next PC. Unknown
instructions remain explicit failures; this is not a general Arm emulator.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
from types import SimpleNamespace

from capstone import arm as A
from translate import Context, Firmware, Translator


class StepTranslator(Translator):
    def jump(self, _function, pc):
        return [f'cpu->r[15]=0x{pc:x}u;', 'goto completed;']

    def instruction(self, function, insn, suppress_nzcv=False):
        ctx = Context(insn)
        ops = insn.operands
        if insn.id == A.ARM_INS_BL:
            return [f'cpu->r[14]=0x{(insn.address+insn.size)|1:x}u;'] + \
                self.jump(function, ops[0].imm & ~1)
        if insn.id in (A.ARM_INS_BX, A.ARM_INS_BLX):
            if ops[0].type != A.ARM_OP_REG:
                return None
            body = [f'uint32_t target={ctx.r(ops[0].reg)};',
                    'if(!(target & 1u)) goto fault;']
            if insn.id == A.ARM_INS_BLX:
                body += [f'cpu->r[14]=0x{(insn.address+insn.size)|1:x}u;']
            return body + ['cpu->r[15]=target & ~1u;', 'goto completed;']
        body = super().instruction(function, insn, suppress_nzcv)
        if body is not None:
            return [line.replace('goto redirect;', 'goto completed;')
                        .replace('goto done;', 'goto completed;')
                        .replace('m8_read(', 'm8_native_read(')
                        .replace('m8_write(', 'm8_native_write(')
                    for line in body]
        return self.special(insn, ctx)

    @staticmethod
    def special(insn, ctx):
        raw = bytes(insn.bytes)
        h1 = int.from_bytes(raw[:2], 'little')
        h2 = int.from_bytes(raw[2:], 'little') if len(raw) == 4 else 0
        word = (h1 << 16) | h2
        if len(raw) == 2 and (h1 & 0xff00) == 0xbf00 and h1 & 15:
            condition, mask = (h1 >> 4) & 15, h1 & 15
            if condition >= 14:
                return None
            return ['if(in_it) goto unhandled;',
                    f'arch->itstate=0x{condition*16+mask:x}u;',
                    'm8_native_publish_it(cpu,arch);', 'advance_it=0;']
        if word in (0xf3bf8f4f, 0xf3bf8f5f, 0xf3bf8f6f):
            # Device callbacks and RAM stores complete synchronously in this
            # single-threaded engine. No host DMA thread accesses guest RAM.
            return ['__asm__ volatile("" ::: "memory");']
        if word == 0xf3bf8f2f:  # CLREX
            return ['arch->exclusive_valid=0;']
        if h1 in (0xb672, 0xb662):
            # PRIMASK is not CPSR.I in the M-class Unicorn reference.
            return [f'if(arch->ipsr || !(arch->control & 1u)) arch->primask={int(h1 == 0xb672)}u;']
        if insn.id in (A.ARM_INS_PLD, A.ARM_INS_PLDW):
            return []  # A prefetch hint performs no architectural memory read.
        if (h1 & 0xfff0) == 0xe850 and (h2 & 0x0f00) == 0x0f00:
            rn, rt, offset = h1 & 15, h2 >> 12, (h2 & 255) * 4
            if rn == 15 or rt >= 13:
                return None
            return [f'int result=m8_native_ldrex(cpu,mem,arch,{rt},cpu->r[{rn}]+{offset}u);',
                    'if(result==2) goto unhandled;', 'if(result) goto fault;']
        if (h1 & 0xfff0) == 0xe840:
            rn, rt, rd, offset = h1 & 15, h2 >> 12, (h2 >> 8) & 15, (h2 & 255) * 4
            if rn == 15 or rt >= 13 or rd >= 13 or rd in (rn, rt):
                return None
            return [f'int result=m8_native_strex(cpu,mem,arch,{rd},cpu->r[{rt}],cpu->r[{rn}]+{offset}u);',
                    'if(result==2) goto unhandled;', 'if(result) goto fault;']
        if h1 == 0xf3ef and h2 & 0xf000 == 0x8000:  # MRS
            rd, sysm = (h2 >> 8) & 15, h2 & 255
            if rd >= 13 or sysm not in (*range(8),8,9,16,17,18,19,20):
                return None
            return ['uint32_t value;',
                    f'if(m8_native_mrs(cpu,arch,{sysm},&value)) goto unhandled;',
                    f'cpu->r[{rd}]=value;']
        if h1 & 0xfff0 == 0xf380 and h2 & 0xf000 == 0x8000:  # MSR
            rn, sysm, mask = h1 & 15, h2 & 255, (h2 >> 8) & 15
            # Changing CONTROL can create or discard a lazy FP context. The
            # playback handoff supplies an active context; writes stay outside
            # this bounded interpreter until that larger contract is checked.
            if rn >= 13 or sysm not in (*range(8),8,9,16,17,18,19):
                return None
            return [f'if(m8_native_msr(cpu,arch,{sysm},{mask},cpu->r[{rn}])) goto unhandled;']
        return None


def emit_case(translator, function, insn, profile_index=None):
    body = translator.instruction(function, insn)
    if body is None:
        return None
    raw = int.from_bytes(insn.bytes, 'little')
    rows = [f'case 0x{insn.address:x}u: {{ /* {insn.mnemonic} {insn.op_str} */',
            f'if(!m8_native_code_matches(mem,pc,{insn.size},0x{raw:x}u)) goto unhandled;',
            'executed=1;',
            f'special={int(translator.special(insn,Context(insn)) is not None)};']
    if profile_index is not None:
        rows += [f'profile_index={profile_index}u;']
    if insn.id != A.ARM_INS_IT:
        rows += ['if(in_it && !m8_condition(cpu->xpsr,(arch->itstate>>4)+1u)) {',
                 f'cpu->r[15]=0x{insn.address+insn.size:x}u;', 'executed=0; goto completed;', '}']
    if insn.id != A.ARM_INS_IT and insn.cc not in (A.ARM_CC_INVALID, A.ARM_CC_AL):
        rows += [f'if(!m8_condition(cpu->xpsr,{insn.cc})) {{',
                 f'cpu->r[15]=0x{insn.address+insn.size:x}u;', 'goto completed;', '}']
    fp = translator.floating(insn, Context(insn)) is not None
    if fp:
        rows += ['if(!(arch->control & 4u)) goto unhandled;',
                 'fp_env=m8_fp_enter(cpu); fp_active=1;']
    suppress = insn.size == 2 and insn.update_flags and \
        insn.id not in (A.ARM_INS_CMP,A.ARM_INS_CMN,A.ARM_INS_TST,A.ARM_INS_TEQ)
    if suppress:
        rows += ['if(in_it) {'] + translator.instruction(function, insn, True) + \
            ['} else {'] + body + ['}']
    else:
        rows += body
    rows += [f'cpu->r[15]=0x{insn.address+insn.size:x}u;', 'goto completed;', '}']
    return rows


def emit_profile_api(records, firmware_sha256):
    count = len(records)
    rows = ['#include <stdio.h>', '#include <inttypes.h>', '#include <errno.h>',
            f'static _Thread_local m8_native_pc_count native_pc_rows[{count or 1}]={{']
    rows += [f'{{0x{record["pc"]:x}u,0,0}},' for record in records] or ['{0,0,0},']
    rows += ['};',
             'uint32_t m8_native_pc_read(const m8_native_pc_count **rows) {',
             'if(rows) *rows=native_pc_rows;', f'return {count}u;', '}',
             'void m8_native_pc_reset(void) {',
             f'for(unsigned i=0;i<{count}u;i++) {{',
             'native_pc_rows[i].instructions=0; native_pc_rows[i].special=0;', '}', '}',
             'int m8_native_pc_write(const char *path) {',
             'if(!path) {errno=EINVAL; return -1;}',
             'FILE *output=fopen(path,"w"); if(!output) return -1;',
             'uint64_t total=0,total_special=0;',
             f'for(unsigned i=0;i<{count}u;i++) {{',
             'total+=native_pc_rows[i].instructions; total_special+=native_pc_rows[i].special;', '}',
             'int failed=fprintf(output,"{\\\"schema\\\":\\\"m8-native-pc/1\\\",'
             '\\\"firmware_sha256\\\":\\\"'+firmware_sha256+'\\\",'
             '\\\"total\\\":%" PRIu64 ",\\\"total_special\\\":%" PRIu64 '
             '",\\\"pcs\\\":[",total,total_special)<0;',
             'int first=1;',
             f'for(unsigned i=0;!failed && i<{count}u;i++) {{',
             'const m8_native_pc_count *row=&native_pc_rows[i];',
             'if(!row->instructions) continue;',
             'failed=fprintf(output,"%s{\\\"pc\\\":%" PRIu32 '
             '",\\\"instructions\\\":%" PRIu64 ",\\\"special\\\":%" PRIu64 '
             '"}",first?"":",",row->pc,row->instructions,row->special)<0;',
             'first=0;', '}',
             'if(!failed) failed=fputs("]}\\n",output)==EOF;',
             'if(fclose(output)) failed=1;', 'return failed?-1:0;', '}', '']
    return rows


def generate(firmware, profiles, extra_pcs, all_pcs, blocks=False, pc_profile=False):
    fw = Firmware(firmware)
    translator = StepTranslator(fw, [])
    function = SimpleNamespace(units={})
    pcs = set(extra_pcs)
    profile_inputs = []
    for path in profiles:
        raw = Path(path).read_bytes()
        profile = json.loads(raw)
        if profile.get('firmware_sha256') not in (None, hashlib.sha256(fw.data).hexdigest()):
            raise ValueError(f'firmware hash differs in {path}')
        profile_inputs.append({'path':str(path),'sha256':hashlib.sha256(raw).hexdigest()})
        for kind in ('jit','aot') if all_pcs else ('jit',):
            pcs.update(int(row['pc']) for row in profile.get(kind,[]))
    # A profile omits predicate-false members. The engine must still consume
    # their widths and advance ITSTATE, so include every observed IT member.
    it_members = set()
    for pc in list(pcs):
        insn = fw.decode(pc)
        if not insn or insn.id != A.ARM_INS_IT:
            continue
        mask = insn.bytes[0] & 15
        count = 4 - ((mask & -mask).bit_length() - 1)
        next_pc = pc + insn.size
        for _ in range(count):
            member = fw.decode(next_pc)
            if not member:
                break
            it_members.add(next_pc)
            next_pc += member.size
    pcs.update(it_members)
    groups = defaultdict(list)
    manifest = {'schema':'m8-native-step/1',
                'firmware_sha256':hashlib.sha256(fw.data).hexdigest(),
                'profiles':profile_inputs,'all_pcs':all_pcs,'blocks':blocks,
                'it_members_added':len(it_members),'instructions':[],
                'unsupported':[]}
    if pc_profile:
        manifest['pc_profile'] = True
    mnemonics = Counter()
    for pc in sorted(pcs):
        insn = fw.decode(pc)
        rows = emit_case(translator, function, insn,
                         len(manifest['instructions']) if pc_profile else None) if insn else None
        record = {'pc':pc, 'bytes':bytes(insn.bytes).hex() if insn else '',
                  'instruction':f'{insn.mnemonic} {insn.op_str}'.strip() if insn else 'decode failed'}
        if rows is None:
            manifest['unsupported'].append(record)
            continue
        manifest['instructions'].append(record)
        mnemonics[insn.insn_name()] += 1
        groups[pc >> 12].extend(rows)
    output = ['/* Generated by native_step.py. Default-off prototype only. */',
              '#include "native_step_special.h"', '']
    if pc_profile:
        output += emit_profile_api(manifest['instructions'],manifest['firmware_sha256'])
    boundaries = ((0x6005118c,0x60051192), (0x60005b98,0x60005bac),
                  (0x3205a,0x3205c), (0x6003b6b0,0x6003b6b4),
                  (0x33f9c,0x33f9c), (0x33fda,0x33fda), (0x21828,0x21828))
    if blocks:
        output += ['static inline int native_boundary(uint32_t pc) {', 'return '+
                   ' || '.join(f'pc==0x{first:x}u' if first == last else
                               f'(uint32_t)(pc-0x{first:x}u)<=0x{last-first:x}u'
                               for first,last in boundaries)+';', '}', '']
    for group, rows in sorted(groups.items()):
        output += [f'static int step_{group:x}(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch) {{']
        if pc_profile:
            output += ['unsigned profile_index=0;']
        if blocks:
            output += ['unsigned completed_units=0,total_executed=0,total_special=0;',
                       'uint32_t pc; int slow,in_it,advance_it,fp_active,executed,special;',
                       'm8_fp_env fp_env;', 'dispatch:',
                       'pc=cpu->r[15]; slow=0; in_it=arch->itstate!=0;',
                       'advance_it=in_it; fp_active=0; executed=0; special=0;']
        else:
            output += ['uint32_t pc=cpu->r[15]; int slow=0;',
                       'int in_it=arch->itstate!=0,advance_it=in_it,fp_active=0;',
                       'int executed=0,special=0;', 'm8_fp_env fp_env;']
        output += ['switch(pc) {'] + rows + \
                  ['default: goto unhandled;', '}', 'completed:',
                   'if(fp_active) m8_fp_leave(fp_env);',
                   'if(advance_it) m8_native_advance_it(cpu,arch);',
                   'm8_native_save_sp(cpu,arch);']
        if pc_profile:
            output += ['if(executed) {',
                       'native_pc_rows[profile_index].instructions++;',
                       'native_pc_rows[profile_index].special+=(uint64_t)special;', '}']
        if blocks:
            output += ['total_executed+=(unsigned)executed;',
                       'total_special+=(unsigned)(special && executed);',
                       'arch->last_executed=total_executed;',
                       'arch->last_special=total_special;',
                       f'if(slow || ++completed_units==256 || (cpu->r[15]>>12)!=0x{group:x}u) return 0;']
            if any(first >> 12 == group for first,_last in boundaries):
                output += ['if(native_boundary(cpu->r[15])) return 0;']
            output += ['goto dispatch;']
        else:
            output += ['arch->last_executed=(uint32_t)executed;',
                       'arch->last_special=(uint32_t)(special && executed);',
                       '(void)slow; return 0;']
        output += ['fault:', 'if(fp_active) m8_fp_leave(fp_env);',
                   'cpu->r[15]=pc; return 1;', 'unhandled:',
                   'return completed_units ? 0 : 2;' if blocks else 'return 2;',
                   '}', '']
    output += ['int m8_native_step(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch) {',
               'switch(cpu->r[15]>>12) {']
    for group in sorted(groups):
        output += [f'case 0x{group:x}u:return step_{group:x}(cpu,mem,arch);']
    output += ['default:return 2;', '}', '}', '']
    manifest['mnemonics'] = dict(sorted(mnemonics.items()))
    manifest['generated_instructions'] = len(manifest['instructions'])
    return '\n'.join(output), manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware',type=Path,required=True)
    parser.add_argument('--profile',type=Path,action='append',default=[])
    parser.add_argument('--pc',type=lambda value:int(value,0),action='append',default=[])
    parser.add_argument('--all-pcs',action='store_true',help='include AOT-attributed PCs as well as JIT PCs')
    parser.add_argument('--blocks',action='store_true',help='execute up to 256 instructions per page, with runtime boundary exits')
    parser.add_argument('--pc-profile',action='store_true',help='export diagnostic counters for completed executed guest PCs')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--manifest',type=Path,required=True)
    args = parser.parse_args()
    output, manifest = generate(args.firmware,args.profile,args.pc,args.all_pcs,args.blocks,args.pc_profile)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.manifest.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(output)
    args.manifest.write_text(json.dumps(manifest,indent=2)+'\n')
    print(f"native step: {manifest['generated_instructions']} cases, {len(manifest['unsupported'])} unsupported PCs")


if __name__ == '__main__':
    main()
