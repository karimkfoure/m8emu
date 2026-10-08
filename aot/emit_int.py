"""Emit portable C11 for Thumb integer instructions.

Unknown or privileged instructions return None. The caller owns condition
execution, IT state, instruction boundaries, and final slow-path exits.
"""
from capstone import arm as A


def _name(insn, ctx):
    return getattr(ctx, "name", None) or insn._cs.insn_name(insn.id).removesuffix(".w").removesuffix(".n")


def _op(ctx, op, carry=None):
    if op.type != A.ARM_OP_REG:
        return ctx.op(op)
    value = ctx.r(op.reg)
    if op.shift.type:
        amount = ctx.r(op.shift.value) if op.shift.type >= A.ARM_SFT_ASR_REG else str(op.shift.value)
        return f"m8_shift({value}, {op.shift.type}, {amount}, {carry or 'NULL'})"
    return value


def _logical_operand(insn, ctx, op):
    """Return operand expression and shift carry adjustment, including imm12."""
    if op.type == A.ARM_OP_REG:
        return _op(ctx, op, "&carry"), []
    changes = []
    if insn.size == 4:
        h1 = int.from_bytes(insn.bytes[:2], "little")
        h2 = int.from_bytes(insn.bytes[2:], "little")
        # Thumb modified immediate: encoding 11110 i 0 opcode S Rn / 0 imm3 ...
        if (h1 & 0xfa00) == 0xf000 and not (h2 & 0x8000):
            imm12 = ((h1 >> 10) & 1) << 11 | ((h2 >> 12) & 7) << 8 | (h2 & 255)
            if imm12 >> 10:
                changes = [f"carry = ({ctx.op(op)}) >> 31;"]
    return ctx.op(op), changes


def _set(ctx, reg, value):
    return ctx.set(reg, value)


def _memory(insn, ctx, name):
    ops = insn.operands
    load = name.startswith("ldr")
    pair = name in ("ldrd", "strd")
    mi = 2 if pair else 1
    if len(ops) <= mi or ops[mi].type != A.ARM_OP_MEM:
        return None
    operand = ops[mi]
    m = operand.mem
    if not m.base:
        return None
    if load and insn.writeback and any(ops[j].reg == m.base for j in range(mi)):
        return None  # Base/destination overlap is architecturally unpredictable.
    base = ctx.r(m.base)
    if ctx.regno(m.base) == 15:
        base = f"({base} & ~UINT32_C(3))"
    offset = str(m.disp & 0xffffffff) + "u"
    if m.index:
        index = ctx.r(m.index)
        if operand.shift.type:
            index = f"m8_shift({index}, {operand.shift.type}, {operand.shift.value}, NULL)"
        # Capstone reports a negative register offset with subtracted=True.
        sign = "-" if operand.subtracted or m.scale < 0 else "+"
        offset = f"({offset} {sign} {index})"
    post = getattr(insn, "post_index", False)
    address = "base" if post else f"base + ({offset})"
    if post:
        if len(ops) <= mi + 1:
            return None
        p = ops[mi+1]
        po = _op(ctx,p)
        update = f"base {'-' if p.subtracted and (p.type != A.ARM_OP_IMM or p.imm >= 0) else '+'} ({po})"
    else:
        update = "addr"
    size = 1 if name in ("ldrb","ldrsb","strb") else 2 if name in ("ldrh","ldrsh","strh") else 4
    lines = ["{",f"uint32_t base = {base};",f"uint32_t addr = {address};"]
    if load:
        lines += ["uint32_t value;",f"if (m8_read(mem, addr, {size}, &value, &slow)) goto fault;"]
        if pair:
            lines += ["uint32_t value2;", "if (m8_read(mem, addr + 4u, 4, &value2, &slow)) goto fault;"]
        if insn.writeback:
            lines += _set(ctx,m.base,update)
        if name == "ldrsb": value = "(uint32_t)m8_signed(value & 255u, 8)"
        elif name == "ldrsh": value = "(uint32_t)m8_signed(value & 65535u, 16)"
        else: value = "value"
        lines += _set(ctx,ops[0].reg,value)
        if pair:
            lines += _set(ctx,ops[1].reg,"value2")
    else:
        lines += [f"uint32_t value = {ctx.r(ops[0].reg)};"]
        if pair:
            lines += [f"uint32_t value2 = {ctx.r(ops[1].reg)};"]
        lines += [f"if (m8_write(mem, addr, {size}, value, &slow)) goto fault;"]
        if pair:
            lines += ["if (m8_write(mem, addr + 4u, 4, value2, &slow)) goto fault;"]
        if insn.writeback:
            lines += _set(ctx,m.base,update)
    return lines + ["}"]


