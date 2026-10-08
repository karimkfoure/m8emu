#include "session.h"
#include "bridge.h"
#include <SDL3/SDL.h>
#include <stdio.h>
static int fail(m8_session *s, const char *message) {
    if (!s->error[0]) snprintf(s->error, sizeof s->error, "%s", message);
    return -1;
}
int m8_session_pump(void *context) {
    m8_session *s = context;
    unsigned char bytes[4096];
    size_t n;
    /* Publish ready audio before display parsing, including full redraws. */
    m8rt_audio_position position;
    while ((n = m8rt_read_local_pcm(s->rt, bytes, sizeof bytes, &position))) {
        if (s->delivering) {
            if (s->observe) s->observe(s->observe_context, bytes, n, &position);
            if (s->error[0]) return -1;
            if (m8live_write(s->audio, bytes, n / 4) < 0)
                return fail(s, "PCM block exceeds available ring space");
        }
    }
    /* Keep the exact USB source active and bounded, but use the local tap. */
    while (m8rt_read_pcm(s->rt, bytes, sizeof bytes)) {}
    size_t display_bytes = 0;
    while ((!s->delivering || display_bytes < sizeof bytes) &&
           (n = m8rt_read_cdc(s->rt, bytes, sizeof bytes))) {
        if (s->display ? s->display(s->transport, bytes, n) < 0 :
            m8_bridge_feed(bytes, n) < 0) return fail(s, "Display transport failed");
        if (m8_demo_feed_cdc(s->demo, bytes, n) < 0) return fail(s, m8_demo_error(s->demo));
        display_bytes += n;
    }
    int row = m8_demo_playhead(s->demo);
    if (row >= 0 && row < 16) s->playhead_rows |= 1u << row;
    return 0;
}
int m8_session_fill(m8_session *s, int prefill) {
    do {
        if (s->input && s->input(s->transport) < 0) return fail(s, "Input queue failed");
        uint64_t queued = m8live_get(s->audio, 0);
        if (queued >= s->high) return 0;
        uint64_t before = m8rt_get(s->rt, M8RT_LOCAL_PCM_BYTES) / 4;
        uint64_t begin = SDL_GetTicksNS();
        int result = m8rt_run_audio_frames(s->rt, before + s->high - queued, s->slice_us);
        if (result < 0) return fail(s, m8rt_error(s->rt));
        if (result > 0) return fail(s, "Guest stopped before close");
        if (m8_session_pump(s) < 0) return -1;
        uint64_t end = SDL_GetTicksNS();
        if (!prefill) {
            s->service_seconds += (end - begin) / 1e9;
            ++s->service_calls;
        }
        uint64_t after = m8rt_get(s->rt, M8RT_LOCAL_PCM_BYTES) / 4;
        if (after > s->last_frames) { s->last_frames = after; s->progress = end; }
        if (end - s->progress > UINT64_C(5000000000))
            return fail(s, "Guest produced no audio for five seconds");
    } while (prefill);
    return 0;
}
int m8_session_start(m8_session *s, uint32_t capacity, uint32_t slice_us, int simulated) {
    s->capacity = capacity; s->high = capacity - 64;
    s->low = s->high > 192 ? s->high - 128 : 64;
    s->slice_us = slice_us;
    s->audio = m8live_create(capacity, !simulated);
    if (!s->audio || *m8live_error(s->audio)) return fail(s, m8live_error(s->audio));
    if (s->onset_threshold) m8live_mark_onset(s->audio, s->onset_threshold);
    s->delivering = 1; s->progress = SDL_GetTicksNS();
    s->output_origin = m8rt_get(s->rt, M8RT_LOCAL_PCM_BYTES) / 4;
    if (m8_session_fill(s, 1) < 0) return -1;
    s->produced_base = s->last_frames;
    s->started = SDL_GetTicksNS();
    if (m8live_start(s->audio) < 0) return fail(s, m8live_error(s->audio));
    return 0;
}
int m8_session_step(m8_session *s) {
    if (!s->consumer_started) {
        uint64_t consumed = m8live_get(s->audio, 3);
        if (consumed) {
            s->consumer_base = consumed; s->consumer_started = SDL_GetTicksNS();
        }
    }
    if (s->input && s->input(s->transport) < 0) return fail(s, "Input queue failed");
    if (m8live_get(s->audio, 9)) return fail(s, "SDL audio callback failed");
    if (m8live_get(s->audio, 0) < s->low) return m8_session_fill(s, 0);
    /* Service deferred display bytes while audio already has enough data. */
    if (m8_session_pump(s) < 0) return -1;
    SDL_DelayNS(500000);
    return 0;
}
