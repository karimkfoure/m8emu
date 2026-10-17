#define _GNU_SOURCE
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include "native-services.h"
#ifdef M8RT_NATIVE_ENGINE
#include "mmio-state.h"
#endif
#ifndef M8RT_DIRECT_MEMORY_DEFAULT
#define M8RT_DIRECT_MEMORY_DEFAULT 1
#endif
/* Legacy external engines do not supply the layout guard. Keep their original
 * memory API path instead of enabling unchecked host-buffer copies. */
extern const uint64_t *uc_m8_memory_generation(uc_engine *)
    __attribute__((weak, visibility("default")));
#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#define USB_BASE 0x402e0000u
#define MAIN_PC 0x6005118cu
#define RESUME_PC 0x6005118eu
#define WAIT_PC 0x60005b98u
#define DELAY_PC 0x3205au
#define SAMPLING_POLL_PC 0x6003b6b0u
#define SCC_ADDR 0x2003a1ccu

uint64_t m8rt_now_ns(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_nsec;
}
void m8rt_fail(m8rt *rt,const char *fmt,...) {
    if (!rt || rt->fatal) return;
    va_list args; va_start(args,fmt); vsnprintf(rt->error,sizeof rt->error,fmt,args); va_end(args);
    if(rt->audit) m8rt_audit_access(rt,M8RT_AUDIT_FAULT,0,0,M8RT_AUDIT_NONE,0,0);
    rt->fatal=true; if(rt->in_emu) m8rt_engine_stop(rt);
}
int m8rt_queue_append(m8rt_queue *q,const void *data,size_t n) {
    if(!n) return 0;
    if(n > SIZE_MAX-q->len) return -1;
    if(q->head && q->len+n>q->cap) { memmove(q->data,q->data+q->head,q->len-q->head); q->len-=q->head; q->head=0; }
    if(q->len+n>q->cap) {
        size_t cap=q->cap?q->cap:4096;
        while(cap<q->len+n) { if(cap>SIZE_MAX/2) return -1; cap*=2; }
        void *p=realloc(q->data,cap); if(!p) return -1; q->data=p; q->cap=cap;
    }
    memcpy(q->data+q->len,data,n); q->len+=n; return 0;
}
size_t m8rt_queue_read(m8rt_queue *q,void *out,size_t n) {
    if(n>q->len-q->head) n=q->len-q->head;
    if(n) memcpy(out,q->data+q->head,n);
    q->head+=n; if(q->head==q->len) q->head=q->len=0; return n;
}
void m8rt_cdc_emit(m8rt *rt,const void *data,size_t n) {
    if(m8rt_queue_append(&rt->cdc,data,n)) m8rt_fail(rt,"CDC queue allocation failed");
    rt->cdc_total+=n;
}
void m8rt_pcm_emit(m8rt *rt,const void *data,size_t n) {
    uint8_t length[4]; m8rt_st32(length,(uint32_t)n);
    m8rt_audio_position position={rt->pcm_total/4,rt->guest_cycles,(uint32_t)(n/4),0};
    if(n>UINT32_MAX || m8rt_queue_append(&rt->pcm_lengths,length,4) || m8rt_queue_append(&rt->pcm_positions,&position,sizeof position) || m8rt_queue_append(&rt->pcm,data,n)) m8rt_fail(rt,"PCM queue allocation failed");
    rt->pcm_total+=n;
}
static bool direct_memory_valid(m8rt *rt) {
    if(!rt->direct_memory) return true;
    uint64_t observed=*rt->direct_memory_generation;
    if(observed==rt->direct_memory_expected_generation) return true;
    m8rt_fail(rt,"direct memory layout changed: expected_generation=%llu observed_generation=%llu; remapping, protection changes and memory snapshots require M8RT_DIRECT_MEMORY=0 before creation",
        (unsigned long long)rt->direct_memory_expected_generation,(unsigned long long)observed);
    return false;
}
static uint8_t *data_ram_pointer(m8rt *rt,uint32_t addr,size_t n) {
    m8_region *regions[]={&rt->memory.dtcm,&rt->memory.ocram};
    for(unsigned i=0;i<2;i++) {
        m8_region *r=regions[i];uint32_t off=addr-r->base;
        if(r->host && addr>=r->base && n<=r->size && off<=r->size-n) return r->host+off;
    }
    return NULL;
}
static uint8_t *mapped_read_pointer(m8rt *rt,uint32_t addr,size_t n) {
    uint8_t *p=data_ram_pointer(rt,addr,n);if(p) return p;
    m8_region *regions[]={&rt->memory.itcm,&rt->memory.flash};
    for(unsigned i=0;i<2;i++) {
        m8_region *r=regions[i];uint32_t off=addr-r->base;
        if(r->host && addr>=r->base && n<=r->size && off<=r->size-n) return r->host+off;
    }
    return NULL;
}
int m8rt_mem_read(m8rt *rt,uint32_t addr,void *out,size_t n) {
    if(rt->dispatch_stats_enabled) {
        ++rt->dispatch_stats.memory_reads;
        if(data_ram_pointer(rt,addr,n)) ++rt->dispatch_stats.ram_reads;
    }
    if(rt->direct_memory && n) {
        if(!direct_memory_valid(rt)) return UC_ERR_MAP;
        uint8_t *p=mapped_read_pointer(rt,addr,n);
        if(p) { memcpy(out,p,n);return UC_ERR_OK; }
    }
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active) {
        if(addr>=0x200000u && n<=128*1024u && addr-0x200000u<=128*1024u-n) {memcpy(out,rt->rom+addr-0x200000u,n);return 0;}
        if(addr>=0xe0001000u && n<=4096 && addr-0xe0001000u<=4096-n) {memcpy(out,rt->dwt+addr-0xe0001000u,n);return 0;}
        m8rt_fail(rt,"native unmapped read address=%08x size=%zu pc=%08x",addr,n,rt->native.cpu.r[15]);return UC_ERR_READ_UNMAPPED;
    }
#endif
    return (int)uc_mem_read(rt->uc,addr,out,n);
}
int m8rt_mem_write(m8rt *rt,uint32_t addr,const void *in,size_t n) {
#ifdef M8RT_NATIVE_ENGINE
    if(n && rt->native_live.enabled && !rt->native.active &&
       (addr<rt->memory.itcm.size || (addr>=rt->memory.flash.base && addr<rt->memory.flash.base+rt->memory.flash.size)))
        m8rt_native_live_code_changed(rt);
    if(n && !rt->in_emu && rt->native_live.enabled &&
       (addr<rt->memory.itcm.size || (addr>=rt->memory.flash.base && addr<rt->memory.flash.base+rt->memory.flash.size)) &&
       m8rt_native_live_host_code_write(rt)) return UC_ERR_WRITE_PROT;
#endif
    if(rt->dispatch_stats_enabled) {
        ++rt->dispatch_stats.memory_writes;
        if(data_ram_pointer(rt,addr,n)) ++rt->dispatch_stats.ram_writes;
    }
    if(rt->direct_memory && n) {
        if(!direct_memory_valid(rt)) return UC_ERR_MAP;
        uint8_t *p=data_ram_pointer(rt,addr,n);
        if(p) { memcpy(p,in,n);return UC_ERR_OK; }
    }
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active) {
        uint8_t *code=mapped_read_pointer(rt,addr,n);
        if(rt->native.boot && addr<rt->memory.itcm.size)
            return m8rt_native_boot_copy(rt,addr,in,n);
        if(rt->native.region && rt->aot_active && code) {
            /* Hand off before this store. TCG may retain a decoded following
             * instruction until a barrier. A post-store cache flush changes
             * that ordering. The region publishes its completed prefix. */
            rt->native.code_invalidated=true;
            rt->native.stats.code_invalidations++;
            m8rt_native_reset_lookup(rt);
            return UC_ERR_WRITE_PROT;
        }
        m8rt_fail(rt,"native unsupported write address=%08x size=%zu pc=%08x",addr,n,rt->native.cpu.r[15]);return UC_ERR_WRITE_UNMAPPED;
    }
#endif
    int err=(int)uc_mem_write(rt->uc,addr,in,n);
    if(!err && (addr<rt->memory.itcm.size || (addr>=rt->memory.flash.base && addr<rt->memory.flash.base+rt->memory.flash.size))) {
        err=(int)uc_ctl_remove_cache(rt->uc,addr,(uint64_t)addr+n);
    }
    return err;
}
uint8_t *m8rt_memory_offset(m8rt *rt) { return rt ? rt->memory_offset : NULL; }
int m8rt_callback_timing_enabled(const m8rt *rt) { return rt ? M8RT_CALLBACK_TIMING_DEFAULT : 0; }
int m8rt_native_device_fast_compiled(void) {
#if defined(M8RT_NATIVE_ENGINE) && defined(M8RT_NATIVE_DEVICE_FAST) && M8RT_NATIVE_DEVICE_FAST
    return 1;
#else
    return 0;
#endif
}
uint32_t m8rt_reg_read(m8rt *rt,int reg) {
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active) return m8rt_native_reg_read(rt,reg);
#endif
    uint32_t v=0; uc_reg_read(rt->uc,reg,&v); return v;
}
static void reg_write(m8rt *rt,int reg,uint32_t v) {
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active) {m8rt_native_reg_write(rt,reg,v);return;}
#endif
    uc_reg_write(rt->uc,reg,&v);
}
uint32_t m8rt_read32(m8rt *rt,uint32_t addr) {
    uint8_t b[4]={0}; if(m8rt_mem_read(rt,addr,b,4)) return 0; return m8rt_le32(b);
}
void m8rt_write32(m8rt *rt,uint32_t addr,uint32_t value) {
    uint8_t b[4]; m8rt_st32(b,value); m8rt_mem_write(rt,addr,b,4);
}
bool m8rt_irq_enabled(m8rt *rt,unsigned irq) { return irq<256 && (rt->nvic_iser[irq>>5]&(1u<<(irq&31))); }
static bool masked(m8rt *rt) { return m8rt_reg_read(rt,UC_ARM_REG_CPSR)&0x80; }
void m8rt_reconcile(m8rt *rt) {
    uint32_t ipsr=m8rt_reg_read(rt,UC_ARM_REG_IPSR)&0x1ff;
    if(!ipsr) { rt->irq_nest=0; memset(rt->exc_stack,0,sizeof rt->exc_stack); }
    else if(!rt->irq_nest) { rt->irq_nest=1; rt->exc_stack[0]=ipsr; }
}
bool m8rt_take_irq(m8rt *rt,unsigned exc) {
    if(!rt->vtor || rt->irq_nest>=32) return false;
    if(rt->audit && !(exc==15 || (exc>=16 && exc<32) || exc==86 || exc==108 || exc==126 || exc==129))
        m8rt_audit_access(rt,M8RT_AUDIT_UNEXPECTED_IRQ,exc,0,M8RT_AUDIT_NONE,2,0);
    uint32_t handler=m8rt_read32(rt,rt->vtor+exc*4);
    if(handler<2) return false;
    if(exc>=16 && exc<272) {
        unsigned bank=(exc-16)>>5;
        m8rt_nvic_pending(rt,bank,rt->nvic_ispr[bank]&~(1u<<((exc-16)&31)));
    }
    int ids[]={UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R12,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_CPSR};
    uint32_t values[8]; void *ptrs[8]; for(unsigned i=0;i<8;i++) ptrs[i]=&values[i];
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active) {for(unsigned i=0;i<8;i++) values[i]=m8rt_reg_read(rt,ids[i]);}
    else
#endif
    uc_reg_read_batch(rt->uc,ids,ptrs,8);
    values[7]|=0x01000000;
    uint32_t sp=(m8rt_reg_read(rt,UC_ARM_REG_SP)&~3u)-32;
    uint8_t frame[32]; for(unsigned i=0;i<8;i++) m8rt_st32(frame+4*i,values[i]);
    if(m8rt_mem_write(rt,sp,frame,32)) return false;
    reg_write(rt,UC_ARM_REG_SP,sp); reg_write(rt,UC_ARM_REG_LR,0xfffffff9);
    reg_write(rt,UC_ARM_REG_CPSR,values[7]); reg_write(rt,UC_ARM_REG_PC,handler&~1u);
    reg_write(rt,UC_ARM_REG_XPSR,m8rt_reg_read(rt,UC_ARM_REG_XPSR)|0x01000000);
    reg_write(rt,UC_ARM_REG_IPSR,exc);
    rt->exc_stack[rt->irq_nest++]=exc;
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active && rt->native.event_loop) rt->native.event_stats.irq_entries++;
#endif
    uint8_t event[4]; m8rt_st32(event,exc); m8rt_queue_append(&rt->irqs,event,4);
    if(rt->in_emu) m8rt_engine_stop(rt);
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active && rt->native.boot && rt->aot_active) {
        /* The device callback keeps running after software exception entry.
         * Generated code must not retire its access or overwrite this CPU. */
        rt->native.boot_irq_cpu=rt->native.cpu;
        rt->native.boot_irq_arch=rt->native.arch;
        rt->native.boot_callback_irq=true;
        rt->native.boot_callback_irqs++;
    }
