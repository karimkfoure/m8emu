#ifndef M8_TEXT_UI_H
#define M8_TEXT_UI_H
#include "keypad.h"
#include "text_font.h"
#define M8_UI_LINE 20.4f
/* CSS-pixel coordinates. Input commands also support a later gamepad overlay. */
typedef enum { M8_UI_PREVIOUS, M8_UI_NEXT, M8_UI_ACTIVATE, M8_UI_BACK,
               M8_UI_DECREASE, M8_UI_INCREASE } m8_ui_command;
typedef struct {
    SDL_Color page,background,foreground,accent,link,warning;
    SDL_Color muted,line,hover,selected,danger;
    bool dark;
} m8_ui_palette;
typedef struct { int id; SDL_FRect rect; bool enabled; } m8_ui_target;
typedef struct {
    SDL_Renderer *renderer;
    m8_text_font *font;
    m8_ui_palette colors;
    float width,height,point_scale,display_scale,cell,focus_offset,line_height;
    float pointer_x,pointer_y;
    int focus,hover,count;
    bool keyboard_focus;
    m8_ui_target targets[256];
} m8_text_ui;
float m8_ui_point_scale(SDL_Window *);
SDL_Color m8_ui_mix(SDL_Color foreground,SDL_Color background,float fraction);
m8_ui_palette m8_ui_colors(M8KeypadTheme);
const char *m8_ui_theme_name(M8KeypadTheme);
bool m8_ui_begin(m8_text_ui *,SDL_Renderer *,M8KeypadTheme);
void m8_ui_destroy(m8_text_ui *);
float m8_ui_text(m8_text_ui *,float x,float y,const char *,SDL_Color);
float m8_ui_measure(m8_text_ui *,const char *);
float m8_ui_wrap_height(m8_text_ui *,float width,const char *);
float m8_ui_wrap(m8_text_ui *,float x,float y,float width,const char *,SDL_Color);
void m8_ui_fill(m8_text_ui *,SDL_FRect,SDL_Color);
void m8_ui_box(m8_text_ui *,SDL_FRect,SDL_Color);
void m8_ui_line(m8_text_ui *,float x,float y,float end,SDL_Color);
void m8_ui_dashed(m8_text_ui *,SDL_FRect,SDL_Color);
void m8_ui_button(m8_text_ui *,int id,float x,float y,const char *,bool boxed,bool enabled);
void m8_ui_danger_button(m8_text_ui *,int id,float x,float y,const char *,bool boxed);
void m8_ui_link(m8_text_ui *,int id,float x,float y,const char *,bool external,bool enabled);
void m8_ui_check(m8_text_ui *,int id,float x,float y,bool checked,bool mixed);
void m8_ui_target_add(m8_text_ui *,int id,SDL_FRect,bool enabled);
int m8_ui_pointer(m8_text_ui *,float x,float y);
bool m8_ui_motion(m8_text_ui *,float x,float y);
int m8_ui_input(m8_text_ui *,m8_ui_command);
bool m8_ui_key(const SDL_KeyboardEvent *,m8_ui_command *);
#endif
