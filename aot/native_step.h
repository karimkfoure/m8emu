/* Default-off playback prototype. This state is separate from the AOT ABI. */
#ifndef M8_NATIVE_STEP_H
#define M8_NATIVE_STEP_H
#include "m8_aot.h"

typedef struct m8_native_arch {
    uint32_t primask, basepri, faultmask, control;
    uint32_t msp, psp, ipsr, itstate;
    uint32_t exclusive_address, exclusive_valid, exclusive_value;
    uint32_t cpsr, fpexc;
    uint32_t last_executed, last_special;
} m8_native_arch;

/* 0: one instruction completed; 1: fault with completed effects preserved;
 * 2: unknown encoding or changed code, with no instruction effects. The
 * caller owns clocks, interrupt delivery and exception entry/return. A build
 * generated with --blocks completes at most 256 instructions and exits before
 * runtime boundaries, after slow memory access, or at unknown code. In that
 * build last_executed and last_special are counts for the completed block. */
int m8_native_step(m8_cpu *cpu, m8_mem *mem, m8_native_arch *arch);

/* Optional exports, present only in a library generated with --pc-profile.
 * Use them on the runtime execution thread, outside native calls. Counts
 * include completed IT instructions and exclude predicate-false members.
 * A faulting instruction contributes no count; its completed prefix does. */
typedef struct m8_native_pc_count {
    uint32_t pc;
    uint64_t instructions;
    uint64_t special;
} m8_native_pc_count;
#if defined(__GNUC__) || defined(__clang__)
#define M8_NATIVE_PROFILE_EXPORT __attribute__((visibility("default")))
#else
#define M8_NATIVE_PROFILE_EXPORT
#endif
M8_NATIVE_PROFILE_EXPORT uint32_t m8_native_pc_read(const m8_native_pc_count **rows);
M8_NATIVE_PROFILE_EXPORT void m8_native_pc_reset(void);
/* Write m8-native-pc/1 JSON. Return 0 on success or -1 on an output error. */
M8_NATIVE_PROFILE_EXPORT int m8_native_pc_write(const char *path);
#undef M8_NATIVE_PROFILE_EXPORT

static inline uint32_t m8_native_it_bits(uint32_t state) {
    return ((state & 3u) << 25) | ((state & 0xfcu) << 8);
}
static inline uint32_t m8_native_it_from_xpsr(uint32_t xpsr) {
    return ((xpsr >> 25) & 3u) | ((xpsr >> 8) & 0xfcu);
}
static inline void m8_native_publish_it(m8_cpu *cpu, m8_native_arch *arch) {
    cpu->xpsr = (cpu->xpsr & ~0x0600fc00u) |
        m8_native_it_bits(arch->itstate);
}
#endif