#endif
    return true;
}
static bool sdhc_take(m8rt *rt) {
    if(rt->sdhc_in_sync || rt->irq_nest || masked(rt) || !rt->vtor || !m8rt_irq_enabled(rt,110) || !(rt->nvic_ispr[3]&(1u<<14))) return false;
    rt->sdhc_in_sync=true; bool took=m8rt_take_irq(rt,126); rt->sdhc_in_sync=false; return took;
}
void m8rt_pend(m8rt *rt,unsigned irq) {
    if(rt->audit && !(irq<16 || irq==70 || irq==92 || irq==110 || irq==113))
        m8rt_audit_access(rt,M8RT_AUDIT_UNEXPECTED_IRQ,(uint64_t)irq+16,0,M8RT_AUDIT_NONE,1,0);
    if(irq>=256) return;
#ifdef M8RT_NATIVE_ENGINE
    /* Re-pending an already set bit still advances its sequence and can
     * require service after this instruction. Do not lose it in a bit test. */
    if(rt->native.active && rt->native.service_continue) rt->native.service_epoch++;
#endif
    m8rt_nvic_pending(rt,irq>>5,rt->nvic_ispr[irq>>5]|(1u<<(irq&31))); rt->nvic_pend_seq[irq]++;
    if(irq==110) {
#ifdef M8RT_NATIVE_ENGINE
        if(rt->native.active && rt->native.boot) sdhc_take(rt);
        else
#endif
        if(rt->aot_active) rt->aot_sdhc_pending=true; else sdhc_take(rt);
    }
}
static void sdhc_drain(m8rt *rt) {
    rt->nvic_isr_done[110]=rt->nvic_pend_seq[110];
    if(rt->nvic_pend_seq[110]>rt->nvic_isr_done[110]) sdhc_take(rt);
}
static bool unstack(m8rt *rt) {
    if(!(m8rt_reg_read(rt,UC_ARM_REG_IPSR)&0x1ff) && !rt->irq_nest) return false;
    uint32_t sp=m8rt_reg_read(rt,UC_ARM_REG_SP)&~3u;
    uint8_t frame[32]; if(m8rt_mem_read(rt,sp,frame,32)) return false;
    uint32_t values[8]; for(unsigned i=0;i<8;i++) values[i]=m8rt_le32(frame+4*i);
    if(values[6]<2 || values[6]>=0xf0000000u) return false;
    int ids[]={UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R12,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_CPSR};
    void *ptrs[8]; values[6]&=~1u; values[7]|=0x01000000;
    for(unsigned i=0;i<8;i++) ptrs[i]=&values[i];
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active) {for(unsigned i=0;i<8;i++) reg_write(rt,ids[i],values[i]);}
    else
#endif
    uc_reg_write_batch(rt->uc,ids,ptrs,8);
    reg_write(rt,UC_ARM_REG_SP,sp+32);
    if(rt->irq_nest) rt->irq_nest--;
    reg_write(rt,UC_ARM_REG_IPSR,rt->irq_nest?rt->exc_stack[rt->irq_nest-1]:0);
    sdhc_drain(rt); return true;
}
static void deliver_usb(m8rt *rt) {
    m8rt_reconcile(rt);
    if(!rt->irq_nest && !masked(rt) && rt->usb_pending && m8rt_irq_enabled(rt,113)) { rt->usb_pending=false; m8rt_take_irq(rt,129); }
}
static bool take_exc(m8rt *rt,unsigned exc) {
    m8rt_reconcile(rt); if(rt->irq_nest || masked(rt)) return false;
    return m8rt_take_irq(rt,exc);
}
static void deliver_watchdog(m8rt *rt) {
    if(m8rt_watchdog_irq(rt) && m8rt_irq_enabled(rt,92)) take_exc(rt,108);
}
static void deliver_systick(m8rt *rt) {
    m8rt_reconcile(rt);
    if(rt->systick_pending && !rt->irq_nest && !masked(rt) && (rt->syst_csr&3)==3 && rt->vtor && m8rt_read32(rt,rt->vtor+60)>=2) {
        rt->systick_pending=false; m8rt_take_irq(rt,15);
    }
}
static void maybe_r(m8rt *rt) {
    if(rt->options.host_handshake && m8rt_usb_configured(rt) && !rt->er_sent && !m8rt_usb_queued(rt)) { m8rt_usb_queue(rt,"R",1); rt->er_sent=true; }
}
static bool due(uint32_t now,uint32_t deadline) { return (uint32_t)(now-deadline)<0x80000000u; }
static void pump(m8rt *rt,uint32_t quantum) {
    rt->cyccnt+=quantum; rt->guest_cycles+=quantum; m8rt_st32(rt->dwt+4,rt->cyccnt);
    m8rt_watchdog_tick(rt);
    if(rt->watchdog.reset_pending) return;
    while(due(rt->cyccnt,rt->next_systick)) { rt->next_systick+=600000; rt->systick_pending=true; }
    while(due(rt->cyccnt,rt->next_sof)) { rt->next_sof+=600000; m8rt_usb_sof(rt); }
    while(due(rt->cyccnt,rt->next_audio)) rt->next_audio+=1741560;
    if(rt->audit) m8rt_audit_time(rt);
    m8rt_usb_poll(rt,rt->cyccnt);
    m8rt_sai_service(rt);
    int ch=m8rt_sai_channel(rt);
    rt->audio_pending=ch>=0 && m8rt_irq_enabled(rt,(unsigned)ch) && (rt->nvic_ispr[ch>>5]&(1u<<(ch&31)));
    if(ch>=0) rt->audio_ch=ch;
}
static uint32_t main_quantum(m8rt *rt) {
    if(rt->options.reference_policy || rt->irq_nest || rt->systick_pending || rt->audio_pending || rt->usb_pending) return 30000;
    uint32_t quantum=150000;
    for(unsigned bank=0;bank<8;bank++) {
        if(rt->nvic_ispr[bank]) return 30000;
        uint32_t known=bank==0?0xffffu:bank==2?(1u<<6)|(1u<<0)|(1u<<28):bank==3?(1u<<14)|(1u<<17):0;
        if(rt->nvic_iser[bank]&~known) return 30000;
    }
    for(unsigned ch=0;ch<4;ch++) if(m8rt_raw_read(rt,0x40084108+16*ch)&1) return 30000;
    uint32_t distances[]={rt->next_systick-rt->cyccnt,rt->next_sof-rt->cyccnt,rt->next_audio-rt->cyccnt,m8rt_usb_deadline(rt),m8rt_sai_deadline(rt)};
    for(unsigned i=0;i<5;i++) { uint32_t d=distances[i]; if(!d || (d>=0x80000000u && d!=UINT32_MAX)) return 30000; if(d<quantum) quantum=d; }
    return quantum;
}
/* Only a proven clock-hook-free stall installs an instruction callback.
 * Normal execution keeps Unicorn count/timeout and global hooks disabled. */
#define WATCHDOG_STALL_INSTRUCTIONS 60000u
static void guest_count_discard(m8rt *rt,uint32_t pc);
static void watchdog_count_stall(uc_engine *uc,uint64_t pc,uint32_t size,void *user) {
    (void)uc;
    (void)size;m8rt *rt=user;
    if(m8rt_reg_read(rt,UC_ARM_REG_PC)!=(uint32_t)pc) return;
    if(rt->watchdog_slice_instructions==WATCHDOG_STALL_INSTRUCTIONS) {
        /* Stop before this instruction. All counted entries have completed. */
        rt->watchdog_counted_stop=true;
        guest_count_discard(rt,(uint32_t)pc);m8rt_engine_stop(rt);return;
    }
    rt->watchdog_slice_instructions++;
}
static int watchdog_counting(m8rt *rt,bool enabled) {
    if(enabled==(rt->watchdog_stall_hook!=0)) return 0;
    uc_err err;
    if(enabled) err=uc_hook_add(rt->uc,&rt->watchdog_stall_hook,UC_HOOK_CODE,watchdog_count_stall,rt,1,0);
    else { err=uc_hook_del(rt->uc,rt->watchdog_stall_hook);rt->watchdog_stall_hook=0; }
    /* Warm blocks do not acquire a new hook. Removing it also discards its
     * translated callbacks before ordinary firmware execution resumes. */
    if(!err) err=uc_ctl_flush_tb(rt->uc);
    if(err) { m8rt_fail(rt,"watchdog stalled counter %s: %s",enabled?"enable":"disable",uc_strerror(err));return -1; }
    return 0;
}
static bool watchdog_self_branch(m8rt *rt,uint32_t pc) {
    const m8_region *r=pc<rt->memory.itcm.size?&rt->memory.itcm:
        pc>=rt->memory.flash.base && (uint64_t)pc+2<=rt->memory.flash.base+rt->memory.flash.size?&rt->memory.flash:NULL;
    return r && (uint64_t)pc+2<=r->base+r->size &&
        m8rt_le16((const uint8_t *)r->host+(pc-r->base))==UINT16_C(0xe7fe);
}
static void watchdog_fallback(m8rt *rt,uint32_t instructions,bool idle) {
    rt->watchdog.fallback_slots+=instructions;rt->watchdog.fallback_cycles+=instructions;
    if(idle) rt->watchdog.idle_batches++;
    pump(rt,instructions);
    if(rt->watchdog.reset_pending) return;
    deliver_watchdog(rt);deliver_systick(rt);
    if(rt->audio_pending && take_exc(rt,(unsigned)(rt->audio_ch>=0?rt->audio_ch:0)+16)) rt->audio_pending=false;
    if((rt->nvic_ispr[2]&(1u<<6)) && m8rt_irq_enabled(rt,70)) take_exc(rt,86);
    deliver_usb(rt);maybe_r(rt);
}
static bool should_stop(m8rt *rt) {
    if(atomic_load_explicit(&rt->host_stop,memory_order_relaxed)) return true;
    if(!rt->deadline_ns || m8rt_now_ns()<rt->deadline_ns) return false;
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active && rt->native_live.deadline_yield) rt->native_live.deadline_seen=true;
#endif
    return true;
}
/* Earlier runtime hooks can redirect or stop an instruction. Check live PC
 * before counting; hooks installed afterward discard a replaced entry. */