def _block(insn,ctx,name):
    ops=insn.operands
    stack = name in ("push","pop")
    load = name == "pop" or name.startswith("ldm")
    if stack:
        base="cpu->r[13]"; regs=[o.reg for o in ops]; wb=True; mode="db" if name=="push" else "ia"; base_reg=None
    else:
        if not ops or ops[0].type != A.ARM_OP_REG: return None
        base_reg=ops[0].reg; base=ctx.r(base_reg); regs=[o.reg for o in ops[1:]]; wb=insn.writeback
        mode=name[3:] or "ia"
        if load and wb and base_reg in regs: return None  # Architecturally unpredictable.
    if mode not in ("ia","ib","da","db") or not regs: return None
    regs=sorted(regs,key=ctx.regno); n=len(regs)
    offset={"ia":0,"ib":4,"da":-4*(n-1),"db":-4*n}[mode]
    lines=["{",f"uint32_t base = {base};",f"uint32_t addr = base + {offset & 0xffffffff}u;"]
    if not load:
        for j,r in enumerate(regs): lines += [f"uint32_t value{j} = {ctx.r(r)};"]
    for j,r in enumerate(regs):
        if load: lines += [f"uint32_t value{j};",f"if (m8_read(mem, addr + {4*j}u, 4, &value{j}, &slow)) goto fault;"]
        else: lines += [f"if (m8_write(mem, addr + {4*j}u, 4, value{j}, &slow)) goto fault;"]
    if wb:
        value=f"base {'+' if mode[0]=='i' else '-'} {4*n}u"
        lines += [f"cpu->r[13] = {value};"] if stack else _set(ctx,base_reg,value)
    if load:
        for j,r in enumerate(regs): lines += _set(ctx,r,f"value{j}")
    return lines+["}"]


