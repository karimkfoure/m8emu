#include "internal.h"
#ifndef M8RT_NATIVE_BOOT_FIFO_CONTINUE_DEFAULT
#define M8RT_NATIVE_BOOT_FIFO_CONTINUE_DEFAULT 0
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "canonical_memory.h"
#ifdef M8RT_STATIC_COVERAGE
#include "native_static.h"
#endif

extern unsigned uc_m8_arm_exclusive_monitor_abi(void)
    __attribute__((weak, visibility("default")));

bool m8rt_native_memory_admitted(const m8rt *rt) {
    if(!rt || !m8_canonical_memory_supported(&rt->memory)) return false;
    if(!rt->compact_memory) return m8_canonical_memory_offset(&rt->memory)!=0;
    /* The supplied code verifier must also qualify its library's addressing
     * mode before execution. Canonical guest bounds alone do not prove this. */
    const uint8_t *base=rt->compact_memory;
    return rt->memory.itcm.host==base &&
        rt->memory.dtcm.host==base+0x80000u &&
        rt->memory.ocram.host==base+0x100000u &&
        rt->memory.flash.host==base+0x180000u &&
        rt->rom==base+0x980000u && rt->dwt==base+0x9a0000u;
}

int m8rt_native_exclusive_transfer_available(void) {
    /* Older Unicorn releases can report success for unknown register IDs.
     * Probe the explicit vendor ABI marker before reading any such register. */
    return uc_m8_arm_exclusive_monitor_abi && uc_m8_arm_exclusive_monitor_abi()==1u;
}
int m8rt_native_exclusive_transfer_enabled(const m8rt *rt) {
    return rt && rt->native.exclusive_transfer;
}
int m8rt_native_import_exclusive(m8rt *rt) {
    if(!rt || rt->in_emu || rt->native.active) return -1;
    const char *setting=getenv("M8RT_NATIVE_EXCLUSIVE_TRANSFER");
    if(setting && strcmp(setting,"0") && strcmp(setting,"1")) {
        m8rt_fail(rt,"native exclusive transfer value=%s; required=0 or 1",setting);return -1;
    }
    bool enabled=setting && !strcmp(setting,"1");
#ifdef M8RT_PURE_NATIVE
    enabled=true;
#endif
    m8_native_arch *arch=&rt->native.arch;
    if(!enabled) {
        rt->native.exclusive_transfer=false;arch->exclusive_valid=0;return 0;
    }
    if(!m8rt_native_exclusive_transfer_available()) {
        m8rt_fail(rt,"native exclusive transfer requires Unicorn monitor ABI 1");return -1;
    }
    uint64_t words[3];
    int ids[]={UC_ARM_REG_EXCLUSIVE_ADDR,UC_ARM_REG_EXCLUSIVE_VAL,UC_ARM_REG_EXCLUSIVE_HIGH};
    void *values[]={&words[0],&words[1],&words[2]};
    if(uc_reg_read_batch(rt->uc,ids,values,3)) return -1;
    /* The unchanged native ABI holds a word reservation. Retain a live wider
     * value in Unicorn rather than truncating it. An invalid monitor may hold
     * arbitrary stale values; preserve those bits until a native LDREX. */
    if(words[0]!=UINT64_MAX && (words[0]>UINT32_MAX || words[1]>UINT32_MAX)) return 1;
    arch->exclusive_address=(uint32_t)words[0];
    arch->exclusive_value=(uint32_t)words[1];
    arch->exclusive_valid=words[0]!=UINT64_MAX;
    memcpy(rt->native.exclusive_uc,words,sizeof words);
    rt->native.exclusive_shadow[0]=arch->exclusive_address;
    rt->native.exclusive_shadow[1]=arch->exclusive_value;
    rt->native.exclusive_shadow[2]=arch->exclusive_valid;
    rt->native.exclusive_transfer=true;
    return 0;
}
static int export_exclusive(m8rt *rt) {
    if(!rt->native.exclusive_transfer) return 0;
    const m8_native_arch *arch=&rt->native.arch;
    uint64_t words[3];memcpy(words,rt->native.exclusive_uc,sizeof words);
    if(arch->exclusive_address!=rt->native.exclusive_shadow[0] ||
       arch->exclusive_value!=rt->native.exclusive_shadow[1] ||
       arch->exclusive_valid!=rt->native.exclusive_shadow[2]) {
        words[0]=arch->exclusive_valid?arch->exclusive_address:UINT64_MAX;
        words[1]=arch->exclusive_value;
    }
    /* AArch32 word LDREX/STREX/CLREX leave exclusive_high unchanged. */
    int ids[]={UC_ARM_REG_EXCLUSIVE_ADDR,UC_ARM_REG_EXCLUSIVE_VAL,UC_ARM_REG_EXCLUSIVE_HIGH};
    void *values[]={&words[0],&words[1],&words[2]};
    return uc_reg_write_batch(rt->uc,ids,values,3)?-1:0;
}
int m8rt_native_export_exclusive(m8rt *rt) {
    if(!rt || rt->in_emu) return -1;
    return export_exclusive(rt);
}

#define NATIVE_LOOKUP_SLOTS 1024u
struct m8rt_native_lookup_entry {
    m8_aot_fn fn;
    uint32_t pc, valid;
};
void m8rt_native_reset_lookup(m8rt *rt) {
    rt->native.generation++;
    if(rt->native.lookup_cache)
        memset(rt->native.lookup_cache,0,NATIVE_LOOKUP_SLOTS*sizeof *rt->native.lookup_cache);
}
/* The maintained AOT lookup is a pure PC-to-function table. Cache NULL too:
 * native playback asks about fallback PCs that the Unicorn entry hooks omit. */
static inline __attribute__((always_inline)) m8_aot_fn cached_lookup(m8rt *rt,uint32_t pc) {
    if(!rt->aot_lookup) return NULL;
    if(rt->native.lookup_cache) {
        unsigned index=((pc>>1)^(pc>>11)^(pc>>21))&(NATIVE_LOOKUP_SLOTS-1u);
        struct m8rt_native_lookup_entry *slot=&rt->native.lookup_cache[index];
        if(slot->valid && slot->pc==pc) {rt->native.stats.lookup_hits++;return slot->fn;}
        m8_aot_fn fn=rt->aot_lookup(pc);
        *slot=(struct m8rt_native_lookup_entry){.fn=fn,.pc=pc,.valid=1};
        rt->native.stats.lookup_misses++;return fn;
    }
    rt->native.stats.lookup_misses++;return rt->aot_lookup(pc);
}
m8_aot_fn m8rt_native_lookup_pc(m8rt *rt,uint32_t pc) {return cached_lookup(rt,pc);}
static m8_aot_fn chain_lookup(void *user,uint32_t pc) {return cached_lookup(user,pc);}

