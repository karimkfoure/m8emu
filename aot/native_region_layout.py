#!/usr/bin/env python3
"""Group measured static region connections without adding guest instructions.

The layout is an explicit, default-off input for native_regions.py. Groups use
at most four guest pages and contain only pages with the same FP classification.
Weights are discovery counts from separate diagnostic intervals, not a rate.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path


def validate(data, firmware_sha256, pages):
    """Validate explicit groups against the current compiled page inventory."""
    if data.get('schema') != 'm8-native-region-layout/1':
        raise ValueError('expected m8-native-region-layout/1 layout')
    if data.get('firmware_sha256') != firmware_sha256:
        raise ValueError('layout firmware identity differs')
    groups, seen = [], set()
    for row in data['groups']:
        members = row['pages']
        if not members or len(members) > 4 or len(set(members)) != len(members):
            raise ValueError(f'layout group must contain one to four unique pages: {members}')
        if type(row.get('has_fp')) is not bool:
            raise ValueError(f'layout group lacks an exact FP classification: {members}')
        for page in members:
            if type(page) is not int or page not in pages:
                raise ValueError(f'layout page is absent from compiled input: {page!r}')
            if page in seen:
                raise ValueError(f'layout page occurs in more than one group: {page:#x}')
            if pages[page]['has_fp'] != row['has_fp']:
                raise ValueError(f'layout FP classification differs at page {page:#x}: '
                                 f'layout={row["has_fp"]}, compiled={pages[page]["has_fp"]}')
            seen.add(page)
        positions = sum(pages[page]['instruction_positions'] for page in members)
        if positions > 8192:
            raise ValueError(f'layout group exceeds 8192 instruction positions: {members}, {positions}')
        groups.append(sorted(members))
    groups += [[page] for page in pages if page not in seen]
    return sorted(groups, key=lambda group: group[0])


def generate(region_manifest, exit_profiles, max_pages=4, max_units=8192, min_exits=1):
    from native_regions import boundary
    if not 1 <= max_pages <= 4 or not 1 <= max_units <= 8192 or min_exits < 1:
        raise ValueError('layout limits require 1..4 pages, 1..8192 positions and positive exit count')
    raw = Path(region_manifest).read_bytes()
    manifest = json.loads(raw)
    if manifest.get('schema') != 'm8-native-regions/1':
        raise ValueError('expected m8-native-regions/1 region manifest')
    # A new layout is derived from the page inventory, not from an already
    # grouped inventory. This keeps each source page's FP proof independent.
    if any(len(row.get('pages', [row['page']])) != 1 for row in manifest['regions']):
        raise ValueError('derive layout from ungrouped regions')
    pages = {row['page']: {'has_fp': row['has_fp'],
                          'instruction_positions': len(row['instructions'])}
             for row in manifest['regions']}
    entries = {pc for row in manifest['regions'] for pc in row['entries']}
    instructions = {pc for row in manifest['regions'] for pc in row['instructions']}
    edges, inputs, ignored = Counter(), [], Counter()
    per_profile = []
    for path in exit_profiles:
        profile_raw = Path(path).read_bytes()
        profile = json.loads(profile_raw)
        if profile.get('schema') != 'm8-native-region-exit/1':
            raise ValueError(f'expected m8-native-region-exit/1 profile: {path}')
        if profile.get('firmware_sha256') != manifest['firmware_sha256']:
            raise ValueError(f'exit profile firmware identity differs: {path}')
        if profile.get('dropped') != 0:
            raise ValueError(f'exit profile has dropped edges: {path}, {profile.get("dropped")}')
        inputs.append({'path': str(path), 'sha256': hashlib.sha256(profile_raw).hexdigest()})
        weights = Counter()
        for row in profile['edges']:
            source, target = row['source_pc'], row['exit_pc']
            if row['result'] or row['itstate'] or row['reason_id'] not in (1, 2):
                ignored['not a completed static edge'] += row['exits']
                continue
            if boundary(target) or source not in instructions or target not in entries:
                ignored['hook or absent compiled endpoint'] += row['exits']
                continue
            left, right = source >> 12, target >> 12
            if left == right:
                ignored['within one page'] += row['exits']
                continue
            if pages[left]['has_fp'] != pages[right]['has_fp']:
                ignored['different FP classification'] += row['exits']
                continue
            key = source, target, row['reason_id']
            edges[key] += row['exits']
            weights[key] += row['exits']
        per_profile.append((str(path), weights))
    links = Counter()
    for (source, target, _reason), weight in edges.items():
        links[tuple(sorted((source >> 12, target >> 12)))] += weight
    owner = {page: page for page in pages}
    groups = {page: {page} for page in pages}
    rejected = []
    for (left, right), weight in sorted(links.items(), key=lambda row: (-row[1], row[0])):
        if weight < min_exits:
            rejected.append({'pages': [left, right], 'weight': weight, 'reason': 'minimum exit count'})
            continue
        a, b = owner[left], owner[right]
        if a == b:
            continue
        merged = groups[a] | groups[b]
        positions = sum(pages[page]['instruction_positions'] for page in merged)
        if len(merged) > max_pages or positions > max_units:
            rejected.append({'pages': [left, right], 'weight': weight, 'reason': 'group bound'})
            continue
        leader = min(merged)
        groups.pop(a)
        groups.pop(b)
        groups[leader] = merged
        for page in merged:
            owner[page] = leader
    selected_edges = [(source, target, reason, weight)
                      for (source, target, reason), weight in sorted(edges.items())
                      if owner[source >> 12] == owner[target >> 12]]
    rows = []
    for leader, members in sorted(groups.items()):
        if len(members) == 1:
            continue
        group_edges = [{'source_pc': source, 'target_pc': target, 'reason': reason, 'weight': weight}
                       for source, target, reason, weight in selected_edges if source >> 12 in members]
        rows.append({'pages': sorted(members), 'has_fp': pages[leader]['has_fp'],
                     'guest_page_bytes': len(members) * 4096,
                     'instruction_positions': sum(pages[page]['instruction_positions'] for page in members),
                     'removed_exit_weight': sum(row['weight'] for row in group_edges),
                     'edges': group_edges})
    profile_summary = []
    for path, weights in per_profile:
        removed = sum(weight for (source, target, _reason), weight in weights.items()
                      if owner[source >> 12] == owner[target >> 12])
        profile_summary.append({'path': path, 'candidate_static_exit_weight': sum(weights.values()),
                                'grouped_static_exit_weight': removed})
    layout = {'schema': 'm8-native-region-layout/1', 'firmware_sha256': manifest['firmware_sha256'],
              'region_manifest': str(region_manifest),
              'region_manifest_sha256': hashlib.sha256(raw).hexdigest(), 'exit_profiles': inputs,
              'max_pages': max_pages, 'max_instruction_positions': max_units, 'min_exits': min_exits,
              'known_pages': [{'page': page, **info} for page, info in sorted(pages.items())],
              'groups': rows, 'candidate_static_exit_weight': sum(edges.values()),
              'grouped_static_exit_weight': sum(row[3] for row in selected_edges),
              'profile_weights': profile_summary, 'ignored_exit_weights': dict(sorted(ignored.items())),
              'rejected_links': rejected}
    validate(layout, manifest['firmware_sha256'], pages)
    return layout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--region-manifest', type=Path, required=True)
    parser.add_argument('--exit-profile', type=Path, action='append', required=True)
    parser.add_argument('--max-pages', type=int, default=4)
    parser.add_argument('--max-units', type=int, default=8192)
    parser.add_argument('--min-exits', type=int, default=1)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    layout = generate(args.region_manifest, args.exit_profile, args.max_pages, args.max_units, args.min_exits)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(layout, indent=2) + '\n')
    print(f'native region layout: {len(layout["groups"])} merged groups, '
          f'{layout["grouped_static_exit_weight"]} measured static exit weight')


if __name__ == '__main__':
    main()
