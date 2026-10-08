/* Preserve the existing m8-live ABI's absolute CLOCK_MONOTONIC timestamps. */
#define _POSIX_C_SOURCE 200809L
#include "audio_clock.h"
#include <time.h>
uint64_t m8_audio_now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}
