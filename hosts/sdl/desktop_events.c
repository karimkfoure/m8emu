#include "desktop_events.h"
#include "bridge.h"
#include "common.h"
#include "input.h"
#include "backends/m8.h"

void m8_desktop_release(m8_keypad_window *pad,struct app_context *ctx) {
    m8_keypad_window_release(pad);
    if(!m8_bridge_host_held())return;
    /* m8c remembers controller bits independently of the bridge. Clear that
     * state through ordered releases so a menu key-up cannot leave EDIT held. */
    const unsigned keys[]={ctx->conf.key_up,ctx->conf.key_down,ctx->conf.key_left,ctx->conf.key_right,
        ctx->conf.key_select,ctx->conf.key_select_alt,ctx->conf.key_start,ctx->conf.key_start_alt,
        ctx->conf.key_opt,ctx->conf.key_opt_alt,ctx->conf.key_edit,ctx->conf.key_edit_alt};
    for(unsigned i=0;i<SDL_arraysize(keys);i++) {
        SDL_Event release={.type=SDL_EVENT_KEY_UP};release.key.scancode=(SDL_Scancode)keys[i];
        input_handle_key_up_event(ctx,&release);
    }
    m8_send_msg_controller(0);
}
bool m8_desktop_event(m8_settings_window *settings,struct app_context *ctx,const SDL_Event *event) {
    m8_keypad_window *pad=settings->keypad;
    if(!pad)return false;
    if(settings->window&&event->type==SDL_EVENT_KEY_DOWN&&!event->key.repeat&&
       event->key.scancode==ctx->conf.key_toggle_settings) {
        m8_desktop_release(pad,ctx);m8_settings_show(settings);return true;
    }
    if(m8_settings_event(settings,event)) {
        if(event->type==SDL_EVENT_KEY_DOWN||event->type==SDL_EVENT_WINDOW_FOCUS_GAINED)m8_desktop_release(pad,ctx);
        return true;
    }
    if(m8_keypad_window_event(pad,event)) {
        if(pad->settings_requested){pad->settings_requested=false;m8_desktop_release(pad,ctx);m8_settings_show(settings);}
        if(pad->about_requested){pad->about_requested=false;m8_desktop_release(pad,ctx);m8_about_show(settings);}
        return true;
    }
    return false;
}
