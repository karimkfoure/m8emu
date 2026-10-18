#include "internal.h"
#include <string.h>
#include <stdlib.h>

static const char *const leave_names[M8RT_NATIVE_LEAVE_COUNT]={
    "unsupported", "architecture", "storage", "recording", "code_write", "reset", "disabled"
};
const char *m8rt_native_leave_name(unsigned reason) {
    return reason<M8RT_NATIVE_LEAVE_COUNT?leave_names[reason]:"invalid";
}
const char *m8rt_native_arch_leave_name(unsigned cause) {
    static const char *const names[M8RT_NATIVE_ARCH_COUNT]={
        "ldrex", "strex", "clrex", "msr_msp", "msr_psp", "msr_control", "other"
    };
    return cause<M8RT_NATIVE_ARCH_COUNT?names[cause]:"invalid";
}
const char *m8rt_native_reject_name(unsigned reason) {
    static const char *const names[M8RT_NATIVE_REJECT_COUNT]={
        "irq_nest", "ipsr", "itstate", "thumb", "control", "aot_active",
        "sdhc_pending", "nvic_pending", "systick_pending", "audio_pending",
        "usb_pending", "primask", "basepri", "faultmask", "exclusive_width",
        "source_mismatch"
    };
    return reason<M8RT_NATIVE_REJECT_COUNT?names[reason]:"invalid";
}
#ifdef M8RT_NATIVE_ENGINE
int m8rt_native_live_available(void) {return 1;}

void m8rt_native_live_code_changed(m8rt *rt) {
    if(!rt->native_live.enabled || !rt->native_live.verified) return;
    rt->native_live.verified=false;rt->native_live.code_disabled=true;
    rt->native_live.invalidate_pending=true;
}
static void code_write_hook(uc_engine *uc,uc_mem_type type,uint64_t addr,int size,
                            int64_t value,void *user) {
    (void)uc;(void)type;(void)addr;(void)size;(void)value;
    m8rt_native_live_code_changed(user);
}
bool m8rt_native_live_reference_entry(m8rt *rt) {
    if(!rt->native_live.enabled || !rt->native_live.invalidate_pending) return false;
    /* The original write completes in Unicorn. Stop at the next native/AOT
     * entry, before it can execute stale source, and remove compiled entries. */
    if(rt->in_emu) m8rt_engine_stop(rt);
    return true;
}
static int verified_source(m8_mem *mem,uint32_t *bad) {
    (void)bad;
    m8rt *rt=mem->ctx;
    return rt->native_live.verified && !rt->native_live.code_disabled?0:-1;
}
static int observe_code_writes(m8rt *rt) {
    if(rt->native_live.write_observer) return 0;
    uc_err e=uc_hook_add(rt->uc,&rt->native_live.code_hooks[0],UC_HOOK_MEM_WRITE,
                       code_write_hook,rt,0,rt->memory.itcm.size-1);
    if(e) return -1;
    e=uc_hook_add(rt->uc,&rt->native_live.code_hooks[1],UC_HOOK_MEM_WRITE,
                  code_write_hook,rt,rt->memory.flash.base,
                  rt->memory.flash.base+rt->memory.flash.size-1);
    if(e) {uc_hook_del(rt->uc,rt->native_live.code_hooks[0]);return -1;}
    rt->native_live.write_observer=true;
    return uc_ctl_flush_tb(rt->uc)?-1:0;
}

static void add_work(m8rt_native_live_stats *out,const m8rt_native_stats *in) {
    out->aot_calls+=in->aot_calls;out->region_calls+=in->region_calls;
    out->region_guest+=in->region_guest;out->step_calls+=in->fallback_steps;
    out->step_guest+=in->fallback_guest;
}
static void add_events(m8rt_native_event_stats *out,const m8rt_native_event_stats *in) {
#define ADD(field) out->field+=in->field
    ADD(entries);ADD(completed);ADD(logical_slices);ADD(enum_ticks);
    ADD(main_hooks);ADD(wait_hooks);ADD(delay_hooks);ADD(sampling_hooks);ADD(audio_hooks);
    ADD(irq_entries);ADD(exception_returns);ADD(memory_returns);ADD(required_returns);
    ADD(budget_returns);ADD(host_returns);
#undef ADD
}
/* Policy-owned transfers preserve the pure PC lookup and chain caches while
 * Unicorn executes a declined prefix. Exclusive state is transferred only
 * under the optional monitor contract; private FP context stays in Unicorn. */
