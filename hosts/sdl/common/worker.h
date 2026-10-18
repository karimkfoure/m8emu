#ifndef M8_HOST_WORKER_H
#define M8_HOST_WORKER_H
#include "session.h"
#include "native-live.h"
#include "session_script.h"
#include "../../common/display.h"
typedef struct m8_worker m8_worker;
typedef enum {M8_WORKER_PAUSE=1,M8_WORKER_RESUME,M8_WORKER_REPLACE} m8_worker_operation;
typedef struct {
    m8_worker_operation operation;
    m8_session_result result;
    m8rt_session_error cause;
    m8rt_session_blob media;
    M8WebDisplay *screen;
    uint64_t epoch;
    int needs_resume,read_only;
    char error[512];
} m8_worker_reply;
typedef struct m8_worker_options {
    struct m8_card *card;
    m8_session_factory fresh;
    void *fresh_context;
    unsigned capacity, slice_us, display_capacity;
    int simulated, demo, play, latency, smoke, display, observe_input;
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
    uint64_t produced_base,output_origin,session_epoch,session_pause_ns;
    double service_seconds, cpu_seconds, process_seconds;
    unsigned rows, cursor_x, cursor_y, have_cursor, held, view_read, row_read, row;
    unsigned char view[4];
    unsigned guest_held, guest_processed, held_read;
    char view_text[64], selected[64];
    m8rt_native_live_stats native_engine;
    m8rt_native_boot_app_stats native_boot;
    m8_session_script_stats script;
    uint64_t session_committed_ns,session_capture_ns;
    uint64_t session_total_ns,session_captures,session_idle_captures;
    int session_dirty;
    int session_paused;
    int session_enabled;
    char session_error[512];
    char audio_error[256];
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
void m8_worker_checkpoint(m8_worker *);
/* Main-thread requests are asynchronous, with one operation in flight.
 * BUSY means retry later; no request or card ownership transfers on refusal.
 * A successful replacement stays paused until the UI installs its screen
 * and requests resume. take_reply also discards the old display queue. */
m8_session_result m8_worker_pause(m8_worker *);
m8_session_result m8_worker_resume(m8_worker *);
m8_session_result m8_worker_replace(m8_worker *,const void *,size_t,int reset);
int m8_worker_take_reply(m8_worker *,m8_worker_reply *);
void m8_worker_reply_release(m8_worker_reply *);
/* Transfers ownership of a UI-prepared stream only on success. */
int m8_worker_output(m8_worker *,m8live_output *);
void m8_worker_stop(m8_worker *);
void m8_worker_join(m8_worker *);
void m8_worker_destroy(m8_worker *);
#endif
