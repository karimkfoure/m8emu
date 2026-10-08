#include <SDL3/SDL.h>

static bool force_fullscreen;
static uint64_t presents;
static int requested_vsync = -1;

void m8_render_configure(bool fullscreen) {
    force_fullscreen = fullscreen;
}
void m8_render_configure_vsync(int interval) { requested_vsync = interval; }

static void fit_display(SDL_DisplayID display, int *w, int *h) {
    SDL_Rect bounds;
    if (!display || !SDL_GetDisplayUsableBounds(display, &bounds) ||
        bounds.w <= 0 || bounds.h <= 0 || *w <= 0 || *h <= 0) return;
    double scale = 1.0;
    if (*w > bounds.w) scale = (double)bounds.w / *w;
    if (*h * scale > bounds.h) scale = (double)bounds.h / *h;
    *w = (int)(*w * scale);
    *h = (int)(*h * scale);
    if (*w < 1) *w = 1;
    if (*h < 1) *h = 1;
}

bool m8_create_window_renderer(const char *name, int w, int h, SDL_WindowFlags flags,
                               SDL_Window **window, SDL_Renderer **renderer) {
    (void)name;
    flags &= ~SDL_WINDOW_OPENGL;
    if (force_fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    if (!(flags & SDL_WINDOW_FULLSCREEN)) fit_display(SDL_GetPrimaryDisplay(), &w, &h);
    if (!SDL_CreateWindowAndRenderer("M8", w, h, flags, window, renderer)) return false;
    if (requested_vsync >= 0) {
        int actual = -1;
        const char *backend = SDL_GetRendererName(*renderer);
        bool software = backend && !SDL_strcmp(backend, "software");
        /* SDL software forwards the interval to its window-texture renderer,
         * whose setting need not update the software renderer's property. */
        bool matched = SDL_SetRenderVSync(*renderer, requested_vsync) &&
            SDL_GetRenderVSync(*renderer, &actual) && (software || actual == requested_vsync);
        if (matched && requested_vsync == 1 && backend &&
            (!SDL_strcmp(backend, "opengles2") || !SDL_strcmp(backend, "opengl"))) {
            int swap = -1;
            matched = SDL_GL_GetCurrentContext() && SDL_GL_GetSwapInterval(&swap) && swap == 1;
            if (!matched) SDL_SetError("OpenGL swap interval must be 1: actual=%d", swap);
        }
        if (!matched) {
            char detail[256];
            SDL_snprintf(detail, sizeof detail, "Required vsync is unavailable: requested=%d actual=%d detail=%s",
                         requested_vsync, actual, SDL_GetError());
            SDL_DestroyRenderer(*renderer); *renderer = NULL;
            SDL_DestroyWindow(*window); *window = NULL;
            return SDL_SetError("%s", detail);
        }
    }
    if (flags & SDL_WINDOW_FULLSCREEN) SDL_HideCursor();
    return true;
}
bool m8_set_vsync(SDL_Renderer *renderer, int interval) {
    (void)interval;
    return SDL_SetRenderVSync(renderer, requested_vsync >= 0 ? requested_vsync : 0);
}
bool m8_render_present(SDL_Renderer *renderer) {
    bool result = SDL_RenderPresent(renderer);
    if (result) ++presents;
    return result;
}
uint64_t m8_render_present_count(void) { return presents; }

bool m8_set_window_size(SDL_Window *window, int w, int h) {
    /* m8c requests two pixels per source pixel after a font change. Fullscreen
     * display modes must keep their physical size; SDL handles letterboxing. */
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) return true;
    fit_display(SDL_GetDisplayForWindow(window), &w, &h);
    return SDL_SetWindowSize(window, w, h);
}
