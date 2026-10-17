#define _POSIX_C_SOURCE 200809L
#include "media_platform_posix.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

/* POSIX private files also serve Android app-private storage. A document
 * provider can supply the same tree adapter without changing FAT handling. */
static int read_file(media *m,int fd,node *n)
{
    struct stat st;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<0 || (uintmax_t)st.st_size>UINT32_MAX)
        return m8_media_fail(m,"Input must be a regular file of at most 4 GiB: %s",n->name);
    n->size=(size_t)st.st_size;n->data=n->size?malloc(n->size):NULL;
    if(n->size && !n->data)return m8_media_fail(m,"Cannot allocate input file: %s",n->name);
    size_t offset=0;
    while(offset<n->size){
        ssize_t got=read(fd,n->data+offset,n->size-offset);
        if(got<0 && errno==EINTR)continue;
        if(got<=0)return m8_media_fail(m,"Cannot read input file: %s",n->name);
        offset+=(size_t)got;
    }
    unsigned char extra;ssize_t got;
    do { got=read(fd,&extra,1); }while(got<0 && errno==EINTR);
    if(got!=0)return m8_media_fail(m,"Input file changed while reading: %s",n->name);
    return 0;
}
static int compare_nodes(const void *a,const void *b)
{
    const node *left=*(node *const *)a,*right=*(node *const *)b;
    int order=strcmp(left->folded,right->folded);return order?order:strcmp(left->name,right->name);
}
static int scan_folder(media *m,int fd,node *parent,unsigned depth)
{
    if(depth>MAX_DEPTH){close(fd);return m8_media_fail(m,"Host directory depth exceeds %u",MAX_DEPTH);}
    DIR *directory=fdopendir(fd);if(!directory){close(fd);return m8_media_fail(m,"Cannot read import directory: %s",strerror(errno));}
    int result=0;struct dirent *entry;
    errno=0;
    while((entry=readdir(directory))){
        if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,".."))continue;
        struct stat st;
        if(fstatat(fd,entry->d_name,&st,AT_SYMLINK_NOFOLLOW) || (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode))){result=m8_media_fail(m,"Links and special files cannot be imported: %s",entry->d_name);break;}
        node *child=m8_media_new_node(m,entry->d_name,S_ISDIR(st.st_mode));if(!child){result=-1;break;}
        int child_fd=openat(fd,entry->d_name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|(child->directory?O_DIRECTORY:0));
        if(child_fd<0){m8_media_free_node(child);result=m8_media_fail(m,"Cannot open input without a link: %s",entry->d_name);break;}
        if(child->directory)result=scan_folder(m,child_fd,child,depth+1);
        else{result=read_file(m,child_fd,child);close(child_fd);}
        if(!result)result=m8_media_add_child(m,parent,child);
        if(result){m8_media_free_node(child);break;}
        errno=0;
    }
    if(!result && errno)result=m8_media_fail(m,"Cannot enumerate import directory: %s",strerror(errno));
    closedir(directory);
    if(!result && parent->child_count)qsort(parent->children,parent->child_count,sizeof(*parent->children),compare_nodes);
    return result;
}
node *m8_media_platform_read_folder(media *m,const char *directory)
{
    node *root=m8_media_new_node(m,"",1);if(!root)return NULL;
    int fd=open(directory,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0){m8_media_free_node(root);m8_media_fail(m,"Import source must be a directory without a link: %s",directory);return NULL;}
    if(scan_folder(m,fd,root,0)){m8_media_free_node(root);return NULL;}
    return root;
}

int m8_media_platform_read_image(media *m,const char *path)
{
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);struct stat st;
    if(fd<0)return m8_media_fail(m,"Cannot open image without a link: %s",path);
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<=0 || (uintmax_t)st.st_size>SIZE_MAX){close(fd);return m8_media_fail(m,"Image must be a nonempty regular file: %s",path);}
    node temporary={0};temporary.name=(char *)path;
    int result=read_file(m,fd,&temporary);close(fd);
    m->image=temporary.data;m->image_size=temporary.size;return result;
}
static int empty_directory(media *m,const char *path)
{
    struct stat st;
    if(lstat(path,&st)){if(errno==ENOENT)return 0;return m8_media_fail(m,"Cannot inspect export destination: %s",strerror(errno));}
    if(!S_ISDIR(st.st_mode))return m8_media_fail(m,"Export folder must be new or empty, without a link: %s",path);
    int fd=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    DIR *d=fd>=0?fdopendir(fd):NULL;
    if(!d){if(fd>=0)close(fd);return m8_media_fail(m,"Cannot open export directory: %s",strerror(errno));}
    int result=0;struct dirent *entry;errno=0;
    while((entry=readdir(d)))if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")){result=m8_media_fail(m,"Export folder must be empty: %s",path);break;}
    if(!result && errno)result=m8_media_fail(m,"Cannot enumerate export directory: %s",strerror(errno));
    closedir(d);return result;
}
#ifdef __APPLE__
/* macOS links /tmp, /var and /etc to /private with root-owned symlinks. */
static int system_link(const char *path,const struct stat *link)
{
    struct stat target;
    return S_ISLNK(link->st_mode) && link->st_uid==0 && !stat(path,&target) && S_ISDIR(target.st_mode);
}
#else
static int system_link(const char *path,const struct stat *link){(void)path;(void)link;return 0;}
#endif
static int create_directory_path(media *m,const char *path)
{
    char *copy=strdup(path);if(!copy)return m8_media_fail(m,"Cannot allocate export path");
    for(char *p=copy+1;;p++){
        if(*p && *p!='/')continue;
        char old=*p;*p=0;
        if(mkdir(copy,0700) && errno!=EEXIST){free(copy);return m8_media_fail(m,"Cannot create export directory: %s",strerror(errno));}
        struct stat st;if(lstat(copy,&st) || !(S_ISDIR(st.st_mode) || system_link(copy,&st))){free(copy);return m8_media_fail(m,"Export path must contain directories without links");}
        *p=old;if(!old)break;
    }
    free(copy);return 0;
}
static int write_file(media *m,int fd,const node *n)
{
    size_t offset=0;
    while(offset<n->size){
        ssize_t wrote=write(fd,n->data+offset,n->size-offset);
        if(wrote<0 && errno==EINTR)continue;
        if(wrote<=0)return m8_media_fail(m,"Cannot write exported file: %s",n->name);
        offset+=(size_t)wrote;
    }
    return 0;
}
static int export_tree(media *m,int fd,const node *root)
{
    for(size_t i=0;i<root->child_count;i++){
        const node *n=root->children[i];int child_fd;
        if(n->directory){
            if(mkdirat(fd,n->name,0700))return m8_media_fail(m,"Cannot create exported directory: %s",n->name);
            child_fd=openat(fd,n->name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        }else child_fd=openat(fd,n->name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(child_fd<0)return m8_media_fail(m,"Cannot create exported entry: %s",n->name);
        int result=n->directory?export_tree(m,child_fd,n):write_file(m,child_fd,n);
        if(close(child_fd) && !result)result=m8_media_fail(m,"Cannot close exported entry: %s",n->name);
        if(result)return -1;
    }
    return 0;
}
int m8_media_platform_export(media *m,const node *root,const char *directory)
{
    if(empty_directory(m,directory) || create_directory_path(m,directory))return -1;
    int fd=open(directory,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return m8_media_fail(m,"Cannot open export destination without a link");
    int result=export_tree(m,fd,root);
    if(close(fd) && !result)result=m8_media_fail(m,"Cannot close export destination: %s",strerror(errno));
    return result;
}
