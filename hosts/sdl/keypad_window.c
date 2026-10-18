#include "keypad_window.h"
#include "bridge.h"
#include <string.h>

static void destroy_window(m8_keypad_window *pad) {
    m8_keypad_window_release(pad);
    m8_keypad_renderer_destroy(&pad->drawing);
    m8_ui_destroy(&pad->ui);
    SDL_DestroyRenderer(pad->renderer);
    SDL_DestroyWindow(pad->window);
    pad->renderer = NULL;
    pad->window = NULL;
}

static void dock(m8_keypad_window *pad, bool initial) {
    int x, y, w, h;
    if (!pad->window || !SDL_GetWindowPosition(pad->display, &x, &y) ||
        !SDL_GetWindowSize(pad->display, &w, &h)) return;
    if (!initial && x == pad->display_x && y == pad->display_y &&
        w == pad->display_w && h == pad->display_h) return;
    SDL_Rect bounds;
    int top = 0, bottom = 0;
    SDL_GetWindowBordersSize(pad->display, &top, NULL, &bottom, NULL);
    int footer=(int)(28*m8_ui_point_scale(pad->display));
    int kh = w * M8_KEYPAD_HEIGHT / M8_KEYPAD_WIDTH + footer;
    if (initial && SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(pad->display), &bounds)) {
        int available = bounds.h - top - bottom - 16;
        if (h + kh > available && available > 200) {
            double scale = (double)available / (h + kh);
            w = (int)(w * scale); h = (int)(h * scale);
            kh = w * M8_KEYPAD_HEIGHT / M8_KEYPAD_WIDTH + footer;
            SDL_SetWindowSize(pad->display, w, h);
        }
        x = bounds.x + (bounds.w - w) / 2;
        y = bounds.y + top + (bounds.h - top - bottom - h - kh - 8) / 2;
        SDL_SetWindowPosition(pad->display, x, y);
    }
    SDL_SetWindowSize(pad->window, w, kh);
    SDL_SetWindowPosition(pad->window, x, y + h + bottom + 8);
    pad->display_x = x; pad->display_y = y;
    pad->display_w = w; pad->display_h = h;
    pad->dirty = true;
}

void m8_keypad_window_open(m8_keypad_window *pad, SDL_Window *display, int choice, int theme_choice) {
    memset(pad, 0, sizeof *pad);
    /* A focus change between desktop windows must also deliver the click.
     * SDL otherwise consumes a click received just after X11 focus changes. */
    if(display)SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH,"1");
    m8_keypad_init(&pad->keys);
    pad->display = display;
    m8_preferences_load(&pad->preferences);
    bool enabled=pad->preferences.controls;
    pad->theme=pad->preferences.theme;pad->hints=pad->preferences.hints;
    if(choice>=0)enabled=choice!=0;
    if(theme_choice>=0&&theme_choice<M8_KEYPAD_THEME_COUNT)pad->theme=(M8KeypadTheme)theme_choice;
    /* Development flags affect this process only. */
    /* A fullscreen handheld has one presentation surface. Its later embedded
     * host can use the same component without a second desktop window. */
    if (!display || !enabled || ((SDL_GetWindowFlags(display) & SDL_WINDOW_FULLSCREEN) && choice < 0)) return;
    const char *driver = SDL_GetCurrentVideoDriver();
    if (driver && (!SDL_strcmp(driver, "kmsdrm") || !SDL_strcmp(driver, "android"))) return;
    m8_keypad_window_visible(pad,true);
}