static void guest_count_hook(uc_engine *uc,uint64_t pc,uint32_t size,void *user) {
    (void)uc;(void)size;m8rt *rt=user;
    if(m8rt_reg_read(rt,UC_ARM_REG_PC)!=(uint32_t)pc) return;
    rt->jit_guest_instructions++;
    rt->guest_count_last_pc=(uint32_t)pc;rt->guest_count_last_valid=true;
#ifdef M8RT_GUEST_PC_PROFILE
    if(rt->guest_pc_enabled) {
        size_t slot=((uint32_t)(pc>>1)*UINT32_C(2654435761))&65535u;
        size_t probes=0;
        while(rt->guest_pcs[slot].pc!=UINT32_MAX && rt->guest_pcs[slot].pc!=(uint32_t)pc) {
            slot=(slot+1)&65535u;
            if(++probes==65536) { m8rt_fail(rt,"guest PC profile capacity exhausted pc=%08x probes=%zu cap=65536",(uint32_t)pc,probes);return; }
        }
        m8rt_guest_pc *entry=&rt->guest_pcs[slot];
        if(!entry->instructions) {entry->pc=(uint32_t)pc;rt->guest_pc_used++;}
        entry->instructions++;rt->guest_pc_last=entry;
    }
#endif
}
static void guest_count_discard(m8rt *rt,uint32_t pc) {
    if(rt->guest_count_enabled && rt->guest_count_last_valid && rt->guest_count_last_pc==pc) {
        rt->jit_guest_instructions--;rt->guest_count_last_valid=false;
#ifdef M8RT_GUEST_PC_PROFILE
        if(rt->guest_pc_enabled && rt->guest_pc_last) {
            if(!--rt->guest_pc_last->instructions) rt->guest_pc_used--;
            rt->guest_pc_last=NULL;
        }
#endif
    }
}
int m8rt_set_guest_count(m8rt *rt,int enabled) {
    if(!rt || rt->in_emu || (enabled!=0 && enabled!=1)) return -1;
    if(rt->guest_count_enabled) {
        uc_err err=uc_hook_del(rt->uc,rt->guest_count_hook);
        if(err) return -1;
    }
    rt->guest_count_enabled=false;rt->guest_count_last_valid=false;
    if(!enabled) return 0;
    uc_err err=uc_hook_add(rt->uc,&rt->guest_count_hook,UC_HOOK_CODE,guest_count_hook,rt,1,0);
    if(err) return -1;
    /* Existing blocks do not gain callbacks when a hook is appended. Project
     * loading has already warmed DSP blocks, so force their retranslation. */
    err=uc_ctl_flush_tb(rt->uc);
    if(err) {uc_hook_del(rt->uc,rt->guest_count_hook);return -1;}
    rt->guest_count_enabled=true;rt->guest_count_before_aot=false;
    rt->jit_guest_instructions=0;
    return 0;
}
#ifdef M8RT_GUEST_PC_PROFILE
int m8rt_set_guest_pc_profile(m8rt *rt,int enabled) {
    if(!rt || rt->in_emu || (enabled!=0 && enabled!=1)) return -1;
    if(enabled) {
        if(!rt->guest_pcs) rt->guest_pcs=calloc(65536,sizeof(*rt->guest_pcs));
        if(!rt->guest_pcs) return -1;
        memset(rt->guest_pcs,0,65536*sizeof(*rt->guest_pcs));rt->guest_pc_used=0;
        for(size_t i=0;i<65536;i++) rt->guest_pcs[i].pc=UINT32_MAX;
    }
    if(m8rt_set_guest_count(rt,enabled)) return -1;
    rt->guest_pc_enabled=enabled;rt->guest_pc_last=NULL;
    return 0;
}
size_t m8rt_read_guest_pc_profile(m8rt *rt,m8rt_guest_pc *out,size_t count) {
    if(!rt || rt->in_emu) return 0;
    if(!out) return rt->guest_pc_used;
    size_t n=0;
    for(size_t i=0;i<65536 && n<count;i++) if(rt->guest_pcs && rt->guest_pcs[i].instructions) out[n++]=rt->guest_pcs[i];
    return n;
}
#endif
static uint64_t limit_frames(m8rt *rt) { return (rt->audio_limit_local?rt->local_total:rt->pcm_total)/4; }
static void main_hook(uc_engine *uc,uint64_t address,uint32_t size,void *user) {
    (void)uc;
    (void)size; m8rt *rt=user;
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native_boot_app.measuring) m8rt_native_boot_app_measure_entry(rt,(uint32_t)address);
    if(m8rt_native_live_entry(rt,(uint32_t)address)) return;
#endif
    uint64_t start=m8rt_callback_start(false);
    if(address==RESUME_PC) rt->entry_pending=false;
    else if(address==MAIN_PC && !rt->entry_pending) {
        rt->entry_pending=true; rt->main_iters++;
        if(rt->audit) m8rt_audit_main(rt);
        if(rt->main_iters-rt->boot_main_start>=1000) m8rt_usb_set_boot(rt);
        uint32_t quantum=main_quantum(rt);
        rt->main_cycles+=quantum;
        if(!rt->options.reference_policy && quantum!=30000) { rt->main_selected++; if(quantum>30000) rt->main_expanded++; }
        pump(rt,quantum);
    }
    if(rt->watchdog.reset_pending) return;
    deliver_watchdog(rt); deliver_systick(rt);
    if(rt->audio_pending && take_exc(rt,(unsigned)(rt->audio_ch>=0?rt->audio_ch:0)+16)) rt->audio_pending=false;
    if((rt->nvic_ispr[2]&(1u<<6)) && m8rt_irq_enabled(rt,70)) take_exc(rt,86);
    deliver_usb(rt); maybe_r(rt);
    if((rt->stop_iters && rt->main_iters>=rt->stop_iters) || (rt->stop_frames && limit_frames(rt)>=rt->stop_frames) || should_stop(rt) || (rt->yield_boot && m8rt_usb_boot(rt))) m8rt_engine_stop(rt);
    m8rt_callback_finish(rt,start,false);
}
static void wait_hook(uc_engine *uc,uint64_t address,uint32_t size,void *user) {
    (void)uc;
    (void)address;(void)size; m8rt *rt=user; uint64_t start=m8rt_callback_start(false);
    pump(rt,60000);
    if(rt->watchdog.reset_pending) return;
    deliver_watchdog(rt); deliver_systick(rt); deliver_usb(rt); maybe_r(rt);
    if(should_stop(rt)) m8rt_engine_stop(rt);
    m8rt_callback_finish(rt,start,false);
}
static void sampling_hook(uc_engine *uc,uint64_t address,uint32_t size,void *user) {
    (void)uc;
    (void)size; m8rt *rt=user;
    if(address==SAMPLING_POLL_PC+4) { rt->sampling_pending=false; return; }
    if(!m8rt_usb_audio_input_enabled(rt)) return;
    uint64_t start=m8rt_callback_start(false);
    uint32_t opcode=m8rt_le32(rt->memory.flash.host+SAMPLING_POLL_PC-0x60000000u);
    if(opcode!=0x303df894u) { m8rt_fail(rt,"sampling poll opcode=%08x expected=303df894 pc=%08x",opcode,SAMPLING_POLL_PC);return; }
    opcode=m8rt_le32(rt->memory.flash.host+SAMPLING_POLL_PC+4-0x60000000u);
    if(opcode!=0x3a90ee0cu) { m8rt_fail(rt,"sampling resume opcode=%08x expected=3a90ee0c pc=%08x",opcode,SAMPLING_POLL_PC+4);return; }
    /* The original synchronous USB recording loop never returns to MAIN_PC
     * and has no delay call. Service the existing WAIT quantum at its outer
     * poll, after the prior iteration restored IRQ70. Never charge a re-entry
     * after an injected IRQ before the original first instruction executes. */
    if(!rt->sampling_pending) { rt->sampling_pending=true; pump(rt,60000); }
    if(rt->watchdog.reset_pending) return;
    deliver_watchdog(rt); deliver_systick(rt);
    if(rt->audio_pending && take_exc(rt,(unsigned)(rt->audio_ch>=0?rt->audio_ch:0)+16)) rt->audio_pending=false;
    if((rt->nvic_ispr[2]&(1u<<6)) && m8rt_irq_enabled(rt,70)) take_exc(rt,86);
    deliver_usb(rt); maybe_r(rt);
    if(should_stop(rt)) m8rt_engine_stop(rt);
    m8rt_callback_finish(rt,start,false);
}
static void delay_hook(uc_engine *uc,uint64_t address,uint32_t size,void *user) {
    (void)uc;
    (void)address;(void)size; m8rt *rt=user; uint64_t start=m8rt_callback_start(false);
    pump(rt,6000);
    if(rt->watchdog.reset_pending) return;
    deliver_watchdog(rt); deliver_systick(rt); deliver_usb(rt); m8rt_write32(rt,SCC_ADDR,rt->cyccnt);
    if(should_stop(rt)) m8rt_engine_stop(rt);
    m8rt_callback_finish(rt,start,false);
}
static void intr_hook(uc_engine *uc,uint32_t intno,void *user) {
    (void)uc;
    m8rt *rt=user; uint32_t pc=m8rt_reg_read(rt,UC_ARM_REG_PC);
    if(intno==8 || ((intno==3 || intno==5 || intno==7) && pc>=0xf0000000u)) { if(!unstack(rt)) m8rt_fail(rt,"exception return failed pc=%08x interrupt=%u",pc,intno); m8rt_engine_stop(rt); }
    else if(intno==17) {
        if(rt->audit) m8rt_audit_access(rt,M8RT_AUDIT_FAULT,pc,0,M8RT_AUDIT_NONE,intno,0);
        if(rt->options.strict) m8rt_fail(rt,"NOCP/FPU pc=%08x",pc);
        else { uint8_t b[2]; m8rt_mem_read(rt,pc&~1u,b,2); uint16_t h=m8rt_le16(b); reg_write(rt,UC_ARM_REG_PC,(pc+(((h&0xe000)==0xe000 && (h&0x1800))?4:2))&~1u); }
    } else if(rt->audit) m8rt_audit_access(rt,M8RT_AUDIT_FAULT,pc,0,M8RT_AUDIT_NONE,intno,0);
}
static bool unmapped_hook(uc_engine *uc,uc_mem_type type,uint64_t addr,int size,int64_t value,void *user) {
    m8rt *rt=user;
    if(rt->audit) {
        unsigned direction=type==UC_MEM_WRITE_UNMAPPED?M8RT_AUDIT_WRITE:
                           type==UC_MEM_FETCH_UNMAPPED?M8RT_AUDIT_FETCH:M8RT_AUDIT_READ;
        m8rt_audit_access(rt,M8RT_AUDIT_UNMAPPED,addr,(unsigned)size,direction,(unsigned)type,(uint64_t)value);
    }
    if(rt->options.strict) { m8rt_fail(rt,"unmapped access=%u address=%llx pc=%08x",type,(unsigned long long)addr,m8rt_reg_read(rt,UC_ARM_REG_PC)); return false; }
    if(rt->direct_memory) {
        fprintf(stderr,"m8rt: direct memory disabled by non-strict unmapped access: type=%u address=%llx size=%d pc=%08x; native admission unavailable\n",
            type,(unsigned long long)addr,size,m8rt_reg_read(rt,UC_ARM_REG_PC));
    }
    rt->direct_memory=false; /* This runtime-owned map needs the original API. */
    return uc_mem_map(uc,addr&~(uint64_t)4095,4096,UC_PROT_READ|UC_PROT_WRITE)==UC_ERR_OK;
}
/* A stop inside MMIO abandons the instruction. Skip all effects first.
 * Multiple-register accesses must finish before a stop is allowed. */
