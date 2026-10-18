#include "keypad_glyphs.h"

#include <math.h>
#include <string.h>

/* Original wide geometric lettering. Each contour is an independent path:
 * bars never acquire a diagonal when the pen returns to another stroke. */
static void path(const M8KeypadPathSink *sink, float x, float y, float size,
                 float weight, M8KeypadColor color, const M8KeypadPoint *points,
                 unsigned count, bool closed) {
    M8KeypadPoint scaled[12];
    if (!sink || !sink->stroke || count > 12 || size <= 0 || weight <= 0)
        return;
    for (unsigned i = 0; i < count; ++i) {
        scaled[i].x = x + points[i].x * size;
        scaled[i].y = y + points[i].y * size;
    }
    sink->stroke(sink->context, scaled, count, closed, weight, color);
}

#define PATH(closed, ...) do { \
    const M8KeypadPoint points[] = { __VA_ARGS__ }; \
    path(sink, x, y, size, weight, color, points, \
         (unsigned)(sizeof points / sizeof points[0]), closed); \
} while (0)

static void glyph(const M8KeypadPathSink *sink, char ch, float x, float y,
                  float size, float weight, M8KeypadColor color) {
    switch (ch) {
    case 'O':
        PATH(true, {1,0}, {5,0}, {6,1}, {6,4}, {5,5}, {1,5}, {0,4}, {0,1});
        break;
    case 'P':
        PATH(false, {0,5}, {0,0});
        PATH(false, {0,0}, {5,0}, {6,0.8f}, {6,1.8f}, {5,2.6f}, {0,2.6f});
        break;
    case 'T':
        PATH(false, {0,0}, {6,0});
        PATH(false, {3,0}, {3,5});
        break;
    case 'E':
        PATH(false, {6,0}, {0,0}, {0,5}, {6,5});
        PATH(false, {0,2.5f}, {5.2f,2.5f});
        break;
    case 'D':
        PATH(true, {0,0}, {4.4f,0}, {6,1.3f}, {6,3.7f}, {4.4f,5}, {0,5});
        break;
    case 'I':
        PATH(false, {1,0}, {5,0});
        PATH(false, {3,0}, {3,5});
        PATH(false, {1,5}, {5,5});
        break;
    case 'S':
        PATH(false, {6,0}, {1,0}, {0,0.8f}, {0,1.7f}, {1,2.5f},
                    {5,2.5f}, {6,3.3f}, {6,4.2f}, {5,5}, {0,5});
        break;
    case 'H':
        PATH(false, {0,0}, {0,5});
        PATH(false, {6,0}, {6,5});
        PATH(false, {0,2.5f}, {6,2.5f});
        break;
    case 'F':
        PATH(false, {0,5}, {0,0}, {6,0});
        PATH(false, {0,2.5f}, {5.2f,2.5f});
        break;
    case 'L':
        PATH(false, {0,0}, {0,5}, {6,5});
        break;
    case 'A':
        PATH(false, {0,5}, {0,1}, {1,0}, {5,0}, {6,1}, {6,5});
        PATH(false, {0,2.8f}, {6,2.8f});
        break;
    case 'Y':
        PATH(false, {0,0}, {3,2.6f}, {6,0});
        PATH(false, {3,2.6f}, {3,5});
        break;
    }
}

void m8_keypad_chevron(const M8KeypadPathSink *sink, float x, float y,
                      unsigned direction, float size, float weight,
                      M8KeypadColor color) {
    M8KeypadPoint points[] = {{-2.3f,1.25f}, {0,-1.25f}, {2.3f,1.25f}};
    for (unsigned i = 0; i < 3; ++i) {
        float a = points[i].x, b = points[i].y;
        switch (direction) {
        case 1: points[i].x = b; points[i].y = -a; break;
        case 2: points[i].x = -a; points[i].y = -b; break;
        case 3: points[i].x = -b; points[i].y = a; break;
        default: break;
        }
    }
    path(sink, x, y, size, weight, color, points, 3, false);
}

static void legend_icon(const M8KeypadPathSink *sink, unsigned icon,
                        float x, float y, float size, float weight,
                        M8KeypadColor color) {
    switch (icon) {
    case 0: /* Two offset nodes connected along their shared diagonal. */
        PATH(true, {0,3.1f}, {0.6f,2.5f}, {2,2.5f}, {2.6f,3.1f},
                   {2.6f,4.5f}, {2,5.1f}, {0.6f,5.1f}, {0,4.5f});
        PATH(true, {3.7f,0.5f}, {4.3f,-0.1f}, {5.7f,-0.1f}, {6.3f,0.5f},
                   {6.3f,1.9f}, {5.7f,2.5f}, {4.3f,2.5f}, {3.7f,1.9f});
        PATH(false, {2.2f,2.8f}, {4.1f,2.2f});
        break;
    case 1: /* Eight arms, with a clear geometric asterisk silhouette. */
        PATH(false, {0.3f,2.5f}, {5.7f,2.5f});
        PATH(false, {3,-0.2f}, {3,5.2f});
        PATH(false, {1.1f,0.6f}, {4.9f,4.4f});
        PATH(false, {1.1f,4.4f}, {4.9f,0.6f});
        break;
    case 2: /* Outlined upward arrow and a separate lower bar. */
        PATH(true, {0,2}, {3,-1}, {6,2}, {4.1f,2},
                   {4.1f,4.2f}, {1.9f,4.2f}, {1.9f,2});
        PATH(false, {0,6}, {6,6});
        break;
    case 3:
        m8_keypad_chevron(sink, x + 3 * size, y + 2.5f * size,
                          3, size, weight, color);
        break;
    }
}

void m8_keypad_legend(const M8KeypadPathSink *sink, const char *label,
                     float center, float y, unsigned icon, float size,
                     float weight, M8KeypadColor color) {
    if (!label || !*label || size <= 0 || weight <= 0)
        return;
    const float text_width = (float)strlen(label) * 7.8f - 1.8f;
    const float x = center - (11 + text_width) * size * 0.5f;
    legend_icon(sink, icon, x, y, size, weight, color);
    float letter_x = x + 11 * size;
    for (; *label; ++label, letter_x += 7.8f * size)
        glyph(sink, *label, letter_x, y, size, weight, color);
}

#undef PATH

