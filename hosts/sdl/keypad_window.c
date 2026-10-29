#include "keypad_window.h"
#include "bridge.h"
#include "render_platform.h"
#undef SDL_RenderTexture
#undef SDL_RenderPresent
#undef SDL_SetWindowSize
#include <string.h>
#ifdef __APPLE__
#include "native_window.h"
static void native_redraw(void *context) {
    m8_keypad_window_draw(context,0);
}
static void native_presentation_changed(void *context) {
    m8_keypad_window *pad=context;
    m8_keypad_window_release(pad);
    if(pad->presentation_changed)pad->presentation_changed(pad->presentation_context);
    /* Draw before returning to AppKit, which can capture the animation frame
     * before the ordinary SDL event loop gets its next turn. */
    m8_keypad_window_draw(pad,0);
}
#endif

static float footer_height(const m8_keypad_window *pad) {
    int width=0;SDL_GetWindowSize(pad->display,&width,NULL);
    return pad->footer_ratio>0?width*pad->footer_ratio:28*m8_ui_point_scale(pad->display);
}
static float screen_aspect(const m8_keypad_window *pad) {
    SDL_Texture *screen=m8_render_screen_texture(pad->renderer);
    float w=0,h=0;
    if(screen&&SDL_GetTextureSize(screen,&w,&h)&&h>0)return w/h;
    return pad->screen_aspect>0?pad->screen_aspect:1.5f;
}
/* Window coordinates, shared by mouse, touch and rendering. */
SDL_FRect m8_keypad_window_viewport(const m8_keypad_window *pad) {
    int w=0,h=0;
    if(!pad->window)return (SDL_FRect){0};
    SDL_GetWindowSize(pad->window,&w,&h);
    float key_ratio=pad->controls_visible?(float)M8_KEYPAD_HEIGHT/M8_KEYPAD_WIDTH:0;
    float content_w=(float)w;
    float screen_h=content_w/screen_aspect(pad);
    return (SDL_FRect){(w-content_w)/2,screen_h,content_w,content_w*key_ratio};
}
void m8_keypad_window_open(m8_keypad_window *pad,SDL_Window *display,int choice,int theme_choice) {
    memset(pad,0,sizeof *pad);
    m8_keypad_init(&pad->keys);
    pad->display=display;
    m8_preferences_load(&pad->preferences);
    pad->theme=pad->preferences.theme;pad->hints=pad->preferences.hints;
    if(theme_choice>=0&&theme_choice<M8_KEYPAD_THEME_COUNT)pad->theme=(M8KeypadTheme)theme_choice;
    if(!display)return;
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH,"1");
    pad->window=display;pad->renderer=SDL_GetRenderer(display);
    /* Unit hosts may supply a window without a renderer. */
    if(!pad->renderer){pad->renderer=SDL_CreateRenderer(display,NULL);pad->owns_renderer=true;}
    int w=0,h=0;SDL_GetWindowSize(display,&w,&h);
    pad->screen_aspect=h>0?(float)w/h:1.5f;
    m8_render_embed(pad->renderer);
    bool visible=choice>=0?choice!=0:pad->preferences.controls;
    float key_ratio=visible?(float)M8_KEYPAD_HEIGHT/M8_KEYPAD_WIDTH:0;
    float ratio=1/pad->screen_aspect+key_ratio;
    float footer=footer_height(pad);
    pad->controls_visible=visible;

    if(!(SDL_GetWindowFlags(display)&SDL_WINDOW_FULLSCREEN)) {
        SDL_Rect bounds;int top=0,bottom=0,left=0,right=0;
        int max_width=w>480?w:480;
        SDL_GetWindowBordersSize(display,&top,&left,&bottom,&right);
        bool have_bounds=SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(display),&bounds);
        if(have_bounds) {
            float available=bounds.h-top-bottom-footer-16;
            if(available>0)max_width=SDL_min(bounds.w-left-right-16,(int)(available/ratio));
        }
        int minimum=SDL_max(1,SDL_min(480,max_width));
        w=SDL_max(minimum,SDL_min(w,max_width));
        SDL_SetWindowMinimumSize(display,minimum,280);
        /* Set the complete size once. Window-system resize requests can be
         * asynchronous, so a second size read here would still see the old height. */
        int height=(int)SDL_ceilf(w*ratio+footer);
        pad->footer_ratio=footer/w;
        SDL_SetWindowAspectRatio(display,(float)w/height,(float)w/height);
        SDL_SetWindowSize(display,w,height);
        /* The screen-only window was centered before chrome was attached.
         * Keep the complete initial window on screen; later toggles retain y. */
        if(have_bounds) {
            int x=0,y=0;SDL_GetWindowPosition(display,&x,&y);
            x=SDL_clamp(x,bounds.x+left,bounds.x+SDL_max(left,bounds.w-w-right));
            y=SDL_clamp(y,bounds.y+top,bounds.y+SDL_max(top,bounds.h-height-bottom));
            SDL_SetWindowPosition(display,x,y);
        }
    }
    if(!pad->footer_ratio)pad->footer_ratio=footer/SDL_max(w,1);
    m8_render_chrome(key_ratio,pad->footer_ratio);
