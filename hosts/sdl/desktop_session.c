#include "desktop_session.h"
#include "desktop_events.h"
#include "checkpoint.h"
#include "bridge.h"
#include "common.h"
#include "backends/m8.h"
#include <string.h>

enum {OPEN=1,RESET,COMMIT,RESUME};
static void error(m8_settings_window *settings,const char *text) {
    SDL_strlcpy(settings->error,text,sizeof settings->error);settings->dirty=true;m8_settings_show(settings);
}
static void start(m8_desktop_session *d,m8_settings_window *settings,struct app_context *ctx,int intent) {
    if(d->intent||d->awaiting)return;
    if(d->explorer.window) {
        if(intent==OPEN){SDL_ShowWindow(d->explorer.window);SDL_RaiseWindow(d->explorer.window);}
        else error(settings,"Close the SD card explorer before resetting the M8.");
        return;
    }
    m8_desktop_release(settings->keypad,ctx);d->intent=intent;d->retry_after=0;
}
static bool guest_event(m8_settings_window *s,const SDL_Event *e) {
    SDL_WindowID display=s->display?SDL_GetWindowID(s->display):0,pad=s->keypad&&s->keypad->window?SDL_GetWindowID(s->keypad->window):0,id=0;
    switch(e->type) {
    case SDL_EVENT_KEY_DOWN:case SDL_EVENT_KEY_UP:id=e->key.windowID;break;
    case SDL_EVENT_MOUSE_MOTION:id=e->motion.windowID;break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:case SDL_EVENT_MOUSE_BUTTON_UP:id=e->button.windowID;break;
    case SDL_EVENT_MOUSE_WHEEL:id=e->wheel.windowID;break;
    case SDL_EVENT_FINGER_DOWN:case SDL_EVENT_FINGER_UP:case SDL_EVENT_FINGER_MOTION:case SDL_EVENT_FINGER_CANCELED:id=e->tfinger.windowID;break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:case SDL_EVENT_GAMEPAD_BUTTON_DOWN:case SDL_EVENT_GAMEPAD_BUTTON_UP:return true;
    default:return false;
    }
    return id&&(id==display||id==pad);
}
bool m8_desktop_session_event(m8_desktop_session *d,m8_settings_window *s,struct app_context *ctx,const SDL_Event *e) {
    if(m8_explorer_event(&d->explorer,e))return true;
    if(!s->window)return false;
    if(e->type==SDL_EVENT_KEY_DOWN&&!e->key.repeat&&(e->key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI))&&e->key.key==SDLK_E) {
        start(d,s,ctx,OPEN);return true;
    }
    if(e->type==SDL_EVENT_KEY_DOWN&&(e->key.scancode==ctx->conf.key_toggle_settings||
       ((e->key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI))&&e->key.key==SDLK_COMMA)))return false;
    bool busy=d->intent||d->awaiting||d->explorer.window;
    if(busy&&(e->type==SDL_EVENT_QUIT||(e->type==SDL_EVENT_WINDOW_CLOSE_REQUESTED&&s->display&&e->window.windowID==SDL_GetWindowID(s->display)))) {
        d->quit_requested=true;return true;
    }
    return busy&&guest_event(s,e);
}
static void completed(m8_desktop_session *d,m8_settings_window *s,m8_worker_reply *reply,bool rendered) {
    d->awaiting=false;
    if(reply->result==M8_SESSION_BUSY){d->retry_after=SDL_GetTicksNS()+20000000;return;}
    if(reply->result!=M8_SESSION_OK) {
        const char *message=*reply->error?reply->error:"The session operation could not be completed.";
        error(s,message);
        if(d->intent==COMMIT&&!d->reset){m8_explorer_reopen(&d->explorer,message);d->intent=0;}
        else if(d->intent==COMMIT){m8_fat_close(d->reset_card);d->reset_card=NULL;d->intent=RESUME;d->reset=false;}
        else if(d->intent==RESUME)d->retry_after=SDL_GetTicksNS()+250000000;
        else d->intent=0;
        return;
    }
    if(reply->operation==M8_WORKER_PAUSE) {
        if(d->intent==OPEN) {
            m8rt_session_section sd;m8rt_session_error cause={0};
            if(d->quit_requested)d->intent=RESUME;
            else if(m8rt_session_find(reply->media.data,reply->media.size,M8RT_SESSION_SD,&sd,&cause)||
                    !m8_explorer_open(&d->explorer,s->display,sd.data,sd.size)) {
                error(s,*d->explorer.message?d->explorer.message:"Cannot open the SD card explorer.");d->intent=RESUME;
            } else {
                d->explorer.read_only=reply->read_only;
                if(reply->read_only)SDL_strlcpy(d->explorer.message,"The SD card is read-only.",sizeof d->explorer.message);
                d->intent=0;
            }
        } else {
            m8_fat_error cause;
            if(m8_fat_create(16u*1024*1024,&d->reset_card,&cause)!=M8_FAT_OK){error(s,cause.message);d->intent=RESUME;}
            else {d->reset=true;d->intent=COMMIT;}
        }
    } else if(reply->operation==M8_WORKER_REPLACE) {
        if(m8_bridge_restore_display(reply->screen,rendered)) {
            error(s,"The new card is committed, but the display could not be restored.");m8_bridge_reset_decoder();m8_reset_display();
        } else if(*reply->error)error(s,reply->error);
        m8_explorer_close(&d->explorer);m8_fat_close(d->reset_card);d->reset_card=NULL;d->reset=false;
        d->intent=reply->needs_resume?RESUME:0;
    } else {m8_explorer_close(&d->explorer);d->intent=0;}
}
bool m8_desktop_session_update(m8_desktop_session *d,m8_settings_window *s,struct app_context *ctx,m8_worker *worker,bool rendered) {
    if(!s->window)return false;
    if(s->action==M8_SETTINGS_EXPLORER||s->action==M8_SETTINGS_RESET) {
        m8_settings_action action=m8_settings_take_action(s);start(d,s,ctx,action==M8_SETTINGS_EXPLORER?OPEN:RESET);
    }
    m8_worker_reply reply;
    if(m8_worker_take_reply(worker,&reply)){completed(d,s,&reply,rendered);m8_worker_reply_release(&reply);}
    if(d->quit_requested&&d->explorer.window&&!d->explorer.closed)m8_explorer_request_close(&d->explorer);
    if(!d->intent&&d->explorer.window&&d->explorer.closed) {
        d->reset=false;d->intent=m8_fat_changed(d->explorer.card)?COMMIT:RESUME;
    }
    if(d->intent&&!d->awaiting&&SDL_GetTicksNS()>=d->retry_after) {
        m8_session_result result;
        if(d->intent==OPEN||d->intent==RESET)result=m8_worker_pause(worker);
        else if(d->intent==RESUME)result=m8_worker_resume(worker);
        else {size_t size;const void *bytes=m8_fat_image(d->reset?d->reset_card:d->explorer.card,&size);result=m8_worker_replace(worker,bytes,size,d->reset);}
        if(result==M8_SESSION_OK)d->awaiting=true;
        else if(result==M8_SESSION_BUSY)d->retry_after=SDL_GetTicksNS()+20000000;
        else {reply=(m8_worker_reply){.result=result};completed(d,s,&reply,rendered);}
    }
    return d->quit_requested&&!d->intent&&!d->awaiting&&!d->explorer.window;
}
void m8_desktop_session_close(m8_desktop_session *d) {
    m8_explorer_close(&d->explorer);m8_fat_close(d->reset_card);d->reset_card=NULL;
}