static int transfer_cpu(m8rt *rt,bool write) {
    m8_cpu *cpu=&rt->native.cpu;
    int ids[50];void *words[50];unsigned n=0;
    for(unsigned i=0;i<16;i++) {
        ids[n]=i<13?UC_ARM_REG_R0+(int)i:i==13?UC_ARM_REG_SP:i==14?UC_ARM_REG_LR:UC_ARM_REG_PC;
        words[n++]=&cpu->r[i];
    }
    ids[n]=UC_ARM_REG_XPSR;words[n++]=&cpu->xpsr;
    for(unsigned i=0;i<32;i++) {ids[n]=UC_ARM_REG_S0+(int)i;words[n++]=&cpu->s[i];}
    ids[n]=UC_ARM_REG_FPSCR;words[n++]=&cpu->fpscr;
    if(!write) return uc_reg_read_batch(rt->uc,ids,words,(int)n)?-1:0;
    uint32_t pc=cpu->r[15]|1u;
    cpu->r[15]=pc;
    uc_err e=uc_reg_write_batch(rt->uc,ids,words,(int)n);
    cpu->r[15]=pc&~1u;
    if(!e) e=uc_reg_write(rt->uc,UC_ARM_REG_APSR_NZCVQG,&cpu->xpsr);
    if(!e) e=uc_reg_write(rt->uc,UC_ARM_REG_PC,&pc);
    return e?-1:0;
}
static void remember_cpu(m8rt *rt) {
    rt->native_live.uc_shadow=rt->native.cpu;
    rt->native_live.uc_masks[0]=rt->native.arch.primask;
    rt->native_live.uc_masks[1]=rt->native.arch.basepri;
    rt->native_live.uc_masks[2]=rt->native.arch.faultmask;
    rt->native_live.uc_masks[3]=rt->native.arch.control;
    rt->native_live.shadow_valid=true;
}
static int publish_delta(m8rt *rt) {
    const m8_cpu *before=&rt->native_live.uc_shadow,*after=&rt->native.cpu;
    int ids[54],n=0;void *values[54];
    for(unsigned i=0;i<15;i++) if(before->r[i]!=after->r[i]) {
        ids[n]=i<13?UC_ARM_REG_R0+(int)i:i==13?UC_ARM_REG_SP:UC_ARM_REG_LR;
        values[n++]=(void *)&after->r[i];
    }
    for(unsigned i=0;i<32;i++) if(before->s[i]!=after->s[i]) {
        ids[n]=UC_ARM_REG_S0+(int)i;values[n++]=(void *)&after->s[i];
    }
    if(before->fpscr!=after->fpscr) {
        ids[n]=UC_ARM_REG_FPSCR;values[n++]=(void *)&after->fpscr;
    }
    uint32_t changed=before->xpsr^after->xpsr;
    if(changed & ~UINT32_C(0xf80f0000)) {
        ids[n]=UC_ARM_REG_XPSR;values[n++]=(void *)&after->xpsr;
        /* XPSR does not publish GE. The following APSR write restores it. */
        ids[n]=UC_ARM_REG_APSR_NZCVQG;values[n++]=(void *)&after->xpsr;
    } else if(changed) {
        ids[n]=UC_ARM_REG_APSR_NZCVQG;values[n++]=(void *)&after->xpsr;
    }
    const int masks[]={UC_ARM_REG_PRIMASK,UC_ARM_REG_BASEPRI,UC_ARM_REG_FAULTMASK,UC_ARM_REG_CONTROL};
    uint32_t *words[]={&rt->native.arch.primask,&rt->native.arch.basepri,
        &rt->native.arch.faultmask,&rt->native.arch.control};
    for(unsigned i=0;i<4;i++) if(rt->native_live.uc_masks[i]!=*words[i]) {
        ids[n]=masks[i];values[n++]=words[i];
    }
    if(n && uc_reg_write_batch(rt->uc,ids,values,n)) return -1;
    rt->native_live.stats.cpu_export_words+=(unsigned)n;
    if(before->r[15]!=after->r[15]) {
        uint32_t pc=after->r[15]|1u;
        if(uc_reg_write(rt->uc,UC_ARM_REG_PC,&pc)) return -1;
        rt->native_live.stats.cpu_export_words++;
        rt->native_live.stats.pc_redirects++;
    }
    return 0;
}
static int publish_cpu(m8rt *rt) {
    if(rt->native_live.reuse_state && m8rt_native_export_exclusive(rt)) return -1;
    if(!rt->native_live.reuse_state) {
        if(m8rt_set_native_playback(rt,NULL,NULL)) return -1;
        rt->native_live.stats.cpu_export_words+=56;
        rt->native_live.stats.pc_redirects+=2;
    } else if(rt->native_live.delta_state && rt->native_live.shadow_valid) {
        if(publish_delta(rt)) return -1;
    } else {
        if(transfer_cpu(rt,true)) return -1;
        int ids[]={UC_ARM_REG_PRIMASK,UC_ARM_REG_BASEPRI,UC_ARM_REG_FAULTMASK,UC_ARM_REG_CONTROL};
        uint32_t *values[]={&rt->native.arch.primask,&rt->native.arch.basepri,
            &rt->native.arch.faultmask,&rt->native.arch.control};
        for(unsigned i=0;i<4;i++) if(uc_reg_write(rt->uc,ids[i],values[i])) return -1;
        rt->native_live.stats.cpu_export_words+=56;
        rt->native_live.stats.pc_redirects+=2;
    }
    rt->native.active=false;
    rt->native_live.shadow_valid=false;
    return 0;
}
static int import_cpu(m8rt *rt) {
    if(transfer_cpu(rt,false)) return -1;
    m8_native_arch *a=&rt->native.arch;
    int ids[]={UC_ARM_REG_PRIMASK,UC_ARM_REG_BASEPRI,UC_ARM_REG_FAULTMASK,
        UC_ARM_REG_CONTROL,UC_ARM_REG_MSP,UC_ARM_REG_PSP,UC_ARM_REG_IPSR,UC_ARM_REG_FPEXC};
    uint32_t *values[]={&a->primask,&a->basepri,&a->faultmask,&a->control,
        &a->msp,&a->psp,&a->ipsr,&a->fpexc};
    for(unsigned i=0;i<8;i++) if(uc_reg_read(rt->uc,ids[i],values[i])) return -1;
    if(uc_reg_read(rt->uc,UC_ARM_REG_CPSR,&rt->native.cpsr)) return -1;
    a->cpsr=rt->native.cpsr;a->itstate=m8_native_it_from_xpsr(rt->native.cpu.xpsr);
    if(m8rt_native_import_exclusive(rt)) return -1;
    memset(&rt->native.stats,0,sizeof rt->native.stats);
    memset(&rt->native.event_stats,0,sizeof rt->native.event_stats);
    rt->native.chain_state.mask_seed=rt->aot_services?1u:0u;
    rt->native.active=true;
    remember_cpu(rt);
    rt->native_live.stats.cpu_import_words+=59;
    return 0;
}
static void release_state(m8rt *rt) {
    free(rt->native.lookup_cache);rt->native.lookup_cache=NULL;
    free(rt->native.chain_state.slots);rt->native.chain_state.slots=NULL;
    rt->native.region=NULL;rt->native.chain=NULL;rt->native.event_chain=NULL;
    rt->native_live.initialized=false;
    rt->native_live.shadow_valid=false;
}
void m8rt_native_live_leave(m8rt *rt,unsigned reason,uint32_t pc) {
    if(!rt->native_live.enabled || !rt->native.active || rt->native_live.leave_pending) return;
    rt->native_live.reason=reason;rt->native_live.leave_pc=pc;
    rt->native_live.leave_pending=true;rt->native.stopped=true;
    rt->native.service_exit=true;
}
static void handoff_time(m8rt *rt,m8rt_native_handoff_profile *out) {
    if(!rt->native_live.handoff_interval_open) return;
    uint64_t elapsed=rt->uc_ns-rt->native_live.handoff_interval_uc_start;
    unsigned bin=rt->native_live.handoff_interval_bin;
    if(bin<M8RT_NATIVE_HANDOFF_DETAILS) out->rows[bin].unicorn_owned_ns+=elapsed;
    else out->overflow_unicorn_owned_ns+=elapsed;
}
static void handoff_close(m8rt *rt) {
    handoff_time(rt,&rt->native_live.handoff_profile);
    rt->native_live.handoff_interval_open=false;
}
static void handoff_record(m8rt *rt,unsigned reason,uint32_t pc) {
    m8rt_native_handoff_profile *out=&rt->native_live.handoff_profile;
    m8rt_native_handoff_detail value={.pc=pc,.reason=reason};
    const m8_region *r=pc<rt->memory.itcm.size?&rt->memory.itcm:&rt->memory.flash;
    uint32_t offset=pc-r->base;
    if(r->host && pc>=r->base && r->size>=2 && offset<=r->size-2) {
        uint32_t h1=m8rt_le16(r->host+offset);
        unsigned bytes=(h1&0xf800u)>=0xe800u?4u:2u;
        if(r->size>=bytes && offset<=r->size-bytes) {
            value.instruction=h1;
            if(bytes==4) value.instruction|=(uint32_t)m8rt_le16(r->host+offset+2)<<16;
            value.instruction_bytes=bytes;value.instruction_valid=1;
            if(reason==M8RT_NATIVE_LEAVE_UNSUPPORTED && rt->native.active &&
               bytes==2 && (h1&0xff87u)==0x4780u) {
                unsigned rm=(h1>>3)&15u;
                if(rm<15) {value.target=rt->native.cpu.r[rm];value.target_valid=1;}
            }
        }
    }
    handoff_close(rt);
    unsigned bin;
    for(bin=0;bin<M8RT_NATIVE_HANDOFF_DETAILS;bin++) {
        m8rt_native_handoff_detail *p=&out->rows[bin];
        if(!p->count || (p->pc==value.pc && p->reason==value.reason &&
            p->instruction==value.instruction && p->instruction_valid==value.instruction_valid &&
            p->instruction_bytes==value.instruction_bytes && p->target==value.target &&
            p->target_valid==value.target_valid)) {
            if(!p->count) *p=value;
            p->count++;break;
        }
    }
    if(bin==M8RT_NATIVE_HANDOFF_DETAILS) out->overflow++;
    rt->native_live.handoff_interval_bin=bin;
    rt->native_live.handoff_interval_uc_start=rt->uc_ns;
    rt->native_live.handoff_interval_open=true;
}
int m8rt_set_native_handoff_profile(m8rt *rt,int enabled) {
    if(!rt || rt->in_emu || (enabled!=0 && enabled!=1)) return -1;
    if(enabled) {
        memset(&rt->native_live.handoff_profile,0,sizeof rt->native_live.handoff_profile);
        rt->native_live.handoff_profile.enabled=1;
        rt->native_live.handoff_interval_open=false;
    } else {
        handoff_close(rt);rt->native_live.handoff_profile.enabled=0;
    }
    return 0;
}
int m8rt_read_native_handoff_profile(m8rt *rt,m8rt_native_handoff_profile *out) {
    if(!rt || !out || rt->in_emu) return -1;
    *out=rt->native_live.handoff_profile;handoff_time(rt,out);return 0;
}

