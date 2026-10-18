#ifndef M8RT_MMIO_STATE_H
#define M8RT_MMIO_STATE_H

/* Private device backing. Native callbacks use the same state owner as the
 * generic bus; no guest register values or transfer descriptors are cached. */
typedef struct { uint32_t address, value; } raw_register;
struct m8rt_mmio {
    uint32_t *periph, gpio[0x10000 / 4], scs[0x1000 / 4];
    raw_register *extra;
    size_t extra_count, extra_cap;
    int route, clock_route;
    bool route_dirty;
    uint32_t clock_last;
    uint64_t fraction, pending;
    uint64_t rtc_last_cycles, rtc_fraction[2];
};

#ifdef M8RT_NATIVE_ENGINE
static inline void m8rt_native_sai_service(m8rt *rt) {
    m8rt_mmio *m=rt->mmio;
    /* Bypass only a clean, active, unchanged route. All pending words,
     * clock changes, invalidation and faults keep the original owner. */
    if(!m->route_dirty && !rt->fatal && m->route>=0 &&
       m->route==m->clock_route && m->clock_last==rt->cyccnt && !m->pending) return;
    m8rt_sai_service(rt);
}
#endif
#endif
