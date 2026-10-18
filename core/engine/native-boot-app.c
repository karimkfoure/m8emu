#include "internal.h"
#include <stdlib.h>
#include <string.h>

#ifdef M8RT_NATIVE_ENGINE
#include "native-services.h"
#include "native-sai-quiet.h"
int m8rt_native_boot_app_available(void) {return 1;}

static const char *warm_boundary(m8rt *rt,uint32_t pc,uint32_t *value) {
#define REQUIRE(name,operand,expected) do {uint32_t v=(operand);if(v!=(expected)) {*value=v;return name;}} while(0)
    REQUIRE("pc",pc,UINT32_C(0x6005118c));
    REQUIRE("usb_boot",m8rt_usb_boot(rt),1u);
    REQUIRE("irq_nest",rt->irq_nest,0u);
    REQUIRE("ipsr",rt->native.arch.ipsr,0u);
    REQUIRE("itstate",rt->native.arch.itstate,0u);
    REQUIRE("thumb",(rt->native.cpu.xpsr>>24)&1u,1u);
    REQUIRE("control",rt->native.arch.control,4u);
    REQUIRE("primask",rt->native.arch.primask,0u);
    REQUIRE("basepri",rt->native.arch.basepri,0u);
    REQUIRE("faultmask",rt->native.arch.faultmask,0u);
    REQUIRE("sdhc_pending",rt->aot_sdhc_pending,0u);
    REQUIRE("nvic_pending",rt->nvic_pending_banks,0u);
    REQUIRE("systick_pending",rt->systick_pending,0u);
    REQUIRE("audio_pending",rt->audio_pending,0u);
    REQUIRE("usb_pending",rt->usb_pending,0u);
    REQUIRE("watchdog_reset",rt->watchdog.reset_pending,0u);
    REQUIRE("aot_active",rt->aot_active,0u);
#undef REQUIRE
    return NULL;
}

int m8rt_set_native_boot_app(m8rt *rt,const m8rt_native_boot_app_config *config) {
    if(!rt || rt->fatal || rt->in_emu || !config) return -1;
#define REQUIRE(name,condition,value) do {if(!(condition)) {m8rt_fail(rt,"native boot app admission predicate=%s value=%llu",name,(unsigned long long)(value));return -1;}} while(0)
    REQUIRE("unused",!rt->native_boot_app.stats.selected,rt->native_boot_app.stats.selected);
    REQUIRE("cold_step",config->cold_step!=NULL,config->cold_step!=NULL);
    REQUIRE("cold_verify",config->cold_verify!=NULL,config->cold_verify!=NULL);
    REQUIRE("warm_step",config->warm.step!=NULL,config->warm.step!=NULL);
    REQUIRE("warm_verify",config->warm.verify!=NULL,config->warm.verify!=NULL);
    REQUIRE("warm_region",config->warm.region!=NULL,config->warm.region!=NULL);
    REQUIRE("warm_region_verify",config->warm.region_verify!=NULL,config->warm.region_verify!=NULL);
    REQUIRE("warm_lookup",config->lookup!=NULL,config->lookup!=NULL);
    REQUIRE("warm_entries",config->entries!=NULL,config->entries!=NULL);
    REQUIRE("warm_services",!(config->services&~M8_AOT_SERVICE_NVIC),config->services);
    REQUIRE("deterministic_clock",rt->deterministic_clock,rt->deterministic_clock);
    REQUIRE("memory_layout",m8rt_native_memory_admitted(rt),rt->compact_memory!=NULL);
    REQUIRE("event_chain",!config->warm.event_chain || config->warm.chain,config->warm.chain!=NULL);
#undef REQUIRE
    uint64_t started=m8rt_now_ns();
    rt->native_boot_app.admission=true;
    int result=m8rt_set_native_boot(rt,config->cold_step,config->cold_verify);
    rt->native_boot_app.admission=false;
    if(result) return -1;
    rt->native_boot_app.config=*config;
    rt->native_boot_app.stats=(m8rt_native_boot_app_stats){.selected=1};
    rt->native_boot_app.started_ns=started;
    rt->native_boot_app.pending=true;
    return 0;
}