/* The generated regions/AOT decline architectural system instructions. The
 * live handler defers exclusives unless monitor transfer is enabled, and
 * always defers inactive-stack writes to Unicorn. Private FP state stays there.
 * CONTROL writes are not implemented by the frozen handler and decline. */
static unsigned private_instruction(m8_mem *mem,uint32_t pc) {
    const m8_region *r=pc<mem->itcm.size?&mem->itcm:&mem->flash;
    uint32_t offset=pc-r->base;
    if(pc<r->base || r->size<4 || offset>r->size-4) return M8RT_NATIVE_ARCH_COUNT;
    const uint8_t *p=r->host+offset;
    uint32_t h1=m8rt_le16(p),h2=m8rt_le16(p+2);
    if((h1&0xfff0u)==0xe850u && (h2&0x0f00u)==0x0f00u) return M8RT_NATIVE_ARCH_LDREX;
    if((h1&0xfff0u)==0xe840u) return M8RT_NATIVE_ARCH_STREX;
    if(h1==0xf3bfu && h2==0x8f2fu) return M8RT_NATIVE_ARCH_CLREX;
    if((h1&0xfff0u)==0xf380u && (h2&0xf000u)==0x8000u) {
        if((h2&255u)==8u) return M8RT_NATIVE_ARCH_MSR_MSP;
        if((h2&255u)==9u) return M8RT_NATIVE_ARCH_MSR_PSP;
        if((h2&255u)==20u) return M8RT_NATIVE_ARCH_MSR_CONTROL;
    }
    return M8RT_NATIVE_ARCH_COUNT;
}
static int live_step(m8_cpu *cpu,m8_mem *mem,m8_native_arch *arch) {
    m8rt *rt=mem->ctx;
    uint32_t pc=cpu->r[15];
    arch->last_executed=arch->last_special=0;
    unsigned reason=M8RT_NATIVE_LEAVE_UNSUPPORTED;
    int result;
    unsigned cause=private_instruction(mem,pc);
    bool native_exclusive=cause<=M8RT_NATIVE_ARCH_CLREX && m8rt_native_exclusive_transfer_enabled(rt);
    if(cause<M8RT_NATIVE_ARCH_COUNT && !native_exclusive) {result=2;reason=M8RT_NATIVE_LEAVE_ARCHITECTURE;}
    else result=rt->native_live.config.step(cpu,mem,arch);
    if(result==2) {
        /* Result 2 guarantees no instruction effects. Return a zero-length
         * prefix to the frozen dispatcher, then resume this PC in Unicorn. */
        m8rt_native_live_leave(rt,reason,pc);
        return 0;
    }
    return result;
}

