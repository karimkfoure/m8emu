#ifndef M8RT_NATIVE_SAI_QUIET_H
#define M8RT_NATIVE_SAI_QUIET_H

#include "mmio-state.h"

/* Prove before a device callback that its SAI pre-service cannot transfer a
 * word, change the route or pend an interrupt. Read only the current owner. */
static inline bool m8rt_native_sai_quiet(const m8rt *rt) {
    const m8rt_mmio *m=rt->mmio;
    return !m->periph[0xe800c/4] ||
        (!m->route_dirty && !rt->fatal && m->route>=0 &&
         m->route==m->clock_route && m->clock_last==rt->cyccnt && !m->pending);
}

#endif
