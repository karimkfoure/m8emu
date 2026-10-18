#ifndef M8_DESKTOP_SESSION_H
#define M8_DESKTOP_SESSION_H
#include "explorer_window.h"
#include "settings_window.h"
struct app_context;
typedef struct {
    m8_explorer_window explorer;
    m8_fat_card *reset_card;
    int intent;
    bool awaiting,reset,quit_requested;
    uint64_t retry_after;
} m8_desktop_session;
bool m8_desktop_session_event(m8_desktop_session *,m8_settings_window *,struct app_context *,const SDL_Event *);
bool m8_desktop_session_update(m8_desktop_session *,m8_settings_window *,struct app_context *,m8_worker *,bool rendered);
void m8_desktop_session_close(m8_desktop_session *);
#endif
