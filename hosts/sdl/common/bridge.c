#include "bridge.h"
#include "backends/m8.h"
#include "backends/audio.h"
#include "backends/slip.h"
#include "command.h"
#include <SDL3/SDL.h>
#include <string.h>

static m8rt *runtime;
static m8live_audio *audio;
static slip_handler_s slip;
static uint8_t packet[1024];
static int render_enabled, failed, muted;
static unsigned held;
static uint64_t packets;
static int (*transport_send)(void *, const void *, size_t);
static void *transport_context;

static int receive(uint8_t *data, uint32_t size) {
    if (!size) return 1;
    ++packets;
    return !render_enabled || process_command(data, size);
}
void m8_bridge_attach(m8rt *rt, int render) {
    static const slip_descriptor_s descriptor = {packet, sizeof packet, receive};
    runtime = rt; render_enabled = render; failed = 0; held = 0; packets = 0;
    transport_send = NULL; transport_context = NULL;
    slip_init(&slip, &descriptor);
}
void m8_bridge_transport(int (*fn)(void *, const void *, size_t), void *context) {
    transport_send = fn; transport_context = context;
}
void m8_bridge_reset_decoder(void) {
    slip_init(&slip, slip.descriptor);
    /* The producer resumes at a complete packet boundary. Do not discard
     * the first new packet: it can contain the redraw's font identity. */
}
void m8_bridge_audio(m8live_audio *a) { audio = a; muted = 0; }
int m8_bridge_feed(const void *bytes, size_t n) {
    const uint8_t *p = bytes;
    for (size_t i = 0; i < n; ++i) {
        if (slip_read_byte(&slip, p[i]) != SLIP_NO_ERROR) {
            failed = 1;
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid in-process display packet");
            return -1;
        }
    }
    return 0;
}
static int send(const void *data, size_t n) {
    if (transport_send ? transport_send(transport_context, data, n) < 0 :
        !runtime || m8rt_queue_host_bytes(runtime, data, n) < 0) {
        failed = 1; return -1;
    }
    return 1;
}
uint64_t m8_bridge_packets(void) { return packets; }
unsigned m8_bridge_held(void) { return held; }
int m8_initialize(int verbose, const char *preferred) {
    (void)verbose; (void)preferred; return runtime != NULL;
}
int m8_list_devices(void) { return 0; }
int m8_reset_display(void) { return send("R", 1); }
int m8_enable_display(unsigned char reset) { return send(reset ? "ER" : "E", reset ? 2 : 1); }
int m8_send_msg_controller(unsigned char input) {
    uint8_t command[2] = {'C', input};
    int result = send(command, sizeof command);
    if (result > 0) held = input;
    return result;
}
int m8_send_msg_keyjazz(unsigned char note, unsigned char velocity) {
    uint8_t command[3] = {'K', note, velocity > 127 ? 127 : velocity};
    return send(command, note == 255 && velocity == 0 ? 2 : 3);
}
int m8_process_data(const config_params_s *conf) {
    (void)conf; return failed ? DEVICE_FATAL_ERROR : DEVICE_PROCESSING;
}
int m8_pause_processing(void) { return m8_send_msg_controller(0); }
int m8_resume_processing(void) { return m8_reset_display(); }
int m8_close(void) { return send("D", 1); }
int audio_initialize(const char *device, unsigned int size) {
    (void)device; (void)size; muted = 0; m8live_mute(audio, 0); return audio != NULL;
}
void audio_toggle(const char *device, unsigned int size) {
    (void)device; (void)size; muted = !muted; m8live_mute(audio, muted);
}
void audio_process(void) {}
void audio_close(void) { muted = 1; m8live_mute(audio, 1); }