bool m8rt_native_boot_app_entry(m8rt *rt,uint32_t pc) {
    if(!rt->native_boot_app.pending) return false;
    uint32_t value;
    if(warm_boundary(rt,pc,&value)) return false;
    rt->native_boot_app.stats.boot_elapsed_ns=m8rt_now_ns()-rt->native_boot_app.started_ns;
    rt->native_boot_app.stats.boot_main_iters=rt->main_iters;
    rt->native_boot_app.stats.boot_cycles=rt->guest_cycles;
    rt->native_boot_app.stats.boot_pc=pc;
    rt->native_boot_app.ready=true;
    rt->native.stopped=true;
    return true;
}
int m8rt_measure_native_boot_app(m8rt *rt) {
    if(!rt || rt->fatal || rt->in_emu || rt->native_boot_app.stats.selected ||
       rt->native_boot_app.measuring || rt->main_iters) return -1;
    rt->native_boot_app.started_ns=m8rt_now_ns();rt->native_boot_app.measuring=true;
    return 0;
}
void m8rt_native_boot_app_measure_entry(m8rt *rt,uint32_t pc) {
    if(!rt->native_boot_app.measuring || pc!=0x6005118cu || !m8rt_usb_boot(rt) ||
       rt->irq_nest || rt->aot_sdhc_pending || rt->nvic_pending_banks ||
       rt->systick_pending || rt->audio_pending || rt->usb_pending ||
       rt->watchdog.reset_pending || rt->aot_active) return;
    /* The reference owner has no current native.arch. Register observations
     * do not touch MMIO, deliver events or advance the guest clock. */
    const int ids[]={UC_ARM_REG_IPSR,UC_ARM_REG_XPSR,UC_ARM_REG_CONTROL,
        UC_ARM_REG_PRIMASK,UC_ARM_REG_BASEPRI,UC_ARM_REG_FAULTMASK};
    uint32_t values[6];
    for(unsigned i=0;i<6;i++) values[i]=m8rt_reg_read(rt,ids[i]);
    if(values[0] || (values[1]&0x0600fc00u) || !(values[1]&0x01000000u) ||
       values[2]!=4u || values[3] || values[4] || values[5]) return;
    rt->native_boot_app.measuring=false;
    rt->native_boot_app.stats.boot_elapsed_ns=m8rt_now_ns()-rt->native_boot_app.started_ns;
    rt->native_boot_app.stats.boot_main_iters=rt->main_iters;
    rt->native_boot_app.stats.boot_cycles=rt->guest_cycles;
    rt->native_boot_app.stats.boot_pc=pc;
}

static int warm_option(m8rt *rt,const char *name,bool *value) {
    const char *setting=getenv(name);
    if(setting && strcmp(setting,"0") && strcmp(setting,"1")) {
        m8rt_fail(rt,"native boot app warm predicate=%s value=%s expected=0_or_1",name,setting);return -1;
    }
    *value=setting && !strcmp(setting,"1");return 0;
}

/* A compiled region can retain registers in locals until its fault exit. SDHC
 * may enter an IRQ inside its callback, before the access retires. Decline that
 * access without device effects, then use the checked cold handler after the
 * region has published its complete CPU. Other device routes stay unchanged. */
static bool defer_sdhc(m8rt *rt,uint32_t address,uint32_t size,bool read,uint32_t value) {
    if(rt->native_boot_app.bus_executing || address<0x402c0000u || address>=0x402c8000u)
        return false;
    if(rt->native_boot_app.bus_pending) {
        m8rt_fail(rt,"native boot SDHC decline predicate=unused value=1 address=%08x size=%u pending_address=%08x",
            address,size,rt->native_boot_app.bus_address);return true;
    }
    rt->native_boot_app.bus_pending=true;
    rt->native_boot_app.bus_address=address;rt->native_boot_app.bus_size=size;
    rt->native_boot_app.bus_read=read;rt->native_boot_app.bus_value=value;
    rt->native_boot_app.bus_declines++;
    return true;
}
/* A pending USB request is delivered only at the retained clock/logical-slice
 * hooks, never at an interior DATPORT return. Regions stop at those hook PCs.
 * This read cannot pump USB or advance its clock. Keep all other observers,
 * the original deadline polling and the post-callback IRQ-enable check. */
