#ifndef M8_NATIVE_EVENTS_H
#define M8_NATIVE_EVENTS_H
#include "native_regions.h"

/* Additive region interface. The runtime keeps ownership of devices, clocks
 * and software exception frames; an event chain owns only their call order. */
enum { M8_NATIVE_EVENT_RUN=0, M8_NATIVE_EVENT_AGAIN=1,
       M8_NATIVE_EVENT_EXIT=2, M8_NATIVE_EVENT_HANDOFF=3,
       M8_NATIVE_EVENT_FAULT=-1 };
enum { M8_NATIVE_EVENT_ENTER=1, M8_NATIVE_EVENT_COMPLETED=2 };
typedef struct {
    void *user;
    int (*event)(void *,unsigned,uint32_t,unsigned);
    int (*step)(m8_cpu *,m8_mem *,m8_native_arch *);
    uint64_t fallback_steps, fallback_guest, interpreter_guest, segments;
} m8_native_events;
typedef int (*m8_native_region_event_fn)(m8_cpu *,m8_mem *,m8_native_arch *,
                                       m8_native_chain *,m8_native_events *);

int m8_native_region_event_loop(void);
int m8_native_region_event_chain(m8_cpu *,m8_mem *,m8_native_arch *,
                                 m8_native_chain *,m8_native_events *);
#endif
