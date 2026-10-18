#!/usr/bin/env python3
"""Split normal entries from exact secondary-PC resumes in shared AOT code.

Run after codegen_v2.shared_source. The normal
entry starts at its original entry label. The cold copy retains every original
PC entrance and computed redirect. Cold memory and service exits publish CPU
state before a shared helper; ordinary guest returns stay inline. The original
input remains under the inverse release guard.
"""
from __future__ import annotations

from collections import Counter
import json
import re
from pathlib import Path

import codegen_v2
import local_flags

GATE = codegen_v2.GATE
FUNCTION = re.compile(r"^static int fn_([0-9a-f]+)\(m8_cpu \*restrict output,m8_mem \*restrict mem\) \{\n", re.M)
ANY_FUNCTION = re.compile(r"^static int fn_([0-9a-f]+)\([^;\n]*\)\s*\{\n", re.M)
EXITS = '''memory_fallback:
M8_COUNT_REWIND();
if(it_active) {m8_it_abort(cpu); M8_LOAD(cpu);}
{int returned=M8_AOT_EXIT; M8_PUBLISH(output); return returned;}
fault:
if(m8_aot_guard_declined) {cpu->r[15]=pc; goto memory_fallback;}
M8_COUNT_REWIND();
if(it_active) {int io=tx.saw_slow; m8_it_abort(cpu); M8_LOAD(cpu); {int returned=io ? M8_AOT_FAULT : M8_AOT_EXIT; M8_PUBLISH(output); return returned;}}
cpu->r[15]=pc; {int returned=m8_slow_fallback ? M8_AOT_EXIT : M8_AOT_FAULT; M8_PUBLISH(output); return returned;}
done:
if(it_active) m8_it_commit();
{int returned=M8_AOT_EXIT; M8_PUBLISH(output); return returned;}
}'''
OUTLINED = '''memory_fallback:
M8_COUNT_REWIND();
M8_PUBLISH(output);
return m8_codegen_entry_cold_exit(output,it_active ? &tx : NULL,0u,pc,0);
fault:
M8_COUNT_REWIND();
M8_PUBLISH(output);
return m8_codegen_entry_cold_exit(output,it_active ? &tx : NULL,1u,pc,m8_aot_guard_declined);
service_exit:
M8_PUBLISH(output);
return m8_codegen_entry_cold_exit(output,it_active ? &tx : NULL,2u,pc,0);
done:
if(it_active) m8_it_commit();
{int returned=M8_AOT_EXIT; M8_PUBLISH(output); return returned;}
}'''
COLD_HELPER = r'''
#if defined(__GNUC__) || defined(__clang__)
__attribute__((cold,noinline))
#endif
static int m8_codegen_entry_cold_exit(m8_cpu *cpu,m8_it_tx *tx,
                                     unsigned kind,uint32_t pc,int declined) {
    /* tx is the original active journal owner. No observer runs between
     * publication and this helper. Abort retains the original TLS owner,
     * RAM rollback, partial MMIO effects and saved host FP environment. */
    if(kind==2u) {
        if(tx) m8_it_commit();
        return M8_AOT_EXIT;
    }
    if(kind==0u || declined) {
        if(declined) cpu->r[15]=pc;
        if(tx) m8_it_abort(cpu);
        return M8_AOT_EXIT;
    }
    if(tx) {
        int io=tx->saw_slow;
        m8_it_abort(cpu);
        return io ? M8_AOT_FAULT : M8_AOT_EXIT;
    }
    cpu->r[15]=pc;
    return m8_slow_fallback ? M8_AOT_EXIT : M8_AOT_FAULT;
}
'''
SLOW_EXIT = re.compile(r"^if\(slow\) (?:\{cpu->r\[15\]=0x[0-9a-f]+u; goto done;\}|goto done;)$", re.M)
DISPATCH_LINE = re.compile(r"(?:switch\(pc\) \{|\}|case 0x[0-9a-f]+u: goto L_[0-9a-f]+;|"
                           r"default: cpu->r\[15\]=pc; goto done;)")
EXACT_CALL = re.compile(
    r"cpu->r\[15\]=0x(?P<pc>[0-9a-f]+)u;\s*"
    r"(?:M8_SAVE\(cpu\);\s*int (?:call_result|returned)=|"
    r"\{?int (?P<result>call_result|returned);\s*"
    r"#ifdef M8_AOT_NARROW_CALLS\s*\n"
    r"(?P<publication>[^\n#{}]{0,16384})\n"
    r"#else\s*\nM8_SAVE\(cpu\);\s*\n#endif\s*\n(?P=result)=)"
    r"(?P<callee>fn_(?P<entry>[0-9a-f]+))\(cpu,mem\);")