static void note_rejection(m8rt *rt,uint32_t pc,unsigned reason,uint32_t value) {
    rt->native_live.rejection.count++;
    rt->native_live.rejection.pc=pc;
    rt->native_live.rejection.reason=reason;
    rt->native_live.rejection.value=value;
    if(reason<M8RT_NATIVE_REJECT_COUNT && !rt->native_live.rejection.reasons[reason]++) {
        rt->native_live.rejection.first_pc[reason]=pc;
        rt->native_live.rejection.first_value[reason]=value;
    }
}
bool m8rt_native_live_entry(m8rt *rt,uint32_t pc) {
    if(m8rt_native_live_reference_entry(rt)) return true;
    if(!rt->native_live.enabled || rt->native.active || rt->native_live.code_disabled ||
       pc!=0x6005118cu || !m8rt_usb_boot(rt) || rt->watchdog.reset_pending) return false;
    /* A wider inherited reservation stays in Unicorn. Permit the stopped
     * main instruction to execute once rather than requesting the same
     * rejected switch again at the same PC. The next main entry can retry. */
    if(rt->native_live.exclusive_rejected) {
        rt->native_live.exclusive_rejected=false;return false;
    }
    uint32_t xpsr=m8rt_reg_read(rt,UC_ARM_REG_XPSR);
    uint32_t control=m8rt_reg_read(rt,UC_ARM_REG_CONTROL);
    /* Software IRQ frames use MSP. Warm FPCA prevents first-use activation;
     * native code cannot write CONTROL, FPCCR or the inactive stack bank. */
    /* Keep the original predicate order and short-circuit register reads. */
#define REJECT(reason,operand) do { uint32_t v=(uint32_t)(operand); if(v) { \
    rt->native_live.stats.rejected_entries++; \
    note_rejection(rt,pc,M8RT_NATIVE_REJECT_##reason,v);return false; \
} } while(0)
    REJECT(IRQ_NEST,rt->irq_nest);
    REJECT(IPSR,xpsr&0x1ffu);
    REJECT(ITSTATE,m8_native_it_from_xpsr(xpsr));
    if(!(xpsr&0x01000000u)) {
        rt->native_live.stats.rejected_entries++;
        note_rejection(rt,pc,M8RT_NATIVE_REJECT_THUMB,0);return false;
    }
    if(control!=4u) {
        rt->native_live.stats.rejected_entries++;
        note_rejection(rt,pc,M8RT_NATIVE_REJECT_CONTROL,control);return false;
    }
    REJECT(AOT_ACTIVE,rt->aot_active);
    REJECT(SDHC_PENDING,rt->aot_sdhc_pending);
    REJECT(NVIC_PENDING,rt->nvic_pending_banks);
    REJECT(SYSTICK_PENDING,rt->systick_pending);
    REJECT(AUDIO_PENDING,rt->audio_pending);
    REJECT(USB_PENDING,rt->usb_pending);
    REJECT(PRIMASK,m8rt_reg_read(rt,UC_ARM_REG_PRIMASK));
    REJECT(BASEPRI,m8rt_reg_read(rt,UC_ARM_REG_BASEPRI));
    REJECT(FAULTMASK,m8rt_reg_read(rt,UC_ARM_REG_FAULTMASK));
