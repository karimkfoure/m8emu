#include "internal.h"
#include "thumb-interpreter.h"
#include "thumb-services.h"

/* The reference's SCS MMIO register file does not write QEMU's private FP
 * state. Cortex-M7 resets that state to ASPEN=1, FPDSCR=0, LSPACT=0. Its
 * fp_exception_el() is disabled, so visible CPACR does not cause NOCP. */
static void prepare_fp(m8rt *rt) {
    rt->native.cpu.fpscr &= ~UINT32_C(0x07c00000);
    rt->native.arch.control |= 4u;
}

static void retire(m8rt *rt, const m8_thumb_result *d) {
    m8_cpu *c=&rt->native.cpu;
    m8_native_arch *a=&rt->native.arch;
    c->r[15]=d->next_pc;
    if(a->itstate) {
        a->itstate=(a->itstate&7u)?((a->itstate&0xe0u)|((a->itstate<<1)&31u)):0;
        c->xpsr=(c->xpsr&~UINT32_C(0x0600fc00))|
            ((a->itstate&3u)<<25)|((a->itstate&0xfcu)<<8);
    }
    a->last_executed=1;
    a->last_special=1;
}

static int interrupt(m8rt *rt, unsigned intno, const m8_thumb_result *d) {
    uint32_t pc=rt->native.cpu.r[15];
    if(intno==8 || ((intno==3 || intno==5 || intno==7) && pc>=0xf0000000u)) {
        if(!m8rt_native_unstack(rt))
            m8rt_fail(rt,"exception return failed pc=%08x interrupt=%u",pc,intno);
        m8rt_engine_stop(rt);
    } else {
        if(rt->audit)
            m8rt_audit_access(rt,M8RT_AUDIT_FAULT,pc,0,M8RT_AUDIT_NONE,intno,0);
        if(intno==17) {
            if(rt->options.strict)m8rt_fail(rt,"NOCP/FPU pc=%08x",pc);
            else rt->native.cpu.r[15]=(pc+d->width)&~1u;
        }
    }
    return rt->fatal?UC_ERR_EXCEPTION:UC_ERR_OK;
}

int m8rt_native_interpreter_service(m8rt *rt, int result) {
    const m8_thumb_result *d=m8_thumb_last_result();
    if(!rt || !rt->native.active)return UC_ERR_ARG;
    if(result!=d->status) {
        m8rt_fail(rt,"portable service predicate=result pc=%08x opcode=%08x width=%u result=%d decoded=%d reason=%u",
            d->pc,d->opcode,d->width,result,d->status,d->reason);
        return UC_ERR_EXCEPTION;
    }
    if(rt->fatal)return UC_ERR_EXCEPTION;
    switch(d->reason) {
    case M8_THUMB_REASON_NONE:
        return result?UC_ERR_INSN_INVALID:UC_ERR_OK;
    case M8_THUMB_REASON_WAIT:
        m8rt_engine_stop(rt);return UC_ERR_OK;
    case M8_THUMB_REASON_SVC:
        retire(rt,d);return interrupt(rt,2,d);
    case M8_THUMB_REASON_BREAKPOINT:
        return interrupt(rt,7,d);
    case M8_THUMB_REASON_EVENT:
        retire(rt,d);
        return d->opcode==0xbf40u || d->opcode==0xf3af8004u?UC_ERR_OK:UC_ERR_INSN_INVALID;
    case M8_THUMB_REASON_YIELD:
        retire(rt,d);return UC_ERR_INSN_INVALID;
    case M8_THUMB_REASON_FP_CONTEXT:
        prepare_fp(rt);return UC_ERR_OK;
    case M8_THUMB_REASON_NOCP:
        return interrupt(rt,17,d);
    case M8_THUMB_REASON_STATE:
        rt->native.cpsr=(rt->native.cpsr&~32u)|((rt->native.cpu.xpsr>>19)&32u);
        return UC_ERR_INSN_INVALID;
    case M8_THUMB_REASON_UNDEFINED:
        return UC_ERR_INSN_INVALID;
    case M8_THUMB_REASON_FETCH:
        return UC_ERR_FETCH_UNMAPPED;
    case M8_THUMB_REASON_MEMORY:
        return d->write?UC_ERR_WRITE_UNMAPPED:UC_ERR_READ_UNMAPPED;
    case M8_THUMB_REASON_UNSUPPORTED:
        return 256; /* The portable backend's explicit unsupported code. */
    default:
        m8rt_fail(rt,"portable service predicate=reason pc=%08x opcode=%08x width=%u result=%d reason=%u",
            d->pc,d->opcode,d->width,result,d->reason);
        return UC_ERR_EXCEPTION;
    }
}
