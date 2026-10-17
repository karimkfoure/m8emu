#!/usr/bin/env python3
"""Compose default-off hot regions and cold session regions without regeneration.

First run ``cold-pcs --hot-manifest H --full-manifest F --output cold.txt``.
Feed cold.txt to the unchanged native_regions.py with --pc-file, the same
firmware/budget, and no --chain-ready or --layout. Then run ``compose`` with
both source/manifest pairs and --output-dir. The output generated.c embeds
both bodies and the chain helper, so the existing PGO trainer can freeze one
source. It retains the qualified hot resolver, edge IDs, and cache slot count.

Compile with the qualified hot build's identical FP, memory, and service
flags. An optional GNU ld script puts hot text before cold text:
``-Wl,-T,/absolute/output/native-sections.ld``. The private prefixes and section
attributes preserve C bodies, not an existing linked object or PGO result.
Train the aggregate afresh and compare complete PCM/state before measuring.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ABI_TYPES = {"m8_native_region_fn", "m8_native_region_exit", "m8_native_region_chain_fn",
             "m8_native_chain_slot", "m8_native_region_read", "m8_native_region_write",
             "m8_native_region_event_fn", "m8_native_region_exclusive_pointer",
             "region_calls", "region_guest"}
TOKEN = re.compile(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\b[A-Za-z_]\w*\b')
FUNCTION = re.compile(
    r'^(?P<declaration>(?:static\s+)?(?:inline\s+)?(?:int|bool|size_t|void|unsigned|'
    r'm8_native_region_fn|m8_native_chain_slot)\s+\*?\s*(?P<name>[A-Za-z_]\w*)'
    r'\s*\([^;{}]*\)\s*)\{', re.M)
CHAIN_INCLUDE = '#include "native_region_chain.h"'
EVENT_INCLUDE = '#include "native_region_event_chain.h"'
LINKER_SCRIPT = '''/* Default-off: preserve hot/cold text as separate contiguous outputs. */
SECTIONS {
  .text.m8native.hot : { KEEP(*(.text.m8native.entry)) KEEP(*(.text.m8native.hot)) }
  .text.m8native.cold : { KEEP(*(.text.m8native.cold)) }
} INSERT BEFORE .text;
'''


def digest(data):
    return hashlib.sha256(data).hexdigest()


def load_manifest(path):
    data = json.loads(Path(path).read_text())
    if data.get("schema") != "m8-native-regions/1":
        raise ValueError(f"Unsupported region manifest: {path}")
    compiled = {int(pc) for region in data["regions"] for pc in region["instructions"]}
    if len(compiled) != data["compiled_instructions"]:
        raise ValueError(f"Compiled PC count differs: {path}")
    return data, compiled


def compatible(hot, other):
    for key in ("firmware_sha256", "budget"):
        if hot.get(key) != other.get(key):
            raise ValueError(f"Region {key} differs: {hot.get(key)!r} vs {other.get(key)!r}")


def profile_weights(manifest, root):
    """Recover discovery weights exactly; they are not a steady-state rate."""
    weights = Counter()
    for item in manifest["profiles"]:
        path = Path(item["path"])
        if not path.is_absolute():
            path = root / path
        raw = path.read_bytes()
        if digest(raw) != item["sha256"]:
            raise ValueError(f"Profile bytes differ from manifest: {path}")
        text = raw.decode()
        if text.lstrip().startswith("{"):
            data = json.loads(text)
            if data.get("firmware_sha256") not in (None, manifest["firmware_sha256"]):
                raise ValueError(f"Profile firmware differs: {path}")
            schema = data.get("schema")
            if schema not in ("m8-native-pc/1", "m8-guest-pc/1"):
                raise ValueError(f"Unsupported profile schema: {path}")
            rows = data["pcs" if schema == "m8-native-pc/1" else "jit"]
            for row in rows:
                weights[int(row["pc"])] += int(row["instructions"])
        else:
            for line in text.splitlines():
                if line.startswith("# firmware_sha256=") and line.split("=", 1)[1].strip() != manifest["firmware_sha256"]:
                    raise ValueError(f"PC file firmware differs: {path}")
                fields = line.partition("#")[0].split()
                if fields:
                    if len(fields) > 2:
                        raise ValueError(f"Malformed PC row: {path}: {line}")
                    weights[int(fields[0], 0)] += int(fields[1], 0) if len(fields) == 2 else 0
    if any(value < 0 for value in weights.values()):
        raise ValueError("Negative profile instruction count")
    return weights


def write_cold_pcs(hot_path, full_path, output, root=ROOT):
    hot, hot_pcs = load_manifest(hot_path)
    full, full_pcs = load_manifest(full_path)
    compatible(hot, full)
    if hot_pcs - full_pcs:
        raise ValueError("Hot compiled PCs are not a subset of full compiled PCs")
    weights = profile_weights(full, Path(root))
    cold = full_pcs - hot_pcs
    lines = ["# schema=m8-native-session-cold-pcs/1",
             f"# firmware_sha256={full['firmware_sha256']}",
             f"# hot_manifest_sha256={digest(Path(hot_path).read_bytes())}",
             f"# full_manifest_sha256={digest(Path(full_path).read_bytes())}",
             "# Full compiled PCs minus hot compiled PCs; IT completion may add overlap.",
             "# Counts are preserved discovery weights, not a playback rate."]
    lines += [f"0x{pc:08x} {weights[pc]}" for pc in sorted(cold)]
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n")
    return {"hot_compiled_pcs": len(hot_pcs), "full_compiled_pcs": len(full_pcs),
            "cold_selected_pcs": len(cold), "cold_discovery_weight": sum(weights[pc] for pc in cold),
            "zero_weight_pcs": sum(weights[pc] == 0 for pc in cold),
            "output_sha256": digest(output.read_bytes())}


def _code_only(source):
    """Mask comments and string literals for declaration matching."""
    return TOKEN.sub(lambda match: match.group() if re.fullmatch(r'[A-Za-z_]\w*', match.group())
                     else re.sub(r'[^\n]', ' ', match.group()), source)


def namespace_source(source, kind, chain_header=None, event_header=None):
    """Change identifiers/attributes only; reject any changed instruction body."""
    if kind not in ("hot", "cold"):
        raise ValueError("Namespace must be hot or cold")
    original = source
    if CHAIN_INCLUDE in source:
        if chain_header is None or source.count(CHAIN_INCLUDE) != 1:
            raise ValueError("A single qualified chain helper is required")
        source = source.replace(CHAIN_INCLUDE, chain_header)
    if EVENT_INCLUDE in source:
        if event_header is None or source.count(EVENT_INCLUDE) != 1:
            raise ValueError("A single qualified event helper is required")
        source = source.replace(EVENT_INCLUDE, event_header)
    expanded = source
    names = {match.group() for match in TOKEN.finditer(source)
             if re.fullmatch(r'[A-Za-z_]\w*', match.group())
             and match.group() not in ABI_TYPES
             and (match.group().startswith(("region_", "m8_native_region_", "m8_native_chain_")))}
    replacements = {name: f"m8_session_{kind}_{name}" for name in names}
    result = TOKEN.sub(lambda match: replacements.get(match.group(), match.group()), source)
    masked = _code_only(result)
    definitions = list(FUNCTION.finditer(masked))
    if not definitions:
        raise ValueError("No generated function definitions found")
    attribute = f'M8_NATIVE_TEXT({kind}) '
    for match in reversed(definitions):
        result = result[:match.start()] + attribute + result[match.start():]
    # This strict inverse check includes all whitespace, literals and comments.
    restored = result.replace(attribute, "")
    inverse = {value: key for key, value in replacements.items()}
    restored = TOKEN.sub(lambda match: inverse.get(match.group(), match.group()), restored)
    if restored != expanded:
        raise ValueError("Source preservation check failed")
    return result, {"original_sha256": digest(original.encode()),
                    "expanded_sha256": digest(expanded.encode()),
                    "transformed_sha256": digest(result.encode()),
                    "inverse_equal": True, "prefixed_symbols": sorted(replacements),
                    "section_function_count": len(definitions)}


WRAPPER = '''
/* Public ABI. Results 0/1 preserve completed effects and never replay them. */
M8_NATIVE_TEXT(entry)
int m8_native_region_step(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch) {
    int result=m8_session_hot_m8_native_region_step(cpu,mem,arch);
    if(result!=2) return result;
    return m8_session_cold_m8_native_region_step(cpu,mem,arch);
}
M8_NATIVE_TEXT(entry)
int m8_native_region_verify_memory(m8_mem *mem,uint32_t *bad_pc) {
    int result=m8_session_hot_m8_native_region_verify_memory(mem,bad_pc);
    if(result) return result;
    return m8_session_cold_m8_native_region_verify_memory(mem,bad_pc);
}
M8_NATIVE_TEXT(entry)
size_t m8_native_region_chain_cache_slots(void) {
    return m8_session_hot_m8_native_region_chain_cache_slots();
}
M8_NATIVE_TEXT(entry)
int m8_native_region_chain(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch,m8_native_chain *chain) {
    int result=m8_session_hot_m8_native_region_chain(cpu,mem,arch,chain);
    if(result!=2) return result;
    /* Preserve the hot chain's invalid-context decline before cold work. */
    if(!chain || !chain->generation || !chain->lookup || !chain->slots ||
       !chain->service_exit || !chain->mask_cache || !chain->max_hops) return 2;
    /* The unhandled hot entry can retain stale instruction counts. */
    arch->last_executed=arch->last_special=0;
    result=m8_session_cold_m8_native_region_step(cpu,mem,arch);
    if(result!=2) {
        chain->region_calls++;
        chain->region_guest+=arch->last_executed;
    }
    chain->exit_reason=result==1?M8_NATIVE_EXIT_FAULT:M8_NATIVE_EXIT_REQUIRED;
    /* Return to the runtime after cold work: retain service/IRQ/IT boundaries. */
    return result;
}
'''

RAM_WRAPPER = '''
M8_NATIVE_TEXT(entry)
int m8_native_region_ram_exclusives(void) {
    return m8_session_hot_m8_native_region_ram_exclusives() &&
           m8_session_cold_m8_native_region_ram_exclusives();
}
'''

EVENT_WRAPPER = '''
typedef struct m8_session_event_context {
    struct m8_session_event_context *previous;
    m8_native_chain *chain;
    int (*step)(m8_cpu *,m8_mem *,m8_native_arch *);
    uint64_t cold_steps,cold_guest;
} m8_session_event_context;
static _Thread_local m8_session_event_context *m8_session_current_event;
/* The hot owner invokes this only after its no-effect decline. Its original
 * prefix completion and event callbacks remain unchanged. */
static int m8_session_event_step(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch) {
    m8_session_event_context *context=m8_session_current_event;
    arch->last_executed=arch->last_special=0;
    int result=m8_session_cold_m8_native_region_step(cpu,mem,arch);
    if(result==2) return context->step(cpu,mem,arch);
    context->chain->region_calls++;
    context->chain->region_guest+=arch->last_executed;
    if(result==0) {context->cold_steps++;context->cold_guest+=arch->last_executed;}
    return result;
}
M8_NATIVE_TEXT(entry)
int m8_native_region_event_loop(void) {
    return m8_session_hot_m8_native_region_event_loop();
}
M8_NATIVE_TEXT(entry)
int m8_native_region_event_chain(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch,
                                 m8_native_chain *chain,m8_native_events *events) {
    if(!events || !events->step) return 2;
    m8_native_events adapted=*events;
    m8_session_event_context context={.previous=m8_session_current_event,
                                     .chain=chain,.step=events->step};
    adapted.step=m8_session_event_step;
    m8_session_current_event=&context;
    int result=m8_session_hot_m8_native_region_event_chain(cpu,mem,arch,chain,&adapted);
    m8_session_current_event=context.previous;
    /* Cold instructions belong to regions, not the interpreter. The hot
     * owner adds its fallback counters after step returns; undo only these
     * additions, retaining the original counters and exact-step work. */
    events->fallback_steps=adapted.fallback_steps-context.cold_steps;
    events->fallback_guest=adapted.fallback_guest-context.cold_guest;
    events->interpreter_guest=adapted.interpreter_guest;
    events->segments=adapted.segments;
    return result;
}
'''


def features(hot_source, cold_source, hot, cold):
    services = bool(hot.get("inline_services") or cold.get("inline_services"))
    for name,source,manifest in (("Hot",hot_source,hot),("Cold",cold_source,cold)):
        if ("int m8_native_region_inline_services(" in source) != bool(manifest.get("inline_services")):
            raise ValueError(f"{name} inline-service source marker differs from its manifest")
    ram = bool(hot.get("ram_exclusives"))
    if ram != bool(cold.get("ram_exclusives")):
        raise ValueError("Hot/cold RAM-exclusive contracts differ")
    marker = "int m8_native_region_ram_exclusives("
    if (marker in hot_source) != ram or (marker in cold_source) != ram:
        raise ValueError("RAM-exclusive source marker differs from its manifest")
    events = EVENT_INCLUDE in hot_source
    if EVENT_INCLUDE in cold_source:
        raise ValueError("Cold source must not contain an event owner")
    if services and not ram:
        raise ValueError("Inline services require the RAM-exclusive monitor contract")
    return {"ram_exclusives": ram, "event_loop": events, "inline_services": services}


def public_wrapper(selected):
    return WRAPPER + (RAM_WRAPPER if selected["ram_exclusives"] else "") + (
        EVENT_WRAPPER if selected["event_loop"] else "") + (
        'M8_NATIVE_TEXT(entry)\n'
        'int m8_native_region_inline_services(void) {return 1;}\n'
        if selected.get("inline_services") else "")


def compose(hot_source, hot_manifest, cold_source, cold_manifest, output_dir, chain_header,
            event_header=ROOT / "aot/native_region_event_chain.h"):
    hot, hot_pcs = load_manifest(hot_manifest)
    cold, cold_pcs = load_manifest(cold_manifest)
    compatible(hot, cold)
    if not hot.get("chain_ready"):
        raise ValueError("Hot source must retain its qualified chain-ready build")
    if cold.get("chain_ready"):
        raise ValueError("Generate cold source without --chain-ready")
    if hot.get("exit_profile") or cold.get("exit_profile"):
        raise ValueError("Use sources without exit profiling; counters change timing")
    chain_bytes = Path(chain_header).read_bytes()
    hot_raw = Path(hot_source).read_text()
    cold_raw = Path(cold_source).read_text()
    selected = features(hot_raw, cold_raw, hot, cold)
    if CHAIN_INCLUDE not in hot_raw or CHAIN_INCLUDE in cold_raw:
        raise ValueError("Hot must include the qualified chain helper; cold must not")
    event_bytes = Path(event_header).read_bytes() if selected["event_loop"] else b""
    hot_body, hot_transform = namespace_source(hot_raw, "hot", chain_bytes.decode(),
                                              event_bytes.decode() if event_bytes else None)
    cold_body, cold_transform = namespace_source(cold_raw, "cold")
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    # Canonical prototypes are included first and retain export visibility.
    aggregate = ('/* Default-off composition; train and qualify this aggregate afresh. */\n'
                 '#include "native_regions.h"\n#include "native_events.h"\n' + hot_body + '\n' +
                 cold_body + '\n' + public_wrapper(selected))
    path = output_dir / "generated.c"
    path.write_text(aggregate)
    (output_dir / "native-sections.ld").write_text(LINKER_SCRIPT)
    report = {"schema": "m8-native-session-compose/1", "default_off": True,
              "firmware_sha256": hot["firmware_sha256"], "budget": hot["budget"],
              "hot": hot_transform, "cold": cold_transform,
              "chain_header": {"path": str(chain_header), "sha256": digest(chain_bytes)},
              "hot_manifest_sha256": digest(Path(hot_manifest).read_bytes()),
              "cold_manifest_sha256": digest(Path(cold_manifest).read_bytes()),
              "hot_compiled_instructions": len(hot_pcs), "cold_compiled_instructions": len(cold_pcs),
              "compiled_union_instructions": len(hot_pcs | cold_pcs),
              "it_completion_overlap": sorted(hot_pcs & cold_pcs),
              "hot_cache_and_edge_ids": "unchanged", "cold_chaining": False,
              "features": selected,
              "event_header_sha256": digest(event_bytes) if event_bytes else None,
              "aggregate_sha256": digest(path.read_bytes()),
              "linked_object_identity": False,
              "limits": ["Source bodies/order are preserved; one translation unit can change compiler optimization.",
                         "Use identical qualified FP/service/memory flags and fresh PGO training.",
                         "Complete PCM and state checks remain caller-owned."]}
    (output_dir / "composition.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    cold = sub.add_parser("cold-pcs", help="write weighted full-minus-hot compiled PCs")
    cold.add_argument("--hot-manifest", type=Path, required=True)
    cold.add_argument("--full-manifest", type=Path, required=True)
    cold.add_argument("--output", type=Path, required=True)
    cold.add_argument("--root", type=Path, default=ROOT, help="base for manifest profile paths")
    joined = sub.add_parser("compose", help="embed preserved hot code and a cold-only fallback")
    for kind in ("hot", "cold"):
        joined.add_argument(f"--{kind}-source", type=Path, required=True)
        joined.add_argument(f"--{kind}-manifest", type=Path, required=True)
    joined.add_argument("--output-dir", type=Path, required=True)
    joined.add_argument("--chain-header", type=Path, default=ROOT / "aot/native_region_chain.h")
    args = parser.parse_args()
    try:
        if args.command == "cold-pcs":
            result = write_cold_pcs(args.hot_manifest, args.full_manifest, args.output, args.root)
        else:
            result = compose(args.hot_source, args.hot_manifest, args.cold_source, args.cold_manifest,
                             args.output_dir, args.chain_header)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError, KeyError) as error:
        parser.exit(2, f"{error}\n")


if __name__ == "__main__":
    main()
