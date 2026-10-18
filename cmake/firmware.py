#!/usr/bin/env python3
"""Verify the supported firmware and create its contiguous flash image."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import tempfile

FLASH_BASE = 0x60000000
FLASH_SIZE = 8 * 1024 * 1024


def hex_to_binary(source: bytes) -> bytes:
    """Parse Intel HEX without silently dropping records or bad checksums."""
    image = bytearray(b"\xff" * FLASH_SIZE)
    base = 0
    end = 0
    eof = False
    for lineno, raw in enumerate(source.decode("ascii").splitlines(), 1):
        line = raw.strip()
        if not line:
            continue
        if eof:
            raise ValueError(f"line {lineno}: record after EOF")
        if not line.startswith(":"):
            raise ValueError(f"line {lineno}: missing record marker")
        try:
            record = bytes.fromhex(line[1:])
        except ValueError as exc:
            raise ValueError(f"line {lineno}: invalid hexadecimal data") from exc
        if len(record) < 5 or len(record) != record[0] + 5:
            raise ValueError(f"line {lineno}: invalid record length")
        if sum(record) & 0xFF:
            raise ValueError(f"line {lineno}: invalid record checksum")
        count, kind = record[0], record[3]
        address = int.from_bytes(record[1:3], "big")
        data = record[4:-1]
        if kind == 0:
            offset = base + address - FLASH_BASE
            if offset < 0 or offset + count > FLASH_SIZE:
                raise ValueError(f"line {lineno}: data outside flash")
            image[offset:offset + count] = data
            end = max(end, offset + count)
        elif kind == 1 and count == 0 and address == 0:
            eof = True
        elif kind == 2 and count == 2 and address == 0:
            base = int.from_bytes(data, "big") << 4
        elif kind == 4 and count == 2 and address == 0:
            base = int.from_bytes(data, "big") << 16
        elif kind in (3, 5) and count == 4 and address == 0:
            # Entry-address records do not contain flash bytes.
            pass
        else:
            raise ValueError(f"line {lineno}: unsupported or malformed record type {kind}")
    if not eof:
        raise ValueError("missing EOF record")
    if not end:
        raise ValueError("no flash data")
    return bytes(image[:end])


def verified_binary(source: bytes, input_sha256: str, output_sha256: str,
                    input_kind: str = "hex") -> bytes:
    actual = hashlib.sha256(source).hexdigest()
    if actual != input_sha256:
        raise ValueError(f"firmware SHA-256 mismatch: expected {input_sha256}, got {actual}")
    binary = hex_to_binary(source) if input_kind == "hex" else source
    actual = hashlib.sha256(binary).hexdigest()
    if actual != output_sha256:
        raise ValueError(f"binary SHA-256 mismatch: expected {output_sha256}, got {actual}")
    return binary


def write_if_changed(output: Path, binary: bytes) -> None:
    if output.is_file() and output.read_bytes() == binary:
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary: str | None = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
            temporary = stream.name
            stream.write(binary)
        os.replace(temporary, output)
    finally:
        if temporary is not None and os.path.exists(temporary):
            os.unlink(temporary)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--input-kind", choices=("hex", "bin"), default="hex")
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--binary-sha256", required=True)
    args = parser.parse_args()
    try:
        binary = verified_binary(args.input.read_bytes(), args.sha256,
                                 args.binary_sha256, args.input_kind)
        write_if_changed(args.output, binary)
    except (OSError, ValueError, UnicodeError) as exc:
        parser.exit(1, f"Firmware verification failed: {exc}\n")


if __name__ == "__main__":
    main()
