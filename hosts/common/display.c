/* CPU implementation of the pinned m8c display command contract. Font masks
 * come directly from its bundled M8 bitmap fonts; no SDL or browser API is
 * needed. See third_party/m8c/LICENSE for the renderer and font attribution. */
#include "display.h"
#include "fonts/fonts.h"
#include <string.h>

static unsigned little16(const uint8_t *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}
static uint32_t little32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint32_t color(const uint8_t *p) {
    return (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
}
static void pixel(M8WebDisplay *d, int x, int y, uint32_t rgb) {
    if (x < 0 || y < 0 || (unsigned)x >= d->width || (unsigned)y >= d->height) return;
    uint8_t *p = d->pixels + ((size_t)y * d->width + (unsigned)x) * 4u;
    p[0] = (uint8_t)(rgb >> 16); p[1] = (uint8_t)(rgb >> 8);
    p[2] = (uint8_t)rgb; p[3] = 255;
}
static void rectangle(M8WebDisplay *d, int x, int y, unsigned width,
                        unsigned height, uint32_t rgb) {
    int right = x + (int)width, bottom = y + (int)height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right > (int)d->width) right = (int)d->width;
    if (bottom > (int)d->height) bottom = (int)d->height;
    for (int row = y; row < bottom; ++row)
        for (int col = x; col < right; ++col) pixel(d, col, row, rgb);
}

/* All five pinned font sheets are uncompressed, bottom-up, one-bit BMPs.
 * Decode the bitmap in place, so the web target needs no image decoder. */
static int font_pixel(const struct inline_font *font, unsigned x, unsigned y) {
    const uint8_t *bmp = font->image_data;
    size_t stride = ((size_t)font->width + 31u) / 32u * 4u;
    size_t at = little32(bmp + 10) + ((size_t)font->height - y - 1u) * stride + x / 8u;
    unsigned index = (bmp[at] >> (7u - x % 8u)) & 1u;
    size_t palette = 14u + little32(bmp + 14) + index * 4u;
    /* Font 1 has black at index 0; fonts 2..5 reverse that palette. */
    return bmp[palette] || bmp[palette + 1] || bmp[palette + 2];
}
static void character(M8WebDisplay *d, const uint8_t *p) {
    const struct inline_font *font = fonts_get(d->font);
    unsigned c = p[1];
    int x = (int)little16(p + 2);
    int y = (int)little16(p + 4) + font->screen_offset_y + font->text_offset_y;
    uint32_t foreground = color(p + 6), background = color(p + 9);
    if (!c || c == '\n') return;
    if (foreground != background)
        rectangle(d, x, y, (unsigned)font->glyph_x, (unsigned)font->glyph_y, background);
    if (c >= 33 && c <= 126) {
        unsigned glyph_width = (unsigned)font->width / 94u;
        unsigned source_x = (c - 33u) * glyph_width;
        for (unsigned row = 0; row < (unsigned)font->height; ++row)
            for (unsigned col = 0; col < glyph_width; ++col)
                if (font_pixel(font, source_x + col, row))
                    pixel(d, x + (int)col, y + (int)row, foreground);
    }
}
static int command(M8WebDisplay *d) {
    const uint8_t *p = d->packet;
    unsigned n = d->packet_size;
    if (!n) return 0;
    switch (p[0]) {
    case 0xfe: {
        if (n != 5 && n != 8 && n != 9 && n != 12) return -1;
        unsigned width = 1, height = 1;
        if (n == 9 || n == 12) { width = little16(p + 5); height = little16(p + 7); }
        if (n == 8 || n == 12) d->rectangle_color = color(p + n - 3);
        int x = (int)little16(p + 1);
        int y = (int)little16(p + 3) + fonts_get(d->font)->screen_offset_y;
        if (!x && y <= 0 && width == d->width && height >= d->height)
            d->background_color = d->rectangle_color;
        rectangle(d, x, y, width, height, d->rectangle_color);
        ++d->dirty_serial;
        break;
    }
    case 0xfd:
        if (n != 12) return -1;
        character(d, p);
        ++d->dirty_serial;
        break;
    case 0xfc: {
        if (n < 4 || n > M8_DISPLAY_PACKET_BYTES) return -1;
        unsigned samples = n - 4;
        if (d->waveform_cleared && !samples) break;
        unsigned height = (unsigned)fonts_get(d->font)->waveform_max_height;
        unsigned clear_width = samples ? samples : d->waveform_size;
        rectangle(d, (int)d->width - (int)clear_width, 0,
                   clear_width, height + 1, d->background_color);
        for (unsigned i = 0; i < samples; ++i)
            pixel(d, (int)d->width - (int)samples + (int)i,
                  p[4 + i] > height ? (int)height : p[4 + i], color(p + 1));
        d->waveform_size = samples;
        d->waveform_cleared = !samples;
        ++d->dirty_serial;
        break;
    }
    case 0xfb:
        if (n != 3) return -1;
        break;
    case 0xff: {
        if (n != 6) return -1;
        unsigned model2 = p[1] == 3;
        unsigned width = model2 ? 480 : 320, height = model2 ? 320 : 240;
        if (width != d->width || height != d->height) {
            d->width = width; d->height = height;
            rectangle(d, 0, 0, width, height, d->background_color);
        }
        if (p[5] <= 2) d->font = p[5] + (model2 ? 2u : 0u);
        ++d->dirty_serial;
        break;
    }
    default: return -1;
    }
    ++d->packets;
    return 0;
}

void m8_display_init(M8WebDisplay *d) {
    memset(d, 0, sizeof *d);
    d->width = 320; d->height = 240;
    rectangle(d, 0, 0, d->width, d->height, 0);
    d->dirty_serial = 1;
}
const uint8_t *m8_display_pixels(const M8WebDisplay *d) { return d->pixels; }
int m8_display_feed(M8WebDisplay *d, const void *bytes, size_t count) {
    const uint8_t *p = bytes;
    int failed = 0;
    if (!d || (!bytes && count)) return -1;
    for (size_t i = 0; i < count; ++i) {
        unsigned value = p[i];
        if (value == 0xc0) {
            if (!d->discard && !d->escaped && command(d)) { ++d->errors; failed = -1; }
            else if (d->escaped) { ++d->errors; failed = -1; }
            d->packet_size = d->escaped = d->discard = 0;
            continue;
        }
        if (d->discard) continue;
        if (d->escaped) {
            if (value == 0xdc) value = 0xc0;
            else if (value == 0xdd) value = 0xdb;
            else { d->discard = 1; d->escaped = 0; ++d->errors; failed = -1; continue; }
            d->escaped = 0;
        } else if (value == 0xdb) { d->escaped = 1; continue; }
        if (d->packet_size == sizeof d->packet) {
            d->discard = 1; ++d->errors; failed = -1; continue;
        }
        d->packet[d->packet_size++] = (uint8_t)value;
    }
    return failed;
}
