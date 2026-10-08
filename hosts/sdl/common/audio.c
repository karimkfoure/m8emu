#include "audio.h"
#include "audio_clock.h"
#include <SDL3/SDL.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct m8live_audio {
    uint8_t *ring;
    uint32_t capacity, device_frames, device_rate;
    _Atomic uint64_t written, read, consumed, xruns, missing, maximum, minimum;
    _Atomic uint64_t rejected, errors, max_request, onset_frame, onset_ns;
    _Atomic uint64_t nonzero, peak, sum_squared, sample_count;
    _Atomic unsigned threshold;
    _Atomic int muted;
    int shortage;
    int sdl_initialized, simulation_running;
    _Atomic int simulation_stop;
    SDL_Thread *simulation_thread;
    SDL_AudioStream *stream;
    char error[256], driver[64];
};

static uint64_t monotonic_ns(void) {
    return m8_audio_now_ns();
}

static void maximum(_Atomic uint64_t *value, uint64_t candidate) {
    uint64_t old = atomic_load_explicit(value, memory_order_relaxed);
    while (candidate > old && !atomic_compare_exchange_weak_explicit(
            value, &old, candidate, memory_order_relaxed, memory_order_relaxed)) {}
}

void m8live_consume(m8live_audio *a, void *output, size_t frames) {
    uint64_t rd = atomic_load_explicit(&a->read, memory_order_relaxed);
    uint64_t wr = atomic_load_explicit(&a->written, memory_order_acquire);
    uint64_t position = atomic_load_explicit(&a->consumed, memory_order_relaxed);
    size_t take = wr - rd < frames ? (size_t)(wr - rd) : frames;
    size_t offset = (size_t)(rd % a->capacity), first = a->capacity - offset;
    if (first > take) first = take;
    if (output) {
        memcpy(output, a->ring + offset * 4, first * 4);
        memcpy((uint8_t *)output + first * 4, a->ring, (take - first) * 4);
        memset((uint8_t *)output + take * 4, 0, (frames - take) * 4);
    }
    unsigned threshold = atomic_load_explicit(&a->threshold, memory_order_relaxed);
    if (threshold && !atomic_load_explicit(&a->onset_frame, memory_order_relaxed)) {
        for (size_t i = 0; i < take; i++) {
            const uint8_t *p = a->ring + ((rd + i) % a->capacity) * 4;
            int l = (int16_t)(p[0] | p[1] << 8), r = (int16_t)(p[2] | p[3] << 8);
            if ((unsigned)abs(l) >= threshold || (unsigned)abs(r) >= threshold) {
                atomic_store_explicit(&a->onset_ns, monotonic_ns(), memory_order_relaxed);
                atomic_store_explicit(&a->onset_frame, position + i + 1, memory_order_release);
                break;
            }
        }
    }
    atomic_store_explicit(&a->read, rd + take, memory_order_release);
    atomic_fetch_add_explicit(&a->consumed, frames, memory_order_relaxed);
    maximum(&a->max_request, frames);
    uint64_t minimum = atomic_load_explicit(&a->minimum, memory_order_relaxed);
    if (wr - rd - take < minimum)
        atomic_store_explicit(&a->minimum, wr - rd - take, memory_order_relaxed);
    if (take < frames) {
        if (!a->shortage) atomic_fetch_add_explicit(&a->xruns, 1, memory_order_relaxed);
        a->shortage = 1;
        atomic_fetch_add_explicit(&a->missing, frames - take, memory_order_relaxed);
    } else a->shortage = 0;
}

static void SDLCALL callback(void *userdata, SDL_AudioStream *stream,
                            int additional, int total) {
    (void)total;
    m8live_audio *a = userdata;
    if (additional <= 0) return;
    /* SDL may round a resampling request up. Each frame is complete stereo. */
    size_t frames = ((size_t)additional + 3) / 4;
    uint8_t scratch[4096 * 4];
    while (frames) {
        size_t n = frames < 4096 ? frames : 4096;
        m8live_consume(a, scratch, n);
        if (atomic_load_explicit(&a->muted, memory_order_relaxed)) memset(scratch, 0, n * 4);
        if (!SDL_PutAudioStreamData(stream, scratch, (int)(n * 4)))
            atomic_fetch_add_explicit(&a->errors, 1, memory_order_relaxed);
        frames -= n;
    }
}