static int general_index(int id) {
    if(id>=UC_ARM_REG_R0 && id<=UC_ARM_REG_R12) return id-UC_ARM_REG_R0;
    if(id==UC_ARM_REG_SP) return 13;
    if(id==UC_ARM_REG_LR) return 14;
    if(id==UC_ARM_REG_PC) return 15;
    return -1;
}
uint32_t m8rt_native_reg_read(m8rt *rt,int id) {
    m8_cpu *c=&rt->native.cpu;m8_native_arch *a=&rt->native.arch;
    int n=general_index(id);
    if(n>=0) return c->r[n];
    if(id>=UC_ARM_REG_S0 && id<=UC_ARM_REG_S31) return c->s[id-UC_ARM_REG_S0];
    switch(id) {
    case UC_ARM_REG_XPSR:return c->xpsr;
    case UC_ARM_REG_CPSR:return (rt->native.cpsr&~0xfe0ffc00u)|(c->xpsr&0xfe0ffc00u);
    case UC_ARM_REG_FPSCR:return c->fpscr;
    case UC_ARM_REG_IPSR:return a->ipsr;
    case UC_ARM_REG_PRIMASK:return a->primask;
    case UC_ARM_REG_BASEPRI:return a->basepri;
    case UC_ARM_REG_FAULTMASK:return a->faultmask;
    case UC_ARM_REG_CONTROL:return a->control;
    case UC_ARM_REG_MSP:return (a->ipsr || !(a->control&2))?c->r[13]:a->msp;
    case UC_ARM_REG_PSP:return (!a->ipsr && (a->control&2))?c->r[13]:a->psp;
    default:m8rt_fail(rt,"native register read unsupported id=%d",id);return 0;
    }
}
void m8rt_native_reg_write(m8rt *rt,int id,uint32_t v) {
    m8_cpu *c=&rt->native.cpu;m8_native_arch *a=&rt->native.arch;
    int n=general_index(id);
    if(n>=0) {c->r[n]=n==15?v&~1u:v;return;}
    if(id>=UC_ARM_REG_S0 && id<=UC_ARM_REG_S31) {c->s[id-UC_ARM_REG_S0]=v;return;}
    switch(id) {
    case UC_ARM_REG_XPSR:
        /* Unicorn's XPSR API writes NZCVQ, exception/IT/Thumb, but not GE. */
        c->xpsr=(c->xpsr&0x000f0000u)|(v&~0x000f0000u);break;
    case UC_ARM_REG_CPSR:
        rt->native.cpsr=v;
        c->xpsr=(c->xpsr&0x1ffu)|(v&0xfe0ffc00u)|((v&0x20u)?0x01000000u:0);break;
    case UC_ARM_REG_IPSR:a->ipsr=v&0x1ffu;c->xpsr=(c->xpsr&~0x1ffu)|a->ipsr;break;
    case UC_ARM_REG_FPSCR:c->fpscr=v;break;
    case UC_ARM_REG_PRIMASK:a->primask=v&1u;break;
    case UC_ARM_REG_BASEPRI:a->basepri=v&255u;break;
    case UC_ARM_REG_FAULTMASK:a->faultmask=v&1u;break;
    default:m8rt_fail(rt,"native register write unsupported id=%d value=%08x",id,v);break;
    }
    a->itstate=((c->xpsr>>25)&3u)|((c->xpsr>>8)&0xfcu);
}
static int cpu_transfer(m8rt *rt,bool write) {
    m8_cpu *c=&rt->native.cpu;
    int ids[50];void *values[50];unsigned n=0;
    for(unsigned i=0;i<16;i++) {ids[n]=i<13?UC_ARM_REG_R0+(int)i:i==13?UC_ARM_REG_SP:i==14?UC_ARM_REG_LR:UC_ARM_REG_PC;values[n++]=&c->r[i];}
    ids[n]=UC_ARM_REG_XPSR;values[n++]=&c->xpsr;
    for(unsigned i=0;i<32;i++) {ids[n]=UC_ARM_REG_S0+(int)i;values[n++]=&c->s[i];}
    ids[n]=UC_ARM_REG_FPSCR;values[n++]=&c->fpscr;
    if(write) {
        /* PC is published last, with Thumb selected. GE has its own writer. */
        uint32_t pc=c->r[15]|1u;c->r[15]=pc;
        uc_err err=uc_reg_write_batch(rt->uc,ids,values,(int)n);
        c->r[15]=pc&~1u;
        if(!err) err=uc_reg_write(rt->uc,UC_ARM_REG_APSR_NZCVQG,&c->xpsr);
        if(!err) err=uc_reg_write(rt->uc,UC_ARM_REG_PC,&pc);
        return err;
    }
    return uc_reg_read_batch(rt->uc,ids,values,(int)n);
}
static int native_publish(m8rt *rt) {
#ifdef M8RT_PURE_NATIVE
        m8rt_fail(rt,"native reference handoff is unavailable pc=%08x",rt->native.cpu.r[15]);return -1;
#endif
        /* The native region has returned before a code-store publication.
         * in_emu still marks that logical slice; Unicorn itself is stopped. */
        int err=export_exclusive(rt);
        if(!err) err=cpu_transfer(rt,true);
        int ids[]={UC_ARM_REG_PRIMASK,UC_ARM_REG_BASEPRI,UC_ARM_REG_FAULTMASK,UC_ARM_REG_CONTROL};
        uint32_t *words[]={&rt->native.arch.primask,&rt->native.arch.basepri,&rt->native.arch.faultmask,&rt->native.arch.control};
        for(unsigned i=0;i<4 && !err;i++) err=uc_reg_write(rt->uc,ids[i],words[i]);
        rt->native.active=false;rt->native.region=NULL;
        if(rt->native.device_direct) m8rt_native_device_bus(rt,false);
        rt->native.chain=NULL;
        rt->native.event_chain=NULL;
        free(rt->native.chain_state.slots);rt->native.chain_state.slots=NULL;
        free(rt->native.lookup_cache);rt->native.lookup_cache=NULL;
        return err?-1:0;
}
int m8rt_set_native_playback(m8rt *rt,m8rt_native_step_fn step,int (*verify)(m8_mem *,uint32_t *)) {
    if(!rt || rt->fatal || rt->in_emu) return -1;
    if(!step) {
#ifdef M8RT_PURE_NATIVE
        m8rt_fail(rt,"native execution cannot be disabled in a Unicorn-free build");return -1;
#else
        return rt->native.active?native_publish(rt):0;
#endif
    }
#ifdef M8RT_PURE_NATIVE
    if(rt->native.active) {
        uint32_t bad=0;
        if(!verify || !rt->aot_lookup || verify(&rt->memory,&bad)) {
            m8rt_fail(rt,"native warm code verification pc=%08x",bad);return -1;
        }
        const char *services=getenv("M8RT_NATIVE_SERVICE_CONTINUE");
        const char *devices=getenv("M8RT_NATIVE_DEVICE_DIRECT");
        const char *events=getenv("M8RT_NATIVE_EVENT_LOOP");
        if((services && strcmp(services,"0") && strcmp(services,"1")) ||
           (devices && strcmp(devices,"0") && strcmp(devices,"1")) ||
           (events && strcmp(events,"0") && strcmp(events,"1"))) {
            m8rt_fail(rt,"native warm options require 0 or 1");return -1;
        }
        rt->native.service_continue=services && !strcmp(services,"1");
        rt->native.device_direct=devices && !strcmp(devices,"1");
        rt->native.event_loop=events && !strcmp(events,"1");
        if(rt->native.device_direct) m8rt_native_device_bus(rt,true);
        m8rt_native_boot_bus_install(rt);
        /* The cold boot step stays installed for callback IRQ ownership.
         * Retain the verified warm single-step as a second fallback so
         * playback PCs declined by cold coverage can still use compiled
         * code instead of the portable interpreter. */
        rt->native.warm_step=step;
        memset(&rt->native.stats,0,sizeof rt->native.stats);
        return 0;
    }
#endif
    if(rt->native.active || !rt->aot_lookup || !rt->direct_memory || !verify) return -1;
    uint32_t bad=0;if(verify(&rt->memory,&bad)) {m8rt_fail(rt,"native admission changed AOT code pc=%08x",bad);return -1;}
    if(cpu_transfer(rt,false)) return -1;
    m8_native_arch *a=&rt->native.arch;
    int ids[]={UC_ARM_REG_PRIMASK,UC_ARM_REG_BASEPRI,UC_ARM_REG_FAULTMASK,UC_ARM_REG_CONTROL,UC_ARM_REG_MSP,UC_ARM_REG_PSP,UC_ARM_REG_IPSR};
    uint32_t *words[]={&a->primask,&a->basepri,&a->faultmask,&a->control,&a->msp,&a->psp,&a->ipsr};
    for(unsigned i=0;i<7;i++) if(uc_reg_read(rt->uc,ids[i],words[i])) return -1;
    uc_reg_read(rt->uc,UC_ARM_REG_CPSR,&rt->native.cpsr);
    a->cpsr=rt->native.cpsr;
    uc_reg_read(rt->uc,UC_ARM_REG_FPEXC,&a->fpexc);
    a->itstate=((rt->native.cpu.xpsr>>25)&3u)|((rt->native.cpu.xpsr>>8)&0xfcu);
    /* A bounded run can stop after main entry with a basic USB IRQ frame
     * active. That frame and its software owner remain in the runtime. */
    bool known_frame=!a->ipsr || (rt->irq_nest>0 && rt->exc_stack[rt->irq_nest-1]==a->ipsr);
    if(!known_frame || a->itstate || (a->control&3u)) {m8rt_fail(rt,"native admission ipsr=%u nest=%d it=%02x control=%08x",a->ipsr,rt->irq_nest,a->itstate,a->control);return -1;}
    const char *services=getenv("M8RT_NATIVE_SERVICE_CONTINUE");
    if(services && strcmp(services,"0") && strcmp(services,"1")) {
        m8rt_fail(rt,"native service continuation value=%s; required=0 or 1",services);return -1;
    }
    rt->native.service_continue=services && !strcmp(services,"1");
    const char *devices=getenv("M8RT_NATIVE_DEVICE_DIRECT");
    if(devices && strcmp(devices,"0") && strcmp(devices,"1")) {
        m8rt_fail(rt,"native direct device value=%s; required=0 or 1",devices);return -1;
    }
    rt->native.device_direct=devices && !strcmp(devices,"1");
    if(rt->native.device_direct && rt->dispatch_stats_enabled) {
        m8rt_fail(rt,"native direct device bus requires dispatch diagnostics disabled");return -1;
    }
    const char *events=getenv("M8RT_NATIVE_EVENT_LOOP");
    if(events && strcmp(events,"0") && strcmp(events,"1")) {
        m8rt_fail(rt,"native event loop value=%s; required=0 or 1",events);return -1;
    }
    rt->native.event_loop=events && !strcmp(events,"1");
    rt->native.event_chain=NULL;rt->native.event_slice_finished=false;
    rt->native.event_polls=0;
    memset(&rt->native.event_stats,0,sizeof rt->native.event_stats);
    rt->native.service_polls=0;
    rt->native.service_epoch=0;
    const char *cache=getenv("M8RT_NATIVE_LOOKUP_CACHE");
    if(cache && strcmp(cache,"0") && strcmp(cache,"1")) {m8rt_fail(rt,"native lookup cache value=%s; required=0 or 1",cache);return -1;}
    if(!cache || strcmp(cache,"0")) {
        rt->native.lookup_cache=calloc(NATIVE_LOOKUP_SLOTS,sizeof *rt->native.lookup_cache);
        if(!rt->native.lookup_cache) {m8rt_fail(rt,"native lookup cache allocation slots=%u",NATIVE_LOOKUP_SLOTS);return -1;}
    }
    int monitor=m8rt_native_import_exclusive(rt);
    if(monitor) return -1;
    memset(&rt->native.stats,0,sizeof rt->native.stats);
    rt->native.step=step;rt->native.region=NULL;rt->native.code_invalidated=false;
    rt->native.generation++;
    rt->native.boot=false;rt->native.boot_callback_irq=false;
    rt->native.active=true;
    if(rt->native.device_direct) m8rt_native_device_bus(rt,true);
    return 0;
}
static m8_aot_fn boot_lookup(uint32_t pc) {(void)pc;return NULL;}
static uint32_t boot_entries(const uint32_t **pcs) {*pcs=NULL;return 0;}
int m8rt_native_boot_enabled(const m8rt *rt) {return rt && rt->native.active && rt->native.boot;}
int m8rt_set_native_boot(m8rt *rt,m8rt_native_step_fn step,int (*verify)(m8_mem *,uint32_t *)) {
    if(!rt || rt->fatal || rt->in_emu || rt->native.active || !step || !verify) return -1;
    if(!rt->options.strict || rt->aot_lookup || rt->native_live.enabled ||
       rt->main_iters || rt->guest_cycles || rt->slices || rt->uc_ns || rt->irq_nest) {
        const char *predicate=!rt->options.strict?"strict":rt->aot_lookup?"aot":
            rt->native_live.enabled?"live":rt->main_iters?"main":rt->guest_cycles?"cycles":
            rt->slices?"slices":rt->uc_ns?"uc_ns":"nest";
        m8rt_fail(rt,"native boot fresh-state predicate=%s strict=%u aot=%d live=%u main=%llu cycles=%llu slices=%llu uc_ns=%llu nest=%d",
            predicate,rt->options.strict,rt->aot_lookup!=NULL,rt->native_live.enabled,
            (unsigned long long)rt->main_iters,(unsigned long long)rt->guest_cycles,
            (unsigned long long)rt->slices,(unsigned long long)rt->uc_ns,rt->irq_nest);return -1;
    }
    if(m8rt_set_aot(rt,boot_lookup,boot_entries) || m8rt_set_native_playback(rt,step,verify)) return -1;
    if(rt->native_boot_app.admission
#ifdef M8RT_PURE_NATIVE
       || true
#endif
    ) {
        /* The app keeps warm settings for its later dispatch installation.
         * Reset executes the same cold service policy as the checked owner. */
        if(rt->native.device_direct) m8rt_native_device_bus(rt,false);
        rt->native.device_direct=rt->native.service_continue=rt->native.event_loop=false;
    }
    m8_cpu *cpu=&rt->native.cpu;m8_native_arch *a=&rt->native.arch;
#define BOOT_REQUIRE(name,value,expected) do {uint32_t v=(value),e=(expected);if(v!=e) { \
    m8rt_fail(rt,"native boot reset predicate=%s value=%08x expected=%08x",name,v,e);return -1;}} while(0)
    BOOT_REQUIRE("pc",cpu->r[15],0x60005c24u);BOOT_REQUIRE("sp",cpu->r[13],0x20070000u);
    BOOT_REQUIRE("lr",cpu->r[14],UINT32_MAX);BOOT_REQUIRE("xpsr",cpu->xpsr,0);
    BOOT_REQUIRE("fpscr",cpu->fpscr,0);BOOT_REQUIRE("cpsr",rt->native.cpsr,0x01000013u);
    BOOT_REQUIRE("control",a->control,0);BOOT_REQUIRE("ipsr",a->ipsr,0);
    BOOT_REQUIRE("itstate",a->itstate,0);BOOT_REQUIRE("msp",a->msp,cpu->r[13]);
    BOOT_REQUIRE("psp",a->psp,0);BOOT_REQUIRE("primask",a->primask,0);
    BOOT_REQUIRE("basepri",a->basepri,0);BOOT_REQUIRE("faultmask",a->faultmask,0);
    BOOT_REQUIRE("fpexc",a->fpexc,0);BOOT_REQUIRE("event_loop",rt->native.event_loop,0);
    for(unsigned i=0;i<13;i++) BOOT_REQUIRE("general_register",cpu->r[i],0);
    for(unsigned i=0;i<32;i++) BOOT_REQUIRE("fp_register",cpu->s[i],0);
