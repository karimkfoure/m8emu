#ifndef M8_PC_PROFILE_H
#define M8_PC_PROFILE_H

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define M8_PROFILE_MAX_TOP_SYMBOLS 1000u

/* Change only report truncation. Sampling and attribution stay unchanged.
 * Default is 30. Set 1..M8_PROFILE_MAX_TOP_SYMBOLS before sampling. */
int m8_profile_set_top_symbols(unsigned count);

/* Call prepare before creating runtime threads. Zero disables all sampling. */
int m8_profile_prepare(unsigned hz);
/* Optional calling-thread scope pointer. Set it before start and keep it live
 * until stop: 0 outside, 1 FM, 2 MACRO, 3 WAV, 4 HYPER, 5 GRAPH, 6 VOICE. */
void m8_profile_set_context(volatile sig_atomic_t *scope);
int m8_profile_start(const char *aot_path);
void m8_profile_stop(void);
/* Stop first. This writes one JSON object, without a trailing newline. */
void m8_profile_write_json(FILE *out);
/* Optional instruction PCs for offline source attribution. Stop sampling first. */
void m8_profile_write_raw_json(FILE *out);
/* Classify caller-owned PCs after sampling stops, while sampled libraries and
 * executable mappings remain loaded. This API needs no timer preparation.
 * Scope tags and timing metadata are absent. A NULL array requires count=0. */
void m8_profile_write_pcs_json(FILE *out, const char *aot_path,
    const uintptr_t *pcs, size_t count);
void m8_profile_destroy(void);

#endif
