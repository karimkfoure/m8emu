#ifndef M8_FAT_INTERNAL_H
#define M8_FAT_INTERNAL_H
/* Private tree shared only with the native folder adapter. */
#include <stddef.h>
#include <stdint.h>
typedef struct node node;
struct node {
    char *name, *folded, *path;
    int directory;
    unsigned char *data, short_name[11], entry[32];
    size_t size, child_count, child_capacity;
    node **children;
    uint32_t *chain;
    size_t cluster_count;
    int names_assigned, dirty;
};
int m8_fat_valid_name(const char *name);
node *m8_fat_new_node(const char *name, int directory);
void m8_fat_free_node(node *n);
/* 0 success, -1 allocation failure, -2 case-folded duplicate. */
int m8_fat_add_child(node *parent, node *child);
#endif
