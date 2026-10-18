#include "media.h"
#include "media_internal.h"
#include "media_platform_posix.h"
#include "fat.h"
#include "storage.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Host-folder ownership stays in the platform adapter. FAT validation,
 * allocation and encoding have one owner in fat.c. */
int m8_media_fail(media *m,const char *format,...)
{
    va_list args;va_start(args,format);
    vsnprintf(m->error,sizeof m->error,format,args);va_end(args);return -1;
}
node *m8_media_new_node(media *m,const char *name,int directory)
{
    if(*name && !m8_fat_valid_name(name)){m8_media_fail(m,"Invalid FAT file name: %s",name);return NULL;}
    node *n=m8_fat_new_node(name,directory);
    if(!n)m8_media_fail(m,"Cannot allocate media node: %s",name);
    return n;
}
void m8_media_free_node(node *n){m8_fat_free_node(n);}
int m8_media_add_child(media *m,node *parent,node *child)
{
    int result=m8_fat_add_child(parent,child);
    if(result)return m8_media_fail(m,result==-2?"FAT names conflict without case: %s":"Cannot allocate media children: %s",child->name);
    return 0;
}
static int card_error(media *m,const m8_fat_error *error)
{
    return m8_media_fail(m,"%s%s%s",error->path,error->path[0]?": ":"",error->message);
}
static char *join_path(media *m,const char *parent,const char *name)
{
    size_t a=strlen(parent),b=strlen(name);
    if(a+b+2>4096){m8_media_fail(m,"FAT path is too long: %s",name);return NULL;}
    char *path=malloc(a+b+2);
    if(path){memcpy(path,parent,a);size_t at=a;if(a)path[at++]='/';memcpy(path+at,name,b+1);}
    else m8_media_fail(m,"Cannot allocate FAT path: %s",name);
    return path;
}
static int import_tree(media *m,m8_fat_card *card,const node *source,const char *parent,int overwrite)
{
    for(size_t i=0;i<source->child_count;i++){
        const node *n=source->children[i];char *path=join_path(m,parent,n->name);if(!path)return -1;
        m8_fat_entry old;int result=0;
        m8_fat_result status=m8_fat_stat(card,path,&old);
        if(status!=M8_FAT_OK && status!=M8_FAT_NOT_FOUND)result=card_error(m,m8_fat_last_error(card));
        else if(!status){
            if(old.directory!=n->directory)result=m8_media_fail(m,"File/directory conflict: %s",path);
            else if(!n->directory && !overwrite)result=m8_media_fail(m,"File exists; use --overwrite to replace it: %s",path);
            else{
                /* Native overwrite keeps the existing card spelling. */
                char *existing=malloc(strlen(old.path)+1);
                if(!existing)result=m8_media_fail(m,"Cannot allocate existing FAT path: %s",path);
                else{strcpy(existing,old.path);free(path);path=existing;}
            }
        }
        /* The old adapter used a fixed timestamp; keep that contract. */
        m8_fat_time time={(uint16_t)(((2026-1980)<<9)|(9<<5)|16),12<<11};
        if(!result && n->directory){
            if(status==M8_FAT_NOT_FOUND && m8_fat_mkdir(card,path,time,0))result=card_error(m,m8_fat_last_error(card));
            if(!result)result=import_tree(m,card,n,path,overwrite);
        }else if(!result && m8_fat_write(card,path,n->data,n->size,time,0))result=card_error(m,m8_fat_last_error(card));
        free(path);if(result)return -1;
    }
    return 0;
}
static int export_tree(media *m,m8_fat_card *card,node *parent,const char *path)
{
    m8_fat_entry folder;
    if(m8_fat_stat(card,path,&folder))return card_error(m,m8_fat_last_error(card));
    for(size_t i=0;i<folder.children;i++){
        m8_fat_entry entry;
        if(m8_fat_list(card,path,i,&entry))return card_error(m,m8_fat_last_error(card));
        node *n=m8_media_new_node(m,entry.name,entry.directory);if(!n)return -1;
        int result=0;
        if(n->directory)result=export_tree(m,card,n,entry.path);
        else{
            const void *bytes=NULL;size_t size=0;
            if(m8_fat_read(card,entry.path,&bytes,&size))result=card_error(m,m8_fat_last_error(card));
            else{
                n->size=size;n->data=size?malloc(size):NULL;
                if(size && !n->data)result=m8_media_fail(m,"Cannot allocate exported file: %s",entry.path);
                else if(size)memcpy(n->data,bytes,size);
            }
        }
        if(!result)result=m8_media_add_child(m,parent,n);
        if(result){m8_media_free_node(n);return -1;}
    }
    return 0;
}
static void finish(media *m,node *root,m8_fat_card *card,char *error,size_t capacity)
{
    if(error && capacity)snprintf(error,capacity,"%s",m->error);
    m8_media_free_node(root);m8_fat_close(card);free(m->image);
}
int m8_media_import(m8rt *rt,const char *directory,int overwrite,char *error,size_t capacity)
{
    media m={0};node *incoming=NULL;m8_fat_card *card=NULL;int result=-1;m8_fat_error fat_error;
    if(!rt || !directory){m8_media_fail(&m,"Import needs a runtime and directory");goto done;}
    if(m8rt_get(rt,M8RT_SLICES)){m8_media_fail(&m,"Import must precede guest execution");goto done;}
    if(m8rt_sd_prepare_export(rt,0)){m8_media_fail(&m,"Import refused: %s",m8rt_error(rt));goto done;}
    m.image_size=m8rt_sd_image_size(rt);m.image=malloc(m.image_size);
    if(!m.image || m8rt_sd_copy_image(rt,m.image,m.image_size)){m8_media_fail(&m,"Cannot copy private image for import");goto done;}
    if(m8_fat_open(m.image,m.image_size,&card,&fat_error)){card_error(&m,&fat_error);goto done;}
    if(m8_fat_bits(card)!=16){m8_media_fail(&m,"Only FAT16 images are supported for native import");goto done;}
    incoming=m8_media_platform_read_folder(&m,directory);if(!incoming)goto done;
    if(import_tree(&m,card,incoming,"",overwrite))goto done;
    size_t size;const void *bytes=m8_fat_image(card,&size);
    if(m8rt_sd_replace_image(rt,bytes,size)){m8_media_fail(&m,"Cannot replace private runtime image after import");goto done;}
    result=0;
done:
    finish(&m,incoming,card,error,capacity);return result;
}
int m8_media_export(const char *image,const char *directory,char *error,size_t capacity)
{
    media m={0};node *root=NULL;m8_fat_card *card=NULL;int result=-1;m8_fat_error fat_error;
    if(!image || !directory || !*directory){m8_media_fail(&m,"Export needs an image and directory");goto done;}
    if(m8_media_platform_read_image(&m,image))goto done;
    if(m8_fat_open(m.image,m.image_size,&card,&fat_error)){card_error(&m,&fat_error);goto done;}
    root=m8_media_new_node(&m,"",1);if(!root || export_tree(&m,card,root,""))goto done;
    result=m8_media_platform_export(&m,root,directory);
done:
    finish(&m,root,card,error,capacity);return result;
}