#ifdef __APPLE__
    pad->native_fullscreen_observer=m8_native_fullscreen_watch(display,native_presentation_changed,native_redraw,pad);
#endif
    pad->dirty=true;
}
void m8_keypad_window_visible(m8_keypad_window *pad,bool visible) {
    if(!pad->window||visible==pad->controls_visible)return;
    m8_keypad_window_release(pad);
    /* Resolve the preceding user/programmatic resize before measuring it. */
    SDL_SyncWindow(pad->window);
    int w=0,h=0,x=0,y=0;
    SDL_GetWindowSize(pad->window,&w,&h);SDL_GetWindowPosition(pad->window,&x,&y);
    SDL_FRect viewport=m8_keypad_window_viewport(pad);
    int key_height=(int)(viewport.w*M8_KEYPAD_HEIGHT/M8_KEYPAD_WIDTH);
    pad->controls_visible=visible;
    m8_render_chrome(visible?(float)M8_KEYPAD_HEIGHT/M8_KEYPAD_WIDTH:0,pad->footer_ratio);
    if(!m8_render_fullscreen(pad->window)) {
        int height=h+(visible?key_height:-key_height);
        SDL_SetWindowAspectRatio(pad->window,(float)w/height,(float)w/height);
        SDL_SetWindowSize(pad->window,w,height);
        SDL_SetWindowPosition(pad->window,x,y);
    }
    pad->dirty=true;
}
void m8_keypad_window_theme(m8_keypad_window *pad,M8KeypadTheme theme) {
    pad->theme=theme;pad->preferences.theme=theme;pad->dirty=true;
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
    SDL_FRect viewport = m8_keypad_window_viewport(pad);
    bool fullscreen = m8_render_fullscreen(pad->window);
    switch (event->type) {
#ifdef __APPLE__
    case SDL_EVENT_WINDOW_MAXIMIZED:
        if(event->window.windowID==id) {
            SDL_RestoreWindow(pad->window);
            SDL_SetWindowFullscreen(pad->window,true);
            m8_keypad_window_release(pad);
            return true;
        }
        break;
#endif
    case SDL_EVENT_WINDOW_RESIZED:
        if(event->window.windowID==id&&!fullscreen) {
            int width=0,height=0;float minimum=0,maximum=0;
            SDL_GetWindowSize(pad->window,&width,&height);
            if(SDL_GetWindowAspectRatio(pad->window,&minimum,&maximum)&&minimum>0) {
                int desired=(int)SDL_roundf(width/minimum);
                /* Some window systems enforce aspect hints only during a
                 * border drag. Correct programmatic and unconstrained resizes too. */
                if(SDL_abs(height-desired)>1)SDL_SetWindowSize(pad->window,width,desired);
            }
            pad->dirty=true;
        }
        return false;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        return false;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.windowID == id && event->key.key == SDLK_ESCAPE && fullscreen) {
            SDL_SetWindowFullscreen(pad->window, false);
            SDL_ShowCursor();
            return true;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.windowID != id || event->button.which == SDL_TOUCH_MOUSEID) break;
        if (!event->button.button || event->button.button > 32) break;
        handled = true;
        if(fullscreen)return false;
        if(event->button.y>=viewport.y+viewport.h) {
            if(event->button.button==SDL_BUTTON_LEFT) {
                int action=m8_ui_pointer(&pad->ui,event->button.x,event->button.y);
                if(action==1)pad->settings_requested=true;
                if(action==2)pad->about_requested=true;
            }
            pad->dirty=true;return true;
        }
        if (pad->controls_visible && m8_keypad_pointer_down(&pad->keys, &viewport, UINT64_MAX, event->button.button,
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
            int width=0;SDL_GetWindowSize(pad->window,&width,NULL);
            float x = event->tfinger.x * width, y = event->tfinger.y * height;
            if(fullscreen)return false;
            if(event->type==SDL_EVENT_FINGER_DOWN&&y>=viewport.y+viewport.h) {
                int action=m8_ui_pointer(&pad->ui,x,y);
                if(action==1)pad->settings_requested=true;
                if(action==2)pad->about_requested=true;
                pad->dirty=true;
            } else if (pad->controls_visible && event->type == SDL_EVENT_FINGER_DOWN)
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
            handled = false;
        }
        break;
    }
    if (m8_keypad_held(&pad->keys) != before) {
        m8_bridge_keypad(m8_keypad_held(&pad->keys));
        pad->dirty = true;
    }
    return handled;
}

