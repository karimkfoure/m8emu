#include "explorer_window.h"
#include "explorer_files.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

enum { UP=1,ADD,FOLDER,DOWNLOAD,DELETE,ALL,IMAGE,CLOSE,ROOT=20,
       CANCEL=100,ACCEPT,CREATE,FOLDER_INPUT,ENTRY=1000 };
enum { WARNING=1,DELETE_CONFIRM,FOLDER_PROMPT };
enum { PICK_ADD=1,PICK_FILE,PICK_FOLDER,PICK_IMAGE };
typedef struct { Uint32 event;SDL_WindowID window;unsigned token;int kind;char **paths;size_t count;char error[256]; } dialog_result;
static Uint32 dialog_event;
static unsigned next_token;

static void free_paths(char **paths,size_t count) {
    for(size_t i=0;i<count;i++)SDL_free(paths[i]);
    SDL_free(paths);
}
static void SDLCALL picked(void *context,const char *const *files,int filter) {
    (void)filter;dialog_result *result=context;
    if(!files)SDL_strlcpy(result->error,SDL_GetError(),sizeof result->error);
    else {
        while(files[result->count])result->count++;
        result->paths=SDL_calloc(result->count,sizeof *result->paths);
        if(result->count&&!result->paths)result->count=0;
        for(size_t i=0;i<result->count;i++) {
            result->paths[i]=SDL_strdup(files[i]);
            if(!result->paths[i]) {
                free_paths(result->paths,i);result->paths=NULL;result->count=0;
                SDL_strlcpy(result->error,"Cannot allocate file selection.",sizeof result->error);break;
            }
        }
    }
    SDL_Event event={.type=result->event};event.user.windowID=result->window;event.user.data1=result;
    if(!SDL_PushEvent(&event)){free_paths(result->paths,result->count);SDL_free(result);}
}
static void picker(m8_explorer_window *e,int kind) {
    if(e->dialog_pending)return;
    dialog_result *result=SDL_calloc(1,sizeof *result);
    if(!result){SDL_strlcpy(e->message,"Cannot allocate file dialog.",sizeof e->message);return;}
    result->event=dialog_event;result->window=SDL_GetWindowID(e->window);result->token=e->token;result->kind=kind;
    e->dialog_pending=true;
    if(kind==PICK_ADD)SDL_ShowOpenFileDialog(picked,result,e->window,NULL,0,NULL,true);
    else if(kind==PICK_FOLDER)SDL_ShowOpenFolderDialog(picked,result,e->window,NULL,false);
    else {
        const char *name="SD-card.img";m8_fat_entry entry;
        if(kind==PICK_FILE)for(size_t i=0;i<e->count;i++)if(e->selected[i]&&m8_fat_list(e->card,e->path,i,&entry)==M8_FAT_OK){name=entry.name;break;}
        SDL_ShowSaveFileDialog(picked,result,e->window,NULL,0,name);
    }
}
static void refresh(m8_explorer_window *e) {
    e->dirty=true;
    m8_fat_entry entry;
    if(m8_fat_stat(e->card,e->path,&entry)!=M8_FAT_OK){SDL_strlcpy(e->message,m8_fat_last_error(e->card)->message,sizeof e->message);return;}
    SDL_free(e->selected);e->count=entry.children;e->selected=SDL_calloc(e->count,sizeof *e->selected);
    if(e->count&&!e->selected){e->count=0;SDL_strlcpy(e->message,"Cannot allocate selection.",sizeof e->message);}
    e->scroll=0;
}
static size_t selection(m8_explorer_window *e) {
    size_t count=0;for(size_t i=0;i<e->count;i++)if(e->selected[i])count++;return count;
}
static void navigate(m8_explorer_window *e,const char *path) {
    SDL_strlcpy(e->path,path,sizeof e->path);refresh(e);e->ui.focus=ENTRY;e->message[0]=0;
}
static void up(m8_explorer_window *e) {
    char path[4096];SDL_strlcpy(path,e->path,sizeof path);char *slash=strrchr(path,'/');
    if(slash)*slash=0;else *path=0;navigate(e,path);
}
static size_t file_count(m8_fat_card *card,const char *path) {
    m8_fat_entry entry;if(m8_fat_stat(card,path,&entry)!=M8_FAT_OK)return 0;
    if(!entry.directory)return 1;
    size_t count=0;
    for(size_t i=0;i<entry.children;i++) {
        m8_fat_entry child;if(m8_fat_list(card,path,i,&child)==M8_FAT_OK)count+=file_count(card,child.path);
    }
    return count;
}
void m8_explorer_request_close(m8_explorer_window *e) {
    if(e->dialog_pending)return;
    e->closed=true;e->dialog=0;SDL_StopTextInput(e->window);SDL_HideWindow(e->window);
}
bool m8_explorer_open(m8_explorer_window *e,SDL_Window *parent,const void *bytes,size_t size) {
    if(e->window){SDL_ShowWindow(e->window);SDL_RaiseWindow(e->window);return true;}
    memset(e,0,sizeof *e);m8_fat_error error;
    if(m8_fat_open(bytes,size,&e->card,&error)!=M8_FAT_OK){SDL_strlcpy(e->message,error.message,sizeof e->message);return false;}
    if(!dialog_event)dialog_event=SDL_RegisterEvents(1);
    e->token=++next_token;
    float scale=parent?m8_ui_point_scale(parent):1;
    e->window=SDL_CreateWindow("SD card explorer",(int)(640*scale),(int)(520*scale),SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if(e->window)e->renderer=SDL_CreateRenderer(e->window,NULL);
    if(!e->window||!e->renderer){SDL_strlcpy(e->message,SDL_GetError(),sizeof e->message);m8_explorer_close(e);return false;}
    SDL_SetWindowMinimumSize(e->window,(int)(560*scale),(int)(420*scale));SDL_SetRenderVSync(e->renderer,0);
    if(parent){int x,y;SDL_GetWindowPosition(parent,&x,&y);SDL_SetWindowPosition(e->window,x+32,y+32);}
    refresh(e);SDL_RaiseWindow(e->window);return true;
}
static void mutate(m8_explorer_window *e,int pending) {
    e->dialog=0;e->pending=0;e->message[0]=0;
    if(pending==ADD) {
        if(!m8_explorer_add(e->card,e->path,(const char *const *)e->additions,e->addition_count,e->message,sizeof e->message))e->failed=true;
        free_paths(e->additions,e->addition_count);e->additions=NULL;e->addition_count=0;refresh(e);
    } else if(pending==CREATE) {
        char path[4096];int n=SDL_snprintf(path,sizeof path,"%s%s%s",e->path,*e->path?"/":"",e->folder_name);
        if(!*e->folder_name||strpbrk(e->folder_name,"/\\")||!strcmp(e->folder_name,".")||!strcmp(e->folder_name,".."))
            SDL_strlcpy(e->message,"Use one folder name without / or \\.",sizeof e->message);
        else if(n<0||(size_t)n>=sizeof path)SDL_strlcpy(e->message,"Path is too long.",sizeof e->message);
        else if(m8_fat_mkdir(e->card,path,(m8_fat_time){0},128u*1024*1024)!=M8_FAT_OK)
            SDL_strlcpy(e->message,m8_fat_last_error(e->card)->message,sizeof e->message);
        else {e->folder_name[0]=0;refresh(e);}
        SDL_StopTextInput(e->window);
    } else if(pending==DELETE) {
        /* Removing from the end preserves earlier list indices. */
        size_t total=selection(e),removed=0;
        for(size_t i=e->count;i>0;i--)if(e->selected[i-1]) {
            m8_fat_entry entry;
            if(m8_fat_list(e->card,e->path,i-1,&entry)!=M8_FAT_OK||m8_fat_remove(e->card,entry.path)!=M8_FAT_OK) {
                SDL_snprintf(e->message,sizeof e->message,"Deleted %zu of %zu items. %s",removed,total,m8_fat_last_error(e->card)->message);break;
            }
            removed++;
        }
        refresh(e);
    }
}
static size_t previous_character(const char *text,size_t at) {
    if(at){do{at--;}while(at&&((unsigned char)text[at]&0xc0)==0x80);}return at;
}
static size_t next_character(const char *text,size_t at) {
    if(text[at]){at++;while(((unsigned char)text[at]&0xc0)==0x80)at++;}return at;
}
static void insert_text(m8_explorer_window *e,const char *text) {
    size_t size=e->folder_select_all?0:strlen(e->folder_name),n=strlen(text);
    if(n>=sizeof e->folder_name-size){SDL_strlcpy(e->message,"Folder name is too long.",sizeof e->message);return;}
    if(e->folder_select_all){e->folder_name[0]=0;e->folder_cursor=0;e->folder_select_all=false;}
    memmove(e->folder_name+e->folder_cursor+n,e->folder_name+e->folder_cursor,size-e->folder_cursor+1);
    memcpy(e->folder_name+e->folder_cursor,text,n);e->folder_cursor+=n;
}
static void permit(m8_explorer_window *e,int pending) {
    if(e->warned)mutate(e,pending);
    else {e->pending=pending;e->dialog=WARNING;e->ui.focus=CANCEL;SDL_StopTextInput(e->window);}
}
static void activate(m8_explorer_window *e,int id,m8_ui_command command) {
    if(e->read_only&&(id==ADD||id==FOLDER||id==DELETE||id==CREATE))return;
    if(id>=ENTRY) {
        size_t index=(size_t)(id-ENTRY)/2;
        if(index>=e->count)return;
        if((id-ENTRY)%2==0)e->selected[index]=!e->selected[index];
        else {m8_fat_entry entry;if(m8_fat_list(e->card,e->path,index,&entry)==M8_FAT_OK) {
            if(entry.directory){char path[4096];SDL_strlcpy(path,entry.path,sizeof path);navigate(e,path);}
            else e->selected[index]=!e->selected[index];
        }}
        return;
    }
    if(id>=ROOT&&id<100) {
        char path[4096];SDL_strlcpy(path,e->path,sizeof path);
        if(id==ROOT)*path=0;
        else {int part=ROOT+1;for(char *p=path;*p;p++)if(*p=='/'&&part++==id){*p=0;break;}}
        navigate(e,path);return;
    }
    (void)command;
    switch(id) {
    case UP:up(e);break;
    case ADD:picker(e,PICK_ADD);break;
    case FOLDER:if(e->dialog==FOLDER_PROMPT){e->dialog=0;SDL_StopTextInput(e->window);break;}e->dialog=FOLDER_PROMPT;e->ui.focus=FOLDER_INPUT;e->folder_name[0]=0;e->folder_cursor=0;e->folder_select_all=false;SDL_StartTextInput(e->window);break;
    case DOWNLOAD: {
        size_t count=selection(e);if(!count)break;
        int kind=PICK_FILE;m8_fat_entry entry;
        for(size_t i=0;i<e->count;i++)if(e->selected[i]&&m8_fat_list(e->card,e->path,i,&entry)==M8_FAT_OK&&entry.directory)kind=PICK_FOLDER;
        picker(e,count==1?kind:PICK_FOLDER);break;
    }
    case DELETE:if(selection(e)){e->dialog=DELETE_CONFIRM;e->ui.focus=CANCEL;}break;
    case ALL: {bool all=selection(e)==e->count;for(size_t i=0;i<e->count;i++)e->selected[i]=!all;break;}
    case IMAGE:picker(e,PICK_IMAGE);break;
    case CLOSE:m8_explorer_request_close(e);break;
    case CANCEL:
        e->dialog=0;e->pending=0;SDL_StopTextInput(e->window);
        free_paths(e->additions,e->addition_count);e->additions=NULL;e->addition_count=0;e->ui.focus=ADD;break;
    case FOLDER_INPUT:SDL_StartTextInput(e->window);break;
    case CREATE:permit(e,CREATE);break;
    case ACCEPT:if(e->dialog==DELETE_CONFIRM)permit(e,DELETE);
        else if(e->dialog==WARNING){e->warned=true;mutate(e,e->pending);}
        break;
    }
}
static void dialog_done(m8_explorer_window *e,dialog_result *result) {
    e->dialog_pending=false;e->dirty=true;
    if(*result->error)SDL_strlcpy(e->message,result->error,sizeof e->message);
    else if(result->count) {
        if(result->kind==PICK_ADD) {
            free_paths(e->additions,e->addition_count);e->additions=result->paths;e->addition_count=result->count;
            result->paths=NULL;result->count=0;permit(e,ADD);
        } else if(result->kind==PICK_IMAGE) {
            size_t size;const void *bytes=m8_fat_image(e->card,&size);
            m8_explorer_write_file(result->paths[0],bytes,size,false,e->message,sizeof e->message);
        } else for(size_t i=0;i<e->count;i++)if(e->selected[i]) {
            m8_fat_entry entry;if(m8_fat_list(e->card,e->path,i,&entry)!=M8_FAT_OK)break;
            if(result->kind==PICK_FILE) {
                const void *bytes;size_t size;
                if(m8_fat_read(e->card,entry.path,&bytes,&size)==M8_FAT_OK)
                    m8_explorer_write_file(result->paths[0],bytes,size,false,e->message,sizeof e->message);
            } else if(!m8_explorer_export(e->card,entry.path,result->paths[0],e->message,sizeof e->message))break;
        }
    }
}
bool m8_explorer_event(m8_explorer_window *e,const SDL_Event *event) {
    if(dialog_event&&event->type==dialog_event) {
        dialog_result *result=event->user.data1;
        if(e->window&&result->token==e->token)dialog_done(e,result);
        free_paths(result->paths,result->count);SDL_free(result);return true;
    }
    if(!e->window||e->closed)return false;
    e->dirty=true;
    SDL_WindowID id=SDL_GetWindowID(e->window);
    if(event->type>=SDL_EVENT_WINDOW_FIRST&&event->type<=SDL_EVENT_WINDOW_LAST&&event->window.windowID==id) {
        if(event->type==SDL_EVENT_WINDOW_CLOSE_REQUESTED)m8_explorer_request_close(e);
        return true;
    }
    if((event->type==SDL_EVENT_DROP_BEGIN||event->type==SDL_EVENT_DROP_POSITION)&&event->drop.windowID==id) {
        e->dragging=!e->read_only&&!e->dialog&&!e->dialog_pending;return true;
    }
    if(event->type==SDL_EVENT_DROP_FILE&&event->drop.windowID==id) {
        if(e->read_only||e->dialog_pending||e->dialog)return true;
        char **paths=SDL_realloc(e->additions,(e->addition_count+1)*sizeof *paths);
        if(paths){e->additions=paths;paths[e->addition_count]=SDL_strdup(event->drop.data);if(paths[e->addition_count])e->addition_count++;}
        return true;
    }
    if(event->type==SDL_EVENT_DROP_COMPLETE&&event->drop.windowID==id) {
        e->dragging=false;
        if(e->addition_count)permit(e,ADD);
        return true;
    }
    if(event->type==SDL_EVENT_MOUSE_WHEEL&&event->wheel.windowID==id) {
        if(event->wheel.y>0)e->scroll=e->scroll>3?e->scroll-3:0;
        else if(event->wheel.y<0)e->scroll=SDL_min(e->scroll+3,e->count?e->count-1:0);
        return true;
    }
    if(event->type==SDL_EVENT_MOUSE_MOTION&&event->motion.windowID==id) {
        m8_ui_motion(&e->ui,event->motion.x,event->motion.y);return true;
    }
    if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN&&event->button.windowID==id) {
        if(!e->dialog_pending)activate(e,m8_ui_pointer(&e->ui,event->button.x,event->button.y),M8_UI_ACTIVATE);
        return true;
    }
    if(event->type==SDL_EVENT_TEXT_INPUT&&event->text.windowID==id) {
        if(e->dialog==FOLDER_PROMPT&&e->ui.focus==FOLDER_INPUT)insert_text(e,event->text.text);
        return true;
    }
    if((event->type==SDL_EVENT_KEY_DOWN||event->type==SDL_EVENT_KEY_UP)&&event->key.windowID==id) {
        if(event->type==SDL_EVENT_KEY_UP||e->dialog_pending)return true;
        SDL_Keycode key=event->key.key;
        if(e->dialog==FOLDER_PROMPT&&e->ui.focus==FOLDER_INPUT) {
            bool control=(event->key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI))!=0;
            if(control&&key==SDLK_A){e->folder_select_all=true;return true;}
            if(control&&key==SDLK_V){char *text=SDL_GetClipboardText();if(text){insert_text(e,text);SDL_free(text);}return true;}
            if(control&&(key==SDLK_C||key==SDLK_X)) {
                if(e->folder_select_all){SDL_SetClipboardText(e->folder_name);if(key==SDLK_X){e->folder_name[0]=0;e->folder_cursor=0;e->folder_select_all=false;}}return true;
            }
            if(key==SDLK_BACKSPACE||key==SDLK_DELETE) {
                size_t size=strlen(e->folder_name),from=e->folder_cursor,to=from;
                if(e->folder_select_all){from=0;to=size;e->folder_select_all=false;}
                else if(key==SDLK_BACKSPACE)from=previous_character(e->folder_name,from);
                else to=next_character(e->folder_name,to);
                memmove(e->folder_name+from,e->folder_name+to,size-to+1);e->folder_cursor=from;return true;
            }
            if(key==SDLK_HOME||key==SDLK_END||key==SDLK_LEFT||key==SDLK_RIGHT) {
                if(key==SDLK_HOME)e->folder_cursor=0;
                else if(key==SDLK_END)e->folder_cursor=strlen(e->folder_name);
                else if(key==SDLK_LEFT)e->folder_cursor=previous_character(e->folder_name,e->folder_cursor);
                else e->folder_cursor=next_character(e->folder_name,e->folder_cursor);
                e->folder_select_all=false;return true;
            }
            if(key==SDLK_RETURN){permit(e,CREATE);return true;}
            if(key==SDLK_SPACE)return true;
        }
        if(!e->dialog) {
            if(key==SDLK_BACKSPACE){up(e);return true;}
            if(key==SDLK_DELETE){activate(e,DELETE,M8_UI_ACTIVATE);return true;}
            if(key==SDLK_A&&(event->key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI))){activate(e,ALL,M8_UI_ACTIVATE);return true;}
            if(key==SDLK_PAGEDOWN||key==SDLK_PAGEUP) {
                size_t rows=SDL_max((size_t)1,e->visible_rows);
                e->scroll=key==SDLK_PAGEUP?(e->scroll>rows?e->scroll-rows:0):SDL_min(e->scroll+rows,e->count?e->count-1:0);return true;
            }
            if(e->ui.focus>=ENTRY&&(key==SDLK_UP||key==SDLK_DOWN)) {
                int index=(e->ui.focus-ENTRY)/2+(key==SDLK_DOWN?1:-1);
                index=SDL_clamp(index,0,e->count?(int)e->count-1:0);e->ui.focus=ENTRY+index*2;
                size_t rows=SDL_max((size_t)1,e->visible_rows);
                if((size_t)index<e->scroll)e->scroll=(size_t)index;
                if((size_t)index>=e->scroll+rows)e->scroll=(size_t)index-rows+1;
                return true;
            }
            if(e->ui.focus>=ENTRY&&key==SDLK_RETURN){activate(e,e->ui.focus|1,M8_UI_ACTIVATE);return true;}
        }
        m8_ui_command command;if(m8_ui_key(&event->key,&command)) {
            if(command==M8_UI_BACK)activate(e,e->dialog?CANCEL:CLOSE,command);
            else if(command==M8_UI_DECREASE||command==M8_UI_INCREASE) {
                if(e->ui.focus>=ENTRY) {
                    if(command==M8_UI_DECREASE)up(e);else activate(e,e->ui.focus|1,command);
                } else m8_ui_input(&e->ui,command==M8_UI_DECREASE?M8_UI_PREVIOUS:M8_UI_NEXT);
            } else {int target=m8_ui_input(&e->ui,command);if(target)activate(e,target,command);}
        }
        return true;
    }
    return false;
}