static void SDLCALL input_callback(void *userdata, SDL_AudioStream *stream,
                                  int additional, int total) {
    (void)additional;
    (void)total;
    m8live_audio *a = userdata;
    uint8_t scratch[4096 * 4];
    int available;
    while ((available = SDL_GetAudioStreamAvailable(stream)) >= 4) {
        int count = available < (int)sizeof(scratch) ? available : (int)sizeof(scratch);
        count -= count % 4;
        int received = SDL_GetAudioStreamData(stream, scratch, count);
        if (received < 0 || received % 4) {
            atomic_fetch_add_explicit(&a->errors, 1, memory_order_relaxed);
            return;
        }
        if (!received) return;
        /* Capture cannot pause a physical source. Count rejected frames and
         * keep the already captured prefix when the bounded ring is full. */
        (void)m8live_write(a, scratch, (size_t)received / 4);
    }
    if (available < 0) atomic_fetch_add_explicit(&a->errors, 1, memory_order_relaxed);
}

static int simulate(void *userdata) {
    m8live_audio *a = userdata;
    uint64_t start = monotonic_ns(), consumed = 0;
    while (!atomic_load(&a->simulation_stop)) {
        uint64_t next = start + (consumed + 128) * 1000000000 / 44100;
        uint64_t now = monotonic_ns();
        if (next > now) SDL_DelayNS(next - now);
        if (atomic_load(&a->simulation_stop)) break;
        m8live_consume(a, NULL, 128);
        consumed += 128;
    }
    return 0;
}

m8live_audio *m8live_create(uint32_t capacity, int sdl) {
    if (!capacity || capacity > 44100 * 2) return NULL;
    m8live_audio *a = calloc(1, sizeof(*a));
    if (!a) return NULL;
    a->capacity = capacity;
    a->ring = malloc((size_t)capacity * 4);
    if (!a->ring) { free(a); return NULL; }
    atomic_store(&a->minimum, capacity);
    if (sdl) {
        /* Without a node latency the PipeWire stream inherits the server
         * quantum (for example 1024 frames at 48 kHz), which exceeds the
         * bounded ring and starves the device. 256/48000 is about 5.3 ms;
         * an explicit environment value still wins. */
        SDL_setenv_unsafe("PIPEWIRE_LATENCY", "256/48000", 0);
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) goto error;
        a->sdl_initialized = 1;
        SDL_AudioSpec input = {SDL_AUDIO_S16LE, 2, 44100}, device;
        a->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                              &input, callback, a);
        if (!a->stream) goto error;
        int frames = 0;
        if (!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(a->stream), &device, &frames))
            goto error;
        a->device_frames = (uint32_t)frames;
        a->device_rate = (uint32_t)device.freq;
        const char *driver = SDL_GetCurrentAudioDriver();
        if (driver) SDL_strlcpy(a->driver, driver, sizeof(a->driver));
    } else {
        SDL_strlcpy(a->driver, "simulated", sizeof(a->driver));
        a->device_frames = 128;
        a->device_rate = 44100;
    }
    return a;
error:
    SDL_strlcpy(a->error, SDL_GetError(), sizeof(a->error));
    return a;
}

m8live_audio *m8live_input_create(uint32_t capacity) {
    m8live_audio *a = m8live_create(capacity, 0);
    if (!a) return NULL;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) goto error;
    a->sdl_initialized = 1;
    SDL_AudioSpec format = {SDL_AUDIO_S16LE, 2, 44100}, device;
    a->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING,
                                        &format, input_callback, a);
    if (!a->stream) goto error;
    int frames = 0;
    if (!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(a->stream), &device, &frames))
        goto error;
    a->device_frames = (uint32_t)frames;
    a->device_rate = (uint32_t)device.freq;
    const char *driver = SDL_GetCurrentAudioDriver();
    if (driver) SDL_strlcpy(a->driver, driver, sizeof(a->driver));
    return a;
error:
    SDL_strlcpy(a->error, SDL_GetError(), sizeof(a->error));
    return a;
}

size_t m8live_input_read(m8live_audio *a, void *pcm, size_t frames) {
    uint64_t rd = atomic_load_explicit(&a->read, memory_order_relaxed);
    uint64_t wr = atomic_load_explicit(&a->written, memory_order_acquire);
    size_t take = wr - rd < frames ? (size_t)(wr - rd) : frames;
    size_t offset = (size_t)(rd % a->capacity), first = a->capacity - offset;
    if (first > take) first = take;
    memcpy(pcm, a->ring + offset * 4, first * 4);
    memcpy((uint8_t *)pcm + first * 4, a->ring, (take - first) * 4);
    atomic_store_explicit(&a->read, rd + take, memory_order_release);
    atomic_fetch_add_explicit(&a->consumed, take, memory_order_relaxed);
    return take;
}

