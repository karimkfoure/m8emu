#include "worker.h"
#include "native.h"
#include "queue.h"
#include "storage.h"
#include "checkpoint.h"
#include "card.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct operation {
    m8_worker_reply reply;
    void *bytes;size_t size;int reset;
} operation;
typedef struct command {
    uint64_t input_ns;
    m8live_output *output;
    operation *operation;
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
    _Atomic int stop, done, ready, failed, reset, checkpoint_request;
    int resume;
    unsigned held,note;
    uint64_t release_frame,epoch;
    operation *pending;
    _Atomic(operation *) reply;
    _Atomic int operation_busy;
    char audio_error[256];
    _Atomic uint64_t discarded;
    uint64_t audio_end;
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
        if(c.operation){w->pending=c.operation;continue;}
        if(c.output) {
            w->audio_error[0]=0;
            m8live_output_apply(w->s->audio,c.output,
                !w->s->explorer_paused&&!(w->s->checkpoint&&w->s->checkpoint->paused),w->audio_error,sizeof w->audio_error);
            continue;
        }
        if (c.phase && w->options.control)
            w->options.control(w->options.control_context, c.phase, c.input_ns);
        if (c.size && m8rt_queue_host_bytes(w->s->rt, c.data, c.size) < 0) return -1;
        if(c.size>=2 && ((c.data[0]=='C' && c.data[1]) || c.data[0]=='K'))w->resume=1;
        if(c.size>=2&&(c.data[0]=='C'||c.data[0]=='K')) {
            int release=c.data[0]=='C'?w->held&&!c.data[1]:w->note&&c.data[1]==255;
            if(c.data[0]=='C')w->held=c.data[1];else w->note=c.data[1]!=255;
            if(release)w->release_frame=m8rt_get(w->s->rt,M8RT_LOCAL_PCM_BYTES)/4+3528;
        }
        if(c.size)m8_checkpoint_input(w->s,SDL_GetTicksNS());
        ++w->inputs;
    }
    return 0;
}
static void publish(m8_worker *w, int final) {
    m8live_update_format(w->s->audio);
    /* The renderer holds this mutex only for a structure copy. The guest
     * never waits for it: a busy reader skips this optional status update. */
    if (!final && !SDL_TryLockMutex(w->state_mutex)) return;
    if (final) SDL_LockMutex(w->state_mutex);
    m8_worker_state *p = &w->state;
    m8_session *s = w->s;
    if(s->checkpoint) {
        p->session_committed_ns=s->checkpoint->committed_ns;
        p->session_paused=s->checkpoint->paused;
        p->session_capture_ns=s->checkpoint->capture_ns;
        p->session_total_ns=s->checkpoint->total_ns;
        p->session_captures=s->checkpoint->captures;p->session_idle_captures=s->checkpoint->idle_captures;
        p->session_dirty=s->checkpoint->changes!=s->checkpoint->committed_changes;
        snprintf(p->session_error,sizeof p->session_error,"%s",s->checkpoint->error);
    }
    snprintf(p->audio_error,sizeof p->audio_error,"%s",w->audio_error);
    p->session_paused=s->explorer_paused||(s->checkpoint&&s->checkpoint->paused);
    p->session_enabled=s->checkpoint&&s->checkpoint->card&&!m8_card_read_only(s->checkpoint->card);
    p->produced_base=s->produced_base;p->output_origin=s->output_origin;p->session_epoch=w->epoch;
    uint64_t pause_end=final&&w->audio_end?w->audio_end:SDL_GetTicksNS();
    p->session_pause_ns=s->pause_total+(s->pause_started&&s->consumer_started?pause_end-s->pause_started:0);
    p->ready = atomic_load_explicit(&w->ready, memory_order_relaxed);
    p->failed = atomic_load_explicit(&w->failed, memory_order_relaxed);
    p->done = final;
    if (final && p->ready) p->end_ns = w->audio_end?w->audio_end:SDL_GetTicksNS();
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
    if (w->options.smoke || w->options.observe_input) {
        p->view_read = !m8rt_mem_read(s->rt, 0x20015ee0, p->view, 4);
        unsigned char row = 255;
        p->row_read = !m8rt_mem_read(s->rt, 0x20014aea, &row, 1); p->row = row;
    }
    if (w->options.observe_input) {
        /* Headless 6.5.2C input object: current and polled controller bytes.
         * This optional read belongs to the guest thread, after execution. */
        unsigned char keys[2] = {0};
        p->held_read = !m8rt_mem_read(s->rt, 0x2001487a, keys, sizeof keys);
        p->guest_held = keys[0]; p->guest_processed = keys[1];
        if (s->demo) {
            p->view_text[0] = 0;
            for (unsigned line = 0; line < 8; ++line) {
                if (!m8_demo_line(s->demo, line, p->view_text, sizeof p->view_text) ||
                    p->view_text[0]) break;
            }
            m8_demo_selected(s->demo, p->selected, sizeof p->selected);
        }
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
static void execute_operation(m8_worker *w) {
    operation *op=w->pending;if(!op)return;
    m8_session *s=w->s;m8_worker_reply *reply=&op->reply;
    if(reply->operation==M8_WORKER_PAUSE) {
        if(!s->explorer_paused&&!(s->checkpoint&&s->checkpoint->paused)&&
           (m8rt_get(s->rt,M8RT_HOST_QUEUED_BYTES)||m8rt_get(s->rt,M8RT_LOCAL_PCM_BYTES)/4<w->release_frame))return;
        reply->result=m8_session_pause(s,&reply->media,&reply->cause,reply->error,sizeof reply->error);
        reply->read_only=w->options.card&&m8_card_read_only(w->options.card);
    } else if(reply->operation==M8_WORKER_RESUME) {
        reply->result=m8_session_resume(s,reply->error,sizeof reply->error);
    } else {
        reply->screen=malloc(sizeof *reply->screen);
        if(!reply->screen)reply->result=M8_SESSION_NOMEM;
        else reply->result=m8_session_replace(s,w->options.card,w->options.fresh,w->options.fresh_context,
            op->bytes,op->size,op->reset,reply->error,sizeof reply->error);
        if(reply->result==M8_SESSION_OK) {
            ++w->epoch;reply->needs_resume=s->explorer_paused;
            *reply->screen=s->checkpoint->screen;
            w->held=w->note=w->resume=0;w->release_frame=0;
            w->drop=w->synchronize=0;w->boundary=!s->checkpoint->screen.packet_size&&!s->checkpoint->screen.escaped;
            w->native_base=m8rt_get(s->rt,M8RT_AOT_NATIVE_NS);w->hook_base=m8rt_get(s->rt,M8RT_AOT_HOOK_NS);
            w->calls_base=m8rt_get(s->rt,M8RT_AOT_CALLS);w->runtime_base=m8rt_get(s->rt,M8RT_UC_NS);
            w->cpu_base=w->options.thread_seconds?w->options.thread_seconds():0;
            m8rt_read_native_live_stats(s->rt,&w->engine_base);memset(&w->notices,0,sizeof w->notices);
        }
    }
    reply->epoch=w->epoch;w->pending=NULL;
    if(w->options.observe_input)fprintf(stderr,
        "session_boundary operation=%u result=%u epoch=%llu paused=%d frames=%llu cycles=%llu held=%u queued=%llu error=%s\n",
        reply->operation,reply->result,(unsigned long long)w->epoch,s->explorer_paused,
        (unsigned long long)(m8rt_get(s->rt,M8RT_LOCAL_PCM_BYTES)/4),
        (unsigned long long)m8rt_get(s->rt,M8RT_GUEST_CYCLES),w->held,
        (unsigned long long)m8rt_get(s->rt,M8RT_HOST_QUEUED_BYTES),reply->error);
    atomic_store_explicit(&w->reply,op,memory_order_release);
}
static int run(void *context) {
    m8_worker *w = context;
    m8_session *s = w->s;
    int measuring = 0;
    s->display = display; s->input = input; s->transport = w;
    if (w->options.session_script) {
        if (m8_session_script_run(s, w->options.session_script, w->options.session_output,
                                  &w->stop, &w->script) < 0) fail(w, s->error);
        atomic_store_explicit(&w->ready, 1, memory_order_release);
        publish(w, 1);
        goto done;
    }
    if(!s->checkpoint&&m8_checkpoint_open(s,w->options.card,NULL,NULL,0)<0){fail(w,"Cannot allocate session display");goto done;}
    if(!s->demo)s->demo = m8_demo_create(s->rt, m8_session_pump, s);
    if (!s->demo) { fail(w, "Cannot allocate display observer"); goto done; }
    if (w->options.observe_input && m8_demo_enable_selection(s->demo) < 0) {
        fail(w, "Cannot allocate input display observer"); goto done;
    }
    if (!(s->checkpoint&&s->checkpoint->restored) && (m8_demo_boot(s->demo) < 0 ||
        (w->options.demo && m8_demo_prepare_simple(s->demo) < 0) ||
        (w->options.project && m8_demo_load_project(s->demo, w->options.project) < 0))) {
        fail(w, m8_demo_error(s->demo)); goto done;
    }
    if (w->options.play && !(s->checkpoint&&s->checkpoint->restored)) {
        int rc = w->options.demo ? m8_demo_transport(s->demo, 1) :
            m8_demo_tap(s->demo, M8_DEMO_SHIFT | M8_DEMO_PLAY);
        if (rc < 0) { fail(w, m8_demo_error(s->demo)); goto done; }
    }
    if (w->options.latency && m8_demo_advance(s->demo, .4) < 0) {
        fail(w, m8_demo_error(s->demo)); goto done;
    }
    /* Prime immutable media before starting audio; this retains no session. */
    if(s->checkpoint)m8_checkpoint_seed(s);
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
        if(input(w)<0){fail(w,"Session input failed");break;}
        execute_operation(w);
        if(s->explorer_paused)SDL_Delay(2);
        else if(s->checkpoint&&s->checkpoint->paused) {
            if(input(w)<0){fail(w,"Retained session input failed");break;}
            if(w->resume) {
                s->checkpoint->paused=0;s->resume_paused=0;w->resume=0;
                if(m8_session_fill(s,1)<0||m8live_start(s->audio)<0){fail(w,"Retained session audio start failed");break;}
            } else SDL_Delay(2);
        } else if (m8_session_step(s) < 0) { fail(w, s->error); break; }
        if(s->checkpoint) {
            if(input(w)<0){fail(w,"Checkpoint input queue failed");break;}
            if(!s->explorer_paused)m8_checkpoint_save(s,atomic_exchange_explicit(&w->checkpoint_request,0,memory_order_acq_rel));
        }
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
    w->audio_end=SDL_GetTicksNS();
    if(!atomic_load(&w->failed)&&s->checkpoint) {
        if(input(w)<0)fail(w,"Final checkpoint input failed");
        else {m8_checkpoint_save(s,2);m8_checkpoint_wait(s);}
    }
    publish(w, 1);
    if (!atomic_load(&w->failed)&&!s->explorer_paused&&!(s->checkpoint&&s->checkpoint->paused)) {
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
int m8_worker_output(m8_worker *w,m8live_output *output) {
    if(!w||!output||atomic_load_explicit(&w->done,memory_order_acquire))return -1;
    command c={.output=output};return m8_queue_write(&w->input,&c,sizeof c);
}
void m8_worker_checkpoint(m8_worker *w) {if(w)atomic_store_explicit(&w->checkpoint_request,1,memory_order_release);}
void m8_worker_stop(m8_worker *w) { if (w) atomic_store_explicit(&w->stop, 1, memory_order_release); }
void m8_worker_join(m8_worker *w) {
    if (w && w->thread && !w->joined) { SDL_WaitThread(w->thread, NULL); w->joined = 1; }
}
void m8_worker_destroy(m8_worker *w) {
    if (!w) return;
    m8_worker_stop(w); m8_worker_join(w);
    command pending;
    while(m8_queue_read(&w->input,&pending,sizeof pending)) {
        m8live_output_discard(pending.output);
        if(pending.operation){m8_worker_reply_release(&pending.operation->reply);free(pending.operation->bytes);free(pending.operation);}
    }
    operation *op=atomic_exchange(&w->reply,NULL);
    if(op){m8_worker_reply_release(&op->reply);free(op->bytes);free(op);}
    if(w->pending){m8_worker_reply_release(&w->pending->reply);free(w->pending->bytes);free(w->pending);}
    m8_queue_close(&w->input); m8_queue_close(&w->display);
    SDL_DestroyMutex(w->state_mutex); free(w);
}

static m8_session_result request(m8_worker *w,m8_worker_operation kind,const void *bytes,size_t size,int reset) {
    if(!w||atomic_load_explicit(&w->done,memory_order_acquire))return M8_SESSION_INVALID;
    if(!atomic_load_explicit(&w->ready,memory_order_acquire))return M8_SESSION_BUSY;
    int expected=0;
    if(!atomic_compare_exchange_strong(&w->operation_busy,&expected,1))return M8_SESSION_BUSY;
    operation *op=calloc(1,sizeof *op);
    if(!op){atomic_store(&w->operation_busy,0);return M8_SESSION_NOMEM;}
    op->reply.operation=kind;op->reset=reset;op->size=size;
    if(size){op->bytes=malloc(size);if(op->bytes)memcpy(op->bytes,bytes,size);}
    command c={.operation=op};
    m8_session_result result=size&&!op->bytes?M8_SESSION_NOMEM:m8_queue_write(&w->input,&c,sizeof c)?M8_SESSION_BUSY:M8_SESSION_OK;
    if(result){free(op->bytes);free(op);atomic_store(&w->operation_busy,0);}
    return result;
}
m8_session_result m8_worker_pause(m8_worker *w){return request(w,M8_WORKER_PAUSE,NULL,0,0);}
m8_session_result m8_worker_resume(m8_worker *w){return request(w,M8_WORKER_RESUME,NULL,0,0);}
m8_session_result m8_worker_replace(m8_worker *w,const void *bytes,size_t n,int reset) {
    if(!bytes||n<512||n>128u*1024*1024||n%512)return M8_SESSION_INVALID;
    return request(w,M8_WORKER_REPLACE,bytes,n,reset);
}
int m8_worker_take_reply(m8_worker *w,m8_worker_reply *out) {
    operation *op=w?atomic_exchange_explicit(&w->reply,NULL,memory_order_acq_rel):NULL;if(!op)return 0;
    *out=op->reply;
    if(out->operation==M8_WORKER_REPLACE&&out->result==M8_SESSION_OK) {
        size_t n=m8_queue_read(&w->display,NULL,w->display.capacity);atomic_fetch_add(&w->discarded,n);atomic_store(&w->reset,0);
    }
    free(op->bytes);free(op);atomic_store(&w->operation_busy,0);return 1;
}
void m8_worker_reply_release(m8_worker_reply *reply) {
    m8rt_session_release(&reply->media);free(reply->screen);reply->screen=NULL;
}