#undef REJECT
    rt->native_live.enter_pending=true;
    if(rt->in_emu) m8rt_engine_stop(rt);
    return true;
}

int m8rt_native_live_switch(m8rt *rt) {
    if(rt->in_emu) return -1;
    if(rt->native_live.invalidate_pending) {
        rt->native_live.invalidate_pending=false;
        rt->native_live.enter_pending=false;
        return m8rt_set_aot(rt,NULL,NULL)?-1:1;
    }
    if(rt->native_live.leave_pending ||
       (rt->native_live.enabled && !rt->native_live.code_disabled && rt->native.code_invalidated)) {
        unsigned reason=rt->native.code_invalidated?M8RT_NATIVE_LEAVE_CODE_WRITE:rt->native_live.reason;
        uint32_t pc=rt->native.code_invalidated?rt->native.cpu.r[15]:rt->native_live.leave_pc;
        if(rt->native_live.handoff_profile.enabled) handoff_record(rt,reason,pc);
        add_work(&rt->native_live.stats,&rt->native.stats);
        add_events(&rt->native_live.stats.events,&rt->native.event_stats);
        if(rt->native.active && publish_cpu(rt)) return -1;
        rt->native_live.stats.leaves++;
        rt->native_live.stats.reasons[reason]++;
        rt->native_live.stats.last_leave_reason=reason;rt->native_live.stats.last_leave_pc=pc;
        unsigned cause=private_instruction(&rt->memory,pc);
        if(cause>=M8RT_NATIVE_ARCH_COUNT) cause=M8RT_NATIVE_ARCH_OTHER;
        if(reason==M8RT_NATIVE_LEAVE_ARCHITECTURE) rt->native_live.stats.architecture[cause]++;
        unsigned i;
        for(i=0;i<M8RT_NATIVE_HANDOFF_PCS;i++) {
            m8rt_native_handoff_pc *p=&rt->native_live.stats.handoff_pcs[i];
            if(!p->count || (p->pc==pc && p->reason==reason && p->architecture==cause)) {
                p->pc=pc;p->reason=reason;p->architecture=cause;p->count++;break;
            }
        }
        if(i==M8RT_NATIVE_HANDOFF_PCS) rt->native_live.stats.handoff_pc_overflow++;
        rt->native_live.leave_pending=false;
        if(reason==M8RT_NATIVE_LEAVE_CODE_WRITE) {
            rt->native_live.code_disabled=true;
            if(m8rt_set_aot(rt,NULL,NULL)) return -1;
        }
        return 1;
    }
    if(!rt->native_live.enter_pending) return 0;
    rt->native_live.enter_pending=false;
    const m8rt_native_live_config *c=&rt->native_live.config;
    if(!rt->native_live.verified || !rt->native_live.verify_cache) {
        rt->native_live.stats.source_checks++;
        uint32_t bad=0;
        if(c->verify(&rt->memory,&bad) || (c->region && c->region_verify(&rt->memory,&bad))) {
            /* A changed source is a sequential fallback, not a new engine
             * fault. Remove stale AOT entries before executing changed code. */
            rt->native_live.code_disabled=true;
            note_rejection(rt,0x6005118cu,M8RT_NATIVE_REJECT_SOURCE,bad);
            return m8rt_set_aot(rt,NULL,NULL)?-1:1;
        }
        rt->native_live.verified=true;
    }
    if(rt->native_live.verify_cache && observe_code_writes(rt)) return -1;
    int monitor=m8rt_native_import_exclusive(rt);
    if(monitor<0) return -1;
    if(monitor>0) {
        rt->native_live.stats.rejected_entries++;
        note_rejection(rt,0x6005118cu,M8RT_NATIVE_REJECT_EXCLUSIVE,(uint32_t)monitor);
        rt->native_live.exclusive_rejected=true;return 1;
    }
    if(rt->native_live.reuse_state && rt->native_live.initialized) {
        if(import_cpu(rt)) return -1;
    } else {
        if(m8rt_set_native_playback(rt,live_step,verified_source) ||
           (c->region && m8rt_set_native_regions(rt,c->region,verified_source)) ||
           (c->chain && m8rt_set_native_region_chain(rt,c->chain,c->chain_cache_slots)) ||
           (c->event_chain && m8rt_set_native_event_loop(rt,c->event_chain))) return -1;
        rt->native_live.initialized=true;
        remember_cpu(rt);
        rt->native_live.stats.cpu_import_words+=59;
    }
    rt->native_live.stats.enters++;
    if(rt->native_live.handoff_profile.enabled) handoff_close(rt);
    return 1;
}
void m8rt_native_live_storage(m8rt *rt,uint32_t addr,bool read) {
    if(addr>=0x402c0000u && addr<0x402c8000u && rt->native_live.enabled && rt->native.active) {
        (void)read;
        m8rt_native_live_leave(rt,M8RT_NATIVE_LEAVE_STORAGE,rt->native.cpu.r[15]);
    }
}
int m8rt_native_live_host_code_write(m8rt *rt) {
    if(!rt->native_live.enabled) return 0;
    if(rt->in_emu) {
        m8rt_fail(rt,"native live host code write inside execution pc=%08x",m8rt_reg_read(rt,UC_ARM_REG_PC));return -1;
    }
    if(rt->native.active) {
        m8rt_native_live_leave(rt,M8RT_NATIVE_LEAVE_CODE_WRITE,rt->native.cpu.r[15]);
        if(m8rt_native_live_switch(rt)<0) return -1;
    }
    rt->native_live.code_disabled=true;
    return m8rt_set_aot(rt,NULL,NULL);
}
int m8rt_set_native_live(m8rt *rt,const m8rt_native_live_config *config) {
    if(!rt || rt->in_emu || rt->fatal) return -1;
    if(!config) {
        if(rt->native.active && rt->native_live.enabled) {
            m8rt_native_live_leave(rt,M8RT_NATIVE_LEAVE_DISABLED,rt->native.cpu.r[15]);
            if(m8rt_native_live_switch(rt)<0) return -1;
        }
        rt->native_live.enabled=false;
        release_state(rt);
        if(rt->native_live.write_observer) {
            if(uc_hook_del(rt->uc,rt->native_live.code_hooks[0]) ||
               uc_hook_del(rt->uc,rt->native_live.code_hooks[1]) || uc_ctl_flush_tb(rt->uc)) return -1;
            rt->native_live.write_observer=false;
        }
        return 0;
    }
    if(rt->native.active || rt->native_live.enabled || !config->step || !config->verify ||
       !config->region || !config->region_verify || !rt->aot_lookup || !rt->direct_memory ||
       !rt->options.strict || !rt->deterministic_clock ||
       (config->event_chain && !config->chain) ||
       (config->chain && (!config->chain_cache_slots || config->chain_cache_slots>65536u || rt->aot_timing_enabled))) return -1;
    if(!m8rt_native_memory_admitted(rt)) {
        m8rt_fail(rt,"native live admission predicate=memory_layout compact_memory=%u fixed_memory=%u",
            rt->compact_memory!=NULL,rt->memory_offset!=NULL);return -1;
    }
    rt->native_live.config=*config;
    const char *cache=getenv("M8RT_NATIVE_LIVE_VERIFY_CACHE");
    if(cache && strcmp(cache,"0") && strcmp(cache,"1")) return -1;
    rt->native_live.verify_cache=!cache || strcmp(cache,"0");
    const char *reuse=getenv("M8RT_NATIVE_LIVE_REUSE");
    if(reuse && strcmp(reuse,"0") && strcmp(reuse,"1")) return -1;
    rt->native_live.reuse_state=!reuse || strcmp(reuse,"0");
    const char *delta=getenv("M8RT_NATIVE_LIVE_DELTA");
    if(delta && strcmp(delta,"0") && strcmp(delta,"1")) return -1;
    rt->native_live.delta_state=!delta || strcmp(delta,"0");
    const char *deadline=getenv("M8RT_NATIVE_DEADLINE_YIELD");
    if(deadline && strcmp(deadline,"0") && strcmp(deadline,"1")) return -1;
    rt->native_live.deadline_yield=deadline && !strcmp(deadline,"1");
    rt->native_live.deadline_seen=false;
    const char *events=getenv("M8RT_NATIVE_EVENT_LOOP");
    if(events && (strcmp(events,"0") && strcmp(events,"1"))) return -1;
    if(events && !strcmp(events,"1") && !config->event_chain) return -1;
    const char *monitor=getenv("M8RT_NATIVE_EXCLUSIVE_TRANSFER");
    if(monitor && strcmp(monitor,"0") && strcmp(monitor,"1")) return -1;
    if(monitor && !strcmp(monitor,"1") && !m8rt_native_exclusive_transfer_available()) return -1;
    memset(&rt->native_live.stats,0,sizeof rt->native_live.stats);
    memset(&rt->native_live.rejection,0,sizeof rt->native_live.rejection);
    rt->native_live.enabled=true;rt->native_live.verified=false;rt->native_live.exclusive_rejected=false;
    return 0;
}
int m8rt_read_native_live_rejection(m8rt *rt,m8rt_native_live_rejection *out) {
    if(!rt || !out || rt->in_emu) return -1;
    *out=rt->native_live.rejection;return 0;
}
int m8rt_read_native_live_stats(m8rt *rt,m8rt_native_live_stats *out) {
    if(!rt || !out || rt->in_emu) return -1;
    *out=rt->native_live.stats;
    if(rt->native.active) {
        add_work(out,&rt->native.stats);add_events(&out->events,&rt->native.event_stats);
    }
    out->enabled=rt->native_live.enabled;out->active=rt->native.active;
    out->code_disabled=rt->native_live.code_disabled;
    out->event_loop=rt->native_live.config.event_chain!=NULL;
    return 0;
}
#else
int m8rt_set_native_handoff_profile(m8rt *rt,int enabled) {(void)rt;(void)enabled;return -1;}
int m8rt_read_native_handoff_profile(m8rt *rt,m8rt_native_handoff_profile *out) {(void)rt;(void)out;return -1;}
int m8rt_native_exclusive_transfer_available(void) {return 0;}
int m8rt_native_exclusive_transfer_enabled(const m8rt *rt) {(void)rt;return 0;}
int m8rt_native_import_exclusive(m8rt *rt) {(void)rt;return -1;}
int m8rt_native_export_exclusive(m8rt *rt) {(void)rt;return -1;}
int m8rt_native_live_available(void) {return 0;}
int m8rt_set_native_live(m8rt *rt,const m8rt_native_live_config *c) {return !rt || rt->in_emu || c?-1:0;}
int m8rt_read_native_live_rejection(m8rt *rt,m8rt_native_live_rejection *out) {
    if(!rt || !out || rt->in_emu) return -1;
    memset(out,0,sizeof *out);return 0;
}
int m8rt_read_native_live_stats(m8rt *rt,m8rt_native_live_stats *out) {
    if(!rt || !out || rt->in_emu) return -1;
    memset(out,0,sizeof *out);return 0;
}
#endif
