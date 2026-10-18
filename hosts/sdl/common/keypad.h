#ifndef M8EMU_KEYPAD_H
#define M8EMU_KEYPAD_H

#include <SDL3/SDL.h>
#include "../../common/keypad_cpu.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t device_id;
    uint64_t pointer_id;
    uint8_t key;
    bool occupied;
    bool inside;
} M8KeypadContact;

typedef struct {
    M8KeypadContact contacts[M8_KEYPAD_CONTACTS];
} M8Keypad;

/* Zero-initialize this cache and destroy it before its SDL renderer. Input
 * contacts have a separate lifetime, so releasing keys never loses textures. */
typedef struct {
    M8KeypadCpuCache cpu;
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    int width, height;
    M8KeypadTheme theme;
    uint8_t held;
    unsigned rasterizations;
} M8KeypadRenderer;
void m8_keypad_renderer_destroy(M8KeypadRenderer *drawing);

void m8_keypad_init(M8Keypad *keypad);

/* The layout keeps its aspect ratio and is centered in viewport. */
bool m8_keypad_button_rect(const SDL_FRect *viewport, uint8_t key,
                          SDL_FRect *rectangle);
uint8_t m8_keypad_hit_test(const SDL_FRect *viewport, float x, float y);

/* True means this contact was handled. Outside presses and excess contacts
 * return false. IDs remain captured until up, cancel or release_all. A captured
 * contact holds only its original key; moving outside releases it, and moving
 * back inside holds it again. Each event preserves the caller's input order. */
bool m8_keypad_pointer_down(M8Keypad *keypad, const SDL_FRect *viewport,
                           uint64_t device_id, uint64_t pointer_id,
                           float x, float y);
bool m8_keypad_pointer_move(M8Keypad *keypad, const SDL_FRect *viewport,
                           uint64_t device_id, uint64_t pointer_id,
                           float x, float y);
bool m8_keypad_pointer_up(M8Keypad *keypad, uint64_t device_id,
                         uint64_t pointer_id);
bool m8_keypad_pointer_cancel(M8Keypad *keypad, uint64_t device_id,
                             uint64_t pointer_id);
void m8_keypad_release_all(M8Keypad *keypad);
uint8_t m8_keypad_held(const M8Keypad *keypad);

/* Draws caps and legends without a console body. external_held highlights
 * keyboard/gamepad keys only; it never changes the pointer-held mask.
 * viewport uses actual renderer output pixels with SDL's default render
 * scale. SDL_GetRenderOutputSize includes HiDPI; do not multiply it again.
 * Only size, palette or held-state changes rasterize. Each draw submits one
 * texture quad. The caller presents its target and schedules redraws. */
bool m8_keypad_render(M8KeypadRenderer *drawing, const M8Keypad *keypad,
                     SDL_Renderer *renderer, const SDL_FRect *viewport,
                     uint8_t external_held, M8KeypadTheme theme);

#endif
