#!/usr/bin/env python3
"""Generate byte-checked cold handlers without changing the warm generator.

The runtime supplies reset CPU state, FP activation, ITCM-copy guards and all
device scheduling. These handlers supply no AOT lookup or Unicorn internals.
"""
from __future__ import annotations
import argparse
import gzip
import hashlib
import json
from pathlib import Path

from native_step import generate as generate_step
from translate import Firmware, ITCM_OFFSET

FIRMWARE_SHA256 = '20498b7637cee17baae249372db3b8377afd4e12a5e7062754c2ddd83621367f'
BARRIER_PCS = (0x21bd4, 0x21c48)
POLL_PCS = (0x3028c, 0x30290, 0x30292, 0x30294, 0x30296, 0x30298, 0x3029a,
            0x3029c, 0x3029e, 0x302a0, 0x302a2, 0x302a4, 0x302a6, 0x302a8,
            0x302aa, 0x302ac, 0x302ae, 0x302b2, 0x302b4, 0x302b6, 0x302ba,
            0x302bc, 0x302be)
PROTECTED_SOURCES = {
    'generated.c': 'df01e62229dc1f79812ee63b08c43a45ca7eee236d1e6b8401912e70d603518c',
    'extra.c': '408993abc3705ea1fd5b54882847873bb3e4325758ac9458a0603d7ddb9424e4',
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    raw = Path(path).read_bytes()
    return json.loads(gzip.decompress(raw) if Path(path).suffix == '.gz' else raw)


def read_pcs(path, identity):
    pcs = set()
    for line in Path(path).read_text().splitlines():
        if line.startswith('# firmware_sha256=') and line.split('=', 1)[1].strip() != identity:
            raise ValueError(f'PC input firmware differs: {path}')
        line = line.split('#', 1)[0].strip()
        if line:
            # Retained census lists can include an instruction count column.
            pcs.add(int(line.split()[0], 0))
    return pcs


def metadata_source(image):
    # Full flash equality covers reset code, vectors and the complete original
    # ITCM copy source. No runtime hash package or private CPU layout is needed.
    rows = ['static const uint8_t boot_flash[]={']
    rows += [','.join(f'0x{byte:02x}' for byte in image[start:start+32])+','
             for start in range(0, len(image), 32)]
    rows += ['};', 'uint32_t m8_native_boot_abi(void) {return 1u;}',
             'const char *m8_native_boot_firmware_sha256(void) {',
             f'return "{digest(image)}";', '}',
             'int m8_native_boot_verify_memory(m8_mem *mem,uint32_t *bad_pc) {',
             'if(bad_pc) *bad_pc=0;',
             'if(!mem || !mem->flash.host || mem->flash.base!=0x60000000u ||',
             '   mem->flash.size<sizeof boot_flash) {',
             'if(bad_pc) *bad_pc=0x60000000u; return 1;', '}',
             'if(memcmp(mem->flash.host,boot_flash,sizeof boot_flash)) {',
             'for(size_t i=0;i<sizeof boot_flash;i++) {',
             'if(mem->flash.host[i]!=boot_flash[i]) {',
             'if(bad_pc) *bad_pc=0x60000000u+(uint32_t)i; return 1;', '}', '}', '}',
             'if(!mem->itcm.host || mem->itcm.base || mem->itcm.size<0x3d5d8u) return 1;',
             'for(uint32_t i=0;i<mem->itcm.size;i++) {',
             'if(mem->itcm.host[i]) {if(bad_pc) *bad_pc=i; return 1;}', '}',
             'return 0;', '}']
    return '\n'.join(rows)+'\n'


def wrapper_source(extra=False):
    rows = ['int m8_native_step(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch) {',
            'arch->last_executed=arch->last_special=0;',
            'if(cpu->r[15]==0x21bd4u || cpu->r[15]==0x21c48u)',
            'return m8_native_boot_barrier(cpu,mem,arch);',
            'int result=m8_native_boot_generated_step(cpu,mem,arch);']
    if extra:
        rows += ['if(result==2 && !arch->last_executed)',
                 'result=m8_native_boot_extra_step(cpu,mem,arch);']
    rows += ['return result;', '}']
    return '\n'.join(rows)+'\n'


def generate(firmware, profiles=(), handler_manifests=(), pc_files=(), extra_pcs=(),
             *, protected_directory=None):
    fw = Firmware(firmware)
    identity = digest(fw.data)
    if identity != FIRMWARE_SHA256:
        raise ValueError(f'Wrong normalized firmware: {identity}')
    pcs = set(extra_pcs) | set(POLL_PCS) | set(BARRIER_PCS) | {0x30280, 0x60005c24}
    inputs = []
    for path in handler_manifests:
        handler = read_json(path)
        if handler.get('firmware_sha256') != identity:
            raise ValueError(f'Handler firmware differs: {path}')
        for kind in ('instructions', 'unsupported'):
            pcs.update(int(row['pc']) for row in handler.get(kind, ()))
        inputs.append({'path': str(path), 'sha256': digest(Path(path).read_bytes())})
    for path in pc_files:
        pcs.update(read_pcs(path, identity))
        inputs.append({'path': str(path), 'sha256': digest(Path(path).read_bytes())})
    normalized_profiles = []
    # native_step reads uncompressed JSON. Preserve its input checks without
    # changing that generator or writing temporary normalized profile files.
    for path in profiles:
        profile = read_json(path)
        if profile.get('firmware_sha256') != identity:
            raise ValueError(f'Profile firmware differs: {path}')
        if profile.get('schema') != 'm8-guest-pc/1':
            raise ValueError(f'Wrong guest profile schema: {path}')
        for kind in ('jit', 'aot'):
            pcs.update(int(row['pc']) for row in profile.get(kind, ()))
        normalized_profiles.append({'path': str(path), 'sha256': digest(Path(path).read_bytes())})
    output = ['/* Generated by native_boot.py. Runtime services remain external. */',
              '#include "native_boot_support.h"']
    protected = []
    if protected_directory:
        directory = Path(protected_directory)
        if handler_manifests or profiles or pc_files or extra_pcs:
            raise ValueError('Protected first-run sources cannot accept added PCs')
        manifests = [read_json(directory / name) for name in ('manifest.json', 'extra-manifest.json')]
        for name, expected in PROTECTED_SOURCES.items():
            path = directory / name
            actual = digest(path.read_bytes())
            if actual != expected:
                raise ValueError(f'Protected source differs: {path} {actual}')
            protected.append({'path': str(path), 'sha256': actual})
        records = {int(row['pc']): row for manifest in manifests for row in manifest['instructions']}
        unsupported = {int(row['pc']): row for manifest in manifests for row in manifest['unsupported']}
        if any(manifest.get('firmware_sha256') != identity for manifest in manifests):
            raise ValueError('Protected handler firmware differs')
        # The protected sources compile as separate translation units with
        # -Dm8_native_step=m8_native_boot_generated_step / ..._extra_step.
        output += ['int m8_native_boot_generated_step(m8_cpu *,m8_mem *,m8_native_arch *);',
                   'int m8_native_boot_extra_step(m8_cpu *,m8_mem *,m8_native_arch *);']
    else:
        source, manifest = generate_step(firmware, [], sorted(pcs), True, blocks=True)
        records = {int(row['pc']): row for row in manifest['instructions']}
        unsupported = {int(row['pc']): row for row in manifest['unsupported']}
        output += ['#define m8_native_step m8_native_boot_generated_step', source,
                   '#undef m8_native_step']
    for pc in BARRIER_PCS:
        record = unsupported.get(pc)
        if not record or record['bytes'] != 'bff35b8f':
            raise ValueError(f'Cold DMB ISH input differs at 0x{pc:x}')
        records[pc] = unsupported.pop(pc)
    for pc, expected in ((0x30280, 'e360'), (0x60005c24, '054b')):
        if pc not in records or records[pc]['bytes'] != expected:
            raise ValueError(f'Required cold instruction differs at 0x{pc:x}')
    output += [metadata_source(fw.data), wrapper_source(bool(protected_directory))]
    result = {'schema': 'm8-native-boot/1', 'cold_abi': 1, 'firmware_sha256': identity,
              'flash_bytes_verified': len(fw.data), 'itcm_copy_source': 0x60000000+ITCM_OFFSET,
              'itcm_copy_bytes': 0x3d5d8, 'profiles': normalized_profiles, 'handler_inputs': inputs,
              'protected_sources': protected, 'blocks': True, 'warm_verification_deferred': True,
              'aot_lookup': False, 'barrier_pcs': list(BARRIER_PCS),
              'instructions': [records[pc] for pc in sorted(records)],
              'unsupported': [unsupported[pc] for pc in sorted(unsupported)],
              'generated_instructions': len(records)}
    return '\n'.join(output)+'\n', result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware', type=Path, required=True)
    parser.add_argument('--profile', type=Path, action='append', default=[])
    parser.add_argument('--handlers-manifest', type=Path, action='append', default=[])
    parser.add_argument('--pc-file', type=Path, action='append', default=[])
    parser.add_argument('--pc', type=lambda text: int(text, 0), action='append', default=[])
    parser.add_argument('--protected-directory', type=Path,
                        help='Use only the exact retained first-run generated.c and extra.c')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    source, manifest = generate(args.firmware, args.profile, args.handlers_manifest,
                                args.pc_file, args.pc, protected_directory=args.protected_directory)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(source)
    args.manifest.write_text(json.dumps(manifest, indent=2)+'\n')
    print(f"cold handlers: {manifest['generated_instructions']} cases, {len(manifest['unsupported'])} unsupported PCs")
    for row in manifest['unsupported']:
        print(f"unsupported 0x{row['pc']:x}: {row['bytes']} {row['instruction']}")


if __name__ == '__main__':
    main()