bool m8_keypad_window_draw(m8_keypad_window *pad,uint8_t external_held) {
    if(!pad->renderer)return true;
    SDL_Renderer *renderer=pad->renderer;
    SDL_Texture *target=SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer,NULL);
    SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderViewport(renderer,NULL);
    m8_text_ui *ui=&pad->ui;
    if(!m8_ui_begin(ui,renderer,pad->theme))return false;
    int w=0,h=0;SDL_GetRenderOutputSize(renderer,&w,&h);
    float scale=ui->display_scale;
    bool fullscreen=m8_render_fullscreen(pad->window);
    float aspect=screen_aspect(pad);
    if(!fullscreen&&SDL_fabsf(aspect-pad->screen_aspect)>.0001f) {
        int width=0;SDL_GetWindowSize(pad->window,&width,NULL);
        float keys_ratio=pad->controls_visible?(float)M8_KEYPAD_HEIGHT/M8_KEYPAD_WIDTH:0;
        int height=(int)SDL_ceilf(width*(1/aspect+keys_ratio+pad->footer_ratio));
        SDL_SetWindowAspectRatio(pad->window,(float)width/height,(float)width/height);
        SDL_SetWindowSize(pad->window,width,height);
        pad->screen_aspect=aspect;
    }
    SDL_FRect keys=m8_keypad_window_viewport(pad);
    float density=SDL_GetWindowPixelDensity(pad->window);
    keys.x*=density;keys.y*=density;keys.w*=density;keys.h*=density;
    SDL_FRect screen={keys.x,0,keys.w,keys.y};
    if(fullscreen) {
        float aspect=screen_aspect(pad);
        screen.w=SDL_min((float)w,h*aspect);screen.h=screen.w/aspect;
        screen.x=(w-screen.w)/2;screen.y=(h-screen.h)/2;
        SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
    }
    SDL_SetRenderScale(renderer,1,1);
    SDL_Texture *texture=m8_render_screen_texture(renderer);
    if(pad->resetting) {
        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderFillRect(renderer,&screen);
    } else if(texture&&!SDL_RenderTexture(renderer,texture,NULL,&screen))return false;
    if(!fullscreen&&pad->controls_visible&&keys.w>=1&&keys.h>=1&&
       !m8_keypad_render(&pad->drawing,&pad->keys,renderer,&keys,external_held,pad->theme))return false;
    SDL_SetRenderScale(renderer,scale,scale);
    if(!fullscreen) {
        if(pad->controls_visible&&pad->hints) {
            const uint8_t codes[]={M8_KEYPAD_UP,M8_KEYPAD_OPTION,M8_KEYPAD_EDIT,M8_KEYPAD_LEFT,
                M8_KEYPAD_DOWN,M8_KEYPAD_RIGHT,M8_KEYPAD_SHIFT,M8_KEYPAD_PLAY};
            const char *const labels[]={"↑","a","s","←","↓","→","z","x"};
            SDL_FRect logical={keys.x/scale,keys.y/scale,keys.w/scale,keys.h/scale};
            for(unsigned i=0;i<SDL_arraysize(codes);i++) {
                SDL_FRect rect;if(m8_keypad_button_rect(&logical,codes[i],&rect))
                    m8_ui_text(ui,rect.x+rect.w/2-4,rect.y+rect.h/2-8,labels[i],ui->colors.foreground);
            }
        }
        /* Scale footer text and its hit targets with the fixed window shape.
         * This keeps labels inside the footer when the user makes it smaller. */
        float footer_scale=footer_height(pad)/(28*m8_ui_point_scale(pad->window));
        ui->point_scale*=footer_scale;ui->width/=footer_scale;ui->height/=footer_scale;
        scale*=footer_scale;
        SDL_SetRenderScale(renderer,scale,scale);
        char speed[64];
        if(pad->resetting)SDL_strlcpy(speed,"resetting M8",sizeof speed);
        else if(pad->realtime_ratio>0)SDL_snprintf(speed,sizeof speed,"running at %.2fx realtime",pad->realtime_ratio);
        else SDL_strlcpy(speed,"running at --x realtime",sizeof speed);
        float y=(keys.y+keys.h)/scale+4;
        m8_ui_text(ui,8,y,speed,ui->colors.muted);
        float about=m8_ui_measure(ui,"> about <"),settings=m8_ui_measure(ui,"> settings <");
        m8_ui_button(ui,1,ui->width-about-settings-33,y,"> settings <",false,true);
        m8_ui_button(ui,2,ui->width-about-8,y,"> about <",false,true);
    }
    bool ok=m8_render_finish(renderer);
    SDL_SetRenderScale(renderer,1,1);
    SDL_SetRenderTarget(renderer,target);
    pad->dirty=false;pad->drawn_held=external_held|m8_keypad_held(&pad->keys);
    pad->output_w=w;pad->output_h=h;
    return ok;
}
bool m8_keypad_window_focused(const m8_keypad_window *pad) {
    return pad->display&&SDL_GetKeyboardFocus()==pad->display;
}
void m8_keypad_window_close(m8_keypad_window *pad) {
#ifdef __APPLE__
    m8_native_fullscreen_unwatch(pad->native_fullscreen_observer);pad->native_fullscreen_observer=NULL;
#endif
    m8_keypad_window_release(pad);
    m8_keypad_renderer_destroy(&pad->drawing);m8_ui_destroy(&pad->ui);
    m8_render_embed(NULL);
    if(pad->owns_renderer)SDL_DestroyRenderer(pad->renderer);
    pad->renderer=NULL;pad->window=NULL;
}
