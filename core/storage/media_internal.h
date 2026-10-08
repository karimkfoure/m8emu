#ifndef M8_NATIVE_MEDIA_INTERNAL_H
#define M8_NATIVE_MEDIA_INTERNAL_H
#include <stddef.h>
#include <stdint.h>

/* Private tree representation shared by the FAT owner and host adapters. */
#define MAX_DEPTH 128
#define NAME_UNITS 255

typedef struct node node;
struct node {
    char *name, *folded;
    int directory;
    unsigned char *data, short_name[11];
    size_t size, child_count, child_capacity;
    node **children;
    uint32_t cluster, cluster_count;
};
typedef struct {
    unsigned char *image, *used;
    size_t image_size, fat_offset, root_offset, root_size, data_offset, cluster_bytes;
    uint32_t clusters, fat_sectors, fat_count, root_entries, next_cluster;
    unsigned char label[11];
    char error[512];
} media;
int m8_media_fail(media *m, const char *format, ...);
node *m8_media_new_node(media *m, const char *name, int directory);
void m8_media_free_node(node *n);
int m8_media_add_child(media *m, node *parent, node *child);
#endif
