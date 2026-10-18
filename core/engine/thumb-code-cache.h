#ifndef M8_THUMB_CODE_CACHE_H
#define M8_THUMB_CODE_CACHE_H
#include "thumb-interpreter.h"

/* Pinned M7 uses 1 KiB target pages. A block that starts with a wide
 * instruction crossing a page contains only that instruction. */
#define M8_THUMB_CODE_CACHE_BYTES 1024u
typedef struct m8_thumb_code_cache {
    m8_thumb_fetch_fn live_fetch;
    void *live_ctx;
    uint32_t start, size, active, generation;
    uint8_t bytes[M8_THUMB_CODE_CACHE_BYTES];
} m8_thumb_code_cache;

enum m8_thumb_code_cache_status {
    M8_THUMB_CACHE_OK = 0,
    M8_THUMB_CACHE_SPAN = 1,
    M8_THUMB_CACHE_FETCH = 2,
    M8_THUMB_CACHE_CHANGED = 3
};

/* Call before any guest code-store effect. The caller owns a trusted TB
 * span for this exact runtime hook/count profile, plus its original bytes.
 * A changed byte rejects the map before the store. No TCG-size heuristic
 * is used. A second store in an active span retains the first snapshot.
 * Zero-initialize the cache before its first use. */
int m8_thumb_code_cache_begin(m8_thumb_code_cache *cache, uint32_t pc,
                             unsigned size, const uint8_t *expected,
                             m8_thumb_fetch_fn live_fetch, void *ctx);

/* Use as the interpreter fetch callback after a successful begin. */
int m8_thumb_code_cache_fetch(void *cache, uint32_t pc, unsigned size,
                            uint8_t *bytes);

/* Call after retirement or a fault, using pre-instruction boundary flags.
 * completed means that service handling completed the instruction too.
 * Do not finish an FP_CONTEXT request before its prepared retry. A stopped
 * hint or fault ends the active snapshot; a serviced SEV can retain it. */
void m8_thumb_code_cache_finish(m8_thumb_code_cache *cache, uint32_t pc,
                               unsigned width, uint32_t actual_pc,
                               unsigned boundary, int completed);

/* The execution owner calls this at external IRQ/fault entry, a hook stop,
 * reset, or a stopped host code write. It does not undo completed stores. */
void m8_thumb_code_cache_invalidate(m8_thumb_code_cache *cache);
#endif
