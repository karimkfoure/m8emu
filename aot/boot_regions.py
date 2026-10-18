#!/usr/bin/env python3
"""Protect boot PC weights and order inputs for the existing region generator.

Counts rank region groups. They cover retained BOOT10000 stimuli, not only the
clean startup boundary, and are not host cost or product acceptance evidence.
This helper generates no instruction implementation and changes no runtime ABI.
"""
from __future__ import annotations

import argparse
from collections import Counter
import gzip
import json
from pathlib import Path


def boot_weights(paths, firmware_sha256):
    """Return both JIT and AOT instruction weights from protected boot profiles."""
    weights = Counter()
    seen = set()
    for path in paths:
        path = Path(path)
        identity = path.resolve()
        if identity in seen:
            raise ValueError(f"Duplicate boot profile: {path}")
        seen.add(identity)
        raw = path.read_bytes()
        profile = json.loads(gzip.decompress(raw) if path.suffix == ".gz" else raw)
        if profile.get("schema") != "m8-guest-pc/1" or profile.get("phase") != "boot":
            raise ValueError(f"Wrong boot profile schema or phase: {path}")
        if profile.get("firmware_sha256") != firmware_sha256:
            raise ValueError(f"Boot profile firmware differs: {path}")
        for kind in ("jit", "aot"):
            rows = profile.get(kind)
            if not isinstance(rows, list):
                raise ValueError(f"Missing boot PC rows: {path} {kind}")
            for row in rows:
                pc, count = row.get("pc"), row.get("instructions")
                if type(pc) is not int or pc < 0 or pc > 0xffffffff or pc & 1:
                    raise ValueError(f"Invalid boot PC: {path} {pc}")
                if type(count) is not int or count < 0:
                    raise ValueError(f"Invalid boot instruction count: {path} {pc}")
                if count:
                    weights[pc] += count
    return weights


def order_page_groups(groups, weights):
    """Put high-weight groups first while preserving membership and tie order."""
    groups = [list(group) for group in groups]
    pages = set()
    for group in groups:
        if not group:
            raise ValueError("Empty boot region group")
        for page in group:
            if type(page) is not int or page < 0 or page > 0xfffff or page in pages:
                raise ValueError(f"Invalid or repeated boot region page: {page}")
            pages.add(page)
    page_weights = Counter()
    for pc, count in weights.items():
        if type(pc) is not int or pc < 0 or pc > 0xffffffff or pc & 1 or type(count) is not int or count < 0:
            raise ValueError(f"Invalid boot region weight: {pc} {count}")
        page_weights[pc >> 12] += count
    return sorted(groups, key=lambda group: -sum(page_weights[page] for page in group))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", type=Path, action="append", required=True)
    parser.add_argument("--firmware-sha256", required=True)
    parser.add_argument("--output", type=Path, required=True,
                        help="New counted PC fragment for native_regions.py --pc-file")
    args = parser.parse_args()
    weights = boot_weights(args.profile, args.firmware_sha256)
    with args.output.open("x") as output:
        output.write(f"# firmware_sha256={args.firmware_sha256}\n")
        output.write("# Retained boot weights; original region generator owns instruction semantics.\n")
        for pc, count in sorted(weights.items()):
            output.write(f"0x{pc:x} {count}\n")
    print(f"boot PCs: {len(weights)}; weighted instructions: {sum(weights.values())}")


if __name__ == "__main__":
    main()