#undef BOOT_REQUIRE
#ifdef M8RT_STATIC_COVERAGE
    uint32_t bad=0;
    if(m8_native_static_region_verify_flash(&rt->memory,&bad)) {
        m8rt_fail(rt,"native static flash verification pc=%08x",bad);return -1;
    }
    rt->native.static_flash_verified=true;rt->native.static_itcm_verified=false;
    m8rt_native_boot_bus_install(rt);
#endif
    const char *fifo=getenv("M8RT_NATIVE_BOOT_FIFO_CONTINUE");
    if(fifo && strcmp(fifo,"0") && strcmp(fifo,"1")) {
        m8rt_fail(rt,"native boot FIFO continuation value=%s expected=0_or_1",fifo);return -1;
    }
    rt->native.boot_fifo_continue=fifo?!strcmp(fifo,"1"):M8RT_NATIVE_BOOT_FIFO_CONTINUE_DEFAULT;
    rt->native.boot=true;rt->native.boot_copied=0;rt->native.boot_callback_irqs=0;
    /* uc_emu_start(pc|1) selects Thumb before the first reset instruction. */
    m8rt_native_reg_write(rt,UC_ARM_REG_CPSR,rt->native.cpsr|0x20u);
    a->cpsr=rt->native.cpsr;
    return 0;
}
#ifdef M8RT_PURE_NATIVE
void m8rt_native_invalidate_compiled_code(m8rt *rt) {
    rt->native.interpret_only=true;
    rt->native.code_invalidated=false;
    rt->native.static_flash_verified=false;
    rt->native.static_itcm_verified=false;
    rt->native.region=NULL;rt->native.chain=NULL;
    rt->native.event_chain=NULL;rt->native.event_loop=false;
    rt->aot_lookup=NULL;m8rt_native_reset_lookup(rt);
}
int m8rt_native_after_reset(m8rt *rt) {
    m8_thumb_code_cache_invalidate(&rt->native.code_cache);
    if(!rt->native_boot_app.stats.selected) {
        extern int m8_native_boot_verify_memory(m8_mem *,uint32_t *);
        rt->native_boot_app.config.cold_step=rt->native.step;
        rt->native_boot_app.config.cold_verify=m8_native_boot_verify_memory;
    }
    if(!rt->native_boot_app.config.cold_step) {
        m8rt_fail(rt,"native reset requires a cold session owner");return -1;
    }
    uint32_t bad=0;
    if(rt->native_boot_app.config.cold_verify(&rt->memory,&bad)) {
        m8rt_fail(rt,"native reset code mismatch pc=%08x",bad);return -1;
    }
    rt->native.step=rt->native_boot_app.config.cold_step;
    /* A post-reset session restarts cold; the previous warm single-step is
     * stale until the next warm activation re-admits it. */
    rt->native.warm_step=NULL;
    rt->native.active=true;rt->native.boot=true;rt->native.boot_copied=0;
    rt->native.boot_callback_irq=false;rt->native.code_invalidated=false;
    rt->native.interpret_only=false;rt->native.interpreting=false;
    rt->native.static_itcm_verified=false;
    rt->native.region=NULL;rt->native.chain=NULL;rt->native.event_chain=NULL;
    rt->native.service_continue=false;rt->native.event_loop=false;
    if(rt->native.device_direct) m8rt_native_device_bus(rt,false);
    rt->native.device_direct=false;
#ifdef M8RT_STATIC_COVERAGE
    m8rt_native_boot_bus_install(rt);
#endif
    rt->native_boot_app.pending=rt->native_boot_app.stats.selected!=0;rt->native_boot_app.ready=false;
    rt->native_boot_app.started_ns=m8rt_now_ns();
    if(m8rt_set_aot(rt,boot_lookup,boot_entries)) return -1;
    m8rt_native_reg_write(rt,UC_ARM_REG_CPSR,rt->native.cpsr|0x20u);
    rt->native.arch.cpsr=rt->native.cpsr;
    return 0;
}
#endif
int m8rt_native_boot_copy(m8rt *rt,uint32_t addr,const void *in,size_t size) {
    m8_cpu *cpu=&rt->native.cpu;
    const uint32_t span=0x3d5d8u,source=0x1809e4u;
    const char *predicate=NULL;
    uint32_t value=size==4?m8rt_le32(in):0;
    uint32_t expected=addr<=span-4?m8rt_le32(rt->memory.flash.host+source+addr):0;
    if(size!=4) predicate="size";
    else if(cpu->r[15]!=0x600059e0u) predicate="pc";
    else if((addr&3u) || addr>=span) predicate="span";
    else if(addr!=rt->native.boot_copied) predicate="order";
    else if(cpu->r[0]!=addr) predicate="r0";
    else if(cpu->r[1]!=0x60000000u+source+addr+4u) predicate="r1";
    else if(cpu->r[2]!=span) predicate="r2";
    else if(cpu->r[3]!=value) predicate="r3";
    else if(cpu->r[14]!=0x60005a4fu) predicate="lr";
    else if(value!=expected) predicate="source_value";
    if(predicate) {
        m8rt_fail(rt,"native boot copy predicate=%s pc=%08x address=%08x size=%zu copied=%08x value=%08x expected=%08x r0=%08x r1=%08x r2=%08x r3=%08x lr=%08x",
            predicate,cpu->r[15],addr,size,rt->native.boot_copied,value,expected,
            cpu->r[0],cpu->r[1],cpu->r[2],cpu->r[3],cpu->r[14]);return UC_ERR_WRITE_PROT;
    }
    memcpy(rt->memory.itcm.host+addr,in,4);rt->native.boot_copied+=4;
#ifdef M8RT_STATIC_COVERAGE
    if(rt->native.boot_copied==span) {
        uint32_t bad=0;
        if(m8_native_static_region_verify_itcm(&rt->memory,&bad)) {
            m8rt_fail(rt,"native static ITCM verification pc=%08x",bad);return UC_ERR_EXCEPTION;
        }
        rt->native.static_itcm_verified=true;
    }
#endif
    return 0;
}
static int boot_before(m8rt *rt,uint32_t pc) {
    if(pc!=0x6005f7aau || rt->native.arch.control) return 0;
    const m8_cpu *cpu=&rt->native.cpu;const m8_native_arch *a=&rt->native.arch;
    uint32_t cpacr=m8rt_raw_read(rt,0xe000ed88u);
    uint32_t opcode=m8rt_le32(rt->memory.flash.host+pc-rt->memory.flash.base);
    if(rt->native.boot_copied!=0x3d5d8u || cpacr!=0x00f00000u ||
       cpu->fpscr || a->ipsr || a->itstate || a->fpexc || opcode!=0x6a09eeb3u) {
        const char *predicate=rt->native.boot_copied!=0x3d5d8u?"copied":cpacr!=0x00f00000u?"cpacr":
            cpu->fpscr?"fpscr":a->ipsr?"ipsr":a->itstate?"itstate":a->fpexc?"fpexc":"opcode";
        m8rt_fail(rt,"native boot first FP predicate=%s pc=%08x copied=%08x cpacr=%08x fpscr=%08x ipsr=%u it=%u fpexc=%08x opcode=%08x",
            predicate,pc,rt->native.boot_copied,cpacr,cpu->fpscr,a->ipsr,a->itstate,a->fpexc,opcode);return -1;
    }
    rt->native.arch.control=4; /* Reference automatic CONTROL.FPCA activation. */
    return 0;
}
int m8rt_read_native_stats(m8rt *rt,m8rt_native_stats *out) {
    if(!rt || !out || rt->in_emu) return -1;
    *out=rt->native.stats;return 0;
}
/* Env-gated per-page dispatch histogram for cold-coverage sizing.
 * M8RT_DISPATCH_HIST=1 counts; the driver dumps cumulative snapshots.
 * Guest execution is unchanged. */
