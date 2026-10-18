"""Apply the pinned native-region PGO scope without changing guest code."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re

ATTRIBUTE = '__attribute__((no_profile_instrument_function)) '


def digest(source):
    return hashlib.sha256(source.encode()).hexdigest()


def profile_scope_source(original, omitted):
    """Add the profile attribute to both generated source branches."""
    if ATTRIBUTE in original or len(omitted) != len(set(omitted)):
        raise ValueError('Profile source is already scoped or has repeated functions')
    scoped = original
    for name in omitted:
        if not re.fullmatch(r'region_[0-9a-f]+_impl', name):
            raise ValueError('Invalid region function: ' + name)
        declaration = 'static int ' + name + '('
        if scoped.count(declaration) != 2:
            raise ValueError('Expected shared and ordinary definitions: ' + name)
        scoped = scoped.replace(declaration, 'static ' + ATTRIBUTE + 'int ' + name + '(')
    if scoped.replace(ATTRIBUTE, '') != original:
        raise ValueError('Profile scope changed a region body')
    return scoped


def apply_scope(original, manifest, path):
    """Require the qualified source, PC counts and firmware before scoping."""
    scope = json.loads(Path(path).read_text())
    if (scope.get('schema') != 'm8-native-pgo-scope/1' or
            scope.get('firmware_sha256') != manifest['firmware_sha256'] or
            scope.get('attribute') != 'no_profile_instrument_function' or
            scope.get('original_source_sha256') != digest(original)):
        raise ValueError('Pinned PGO scope source, firmware or attribute differs')
    omitted = scope['omitted_bodies']
    regions = {f"region_{row['page']:x}_impl": row for row in manifest['regions']}
    if (len(omitted) != scope['omitted_body_count'] or
            len(regions) != scope['omitted_body_count'] + scope['profiled_body_count'] or
            sum(row['pcs'] for row in omitted) != scope['omitted_pcs'] or
            scope['attribute_insertions'] != 2 * len(omitted)):
        raise ValueError('Pinned PGO scope region totals differ')
    for row in omitted:
        if row['function'] not in regions or len(regions[row['function']]['instructions']) != row['pcs']:
            raise ValueError('Pinned PGO scope region PCs differ: ' + row['function'])
    scoped = profile_scope_source(original, [row['function'] for row in omitted])
    if digest(scoped) != scope['scoped_source_sha256']:
        raise ValueError('Pinned PGO scope output differs')
    return scoped, {**scope, 'attribute_only_diff': True, 'guest_bodies_unchanged': True}
