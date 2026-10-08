#!/usr/bin/env python3
"""Copy guarded AOT source with default-off shared architectural registers.

The candidate changes register storage, not instructions or arithmetic. The
existing state bridges still synchronize NZCV. Registers are already in the
initialized caller CPU, so their bridges become self-copies. IT snapshots and
rollback retain their original owner and complete CPU state. Guest memory must
not alias the CPU object, as required by the existing restrict-qualified ABI.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil
import tempfile

SCHEMA = "m8-aot-codegen-v2/1"
INPUT_MANIFEST = "codegen-v2-input-manifest.json"
PREREQUISITES = ("M8_AOT_NATIVE_FP", "M8_AOT_FAST_MEMORY",
                 "M8_AOT_FIXED_MEMORY", "M8_AOT_NARROW_CALLS")
PROTECTED = ("M8_AOT_DIAGNOSTIC", "M8_AOT_GUEST_COUNT")
GATE = " && ".join(["defined(M8_AOT_CODEGEN_V2)"] +
                   [f"defined({name})" for name in PREREQUISITES] +
                   [f"!defined({name})" for name in PROTECTED])
FUNCTION = re.compile(r"^static int fn_([0-9a-f]+)\(m8_cpu \*restrict output,m8_mem \*restrict mem\) \{\n", re.M)
OTHER_FUNCTION = re.compile(r"^static int fn_[0-9a-f]+\([^;\n]*\)\s*\{", re.M)
PRIVATE_CPU = ("#ifdef M8_AOT_NARROW_CALLS\nm8_cpu flags=*output;\n"
               "#else\nm8_cpu flags;\n#endif\nm8_cpu *cpu=&flags;\n")
R_IMPORT = re.compile(r"^uint32_t r_(\d+)=output->r\[(\d+)\];\n", re.M)
S_IMPORT = re.compile(r"^(float|uint32_t) s_(\d+)=(m8_fp_float\(output->s\[(\d+)\]\)|output->s\[(\d+)\]);\n", re.M)
REGISTER = re.compile(r"\b([rs])_(\d+)\b")
LEXICAL = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
ASSIGNMENT = re.compile(r"\bs_(\d+)\s*=(?!=)")
SCALAR_WRITE = re.compile(r"\b([rs])_(\d+)\s*(?:(?:[+\-*/%|&^]|<<|>>)?=(?!=)|\+\+|--)")
PUBLICATION = re.compile(r"^#define M8_PUBLISH\(dst\) do \{ ([^\n]+) \} while\(0\)$", re.M)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_hash(path):
    return digest(Path(path).read_bytes())


def code_mask(source):
    """Keep character positions while excluding comments and quoted tokens."""
    return LEXICAL.sub(lambda match: re.sub(r"[^\n]", " ", match[0]), source)


def function_end(mask, opening):
    depth = 0
    for offset in range(opening, len(mask)):
        if mask[offset] == "{":
            depth += 1
        elif mask[offset] == "}":
            depth -= 1
            if depth == 0:
                return offset + 1
    raise ValueError("Unclosed generated function")


def assignment_end(mask, start):
    stack = []
    pairs = {")": "(", "]": "[", "}": "{"}
    for offset in range(start, len(mask)):
        character = mask[offset]
        if character in "([{":
            stack.append(character)
        elif character in ")]}":
            if not stack or stack.pop() != pairs[character]:
                raise ValueError("Unknown S-register assignment expression")
        elif character == ";" and not stack:
            return offset
    raise ValueError("Unclosed S-register assignment")


def rewrite_reads(source, floats):
    edits = [(match.start(), match.end(),
              f"m8_fp_float(cpu->s[{match[2]}])" if floats and match[1] == "s" else
              f"cpu->{match[1]}[{match[2]}]") for match in REGISTER.finditer(code_mask(source))]
    for start, end, replacement in reversed(edits):
        source = source[:start] + replacement + source[end:]
    return source


def rewrite_registers(source, floats):
    """Accept only plain S assignments and non-escaped scalar registers."""
    mask = code_mask(source)
    if re.search(r"&\s*[rs]_\d+\b|(?:\+\+|--)\s*[rs]_\d+\b|"
                 r"\bs_\d+\s*(?:\+\+|--|[+\-*/%|&^]=|<<=|>>=)", mask):
        raise ValueError("Escaped register or unsupported S-register write")
    if re.search(r"\bs_\d+\s*\)\s*=(?!=)", mask):
        raise ValueError("Parenthesized S-register assignment is unsupported")
    for match in REGISTER.finditer(mask):
        if int(match[2]) >= (16 if match[1] == "r" else 32):
            raise ValueError("Register index is outside the architectural state")
    edits = []
    for match in ASSIGNMENT.finditer(mask):
        end = assignment_end(mask, match.end())
        expression = source[match.end():end]
        if ASSIGNMENT.search(code_mask(expression)):
            raise ValueError("Nested S-register assignment is unsupported")
        if not expression.strip():
            raise ValueError("Empty S-register assignment")
        if floats:
            expression = rewrite_reads(expression, floats)
            edits.append((match.start(), end,
                          f"cpu->s[{match[1]}]=m8_fp_bits32({expression.strip()})"))
    for start, end, replacement in reversed(edits):
        source = source[:start] + replacement + source[end:]
    return rewrite_reads(source, floats)


def check_publications(prefix, body, name):
    """Do not turn a private, discarded scalar write into a shared effect.

    Every real scalar write and declared kernel output must cross the original
    narrow publication. SAVE/LOAD may mention more fields because they also
    carry untouched state. Unknown publication forms reject the whole copy.
    """
    # A selected bridge reloads the referenced set, which can be larger than
    # the callee's modified set. An exact same-field reload is a state transfer,
    # not a new architectural write. Each callee is checked independently.
    analyzed = re.sub(r"\br_(\d+)=cpu->r\[\1\];", "", body)
    analyzed = re.sub(r"\bs_(\d+)=(?:m8_fp_float\(cpu->s\[\1\]\)|cpu->s\[\1\]);", "", analyzed)
    writes = {(row[1], int(row[2])) for row in SCALAR_WRITE.finditer(code_mask(analyzed))}
    for line in prefix.splitlines():
        if line.startswith("#define M8_KLOAD_"):
            writes.update((row[1], int(row[2])) for row in SCALAR_WRITE.finditer(code_mask(line)))
    publications = list(PUBLICATION.finditer(prefix))
    definitions = re.findall(r"^#define M8_PUBLISH\(dst\) ([^\n]+)$", prefix, re.M)
    if not definitions or any(not (value == "M8_SAVE(dst)" or
                                  value.startswith("do { ") and value.endswith(" } while(0)"))
                              for value in definitions):
        raise ValueError("Unknown generated publication form")
    for publication in publications:
        fields = {(kind, int(index)) for kind, index in re.findall(
            r"\(dst\)->([rs])\[(\d+)\]\s*=(?!=)", publication[1])}
        missing = writes - fields
        if missing:
            raise ValueError(f"Private scalar write is not published in fn_{name}: {sorted(missing)}")


def shared_source(original):
    """Require both complete scalar variants of every generated function."""
    if "m8_aot_codegen_v2" in original or "codegen-v2-input-manifest" in original:
        raise ValueError("Require source before the codegen V2 copy")
    mask = code_mask(original)
    functions = list(FUNCTION.finditer(mask))
    if not functions or len(functions) != len(OTHER_FUNCTION.findall(mask)):
        raise ValueError("Generated function signature differs")
    variants = Counter()
    output, position = [], 0
    for match in functions:
        end = function_end(mask, match.end() - 2)
        prefix, body = original[position:match.start()], original[match.start():end]
        r_imports, s_imports = list(R_IMPORT.finditer(body)), list(S_IMPORT.finditer(body))
        if (Counter(int(row[1]) for row in r_imports) != Counter(range(16)) or
                any(row[1] != row[2] for row in r_imports)):
            raise ValueError(f"Integer imports differ in fn_{match[1]}")
        if Counter(int(row[2]) for row in s_imports) != Counter(range(32)):
            raise ValueError(f"S-register imports differ in fn_{match[1]}")
        kinds = {row[1] for row in s_imports}
        if len(kinds) != 1:
            raise ValueError("Mixed scalar S-register types")
        floats = kinds == {"float"}
        for row in s_imports:
            index = row[4] if floats else row[5]
            if index != row[2] or (floats != row[3].startswith("m8_fp_float(")):
                raise ValueError("S-register import type or index differs")
        if body.count(PRIVATE_CPU) != 1:
            raise ValueError(f"Private CPU shape differs in fn_{match[1]}")
        if any(label not in body for label in ("memory_fallback:", "fault:", "done:")):
            raise ValueError("Generated exit labels differ")
        if "M8_PUBLISH(output); return" not in body:
            raise ValueError("Generated exit publication differs")
        if "#define M8_SAVE(dst)" not in prefix or "#define M8_LOAD(src)" not in prefix:
            raise ValueError("Generated state bridges differ")
        body = body.replace(PRIVATE_CPU, "m8_cpu *cpu=output;\n", 1)
        body = R_IMPORT.sub("", S_IMPORT.sub("", body))
        if re.search(r"\bflags\b|\b(?:float|uint32_t) [rs]_\d+\b", code_mask(body)):
            raise ValueError("Unexpected private register storage")
        check_publications(prefix, body, match[1])
        output += [rewrite_registers(prefix, floats), rewrite_registers(body, floats)]
        variants[(match[1], floats)] += 1
        position = end
    owners = {name for name, _ in variants}
    if any(variants[(name, mode)] != 1 for name in owners for mode in (False, True)):
        raise ValueError("Require one float and one integer variant per function")
    if REGISTER.search(code_mask(original[position:])):
        raise ValueError("Scalar register outside a generated function")
    output.append(original[position:])
    return "".join(output), {"functions": len(owners), "scalar_variants": len(functions),
        "register_storage": "shared initialized caller CPU", "arithmetic_changed": False,
        "it_owner_changed": False, "memory_guards_changed": False}


def marker_source(stage_flags=1):
    return ("\nuint32_t m8_aot_codegen_v2(void) {\n#if " + GATE +
            "\nreturn 1;\n#else\nreturn 0;\n#endif\n}\n" +
            "uint32_t m8_aot_codegen_v2_flags(void) {\n#if " + GATE +
            f"\nreturn {stage_flags}u;\n#else\nreturn 0;\n#endif\n}}\n")


def transform(original, *, structural=False):
    stage_flags = 1
    if structural:
        import codegen_entry
        candidate, changes = shared_source(original)
        changes["structural"] = True
        stage_flags |= 2 | 4
        candidate, changes["entry"] = codegen_entry.transform(candidate)
    else:
        candidate, changes = shared_source(original)
    changes["stage_flags"] = stage_flags
    return ("#if " + GATE + "\n" + candidate + "\n#else\n" + original +
            "\n#endif\n" + marker_source(stage_flags)), changes


def regular_files(directory):
    directory = Path(directory)
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError("Require a regular source directory")
    paths = {path.name: path for path in directory.iterdir()}
    if any(path.is_symlink() or not path.is_file() for path in paths.values()):
        raise ValueError("Source bundle contains a non-regular file")
    return paths


def validate_input(directory):
    paths = regular_files(directory)
    if not {"generated.c", "manifest.json", "native_aot_guard.h"} <= paths.keys():
        raise ValueError("Require a guarded source bundle")
    manifest = json.loads(paths["manifest.json"].read_text())
    if manifest.get("output_sha256") != file_hash(paths["generated.c"]):
        raise ValueError("Protected generated source hash differs")
    if manifest.get("schema") == "m8-native-aot-guard/1":
        expected = {name: item["output_sha256"] for name, item in manifest["headers"].items()}
        expected["native_aot_guard.h"] = manifest["guard_header_sha256"]
        if manifest.get("transformations", {}).get("rejected_write_paths") != []:
            raise ValueError("Native store guard has unclassified paths")
        if set(paths) != {*expected, "generated.c", "manifest.json"}:
            raise ValueError("Protected source closure differs")
    elif manifest.get("schema") == "m8-native-performance/1":
        expected = manifest["files"]
        if set(paths) != {*expected, "manifest.json"}:
            raise ValueError("Protected performance source closure differs")
    else:
        raise ValueError("Unknown protected input manifest")
    for name, wanted in expected.items():
        if file_hash(paths[name]) != wanted:
            raise ValueError(f"Protected input hash differs: {name}")
    return paths


def validate_bundle(directory, *, check_original=True, check_tool=True):
    paths = regular_files(directory)
    manifest = json.loads(paths["manifest.json"].read_text())
    if manifest.get("schema") != SCHEMA or set(paths) != {*manifest.get("files", {}), "manifest.json"}:
        raise ValueError("Codegen V2 file inventory differs")
    for name, wanted in manifest["files"].items():
        if file_hash(paths[name]) != wanted:
            raise ValueError(f"Codegen V2 copy differs: {name}")
    if manifest.get("output_sha256") != manifest["files"].get("generated.c"):
        raise ValueError("Codegen V2 output identity differs")
    if manifest.get("input_files", {}).get("manifest.json") != manifest["files"].get(INPUT_MANIFEST):
        raise ValueError("Protected input manifest identity differs")
    if manifest.get("gate") != GATE or manifest.get("marker") != "m8_aot_codegen_v2":
        raise ValueError("Codegen V2 compile contract differs")
    flags = manifest.get("changes", {}).get("stage_flags", 1)
    if type(flags) is not int or flags not in (1, 7):
        raise ValueError(f"Codegen V2 stage selection differs: flags={flags!r}")
    expected_tools = {"codegen_entry"} if flags & 2 else set()
    if set(manifest.get("stage_tools", {})) != expected_tools:
        raise ValueError("Codegen V2 stage tool inventory differs")
    if check_tool and file_hash(manifest["tool"]["path"]) != manifest["tool"]["sha256"]:
        raise ValueError("Codegen V2 copier changed")
    for name, record in manifest.get("stage_tools", {}).items():
        if check_tool and file_hash(record["path"]) != record["sha256"]:
            raise ValueError(f"Codegen V2 stage tool changed: {name}")
    if check_original:
        original = validate_input(manifest["input"])
        if {name: file_hash(path) for name, path in original.items()} != manifest["input_files"]:
            raise ValueError("Original protected source changed")
    return manifest


def copy_source(source, output, *, replace=False, structural=False):
    source, output = Path(source), Path(output)
    if source.is_symlink() or output.is_symlink():
        raise ValueError("Source and output symlinks are unsupported")
    source, output = source.resolve(), output.resolve()
    if source == output or source in output.parents or output in source.parents:
        raise ValueError("Output must be separate from the protected source")
    if output.exists():
        if not replace:
            raise ValueError("Output exists; use --replace for a checked copy")
        if any(output.iterdir()):
            validate_bundle(output, check_original=False, check_tool=False)
    paths = validate_input(source)
    identities = {name: file_hash(path) for name, path in paths.items()}
    stage_names = ["codegen_entry"] if structural else []
    tools = {name: {"path": str(Path(__file__).with_name(name + ".py")),
        "sha256": file_hash(Path(__file__).with_name(name + ".py"))} for name in stage_names}
    copier_hash = file_hash(__file__)
    original = paths["generated.c"].read_bytes()
    generated, changes = transform(original.decode("utf-8"), structural=structural)
    if identities != {name: file_hash(path) for name, path in paths.items()}:
        raise ValueError("Protected input changed during source generation")
    if copier_hash != file_hash(__file__) or any(
            file_hash(record["path"]) != record["sha256"] for record in tools.values()):
        raise ValueError("Codegen tool changed during source generation")
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=".codegen-v2-", dir=output.parent))
    try:
        for name, path in paths.items():
            if name != "generated.c":
                shutil.copyfile(path, staging / (INPUT_MANIFEST if name == "manifest.json" else name))
        (staging / "generated.c").write_bytes(generated.encode("utf-8"))
        manifest = {"schema": SCHEMA, "input": str(source), "input_files": identities,
            "input_sha256": digest(original), "output_sha256": file_hash(staging / "generated.c"),
            "gate": GATE, "marker": "m8_aot_codegen_v2", "changes": changes,
            "tool": {"path": str(Path(__file__).resolve()), "sha256": copier_hash},
            "files": {path.name: file_hash(path) for path in sorted(staging.iterdir())}}
        manifest["stage_tools"] = tools
        (staging / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        validate_bundle(staging)
        if output.exists():
            shutil.rmtree(output)
        staging.rename(output)
        return manifest
    finally:
        if staging.exists():
            shutil.rmtree(staging)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--replace", action="store_true")
    parser.add_argument("--structural", action="store_true", help="Split hot entry and cold edges")
    args = parser.parse_args()
    try:
        result = copy_source(args.input, args.output, replace=args.replace,
                             structural=args.structural)
    except (ValueError, KeyError, OSError) as error:
        parser.error(str(error))
    print(json.dumps({"output": str(args.output.resolve()), "output_sha256": result["output_sha256"],
                      "changes": result["changes"]}))


if __name__ == "__main__":
    main()
