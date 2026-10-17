#ifndef M8_SETTINGS_PLATFORM_H
#define M8_SETTINGS_PLATFORM_H
#include "gamepad_view.h"
/* m8c's settings view prints SDL mapping names such as "a"; print the
 * controller's own labels instead, without changing the pinned submodule. */
#define SDL_GetGamepadStringForButton m8_gamepad_button_name
#define SDL_GetGamepadStringForAxis m8_gamepad_axis_name
#endif
