#include "preferences.h"
#include <stdio.h>
#include <string.h>

static char *path(void) {
    char *directory=SDL_GetPrefPath("","m8emu"),*result=NULL;
    if(directory){SDL_asprintf(&result,"%skeypad.state",directory);SDL_free(directory);}
    return result;
}
void m8_preferences_load(m8_preferences *prefs) {
    *prefs=(m8_preferences){.controls=true,.theme=M8_KEYPAD_THEME_A};
    char *name=path();if(!name)return;
    size_t size=0;char *bytes=SDL_LoadFile(name,&size);SDL_free(name);
    if(!bytes)return;
    /* Preserve the two legacy lines: visibility and canonical keypad theme. */
    if(size&&bytes[0]=='0')prefs->controls=false;
    char *line=strchr(bytes,'\n');
    if(line) {
        char *end=strchr(++line,'\n');if(end)*end=0;
        m8_keypad_theme_parse(line,&prefs->theme);
        line=end?end+1:NULL;
    }
    while(line&&*line) {
        char *end=strchr(line,'\n');if(end)*end=0;
        int x,y;
        if(!strcmp(line,"hints=1"))prefs->hints=true;
        else if(sscanf(line,"settings=%d,%d",&x,&y)==2) {
            prefs->settings_x=x;prefs->settings_y=y;prefs->positioned=true;
        } else if(!strncmp(line,"audio=",6))SDL_strlcpy(prefs->audio,line+6,sizeof prefs->audio);
        line=end?end+1:NULL;
    }
    SDL_free(bytes);
}
bool m8_preferences_save(const m8_preferences *prefs) {
    char *name=path(),*temporary=NULL;if(!name)return false;
    if(SDL_asprintf(&temporary,"%s.tmp",name)<0){SDL_free(name);return false;}
    char value[512];int length=SDL_snprintf(value,sizeof value,"%d\n%s\nhints=%d\naudio=%s\n",
        prefs->controls,m8_keypad_theme_name(prefs->theme),prefs->hints,prefs->audio);
    if(prefs->positioned)length+=SDL_snprintf(value+length,sizeof value-(size_t)length,
        "settings=%d,%d\n",prefs->settings_x,prefs->settings_y);
    SDL_IOStream *file=SDL_IOFromFile(temporary,"wb");bool saved=false;
    if(file){bool written=SDL_WriteIO(file,value,(size_t)length)==(size_t)length;
        bool closed=SDL_CloseIO(file);saved=written&&closed&&SDL_RenamePath(temporary,name);}
    if(!saved)SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Cannot remember settings: %s",SDL_GetError());
    SDL_RemovePath(temporary);SDL_free(temporary);SDL_free(name);return saved;
}
