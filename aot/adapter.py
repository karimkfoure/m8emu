"""Pointer-backed guest RAM and the native Unicorn AOT entry adapter.

Only this module patches Unicorn. The maintained m8emu modules stay unchanged.
Keep OwnedMappings and Adapter alive for the lifetime of their Uc engine.
"""
from __future__ import annotations

import ctypes as C
from contextlib import contextmanager
from pathlib import Path
import subprocess
import sys

import unicorn
from unicorn import Uc
from unicorn import arm_const as A

HERE = Path(__file__).resolve().parent
DIRECT_BASES = (0x00000000, 0x20000000, 0x20200000, 0x60000000)
RAM_BASES = DIRECT_BASES[:3]
REG_IDS = ([getattr(A, f"UC_ARM_REG_R{i}") for i in range(13)]
           + [A.UC_ARM_REG_SP, A.UC_ARM_REG_LR, A.UC_ARM_REG_PC,
              A.UC_ARM_REG_XPSR]
           + [getattr(A, f"UC_ARM_REG_S{i}") for i in range(32)]
           + [A.UC_ARM_REG_FPSCR])
EXTRA_REG_IDS = [getattr(A, name) for name in
                 ("UC_ARM_REG_C1_C0_2", "UC_ARM_REG_FPEXC", "UC_ARM_REG_CONTROL",
                  "UC_ARM_REG_PRIMASK", "UC_ARM_REG_BASEPRI", "UC_ARM_REG_FAULTMASK")]


class Cpu(C.Structure):
    _fields_ = [("r", C.c_uint32 * 16), ("xpsr", C.c_uint32),
                ("s", C.c_uint32 * 32), ("fpscr", C.c_uint32)]

    def words(self):
        return tuple(C.cast(C.byref(self), C.POINTER(C.c_uint32 * 50)).contents)

    def copy(self):
        return Cpu.from_buffer_copy(self)


class Region(C.Structure):
    _fields_ = [("host", C.c_void_p), ("base", C.c_uint32), ("size", C.c_uint32)]


READ = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint32, C.c_uint32,
                  C.POINTER(C.c_uint32))
WRITE = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint32)


class Memory(C.Structure):
    _fields_ = [(name, Region) for name in ("itcm", "dtcm", "ocram", "flash")] + [
        ("ctx", C.c_void_p), ("read", READ), ("write", WRITE)]


class EntryStats(C.Structure):
    _fields_ = [("pc", C.c_uint32), ("reserved", C.c_uint32)] + [
        (name, C.c_uint64) for name in
        ("calls", "native_ns", "total_ns", "exits", "faults", "skipped", "no_progress")] + [
        ("fn", C.c_void_p), ("hook", C.c_size_t)]


def read_cpu(uc):
    words = uc.reg_read_batch(REG_IDS)
    return Cpu.from_buffer_copy((C.c_uint32 * 50)(*words))


def write_cpu(uc, cpu):
    words = cpu.words()
    # XPSR and FPSCR must be written before redirecting PC.
    uc.reg_write_batch([(reg, word) for reg, word in zip(REG_IDS, words)
                        if reg != A.UC_ARM_REG_PC])
    uc.reg_write(A.UC_ARM_REG_APSR_NZCVQG, int(cpu.xpsr))
    uc.reg_write(A.UC_ARM_REG_PC, int(cpu.r[15]) | 1)


class OwnedMappings:
    """Map the four ABI regions with uc_mem_map_ptr on Python-owned buffers."""

    @contextmanager
    def patch(self):
        original = Uc.mem_map

        def mem_map(uc, address, size, perms=unicorn.UC_PROT_ALL):
            if address not in DIRECT_BASES:
                return original(uc, address, size, perms)
            buffers = getattr(uc, "_m8_aot_buffers", None)
            if buffers is None:
                buffers = uc._m8_aot_buffers = {}
            if address in buffers:
                raise ValueError(f"Duplicate AOT mapping at {address:#x}")
            buf = (C.c_uint8 * size)()
            uc.mem_map_ptr(address, size, perms, C.addressof(buf))
            buffers[address] = (buf, perms)

        Uc.mem_map = mem_map
        try:
            yield self
        finally:
            Uc.mem_map = original

    @staticmethod
    def memory(uc):
        buffers = getattr(uc, "_m8_aot_buffers", {})
        mem = Memory()
        for name, base in zip(("itcm", "dtcm", "ocram", "flash"), DIRECT_BASES):
            if base not in buffers:
                raise ValueError(f"Guest region {base:#x} was not mapped with OwnedMappings")
            buf, _ = buffers[base]
            setattr(mem, name, Region(C.addressof(buf), base, len(buf)))
        return mem

    @staticmethod
    def snapshot(uc):
        return {base: (bytes(buf), perms)
                for base, (buf, perms) in uc._m8_aot_buffers.items()}


