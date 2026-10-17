#ifndef M8_HOST_AOT_H
#define M8_HOST_AOT_H
#include "m8rt.h"

/* Bundled runtime libraries use the host's shared-library suffix. */
#if defined(__APPLE__)
#define M8_LIBRARY_SUFFIX ".dylib"
#else
#define M8_LIBRARY_SUFFIX ".so"
#endif

typedef struct m8_host_aot m8_host_aot;
typedef struct m8_host_aot_stats {
    uint64_t calls, native_ns, hook_ns, runtime_ns;
    unsigned timing_enabled;
} m8_host_aot_stats;

/* Preload before runtime creation. NULL path selects
 * M8RT_AOT_LIBRARY; an empty string explicitly disables AOT. An absent path
 * returns a valid disabled handle. Pass the requirements to m8rt_create_aot,
 * then attach on the idle runtime owner. This selects backing before mapping
 * guest memory and does not change environment variables. */
m8_host_aot *m8_host_aot_preload(const char *path, char *error, size_t error_capacity);
unsigned m8_host_aot_requirements(const m8_host_aot *aot);
int m8_host_aot_attach(m8rt *rt, m8_host_aot *aot, char *error, size_t error_capacity);
/* Compatibility helper for an existing idle runtime. It rejects a library
 * that requires fixed memory when this runtime has ordinary backing.
 * Set M8RT_AOT_TIMING=1 before creation to measure native time. */
m8_host_aot *m8_host_aot_open(m8rt *rt, const char *path,
                             char *error, size_t error_capacity);
const char *m8_host_aot_path(const m8_host_aot *aot);
unsigned m8_host_aot_entries(const m8_host_aot *aot);
unsigned m8_host_aot_services(const m8_host_aot *aot);
/* Native execution needs verified code and write guards. This check performs
 * no guest reads; the runtime verifies memory at its admitted entry. */
int m8_host_aot_native_contract(const m8_host_aot *aot,
    int (**verify)(m8_mem *, uint32_t *), char *error, size_t error_capacity);
int m8_host_aot_native_dispatch(const m8_host_aot *aot,
    m8_aot_fn (**lookup)(uint32_t), uint32_t (**entries)(const uint32_t **),
    unsigned *services);
/* Read only on the runtime owner, outside guest execution. Hook time includes
 * native time. Native share is native_ns/runtime_ns; runtime time includes
 * callbacks. Subtract two snapshots for a playback interval. */
void m8_host_aot_snapshot(m8rt *rt, m8_host_aot_stats *out);
/* Join the runtime owner first. With rt, remove all hooks before unloading.
 * If detach fails, keep the handle alive, destroy rt, then close with NULL.
 * With NULL rt, the caller must have already destroyed the runtime. */
int m8_host_aot_close(m8rt *rt, m8_host_aot *aot);
#endif
