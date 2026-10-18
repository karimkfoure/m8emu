#!/usr/bin/env python3
"""Find firmware CFG coverage and generate separate cold translation units.

Direct CFG reachability, interpreted function roots, observed roots and possible
code pointers retain separate provenance. Computed edges remain explicit gaps.
This is static translation coverage, not a complete runtime qualification.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict, deque
from dataclasses import dataclass
import gzip
import hashlib
import json
from pathlib import Path
import re
from types import SimpleNamespace

from capstone import arm as A
from native_step import StepTranslator
from translate import Context, Firmware, ITCM_OFFSET, ITCM_SIZE

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = ROOT / 'aot/catalogs/static-coverage.json'
KINDS = {'reset': 1, 'vector': 1, 'function': 2, 'observed': 4, 'pointer': 8,
         'explicit': 16}
PUBLIC_REGION = ('m8_native_region_step', 'm8_native_region_verify_memory')
EXACT_BOOT_PCS = frozenset((0x600059e0, 0x6005f7aa))


def number(value):
    return int(value, 0) if isinstance(value, str) else int(value)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    raw = Path(path).read_bytes()
    return json.loads(gzip.decompress(raw) if str(path).endswith('.gz') else raw)


def read_pcs(path, identity):
    pcs = set()
    for line in Path(path).read_text().splitlines():
        if line.startswith('# firmware_sha256=') and line.split('=', 1)[1].strip() != identity:
            raise ValueError(f'PC input firmware differs: {path}')
        fields = line.partition('#')[0].split()
        if fields:
            pcs.add(int(fields[0], 0))
    return pcs


@dataclass(frozen=True)
class Mapping:
    name: str
    source: int
    destination: int
    size: int
    executable: bool

    def contains(self, pc, size=1):
        return self.destination <= pc and pc + size <= self.destination + self.size

    def row(self):
        return dict(name=self.name, source=self.source, destination=self.destination,
                    size=self.size, executable=self.executable)


class Coverage:
    """Read only known image-backed maps; never invent bytes for RAM or ROM."""

    def __init__(self, firmware, config):
        self.fw = Firmware(firmware)
        self.identity = sha(self.fw.data)
        if config.get('schema') != 'm8-static-coverage-input/1':
            raise ValueError('Wrong static coverage input schema')
        if config.get('firmware_sha256') != self.identity:
            raise ValueError('Static coverage firmware differs')
        self.config = config
        self.flash_base = number(config.get('flash_base', 0x60000000))
        self.maps = [Mapping('flash', self.flash_base, self.flash_base, len(self.fw.data), True)]
        for row in config.get('copied_maps', ()):
            mapping = Mapping(row['name'], number(row['source']), number(row['destination']),
                              number(row['size']), bool(row['executable']))
            if mapping.size <= 0 or not self.maps[0].contains(mapping.source, mapping.size):
                raise ValueError(f'Copied map source outside firmware: {mapping.name}')
            if any(mapping.destination < old.destination + old.size and
                   old.destination < mapping.destination + mapping.size for old in self.maps):
                raise ValueError(f'Overlapping copied map: {mapping.name}')
            for guard in row.get('guards', ()):
                expected = bytes.fromhex(guard['bytes'])
                if self.read(number(guard['address']), len(expected)) != expected:
                    raise ValueError(f'Copied map guard differs: {mapping.name} at {guard["address"]}')
            self.maps.append(mapping)
        self.instructions = {}
        self.origins = defaultdict(int)
        self.roots = defaultdict(lambda: defaultdict(set))
        self.edges = set()
        self.unknown = set()
        self.data = set()
        self.tables = {}
        self.it_members = set()
        self.block_entries = set()
        self.queue = deque()
        self.inputs = {}
        self.required = set()
        self.hot = set()
        self.boot_pcs = set()
        self.boot_weights = Counter()

    def mapping(self, pc, size=1, executable=False):
        return next((m for m in self.maps if m.contains(pc, size) and
                     (m.executable or not executable)), None)

    def read(self, address, size):
        mapping = self.mapping(address, size)
        if not mapping:
            return b''
        offset = mapping.source - self.flash_base + address - mapping.destination
        return self.fw.data[offset:offset + size]

    def decode(self, pc):
        if pc & 1 or not self.mapping(pc, 2, executable=True):
            return None
        raw = self.read(pc, 4) or self.read(pc, 2)
        return next(self.fw.md.disasm(raw, pc, count=1), None)

    def root(self, pc, kind, source):
        pc = number(pc) & ~1
        self.roots[pc][kind].add(str(source))
        if kind in ('function', 'reset', 'vector', 'explicit'):
            self.block_entries.add(pc)
        self.queue.append((pc, KINDS[kind]))

    def target(self, source, target, kind):
        target &= 0xffffffff
        if target & 1:
            self.unknown.add((source, 'invalid_direct_alignment', target))
            return None
        self.edges.add((source, target, kind))
        if kind not in ('fallthrough', 'call_continuation', 'it_false_fallthrough'):
            self.block_entries.add(target)
        if not self.mapping(target, 2, executable=True):
            self.unknown.add((source, 'target_outside_known_executable_maps', target))
            return None
        return target

    def input(self, path):
        self.inputs[str(Path(path).resolve())] = sha(Path(path).read_bytes())

    def seed(self, base=ROOT, pc_files=(), function_files=(), extra_roots=(), hot_files=()):
        def paths(key):
            for pattern in self.config.get(key, ()):
                found = sorted(Path(base).glob(pattern))
                if not found:
                    raise ValueError(f'Static coverage input pattern has no files: {pattern}')
                yield from found
        for word in self.config.get('reset_words', ()):
            address = number(word)
            raw = self.read(address, 4)
            if len(raw) != 4 or not int.from_bytes(raw, 'little') & 1:
                raise ValueError(f'Invalid reset word at 0x{address:x}')
            self.root(int.from_bytes(raw, 'little'), 'reset', f'word:0x{address:x}')
        for table in self.config.get('vector_tables', ()):
            start, count = number(table['address']), number(table['count'])
            # Slot zero is the stack pointer in a Cortex-M vector table.
            for index in range(number(table.get('first_handler', 1)), count):
                word = int.from_bytes(self.read(start + 4 * index, 4), 'little')
                if word in (0, 0xffffffff):
                    continue
                if not word & 1:
                    raise ValueError(f'Non-Thumb vector at 0x{start + 4 * index:x}')
                self.root(word, 'vector', f'vector:0x{start + 4 * index:x}')
        for path in [*paths('function_files'), *function_files]:
            self.input(path)
            if Path(path).name.endswith('-pcs.txt'):
                continue
            for line in Path(path).read_text().splitlines():
                fields = line.partition('#')[0].split()
                # PC-only function fragments are observed inputs, not bounds.
                if len(fields) >= 2 and int(fields[1], 0) > int(fields[0], 0):
                    self.root(int(fields[0], 0), 'function', path)
        for path in [*paths('observed_pc_files'), *pc_files]:
            self.input(path)
            pcs = read_pcs(path, self.identity)
            self.required.update(pcs)
            for pc in pcs:
                self.root(pc, 'observed', path)
        for key in ('observed_profiles', 'observed_manifests'):
            selected_paths = list(paths(key))
            if key == 'observed_profiles':
                from boot_regions import boot_weights
                self.boot_weights = boot_weights(selected_paths, self.identity)
            for path in selected_paths:
                self.input(path)
                value = read_json(path)
                if value.get('firmware_sha256') != self.identity:
                    raise ValueError(f'Observed firmware differs: {path}')
                for field in ('jit', 'aot', 'pcs', 'instructions', 'unsupported'):
                    for row in value.get(field, ()):
                        pc = number(row['pc'])
                        self.required.add(pc)
                        if key == 'observed_profiles':
                            self.boot_pcs.add(pc)
                        self.root(pc, 'observed', path)
        for path in paths('boot_weight_files'):
            self.input(path)
            selected = read_pcs(path, self.identity)
            for line in Path(path).read_text().splitlines():
                fields = line.partition('#')[0].split()
                if not fields:
                    continue
                if len(fields) != 2:
                    raise ValueError(f'Boot PC needs an instruction weight: {path}')
                pc, count = (int(field, 0) for field in fields)
                if pc & 1 or not 0 <= pc <= 0xffffffff or count < 0:
                    raise ValueError(f'Invalid boot PC/weight: {path} {pc} {count}')
                self.boot_weights[pc] += count
            self.boot_pcs.update(selected)
            self.required.update(selected)
            for pc in selected:
                self.root(pc, 'observed', path)
        for path in paths('provenance_files'):
            self.input(path)
        for path in [*paths('hot_pc_files'), *hot_files]:
            self.input(path)
            self.hot.update(read_pcs(path, self.identity))
        for pc in extra_roots:
            self.root(pc, 'explicit', 'command-line')
        if self.config.get('pointer_scan', False):
            # Aligned words include vtables, constructors, vector assignments
            # and copied tables. A matching value is a possible root only.
            # No instruction decoder sweep is used to establish reachability.
            for mapping in self.maps:
                raw = self.read(mapping.destination, mapping.size)
                for offset in range((-mapping.destination) % 4, len(raw) - 3, 4):
                    word = int.from_bytes(raw[offset:offset + 4], 'little')
                    pc = word & ~1
                    if not word & 1 or pc < 0x20 or not self.mapping(pc, 2, executable=True):
                        continue
                    if self.read(pc, 4) in (b'\0' * 4, b'\xff' * 4) or not self.decode(pc):
                        continue
                    self.root(pc, 'pointer', f'{mapping.name}:0x{mapping.destination + offset:x}')

    def prior(self, pc, limit=12):
        """Find an unambiguous straight-line prefix already decoded by the CFG."""
        result = []
        for _ in range(limit):
            if pc in self.block_entries:
                break
            choices = [self.instructions[prev] for prev in (pc - 2, pc - 4)
                       if prev in self.instructions and prev + self.instructions[prev].size == pc]
            if len(choices) != 1:
                break
            insn = choices[0]
            result.append(insn)
            pc = insn.address
        return result

    def constant(self, reg, pc, seen=None):
        """Resolve local immediate/address/literal definitions, without RAM reads."""
        seen = set() if seen is None else seen
        if (reg, pc) in seen:
            return None
        seen.add((reg, pc))
        if reg == A.ARM_REG_PC:
            return pc + 4
        for insn in self.prior(pc):
            if insn.id in (A.ARM_INS_B, A.ARM_INS_BX, A.ARM_INS_BL, A.ARM_INS_BLX):
                return None  # A control transfer ends the local definition.
            ops = insn.operands
            if not ops or ops[0].type != A.ARM_OP_REG or ops[0].reg != reg:
                if reg in insn.regs_access()[1]:
                    return None
                continue
            if insn.cc not in (A.ARM_CC_AL, A.ARM_CC_INVALID) or insn.address in self.it_members:
                return None
            if insn.id in (A.ARM_INS_MOV, A.ARM_INS_MOVW) and len(ops) == 2:
                return ops[1].imm & 0xffffffff if ops[1].type == A.ARM_OP_IMM else \
                    self.constant(ops[1].reg, insn.address, seen) if ops[1].type == A.ARM_OP_REG else None
            if insn.id == A.ARM_INS_MOVT and ops[1].type == A.ARM_OP_IMM:
                low = self.constant(reg, insn.address, seen)
                return None if low is None else (low & 65535) | (ops[1].imm << 16)
            if insn.id == A.ARM_INS_ADR and ops[1].type == A.ARM_OP_IMM:
                return ((insn.address + 4) & ~3) + ops[1].imm
            if insn.id in (A.ARM_INS_ADD, A.ARM_INS_SUB) and len(ops) == 3 and ops[2].type == A.ARM_OP_IMM:
                value = self.constant(ops[1].reg, insn.address, seen) if ops[1].type == A.ARM_OP_REG else None
                return None if value is None else (value + (1 if insn.id == A.ARM_INS_ADD else -1) * ops[2].imm) & 0xffffffff
            if insn.id == A.ARM_INS_LDR and len(ops) == 2 and ops[1].type == A.ARM_OP_MEM:
                return self.literal(insn)
            return None
        return None

    def literal(self, insn):
        mem = insn.operands[1].mem
        if mem.base != A.ARM_REG_PC or mem.index:
            return None
        address = ((insn.address + 4) & ~3) + mem.disp
        raw = self.read(address, 4)
        if len(raw) != 4:
            return None
        self.data.update(range(address, address + 4))
        return int.from_bytes(raw, 'little')

    def table(self, insn):
        mem = insn.operands[0].mem
        if mem.base != A.ARM_REG_PC:
            return None, 'table_base_not_pc'
        # A bound is accepted only from CMP index,#n followed immediately by
        # BHI/BHS. Stop if another instruction writes the index or flags.
        prefix = self.prior(insn.address)
        count = None
        for index, prior in enumerate(prefix):
            if prior.id == A.ARM_INS_B and prior.cc in (A.ARM_CC_HI, A.ARM_CC_HS):
                if index + 1 >= len(prefix):
                    break
                compare = prefix[index + 1]
                ops = compare.operands
                if compare.id == A.ARM_INS_CMP and len(ops) == 2 and \
                        ops[0].type == A.ARM_OP_REG and ops[0].reg == mem.index and \
                        ops[1].type == A.ARM_OP_IMM:
                    count = ops[1].imm + int(prior.cc == A.ARM_CC_HI)
                break
            if mem.index in prior.regs_access()[1] or prior.update_flags or prior.id in (
                    A.ARM_INS_B, A.ARM_INS_BL, A.ARM_INS_BX, A.ARM_INS_BLX):
                break
        if count is None or not 1 <= count <= 4096:
            return None, 'table_bound_not_proven'
        width = 2 if insn.id == A.ARM_INS_TBH else 1
        base = insn.address + 4
        raw = self.read(base, count * width)
        if len(raw) != count * width:
            return None, 'table_bytes_unmapped'
        targets = [base + 2 * int.from_bytes(raw[i:i + width], 'little')
                   for i in range(0, len(raw), width)]
        self.data.update(range(base, base + len(raw)))
        self.tables[insn.address] = {'pc': insn.address, 'address': base, 'width': width,
                                    'count': count, 'targets': targets}
        return targets, None

    def successors(self, insn):
        pc, ops = insn.address, insn.operands
        fall = pc + insn.size
        conditional = insn.cc not in (A.ARM_CC_AL, A.ARM_CC_INVALID) or pc in self.it_members
        result = []
        def edge(target, kind):
            value = self.target(pc, target, kind)
            if value is not None:
                result.append(value)
        if insn.id == A.ARM_INS_IT:
            mask = insn.bytes[0] & 15
            count = 4 - ((mask & -mask).bit_length() - 1)
            member_pc = fall
            for _ in range(count):
                member = self.decode(member_pc)
                if member is None:
                    self.unknown.add((pc, 'truncated_it_block', member_pc))
                    break
                self.it_members.add(member_pc)
                member_pc += member.size
            edge(fall, 'fallthrough')
        elif insn.id in (A.ARM_INS_B, A.ARM_INS_BL, A.ARM_INS_CBZ, A.ARM_INS_CBNZ):
            edge(ops[-1].imm & ~1, 'call' if insn.id == A.ARM_INS_BL else 'branch')
            if insn.id != A.ARM_INS_B or conditional:
                edge(fall, 'call_continuation' if insn.id == A.ARM_INS_BL else 'fallthrough')
        elif insn.id in (A.ARM_INS_TBB, A.ARM_INS_TBH):
            targets, reason = self.table(insn)
            if reason:
                self.unknown.add((pc, reason, None))
            else:
                for target in targets:
                    edge(target, 'table_branch')
            if conditional:
                edge(fall, 'fallthrough')
        elif insn.id in (A.ARM_INS_BX, A.ARM_INS_BLX):
            if ops[0].type == A.ARM_OP_IMM:
                edge(ops[0].imm & ~1, 'call')
            elif ops[0].reg == A.ARM_REG_LR and insn.id == A.ARM_INS_BX:
                self.edges.add((pc, None, 'return_or_exception_return'))
            else:
                value = self.constant(ops[0].reg, pc)
                if value is not None and value & 1:
                    edge(value & ~1, 'resolved_computed')
                else:
                    self.unknown.add((pc, 'computed_call' if insn.id == A.ARM_INS_BLX else 'computed_branch', value))
            if insn.id == A.ARM_INS_BLX or conditional:
                edge(fall, 'call_continuation' if insn.id == A.ARM_INS_BLX else 'fallthrough')
        elif A.ARM_REG_PC in insn.regs_access()[1]:
            if insn.id in (A.ARM_INS_POP, A.ARM_INS_LDM, A.ARM_INS_LDMDB):
                self.edges.add((pc, None, 'stack_loaded_pc'))
                # Usually a return, but arbitrary stack values remain unknown.
                self.unknown.add((pc, 'stack_loaded_pc', None))
            elif insn.id == A.ARM_INS_LDR and len(ops) == 2:
                value = self.literal(insn)
                if value is not None and value & 1:
                    edge(value & ~1, 'literal_loaded_pc')
                else:
                    self.unknown.add((pc, 'loaded_pc', value))
            elif insn.id == A.ARM_INS_MOV and len(ops) == 2 and ops[1].type == A.ARM_OP_REG and ops[1].reg == A.ARM_REG_LR:
                self.edges.add((pc, None, 'return_or_exception_return'))
            else:
                self.unknown.add((pc, 'computed_pc_write', None))
            if conditional:
                edge(fall, 'fallthrough')
        elif insn.id in (A.ARM_INS_UDF, A.ARM_INS_BKPT, A.ARM_INS_SVC):
            self.unknown.add((pc, 'trap_or_exception', None))
            if conditional:
                edge(fall, 'fallthrough')
        else:
            edge(fall, 'fallthrough')
        return result

    def walk(self):
        successors = {}
        while self.queue:
            pc, origin = self.queue.popleft()
            if self.origins[pc] & origin == origin:
                continue
            self.origins[pc] |= origin
            if pc not in successors:
                insn = self.decode(pc)
                if not insn:
                    self.unknown.add((pc, 'decode_failed', None))
                    successors[pc] = []
                    continue
                self.instructions[pc] = insn
                successors[pc] = self.successors(insn)
            self.queue.extend((target, origin) for target in successors[pc])
        # A computed edge can be visited before its defining prefix. Re-evaluate
        # unresolved edges after the CFG and IT membership are available.
        retry = [row for row in self.unknown if row[1] == 'table_bound_not_proven']
        for pc, reason, value in retry:
            targets, error = self.table(self.instructions[pc])
            if error is None:
                self.unknown.remove((pc, reason, value))
                for target in targets:
                    accepted = self.target(pc, target, 'table_branch')
                    if accepted is not None:
                        self.queue.append((accepted, self.origins[pc]))
        for pc, reason, value in list(self.unknown):
            if reason not in ('computed_call', 'computed_branch'):
                continue
            insn = self.instructions[pc]
            target = self.constant(insn.operands[0].reg, pc)
            if target is not None and target & 1:
                self.unknown.remove((pc, reason, value))
                accepted = self.target(pc, target & ~1, 'resolved_computed')
                if accepted is not None:
                    self.queue.append((accepted, self.origins[pc]))
        for pc in sorted(self.it_members & self.instructions.keys()):
            insn = self.instructions[pc]
            if A.ARM_REG_PC in insn.regs_access()[1] or insn.id in (
                    A.ARM_INS_B, A.ARM_INS_BX, A.ARM_INS_TBB, A.ARM_INS_TBH):
                accepted = self.target(pc, pc + insn.size, 'it_false_fallthrough')
                if accepted is not None and self.origins[accepted] & self.origins[pc] != self.origins[pc]:
                    self.queue.append((accepted, self.origins[pc]))
        if self.queue:
            self.walk()

    def report(self):
        translator = StepTranslator(self.fw, [])
        function = SimpleNamespace(units={})
        supported, rejected = [], []
        opcodes = Counter()
        rejected_opcodes = Counter()
        for pc, insn in sorted(self.instructions.items()):
            opcodes[insn.insn_name()] += 1
            # Existing emitters read only their own flash and ITCM mapping.
            compatible = pc >= 0x60000000 or (pc < ITCM_SIZE and
                self.read(pc, insn.size) == self.fw.code(pc, insn.size))
            try:
                body = translator.instruction(function, insn) if compatible else None
            except (ValueError, IndexError, KeyError, NotImplementedError) as error:
                body = None
                detail = f'{type(error).__name__}: {error}'
            else:
                detail = 'existing exact handler rejects encoding' if compatible else 'map unsupported by existing generators'
            if body is None:
                rejected_opcodes[insn.insn_name()] += 1
                rejected.append({'pc': pc, 'bytes': bytes(insn.bytes).hex(),
                                 'instruction': f'{insn.mnemonic} {insn.op_str}'.strip(), 'reason': detail,
                                 'origin_mask': self.origins[pc]})
            else:
                supported.append(pc)
        pcs = set(self.instructions)
        missing = sorted(self.required - pcs)
        overlap = []
        for pc in sorted(pcs):
            prior = self.instructions.get(pc - 2)
            if prior and prior.size == 4:
                overlap.append([pc - 2, pc])
        unknown = [{'pc': pc, 'reason': reason, 'operand': value}
                   for pc, reason, value in sorted(self.unknown, key=lambda r: (r[0], r[1], r[2] or 0))]
        return {
            'schema': 'm8-static-coverage/1', 'firmware_sha256': self.identity,
            'firmware_bytes': len(self.fw.data), 'inputs': dict(sorted(self.inputs.items())),
            'maps': [m.row() for m in self.maps],
            'roots': [{'pc': pc, 'origins': {kind: sorted(sources) for kind, sources in sorted(kinds.items())}}
                      for pc, kinds in sorted(self.roots.items())],
            'root_counts': dict(Counter(kind for kinds in self.roots.values() for kind in kinds)),
            'coverage': {'decoded_pcs': len(pcs), 'exact_handler_supported': len(supported),
                         'exact_handler_rejected': len(rejected),
                         'reset_vector_cfg_pcs': sum(bool(self.origins[pc] & 1) for pc in pcs),
                         'function_cfg_pcs': sum(bool(self.origins[pc] & 2) for pc in pcs),
                         'observed_cfg_pcs': sum(bool(self.origins[pc] & 4) for pc in pcs),
                         'pointer_only_cfg_pcs': sum(self.origins[pc] == 8 for pc in pcs),
                         'required_observed_pcs': len(self.required), 'missing_observed_pcs': missing,
                         'unknown_edges': len(unknown), 'overlapping_decode_pairs': len(overlap)},
            'reachability_complete': not unknown and not missing and not overlap,
            'qualification': 'Static candidate coverage only. Pointer roots can be data. Unresolved computed targets, mutable vectors, executable writes and runtime behavior require independent checks.',
            'opcode_support': {name: {'decoded': count, 'supported': count - rejected_opcodes[name],
                                     'rejected': rejected_opcodes[name]} for name, count in sorted(opcodes.items())},
            'supported_pcs': supported,
            'rejected': rejected, 'unknown_edges': unknown, 'overlapping_decodes': overlap,
            'table_branches': [self.tables[pc] for pc in sorted(self.tables)],
            'edges': [{'source': source, 'target': target, 'kind': kind}
                      for source, target, kind in sorted(self.edges, key=lambda r: (r[0], r[1] or 0, r[2]))],
            'data_ranges': compact_ranges(self.data),
        }


def compact_ranges(values):
    result = []
    for value in sorted(values):
        if result and result[-1][1] == value:
            result[-1][1] += 1
        else:
            result.append([value, value + 1])
    return result


def write_pcs(path, pcs, identity):
    Path(path).write_text(f'# firmware_sha256={identity}\n' + ''.join(f'0x{pc:08x}\n' for pc in sorted(pcs)))


def write_source(path, source):
    """Keep object prerequisites unchanged when generated source is identical."""
    data = source.encode()
    if not path.is_file() or path.read_bytes() != data:
        path.write_bytes(data)


def generation_options(args, sources):
    result = {'pages_per_shard': args.pages_per_shard, 'sources': sources,
              'extra_roots': sorted(args.root)}
    if getattr(args, 'shared_state', False):
        result['shared_state'] = True
    if getattr(args, 'regions_only', False):
        result['regions_only'] = True
    for key in ('pc_file', 'function_file', 'hot_pc_file'):
        result[key] = sorted(str(Path(path).resolve()) for path in getattr(args, key))
    return result


def normalize_pc_literals(source):
    """Capstone can expose a wrapped Thumb branch address as a signed integer."""
    targets = []
    def replace(match):
        target = -int(match[2], 16) & 0xffffffff
        targets.append(target)
        return f'{match[1]}0x{target:x}u;'
    source = re.sub(r'((?:cpu->r\[15\]|r_15)\s*=\s*)0x-([0-9a-f]+)u;', replace, source)
    if '0x-' in source:
        raise ValueError('Unexpected signed hexadecimal expression outside PC assignment')
    return source, sorted(set(targets))


def generate_sources(firmware, out, pcs, identity, pages_per_shard=4, boot_weights=None,
                     shared_state=False, regions_only=False):
    """Use existing emitters; shard only cold pages, with independent symbols."""
    from native_regions import generate as regions
    from native_step import generate as steps
    from boot_regions import order_page_groups
    if pages_per_shard < 1:
        raise ValueError('pages_per_shard must be positive')
    pages = defaultdict(list)
    for pc in sorted(pcs):
        pages[pc >> 12].append(pc)
    page_ids = sorted(pages)
    shards = []
    groups = []
    # A cold reset can verify flash while ITCM is still zero. Keep their sources
    # separate so a verifier never assumes that the boot copy has completed.
    for space_pages in ([p for p in page_ids if p < 0x60000],
                        [p for p in page_ids if p >= 0x60000]):
        groups.extend(space_pages[start:start + pages_per_shard]
                      for start in range(0, len(space_pages), pages_per_shard))
    weights = boot_weights or {}
    groups = order_page_groups(groups, weights)
    for selected_pages in groups:
        index = len(shards)
        selected = [pc for page in selected_pages for pc in pages[page]]
        stem = f'cold-{index:03d}'
        write_pcs(out / f'{stem}.pcs.txt', selected, identity)
        if regions_only:
            step, step_manifest = '', {'instructions': [], 'unsupported': []}
        else:
            step, step_manifest = steps(firmware, [], selected, False, blocks=False)
        region, region_manifest = regions(firmware, [], sorted(set(selected) - EXACT_BOOT_PCS),
                                             inline_services=True, ram_exclusives=True)
        step, step_normalized = normalize_pc_literals(step)
        region, region_normalized = normalize_pc_literals(region)
        state_manifest = None
        if shared_state:
            from native_region_state import transform
            region, state_manifest = transform(region)
        region_compiled = {pc for row in region_manifest['regions'] for pc in row['instructions']}
        if region_compiled & EXACT_BOOT_PCS:
            raise ValueError(f'Exact boot boundary absorbed by region: {sorted(region_compiled & EXACT_BOOT_PCS)}')
        aliases = '\n'.join(f'#define {symbol} m8_static_{index:03d}_{symbol}'
                            for symbol in (*PUBLIC_REGION, 'm8_native_region_inline_services',
                                           'm8_native_region_ram_exclusives'))
        write_source(out / f'{stem}-regions.c', aliases + '\n' + region)
        if not regions_only:
            write_source(out / f'{stem}-handlers.c',
                f'#define m8_native_step m8_static_{index:03d}_m8_native_step\n' + step)
        shards.append({'index': index, 'space': 'flash' if selected_pages[0] >= 0x60000 else 'itcm',
                       'pages': selected_pages, 'input_pcs': len(selected),
                       'boot_weight': sum(weights.get(pc, 0) for pc in selected),
                       'normalized_uint32_pc_targets': sorted(set(step_normalized) | set(region_normalized)),
                       'regions_source': f'{stem}-regions.c',
                       'pc_file': f'{stem}.pcs.txt',
                       'region_translated_pcs': sorted(region_compiled),
                       'handler_translated_pcs': [row['pc'] for row in step_manifest['instructions']],
                       'rejected': step_manifest['unsupported']})
        if not regions_only:
            shards[-1]['handlers_source'] = f'{stem}-handlers.c'
        if state_manifest is not None:
            shards[-1]['shared_state'] = state_manifest
    rows = ['/* Generated by static_coverage.py. Separate cold companion. */',
            '#include "native_static.h"',
            'uint32_t m8_native_static_abi(void) {return 1u;}',
            f'const char *m8_native_static_firmware_sha256(void) {{return "{identity}";}}']
    for shard in shards:
        prefix = f'm8_static_{shard["index"]:03d}_'
        if not regions_only:
            rows += [f'int {prefix}m8_native_step(m8_cpu *,m8_mem *,m8_native_arch *);']
        rows += [f'int {prefix}m8_native_region_step(m8_cpu *,m8_mem *,m8_native_arch *);',
                 f'int {prefix}m8_native_region_verify_memory(m8_mem *,uint32_t *);']
    kinds = (('region_step', 'm8_cpu *cpu,m8_mem *mem,m8_native_arch *arch', 'cpu,mem,arch'),)
    if not regions_only:
        kinds = (('step', 'm8_cpu *cpu,m8_mem *mem,m8_native_arch *arch', 'cpu,mem,arch'),) + kinds
    for kind, signature, args in kinds:
        rows += [f'int m8_native_static_{kind}({signature}) {{',
                 'arch->last_executed=arch->last_special=0;', 'switch(cpu->r[15]>>12) {']
        for shard in shards:
            rows += [f'case 0x{page:x}u:' for page in shard['pages']]
            rows += [f'return m8_static_{shard["index"]:03d}_m8_native_{kind}({args});']
        rows += ['default:return 2;', '}', '}']
    for name, selected_space in (('memory', None), ('flash', 'flash'), ('itcm', 'itcm')):
        rows += [f'int m8_native_static_region_verify_{name}(m8_mem *mem,uint32_t *bad_pc) {{',
                 'if(bad_pc) *bad_pc=0;']
        for shard in shards:
            if selected_space is None or shard['space'] == selected_space:
                rows += [f'if(m8_static_{shard["index"]:03d}_m8_native_region_verify_memory(mem,bad_pc)) return 1;']
        rows += ['return 0;', '}']
    write_source(out / 'wrapper.c', '\n'.join(rows) + '\n')
    source_keys = ('regions_source',) if regions_only else ('regions_source', 'handlers_source')
    write_source(out / 'sources.cmake', 'set(M8_NATIVE_STATIC_GENERATED_SOURCES\n' + ''.join(
        f'  "${{CMAKE_CURRENT_LIST_DIR}}/{source}"\n' for source in
        ['wrapper.c', *(s[key] for s in shards for key in source_keys)]) + ')\n')
    return shards


def run(args):
    config = read_json(args.config)
    coverage = Coverage(args.firmware, config)
    coverage.input(args.config)
    coverage.input(args.firmware)
    for source in sorted((ROOT / 'aot').glob('*.py')):
        coverage.input(source)
    for source in sorted((ROOT / 'aot').glob('*.h')):
        coverage.input(source)
    coverage.seed(args.input_root, args.pc_file, args.function_file, args.root, args.hot_pc_file)
    coverage.walk()
    report = coverage.report()
    report['input_root'] = str(args.input_root.resolve())
    report['firmware_path'] = str(args.firmware.resolve())
    report['config_path'] = str(args.config.resolve())
    report['generation_options'] = generation_options(args, args.generate_sources)
    args.output.mkdir(parents=True, exist_ok=True)
    for pattern in ('cold-*.c', 'cold-*.pcs.txt', '*.pcs.txt', 'coverage*.json*', 'wrapper.c', 'sources.cmake'):
        for path in args.output.glob(pattern):
            if args.generate_sources and (path.suffix == '.c' or path.name == 'sources.cmake'):
                continue
            if path.is_file():
                path.unlink()
    pcs = set(coverage.instructions)
    cold = (pcs - coverage.hot) | coverage.boot_pcs
    for name, selected in (('reachable', pcs), ('cold', cold), ('handlers', set(report['supported_pcs'])),
                           ('regions', pcs)):
        write_pcs(args.output / f'{name}.pcs.txt', selected, coverage.identity)
    report['cold_pcs'] = len(cold)
    report['hot_pcs_excluded_from_cold'] = len((pcs & coverage.hot) - coverage.boot_pcs)
    report['boot_pcs_retained_in_cold'] = len(coverage.boot_pcs)
    report['exact_boot_region_exclusions'] = sorted(EXACT_BOOT_PCS)
    # Keep the public report small enough for build logs and review. The complete
    # graph retains deterministic compressed bytes, including every root source.
    detail_keys = ('roots', 'supported_pcs', 'edges', 'overlapping_decodes', 'data_ranges')
    details = {key: report.pop(key) for key in detail_keys}
    details['origin_masks'] = {str(pc): coverage.origins[pc] for pc in sorted(pcs)}
    (args.output / 'coverage-detail.json.gz').write_bytes(gzip.compress(
        json.dumps(details, separators=(',', ':'), sort_keys=True).encode(), mtime=0))
    report['detail_file'] = 'coverage-detail.json.gz'
    report['origin_mask_bits'] = KINDS
    report['complete'] = False
    report['first_failure'] = None
    report_path = args.output / 'coverage.json'
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    try:
        if report['coverage']['missing_observed_pcs']:
            raise ValueError(f'Missing observed PCs: {report["coverage"]["missing_observed_pcs"][:8]}')
        if args.generate_sources:
            report['shards'] = generate_sources(args.firmware, args.output, cold,
                                                 coverage.identity, args.pages_per_shard, coverage.boot_weights,
                                                 shared_state=getattr(args, 'shared_state', False),
                                                 regions_only=getattr(args, 'regions_only', False))
            expected_sources = {shard[key] for shard in report['shards']
                                for key in ('regions_source', 'handlers_source') if key in shard}
            for path in args.output.glob('cold-*.c'):
                if path.name not in expected_sources:
                    path.unlink()
            if getattr(args, 'regions_only', False):
                translated = {pc for shard in report['shards'] for pc in shard['region_translated_pcs']}
            else:
                translated = {pc for shard in report['shards'] for pc in shard['handler_translated_pcs']}
            report['coverage']['cold_translated_pcs'] = len(translated & cold)
            report['coverage']['cold_untranslated_pcs'] = sorted(cold - translated)
            report['coverage']['additional_it_member_pcs'] = sorted(translated - cold)
        changed = [str(path) for path, expected in coverage.inputs.items()
                   if sha(Path(path).read_bytes()) != expected]
        if changed:
            raise ValueError(f'Input changed during generation: {changed[0]}')
    except BaseException as error:
        report['first_failure'] = f'{type(error).__name__}: {error}'
        report_path.write_text(json.dumps(report, indent=2) + '\n')
        raise
    report['complete'] = True
    dependencies = []
    for filename in sorted(coverage.inputs):
        path = Path(filename)
        if path == args.firmware.resolve():
            value = '${M8_FIRMWARE_BIN}'
        elif path.is_relative_to(args.input_root.resolve()):
            value = '${CMAKE_CURRENT_SOURCE_DIR}/' + str(path.relative_to(args.input_root.resolve()))
        else:
            value = str(path)
        dependencies.append('  "' + value.replace('\\', '/').replace('"', '\\"') + '"\n')
    (args.output / 'inputs.cmake').write_text('set(M8_NATIVE_STATIC_GENERATION_INPUTS\n' + ''.join(dependencies) + ')\n')
    report['outputs'] = {p.name: sha(p.read_bytes()) for p in sorted(args.output.iterdir())
                         if p.is_file() and p.name != 'coverage.json'}
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    return report


def check_output(args):
    """Reject stale generated input without repeating the CFG or translation."""
    report = read_json(args.output / 'coverage.json')
    if not report.get('complete') or not report.get('shards'):
        raise ValueError('Static generation is incomplete or has no source shards')
    if report.get('generation_options') != generation_options(args, True):
        raise ValueError('Static generation options differ')
    old_root = Path(report['input_root'])
    config = read_json(args.config)
    expected = {args.config.resolve(), args.firmware.resolve()}
    expected.update(Path(path).resolve() for key in ('pc_file', 'function_file', 'hot_pc_file')
                    for path in getattr(args, key))
    for key in ('function_files', 'observed_pc_files', 'observed_profiles',
                'observed_manifests', 'hot_pc_files', 'boot_weight_files', 'provenance_files'):
        for pattern in config.get(key, ()):
            matches = list(args.input_root.glob(pattern))
            if not matches:
                raise ValueError(f'Static coverage input pattern has no files: {pattern}')
            expected.update(path.resolve() for path in matches)
    expected.update(path.resolve() for pattern in ('*.py', '*.h') for path in (ROOT / 'aot').glob(pattern))
    actual = set()
    for filename, identity in report['inputs'].items():
        path = Path(filename)
        if filename == report['firmware_path']:
            path = args.firmware.resolve()
        elif filename == report['config_path']:
            path = args.config.resolve()
        elif path.is_relative_to(old_root):
            path = args.input_root / path.relative_to(old_root)
        path = path.resolve()
        actual.add(path)
        if not path.is_file() or sha(path.read_bytes()) != identity:
            raise ValueError(f'Static generation input differs: {path}')
    if actual != expected:
        raise ValueError(f'Static generation input membership differs: {sorted(map(str, actual ^ expected))[:4]}')
    for filename, identity in report['outputs'].items():
        path = args.output / filename
        if not path.is_file() or sha(path.read_bytes()) != identity:
            raise ValueError(f'Static generation output differs: {path}')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware', type=Path, required=True)
    parser.add_argument('--config', type=Path, default=DEFAULT_CONFIG)
    parser.add_argument('--input-root', type=Path, default=ROOT)
    parser.add_argument('--pc-file', type=Path, action='append', default=[])
    parser.add_argument('--hot-pc-file', type=Path, action='append', default=[])
    parser.add_argument('--function-file', type=Path, action='append', default=[])
    parser.add_argument('--root', type=lambda value: int(value, 0), action='append', default=[])
    parser.add_argument('--generate-sources', action='store_true')
    parser.add_argument('--check-output', action='store_true', help='Validate existing generation without translating again')
    parser.add_argument('--pages-per-shard', type=int, default=4)
    parser.add_argument('--shared-state', action='store_true',
                        help='Emit the default-off shared CPU register copy for cold regions')
    parser.add_argument('--regions-only', action='store_true',
                        help='Emit bulk regions without the duplicate single-step decoder')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = check_output(args) if args.check_output else run(args)
    counts = {key: value for key, value in report['coverage'].items() if not isinstance(value, list)}
    print(json.dumps({'coverage': counts, 'cold_pcs': report['cold_pcs'],
                      'shards': len(report.get('shards', ())),
                      'reachability_complete': report['reachability_complete']}, sort_keys=True))


if __name__ == '__main__':
    main()
