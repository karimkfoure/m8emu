#ifndef M8_DESKTOP_PREFERENCES_H
#define M8_DESKTOP_PREFERENCES_H
#include "keypad.h"
typedef struct {
    bool controls, hints, positioned;
    M8KeypadTheme theme;
    int settings_x, settings_y;
    char audio[256];
} m8_preferences;
void m8_preferences_load(m8_preferences *);
bool m8_preferences_save(const m8_preferences *);
#endif
