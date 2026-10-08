#ifndef M8RT_INTERNAL_H
#define M8RT_INTERNAL_H
#define _POSIX_C_SOURCE 200809L
#include <m8emu/m8rt.h>
#include "storage.h"
#include "native-live.h"
#include <unicorn/unicorn.h>
#include <unicorn/arm.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdatomic.h>
#if defined(M8RT_LOCAL_CALLS) && defined(__ELF__) && (defined(__GNUC__) || defined(__clang__))
/* Keep test/diagnostic symbol access, but bind calls within this DSO locally.
 * Public API declarations in m8rt.h and storage.h retain default visibility. */
#define M8RT_LOCAL __attribute__((visibility("protected")))
#else
#define M8RT_LOCAL
#endif
typedef struct { uint8_t *data; size_t len, cap, head; } m8rt_queue;
typedef struct m8rt_mmio m8rt_mmio;
typedef struct m8rt_usb m8rt_usb;
typedef struct m8rt_sd m8rt_sd;
typedef struct m8rt_audit m8rt_audit;
typedef struct m8rt_nor m8rt_nor;
typedef struct m8rt_stall_guard m8rt_stall_guard;
#ifdef M8RT_ACCESS_PROFILE
typedef struct m8rt_access_profile m8rt_access_profile;
#endif
typedef struct { uint32_t pc, size; uint64_t hits; } m8rt_block;
typedef struct {
    m8rt *rt;
    m8rt_aot_timing timing;
} m8rt_aot_entry;
struct m8rt {
    uc_engine *uc;
    uc_context *reset_context;
#ifdef M8RT_NATIVE_ENGINE
    struct {
        m8_cpu cpu;
        m8_native_arch arch;
        m8rt_native_step_fn step;
        m8rt_native_step_fn region;
        m8_native_region_chain_fn chain;
        m8_native_chain chain_state;
        m8_native_region_event_fn event_chain;
        m8_native_events events;
        m8rt_native_event_stats event_stats;
        uint32_t event_polls;
        uint64_t generation, service_epoch;
        uint32_t service_polls;
        m8rt_native_stats stats;
        struct m8rt_native_lookup_entry *lookup_cache;
        uint32_t cpsr;
        bool active, stopped;
        bool code_invalidated, service_exit, service_continue;
        bool event_loop, event_slice_finished, device_direct;
        bool exclusive_transfer;
        uint64_t exclusive_uc[3];
        uint32_t exclusive_shadow[3];
        bool boot, boot_callback_irq;
        uint32_t boot_copied;
        uint64_t boot_callback_irqs;
        m8_cpu boot_irq_cpu;
        m8_native_arch boot_irq_arch;
    } native;
    struct {
        m8rt_native_live_config config;
        m8rt_native_live_stats stats;
        m8rt_native_live_rejection rejection;
        m8rt_native_handoff_profile handoff_profile;
        bool handoff_interval_open;
        unsigned handoff_interval_bin;
        uint64_t handoff_interval_uc_start;
        bool enabled, verified, enter_pending, leave_pending, code_disabled, completed_stop;
        bool verify_cache, invalidate_pending, write_observer;
        bool reuse_state, initialized, delta_state, shadow_valid;
        bool exclusive_rejected;
        bool deadline_yield, deadline_seen;
        m8_cpu uc_shadow;
        uint32_t uc_masks[4];
        uc_hook code_hooks[2];
        unsigned reason;
        uint32_t leave_pc;
    } native_live;
    struct {
        m8rt_native_boot_app_config config;
        m8rt_native_boot_app_stats stats;
        uint64_t started_ns;
        bool admission, pending, ready, measuring;
        bool bus_pending, bus_executing, bus_read;
        uint32_t bus_address, bus_size, bus_value;
        uint64_t bus_declines, bus_steps;
        int (*bus_read_fn)(void *,uint32_t,uint32_t,uint32_t *);
        int (*bus_write_fn)(void *,uint32_t,uint32_t,uint32_t);
    } native_boot_app;
#endif
    m8_mem memory;
    uint8_t *rom, *dwt;
    uint8_t *memory_offset;
    void *memory_reservation;
    size_t memory_reservation_size;
    m8rt_mmio *mmio;
    m8rt_usb *usb;
    m8rt_sd *sd;
    m8rt_nor *nor;
    m8rt_options options;
    m8rt_audit *audit;
    m8rt_watchdog_status watchdog;
    bool watchdog_service_armed, watchdog_control_locked, watchdog_interrupt_locked;
    bool watchdog_warned;
    bool deterministic_clock;
    m8rt_stall_guard *stall_guard;
    uc_hook watchdog_stall_hook;
    uint32_t watchdog_slice_instructions;
    bool watchdog_counted_stop;
    uint64_t watchdog_no_clock_since_ns;
    uint64_t boot_main_start;
    uint32_t cyccnt, vtor, syst_csr, syst_rvr, syst_cvr;
    uint32_t nvic_iser[8], nvic_ispr[8], nvic_ipr[64];
    uint32_t nvic_pending_banks;
    uint64_t nvic_pend_seq[256], nvic_isr_done[256];
    uint32_t next_systick, next_sof, next_audio;
    bool systick_pending, audio_pending, usb_pending, entry_pending, er_sent;
    int audio_ch, irq_nest;
    uint32_t exc_stack[32];
    bool sdhc_in_sync, started, fatal, in_emu, aot_active, aot_sdhc_pending, sampling_pending;
    atomic_bool host_stop;
    uint64_t main_iters, slices, stop_iters, stop_frames, pcm_total, cdc_total;
    uint64_t uc_ns, callback_ns, callback_count, deadline_ns;
    uint64_t main_selected, main_expanded, main_cycles;
    uint64_t guest_cycles, local_total, local_blocks, local_last_cycles;
    bool local_enabled, local_selected, local_capture, audio_limit_local;
    unsigned local_phase;
    uint16_t local_head;
    uint32_t local_sp, local_receiver, local_writer;
    uint8_t local_raw[512], local_flags[2];
    uc_hook local_hooks[3];
    bool yield_boot;
    uint32_t entry, last_pc;
    char error[512];
    m8rt_queue cdc, pcm, irqs, pcm_lengths, pcm_positions, local_pcm, local_positions;
    uint32_t pcm_packet_left;
    m8rt_audio_position pcm_position;
    m8rt_block *blocks; size_t block_cap, block_used;
    m8_aot_fn (*aot_lookup)(uint32_t);
    uc_hook *aot_hooks; size_t aot_count;
    m8rt_aot_entry *aot_entries;
    bool aot_timing_enabled;
    bool guest_count_enabled, guest_count_last_valid, guest_count_before_aot;
    uc_hook guest_count_hook;
    uint32_t guest_count_last_pc;
    uint64_t jit_guest_instructions;
#ifdef M8RT_GUEST_PC_PROFILE
    m8rt_guest_pc *guest_pcs, *guest_pc_last;
    size_t guest_pc_used;
    bool guest_pc_enabled;
#endif
    uint64_t aot_calls, aot_native_ns, aot_hook_ns;
    unsigned aot_services;
    unsigned aot_mask_cache;
    uint32_t aot_mask_cpsr;
    bool dispatch_stats_enabled;
    bool dispatch_service, direct_memory;
    const uint64_t *direct_memory_generation;
    uint64_t direct_memory_expected_generation;
    m8rt_dispatch_stats dispatch_stats;
#ifdef M8RT_ACCESS_PROFILE
    m8rt_access_profile *access_profile;
#endif
};
static inline uint16_t m8rt_le16(const void *p) { const uint8_t *b=p; return (uint16_t)(b[0]|(uint16_t)b[1]<<8); }
static inline uint32_t m8rt_le32(const void *p) { const uint8_t *b=p; return b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24; }
static inline void m8rt_st16(void *p,uint16_t v) { uint8_t *b=p; b[0]=(uint8_t)v; b[1]=(uint8_t)(v>>8); }
static inline void m8rt_st32(void *p,uint32_t v) { uint8_t *b=p; for(unsigned i=0;i<4;i++) b[i]=(uint8_t)(v>>(8*i)); }
/* Update pending state and its summary together. ISER changes do not affect
 * this summary: a nonempty bank still needs the ordinary enable-mask test. */