static uint64_t *m8_dispatch_hist=NULL;
static int m8_dispatch_hist_on=-1;
void m8rt_native_dispatch_hist_record(m8rt *rt,uint32_t pc) {
    (void)rt;
    if(m8_dispatch_hist_on<0) {
        const char *marker=getenv("M8RT_DISPATCH_HIST");
        m8_dispatch_hist_on=marker && *marker?1:0;
        if(m8_dispatch_hist_on) m8_dispatch_hist=calloc((size_t)1<<20,sizeof(uint64_t));
        if(m8_dispatch_hist_on && !m8_dispatch_hist) m8_dispatch_hist_on=0;
    }
    if(m8_dispatch_hist_on) m8_dispatch_hist[pc>>12]++;
}
int m8rt_dump_dispatch_hist(const char *path) {
    FILE *out;
    uint32_t page;
    if(m8_dispatch_hist_on<=0 || !m8_dispatch_hist || !path) return -1;
    out=fopen(path,"w");
    if(!out) return -1;
    for(page=0;page<(uint32_t)1<<20;page++)
        if(m8_dispatch_hist[page]) fprintf(out,"%08x %" PRIu64 "\n",page<<12,m8_dispatch_hist[page]);
    return fclose(out)?-1:0;
}
int m8rt_native_service_continue_enabled(const m8rt *rt) {
    return rt && rt->native.service_continue;
}
int m8rt_native_device_direct_enabled(const m8rt *rt) {
    return rt && rt->native.active && rt->native.device_direct;
}
int m8rt_native_event_loop_enabled(const m8rt *rt) {return rt && rt->native.event_loop;}
int m8rt_read_native_event_stats(m8rt *rt,m8rt_native_event_stats *out) {
    if(!rt || !out || rt->in_emu) return -1;
    *out=rt->native.event_stats;return 0;
}
static int native_event(void *user,unsigned phase,uint32_t pc,unsigned cause) {
    m8rt *rt=user;
    return phase==M8_NATIVE_EVENT_ENTER?m8rt_native_event_enter(rt,pc):
        m8rt_native_event_completed(rt,pc,cause);
}
int m8rt_set_native_event_loop(m8rt *rt,m8_native_region_event_fn fn) {
    if(!rt || rt->fatal || rt->in_emu || !rt->native.active ||
       (fn && (!rt->native.chain || !rt->native.region || rt->aot_timing_enabled))) return -1;
    rt->native.event_chain=fn;
    rt->native.event_loop=fn!=NULL;
    rt->native.events=(m8_native_events){.user=rt,.event=native_event,.step=rt->native.step};
#ifdef M8RT_INTERPRETER
    rt->native.events.step=m8rt_native_step_dispatch;
#endif
    return 0;
}
int m8rt_set_native_regions(m8rt *rt,m8rt_native_step_fn region,
                            int (*verify)(m8_mem *,uint32_t *)) {
    if(!rt || rt->fatal || rt->in_emu || !rt->native.active) return -1;
    if(region) {
        uint32_t bad=0;
        if(!verify) return -1;
        if(!m8rt_native_memory_admitted(rt)) {
            m8rt_fail(rt,"native region admission predicate=memory_layout compact_memory=%u fixed_memory=%u",
                rt->compact_memory!=NULL,rt->memory_offset!=NULL);return -1;
        }
        if(verify(&rt->memory,&bad)) {
            m8rt_fail(rt,"native admission changed region code pc=%08x",bad);return -1;
        }
    }
    rt->native.region=region;
    m8rt_native_reset_lookup(rt);
    return 0;
}
int m8rt_set_native_region_chain(m8rt *rt,m8_native_region_chain_fn chain,size_t slots) {
    if(!rt || rt->fatal || rt->in_emu || !rt->native.active || !rt->native.region ||
       rt->aot_timing_enabled || (chain && (!slots || slots>65536u))) return -1;
    m8_native_chain_slot *cache=chain?calloc(slots,sizeof *cache):NULL;
    if(chain && !cache) return -1;
    free(rt->native.chain_state.slots);
    rt->native.chain_state=(m8_native_chain){.user=rt,.lookup=chain_lookup,
        .generation=&rt->native.generation,.service_exit=&rt->native.service_exit,
        .mask_cache=&rt->aot_mask_cache,.mask_seed=rt->aot_services?1u:0u,
        .slots=cache,.slot_count=slots,.max_hops=256u};
    rt->native.chain=chain;
    return 0;
}
uc_err m8rt_native_exec(m8rt *rt) {
    rt->native.stopped=false;rt->native.stats.slices++;
#ifdef M8RT_PURE_NATIVE
    m8_cpu_backend_clear_stop(rt->uc);
#endif
    rt->native.event_slice_finished=false;
#ifdef M8RT_INTERPRETER
    rt->native.interpreter_error=0;
#endif
    if(rt->native.event_loop) {
        rt->native.event_polls=0;
        if(!rt->native.event_chain || !rt->native.chain || rt->watchdog_stall_hook) {
            const char *predicate=!rt->native.event_chain?"event_chain":
                !rt->native.chain?"chain":"watchdog_stall_hook";
            m8rt_fail(rt,"native event admission predicate=%s event_chain=%d chain=%d watchdog_stall_hook=%llu",predicate,
                rt->native.event_chain!=NULL,rt->native.chain!=NULL,(unsigned long long)rt->watchdog_stall_hook);
            return UC_ERR_EXCEPTION;
        }
event_dispatch:;
        m8_native_chain *state=&rt->native.chain_state;
        state->aot_calls=state->region_calls=state->region_guest=0;
        state->hops=state->cache_hits=state->cache_misses=0;
        m8_native_events *events=&rt->native.events;
        events->fallback_steps=events->fallback_guest=events->interpreter_guest=events->segments=0;
        rt->native.stats.dispatch_calls++;rt->native.stats.chain_calls++;
        int result=rt->native.event_chain(&rt->native.cpu,&rt->memory,&rt->native.arch,state,events);
        rt->native.stats.chain_hops+=state->hops;
        rt->native.stats.chain_hits+=state->cache_hits;
        rt->native.stats.chain_misses+=state->cache_misses;
        rt->native.stats.aot_calls+=state->aot_calls;
        rt->native.stats.region_calls+=state->region_calls;
        rt->native.stats.region_guest+=state->region_guest;
        rt->native.stats.fallback_steps+=events->fallback_steps;
        rt->native.stats.fallback_guest+=events->fallback_guest;
        rt->native.stats.interpreter_guest+=events->interpreter_guest;
        rt->aot_active=false;rt->aot_mask_cache=0;
        bool resumed=rt->native_boot_app.bus_pending && !rt->fatal;
        if(resumed) result=m8rt_native_boot_app_resume(rt,result);
        if(rt->native.boot_callback_irq && !rt->fatal) {
            if(result!=1) {
                m8rt_fail(rt,"native boot chained callback IRQ predicate=result value=%d expected=1 pc=%08x handler=%08x",
                    result,rt->native.stats.last_pc,rt->native.boot_irq_cpu.r[15]);return UC_ERR_EXCEPTION;
            }
            rt->native.cpu=rt->native.boot_irq_cpu;
            rt->native.arch=rt->native.boot_irq_arch;
            rt->native.boot_callback_irq=false;
            return UC_ERR_OK;
        }
        if(rt->native.code_invalidated && !rt->fatal) {
#ifdef M8RT_PURE_NATIVE
            rt->native.interpret_only=true;rt->native.event_loop=false;
            return UC_ERR_OK;
#else
            if(native_publish(rt)) {m8rt_fail(rt,"native event code-write handoff pc=%08x",rt->native.cpu.r[15]);return UC_ERR_EXCEPTION;}
            return UC_ERR_OK;
#endif
        }
        if(result || rt->fatal) {
#ifdef M8RT_INTERPRETER
            if(!rt->fatal && rt->native.interpreter_error) return rt->native.interpreter_error;
#endif
            if(!rt->fatal) {
                if(result==2) rt->native.stats.unsupported_pc=rt->native.cpu.r[15];
                m8rt_fail(rt,"native event execution pc=%08x next=%08x result=%d",
                    rt->native.stats.last_pc,rt->native.cpu.r[15],result);
            }
            return UC_ERR_EXCEPTION;
        }
        if(resumed) {
            int action=m8rt_native_event_completed(rt,rt->native.stats.last_pc,M8_NATIVE_EXIT_MEMORY);
            if(action==M8_NATIVE_EVENT_FAULT) return UC_ERR_EXCEPTION;
            if(action==M8_NATIVE_EVENT_EXIT || action==M8_NATIVE_EVENT_HANDOFF) return UC_ERR_OK;
            goto event_dispatch;
        }
        return UC_ERR_OK;
    }
    uint32_t polls=0;
    while(!rt->fatal && !rt->native.stopped) {
        uint32_t pc=rt->native.cpu.r[15];rt->native.stats.last_pc=pc;
        /* The 1/256 poll count and short-circuit order below are load-bearing:
         * device tests pin the observer call counts. Latch the observation so
         * later hook checks stop on the flag without rereading the clock. */
        bool deadline_fired=rt->deadline_ns && !(polls++&255u) && m8rt_now_ns()>=rt->deadline_ns;
        if(deadline_fired) rt->native_live.deadline_seen=true;
        if(atomic_load_explicit(&rt->host_stop,memory_order_relaxed) ||
#ifdef M8RT_PURE_NATIVE
           m8_cpu_backend_stop_requested(rt->uc) ||
#endif
           deadline_fired || rt->watchdog.reset_pending) break;
        if(m8rt_native_idle(rt,pc)) continue;
        if(pc>=0xf0000000u) {
            if(!m8rt_native_unstack(rt)) {m8rt_fail(rt,"native exception return pc=%08x",pc);return UC_ERR_EXCEPTION;}
            break;
        }
        if(rt->native_boot_app.pending && m8rt_native_boot_app_entry(rt,pc)) break;
        m8rt_native_boundary(rt,pc);
        if(rt->fatal || rt->native.stopped || rt->native.cpu.r[15]!=pc) continue;
        if(rt->native.boot && boot_before(rt,pc)) return UC_ERR_EXCEPTION;
        rt->native.stats.dispatch_calls++;
        m8rt_native_dispatch_hist_record(rt,pc);
        m8_aot_fn fn=rt->native.interpret_only || rt->native.chain || rt->native.arch.itstate?NULL:cached_lookup(rt,pc);
        rt->aot_active=true;rt->aot_mask_cache=rt->aot_services?1u:0u;
        int result=M8_AOT_EXIT;
        if(rt->native.chain && !rt->native.interpret_only) {
            m8_native_chain *state=&rt->native.chain_state;
            state->aot_calls=state->region_calls=state->region_guest=0;
            state->hops=state->cache_hits=state->cache_misses=0;
            rt->native.service_exit=false;
            result=rt->native.chain(&rt->native.cpu,&rt->memory,&rt->native.arch,state);
            rt->native.stats.chain_calls++;
            rt->native.stats.chain_hops+=state->hops;
            rt->native.stats.chain_hits+=state->cache_hits;
            rt->native.stats.chain_misses+=state->cache_misses;
            rt->native.stats.aot_calls+=state->aot_calls;
            rt->native.stats.region_calls+=state->region_calls;
            rt->native.stats.region_guest+=state->region_guest;
        } else if(fn) {
            uint64_t start=rt->aot_timing_enabled?m8rt_now_ns():0;
            result=fn(&rt->native.cpu,&rt->memory);rt->native.stats.aot_calls++;
            if(rt->aot_timing_enabled) {rt->aot_calls++;rt->aot_native_ns+=m8rt_now_ns()-start;}
        }
        if((rt->native.chain && result==2) || (!rt->native.chain &&
           (!fn || (result==M8_AOT_EXIT && rt->native.cpu.r[15]==pc)))) {
            result=2;
            if(!rt->native.interpret_only && !rt->native.chain && rt->native.region && !rt->native.arch.itstate) {
                result=rt->native.region(&rt->native.cpu,&rt->memory,&rt->native.arch);
                if(result!=2) {
                    rt->native.stats.region_calls++;
                    rt->native.stats.region_guest+=rt->native.arch.last_executed;
                }
            }
#ifdef M8RT_STATIC_COVERAGE
            /* Web cold-only builds skip the static companion once the warm
             * single-step exists: post-boot fallbacks keep their warm path
             * without a declined static attempt per step. */
            bool static_cold=true;
#ifdef M8RT_STATIC_COLD_ONLY
            static_cold=rt->native.warm_step==NULL;
#endif
            if(static_cold && result==2 && !rt->native.interpret_only && !rt->native.arch.itstate &&
               ((pc>=0x60000000u && rt->native.static_flash_verified) ||
                (pc<0x80000u && rt->native.static_itcm_verified))) {
                result=m8_native_static_region_step(&rt->native.cpu,&rt->memory,&rt->native.arch);
                if(result!=2) {
                    rt->native.stats.region_calls++;
                    rt->native.stats.region_guest+=rt->native.arch.last_executed;
                }
            }
#endif
            bool fallback=result==2;
            if(fallback) {
#ifdef M8RT_INTERPRETER
                result=m8rt_native_step_dispatch(&rt->native.cpu,&rt->memory,&rt->native.arch);
#else
                result=rt->native.step(&rt->native.cpu,&rt->memory,&rt->native.arch);
#endif
            }
            if(!result && fallback) {
                rt->native.stats.fallback_steps++;
                rt->native.stats.fallback_guest+=rt->native.arch.last_executed;
                rt->native.stats.interpreter_guest+=rt->native.arch.last_special;
            }
            else if(result==2) {rt->native.stats.unsupported_pc=pc;m8rt_fail(rt,"native unsupported instruction pc=%08x xpsr=%08x it=%02x",pc,rt->native.cpu.xpsr,rt->native.arch.itstate);}
        }
        rt->aot_active=false;rt->aot_mask_cache=0;
        if(rt->native_boot_app.bus_pending && !rt->fatal)
            result=m8rt_native_boot_app_resume(rt,result);
        if(rt->native.boot_callback_irq && !rt->fatal) {
            /* A callback took the IRQ after its partial device effects. The
             * handler's fault return must not replace that saved context. */
            if(result!=1) {
                m8rt_fail(rt,"native boot callback IRQ result=%d expected=1 pc=%08x handler=%08x",
                    result,pc,rt->native.boot_irq_cpu.r[15]);return UC_ERR_EXCEPTION;
            }
            rt->native.cpu=rt->native.boot_irq_cpu;
            rt->native.arch=rt->native.boot_irq_arch;
            rt->native.boot_callback_irq=false;
            break;
        }
        if(rt->native.code_invalidated && !rt->fatal) {
#ifdef M8RT_PURE_NATIVE
            rt->native.interpret_only=true;
            result=m8rt_native_step_dispatch(&rt->native.cpu,&rt->memory,&rt->native.arch);
#else
            if(native_publish(rt)) {m8rt_fail(rt,"native code-write handoff failed pc=%08x",rt->native.cpu.r[15]);return UC_ERR_EXCEPTION;}
            break;
#endif
        }
#ifdef M8RT_INTERPRETER
        if(result && !rt->fatal && rt->native.interpreter_error) return rt->native.interpreter_error;
#endif
        if(result) {if(!rt->fatal) m8rt_fail(rt,"native execution fault pc=%08x next=%08x result=%d",pc,rt->native.cpu.r[15],result);return UC_ERR_EXCEPTION;}
        m8rt_native_completed(rt);
    }
    return UC_ERR_OK;
}
static int dump_bytes(const char *prefix,const char *suffix,const void *p,size_t n) {
    char path[4096];if(snprintf(path,sizeof path,"%s.%s",prefix,suffix)>=(int)sizeof path) return -1;
    FILE *f=fopen(path,"wb");if(!f) return -1;
    int err=fwrite(p,1,n,f)==n?0:-1;if(fclose(f)) err=-1;return err;
}
int m8rt_dump_native_state(m8rt *rt,const char *prefix) {
    if(!rt || !prefix || rt->in_emu) return -1;
    if(!rt->native.active) {
        if(cpu_transfer(rt,false)) return -1;
        m8_native_arch *a=&rt->native.arch;
        int ids[]={UC_ARM_REG_PRIMASK,UC_ARM_REG_BASEPRI,UC_ARM_REG_FAULTMASK,UC_ARM_REG_CONTROL,UC_ARM_REG_MSP,UC_ARM_REG_PSP,UC_ARM_REG_IPSR,UC_ARM_REG_CPSR,UC_ARM_REG_FPEXC};
        uint32_t *words[]={&a->primask,&a->basepri,&a->faultmask,&a->control,&a->msp,&a->psp,&a->ipsr,&rt->native.cpsr,&a->fpexc};
        for(unsigned i=0;i<9;i++) if(uc_reg_read(rt->uc,ids[i],words[i])) return -1;
        a->cpsr=rt->native.cpsr;a->itstate=m8_native_it_from_xpsr(rt->native.cpu.xpsr);
    }
    /* This raw runtime object includes scheduler, NVIC, frame owner and queue
     * descriptors. Device payloads remain separate owners; see contract limits. */
    int err=dump_bytes(prefix,"cpu",&rt->native.cpu,sizeof rt->native.cpu);
    err|=dump_bytes(prefix,"arch",&rt->native.arch,sizeof rt->native.arch);
    err|=dump_bytes(prefix,"runtime",rt,sizeof *rt);
    const m8_region *regions[]={&rt->memory.itcm,&rt->memory.dtcm,&rt->memory.ocram,&rt->memory.flash};
    const char *names[]={"itcm","dtcm","ocram","flash"};
    for(unsigned i=0;i<4;i++) err|=dump_bytes(prefix,names[i],regions[i]->host,regions[i]->size);
    err|=dump_bytes(prefix,"dwt",rt->dwt,4096);
    char path[4096];if(snprintf(path,sizeof path,"%s.json",prefix)>=(int)sizeof path) return -1;
    FILE *f=fopen(path,"w");if(!f) return -1;
    fprintf(f,"{\"pc\":%u,\"xpsr\":%u,\"cpsr\":%u,\"fpscr\":%u,\"r\":[",rt->native.cpu.r[15],rt->native.cpu.xpsr,m8rt_reg_read(rt,UC_ARM_REG_CPSR),rt->native.cpu.fpscr);
    for(unsigned i=0;i<16;i++) fprintf(f,"%s%u",i?",":"",rt->native.cpu.r[i]);
    fprintf(f,"],\"s\":[");for(unsigned i=0;i<32;i++) fprintf(f,"%s%u",i?",":"",rt->native.cpu.s[i]);
    fprintf(f,"],\"main_iters\":%llu,\"guest_cycles\":%llu,\"cyccnt\":%u,\"irq_nest\":%d,\"next_systick\":%u,\"next_sof\":%u,\"next_audio\":%u,\"primask\":%u,\"basepri\":%u,\"control\":%u,\"itstate\":%u,\"error\":\"%s\"}\n",
        (unsigned long long)rt->main_iters,(unsigned long long)rt->guest_cycles,rt->cyccnt,rt->irq_nest,rt->next_systick,rt->next_sof,rt->next_audio,rt->native.arch.primask,rt->native.arch.basepri,rt->native.arch.control,rt->native.arch.itstate,rt->error);
    if(fclose(f)) err=-1;
    return err;
}

