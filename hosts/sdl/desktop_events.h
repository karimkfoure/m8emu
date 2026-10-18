#ifndef M8_DESKTOP_EVENTS_H
#define M8_DESKTOP_EVENTS_H
#include "settings_window.h"
struct app_context;
bool m8_desktop_event(m8_settings_window *,struct app_context *,const SDL_Event *);
void m8_desktop_release(m8_keypad_window *,struct app_context *);
#endif
