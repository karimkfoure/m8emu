#ifndef M8_HOST_SESSION_H
#define M8_HOST_SESSION_H
#include "audio.h"
#include "demo.h"
#include "m8rt.h"
typedef struct m8_session {
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
int m8_session_fill(m8_session *s, int prefill);
int m8_session_start(m8_session *s, uint32_t capacity, uint32_t slice_us, int simulated);
int m8_session_step(m8_session *s);
#endif