static void confirmation(m8_explorer_window *e) {
    m8_text_ui *u=&e->ui;char title[80],detail[2048]={0};
    if(e->dialog==WARNING) {
        SDL_strlcpy(title,"Change SD card?",sizeof title);
        SDL_strlcpy(detail,"Changing the card restarts the M8. Edits not saved with PROJECT > SAVE will be lost.",sizeof detail);
    } else {
        size_t count=selection(e);SDL_snprintf(title,sizeof title,"Delete %zu %s?",count,count==1?"item":"items");
        for(size_t i=0;i<e->count;i++)if(e->selected[i]) {
            m8_fat_entry entry;if(m8_fat_list(e->card,e->path,i,&entry)!=M8_FAT_OK)continue;
            char name[320];
            if(entry.directory)SDL_snprintf(name,sizeof name,"%s/ (%zu files)",entry.name,file_count(e->card,entry.path));
            else SDL_strlcpy(name,entry.name,sizeof name);
            if(*detail)SDL_strlcat(detail,", ",sizeof detail);
            SDL_strlcat(detail,name,sizeof detail);
        }
        SDL_strlcat(detail,". This cannot be undone.",sizeof detail);
    }
    float width=SDL_min(44*u->cell,u->width-32),inner=width-42;
    float detail_height=m8_ui_wrap_height(u,inner,detail);
    float height=2+32+M8_UI_LINE+4+detail_height+20+M8_UI_LINE+8;
    height=SDL_min(height,u->height-32);
    float x=(u->width-width)/2,y=(u->height-height)/2;
    SDL_Color backdrop=u->colors.page;backdrop.a=179;m8_ui_fill(u,(SDL_FRect){0,0,u->width,u->height},backdrop);
    m8_ui_fill(u,(SDL_FRect){x,y,width,height},u->colors.page);m8_ui_box(u,(SDL_FRect){x,y,width,height},u->colors.foreground);
    m8_ui_text(u,x+21,y+17,title,u->colors.foreground);
    m8_ui_wrap(u,x+21,y+17+M8_UI_LINE+4,inner,detail,u->colors.muted);
    u->count=0;
    float buttons=y+height-17-M8_UI_LINE-8;
    m8_ui_button(u,CANCEL,x+21,buttons,"cancel",true,true);
    const char *accept=e->dialog==WARNING?"continue":"delete";
    if(e->dialog==WARNING)m8_ui_button(u,ACCEPT,x+width-21-m8_ui_measure(u,accept)-22,buttons,accept,true,true);
    else m8_ui_danger_button(u,ACCEPT,x+width-21-m8_ui_measure(u,accept)-22,buttons,accept,true);
}
bool m8_explorer_draw(m8_explorer_window *e,M8KeypadTheme theme) {
    if(!e->renderer||e->closed)return true;
    if(!e->dirty&&e->drawn_theme==theme)return true;
    e->dirty=false;e->drawn_theme=theme;
    m8_text_ui *u=&e->ui;if(!m8_ui_begin(u,e->renderer,theme))return false;
    u->focus_offset=2;u->colors.muted=m8_ui_mix(u->colors.foreground,u->colors.page,.55f);
    m8_ui_box(u,(SDL_FRect){0,0,u->width,u->height},u->colors.foreground);
    float x=21,y=15;
    if(*e->path)m8_ui_link(u,ROOT,x,y,"SD card",false,true);
    else m8_ui_text(u,x,y,"SD card",u->colors.foreground);
    x+=m8_ui_measure(u,"SD card");
    char path[4096];SDL_strlcpy(path,e->path,sizeof path);int part=ROOT+1;
    for(char *p=path;*p;) {
        char *end=strchr(p,'/');if(end)*end=0;
        if(x+3*u->cell+m8_ui_measure(u,p)>u->width-80)break;
        x=m8_ui_text(u,x,y," / ",u->colors.muted);
        if(end)m8_ui_link(u,part,x,y,p,false,true);else m8_ui_text(u,x,y,p,u->colors.foreground);
        part++;x+=m8_ui_measure(u,p);if(!end)break;p=end+1;
    }
    if(*e->path)m8_ui_link(u,UP,u->width-21-m8_ui_measure(u,"↑ up"),y,"↑ up",false,true);
    size_t count=selection(e);char download[64],remove[64];
    if(count){SDL_snprintf(download,sizeof download,"download (%zu)",count);SDL_snprintf(remove,sizeof remove,"delete (%zu)",count);}
    else {SDL_strlcpy(download,"download",sizeof download);SDL_strlcpy(remove,"delete",sizeof remove);}
    const char *labels[]={"add files","new folder",download,remove};const int ids[]={ADD,FOLDER,DOWNLOAD,DELETE};
    x=21;y=47.4f;
    for(unsigned i=0;i<4;i++) {
        float width=m8_ui_measure(u,labels[i])+22;
        if(x+width>u->width-21&&x>21){x=21;y+=36.4f;}
        if(ids[i]==DELETE&&count&&!e->dialog_pending&&!e->read_only)m8_ui_danger_button(u,ids[i],x,y,labels[i],true);
        else m8_ui_button(u,ids[i],x,y,labels[i],true,!e->dialog_pending&&((i<2&&!e->read_only)||(i==2&&count>0)||(i==3&&count>0&&!e->read_only)));
        x+=width+8;
    }
    float list=y+41.4f;m8_ui_line(u,1,list-1,u->width-1,u->colors.line);
    if(e->dialog==FOLDER_PROMPT) {
        y=list+8;x=m8_ui_text(u,21,y,"folder name ",u->colors.foreground);
        float input_width=24*u->cell;
        size_t start=0,columns=0;
        for(size_t i=0;i<e->folder_cursor;i=next_character(e->folder_name,i))columns++;
        while(columns>23){start=next_character(e->folder_name,start);columns--;}
        SDL_Rect field={(int)x,(int)y,(int)input_width,(int)M8_UI_LINE};SDL_SetRenderClipRect(u->renderer,&field);
        if(e->folder_select_all)m8_ui_fill(u,(SDL_FRect){x,y,input_width,M8_UI_LINE},u->colors.selected);
        m8_ui_text(u,x,y,e->folder_name+start,u->colors.foreground);
        if(u->focus==FOLDER_INPUT)m8_ui_fill(u,(SDL_FRect){x+columns*u->cell,y+3,1,14},u->colors.foreground);
        SDL_SetRenderClipRect(u->renderer,NULL);
        m8_ui_line(u,x,y+M8_UI_LINE,x+input_width,u->colors.line);
        m8_ui_target_add(u,FOLDER_INPUT,(SDL_FRect){x,y,input_width,M8_UI_LINE+1},true);
        float cancel_width=m8_ui_measure(u,"cancel")+22,create_width=m8_ui_measure(u,"create")+22;
        m8_ui_button(u,CREATE,u->width-21-cancel_width-u->cell-create_width,y,"create",true,true);
        m8_ui_button(u,CANCEL,u->width-21-cancel_width,y,"cancel",true,true);
        list=y+36.4f;m8_ui_line(u,1,list-1,u->width-1,u->colors.line);
    }
    float footer=u->height-45.4f;
    float status_height=*e->message?m8_ui_wrap_height(u,u->width-42,e->message):0;
    float bottom=footer-status_height;
    SDL_Rect clip={1,(int)ceilf(list),(int)u->width-2,(int)floorf(bottom-list)};
    SDL_SetRenderClipRect(u->renderer,&clip);
    if(e->dragging)m8_ui_fill(u,(SDL_FRect){1,list,u->width-2,bottom-list},m8_ui_mix(u->colors.accent,u->colors.page,.06f));
    y=list+6;
    m8_ui_check(u,ALL,21,y+3.5f,count>0&&count==e->count,count>0&&count<e->count);
    m8_ui_text(u,51,y,"name",u->colors.muted);y=list+32.4f;
    e->scroll=SDL_min(e->scroll,e->count?e->count-1:0);e->visible_rows=0;
    size_t index=e->scroll;
    for(;index<e->count&&y<bottom;index++) {
        m8_fat_entry entry;if(m8_fat_list(e->card,e->path,index,&entry)!=M8_FAT_OK)break;
        char label[1024];SDL_snprintf(label,sizeof label,"%s%s",entry.name,entry.directory?"/":"");
        float height=m8_ui_wrap_height(u,u->width-72,label)+6;
        SDL_FRect row={1,y,u->width-2,height};
        int select=ENTRY+(int)index*2,open=select+1;
        if(e->selected[index])m8_ui_fill(u,row,u->colors.selected);
        else if(u->hover==select||u->hover==open)m8_ui_fill(u,row,u->colors.hover);
        m8_ui_check(u,select,21,y+6.5f,e->selected[index],false);
        /* Keep the whole row clickable, with focus on its checkbox as on web. */
        if(u->count&&u->targets[u->count-1].id==select)u->targets[u->count-1].rect=row;
        if(entry.directory)m8_ui_link(u,open,51,y+3,label,false,true);
        else m8_ui_wrap(u,51,y+3,u->width-72,label,u->colors.foreground);
        y+=height;e->visible_rows++;
    }
    if(!e->count) {
        const char *first="This folder is empty.",*second="Use add files or drop files here.";
        m8_ui_text(u,(u->width-m8_ui_measure(u,first))/2,y+40,first,u->colors.muted);
        m8_ui_text(u,(u->width-m8_ui_measure(u,second))/2,y+40+M8_UI_LINE,second,u->colors.muted);
    } else if(!e->dragging&&index==e->count) {
        SDL_FRect drop={21,y+12,u->width-42,46.4f};
        m8_ui_dashed(u,drop,m8_ui_mix(u->colors.foreground,u->colors.page,.08f));
        m8_ui_text(u,(u->width-m8_ui_measure(u,"drop files here"))/2,drop.y+13,"drop files here",u->colors.muted);
    }
    if(e->dragging) {
        m8_ui_dashed(u,(SDL_FRect){9,list+8,u->width-18,bottom-list-16},u->colors.accent);
        char label[100];SDL_snprintf(label,sizeof label,"drop to add files to %.64s",*e->path?e->path:"SD card");
        m8_ui_text(u,(u->width-m8_ui_measure(u,label))/2,bottom-20-M8_UI_LINE,label,u->colors.accent);
    }
    SDL_SetRenderClipRect(u->renderer,NULL);
    if(*e->message)m8_ui_wrap(u,21,bottom,u->width-42,e->message,u->colors.danger);
    m8_ui_line(u,1,footer-1,u->width-1,u->colors.line);
    m8_ui_button(u,IMAGE,21,footer+10,"> download SD card <",false,!e->dialog_pending);
    m8_ui_button(u,CLOSE,u->width-21-m8_ui_measure(u,"> close <"),footer+10,"> close <",false,!e->dialog_pending);
    if(e->dialog==WARNING||e->dialog==DELETE_CONFIRM)confirmation(e);
    return SDL_RenderPresent(e->renderer);
}
void m8_explorer_reopen(m8_explorer_window *e,const char *error) {
    e->closed=false;e->dirty=true;SDL_strlcpy(e->message,error,sizeof e->message);SDL_ShowWindow(e->window);SDL_RaiseWindow(e->window);
}
void m8_explorer_close(m8_explorer_window *e) {
    if(e->window)SDL_StopTextInput(e->window);
    m8_ui_destroy(&e->ui);SDL_DestroyRenderer(e->renderer);SDL_DestroyWindow(e->window);
    m8_fat_close(e->card);SDL_free(e->selected);free_paths(e->additions,e->addition_count);
    e->window=NULL;e->renderer=NULL;e->card=NULL;e->selected=NULL;e->additions=NULL;e->addition_count=0;
}
