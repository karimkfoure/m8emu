/* Shared ABI between the m8rt runtime and AOT-translated guest code.
 * Owned by the AOT lane (aot/m8_aot.h); m8rt includes it.
 * Portable C11: must build on x86-64 Linux and aarch64 (Android later).
 */
#ifndef M8_AOT_H
#define M8_AOT_H
#include <stdint.h>

typedef struct m8_cpu {
    uint32_t r[16];   /* r0-r12, r13 = active SP, r14 = LR, r15 = PC (no Thumb bit) */
    uint32_t xpsr;    /* N Z C V Q (31..27), GE (19..16); IT bits must be zero at entry/exit */
    uint32_t s[32];   /* VFP single-precision registers; d[n] = s[2n] | s[2n+1] << 32 */
    uint32_t fpscr;   /* full FPSCR: NZCV, DN (25), FZ (24), RMode, sticky flags */
} m8_cpu;

typedef struct m8_region {
    uint8_t *host;    /* host pointer to guest bytes (little endian), NULL if absent */
    uint32_t base;
    uint32_t size;
} m8_region;

typedef struct m8_mem {
    m8_region itcm;   /* 0x00000000 */
    m8_region dtcm;   /* 0x20000000 */
    m8_region ocram;  /* 0x20200000 */
    m8_region flash;  /* 0x60000000 */
    void *ctx;
    /* Slow path for any other address (MMIO, unmapped). Return 0 on success
     * and require an exit after this instruction so the runtime can deliver
     * IRQs. Return M8_AOT_MEM_CONTINUE only for a negotiated service whose
     * complete access requires no runtime action. Any other result is a fault. */
    int (*read)(void *ctx, uint32_t addr, uint32_t size, uint32_t *value);
    int (*write)(void *ctx, uint32_t addr, uint32_t size, uint32_t value);
} m8_mem;

/* Translated code runs from cpu->r[15] and returns when it reaches a guest
 * instruction it does not execute natively (function return address,
 * untranslated callee, unsupported instruction, slow-path access, ...).
 * On return cpu holds the complete architectural state at that boundary and
 * cpu->r[15] is the next guest PC to execute. */
#define M8_AOT_EXIT  0
#define M8_AOT_FAULT 1   /* faulting instruction PC; completed memory effects remain */
#define M8_AOT_MEM_CONTINUE 2 /* completed qualified access; no runtime service is due */
#define M8_AOT_SERVICE_NVIC 1u

typedef int (*m8_aot_fn)(m8_cpu *cpu, m8_mem *mem);

/* NULL if entry_pc has no translation. */
m8_aot_fn m8_aot_lookup(uint32_t entry_pc);
/* All translated entry PCs (for installing hooks). */
uint32_t m8_aot_entries(const uint32_t **pcs);
/* Optional cooperative service request. The CPU and memory layouts stay fixed.
 * Without this export or with zero flags, callbacks retain the original exits. */
uint32_t m8_aot_memory_services(void);
/* Optional creation requirement. A release fixed-offset library returns 1;
 * generic, diagnostic and counted libraries return 0. Read before creating
 * the runtime so memory is mapped once, without changing the environment. */
uint32_t m8_aot_fixed_memory_required(void);

/* Additive optional exports. These do not change the CPU or memory layout. */
int m8_aot_verify_memory(m8_mem *mem, uint32_t *bad_pc);
/* Python Machine can abandon an MMIO instruction at a wall deadline. Use 1
 * for that driver: let Unicorn perform non-region accesses itself. Default 0
 * invokes memory callbacks and exits after the completed instruction. */
void m8_aot_set_slow_fallback(int enabled);
/* Prefix controls operate only in a build with M8_AOT_DIAGNOSTIC. A step is
 * one instruction or one complete IT block; the release build omits checks. */
void m8_aot_set_stop_pc(uint32_t pc);
void m8_aot_set_stop_after(uint64_t units);
uint64_t m8_aot_get_steps(void);
/* Counted builds disable loop specializations. The thread-local counter counts
 * completed original instructions. Predicate-suppressed IT members are excluded.
 * IT rollback and pre-access fallback remove instructions that Unicorn repeats.
 * Reset and read only on the runtime execution thread, outside native calls. */
uint32_t m8_aot_guest_count_enabled(void);
uint64_t m8_aot_guest_instructions(void);
void m8_aot_reset_guest_instructions(void);

#endif
