import sys, collections
import capstone
from pathlib import Path
BIN = (Path(__file__).resolve().parents[2] / 'build/firmware/M8_V6_5_2C_HEADLESS.bin').read_bytes()
ITCM_OFF = 0x1809E4  # ITCM [0,0x3D5D8) == bin[0x1809E4:]
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB | capstone.CS_MODE_MCLASS)

def code(addr, size):
    if addr < 0x3D5D8:
        o = ITCM_OFF + addr
    elif 0x60000000 <= addr < 0x60000000 + len(BIN):
        o = addr - 0x60000000
    else:
        return None
    return BIN[o:o + size]

path = sys.argv[1]
rows = []
for line in open(path):
    if line.startswith('#'):
        print(line.strip()); continue
    a, s, c = line.split()
    rows.append((int(a, 16), int(s), int(c)))
total_insn = 0; fp_insn = 0; mem_insn = 0
per_block = []
cache = {}
for a, s, c in rows:
    b = code(a, s)
    if b is None:
        n = s // 3; fp = 0; mem = 0
    else:
        insns = list(md.disasm(b, a))
        n = len(insns) or s // 3
        fp = sum(1 for i in insns if i.mnemonic.startswith('v'))
        mem = sum(1 for i in insns if i.mnemonic.startswith(('ldr', 'str', 'ldm', 'stm', 'push', 'pop', 'vldr', 'vstr', 'vldm', 'vstm', 'vpush', 'vpop')))
    total_insn += n * c; fp_insn += fp * c; mem_insn += mem * c
    per_block.append((n * c, a, s, c, n))
print(f"instructions={total_insn:,} fp={fp_insn:,} ({fp_insn/total_insn:.1%}) mem={mem_insn:,} ({mem_insn/total_insn:.1%})")
per_block.sort(reverse=True)
# bucket by 1KB region
buckets = collections.Counter()
for w, a, s, c, n in per_block:
    buckets[a & ~0x3ff] += w
print("top 1KB regions:")
acc = 0
for a, w in buckets.most_common(25):
    acc += w
    print(f"  {a:#010x} {w/total_insn:6.2%} cum {acc/total_insn:6.2%}")
print("top blocks:")
for w, a, s, c, n in per_block[:30]:
    print(f"  {a:#010x} size={s:4d} insns={n:4d} entries={c:10,} share={w/total_insn:6.2%}")