def build_adapter(aot_library=HERE / "libm8aot.so", output=HERE / "libm8adapter.so"):
    """Compile with the headers and library from this Python's Unicorn package."""
    package = Path(unicorn.__file__).resolve().parent
    lib = package / "lib" / "libunicorn.so.2"
    aot_library, output = Path(aot_library).resolve(), Path(output).resolve()
    command = ["cc", "-std=c11", "-O2", "-fPIC", "-shared", "-Wall", "-Wextra",
               "-I", str(package / "include"), str(HERE / "adapter.c"),
               str(aot_library), str(lib), "-Wl,-rpath," + str(package / "lib"),
               "-Wl,-rpath," + str(aot_library.parent), "-o", str(output)]
    subprocess.run(command, check=True)
    return output


class NativeLibrary:
    def __init__(self, path=HERE / "libm8adapter.so"):
        self.lib = C.CDLL(str(Path(path).resolve()))
        self.lib.m8_aot_entries.argtypes = [C.POINTER(C.POINTER(C.c_uint32))]
        self.lib.m8_aot_entries.restype = C.c_uint32
        self.lib.m8_adapter_install.argtypes = [C.c_void_p, C.POINTER(Memory),
                                                C.POINTER(C.c_uint32), C.c_uint32]
        self.lib.m8_adapter_install.restype = C.c_void_p
        self.lib.m8_adapter_uninstall.argtypes = [C.c_void_p]
        self.lib.m8_adapter_uninstall.restype = C.c_int
        self.lib.m8_adapter_set_slow_fallback.argtypes = [C.c_void_p, C.c_void_p]
        self.lib.m8_adapter_set_slow_fallback.restype = None
        self.lib.m8_adapter_error.argtypes = [C.c_void_p]
        self.lib.m8_adapter_error.restype = C.c_int
        self.lib.m8_adapter_stats.argtypes = [C.c_void_p,
                                             C.POINTER(C.POINTER(EntryStats))]
        self.lib.m8_adapter_stats.restype = C.c_uint32
        self.lib.m8_adapter_run.argtypes = [C.c_uint32, C.POINTER(Cpu), C.POINTER(Memory)]
        self.lib.m8_adapter_run.restype = C.c_int
        self.lib.m8_adapter_noop.argtypes = self.lib.m8_adapter_run.argtypes
        self.lib.m8_adapter_noop.restype = C.c_int
        for operation in ("read", "write"):
            fn = getattr(self.lib, f"m8_adapter_{operation}_cpu")
            fn.argtypes = [C.c_void_p, C.POINTER(Cpu)]
            fn.restype = C.c_int

    def entries(self):
        pcs = C.POINTER(C.c_uint32)()
        count = self.lib.m8_aot_entries(C.byref(pcs))
        return list(pcs[:count])

    def verify(self, mem):
        verify = getattr(self.lib, "m8_aot_verify_memory", None)
        if verify is None:
            return
        verify.argtypes = [C.POINTER(Memory), C.POINTER(C.c_uint32)]
        verify.restype = C.c_int
        bad_pc = C.c_uint32()
        if verify(C.byref(mem), C.byref(bad_pc)):
            raise RuntimeError(f"AOT firmware bytes differ at guest PC {bad_pc.value:#010x}")

    def run(self, entry, cpu, mem):
        return self.lib.m8_adapter_run(entry, C.byref(cpu), C.byref(mem))


class Adapter:
    def __init__(self, uc, library=HERE / "libm8adapter.so", entries=None):
        self.uc = uc
        self.native = library if isinstance(library, NativeLibrary) else NativeLibrary(library)
        self.mem = OwnedMappings.memory(uc)
        self.native.verify(self.mem)
        selected = None if entries is None else (C.c_uint32 * len(entries))(*entries)
        self.handle = self.native.lib.m8_adapter_install(
            uc._uch, C.byref(self.mem), selected, 0 if entries is None else len(entries))
        if not self.handle:
            raise RuntimeError("AOT adapter installation failed (missing entries or native hook failure)")
        setter = getattr(self.native.lib, "m8_aot_set_slow_fallback", None)
        if setter is not None:
            self.native.lib.m8_adapter_set_slow_fallback(self.handle, C.cast(setter, C.c_void_p))
        uc.ctl_flush_tb()

    @property
    def error(self):
        return self.native.lib.m8_adapter_error(self.handle) if self.handle else 0

    def stats(self):
        ptr = C.POINTER(EntryStats)()
        count = self.native.lib.m8_adapter_stats(self.handle, C.byref(ptr))
        return [{name: int(getattr(ptr[i], name)) for name in
                 ("pc", "calls", "native_ns", "total_ns", "exits", "faults", "skipped", "no_progress")}
                for i in range(count)]

    def close(self):
        if self.handle:
            error = self.native.lib.m8_adapter_uninstall(self.handle)
            if error:
                raise RuntimeError(f"AOT hook deletion failed: Unicorn error {error}")
            self.handle = None


def install(uc_handle, library=HERE / "libm8adapter.so", entries=None):
    """Install on a Uc object that was constructed within OwnedMappings.patch()."""
    if not isinstance(uc_handle, Uc):
        raise TypeError("install needs the Uc object to retain the pointer-backed mappings")
    return Adapter(uc_handle, library, entries)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Build the native Unicorn entry adapter")
    parser.add_argument("--library", type=Path, default=HERE / "libm8aot.so")
    parser.add_argument("--output", type=Path, default=HERE / "libm8adapter.so")
    args = parser.parse_args()
    print(build_adapter(args.library, args.output))