static bool safe_mmio_stop(m8rt *rt) {
    if(!should_stop(rt)) return false;
    uint32_t pc=m8rt_reg_read(rt,UC_ARM_REG_PC); uint8_t b[2];
    if(m8rt_mem_read(rt,pc&~1u,b,2)) return false;
    uint16_t h=m8rt_le16(b);
    if((h&0xf000)==0xc000 || (h&0xf600)==0xb400 || (h&0xfe00)==0xe800 || (h&0xfe00)==0xec00) return false;
    guest_count_discard(rt,pc);m8rt_engine_stop(rt); return true;
}
static void audit_mmio(m8rt *rt,uint32_t addr,unsigned size,bool write,uint32_t value) {
    if(!rt->audit || !(rt->in_emu || rt->aot_active)) return;
    if((addr>=USB_BASE && addr<USB_BASE+0x200) || m8rt_sd_handles(addr) || m8rt_mmio_modeled(addr,write)) return;
    if(size<4) value&=(1u<<(size*8))-1u;
    m8rt_audit_access(rt,M8RT_AUDIT_GENERIC_MMIO,addr,size,
                      write?M8RT_AUDIT_WRITE:M8RT_AUDIT_READ,0,value);
}
enum { DEVICE_GENERIC,DEVICE_WATCHDOG,DEVICE_USB,DEVICE_SD };
static unsigned device_route(uint32_t addr) {
    /* Only USB and watchdog need a sub-page boundary check. Both SDHC
     * controller windows occupy complete 16 KiB pages. */
    static const uint8_t routes[256]={
        [0x2e]=DEVICE_WATCHDOG,[0xb0]=DEVICE_SD,[0xb1]=DEVICE_SD,[0xb8]=DEVICE_USB
    };
    uint32_t offset=addr-UINT32_C(0x40000000);
    if(offset>=UINT32_C(0x400000)) return DEVICE_GENERIC;
    unsigned route=routes[offset>>14];
    if(route==DEVICE_WATCHDOG && addr-UINT32_C(0x400b8000)>=10) return DEVICE_GENERIC;
    if(route==DEVICE_USB && addr-USB_BASE>=0x200) return DEVICE_GENERIC;
    return route;
}
static uint32_t device_read(m8rt *rt,uint32_t addr,unsigned size) {
    switch(device_route(addr)) {
    case DEVICE_WATCHDOG: return m8rt_watchdog_read(rt,addr,size);
    case DEVICE_USB: return m8rt_usb_read(rt,addr);
    case DEVICE_SD:
        if(m8rt_raw_read(rt,0x400e800c)) m8rt_sai_service(rt);
        return m8rt_sd_read(rt,addr);
    default: return m8rt_mmio_read(rt,addr);
    }
}
static void device_write(m8rt *rt,uint32_t addr,unsigned size,uint32_t value) {
    switch(device_route(addr)) {
    case DEVICE_WATCHDOG: m8rt_watchdog_write(rt,addr,size,value);return;
    case DEVICE_USB: m8rt_usb_write(rt,addr,value);return;
    case DEVICE_SD: {
        m8rt_sd_write(rt,addr,value);
        unsigned off=addr&0xffc;
        if(off!=0x30 && off!=0x34 && off!=0x38) m8rt_sai_service(rt);
        return;
    }
    default: m8rt_mmio_write(rt,addr,value);return;
    }
}
static uint64_t mmio_read_cb(uc_engine *uc,uint64_t offset,unsigned size,void *user) {
    (void)uc;(void)size; m8rt *rt=user; uint64_t start=m8rt_callback_start(false);
    uint32_t v=0;
    if(!safe_mmio_stop(rt)) {
        M8_ACCESS_RECORD(rt,(uint32_t)offset,size,1,0);
        v=device_read(rt,(uint32_t)offset,size);
        if(rt->audit) audit_mmio(rt,(uint32_t)offset,size,false,v);
    }
    m8rt_callback_finish(rt,start,false); return v;
}
static void mmio_write_cb(uc_engine *uc,uint64_t offset,unsigned size,uint64_t value,void *user) {
    (void)uc;(void)size; m8rt *rt=user; uint64_t start=m8rt_callback_start(false);
    if(!safe_mmio_stop(rt)) {
        M8_ACCESS_RECORD(rt,(uint32_t)offset,size,1,1);
        if(rt->audit) audit_mmio(rt,(uint32_t)offset,size,true,(uint32_t)value);
        device_write(rt,(uint32_t)offset,size,(uint32_t)value);
    }
    m8rt_callback_finish(rt,start,false);
}
typedef struct { m8rt *rt; uint32_t base; } mmio_region;
/* Use one context per mapped region, owned with the runtime. */
static uint64_t mapped_read(uc_engine *uc,uint64_t off,unsigned size,void *user) { mmio_region *r=user; return mmio_read_cb(uc,r->base+off,size,r->rt); }
static void mapped_write(uc_engine *uc,uint64_t off,unsigned size,uint64_t v,void *user) { mmio_region *r=user; mmio_write_cb(uc,r->base+off,size,v,r->rt); }
enum { AOT_MASK_UNCACHED,AOT_MASK_UNREAD,AOT_MASK_SAFE,AOT_MASK_UNSAFE };
static bool aot_nvic_admit(m8rt *rt,uint32_t addr,uint32_t size,uint32_t *cpsr) {
    if(!rt->aot_active || !(rt->aot_services & M8_AOT_SERVICE_NVIC) || size!=4 || (addr&3)) return false;
    if((uint32_t)(addr-UINT32_C(0xe000e100))>28 &&
       (uint32_t)(addr-UINT32_C(0xe000e180))>28) return false;
    if(rt->aot_mask_cache==AOT_MASK_SAFE) {
        *cpsr=rt->aot_mask_cpsr;
        return true;
    }
    if(rt->aot_mask_cache==AOT_MASK_UNSAFE) return false;
    /* Capture masks before the ordinary access. Current IRQ delivery uses
     * CPSR.I and forbids preemption. PRIMASK and BASEPRI have separate Unicorn
     * state; exit for those masks rather than infer a new priority contract. */
    int ids[]={UC_ARM_REG_CPSR,UC_ARM_REG_PRIMASK,UC_ARM_REG_BASEPRI};
    uint32_t masks[3];void *values[]={&masks[0],&masks[1],&masks[2]};
    bool safe;
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active) {
        for(unsigned i=0;i<3;i++) masks[i]=m8rt_reg_read(rt,ids[i]);
        safe=!masks[1] && !masks[2];
    } else
#endif
    safe=uc_reg_read_batch(rt->uc,ids,values,3)==UC_ERR_OK && !masks[1] && !masks[2];
    if(rt->aot_mask_cache==AOT_MASK_UNREAD) {
        rt->aot_mask_cache=safe?AOT_MASK_SAFE:AOT_MASK_UNSAFE;
        if(safe) rt->aot_mask_cpsr=masks[0];
    }
    if(!safe) return false;
    *cpsr=masks[0];return true;
}
static int aot_nvic_result(m8rt *rt,bool admitted,uint32_t cpsr) {
    M8_ACCESS_NVIC(rt,0);
    if(admitted) M8_ACCESS_NVIC(rt,1);
    if(rt->fatal) return M8_AOT_FAULT;
    if(!admitted || rt->aot_sdhc_pending || should_stop(rt)) return 0;
    if(!rt->irq_nest && !(cpsr&0x80)) {
        M8_ACCESS_NVIC(rt,2);
        if(rt->audio_pending || (rt->usb_pending && m8rt_irq_enabled(rt,113)) ||
           (rt->systick_pending && (rt->syst_csr&3)==3)) return 0;
        if(rt->nvic_pending_banks) for(unsigned bank=0;bank<8;bank++) {
            M8_ACCESS_NVIC(rt,3);
            if(rt->nvic_iser[bank]&rt->nvic_ispr[bank]) return 0;
        }
    }
    return M8_AOT_MEM_CONTINUE;
}
static inline int native_callback_result(m8rt *rt,uint32_t addr,unsigned size,
                                         bool read,int result,
                                         const m8rt_native_service_token *token) {
    if(result==0 && m8rt_native_service_unchanged(rt,token)) result=M8_AOT_MEM_CONTINUE;
#ifdef M8RT_NATIVE_ENGINE
    m8rt_native_live_storage(rt,addr,read);
    if(rt->native.active && rt->native.boot_callback_irq) return 1;
    bool dwt=read && result==0 && size && size<=4 &&
        (uint32_t)(addr-0xe0001000u)<=4096u-size;
    if(rt->native.active && rt->native.chain && result!=M8_AOT_MEM_CONTINUE && !dwt)
        rt->native.service_exit=true;
#else
    (void)rt;(void)addr;(void)size;(void)read;
#endif
    return result;
}
static int slow_read(void *ctx,uint32_t addr,uint32_t size,uint32_t *value) {
    if(size!=1 && size!=2 && size!=4) return -1;
    m8rt *rt=ctx;
    M8_ACCESS_RECORD(rt,addr,size,2,0);
    /* DWT is already a pointer-mapped RAM page. pump() updates CYCCNT in
     * this buffer; reads have no device callback or time-advance effect. */
    uint32_t dwt_offset=addr-UINT32_C(0xe0001000);
    if(dwt_offset<=4096-size) {
        const uint8_t *p=rt->dwt+dwt_offset;
        *value=size==4?m8rt_le32(p):size==2?m8rt_le16(p):*p;
        return 0;
    }
    if((addr>=0x40000000 && addr<0x40400000) || (addr>=0x42000000 && addr<0x42010000) || (addr>=0xe000e000 && addr<0xe000f000)) {
        m8rt_native_service_token token=m8rt_native_service_begin(rt,addr,size,true);
        uint32_t cpsr=0;bool admitted=aot_nvic_admit(rt,addr,size,&cpsr);
        *value=device_read(rt,addr,size);
        if(rt->audit) audit_mmio(rt,addr,size,false,*value);
        if(size<4) *value&=(1u<<(size*8))-1u;
        return native_callback_result(rt,addr,size,true,aot_nvic_result(rt,admitted,cpsr),&token);
    }
    uint8_t b[4]={0}; int err=m8rt_mem_read(rt,addr,b,size); *value=m8rt_le32(b);
    m8rt_native_service_token token={0};
    return native_callback_result(rt,addr,size,true,err,&token);
}
static int slow_write(void *ctx,uint32_t addr,uint32_t size,uint32_t value) {
    if(size!=1 && size!=2 && size!=4) return -1;
    m8rt *rt=ctx;
    M8_ACCESS_RECORD(rt,addr,size,2,1);
    uint32_t dwt_offset=addr-UINT32_C(0xe0001000);
    if(dwt_offset<=4096-size) {
        uint8_t *p=rt->dwt+dwt_offset;
        if(size==4) m8rt_st32(p,value);
        else if(size==2) m8rt_st16(p,(uint16_t)value);
        else *p=(uint8_t)value;
        m8rt_native_service_token token={0};
        return native_callback_result(rt,addr,size,false,0,&token);
    }
    if((addr>=0x40000000 && addr<0x40400000) || (addr>=0x42000000 && addr<0x42010000) || (addr>=0xe000e000 && addr<0xe000f000)) { if(size<4) value&=(1u<<(size*8))-1u;
        m8rt_native_service_token token=m8rt_native_service_begin(rt,addr,size,false);
        uint32_t cpsr=0;bool admitted=aot_nvic_admit(rt,addr,size,&cpsr);
        if(rt->audit) audit_mmio(rt,addr,size,true,value);
        device_write(rt,addr,size,value);
        return native_callback_result(rt,addr,size,false,aot_nvic_result(rt,admitted,cpsr),&token); }
    uint8_t b[4]; m8rt_st32(b,value);
    m8rt_native_service_token token={0};
    return native_callback_result(rt,addr,size,false,m8rt_mem_write(rt,addr,b,size),&token);
}
#ifdef M8RT_NATIVE_ENGINE
/* Native admission selects this bus once. The normal Unicorn/AOT path keeps
 * slow_read/slow_write, and unqualified addresses use that same path. */
