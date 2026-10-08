"""FPv5-D16 C emitter. Decode VFP operations from encoding, not mnemonics.

Capstone 5.0.7 interchanges the negative fused single-precision names. The
encoding below is therefore the source of truth for every FP data operation.
"""
from __future__ import annotations


def word(insn):
    b = bytes(insn.bytes)
    return (int.from_bytes(b[:2], 'little') << 16) | int.from_bytes(b[2:], 'little') if len(b) == 4 else 0


def regs(w, dp):
    if dp:
        return ((w >> 12) & 15) | ((w >> 18) & 16), ((w >> 16) & 15) | ((w >> 3) & 16), (w & 15) | ((w >> 1) & 16)
    return (((w >> 12) & 15) * 2 + ((w >> 22) & 1),
            ((w >> 16) & 15) * 2 + ((w >> 7) & 1),
            (w & 15) * 2 + ((w >> 5) & 1))


def get(r, dp):
    return f'm8_fp_get64(cpu, {r})' if dp else f'cpu->s[{r}]'


def put(r, dp, value):
    return f'm8_fp_set64(cpu, {r}, {value});' if dp else f'cpu->s[{r}] = {value};'


def expand_imm(i, dp):
    if dp:
        return (((0x8000 if i & 128 else 0) | (0x3fc0 if i & 64 else 0x4000) | (i & 63)) << 48)
    return (((0x8000 if i & 128 else 0) | (0x3e00 if i & 64 else 0x4000) | ((i & 63) << 3)) << 16)


