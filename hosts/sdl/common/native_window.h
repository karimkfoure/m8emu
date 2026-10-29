#ifndef M8_NATIVE_WINDOW_H
#define M8_NATIVE_WINDOW_H
#include <SDL3/SDL.h>
/* Native callbacks run on the SDL main thread. Stop before destroying window. */
void *m8_native_fullscreen_watch(SDL_Window *,void (*changed)(void *),void (*redraw)(void *),void *);
void m8_native_fullscreen_unwatch(void *);
#endif