def same_field_publication(source, destination):
    """Recognize state copies that cannot change PC or call an observer."""
    pattern = re.escape(destination)
    for statement in source.split(";"):
        statement = re.sub(r"\s+", "", statement)
        if not statement:
            continue
        register = re.fullmatch(pattern + r"->r\[(\d+)\]=cpu->r\[\1\]", statement)
        if register and int(register[1]) < 16:
            continue
        register = re.fullmatch(pattern + r"->s\[(\d+)\]="
                                r"(?:cpu->s\[\1\]|m8_fp_bits32\(m8_fp_float\(cpu->s\[\1\]\)\))",
                                statement)
        if register and int(register[1]) < 32:
            continue
        if statement in (destination + "->xpsr=M8_FLAGS_WORD",
                         destination + "->fpscr=cpu->fpscr"):
            continue
        return False
    return True


def direct_hot_calls(body, entries, safe_save, *, safe_flags=False):
    """Select only exact entries with an adjacent, non-observing state bridge.

    The strict bridge grammar is bounded to one generated publication line.
    Unknown statements, callbacks, labels and secondary PCs keep the wrapper.
    Replacement changes no line count, PC/LR write or state publication.
    """
    if not safe_flags:
        return body, 0
    mask = codegen_v2.code_mask(body)
    edits = []
    for match in EXACT_CALL.finditer(mask):
        if (match["entry"] not in entries or
                int(match["entry"], 16) != int(match["pc"], 16)):
            continue
        if match["publication"] is None:
            if not safe_save:
                continue
        elif not same_field_publication(match["publication"], "cpu"):
            continue
        edits.append((match.start("callee"), match.end("callee"),
                      "hot_" + match["callee"]))
    for start, end, replacement in reversed(edits):
        body = body[:start] + replacement + body[end:]
    return body, len(edits)


def bare_registers(mask):
    """Exclude C member names while retaining every bare guest local."""
    registers = []
    for match in codegen_v2.REGISTER.finditer(mask):
        previous = match.start() - 1
        while previous >= 0 and mask[previous].isspace():
            previous -= 1
        if previous >= 0 and (mask[previous] == "." or mask[previous - 1:previous + 1] == "->"):
            continue
        registers.append((match[1], match[2]))
    return registers


def load_clone_profile(path):
    """Load a secondary-entry clone profile reusing the web selection data.

    The profile maps lowered hex roots to secondary guest PCs from recorded
    real-input histograms (aot/catalogs/entry-clones.json). Native clone names
    are fixed (hot_fn_<root>_at_<pc>), so the web name prefix is accepted
    and ignored. Returns {root: [pcs]}.
    """
    profile = json.loads(Path(path).read_text())
    if profile.get('schema') != 'm8-web-entry-clones/1':
        raise ValueError('entry-clone profile schema')
    roots = profile.get('roots', {})
    if not roots or not isinstance(roots, dict):
        raise ValueError('entry-clone roots')
    for root, pcs in roots.items():
        if re.fullmatch(r'[0-9a-f]+', root) is None:
            raise ValueError(f'entry-clone root hex: {root!r}')
        if (not pcs or len(set(pcs)) != len(pcs)
                or any(re.fullmatch(r'[0-9a-f]+', pc) is None or pc == root for pc in pcs)):
            raise ValueError(f'entry-clone PCs for {root!r}')
    return {root: list(pcs) for root, pcs in roots.items()}


def check_clone_map(clones):
    if not isinstance(clones, dict) or not clones:
        raise ValueError('entry-clone roots')
    for root, pcs in clones.items():
        if re.fullmatch(r'[0-9a-f]+', root) is None:
            raise ValueError(f'entry-clone root hex: {root!r}')
        if (not isinstance(pcs, (list, tuple)) or not pcs or len(set(pcs)) != len(pcs)
                or any(re.fullmatch(r'[0-9a-f]+', pc) is None or pc == root for pc in pcs)):
            raise ValueError(f'entry-clone PCs for {root!r}')
    return {root: list(pcs) for root, pcs in clones.items()}


