#ifndef M8_HOST_WORKER_H
#define M8_HOST_WORKER_H
#include "session.h"
#include "native-live.h"
#include "session_script.h"
typedef struct m8_worker m8_worker;
typedef struct m8_worker_options {
    unsigned capacity, slice_us, display_capacity;
    int simulated, demo, play, latency, smoke, display;
    const char *project, *session_script, *session_output;
    double (*thread_seconds)(void);
    double (*process_seconds)(void);
    int (*thread_setup)(void *, char *, size_t);
    void *thread_setup_context;
    /* Optional calling-thread diagnostics, outside boot and clean release. */
    int (*measurement_start)(void *, char *, size_t);
    void (*measurement_stop)(void *);
    int (*measurement_finish)(void *, char *, size_t);
    void *measurement_context;
    /* Called only by the guest owner, before its measurement command. */
    void (*control)(void *, unsigned phase, uint64_t input_ns);
    void *control_context;
} m8_worker_options;
typedef struct m8_worker_state {
    uint64_t frames, cycles, boot_done, sd_revision, native_calls, native_ns, hook_ns, runtime_ns;
    uint64_t display_resets, display_skipped, inputs, end_ns, snapshot_ns, main_iters, pc;
    uint64_t consumer_started, consumer_base;
    double service_seconds, cpu_seconds, process_seconds;
    unsigned rows, cursor_x, cursor_y, have_cursor, held, view_read, row_read, row;
    unsigned char view[4];
    m8rt_native_live_stats native_engine;
    m8rt_native_boot_app_stats native_boot;
    m8_session_script_stats script;
    int ready, done, failed, timing_enabled;
    char error[512];
} m8_worker_state;
m8_worker *m8_worker_start(m8_session *session, const m8_worker_options *options);
/* SDL main-thread producer. Input exhaustion is explicit; no packet is lost. */
int m8_worker_send(void *worker, const void *bytes, size_t n);
int m8_worker_control(m8_worker *worker, unsigned phase, uint64_t input_ns);
/* SDL main-thread consumer. reset=1 means clear renderer/decoder, then redraw. */
size_t m8_worker_display(m8_worker *, void *bytes, size_t n, int *reset);
void m8_worker_snapshot(m8_worker *, m8_worker_state *state);
void m8_worker_stop(m8_worker *);
void m8_worker_join(m8_worker *);
void m8_worker_destroy(m8_worker *);
#endif