static bool native_nvic_enable(uint32_t addr) {
    uint32_t a=addr&~3u;
    return a-UINT32_C(0xe000e100)<32u || a-UINT32_C(0xe000e180)<32u;
}
static bool native_plain_device_read(uint32_t addr) {
    uint32_t a=addr&~3u;
    return a==UINT32_C(0x40384008) || a==UINT32_C(0x400e800c) ||
        a==UINT32_C(0x400e8024) || a==UINT32_C(0x400e802c) ||
        a-UINT32_C(0x400e9000)<0x400u;
}
#if defined(M8RT_NATIVE_DEVICE_FAST) && M8RT_NATIVE_DEVICE_FAST
#include "native_device_fast.h"
#include "native-device-read-fast.h"
#endif
static int native_device_read(void *ctx,uint32_t addr,uint32_t size,uint32_t *value) {
    m8rt *rt=ctx;
    M8_DEVICE_SITE(rt,addr,size,0);
#if defined(M8RT_NATIVE_DEVICE_FAST) && M8RT_NATIVE_DEVICE_FAST
    int fast_result;
    if(m8rt_native_device_fast_read(rt,addr,size,value,&fast_result) ||
       m8rt_native_plain_read_fast(rt,addr,size,value,&fast_result)) return fast_result;
#endif
    if(size!=1 && size!=2 && size!=4) return -1;
    uint32_t dwt_offset=addr-UINT32_C(0xe0001000);
    if(dwt_offset<=4096-size) {
        M8_ACCESS_RECORD(rt,addr,size,2,0);
        const uint8_t *p=rt->dwt+dwt_offset;
        *value=size==4?m8rt_le32(p):size==2?m8rt_le16(p):*p;
        return 0;
    }
    bool nvic=native_nvic_enable(addr);
    bool usb=addr-USB_BASE<0x200u;
    bool plain=native_plain_device_read(addr);
    if(!nvic && !usb && !plain) return slow_read(ctx,addr,size,value);
    M8_ACCESS_RECORD(rt,addr,size,2,0);
    m8rt_native_service_token token=m8rt_native_service_begin(rt,addr,size,true);
    uint32_t cpsr=0;bool admitted=nvic && aot_nvic_admit(rt,addr,size,&cpsr);
    if(usb) *value=m8rt_usb_read(rt,addr);
    else {
        /* Generic reads service SAI before returning the register value. */
        if(rt->mmio->periph[0xe800c/4]) m8rt_native_sai_service(rt);
        uint32_t a=addr&~3u;
        if(nvic) *value=a-UINT32_C(0xe000e100)<32u?
            rt->nvic_iser[(a&31u)/4]:rt->mmio->scs[(a-0xe000e000u)/4];
        else *value=rt->mmio->periph[(a-0x40000000u)/4];
    }
    if(rt->audit) audit_mmio(rt,addr,size,false,*value);
    if(size<4) *value&=(1u<<(size*8))-1u;
    return native_callback_result(rt,addr,size,true,aot_nvic_result(rt,admitted,cpsr),&token);
}
static int native_device_write(void *ctx,uint32_t addr,uint32_t size,uint32_t value) {
    m8rt *rt=ctx;
    M8_DEVICE_SITE(rt,addr,size,1);
#if defined(M8RT_NATIVE_DEVICE_FAST) && M8RT_NATIVE_DEVICE_FAST
    int fast_result;
    if(m8rt_native_device_fast_write(rt,addr,size,value,&fast_result)) return fast_result;
#endif
    if(size!=1 && size!=2 && size!=4) return -1;
    bool nvic=native_nvic_enable(addr);
    bool usb=addr-USB_BASE<0x200u;
    if(!nvic && !usb) return slow_write(ctx,addr,size,value);
    M8_ACCESS_RECORD(rt,addr,size,2,1);
    if(size<4) value&=(1u<<(size*8))-1u;
    m8rt_native_service_token token=m8rt_native_service_begin(rt,addr,size,false);
    uint32_t cpsr=0;bool admitted=nvic && aot_nvic_admit(rt,addr,size,&cpsr);
    if(rt->audit) audit_mmio(rt,addr,size,true,value);
    if(usb) m8rt_usb_write(rt,addr,value);
    else {
        uint32_t a=addr&~3u;unsigned bank=(a&31u)/4;
        if(a-UINT32_C(0xe000e100)<32u) rt->nvic_iser[bank]|=value;
        else rt->nvic_iser[bank]&=~value;
    }
    return native_callback_result(rt,addr,size,false,aot_nvic_result(rt,admitted,cpsr),&token);
}
void m8rt_native_device_bus(m8rt *rt,bool direct) {
    rt->memory.read=direct?native_device_read:slow_read;
    rt->memory.write=direct?native_device_write:slow_write;
}
void m8rt_native_boundary(m8rt *rt,uint32_t pc) {
    if(rt->native_live.enabled && pc>=SAMPLING_POLL_PC && pc<=SAMPLING_POLL_PC+4 &&
       m8rt_usb_audio_input_enabled(rt)) {
        m8rt_native_live_leave(rt,M8RT_NATIVE_LEAVE_RECORDING,pc);return;
    }
    if(pc>=MAIN_PC && pc<=MAIN_PC+6) main_hook(NULL,pc,0,rt);
    else if(pc>=WAIT_PC && pc<=WAIT_PC+20) wait_hook(NULL,pc,0,rt);
    else if(pc>=DELAY_PC && pc<=DELAY_PC+2) delay_hook(NULL,pc,0,rt);
    else if(pc>=SAMPLING_POLL_PC && pc<=SAMPLING_POLL_PC+4) sampling_hook(NULL,pc,0,rt);
    if(rt->local_enabled && (pc==0x33f9cu || pc==0x33fdau || pc==0x21828u)) m8rt_native_audio_boundary(rt,pc);
}
void m8rt_native_completed(m8rt *rt) {
    if(rt->aot_sdhc_pending) {rt->aot_sdhc_pending=false;if(!rt->fatal) sdhc_take(rt);}
    if(rt->watchdog.reset_pending)
        m8rt_native_live_leave(rt,M8RT_NATIVE_LEAVE_RESET,rt->native.cpu.r[15]);
}
bool m8rt_native_unstack(m8rt *rt) {return unstack(rt);}
bool m8rt_native_idle(m8rt *rt,uint32_t pc) {
    if(!rt->watchdog.enabled || rt->main_iters<=rt->boot_main_start || !watchdog_self_branch(rt,pc)) return false;
    /* The reference batches this branch outside an execution slice. Keep
     * that stop policy, so IRQ entry does not add a USB enumeration tick. */
    rt->in_emu=false;watchdog_fallback(rt,WATCHDOG_STALL_INSTRUCTIONS,true);rt->in_emu=true;
    return true;
}
int m8rt_native_event_slice(m8rt *rt) {
    if(rt->fatal) return M8_NATIVE_EVENT_FAULT;
    /* A live CPU transfer is not a logical execution slice. Preserve a real
     * stop that preceded the transfer, but do not add time for a decline. */
    if(rt->native_live.leave_pending && !rt->native_live.completed_stop)
        return M8_NATIVE_EVENT_HANDOFF;
    if(rt->watchdog.reset_pending) return M8_NATIVE_EVENT_EXIT;
    /* A logical engine slice still owns exactly one enumeration step. IRQ
     * delivery here ran outside in_emu in the reference owner. */
    rt->aot_active=false;rt->aot_mask_cache=0;
    rt->in_emu=false;
    m8rt_reconcile(rt);
    m8rt_usb_tick_enum(rt,100000);rt->native.event_stats.enum_ticks++;
    deliver_usb(rt);rt->last_pc=m8rt_reg_read(rt,UC_ARM_REG_PC);
    rt->slices++;rt->native.event_stats.logical_slices++;
    rt->native.event_slice_finished=true;
    rt->watchdog_no_clock_since_ns=0;
    rt->in_emu=true;
    if(rt->fatal) return M8_NATIVE_EVENT_FAULT;
    if(rt->native_live.leave_pending) return M8_NATIVE_EVENT_HANDOFF;
    if((rt->stop_iters && rt->main_iters>=rt->stop_iters) ||
       (rt->stop_frames && limit_frames(rt)>=rt->stop_frames) ||
       should_stop(rt) || (rt->yield_boot && m8rt_usb_boot(rt))) {
        rt->native.event_stats.host_returns++;
        return M8_NATIVE_EVENT_EXIT;
    }
    rt->native.stopped=false;rt->native.event_slice_finished=false;
    rt->native_live.completed_stop=false;
    rt->native.event_polls=0;
    rt->native.stats.slices++;
    return M8_NATIVE_EVENT_AGAIN;
}
int m8rt_native_event_enter(m8rt *rt,uint32_t pc) {
    rt->native.event_stats.entries++;rt->native.stats.last_pc=pc;
    if(rt->fatal) return M8_NATIVE_EVENT_FAULT;
    if(rt->native.code_invalidated) return M8_NATIVE_EVENT_HANDOFF;
    if(atomic_load_explicit(&rt->host_stop,memory_order_relaxed) ||
       (rt->deadline_ns && !(rt->native.event_polls++&255u) && m8rt_now_ns()>=rt->deadline_ns) ||
       rt->watchdog.reset_pending) return m8rt_native_event_slice(rt);
    if(m8rt_native_idle(rt,pc)) return M8_NATIVE_EVENT_AGAIN;
    if(pc>=0xf0000000u) {
        if(!m8rt_native_unstack(rt)) {
            m8rt_fail(rt,"native event exception return pc=%08x",pc);return M8_NATIVE_EVENT_FAULT;
        }
        rt->native.event_stats.exception_returns++;
        return m8rt_native_event_slice(rt);
    }
    if(pc>=MAIN_PC && pc<=MAIN_PC+6) rt->native.event_stats.main_hooks++;
    else if(pc>=WAIT_PC && pc<=WAIT_PC+20) rt->native.event_stats.wait_hooks++;
    else if(pc>=DELAY_PC && pc<=DELAY_PC+2) rt->native.event_stats.delay_hooks++;
    else if(pc>=SAMPLING_POLL_PC && pc<=SAMPLING_POLL_PC+4) rt->native.event_stats.sampling_hooks++;
    if(rt->local_enabled && (pc==0x33f9cu || pc==0x33fdau || pc==0x21828u))
        rt->native.event_stats.audio_hooks++;
    m8rt_native_boundary(rt,pc);
    if(rt->fatal) return M8_NATIVE_EVENT_FAULT;
    if(rt->native.stopped) return m8rt_native_event_slice(rt);
    if(rt->native.cpu.r[15]!=pc) return M8_NATIVE_EVENT_AGAIN;
    rt->aot_active=true;rt->aot_mask_cache=rt->aot_services?1u:0u;
    rt->native.service_exit=false;
    return M8_NATIVE_EVENT_RUN;
}
int m8rt_native_event_completed(m8rt *rt,uint32_t source_pc,unsigned cause) {
    (void)source_pc;
    rt->native.event_stats.completed++;
    if(cause==M8_NATIVE_EXIT_MEMORY) rt->native.event_stats.memory_returns++;
    else if(cause==M8_NATIVE_EXIT_REQUIRED) rt->native.event_stats.required_returns++;
    else if(cause==M8_NATIVE_EXIT_BUDGET) rt->native.event_stats.budget_returns++;
    rt->aot_active=false;rt->aot_mask_cache=0;
    if(rt->fatal) return M8_NATIVE_EVENT_FAULT;
    if(rt->native.code_invalidated) return M8_NATIVE_EVENT_HANDOFF;
    m8rt_native_completed(rt);
    if(rt->native_live.leave_pending && !rt->native_live.completed_stop)
        return M8_NATIVE_EVENT_HANDOFF;
    /* A callback has completed its effects and published the resume PC.
     * Do not run more entries after it observed the host slice deadline. */
    if(rt->native_live.deadline_seen) return m8rt_native_event_slice(rt);
    return rt->native.stopped?m8rt_native_event_slice(rt):M8_NATIVE_EVENT_RUN;
}
#endif
static int load_firmware(m8rt *rt,const char *path) {
    FILE *f=fopen(path,"rb"); if(!f) return -1;
    uint8_t *flash=rt->memory.flash.host; memset(flash,255,rt->memory.flash.size);
    int first=fgetc(f); rewind(f);
    if(first!=':') { size_t n=fread(flash,1,rt->memory.flash.size,f); if(!n || ferror(f)) { fclose(f); return -1; } }
    else {
        char line[600]; uint32_t base=0;
        while(fgets(line,sizeof line,f)) {
            if(line[0]!=':') continue;
            size_t n=strcspn(line,"\r\n"); if(n<11 || !(n&1)) { fclose(f); return -1; }
            uint8_t rec[260]; size_t bytes=(n-1)/2; if(bytes>sizeof rec) { fclose(f); return -1; }
            unsigned sum=0;
            for(size_t i=0;i<bytes;i++) { unsigned v; if(sscanf(line+1+2*i,"%2x",&v)!=1) { fclose(f);return -1; } rec[i]=(uint8_t)v; sum+=v; }
            if((sum&255) || bytes!=(size_t)rec[0]+5) { fclose(f);return -1; }
            uint32_t addr=(uint32_t)rec[1]<<8|rec[2];
            if(rec[3]==0) { uint64_t dest=(uint64_t)base+addr; if(dest>=0x60000000 && dest+rec[0]<=0x60800000) memcpy(flash+(dest-0x60000000),rec+4,rec[0]); }
            else if(rec[3]==1) break;
            else if(rec[3]==4 && rec[0]==2) base=((uint32_t)rec[4]<<24)|((uint32_t)rec[5]<<16);
        }
    }
    fclose(f);
    uint32_t ivt=m8rt_le32(flash+0x1000);
    if(m8rt_le32(flash)!=0x42464346 || (ivt!=0x432000d1 && ivt!=0x402000d1)) return -1;
    rt->entry=m8rt_le32(flash+0x1004);
    uint8_t *sig=flash+0x7ff000;
    if(memcmp(sig,"FCFB",4) && memcmp(sig,"\xff\xff\xff\xff",4)) return -1;
    memcpy(sig,"FCFB",4); return 0;
}
/* Reserve address space, not four GiB of resident memory. Only actual guest
 * regions receive pages. Leading/trailing guards also cover a host access
 * that crosses the unsigned guest address range. DWT keeps a separate buffer
 * because its 4 KiB page can be smaller than the host's page size. */
