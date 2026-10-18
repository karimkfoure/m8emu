#ifndef M8_HOST_NATIVE_H
#define M8_HOST_NATIVE_H
#include "aot.h"
#include "native-live.h"
#include <stdio.h>

typedef struct m8_host_native m8_host_native;
#if M8EMU_LAB_REFERENCE
/* CLI values 0/1 override the environment; -1 means no CLI selection.
 * Return -1 for an invalid environment without changing it. */
int m8_host_native_choose(int cli, const char *environment, int target_default,
                          char *error, size_t capacity);
#endif
/* Configure qualified native controls before runtime creation. Preserve every
 * explicit diagnostic environment value. */
int m8_host_native_default_environment(char *error, size_t capacity);
#if M8EMU_LAB_REFERENCE
void m8_host_native_report_fallback(FILE *file, const char *phase, const char *reason);
#endif
/* An enabled handle owns all native code until after the runtime is destroyed.
 * NULL paths select M8RT_NATIVE_LIBRARY/M8RT_NATIVE_REGIONS, then bundled libs.
 * Disabled preload does not inspect libraries or the AOT native contract. */
m8_host_native *m8_host_native_preload(int enabled, const char *step_path,
    const char *region_path, m8_host_aot *aot, char *error, size_t capacity);
#if M8EMU_LAB_REFERENCE
/* With fallback enabled, pre-execution configuration failures return a
 * disabled handle. Allocation failures and fatal runtime errors still fail. */
m8_host_native *m8_host_native_preload_policy(int enabled, int fallback,
    const char *step_path, const char *region_path, m8_host_aot *aot,
    char *error, size_t capacity);
int m8_host_native_attach_policy(m8rt *rt, m8_host_native *native, int fallback,
                                 char *error, size_t capacity);
#endif
int m8_host_native_enabled(const m8_host_native *native);
#if M8EMU_LAB_REFERENCE
const char *m8_host_native_fallback(const m8_host_native *native);
#endif
const m8rt_native_live_config *m8_host_native_config(const m8_host_native *native);
unsigned m8_host_native_requirements(const m8_host_native *native);
#if M8EMU_LAB_REFERENCE
int m8_host_native_attach(m8rt *rt, m8_host_native *native, char *error, size_t capacity);
#endif
const char *m8_host_native_step_path(const m8_host_native *native);
const char *m8_host_native_region_path(const m8_host_native *native);
size_t m8_host_native_chain_slots(const m8_host_native *native);
/* Close only after joining the guest owner and destroying its runtime. */
void m8_host_native_close(m8_host_native *native);
int m8_host_native_stats_json(FILE *file, const m8rt_native_live_stats *stats);
/* Guest-owner diagnostics, outside execution. Each observed handoff tuple and
 * rejection predicate is printed once. This is not an instruction trace. */
typedef struct {
    m8rt_native_handoff_pc handoffs[M8RT_NATIVE_HANDOFF_PCS];
    unsigned handoff_count;
    unsigned char rejected[M8RT_NATIVE_REJECT_COUNT];
    int overflow, code_disabled;
} m8_host_native_notices;
void m8_host_native_report_runtime(FILE *file, m8_host_native_notices *notices,
    const m8rt_native_live_stats *stats, const m8rt_native_live_rejection *rejection,
    uint32_t sample_pc);
#endif
