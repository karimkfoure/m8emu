#include "settings_window.h"
#include <string.h>

enum { FULLSCREEN=1,EXPLORER,RESET,THEME_PREV,THEME_NEXT,CONTROLS,HINTS,AUDIO_PREV,AUDIO_NEXT,GITHUB,
       CANCEL=20,CONFIRM,CONTROLS_NEXT,HINTS_NEXT };
static void save(m8_settings_window *s) {
    if(!m8_preferences_save(&s->keypad->preferences))
        SDL_strlcpy(s->error,"Cannot remember settings. Check the preference folder.",sizeof s->error);
}
void m8_settings_show(m8_settings_window *s) {
    if(s->window){s->dirty=true;SDL_ShowWindow(s->window);SDL_RaiseWindow(s->window);}
}
void m8_settings_open(m8_settings_window *s,SDL_Window *display,m8_keypad_window *keypad) {
    memset(s,0,sizeof *s);s->dirty=true;s->display=display;s->keypad=keypad;s->realtime=-1;
    const char *driver=SDL_GetCurrentVideoDriver();
    if(!display||(driver&&(!strcmp(driver,"kmsdrm")||!strcmp(driver,"android"))))return;
    float scale=m8_ui_point_scale(display);int width=(int)(440*scale),height=(int)(470*scale);
    s->window=SDL_CreateWindow("m8emu",width,height,SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_RESIZABLE);
    if(s->window)s->renderer=SDL_CreateRenderer(s->window,NULL);
    if(!s->window||!s->renderer){m8_settings_close(s);return;}
    SDL_SetRenderVSync(s->renderer,0);
    SDL_SetWindowMinimumSize(s->window,(int)(340*scale),(int)(420*scale));
    m8_preferences *p=&keypad->preferences;
    int x=0,y=0,w=0;SDL_GetWindowPosition(display,&x,&y);SDL_GetWindowSize(display,&w,NULL);
    SDL_Rect bounds;
    if(p->positioned){x=p->settings_x;y=p->settings_y;}else x+=w+16;
    if(SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(display),&bounds)) {
        x=SDL_clamp(x,bounds.x,bounds.x+SDL_max(0,bounds.w-width));
        y=SDL_clamp(y,bounds.y,bounds.y+SDL_max(0,bounds.h-height));
    }
    SDL_SetWindowPosition(s->window,x,y);SDL_strlcpy(s->audio_name,"default",sizeof s->audio_name);
    if(p->audio[0])SDL_strlcpy(s->audio_name,p->audio,sizeof s->audio_name);
}
static void audio(m8_settings_window *s,int step) {
    int count=0,at=0;SDL_AudioDeviceID *devices=SDL_GetAudioPlaybackDevices(&count);
    for(int i=0;i<count;i++)if(devices[i]==s->audio_device)at=i+1;
    at=(at+step+count+1)%(count+1);s->audio_device=at?devices[at-1]:0;
    const char *name=at?SDL_GetAudioDeviceName(s->audio_device):"default";
    SDL_strlcpy(s->audio_name,name?name:"default",sizeof s->audio_name);
    SDL_strlcpy(s->keypad->preferences.audio,at?s->audio_name:"",sizeof s->keypad->preferences.audio);
    s->action=M8_SETTINGS_AUDIO;SDL_free(devices);save(s);
}
static void activate(m8_settings_window *s,int id,int step) {
    m8_keypad_window *pad=s->keypad;
    switch(id) {
    case FULLSCREEN:
        if(!SDL_SetWindowFullscreen(s->display,!(SDL_GetWindowFlags(s->display)&SDL_WINDOW_FULLSCREEN)))
            SDL_strlcpy(s->error,SDL_GetError(),sizeof s->error);
        break;
    case EXPLORER:s->action=M8_SETTINGS_EXPLORER;break;
    case RESET:s->dialog=1;s->ui.focus=CANCEL;break;
    case THEME_PREV:case THEME_NEXT:
        m8_keypad_window_theme(pad,(M8KeypadTheme)((pad->theme+(step?step:id==THEME_PREV?-1:1)+5)%5));save(s);break;
    case CONTROLS:case CONTROLS_NEXT:m8_keypad_window_visible(pad,!pad->window);pad->preferences.controls=pad->window!=NULL;save(s);break;
    case HINTS:case HINTS_NEXT:pad->hints=!pad->hints;pad->preferences.hints=pad->hints;pad->dirty=true;save(s);break;
    case AUDIO_PREV:case AUDIO_NEXT:audio(s,step?step:id==AUDIO_PREV?-1:1);break;
    case GITHUB:SDL_OpenURL("https://github.com/karimkfoure/m8emu");break;
    case CANCEL:s->dialog=0;s->ui.focus=RESET;break;
    case CONFIRM:s->dialog=0;s->action=M8_SETTINGS_RESET;break;
    }
}
bool m8_settings_event(m8_settings_window *s,const SDL_Event *e) {
    if(e->type==SDL_EVENT_KEY_DOWN && !e->key.repeat &&
       (e->key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI))&&e->key.key==SDLK_COMMA) {
        m8_settings_show(s);return true;
    }
    if(!s->window)return false;
    if(e->type==SDL_EVENT_AUDIO_DEVICE_REMOVED && !e->adevice.recording&&s->audio_device==e->adevice.which) {
        s->audio_device=0;SDL_strlcpy(s->audio_name,"default",sizeof s->audio_name);
        s->keypad->preferences.audio[0]=0;s->action=M8_SETTINGS_AUDIO;save(s);
        SDL_strlcpy(s->error,"Audio output removed; using default.",sizeof s->error);return true;
    }
    SDL_WindowID id=SDL_GetWindowID(s->window);
    s->dirty=true;
    if(e->type>=SDL_EVENT_WINDOW_FIRST&&e->type<=SDL_EVENT_WINDOW_LAST&&e->window.windowID==id) {
        if(e->type==SDL_EVENT_WINDOW_CLOSE_REQUESTED)SDL_HideWindow(s->window);
        if(e->type==SDL_EVENT_WINDOW_MOVED) {
            m8_preferences *p=&s->keypad->preferences;
            p->settings_x=e->window.data1;p->settings_y=e->window.data2;p->positioned=true;save(s);
        }
        return true;
    }
    if(e->type==SDL_EVENT_MOUSE_MOTION&&e->motion.windowID==id) {
        s->dirty|=m8_ui_motion(&s->ui,e->motion.x,e->motion.y);return true;
    }
    if(e->type==SDL_EVENT_MOUSE_BUTTON_DOWN&&e->button.windowID==id) {
        activate(s,m8_ui_pointer(&s->ui,e->button.x,e->button.y),0);return true;
    }
    if((e->type==SDL_EVENT_KEY_DOWN||e->type==SDL_EVENT_KEY_UP)&&e->key.windowID==id) {
        m8_ui_command command;
        if(e->type==SDL_EVENT_KEY_DOWN&&m8_ui_key(&e->key,&command)) {
            if(command==M8_UI_BACK){if(s->dialog){s->dialog=0;s->ui.focus=RESET;}else SDL_HideWindow(s->window);}
            else {int target=m8_ui_input(&s->ui,command);
                bool adjust=command==M8_UI_DECREASE||command==M8_UI_INCREASE;
                bool option=(target>=THEME_PREV&&target<=AUDIO_NEXT)||target==CONTROLS_NEXT||target==HINTS_NEXT;
                if(adjust&&!option)m8_ui_input(&s->ui,command==M8_UI_DECREASE?M8_UI_PREVIOUS:M8_UI_NEXT);
                else if(target)activate(s,target,command==M8_UI_DECREASE?-1:command==M8_UI_INCREASE?1:0);}
        }
        return true;
    }
    return false;
}
void m8_settings_update(m8_settings_window *s,m8_worker *worker,m8live_audio *output,const m8_worker_state *state) {
    if(!s->window||!state->ready)return;
    if(s->sample_epoch!=state->session_epoch){s->sample_epoch=state->session_epoch;s->sample_ns=0;s->realtime=-1;}
    s->retained_ns=state->session_committed_ns;
    s->retained_before_launch=state->session_committed_ns&&!state->session_captures;
    SDL_strlcpy(s->storage_error,state->session_error,sizeof s->storage_error);
    if(!s->audio_initialized) {
        s->audio_initialized=true;
        const char *name=s->keypad->preferences.audio;
        if(*name) {
            int count=0;SDL_AudioDeviceID *devices=SDL_GetAudioPlaybackDevices(&count);
            for(int i=0;i<count;i++) {
                const char *device=SDL_GetAudioDeviceName(devices[i]);
                if(device&&!strcmp(device,name)){s->audio_device=devices[i];break;}
            }
            SDL_free(devices);
            if(!s->audio_device){s->keypad->preferences.audio[0]=0;save(s);SDL_strlcpy(s->error,"Audio output unavailable; using default.",sizeof s->error);SDL_strlcpy(s->audio_name,"default",sizeof s->audio_name);}
            else s->action=M8_SETTINGS_AUDIO;
        }
    }
    if(s->action==M8_SETTINGS_AUDIO) {
        s->action=M8_SETTINGS_NONE;
        if(output&&!strcmp(m8live_driver(output),"simulated"))SDL_strlcpy(s->error,"Audio device selection requires SDL playback.",sizeof s->error);
        else {
            char error[256]={0};m8live_output *prepared=m8live_output_prepare(output,s->audio_device,error,sizeof error);
            if(!prepared)SDL_strlcpy(s->error,error,sizeof s->error);
            else if(m8_worker_output(worker,prepared)<0){m8live_output_discard(prepared);SDL_strlcpy(s->error,"Audio request queue is full. Try again.",sizeof s->error);}
        }
    }
    if(*state->audio_error)SDL_strlcpy(s->error,state->audio_error,sizeof s->error);
    uint64_t now=SDL_GetTicksNS();
    if(state->session_paused){s->realtime=-1;s->sample_ns=0;}
    else if(!s->sample_ns){s->sample_ns=now;s->sample_frames=state->frames;s->sample_service=state->service_seconds;}
    else if(now-s->sample_ns>=500000000) {
        double seconds=state->service_seconds-s->sample_service;
        s->realtime=seconds>0&&state->frames>=s->sample_frames?(state->frames-s->sample_frames)/44100.0/seconds:-1;
        s->sample_ns=now;s->sample_frames=state->frames;s->sample_service=state->service_seconds;
    }
}
static void option(m8_text_ui *u,float y,const char *label,const char *value,int previous,int next) {
    float x=m8_ui_text(u,20,y,label,u->colors.muted);
    m8_ui_button(u,previous,x,y,"<",false,true);x+=2*u->cell;
    x=m8_ui_text(u,x,y,value,u->colors.foreground)+u->cell;
    m8_ui_button(u,next,x,y,">",false,true);
}
bool m8_settings_draw(m8_settings_window *s) {
    if(!s->renderer||(SDL_GetWindowFlags(s->window)&SDL_WINDOW_HIDDEN))return true;
    uint64_t now=SDL_GetTicksNS();
    if(!s->dirty&&now<s->draw_after&&s->drawn_theme==s->keypad->theme)return true;
    s->dirty=false;s->draw_after=now+500000000;s->drawn_theme=s->keypad->theme;
    m8_text_ui *u=&s->ui;if(!m8_ui_begin(u,s->renderer,s->keypad->theme))return false;
    if(s->dialog) {
        m8_ui_text(u,20,16,"reset M8",u->colors.muted);
        float end=m8_ui_wrap(u,20,48,u->width-40,"This erases the SD card and M8 settings, then starts with an empty card. This cannot be undone.",u->colors.muted);
        m8_ui_button(u,CANCEL,20,end+16,"> cancel <",false,true);
        m8_ui_danger_button(u,CONFIRM,u->width-20-m8_ui_measure(u,"> reset M8 <"),end+16,"> reset M8 <",false);
    } else {
        const float row=30.6f;float y=16;u->line_height=row;
        m8_ui_button(u,FULLSCREEN,20,y,"> full screen <",false,true);y+=row;
        m8_ui_button(u,EXPLORER,20,y,"> SD card explorer <",false,true);y+=row;
        m8_ui_button(u,RESET,20,y,"> reset M8 <",false,true);y+=row+7.65f;
        u->line_height=M8_UI_LINE;
        m8_ui_text(u,20,y,"settings",u->colors.muted);y+=M8_UI_LINE;u->line_height=row;
        option(u,y,"└ theme ",m8_ui_theme_name(s->keypad->theme),THEME_PREV,THEME_NEXT);y+=row;
        option(u,y,"└ controls ",s->keypad->window?"show":"hide",CONTROLS,CONTROLS_NEXT);y+=row;
        if(s->keypad->window){option(u,y,"  └ keyboard hints ",s->keypad->hints?"show":"hide",HINTS,HINTS_NEXT);y+=row;}
        char name[40];SDL_strlcpy(name,s->audio_name,sizeof name);
        option(u,y,"└ audio output ",name,AUDIO_PREV,AUDIO_NEXT);
        u->line_height=M8_UI_LINE;
        bool wide=u->width>=660;float status_width=wide?u->width/2-40:u->width-40;
        char status[100];
        bool error=*s->storage_error||*s->error;
        if(error)SDL_strlcpy(status,*s->storage_error?s->storage_error:s->error,sizeof status);
        else {
            uint64_t seconds=s->retained_ns?(SDL_GetTicksNS()-s->retained_ns)/1000000000:0;
            if(!s->retained_ns)SDL_strlcpy(status,"session not retained yet",sizeof status);
            else if(s->retained_before_launch)SDL_strlcpy(status,"saved before this launch",sizeof status);
            else if(seconds<60)SDL_strlcpy(status,"saved just now",sizeof status);
            else if(seconds<3600)SDL_snprintf(status,sizeof status,"saved %llu min ago",(unsigned long long)(seconds/60));
            else SDL_snprintf(status,sizeof status,"saved %llu hr ago",(unsigned long long)(seconds/3600));
        }
        float status_height=error?m8_ui_wrap_height(u,status_width,status):M8_UI_LINE;
        float footer=u->height-16-status_height-(wide?M8_UI_LINE:3*M8_UI_LINE+17);
        if(error)m8_ui_wrap(u,20,footer,status_width,status,u->colors.warning);
        else m8_ui_text(u,20,footer,status,u->colors.muted);
        if(s->realtime>=0) {
            SDL_snprintf(status,sizeof status,"(running at %.1fx realtime)",s->realtime);
            m8_ui_text(u,20,footer+status_height,status,s->realtime<1?u->colors.warning:u->colors.muted);
        }
        const char *version="m8emu v0.2.0",*github="github.com/karimkfoure/m8emu";
        if(wide) {
            m8_ui_text(u,u->width-20-m8_ui_measure(u,version),footer,version,u->colors.muted);
            m8_ui_link(u,GITHUB,u->width-20-m8_ui_measure(u,github),footer+M8_UI_LINE,github,true,true);
        } else {
            float line=footer+status_height+M8_UI_LINE+8;
            m8_ui_line(u,20,line,u->width-20,m8_ui_mix(u->colors.muted,u->colors.page,.4f));
            m8_ui_text(u,20,line+9,version,u->colors.muted);
            m8_ui_link(u,GITHUB,20,line+9+M8_UI_LINE,github,true,true);
        }
    }
    return SDL_RenderPresent(s->renderer);
}
m8_settings_action m8_settings_take_action(m8_settings_window *s) {
    m8_settings_action action=s->action;s->action=M8_SETTINGS_NONE;return action;
}
void m8_settings_close(m8_settings_window *s) {
    m8_ui_destroy(&s->ui);SDL_DestroyRenderer(s->renderer);SDL_DestroyWindow(s->window);
    s->renderer=NULL;s->window=NULL;
}