def split_body(body, owner, *, entries=(), safe_save=False, safe_flags=False, clones=()):
    """Reject unknown admission or cold-exit shapes before changing code."""
    entry = f"L_{owner}:\n"
    mask = codegen_v2.code_mask(body)
    registers = bare_registers(mask)
    if body.count("m8_cpu *cpu=output;\n") != 1 or registers:
        raise ValueError(f"fn_{owner} is not shared-register code")
    if (body.count("goto dispatch;") != 1 or body.count("\ndispatch:\n") != 1 or
            body.count("\nredirect:\n") != 1 or body.count(entry) != 1):
        raise ValueError(f"fn_{owner} entry or redirect shape differs")
    if body.count(EXITS) != 1 or not body.endswith(EXITS):
        raise ValueError(f"fn_{owner} cold exit contract differs")
    if body.count("uint32_t pc=cpu->r[15];") != 1:
        raise ValueError(f"fn_{owner} initial PC storage differs")
    original = body
    body, direct_calls = direct_hot_calls(body, entries, safe_save, safe_flags=safe_flags)
    if len(set(clones)) != len(list(clones)) or any(
            re.fullmatch(r'[0-9a-f]+', pc) is None or pc == owner for pc in clones):
        raise ValueError(f"fn_{owner} secondary clone PCs differ")
    for pc in clones:
        if re.search(r'\bhot_fn_' + owner + r'_at_' + pc + r'\b', codegen_v2.code_mask(body)):
            raise ValueError(f"fn_{owner} clone name is already used: {pc}")
    dispatch_start = body.index("\ndispatch:\n")
    dispatch_end = body.index("\nmemory_fallback:\n", dispatch_start)
    dispatch = body[dispatch_start:dispatch_end]
    if (not re.search(r"switch\(pc\) \{", dispatch) or
            "default: cpu->r[15]=pc; goto done;" not in dispatch or
            any(not DISPATCH_LINE.fullmatch(line) for line in dispatch.splitlines()[2:])):
        raise ValueError(f"fn_{owner} initial dispatcher has an unclassified effect")
    for pc in clones:
        if dispatch.count(f"case 0x{pc}u: goto L_{pc};") != 1:
            raise ValueError(f"fn_{owner} dispatch does not contain 0x{pc} exactly once")
        if body.count(f"\nL_{pc}:\n") != 1:
            raise ValueError(f"fn_{owner} secondary label L_{pc} differs")
    # This switch has no incoming edge after replacing the sole initial goto.
    # The separate redirect switch remains reachable for computed guest jumps.
    def hot_variant(pc, name):
        variant = body[:dispatch_start] + body[dispatch_end:]
        variant = variant.replace("goto dispatch;", f"goto L_{pc};", 1)
        variant = variant.replace("uint32_t pc=cpu->r[15];", f"uint32_t pc=0x{pc}u;", 1)
        slow_matches = list(SLOW_EXIT.finditer(variant))
        all_slow = list(re.finditer(r"\bif\s*\(slow\)", codegen_v2.code_mask(variant)))
        if len(slow_matches) != len(all_slow):
            raise ValueError(f"fn_{owner} has an unclassified slow exit")
        variant = SLOW_EXIT.sub(lambda match: match[0].replace("goto done;", "goto service_exit;"), variant)
        variant = variant.replace(EXITS, OUTLINED, 1)
        variant = variant.replace(f"static int fn_{owner}(", f"static int {name}(", 1)
        return variant, len(slow_matches)
    hot, slow_edges = hot_variant(owner, f"hot_fn_{owner}")
    secondaries, clone_rows = [], []
    for pc in clones:
        name = f"hot_fn_{owner}_at_{pc}"
        variant, variant_slows = hot_variant(pc, name)
        if variant_slows != slow_edges:
            raise ValueError(f"fn_{owner} secondary slow edges differ: {pc}")
        secondaries.append(variant)
        clone_rows.append({"pc": f"0x{pc}", "name": name,
                           "dispatch_cases_removed": dispatch.count("case "),
                           "slow_edges": variant_slows})
    cold = body.replace(f"static int fn_{owner}(", f"static int cold_fn_{owner}(", 1)
    cold = "#if defined(__GNUC__) || defined(__clang__)\n__attribute__((cold,noinline))\n#endif\n" + cold
    if clone_rows:
        cases = (f"case 0x{owner}u: return hot_fn_{owner}(output,mem);\n" +
                 "".join(f"case 0x{row['pc'][2:]}u: return {row['name']}(output,mem);\n"
                         for row in clone_rows))
        wrapper = (f"static int fn_{owner}(m8_cpu *restrict output,m8_mem *restrict mem) {{\n"
                   f"switch(output->r[15]) {{\n{cases}"
                   f"default: return cold_fn_{owner}(output,mem);\n}}\n}}")
    else:
        wrapper = (f"static int fn_{owner}(m8_cpu *restrict output,m8_mem *restrict mem) {{\n"
                   f"if(output->r[15]==0x{owner}u) return hot_fn_{owner}(output,mem);\n"
                   f"return cold_fn_{owner}(output,mem);\n}}")
    replacement = "#if " + GATE + "\n" + "\n".join([cold, hot, *secondaries, wrapper]) + "\n#else\n" + original + "\n#endif"
    return replacement, {"entry": f"0x{owner}", "hot_initial_dispatch": False,
                         "cold_initial_dispatch": True, "computed_redirect_preserved": True,
                         "s_register_storage": "CPU",
                         "slow_edges": slow_edges, "outlined_labels": ["memory_fallback", "fault", "service_exit"],
                         "direct_hot_calls": direct_calls, "clones": clone_rows,
                         "secondary_clones": len(clone_rows)}


