#ifndef M8RT_NATIVE_ENGINE_H
#define M8RT_NATIVE_ENGINE_H
#include "native_regions.h"
#include "native_events.h"

typedef int (*m8rt_native_step_fn)(m8_cpu *,m8_mem *,m8_native_arch *);
typedef struct {
    uint64_t aot_calls, fallback_steps, fallback_guest, interpreter_guest, slices;
    uint64_t lookup_hits, lookup_misses;
    uint64_t region_calls, region_guest, code_invalidations;
    uint64_t dispatch_calls, chain_calls, chain_hops, chain_hits, chain_misses;
    uint32_t last_pc, unsupported_pc;
} m8rt_native_stats;
typedef struct {
    uint64_t entries, completed, logical_slices, enum_ticks;
    uint64_t main_hooks, wait_hooks, delay_hooks, sampling_hooks, audio_hooks;
    uint64_t irq_entries, exception_returns, memory_returns, required_returns;
    uint64_t budget_returns, host_returns;
} m8rt_native_event_stats;
struct m8rt_native_lookup_entry;
void m8rt_native_reset_lookup(m8rt *);
m8_aot_fn m8rt_native_lookup_pc(m8rt *,uint32_t);
/* Activate only after ordinary boot/load. The existing RAM and device owners
 * remain in place. A NULL step transfers the published CPU back to Unicorn. */
int m8rt_set_native_playback(m8rt *,m8rt_native_step_fn,
                            int (*verify)(m8_mem *,uint32_t *));
/* Default-off cold experiment. Require a fresh reset state and a checked
 * cold handler library. This owner preserves synchronous callback IRQs. */
int m8rt_set_native_boot(m8rt *,m8rt_native_step_fn,
                        int (*verify)(m8_mem *,uint32_t *));
int m8rt_native_boot_enabled(const m8rt *);
int m8rt_set_native_regions(m8rt *,m8rt_native_step_fn,
                           int (*verify)(m8_mem *,uint32_t *));
int m8rt_set_native_region_chain(m8rt *,m8_native_region_chain_fn,size_t);
int m8rt_read_native_stats(m8rt *,m8rt_native_stats *);
int m8rt_native_service_continue_enabled(const m8rt *);
int m8rt_native_device_direct_enabled(const m8rt *);
/* Optional Unicorn monitor transfer. Import returns 1 for an unrepresentable
 * reservation, before any state changes; a switching owner must stay in
 * Unicorn. Both transfers require a stopped execution owner. The public CPU,
 * memory, architecture and region interfaces remain unchanged. */
int m8rt_native_exclusive_transfer_available(void);
int m8rt_native_exclusive_transfer_enabled(const m8rt *);
int m8rt_native_import_exclusive(m8rt *);
int m8rt_native_export_exclusive(m8rt *);
int m8rt_set_native_event_loop(m8rt *,m8_native_region_event_fn);
int m8rt_native_event_loop_enabled(const m8rt *);
int m8rt_read_native_event_stats(m8rt *,m8rt_native_event_stats *);
int m8rt_dump_native_state(m8rt *,const char *prefix);
#endif
