#include "native-interpreter-profile.h"
#include "thumb-interpreter.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

typedef struct profile_entry {
    uint32_t pc, opcode, width;
    uint64_t attempts, completed, faults;
} profile_entry;

struct m8_native_interpreter_profile {
    bool enabled, allocation_attempted;
    size_t slot_count;
    uint32_t *slots; /* dense row index plus one; zero means empty */
    profile_entry *rows;
    m8_native_interpreter_profile_totals totals;
};

m8_native_interpreter_profile *m8_native_interpreter_profile_create(size_t max_entries)
{
    if (!max_entries) max_entries = M8_NATIVE_INTERPRETER_PROFILE_DEFAULT_ENTRIES;
    if (max_entries > M8_NATIVE_INTERPRETER_PROFILE_MAX_ENTRIES) {
        errno = EINVAL;
        return NULL;
    }
    m8_native_interpreter_profile *profile = calloc(1, sizeof(*profile));
    if (profile) profile->totals.max_entries = max_entries;
    return profile;
}

void m8_native_interpreter_profile_enable(m8_native_interpreter_profile *profile,
                                          bool enabled)
{
    if (profile) profile->enabled = enabled;
}

static uint32_t key_hash(uint32_t pc, uint32_t opcode, uint32_t width)
{
    uint32_t value = pc * UINT32_C(0x9e3779b1);
    value ^= opcode * UINT32_C(0x85ebca6b);
    value ^= width * UINT32_C(0xc2b2ae35);
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    return value ^ (value >> 15);
}

static void allocate_rows(m8_native_interpreter_profile *profile)
{
    profile->allocation_attempted = true;
    size_t slots = 2;
    while (slots < profile->totals.max_entries * 2) slots *= 2;
    profile->rows = calloc(profile->totals.max_entries, sizeof(*profile->rows));
    profile->slots = calloc(slots, sizeof(*profile->slots));
    if (!profile->rows || !profile->slots) {
        free(profile->rows);
        free(profile->slots);
        profile->rows = NULL;
        profile->slots = NULL;
        profile->totals.allocation_failures++;
        return;
    }
    profile->slot_count = slots;
}

void m8_native_interpreter_profile_record(m8_native_interpreter_profile *profile,
                                          const struct m8_thumb_result *result,
                                          int service_status,
                                          uint32_t completed_guest)
{
    if (!profile || !profile->enabled) return;
    if (!result) return;
    if (result->reason == M8_THUMB_REASON_FP_CONTEXT && !service_status &&
        !completed_guest) {
        profile->totals.preparations++;
        return;
    }
    profile->totals.attempts++;
    profile->totals.completed += completed_guest;
    profile->totals.faults += service_status != 0;
    if (!profile->allocation_attempted) allocate_rows(profile);
    if (profile->slots) {
        size_t slot = key_hash(result->pc, result->opcode, result->width) &
                      (profile->slot_count - 1);
        while (profile->slots[slot]) {
            profile_entry *entry = &profile->rows[profile->slots[slot] - 1];
            if (entry->pc == result->pc && entry->opcode == result->opcode &&
                entry->width == result->width) {
                entry->attempts++;
                entry->completed += completed_guest;
                entry->faults += service_status != 0;
                return;
            }
            slot = (slot + 1) & (profile->slot_count - 1);
        }
        if (profile->totals.entries < profile->totals.max_entries) {
            size_t index = profile->totals.entries++;
            profile->rows[index] = (profile_entry){
                .pc = result->pc, .opcode = result->opcode, .width = result->width,
                .attempts = 1, .completed = completed_guest,
                .faults = service_status != 0
            };
            profile->slots[slot] = (uint32_t)index + 1;
            return;
        }
    }
    profile->totals.dropped_entries++;
    profile->totals.dropped_completed += completed_guest;
    profile->totals.dropped_faults += service_status != 0;
}