def emit(insn, ctx):
    w = word(insn)
    cp = (w >> 8) & 15
    if cp not in (10, 11) or (w >> 24) not in (0xec, 0xed, 0xee, 0xfe):
        return None
    dp = cp == 11
    width = 64 if dp else 32
    d, n, m = regs(w, dp)
    out = []
    # VMRS/VMSR FPSCR. Other VFP system registers stay in Unicorn.
    if (w & 0xffe00fff) == 0xeee00a10:
        if ((w >> 16) & 15) != 1:
            return None
        rt = (w >> 12) & 15
        if w & (1 << 20):
            if rt == 15:
                # APSR reads only the comparison flags, which are already in
                # FPSCR. Native exception flags can wait for a full FPSCR read
                # or the native exit; the exact path keeps its current sync.
                out += ['#ifndef M8_AOT_NATIVE_FP', 'm8_fp_sync(cpu);', '#endif']
                out.append('cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u);')
            else:
                out += ['m8_fp_sync(cpu);', f'cpu->r[{rt}] = cpu->fpscr;']
        else:
            if rt == 15:
                return None
            out += [f'cpu->fpscr = cpu->r[{rt}] & 0xf7c0009fu;', 'm8_fp_clear();', 'm8_fp_configure(cpu);']
        return out
    # VMOV single <-> ARM core.
    if (w & 0xffe00f7f) == 0xee000a10:
        s = ((w >> 16) & 15) * 2 + ((w >> 7) & 1)
        rt = (w >> 12) & 15
        if rt == 15:
            return None
        return [f'cpu->r[{rt}] = cpu->s[{s}];'] if w & (1 << 20) else [f'cpu->s[{s}] = cpu->r[{rt}];']
    # VMOV a 32-bit double-register lane <-> an ARM core register.
    if (w & 0xffc00f7f) == 0xee000b10:
        dn=((w >> 16) & 15) | ((w >> 3) & 16)
        rt=(w >> 12) & 15
        if dn>=16 or rt==15:
            return None
        idx=dn*2+((w >> 21) & 1)
        return [f'cpu->r[{rt}] = cpu->s[{idx}];'] if w & (1 << 20) else [f'cpu->s[{idx}] = cpu->r[{rt}];']
    # VMOV two ARM registers <-> a double or two consecutive singles.
    if (w & 0xffe00fd0) in (0xec400a10, 0xec400b10):
        rt, rt2 = (w >> 12) & 15, (w >> 16) & 15
        idx = m * 2 if dp else m
        if idx + 1 >= 32 or rt == 15 or rt2 == 15 or (w & (1 << 20) and rt == rt2):
            return None
        if w & (1 << 20):
            return [f'cpu->r[{rt}] = cpu->s[{idx}];', f'cpu->r[{rt2}] = cpu->s[{idx+1}];']
        return [f'cpu->s[{idx}] = cpu->r[{rt}];', f'cpu->s[{idx+1}] = cpu->r[{rt2}];']
    # VLDR/VSTR and VLDM/VSTM (including VPUSH/VPOP).
    if (w >> 24) in (0xec, 0xed):
        p, u, wb, load = ((w >> bit) & 1 for bit in (24, 23, 21, 20))
        rn, imm = (w >> 16) & 15, w & 255
        first = d * 2 if dp else d
        if p and not wb:
            count = 2 if dp else 1
            # PC-relative VFP loads use Align(PC+4,4).
            base = f'0x{((insn.address + 4) & ~3):x}u' if rn == 15 else f'cpu->r[{rn}]'
            addr = f'({base} {"+" if u else "-"} {imm*4}u)'
        elif (not p and u) or (p and not u and wb):
            if rn == 15 or not imm or (dp and (imm & 1)):
                return None
            count = imm
            addr = f'cpu->r[{rn}]' if u else f'(cpu->r[{rn}] - {imm*4}u)'
        else:
            return None
        if first + count > 32:
            return None
        out = ['{', f'uint32_t fp_addr = {addr};', 'uint32_t fp_value;']
        for i in range(count):
            if load:
                out += [f'if (m8_read(mem, fp_addr + {i*4}u, 4, &fp_value, &slow)) goto fault;', f'cpu->s[{first+i}] = fp_value;']
            else:
                out += [f'if (m8_write(mem, fp_addr + {i*4}u, 4, cpu->s[{first+i}], &slow)) goto fault;']
        if wb:
            out += [f'cpu->r[{rn}] {"+=" if u else "-="} {imm*4}u;']
        return out + ['}']
    # All data processing below requires bit4 == 0.
    if w & 16:
        return None
    a, b, acc = get(n, dp), get(m, dp), get(d, dp)
    def op(operation, x=a, y=b, z='0'):
        helper='m8_fp_eval32' if width==32 else 'm8_fp_op64'
        return f'{helper}(cpu, M8_FP_{operation}, {x}, {y}, {z})'
    sign = 'UINT64_C(0x8000000000000000)' if dp else '0x80000000u'
    # FPv5 unconditional additions.
    if (w >> 24) == 0xfe:
        major = (w >> 20) & 11
        if not (w & (1 << 23)) and not (w & 64):
            if dp and max(d,n,m) >= 16:
                return None
            conditions = ['(cpu->xpsr & 0x40000000u)', '(cpu->xpsr & 0x10000000u)',
                          '(((cpu->xpsr >> 31) ^ (cpu->xpsr >> 28)) & 1u) == 0',
                          '!(cpu->xpsr & 0x40000000u) && ((((cpu->xpsr >> 31) ^ (cpu->xpsr >> 28)) & 1u) == 0)']
            return [put(d, dp, f'({conditions[(w >> 20) & 3]}) ? {a} : {b}')]
        if major == 8:
            if dp and max(d,n,m)>=16:
                return None
            return [put(d, dp, op('MINNM' if w & 64 else 'MAXNM'))]
        if major == 11 and w & 64:
            sub = (w >> 16) & 15
            modes = [4, 0, 1, 2]
            mode = modes[sub & 3]
            if 8 <= sub <= 11 and not (w & 128):
                if dp and max(d,m)>=16:
                    return None
                return [put(d,dp,f'm8_fp_rint(cpu, {b}, {int(dp)}, {mode}, 0)')]
            if 12 <= sub <= 15:
                if dp and m>=16:
                    return None
                ds, _, _ = regs(w,False)
                return [put(ds, False, f'm8_fp_to_int(cpu, {b}, {int(dp)}, {int(not bool(w & 128))}, {mode}, 32, 0)')]
        return None
    major = (w >> 20) & 11
    bit6 = bool(w & 64)
    if major in (0, 1, 2, 3, 8, 9, 10):
        if dp and max(d,n,m) >= 16:
            return None
        if major in (0, 1):
            prod = op('MUL')
            # VMLS is destination + negated rounded product. Negation occurs
            # after multiplication; do not merge these into an FMA.
            if (major == 0 and bit6) or (major == 1 and bit6):
                prod = f'({prod} ^ {sign})'
            dest = f'({acc} ^ {sign})' if major == 1 else acc
            return [put(d, dp, op('ADD', dest, prod))]
        if major == 2:
            result = op('MUL')
            return [put(d,dp,f'({result} ^ {sign})' if bit6 else result)]
        if major == 3:
            return [put(d,dp,op('SUB' if bit6 else 'ADD'))]
        if major == 8:
            return None if bit6 else [put(d,dp,op('DIV'))]
        # Encoding 9: product - destination (bit6=0), -product-dest (bit6=1).
        # Encoding 10: product + destination (bit6=0), -product+dest (bit6=1).
        x = f'({a} ^ {sign})' if bit6 else a
        z = f'({acc} ^ {sign})' if major == 9 else acc
        return [put(d,dp,op('FMA', x,b,z))]
    if major != 11:
        return None
    sub = (w >> 16) & 15
    if not (w & 0xe0):
        if dp and d>=16:
            return None
        imm = ((w >> 12) & 0xf0) | (w & 15)
        return [put(d,dp,f'UINT64_C(0x{expand_imm(imm,dp):x})' if dp else f'0x{expand_imm(imm,dp):x}u')]
    if not bit6:
        return None
    if dp:
        d_is_fp = sub not in (12,13) and not (sub == 7 and w & 128)
        m_is_fp = sub in (0,1,4,6,7,12,13)
        if (d_is_fp and d>=16) or (m_is_fp and m>=16):
            return None
    if sub == 0:
        return [put(d,dp,f'({b} & ~{sign})' if w & 128 else b)]
    if sub == 1:
        return [put(d,dp,op('SQRT', b,'0') if w & 128 else f'({b} ^ {sign})')]
    if sub in (4,5):
        return [f'm8_fp_cmp{width}(cpu, {acc}, {"0" if sub == 5 else b}, {int(bool(w & 128))});']
    if sub == 6:
        mode = '3' if w & 128 else '((cpu->fpscr >> 22) & 3u)'
        return [put(d,dp,f'm8_fp_rint(cpu, {b}, {int(dp)}, {mode}, 0)')]
    if sub == 7:
        if w & 128:
            dest,_,_=regs(w, not dp)
            if not dp and dest >= 16:
                return None
            return [put(dest,not dp,f'm8_fp_convert(cpu, {b}, {int(dp)})')]
        return [put(d,dp,f'm8_fp_rint(cpu, {b}, {int(dp)}, ((cpu->fpscr >> 22) & 3u), 1)')]
    if sub == 8:
        _,_,ms=regs(w,False)
        return [put(d,dp,f'm8_fp_from_int(cpu, cpu->s[{ms}], {int(dp)}, {int(not bool(w & 128))}, 32, 0, 0)')]
    if sub in (12,13):
        ds,_,_=regs(w,False)
        mode = '3' if w & 128 else '((cpu->fpscr >> 22) & 3u)'
        return [put(ds,False,f'm8_fp_to_int(cpu, {b}, {int(dp)}, {int(sub == 12)}, {mode}, 32, 0)')]
    if sub in (10,11,14,15):
        bits = 32 if w & 128 else 16
        imm = (w & 15) * 2 + ((w >> 5) & 1)
        frac = bits - imm
        if frac < 0:
            return None
        if sub & 4:
            result=f'm8_fp_to_int(cpu, {acc}, {int(dp)}, {sub & 1}, 3, {bits}, {frac})'
            if dp and not (sub & 1):
                result=f'(uint64_t)(int64_t)(int32_t){result}'
        else:
            result=f'm8_fp_from_int(cpu, (uint32_t){acc}, {int(dp)}, {sub & 1}, {bits}, {frac}, 1)'
        prefix=[]
        if not (sub & 4):
            # Native from-int uses the exact nearest helper in a directed
            # guest mode. That helper can clear host exceptions while IXC is
            # not sticky, so collect pending flags before this rare fallback.
            prefix=['#ifdef M8_AOT_NATIVE_FP',
                    'if (cpu->fpscr & (3u << 22)) m8_fp_sync(cpu);', '#endif']
        return prefix+[put(d,dp,result)]
    return None
