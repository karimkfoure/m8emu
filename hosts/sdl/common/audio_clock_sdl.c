#include "audio_clock.h"
#include <SDL3/SDL.h>
uint64_t m8_audio_now_ns(void) { return SDL_GetTicksNS(); }
