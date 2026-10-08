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
import re

import codegen_v2

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


def split_body(body, owner):
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
    dispatch_start = body.index("\ndispatch:\n")
    dispatch_end = body.index("\nmemory_fallback:\n", dispatch_start)
    dispatch = body[dispatch_start:dispatch_end]
    if (not re.search(r"switch\(pc\) \{", dispatch) or
            "default: cpu->r[15]=pc; goto done;" not in dispatch or
            any(not DISPATCH_LINE.fullmatch(line) for line in dispatch.splitlines()[2:])):
        raise ValueError(f"fn_{owner} initial dispatcher has an unclassified effect")
    # This switch has no incoming edge after replacing the sole initial goto.
    # The separate redirect switch remains reachable for computed guest jumps.
    hot = body[:dispatch_start] + body[dispatch_end:]
    hot = hot.replace("goto dispatch;", f"goto L_{owner};", 1)
    hot = hot.replace("uint32_t pc=cpu->r[15];", f"uint32_t pc=0x{owner}u;", 1)
    slow_matches = list(SLOW_EXIT.finditer(hot))
    all_slow = list(re.finditer(r"\bif\s*\(slow\)", codegen_v2.code_mask(hot)))
    if len(slow_matches) != len(all_slow):
        raise ValueError(f"fn_{owner} has an unclassified slow exit")
    hot = SLOW_EXIT.sub(lambda match: match[0].replace("goto done;", "goto service_exit;"), hot)
    hot = hot.replace(EXITS, OUTLINED, 1)
    hot = hot.replace(f"static int fn_{owner}(", f"static int hot_fn_{owner}(", 1)
    cold = body.replace(f"static int fn_{owner}(", f"static int cold_fn_{owner}(", 1)
    cold = "#if defined(__GNUC__) || defined(__clang__)\n__attribute__((cold,noinline))\n#endif\n" + cold
    wrapper = (f"static int fn_{owner}(m8_cpu *restrict output,m8_mem *restrict mem) {{\n"
               f"if(output->r[15]==0x{owner}u) return hot_fn_{owner}(output,mem);\n"
               f"return cold_fn_{owner}(output,mem);\n}}")
    replacement = "#if " + GATE + "\n" + cold + "\n" + hot + "\n" + wrapper + "\n#else\n" + body + "\n#endif"
    return replacement, {"entry": f"0x{owner}", "hot_initial_dispatch": False,
                         "cold_initial_dispatch": True, "computed_redirect_preserved": True,
                         "s_register_storage": "CPU",
                         "slow_edges": len(slow_matches), "outlined_labels": ["memory_fallback", "fault", "service_exit"]}


def transform(source):
    """Return copied source and metadata; do not change input files."""
    if "m8_codegen_entry_cold_exit" in source or re.search(r"\b(?:hot_fn_|cold_fn_)\w+", source):
        raise ValueError("Require source before the entry split")
    mask = codegen_v2.code_mask(source)
    functions = list(ANY_FUNCTION.finditer(mask))
    if not functions:
        raise ValueError("Require shared-register functions")
    allowed = {match.start(): match for match in FUNCTION.finditer(mask)}
    edits, variants, rows = [], Counter(), []
    for match in functions:
        if match.start() not in allowed:
            raise ValueError(f"fn_{match[1]} public signature differs")
        end = codegen_v2.function_end(mask, match.end() - 2)
        body = source[match.start():end]
        replacement, metadata = split_body(body, match[1])
        edits.append((match.start(), end, replacement))
        variants[match[1]] += 1
        rows.append(metadata)
    if any(count != 2 for count in variants.values()):
        raise ValueError("Require two shared scalar variants per split owner")
    if edits:
        pieces, position = [], 0
        for index, (start, end, replacement) in enumerate(edits):
            pieces.append(source[position:start])
            if index == 0:
                pieces.append("#if " + GATE + "\n" + COLD_HELPER + "#endif\n")
            pieces.append(replacement)
            position = end
        pieces.append(source[position:])
        source = "".join(pieces)
    return source, {"schema": "m8-aot-codegen-entry/1", "input_register_storage": "shared CPU",
                    "split_owners": sorted(variants), "split_scalar_variants": len(rows),
                    "functions": rows,
                    "ordinary_returns_outlined": False, "it_owner_changed": False,
                    "original_guard": GATE}
