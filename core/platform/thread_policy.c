#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif
#include "thread_policy.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef __linux__
#include <sched.h>

static cpu_set_t initial_allowed;
static int initial_saved;
#elif defined(__APPLE__)
#include <pthread.h>
#include <pthread/qos.h>
#endif

static int fail(char *error, size_t capacity, const char *format, ...)
{
    if (error && capacity) {
        va_list args;
        va_start(args, format);
        vsnprintf(error, capacity, format, args);
        va_end(args);
    }
    return -1;
}

int m8_thread_policy_bind(const char *cpus, char *error, size_t capacity)
{
    if (error && capacity) *error = 0;
#ifdef __linux__
    if (!cpus || !*cpus)
        return fail(error, capacity, "first=cpu_list_present present=%u empty=%u",
                    cpus != NULL, cpus && !*cpus);
    cpu_set_t requested;
    unsigned order[CPU_SETSIZE], count = 0;
    CPU_ZERO(&requested);
    size_t offset = 0;
    for (;;) {
        size_t token = offset;
        unsigned cpu = 0;
        if (cpus[offset] < '0' || cpus[offset] > '9')
            return fail(error, capacity, "first=cpu_token_integer offset=%zu byte=%u",
                        offset, (unsigned char)cpus[offset]);
        do {
            /* The previous value is below CPU_SETSIZE, so this cannot
             * overflow before the range guard rejects the next digit. */
            cpu = cpu * 10 + (unsigned)(cpus[offset] - '0');
            if (cpu >= CPU_SETSIZE)
                return fail(error, capacity,
                            "first=cpu_index_range offset=%zu parsed=%u limit=%u",
                            token, cpu, (unsigned)CPU_SETSIZE);
            ++offset;
        } while (cpus[offset] >= '0' && cpus[offset] <= '9');
        if (cpus[offset] && cpus[offset] != ',')
            return fail(error, capacity, "first=cpu_separator offset=%zu byte=%u",
                        offset, (unsigned char)cpus[offset]);
        if (CPU_ISSET(cpu, &requested))
            return fail(error, capacity, "first=cpu_unique offset=%zu cpu=%u", token, cpu);
        CPU_SET(cpu, &requested);
        order[count++] = cpu;
        if (!cpus[offset]) break;
        ++offset;
    }
    if (!initial_saved) {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        if (sched_getaffinity(0, sizeof mask, &mask)) {
            int saved_errno = errno;
            return fail(error, capacity,
                        "first=initial_affinity_read errno=%d mask_bytes=%zu: %s",
                        saved_errno, sizeof mask, strerror(saved_errno));
        }
        if (!CPU_COUNT(&mask))
            return fail(error, capacity, "first=initial_affinity_nonempty count=0");
        initial_allowed = mask;
        initial_saved = 1;
    }
    for (unsigned i = 0; i < count; ++i)
        if (!CPU_ISSET(order[i], &initial_allowed))
            return fail(error, capacity,
                        "first=cpu_initially_allowed index=%u cpu=%u allowed_count=%u",
                        i, order[i], (unsigned)CPU_COUNT(&initial_allowed));
    if (sched_setaffinity(0, sizeof requested, &requested)) {
        int saved_errno = errno;
        return fail(error, capacity,
                    "first=thread_affinity_apply errno=%d requested_count=%u: %s",
                    saved_errno, count, strerror(saved_errno));
    }
    return 0;
#else
    errno = ENOTSUP;
    return fail(error, capacity,
                "first=cpu_affinity_supported platform=non_linux cpus_present=%u",
                cpus != NULL);
#endif
}

int m8_thread_policy_interactive(char *error, size_t capacity)
{
    if (error && capacity) *error = 0;
#ifdef __APPLE__
    int result = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    if (result) {
        errno = result;
        return fail(error, capacity,
                    "first=thread_qos_apply errno=%d qos=user_interactive: %s",
                    result, strerror(result));
    }
#endif
    return 0;
}
