#include "internal.h"
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* One thread per runtime, with no timer creation or hook in a normal slice.
 * The execution owner publishes generations; the monitor owns STOPPING.
 * A stop can never cross generations because leave waits for STOPPING. */
enum { STALL_IDLE, STALL_ACTIVE, STALL_STOPPING, STALL_FIRED };
#define STALL_PHASE_MASK UINT64_C(3)
#define STALL_POLL_NS 50000000L
#define STALL_DEFAULT_LIMIT_NS UINT64_C(100000000)

struct m8rt_stall_guard {
    uc_engine *uc; /* Immutable until the monitor has joined. */
    pthread_t thread;
    atomic_uint_fast64_t state;
    atomic_bool quit;
    uint64_t limit_ns;
};

static void *stall_monitor(void *opaque)
{
    struct m8rt_stall_guard *guard = opaque;
    uint64_t observed = 0, since = 0;
    while (!atomic_load_explicit(&guard->quit, memory_order_acquire)) {
        struct timespec remaining = {0, STALL_POLL_NS};
        while (nanosleep(&remaining, &remaining) && errno == EINTR) {
            if (atomic_load_explicit(&guard->quit, memory_order_acquire)) return NULL;
        }
        if (atomic_load_explicit(&guard->quit, memory_order_acquire)) break;
        uint64_t state = atomic_load_explicit(&guard->state, memory_order_acquire);
        unsigned phase = (unsigned)(state & STALL_PHASE_MASK);
        if (phase != STALL_ACTIVE && phase != STALL_FIRED) {
            observed = since = 0;
            continue;
        }
        if (phase == STALL_ACTIVE) {
            uint64_t now = m8rt_now_ns();
            if (state != observed) {
                observed = state;
                since = now;
                continue;
            }
            if (now - since < guard->limit_ns) continue;
        }
        /* If the owner was descheduled between enter and uc_emu_start, its
         * first stop can be cleared by Unicorn's entry. Repeat a fired stop
         * until leave acknowledges it, using STOPPING on every attempt so
         * none can cross into a later generation. Normal slices never fire. */
        uint_fast64_t expected = state;
        uint64_t stopping = (state & ~STALL_PHASE_MASK) | STALL_STOPPING;
        if (!atomic_compare_exchange_strong_explicit(&guard->state, &expected, stopping,
                memory_order_acq_rel, memory_order_acquire)) continue;
        /* This uses Unicorn's own timeout stop mechanism. No registers,
         * clocks, peripheral state or mutable runtime fields are read here. */
        uc_emu_stop(guard->uc);
        atomic_store_explicit(&guard->state,
                (state & ~STALL_PHASE_MASK) | STALL_FIRED, memory_order_release);
        observed = since = 0;
    }
    return NULL;
}

int m8rt_stall_init(m8rt *rt)
{
    if (!rt || !rt->uc) {
        if (rt) m8rt_fail(rt, "stall guard initialization requires a Unicorn engine");
        return -1;
    }
    if (rt->stall_guard) return 0;
    struct m8rt_stall_guard *guard = calloc(1, sizeof(*guard));
    if (!guard) { m8rt_fail(rt, "stall guard allocation failed"); return -1; }
    guard->limit_ns = STALL_DEFAULT_LIMIT_NS;
    const char *setting = getenv("M8RT_HOST_STALL_MS");
    if (setting) {
        char *end = NULL;
        errno = 0;
        unsigned long milliseconds = strtoul(setting, &end, 10);
        if (!setting[0] || strspn(setting, "0123456789") != strlen(setting) || errno || *end || milliseconds < 100 || milliseconds > 60000) {
            m8rt_fail(rt, "host stall budget M8RT_HOST_STALL_MS=%s requires integer 100..60000", setting);
            free(guard);
            return -1;
        }
        guard->limit_ns = (uint64_t)milliseconds * UINT64_C(1000000);
    }
    guard->uc = rt->uc;
    atomic_init(&guard->state, STALL_IDLE);
    atomic_init(&guard->quit, false);
    if (!atomic_is_lock_free(&guard->state)) {
        free(guard);
        m8rt_fail(rt, "stall guard state atomic is not lock-free");
        return -1;
    }
    if (!atomic_is_lock_free(&guard->quit)) {
        free(guard);
        m8rt_fail(rt, "stall guard quit atomic is not lock-free");
        return -1;
    }
    int error = pthread_create(&guard->thread, NULL, stall_monitor, guard);
    if (error) {
        free(guard);
        m8rt_fail(rt, "stall guard pthread_create error=%d: %s", error, strerror(error));
        return -1;
    }
    rt->stall_guard = guard;
    return 0;
}

void m8rt_stall_destroy(m8rt *rt)
{
    if (!rt || !rt->stall_guard) return;
    struct m8rt_stall_guard *guard = rt->stall_guard;
    atomic_store_explicit(&guard->quit, true, memory_order_release);
    pthread_join(guard->thread, NULL);
    free(guard);
    rt->stall_guard = NULL;
}

void m8rt_stall_enter(m8rt *rt)
{
    if (!rt || !rt->stall_guard) return;
    struct m8rt_stall_guard *guard = rt->stall_guard;
    uint64_t state = atomic_load_explicit(&guard->state, memory_order_acquire);
    if ((state & STALL_PHASE_MASK) != STALL_IDLE) {
        m8rt_fail(rt, "stall guard enter phase=%u state=%" PRIu64 " expected=IDLE",
                (unsigned)(state & STALL_PHASE_MASK), state);
        return;
    }
    /* A single guest owner calls enter/leave. The monitor never changes IDLE. */
    atomic_store_explicit(&guard->state,
            ((state & ~STALL_PHASE_MASK) + 4) | STALL_ACTIVE, memory_order_release);
}

bool m8rt_stall_leave(m8rt *rt)
{
    if (!rt || !rt->stall_guard) return false;
    struct m8rt_stall_guard *guard = rt->stall_guard;
    uint_fast64_t state = atomic_load_explicit(&guard->state, memory_order_acquire);
    for (;;) {
        unsigned phase = (unsigned)(state & STALL_PHASE_MASK);
        if (phase == STALL_IDLE) return false;
        if (phase == STALL_STOPPING) {
            /* Only the rare fired stop waits. Once FIRED is published,
             * uc_emu_stop has returned and cannot stop a future slice. */
            sched_yield();
            state = atomic_load_explicit(&guard->state, memory_order_acquire);
            continue;
        }
        uint64_t idle = state & ~STALL_PHASE_MASK;
        if (atomic_compare_exchange_weak_explicit(&guard->state, &state, idle,
                memory_order_acq_rel, memory_order_acquire)) return phase == STALL_FIRED;
    }
}

uint64_t m8rt_stall_limit_ns(const m8rt *rt)
{
    return rt && rt->stall_guard ? rt->stall_guard->limit_ns : STALL_DEFAULT_LIMIT_NS;
}
