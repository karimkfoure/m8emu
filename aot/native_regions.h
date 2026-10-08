/* Default-off playback regions. The normal Unicorn path remains the reference. */
#ifndef M8_NATIVE_REGIONS_H
#define M8_NATIVE_REGIONS_H
#include "native_step.h"
#include <stdbool.h>
#include <stddef.h>

enum {
    M8_NATIVE_EXIT_STATIC=1, M8_NATIVE_EXIT_FALLTHROUGH,
    M8_NATIVE_EXIT_REDIRECT, M8_NATIVE_EXIT_BUDGET, M8_NATIVE_EXIT_MEMORY,
    M8_NATIVE_EXIT_REQUIRED, M8_NATIVE_EXIT_FAULT
};
typedef struct {
    uint32_t source_pc, target_pc, reason, edge_id;
} m8_native_region_exit;
typedef int (*m8_native_region_fn)(m8_cpu *,m8_mem *,m8_native_arch *,
                                   m8_native_region_exit *);
typedef struct {
    m8_aot_fn aot;
    m8_native_region_fn region;
    uint64_t generation;
    uint32_t pc;
    bool valid, has_fp;
} m8_native_chain_slot;
/* All mutable cache state belongs to one runtime. Only pure PC resolution
 * occurs on a cache miss. The runtime owns generation, service completion
 * and masks; these pointers remain live for the complete bounded call. */
typedef struct {
    void *user;
    m8_aot_fn (*lookup)(void *,uint32_t);
    const uint64_t *generation;
    const bool *service_exit;
    unsigned *mask_cache;
    unsigned mask_seed;
    m8_native_chain_slot *slots;
    size_t slot_count;
    unsigned max_hops;
    uint64_t aot_calls, region_calls, region_guest;
    uint64_t cache_hits, cache_misses, hops;
    uint32_t exit_reason;
} m8_native_chain;
typedef int (*m8_native_region_chain_fn)(m8_cpu *,m8_mem *,m8_native_arch *,
                                        m8_native_chain *);
#if defined(__GNUC__) || defined(__clang__)
#define M8_NATIVE_REGION_EXPORT __attribute__((visibility("default")))
#else
#define M8_NATIVE_REGION_EXPORT
#endif

/* 0: a completed prefix, with PC at the next runtime boundary.
 * 1: fault, with completed register/memory effects retained.
 * 2: no instruction effects; use the existing exact step handler at this PC.
 * last_executed counts completed, predicate-true instructions; last_special=0.
 * Each call consumes at most the generated budget of instruction positions,
 * with checks at basic-block entries. Wrapped multiword stores fault before
 * effects. Memory helpers must mark every slow access and retain its result.
 * Entry with live ITSTATE is declined. A slow access in an IT block publishes
 * the remaining ITSTATE so the exact step handler can continue without replay.
 * The caller owns clocks, IRQs, hooks, exception returns and code invalidation.
 */
M8_NATIVE_REGION_EXPORT int m8_native_region_step(m8_cpu *cpu, m8_mem *mem,
                                                m8_native_arch *arch);
/* Call before execution and after any code mutation. Execution omits code
 * byte checks and therefore requires immutable verified code between calls. */
M8_NATIVE_REGION_EXPORT int m8_native_region_verify_memory(m8_mem *mem,
                                                         uint32_t *bad_pc);
/* Optional marker, present only with --inline-services. No ABI field changes. */
M8_NATIVE_REGION_EXPORT int m8_native_region_inline_services(void);
/* Optional marker: word LDREX/STREX recipes are present with
 * --ram-exclusives or --inline-services. Live switching needs the explicit
 * runtime monitor-transfer contract before admitting either form. */
M8_NATIVE_REGION_EXPORT int m8_native_region_ram_exclusives(void);
/* Optional exports in a --chain-ready build. The cache contains static exit
 * slots followed by a small entry cache. All effects and counts are retained
 * on a forced return; an unknown entry with no effects returns 2. */
M8_NATIVE_REGION_EXPORT size_t m8_native_region_chain_cache_slots(void);
M8_NATIVE_REGION_EXPORT int m8_native_region_chain(m8_cpu *,m8_mem *,
                                                 m8_native_arch *,m8_native_chain *);
#undef M8_NATIVE_REGION_EXPORT
#endif
