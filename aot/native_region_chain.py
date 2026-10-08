#!/usr/bin/env python3
"""Append direct chaining to a --chain-ready region copy."""
import argparse
import json
from pathlib import Path


def append_chain(source, manifest, event_loop=False):
    if 'm8_native_region_chain_lookup' not in source or 'region_' not in source:
        raise ValueError('input must be generated with --chain-ready')
    if '#include "native_region_chain.h"' in source:
        raise ValueError('input already contains the chaining implementation')
    rows=['', 'static bool m8_native_region_chain_has_fp(uint32_t pc) {',
          'switch(pc>>12) {']
    for region in manifest['regions']:
        if region['has_fp']:
            for page in region.get('pages',[region['page']]):
                rows.append(f'case 0x{page:x}u: return true;')
    rows.extend(['default: return false;', '}', '}',
                 '#include "native_region_chain.h"', ''])
    if event_loop:
        rows.extend(['#include "native_region_event_chain.h"', ''])
    return source+'\n'.join(rows)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',required=True,type=Path)
    parser.add_argument('--manifest',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    parser.add_argument('--event-loop',action='store_true')
    args=parser.parse_args()
    source=append_chain(args.input.read_text(),json.loads(args.manifest.read_text()),args.event_loop)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(source)


if __name__=='__main__':
    main()