static bool boot_fifo_observer(m8rt *rt) {
    if(rt->deadline_ns && !(rt->native.service_polls++ & 255u) &&
       m8rt_now_ns()>=rt->deadline_ns) {
        rt->native.service_exit=true;
        rt->native_live.deadline_seen=true;
    }
    return rt->fatal || rt->native.stopped || rt->native.code_invalidated ||
        rt->native.service_exit || rt->watchdog.reset_pending || rt->aot_sdhc_pending ||
        rt->audio_pending || rt->systick_pending || rt->nvic_pending_banks ||
        atomic_load_explicit(&rt->host_stop,memory_order_relaxed);
}
static bool boot_fifo_unchanged(m8rt *rt,const m8rt_native_service_token *token,
                                uint32_t usb_enable) {
    return token->admitted && !boot_fifo_observer(rt) &&
        usb_enable==(rt->nvic_iser[113u>>5]&(1u<<(113u&31u))) &&
        token->epoch==rt->native.service_epoch && token->cycles==rt->guest_cycles &&
        token->clock==rt->cyccnt && token->pcm==rt->pcm_total && token->local==rt->local_total &&
        token->pc==rt->native.cpu.r[15] && token->nesting==rt->irq_nest;
}
int m8rt_native_boot_device_read(void *ctx,uint32_t address,uint32_t size,uint32_t *value,
                               int (*read)(void *,uint32_t,uint32_t,uint32_t *)) {
    m8rt *rt=ctx;
    if(rt->native.boot_fifo_continue && rt->native.boot && size==4 &&
       address-UINT32_C(0x402c0000)<0x8000u && (address&0xfffu)==0x20u &&
       m8rt_sd_datport_read_quiet(rt,address,size) &&
       m8rt_native_sai_quiet(rt) &&
       !boot_fifo_observer(rt)) {
        m8rt_native_service_token token=m8rt_native_service_capture(rt);
        uint32_t usb_enable=rt->nvic_iser[113u>>5]&(1u<<(113u&31u));
        int result=read(ctx,address,size,value);
        return !result && boot_fifo_unchanged(rt,&token,usb_enable)?M8_AOT_MEM_CONTINUE:result;
    }
    if(defer_sdhc(rt,address,size,true,0)) return 1;
    return read(ctx,address,size,value);
}
int m8rt_native_boot_device_write(void *ctx,uint32_t address,uint32_t size,uint32_t value,
                                int (*write)(void *,uint32_t,uint32_t,uint32_t)) {
    m8rt *rt=ctx;
    if(defer_sdhc(rt,address,size,false,value)) return 1;
    return write(ctx,address,size,value);
}
static int boot_bus_read(void *ctx,uint32_t address,uint32_t size,uint32_t *value) {
    m8rt *rt=ctx;
    return m8rt_native_boot_device_read(ctx,address,size,value,rt->native_boot_app.bus_read_fn);
}
static int boot_bus_write(void *ctx,uint32_t address,uint32_t size,uint32_t value) {
    m8rt *rt=ctx;
    return m8rt_native_boot_device_write(ctx,address,size,value,rt->native_boot_app.bus_write_fn);
}
void m8rt_native_boot_bus_install(m8rt *rt) {
#ifdef M8RT_PURE_NATIVE
    /* The warm direct bus applies the same protocol only to SDHC addresses. */
    if(m8rt_native_device_bus_installed(rt)) return;
#endif
    if(rt->memory.read==boot_bus_read && rt->memory.write==boot_bus_write) return;
    rt->native_boot_app.bus_read_fn=rt->memory.read;
    rt->native_boot_app.bus_write_fn=rt->memory.write;
    rt->memory.read=boot_bus_read;rt->memory.write=boot_bus_write;
}
/* Only scalar Thumb loads/stores can be resumed once. In particular, exclude
 * LDM/STM, doubleword accesses and VFP transfers: a previous access in the same
 * instruction might already have effects. The cold library checks code bytes
 * and owns effective addresses, predication and writeback. */
