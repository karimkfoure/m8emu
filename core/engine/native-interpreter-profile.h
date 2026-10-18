#ifndef M8_NATIVE_INTERPRETER_PROFILE_H
#define M8_NATIVE_INTERPRETER_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct m8_thumb_result;
typedef struct m8_native_interpreter_profile m8_native_interpreter_profile;

#define M8_NATIVE_INTERPRETER_PROFILE_DEFAULT_ENTRIES 65536u
#define M8_NATIVE_INTERPRETER_PROFILE_MAX_ENTRIES 1048576u

typedef struct m8_native_interpreter_profile_totals {
    uint64_t attempts, completed, faults, preparations;
    /* Admission failures for records whose key has no stored row. These
     * are events, not distinct keys; total counts still include them. */
    uint64_t dropped_entries, dropped_completed, dropped_faults;
    uint64_t allocation_failures;
    size_t entries, max_entries;
} m8_native_interpreter_profile_totals;

/* Execution-thread only. Zero selects the default capacity. Creation leaves
 * recording disabled; rows are allocated on the first enabled attempt.
 * The caller owns the lab/product default and any diagnostic flag. */
m8_native_interpreter_profile *m8_native_interpreter_profile_create(size_t max_entries);
void m8_native_interpreter_profile_enable(m8_native_interpreter_profile *profile,
                                          bool enabled);

/* Record the final service outcome, including early callback exits. Preserve
 * the original instruction detail if a callback changes PC. completed_guest
 * is the runtime's exact retirement count (0 or 1), not a value inferred from
 * status or successor PC. Nonzero service_status counts one fault, even when
 * the instruction completed. A successful FP_CONTEXT preparation with zero
 * completions counts only as a preparation; record its final retry separately.
 * A disabled call does not inspect result, allocate, hash, or change counters.
 * Allocation failure or a full table never changes execution behavior. */
void m8_native_interpreter_profile_record(m8_native_interpreter_profile *profile,
                                          const struct m8_thumb_result *result,
                                          int service_status,
                                          uint32_t completed_guest);

/* Clear the interval while retaining capacity, allocated storage and enable
 * state. This permits a separate playback interval after boot and LOAD. */
void m8_native_interpreter_profile_reset(m8_native_interpreter_profile *profile);
void m8_native_interpreter_profile_read_totals(
    const m8_native_interpreter_profile *profile,
    m8_native_interpreter_profile_totals *totals);

/* Compact m8-native-interpreter-profile/1 JSON. Rows sort by PC, opcode, width.
 * write_json emits one object without a newline, so the caller can wrap it
 * with an independently measured denominator and source metadata. dump adds
 * a newline. Return zero on success, -1 on allocation or output failure. */
int m8_native_interpreter_profile_write_json(
    const m8_native_interpreter_profile *profile, FILE *output);
int m8_native_interpreter_profile_dump(
    const m8_native_interpreter_profile *profile, const char *path);
void m8_native_interpreter_profile_free(m8_native_interpreter_profile *profile);

#endif