static inline void m8rt_nvic_pending(m8rt *rt,unsigned bank,uint32_t value) {
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active && rt->native.service_continue && rt->nvic_ispr[bank]!=value)
        rt->native.service_epoch++;
#endif
    rt->nvic_ispr[bank]=value;
    rt->nvic_pending_banks=(rt->nvic_pending_banks&~(1u<<bank)) | ((value!=0u)<<bank);
}
M8RT_LOCAL uint64_t m8rt_now_ns(void);
#ifndef M8RT_CALLBACK_TIMING_DEFAULT
#define M8RT_CALLBACK_TIMING_DEFAULT 1
#endif
#if M8RT_CALLBACK_TIMING_DEFAULT != 0 && M8RT_CALLBACK_TIMING_DEFAULT != 1
#error M8RT_CALLBACK_TIMING_DEFAULT must be 0 or 1
#endif
/* Explicit AOT timing remains available when aggregate timing is disabled.
 * These helpers do not control execution deadlines or guest clocks. */
static inline uint64_t m8rt_callback_start(bool explicit_timing) {
#if M8RT_CALLBACK_TIMING_DEFAULT
    (void)explicit_timing;
    return m8rt_now_ns();
#else
    return explicit_timing?m8rt_now_ns():0;
#endif
}
static inline uint64_t m8rt_callback_finish(m8rt *rt,uint64_t start,bool explicit_timing) {
#if M8RT_CALLBACK_TIMING_DEFAULT
    (void)explicit_timing;
    uint64_t elapsed=m8rt_now_ns()-start;
    rt->callback_ns+=elapsed;
#else
    uint64_t elapsed=explicit_timing?m8rt_now_ns()-start:0;
#endif
    rt->callback_count++;
    return elapsed;
}
#ifdef M8RT_ACCESS_PROFILE
void m8rt_access_record(m8rt *rt,uint32_t address,unsigned size,unsigned path,unsigned write);
void m8rt_access_site(m8rt *rt,uint32_t address,unsigned size,unsigned write,uintptr_t caller);
void m8rt_access_nvic(m8rt *rt,unsigned stage);
void m8rt_access_profile_destroy(m8rt *rt);
void m8rt_access_storage_phase(m8rt *,unsigned);
void m8rt_access_storage_exec(m8rt *,uint64_t);
void m8rt_access_storage_read(m8rt *,unsigned);
void m8rt_access_storage_guard(m8rt *,uint32_t,bool);
#define M8_ACCESS_RECORD(rt,addr,size,path,write) m8rt_access_record(rt,addr,size,path,write)
#define M8_ACCESS_NVIC(rt,stage) m8rt_access_nvic(rt,stage)
#if defined(M8RT_DEVICE_SITE_PROFILE) && M8RT_DEVICE_SITE_PROFILE
#define M8_DEVICE_SITE(rt,addr,size,write) m8rt_access_site(rt,addr,size,write,(uintptr_t)__builtin_return_address(0))
#else
#define M8_DEVICE_SITE(rt,addr,size,write) ((void)0)
#endif
#else
#define M8_ACCESS_RECORD(rt,addr,size,path,write) ((void)0)
#define M8_ACCESS_NVIC(rt,stage) ((void)0)
#define M8_DEVICE_SITE(rt,addr,size,write) ((void)0)
#endif
void m8rt_fail(m8rt *rt, const char *fmt, ...);
#ifdef M8RT_NATIVE_ENGINE
M8RT_LOCAL uint32_t m8rt_native_reg_read(m8rt *,int);
M8RT_LOCAL void m8rt_native_reg_write(m8rt *,int,uint32_t);
M8RT_LOCAL uc_err m8rt_native_exec(m8rt *);
M8RT_LOCAL void m8rt_native_boundary(m8rt *,uint32_t);
M8RT_LOCAL void m8rt_native_completed(m8rt *);
M8RT_LOCAL bool m8rt_native_unstack(m8rt *);
M8RT_LOCAL bool m8rt_native_idle(m8rt *,uint32_t);
M8RT_LOCAL int m8rt_native_event_enter(m8rt *,uint32_t);
M8RT_LOCAL int m8rt_native_event_completed(m8rt *,uint32_t,unsigned);
M8RT_LOCAL int m8rt_native_event_slice(m8rt *);
M8RT_LOCAL void m8rt_native_audio_boundary(m8rt *,uint32_t);
M8RT_LOCAL void m8rt_native_device_bus(m8rt *,bool);
M8RT_LOCAL int m8rt_native_boot_copy(m8rt *,uint32_t,const void *,size_t);
M8RT_LOCAL bool m8rt_native_live_entry(m8rt *,uint32_t);
M8RT_LOCAL int m8rt_native_live_switch(m8rt *);
M8RT_LOCAL bool m8rt_native_boot_app_entry(m8rt *,uint32_t);
M8RT_LOCAL int m8rt_native_boot_app_finish(m8rt *);
M8RT_LOCAL int m8rt_native_boot_app_resume(m8rt *,int);
M8RT_LOCAL void m8rt_native_boot_app_measure_entry(m8rt *,uint32_t);
M8RT_LOCAL void m8rt_native_live_leave(m8rt *,unsigned,uint32_t);
M8RT_LOCAL void m8rt_native_live_storage(m8rt *,uint32_t,bool);
M8RT_LOCAL int m8rt_native_live_host_code_write(m8rt *);
M8RT_LOCAL bool m8rt_native_live_reference_entry(m8rt *);
M8RT_LOCAL void m8rt_native_live_code_changed(m8rt *);
#endif
/* Native execution observes the same runtime stop, without entering Unicorn. */
static inline void m8rt_engine_stop(m8rt *rt) {
#ifdef M8RT_NATIVE_ENGINE
    if(rt->native.active) {
        rt->native.stopped=true;
        if(rt->native_live.enabled) rt->native_live.completed_stop=true;
        return;
    }
#endif
    uc_emu_stop(rt->uc);
}
int m8rt_queue_append(m8rt_queue *q, const void *data, size_t n);
size_t m8rt_queue_read(m8rt_queue *q, void *data, size_t n);
uint32_t m8rt_read32(m8rt *rt,uint32_t addr);
void m8rt_write32(m8rt *rt,uint32_t addr,uint32_t value);
void m8rt_pend(m8rt *rt, unsigned irq);
bool m8rt_take_irq(m8rt *rt, unsigned exc);
void m8rt_reconcile(m8rt *rt);
M8RT_LOCAL bool m8rt_irq_enabled(m8rt *rt,unsigned irq);
void m8rt_cdc_emit(m8rt *rt,const void *data,size_t n);
void m8rt_pcm_emit(m8rt *rt,const void *data,size_t n);
int m8rt_convert_audio_block(const uint8_t raw[512], const uint8_t flags[2], uint8_t pcm[256]);
/* Generic MMIO excludes USB and SDHC. Raw registers have no read effects. */
int m8rt_mmio_init(m8rt *rt);
void m8rt_mmio_destroy(m8rt *rt);
int m8rt_mmio_reset(m8rt *rt);
M8RT_LOCAL uint32_t m8rt_mmio_read(m8rt *rt,uint32_t addr);
M8RT_LOCAL void m8rt_mmio_write(m8rt *rt,uint32_t addr,uint32_t value);
M8RT_LOCAL uint32_t m8rt_raw_read(m8rt *rt,uint32_t addr);
M8RT_LOCAL void m8rt_raw_write(m8rt *rt,uint32_t addr,uint32_t value);
M8RT_LOCAL bool m8rt_mmio_modeled(uint32_t addr, bool write);
int m8rt_stall_init(m8rt *rt);
void m8rt_stall_destroy(m8rt *rt);
void m8rt_stall_enter(m8rt *rt);
bool m8rt_stall_leave(m8rt *rt);
uint64_t m8rt_stall_limit_ns(const m8rt *rt);
M8RT_LOCAL bool m8rt_watchdog_handles(uint32_t addr);
void m8rt_watchdog_init(m8rt *rt, bool reset);
M8RT_LOCAL uint32_t m8rt_watchdog_read(m8rt *rt, uint32_t addr, unsigned size);
M8RT_LOCAL void m8rt_watchdog_write(m8rt *rt, uint32_t addr, unsigned size, uint32_t value);
void m8rt_watchdog_tick(m8rt *rt);
bool m8rt_watchdog_irq(m8rt *rt);
void m8rt_usb_reset(m8rt *rt);
void m8rt_sd_reset(m8rt *rt);
int m8rt_nor_init(m8rt *rt);
void m8rt_nor_destroy(m8rt *rt);
void m8rt_nor_reset(m8rt *rt);
M8RT_LOCAL bool m8rt_nor_read(m8rt *rt, uint32_t addr, uint32_t *value);
M8RT_LOCAL bool m8rt_nor_write(m8rt *rt, uint32_t addr, uint32_t value);
void m8rt_audit_access(m8rt *rt, unsigned kind, uint64_t addr, unsigned size,
                      unsigned direction, unsigned code, uint64_t value);
