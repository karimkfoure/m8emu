#ifndef M8_HANDHELD_H
#define M8_HANDHELD_H

#include "config.h"
#include <SDL3/SDL.h>

struct app_context;

/* Apply before config_read(), so every user binding has priority. Face button
 * positions do not depend on the controller's printed letter names. */
void m8_handheld_config_defaults(config_params_s *conf);

/* Call before SDL_AppEvent(). Back/Select and Start are reserved for quitting:
 * a same-device chord sets QUIT and releases the M8 mask. Returns true for
 * these buttons and hotplug events. Other input retains m8c's handlers. */
bool m8_handheld_event(struct app_context *ctx, const SDL_Event *event);

/* These replace m8c gamepads.c and retain its public symbols and settings ABI. */
int gamepads_initialize(void);
void gamepads_close(void);

#endif
