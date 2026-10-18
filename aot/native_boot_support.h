/* Cold boot handlers own instructions only. The runtime owns time, devices,
 * reset CPU setup, first FP activation, and the checked initial ITCM copy. */
#ifndef M8_NATIVE_BOOT_SUPPORT_H
#define M8_NATIVE_BOOT_SUPPORT_H
#include "native_step_special.h"
#include <stddef.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define M8_NATIVE_BOOT_EXPORT __attribute__((visibility("default")))
#else
#define M8_NATIVE_BOOT_EXPORT
#endif
M8_NATIVE_BOOT_EXPORT int m8_native_step(m8_cpu *,m8_mem *,m8_native_arch *);
M8_NATIVE_BOOT_EXPORT int m8_native_boot_verify_memory(m8_mem *,uint32_t *);
M8_NATIVE_BOOT_EXPORT uint32_t m8_native_boot_abi(void);
M8_NATIVE_BOOT_EXPORT const char *m8_native_boot_firmware_sha256(void);

/* Only these two original DMB ISH instructions extend the existing generator.
 * A changed instruction declines before effects, including ITSTATE effects. */
static inline int m8_native_boot_barrier(m8_cpu *cpu,m8_mem *mem,
                                        m8_native_arch *arch) {
    uint32_t pc=cpu->r[15];
    if(pc!=0x21bd4u && pc!=0x21c48u) return 2;
    if(!m8_native_code_matches(mem,pc,4,0x8f5bf3bfu)) return 2;
    unsigned executed=!arch->itstate ||
        m8_condition(cpu->xpsr,(arch->itstate>>4)+1u);
    /* Guest RAM and device callbacks are synchronous on the execution thread.
     * No guest DMA worker can reorder these completed accesses. */
    if(executed) {__asm__ volatile("" ::: "memory");}
    if(arch->itstate) m8_native_advance_it(cpu,arch);
    cpu->r[15]=pc+4;
    m8_native_save_sp(cpu,arch);
    arch->last_executed=executed;
    arch->last_special=executed;
    return 0;
}
#endif
