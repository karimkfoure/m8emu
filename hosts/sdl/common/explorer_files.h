#ifndef M8_EXPLORER_FILES_H
#define M8_EXPLORER_FILES_H
#include "fat.h"
#include <stdbool.h>
/* Only the offline FAT copy is changed. Each insertion is atomic; a batch can
 * retain a successful prefix and reports its count on failure. */
bool m8_explorer_add(m8_fat_card *,const char *folder,const char *const *files,
                     size_t count,char *error,size_t capacity);
/* Export a card entry beneath an existing host folder. Refuse collisions,
 * including existing directories, rather than overwrite unrelated host files. */
bool m8_explorer_export(m8_fat_card *,const char *path,const char *folder,
                        char *error,size_t capacity);
bool m8_explorer_write_file(const char *path,const void *,size_t,bool exclusive,
                            char *error,size_t capacity);
#endif
