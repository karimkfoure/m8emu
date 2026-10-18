#ifndef M8_NATIVE_MEDIA_INTERNAL_H
#define M8_NATIVE_MEDIA_INTERNAL_H
#include <stddef.h>
#include <stdint.h>

/* Private tree representation shared by the FAT owner and host adapters. */
#define MAX_DEPTH 128
#define NAME_UNITS 255

#include "fat_internal.h"
typedef struct {
    unsigned char *image;
    size_t image_size;
    char error[512];
} media;
int m8_media_fail(media *m, const char *format, ...);
node *m8_media_new_node(media *m, const char *name, int directory);
void m8_media_free_node(node *n);
int m8_media_add_child(media *m, node *parent, node *child);
#endif
