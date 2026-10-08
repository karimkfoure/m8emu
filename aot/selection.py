#!/usr/bin/env python3
"""Inspect candidate CFGs and count exact profile instruction coverage.

The CFG report describes code that can be reached statically. It does not
establish runtime acceleration: calls, unsupported instructions, and slow memory
can make a translation exit before it reaches a hot loop. Use --manifest to
count the exact emitted instruction addresses after generation.
"""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import sys

import capstone
from capstone.arm import ARM_OP_IMM, ARM_OP_MEM, ARM_OP_REG, ARM_REG_PC

ITCM_OFFSET = 0x1809E4
ITCM_SIZE = 0x3D5D8
DEFAULT_FIRMWARE = Path(__file__).resolve().parent.parent / "build/firmware/M8_V6_5_2C_HEADLESS.bin"
HERE = Path(__file__).resolve().parent


class Firmware:
    def __init__(self, path: Path):
        self.binary = path.read_bytes()
        self.decoder = capstone.Cs(
            capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB | capstone.CS_MODE_MCLASS
        )
        self.decoder.detail = True

    def code(self, address: int, size: int) -> bytes:
        if 0 <= address < ITCM_SIZE and address + size <= ITCM_SIZE:
            offset = ITCM_OFFSET + address
        elif 0x60000000 <= address and address + size <= 0x60000000 + len(self.binary):
            offset = address - 0x60000000
        else:
            raise ValueError(f"address is outside the firmware: {address:#x}+{size}")
        return self.binary[offset : offset + size]

    def instruction(self, address: int):
        return next(self.decoder.disasm(self.code(address, 4), address, count=1), None)

    def cfg(self, entry: int, bound: int) -> dict:
        todo = [entry]
        instructions = {}
        calls = set()
        boundaries = set()
        tables = []
        while todo:
            address = todo.pop()
            if address in instructions:
                continue
            if not entry <= address < bound:
                boundaries.add((address, "branch outside function"))
                continue
            insn = self.instruction(address)
            if insn is None:
                boundaries.add((address, "decode failure"))
                continue
            instructions[address] = insn
            next_pc = address + insn.size
            mnemonic = insn.mnemonic.split(".")[0]
            operands = insn.operands
            if mnemonic in ("bl", "blx"):
                if operands[0].type == ARM_OP_IMM:
                    calls.add(operands[0].imm & ~1)
                else:
                    boundaries.add((address, "indirect call"))
                # This is the static continuation after a returning call.
                todo.append(next_pc)
                continue
            if mnemonic in ("tbb", "tbh"):
                memory = operands[0].mem
                limit = None
                # GCC emits CMP index,#N followed by a range branch and TBB/TBH.
                # Accept a bound only if the same index is compared nearby.
                for pc in sorted(instructions, reverse=True):
                    previous = instructions[pc]
                    if not 0 < address - pc < 32:
                        continue
                    op = previous.operands
                    if (previous.mnemonic.split(".")[0] == "cmp" and len(op) == 2
                            and op[0].type == ARM_OP_REG and op[0].reg == memory.index
                            and op[1].type == ARM_OP_IMM):
                        limit = op[1].imm
                        break
                if memory.base != ARM_REG_PC or limit is None or not 0 <= limit < 256:
                    boundaries.add((address, "unresolved table branch"))
                    continue
                size = 1 if mnemonic == "tbb" else 2
                table_base = address + 4
                targets = [table_base + 2 * int.from_bytes(
                    self.code(table_base + n * size, size), "little"
                ) for n in range(limit + 1)]
                tables.append({"pc": address, "limit": limit, "targets": targets})
                todo.extend(targets)
                continue
            if mnemonic == "bx":
                if insn.op_str != "lr":
                    boundaries.add((address, "indirect branch"))
                continue
            # The common return forms write PC from a saved stack value.
            if mnemonic in ("pop", "ldm", "ldmia", "ldmdb") and any(
                op.type == ARM_OP_REG and op.reg == ARM_REG_PC for op in operands
            ):
                continue
            if mnemonic in ("ldr", "mov") and operands and operands[0].type == ARM_OP_REG and operands[0].reg == ARM_REG_PC:
                if (mnemonic == "ldr" and len(operands) == 2
                        and operands[1].type == ARM_OP_MEM
                        and operands[1].mem.base == ARM_REG_PC
                        and not operands[1].mem.index):
                    # Report immutable linker veneers without treating flash as
                    # part of the caller's function.
                    literal = ((address + 4) & ~3) + operands[1].mem.disp
                    target = int.from_bytes(self.code(literal, 4), "little") & ~1
                    calls.add(target)
                    boundaries.add((address, f"literal PC tail call to {target:#x}"))
                elif not insn.op_str.startswith("pc, [sp]"):
                    boundaries.add((address, "indirect PC write"))
                continue
            if mnemonic in ("cbz", "cbnz"):
                todo.extend((operands[-1].imm, next_pc))
                continue
            # B and its condition variants; exclude other B-prefixed operations.
            if (mnemonic.startswith("b") and mnemonic not in
                    ("bic", "bics", "bfi", "bfc", "bkpt") and operands
                    and operands[-1].type == ARM_OP_IMM):
                todo.append(operands[-1].imm)
                if mnemonic != "b":
                    todo.append(next_pc)
                continue
            todo.append(next_pc)
        return {"entry": entry, "bound": bound, "instructions": instructions,
                "calls": sorted(calls), "boundaries": sorted(boundaries),
                "tables": tables}


def functions(path: Path):
    for line in path.read_text().splitlines():
        fields = line.split("#", 1)[0].split()
        if fields:
            yield int(fields[0], 0), int(fields[1], 0)