const char *m8live_error(m8live_audio *a) { return a ? a->error : "audio allocation failed"; }
const char *m8live_driver(m8live_audio *a) { return a->driver; }
int m8live_start(m8live_audio *a) {
    if (a->error[0]) return -1;
    if (a->stream && !SDL_ResumeAudioStreamDevice(a->stream)) {
        SDL_strlcpy(a->error, SDL_GetError(), sizeof(a->error));
        return -1;
    }
    if (!a->stream && !a->simulation_running) {
        atomic_store(&a->simulation_stop, 0);
        a->simulation_thread = SDL_CreateThread(simulate, "m8-consumer", a);
        if (!a->simulation_thread) {
            SDL_strlcpy(a->error, "cannot start simulated consumer", sizeof(a->error));
            return -1;
        }
        a->simulation_running = 1;
    }
    return 0;
}
void m8live_pause(m8live_audio *a) {
    if (a && a->stream) SDL_PauseAudioStreamDevice(a->stream);
    if (a && a->simulation_running) {
        atomic_store(&a->simulation_stop, 1);
        SDL_WaitThread(a->simulation_thread, NULL);
        a->simulation_thread = NULL;
        a->simulation_running = 0;
    }
}
void m8live_destroy(m8live_audio *a) {
    if (!a) return;
    m8live_pause(a);
    if (a->stream) {
        SDL_DestroyAudioStream(a->stream);
    }
    if (a->sdl_initialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    free(a->ring);
    free(a);
}

int m8live_write(m8live_audio *a, const void *pcm, size_t frames) {
    uint64_t wr = atomic_load_explicit(&a->written, memory_order_relaxed);
    uint64_t rd = atomic_load_explicit(&a->read, memory_order_acquire);
    if (frames > a->capacity - (wr - rd)) {
        atomic_fetch_add_explicit(&a->rejected, frames, memory_order_relaxed);
        return -1;
    }
    size_t offset = (size_t)(wr % a->capacity), first = a->capacity - offset;
    if (first > frames) first = frames;
    memcpy(a->ring + offset * 4, pcm, first * 4);
    memcpy(a->ring, (const uint8_t *)pcm + first * 4, (frames - first) * 4);
    const uint8_t *p = pcm;
    uint64_t nonzero = 0, square = 0, peak = 0;
    for (size_t i = 0; i < frames; i++, p += 4) {
        int l = (int16_t)(p[0] | p[1] << 8), r = (int16_t)(p[2] | p[3] << 8);
        if (l || r) nonzero++;
        if ((uint64_t)abs(l) > peak) peak = (uint64_t)abs(l);
        if ((uint64_t)abs(r) > peak) peak = (uint64_t)abs(r);
        square += (uint64_t)((int64_t)l * l + (int64_t)r * r);
    }
    atomic_fetch_add(&a->nonzero, nonzero);
    atomic_fetch_add(&a->sum_squared, square);
    atomic_fetch_add(&a->sample_count, frames * 2);
    maximum(&a->peak, peak);
    atomic_store_explicit(&a->written, wr + frames, memory_order_release);
    maximum(&a->maximum, wr + frames - rd);
    return 0;
}

void m8live_mark_onset(m8live_audio *a, unsigned threshold) {
    /* Call while the consumer is paused, or before it is started. */
    atomic_store(&a->onset_frame, 0);
    atomic_store(&a->onset_ns, 0);
    atomic_store(&a->threshold, threshold);
}

void m8live_mute(m8live_audio *a, int muted) {
    if (a) atomic_store_explicit(&a->muted, muted != 0, memory_order_relaxed);
}

uint64_t m8live_get(m8live_audio *a, unsigned field) {
    switch (field) {
    case 0: {
        /* Read the consumer first, so concurrent consumption cannot underflow. */
        uint64_t rd = atomic_load_explicit(&a->read, memory_order_acquire);
        return atomic_load_explicit(&a->written, memory_order_acquire) - rd;
    }
    case 1: return atomic_load(&a->written);
    case 2: return atomic_load(&a->consumed);
    case 3: return atomic_load(&a->read);
    case 4: return atomic_load(&a->xruns);
    case 5: return atomic_load(&a->missing);
    case 6: return atomic_load(&a->maximum);
    case 7: return atomic_load(&a->minimum);
    case 8: return atomic_load(&a->rejected);
    case 9: return atomic_load(&a->errors);
    case 10:
    case 11: {
        /* A backend can renegotiate its period after stream creation. Report
         * the current device format rather than the initial hint. */
        SDL_AudioSpec device;
        int frames = 0;
        if (a->stream && SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(a->stream), &device, &frames))
            return field == 10 ? (uint64_t)frames : (uint64_t)device.freq;
        return field == 10 ? a->device_frames : a->device_rate;
    }
    case 12: return atomic_load(&a->max_request);
    case 13: return atomic_load_explicit(&a->onset_frame, memory_order_acquire);
    case 14: return atomic_load(&a->onset_ns);
    case 15: return atomic_load(&a->nonzero);
    case 16: return atomic_load(&a->peak);
    case 17: return atomic_load(&a->sum_squared);
    case 18: return atomic_load(&a->sample_count);
    default: return 0;
    }
}
