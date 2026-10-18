#ifndef M8_WEB_DISPLAY_H
#define M8_WEB_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

#define M8_DISPLAY_MAX_WIDTH 480u
#define M8_DISPLAY_MAX_HEIGHT 320u
#define M8_DISPLAY_PACKET_BYTES 484u

/* Packed RGBA bytes, with an opaque alpha channel and no row padding. The
 * active framebuffer is width * height pixels at the start of pixels. */
typedef struct {
    uint8_t pixels[M8_DISPLAY_MAX_WIDTH * M8_DISPLAY_MAX_HEIGHT * 4u];
    uint8_t packet[M8_DISPLAY_PACKET_BYTES];
    unsigned width, height, font, packet_size, waveform_size;
    uint32_t rectangle_color, background_color;
    uint64_t dirty_serial, packets, errors;
    unsigned escaped, discard, waveform_cleared;
} M8WebDisplay;

void m8_display_init(M8WebDisplay *display);
/* Preserve SLIP state across arbitrary feed boundaries. Invalid packets or
 * escapes return -1; the parser discards a damaged packet through its END. */
int m8_display_feed(M8WebDisplay *display, const void *bytes, size_t count);
const uint8_t *m8_display_pixels(const M8WebDisplay *display);

#endif
