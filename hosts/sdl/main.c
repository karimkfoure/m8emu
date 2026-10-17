#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "session.h"
#include "card.h"
#include "media.h"
#include "bridge.h"
#include "worker.h"
#include "handheld.h"
#include "host_probe.h"
#include "render_platform.h"
#include "aot.h"
#include "native.h"
#include "native_boot.h"
#include "metrics.h"
#include "profiling.h"
#include "thread_policy.h"
#include "storage.h"
#include "common.h"
#include "render.h"
#include "input.h"
#include "gamepads.h"
#include "log_overlay.h"
#include "SDL2_inprint.h"
#include "backends/m8.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdatomic.h>

#ifndef M8_NATIVE_APP_DEFAULT
#define M8_NATIVE_APP_DEFAULT 0
#endif
static _Atomic int stop_requested;
static void request_stop(int signal_number) {
    (void)signal_number;
    atomic_store_explicit(&stop_requested, 1, memory_order_relaxed);
}

extern SDL_AppResult SDL_AppEvent(void *, SDL_Event *);
typedef struct options {
    const char *firmware, *card, *image, *config, *import_folder, *export_folder, *aot, *project, *pcm, *host_cpus, *guest_cpu, *native_library, *native_regions, *native_boot_library, *session_script, *session_output;
    double seconds, queue_ms;
    unsigned slice_us, fps, render_stall_ms, display_kib, poll_ms, audio_frames;
    int headless, simulated, consumer_selected, demo, smoke, latency, read_only, play, no_play, overwrite, fullscreen, aot_timing, integer_scale, expect_silence, native_engine, native_boot;
    int video_probe, host_info, vsync, title_stats;
    unsigned window_scale;
    const char *record_log;
    unsigned profile_hz;
    int host_pmu;
    const char *pmu_sample, *profile_output;
} options;
typedef struct latency {
    m8_session *session;
    uint64_t input_ns, source_frame, input_cycles, consumed, queued;
    uint64_t onset_frame, onset_cycles, onset_ns;
    unsigned threshold, phase;
    int16_t samples[44100 * 2];
    size_t sample_count;
    unsigned release_peak;
} latency;
typedef struct pcm_capture {
    FILE *file;
    m8_session *session;
    uint64_t bytes, first_frame, end_frame, first_cycles, last_cycles;
} pcm_capture;

#ifndef M8_NATIVE_AUDIO_QUEUE_MS
#define M8_NATIVE_AUDIO_QUEUE_MS 20
#endif
#ifndef M8_NATIVE_POLL_MS
#define M8_NATIVE_POLL_MS 1
#endif
#define M8_STRING_VALUE_(value) #value
#define M8_STRING_VALUE(value) M8_STRING_VALUE_(value)
static void usage(void) {
    puts("m8emu [--firmware PATH] [--card PATH | --image PATH | --demo simple]\n"
         "  --headless             No window; simulated audio by default\n"
         "  --consumer sdl|simulated Select the audio consumer\n"
         "  --fullscreen           Fit the display; suitable for KMSDRM\n"
         "  --title-stats          Show potential realtime speed and RSS twice per second\n"
         "  --window-scale N       Initial window size, N times 320x240 (1..10)\n"
         "  --record-log PATH      New JSONL recording clocks and live stats\n"
         "  --integer-scale        Use the renderer's direct integer-scale path\n"
         "  --vsync 0|1            Require the renderer interval (default: 0)\n"
         "  --video-probe          Test three display presents without a guest\n"
         "  --host-info            Report SDL drivers and modes without a guest\n"
         "  --expect-silence       Check a loaded idle project for zero PCM\n"
         "  --fps N                Maximum display updates, 10..120 (default: 60)\n"
         "  --poll-ms N            Main-thread sleep, 1..20 ms (default: " M8_STRING_VALUE(M8_NATIVE_POLL_MS) ")\n"
         "  --audio-frames N       SDL period hint, 64..4096 (default: 256)\n"
         "  --guest-cpu N          Linux guest-thread CPU; default: scheduler\n"
         "  --profile-hz N         Guest-thread PC samples, 0..10000 (default: off)\n"
         "  --host-pmu             Guest-thread hardware counters (Linux)\n"
         "  --pmu-sample EVENT:N    Guest cycles/l1i-refill/l2d-refill samples\n"
         "  --profile-output PATH  New JSON file; excludes boot and clean release\n"
         "  --host-cpus LIST       Linux main/audio CPU list; default: scheduler\n"
         "  --render-stall-ms N     Test audio while the main thread is blocked\n"
         "  --display-queue-kib N   Display FIFO, power of two 4..1024 (default: 256)\n"
         "  --aot PATH             Load AOT; default env or bundled lib/libm8aot.so\n"
         "  --no-aot               Use guest execution without AOT\n"
         "  --aot-timing           Enable diagnostic per-call AOT clocks\n"
         "  --native-engine        Enable guarded native switching (ARM64 default)\n"
         "  --no-native-engine     Override M8_NATIVE_ENGINE=1\n"
         "  --native-library PATH  Native instruction handlers; env or bundled lib\n"
         "  --native-boot          Select checked cold boot and native session owner\n"
         "  --no-native-boot       Use the reference boot path; override M8_NATIVE_BOOT\n"
         "  --native-boot-library PATH  Checked cold handlers; env or bundled lib\n"
         "  --native-regions PATH  Verified native regions; env or bundled lib\n"
         "  --session-script PATH  Headless diagnostic commands; - reads stdin\n"
         "  --session-output DIR   New diagnostic files in an existing directory\n"
         "  --project NAME         Load /Songs/NAME.M8S; select the first Phrase\n"
         "  --pcm-tap PATH         Capture guest live PCM, including ring prefill\n"
         "  --seconds N            Stop after N live seconds (headless default: 60)\n"
         "  --client-smoke         Dummy video; verify one DOWN press and release\n"
         "  --latency-test         Measure a stopped demo note at the consumer\n"
         "  --read-only            Reject guest writes; do not save the card\n"
         "  --import DIRECTORY     Merge into private card storage before boot\n"
         "  --overwrite            Permit imported files to replace card files\n"
         "  --export DIRECTORY     Export the card without booting, to a new/empty folder\n"
         "  --queue-ms N           Ring capacity, 5..1000 ms (default: " M8_STRING_VALUE(M8_NATIVE_AUDIO_QUEUE_MS) ")\n"
         "  --slice-us N           Guest slice, 100..10000 us (default: 3000)\n"
         "  --config NAME          m8c config file in its SDL preference directory\n"
         "  --play / --no-play     Start playback / leave transport stopped\n"
         "The default card is $XDG_DATA_HOME/m8-live/card.img or\n"
#ifdef __APPLE__
         "$HOME/Library/Application Support/m8-live/card.img. Clean exit saves it with .bak.");
#else
         "$HOME/.local/share/m8-live/card.img. Clean exit saves it with .bak.");
