#ifndef M8_NATIVE_REGION_SERVICES_H
#define M8_NATIVE_REGION_SERVICES_H
#include "native_region_memory.h"

/* Same region direct-call contract as integer.h. The region generator enables
 * this only with --direct-calls (default off); all other users of this header
 * keep a shared out-of-line copy. */
#ifndef M8_REGION_INLINE
#if defined(M8_NATIVE_REGION_DIRECT_CALLS) && M8_NATIVE_REGION_DIRECT_CALLS
#define M8_REGION_INLINE static inline __attribute__((always_inline))
#else
#define M8_REGION_INLINE static inline
#endif
#endif

/* Match the tiny handler's observed, aligned RAM-word exclusive contract.
 * Device exclusives retain the step-handler boundary before any effects. */
M8_REGION_INLINE uint8_t *m8_native_region_exclusive_pointer(m8_mem *mem,uint32_t addr) {
    if((uint32_t)(addr-UINT32_C(0x20000000))<=UINT32_C(0x80000)-4 ||
       (uint32_t)(addr-UINT32_C(0x20200000))<=UINT32_C(0x80000)-4)
        return M8_NATIVE_REGION_RAM_POINTER(mem,addr,4);
    return NULL;
}
#endif
