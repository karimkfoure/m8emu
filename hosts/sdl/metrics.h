#ifndef M8_HOST_METRICS_H
#define M8_HOST_METRICS_H
#include <stdint.h>
uint64_t m8_monotonic_ns(void);
double m8_process_seconds(void);
double m8_thread_seconds(void);
double m8_max_rss_mib(void);
double m8_current_rss_mib(void);
#endif