def transform(source, *, clones=None):
    """Return copied source and metadata; do not change input files.

    clones maps hex roots to secondary hex PCs. Each listed root must own a
    generated function, and each PC must appear as a dispatch case with its
    label exactly once; otherwise the build fails instead of skipping.
    """
    if ("m8_codegen_entry_cold_exit" in source or
            re.search(r"\b(?:hot_fn_|cold_fn_)\w+", source)):
        raise ValueError("Require source before the entry split")
    clone_map = check_clone_map(clones) if clones is not None else {}
    mask = codegen_v2.code_mask(source)
    functions = list(ANY_FUNCTION.finditer(mask))
    if not functions:
        raise ValueError("Require shared-register functions")
    allowed = {match.start(): match for match in FUNCTION.finditer(mask)}
    entries = {match[1] for match in functions}
    saves = re.findall(r"^#define M8_SAVE\(dst\) do\s*\{([^\n]+)\}\s*while\(0\)$", mask, re.M)
    all_saves = re.findall(r"^#define M8_SAVE\(dst\)", mask, re.M)
    safe_save = (bool(saves) and len(saves) == len(all_saves) and
                 all(same_field_publication(save, "(dst)") for save in saves))
    words = re.findall(r"^#define M8_FLAGS_WORD ([^\n]+)$", mask, re.M)
    flags_word = local_flags.MACROS.splitlines()[0].split(" ", 2)[2]
    safe_flags = bool(words) and all(re.sub(r"\s+", "", word) ==
                                     re.sub(r"\s+", "", flags_word) for word in words)
    edits, variants, rows = [], Counter(), []
    for match in functions:
        if match.start() not in allowed:
            raise ValueError(f"fn_{match[1]} public signature differs")
        end = codegen_v2.function_end(mask, match.end() - 2)
        body = source[match.start():end]
        replacement, metadata = split_body(body, match[1], entries=entries,
                                           safe_save=safe_save, safe_flags=safe_flags,
                                           clones=tuple(clone_map.get(match[1], ())))
        edits.append((match.start(), end, replacement))
        variants[match[1]] += 1
        rows.append(metadata)
    if any(count != 2 for count in variants.values()):
        raise ValueError("Require two shared scalar variants per split owner")
    missing = [root for root in clone_map if root not in variants]
    if missing:
        raise ValueError(f"entry-clone roots without a generated function: {missing}")
    if edits:
        pieces, position = [], 0
        for index, (start, end, replacement) in enumerate(edits):
            pieces.append(source[position:start])
            if index == 0:
                pieces.append("#if " + GATE + "\n" + COLD_HELPER + "#endif\n")
                declarations = []
                for owner in sorted(entries):
                    declarations.append(
                        f"static int hot_fn_{owner}(m8_cpu *restrict,m8_mem *restrict);\n")
                    for pc in clone_map.get(owner, ()):
                        declarations.append(
                            f"static int hot_fn_{owner}_at_{pc}(m8_cpu *restrict,m8_mem *restrict);\n")
                pieces.append("#if " + GATE + "\n" + "".join(declarations) +
                              "#endif\n")
            pieces.append(replacement)
            position = end
        pieces.append(source[position:])
        source = "".join(pieces)
    return source, {"schema": "m8-aot-codegen-entry/1", "input_register_storage": "shared CPU",
                    "split_owners": sorted(variants), "split_scalar_variants": len(rows),
                    "functions": rows,
                    "ordinary_returns_outlined": False, "it_owner_changed": False,
                    "direct_hot_calls": {"gate": GATE,
                                         "source_calls": sum(row["direct_hot_calls"] for row in rows)},
                    "entry_clones": {"roots": sorted(clone_map),
                                     "total": sum(len(pcs) for pcs in clone_map.values()),
                                     "secondary_clones": sum(row["secondary_clones"] for row in rows)},
                    "original_guard": GATE}
