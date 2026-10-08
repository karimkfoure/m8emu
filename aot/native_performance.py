#!/usr/bin/env python3
"""Create a separate, checked source copy for the combined native profile.

The profile uses SAMPLER with loops, HYPERSYNTH and the integer audio pool.
WAV batching was removed. No input file changes. Without --enable, the copy
retains the original guards and instruction paths. A compiled marker reports
the actual options; the manifest alone does not qualify a loaded library.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import tempfile

import audio_pool

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
REPAIR = HERE / "readonly_literals.py"
SCHEMA = "m8-native-performance/1"
GUARD_COPY = "native-performance-input-guard.json"
REQUIRED_FUNCTIONS = (0x15da4, 0x168e8, 0x1b938, 0x1c414, 0x1caa8,
                      *audio_pool.ENTRIES)
FLAGS = {"M8_AOT_SAMPLER_VOICE": 1, "M8_AOT_SAMPLER_LOOPS": 2,
         "M8_AOT_HYPER_VOICE": 4, "M8_AOT_AUDIO_POOL": 8,
         "M8_AOT_DIAGNOSTIC": 32,
         "M8_AOT_GUEST_COUNT": 64}
REQUIRED_FLAGS = tuple(FLAGS)[:4]
PREREQUISITES = ("M8_AOT_NATIVE_FP", "M8_AOT_FAST_MEMORY", "M8_AOT_FIXED_MEMORY")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_hash(path):
    return digest(Path(path).read_bytes())


def repair_module():
    spec = importlib.util.spec_from_file_location("native_performance_readonly", REPAIR)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    # This historical repair helper also records the removed WAV prototype.
    # Keep only the two voice kernels retained by the current combined profile.
    module.PINNED_HEADERS = {name: value for name, value in module.PINNED_HEADERS.items()
                            if name in ("sampler_voice.h", "hyper_voice.h")}
    return module


def regular_files(directory):
    directory = Path(directory)
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError(f"Require a regular source directory: {directory}")
    paths = {}
    for path in directory.iterdir():
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"Unsupported source directory entry: {path.name}")
        paths[path.name] = path
    return paths


def compile_contract(enabled):
    return {"enabled": list(REQUIRED_FLAGS), "disabled": list(FLAGS)[4:],
            "prerequisites": list(PREREQUISITES), "flags_export": "m8_aot_native_performance_flags",
            "expected_flags": 15, "profile_export": "m8_aot_native_performance_profile",
            "expected_profile": int(enabled)}


def inventory(source):
    """Require existing voice owners, callbacks and original pool bodies."""
    functions = {int(value, 16) for value in re.findall(
        r"^static int fn_([0-9a-f]+)\(m8_cpu \*restrict output,m8_mem \*restrict mem\) \{", source, re.M)}
    missing = set(REQUIRED_FUNCTIONS) - functions
    if missing:
        raise ValueError(f"Combined profile function inventory missing: {sorted(map(hex, missing))}")
    kernels = {}
    for header, call, entry in (
        ("sampler_voice.h", "m8_sampler_voice_kernel(output,mem,fn_1c414,fn_1b938)", 0x1caa8),
        ("hyper_voice.h", "m8_hyper_voice_kernel(output,mem,fn_15da4)", 0x168e8),
    ):
        include = f'#include "{header}"'
        select = f'if(output->r[15]==0x{entry:x}u && output->r[3]>1u) {{'
        if source.count(include) != 1 or source.count(call) != 2 or source.count(select) != 2:
            raise ValueError(f"Combined profile kernel entry shape differs: {header}")
        kernels[header] = {"entry": hex(entry), "call": call, "scalar_variants": 2}
    if source.count("uint32_t m8_aot_native_code_guard(void) {return 1;}") != 1:
        raise ValueError("Original native code guard marker is missing or ambiguous")
    if "m8_aot_native_performance_profile" in source or "m8_audio_pool_base_verify_memory" in source:
        raise ValueError("Require original guarded source, before performance or pool overlays")
    return {"required_functions": sorted(map(hex, REQUIRED_FUNCTIONS)), "voice_kernels": kernels}


def marker_source(enabled):
    """Emit option checks which run in the loaded library, not the copier."""
    rows = ["\n/* Compiled combined-profile options; no guest state changes. */",
            "uint32_t m8_aot_native_performance_flags(void) {", "uint32_t flags=0;"]
    for flag, value in FLAGS.items():
        rows += [f"#ifdef {flag}", f"flags|={value}u;", "#endif"]
    rows += ["return flags;", "}", "uint32_t m8_aot_native_performance_profile(void) {"]
    positive = [*REQUIRED_FLAGS, *PREREQUISITES]
    negative = tuple(FLAGS)[4:]
    condition = " && ".join([f"defined({flag})" for flag in positive] +
                            [f"!defined({flag})" for flag in negative])
    rows += [f"#if {int(enabled)} && {condition}",
             "return m8_aot_native_code_guard()==1u && m8_aot_audio_pool_enabled()==1u;",
             "#else", "return 0;", "#endif", "}"]
    return "\n".join(rows) + "\n"


def validate_bundle(directory, *, check_original=True, check_tools=True):
    """Check all copied inputs before a trainer freezes or compiles them.

    Training must freeze every returned file, including sibling headers that
    the compiler omits when their options are off. Check the loaded marker
    separately after each compilation. The marker does not prove useful work.
    """
    directory = Path(directory)
    if directory.is_symlink():
        raise ValueError("Performance copy directory symlinks are unsupported")
    directory = directory.resolve()
    paths = regular_files(directory)
    manifest = json.loads(paths["manifest.json"].read_text())
    if manifest.get("schema") != SCHEMA or type(manifest.get("enabled")) is not bool:
        raise ValueError("Require a native performance source manifest")
    files = manifest.get("files", {})
    if set(paths) != set(files) | {"manifest.json"}:
        raise ValueError("Performance copy file inventory differs from its manifest")
    for name, expected in files.items():
        if file_hash(paths[name]) != expected:
            raise ValueError(f"Performance copy differs: {name}")
    expected_tools = {"copier", "readonly_literals", "audio_pool"}
    if manifest["enabled"]:
        expected_tools.update(("audio_pool.h", "audio_pool_memset.h"))
    if set(manifest.get("tools", {})) != expected_tools:
        raise ValueError("Performance transform tool inventory differs")
    if check_tools:
        for record in manifest["tools"].values():
            if file_hash(record["path"]) != record["sha256"]:
                raise ValueError(f"Performance transform tool changed: {record['path']}")
    guard = json.loads(paths[GUARD_COPY].read_text())
    if guard.get("schema") != "m8-native-aot-guard/1" or guard.get("transformations", {}).get("rejected_write_paths") != []:
        raise ValueError("Protected source guard manifest differs")
    if files.get("native_aot_guard.h") != guard["guard_header_sha256"]:
        raise ValueError("Native guard header changed in the performance copy")
    if manifest.get("output_sha256") != files.get("generated.c"):
        raise ValueError("Performance source output identity differs")
    if manifest.get("compile_contract") != compile_contract(manifest["enabled"]):
        raise ValueError("Performance compile contract differs")
    if files.get(GUARD_COPY) != manifest.get("original_manifest", {}).get("sha256"):
        raise ValueError("Copied original guard manifest identity differs")
    original_files = manifest.get("original_files", {})
    if set(original_files) != {"manifest.json", "generated.c", "native_aot_guard.h", *guard["headers"]}:
        raise ValueError("Original guarded file inventory differs")
    if original_files["generated.c"] != guard["output_sha256"]:
        raise ValueError("Original guarded source identity differs")
    repair = repair_module()
    changed_headers = set(repair.PINNED_HEADERS) if manifest["enabled"] else set()
    for name, expected in original_files.items():
        if name not in {"generated.c", "manifest.json", *changed_headers} and files.get(name) != expected:
            raise ValueError(f"Unchanged copied guard input differs: {name}")
    expected_files = set(original_files) - {"manifest.json"} | {GUARD_COPY}
    if manifest["enabled"]:
        expected_files.update((repair.LAYER_NAME, "audio_pool.h", "audio_pool_memset.h"))
        layer = json.loads(paths[repair.LAYER_NAME].read_text())
        if layer != manifest.get("readonly_literals") or layer.get("schema") != repair.SCHEMA or layer.get("enabled") is not True:
            raise ValueError("Copied read-only literal layer differs")
        for name, expected in repair.PINNED_HEADERS.items():
            record = layer.get("headers", {}).get(name, {})
            if (record.get("before_sha256") != expected or original_files.get(name) != expected or
                    record.get("after_sha256") != files.get(name)):
                raise ValueError(f"Copied read-only literal identity differs: {name}")
        for name in ("audio_pool.h", "audio_pool_memset.h"):
            if files.get(name) != manifest["tools"][name]["sha256"]:
                raise ValueError(f"Copied audio-pool helper identity differs: {name}")
        if manifest.get("audio_pool", {}).get("entries") != list(map(hex, audio_pool.ENTRIES)):
            raise ValueError("Copied audio-pool entry inventory differs")
    elif manifest.get("readonly_literals") is not None or manifest.get("audio_pool") is not None:
        raise ValueError("Disabled source copy contains active transform layers")
    if set(files) != expected_files:
        raise ValueError("Copied transform file inventory differs")
    if check_original:
        original = Path(manifest["original_manifest"]["path"])
        if file_hash(original) != manifest["original_manifest"]["sha256"]:
            raise ValueError("Original guard manifest changed")
        repair.validate_original(original)
        for name, expected in manifest["original_files"].items():
            if file_hash(original.parent / name) != expected:
                raise ValueError(f"Original guarded source changed: {name}")
    return manifest


def copy_profile(source, output, *, enabled=False, replace=False):
    source, output = Path(source), Path(output)
    if source.is_symlink() or output.is_symlink():
        raise ValueError("Source and output directory symlinks are unsupported")
    source, output = source.resolve(), output.resolve()
    if (output.exists() and not replace) or source == output or source in output.parents or output in source.parents:
        raise ValueError("Require a new output directory outside the original source")
    if output.exists():
        # Old tools or source inputs can change during an ordinary rebuild.
        # Check the retained copy itself before replacement; trainers use the
        # stronger current-input checks in validate_bundle's default mode.
        validate_bundle(output, check_original=False, check_tools=False)
    paths = regular_files(source)
    repair = repair_module()
    manifest_path, protected = repair.validate_original(source / "manifest.json")
    if set(paths) != set(protected) | {"manifest.json"}:
        raise ValueError("Require only the original guarded source, headers and manifest")
    original_files = {name: file_hash(path) for name, path in sorted(paths.items())}
    original = paths["generated.c"].read_text()
    metadata = inventory(original) if enabled else None
    # Prepare every transformation before creating the output. A changed
    # instruction inventory must not leave a partially usable source bundle.
    transformed = audio_pool.transform(original) if enabled else original
    if enabled:
        for name, expected in repair.PINNED_HEADERS.items():
            if protected.get(name) != expected:
                raise ValueError(f"Read-only literal proof differs: {name}")
            repair.repaired_header(name, paths[name].read_text())
    transformed += marker_source(enabled)
    tools = {"copier": Path(__file__).resolve(), "readonly_literals": REPAIR,
             "audio_pool": Path(audio_pool.__file__).resolve()}
    if enabled:
        tools.update({name: HERE / name for name in ("audio_pool.h", "audio_pool_memset.h")})
    tool_records = {name: {"path": str(path), "sha256": file_hash(path)} for name, path in tools.items()}
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=output.name + ".", dir=output.parent))
    try:
        for name, path in paths.items():
            shutil.copyfile(path, temporary / (GUARD_COPY if name == "manifest.json" else name))
        readonly = None
        if enabled:
            # The established repair patches only its five pinned read-only
            # spans. WAV's repaired copy is present, but the profile forbids
            # WAV batching. All data and write guards remain unchanged.
            readonly = repair.repair_directory(temporary, manifest_path)
            readonly["copies"] = str(output)
            (temporary / repair.LAYER_NAME).write_text(json.dumps(readonly, indent=2) + "\n")
            for name in ("audio_pool.h", "audio_pool_memset.h"):
                shutil.copyfile(HERE / name, temporary / name)
        (temporary / "generated.c").write_text(transformed)
        files = {name: file_hash(path) for name, path in sorted(regular_files(temporary).items())}
        manifest = {"schema": SCHEMA, "enabled": bool(enabled),
            "original_manifest": {"path": str(manifest_path), "sha256": original_files["manifest.json"]},
            "original_files": original_files, "tools": tool_records,
            "files": files, "output_sha256": files["generated.c"],
            "source_inventory": metadata, "readonly_literals": readonly,
            "audio_pool": {"entries": list(map(hex, audio_pool.ENTRIES)),
                "verifier": "Original verifier first, then exact pool literal bytes",
                "body_inventory_sha256": {hex(pc): record[1] for pc, record in audio_pool.BODIES.items()}} if enabled else None,
            "compile_contract": compile_contract(enabled),
            "limits": ["Source selection only; no PCM, schedule or speed acceptance.",
                "The loaded profile marker requires the actual compile options.",
                "The profile marker does not show accepted voice calls or frames.",
                "PGO generation and use must compile the same frozen source and headers."]}
        (temporary / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        validate_bundle(temporary)
        for name, expected in original_files.items():
            if file_hash(source / name) != expected:
                raise ValueError(f"Original source changed during copying: {name}")
        if output.exists():
            backup = Path(tempfile.mkdtemp(prefix=output.name + ".previous.", dir=output.parent))
            backup.rmdir()
            output.rename(backup)
            try:
                temporary.rename(output)
            except BaseException:
                backup.rename(output)
                raise
            shutil.rmtree(backup)
        else:
            temporary.rename(output)
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)
    return manifest


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, help="Original native-aot-guard directory")
    parser.add_argument("--output", type=Path, required=True, help="New source-copy directory")
    parser.add_argument("--enable", action="store_true", help="Apply the combined voice and audio-pool overlays")
    parser.add_argument("--replace", action="store_true", help="Replace a checked old copy only after the new copy passes")
    args = parser.parse_args(argv)
    try:
        result = copy_profile(args.input, args.output, enabled=args.enable, replace=args.replace)
    except (ValueError, KeyError, OSError) as error:
        parser.error(str(error))
    print(json.dumps({"schema": result["schema"], "enabled": result["enabled"],
                      "source": str(args.output.resolve() / "generated.c"),
                      "output_sha256": result["output_sha256"],
                      "compile_contract": result["compile_contract"]}, indent=2))


if __name__ == "__main__":
    main()