def emit(insn, ctx):
    """Return C statements or None when exact execution is not supported."""
    name=_name(insn,ctx); ops=insn.operands
    name = {"movs":"mov", "muls":"mul"}.get(name, name)
    if name in ("nop", "hint"): return [] if name=="nop" else None
    if name in ("ldr","ldrb","ldrh","ldrsb","ldrsh","ldrd","str","strb","strh","strd"):
        return _memory(insn,ctx,name)
    if name in ("push","pop","ldm","ldmia","ldmib","ldmda","ldmdb","stm","stmia","stmib","stmda","stmdb"):
        return _block(insn,ctx,name)
    if not ops or ops[0].type != A.ARM_OP_REG: return None
    dst=ops[0].reg
    flags=insn.update_flags
    # Capstone 5 reports ADC/SBC carry reads as update_flags=True. Decode S.
    if insn.size == 4 and name in ("mov","mvn","and","bic","orr","orn","eor","add","adc","sub","sbc","rsb","rsc","lsl","lsr","asr","ror","rrx"):
        flags = bool(int.from_bytes(insn.bytes[:2], "little") & 16)
    if getattr(ctx, "suppress_nzcv", False):
        flags = False
        if name in ("cmp", "cmn", "tst", "teq"):
            return []
    lines=["{"]
    if name in ("mov","mvn","and","bic","orr","orn","eor","tst","teq"):
        unary=name in ("mov","mvn")
        test=name in ("tst","teq")
        if unary: src=ops[1]; a=None
        elif test: a=_op(ctx,ops[0]); src=ops[1]
        else: a=ctx.r(dst) if len(ops)==2 else _op(ctx,ops[1]); src=ops[-1]
        b,carry_adjust=_logical_operand(insn,ctx,src)
        lines += ["uint32_t carry = (cpu->xpsr >> 29) & 1u;"]+carry_adjust
        lines += ([f"uint32_t a = {a};"] if a is not None else [])+[f"uint32_t b = {b};"]
        expr={"mov":"b","mvn":"~b","and":"a & b","bic":"a & ~b","orr":"a | b","orn":"a | ~b","eor":"a ^ b","tst":"a & b","teq":"a ^ b"}[name]
        lines += [f"uint32_t result = {expr};"]
        if flags or test: lines += ["m8_logic_flags(cpu, result, carry);"]
        if not test: lines += _set(ctx,dst,"result")
    elif name in ("add","addw","adc","sub","subw","sbc","rsb","rsc","cmp","cmn"):
        test=name in ("cmp","cmn")
        a=_op(ctx,ops[0]) if test or len(ops)==2 else _op(ctx,ops[1])
        b=_op(ctx,ops[-1])
        # ADR.W is decoded as ADDW/SUBW with Rn=PC. It aligns PC to 4.
        if insn.size == 4 and len(ops) == 3 and ops[1].type == A.ARM_OP_REG and ctx.regno(ops[1].reg) == 15:
            h1 = int.from_bytes(insn.bytes[:2], "little")
            if (h1 & 0xfbf0) in (0xf200, 0xf2a0):
                a = f"({a} & ~UINT32_C(3))"
        lines += [f"uint32_t a = {a};",f"uint32_t b = {b};"]
        c="((cpu->xpsr >> 29) & 1u)" if name=="adc" else "(1u - ((cpu->xpsr >> 29) & 1u))" if name in ("sbc","rsc") else "0u"
        if name in ("rsb","rsc"): aa,bb="b","a"
        else: aa,bb="a","b"
        add=name in ("add","addw","adc","cmn")
        if flags or test: expr=f"m8_{'add' if add else 'sub'}_flags(cpu, {aa}, {bb}, {c})"
        else: expr=f"{aa} {'+' if add else '-'} {bb} {'+' if add else '-'} {c}"
        lines += [f"uint32_t result = {expr};"]
        if not test: lines += _set(ctx,dst,"result")
    elif name in ("lsl","lsr","asr","ror","rrx"):
        typ={"asr":1,"lsl":2,"lsr":3,"ror":4,"rrx":5}[name]
        if name=="rrx": a=_op(ctx,ops[1]); count="1u"
        elif len(ops)==2: a=ctx.r(dst); count=_op(ctx,ops[1]); typ += 5 if ops[1].type==A.ARM_OP_REG else 0
        else: a=_op(ctx,ops[1]); count=_op(ctx,ops[2]); typ += 5 if ops[2].type==A.ARM_OP_REG else 0
        lines += ["uint32_t carry = (cpu->xpsr >> 29) & 1u;",f"uint32_t result = m8_shift({a}, {typ}, {count}, &carry);"]
        if flags: lines += ["m8_logic_flags(cpu, result, carry);"]
        lines += _set(ctx,dst,"result")
    elif name=="movw": lines += _set(ctx,dst,_op(ctx,ops[1]))
    elif name=="movt": lines += _set(ctx,dst,f"({ctx.r(dst)} & 65535u) | (({_op(ctx,ops[1])}) << 16)")
    elif name=="adr": lines += _set(ctx,dst,f"({ctx.r(A.ARM_REG_PC)} & ~UINT32_C(3)) + ({_op(ctx,ops[1])})")
    elif name in ("mul","mla","mls"):
        if name=="mul" and len(ops)==2: a=ctx.r(dst);b=_op(ctx,ops[1])
        else: a=_op(ctx,ops[1]);b=_op(ctx,ops[2])
        lines += [f"uint32_t result = ({a}) * ({b});"]
        if name=="mla": lines += [f"result += {_op(ctx,ops[3])};"]
        if name=="mls": lines += [f"result = ({_op(ctx,ops[3])}) - result;"]
        if flags: lines += ["m8_nz(cpu, result);"]
        lines += _set(ctx,dst,"result")
    elif name in ("sdiv","udiv"):
        a=_op(ctx,ops[1]);b=_op(ctx,ops[2]); signed=name=="sdiv"
        lines += [f"{'int64_t' if signed else 'uint32_t'} a = {'m8_s32('+a+')' if signed else a};",f"{'int64_t' if signed else 'uint32_t'} b = {'m8_s32('+b+')' if signed else b};"]
        lines += _set(ctx,dst,"(uint32_t)(b ? a / b : 0)")
    elif name in ("umull","smull","umlal","smlal","umaal"):
        a=_op(ctx,ops[2]);b=_op(ctx,ops[3]);signed=name.startswith("s")
        product=f"(uint64_t)(m8_s32({a}) * m8_s32({b}))" if signed else f"(uint64_t)({a}) * ({b})"
        lines += [f"uint64_t value = {product};"]
        if name in ("umlal","smlal"): lines += [f"value += (uint64_t){ctx.r(dst)} | ((uint64_t){ctx.r(ops[1].reg)} << 32);"]
        if name=="umaal": lines += [f"value += (uint64_t){ctx.r(dst)} + {ctx.r(ops[1].reg)};"]
        lines += _set(ctx,dst,"(uint32_t)value")+_set(ctx,ops[1].reg,"(uint32_t)(value >> 32)")
    elif name in ("clz","rbit","rev","rev16","revsh"):
        a=_op(ctx,ops[1]);lines += [f"uint32_t value = {a};"]
        if name in ("clz","rbit"): expr=f"m8_{name}(value)"
        elif name=="rev": expr="(value >> 24) | ((value >> 8) & 65280u) | ((value << 8) & 16711680u) | (value << 24)"
        elif name=="rev16":expr="((value >> 8) & 16711935u) | ((value << 8) & 4278255360u)"
        else:expr="(uint32_t)m8_signed(((value >> 8) & 255u) | ((value & 255u) << 8), 16)"
        lines += _set(ctx,dst,expr)
    elif name in ("ubfx","sbfx","bfi","bfc"):
        src=ctx.r(dst) if name=="bfc" else _op(ctx,ops[1]); start=ops[1].imm if name=="bfc" else ops[2].imm; width=ops[2].imm if name=="bfc" else ops[3].imm
        if not 0<=start<32 or not 1<=width<=32-start: return None
        mask=(1<<width)-1
        if name in ("ubfx","sbfx"):
            expr=f"(({src}) >> {start}) & {mask}u"
            if name=="sbfx":expr=f"(uint32_t)m8_signed({expr}, {width})"
        else:
            masked=mask<<start
            expr=f"({ctx.r(dst)} & {~masked & 0xffffffff}u)"
            if name=="bfi":expr+=f" | ((({src}) << {start}) & {masked}u)"
        lines += _set(ctx,dst,expr)
    elif name in ("uxtb","uxth","sxtb","sxth","uxtab","uxtah","sxtab","sxtah","uxtb16","sxtb16","uxtab16","sxtab16"):
        adding=name in ("uxtab","uxtah","sxtab","sxtah","uxtab16","sxtab16")
        src=ops[2] if adding else ops[1]
        # The extension rotation is an operand shift, applied before extension.
        lines += [f"uint32_t value = {_op(ctx,src)};"]
        if name.endswith("16"):
            lo="(uint32_t)m8_signed(value & 255u, 8)" if name.startswith("s") else "value & 255u"
            hi="(uint32_t)m8_signed((value >> 16) & 255u, 8)" if name.startswith("s") else "(value >> 16) & 255u"
            lines += [f"uint32_t lo = {lo};",f"uint32_t hi = {hi};"]
            if adding: lines += [f"uint32_t base = {_op(ctx,ops[1])};","lo += base & 65535u;","hi += base >> 16;"]
            expr="(lo & 65535u) | (hi << 16)"
        else:
            bits=8 if name.endswith("b") else 16
            expr=f"(uint32_t)m8_signed(value & {(1<<bits)-1}u, {bits})" if name.startswith("s") else f"value & {(1<<bits)-1}u"
            if adding:expr=f"({_op(ctx,ops[1])}) + ({expr})"
        lines += _set(ctx,dst,expr)
    elif name in ("ssat","usat","ssat16","usat16"):
        bits=ops[1].imm;src=_op(ctx,ops[2]);signed=name.startswith("s")
        helper="m8_sat_signed" if signed else "m8_sat_unsigned"
        if name.endswith("16"):
            lines += [f"uint32_t value = {src};",f"uint32_t lo = {helper}(cpu, m8_signed(value & 65535u, 16), {bits});",f"uint32_t hi = {helper}(cpu, m8_signed(value >> 16, 16), {bits});"]
            expr="(lo & 65535u) | (hi << 16)"
        else:expr=f"{helper}(cpu, m8_s32({src}), {bits})"
        lines += _set(ctx,dst,expr)
    elif name in ("qadd","qsub","qdadd","qdsub"):
        a=_op(ctx,ops[1]);b=_op(ctx,ops[2]);lines += [f"int64_t a = m8_s32({a});",f"int64_t b = m8_s32({b});"]
        if name.startswith("qd"):lines += ["b = m8_s32(m8_sat_signed(cpu, 2 * b, 32));"]
        lines += _set(ctx,dst,f"m8_sat_signed(cpu, a {'-' if name.endswith('sub') else '+'} b, 32)")
    elif name=="sel": lines += _set(ctx,dst,f"m8_sel(cpu->xpsr, {_op(ctx,ops[1])}, {_op(ctx,ops[2])})")
    elif name in ("pkhbt","pkhtb"):
        a=_op(ctx,ops[1]);b=_op(ctx,ops[2]);expr=f"(({a}) & 65535u) | (({b}) & 4294901760u)" if name=="pkhbt" else f"(({a}) & 4294901760u) | (({b}) & 65535u)"
        lines += _set(ctx,dst,expr)
    elif name in ("usad8","usada8"):
        expr=f"m8_usad8({_op(ctx,ops[1])}, {_op(ctx,ops[2])})"
        if name=="usada8":expr+=f" + ({_op(ctx,ops[3])})"
        lines += _set(ctx,dst,expr)
    else:
        result=_dsp(insn,ctx,name)
        if result is None:return None
        return result
    return lines+["}"]


