#!/usr/bin/env python3
"""Copy generated regions with default-off shared architectural registers."""
import argparse
import json
from pathlib import Path
import re

import codegen_v2 as V2

MARKER = "/* m8-native-region-shared-state/1 */"
FUNCTION = re.compile(r"^static int region_([0-9a-f]+)(?:_impl)?\(m8_cpu \*restrict output,m8_mem \*restrict mem[^\n]*\) \{\n", re.M)


def transform(original):
    if MARKER in original:
        raise ValueError("Require regions before the shared-state copy")
    mask = V2.code_mask(original)
    functions = list(FUNCTION.finditer(mask))
    if not functions:
        raise ValueError("Require scalarized native regions")
    output, position, variants = [], 0, []
    for match in functions:
        end = V2.function_end(mask, match.end() - 2)
        prefix, body = original[position:match.start()], original[match.start():end]
        r_imports, s_imports = list(V2.R_IMPORT.finditer(body)), list(V2.S_IMPORT.finditer(body))
        if (sorted(int(row[1]) for row in r_imports) != list(range(16)) or
                any(row[1] != row[2] for row in r_imports) or
                sorted(int(row[2]) for row in s_imports) != list(range(32))):
            raise ValueError("Region register import shape differs: " + match[1])
        kinds = {row[1] for row in s_imports}
        if len(kinds) != 1 or body.count(V2.PRIVATE_CPU) != 1:
            raise ValueError("Region private CPU shape differs: " + match[1])
        floats = kinds == {"float"}
        for row in s_imports:
            if (row[4] if floats else row[5]) != row[2]:
                raise ValueError("Region S-register import index differs")
        if not all(token in body for token in ("done:", "fault:", "M8_PUBLISH(output); return")):
            raise ValueError("Region exit publication shape differs")
        V2.check_publications(prefix, body, match[1])
        body = body.replace(V2.PRIVATE_CPU, "m8_cpu *cpu=output;\n", 1)
        body = V2.R_IMPORT.sub("", V2.S_IMPORT.sub("", body))
        if re.search(r"\bflags\b|\b(?:float|uint32_t) [rs]_\d+\b", V2.code_mask(body)):
            raise ValueError("Unexpected private register storage")
        output += [V2.rewrite_registers(prefix, floats),
                   V2.rewrite_registers(body, floats)]
        variants.append({"region": match[1], "float_locals": floats})
        position = end
    tail = original[position:]
    if V2.REGISTER.search(V2.code_mask(tail)):
        raise ValueError("Scalar register outside a region")
    output.append(tail)
    candidate = "".join(output)
    result = (MARKER + "\n#if defined(M8_NATIVE_REGION_SHARED_STATE)\n" + candidate +
              "\n#else\n" + original + "\n#endif\n")
    return result, {"schema": "m8-native-region-shared-state/1", "default_off": True,
                    "variants": variants, "entry_switch_changed": False,
                    "arithmetic_changed": False, "memory_checks_changed": False,
                    "event_boundaries_changed": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    source, report = transform(args.input.read_text())
    report.update(input_sha256=V2.file_hash(args.input), output_sha256=V2.digest(source.encode()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(source)
    if args.manifest:
        args.manifest.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
