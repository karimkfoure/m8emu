#!/usr/bin/env python3
"""Adapt generated native entry section names for the Mach-O compiler.

Instruction bodies and exported names remain unchanged. Linux generation does
not invoke this tool; its existing ELF layout remains unchanged.
"""
import argparse
from pathlib import Path
import re

PATTERN = re.compile(r'section\("\.text\.m8native\.(entry|hot|cold)"\)')

def convert(source):
    result = PATTERN.sub(lambda m: 'section("__TEXT,__m8_' + m[1] + '")', source)
    if '.text.m8native.' in result:
        raise ValueError("Unknown native ELF section remains")
    return result

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    source = args.source.read_text()
    result = convert(source)
    if result != source:
        args.source.write_text(result)