static int reserve_guest_memory(m8rt *rt) {
#if (defined(__linux__) || defined(__APPLE__)) && UINTPTR_MAX > UINT32_MAX
    long page=sysconf(_SC_PAGESIZE);
    if(page<=0 || (page & (page-1)) || page>128*1024) return -1;
    size_t size=UINT64_C(0x100000000)+2*(size_t)page;
    void *p=mmap(NULL,size,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(p==MAP_FAILED) return -1;
    rt->memory_reservation=p;rt->memory_reservation_size=size;
    rt->memory_offset=(uint8_t *)p+page;
    return 0;
#else
    (void)rt;
    return -1;
#endif
}
static int map_buffer(m8rt *rt,m8_region *r,uint32_t base,uint32_t size,unsigned perms) {
    rt->direct_memory=false; /* Also protect any future use after creation. */
    void *p=NULL;
    if(rt->memory_offset && base!=0xe0001000u) {
#if defined(__linux__) || defined(__APPLE__)
        p=rt->memory_offset+(size_t)base;
        if(mprotect(p,size,PROT_READ|PROT_WRITE)) return -1;
#else
        return -1;
#endif
    } else {
        if(posix_memalign(&p,4096,size)) return -1;
        memset(p,0,size);
    }
    *r=(m8_region){p,base,size}; return (int)uc_mem_map_ptr(rt->uc,base,size,perms,p);
}
static void block_hook(uc_engine *uc,uint64_t pc,uint32_t size,void *user) {
    (void)uc; m8rt *rt=user;
    if(!rt->block_cap) return;
    size_t slot=((uint32_t)pc*2654435761u)&(rt->block_cap-1);
    for(size_t i=0;i<rt->block_cap;i++) {
        m8rt_block *b=&rt->blocks[slot];
        if(!b->hits) { b->pc=(uint32_t)pc;b->size=size;rt->block_used++; }
        if(b->pc==(uint32_t)pc) { b->hits++; return; }
        slot=(slot+1)&(rt->block_cap-1);
    }
}
/* Region contexts are allocated after the public runtime to keep stable pointers. */
m8rt *m8rt_create(const char *firmware,const char *image,const m8rt_options *opts) {
    return m8rt_create_aot(firmware,image,opts,0);
}
m8rt *m8rt_create_aot(const char *firmware,const char *image,const m8rt_options *opts,unsigned requirements) {
    m8rt *rt=calloc(1,sizeof(*rt)+3*sizeof(mmio_region)); if(!rt) return NULL;
    rt->options=opts?*opts:(m8rt_options){0,1,1,0}; rt->audio_ch=-1;
    const char *aot_timing=getenv("M8RT_AOT_TIMING");
    rt->aot_timing_enabled=aot_timing && *aot_timing && strcmp(aot_timing,"0");
    const char *direct_memory=getenv("M8RT_DIRECT_MEMORY");
    bool direct_requested=direct_memory?(*direct_memory && strcmp(direct_memory,"0")):M8RT_DIRECT_MEMORY_DEFAULT;
    const char *guest_count=getenv("M8RT_GUEST_COUNT");
    bool guest_count_requested=guest_count && *guest_count && strcmp(guest_count,"0");
    rt->next_systick=rt->next_sof=600000;rt->next_audio=1741560;
    atomic_init(&rt->host_stop,false);
    const char *stall_policy=getenv("M8RT_STALL_POLICY");
    if(stall_policy && strcmp(stall_policy,"guarded") && strcmp(stall_policy,"deterministic")) {
        m8rt_fail(rt,"invalid M8RT_STALL_POLICY=%s: expected guarded or deterministic",stall_policy);
        return rt;
    }
    rt->deterministic_clock=stall_policy && !strcmp(stall_policy,"deterministic");
    if(requirements & ~M8RT_AOT_REQUIRE_FIXED_MEMORY) {
        m8rt_fail(rt,"unsupported AOT creation requirements: flags=%08x, supported=%08x",requirements,M8RT_AOT_REQUIRE_FIXED_MEMORY);
        return rt;
    }
    unsigned major=0,minor=0; uc_version(&major,&minor);
    if(major!=2 || minor!=1) { m8rt_fail(rt,"Unicorn 2.1.x required, found %u.%u",major,minor); return rt; }
    uc_err err=uc_open(UC_ARCH_ARM,UC_MODE_THUMB,&rt->uc);
    if(err) { m8rt_fail(rt,"uc_open: %s",uc_strerror(err)); return rt; }
    /* Unicorn reserves a 1 GiB code buffer by default on 64-bit hosts, which
     * small devices refuse (a 512 MB board cannot map it). The translated
     * firmware fits easily in 128 MiB; M8RT_TCG_BUFFER_MB overrides it. */
    uint32_t tcg_mb=128;
    const char *tcg_env=getenv("M8RT_TCG_BUFFER_MB");
    if(tcg_env && *tcg_env) { long v=strtol(tcg_env,NULL,10); if(v>=8 && v<=2048) tcg_mb=(uint32_t)v; }
    uc_ctl_set_tcg_buffer_size(rt->uc,tcg_mb*1024u*1024u);
    uc_ctl_set_cpu_model(rt->uc,UC_CPU_ARM_CORTEX_M7);
    const char *fixed_memory=getenv("M8RT_FIXED_MEMORY");
    bool fixed_requested=(requirements & M8RT_AOT_REQUIRE_FIXED_MEMORY) ||
                         (fixed_memory && *fixed_memory && strcmp(fixed_memory,"0"));
    if(fixed_requested && reserve_guest_memory(rt)) {
        m8rt_fail(rt,"fixed guest memory reservation failed (64-bit Linux or macOS required)");return rt;
    }
    if(map_buffer(rt,&rt->memory.itcm,0,512*1024,UC_PROT_ALL) || map_buffer(rt,&rt->memory.dtcm,0x20000000,512*1024,UC_PROT_READ|UC_PROT_WRITE) || map_buffer(rt,&rt->memory.ocram,0x20200000,512*1024,UC_PROT_READ|UC_PROT_WRITE) || map_buffer(rt,&rt->memory.flash,0x60000000,8*1024*1024,UC_PROT_ALL)) { m8rt_fail(rt,"RAM map failed"); return rt; }
    m8_region rom={0},dwt={0};
    if(map_buffer(rt,&rom,0x200000,128*1024,UC_PROT_READ|UC_PROT_EXEC) || map_buffer(rt,&dwt,0xe0001000,4096,UC_PROT_READ|UC_PROT_WRITE)) { rt->rom=rom.host;rt->dwt=dwt.host;m8rt_fail(rt,"ROM/DWT map failed");return rt; }
    rt->rom=rom.host;rt->dwt=dwt.host;
    rt->memory.ctx=rt;rt->memory.read=slow_read;rt->memory.write=slow_write;
    if(load_firmware(rt,firmware)) { m8rt_fail(rt,"firmware load failed: %s",firmware); return rt; }
    if(m8rt_mmio_init(rt) || m8rt_nor_init(rt) || m8rt_usb_init(rt) || m8rt_sd_init(rt,image)) { if(!rt->fatal) m8rt_fail(rt,"device initialization failed");return rt; }
    if(image) {
        char *path=malloc(strlen(image)+sizeof(".settings.nor"));
        if(!path) {m8rt_fail(rt,"NOR image path allocation failed");return rt;}
        strcpy(path,image);strcat(path,".settings.nor");
        int rc=m8rt_nor_set_file(rt,path);free(path);if(rc) return rt;
    }
    m8rt_watchdog_init(rt,false);
    mmio_region *regions=(mmio_region *)(rt+1);
    uint32_t bases[]={0x40000000,0x42000000,0xe000e000}, sizes[]={0x400000,0x10000,0x1000};
    for(unsigned i=0;i<3;i++) { regions[i]=(mmio_region){rt,bases[i]}; err=uc_mmio_map(rt->uc,bases[i],sizes[i],mapped_read,&regions[i],mapped_write,&regions[i]); if(err) { m8rt_fail(rt,"MMIO map: %s",uc_strerror(err));return rt; } }
    uc_hook h;
    uc_hook_add(rt->uc,&h,UC_HOOK_CODE,main_hook,rt,MAIN_PC,MAIN_PC+6);
    uc_hook_add(rt->uc,&h,UC_HOOK_CODE,wait_hook,rt,WAIT_PC,WAIT_PC+20);
    uc_hook_add(rt->uc,&h,UC_HOOK_CODE,delay_hook,rt,DELAY_PC,DELAY_PC+2);
    uc_hook_add(rt->uc,&h,UC_HOOK_CODE,sampling_hook,rt,SAMPLING_POLL_PC,SAMPLING_POLL_PC+4);
    uc_hook_add(rt->uc,&h,UC_HOOK_INTR,intr_hook,rt,1,0);
    uc_hook_add(rt->uc,&h,UC_HOOK_MEM_UNMAPPED,unmapped_hook,rt,1,0);
    if(rt->options.profile_blocks) { rt->block_cap=65536;rt->blocks=calloc(rt->block_cap,sizeof(*rt->blocks));if(!rt->blocks) m8rt_fail(rt,"profile allocation failed");else uc_hook_add(rt->uc,&h,UC_HOOK_BLOCK,block_hook,rt,1,0); }
    if(guest_count_requested && m8rt_set_guest_count(rt,1)) {m8rt_fail(rt,"guest count hook failed");return rt;}
    reg_write(rt,UC_ARM_REG_SP,0x20070000);reg_write(rt,UC_ARM_REG_CPSR,0x01000000);reg_write(rt,UC_ARM_REG_LR,0xffffffff);reg_write(rt,UC_ARM_REG_PC,rt->entry&~1u);
    err=uc_context_alloc(rt->uc,&rt->reset_context);
    if(!err) err=uc_context_save(rt->uc,rt->reset_context);
    if(err) m8rt_fail(rt,"reset CPU context: %s",uc_strerror(err));
    if(!rt->fatal && !rt->deterministic_clock) m8rt_stall_init(rt);
    if(!rt->fatal && direct_requested && uc_m8_memory_generation) {
        rt->direct_memory_generation=uc_m8_memory_generation(rt->uc);
        if(rt->direct_memory_generation) {
            rt->direct_memory_expected_generation=*rt->direct_memory_generation;
            rt->direct_memory=true;
        }
    }
    return rt;
}
void m8rt_destroy(m8rt *rt) {
    if(!rt) return;
#ifdef M8RT_NATIVE_ENGINE
    free(rt->native.lookup_cache);
    free(rt->native.chain_state.slots);
#endif
#ifdef M8RT_ACCESS_PROFILE
    m8rt_access_profile_destroy(rt);
#endif
    m8rt_audit_destroy(rt);
    m8rt_stall_destroy(rt);
    if(rt->reset_context) uc_context_free(rt->reset_context);
    if(rt->uc) uc_close(rt->uc);
    m8rt_sd_destroy(rt);m8rt_usb_destroy(rt);m8rt_nor_destroy(rt);m8rt_mmio_destroy(rt);
    if(rt->memory_reservation) {
#if defined(__linux__) || defined(__APPLE__)
        munmap(rt->memory_reservation,rt->memory_reservation_size);
#endif
    } else {
        free(rt->memory.itcm.host);free(rt->memory.dtcm.host);free(rt->memory.ocram.host);free(rt->memory.flash.host);free(rt->rom);
    }
    free(rt->dwt);
#ifdef M8RT_GUEST_PC_PROFILE
    free(rt->guest_pcs);
#endif
    free(rt->cdc.data);free(rt->pcm.data);free(rt->pcm_lengths.data);free(rt->pcm_positions.data);free(rt->local_pcm.data);free(rt->local_positions.data);free(rt->irqs.data);free(rt->blocks);free(rt->aot_hooks);free(rt->aot_entries);free(rt);
}
const char *m8rt_error(m8rt *rt) { return rt?rt->error:"runtime allocation failed"; }
static int watchdog_reset(m8rt *rt) {
    /* Run only after Unicorn returns. The mapped flash is retained in place,
     * including EEPROM/NOR settings and the Teensy board signature. */
    if(rt->in_emu) { m8rt_fail(rt,"watchdog reset inside guest callback");return -1; }
#ifdef M8RT_NATIVE_ENGINE
    rt->native_live.verified=false;rt->native_live.invalidate_pending=false;
#endif
    ++rt->watchdog.resets;rt->watchdog.last_reset_cycles=rt->guest_cycles;
    m8rt_sd_reset(rt);m8rt_usb_reset(rt);m8rt_nor_reset(rt);
    if(m8rt_mmio_reset(rt)) { m8rt_fail(rt,"watchdog MMIO reset allocation failed");return -1; }
    memset(rt->memory.itcm.host,0,rt->memory.itcm.size);
    memset(rt->memory.dtcm.host,0,rt->memory.dtcm.size);
    memset(rt->memory.ocram.host,0,rt->memory.ocram.size);
    memset(rt->dwt,0,4096);
    memset(rt->nvic_iser,0,sizeof rt->nvic_iser);memset(rt->nvic_ispr,0,sizeof rt->nvic_ispr);
    rt->nvic_pending_banks=0;
    memset(rt->nvic_ipr,0,sizeof rt->nvic_ipr);memset(rt->nvic_pend_seq,0,sizeof rt->nvic_pend_seq);
    memset(rt->nvic_isr_done,0,sizeof rt->nvic_isr_done);memset(rt->exc_stack,0,sizeof rt->exc_stack);
    rt->cyccnt=rt->vtor=rt->syst_csr=rt->syst_cvr=0;
    rt->next_systick=rt->next_sof=600000;rt->next_audio=1741560;
    rt->systick_pending=rt->audio_pending=rt->usb_pending=rt->entry_pending=rt->er_sent=false;
    rt->sdhc_in_sync=rt->aot_sdhc_pending=rt->sampling_pending=false;
    if(watchdog_counting(rt,false)) return -1;
    rt->watchdog_slice_instructions=0;rt->watchdog_counted_stop=false;
    rt->watchdog_no_clock_since_ns=0;
    rt->audio_ch=-1;rt->irq_nest=0;rt->boot_main_start=rt->main_iters;
    rt->local_phase=0;rt->local_head=0;rt->local_sp=rt->local_receiver=rt->local_writer=0;
    rt->guest_count_last_valid=false;
    m8rt_watchdog_init(rt,true);
    uc_err err=uc_context_restore(rt->uc,rt->reset_context);
    if(!err) err=uc_ctl_flush_tb(rt->uc);
    if(err) { m8rt_fail(rt,"watchdog CPU reset: %s",uc_strerror(err));return -1; }
    if(rt->options.host_handshake && m8rt_usb_queue(rt,"E",1)) return -1;
    return 0;
}
int m8rt_run(m8rt *rt,uint64_t until_iters,uint64_t until_frames,uint32_t wall_us,int yield_boot) {
    if(!rt || rt->fatal || rt->in_emu || !direct_memory_valid(rt)) return -1;
    if(!rt->started) { rt->started=true;if(rt->options.host_handshake) m8rt_usb_queue(rt,"E",1); }
    rt->stop_iters=until_iters;rt->stop_frames=until_frames;rt->yield_boot=yield_boot!=0;
    rt->deadline_ns=wall_us?m8rt_now_ns()+(uint64_t)wall_us*1000:0;
#ifdef M8RT_NATIVE_ENGINE
    rt->native_live.deadline_seen=false;
#endif
    atomic_store(&rt->host_stop,false);
    bool retried_invalid=false;
    uint64_t host_stall_ns=m8rt_stall_limit_ns(rt);
#ifdef M8RT_NATIVE_ENGINE
    bool native=rt->native.active;
    if(native && !rt->deterministic_clock) {m8rt_fail(rt,"native playback requires deterministic clock policy");return -1;}
#endif
    while(!rt->fatal && !atomic_load(&rt->host_stop) && (!until_iters || rt->main_iters<until_iters) && (!until_frames || limit_frames(rt)<until_frames)) {
#ifdef M8RT_NATIVE_ENGINE
        if(rt->native_live.enabled && rt->native.active && rt->watchdog.reset_pending) {
            m8rt_native_live_leave(rt,M8RT_NATIVE_LEAVE_RESET,rt->native.cpu.r[15]);
            if(m8rt_native_live_switch(rt)<0) {m8rt_fail(rt,"native live reset handoff failed");break;}
        }
        if(rt->native.code_invalidated && !rt->native.active && rt->aot_lookup) {
            /* Publish the pre-store CPU. Unicorn owns the code store and its
             * instruction-cache ordering. Remove every native entry hook. */
            if(m8rt_set_aot(rt,NULL,NULL)) {m8rt_fail(rt,"native code-write AOT removal failed");break;}
        }
        native=rt->native.active;
#endif
        if(rt->watchdog.reset_pending) {
#ifdef M8RT_NATIVE_ENGINE
            if(native) {m8rt_fail(rt,"native watchdog reset requires reference engine");break;}
#endif
            if(watchdog_reset(rt)) break;retried_invalid=false;
        }
        if(rt->deadline_ns && m8rt_now_ns()>=rt->deadline_ns) break;
        uint32_t pc=m8rt_reg_read(rt,UC_ARM_REG_PC);
        bool eligible=rt->watchdog.enabled && rt->main_iters>rt->boot_main_start;
        if(eligible && watchdog_self_branch(rt,pc)) {
            if(watchdog_counting(rt,false)) break;
            /* B . has no state effects between events. Batch exact repeats
             * outside Unicorn; no normal instruction/block hook is needed. */
            watchdog_fallback(rt,WATCHDOG_STALL_INSTRUCTIONS,true);continue;
        }
        if(!eligible && watchdog_counting(rt,false)) break;
        bool counted=rt->watchdog_stall_hook!=0;
        uint64_t cycles_before=rt->guest_cycles;
        /* The stall clock is required even when callback statistics are off. */
        uint64_t start=m8rt_now_ns();
        rt->watchdog_slice_instructions=0;rt->watchdog_counted_stop=false;
        if(eligible && !counted
#ifdef M8RT_NATIVE_ENGINE
            && !native
#endif
        ) { m8rt_stall_enter(rt);if(rt->fatal) break; }
        rt->in_emu=true;
#ifdef M8RT_NATIVE_ENGINE
        rt->native_live.completed_stop=false;
#endif
        uc_err err;
#ifdef M8RT_NATIVE_ENGINE
        if(native) err=m8rt_native_exec(rt);else
#endif
        err=uc_emu_start(rt->uc,pc|1u,0,0,0);
        rt->in_emu=false;
        bool interrupted=m8rt_stall_leave(rt);
        uint64_t finished=m8rt_now_ns();
#ifdef M8RT_NATIVE_ENGINE
        if(native && rt->native_boot_app.ready && !rt->fatal && !err) {
            if(m8rt_native_boot_app_finish(rt)<0) break;
            /* Changing the dispatcher completes no guest instruction or
             * logical slice. Preserve the original enumeration charge. */
            continue;
        }
        if(!native)
#endif
        rt->uc_ns+=finished-start;
#ifdef M8RT_ACCESS_PROFILE
#ifdef M8RT_NATIVE_ENGINE
        if(!native)
#endif
        m8rt_access_storage_exec(rt,finished-start);
#endif
#ifdef M8RT_NATIVE_ENGINE
        if(rt->native_live.enabled && !rt->fatal && !err) {
            if(native && rt->watchdog.reset_pending)
                m8rt_native_live_leave(rt,M8RT_NATIVE_LEAVE_RESET,rt->native.cpu.r[15]);
            int switched=m8rt_native_live_switch(rt);
            if(switched<0) {m8rt_fail(rt,"native live switch failed pc=%08x",m8rt_reg_read(rt,UC_ARM_REG_PC));break;}
            /* These are internal CPU transfers. Only a real execution stop
             * can complete the slice and charge USB enumeration time. */
            if(switched && !rt->native_live.completed_stop) continue;
        }
        if(!native || !rt->native.event_slice_finished)
#endif
        rt->slices++;
        if(rt->watchdog.reset_pending && !rt->fatal) {
#ifdef M8RT_NATIVE_ENGINE
            if(native) {m8rt_fail(rt,"native watchdog reset requires reference engine");break;}
#endif
            if(watchdog_reset(rt)) break;retried_invalid=false;continue;
        }
        if(!err) retried_invalid=false;
        bool clock_progress=rt->guest_cycles!=cycles_before;
        if(!rt->deterministic_clock && !err && eligible && !clock_progress) {
            if(!rt->watchdog_no_clock_since_ns) rt->watchdog_no_clock_since_ns=start;
            if(counted && rt->watchdog_counted_stop) {
                watchdog_fallback(rt,rt->watchdog_slice_instructions,false);continue;
            }
            if(interrupted || finished-rt->watchdog_no_clock_since_ns>=host_stall_ns) {
                if(watchdog_counting(rt,true)) break;
                /* Neither a monitor stop nor an instruction-bound stop is a
                 * new transport slice. Never add enumeration time for it. */
                continue;
            }
        } else {
            rt->watchdog_no_clock_since_ns=0;
            if(watchdog_counting(rt,false)) break;
        }
        if(interrupted && !err) continue;
        if(err) guest_count_discard(rt,m8rt_reg_read(rt,UC_ARM_REG_PC));
        if(err==UC_ERR_INSN_INVALID && !retried_invalid) {
            retried_invalid=true;
            reg_write(rt,UC_ARM_REG_CPSR,m8rt_reg_read(rt,UC_ARM_REG_CPSR)|0x01000000);
            pc=m8rt_reg_read(rt,UC_ARM_REG_PC);reg_write(rt,UC_ARM_REG_PC,pc&~1u);
            /* Retry through the same slice owner so a retry cannot bypass
             * stall monitoring, deadlines or completed-instruction bounds. */
            continue;
        }
        if(err) { m8rt_fail(rt,"guest execution pc=%08x: %s",m8rt_reg_read(rt,UC_ARM_REG_PC),uc_strerror(err));break; }
#ifdef M8RT_NATIVE_ENGINE
        if(!native || !rt->native.event_slice_finished) {
#endif
            pc=m8rt_reg_read(rt,UC_ARM_REG_PC);
            if(pc>=0xf0000000u) { if(!unstack(rt)) { m8rt_fail(rt,"exception unstack failed pc=%08x",pc);break; } }
            else m8rt_reconcile(rt);
            m8rt_usb_tick_enum(rt,100000);deliver_usb(rt);
            rt->last_pc=m8rt_reg_read(rt,UC_ARM_REG_PC);
#ifdef M8RT_NATIVE_ENGINE
        }
#endif
        if(yield_boot && m8rt_usb_boot(rt)) break;
    }
    rt->deadline_ns=0;rt->stop_iters=rt->stop_frames=0;rt->yield_boot=false;
    if(!rt->fatal && m8rt_nor_flush(rt)) return -1;
    return rt->fatal?-1:atomic_load(&rt->host_stop)?1:0;
}
int m8rt_run_main_iters(m8rt *rt,uint64_t n) {
    if(!rt || rt->fatal || n>UINT64_MAX-rt->main_iters) return -1;
    return n?m8rt_run(rt,rt->main_iters+n,0,0,0):0;
}
int m8rt_run_until_frames(m8rt *rt,uint64_t n) {
    if(!rt || rt->fatal) return -1;
    return n>rt->pcm_total/4?m8rt_run(rt,0,n,0,0):0;
}
int m8rt_queue_host_bytes(m8rt *rt,const void *data,size_t n) { return m8rt_usb_queue(rt,data,n); }
size_t m8rt_read_cdc(m8rt *rt,void *out,size_t n) { return m8rt_queue_read(&rt->cdc,out,n); }
size_t m8rt_read_pcm(m8rt *rt,void *out,size_t n) {
    return m8rt_read_pcm_timed(rt,out,n,NULL);
}
size_t m8rt_read_pcm_timed(m8rt *rt,void *out,size_t n,m8rt_audio_position *position) {
    if(!rt->pcm_packet_left) { uint8_t length[4]; if(m8rt_queue_read(&rt->pcm_lengths,length,4)!=4) return 0; rt->pcm_packet_left=m8rt_le32(length);m8rt_queue_read(&rt->pcm_positions,&rt->pcm_position,sizeof rt->pcm_position); }
    if(n>rt->pcm_packet_left) n=rt->pcm_packet_left;
    if(position) { *position=rt->pcm_position;position->frame+=(rt->pcm_position.frames*4-rt->pcm_packet_left)/4;position->frames=(uint32_t)(n/4); }
    size_t count=m8rt_queue_read(&rt->pcm,out,n);rt->pcm_packet_left-=(uint32_t)count;return count;
}
int m8rt_run_audio_frames(m8rt *rt,uint64_t frames,uint32_t wall_us) {
    if(!rt || rt->fatal || rt->in_emu) return -1;
    rt->audio_limit_local=rt->local_selected;
    int result=m8rt_run(rt,0,frames,wall_us,0);
    rt->audio_limit_local=false;
    return result;
}
size_t m8rt_read_irqs(m8rt *rt,uint32_t *out,size_t n) { size_t count=0;uint8_t b[4];while(count<n && m8rt_queue_read(&rt->irqs,b,4)==4) out[count++]=m8rt_le32(b);return count; }
uint64_t m8rt_get(m8rt *rt,unsigned f) {
    switch(f) {
        case M8RT_GUEST_COUNT_ENABLED:return rt->guest_count_enabled;
        case M8RT_JIT_GUEST_INSTRUCTIONS:return rt->jit_guest_instructions;
        case M8RT_AOT_TIMING_ENABLED:return rt->aot_timing_enabled;
        case M8RT_AOT_CALLS:return rt->aot_calls;
        case M8RT_AOT_NATIVE_NS:return rt->aot_native_ns;
        case M8RT_AOT_HOOK_NS:return rt->aot_hook_ns;
        default:break;
    }
    switch(f) { case 0:return rt->main_iters;case 1:return rt->cyccnt;case 2:return m8rt_reg_read(rt,UC_ARM_REG_PC);case 3:return m8rt_usb_boot(rt);case 4:return rt->pcm_total;case 5:return rt->slices;case 6:return rt->uc_ns;case 7:return rt->callback_ns;case 8:return rt->callback_count;case 9:return rt->cdc_total;case 10:return m8rt_usb_configured(rt);case 11:return m8rt_usb_dtr(rt);case 12:return m8rt_usb_queued(rt);case 13:return rt->main_selected;case 14:return rt->main_expanded;case 15:return rt->main_cycles;case 16:return rt->local_total;case 17:return rt->local_pcm.len-rt->local_pcm.head;case 18:return rt->local_blocks;case 19:return rt->guest_cycles;case 20:return rt->local_last_cycles;default:return 0; }
}
int m8rt_set(m8rt *rt,unsigned field,uint64_t value) {
    if(!rt || rt->in_emu) return -1;
    if(field==0) rt->options.host_handshake=value!=0;
    else if(field==1) { m8rt_usb_set_capture(rt,value!=0);rt->local_capture=value!=0; }
    else return -1;
    return 0;
}
void m8rt_request_stop(m8rt *rt) { atomic_store(&rt->host_stop,true); }
enum { AOT_CPU_WORDS=50 };
static const int aot_reg_ids[AOT_CPU_WORDS]={
    UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,
    UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,
    UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11,
    UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,
    UC_ARM_REG_XPSR,
    UC_ARM_REG_S0,UC_ARM_REG_S1,UC_ARM_REG_S2,UC_ARM_REG_S3,
    UC_ARM_REG_S4,UC_ARM_REG_S5,UC_ARM_REG_S6,UC_ARM_REG_S7,
    UC_ARM_REG_S8,UC_ARM_REG_S9,UC_ARM_REG_S10,UC_ARM_REG_S11,
    UC_ARM_REG_S12,UC_ARM_REG_S13,UC_ARM_REG_S14,UC_ARM_REG_S15,
    UC_ARM_REG_S16,UC_ARM_REG_S17,UC_ARM_REG_S18,UC_ARM_REG_S19,
    UC_ARM_REG_S20,UC_ARM_REG_S21,UC_ARM_REG_S22,UC_ARM_REG_S23,
    UC_ARM_REG_S24,UC_ARM_REG_S25,UC_ARM_REG_S26,UC_ARM_REG_S27,
    UC_ARM_REG_S28,UC_ARM_REG_S29,UC_ARM_REG_S30,UC_ARM_REG_S31,
    UC_ARM_REG_FPSCR
};
_Static_assert(sizeof(m8_cpu)==AOT_CPU_WORDS*sizeof(uint32_t),"AOT CPU layout changed");
_Static_assert(offsetof(m8_cpu,xpsr)==64 && offsetof(m8_cpu,s)==68 &&
               offsetof(m8_cpu,fpscr)==196,"AOT CPU word offsets changed");
static uc_err aot_write_delta(uc_engine *uc,const m8_cpu *before,const m8_cpu *after) {
    int ids[AOT_CPU_WORDS],count=0;void *values[AOT_CPU_WORDS];
    for(unsigned i=0;i<15;i++) if(before->r[i]!=after->r[i]) {
        ids[count]=aot_reg_ids[i];values[count++]=(void *)&after->r[i];
    }
    for(unsigned i=0;i<32;i++) if(before->s[i]!=after->s[i]) {
        ids[count]=aot_reg_ids[17+i];values[count++]=(void *)&after->s[i];
    }
    if(before->fpscr!=after->fpscr) {
        ids[count]=UC_ARM_REG_FPSCR;values[count++]=(void *)&after->fpscr;
    }
    uint32_t changed=before->xpsr^after->xpsr;
    if(changed & ~UINT32_C(0xf80f0000)) {
        ids[count]=UC_ARM_REG_XPSR;values[count++]=(void *)&after->xpsr;
        /* XPSR writes omit GE. Publish the full APSR flag set afterward. */
        ids[count]=UC_ARM_REG_APSR_NZCVQG;values[count++]=(void *)&after->xpsr;
    } else if(changed) {
        ids[count]=UC_ARM_REG_APSR_NZCVQG;values[count++]=(void *)&after->xpsr;
    }
    uc_err err=count?uc_reg_write_batch(uc,ids,values,count):UC_ERR_OK;
    if(err || before->r[15]==after->r[15]) return err;
    /* A singular PC write redirects a live TCG block. A same-PC exit must
     * leave the original instruction running instead of re-entering AOT. */
    uint32_t pc=after->r[15]|1u;
    return uc_reg_write(uc,UC_ARM_REG_PC,&pc);
}
static int dispatch_read(void *ctx,uint32_t addr,uint32_t size,uint32_t *value) {
    m8rt *rt=ctx;rt->dispatch_service=true;return slow_read(ctx,addr,size,value);
}
static int dispatch_write(void *ctx,uint32_t addr,uint32_t size,uint32_t value) {
    m8rt *rt=ctx;rt->dispatch_service=true;return slow_write(ctx,addr,size,value);
}
static void aot_hook(uc_engine *uc,uint64_t pc,uint32_t size,void *user) {
    (void)size;m8rt_aot_entry *entry=user;m8rt *rt=entry->rt;
#ifdef M8RT_NATIVE_ENGINE
    if(m8rt_native_live_reference_entry(rt)) return;
#endif
    if(!direct_memory_valid(rt)) return;
    m8_aot_fn fn=rt->aot_lookup((uint32_t)pc);if(!fn) return;
    if(should_stop(rt)) { if(rt->guest_count_before_aot) guest_count_discard(rt,(uint32_t)pc);m8rt_engine_stop(rt);return; }
    uint64_t start=m8rt_callback_start(rt->aot_timing_enabled);
    m8_cpu cpu;void *ptrs[AOT_CPU_WORDS];
    for(unsigned i=0;i<AOT_CPU_WORDS;i++) ptrs[i]=(uint8_t *)&cpu+i*sizeof(uint32_t);
    uc_err err=uc_reg_read_batch(uc,(int *)aot_reg_ids,ptrs,AOT_CPU_WORDS);
    if(err) { m8rt_fail(rt,"AOT register read entry=%08x: %s",(uint32_t)pc,uc_strerror(err));return; }
    /* Callers must supply entry PCs outside IT blocks. Unicorn can keep live
     * IT state inside TCG, so the visible XPSR guard is only an extra check. */
    if(cpu.xpsr & UINT32_C(0x0600fc00)) return;
    m8_cpu before=cpu;
    rt->aot_active=true;
    /* Cooperative code must exit before any CPS/MSR mask instruction. Cache
     * lazily so native calls without NVIC accesses need no extra register read. */
    rt->aot_mask_cache=rt->aot_services?AOT_MASK_UNREAD:AOT_MASK_UNCACHED;
    rt->dispatch_service=false;
    uint64_t native_start=rt->aot_timing_enabled?m8rt_now_ns():0;
    int result=fn(&cpu,&rt->memory);
    uint64_t native_ns=rt->aot_timing_enabled?m8rt_now_ns()-native_start:0;
    rt->aot_active=false;
    rt->aot_mask_cache=AOT_MASK_UNCACHED;
    if(rt->dispatch_stats_enabled) {
        ++rt->dispatch_stats.aot_calls;
        if(cpu.r[15]==before.r[15]) ++rt->dispatch_stats.same_pc;
        else if(result==M8_AOT_EXIT && !(cpu.xpsr & UINT32_C(0x0600fc00)) && rt->aot_lookup(cpu.r[15])) {
            ++rt->dispatch_stats.lookup_resumes;
            if(!rt->dispatch_service) ++rt->dispatch_stats.pure_resumes;
            for(size_t i=0;i<rt->aot_count;i++) if(rt->aot_entries[i].timing.pc==cpu.r[15]) {
                ++rt->dispatch_stats.installed_resumes;break;
            }
        }
    }
    if(rt->guest_count_before_aot && (cpu.r[15]!=before.r[15] || result!=M8_AOT_EXIT)) guest_count_discard(rt,(uint32_t)pc);
    err=aot_write_delta(uc,&before,&cpu);
    if(err) m8rt_fail(rt,"AOT register write entry=%08x pc=%08x: %s",(uint32_t)pc,cpu.r[15],uc_strerror(err));
    if(result!=M8_AOT_EXIT) m8rt_fail(rt,"AOT fault entry=%08x pc=%08x",(uint32_t)pc,cpu.r[15]);
    if(rt->aot_sdhc_pending) { rt->aot_sdhc_pending=false; if(!rt->fatal) sdhc_take(rt); }
    uint64_t hook_ns=m8rt_callback_finish(rt,start,rt->aot_timing_enabled);
    if(rt->aot_timing_enabled) {
        rt->aot_calls++;rt->aot_native_ns+=native_ns;rt->aot_hook_ns+=hook_ns;
        entry->timing.calls++;entry->timing.native_ns+=native_ns;entry->timing.hook_ns+=hook_ns;
    }
}
int m8rt_set_aot(m8rt *rt,m8_aot_fn (*lookup)(uint32_t),uint32_t (*entries)(const uint32_t **)) {
    if(rt->in_emu) return -1;
#ifdef M8RT_NATIVE_ENGINE
    m8rt_native_reset_lookup(rt);
#endif
    rt->aot_services=0;
    rt->aot_mask_cache=AOT_MASK_UNCACHED;
    for(size_t i=0;i<rt->aot_count;i++) uc_hook_del(rt->uc,rt->aot_hooks[i]);
    free(rt->aot_hooks);rt->aot_hooks=NULL;rt->aot_count=0;rt->aot_lookup=lookup;
    free(rt->aot_entries);rt->aot_entries=NULL;
    rt->aot_calls=rt->aot_native_ns=rt->aot_hook_ns=0;
    if(!lookup || !entries) return 0;
    if(rt->guest_count_enabled) rt->guest_count_before_aot=true;
    const uint32_t *pcs=NULL;uint32_t n=entries(&pcs);if(n && !pcs) return -1;
    rt->aot_hooks=calloc(n,sizeof(uc_hook));if(n && !rt->aot_hooks) return -1;
    rt->aot_entries=calloc(n,sizeof(*rt->aot_entries));if(n && !rt->aot_entries) return -1;
    for(unsigned i=0;i<n;i++) {
        rt->aot_entries[i].rt=rt;rt->aot_entries[i].timing.pc=pcs[i]&~1u;
        uc_err err=uc_hook_add(rt->uc,&rt->aot_hooks[i],UC_HOOK_CODE,aot_hook,&rt->aot_entries[i],pcs[i]&~1u,pcs[i]&~1u);
        if(err) return -1;
        rt->aot_count++;
    }
    return 0;
}
int m8rt_set_aot_services(m8rt *rt,unsigned flags) {
    if(!rt || rt->in_emu || (flags & ~M8_AOT_SERVICE_NVIC)) return -1;
    rt->aot_services=flags;rt->aot_mask_cache=AOT_MASK_UNCACHED;return 0;
}
int m8rt_set_dispatch_stats(m8rt *rt,int enabled) {
    if(!rt || rt->in_emu) return -1;
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active && rt->native.device_direct) return -1;
#endif
    if(enabled) memset(&rt->dispatch_stats,0,sizeof rt->dispatch_stats);
    rt->dispatch_stats_enabled=enabled!=0;
    rt->memory.read=enabled?dispatch_read:slow_read;
    rt->memory.write=enabled?dispatch_write:slow_write;
    return 0;
}
int m8rt_read_dispatch_stats(m8rt *rt,m8rt_dispatch_stats *out) {
    if(!rt || !out || rt->in_emu) return -1;
    *out=rt->dispatch_stats;return 0;
}
size_t m8rt_read_aot_timing(m8rt *rt,m8rt_aot_timing *out,size_t count) {
    if(!rt || rt->in_emu) return 0;
    if(!out) return rt->aot_count;
    if(count>rt->aot_count) count=rt->aot_count;
    for(size_t i=0;i<count;i++) out[i]=rt->aot_entries[i].timing;
    return count;
}
static int compare_blocks(const void *a,const void *b) { const m8rt_block *x=a,*y=b;return x->hits<y->hits?1:x->hits>y->hits?-1:0; }
int m8rt_reset_profile(m8rt *rt) {
    if(!rt || rt->in_emu) return -1;
    if(rt->blocks) memset(rt->blocks,0,rt->block_cap*sizeof(*rt->blocks));
    rt->block_used=0;
    return 0;
}
int m8rt_dump_profile(m8rt *rt,const char *path) {
    FILE *f=fopen(path,"w");if(!f) return -1;
    m8rt_block *sorted=malloc(rt->block_used*sizeof(*sorted));if(rt->block_used && !sorted) { fclose(f);return -1; }
    size_t n=0;for(size_t i=0;i<rt->block_cap;i++) if(rt->blocks[i].hits) sorted[n++]=rt->blocks[i];qsort(sorted,n,sizeof(*sorted),compare_blocks);
    fprintf(f,"[\n");for(size_t i=0;i<n;i++) fprintf(f,"%s{\"pc\":%u,\"size\":%u,\"entries\":%llu,\"halfwords\":%llu}",i?",\n":"",sorted[i].pc,sorted[i].size,(unsigned long long)sorted[i].hits,(unsigned long long)(sorted[i].hits*(sorted[i].size/2)));
    fprintf(f,"\n]\n");free(sorted);return fclose(f)?-1:0;
}
