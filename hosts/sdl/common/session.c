#include "session.h"
#include "bridge.h"
#include "checkpoint.h"
#include <stdlib.h>
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
        m8_checkpoint_pcm(s,bytes,n);
        if (s->delivering) {
            if (s->observe) s->observe(s->observe_context, bytes, n, &position);
            if (s->error[0]) return -1;
            if (m8live_write(s->audio, bytes, n / 4) < 0)
                return fail(s, "PCM block exceeds available ring space");
        }
    }
    /* This host does not expose the optional IRQ trace. Keep observation
     * bounded during long playback without changing pending interrupts. */
    uint32_t irqs[256];while(m8rt_read_irqs(s->rt,irqs,256)){}
    /* Keep the exact USB source active and bounded, but use the local tap. */
    while (m8rt_read_pcm(s->rt, bytes, sizeof bytes)) {}
    size_t display_bytes = 0;
    while ((!s->delivering || display_bytes < sizeof bytes) &&
           (n = m8rt_read_cdc(s->rt, bytes, sizeof bytes))) {
        if (m8_checkpoint_feed(s,bytes,n)<0) return fail(s,"Session display decoder failed");
        if (s->display ? s->display(s->transport, bytes, n) < 0 :
            m8_bridge_feed(bytes, n) < 0) return fail(s, "Display transport failed");
        if (m8_demo_feed_cdc(s->demo, bytes, n) < 0) return fail(s, m8_demo_error(s->demo));
        display_bytes += n;
    }
    int row = m8_demo_playhead(s->demo);
    if (row >= 0 && row < 16) s->playhead_rows |= 1u << row;
    return 0;
}
int m8_session_fill(m8_session *s,m8_session_fill_mode prefill) {
    do {
        if (s->input && s->input(s->transport) < 0) return fail(s, "Input queue failed");
        uint64_t queued = m8live_get(s->audio, 0);
        if (queued >= s->high) return 0;
        uint64_t before = m8rt_get(s->rt, M8RT_LOCAL_PCM_BYTES) / 4;
        uint64_t begin = SDL_GetTicksNS();
        uint64_t frames=s->high-queued;
        if(prefill==M8_SESSION_PREPARE&&frames>256)frames=256;
        int result = m8rt_run_audio_frames(s->rt,before+frames,prefill==M8_SESSION_PREPARE?0:s->slice_us);
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
    uint32_t allocation=capacity;
    if(s->capacity<allocation)s->capacity=allocation;
    s->high = capacity - 64;
    s->low = s->high > 192 ? s->high - 128 : 64;
    s->slice_us = slice_us;
    s->audio = m8live_create(s->capacity, !simulated);
    if (!s->audio || *m8live_error(s->audio)) return fail(s, m8live_error(s->audio));
    uint64_t retained_frames=s->initial_pcm_size/4;
    if(s->initial_pcm_size && m8live_write(s->audio,s->initial_pcm,s->initial_pcm_size/4)<0)
        return fail(s,"Retained session audio exceeds output capacity");
    free(s->initial_pcm);s->initial_pcm=NULL;s->initial_pcm_size=0;
    if (s->onset_threshold) m8live_mark_onset(s->audio, s->onset_threshold);
    s->delivering = 1; s->progress = SDL_GetTicksNS();
    s->output_origin = m8rt_get(s->rt, M8RT_LOCAL_PCM_BYTES) / 4-retained_frames;
    s->last_frames=m8rt_get(s->rt,M8RT_LOCAL_PCM_BYTES)/4;
    if (!s->resume_paused && m8_session_fill(s, 1) < 0) return -1;
    s->produced_base = s->last_frames;
    s->started = SDL_GetTicksNS();
    if (!s->resume_paused && m8live_start(s->audio) < 0) return fail(s, m8live_error(s->audio));
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

m8_session_result m8_session_resume(m8_session *s,char *error,size_t capacity) {
    if(!s->explorer_paused)return M8_SESSION_INVALID;
    if(s->explorer_audio_running&&m8live_start(s->audio)<0) {
        snprintf(error,capacity,"Cannot resume audio: %s",m8live_error(s->audio));return M8_SESSION_STORAGE;
    }
    uint64_t now=SDL_GetTicksNS();
    if(s->pause_started&&s->consumer_started)s->pause_total+=now-s->pause_started;
    s->pause_started=0;s->progress=now;s->explorer_paused=0;return M8_SESSION_OK;
}
m8_session_result m8_session_pause(m8_session *s,m8rt_session_blob *media,m8rt_session_error *cause,char *error,size_t capacity) {
    *media=(m8rt_session_blob){0};
    if(s->explorer_paused)return M8_SESSION_INVALID;
    s->explorer_audio_running=m8live_running(s->audio);
    uint64_t begin=SDL_GetTicksNS();m8live_pause(s->audio);
    s->pause_started=s->explorer_audio_running?begin:0;s->explorer_paused=1;
    m8_session_result result=(m8_session_result)m8rt_media_capture(s->rt,media,cause);
    if(!result)result=(m8_session_result)m8_checkpoint_save(s,2);
    if(!result) {
        m8_checkpoint_wait(s);
        if(s->checkpoint&&*s->checkpoint->error) {
            snprintf(error,capacity,"%s",s->checkpoint->error);result=M8_SESSION_STORAGE;
        }
    }
    if(result) {
        if(result!=M8_SESSION_STORAGE)snprintf(error,capacity,"Explorer capture refused (%d): %s",result,
            cause&&cause->predicate?cause->predicate:s->checkpoint?s->checkpoint->error:"capture");
        m8rt_session_release(media);
        m8_session_result resumed=m8_session_resume(s,error,capacity);
        return resumed?resumed:(m8_session_result)result;
    }
    return M8_SESSION_OK;
}
