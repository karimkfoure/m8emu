#ifndef M8_KEYPAD_RASTER_H
#define M8_KEYPAD_RASTER_H
#include "keypad_cpu.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint8_t *pixels;
    int width, height;
    float scale, x, y;
} M8KeypadRaster;
/* A nonempty stem joins the rounded base into the Model:02 D-pad frame. */
typedef struct { M8KeypadRect base, stem; float radius; } M8KeypadRasterShape;
float m8_keypad_raster_distance(M8KeypadRasterShape shape, float x, float y);
void m8_keypad_raster_shape(M8KeypadRaster *raster, M8KeypadRasterShape shape, M8KeypadColor color);
void m8_keypad_raster_stroke(void *context, const M8KeypadPoint *points, unsigned count,
                      bool closed, float width, M8KeypadColor color);
void m8_keypad_raster_pixels(void *context, int x, int y, int w, int h, M8KeypadColor color);
#endif
