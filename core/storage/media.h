#ifndef M8_NATIVE_MEDIA_H
#define M8_NATIVE_MEDIA_H
#include <stddef.h>
#include "m8rt.h"

/* Merge a host folder into private runtime storage before its first run.
 * FAT16 geometry and image size stay fixed. Names, empty directories and
 * file bytes are preserved; allocation and timestamps can change.
 * Existing files require overwrite. File/directory conflicts, case-folded
 * duplicates, links, special files and read-only storage are refused.
 * Failure leaves the complete runtime image and source card unchanged. */
int m8_media_import(m8rt *runtime, const char *directory, int overwrite,
                    char *error, size_t error_capacity);
/* Export without creating or running a runtime. The destination must be a
 * new or empty directory. Every FAT path and chain is checked before any
 * host file is created. Links and special destinations are refused. */
int m8_media_export(const char *image_path, const char *directory,
                    char *error, size_t error_capacity);
#endif
