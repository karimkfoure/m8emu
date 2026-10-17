#include "worker.h"
#include "native.h"
#include "queue.h"
#include "storage.h"
#include "thread_policy.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct command {
    uint64_t input_ns;
    unsigned phase, size;
    unsigned char data[8];
} command;
struct m8_worker {
    m8_session *s;
    m8_worker_options options;
    m8_queue input, display;
    SDL_Thread *thread;
    SDL_Mutex *state_mutex;
    m8_worker_state state;
    _Atomic int stop, done, ready, failed, reset;
    _Atomic uint64_t discarded;
    uint64_t resets, skipped, inputs, native_base, hook_base, calls_base, runtime_base;
    double cpu_base;
    m8rt_native_live_stats engine_base;
    m8_session_script_stats script;
    m8_host_native_notices notices;
    int drop, joined, boundary, synchronize;
};
static int display(void *context, const void *bytes, size_t n) {
    m8_worker *w = context;
    if (!w->options.display) return 0;
    const unsigned char *p = bytes;
    if (n) w->boundary = p[n-1] == 0xc0;
    if (!w->drop && w->synchronize) {
        size_t prefix = 0;
        while (prefix < n && p[prefix++] != 0xc0) {}
        w->skipped += prefix;
        if (!prefix || p[prefix-1] != 0xc0) return 0;
        w->synchronize = 0; p += prefix; n -= prefix; bytes = p;
    }
    if (!w->drop && m8_queue_write(&w->display, bytes, n) == 0) return 0;
    /* A stalled renderer cannot exert back pressure on the instrument. Keep
     * the observer current, discard stale display deltas, then request one
     * full redraw after the main thread acknowledges the decoder reset. */
    w->skipped += n;
    if (!w->drop) {
        w->drop = 1; ++w->resets;
        atomic_store_explicit(&w->reset, 1, memory_order_release);
    }
    return 0;
}
static int input(void *context) {
    m8_worker *w = context;
    if (atomic_load_explicit(&w->reset, memory_order_acquire) == 2) {
        if (m8rt_queue_host_bytes(w->s->rt, "R", 1) < 0) return -1;
        w->synchronize = !w->boundary;
        w->drop = 0;
        atomic_store_explicit(&w->reset, 0, memory_order_release);
    }
    command c;
    while (m8_queue_read(&w->input, &c, sizeof c)) {
        if (c.phase && w->options.control)
            w->options.control(w->options.control_context, c.phase, c.input_ns);
        if (c.size && m8rt_queue_host_bytes(w->s->rt, c.data, c.size) < 0) return -1;
        ++w->inputs;
    }
    return 0;
}
static void publish(m8_worker *w, int final) {
    /* The renderer holds this mutex only for a structure copy. The guest
     * never waits for it: a busy reader skips this optional status update. */
    if (!final && !SDL_TryLockMutex(w->state_mutex)) return;
    if (final) SDL_LockMutex(w->state_mutex);
    m8_worker_state *p = &w->state;
    m8_session *s = w->s;
    p->ready = atomic_load_explicit(&w->ready, memory_order_relaxed);
    p->failed = atomic_load_explicit(&w->failed, memory_order_relaxed);
    p->done = final;
    if (final && p->ready) p->end_ns = SDL_GetTicksNS();
    p->frames = m8rt_get(s->rt, M8RT_LOCAL_PCM_BYTES) / 4;
    p->consumer_started = s->consumer_started; p->consumer_base = s->consumer_base;
    p->cycles = m8rt_get(s->rt, M8RT_GUEST_CYCLES);
    p->main_iters = m8rt_get(s->rt, M8RT_MAIN_ITERS);
    p->pc = m8rt_get(s->rt, M8RT_PC);
    p->boot_done = m8rt_get(s->rt, M8RT_BOOT_DONE);
    p->sd_revision = m8rt_sd_image_revision(s->rt);
    p->rows = s->playhead_rows; p->service_seconds = s->service_seconds;
    p->cpu_seconds = w->options.thread_seconds && p->ready ?
        w->options.thread_seconds() - w->cpu_base : 0;
    p->process_seconds = w->options.process_seconds ? w->options.process_seconds() : -1;
    p->native_calls = m8rt_get(s->rt, M8RT_AOT_CALLS) - w->calls_base;
    p->native_ns = m8rt_get(s->rt, M8RT_AOT_NATIVE_NS) - w->native_base;
    p->hook_ns = m8rt_get(s->rt, M8RT_AOT_HOOK_NS) - w->hook_base;
    p->runtime_ns = m8rt_get(s->rt, M8RT_UC_NS) - w->runtime_base;
    p->timing_enabled = (int)m8rt_get(s->rt, M8RT_AOT_TIMING_ENABLED);
    if (m8rt_read_native_boot_app_stats(s->rt, &p->native_boot))
        p->native_boot = (m8rt_native_boot_app_stats){0};
    m8rt_native_live_stats full_native;
    int native_read = !m8rt_read_native_live_stats(s->rt, &full_native);
    if (native_read) {
        p->native_engine = full_native;
        p->native_engine.enters -= w->engine_base.enters;
        p->native_engine.leaves -= w->engine_base.leaves;
        p->native_engine.rejected_entries -= w->engine_base.rejected_entries;
        p->native_engine.aot_calls -= w->engine_base.aot_calls;
        p->native_engine.region_calls -= w->engine_base.region_calls;
        p->native_engine.region_guest -= w->engine_base.region_guest;
        p->native_engine.step_calls -= w->engine_base.step_calls;
        p->native_engine.step_guest -= w->engine_base.step_guest;
        p->native_engine.source_checks -= w->engine_base.source_checks;
        p->native_engine.cpu_import_words -= w->engine_base.cpu_import_words;
        p->native_engine.cpu_export_words -= w->engine_base.cpu_export_words;
        p->native_engine.pc_redirects -= w->engine_base.pc_redirects;
        for (unsigned i = 0; i < M8RT_NATIVE_LEAVE_COUNT; ++i)
            p->native_engine.reasons[i] -= w->engine_base.reasons[i];
        for (unsigned i = 0; i < M8RT_NATIVE_ARCH_COUNT; ++i)
            p->native_engine.architecture[i] -= w->engine_base.architecture[i];
        for (unsigned i = 0; i < M8RT_NATIVE_HANDOFF_PCS; ++i)
            p->native_engine.handoff_pcs[i].count -= w->engine_base.handoff_pcs[i].count;
        p->native_engine.handoff_pc_overflow -= w->engine_base.handoff_pc_overflow;
#define SUB_EVENT(field) p->native_engine.events.field -= w->engine_base.events.field
        SUB_EVENT(entries);SUB_EVENT(completed);SUB_EVENT(logical_slices);SUB_EVENT(enum_ticks);
        SUB_EVENT(main_hooks);SUB_EVENT(wait_hooks);SUB_EVENT(delay_hooks);SUB_EVENT(sampling_hooks);SUB_EVENT(audio_hooks);
        SUB_EVENT(irq_entries);SUB_EVENT(exception_returns);SUB_EVENT(memory_returns);SUB_EVENT(required_returns);
        SUB_EVENT(budget_returns);SUB_EVENT(host_returns);
#undef SUB_EVENT
    } else p->native_engine = (m8rt_native_live_stats){0};
    p->script = w->script;
    p->display_resets = w->resets;
    p->display_skipped = w->skipped + atomic_load_explicit(&w->discarded, memory_order_relaxed);
    p->inputs = w->inputs;
    if (s->demo) {
        p->held = m8_demo_held_keys(s->demo);
        p->have_cursor = m8_demo_cursor(s->demo, &p->cursor_x, &p->cursor_y);
    }
    if (w->options.smoke) {
        p->view_read = !m8rt_mem_read(s->rt, 0x20015ee0, p->view, 4);
        unsigned char row = 255;
        p->row_read = !m8rt_mem_read(s->rt, 0x20014aea, &row, 1); p->row = row;
    }
    snprintf(p->error, sizeof p->error, "%s", s->error);
    p->snapshot_ns = SDL_GetTicksNS();
    uint32_t sample_pc = (uint32_t)p->pc;
    SDL_UnlockMutex(w->state_mutex);
    if (native_read) {
        m8rt_native_live_rejection rejection;
        if (!m8rt_read_native_live_rejection(s->rt, &rejection))
            m8_host_native_report_runtime(stderr, &w->notices, &full_native, &rejection, sample_pc);
    }
}
static int fail(m8_worker *w, const char *detail) {
    if (!w->s->error[0]) snprintf(w->s->error, sizeof w->s->error, "%s", detail);
    atomic_store_explicit(&w->failed, 1, memory_order_release);
    return -1;
}
static int run(void *context) {
    m8_worker *w = context;
    m8_session *s = w->s;
    int measuring = 0;
    s->display = display; s->input = input; s->transport = w;
    if (m8_thread_policy_interactive(s->error, sizeof s->error)) { fail(w, s->error); goto done; }
    if (w->options.session_script) {
        if (m8_session_script_run(s, w->options.session_script, w->options.session_output,
                                  &w->stop, &w->script) < 0) fail(w, s->error);
        atomic_store_explicit(&w->ready, 1, memory_order_release);
        publish(w, 1);
        goto done;
    }
    s->demo = m8_demo_create(s->rt, m8_session_pump, s);
    if (!s->demo) { fail(w, "Cannot allocate display observer"); goto done; }
    if (m8_demo_boot(s->demo) < 0 ||
        (w->options.demo && m8_demo_prepare_simple(s->demo) < 0) ||
        (w->options.project && m8_demo_load_project(s->demo, w->options.project) < 0)) {
        fail(w, m8_demo_error(s->demo)); goto done;
    }
    if (w->options.play) {
        int rc = w->options.demo ? m8_demo_transport(s->demo, 1) :
            m8_demo_tap(s->demo, M8_DEMO_SHIFT | M8_DEMO_PLAY);
        if (rc < 0) { fail(w, m8_demo_error(s->demo)); goto done; }
    }
    if (w->options.latency && m8_demo_advance(s->demo, .4) < 0) {
        fail(w, m8_demo_error(s->demo)); goto done;
    }
    if (m8_session_start(s, w->options.capacity, w->options.slice_us, w->options.simulated) < 0) {
        fail(w, s->error); goto done;
    }
    /* Audio threads inherit the host mask during creation. Bind the live
     * guest only after both SDL and simulated consumers have started. */
    if (w->options.thread_setup && w->options.thread_setup(
            w->options.thread_setup_context, s->error, sizeof s->error)) {
        m8live_pause(s->audio);
        fail(w, s->error); goto done;
    }
    if (w->options.measurement_start && w->options.measurement_start(
            w->options.measurement_context, s->error, sizeof s->error)) {
        m8live_pause(s->audio);
        fail(w, s->error); goto done;
    }
    measuring = w->options.measurement_start != NULL;
    w->cpu_base = w->options.thread_seconds ? w->options.thread_seconds() : 0;
    w->native_base = m8rt_get(s->rt, M8RT_AOT_NATIVE_NS);
    w->hook_base = m8rt_get(s->rt, M8RT_AOT_HOOK_NS);
    w->calls_base = m8rt_get(s->rt, M8RT_AOT_CALLS);
    w->runtime_base = m8rt_get(s->rt, M8RT_UC_NS);
    m8rt_read_native_live_stats(s->rt, &w->engine_base);
#ifdef M8RT_ACCESS_PROFILE
    const char *access_path=getenv("M8RT_ACCESS_PROFILE_FILE");
    if(access_path && m8rt_set_access_profile(s->rt,1)) {
        fail(w,"Cannot start playback access profile");goto done;
    }
#endif
    atomic_store_explicit(&w->ready, 1, memory_order_release);
    publish(w, 0);
    uint64_t last = SDL_GetTicksNS();
    while (!atomic_load_explicit(&w->stop, memory_order_acquire)) {
        if (m8_session_step(s) < 0) { fail(w, s->error); break; }
        uint64_t now = SDL_GetTicksNS();
        if (now - last >= UINT64_C(10000000)) { publish(w, 0); last = now; }
    }
#ifdef M8RT_ACCESS_PROFILE
    if(access_path && (m8rt_set_access_profile(s->rt,0) ||
                      m8rt_dump_access_profile(s->rt,access_path)))
        fail(w,"Cannot write playback access profile");
#endif
    if (measuring && w->options.measurement_stop)
        w->options.measurement_stop(w->options.measurement_context);
    m8live_pause(s->audio);
    publish(w, 1);
    if (!atomic_load(&w->failed)) {
        /* Finish accepted packets, then release keys with ordered guest time. */
        s->delivering = 0;
        if (input(w) < 0) fail(w, "Clean release failed: first=accepted_input_queue");
        else if (m8rt_queue_host_bytes(s->rt, "K\xff" "C\0", 4) < 0)
            fail(w, "Clean release failed: first=release_packet note=255 mask=0");
        else {
            uint64_t queued = 0;
            unsigned dwell;
            for (dwell = 0; dwell < 25; ++dwell) {
                if (m8_demo_advance(s->demo, .080) < 0) {
                    fail(w, m8_demo_error(s->demo)); break;
                }
                queued = m8rt_get(s->rt, M8RT_HOST_QUEUED_BYTES);
                if (!queued) break;
            }
            if (dwell == 25) {
                char message[128];
                snprintf(message, sizeof message, "Clean release failed: first=guest_input_drain queued=%llu guest_seconds=2",
                         (unsigned long long)queued);
                fail(w, message);
            }
        }
    }
done:
    if (measuring && w->options.measurement_finish && w->options.measurement_finish(
            w->options.measurement_context, s->error, sizeof s->error)) fail(w, s->error);
    if (!atomic_load(&w->ready)) publish(w, 1);
    else {
        SDL_LockMutex(w->state_mutex);
        snprintf(w->state.error, sizeof w->state.error, "%s", s->error);
        SDL_UnlockMutex(w->state_mutex);
    }
    atomic_store_explicit(&w->done, 1, memory_order_release);
    return atomic_load(&w->failed) ? -1 : 0;
}
m8_worker *m8_worker_start(m8_session *s, const m8_worker_options *options) {
    m8_worker *w = calloc(1, sizeof *w);
    if (!w) return NULL;
    w->s = s; w->options = *options;
    w->state_mutex = SDL_CreateMutex();
    if (!w->state_mutex || m8_queue_init(&w->input, 8192) ||
        m8_queue_init(&w->display, options->display_capacity ? options->display_capacity : 262144)) goto failed;
    w->thread = SDL_CreateThread(run, "m8-guest", w);
    if (!w->thread) goto failed;
    return w;
failed:
    m8_worker_destroy(w); return NULL;
}
int m8_worker_send(void *context, const void *bytes, size_t n) {
    m8_worker *w = context;
    if (!w || n > 8 || atomic_load_explicit(&w->done, memory_order_acquire)) return -1;
    command c = {.size = (unsigned)n}; memcpy(c.data, bytes, n);
    return m8_queue_write(&w->input, &c, sizeof c);
}
int m8_worker_control(m8_worker *w, unsigned phase, uint64_t input_ns) {
    if (!w || !w->options.latency || phase < 1 || phase > 3 || atomic_load(&w->done)) return -1;
    command c = {.phase = phase, .input_ns = input_ns};
    if (phase == 1) { c.size = 3; memcpy(c.data, "K\x30\x64", 3); }
    else if (phase == 2) { c.size = 2; memcpy(c.data, "K\xff", 2); }
    return m8_queue_write(&w->input, &c, sizeof c);
}
size_t m8_worker_display(m8_worker *w, void *bytes, size_t n, int *reset) {
    *reset = 0;
    if (atomic_load_explicit(&w->reset, memory_order_acquire) == 1) {
        size_t skipped = m8_queue_read(&w->display, NULL, w->display.capacity);
        atomic_fetch_add_explicit(&w->discarded, skipped, memory_order_relaxed);
        *reset = 1;
        atomic_store_explicit(&w->reset, 2, memory_order_release);
        return 0;
    }
    return m8_queue_read(&w->display, bytes, n);
}
void m8_worker_snapshot(m8_worker *w, m8_worker_state *p) {
    SDL_LockMutex(w->state_mutex); *p = w->state; SDL_UnlockMutex(w->state_mutex);
    p->ready = atomic_load_explicit(&w->ready, memory_order_acquire);
    p->done = atomic_load_explicit(&w->done, memory_order_acquire);
    p->failed = atomic_load_explicit(&w->failed, memory_order_acquire);
}
void m8_worker_stop(m8_worker *w) { if (w) atomic_store_explicit(&w->stop, 1, memory_order_release); }
void m8_worker_join(m8_worker *w) {
    if (w && w->thread && !w->joined) { SDL_WaitThread(w->thread, NULL); w->joined = 1; }
}
void m8_worker_destroy(m8_worker *w) {
    if (!w) return;
    m8_worker_stop(w); m8_worker_join(w);
    m8_queue_close(&w->input); m8_queue_close(&w->display);
    SDL_DestroyMutex(w->state_mutex); free(w);
}