void m8_keypad_window_visible(m8_keypad_window *pad,bool visible) {
    if(!visible){destroy_window(pad);return;}
    if(pad->window||!pad->display)return;
    int w=0;SDL_GetWindowSize(pad->display,&w,NULL);
    pad->window=SDL_CreateWindow("M8 Keypad",w,w*M8_KEYPAD_HEIGHT/M8_KEYPAD_WIDTH+(int)(28*m8_ui_point_scale(pad->display)),
        SDL_WINDOW_BORDERLESS|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if(pad->window)pad->renderer=SDL_CreateRenderer(pad->window,NULL);
    if(!pad->window||!pad->renderer){
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Keypad window is unavailable: %s",SDL_GetError());
        destroy_window(pad);return;
    }
    SDL_SetRenderVSync(pad->renderer,0);dock(pad,true);SDL_RaiseWindow(pad->display);
}
void m8_keypad_window_theme(m8_keypad_window *pad,M8KeypadTheme theme) {
    pad->theme=theme;pad->preferences.theme=theme;pad->dirty=true;
}

static SDL_FRect input_viewport(const m8_keypad_window *pad) {
    int w = 0, h = 0;
    SDL_GetWindowSize(pad->window, &w, &h);
    return (SDL_FRect){0, 0, (float)w, (float)SDL_max(1,h-(int)(28*m8_ui_point_scale(pad->window)))};
}

void m8_keypad_window_release(m8_keypad_window *pad) {
    if (m8_keypad_held(&pad->keys)) {
        m8_keypad_release_all(&pad->keys);
        m8_bridge_keypad(0);
        pad->dirty = true;
    }
    if (pad->mouse_buttons) SDL_CaptureMouse(false);
    pad->mouse_buttons = 0;
}

bool m8_keypad_window_event(m8_keypad_window *pad, const SDL_Event *event) {
    if (!pad->window) return false;
    SDL_WindowID id = SDL_GetWindowID(pad->window);
    uint8_t before = m8_keypad_held(&pad->keys);
    bool handled = false;
    SDL_FRect viewport = input_viewport(pad);
    switch (event->type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (event->window.windowID != id) break;
        pad->preferences.controls=false;
        m8_preferences_save(&pad->preferences);
        destroy_window(pad);
        return true;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.windowID == id && event->key.key == SDLK_F4 &&
            (event->key.mod & SDL_KMOD_ALT)) {
            pad->preferences.controls=false;
            m8_preferences_save(&pad->preferences);
            destroy_window(pad);
            return true;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.windowID != id || event->button.which == SDL_TOUCH_MOUSEID) break;
        if (!event->button.button || event->button.button > 32) break;
        handled = true;
        if(event->button.y>=viewport.h) {
            if(event->button.button==SDL_BUTTON_LEFT&&m8_ui_pointer(&pad->ui,event->button.x,event->button.y)==1)
                pad->settings_requested=true;
            pad->dirty=true;return true;
        }
        if (m8_keypad_pointer_down(&pad->keys, &viewport, UINT64_MAX, event->button.button,
                                  event->button.x, event->button.y)) {
            pad->mouse_buttons |= SDL_BUTTON_MASK(event->button.button);
            SDL_CaptureMouse(true);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.which == SDL_TOUCH_MOUSEID) break;
        if (!event->button.button || event->button.button > 32) break;
        handled = m8_keypad_pointer_up(&pad->keys, UINT64_MAX, event->button.button);
        pad->mouse_buttons &= ~SDL_BUTTON_MASK(event->button.button);
        if (!pad->mouse_buttons) SDL_CaptureMouse(false);
        handled |= event->button.windowID == id;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if(event->motion.windowID==id&&event->motion.which!=SDL_TOUCH_MOUSEID) {
            pad->dirty|=m8_ui_motion(&pad->ui,event->motion.x,event->motion.y);
            handled=true;
        }
        break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        if (event->tfinger.touchID == SDL_MOUSE_TOUCHID) break;
        if (event->type == SDL_EVENT_FINGER_UP || event->type == SDL_EVENT_FINGER_CANCELED) {
            handled = m8_keypad_pointer_up(&pad->keys, event->tfinger.touchID, event->tfinger.fingerID);
        } else if (event->tfinger.windowID == id) {
            int height=0;SDL_GetWindowSize(pad->window,NULL,&height);
            float x = event->tfinger.x * viewport.w, y = event->tfinger.y * height;
            if(event->type==SDL_EVENT_FINGER_DOWN&&y>=viewport.h) {
                if(m8_ui_pointer(&pad->ui,x,y)==1)pad->settings_requested=true;
                pad->dirty=true;
            } else if (event->type == SDL_EVENT_FINGER_DOWN)
                m8_keypad_pointer_down(&pad->keys, &viewport, event->tfinger.touchID, event->tfinger.fingerID, x, y);
            else m8_keypad_pointer_move(&pad->keys, &viewport, event->tfinger.touchID, event->tfinger.fingerID, x, y);
            handled = true;
        }
        handled |= event->tfinger.windowID == id;
        break;
    default:
        if (event->type >= SDL_EVENT_WINDOW_FIRST && event->type <= SDL_EVENT_WINDOW_LAST &&
            event->window.windowID == id) {
            pad->dirty = true;
            handled = true;
        }
        break;
    }
    if (m8_keypad_held(&pad->keys) != before) {
        m8_bridge_keypad(m8_keypad_held(&pad->keys));
        pad->dirty = true;
    }
    return handled;
}

bool m8_keypad_window_draw(m8_keypad_window *pad, uint8_t external_held) {
    if (!pad->renderer) return true;
    dock(pad, false);
    int w = 0, h = 0;
    if (!SDL_GetRenderOutputSize(pad->renderer, &w, &h)) return false;
    uint8_t held = external_held | m8_keypad_held(&pad->keys);
    if (!pad->dirty && held == pad->drawn_held && w == pad->output_w && h == pad->output_h) return true;
    m8_text_ui *ui=&pad->ui;
    if(!m8_ui_begin(ui,pad->renderer,pad->theme))return false;
    float scale=ui->width>0?w/ui->width:1;
    SDL_FRect viewport={0,0,(float)w,(float)h-28*scale};
    SDL_SetRenderScale(pad->renderer,1,1);
    if(!m8_keypad_render(&pad->drawing,&pad->keys,pad->renderer,&viewport,external_held,pad->theme))return false;
    SDL_SetRenderScale(pad->renderer,scale,scale);
    if(pad->hints) {
        const uint8_t keys[]={M8_KEYPAD_UP,M8_KEYPAD_OPTION,M8_KEYPAD_EDIT,M8_KEYPAD_LEFT,
            M8_KEYPAD_DOWN,M8_KEYPAD_RIGHT,M8_KEYPAD_SHIFT,M8_KEYPAD_PLAY};
        const char *const labels[]={"↑","a","s","←","↓","→","z","x"};
        SDL_FRect logical={0,0,ui->width,ui->height-28};
        for(unsigned i=0;i<SDL_arraysize(keys);i++) {
            SDL_FRect rect;if(m8_keypad_button_rect(&logical,keys[i],&rect))
                m8_ui_text(ui,rect.x+rect.w/2-4,rect.y+rect.h/2-8,labels[i],ui->colors.foreground);
        }
    }
    m8_ui_button(ui,1,ui->width-80,ui->height-24,"settings",false,true);
    if(!SDL_RenderPresent(pad->renderer))return false;
    pad->dirty = false; pad->drawn_held = held;
    pad->output_w = w; pad->output_h = h;
    return true;
}

bool m8_keypad_window_focused(const m8_keypad_window *pad) {
    SDL_Window *focus = SDL_GetKeyboardFocus();
    return focus && (focus == pad->display || focus == pad->window);
}

void m8_keypad_window_close(m8_keypad_window *pad) {
    destroy_window(pad);

}
