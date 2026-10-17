#ifndef M8_RENDER_PLATFORM_H
#define M8_RENDER_PLATFORM_H
#include <SDL3/SDL.h>
/* Call on the main thread before renderer_initialize(). The config fullscreen
 * flag remains valid when force_fullscreen is false. */
void m8_render_configure(bool force_fullscreen);
/* -1 retains the desktop's nonblocking default. Explicit 0 or 1 is required
 * during creation; an unsupported interval fails before guest execution. */
void m8_render_configure_vsync(int interval);
/* The upstream renderer requests OpenGL even for SDL software rendering.
 * SDL selects the graphics backend; the producer must not block on vsync. */
bool m8_create_window_renderer(const char *, int, int, SDL_WindowFlags,
                               SDL_Window **, SDL_Renderer **);
bool m8_set_vsync(SDL_Renderer *, int);
bool m8_set_window_size(SDL_Window *, int, int);
bool m8_render_present(SDL_Renderer *);
uint64_t m8_render_present_count(void);
#define SDL_CreateWindowAndRenderer m8_create_window_renderer
#define SDL_SetRenderVSync m8_set_vsync
#define SDL_SetWindowSize m8_set_window_size
#define SDL_RenderPresent m8_render_present
/* The gamepad view composites after m8c's log overlay, on the same target. */
#include "log_overlay.h"
#include "gamepad_view.h"
#define log_overlay_render(r, w, h, s, f) (log_overlay_render(r, w, h, s, f), m8_overlay_render(r, w, h, s, f))
#endif