static bool scalar_access(uint16_t first,uint16_t second,bool *read,unsigned *size) {
    if((first&0xe000u)==0x6000u) {
        *read=(first&0x0800u)!=0;*size=(first&0x1000u)?1u:4u;return true;
    }
    if((first&0xf000u)==0x8000u || (first&0xf000u)==0x9000u) {
        *read=(first&0x0800u)!=0;*size=(first&0xf000u)==0x8000u?2u:4u;return true;
    }
    if((first&0xf000u)==0x5000u) {
        unsigned op=(first>>9)&7u;
        *read=op>=3u;*size=(op==1u || op==5u || op==7u)?2u:(op==0u || op==4u)?4u:1u;
        return true;
    }
    unsigned op=(first>>4)&15u;
    if((first&0xff00u)==0xf800u && (op<=5u || (op>=8u && op<=13u))) {
        *read=(op&1u)!=0;*size=(op&6u)==0u?1u:(op&6u)==2u?2u:4u;
    } else if((first&0xff00u)==0xf900u && (op==1u || op==3u || op==9u || op==11u)) {
        *read=true;*size=(op&2u)?2u:1u;
    } else return false;
    /* Offset-register encodings reserve bits 11:6. The other scalar form
     * requires either pre-index or writeback. Reject undefined encodings. */
    if(!(op&8u) && ((second&0x0800u)?!(second&0x0500u):(second&0x0fc0u))) return false;
    return true;
}
int m8rt_native_boot_app_resume(m8rt *rt,int result) {
    uint32_t pc=rt->native.cpu.r[15],address=rt->native_boot_app.bus_address;
    unsigned size=rt->native_boot_app.bus_size;
    const m8_region *region=pc<rt->memory.itcm.size?&rt->memory.itcm:&rt->memory.flash;
    uint32_t offset=pc-region->base;
    const uint8_t *p=region->host && region->size>=4 && offset<=region->size-4?region->host+offset:NULL;
    uint32_t opcode=p?m8rt_le32(p):0;
    bool read=false;unsigned expected_size=0;
    const char *predicate=result!=1?"fault_result":!p?"code_pointer":
        !scalar_access((uint16_t)opcode,(uint16_t)(opcode>>16),&read,&expected_size)?"scalar_access":
        read!=rt->native_boot_app.bus_read?"access_direction":size!=expected_size?"access_size":NULL;
    if(predicate) {
        m8rt_fail(rt,"native boot SDHC resume predicate=%s result=%d pc=%08x opcode=%08x address=%08x size=%u read=%u expected_size=%u expected_read=%u",
            predicate,result,pc,opcode,address,size,rt->native_boot_app.bus_read,expected_size,read);return 1;
    }
    rt->native_boot_app.bus_pending=false;rt->native_boot_app.bus_executing=true;
    rt->aot_active=true;rt->aot_mask_cache=rt->aot_services?1u:0u;
#ifdef M8RT_INTERPRETER
    result=m8rt_native_step_dispatch(&rt->native.cpu,&rt->memory,&rt->native.arch);
#else
    result=rt->native.step(&rt->native.cpu,&rt->memory,&rt->native.arch);
#endif
    rt->native_boot_app.bus_executing=false;
    rt->aot_active=false;rt->aot_mask_cache=0;
    rt->native_boot_app.bus_steps++;
    rt->native.stats.fallback_steps++;
    rt->native.stats.fallback_guest+=rt->native.arch.last_executed;
    rt->native.stats.interpreter_guest+=rt->native.arch.last_special;
    if((result==0 && rt->native.arch.last_executed!=1) ||
       (result && !(result==1 && rt->native.boot_callback_irq))) {
        m8rt_fail(rt,"native boot SDHC step predicate=%s result=%d executed=%u pc=%08x next=%08x opcode=%08x address=%08x size=%u callback_irq=%u",
            result==0?"single_instruction":"callback_irq",result,rt->native.arch.last_executed,
            pc,rt->native.cpu.r[15],opcode,address,size,rt->native.boot_callback_irq);return 1;
    }
    return result;
}

