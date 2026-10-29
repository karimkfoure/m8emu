#ifndef M8_RENDER_PLATFORM_H
#define M8_RENDER_PLATFORM_H
#include <SDL3/SDL.h>
/* Call on the main thread before renderer_initialize(). The config fullscreen
 * flag remains valid when force_fullscreen is false. */
void m8_render_configure(bool force_fullscreen);
/* Includes the native fullscreen transition before SDL updates its flags. */
bool m8_render_fullscreen(SDL_Window *);
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
/* The desktop compositor owns presentation while attached. */
void m8_render_embed(SDL_Renderer *);
/* Both values are heights per unit window width. */
void m8_render_chrome(float key_ratio, float footer_ratio);
SDL_Texture *m8_render_screen_texture(SDL_Renderer *);
bool m8_render_texture(SDL_Renderer *, SDL_Texture *, const SDL_FRect *, const SDL_FRect *);
bool m8_render_finish(SDL_Renderer *);
void m8_render_destroy_texture(SDL_Texture *);
#define SDL_CreateWindowAndRenderer m8_create_window_renderer
#define SDL_SetRenderVSync m8_set_vsync
#define SDL_SetWindowSize m8_set_window_size
#define SDL_RenderPresent m8_render_present
#define SDL_RenderTexture m8_render_texture
#define SDL_DestroyTexture m8_render_destroy_texture
#endif
