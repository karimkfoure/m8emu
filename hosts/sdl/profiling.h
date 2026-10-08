#ifndef M8_DESKTOP_PROFILING_H
#define M8_DESKTOP_PROFILING_H
#include <stddef.h>
#include <stdio.h>
typedef struct m8_app_profile {
    unsigned hz;
    int pmu;
    const char *sample, *aot;
    FILE *output;
} m8_app_profile;
int m8_app_profile_start(void *, char *, size_t);
void m8_app_profile_stop(void *);
int m8_app_profile_finish(void *, char *, size_t);
#endif
