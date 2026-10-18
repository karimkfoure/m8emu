#include "explorer_files.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static m8_fat_time modified(SDL_Time time) {
    SDL_DateTime date;
    if(!SDL_TimeToDateTime(time,&date,true))return (m8_fat_time){0};
    int year=SDL_clamp(date.year,1980,2107);
    return (m8_fat_time){(uint16_t)((year-1980)<<9|date.month<<5|date.day),
        (uint16_t)(date.hour<<11|date.minute<<5|date.second/2)};
}
bool m8_explorer_write_file(const char *path,const void *bytes,size_t size,bool exclusive,char *error,size_t capacity) {
    SDL_IOStream *file=SDL_IOFromFile(path,exclusive?"wbx":"wb");
    if(!file){SDL_snprintf(error,capacity,"Cannot create %s: %s",path,SDL_GetError());return false;}
    bool written=SDL_WriteIO(file,bytes,size)==size,closed=SDL_CloseIO(file);
    if(!written||!closed){SDL_snprintf(error,capacity,"Cannot write %s: %s",path,SDL_GetError());return false;}
    return true;
}
bool m8_explorer_add(m8_fat_card *card,const char *folder,const char *const *files,size_t count,char *error,size_t capacity) {
    for(size_t i=0;i<count;i++) {
        const char *name=files[i];
        for(const char *p=name;*p;p++) {
            if(*p=='/')name=p+1;
#ifdef _WIN32
            if(*p=='\\')name=p+1;
#endif
        }
        SDL_PathInfo info;char detail[512];size_t size=0;void *bytes=NULL;
        if(!*name||strchr(name,'\\'))SDL_strlcpy(detail,"Invalid file name.",sizeof detail);
        else if(!SDL_GetPathInfo(files[i],&info)||info.type!=SDL_PATHTYPE_FILE||info.size>128u*1024*1024)
            SDL_strlcpy(detail,"Choose a regular file of at most 128 MiB.",sizeof detail);
        else if(!(bytes=SDL_LoadFile(files[i],&size)))SDL_strlcpy(detail,SDL_GetError(),sizeof detail);
        else {
            char target[4096];int n=SDL_snprintf(target,sizeof target,"%s%s%s",folder,*folder?"/":"",name);
            m8_fat_result result=n<0||(size_t)n>=sizeof target?M8_FAT_INVALID:
                m8_fat_write(card,target,bytes,size,modified(info.modify_time),128u*1024*1024);
            SDL_free(bytes);
            if(result==M8_FAT_OK)continue;
            SDL_strlcpy(detail,n<0||(size_t)n>=sizeof target?"Path is too long.":m8_fat_last_error(card)->message,sizeof detail);
        }
        SDL_snprintf(error,capacity,"Added %zu of %zu files. %s: %s",i,count,name,detail);return false;
    }
    return true;
}
bool m8_explorer_export(m8_fat_card *card,const char *path,const char *folder,char *error,size_t capacity) {
    m8_fat_entry entry;
    if(m8_fat_stat(card,path,&entry)!=M8_FAT_OK){SDL_strlcpy(error,m8_fat_last_error(card)->message,capacity);return false;}
    char *target=NULL;if(SDL_asprintf(&target,"%s/%s",folder,entry.name)<0)return false;
    bool success=false;
    if(entry.directory) {
        SDL_PathInfo info;
        if(SDL_GetPathInfo(target,&info)&&info.type!=SDL_PATHTYPE_NONE)SDL_snprintf(error,capacity,"Destination already exists: %s",target);
        else if(!SDL_CreateDirectory(target))SDL_snprintf(error,capacity,"Cannot create %s: %s",target,SDL_GetError());
        else {
            success=true;
            for(size_t i=0;i<entry.children;i++) {
                m8_fat_entry child;
                if(m8_fat_list(card,path,i,&child)!=M8_FAT_OK||!m8_explorer_export(card,child.path,target,error,capacity)){success=false;break;}
            }
        }
    } else {
        const void *bytes;size_t size;
        if(m8_fat_read(card,path,&bytes,&size)==M8_FAT_OK)success=m8_explorer_write_file(target,bytes,size,true,error,capacity);
        else SDL_strlcpy(error,m8_fat_last_error(card)->message,capacity);
    }
    SDL_free(target);return success;
}
