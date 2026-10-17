#ifndef M8_GAMEPAD_VIEW_H
#define M8_GAMEPAD_VIEW_H

#include <SDL3/SDL.h>

struct app_context;

/* An M8-style gamepad screen drawn over the device canvas below the scope:
 * pads, button mapping and analog/system settings. F3, Cmd+G or a Back tap
 * toggles it; arrows move, EDIT activates, EDIT+arrows changes a value,
 * OPTION+EDIT clears, OPTION closes and SHIFT+LEFT/RIGHT changes the page.
 * Bindings persist to the m8c config on close, without its other overrides. */
bool m8_gamepad_view_is_open(void);
void m8_gamepad_view_toggle(struct app_context *ctx);
/* ctx may be NULL at shutdown; pending bindings are still saved. */
void m8_gamepad_view_close(struct app_context *ctx);
void m8_gamepad_view_refresh(void);
/* Consumes keyboard and gamepad input while the view is open. */
bool m8_gamepad_view_event(struct app_context *ctx, const SDL_Event *event);
/* Called by m8c render.c after its log overlay, on the logical canvas target. */
void m8_overlay_render(SDL_Renderer *renderer, int width, int height, SDL_ScaleMode mode, int font_mode);

/* Printed names, e.g. CROSS, L2 or CREATE; "---" for no binding. The second
 * pair names buttons on the active or last used pad, for m8c's settings. */
const char *m8_gamepad_label(SDL_Gamepad *pad, int button);
const char *m8_gamepad_axis_label(SDL_Gamepad *pad, int axis);
const char *m8_gamepad_button_name(SDL_GamepadButton button);
const char *m8_gamepad_axis_name(SDL_GamepadAxis axis);

#endif