int m8rt_native_boot_app_finish(m8rt *rt) {
    if(!rt->native_boot_app.ready) return 0;
    uint32_t value=0,pc=rt->native.cpu.r[15];
    const char *predicate=warm_boundary(rt,pc,&value);
    if(rt->in_emu || !rt->native.active || !rt->native.boot || predicate) {
        m8rt_fail(rt,"native boot app warm predicate=%s value=%u pc=%08x",
            rt->in_emu?"in_emu":!rt->native.active?"active":!rt->native.boot?"boot":predicate,
            rt->in_emu?1u:!rt->native.active?0u:!rt->native.boot?0u:value,pc);return -1;
    }
    const m8rt_native_boot_app_config *c=&rt->native_boot_app.config;
    uint32_t bad=0;
    if(c->warm.verify(&rt->memory,&bad)) {
        m8rt_fail(rt,"native boot app warm predicate=aot_source value=%08x",bad);return -1;
    }
    if(c->warm.region_verify(&rt->memory,&bad)) {
        m8rt_fail(rt,"native boot app warm predicate=region_source value=%08x",bad);return -1;
    }
    bool services,devices,events,deadline;
    if(warm_option(rt,"M8RT_NATIVE_SERVICE_CONTINUE",&services) ||
       warm_option(rt,"M8RT_NATIVE_DEVICE_DIRECT",&devices) ||
       warm_option(rt,"M8RT_NATIVE_EVENT_LOOP",&events) ||
       warm_option(rt,"M8RT_NATIVE_DEADLINE_YIELD",&deadline)) return -1;
    if(events && !c->warm.event_chain) {
        m8rt_fail(rt,"native boot app warm predicate=event_chain value=0");return -1;
    }
    m8_cpu cpu=rt->native.cpu;
    m8_native_arch arch=rt->native.arch;
    uint32_t cpsr=rt->native.cpsr;
#define INSTALL(name,call) do {int result=(call);if(result) {if(!rt->fatal) m8rt_fail(rt,"native boot app warm predicate=%s value=%d pc=%08x",name,result,pc);return -1;}} while(0)
    INSTALL("aot_install",m8rt_set_aot(rt,c->lookup,c->entries));
    INSTALL("aot_services",m8rt_set_aot_services(rt,c->services));
    INSTALL("region_install",m8rt_set_native_regions(rt,c->warm.region,c->warm.region_verify));
    if(c->warm.chain) INSTALL("chain_install",m8rt_set_native_region_chain(rt,c->warm.chain,c->warm.chain_cache_slots));
    if(events) INSTALL("event_install",m8rt_set_native_event_loop(rt,c->warm.event_chain));
#undef INSTALL
    rt->native.service_continue=services;
    rt->native.device_direct=devices;
    rt->native_live.deadline_yield=deadline;
    rt->native_live.deadline_seen=false;
    if(devices) m8rt_native_device_bus(rt,true);
    m8rt_native_boot_bus_install(rt);
    /* Preserve the cold fallback and callback IRQ owner. Only dispatch changes;
     * CPU, masks, FP state and the exclusive monitor remain in their owner.
     * Retain the warm single-step as a second fallback behind cold and
     * static coverage, so warm playback PCs use compiled code. */
    rt->native.warm_step=c->warm.step;
    if(memcmp(&cpu,&rt->native.cpu,sizeof cpu)) {
        const unsigned char *actual=(const unsigned char *)&rt->native.cpu;
        const unsigned char *expected=(const unsigned char *)&cpu;
        size_t offset=0;while(actual[offset]==expected[offset]) offset++;
        m8rt_fail(rt,"native boot app warm predicate=cpu_state byte=%zu value=%02x expected=%02x pc=%08x",offset,actual[offset],expected[offset],pc);return -1;
    }
    if(memcmp(&arch,&rt->native.arch,sizeof arch)) {
        const unsigned char *actual=(const unsigned char *)&rt->native.arch;
        const unsigned char *expected=(const unsigned char *)&arch;
        size_t offset=0;while(actual[offset]==expected[offset]) offset++;
        m8rt_fail(rt,"native boot app warm predicate=arch_state byte=%zu value=%02x expected=%02x pc=%08x",offset,actual[offset],expected[offset],pc);return -1;
    }
    if(cpsr!=rt->native.cpsr) {
        m8rt_fail(rt,"native boot app warm predicate=cpsr_state value=%08x expected=%08x pc=%08x",rt->native.cpsr,cpsr,pc);return -1;
    }
    rt->native_boot_app.pending=rt->native_boot_app.ready=false;
    rt->native_boot_app.stats.warm=1;
    return 1;
}
int m8rt_read_native_boot_app_stats(m8rt *rt,m8rt_native_boot_app_stats *out) {
    if(!rt || !out || rt->in_emu) return -1;
    *out=rt->native_boot_app.stats;return 0;
}
#else
int m8rt_native_boot_app_available(void) {return 0;}
int m8rt_set_native_boot_app(m8rt *rt,const m8rt_native_boot_app_config *config) {(void)rt;(void)config;return -1;}
int m8rt_measure_native_boot_app(m8rt *rt) {(void)rt;return -1;}
int m8rt_read_native_boot_app_stats(m8rt *rt,m8rt_native_boot_app_stats *out) {
    if(!rt || !out || rt->in_emu) return -1;
    memset(out,0,sizeof *out);return 0;
}
#endif
