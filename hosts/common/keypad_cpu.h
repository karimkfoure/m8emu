#ifndef M8_KEYPAD_CPU_H
#define M8_KEYPAD_CPU_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Shared logical geometry and flat RGBA rendering; no SDL or GPU dependency. */
typedef struct { float x, y; } M8KeypadPoint;
typedef struct { float x, y, w, h; } M8KeypadRect;
typedef struct { uint8_t r, g, b, a; } M8KeypadColor;
#define M8_KEYPAD_WIDTH 352
#define M8_KEYPAD_HEIGHT 312
#define M8_KEYPAD_CONTACTS 16

/* These values are the M8 controller protocol bits. */
enum {
    M8_KEYPAD_EDIT = 1,
    M8_KEYPAD_OPTION = 2,
    M8_KEYPAD_RIGHT = 4,
    M8_KEYPAD_PLAY = 8,
    M8_KEYPAD_SHIFT = 16,
    M8_KEYPAD_DOWN = 32,
    M8_KEYPAD_UP = 64,
    M8_KEYPAD_LEFT = 128
};

typedef enum {
    M8_KEYPAD_THEME_A, M8_KEYPAD_THEME_B, M8_KEYPAD_THEME_C,
    M8_KEYPAD_THEME_D, M8_KEYPAD_THEME_E, M8_KEYPAD_THEME_COUNT
} M8KeypadTheme;
#define M8_KEYPAD_THEME_DEFAULT M8_KEYPAD_THEME_B
bool m8_keypad_theme_parse(const char *name, M8KeypadTheme *theme);
const char *m8_keypad_theme_name(M8KeypadTheme theme);

typedef struct { uint8_t key; M8KeypadRect rectangle; } M8KeypadKey;
typedef struct { M8KeypadRect rectangle, stem; } M8KeypadGroup;
typedef struct { const char *text; M8KeypadPoint position; unsigned icon; } M8KeypadLabel;
typedef struct {
    M8KeypadKey keys[8];
    M8KeypadGroup groups[3];
    M8KeypadLabel labels[4];
    M8KeypadPoint markers[4];
    M8KeypadRect grilles[2];
    float grille_pitch;
    unsigned grille_slots;
    float frame_radius, key_radius, outer_width, inner_width, outline_gap;
    float key_width, label_size, label_weight, marker_size, marker_weight;
} M8KeypadLayout;
extern const M8KeypadLayout m8_keypad_layout;


/* RGBA32 output uses tightly packed rows of width*4 bytes. The caller owns
 * at least width*height*4 bytes. Layout is centered without changing its
 * aspect ratio. Render only after size, palette or held-state changes. */
bool m8_keypad_cpu_render(uint8_t *rgba, int width, int height,
                          M8KeypadTheme theme, uint8_t held);
bool m8_keypad_cpu_button_rect(const M8KeypadRect *viewport, uint8_t key,
                               M8KeypadRect *rectangle);
uint8_t m8_keypad_cpu_hit_test(const M8KeypadRect *viewport, float x, float y);
uint8_t m8_keypad_cpu_hit(float logical_x, float logical_y);
uint8_t m8_keypad_cpu_hit_pixels(int width, int height, float x, float y);

/* Zero-initialize, then destroy when the host releases the view. Static frames
 * and labels rasterize only on resize or theme change. A held-state update
 * restores and redraws changed caps; pixels remain one complete RGBA image. */
typedef struct {
    uint8_t *pixels, *background;
    int width, height;
    M8KeypadTheme theme;
    uint8_t held;
} M8KeypadCpuCache;
bool m8_keypad_cpu_cache_update(M8KeypadCpuCache *cache, int width, int height,
                                M8KeypadTheme theme, uint8_t held);
void m8_keypad_cpu_cache_destroy(M8KeypadCpuCache *cache);
#endif