void m8rt_audit_main(m8rt *rt);
void m8rt_audit_time(m8rt *rt);
void m8rt_audit_audio(m8rt *rt, uint32_t pc);
void m8rt_audit_destroy(m8rt *rt);
M8RT_LOCAL void m8rt_sai_service(m8rt *rt);
int m8rt_sai_channel(m8rt *rt);
uint32_t m8rt_sai_deadline(m8rt *rt); /* relative cycles, UINT32_MAX if inactive */
/* USB controller and host queues. */
int m8rt_usb_init(m8rt *rt);
void m8rt_usb_destroy(m8rt *rt);
M8RT_LOCAL uint32_t m8rt_usb_read(m8rt *rt,uint32_t addr);
M8RT_LOCAL void m8rt_usb_write(m8rt *rt,uint32_t addr,uint32_t value);
void m8rt_usb_poll(m8rt *rt,uint32_t cycles);
void m8rt_usb_tick_enum(m8rt *rt,uint32_t delta);
void m8rt_usb_sof(m8rt *rt);
void m8rt_usb_set_boot(m8rt *rt);
void m8rt_usb_set_capture(m8rt *rt, bool enabled);
bool m8rt_usb_audio_input_enabled(m8rt *rt);
bool m8rt_usb_boot(m8rt *rt);
bool m8rt_usb_configured(m8rt *rt);
bool m8rt_usb_dtr(m8rt *rt);
bool m8rt_usb_dtr(m8rt *rt);
size_t m8rt_usb_queued(m8rt *rt);
uint32_t m8rt_usb_deadline(m8rt *rt); /* relative, UINT32_MAX if inactive; 0 forces reference quantum */
int m8rt_usb_queue(m8rt *rt,const void *data,size_t n);
/* SDHC stores registers through raw access and pends via m8rt_pend. */
int m8rt_sd_init(m8rt *rt,const char *image);
void m8rt_sd_destroy(m8rt *rt);
M8RT_LOCAL bool m8rt_sd_handles(uint32_t addr);
M8RT_LOCAL uint32_t m8rt_sd_read(m8rt *rt,uint32_t addr);
M8RT_LOCAL void m8rt_sd_write(m8rt *rt,uint32_t addr,uint32_t value);
int m8rt_sd_flush(m8rt *rt);
int m8rt_sd_abort_read(m8rt *rt);
bool m8rt_sd_active(m8rt *rt);
#endif
