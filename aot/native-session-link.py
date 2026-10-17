#!/usr/bin/env python3
"""Link a trained hot region object with separate O2 cold session regions.

This default-off candidate preserves the hot relocatable text bytes and GCC's
hot/cold block partition. Final relocation and layout still change. The cold
object has no PGO training. Complete runtime checks remain required.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
COMPOSER_PATH = ROOT / "aot/native-session-compose.py"
SPEC = importlib.util.spec_from_file_location("m8_session_composer", COMPOSER_PATH)
COMPOSER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMPOSER)
PUBLIC = ("m8_native_region_step", "m8_native_region_verify_memory",
          "m8_native_region_chain_cache_slots", "m8_native_region_chain")
SERVICE_MARKER = "m8_native_region_inline_services"
RAM_EXPORT = "m8_native_region_ram_exclusives"
SERVICE_EXPORT = SERVICE_MARKER
EVENT_EXPORTS = {"m8_native_region_event_loop", "m8_native_region_event_chain"}
PROTOTYPES = '''#include "native_regions.h"
int m8_session_hot_m8_native_region_step(m8_cpu *,m8_mem *,m8_native_arch *);
int m8_session_hot_m8_native_region_verify_memory(m8_mem *,uint32_t *);
size_t m8_session_hot_m8_native_region_chain_cache_slots(void);
int m8_session_hot_m8_native_region_chain(m8_cpu *,m8_mem *,m8_native_arch *,m8_native_chain *);
int m8_session_cold_m8_native_region_step(m8_cpu *,m8_mem *,m8_native_arch *);
int m8_session_cold_m8_native_region_verify_memory(m8_mem *,uint32_t *);
'''


def prototypes(selected):
    result = PROTOTYPES
    if selected["ram_exclusives"]:
        result += ("int m8_session_hot_m8_native_region_ram_exclusives(void);\n"
                   "int m8_session_cold_m8_native_region_ram_exclusives(void);\n")
    if selected["event_loop"]:
        result += ('#include "native_events.h"\n'
                   'int m8_session_hot_m8_native_region_event_loop(void);\n'
                   'int m8_session_hot_m8_native_region_event_chain(m8_cpu *,m8_mem *,'
                   'm8_native_arch *,m8_native_chain *,m8_native_events *);\n')
    return result
LINKER_SCRIPT = '''/* Keep each input text section intact; GNU ld applies its relocations. */
SECTIONS {
  .text.m8native.entry : { KEEP(wrapper.o(.text.m8native.entry)) KEEP(wrapper.o(.text .text.*)) }
  .text.m8native.hot : { KEEP(hot.o(.text.hot .text.hot.*)) KEEP(hot.o(.text)) }
  .text.m8native.cold : { KEEP(cold.o(.text .text.*)) }
  .text.m8native.hot_unlikely : { KEEP(hot.o(.text.unlikely .text.unlikely.*)) }
} INSERT BEFORE .text;
'''


def wrapper_source(inline_services=False, selected=None):
    selected = selected or {"ram_exclusives": False, "event_loop": False}
    declarations, marker = "", ""
    if inline_services:
        declarations = '''int m8_session_hot_m8_native_region_inline_services(void);
int m8_session_cold_m8_native_region_inline_services(void);
'''
        marker = '''
M8_NATIVE_TEXT(entry)
int m8_native_region_inline_services(void) {
    return m8_session_hot_m8_native_region_inline_services() &&
           m8_session_cold_m8_native_region_inline_services();
}
'''
    return prototypes(selected) + declarations + COMPOSER.public_wrapper(
        {**selected, "inline_services": False}) + marker


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def identity(path):
    path = Path(path).resolve()
    return {"path": str(path), "sha256": sha(path), "bytes": path.stat().st_size}


class Elf:
    """Small ELF64 little-endian reader; no external Python dependency."""
    def __init__(self, path):
        self.path, self.raw = Path(path), Path(path).read_bytes()
        if len(self.raw) < 64 or self.raw[:7] != b"\x7fELF\x02\x01\x01":
            raise ValueError(f"ELF64 little-endian input required: {path}")
        header = struct.unpack_from("<16sHHIQQQIHHHHHH", self.raw)
        self.type, self.machine = header[1:3]
        offset, entry_size, count, names = header[6], header[11], header[12], header[13]
        if entry_size != 64 or not count or names >= count or offset + count * 64 > len(self.raw):
            raise ValueError(f"Unsupported ELF section table: {path}")
        headers = [struct.unpack_from("<IIQQQQIIQQ", self.raw, offset + 64 * i) for i in range(count)]
        strings = self.slice(headers[names][4], headers[names][5])
        self.sections = []
        for row in headers:
            name = self.string(strings, row[0])
            data = b"" if row[1] == 8 else self.slice(row[4], row[5])
            self.sections.append({"name": name, "type": row[1], "flags": row[2],
                                  "address": row[3], "size": row[5], "link": row[6],
                                  "entry_size": row[9], "data": data})

    def slice(self, start, size):
        if start > len(self.raw) or size > len(self.raw) - start:
            raise ValueError(f"ELF data outside file: {self.path}")
        return self.raw[start:start + size]

    @staticmethod
    def string(data, offset):
        end = data.find(b"\0", offset)
        if offset >= len(data) or end < 0:
            raise ValueError("Invalid ELF string offset")
        return data[offset:end].decode()

    def symbols(self, dynamic=False):
        wanted = 11 if dynamic else 2
        result = []
        for section in self.sections:
            if section["type"] != wanted:
                continue
            if section["entry_size"] != 24 or section["size"] % 24 or section["link"] >= len(self.sections):
                raise ValueError(f"Unsupported ELF symbol table: {self.path}")
            names = self.sections[section["link"]]["data"]
            for offset in range(0, section["size"], 24):
                name, info, other, index, value, size = struct.unpack_from("<IBBHQQ", section["data"], offset)
                result.append({"name": self.string(names, name), "bind": info >> 4,
                               "type": info & 15, "visibility": other & 3, "section": index,
                               "value": value, "size": size})
        return result

    def executable(self):
        return [{"name": s["name"], "flags": s["flags"], "address": s["address"],
                 "bytes": s["size"], "sha256": hashlib.sha256(s["data"]).hexdigest()}
                for s in self.sections if s["flags"] & 4]


def run(command, deadline, directory, log=None):
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise TimeoutError("Object composition deadline expired")
    process = subprocess.Popen(list(map(str, command)), cwd=directory, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, start_new_session=True)
    try:
        stdout, stderr = process.communicate(timeout=remaining)
    except BaseException:
        if process.poll() is None:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        stdout, stderr = process.communicate(timeout=10)
        if log:
            Path(log).write_bytes(stdout + stderr)
        raise
    if log:
        Path(log).write_bytes(stdout + stderr)
    if process.returncode:
        raise ValueError(f"Command failed ({process.returncode}): {shlex.join(list(map(str, command)))}\n"
                         + stderr.decode(errors="replace")[-4000:])
    return stdout.decode()


def rename_object(source, target, kind, objcopy, deadline, directory, machine=183, inline_services=False):
    original = Elf(source)
    if original.type != 1 or original.machine != machine:
        raise ValueError(f"Relocatable ELF machine mismatch: {source}")
    symbols = original.symbols()
    exported = {s["name"] for s in symbols if s["bind"] in (1, 2) and s["section"]}
    expected = set(PUBLIC if kind == "hot" else PUBLIC[:2])
    if inline_services:
        expected.add(SERVICE_MARKER)
    if RAM_EXPORT in exported:
        expected.add(RAM_EXPORT)
    if kind == "hot" and EVENT_EXPORTS <= exported:
        expected.update(EVENT_EXPORTS)
    if exported != expected or any(s["type"] != 2 for s in symbols if s["name"] in expected):
        raise ValueError(f"Defined public ABI differs: kind={kind}, actual={sorted(exported)}, required={sorted(expected)}")
    renames = {name: f"m8_session_{kind}_{name}" for name in sorted(expected)}
    command = [objcopy, *[f"--redefine-sym={name}={value}" for name, value in renames.items()], source, target]
    run(command, deadline, directory, Path(directory) / f"{kind}-objcopy.log")
    renamed = Elf(target)
    if original.executable() != renamed.executable():
        raise ValueError(f"Executable section bytes changed during {kind} symbol rename")
    expected_symbols = [{**s, "name": renames.get(s["name"], s["name"])} for s in symbols]
    if renamed.symbols() != expected_symbols:
        raise ValueError(f"Unexpected symbol change during {kind} rename")
    return {"original": identity(source), "renamed": identity(target), "symbol_renames": renames,
            "executable_sections_identical": True, "symbols_only_expected_renames": True,
            "sections": original.executable(), "command": list(map(str, command))}


def recipe(build):
    directory = build / "CMakeFiles/m8native-regions.dir"
    flags_path, link_path = directory / "flags.make", directory / "link.txt"
    values = dict(re.findall(r"^(C_DEFINES|C_INCLUDES|C_FLAGS) = (.*)$", flags_path.read_text(), re.M))
    if set(values) != {"C_DEFINES", "C_INCLUDES", "C_FLAGS"}:
        raise ValueError("Native CMake compile recipe is incomplete")
    flags = sum((shlex.split(values[key]) for key in ("C_DEFINES", "C_INCLUDES", "C_FLAGS")), [])
    command = shlex.split(link_path.read_text())
    compiler, link, index = Path(command[0]).resolve(), [], 1
    while index < len(command):
        token = command[index]
        if token == "-o":
            index += 2
            continue
        if not token.endswith(".o") and not token.startswith("-Wl,--dependency-file="):
            link.append(token)
        index += 1
    if "-shared" not in link or [f for f in flags if re.fullmatch(r"-O.+", f)][-1:] != ["-O2"]:
        raise ValueError("Expected exact O2 compilation and shared-library link recipe")
    if any(f.startswith(("-fprofile", "--coverage", "-ftest-coverage", "-flto")) for f in flags + link):
        raise ValueError("Input recipe must not introduce PGO instrumentation or LTO")
    return compiler, flags, link, flags_path, link_path


def dependencies(compiler, flags, source, deadline, directory):
    result = run([compiler, *flags, "-M", "-MT", "native-object", source], deadline, directory)
    result = result.replace("\\\n", " ")
    if not result.startswith("native-object:"):
        raise ValueError("Unexpected GCC dependency format")
    paths = [Path(name).resolve() for name in shlex.split(result.partition(":")[2])]
    if any(not path.is_file() for path in paths):
        raise ValueError("Missing native compile dependency")
    return paths


def cache_values(path):
    # Blank lines must not become part of the next cache key.
    return dict(re.findall(r'^([^#/\r\n:][^:=\r\n]*):[^=\r\n]*=(.*)$',
                           Path(path).read_text(), re.M))


def build(args):
    deadline = time.monotonic() + args.timeout
    output, build_dir = args.output_dir.resolve(), args.build_dir.resolve()
    if output.exists():
        raise ValueError(f"Output directory already exists: {output}")
    hot, hot_pcs = COMPOSER.load_manifest(args.hot_manifest)
    cold, cold_pcs = COMPOSER.load_manifest(args.cold_manifest)
    COMPOSER.compatible(hot, cold)
    inline_services = getattr(args, "inline_services", False)
    hot_source, cold_source = args.hot_source.read_text(), args.cold_source.read_text()
    for kind, manifest, source in (("hot", hot, hot_source), ("cold", cold, cold_source)):
        actual = manifest.get("inline_services", False)
        if actual is not inline_services:
            raise ValueError(f"Inline-service manifest differs: kind={kind}, actual={actual!r}, required={inline_services!r}")
        if (f"int {SERVICE_MARKER}(" in source) is not inline_services:
            raise ValueError(f"Inline-service source marker differs: kind={kind}, required={inline_services!r}")
    # Service markers were checked above. Select the independent RAM/event
    # features without the source composer's stricter combined-service rule.
    # This is metadata inspection only; the original object text is unchanged.
    def without_service_marker(source):
        return source.replace(f"int {SERVICE_MARKER}(", "int m8_object_checked_service_marker(")
    selected = COMPOSER.features(without_service_marker(hot_source), without_service_marker(cold_source),
                                 {**hot, "inline_services": False},
                                 {**cold, "inline_services": False})
    selected["inline_services"] = inline_services
    if not hot.get("chain_ready") or cold.get("chain_ready") or hot.get("exit_profile") or cold.get("exit_profile"):
        raise ValueError("Require chain-ready hot, unchained cold, and no exit profiling")
    compiler, flags, link, flags_path, link_path = recipe(build_dir)
    if args.untrained:
        cache = cache_values(build_dir / 'CMakeCache.txt')
        frozen_source = Path(cache.get('M8_NATIVE_REGION_SOURCE') or build_dir/'native-regions/generated.c')
        objects = [Path(token) for token in shlex.split(link_path.read_text()) if token.endswith('.o')]
        objects = [(build_dir/path).resolve() if not path.is_absolute() else path.resolve() for path in objects]
        if args.hot_object.resolve() not in objects:
            raise ValueError('Untrained hot object differs from the fixed CMake link recipe')
    else:
        training = json.loads(args.hot_training.read_text())
        if training.get("complete") is not True or "regions" not in training.get("components", []):
            raise ValueError("Hot native PGO training is incomplete")
        rows = [row for row in training["compile_inputs"] if row["component"] == "regions"]
        if len(rows) != 1 or rows[0]["link_flags"] != link:
            raise ValueError("Trained hot link recipe differs from current fixed build")
        # Keep the exact frozen header directory used during training.
        frozen_flags = rows[0]["flags"]
        def without_includes(items):
            return [item for item in items if not item.startswith("-I")]
        if without_includes(frozen_flags) != without_includes(flags):
            raise ValueError("Trained hot compile flags differ from fixed build")
        flags = frozen_flags
        frozen_source = Path(rows[0]["source"])
    source_identity = identity(frozen_source)
    if source_identity["sha256"] != sha(args.hot_source):
        raise ValueError("Selected hot source differs from trained frozen source")
    output.mkdir(parents=True)
    report = {"schema": "m8-native-session-object-link/1", "complete": False, "default_off": True,
              "firmware_sha256": hot["firmware_sha256"], "budget": hot["budget"],
              "hot_compiled_pcs": len(hot_pcs), "cold_compiled_pcs": len(cold_pcs),
              "overlap_compiled_pcs": len(hot_pcs & cold_pcs), "union_compiled_pcs": len(hot_pcs | cold_pcs),
              "hot_text_recompiled": False, "hot_pgo": not args.untrained, "cold_pgo": False,
              "inline_services": inline_services,
              "final_relocation_and_layout_changed": True, "compile_flags": flags,
              "link_flags": link, "commands": []}
    paths = {Path(__file__).resolve(), COMPOSER_PATH, args.hot_object, args.hot_source,
             args.hot_manifest, args.cold_source, args.cold_manifest,
             frozen_source, flags_path, link_path}
    paths.add(build_dir/'CMakeCache.txt' if args.untrained else args.hot_training)
    try:
        stem = re.fullmatch(r"(.*)gcc(?:-\d+)?", compiler.name)
        if not stem:
            raise ValueError("Unsupported GCC executable name")
        tools = {"compiler": compiler}
        for name in ("objcopy", "readelf", "ld"):
            found = shutil.which(stem[1] + name)
            if not found:
                raise ValueError(f"Missing binutils tool: {name}")
            tools[name] = Path(found).resolve()
        machine = run([compiler, "-dumpmachine"], deadline, output).strip()
        if not machine.startswith("aarch64"):
            raise ValueError(f"Compiler target differs: actual={machine}; required=aarch64")
        report["tools"] = {name: {**identity(path), "version": run([path, "--version"], deadline, output).splitlines()[0]}
                           for name, path in tools.items()}
        paths.update(tools.values())
        # The driver executable does not contain GCC's compiler or assembler.
        for option in ("-print-prog-name=cc1", "-print-prog-name=as", "-print-prog-name=collect2"):
            value = run([compiler, option], deadline, output).strip()
            resolved = Path(value) if Path(value).is_absolute() else Path(shutil.which(value) or value)
            if not resolved.is_file():
                raise ValueError(f"Missing GCC program: {option}={value}")
            paths.add(resolved.resolve())
        shutil.copyfile(args.cold_source, output / "cold.c")
        (output / "wrapper.c").write_text(wrapper_source(inline_services, selected))
        (output / "native-sections.ld").write_text(LINKER_SCRIPT)
        paths.update(dependencies(compiler, flags, output / "cold.c", deadline, output))
        paths.update(dependencies(compiler, flags, output / "wrapper.c", deadline, output))
        paths.add(output / "native-sections.ld")
        frozen = {str(path.resolve()): identity(path) for path in sorted(paths)}
        report["inputs"] = frozen
        report["wrapper_matches_composer"] = not inline_services
        report["base_wrapper_matches_composer"] = True
        report["hot"] = rename_object(args.hot_object.resolve(), output / "hot.o", "hot", tools["objcopy"], deadline, output,
                                      inline_services=inline_services)
        expected_hot = set(PUBLIC) | ({RAM_EXPORT} if selected["ram_exclusives"] else set()) | (
            EVENT_EXPORTS if selected["event_loop"] else set()) | ({SERVICE_MARKER} if inline_services else set())
        if set(report["hot"]["symbol_renames"]) != expected_hot:
            raise ValueError("Hot object feature exports differ from selected source and manifest")
        for name in ("cold", "wrapper"):
            command = [compiler, *flags, "-c", f"{name}.c", "-o", f"{name}-original.o" if name == "cold" else "wrapper.o"]
            report["commands"].append(list(map(str, command)))
            run(command, deadline, output, output / f"{name}-compile.log")
        report["cold"] = rename_object(output / "cold-original.o", output / "cold.o", "cold", tools["objcopy"], deadline, output,
                                       inline_services=inline_services)
        expected_cold = set(PUBLIC[:2]) | ({RAM_EXPORT} if selected["ram_exclusives"] else set()) | ({SERVICE_MARKER} if inline_services else set())
        if set(report["cold"]["symbol_renames"]) != expected_cold:
            raise ValueError("Cold object feature exports differ from selected source and manifest")
        libraries = [flag for flag in link if flag.startswith("-l") or flag.endswith((".a", ".so"))]
        options = [flag for flag in link if flag not in libraries]
        command = [compiler, *options, "-Wl,-T,native-sections.ld", "-Wl,-Map,native-sections.map",
                   "wrapper.o", "hot.o", "cold.o", "-o", "libm8native-regions.so", *libraries]
        report["commands"].append(list(map(str, command)))
        run(command, deadline, output, output / "link.log")
        library = output / "libm8native-regions.so"
        final = Elf(library)
        sections = {s["name"]: s for s in final.sections}
        order = [sections[name] for name in (".text.m8native.entry", ".text.m8native.hot",
                                            ".text.m8native.cold")]
        if sections.get(".text.m8native.hot_unlikely", {}).get("size", 0):
            order.append(sections[".text.m8native.hot_unlikely"])
        if not all(s["size"] for s in order) or any(a["address"] + a["size"] > b["address"] for a, b in zip(order, order[1:])):
            raise ValueError("Linked entry/hot/cold/hot-unlikely text order differs")
        # Reject any hot executable section that escaped the declared groups.
        unexpected = [s["name"] for s in Elf(output / "hot.o").sections
                      if s["flags"] & 4 and s["size"] and s["name"] not in (".text", ".text.hot", ".text.unlikely")]
        if unexpected:
            raise ValueError(f"Unhandled hot executable sections: {unexpected}")
        dynamic = final.symbols(dynamic=True)
        exports = {s["name"] for s in dynamic if s["section"] and s["bind"] in (1, 2)}
        if not expected_hot <= exports or any(name.startswith("__gcov") for name in exports):
            raise ValueError("Linked native ABI missing or PGO instrumentation present")
        for path, old in frozen.items():
            if identity(path) != old:
                raise ValueError(f"Pinned input changed while building: {path}")
        report.update(complete=True, library=identity(library), sections=final.executable(),
                      features=selected,
                      exports=sorted(exports), imports=sorted(s["name"] for s in dynamic if s["name"] and not s["section"]))
        report["output_files"] = {p.name: identity(p) for p in sorted(output.iterdir()) if p.is_file()}
        return report
    except BaseException as error:
        report["first_failure"] = str(error)
        raise
    finally:
        (output / "object-link.json").write_text(json.dumps(report, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("build-dir", "hot-object", "hot-source", "hot-manifest",
                 "cold-source", "cold-manifest", "output-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    training = parser.add_mutually_exclusive_group(required=True)
    training.add_argument('--hot-training', type=Path)
    training.add_argument('--untrained',action='store_true',help='Use the exact O2 object from the fixed CMake build')
    parser.add_argument("--timeout", type=float, default=600, help="Total build deadline in seconds (maximum 900)")
    parser.add_argument("--inline-services", action="store_true", help="Require and preserve both hot/cold inline-service markers")
    args = parser.parse_args()
    if not 0 < args.timeout <= 900:
        parser.error("--timeout must be greater than zero and at most 900")
    try:
        report = build(args)
    except (OSError, ValueError, TimeoutError, subprocess.SubprocessError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print(json.dumps({key: report[key] for key in ("complete", "library", "hot_compiled_pcs", "cold_compiled_pcs", "union_compiled_pcs")}, indent=2))


if __name__ == "__main__":
    main()