void m8_native_interpreter_profile_reset(m8_native_interpreter_profile *profile)
{
    if (!profile) return;
    size_t capacity = profile->totals.max_entries;
    memset(&profile->totals, 0, sizeof(profile->totals));
    profile->totals.max_entries = capacity;
    if (profile->slots)
        memset(profile->slots, 0, profile->slot_count * sizeof(*profile->slots));
    else
        profile->allocation_attempted = false;
}

void m8_native_interpreter_profile_read_totals(
    const m8_native_interpreter_profile *profile,
    m8_native_interpreter_profile_totals *totals)
{
    if (totals) {
        if (profile) *totals = profile->totals;
        else memset(totals, 0, sizeof(*totals));
    }
}

static int compare_entries(const void *left, const void *right)
{
    const profile_entry *a = left, *b = right;
    if (a->pc != b->pc) return a->pc < b->pc ? -1 : 1;
    if (a->opcode != b->opcode) return a->opcode < b->opcode ? -1 : 1;
    return (a->width > b->width) - (a->width < b->width);
}

int m8_native_interpreter_profile_write_json(
    const m8_native_interpreter_profile *profile, FILE *output)
{
    if (!output) {
        errno = EINVAL;
        return -1;
    }
    m8_native_interpreter_profile_totals totals;
    m8_native_interpreter_profile_read_totals(profile, &totals);
    profile_entry *sorted = NULL;
    if (totals.entries) {
        sorted = malloc(totals.entries * sizeof(*sorted));
        if (!sorted) return -1;
        memcpy(sorted, profile->rows, totals.entries * sizeof(*sorted));
        qsort(sorted, totals.entries, sizeof(*sorted), compare_entries);
    }
    int error = fprintf(output,
        "{\"schema\":\"m8-native-interpreter-profile/1\",\"enabled\":%s,"
        "\"max_entries\":%zu,\"entries\":%zu,\"attempts\":%" PRIu64 ","
        "\"completed\":%" PRIu64 ",\"faults\":%" PRIu64 ","
        "\"preparations\":%" PRIu64 ",\"dropped_entries\":%" PRIu64 ","
        "\"dropped_completed\":%" PRIu64 ",\"dropped_faults\":%" PRIu64 ","
        "\"allocation_failures\":%" PRIu64 ",\"pcs\":[",
        profile && profile->enabled ? "true" : "false", totals.max_entries,
        totals.entries, totals.attempts, totals.completed, totals.faults,
        totals.preparations, totals.dropped_entries, totals.dropped_completed,
        totals.dropped_faults, totals.allocation_failures) < 0;
    for (size_t i = 0; i < totals.entries && !error; i++) {
        const profile_entry *entry = &sorted[i];
        error = fprintf(output,
            "%s{\"pc\":%" PRIu32 ",\"opcode\":%" PRIu32 ",\"width\":%" PRIu32 ","
            "\"attempts\":%" PRIu64 ",\"completed\":%" PRIu64 ",\"faults\":%" PRIu64 "}",
            i ? "," : "", entry->pc, entry->opcode, entry->width,
            entry->attempts, entry->completed, entry->faults) < 0;
    }
    if (!error) error = fputs("]}", output) == EOF;
    free(sorted);
    return error || ferror(output) ? -1 : 0;
}

int m8_native_interpreter_profile_dump(
    const m8_native_interpreter_profile *profile, const char *path)
{
    if (!path) {
        errno = EINVAL;
        return -1;
    }
    FILE *output = fopen(path, "w");
    if (!output) return -1;
    int result = m8_native_interpreter_profile_write_json(profile, output);
    if (!result && fputc('\n', output) == EOF) result = -1;
    if (fclose(output)) result = -1;
    return result;
}

void m8_native_interpreter_profile_free(m8_native_interpreter_profile *profile)
{
    if (!profile) return;
    free(profile->rows);
    free(profile->slots);
    free(profile);
}
