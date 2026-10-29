#ifndef M8_SETTINGS_WINDOW_H
#define M8_SETTINGS_WINDOW_H
#include "keypad_window.h"
#include "text_ui.h"
#include "worker.h"

typedef enum { M8_SETTINGS_NONE, M8_SETTINGS_EXPLORER, M8_SETTINGS_RESET,
               M8_SETTINGS_AUDIO } m8_settings_action;
typedef struct {
    SDL_Window *window,*display,*about_window;
    SDL_Renderer *renderer,*about_renderer;
    m8_text_ui ui,about_ui;
    m8_keypad_window *keypad;
    m8_settings_action action;
    SDL_AudioDeviceID audio_device;
    char audio_name[256],error[512],storage_error[512];
    bool audio_initialized,dirty,retained_before_launch,explorer_busy,reset_busy;
    bool fullscreen,restore_settings,restore_about,restore_explorer;
    uint64_t draw_after;
    M8KeypadTheme drawn_theme;
    uint64_t sample_ns,sample_frames,sample_epoch;
    double sample_service;
    uint64_t retained_ns;
    double realtime;
    int dialog;
} m8_settings_window;
void m8_settings_open(m8_settings_window *, SDL_Window *, m8_keypad_window *);
void m8_settings_show(m8_settings_window *);
void m8_about_show(m8_settings_window *);
void m8_settings_fullscreen(m8_settings_window *,SDL_Window *explorer);
bool m8_settings_event(m8_settings_window *,const SDL_Event *);
void m8_settings_update(m8_settings_window *,m8_worker *,m8live_audio *,const m8_worker_state *);
bool m8_settings_draw(m8_settings_window *);
void m8_settings_close(m8_settings_window *);
/* Main thread takes actions and passes guest work to the session owner. */
m8_settings_action m8_settings_take_action(m8_settings_window *);
#endif