#ifdef M8RT_PURE_NATIVE
/* Reconstruct dispatch from this build's verified libraries, never saved host
 * addresses. This runs on a disposable fresh instance, without guest steps. */
int m8rt_native_session_prepare(m8rt *rt) {
    if(rt->native.active)return 0;
    extern int m8_native_cold_entry(m8_cpu *,m8_mem *,m8_native_arch *);
    extern int m8_native_boot_verify_memory(m8_mem *,uint32_t *);
    m8_aot_fn (*lookup)(uint32_t)=rt->aot_lookup;
    uint32_t (*entries)(const uint32_t **)=rt->aot_entry_function;
    unsigned services=rt->aot_services;
    return m8rt_set_aot(rt,NULL,NULL) ||
        m8rt_set_native_boot(rt,m8_native_cold_entry,m8_native_boot_verify_memory) ||
        m8rt_set_aot(rt,lookup,entries) || m8rt_set_aot_services(rt,services)?-1:0;
}
int m8rt_native_session_rebuild(m8rt *rt) {
    m8_thumb_code_cache_invalidate(&rt->native.code_cache);
    m8rt_native_reset_lookup(rt);
    rt->native.boot_copied=0x3d5d8u;
#ifdef M8RT_STATIC_COVERAGE
    uint32_t bad=0;
    rt->native.static_flash_verified=!m8_native_static_region_verify_flash(&rt->memory,&bad);
    rt->native.static_itcm_verified=!m8_native_static_region_verify_itcm(&rt->memory,&bad);
#endif
    const m8rt_native_boot_app_config *c=&rt->native_boot_app.config;
    if(c->warm.step) {
        uint32_t bad=0;
        if(c->warm.verify(&rt->memory,&bad) || c->warm.region_verify(&rt->memory,&bad)) {
            /* Writable ITCM may contain guest code absent from this build.
             * The portable decoder rebuilds it from the captured bytes. */
            m8rt_native_invalidate_compiled_code(rt);
            rt->native_boot_app.pending=rt->native_boot_app.ready=false;
            return 0;
        }
        if(m8rt_set_aot(rt,c->lookup,c->entries) || m8rt_set_aot_services(rt,c->services) ||
           m8rt_set_native_playback(rt,c->warm.step,c->warm.verify) ||
           m8rt_set_native_regions(rt,c->warm.region,c->warm.region_verify))return -1;
        if(c->warm.chain && m8rt_set_native_region_chain(rt,c->warm.chain,c->warm.chain_cache_slots))return -1;
        if(rt->native.event_loop && m8rt_set_native_event_loop(rt,c->warm.event_chain))return -1;
        rt->native_boot_app.pending=rt->native_boot_app.ready=false;
        rt->native_boot_app.stats.warm=1;
    }
    rt->native.stopped=false;rt->native.service_exit=false;
    rt->native.event_slice_finished=false;
    return 0;
}
#endif