#endif
}
static double number(const char *s) {
    char *end;
    errno = 0;
    double value = strtod(s, &end);
    return errno || end == s || *end || !isfinite(value) ? -1 : value;
}
static int parse(int argc, char **argv, options *o) {
    *o = (options){.firmware=M8_DEFAULT_FIRMWARE, .seconds=-1, .queue_ms=M8_NATIVE_AUDIO_QUEUE_MS, .slice_us=3000, .fps=60, .display_kib=256, .poll_ms=M8_NATIVE_POLL_MS, .audio_frames=256, .native_engine=-1, .native_boot=-1, .vsync=-1};
    unsigned media = 0;
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(); return 1; }
        if (!strcmp(a, "--headless")) o->headless = 1;
        else if (!strcmp(a, "--fullscreen")) o->fullscreen = 1;
        else if (!strcmp(a, "--title-stats")) o->title_stats = 1;
        else if (!strcmp(a, "--integer-scale")) o->integer_scale = 1;
        else if (!strcmp(a, "--video-probe")) o->video_probe = 1;
        else if (!strcmp(a, "--host-info")) o->host_info = 1;
        else if (!strcmp(a, "--expect-silence")) o->expect_silence = 1;
        else if (!strcmp(a, "--client-smoke")) o->smoke = 1;
        else if (!strcmp(a, "--latency-test")) o->latency = 1;
        else if (!strcmp(a, "--read-only")) o->read_only = 1;
        else if (!strcmp(a, "--play")) o->play = 1;
        else if (!strcmp(a, "--no-play")) o->no_play = 1;
        else if (!strcmp(a, "--overwrite")) o->overwrite = 1;
        else if (!strcmp(a, "--no-aot")) o->aot = "";
        else if (!strcmp(a, "--aot-timing")) o->aot_timing = 1;
        else if (!strcmp(a, "--native-engine")) o->native_engine = 1;
        else if (!strcmp(a, "--no-native-engine")) o->native_engine = 0;
        else if (!strcmp(a, "--native-boot")) o->native_boot = 1;
        else if (!strcmp(a, "--no-native-boot")) o->native_boot = 0;
        else if (!strcmp(a, "--host-pmu")) o->host_pmu = 1;
        else {
            if (++i >= argc) { fprintf(stderr, "Missing value for %s\n", a); return -1; }
            const char *v = argv[i];
            if (!strcmp(a, "--firmware")) o->firmware = v;
            else if (!strcmp(a, "--record-log")) o->record_log = v;
            else if (!strcmp(a, "--window-scale")) {
                double n = number(v);
                if (floor(n) != n || n < 1 || n > 10) return -1;
                o->window_scale = (unsigned)n;
            }
            else if (!strcmp(a, "--vsync")) {
                if (!strcmp(v, "0")) o->vsync = 0;
                else if (!strcmp(v, "1")) o->vsync = 1;
                else return -1;
            }
            else if (!strcmp(a, "--aot")) o->aot = v;
            else if (!strcmp(a, "--native-library")) o->native_library = v;
            else if (!strcmp(a, "--native-regions")) o->native_regions = v;
            else if (!strcmp(a, "--native-boot-library")) o->native_boot_library = v;
            else if (!strcmp(a, "--session-script")) o->session_script = v;
            else if (!strcmp(a, "--session-output")) o->session_output = v;
            else if (!strcmp(a, "--profile-output")) o->profile_output = v;
            else if (!strcmp(a, "--pmu-sample")) o->pmu_sample = v;
            else if (!strcmp(a, "--profile-hz")) {
                double n = number(v);
                if (floor(n) != n || n < 0 || n > 10000) return -1;
                o->profile_hz = (unsigned)n;
            }
            else if (!strcmp(a, "--project")) o->project = v;
            else if (!strcmp(a, "--pcm-tap")) o->pcm = v;
            else if (!strcmp(a, "--host-cpus")) o->host_cpus = v;
            else if (!strcmp(a, "--guest-cpu")) {
                double n = number(v);
                if (floor(n) != n || n < 0 || n > 1023) return -1;
                o->guest_cpu = v;
            }
            else if (!strcmp(a, "--poll-ms")) {
                double n = number(v);
                if (floor(n) != n || n < 1 || n > 20) return -1;
                o->poll_ms = (unsigned)n;
            }
            else if (!strcmp(a, "--audio-frames")) {
                double n = number(v);
                if (floor(n) != n || n < 64 || n > 4096) return -1;
                o->audio_frames = (unsigned)n;
                if (o->audio_frames & (o->audio_frames-1)) return -1;
            }
            else if (!strcmp(a, "--card")) { o->card = v; ++media; }
            else if (!strcmp(a, "--image")) { o->image = v; ++media; }
            else if (!strcmp(a, "--config")) o->config = v;
            else if (!strcmp(a, "--import")) o->import_folder = v;
            else if (!strcmp(a, "--export") || !strcmp(a, "export")) o->export_folder = v;
            else if (!strcmp(a, "--demo")) {
                if (strcmp(v, "simple")) { fputs("Only --demo simple is supported\n", stderr); return -1; }
                o->demo = 1; ++media;
            } else if (!strcmp(a, "--consumer")) {
                o->consumer_selected = 1;
                if (!strcmp(v, "simulated")) o->simulated = 1;
                else if (!strcmp(v, "sdl")) o->simulated = 0;
                else return -1;
            } else if (!strcmp(a, "--seconds")) {
                o->seconds = number(v); if (o->seconds <= 0) return -1;
            } else if (!strcmp(a, "--queue-ms")) o->queue_ms = number(v);
            else if (!strcmp(a, "--display-queue-kib")) {
                double n = number(v);
                if (floor(n) != n || n < 4 || n > 1024) return -1;
                o->display_kib = (unsigned)n;
                if (o->display_kib & (o->display_kib-1)) return -1;
            } else if (!strcmp(a, "--fps") || !strcmp(a, "--render-stall-ms")) {
                double n = number(v);
                if (floor(n) != n || n < (!strcmp(a, "--fps") ? 10 : 1) ||
                    n > (!strcmp(a, "--fps") ? 120 : 60000)) return -1;
                if (!strcmp(a, "--fps")) o->fps = (unsigned)n;
                else o->render_stall_ms = (unsigned)n;
            }
            else if (!strcmp(a, "--slice-us")) {
                double n = number(v);
                if (n < 100 || n > 10000 || floor(n) != n) return -1;
                o->slice_us = (unsigned)n;
            } else { fprintf(stderr, "Unknown option: %s\n", a); return -1; }
        }
    }
    if (o->video_probe || o->host_info) return o->video_probe && o->host_info ? -1 : 0;
    if ((o->profile_hz || o->host_pmu || o->pmu_sample) != (o->profile_output != NULL) ||
        (o->profile_output && (o->session_script || o->export_folder || o->latency || o->smoke))) {
        fputs("Live guest profiling needs --profile-output and no session, export, latency or smoke mode\n", stderr);
        return -1;
    }
    char native_error[256];
    o->native_engine = m8_host_native_choose(o->native_engine, getenv("M8_NATIVE_ENGINE"),
                                            M8_NATIVE_APP_DEFAULT, native_error, sizeof native_error);
    if (o->native_engine < 0) {
        if (!M8_NATIVE_APP_DEFAULT) { fprintf(stderr, "%s\n", native_error); return -1; }
        m8_host_native_report_fallback(stderr, "selection", native_error);
        o->native_engine = 0;
    }
    /* An explicit guest-only option remains a complete reference selection. */
    if (o->aot && !*o->aot) o->native_engine = 0;
    int boot_cli = o->native_boot;
    o->native_boot = m8_host_native_boot_choose(boot_cli, getenv("M8_NATIVE_BOOT"),
        M8_NATIVE_BOOT_DEFAULT, native_error, sizeof native_error);
    if (o->native_boot < 0) { fprintf(stderr, "%s\n", native_error); return -1; }
    if (!o->native_engine) {
        const char *boot_env = getenv("M8_NATIVE_BOOT");
        if (o->native_boot && (boot_cli == 1 || (boot_cli < 0 && boot_env && !strcmp(boot_env,"1")))) {
            fprintf(stderr,"Native boot requires --native-engine and guarded AOT\n");return -1;
        }
        o->native_boot = 0;
    }
    if ((o->session_script != NULL) != (o->session_output != NULL) ||
        (o->session_script && (!o->headless || o->demo || o->project || o->export_folder ||
                              o->smoke || o->latency || o->pcm || o->play || o->no_play ||
                              o->consumer_selected || o->expect_silence))) {
        fputs("A session script needs --headless and --session-output without live controls\n", stderr);
        return -1;
    }
    if (media > 1 || o->queue_ms < 5 || o->queue_ms > 1000 ||
        (o->smoke && (!o->demo || o->headless || o->latency)) ||
        (o->latency && (!o->demo || o->no_play || o->play)) ||
        (o->overwrite && !o->import_folder) ||
        (o->import_folder && (o->read_only || o->demo || o->image)) ||
        (o->export_folder && (o->demo || o->image || o->import_folder)) ||
        (o->project && (o->demo || o->export_folder)) ||
        (o->expect_silence && (!o->project || o->no_play)) ||
        (o->pcm && (o->latency || o->export_folder))) return -1;
    if (o->headless && !o->consumer_selected) o->simulated = 1;
    if (o->seconds < 0) o->seconds = o->headless ? 60 : o->smoke ? 5 : 0;
    if (o->latency) o->seconds = 1.0;
    return 0;
}
static void observe(void *context, const unsigned char *pcm, size_t n,
                    const m8rt_audio_position *position) {
    latency *l = context;
    for (size_t i = 0; i < n / 4; ++i) {
        int left = (int16_t)(pcm[4*i] | pcm[4*i+1] << 8);
        int right = (int16_t)(pcm[4*i+2] | pcm[4*i+3] << 8);
        unsigned peak = (unsigned)(abs(left) > abs(right) ? abs(left) : abs(right));
        if (l->phase == 1) {
            if (!l->onset_ns && peak >= l->threshold) {
                l->onset_frame = position->frame + i;
                l->onset_cycles = position->cycles;
                l->onset_ns = SDL_GetTicksNS();
            }
            if (l->sample_count + 2 <= sizeof l->samples / sizeof l->samples[0]) {
                l->samples[l->sample_count++] = (int16_t)left;
                l->samples[l->sample_count++] = (int16_t)right;
            }
        } else if (l->phase == 3 && peak > l->release_peak) l->release_peak = peak;
    }
}
static void observe_pcm(void *context, const unsigned char *pcm, size_t n,
                        const m8rt_audio_position *position) {
    pcm_capture *c = context;
    const char *first = n % 4 || n != position->frames*4 ? "complete_block" :
                        c->bytes && position->frame != c->end_frame ? "contiguous_frames" : NULL;
    if (first) {
        snprintf(c->session->error, sizeof c->session->error,
                 "PCM tap failed: first=%s bytes=%zu block_frames=%u frame=%" PRIu64 " expected=%" PRIu64,
                 first, n, position->frames, position->frame, c->end_frame);
        return;
    }
    if (!c->bytes) { c->first_frame = position->frame; c->first_cycles = position->cycles; }
    size_t written = fwrite(pcm, 1, n, c->file);
    c->bytes += written;
    if (written != n) {
        snprintf(c->session->error, sizeof c->session->error,
                 "PCM tap failed: first=write written=%zu expected=%zu error=%d", written, n, errno);
        return;
    }
    c->end_frame = position->frame + position->frames;
    c->last_cycles = position->cycles;
}
static int latency_result(m8_session *s, latency *l) {
    uint64_t output = m8live_get(s->audio, 13);
    uint64_t output_ns = m8live_get(s->audio, 14);
    if (!l->onset_ns || !output || output_ns < l->input_ns || l->sample_count < 16384)
        return -1;
    double expected = 440 * pow(2, (48.0 - 69) / 12);
    int passed = 1;
    for (unsigned channel = 0; channel < 2; ++channel) {
        size_t frames = l->sample_count / 2;
        size_t begin = (frames - 8192) / 2, end = begin + 8192;
        size_t first = 0, last = 0, crossings = 0;
        double mean = 0;
        for (size_t i = begin; i < end; ++i) mean += l->samples[2*i + channel];
        mean /= 8192;
        for (size_t i = begin+1; i < end; ++i) {
            if (l->samples[2*(i-1)+channel] <= mean && l->samples[2*i+channel] > mean) {
                if (!crossings) first = i;
                last = i; ++crossings;
            }
        }
        double hz = crossings > 2 ? 44100.0 * (crossings - 1) / (last - first) : 0;
        double cents = hz > 0 ? 1200 * log2(hz / expected) : 1e9;
        if (fabs(cents) > 5) passed = 0;
        printf("latency_pitch channel=%u hz=%.6f cents=%.6f\n", channel, hz, cents);
    }
    int same = output - 1 + s->output_origin == l->onset_frame;
    printf("latency input_to_consumer_ms=%.6f source_guest_ms=%.6f queue_ms=%.6f "
           "release_peak=%u same_source_frame=%d\n",
           (output_ns-l->input_ns)/1e6, (l->onset_cycles-l->input_cycles)/600000.0,
           l->queued*1000.0/44100, l->release_peak, same);
    return passed && same && l->release_peak <= 8 ? 0 : -1;
}
static void latency_control(void *context, unsigned phase, uint64_t input_ns) {
    latency *l = context;
    m8_session *s = l->session;
    if (phase == 1) {
        l->source_frame = m8rt_get(s->rt, M8RT_LOCAL_PCM_BYTES) / 4;
        l->input_cycles = m8rt_get(s->rt, M8RT_GUEST_CYCLES);
        l->consumed = m8live_get(s->audio, 2); l->queued = m8live_get(s->audio, 0);
        l->input_ns = input_ns;
    }
    l->phase = phase;
}
static void status(m8_session *s, const m8_worker_state *p, double cpu_base, double main_base,
                   double main_end, uint64_t renders) {
    uint64_t end = p->end_ns ? p->end_ns : SDL_GetTicksNS();
    double elapsed = (end - s->started) / 1e9;
    double frames = p->frames - s->produced_base;
    double cpu = (p->end_ns && p->process_seconds >= 0 ? p->process_seconds : m8_process_seconds()) - cpu_base;
    double main_cpu = (main_end >= 0 ? main_end : m8_thread_seconds()) - main_base;
    double consumer_seconds = p->consumer_started ? (end-p->consumer_started)/1e9 : 0;
    double steady = consumer_seconds > 0 ?
        (m8live_get(s->audio, 3)-p->consumer_base)/44100.0/consumer_seconds : 0;
    printf("live_seconds=%.6f speed=%.6f cpu_seconds=%.6f cpu_cores=%.6f rss_mib=%.3f "
           "current_rss_mib=%.3f live_frames=%" PRIu64 " service_seconds=%.9f renders=%" PRIu64 " guest_snapshot_seconds=%.6f "
           "guest_cpu=%.6f main_cpu=%.6f other_cpu=%.6f "
           "produced=%" PRIu64 " consumed=%" PRIu64 " actual_consumed=%" PRIu64 " underruns=%" PRIu64 " "
           "missing=%" PRIu64 " rejected=%" PRIu64 " callback_errors=%" PRIu64 " "
           "nonzero=%" PRIu64 " peak=%" PRIu64 " max_queue_ms=%.3f playhead_rows=0x%04x\n",
           elapsed, p->service_seconds ? frames / 44100 / p->service_seconds : 0,
           cpu, elapsed > 0 ? cpu / elapsed : 0, m8_max_rss_mib(),
           m8_current_rss_mib(), p->frames-s->produced_base, p->service_seconds, renders,
           p->snapshot_ns >= s->started ? (p->snapshot_ns-s->started)/1e9 : 0,
           p->cpu_seconds, main_cpu, cpu - p->cpu_seconds - main_cpu,
           m8live_get(s->audio, 1), m8live_get(s->audio, 2), m8live_get(s->audio, 3), m8live_get(s->audio, 4),
           m8live_get(s->audio, 5), m8live_get(s->audio, 8), m8live_get(s->audio, 9),
           m8live_get(s->audio, 15), m8live_get(s->audio, 16),
           m8live_get(s->audio, 6)*1000.0/44100, p->rows);
    printf("device_rate=%" PRIu64 " device_frames=%" PRIu64 " max_request=%" PRIu64 " delivery=%.6f "
           "steady_delivery=%.6f audio_startup_ms=%.3f "
           "display_resets=%" PRIu64 " display_skipped=%" PRIu64 " input_packets=%" PRIu64
           " device_latency_ms=%.3f\n",
           m8live_get(s->audio, 11), m8live_get(s->audio, 10), m8live_get(s->audio, 12),
           elapsed > 0 ? m8live_get(s->audio, 3) / 44100.0 / elapsed : 0,
           steady, p->consumer_started ? (p->consumer_started-s->started)/1e6 : 0,
           p->display_resets, p->display_skipped, p->inputs, m8live_get(s->audio, 19) / 1e3);
    printf("aot_timing=%d native_calls=%" PRIu64 " native_ms=%.3f hook_ms=%.3f native_share=%.6f\n",
           p->timing_enabled, p->native_calls, p->native_ns/1e6, p->hook_ns/1e6,
           p->runtime_ns ? (double)p->native_ns / p->runtime_ns : 0);
    const m8rt_native_live_stats *e = &p->native_engine;
    printf("native_engine=%d native_active=%d native_code_disabled=%d native_enters=%" PRIu64
           " native_leaves=%" PRIu64 " native_leaves_per_second=%.6f native_leaves_per_audio_second=%.6f native_rejected_entries=%" PRIu64
           " native_aot_calls=%" PRIu64 " native_region_calls=%" PRIu64 " native_region_guest=%" PRIu64
           " native_step_calls=%" PRIu64 " native_step_guest=%" PRIu64 " native_source_checks=%" PRIu64
           " native_cpu_import_words=%" PRIu64 " native_cpu_export_words=%" PRIu64 " native_pc_redirects=%" PRIu64,
           e->enabled, e->active, e->code_disabled, e->enters, e->leaves,
           elapsed > 0 ? e->leaves/elapsed : 0, frames > 0 ? e->leaves/(frames/44100) : 0, e->rejected_entries,
           e->aot_calls, e->region_calls, e->region_guest, e->step_calls, e->step_guest, e->source_checks,
           e->cpu_import_words, e->cpu_export_words, e->pc_redirects);
    for (unsigned i = 0; i < M8RT_NATIVE_LEAVE_COUNT; ++i)
        printf(" native_leave_%s=%" PRIu64, m8rt_native_leave_name(i), e->reasons[i]);
    for (unsigned i = 0; i < M8RT_NATIVE_ARCH_COUNT; ++i)
        printf(" native_arch_%s=%" PRIu64, m8rt_native_arch_leave_name(i), e->architecture[i]);
    printf(" native_event_mode=%d native_handoff_pc_overflow=%" PRIu64, e->event_loop, e->handoff_pc_overflow);
    for (unsigned i = 0; i < M8RT_NATIVE_HANDOFF_PCS; ++i) {
        const m8rt_native_handoff_pc *p = &e->handoff_pcs[i];
        if (p->count) printf(" native_pc_0x%08x_%s=%" PRIu64, p->pc,
                            m8rt_native_leave_name(p->reason), p->count);
    }
#define EVENT(field) printf(" native_events_" #field "=%" PRIu64, e->events.field)
    EVENT(entries);EVENT(completed);EVENT(logical_slices);EVENT(enum_ticks);
    EVENT(main_hooks);EVENT(wait_hooks);EVENT(delay_hooks);EVENT(sampling_hooks);EVENT(audio_hooks);
    EVENT(irq_entries);EVENT(exception_returns);EVENT(memory_returns);EVENT(required_returns);
    EVENT(budget_returns);EVENT(host_returns);
#undef EVENT
    printf(" native_last_leave_pc=0x%08x native_last_leave_reason=%u\n", e->last_leave_pc, e->last_leave_reason);
    fflush(stdout);
}
static int drain_display(m8_worker *worker, int rendered) {
    unsigned char bytes[4096];
    for (unsigned i = 0; i < 64; ++i) {
        int reset;
        size_t n = m8_worker_display(worker, bytes, sizeof bytes, &reset);
        if (reset) {
            m8_bridge_reset_decoder();
            if (rendered) renderer_clear_screen();
        }
        if (n && m8_bridge_feed(bytes, n) < 0) return -1;
        if (!n) break;
    }
    return 0;
}
static int guest_thread_setup(void *context, char *error, size_t capacity) {
    const options *o = context;
    return m8_thread_policy_bind(o->guest_cpu, error, capacity);
}
/* Three samples at 2 Hz give about one second of smoothing. Recording
 * diagnostics stay on the main thread and are opt-in. */