def _dsp(insn,ctx,name):
    ops=insn.operands;dst=ops[0].reg
    modes={"s":0,"u":1,"q":2,"uq":3,"sh":4,"uh":5}
    for prefix,mode in sorted(modes.items(),key=lambda item:-len(item[0])):
        tail=name[len(prefix):] if name.startswith(prefix) else ""
        if tail in ("add8","sub8","add16","sub16","asx","sax"):
            bits=8 if tail.endswith("8") else 16
            kind=0 if tail.startswith("add") else 1 if tail.startswith("sub") else 2 if tail=="asx" else 3
            return ["{"]+_set(ctx,dst,f"m8_parallel(cpu, {_op(ctx,ops[1])}, {_op(ctx,ops[2])}, {bits}, {kind}, {mode})")+["}"]
    lines=["{"]
    if name.startswith(("smul","smla")) and len(name)>=6 and name[-2:] in ("bb","bt","tb","tt"):
        long=name.startswith("smlal");acc=name.startswith("smla")
        index=2 if long else 1
        a=_op(ctx,ops[index]);b=_op(ctx,ops[index+1]);x=16 if name[-2]=="t" else 0;y=16 if name[-1]=="t" else 0
        lines += [f"int64_t product = m8_signed((({a}) >> {x}) & 65535u, 16) * m8_signed((({b}) >> {y}) & 65535u, 16);"]
        if long:
            lines += [f"uint64_t value = (uint64_t)product + ((uint64_t){ctx.r(dst)} | ((uint64_t){ctx.r(ops[1].reg)} << 32));"]
            lines += _set(ctx,dst,"(uint32_t)value")+_set(ctx,ops[1].reg,"(uint32_t)(value >> 32)")
        else:
            if acc:lines += [f"product += m8_s32({_op(ctx,ops[3])});"]
            lines += _set(ctx,dst,"m8_qresult(cpu, product)" if acc else "(uint32_t)product")
    elif name in ("smulwb","smulwt","smlawb","smlawt"):
        a=_op(ctx,ops[1]);b=_op(ctx,ops[2]);shift=16 if name.endswith("t") else 0
        lines += [f"int64_t product = m8_s32({a}) * m8_signed((({b}) >> {shift}) & 65535u, 16);", "uint32_t high = (uint32_t)((uint64_t)product >> 16);"]
        expr=f"m8_qresult(cpu, m8_s32(high) + m8_s32({_op(ctx,ops[3])}))" if name.startswith("smla") else "high"
        lines += _set(ctx,dst,expr)
    elif name in ("smuad","smuadx","smusd","smusdx","smlad","smladx","smlsd","smlsdx","smlald","smlaldx","smlsld","smlsldx"):
        long=name.startswith(("smlald","smlsld"));index=2 if long else 1
        a=_op(ctx,ops[index]);b=_op(ctx,ops[index+1]);sub=name.startswith(("smusd","smlsd","smlsld"))
        lines += [f"uint32_t a = {a};",f"uint32_t b = {b};"]
        if name.endswith("x"):lines += ["b = m8_ror(b, 16);"]
        lines += [f"int64_t product = m8_signed(a & 65535u, 16) * m8_signed(b & 65535u, 16) {'-' if sub else '+'} m8_signed(a >> 16, 16) * m8_signed(b >> 16, 16);"]
        if long:
            lines += [f"uint64_t value = (uint64_t)product + ((uint64_t){ctx.r(dst)} | ((uint64_t){ctx.r(ops[1].reg)} << 32));"]
            lines += _set(ctx,dst,"(uint32_t)value")+_set(ctx,ops[1].reg,"(uint32_t)(value >> 32)")
        else:
            acc=name.startswith("sml")
            if acc:
                if not sub: lines += ["(void)m8_qresult(cpu, product);"]
                lines += [f"product += m8_s32({_op(ctx,ops[3])});"]
            lines += _set(ctx,dst,"m8_qresult(cpu, product)" if acc or not sub else "(uint32_t)product")
    elif name in ("smmul","smmulr","smmla","smmlar","smmls","smmlsr"):
        a=_op(ctx,ops[1]);b=_op(ctx,ops[2]);rounding=name.endswith("r")
        lines += [f"uint64_t product = (uint64_t)(m8_s32({a}) * m8_s32({b}));"]
        if name.startswith(("smmla","smmls")):
            lines += [f"uint64_t accum = (uint64_t){_op(ctx,ops[3])} << 32;",f"product = accum {'-' if name.startswith('smmls') else '+'} product;"]
        if rounding:lines += ["product += UINT64_C(0x80000000);"]
        lines += _set(ctx,dst,"(uint32_t)(product >> 32)")
    else:return None
    return lines+["}"]
