#ifndef M8_HOST_PMU_H
#define M8_HOST_PMU_H

#include <stdint.h>
#include <stdio.h>

/* Prepare outside playback. count enables the independent counter set.
 * sample_spec is NULL, or cycles|l1i-refill|l2d-refill:PERIOD.
 * Invalid specifications return -1/EINVAL. Event availability and access
 * errors are retained in JSON and do not prevent benchmark execution. */
int m8_pmu_prepare(int count, const char *sample_spec);
/* Start/stop on the calling playback thread. No pinned events are used. */
int m8_pmu_start(void);
void m8_pmu_stop(void);
/* Return a rounded, multiplex-scaled instruction count, or -1/errno. */
int m8_pmu_instructions(uint64_t *value);
/* Stop first. Write one JSON object, without a trailing newline. Keep sampled
 * executable mappings loaded until the shared PC classifier has run. */
void m8_pmu_write_json(FILE *out, const char *aot_path);
void m8_pmu_destroy(void);

#endif
