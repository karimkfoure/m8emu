#!/usr/bin/env python3
"""Fuse measured computed transfers inside verified playback region groups.

This optional postprocessor leaves the region generator unchanged. It adds one
finite target dispatch at an existing computed-transfer boundary. It adds no
guest instruction, decoder, AOT body or target outside the measured groups.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "aot"))
from native_regions import boundary


def sha256(raw):
    return hashlib.sha256(raw).hexdigest()


def contract_hash(regions):
    """Hash compiled membership independently of local input path spelling."""
    contract = {'firmware_sha256': regions['firmware_sha256'], 'budget': regions['budget'],
                'chain_ready': regions.get('chain_ready'),
                'regions': [{key: row.get(key, [row['page']] if key == 'pages' else None)
                             for key in ('page', 'pages', 'has_fp', 'entries', 'instructions')}
                            for row in regions['regions']]}
    return sha256(json.dumps(contract, sort_keys=True, separators=(',', ':')).encode())


def read_targets(raw):
    headers, rows = {}, []
    for text in raw.decode().splitlines():
        if text.startswith('# ') and '=' in text:
            key, value = text[2:].split('=', 1)
            headers[key] = value.strip()
        fields = text.partition('#')[0].split()
        if fields:
            if len(fields) != 4:
                raise ValueError('target rows require group, PC, bytes and measured weight')
            rows.append({'group_page': int(fields[0], 0), 'target_pc': int(fields[1], 0),
                         'bytes': bytes.fromhex(fields[2]).hex(), 'weight': int(fields[3], 0)})
    if headers.get('schema') != 'm8-native-region-redirect-targets/1':
        raise ValueError('expected m8-native-region-redirect-targets/1 input')
    return headers, rows


def transform(source, regions, profiles=(), enabled=False, target_input=None):
    """Return transformed C and a deterministic audit manifest.

    Profiles contain pairs of (input name, decoded profile, raw-byte hash).
    The target whitelist is per implementation group. Unobserved transfers to
    a whitelisted target are safe too: every such target remains a compiled
    entry and a checked budget leader in the unchanged verified body.
    """
    if regions.get('schema') != 'm8-native-regions/1' or not regions.get('chain_ready'):
        raise ValueError('requires a chain-ready m8-native-regions/1 manifest')
    digest = regions['firmware_sha256']
    owners, entries, instructions = {}, set(), set()
    for region in regions['regions']:
        for page in region.get('pages', [region['page']]):
            if page in owners:
                raise ValueError(f'duplicate compiled page {page:#x}')
            owners[page] = region['page']
        entries.update(region['entries'])
        instructions.update(region['instructions'])
    edges, inputs, ignored, targets = Counter(), [], Counter(), {}
    for name, profile, profile_hash in profiles:
        if profile.get('schema') != 'm8-native-region-exit/1' or \
                profile.get('firmware_sha256') != digest or profile.get('dropped') != 0:
            raise ValueError(f'incompatible or incomplete exit profile: {name}')
        inputs.append({'name': name, 'sha256': profile_hash})
        for row in profile['edges']:
            source_pc, target = row['source_pc'], row['exit_pc']
            if row['result'] or row['itstate'] or row['reason_id'] != 3:
                ignored['forced or incomplete exit'] += row['exits']
                continue
            if source_pc not in instructions or target not in entries or boundary(target) or \
                    target >= 0xffffffe0 or source_pc >> 12 not in owners or \
                    target >> 12 not in owners:
                ignored['hook, exception return or absent compiled endpoint'] += row['exits']
                continue
            owner = owners[source_pc >> 12]
            if owner != owners[target >> 12]:
                ignored['different implementation group'] += row['exits']
                continue
            edges[owner, source_pc, target] += row['exits']
            targets.setdefault(owner, set()).add(target)
    target_metadata, fixed_rows = None, []
    if target_input:
        name, raw = target_input
        header, fixed_rows = read_targets(raw)
        if header.get('firmware_sha256') != digest or \
                header.get('region_contract_sha256') != contract_hash(regions):
            raise ValueError('fixed targets differ from firmware or compiled region membership')
        verify = source.partition('int m8_native_region_verify_memory(')[2]
        bytes_by_pc = {}
        for pc, size, values in re.findall(r'\{0x([0-9a-f]+)u,([24]),\{([\d,]+)\}\}', verify):
            bytes_by_pc[int(pc, 16)] = bytes(map(int, values.split(',')))[:int(size)].hex()
        seen = set()
        for row in fixed_rows:
            owner, target = row['group_page'], row['target_pc']
            if target in seen or target not in entries or owner != owners.get(target >> 12) or \
                    boundary(target) or target >= 0xffffffe0 or \
                    row['bytes'] != bytes_by_pc.get(target) or row['weight'] <= 0:
                raise ValueError(f'invalid group, target or current code bytes: {target:#x}')
            seen.add(target)
            targets.setdefault(owner, set()).add(target)
        target_metadata = {'name': name, 'sha256': sha256(raw), **header}
    output, checked_targets = source, []
    if enabled:
        if '/* Measured computed region continuations. */' in source:
            raise ValueError('source already contains computed fusion')
        for owner, selected in sorted(targets.items()):
            pattern = rf'(static int region_{owner:x}_impl\([^\n]+\) \{{\n)(.*?)(?=\n#undef M8_SAVE\n)'
            matches = list(re.finditer(pattern, output, re.S))
            if len(matches) != 1:
                raise ValueError(f'cannot identify exactly one scalarized region {owner:#x}')
            match = matches[0]
            body = match[2]
            if 'uint32_t r_15=output->r[15];' not in body or \
                    body.count('\nredirect:\n') != 1 or 'M8_PUBLISH(output)' not in body:
                raise ValueError(f'unknown scalarized publication contract in region {owner:#x}')
            for target in sorted(selected):
                leader = rf'\nL_{target:x}:\n\{{\nif\(units_used\+\d+u>\d+u\) \{{r_15=0x{target:x}u;'
                if not re.search(leader, body):
                    raise ValueError(f'computed target lacks a budget leader: {target:#x}')
                checked_targets.append(target)
            before, tail = body.split('\nredirect:\n')
            redirect, after = tail.split('\nfault:\n', 1)
            if not redirect.endswith('goto done;') or 'exit_reason=slow?5u:3u;' not in redirect:
                raise ValueError(f'unknown redirect exit contract in region {owner:#x}')
            insert = ['/* Measured computed region continuations. */',
                      'if(exit_reason==3u && !slow && !arch->itstate &&',
                      '   !m8_native_it_from_xpsr(cpu->xpsr)) {', 'switch(r_15) {']
            insert += [f'case 0x{target:x}u: goto L_{target:x};' for target in sorted(selected)]
            insert += ['default: break;', '}', '}']
            redirect = redirect[:-len('goto done;')] + '\n'.join(insert) + '\ngoto done;'
            changed = match[1] + before + '\nredirect:\n' + redirect + '\nfault:\n' + after
            output = output[:match.start()] + changed + output[match.end():]
    metadata = {'schema': 'm8-native-region-redirect-fusion/1', 'enabled': enabled,
                'firmware_sha256': digest, 'input_source_sha256': sha256(source.encode()),
                'output_source_sha256': sha256(output.encode()), 'exit_profiles': inputs,
                'compiled_guest_instructions': sum(len(r['instructions']) for r in regions['regions']),
                'implementation_groups': len(regions['regions']), 'transformed_groups': len(targets) if enabled else 0,
                'targets': sorted(checked_targets), 'historical_candidate_weight':
                sum(edges.values()) if profiles else sum(row['weight'] for row in fixed_rows),
                'measured_edges': [{'group_page': group, 'source_pc': pc, 'target_pc': target, 'weight': count}
                                   for (group, pc, target), count in sorted(edges.items())],
                'ignored_exit_weights': dict(sorted(ignored.items())),
                'fixed_target_input': target_metadata, 'fixed_target_rows': fixed_rows,
                'region_contract_sha256': contract_hash(regions),
                'guards': ['computed exit only', 'no slow memory', 'no live ITSTATE',
                           'measured same-group compiled target', 'target budget leader',
                           'no runtime hook or exception return'],
                'limits': ['Historical weights are discovery counts, not a CPU speed bound.',
                           'Unknown targets retain the original region return.',
                           'The runtime still owns IRQ timing, code verification and invalidation.']}
    return output, metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--region-manifest', type=Path, required=True)
    parser.add_argument('--exit-profile', type=Path, action='append', default=[])
    parser.add_argument('--targets', type=Path, help='committed exact target list; ignored diagnostic profiles are optional')
    parser.add_argument('--fuse-computed', action='store_true', help='enable exact measured in-group continuations')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.read_text()
    raw = args.region_manifest.read_bytes()
    profiles = []
    for path in args.exit_profile:
        data = path.read_bytes()
        profiles.append((path.name, json.loads(data), sha256(data)))
    target_input = (args.targets.name, args.targets.read_bytes()) if args.targets else None
    output, manifest = transform(source, json.loads(raw), profiles, args.fuse_computed, target_input)
    manifest['region_manifest_sha256'] = sha256(raw)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)
    args.manifest.write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'computed fusion: {manifest["transformed_groups"]} groups, {len(manifest["targets"])} target leaders')


if __name__ == '__main__':
    main()
