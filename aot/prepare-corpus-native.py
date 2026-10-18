#!/usr/bin/env python3
"""Generate the default-off corpus region catalog with existing emitters.

The hot catalog includes the qualified hot and cold PC union. Cold source and
the public wrapper stay separate, so the compiler can retain their old recipe.
This tool performs no compilation, PGO training, or behavioral qualification.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
AOT = ROOT / "aot"


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def pcs(path):
    return {int(line.partition('#')[0].split()[0], 0)
            for line in Path(path).read_text().splitlines() if line.partition('#')[0].split()}


def write_source(path, source):
    # CMake also tracks generator inputs that do not affect every output. Keep
    # the old timestamp when the bytes match, so Ninja can skip recompilation.
    data = source.encode('utf-8')
    if not path.is_file() or path.read_bytes() != data:
        path.write_bytes(data)


def generate(args):
    sys.path.insert(0, str(AOT))
    from native_regions import generate as regions
    from native_region_chain import append_chain as chain
    from native_step import generate as handlers
    from native_region_state import transform as shared
    catalog = load('corpus_catalog', ROOT / 'aot/corpus-coverage-catalog.py')
    redirect = load('corpus_redirect', ROOT / 'aot/native-engine-redirects.py')
    linker = load('corpus_link', ROOT / 'aot/native-session-link.py')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    base = [AOT / 'functions-playback-native-pcs.txt', AOT / 'functions-playback-native-groove-pcs.txt',
            AOT / 'catalogs/session-pcs.txt']
    old_layout = AOT / 'functions-playback-native-layout.json'
    old_targets = AOT / 'catalogs/redirect-targets-services.txt'
    scope_path = getattr(args, 'pgo_scope', None)
    # The retained PGO scope pins the exact default-recipe source bytes. The
    # direct-call recipe changes those bytes, so it runs without the scope;
    # the attributes are inert without PGO instrumentation. Cold keeps them.
    direct = getattr(args, 'direct_calls', False)
    inputs = [args.firmware, args.pc_file, args.handler_pc_file, args.cold_pc_file, old_layout, old_targets,
              *base, Path(__file__), ROOT / 'aot/corpus-coverage-catalog.py',
              ROOT / 'aot/native-engine-redirects.py', ROOT / 'aot/native-session-link.py',
              ROOT / 'aot/native-session-compose.py', *AOT.glob('*.py'), *AOT.glob('*.h')]
    if scope_path:
        inputs.append(scope_path)
    pins = {str(path.resolve()): sha(path) for path in inputs}
    report = {'schema': 'm8-corpus-native-generation/1', 'complete': False, 'default_off': True,
              'inputs': pins, 'first_failure': None, 'phases': [], 'compiler_macros': [
                  'M8_AOT_NATIVE_FP', 'M8_NATIVE_REGION_INLINE_MEMORY=1',
                  'M8_NATIVE_REGION_INTEGER_FP_FASTPATH=1', 'M8_NATIVE_REGION_SERVICE_CONTINUE=1'],
              'hot_only_macros': ['M8_NATIVE_REGION_SHARED_STATE'],
              'qualification': 'Source generation only; exact runtime checks remain required.'}
    if args.region_memory_spans:
        report['compiler_macros'] = report['compiler_macros'] + ['M8_NATIVE_REGION_MEMORY_SPANS=1']
        report['region_memory_spans'] = True
    def save():
        (out / 'generation.json').write_text(json.dumps(report, indent=2) + '\n')
    def phase(name, function):
        start = time.monotonic()
        result = function()
        report['phases'].append({'name': name, 'seconds': time.monotonic() - start})
        save()
        return result
    try:
        files = [*base, args.pc_file]
        if args.layout_input:
            layout = json.loads(args.layout_input.read_text())
        else:
            _, inventory = phase('ungrouped_inventory', lambda: regions(args.firmware, [], pc_files=files,
                chain_ready=True, inline_services=True, ram_exclusives=True,
                direct_calls=direct, local_nzcv=args.local_nzcv, dead_nzcv=args.dead_nzcv))
            layout = catalog.adapt_layout(json.loads(old_layout.read_text()), inventory)
        (out / 'layout.json').write_text(json.dumps(layout, indent=2) + '\n')
        hot, manifest = phase('grouped_hot', lambda: regions(args.firmware, [], pc_files=files,
            chain_ready=True, layout=out / 'layout.json', inline_services=True, ram_exclusives=True,
            direct_calls=direct, local_nzcv=args.local_nzcv, dead_nzcv=args.dead_nzcv,
            region_memory_spans=args.region_memory_spans))
        (out / 'hot.json').write_text(json.dumps(manifest, indent=2) + '\n')
        hot = phase('chain', lambda: chain(hot, manifest, event_loop=True))
        (out / 'hot-chain.c').write_text(hot)
        targets = catalog.adapt_targets(old_targets.read_text(), manifest, redirect.contract_hash(manifest), hot)
        (out / 'targets.txt').write_text(targets)
        hot, redirect_manifest = phase('redirects', lambda: redirect.transform(hot, manifest,
            enabled=True, target_input=(str(out / 'targets.txt'), targets.encode())))
        (out / 'redirects.json').write_text(json.dumps(redirect_manifest, indent=2) + '\n')
        hot, state_manifest = phase('shared_state', lambda: shared(hot))
        (out / 'shared-state.json').write_text(json.dumps(state_manifest, indent=2) + '\n')
        if scope_path:
            if direct:
                report['pgo_scope_skipped'] = 'direct_calls recipe is outside the retained scope pin'
            else:
                from pgo_scope import apply_scope
                hot, scope = phase('pgo_scope', lambda: apply_scope(hot, manifest, scope_path))
                (out / 'pgo-scope.json').write_text(json.dumps(scope, indent=2) + '\n')
                report['pgo_scope'] = scope
        write_source(out / 'hot.c', hot)
        cold, cold_manifest = phase('cold', lambda: regions(args.firmware, [],
            pc_files=[args.cold_pc_file], inline_services=True, ram_exclusives=True,
            local_nzcv=args.local_nzcv, dead_nzcv=args.dead_nzcv,
            region_memory_spans=args.region_memory_spans))
        write_source(out / 'cold.c', cold)
        (out / 'cold.json').write_text(json.dumps(cold_manifest, indent=2) + '\n')
        compiled = {pc for r in manifest['regions'] for pc in r['instructions']}
        baseline = pcs(args.handler_pc_file)
        selected = baseline | (pcs(args.pc_file) - compiled) if args.discover_handlers else baseline
        handler_source, handler_manifest = phase('handlers', lambda: handlers(args.firmware, [],
            sorted(selected), False, blocks=False))
        supported = {r['pc'] for r in handler_manifest['instructions']}
        if baseline - supported:
            raise ValueError('Existing handler input contains unsupported PCs')
        write_source(out / 'handlers.c', handler_source)
        (out / 'handlers.json').write_text(json.dumps(handler_manifest, indent=2) + '\n')
        (out / 'handler-pcs.txt').write_text(''.join(f'0x{pc:08x}\n' for pc in sorted(supported)))
        selected_features = {'ram_exclusives': True, 'inline_services': True, 'event_loop': True}
        write_source(out / 'wrapper.c', linker.wrapper_source(True, selected_features))
        (out / 'native-sections.ld').write_text(linker.LINKER_SCRIPT)
        headers = out / 'headers'
        headers.mkdir(exist_ok=True)
        for path in AOT.glob('*.h'):
            shutil.copy2(path, headers / path.name)
        if any(sha(path) != value for path, value in pins.items()):
            raise ValueError('A source input changed during generation')
        report.update(complete=True, firmware_sha256=manifest['firmware_sha256'],
            hot_compiled_pcs=manifest['compiled_instructions'], hot_regions=len(manifest['regions']),
            cold_compiled_pcs=cold_manifest['compiled_instructions'], handler_pcs=len(supported),
            unsupported_new_handlers=handler_manifest['unsupported'],
            outputs={str(path.resolve()): sha(path) for path in out.iterdir() if path.is_file() and path.name != 'generation.json'},
            headers={str(path.resolve()): sha(path) for path in headers.iterdir() if path.is_file()})
        save()
        return report
    except BaseException as error:
        report['first_failure'] = str(error)
        save()
        raise


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--firmware', type=Path, required=True)
    p.add_argument('--pc-file', type=Path, default=AOT / 'catalogs/corpus/playback.txt')
    p.add_argument('--handler-pc-file', type=Path, default=AOT / 'catalogs/corpus/handlers-complete.txt')
    p.add_argument('--cold-pc-file', type=Path, default=AOT / 'catalogs/corpus/baseline-cold.txt')
    p.add_argument('--discover-handlers', action='store_true', help='Select supported new region declines; discovery only')
    p.add_argument('--layout-input', type=Path, help='Reuse a retained derived layout; the existing generator validates it')
    p.add_argument('--region-memory-spans', action='store_true', help='Hoist proven region memory range checks; off by default')
    p.add_argument('--pgo-scope', type=Path, help='Apply an exact pinned region instrumentation scope; guest bodies stay unchanged')
    p.add_argument('--direct-calls', action='store_true', help='Inline hot flag/FP helper calls in hot regions; cold keeps the shared recipe')
    p.add_argument('--local-nzcv', action='store_true', help='Keep NZCV values in locals and pack only at state boundaries')
    p.add_argument('--dead-nzcv', action='store_true', help='Omit provably dead whole NZCV writes')
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    try:
        result = generate(args)
        print(json.dumps({key: result[key] for key in ('complete', 'hot_compiled_pcs', 'hot_regions', 'handler_pcs')}))
    except (OSError, ValueError, KeyError) as error:
        p.exit(1, f'Corpus source generation: {error}\n')


if __name__ == '__main__':
    main()
