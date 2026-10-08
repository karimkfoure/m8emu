#ifndef M8_NATIVE_REGION_SERVICES_H
#define M8_NATIVE_REGION_SERVICES_H
#include "native_region_memory.h"

/* Match the tiny handler's observed, aligned RAM-word exclusive contract.
 * Device exclusives retain the step-handler boundary before any effects. */
static inline uint8_t *m8_native_region_exclusive_pointer(m8_mem *mem,uint32_t addr) {
    if((uint32_t)(addr-UINT32_C(0x20000000))<=UINT32_C(0x80000)-4 ||
       (uint32_t)(addr-UINT32_C(0x20200000))<=UINT32_C(0x80000)-4)
        return (uint8_t *)((uintptr_t)mem->itcm.host+addr);
    return NULL;
}
#endif
