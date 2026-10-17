#ifndef M8_HANDHELD_H
#define M8_HANDHELD_H

#include "config.h"
#include <SDL3/SDL.h>

struct app_context;

#define M8_MAX_GAMEPADS 4

/* Host options stored beside the m8c config, in m8emu_gamepad.ini. Button and
 * analog bindings remain m8c [gamepad] keys. An empty active GUID, or an
 * active pad that is not connected, accepts every gamepad. */
typedef struct m8_pad_options {
    bool exit_combo, background;
    char active[64];
} m8_pad_options;

extern SDL_Gamepad *game_controllers[M8_MAX_GAMEPADS];

/* Apply before config_read(), so every user binding has priority. Face button
 * positions do not depend on the controller's printed letter names. */
void m8_handheld_config_defaults(config_params_s *conf);

/* Call before SDL_AppEvent(). Back/Select and Start are reserved: a same-device
 * chord sets QUIT and releases the M8 mask, and a Back tap toggles the gamepad
 * view. Returns true for consumed events, including hotplug and filtered axis
 * noise. Other input retains m8c's handlers. */
bool m8_handheld_event(struct app_context *ctx, const SDL_Event *event);

/* Release every held gamepad control through m8c and send an empty mask. */
void m8_handheld_release(struct app_context *ctx);

m8_pad_options *m8_handheld_options(void);
void m8_handheld_options_changed(void);
/* The connected active pad, else NULL. Labels prefer it, then the last used. */
SDL_Gamepad *m8_handheld_active(void);
SDL_Gamepad *m8_handheld_label_pad(void);
uint64_t m8_handheld_activity(SDL_JoystickID id);
bool m8_handheld_guid(SDL_JoystickID id, char *text, int capacity);

/* These replace m8c gamepads.c and retain its public symbols and settings ABI. */
int gamepads_initialize(void);
void gamepads_close(void);

#endif
