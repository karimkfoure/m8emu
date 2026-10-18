#ifndef M8_KEYPAD_GLYPHS_H
#define M8_KEYPAD_GLYPHS_H

#include "keypad_cpu.h"
#include <stdbool.h>

/* Logical paths only. The sink controls rasterization and antialiasing. */
typedef struct {
    void *context;
    void (*stroke)(void *context, const M8KeypadPoint *points, unsigned count,
                   bool closed, float width, M8KeypadColor color);
} M8KeypadPathSink;

void m8_keypad_legend(const M8KeypadPathSink *sink, const char *label,
                     float center, float y, unsigned icon, float size,
                     float weight, M8KeypadColor color);

/* Directions: 0 up, 1 left, 2 down, 3 right. */
void m8_keypad_chevron(const M8KeypadPathSink *sink, float x, float y,
                      unsigned direction, float size, float weight,
                      M8KeypadColor color);

#endif
