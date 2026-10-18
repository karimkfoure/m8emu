#ifndef M8_FAT_H
#define M8_FAT_H
#include <stddef.h>
#include <stdint.h>

/* A card owns a copy of its image. Open validates every reachable name and
 * chain before returning. No call reads or writes host files. */
typedef struct m8_fat_card m8_fat_card;
typedef enum {
    M8_FAT_OK = 0, M8_FAT_INVALID, M8_FAT_DAMAGED, M8_FAT_NOT_FOUND,
    M8_FAT_CONFLICT, M8_FAT_NO_SPACE, M8_FAT_MEMORY
} m8_fat_result;
typedef struct {
    m8_fat_result code;
    char path[4096];
    char message[512];
} m8_fat_error;
typedef struct { uint16_t date, time; } m8_fat_time;
typedef struct {
    const char *name, *path;
    int directory;
    size_t size, children;
    m8_fat_time modified;
} m8_fat_entry;

m8_fat_result m8_fat_open(const void *bytes, size_t size,
                         m8_fat_card **out, m8_fat_error *error);
m8_fat_result m8_fat_create(size_t size, m8_fat_card **out, m8_fat_error *error);
void m8_fat_close(m8_fat_card *card);
const m8_fat_error *m8_fat_last_error(const m8_fat_card *card);
unsigned m8_fat_bits(const m8_fat_card *card);
m8_fat_result m8_fat_stat(m8_fat_card *card, const char *path, m8_fat_entry *out);
/* List is sorted: directories first, then Unicode case-folded name. */
m8_fat_result m8_fat_list(m8_fat_card *card, const char *path, size_t index, m8_fat_entry *out);
m8_fat_result m8_fat_read(m8_fat_card *card, const char *path, const void **bytes, size_t *size);
/* Mutations are atomic. max_size=0 prohibits growth; otherwise it is capped
 * at 128 MiB. Existing geometry and file chains stay in place when they fit.
 * Write creates parents and replaces files without case sensitivity. */
m8_fat_result m8_fat_write(m8_fat_card *card, const char *path, const void *bytes,
                          size_t size, m8_fat_time modified, size_t max_size);
m8_fat_result m8_fat_mkdir(m8_fat_card *card, const char *path,
                          m8_fat_time modified, size_t max_size);
m8_fat_result m8_fat_remove(m8_fat_card *card, const char *path);
void m8_fat_usage(const m8_fat_card *card, size_t *used, size_t *capacity);
/* Returned pointers remain valid until mutation or close. Callers copy bytes
 * they need to retain. A failed mutation leaves all pointers valid. */
const void *m8_fat_image(const m8_fat_card *card, size_t *size);
int m8_fat_changed(const m8_fat_card *card);
#endif
