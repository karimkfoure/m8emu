#ifndef M8RT_NATIVE_LIVE_H
#define M8RT_NATIVE_LIVE_H
#include <m8emu/m8rt.h>
#include "native-engine.h"

enum m8rt_native_leave_reason {
    M8RT_NATIVE_LEAVE_UNSUPPORTED, M8RT_NATIVE_LEAVE_ARCHITECTURE,
    M8RT_NATIVE_LEAVE_STORAGE, M8RT_NATIVE_LEAVE_RECORDING,
    M8RT_NATIVE_LEAVE_CODE_WRITE, M8RT_NATIVE_LEAVE_RESET,
    M8RT_NATIVE_LEAVE_DISABLED, M8RT_NATIVE_LEAVE_COUNT
};
enum m8rt_native_arch_leave {
    M8RT_NATIVE_ARCH_LDREX, M8RT_NATIVE_ARCH_STREX, M8RT_NATIVE_ARCH_CLREX,
    M8RT_NATIVE_ARCH_MSR_MSP, M8RT_NATIVE_ARCH_MSR_PSP,
    M8RT_NATIVE_ARCH_MSR_CONTROL, M8RT_NATIVE_ARCH_OTHER, M8RT_NATIVE_ARCH_COUNT
};
/* Rejection observations are separate from the existing statistics ABI. */
enum m8rt_native_reject_reason {
    M8RT_NATIVE_REJECT_IRQ_NEST, M8RT_NATIVE_REJECT_IPSR,
    M8RT_NATIVE_REJECT_ITSTATE, M8RT_NATIVE_REJECT_THUMB,
    M8RT_NATIVE_REJECT_CONTROL, M8RT_NATIVE_REJECT_AOT_ACTIVE,
    M8RT_NATIVE_REJECT_SDHC_PENDING, M8RT_NATIVE_REJECT_NVIC_PENDING,
    M8RT_NATIVE_REJECT_SYSTICK_PENDING, M8RT_NATIVE_REJECT_AUDIO_PENDING,
    M8RT_NATIVE_REJECT_USB_PENDING, M8RT_NATIVE_REJECT_PRIMASK,
    M8RT_NATIVE_REJECT_BASEPRI, M8RT_NATIVE_REJECT_FAULTMASK,
    M8RT_NATIVE_REJECT_EXCLUSIVE, M8RT_NATIVE_REJECT_SOURCE,
    M8RT_NATIVE_REJECT_COUNT
};
typedef struct {
    uint64_t count;
    uint32_t pc, reason, value;
    uint64_t reasons[M8RT_NATIVE_REJECT_COUNT];
    uint32_t first_pc[M8RT_NATIVE_REJECT_COUNT], first_value[M8RT_NATIVE_REJECT_COUNT];
} m8rt_native_live_rejection;
#define M8RT_NATIVE_HANDOFF_PCS 32
typedef struct {
    uint32_t pc, reason, architecture;
    uint64_t count;
} m8rt_native_handoff_pc;
#define M8RT_NATIVE_HANDOFF_DETAILS 64
typedef struct {
    uint32_t pc, reason, instruction, instruction_bytes, instruction_valid;
    uint32_t target, target_valid;
    uint64_t count, unicorn_owned_ns;
} m8rt_native_handoff_detail;
typedef struct {
    unsigned enabled;
    uint64_t overflow, overflow_unicorn_owned_ns;
    m8rt_native_handoff_detail rows[M8RT_NATIVE_HANDOFF_DETAILS];
} m8rt_native_handoff_profile;
typedef struct {
    m8rt_native_step_fn step;
    int (*verify)(m8_mem *,uint32_t *);
    m8rt_native_step_fn region;
    int (*region_verify)(m8_mem *,uint32_t *);
    m8_native_region_chain_fn chain;
    size_t chain_cache_slots;
    m8_native_region_event_fn event_chain;
} m8rt_native_live_config;
/* Explicit cold application owner. Keep the checked cold step as the fallback
 * and add the qualified warm graph only at a clean post-USB main boundary. */
typedef struct {
    m8rt_native_step_fn cold_step;
    int (*cold_verify)(m8_mem *,uint32_t *);
    m8rt_native_live_config warm;
    m8_aot_fn (*lookup)(uint32_t);
    uint32_t (*entries)(const uint32_t **);
    unsigned services;
} m8rt_native_boot_app_config;
typedef struct {
    int selected, warm;
    uint64_t boot_elapsed_ns, boot_main_iters, boot_cycles;
    uint32_t boot_pc;
} m8rt_native_boot_app_stats;
int m8rt_native_boot_app_available(void);
int m8rt_set_native_boot_app(m8rt *,const m8rt_native_boot_app_config *);
/* Explicit observation only: no stop, device reads or guest clock changes.
 * The endpoint is the first clean post-USB main entry, before warm install. */
int m8rt_measure_native_boot_app(m8rt *);
int m8rt_read_native_boot_app_stats(m8rt *,m8rt_native_boot_app_stats *);
typedef struct {
    uint64_t enters, leaves, rejected_entries;
    uint64_t reasons[M8RT_NATIVE_LEAVE_COUNT];
    uint64_t aot_calls, region_calls, region_guest, step_calls, step_guest;
    uint64_t source_checks;
    uint64_t cpu_import_words, cpu_export_words, pc_redirects;
    uint32_t last_leave_pc, last_leave_reason;
    int enabled, active, code_disabled;
    uint64_t architecture[M8RT_NATIVE_ARCH_COUNT], handoff_pc_overflow;
    m8rt_native_handoff_pc handoff_pcs[M8RT_NATIVE_HANDOFF_PCS];
    m8rt_native_event_stats events;
    int event_loop;
} m8rt_native_live_stats;

/* All calls belong to the guest owner, outside execution. Enable requires
 * deterministic clocks, strict memory, fixed direct RAM and guarded libraries.
 * NULL disables the policy and publishes any active CPU without execution. */
int m8rt_native_live_available(void);
int m8rt_set_native_live(m8rt *,const m8rt_native_live_config *);
int m8rt_read_native_live_stats(m8rt *,m8rt_native_live_stats *);
/* Return the last rejected boundary and its first failed predicate. value is
 * the already-read operand; source rejection uses the mismatched address,
 * and exclusive rejection uses the import result. Per-reason counts and first
 * operands retain rejections between host observations. No guest reads occur. */
int m8rt_read_native_live_rejection(m8rt *,m8rt_native_live_rejection *);
/* Opt-in diagnostics, outside execution. Enable resets the bins. Code comes
 * only from validated ITCM/flash backing; BLX targets use native registers.
 * Owned time includes all Unicorn callbacks until the next native entry.
 * Reading includes an open reference interval without changing its state. */
int m8rt_set_native_handoff_profile(m8rt *,int enabled);
int m8rt_read_native_handoff_profile(m8rt *,m8rt_native_handoff_profile *);
const char *m8rt_native_reject_name(unsigned);
const char *m8rt_native_leave_name(unsigned);
const char *m8rt_native_arch_leave_name(unsigned);
#endif
