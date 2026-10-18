#ifndef M8_CARD_PLATFORM_H
#define M8_CARD_PLATFORM_H
#include <stddef.h>

/* The card policy and FAT formatter use this filesystem boundary. A backend
 * holds one exclusive card lock, reads the opened source, rejects source
 * changes, verifies staged images, and publishes .bak before the card. It
 * must preserve partial publication on failure; the caller never retries.
 * POSIX app-private files implement this contract. Android document-provider
 * URIs require a separate backend with its own stated durability contract. */
typedef struct m8_card_platform m8_card_platform;
typedef int (*m8_card_platform_reader)(void *context, size_t offset,
                                     unsigned char *out, size_t count);

m8_card_platform *m8_card_platform_open(const char *path, int read_only,
    int create, size_t initial_size, m8_card_platform_reader initial_reader,
    void *initial_context, char *error, size_t error_capacity);
const char *m8_card_platform_path(const m8_card_platform *card);
int m8_card_platform_read_only(const m8_card_platform *card);
size_t m8_card_platform_size(const m8_card_platform *card);
const char *m8_card_platform_error(const m8_card_platform *card);
int m8_card_platform_protect_file(m8_card_platform *card, const char *path);
int m8_card_platform_save(m8_card_platform *card, size_t size,
                         m8_card_platform_reader reader, void *context);
void m8_card_platform_close(m8_card_platform *card);

/* Companion generations share the already-held card lock. They do not publish
 * or retire the running card. Load returns malloc-owned bytes, or 1 if absent. */
int m8_card_platform_session_load(m8_card_platform *,void **,size_t *);
int m8_card_platform_session_save(m8_card_platform *,const void *,size_t);
/* Paused replacement. The card rename commits both the image and its fresh
 * media generation. Before that point failure preserves the old generation.
 * Success can retain a post-commit directory-sync warning in error(). */
int m8_card_platform_replace(m8_card_platform *,const void *,size_t,const void *,size_t);

#endif
