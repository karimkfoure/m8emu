/* Optional desktop measurements. Runtime and audio do not use these calls. */
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include "metrics.h"
#include <time.h>
#include <stdio.h>
#if defined(__linux__)
#include <unistd.h>
#endif
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
double m8_process_seconds(void) {
    struct rusage u;
    if (getrusage(RUSAGE_SELF, &u)) return -1;
    return u.ru_utime.tv_sec + u.ru_stime.tv_sec +
           (u.ru_utime.tv_usec + u.ru_stime.tv_usec) / 1e6;
}
double m8_max_rss_mib(void) {
    struct rusage u;
    if (getrusage(RUSAGE_SELF, &u)) return -1;
#if defined(__APPLE__)
    return u.ru_maxrss / 1048576.0;
#else
    return u.ru_maxrss / 1024.0;
#endif
}
#else
double m8_process_seconds(void) { return (double)clock() / CLOCKS_PER_SEC; }
double m8_max_rss_mib(void) { return -1; }
#endif
double m8_thread_seconds(void) {
#if defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec t;
    if (!clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t)) return t.tv_sec + t.tv_nsec / 1e9;
#endif
    return -1;
}
double m8_current_rss_mib(void) {
#if defined(__linux__)
    FILE *f = fopen("/proc/self/statm", "r");
    unsigned long total = 0, resident = 0;
    if (!f) return -1;
    int valid = fscanf(f, "%lu %lu", &total, &resident) == 2;
    fclose(f);
    long pages = sysconf(_SC_PAGESIZE);
    return valid && pages > 0 ? resident * (double)pages / 1048576.0 : -1;
#else
    return -1;
#endif
}

uint64_t m8_monotonic_ns(void) {
#if defined(CLOCK_MONOTONIC)
    struct timespec t;
    if (!clock_gettime(CLOCK_MONOTONIC, &t))
        return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
#endif
    return 0;
}
