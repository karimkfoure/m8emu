#ifndef M8_NATIVE_CARD_H
#define M8_NATIVE_CARD_H
#include <stddef.h>
#include "m8rt.h"

typedef struct m8_card m8_card;

/* NULL path selects $XDG_DATA_HOME/m8-live/card.img, or
 * $HOME/.local/share/m8-live/card.img. create permits a new empty 128 MiB
 * FAT16 image. Each open holds an exclusive lock until close. */
m8_card *m8_card_open(const char *path, int read_only, int create,
                      char *error, size_t error_capacity);
const char *m8_card_path(const m8_card *card);
int m8_card_read_only(const m8_card *card);
const char *m8_card_error(const m8_card *card);
/* Call after open and before m8rt_create. Refuse source, backup, or lock files
 * that alias a protected input, including hard links. Keep the protection
 * through save. Failure makes publication terminally unavailable. */
int m8_card_protect_file(m8_card *card, const char *path);

/* Call only on clean exit, after the runtime execution owner has stopped.
 * This admits terminal CMD18 read retirement, checks write safety, writes
 * and verifies temporary files, then atomically publishes .bak and card.
 * The caller must apply m8_card_read_only via m8rt_sd_set_read_only before
 * guest execution. A read-only or failed session is never published. */
int m8_card_save(m8_card *card, m8rt *runtime);
void m8_card_close(m8_card *card);
/* Fresh 16 MiB demo medium in memory; no persistent card is opened. */
int m8_card_empty_runtime(m8rt *runtime);
#endif
