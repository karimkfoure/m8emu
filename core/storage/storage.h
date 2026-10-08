#ifndef M8RT_STORAGE_H
#define M8RT_STORAGE_H
#include <m8emu/m8rt.h>
/* The live image is owned by the runtime. None of these functions writes the
 * source image. Attach and image copies are allowed only outside emulation. */
int m8rt_sd_replace_image(m8rt *rt, const void *data, size_t n);
size_t m8rt_sd_image_size(m8rt *rt);
uint64_t m8rt_sd_image_revision(m8rt *rt);
int m8rt_sd_copy_image(m8rt *rt, void *out, size_t n);
/* Copy an image interval while the execution owner is stopped. This is an
 * inspection operation; publication must first pass prepare_export. */
int m8rt_sd_read_image(m8rt *rt, size_t offset, void *out, size_t n);
/* Publication gate. The caller must stop and join its execution owner first.
 * abort_read permits terminal retirement of the maintained CMD18 DATPORT
 * read only. It never completes guest SAVE or accepts unresolved writes.
 * On refusal m8rt_error identifies the first failed check and its operands. */
int m8rt_sd_prepare_export(m8rt *rt, int abort_read);
/* Host policy controls are allowed only outside emulation. Faults remain
 * armed across transfers and reject bytes after the relative write budget. */
int m8rt_sd_set_read_only(m8rt *rt, int enabled);
int m8rt_sd_set_write_fault(m8rt *rt, int enabled, uint64_t after_bytes);
/* Return the required capacity, including NUL. A short output is truncated. */
size_t m8rt_sd_snapshot_json(m8rt *rt, char *out, size_t n);
int m8rt_sd_flush(m8rt *rt);
int m8rt_sd_abort_read(m8rt *rt);
#endif
