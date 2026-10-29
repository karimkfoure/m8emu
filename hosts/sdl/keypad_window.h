#ifndef M8_KEYPAD_WINDOW_H
#define M8_KEYPAD_WINDOW_H
#include "keypad.h"
#include "preferences.h"
#include "text_ui.h"

/* The display owns the shared window; this component draws its controls. */
typedef struct {
    SDL_Window *display, *window;
    SDL_Renderer *renderer;
    M8Keypad keys;
    M8KeypadRenderer drawing;
    M8KeypadTheme theme;
    m8_preferences preferences;
    m8_text_ui ui;
    bool hints, settings_requested, about_requested, controls_visible, owns_renderer;
    float screen_aspect, footer_ratio;
    bool resetting;
    void *native_fullscreen_observer;
    void (*presentation_changed)(void *);
    void *presentation_context;
    double realtime_ratio;
    int display_x, display_y, display_w, display_h;
    int output_w, output_h;
    uint8_t drawn_held;
    unsigned mouse_buttons;
    bool dirty;
} m8_keypad_window;

SDL_FRect m8_keypad_window_viewport(const m8_keypad_window *);
void m8_keypad_window_open(m8_keypad_window *, SDL_Window *display, int choice, int theme_choice);
bool m8_keypad_window_event(m8_keypad_window *, const SDL_Event *);
bool m8_keypad_window_draw(m8_keypad_window *, uint8_t external_held);
void m8_keypad_window_visible(m8_keypad_window *, bool);
void m8_keypad_window_theme(m8_keypad_window *, M8KeypadTheme);
void m8_keypad_window_release(m8_keypad_window *);
void m8_keypad_window_close(m8_keypad_window *);
bool m8_keypad_window_focused(const m8_keypad_window *);
#endif
