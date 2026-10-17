#ifndef M8LIVE_AUDIO_COREAUDIO_H
#define M8LIVE_AUDIO_COREAUDIO_H
#include <stddef.h>
#include <stdint.h>
typedef struct m8_coreaudio m8_coreaudio;
/* Fills complete stereo S16LE frames at 44100 Hz on the real-time I/O thread. */
typedef void (*m8_coreaudio_render)(void *context, void *pcm, size_t frames);
/* period is the requested I/O size in 44100 Hz frames; 0 keeps the device's. */
m8_coreaudio *m8_coreaudio_open(m8_coreaudio_render render, void *context,
                                uint32_t period, char *error, size_t capacity);
int m8_coreaudio_start(m8_coreaudio *c, char *error, size_t capacity);
void m8_coreaudio_stop(m8_coreaudio *c);
void m8_coreaudio_close(m8_coreaudio *c);
/* Current device period and rate, and the output delay after a callback. */
void m8_coreaudio_format(m8_coreaudio *c, uint32_t *frames, uint32_t *rate, double *latency_ms);
#endif
