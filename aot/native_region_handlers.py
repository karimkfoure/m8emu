#!/usr/bin/env python3
"""Generate exact single-instruction handlers for native region exits.

Select only the region manifest's declined entries and every member of its IT
blocks. Reuse the existing native step generator unchanged, with block execution
disabled, so each completed instruction returns to the runtime's region/AOT
selection. The seven firmware page-crossing ordinary entries remain explicit
handlers; this is not a general interpreter or a new broad PC fragment.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path

from capstone import arm as A
from native_step import generate as generate_step
from translate import Firmware


def select(firmware, region_manifest):
    fw = Firmware(firmware)
    raw = Path(region_manifest).read_bytes()
    manifest = json.loads(raw)
    if manifest.get('schema') != 'm8-native-regions/1':
        raise ValueError('expected m8-native-regions/1 manifest')
    digest = hashlib.sha256(fw.data).hexdigest()
    if manifest.get('firmware_sha256') != digest:
        raise ValueError('region manifest firmware identity differs')
    declined = {int(row['pc']) for row in manifest['declined']}
    compiled = {int(pc) for region in manifest['regions']
                for pc in region['instructions']}
    members, it_blocks = set(), []
    for pc in sorted(compiled | declined):
        insn = fw.decode(pc)
        if insn is None:
            raise ValueError(f'manifest PC does not decode: {pc:#x}')
        if insn.id != A.ARM_INS_IT:
            continue
        state = insn.bytes[0]
        mask = state & 15
        if not mask or state >> 4 >= 14:
            raise ValueError(f'invalid IT encoding: {pc:#x}')
        count = 4 - ((mask & -mask).bit_length() - 1)
        next_pc, block_members = pc + insn.size, []
        for _ in range(count):
            member = fw.decode(next_pc)
            if member is None:
                raise ValueError(f'IT member does not decode: {next_pc:#x}')
            members.add(next_pc)
            block_members.append(next_pc)
            next_pc += member.size
        it_blocks.append({'pc': pc, 'members': block_members})
    selected = declined | members
    return selected, {'region_manifest': str(region_manifest),
                      'region_manifest_sha256': hashlib.sha256(raw).hexdigest(),
                      'declined_pcs': sorted(declined),
                      'it_member_pcs': sorted(members), 'it_blocks': it_blocks,
                      'selected_pcs': sorted(selected)}


def generate(firmware, region_manifest, pc_profile=False):
    selected, selection = select(firmware, region_manifest)
    source, manifest = generate_step(firmware, [], sorted(selected), False,
                                     blocks=False, pc_profile=pc_profile)
    if manifest['unsupported']:
        first = manifest['unsupported'][0]
        raise ValueError(f'unsupported selected handler {first["pc"]:#x}: '
                         f'{first["instruction"]}')
    actual = {row['pc'] for row in manifest['instructions']}
    if actual != selected:
        raise ValueError('native step generator changed the exact selection')
    manifest['selection'] = selection
    manifest['single_instruction_region_handlers'] = True
    manifest['selected_firmware_bytes'] = sum(
        len(bytes.fromhex(row['bytes'])) for row in manifest['instructions'])
    return source, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware', type=Path, required=True)
    parser.add_argument('--region-manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--pc-profile', action='store_true',
                        help='count completed executed handler instructions; do not use for timing')
    args = parser.parse_args()
    source, manifest = generate(args.firmware, args.region_manifest, args.pc_profile)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(source)
    args.manifest.write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'native region handlers: {manifest["generated_instructions"]} PCs, '
          f'{manifest["selected_firmware_bytes"]} firmware bytes; one instruction per return')


if __name__ == '__main__':
    main()