typedef struct title_sample { uint64_t frames; double guest_cpu; } title_sample;
typedef struct title_history { title_sample samples[3]; unsigned count; } title_history;
static int record_stats(FILE *file, SDL_Window *window, m8_session *s,
                        const m8_worker_state *p, uint64_t *last_ns,
                        uint64_t *last_frames, double *last_cpu, title_history *history) {
    uint64_t now = m8_monotonic_ns();
    double cpu = m8_process_seconds();
    double seconds = *last_ns ? (now - *last_ns) / 1e9 : 0;
    unsigned slot = history->count % 3;
    title_sample base = history->samples[history->count >= 2 ? (slot + 1) % 3 : 0];
    double busy = p->cpu_seconds - base.guest_cpu;
    double speed = busy > 0 ? (p->frames - base.frames) / 44100.0 / busy : 0;
    history->samples[slot] = (title_sample){p->frames, p->cpu_seconds};
    ++history->count;
    double ram_mb = m8_current_rss_mib() * 1.048576;
    double percent = seconds > 0 && cpu >= 0 && *last_cpu >= 0 ?
        100 * (cpu - *last_cpu) / seconds : 0;
    const char *engine = p->native_engine.active ? "native" : p->native_calls ? "aot" : "guest";
    if (window) {
        char title[160];
        snprintf(title, sizeof title, "M8 / %.1fx realtime / %.0fMB RAM", speed, ram_mb);
        SDL_SetWindowTitle(window, title);
    }
    int failed = 0;
    if (file) {
        failed = fprintf(file, "{\"event\":\"stats\",\"monotonic_ns\":%" PRIu64
            ",\"realtime\":%.6f,\"cpu_percent\":%.3f,\"rss_mb\":%.3f,\"engine\":\"%s\",\"frames\":%" PRIu64
            ",\"consumed\":%" PRIu64 ",\"underruns\":%" PRIu64 ",\"missing_frames\":%" PRIu64
            ",\"display_resets\":%" PRIu64 ",\"display_skipped\":%" PRIu64 "}\n",
            now, speed, percent, ram_mb, engine, p->frames, m8live_get(s->audio, 3),
            m8live_get(s->audio, 4), m8live_get(s->audio, 5), p->display_resets, p->display_skipped) < 0;
        if (fflush(file)) failed = 1;
    }
    *last_ns = now; *last_frames = p->frames; *last_cpu = cpu;
    return failed ? -1 : 0;
}
int main(int argc, char **argv) {
    options o;
    int parsed = parse(argc, argv, &o);
    if (parsed) { if (parsed < 0) usage(); return parsed < 0 ? 2 : 0; }
    if (o.host_info) return m8_host_info(stdout);
    if (o.video_probe) return m8_host_video_probe(stdout, o.fullscreen, o.integer_scale, o.vsync);
    char automatic_firmware[4096];
    if (!o.firmware || !o.firmware[0]) {
        const char *base = SDL_GetBasePath();
        int size = base ? snprintf(automatic_firmware, sizeof automatic_firmware,
                                  "%sfirmware/M8_V6_5_2C_HEADLESS.bin", base) : -1;
        if (size < 0 || (size_t)size >= sizeof automatic_firmware) {
            fputs("Cannot find the bundled firmware path\n", stderr);
            return 2;
        }
        o.firmware = automatic_firmware;
    }
    signal(SIGINT, request_stop);
    signal(SIGTERM, request_stop);
    m8_session s = {0};
    FILE *record = NULL;
    SDL_Window *stats_window = NULL;
    uint64_t record_ns = 0, record_frames = 0;
    double record_cpu = -1;
    title_history title = {0};
    pcm_capture capture = {.session=&s};
    m8_card *card = NULL;
    m8_worker *worker = NULL;
    m8_worker_state state = {0};
    m8_app_profile profile = {.hz=o.profile_hz, .pmu=o.host_pmu,
                             .sample=o.pmu_sample, .aot=o.aot};
    m8_host_aot *aot = NULL;
    m8_host_native *native = NULL;
    m8_host_native_boot *native_boot = NULL;
    struct app_context ctx = {0};
    latency *l = NULL;
    int rendered = 0, gamepads = 0, result = 1;
    int smoke_down = 0, smoke_up = 0, smoke_checked = 0;
    unsigned initial_x = 0, initial_y = 0;
    uint64_t renders = 0, renders_base = 0, last_render = 0, last_status = 0, smoke_down_ns = 0;
    char error[512] = {0}, delivery_error[512] = {0};
    if (o.record_log) {
        record = fopen(o.record_log, "wx");
        if (!record) { snprintf(error, sizeof error, "Cannot create recording log: error=%d", errno); goto close; }
    }
    if (o.profile_output) {
        profile.output = fopen(o.profile_output, "wx");
        if (!profile.output) {
            snprintf(error, sizeof error, "Cannot create guest profile: path=%s error=%d", o.profile_output, errno);
            goto close;
        }
    }
    if (o.host_cpus && m8_thread_policy_bind(o.host_cpus, error, sizeof error)) goto close;
    if (o.pcm) {
        capture.file = fopen(o.pcm, "wbx");
        if (!capture.file) {
            snprintf(error, sizeof error, "Cannot create new PCM tap: path=%s error=%d", o.pcm, errno);
            goto close;
        }
        s.observe = observe_pcm; s.observe_context = &capture;
    }
    if (o.export_folder) {
        card = m8_card_open(o.card, 1, 0, error, sizeof error);
        if (!card || m8_media_export(m8_card_path(card), o.export_folder, error, sizeof error)) goto close;
        printf("card_exported=%s destination=%s\n", m8_card_path(card), o.export_folder);
        result = 0; goto close;
    }
    if (o.smoke) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        o.simulated = 1;
    }
    if (!SDL_Init(SDL_INIT_EVENTS)) { snprintf(error, sizeof error, "%s", SDL_GetError()); goto close; }
    char audio_hint[16];
    snprintf(audio_hint, sizeof audio_hint, "%u", o.audio_frames);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, audio_hint,
                           o.audio_frames == 256 ? SDL_HINT_DEFAULT : SDL_HINT_OVERRIDE);
    printf("host_cpus=%s guest_cpu=%s fps=%u poll_ms=%u audio_frames_hint=%u slice_us=%u\n",
           o.host_cpus ? o.host_cpus : "scheduler", o.guest_cpu ? o.guest_cpu : "scheduler",
           o.fps, o.poll_ms, o.audio_frames, o.slice_us);
    ctx.conf = config_initialize((char *)o.config);
    m8_handheld_config_defaults(&ctx.conf);
    if (!o.smoke && !o.headless) config_read(&ctx.conf);
    if (o.integer_scale) ctx.conf.integer_scaling = 1;
    ctx.conf.audio_enabled = 1;
    ctx.app_state = RUN; ctx.device_connected = 1;
    if (!o.headless) {
        m8_render_configure(o.fullscreen);
        m8_render_configure_vsync(o.vsync);
        log_overlay_init();
        if (!renderer_initialize(&ctx.conf)) { snprintf(error, sizeof error, "Renderer: %s", SDL_GetError()); goto close; }
        rendered = 1;
        if (o.window_scale || o.title_stats) {
            int count = 0;
            SDL_Window **windows = SDL_GetWindows(&count);
            if (count) stats_window = windows[0];
            SDL_free(windows);
            if (!stats_window || (o.window_scale && !SDL_SetWindowSize(stats_window,
                    320 * (int)o.window_scale, 240 * (int)o.window_scale))) {
                snprintf(error, sizeof error, "Cannot set recording window: %s", SDL_GetError()); goto close;
            }
            if (o.title_stats) {
                char title[160];
                snprintf(title, sizeof title, "M8 / 0.0x realtime / %.0fMB RAM", m8_current_rss_mib() * 1.048576);
                SDL_SetWindowTitle(stats_window, title);
            }
        }
        if (record) {
            fprintf(record, "{\"event\":\"window\",\"monotonic_ns\":%" PRIu64 "}\n", m8_monotonic_ns());
            fflush(record);
        }
        if (gamepads_initialize() < 0) { snprintf(error, sizeof error, "Gamepad initialization failed"); goto close; }
        gamepads = 1;
    }
    if (!o.demo && !o.image) {
        card = m8_card_open(o.card, o.read_only, 1, error, sizeof error);
        if (!card) goto close;
        if (m8_card_protect_file(card, o.firmware)) {
            snprintf(error, sizeof error, "%s", m8_card_error(card)); goto close;
        }
    }
    m8rt_options rt_options = {0, 1, 1, 0};
    if (M8_NATIVE_APP_DEFAULT && o.native_engine &&
        m8_host_native_default_environment(error, sizeof error)) {
        m8_host_native_report_fallback(stderr, "environment", error);
        o.native_engine = 0;
        *error = 0;
    }
    if (o.aot_timing) SDL_setenv_unsafe("M8RT_AOT_TIMING", "1", 1);
    aot = m8_host_aot_preload(o.aot, error, sizeof error);
    if (!aot && M8_NATIVE_APP_DEFAULT && o.native_engine) {
        m8_host_native_report_fallback(stderr, "aot_load", error);
        o.native_engine = 0;
        aot = m8_host_aot_preload("", error, sizeof error);
    }
    if (!aot) goto close;
    native = m8_host_native_preload_policy(o.native_engine, M8_NATIVE_APP_DEFAULT,
        o.native_library, o.native_regions, aot, error, sizeof error);
    if (!native) goto close;
    if (*m8_host_native_fallback(native))
        m8_host_native_report_fallback(stderr, "native_load", m8_host_native_fallback(native));
    native_boot = m8_host_native_boot_preload(o.native_boot, o.native_boot_library,
        native, aot, error, sizeof error);
    if (!native_boot) goto close;
    s.rt = m8rt_create_aot(o.firmware, card ? m8_card_path(card) : o.image,
                          &rt_options, m8_host_aot_requirements(aot) | m8_host_native_requirements(native));
    if (!s.rt || *m8rt_error(s.rt)) { snprintf(error, sizeof error, "%s", m8rt_error(s.rt)); goto close; }
    if (!o.native_boot && m8_host_aot_attach(s.rt, aot, error, sizeof error)) goto close;
    profile.aot = m8_host_aot_path(aot);
    int native_loaded = m8_host_native_enabled(native);
    if (o.native_boot ? m8_host_native_boot_attach(s.rt, native_boot, error, sizeof error) :
        m8_host_native_attach_policy(s.rt, native, M8_NATIVE_APP_DEFAULT, error, sizeof error)) goto close;
    if (!o.native_boot && m8_host_native_boot_attach(s.rt, native_boot, error, sizeof error)) goto close;
    if (native_loaded && !m8_host_native_enabled(native))
        m8_host_native_report_fallback(stderr, "native_admission", m8_host_native_fallback(native));
    if (m8_host_native_enabled(native)) printf("native_library=%s native_regions=%s native_chain=%u native_chain_slots=%zu\n",
        m8_host_native_step_path(native), m8_host_native_region_path(native),
        m8_host_native_chain_slots(native) != 0, m8_host_native_chain_slots(native));
    if (m8_host_aot_path(aot)) printf("aot_library=%s entries=%u services=%u\n",
        m8_host_aot_path(aot), m8_host_aot_entries(aot), m8_host_aot_services(aot));
    if (m8_host_native_boot_path(native_boot)) printf("native_boot_library=%s\n",m8_host_native_boot_path(native_boot));
    if (o.demo && m8_card_empty_runtime(s.rt)) { snprintf(error, sizeof error, "Cannot format the demo card"); goto close; }
    int read_only = card ? m8_card_read_only(card) : o.read_only;
    if (m8rt_sd_set_read_only(s.rt, read_only)) {
        snprintf(error, sizeof error, "Cannot select card policy: read_only=%d", read_only); goto close;
    }
    if (m8rt_set_audio_tap(s.rt, 1)) {
        snprintf(error, sizeof error, "Cannot select audio tap: local=1"); goto close;
    }
    if (m8rt_set(s.rt, 1, 1)) {
        snprintf(error, sizeof error, "Cannot enable PCM capture: field=1 value=1"); goto close;
    }
    if (o.import_folder && m8_media_import(s.rt, o.import_folder, o.overwrite, error, sizeof error)) goto close;
    m8_bridge_attach(s.rt, rendered);
    if (o.latency) {
        l = calloc(1, sizeof *l);
        if (!l) goto close;
        l->session = &s; l->threshold = 32;
        s.observe = observe; s.observe_context = l; s.onset_threshold = l->threshold;
    }
    m8_worker_options wo = {
        .capacity = (unsigned)llround(o.queue_ms*44100/1000), .slice_us = o.slice_us,
        .display_capacity = o.display_kib * 1024,
        .simulated = o.simulated, .demo = o.demo, .latency = o.latency,
        .smoke = o.smoke, .display = rendered,
        .project = o.project, .session_script = o.session_script, .session_output = o.session_output,
        .play = !o.no_play && !o.latency && (o.play || o.project || (o.demo && o.headless)),
        .thread_seconds = m8_thread_seconds, .process_seconds = m8_process_seconds,
        .thread_setup = o.guest_cpu ? guest_thread_setup : NULL, .thread_setup_context = &o,
        .measurement_start = profile.output ? m8_app_profile_start : NULL,
        .measurement_stop = m8_app_profile_stop, .measurement_finish = m8_app_profile_finish,
        .measurement_context = &profile,
        .control = latency_control, .control_context = l
    };
    worker = m8_worker_start(&s, &wo);
    if (!worker) { snprintf(error, sizeof error, "Cannot start guest thread: %s", SDL_GetError()); goto close; }
    m8_bridge_transport(m8_worker_send, worker);
    if (o.session_script) {
        while (!stop_requested) {
            m8_worker_snapshot(worker, &state);
            if (state.done) break;
            SDL_Delay(1);
        }
        m8_worker_stop(worker); m8_worker_join(worker); m8_worker_snapshot(worker, &state);
        if (state.failed || !state.script.complete || stop_requested) {
            snprintf(error, sizeof error, "%s", state.failed ? state.error : "Diagnostic session interrupted");
            goto close;
        }
        if (card && !m8_card_read_only(card)) {
            if (m8_card_save(card, s.rt)) { snprintf(error, sizeof error, "%s", m8_card_error(card)); goto close; }
            printf("card_saved=%s backup=%s.bak\n", m8_card_path(card), m8_card_path(card));
        }
        printf("session_done={\"complete\":true,\"contiguous\":true,\"pcm_bytes\":%" PRIu64
               ",\"display_bytes\":%" PRIu64 ",\"blocks\":%" PRIu64 ",\"first_frame\":%" PRIu64
               ",\"end_frame\":%" PRIu64 ",\"iters\":%" PRIu64 ",\"cycles\":%" PRIu64
               ",\"pc\":%" PRIu64 ",\"native\":",
               state.script.pcm_bytes, state.script.display_bytes, state.script.blocks,
               state.script.first_frame, state.script.end_frame, state.main_iters, state.cycles, state.pc);
        if (m8_host_native_stats_json(stdout, &state.native_engine) ||
            fputs(",\"native_boot\":",stdout)==EOF ||
            m8_host_native_boot_stats_json(stdout,&state.native_boot) || puts("}")==EOF) {
            snprintf(error, sizeof error, "Cannot write diagnostic completion"); goto close;
        }
        result = 0;
        goto close;
    }
    while (!stop_requested) {
        m8_worker_snapshot(worker, &state);
        if (state.done || (state.ready && state.boot_done)) break;
        if (drain_display(worker, rendered)) goto close;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) stop_requested = 1;
            m8_handheld_event(&ctx, &event);
            if (ctx.app_state == QUIT) { stop_requested = 1; break; }
        }
        uint64_t now = SDL_GetTicksNS();
        if (rendered && now-last_render >= UINT64_C(1000000000)/o.fps) {
            render_screen(&ctx.conf); last_render = now;
        }
        SDL_Delay(2);
    }
    if (!state.ready || state.failed || stop_requested) {
        snprintf(error, sizeof error, "%s", state.failed ? state.error : "Boot interrupted"); goto close;
    }
    if (record) {
        uint64_t ticks = SDL_GetTicksNS(), clock = m8_monotonic_ns();
        fprintf(record, "{\"event\":\"ready\",\"monotonic_ns\":%" PRIu64
            ",\"audio_start_ns\":%" PRIu64 ",\"pcm_first_frame\":%" PRIu64
            ",\"prefill_frames\":%" PRIu64 ",\"held\":%u}\n",
            clock, clock - ticks + s.started, s.output_origin,
            s.produced_base - s.output_origin, state.held);
        fflush(record);
    }
    if (o.title_stats) {
        char title[160];
        snprintf(title, sizeof title, "M8 / 0.0x realtime / %.0fMB RAM", m8_current_rss_mib() * 1.048576);
        SDL_SetWindowTitle(stats_window, title);
    }
    record_ns = m8_monotonic_ns(); record_frames = state.frames; record_cpu = m8_process_seconds();
    title.samples[0] = (title_sample){state.frames, state.cpu_seconds};
    title.count = 1;
    m8_bridge_audio(s.audio);
    printf("boot_done=%" PRIu64 " sd_revision=%" PRIu64 " held=%u\n",
           state.boot_done, state.sd_revision, state.held);
    printf("native_boot=%d native_boot_warm=%d native_boot_seconds=%.6f native_boot_main_iters=%" PRIu64
        " native_boot_cycles=%" PRIu64 " native_boot_pc=0x%08x\n",state.native_boot.selected,state.native_boot.warm,
        state.native_boot.boot_elapsed_ns/1e9,state.native_boot.boot_main_iters,state.native_boot.boot_cycles,state.native_boot.boot_pc);
    if (o.smoke && !state.have_cursor) { snprintf(error, sizeof error, "No initial Phrase cursor"); goto close; }
    initial_x = state.cursor_x; initial_y = state.cursor_y;
    double cpu_base = m8_process_seconds(), main_base = m8_thread_seconds();
    unsigned latency_phase = 0;
    int stalled = 0;
    last_status = s.started;
    printf("M8 Native 6.5.2C threaded local / %s; firmware=%s card=%s\n", m8live_driver(s.audio),
           o.firmware, card ? m8_card_path(card) : o.demo ? "temporary demo" : o.image);
    fflush(stdout);
    renders_base = m8_render_present_count();
    status(&s, &state, cpu_base, main_base, -1, 0);
    while (ctx.app_state != QUIT && !stop_requested) {
        uint64_t now = SDL_GetTicksNS();
        double elapsed = (now - s.started)/1e9;
        m8_worker_snapshot(worker, &state);
        if (state.failed || state.done) { snprintf(error, sizeof error, "%s", state.error); goto close; }
        if (o.seconds > 0 && elapsed >= o.seconds) break;
        if (o.render_stall_ms && elapsed > .3 && !stalled) {
            SDL_Delay(o.render_stall_ms); stalled = 1; continue;
        }
        if (elapsed > 1 && o.smoke && !smoke_down) {
            SDL_Event event = {0}; event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = SDL_SCANCODE_DOWN; event.key.key = SDLK_DOWN;
            if (!SDL_PushEvent(&event)) { snprintf(error, sizeof error, "Cannot inject DOWN"); goto close; }
            smoke_down = 1; smoke_down_ns = now;
        }
        if (smoke_down && now-smoke_down_ns > UINT64_C(80000000) && o.smoke && !smoke_up) {
            SDL_Event event = {0}; event.type = SDL_EVENT_KEY_UP;
            event.key.scancode = SDL_SCANCODE_DOWN; event.key.key = SDLK_DOWN;
            if (!SDL_PushEvent(&event)) { snprintf(error, sizeof error, "Cannot release DOWN"); goto close; }
            smoke_up = 1;
        }
        if (l && elapsed > .2 && m8live_get(s.audio, 2) >= 4410 && !latency_phase) {
            if (m8live_get(s.audio, 15) || m8live_get(s.audio, 13)) {
                snprintf(error, sizeof error, "Stopped demo was not silent before note"); goto close;
            }
            if (m8_worker_control(worker, 1, SDL_GetTicksNS()) < 0) goto close;
            latency_phase = 1;
        } else if (l && elapsed > .65 && latency_phase == 1) {
            if (m8_worker_control(worker, 2, 0) < 0) goto close;
            latency_phase = 2;
        } else if (l && elapsed > .9 && latency_phase == 2) {
            if (m8_worker_control(worker, 3, 0) < 0) goto close;
            latency_phase = 3;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) m8_send_msg_controller(0);
            if (m8_handheld_event(&ctx, &event)) {
                if (ctx.app_state == QUIT) break;
                continue;
            }
            SDL_AppResult ev = SDL_AppEvent(&ctx, &event);
            if (ev != SDL_APP_CONTINUE) ctx.app_state = QUIT;
        }
        if (m8_process_data(&ctx.conf) == DEVICE_FATAL_ERROR) { snprintf(error, sizeof error, "Host input queue full"); goto close; }
        if (drain_display(worker, rendered)) { snprintf(error, sizeof error, "Display decode failed"); goto close; }
        if (rendered && now-last_render >= UINT64_C(1000000000)/o.fps) {
            render_screen(&ctx.conf); last_render = now;
            renders = m8_render_present_count() - renders_base;
        }
        if (o.smoke && smoke_up && now-smoke_down_ns > UINT64_C(300000000) && !smoke_checked) {
            unsigned x = state.cursor_x, y = state.cursor_y;
            const char *first = !state.have_cursor ? "cursor_present" :
                !state.view_read ? "view_read" : !state.row_read ? "row_read" :
                memcmp(state.view, (unsigned char[]){0xe4, 0x4a, 0x01, 0x20}, 4) ? "phrase_view" :
                state.row != 1 ? "selected_row" : x != initial_x ? "cursor_x" :
                y != initial_y+10 ? "cursor_y" : m8_bridge_held() ? "released_keys" : NULL;
            if (first) {
                snprintf(error, sizeof error, "Input smoke failed: first=%s view_read=%u view=%02x%02x%02x%02x "
                         "row_read=%u row=%u cursor=(%u,%u) expected=(%u,%u) held=%u",
                         first, state.view_read, state.view[3], state.view[2], state.view[1], state.view[0],
                         state.row_read, state.row, x, y, initial_x, initial_y+10, m8_bridge_held()); goto close;
            }
            printf("client_smoke=PASS cursor=(%u,%u)->(%u,%u) row=%u held=%u rendered_frames=%" PRIu64 "\n",
                   initial_x, initial_y, x, y, state.row, m8_bridge_held(), renders);
            smoke_checked = 1;
        }
        if ((o.title_stats || record) && m8_monotonic_ns()-record_ns >= UINT64_C(500000000)) {
            if (record_stats(record, o.title_stats ? stats_window : NULL, &s, &state,
                             &record_ns, &record_frames, &record_cpu, &title)) {
                snprintf(error, sizeof error, "Recording log write failed: error=%d", errno); goto close;
            }
        }
        if (now-last_status >= UINT64_C(1000000000)) { status(&s, &state, cpu_base, main_base, -1, renders); last_status = now; }
        SDL_DelayNS((uint64_t)o.poll_ms*1000000);
    }
    double main_end = m8_thread_seconds();
    m8_worker_stop(worker); m8_worker_join(worker); m8_worker_snapshot(worker, &state);
    if (state.failed) { snprintf(error, sizeof error, "%s", state.error); goto close; }
    status(&s, &state, cpu_base, main_base, main_end, renders);
    if (record && record_stats(record, NULL, &s, &state, &record_ns, &record_frames, &record_cpu, &title)) {
        snprintf(error, sizeof error, "Recording log write failed: error=%d", errno); goto close;
    }
    if (capture.file) {
        if (fflush(capture.file)) {
            snprintf(error, sizeof error, "PCM tap failed: first=flush error=%d", errno);
            goto close;
        }
        printf("pcm_tap=%s bytes=%" PRIu64 " first_frame=%" PRIu64 " end_frame=%" PRIu64
               " first_cycles=%" PRIu64 " last_block_cycles=%" PRIu64 " end_cycles=%" PRIu64
               " contiguous=1 prefill_frames=%" PRIu64 " live_frames=%" PRIu64
               " end_iters=%" PRIu64 " end_pc=0x%08" PRIx64 "\n",
               o.pcm, capture.bytes, capture.first_frame, capture.end_frame, capture.first_cycles,
               capture.last_cycles, state.cycles, s.produced_base-s.output_origin,
               state.frames-s.produced_base, state.main_iters, state.pc);
    }
    uint64_t underruns = m8live_get(s.audio, 4), missing = m8live_get(s.audio, 5);
    uint64_t rejected = m8live_get(s.audio, 8), callback_errors = m8live_get(s.audio, 9);
    if (underruns || missing || rejected || callback_errors) {
        const char *first = underruns ? "underruns" : missing ? "missing_frames" :
                            rejected ? "rejected_frames" : "callback_errors";
        snprintf(delivery_error, sizeof delivery_error,
                 "Audio continuity failed: first=%s underruns=%" PRIu64 " missing=%" PRIu64
                 " rejected=%" PRIu64 " callback_errors=%" PRIu64,
                 first, underruns, missing, rejected, callback_errors);
    }
    double elapsed = (state.end_ns - s.started) / 1e9;
    double consumer_seconds = state.consumer_started ? (state.end_ns-state.consumer_started)/1e9 : 0;
    double delivered = consumer_seconds > 0 ?
        (m8live_get(s.audio, 3)-state.consumer_base)/44100.0/consumer_seconds : 0;
    if (!*delivery_error && elapsed >= 2 && (delivered < .95 || delivered > 1.05)) {
        snprintf(delivery_error, sizeof delivery_error,
                 "Consumer clock differs from real time: steady_delivery=%.6f consumer_seconds=%.6f", delivered, consumer_seconds);
    }
    if (o.smoke && (!smoke_checked || !renders)) { snprintf(error, sizeof error, "Input smoke did not complete"); goto close; }
    if ((o.project || (o.headless && o.demo)) && !o.no_play && !o.latency) {
        uint64_t nonzero = m8live_get(s.audio, 15);
        int sound_failed = o.expect_silence ? nonzero != 0 : nonzero < 128;
        if (sound_failed || !state.rows) {
            snprintf(error, sizeof error, "Demo playback failed: first=%s nonzero=%" PRIu64 " rows=0x%04x",
                     sound_failed ? (o.expect_silence ? "silent_frames" : "nonzero_frames") : "playhead_rows",
                     nonzero, state.rows); goto close;
        }
    }
    if (l && latency_result(&s, l)) { snprintf(error, sizeof error, "Latency note or release check failed"); goto close; }
    if (card && !m8_card_read_only(card)) {
        if (m8_card_save(card, s.rt)) { snprintf(error, sizeof error, "%s", m8_card_error(card)); goto close; }
        printf("card_saved=%s backup=%s.bak\n", m8_card_path(card), m8_card_path(card));
    } else if (card) printf("card_read_only=%s saved=0\n", m8_card_path(card));
    /* A delivery diagnostic must not discard successfully written card files.
     * Storage still has its independent runtime/export/publication guards. */
    if (*delivery_error) snprintf(error, sizeof error, "%s", delivery_error);
    else result = 0;
close:
    m8_worker_stop(worker); m8_worker_join(worker);
    if (profile.output && fclose(profile.output)) {
        snprintf(error, sizeof error, "Guest profile close failed: error=%d", errno);
        result = 1;
    }
    if (result) fprintf(stderr, "first_failure=%s\n", *error ? error : *s.error ? s.error : "Native application failed");
    m8_worker_destroy(worker);
    if (capture.file && fclose(capture.file)) {
        fprintf(stderr, "first_failure=PCM tap close failed: error=%d\n", errno);
        result = 1;
    }
    m8live_destroy(s.audio);
    m8_demo_destroy(s.demo);
    m8rt_destroy(s.rt);
    m8_host_aot_close(NULL, aot);
    m8_host_native_close(native);
    m8_host_native_boot_close(native_boot);
    m8_card_close(card);
    free(l);
    if (gamepads) gamepads_close();
    if (rendered) { renderer_close(); inline_font_close(); }
    if (record && fclose(record)) result = 1;
    SDL_Quit();
    return result;
}
