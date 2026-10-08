#!/usr/bin/env python3
"""Compare native functions with Unicorn from actual captured entry states.

Capture files are trusted local pickle files from run_demo.py. Each test uses
independent region buffers and compares every ABI register and RAM byte.
"""
from __future__ import annotations

import argparse
import ctypes as C
import gzip
import json
import pickle
from pathlib import Path
import statistics
import sys
import time

import unicorn
from unicorn import Uc, UcError
from unicorn.arm_const import UC_CPU_ARM_CORTEX_M7, UC_ARM_REG_PC

from adapter import (Cpu, DIRECT_BASES, EXTRA_REG_IDS, Memory, NativeLibrary,
                     READ, WRITE, read_cpu, write_cpu)

WORD_NAMES = ([f"r{i}" for i in range(13)] + ["sp", "lr", "pc", "xpsr"]
              + [f"s{i}" for i in range(32)] + ["fpscr"])
RAM_BASES = set(DIRECT_BASES[:3])


class FixedArena:
    """Use the runtime's guarded Linux 64-bit backing for native comparisons."""
    REGIONS = (("itcm", 0, 0x80000), ("dtcm", 0x20000000, 0x80000),
               ("ocram", 0x20200000, 0x80000), ("flash", 0x60000000, 0x800000))

    def __init__(self):
        if not sys.platform.startswith("linux") or C.sizeof(C.c_void_p) != 8:
            raise RuntimeError("Fixed backing requires Linux with 64-bit pointers")
        self.libc = C.CDLL(None, use_errno=True)
        self.libc.mmap.argtypes = [C.c_void_p, C.c_size_t, C.c_int, C.c_int,
                                  C.c_int, C.c_size_t]
        self.libc.mmap.restype = C.c_void_p
        self.libc.mprotect.argtypes = [C.c_void_p, C.c_size_t, C.c_int]
        self.libc.munmap.argtypes = [C.c_void_p, C.c_size_t]
        self.length = 1 << 32
        self.address = self.libc.mmap(None, self.length, 0, 0x22, -1, 0)
        if self.address in (None, C.c_void_p(-1).value):
            raise OSError(C.get_errno(), "Cannot reserve the guarded fixed arena")
        try:
            for name, base, size in self.REGIONS:
                if self.libc.mprotect(self.address + base, size, 3):
                    raise OSError(C.get_errno(), f"Cannot enable fixed {name} backing")
        except Exception:
            self.close()
            raise

    def install(self, replay):
        for name, base, size in self.REGIONS:
            region = getattr(replay.mem, name)
            if region.base != base or region.size != size:
                raise ValueError(f"Capture has a noncanonical {name} region")
        for name, base, size in self.REGIONS:
            C.memmove(self.address + base, replay.native[base], size)
            replay.native[base] = (C.c_uint8 * size).from_address(self.address + base)
            getattr(replay.mem, name).host = self.address + base

    def close(self):
        if self.address is not None:
            address, self.address = self.address, None
            if self.libc.munmap(address, self.length):
                raise OSError(C.get_errno(), "Cannot release fixed backing")


class Replay:
    def __init__(self, sample, images):
        self.sample = sample
        self.images = images
        self.uc = Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB)
        self.uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M7)
        self.reference = {}
        self.native = {}
        self.mem = Memory()
        for name, base in zip(("itcm", "dtcm", "ocram", "flash"), DIRECT_BASES):
            region = next(region for region in sample["regions"] if region[0] == base)
            _, perms, image = region
            data = images[image]
            reference = (C.c_uint8 * len(data)).from_buffer_copy(data)
            native = (C.c_uint8 * len(data)).from_buffer_copy(data)
            self.uc.mem_map_ptr(base, len(data), perms, C.addressof(reference))
            self.reference[base] = reference
            self.native[base] = native
            setattr(self.mem, name, __import__("adapter").Region(C.addressof(native), base, len(data)))
        self.slow_access = None

        def read(_ctx, address, size, value):
            del value
            self.slow_access = ("read", address, size)
            return -1

        def write(_ctx, address, size, value):
            self.slow_access = ("write", address, size, value)
            return -1

        self.read_callback, self.write_callback = READ(read), WRITE(write)
        self.mem.read, self.mem.write = self.read_callback, self.write_callback
        self.reset()

    def reset(self):
        for base, _, image in self.sample["regions"]:
            data = self.images[image]
            C.memmove(self.reference[base], data, len(data))
            C.memmove(self.native[base], data, len(data))
        self.cpu = Cpu.from_buffer_copy(self.sample["cpu"])
        self.uc.reg_write_batch(list(zip(EXTRA_REG_IDS, self.sample["extras"])))
        write_cpu(self.uc, self.cpu)
        self.slow_access = None

    def reference_run(self, exit_pc, max_instructions, timeout_us, trace=None):
        entry = self.sample["entry"]
        if exit_pc == entry:
            return 0, read_cpu(self.uc), None
        hook = None
        if trace is not None:
            def visit(uc, address, size, opaque):
                del uc, size, opaque
                trace.append(address)
            hook = self.uc.hook_add(unicorn.UC_HOOK_CODE, visit)
        started = time.perf_counter_ns()
        error = None
        try:
            self.uc.emu_start(entry | 1, exit_pc, timeout=timeout_us, count=max_instructions)
        except UcError as exc:
            error = str(exc)
        elapsed = time.perf_counter_ns() - started
        if hook is not None:
            self.uc.hook_del(hook)
        cpu = read_cpu(self.uc)
        if cpu.r[15] != exit_pc and error is None:
            error = f"Reference did not reach exit {exit_pc:#x}; stopped at {cpu.r[15]:#x} (instruction/time bound)"
        return elapsed, cpu, error

    def differences(self, native, reference):
        changes = [(name, actual, expected)
                   for name, actual, expected in zip(WORD_NAMES, native.words(), reference.words())
                   if actual != expected]
        for base in sorted(self.native):
            # Flash is compared too: a translation must not damage code/data.
            actual, expected = bytes(self.native[base]), bytes(self.reference[base])
            if actual != expected:
                offset = next(i for i, (a, b) in enumerate(zip(actual, expected)) if a != b)
                changes.append((f"memory[{base + offset:#010x}]", actual[offset], expected[offset]))
        extras = tuple(self.uc.reg_read_batch(EXTRA_REG_IDS))
        if extras != self.sample["extras"]:
            changes.append(("unsupported_control_state_changed", extras, self.sample["extras"]))
        return changes


