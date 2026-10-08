#ifndef M8_NATIVE_MEDIA_PLATFORM_POSIX_H
#define M8_NATIVE_MEDIA_PLATFORM_POSIX_H
#include "media_internal.h"
/* Adapters return copied byte/tree ownership to the portable media owner.
 * Folder reads consume their own descriptors on every outcome. */
node *m8_media_platform_read_folder(media *m, const char *directory);
int m8_media_platform_read_image(media *m, const char *path);
int m8_media_platform_export(media *m, const node *root, const char *directory);
#endif
