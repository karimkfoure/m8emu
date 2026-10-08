#!/usr/bin/env python3
"""Compile measured playback fallback PCs into default-off native regions.

Ordinary instructions use the unchanged AOT integer/FP emitter and scalarizer.
Each region dispatches once at entry. Static internal branches use C labels;
there is no per-instruction decode, dispatch or code-byte check. The caller
retains exact special handlers, runtime hooks and the code-invalidation owner.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
from types import SimpleNamespace

from capstone import arm as A
from translate import Context, Firmware, Translator

BOUNDARIES = ((0x6005118c, 0x60051192), (0x60005b98, 0x60005bac),
              (0x3205a, 0x3205c), (0x6003b6b0, 0x6003b6b4),
              (0x33f9c, 0x33f9c), (0x33fda, 0x33fda), (0x21828, 0x21828))
CONTROL = {A.ARM_INS_B, A.ARM_INS_BL, A.ARM_INS_BLX, A.ARM_INS_BX,
           A.ARM_INS_CBZ, A.ARM_INS_CBNZ, A.ARM_INS_TBB, A.ARM_INS_TBH}
EXIT_REASONS = {1: 'static_branch', 2: 'fallthrough', 3: 'computed_redirect',
                4: 'budget', 5: 'slow_memory', 6: 'qualification_or_hook', 7: 'fault'}


def exit_tag(enabled, source, target, reason):
    if not enabled:
        return []
    if reason in (1, 2) and boundary(target):
        reason = 6
    edge = f'M8_REGION_EDGE_{source:x}_{target:x}_{reason}' if reason in (1, 2) else '0'
    return [f'exit_source=0x{source:x}u; exit_reason={reason}u; exit_edge={edge};']


def emit_exit_profile(digest):
    return ['#include <inttypes.h>', '#include <stdio.h>',
            'enum { M8_REGION_EXIT_CAPACITY=65536 };',
            'typedef struct {uint32_t entry_pc,source_pc,exit_pc,reason,edge_id,itstate;'
            ' int result; uint64_t exits,executed_exits,instructions;} m8_region_exit_count;',
            'static _Thread_local m8_region_exit_count region_exit_rows[M8_REGION_EXIT_CAPACITY];',
            'static _Thread_local uint64_t region_exit_calls,region_exit_dropped;',
            'static void m8_native_region_exit_record(uint32_t entry,uint32_t source,uint32_t target,',
            ' uint32_t reason,uint32_t edge,uint32_t itstate,int result,uint32_t instructions) {',
            'region_exit_calls++;',
            'uint32_t hash=(entry*2654435761u) ^ (source*2246822519u) ^ (target*3266489917u);',
            'hash^=reason*668265263u ^ edge*374761393u ^ itstate*1274126177u ^ (uint32_t)result;',
            'for(unsigned probe=0;probe<M8_REGION_EXIT_CAPACITY;probe++) {',
            'm8_region_exit_count *row=&region_exit_rows[(hash+probe)&(M8_REGION_EXIT_CAPACITY-1)];',
            'if(!row->exits) {row->entry_pc=entry;row->source_pc=source;row->exit_pc=target;',
            'row->reason=reason;row->edge_id=edge;row->itstate=itstate;row->result=result;}',
            'else if(row->entry_pc!=entry || row->source_pc!=source || row->exit_pc!=target ||',
            ' row->reason!=reason || row->edge_id!=edge || row->itstate!=itstate || row->result!=result) continue;',
            'row->exits++;row->executed_exits+=(instructions!=0);row->instructions+=instructions;return;',
            '}', 'region_exit_dropped++;', '}',
            '__attribute__((visibility("default"))) void m8_native_region_exit_reset(void) {',
            'memset(region_exit_rows,0,sizeof region_exit_rows);region_exit_calls=region_exit_dropped=0;', '}',
            '__attribute__((visibility("default"))) int m8_native_region_exit_write(const char *path) {',
            'if(!path) return -1; FILE *file=fopen(path,"w");if(!file) return -1;',
            'uint64_t instructions=0,executed=0,unhandled=0,faults=0;',
            'for(unsigned i=0;i<M8_REGION_EXIT_CAPACITY;i++) {',
            'm8_region_exit_count *row=&region_exit_rows[i];instructions+=row->instructions;',
            'executed+=row->executed_exits;unhandled+=row->result==2?row->exits:0;',
            'faults+=row->result==1?row->exits:0;', '}',
            'int failed=fprintf(file,"{\\"schema\\":\\"m8-native-region-exit/1\\",'
            '\\"firmware_sha256\\":\\"'+digest+'\\",\\"calls\\":%" PRIu64 ',
            '",\\"executed_exits\\":%" PRIu64 ",\\"unhandled_exits\\":%" PRIu64 ',
            '",\\"fault_exits\\":%" PRIu64 ",\\"instructions\\":%" PRIu64 ',
            '",\\"dropped\\":%" PRIu64 ",\\"scope\\":\\"calling thread\\",\\"edges\\":[",',
            'region_exit_calls,executed,unhandled,faults,instructions,region_exit_dropped)<0;',
            'int first=1;',
            'static const char *reasons[]={"unknown","static_branch","fallthrough","computed_redirect",',
            '"budget","slow_memory","qualification_or_hook","fault"};',
            'for(unsigned i=0;!failed && i<M8_REGION_EXIT_CAPACITY;i++) {',
            'm8_region_exit_count *row=&region_exit_rows[i];if(!row->exits) continue;',
            'failed=fprintf(file,"%s{\\"entry_pc\\":%" PRIu32 ",\\"source_pc\\":%" PRIu32 ',
            '",\\"exit_pc\\":%" PRIu32 ",\\"reason\\":\\"%s\\",\\"reason_id\\":%" PRIu32 ',
            '",\\"edge_id\\":%" PRIu32 ",\\"itstate\\":%" PRIu32 ",\\"result\\":%d"',
            '",\\"exits\\":%" PRIu64 ",\\"executed_exits\\":%" PRIu64 ",\\"instructions\\":%" PRIu64 "}",',
            'first?"":",",row->entry_pc,row->source_pc,row->exit_pc,reasons[row->reason],',
            'row->reason,row->edge_id,row->itstate,row->result,row->exits,row->executed_exits,row->instructions)<0;',
            'first=0;', '}', 'if(!failed) failed=fputs("]}\\n",file)==EOF;',
            'if(fclose(file)) failed=1;return failed?-1:0;', '}', '']


def boundary(pc):
    return any(first <= pc <= last for first, last in BOUNDARIES)


def changes_pc(insn):
    return insn.id in CONTROL or A.ARM_REG_PC in insn.regs_access()[1]


def advance_it(state):
    return 0 if not state & 7 else (state & 0xe0) | ((state << 1) & 31)


@dataclass
class Unit:
    pc: int
    instructions: list
    next_pc: int
    itstate: int = 0

    @property
    def consumed(self):
        return len(self.instructions) + bool(self.itstate)


class RegionTranslator(Translator):
    def jump(self, function, pc):
        rows = ['completed++;']
        if pc in function.units and not boundary(pc):
            return rows + [f'goto L_{pc:x};']
        return rows + [f'cpu->r[15]=0x{pc:x}u;'] + \
            exit_tag(getattr(function, 'exit_metadata', False), function.current_pc, pc, 1) + ['goto done;']

    def instruction(self, function, insn, suppress_nzcv=False):
        function.current_pc = insn.address
        context = Context(insn)
        ops = insn.operands
        if insn.id == A.ARM_INS_BL:
            return [f'cpu->r[14]=0x{(insn.address + insn.size) | 1:x}u;'] + \
                self.jump(function, ops[0].imm & ~1)
        if insn.id in (A.ARM_INS_BX, A.ARM_INS_BLX):
            if ops[0].type != A.ARM_OP_REG:
                return None
            rows = [f'uint32_t target={context.r(ops[0].reg)};',
                    'if(!(target & 1u)) goto fault;']
            if insn.id == A.ARM_INS_BLX:
                rows += [f'cpu->r[14]=0x{(insn.address + insn.size) | 1:x}u;']
            return rows + ['cpu->r[15]=target & ~1u;', 'goto redirect;']
        body = super().instruction(function, insn, suppress_nzcv)
        inline_services = getattr(self, 'inline_services', False)
        if body is None and (inline_services or getattr(self, 'ram_exclusives', False)):
            from native_region_services import instruction as service_instruction
            body = service_instruction(insn, ram_only=not inline_services)
        if body is not None and insn.id == A.ARM_INS_POP and any(
                op.type == A.ARM_OP_REG and op.reg == A.ARM_REG_PC for op in ops):
            # The existing AOT emitter gives POP-to-PC a direct return. A
            # region uses the common computed-transfer edge to count it.
            body = [line.replace('goto done;', 'goto redirect;') for line in body]
        return body


def collect(fw, profiles, extra_pcs, pc_files=()):
    pcs, inputs, weights = set(extra_pcs), [], Counter()
    digest = hashlib.sha256(fw.data).hexdigest()
    for path in profiles:
        raw = Path(path).read_bytes()
        data = json.loads(raw)
        if data.get('firmware_sha256') not in (None, digest):
            raise ValueError(f'firmware identity differs: {path}')
        if data.get('schema') == 'm8-native-pc/1':
            rows = data['pcs']
        elif data.get('schema') == 'm8-guest-pc/1':
            rows = data['jit']
        else:
            raise ValueError(f'unsupported profile schema: {path}')
        inputs.append({'path': str(path), 'sha256': hashlib.sha256(raw).hexdigest()})
        for row in rows:
            if row['instructions']:
                pcs.add(int(row['pc']))
                weights[int(row['pc'])] += int(row['instructions'])
    for path in pc_files:
        raw = Path(path).read_bytes()
        inputs.append({'path': str(path), 'sha256': hashlib.sha256(raw).hexdigest()})
        for line in raw.decode().splitlines():
            if line.startswith('# firmware_sha256='):
                if line.split('=', 1)[1].strip() != digest:
                    raise ValueError(f'firmware identity differs: {path}')
            fields = line.partition('#')[0].split()
            if not fields:
                continue
            pc = int(fields[0], 0)
            pcs.add(pc)
            weights[pc] += int(fields[1], 0) if len(fields) > 1 else 0
    return pcs, inputs, weights


def units_from_pcs(fw, pcs):
    units, members, added = {}, set(), set()
    for pc in sorted(pcs):
        insn = fw.decode(pc)
        if insn is None:
            continue
        if insn.id != A.ARM_INS_IT:
            units[pc] = Unit(pc, [insn], pc + insn.size)
            continue
        state = insn.bytes[0]
        count = 4 - ((state & 15 & -(state & 15)).bit_length() - 1)
        decoded = list(fw.md.disasm(fw.code(pc, 20), pc, count=count + 1))
        if len(decoded) != count + 1:
            continue
        units[pc] = Unit(pc, decoded[1:], decoded[-1].address + decoded[-1].size, state)
        members.update(i.address for i in decoded[1:])
        added.update(i.address for i in decoded[1:] if i.address not in pcs)
    for pc in members:
        units.pop(pc, None)
    return units, members, added


def supported_body(translator, function, unit):
    result = []
    for i, insn in enumerate(unit.instructions):
        # An IT block ends on a control transfer. Refuse malformed encodings
        # before effects, rather than guessing its surviving ITSTATE.
        if unit.itstate and changes_pc(insn) and i != len(unit.instructions) - 1:
            return None
        suppress = bool(unit.itstate and insn.size == 2 and insn.update_flags and
                        insn.id not in (A.ARM_INS_CMP, A.ARM_INS_CMN,
                                        A.ARM_INS_TST, A.ARM_INS_TEQ))
        body = translator.instruction(function, insn, suppress)
        if body is None:
            return None
        result.append(body)
    return result


def leaders_and_bounds(units, inline_services=False, ram_exclusives=False):
    from native_region_services import boundary as service_boundary
    leaders, previous = set(), None
    for pc, unit in sorted(units.items()):
        if previous is None or previous.next_pc != pc or previous.pc >> 12 != pc >> 12 or \
                changes_pc(previous.instructions[-1]) or boundary(pc) or \
                ((inline_services or ram_exclusives) and
                 service_boundary(previous.instructions[-1], ram_only=not inline_services)):
            leaders.add(pc)
        for insn in unit.instructions:
            if insn.id in (A.ARM_INS_B, A.ARM_INS_BL, A.ARM_INS_CBZ, A.ARM_INS_CBNZ):
                target = insn.operands[-1].imm & ~1
                if target in units:
                    leaders.add(target)
        previous = unit
    # A bounded straight-line group prevents a large root watchdog gap. Check
    # the budget at group entries, never after each ordinary instruction.
    size = 0
    for pc, unit in sorted(units.items()):
        if pc in leaders or size + unit.consumed > 32:
            leaders.add(pc)
            size = 0
        size += unit.consumed
    bounds = {}
    active = None
    for pc, unit in sorted(units.items()):
        if pc in leaders:
            active = pc
            bounds[active] = 0
        bounds[active] += unit.consumed
    return bounds


def publish_it(state):
    return [f'arch->itstate=0x{state:x}u;',
            f'cpu->xpsr=(cpu->xpsr & ~0x0600fc00u) | 0x{((state & 3) << 25) | ((state & 0xfc) << 8):x}u;']


def emit_region(translator, page, units, budget, exit_profile=False, chain_ready=False):
    metadata = exit_profile or chain_ready
    function = SimpleNamespace(units=units, exit_metadata=metadata)
    bodies = {pc: supported_body(translator, function, unit) for pc, unit in units.items()}
    supported = {pc: unit for pc, unit in units.items() if bodies[pc] is not None}
    function.units = supported
    # Regenerate jumps after removing special units. Internal transfers to a
    # special PC publish it as a runtime return, with no instruction effects.
    bodies = {pc: supported_body(translator, function, unit) for pc, unit in supported.items()}
    bounds = leaders_and_bounds(supported, getattr(translator, 'inline_services', False),
                               getattr(translator, 'ram_exclusives', False)) if supported else {}
    name = f'region_{page:x}' + ('_impl' if chain_ready else '')
    rows = [f'static int {name}(m8_cpu *restrict cpu,m8_mem *restrict mem) {{',
            'unsigned completed=0,units_used=0; int slow=0;', 'uint32_t pc=cpu->r[15];']
    if metadata:
        rows += ['uint32_t exit_source=pc,exit_reason=6,exit_edge=0;']
    if exit_profile:
        rows += ['uint32_t entry_pc=pc;']
    if chain_ready:
        rows += ['if(arch->itstate || m8_native_it_from_xpsr(cpu->xpsr)) goto done;']
    rows += ['switch(pc) {']
    rows += [f'case 0x{pc:x}u: goto L_{pc:x};' for pc in sorted(supported)]
    rows += ['default: goto done;', '}']
    records = []
    for pc, unit in sorted(supported.items()):
        rows += [f'L_{pc:x}:', '{']
        if pc in bounds:
            if metadata:
                rows += [f'if(units_used+{bounds[pc]}u>{budget}u) {{cpu->r[15]=0x{pc:x}u;'] + \
                    exit_tag(True, pc, pc, 4) + ['goto done;}']
            else:
                rows += [f'if(units_used+{bounds[pc]}u>{budget}u) {{cpu->r[15]=0x{pc:x}u; goto done;}}']
        rows += [f'units_used+={unit.consumed}u;']
        if unit.itstate:
            rows += ['completed++; /* IT instruction */']
            records.append({'pc': pc, 'bytes': translator.fw.code(pc, 2).hex()})
        state = unit.itstate
        for insn, original in zip(unit.instructions, bodies[pc]):
            rows += ['{', f'/* {insn.address:08x}: {insn.mnemonic} {insn.op_str} */']
            records.append({'pc': insn.address, 'bytes': insn.bytes.hex()})
            predicate = unit.itstate and insn.cc not in (A.ARM_CC_INVALID, A.ARM_CC_AL)
            if predicate:
                rows += [f'if(m8_condition(cpu->xpsr,{insn.cc})) {{']
            elif insn.cc not in (A.ARM_CC_INVALID, A.ARM_CC_AL):
                rows += [f'if(m8_condition(cpu->xpsr,{insn.cc})) {{']
            # FP lazy context creation belongs to the special handler. No
            # ordinary prefix is replayed when this qualification fails.
            if translator.floating(insn, Context(insn)) is not None:
                rows += [f'if(!(arch->control & 4u)) {{cpu->r[15]=0x{insn.address:x}u;']
                if state:
                    rows += publish_it(state)
                rows += exit_tag(metadata, insn.address, insn.address, 6)
                rows += ['goto done;', '}']
            # All emitted multiword stores use consecutive four-byte writes.
            # Reject address wrap before any effect: a later code-write
            # handoff must not replay an earlier RAM or device store.
            writes = sum('m8_write(' in line for line in original)
            if writes > 1:
                guarded = []
                for line in original:
                    guarded.append(line)
                    address = re.search(r'uint32_t (addr|fp_addr) =', line)
                    if address:
                        guarded.append(f'if((uint64_t){address[1]}+{writes * 4}u>'
                                       'UINT64_C(0x100000000)) goto fault;')
                original = guarded
            body = []
            for line in original:
                if 'goto services_unhandled;' in line:
                    decline = [f'cpu->r[15]=0x{insn.address:x}u;'] + \
                        (publish_it(state) if state else []) + exit_tag(metadata, insn.address, insn.address, 6) + ['goto done;']
                    line = line.replace('goto services_unhandled;', '{' + ' '.join(decline) + '}')
                line = line.replace('m8_read(', 'm8_native_region_read(').replace('m8_write(', 'm8_native_region_write(')
                line = line.replace('goto redirect;', 'completed++; goto redirect;')
                if metadata:
                    line = line.replace('goto redirect;', f'exit_source=0x{insn.address:x}u; goto redirect;')
                if 'goto fault;' in line:
                    fault = [f'pc=0x{insn.address:x}u;'] + (publish_it(state) if state else []) + ['goto fault;']
                    line = line.replace('goto fault;', '{' + ' '.join(fault) + '}')
                body.append(line)
            rows += body
            # Static branches include their own count before the taken jump;
            # the fall-through path reaches this count exactly once.
            rows += ['completed++;']
            if insn.cc not in (A.ARM_CC_INVALID, A.ARM_CC_AL):
                rows += ['}']
                if not predicate:
                    rows += ['else {completed++;}']
            next_state = advance_it(state) if state else 0
            if any('m8_native_region_read(' in s or 'm8_native_region_write(' in s for s in body):
                rows += [f'if(slow) {{cpu->r[15]=0x{insn.address + insn.size:x}u;']
                if state:
                    rows += publish_it(next_state)
                rows += exit_tag(metadata, insn.address, insn.address + insn.size, 5)
                rows += ['goto done;', '}']
            rows += ['}']
            state = next_state
        # Ordinary fall-through does not add another guest instruction.
        if unit.next_pc in supported and not boundary(unit.next_pc):
            rows += [f'goto L_{unit.next_pc:x};']
        else:
            if metadata:
                rows += [f'cpu->r[15]=0x{unit.next_pc:x}u;'] + \
                    exit_tag(True, unit.instructions[-1].address, unit.next_pc, 2) + ['goto done;']
            else:
                rows += [f'cpu->r[15]=0x{unit.next_pc:x}u; goto done;']
        rows += ['}']
    rows += ['redirect:']
    if metadata:
        rows += ['exit_reason=slow?5u:3u; exit_edge=0;']
        # A computed transfer can target a hook. Preserve the same required
        # return as a static edge to that address; it has no chain edge ID.
        rows += ['if(!slow && (' + ' || '.join(f'(uint32_t)(cpu->r[15]-0x{first:x}u)<=0x{last-first:x}u'
                                              for first, last in BOUNDARIES) + ')) exit_reason=6u;']
    rows += ['goto done;', 'fault:', 'cpu->r[15]=pc;']
    if metadata:
        rows += ['exit_source=pc; exit_reason=7; exit_edge=0;']
    if chain_ready:
        rows += ['if(out) {out->source_pc=exit_source;out->target_pc=cpu->r[15];',
                 'out->reason=exit_reason;out->edge_id=exit_edge;}']
    if exit_profile:
        rows += ['m8_native_region_exit_record(entry_pc,exit_source,cpu->r[15],exit_reason,exit_edge,',
                 'arch->itstate,1,completed);']
    rows += ['arch->last_executed=completed; arch->last_special=0; return 1;', 'done:']
    if chain_ready:
        rows += ['if(out) {out->source_pc=exit_source;out->target_pc=cpu->r[15];',
                 'out->reason=exit_reason;out->edge_id=exit_edge;}']
    if exit_profile:
        rows += ['m8_native_region_exit_record(entry_pc,exit_source,cpu->r[15],exit_reason,exit_edge,',
                 'arch->itstate,completed?0:2,completed);']
    rows += ['arch->last_executed=completed; arch->last_special=0;',
             'return completed ? 0 : 2;', '}']
    source = Translator.scalarize('\n'.join(rows))
    arguments = 'm8_cpu *restrict output,m8_mem *restrict mem,m8_native_arch *arch'
    if chain_ready:
        arguments += ',m8_native_region_exit *out'
    source = source.replace('m8_cpu *restrict output,m8_mem *restrict mem) {', arguments + ') {')
    if chain_ready:
        source += f'\nstatic int region_{page:x}(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch) {{' \
                  f'return {name}(cpu,mem,arch,NULL);}}\n'
    return source, supported, records


def has_fp_instructions(translator, units):
    return any(insn.mnemonic.lower().startswith(('v', 'f')) or
               translator.floating(insn, Context(insn)) is not None
               for unit in units.values() for insn in unit.instructions)


def generate(firmware, profiles, extra_pcs=(), budget=256, pc_files=(), exit_profile=False, chain_ready=False, layout=None, inline_services=False, ram_exclusives=False):
    if not 32 <= budget <= 4096:
        raise ValueError('budget must be between 32 and 4096')
    fw = Firmware(firmware)
    translator = RegionTranslator(fw, [])
    translator.inline_services = inline_services
    translator.ram_exclusives = ram_exclusives
    pcs, inputs, weights = collect(fw, profiles, extra_pcs, pc_files)
    units, members, added = units_from_pcs(fw, pcs)
    pages = defaultdict(dict)
    for pc, unit in units.items():
        if (unit.next_pc - 1) >> 12 != pc >> 12:
            continue
        pages[pc >> 12][pc] = unit
    groups = [[page] for page in sorted(pages)]
    layout_info = None
    if layout:
        from native_region_layout import validate
        raw = Path(layout).read_bytes()
        inventory, compiled_pages = {}, {}
        for page, page_units in sorted(pages.items()):
            function = SimpleNamespace(units=page_units)
            supported = {pc: unit for pc, unit in page_units.items()
                         if supported_body(translator, function, unit) is not None}
            if supported:
                compiled_pages[page] = supported
                inventory[page] = {'has_fp': has_fp_instructions(translator, supported),
                                   'instruction_positions': sum(unit.consumed for unit in supported.values())}
        groups = validate(json.loads(raw), hashlib.sha256(fw.data).hexdigest(), inventory)
        pages = compiled_pages
        layout_info = {'path': str(layout), 'sha256': hashlib.sha256(raw).hexdigest(),
                       'max_guest_page_bytes': 16384, 'max_instruction_positions': 8192}
    rows = ['/* Generated by native_regions.py. Default-off playback only. */',
            '#include "native_regions.h"', '#include "integer.h"', '#include "fp.h"',
            '#include "native_region_memory.h"', '']
    if inline_services or ram_exclusives:
        rows += ['#include "native_region_services.h"',
                 'int m8_native_region_ram_exclusives(void) {return 1;}', '']
    if inline_services:
        rows += ['int m8_native_region_inline_services(void) {return 1;}', '']
    if exit_profile:
        rows += emit_exit_profile(hashlib.sha256(fw.data).hexdigest())
    manifest = {'schema': 'm8-native-regions/1', 'firmware_sha256': hashlib.sha256(fw.data).hexdigest(),
                'profiles': inputs, 'budget': budget, 'input_pcs': len(pcs),
                'it_members_added': sorted(added), 'regions': [], 'declined': []}
    if inline_services:
        manifest['inline_services'] = True
    if inline_services or ram_exclusives:
        manifest['ram_exclusives'] = True
    checks, eligible = {}, set()
    for group in groups:
        page = group[0]
        page_units = {pc: unit for member in group for pc, unit in pages[member].items()}
        source, supported, records = emit_region(translator, page, page_units, budget, exit_profile, chain_ready)
        if not supported:
            continue
        rows += [source, '']
        eligible.update(supported)
        checks.update((row['pc'], row) for row in records)
        # Keep FP memory transfers and register moves in the FP environment,
        # even when they do not perform arithmetic. Aliases such as fmstat
        # also require it. Classify only instructions in the compiled body.
        has_fp = has_fp_instructions(translator, supported)
        region = {'page': page, 'entries': sorted(supported),
                  'instructions': sorted(row['pc'] for row in records), 'has_fp': has_fp}
        if layout:
            region.update(pages=group, guest_page_bytes=len(group) * 4096,
                          instruction_positions=sum(unit.consumed for unit in supported.values()))
        manifest['regions'].append(region)
    if layout:
        manifest['layout'] = layout_info
        manifest['page_aliases'] = sorted(
            ({'page': page, 'group_page': region['page'], 'has_fp': region['has_fp']}
             for region in manifest['regions'] for page in region['pages']),
            key=lambda row: row['page'])
    for pc in sorted(pcs - eligible):
        insn = fw.decode(pc)
        reason = 'live IT member requires exact handler' if pc in members else \
            'special instruction, unsupported encoding, or page-crossing unit'
        manifest['declined'].append({'pc': pc, 'reason': reason,
                                     'instruction': f'{insn.mnemonic} {insn.op_str}' if insn else 'decode failed'})
    rows += ['int m8_native_region_step(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch) {',
             'arch->last_executed=arch->last_special=0;']
    if exit_profile:
        rows += ['uint32_t entry_pc=cpu->r[15];uint64_t calls_before=region_exit_calls;',
                 'if(arch->itstate || m8_native_it_from_xpsr(cpu->xpsr)) {',
                 'm8_native_region_exit_record(entry_pc,entry_pc,entry_pc,6,0,arch->itstate,2,0);return 2;}']
    else:
        rows += ['if(arch->itstate || m8_native_it_from_xpsr(cpu->xpsr)) return 2;']
    rows += [
             'int result; m8_fp_env fp_env;',
             '#if defined(M8_NATIVE_REGION_INTEGER_FP_FASTPATH) && M8_NATIVE_REGION_INTEGER_FP_FASTPATH',
             'int fp_active=0;', '#else', 'fp_env=m8_fp_enter(cpu);', '#endif',
             'switch(cpu->r[15]>>12) {']
    for region in manifest['regions']:
        rows += [f'case 0x{page:x}u:' for page in region.get('pages', [region['page']])]
        if region['has_fp']:
            rows += ['#if defined(M8_NATIVE_REGION_INTEGER_FP_FASTPATH) && M8_NATIVE_REGION_INTEGER_FP_FASTPATH',
                     'fp_env=m8_fp_enter(cpu); fp_active=1;', '#endif']
        rows += [f'result=region_{region["page"]:x}(cpu,mem,arch); break;']
    rows += ['default: result=2; break;', '}',
             '#if defined(M8_NATIVE_REGION_INTEGER_FP_FASTPATH) && M8_NATIVE_REGION_INTEGER_FP_FASTPATH',
             'if(fp_active) m8_fp_leave(fp_env);', '#else', 'm8_fp_leave(fp_env);', '#endif',
             'if(result!=2) {', 'if(!arch->ipsr && (arch->control & 2u)) arch->psp=cpu->r[13];',
             'else arch->msp=cpu->r[13];', '}']
    if exit_profile:
        rows += ['if(region_exit_calls==calls_before)',
                 'm8_native_region_exit_record(entry_pc,entry_pc,cpu->r[15],6,0,arch->itstate,result,arch->last_executed);']
    rows += ['return result;', '}', '']
    if chain_ready:
        rows += ['static m8_native_region_fn m8_native_region_chain_lookup(uint32_t pc) {',
                 'switch(pc>>12) {']
        rows += [f'case 0x{page:x}u:return region_{region["page"]:x}_impl;'
                 for region in manifest['regions'] for page in region.get('pages', [region['page']])]
        rows += ['default:return NULL;', '}', '}', '']
    rows += ['int m8_native_region_verify_memory(m8_mem *mem,uint32_t *bad_pc) {',
             'static const struct {uint32_t pc; uint8_t size,bytes[4];} checks[]={']
    rows += [f'{{0x{pc:x}u,{len(bytes.fromhex(record["bytes"]))},'
             '{'+','.join(str(b) for b in bytes.fromhex(record['bytes']))+'}},'
             for pc, record in sorted(checks.items())]
    if not checks:
        rows += ['{0,0,{0}},']
    rows += ['};', f'for(unsigned i=0;i<{len(checks)}u;i++) {{',
             'uint8_t *p=m8_region_pointer(&mem->itcm,checks[i].pc,checks[i].size);',
             'if(!p) p=m8_region_pointer(&mem->flash,checks[i].pc,checks[i].size);',
             'if(!p || memcmp(p,checks[i].bytes,checks[i].size)) {',
             'if(bad_pc) *bad_pc=checks[i].pc; return 1;', '}', '}', 'return 0;', '}', '']
    manifest['compiled_instructions'] = len(checks)
    manifest['eligible_profile_instructions'] = sum(weights[pc] for pc in checks)
    manifest['profile_instructions'] = sum(weights.values())
    manifest['entry_only_dispatch'] = True
    if exit_profile or chain_ready:
        body = '\n'.join(rows)
        names = sorted(set(re.findall(r'M8_REGION_EDGE_([0-9a-f]+)_([0-9a-f]+)_([12])', body)),
                       key=lambda row: tuple(int(value, 16) for value in row))
        edges = []
        for identifier, (source, target, reason) in enumerate(names, 1):
            token = f'M8_REGION_EDGE_{source}_{target}_{reason}'
            body = body.replace(token + ';', f'{identifier}u;')
            edges.append({'edge_id': identifier, 'source_pc': int(source, 16),
                          'target_pc': int(target, 16), 'reason': int(reason)})
        rows = [body]
        if chain_ready:
            rows += [f'static const unsigned m8_native_region_chain_edge_count={len(edges)+1}u;']
        manifest['exit_profile'] = exit_profile
        manifest['chain_ready'] = chain_ready
        manifest['exit_reasons'] = EXIT_REASONS
        manifest['static_exit_edges'] = edges
    return '\n'.join(rows), manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware', type=Path, required=True)
    parser.add_argument('--profile', type=Path, action='append', default=[])
    parser.add_argument('--pc-file', type=Path, action='append', default=[],
                        help='committed exact-PC fragment; optional instruction count column')
    parser.add_argument('--pc', type=lambda value: int(value, 0), action='append', default=[])
    parser.add_argument('--budget', type=int, default=256)
    parser.add_argument('--exit-profile', action='store_true', help='count exits by source/target/reason; do not use for timing')
    parser.add_argument('--chain-ready', action='store_true', help='emit private implementations and exact exit metadata; no chaining')
    parser.add_argument('--inline-services', action='store_true', help='compile qualified synchronous barriers, hints and RAM-word exclusives')
    parser.add_argument('--ram-exclusives', action='store_true', help='compile qualified RAM-word exclusives; retain barrier and prefetch exits')
    parser.add_argument('--shared-state', action='store_true', help='emit the default-off shared architectural register copy')
    parser.add_argument('--layout', type=Path, help='explicit default-off page grouping from native_region_layout.py')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    source, manifest = generate(args.firmware, args.profile, args.pc, args.budget, args.pc_file,
                                args.exit_profile, args.chain_ready, args.layout, args.inline_services,
                                args.ram_exclusives)
    if args.shared_state:
        from native_region_state import transform
        source, manifest['shared_state'] = transform(source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(source)
    args.manifest.write_text(json.dumps(manifest, indent=2) + '\n')
    print(f"native regions: {len(manifest['regions'])} pages, "
          f"{manifest['compiled_instructions']} instructions")


if __name__ == '__main__':
    main()