def profile(firmware: Firmware, path: Path) -> Counter:
    weights = Counter()
    for line in path.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        address, size, count = line.split()
        address, size, count = int(address, 16), int(size), int(count)
        try:
            raw = firmware.code(address, size)
        except ValueError as error:
            raise ValueError(f"cannot count profile block {address:#x}") from error
        insns = list(firmware.decoder.disasm(raw, address))
        if not insns or sum(insn.size for insn in insns) != size:
            raise ValueError(f"profile block does not fully decode: {address:#x}+{size}")
        for insn in insns:
            weights[insn.address] += count
    return weights


def addresses_from_manifest(path: Path) -> set[int]:
    """Read emitted addresses from common JSON manifest or plain address lists.

    JSON uses an `addresses`, `instruction_addresses`, or `instructions` member
    per function. Instruction entries can be integers, hex strings, or objects
    with an `address` or `pc` member. Metadata and function byte bounds do not
    count as instruction coverage.
    """
    def number(value):
        return int(value, 0) if isinstance(value, str) else int(value)

    result = set()
    if path.suffix != ".json":
        for line in path.read_text().splitlines():
            fields = line.split("#", 1)[0].split()
            if fields:
                result.add(number(fields[0]))
        return result
    obj = json.loads(path.read_text())

    def walk(value):
        if isinstance(value, dict):
            for key, child in value.items():
                if key in ("addresses", "instruction_addresses", "instructions"):
                    if isinstance(child, dict):
                        result.update(number(pc) for pc in child)
                    elif isinstance(child, list):
                        for item in child:
                            if isinstance(item, dict):
                                item = item.get("address", item.get("pc"))
                            if item is not None:
                                result.add(number(item))
                elif isinstance(child, (dict, list)):
                    walk(child)
        elif isinstance(value, list):
            for child in value:
                walk(child)
    walk(obj)
    if not result:
        raise ValueError("manifest contains no explicit instruction addresses")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", type=Path, default=DEFAULT_FIRMWARE)
    parser.add_argument("--functions", type=Path, default=HERE / "functions.txt")
    parser.add_argument("--profiles", type=Path, nargs="+", default=[
        HERE / "profiles/blocks-simple.txt", HERE / "profiles/blocks-rich.txt"
    ])
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--roots", type=lambda value: {int(pc, 0) for pc in value.split(",")},
                        help="report selected root functions and their native direct-call closure")
    parser.add_argument("--cfg-json", type=Path, help="write instruction addresses and CFG details")
    args = parser.parse_args()
    firmware = Firmware(args.firmware)
    profiles = [(path.name, profile(firmware, path)) for path in args.profiles]
    totals = [sum(weights.values()) for _, weights in profiles]
    for (name, _), total in zip(profiles, totals):
        print(f"{name}: {total:,} profiled guest instructions")
    cfgs = [firmware.cfg(entry, bound) for entry, bound in functions(args.functions)]
    if args.roots is not None:
        by_entry = {cfg["entry"]: cfg for cfg in cfgs}
        missing = args.roots - by_entry.keys()
        if missing:
            parser.error("unknown root functions: " + ", ".join(hex(pc) for pc in sorted(missing)))
        closure = set(args.roots)
        todo = list(args.roots)
        while todo:
            cfg = by_entry[todo.pop()]
            # Native tail branches use the same entry lookup as direct calls.
            targets = set(cfg["calls"]) | {pc for pc, reason in cfg["boundaries"]
                                              if reason == "branch outside function"}
            for target in targets & by_entry.keys() - closure:
                closure.add(target)
                todo.append(target)
        print("Root functions: " + ", ".join(hex(pc) for pc in sorted(args.roots)))
        print(f"Direct-call closure: {len(closure)} functions. Indirect targets require runtime dispatch.")
        cfgs = [cfg for cfg in cfgs if cfg["entry"] in closure]
    emitted = addresses_from_manifest(args.manifest) if args.manifest else None
    covered = set()
    print("entry       bound       insns   " + "  ".join(name for name, _ in profiles))
    for cfg in cfgs:
        addresses = set(cfg["instructions"])
        if emitted is not None:
            addresses &= emitted
        covered.update(addresses)
        shares = [sum(weights[pc] for pc in addresses) / total
                  for (_, weights), total in zip(profiles, totals)]
        print(f'{cfg["entry"]:#010x}  {cfg["bound"]:#010x}  {len(addresses):5d}   '
              + "  ".join(f"{share:6.2%}" for share in shares))
        for table in cfg["tables"]:
            print(f'  table {table["pc"]:#x}, index <= {table["limit"]}: '
                  + ", ".join(hex(pc) for pc in table["targets"]))
        if cfg["boundaries"]:
            print("  boundaries: " + "; ".join(f"{pc:#x} {why}" for pc, why in cfg["boundaries"]))
        if cfg["calls"]:
            print("  calls: " + ", ".join(hex(pc) for pc in cfg["calls"]))
    print("Union coverage: " + "  ".join(
        f"{sum(weights[pc] for pc in covered) / total:.2%}"
        for (_, weights), total in zip(profiles, totals)
    ))
    if emitted is None:
        print("Static CFG coverage assumes returning calls and complete instruction support.")
    else:
        print("Emitted-address coverage measures code coverage, not acceleration before exits.")
    if args.cfg_json:
        output = []
        for cfg in cfgs:
            result = dict(cfg)
            result["instructions"] = sorted(cfg["instructions"])
            output.append(result)
        args.cfg_json.write_text(json.dumps({"functions": output}, indent=2) + "\n")


if __name__ == "__main__":
    main()
