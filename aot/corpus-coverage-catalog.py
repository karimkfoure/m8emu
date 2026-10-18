"""Adapt the retained native layout and redirect catalog without project data."""
from __future__ import annotations
import re


def integer(value, label):
    if type(value) is not int or value < 0:
        raise ValueError(f'{label}: expected a nonnegative integer')
    return value


def adapt_layout(layout, ungrouped):
    """Keep compatible old groups; never guess a new profile-guided group."""
    if layout.get('schema') != 'm8-native-region-layout/1' or \
            layout['firmware_sha256'] != ungrouped['firmware_sha256']:
        raise ValueError('Layout and region firmware differ')
    if any(len(r.get('pages', [r['page']])) != 1 for r in ungrouped['regions']):
        raise ValueError('Layout adaptation requires an ungrouped region inventory')
    pages = {r['page']: r for r in ungrouped['regions']}
    groups, split = [], []
    for row in layout['groups']:
        members = row['pages']
        if len(set(members)) != len(members) or not 1 <= len(members) <= 4:
            raise ValueError('Invalid retained layout group')
        if any(p not in pages for p in members):
            raise ValueError('Retained layout page disappeared')
        classes = {pages[p]['has_fp'] for p in members}
        positions = sum(len(pages[p]['instructions']) for p in members)
        if len(classes) != 1 or positions > 8192:
            split.append({'pages': members, 'reason': 'FP class or instruction-position bound'})
            continue
        groups.append({'pages': members, 'has_fp': classes.pop()})
    return {'schema': 'm8-native-region-layout/1', 'firmware_sha256': layout['firmware_sha256'],
            'groups': groups, 'split_retained_groups': split,
            'method': 'Retain compatible old groups. The existing generator leaves all other pages separate.'}


def adapt_targets(text, regions, contract_sha256, source=None):
    """Re-pin an unchanged target whitelist to the expanded compiled catalog."""
    headers, rows = {}, []
    owners_by_page = {p: r['page'] for r in regions['regions'] for p in r.get('pages', [r['page']])}
    entries = {pc for r in regions['regions'] for pc in r['entries']}
    for line in text.splitlines():
        if line.startswith('# ') and '=' in line:
            key, value = line[2:].split('=', 1)
            headers[key] = value.strip()
        fields = line.partition('#')[0].split()
        if fields:
            if len(fields) != 4:
                raise ValueError('Target whitelist row requires four fields')
            rows.append(fields)
    if headers.get('schema') != 'm8-native-region-redirect-targets/1' or \
            headers.get('firmware_sha256') != regions['firmware_sha256']:
        raise ValueError('Target whitelist firmware differs')
    seen, output = set(), []
    for _, pc, code, weight in rows:
        target = int(pc, 0)
        if target in seen or target not in entries or int(weight, 0) <= 0 or len(bytes.fromhex(code)) not in (2, 4):
            raise ValueError(f'Retained target absent or invalid: {pc}')
        seen.add(target)
        # Expanded IT blocks can consume a previously independent instruction.
        # The unchanged redirect transformer requires its own budget leader.
        if source is not None and not re.search(
                rf'\nL_{target:x}:\n\{{\nif\(units_used\+\d+u>\d+u\) \{{r_15=0x{target:x}u;', source):
            continue
        output.append(f'0x{owners_by_page[target >> 12]:x} {pc} {code} {weight}\n')
    headers['region_contract_sha256'] = contract_sha256
    return ''.join(f'# {key}={value}\n' for key, value in headers.items()) + ''.join(output)