def print_differences(changes):
    for name, actual, expected in changes[:12]:
        if isinstance(actual, int):
            print(f"  {name}: native={actual:#010x} unicorn={expected:#010x}")
        else:
            print(f"  {name}: native={actual} unicorn={expected}")


def diagnose(library, replay, exit_pc, args):
    """Compare consecutive safe prefixes, including repeated loop visits."""
    setter = getattr(library.lib, "m8_aot_set_stop_after", None)
    steps = getattr(library.lib, "m8_aot_get_steps", None)
    if setter is None or steps is None or not args.manifest.exists():
        print("  First differing instruction is not isolated: use a diagnostic build and its manifest.")
        return
    setter.argtypes, setter.restype = [C.c_uint64], None
    steps.argtypes, steps.restype = [], C.c_uint64
    manifest = json.loads(args.manifest.read_text())
    units = {unit["pc"]: unit for fn in manifest["functions"] for unit in fn["units"]}
    replay.reset()
    setter(0xffffffffffffffff)
    library.run(replay.sample["entry"], replay.cpu, replay.mem)
    total = steps()
    if not total:
        print("  First differing instruction is not isolated: this library has no diagnostic counters.")
        return
    print(f"  native_safe_units={total}; checking at most {args.diagnose_limit} consecutive prefixes")
    previous = None
    try:
        for limit in range(1, min(total, args.diagnose_limit) + 1):
            replay.reset()
            setter(limit)
            result = library.run(replay.sample["entry"], replay.cpu, replay.mem)
            native_pc = replay.cpu.r[15]
            seen = [0]
            last = [None]
            stopped = [False]

            def visit(uc, address, size, opaque):
                del size, opaque
                if address not in units:
                    return
                if seen[0] == limit:
                    stopped[0] = True
                    uc.emu_stop()
                    return
                seen[0] += 1
                last[0] = address

            hook = replay.uc.hook_add(unicorn.UC_HOOK_CODE, visit)
            try:
                replay.uc.emu_start(replay.sample["entry"] | 1, exit_pc,
                                    timeout=args.timeout_us, count=args.max_instructions)
                reference = read_cpu(replay.uc)
            except UcError as exc:
                print(f"  Prefix reference failed at unit {limit}: {exc}")
                return
            finally:
                replay.uc.hook_del(hook)
            if seen[0] != limit:
                print(f"  Prefix reference reached only {seen[0]}/{limit} safe units; last={last[0]}")
                return
            changes = replay.differences(replay.cpu, reference)
            if result != 0 or native_pc != reference.r[15] or changes:
                unit = units.get(last[0], {})
                members = unit.get("members", [last[0]])
                if unit.get("it"):
                    print("  First differing architectural unit is an IT block at " +
                          f"{last[0]:#x}; members=" + ",".join(f"{pc:#x}" for pc in members))
                    print("  A native exit inside this IT block is forbidden; the block is the smallest legal comparison boundary.")
                else:
                    print(f"  First differing instruction: {last[0]:#x} (safe unit {limit}).")
                print_differences(changes)
                return
            previous = last[0]
        print(f"  No difference in the first {min(total, args.diagnose_limit)} safe units; last={previous:#x}.")
        if total > args.diagnose_limit:
            print("  Increase --diagnose-limit to locate the remaining failure.")
    finally:
        setter(0xffffffffffffffff)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("samples", type=Path)
    parser.add_argument("--library", type=Path, default=Path(__file__).resolve().parent / "libm8adapter.so")
    parser.add_argument("--entries", type=lambda s: set(int(x, 0) for x in s.split(",")))
    parser.add_argument("--fixed-memory", action="store_true",
                        help="Use guarded Linux 64-bit backing for a fixed-offset native library")
    parser.add_argument("--max-instructions", type=int, default=5000000)
    parser.add_argument("--timeout-us", type=int, default=3000000)
    parser.add_argument("--slow-callbacks", action="store_true",
                        help="Exercise native callbacks; default exits before memory outside the four captured regions")
    parser.add_argument("--repeat", type=int, default=5, help="Warm repetitions per sample for paired function timings")
    parser.add_argument("--diagnose", action="store_true")
    parser.add_argument("--diagnose-limit", type=int, default=300)
    parser.add_argument("--manifest", type=Path, default=Path(__file__).resolve().parent / "manifest.json",
                        help="Manifest from the same diagnostic native build")
    parser.add_argument("--keep-going", action="store_true")
    args = parser.parse_args()
    if args.repeat < 1:
        parser.error("--repeat must be positive")
    with gzip.open(args.samples, "rb") as stream:
        bundle = pickle.load(stream)
    if bundle.get("format") != 1:
        parser.error("Unknown capture format")
    library = NativeLibrary(args.library)
    slow_fallback = getattr(library.lib, "m8_aot_set_slow_fallback", None)
    if slow_fallback is not None:
        slow_fallback.argtypes, slow_fallback.restype = [C.c_int], None
        slow_fallback(0 if args.slow_callbacks else 1)
    reset_steps = getattr(library.lib, "m8_aot_set_stop_after", None)
    if reset_steps is not None:
        reset_steps.argtypes, reset_steps.restype = [C.c_uint64], None
        reset_steps(0xffffffffffffffff)
    supported = set(library.entries())
    samples = [sample for sample in bundle["samples"]
               if sample["entry"] in (args.entries if args.entries is not None else supported)]
    if not samples:
        parser.error("No samples match the selected translated entries")
    arena = FixedArena() if args.fixed_memory else None
    try:
        passed, failed, timings = 0, 0, {}
        sample_cpu = Cpu.from_buffer_copy(samples[0]["cpu"])
        noop_mem = Memory()
        started = time.perf_counter_ns()
        for _ in range(10000):
            library.lib.m8_adapter_noop(samples[0]["entry"], C.byref(sample_cpu), C.byref(noop_mem))
        print(f"CTYPES empty_call_us={(time.perf_counter_ns()-started)/10000/1000:.3f}; native timings include this marshaling cost")
        for sample_index, sample in enumerate(samples):
            entry = sample["entry"]
            replay = Replay(sample, bundle["images"])
            if arena is not None:
                arena.install(replay)
            library.verify(replay.mem)
            started = time.perf_counter_ns()
            result = library.run(entry, replay.cpu, replay.mem)
            native_ns = time.perf_counter_ns() - started
            exit_pc = replay.cpu.r[15]
            ref_ns, reference, error = replay.reference_run(exit_pc, args.max_instructions, args.timeout_us)
            changes = replay.differences(replay.cpu, reference)
            if result != 0 or error or changes:
                failed += 1
                print(f"FAIL sample={sample_index} entry={entry:#x} exit={exit_pc:#x} result={result} slow_access={replay.slow_access}")
                if error:
                    print("  " + error)
                print_differences(changes)
                if args.diagnose and not error:
                    diagnose(library, replay, exit_pc, args)
                if not args.keep_going:
                    break
                continue
            if exit_pc == entry:
                failed += 1
                print(f"FAIL sample={sample_index} entry={entry:#x}: no guest instruction executed")
                if not args.keep_going:
                    break
                continue
            passed += 1
            # Reset inputs outside both timings. Run each native/reference pair
            # sequentially on the same host; the first run warms code and TCG.
            pairs = []
            for _ in range(args.repeat):
                replay.reset()
                started = time.perf_counter_ns()
                result = library.run(entry, replay.cpu, replay.mem)
                native_ns = time.perf_counter_ns() - started
                ref_ns, reference, error = replay.reference_run(exit_pc, 0, 0)
                if result != 0 or error or replay.differences(replay.cpu, reference):
                    raise RuntimeError(f"Non-repeatable validation at {entry:#x}")
                pairs.append((native_ns, ref_ns))
            native_ns = statistics.median(pair[0] for pair in pairs)
            ref_ns = statistics.median(pair[1] for pair in pairs)
            timings.setdefault(entry, []).append((native_ns, ref_ns))
            print(f"PASS sample={sample_index} entry={entry:#x} exit={exit_pc:#x} native_us={native_ns/1000:.3f} "
                  f"unicorn_us={ref_ns/1000:.3f} speedup={ref_ns/native_ns:.2f}x", flush=True)
        for entry, times in sorted(timings.items()):
            native = sum(pair[0] for pair in times)
            reference = sum(pair[1] for pair in times)
            print(f"FUNCTION entry={entry:#x} samples={len(times)} native_us={native/len(times)/1000:.3f} "
                  f"unicorn_us={reference/len(times)/1000:.3f} speedup={reference/native:.2f}x")
        print(f"RESULT passed={passed} failed={failed} selected={len(samples)}")
        return 1 if failed else 0
    finally:
        if arena is not None:
            arena.close()


if __name__ == "__main__":
    raise SystemExit(main())
