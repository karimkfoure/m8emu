#ifndef M8_EXPLORER_WINDOW_H
#define M8_EXPLORER_WINDOW_H
#include "text_ui.h"
#include "fat.h"

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    m8_text_ui ui;
    m8_fat_card *card;
    char path[4096],message[1024],folder_name[1024];
    size_t folder_cursor;
    bool folder_select_all;
    bool *selected;
    size_t count,scroll,visible_rows;
    int dialog,pending;
    bool warned,closed,failed,dialog_pending,dragging,dirty,read_only;
    M8KeypadTheme drawn_theme;
    unsigned token;
    char **additions;
    size_t addition_count;
} m8_explorer_window;
/* Zero-initialize before the first open. The caller first obtains a checkpoint and paused immutable card copy from
 * the session owner. Open copies bytes; no operation touches the live guest. */
bool m8_explorer_open(m8_explorer_window *,SDL_Window *parent,const void *,size_t);
bool m8_explorer_event(m8_explorer_window *,const SDL_Event *);
bool m8_explorer_draw(m8_explorer_window *,M8KeypadTheme);
/* Keep the offline card until the owner acknowledges commit/reboot. A failed
 * commit leaves the explorer and its edits available for a retry. */
void m8_explorer_reopen(m8_explorer_window *,const char *error);
void m8_explorer_request_close(m8_explorer_window *);
void m8_explorer_close(m8_explorer_window *);
#endif
