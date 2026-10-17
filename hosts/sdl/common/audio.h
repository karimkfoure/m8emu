#ifndef M8LIVE_AUDIO_H
#define M8LIVE_AUDIO_H
#include <stddef.h>
#include <stdint.h>
typedef struct m8live_audio m8live_audio;
/* One producer, one consumer. The callback never executes the guest. */
m8live_audio *m8live_create(uint32_t capacity_frames, int sdl);
/* Default recording device. SDL converts to stereo S16LE at 44100 Hz.
 * The callback fills a bounded ring and counts dropped frames if it is full. */
m8live_audio *m8live_input_create(uint32_t capacity_frames);
/* Host execution thread drains complete captured frames, without padding. */
size_t m8live_input_read(m8live_audio *a, void *pcm, size_t frames);
const char *m8live_error(m8live_audio *a);
int m8live_start(m8live_audio *a);
void m8live_pause(m8live_audio *a);
void m8live_destroy(m8live_audio *a);
/* Whole-payload admission. A full ring fails; it never discards samples. */
int m8live_write(m8live_audio *a, const void *pcm, size_t frames);
/* Simulated device: NULL output is permitted. Output includes shortage silence. */
void m8live_consume(m8live_audio *a, void *out, size_t frames);
/* 0 queued; 1 produced; 2 consumed incl silence; 3 actual PCM consumed;
 * 4 shortage events; 5 shortage frames; 6 maximum occupancy; 7 minimum
 * occupancy after consumption; 8 rejected frames; 9 callback errors;
 * 10 device frames (at device rate); 11 device rate; 12 maximum callback
 * request; 13 first marked onset output frame + 1; 14 onset monotonic ns;
 * 15 nonzero admitted frames; 16 peak; 17 sum of squared samples; 18 samples;
 * 19 device output delay after the callback in microseconds, 0 if unknown. */
uint64_t m8live_get(m8live_audio *a, unsigned field);
const char *m8live_driver(m8live_audio *a);
void m8live_mark_onset(m8live_audio *a, unsigned threshold);
/* Muting keeps device consumption and guest pacing active. */
void m8live_mute(m8live_audio *a, int muted);
#endif
