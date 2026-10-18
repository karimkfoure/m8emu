#ifndef M8_HOST_SESSION_H
#define M8_HOST_SESSION_H
#include "audio.h"
#include "demo.h"
#include "m8rt.h"
struct m8_checkpoint;
struct m8_card;
typedef m8rt *(*m8_session_factory)(void *,char *,size_t);
typedef enum {
    M8_SESSION_OK=0,M8_SESSION_BUSY,M8_SESSION_UNSUPPORTED,M8_SESSION_INVALID,
    M8_SESSION_INCOMPATIBLE,M8_SESSION_NOMEM,M8_SESSION_GUEST_FAILED,M8_SESSION_STORAGE
} m8_session_result;
typedef enum {M8_SESSION_LIVE=0,M8_SESSION_PREFILL,M8_SESSION_PREPARE} m8_session_fill_mode;
typedef struct m8_session {
    struct m8_checkpoint *checkpoint;
    unsigned char *initial_pcm;
    size_t initial_pcm_size;
    int resume_paused;
    int explorer_paused,explorer_audio_running;
    uint64_t pause_started,pause_total;
    m8rt *rt;
    m8_demo *demo;
    m8live_audio *audio;
    uint32_t capacity, high, low, slice_us;
    uint64_t produced_base, output_origin, started, progress, last_frames;
    uint64_t consumer_started, consumer_base;
    double service_seconds;
    unsigned service_calls;
    uint32_t playhead_rows;
    int delivering;
    unsigned onset_threshold;
    /* Called on complete live PCM blocks, for optional measurements. */
    void (*observe)(void *, const unsigned char *, size_t, const m8rt_audio_position *);
    void *observe_context;
    /* Optional worker-owned transport. No renderer runs from these callbacks. */
    int (*display)(void *, const void *, size_t);
    int (*input)(void *);
    void *transport;
    char error[512];
} m8_session;
int m8_session_pump(void *context);
/* PREPARE uses bounded 256-frame steps without a host deadline while paused. */
int m8_session_fill(m8_session *s,m8_session_fill_mode);
int m8_session_start(m8_session *s, uint32_t capacity, uint32_t slice_us, int simulated);
int m8_session_step(m8_session *s);
/* Execution owner only, between normal steps. A busy refusal resumes audio
 * without advancing the guest. Release user controls through ordered input
 * before requesting this boundary. The caller owns returned media bytes. */
m8_session_result m8_session_pause(m8_session *,m8rt_session_blob *,m8rt_session_error *,char *,size_t);
m8_session_result m8_session_resume(m8_session *,char *,size_t);
m8_session_result m8_session_replace(m8_session *,struct m8_card *,m8_session_factory,void *,
    const void *,size_t,int reset,char *,size_t);
#endif
