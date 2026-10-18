#!/usr/bin/env python3
"""Restore proven read-only spans in explicit native voice header copies.

The CLI copies an existing, valid guarded directory and leaves the copies
unchanged unless --restore-readonly-literals is set. The importable repair
function is an explicit operation for a trainer's separate dependency mirror.
Neither path changes the original guard source, headers or manifest.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

SCHEMA = "m8-native-voices-readonly-literals/1"
PINNED_HEADERS = {
    "sampler_voice.h": "50c04b37adc2f1fd4fbf27055abb54ef260321976b2fa153abd9e9f642eabaf4",
    "hyper_voice.h": "668109946dd7b5f257ffd9a83a4775ed39b2c0a7db024430a586856c4c387628",
    "wav_batch.h": "ca80d30c24001649fce41f6390c7156d6eec409b743495822ae1dfcad3c12348",
}
LAYER_NAME = "native-voices-readonly-literals.json"


def digest_bytes(data):
    return hashlib.sha256(data).hexdigest()


def digest(path):
    return digest_bytes(Path(path).read_bytes())


def exact(source, old, new=None):
    count = source.count(old)
    if count != 1:
        raise ValueError(f"Read-only source pattern differs: expected=1; actual={count}; token={old!r}")
    return source if new is None else source.replace(old, new)


def repaired_header(name, source):
    """The whole-header pin protects the audited access-use proof."""
    pointer = "pointer[i]=m8_aot_guard_offset_data_pointer(arena,address[i],size[i]);"
    if name == "sampler_voice.h":
        exact(source, "m8_le_read(object+0x50,4),0,0x1cdc8u};")
        exact(source, "uint32_t size[]={480,152,16384,640,count*4u,count*4u,124,4,4,5196};")
        source = exact(source, pointer,
            "#ifdef M8_SV_SHAPE\n"
            "        pointer[i]=((i==9u && address[i]==0x1cdc8u && size[i]==5196u)\n"
            "            || (i==10u && address[i]==0x2070u && size[i]==12u)\n"
            "            ) ?\n"
            "#else\n"
            "        pointer[i]=(i==9u && address[i]==0x1cdc8u && size[i]==5196u) ?\n"
            "#endif\n"
            "            m8_canonical_offset_pointer(arena,address[i],size[i]) :\n"
            "            m8_aot_guard_offset_data_pointer(arena,address[i],size[i]);")
        source = exact(source, "m8_le_read(m8_aot_guard_offset_data_pointer(arena,0x1d484,4),4)",
                       "m8_le_read(m8_canonical_offset_pointer(arena,0x1d484,4),4)")
        changes = [
            {"pattern_count": 1, "array_index": 9, "address": "0x1cdc8", "bytes": 5196,
             "proof": "Qualification uses pointer[9] only for bounds, alignment and disjointness. The pinned body reads PC-relative literals from this ITCM span. It does not write through pointer[9]."},
            {"pattern_count": 1, "address": "0x1d484", "bytes": 4,
             "proof": "The restored pointer is used only as the input to m8_le_read(...,4)."},
            {"pattern_count": 1, "array_index": 10, "address": "0x2070", "bytes": 12,
             "proof": "Only the opt-in shape-5 path qualifies this span. The pinned 1fa4/1fec methods read its three PC-relative literals and never write it. All other spans retain the data-only guard."},
        ]
    elif name == "hyper_voice.h":
        exact(source, "left,right,m8_le_read(object+0x50,4),0,0x16998u,0x17470u};")
        exact(source, "uint32_t size[]={256,80,124,48,48,48,48,52,32,640,count*4u,count*4u,4,4,16,24};")
        exact(source, "m8_le_read(pointer[15],4)==0x43a00000u")
        source = exact(source, pointer,
            "pointer[i]=((i==14u && address[i]==0x16998u && size[i]==16u) ||\n"
            "                    (i==15u && address[i]==0x17470u && size[i]==24u)) ?\n"
            "            m8_canonical_offset_pointer(arena,address[i],size[i]) :\n"
            "            m8_aot_guard_offset_data_pointer(arena,address[i],size[i]);")
        changes = [
            {"pattern_count": 1, "array_index": 14, "address": "0x16998", "bytes": 16,
             "proof": "Qualification uses pointer[14] only for bounds, alignment and disjointness. The pinned body reads its PC-relative literals and does not write through pointer[14]."},
            {"pattern_count": 1, "array_index": 15, "address": "0x17470", "bytes": 24,
             "proof": "Qualification additionally reads pointer[15] with m8_le_read. The pinned body reads PC-relative literals and does not write through pointer[15]."},
        ]
    elif name == "wav_batch.h":
        exact(source, "const uint8_t *restrict constants=literals;")
        exact(source, "s16=m8_le_read(constants,4);")
        exact(source, "s15=m8_le_read(constants+4,4);")
        source = exact(source, "uint8_t *literals=m8_aot_guard_data_pointer(mem,0x209a4u,8u);",
                       "uint8_t *literals=m8_span_pointer(mem,0x209a4u,8u);")
        changes = [{"pattern_count": 1, "address": "0x209a4", "bytes": 8,
                    "proof": "literals becomes the const constants pointer. Both uses are m8_le_read; no store uses literals or constants."}]
    else:
        raise ValueError(f"Unsupported copied header: {name}")
    return source, changes


def validate_original(manifest_path):
    manifest_path = Path(manifest_path).resolve()
    base = manifest_path.parent
    manifest = json.loads(manifest_path.read_text())
    if manifest.get("schema") != "m8-native-aot-guard/1":
        raise ValueError("Require an original native AOT guard manifest")
    if manifest.get("transformations", {}).get("rejected_write_paths") != []:
        raise ValueError("Original guard has rejected write paths")
    protected = {"generated.c": manifest["output_sha256"],
                 "native_aot_guard.h": manifest["guard_header_sha256"]}
    for name, record in manifest["headers"].items():
        if Path(name).name != name or not name.endswith(".h"):
            raise ValueError(f"Unsupported original header name: {name}")
        protected[name] = record["output_sha256"]
    for name, wanted in protected.items():
        if digest(base / name) != wanted:
            raise ValueError(f"Original guard manifest input differs: {name}")
    return manifest_path, protected


def atomic_write(path, data):
    path = Path(path)
    mode = path.stat().st_mode & 0o777 if path.exists() else 0o644
    with tempfile.NamedTemporaryFile(prefix=path.name + ".", dir=path.parent, delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(data)
    try:
        temporary.chmod(mode)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def repair_directory(directory, manifest_path):
    """Patch a separate header mirror and return its explicit layered manifest.

    Validate all original files before changing any copy. Exact header hashes
    and source patterns define the audited scope. Atomic replacement also
    preserves originals if the caller made its mirror with hard links.
    """
    directory = Path(directory).resolve()
    manifest_path, protected = validate_original(manifest_path)
    if not directory.is_dir() or directory == manifest_path.parent:
        raise ValueError("Require an existing copy directory distinct from the original")
    layer_path = directory / LAYER_NAME
    if layer_path.exists():
        raise ValueError("Read-only layer already exists; require fresh copies")
    original_manifest_hash = digest(manifest_path)
    originals = {name: digest(manifest_path.parent / name) for name in protected}
    prepared, changes = {}, {}
    for name, wanted in PINNED_HEADERS.items():
        path = directory / name
        if path.is_symlink() or digest(path) != wanted or protected.get(name) != wanted:
            raise ValueError(f"Copied kernel differs from the pinned access-use proof: {name}")
        source, transformations = repaired_header(name, path.read_text())
        data = source.encode()
        prepared[name] = data
        changes[name] = {"before_sha256": wanted, "after_sha256": digest_bytes(data),
                         "restored_read_spans": transformations}
    # No other guarded helper, source or guard may differ in a full mirror.
    for name, wanted in protected.items():
        if (directory / name).exists() and digest(directory / name) != wanted:
            raise ValueError(f"Unchanged copied guard input differs: {name}")
    metadata = {"schema": SCHEMA, "enabled": True,
                "scope": "Restore only pinned read-only literal spans; preserve all other data/write guards",
                "original_manifest": str(manifest_path), "original_manifest_sha256": original_manifest_hash,
                "tool": str(Path(__file__).resolve()), "tool_sha256": digest(__file__),
                "copies": str(directory), "headers": changes,
                "unchanged_original_sha256": originals}
    for name, data in prepared.items():
        atomic_write(directory / name, data)
    for name, wanted in originals.items():
        if digest(manifest_path.parent / name) != wanted:
            raise ValueError(f"Original changed during copy repair: {name}")
    if digest(manifest_path) != original_manifest_hash:
        raise ValueError("Original manifest changed during copy repair")
    for name, record in changes.items():
        if digest(directory / name) != record["after_sha256"]:
            raise ValueError(f"Repaired header changed: {name}")
    atomic_write(layer_path, (json.dumps(metadata, indent=2) + "\n").encode())
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--restore-readonly-literals", action="store_true")
    args = parser.parse_args()
    source, output = args.input.resolve(), args.output.resolve()
    manifest_path, protected = validate_original(source / "manifest.json")
    if output.exists() or output == source or source in output.parents:
        parser.error("Require a new output directory outside the original")
    output.mkdir(parents=True)
    for path in source.iterdir():
        if not path.is_file() or path.is_symlink():
            raise ValueError(f"Unsupported guarded directory entry: {path.name}")
        shutil.copy2(path, output / path.name)
    if args.restore_readonly_literals:
        metadata = repair_directory(output, manifest_path)
    else:
        metadata = {"schema": SCHEMA, "enabled": False,
                    "original_manifest": str(manifest_path), "original_manifest_sha256": digest(manifest_path),
                    "copies": str(output), "headers": {}, "unchanged_original_sha256": protected}
        atomic_write(output / LAYER_NAME, (json.dumps(metadata, indent=2) + "\n").encode())
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
