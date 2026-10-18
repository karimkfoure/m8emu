#include "thumb-code-cache.h"
#include <string.h>

void m8_thumb_code_cache_invalidate(m8_thumb_code_cache *cache) {
    cache->active=0;
    ++cache->generation;
}

int m8_thumb_code_cache_begin(m8_thumb_code_cache *cache, uint32_t pc,
                             unsigned size, const uint8_t *expected,
                             m8_thumb_fetch_fn live_fetch, void *ctx) {
    if(cache->active && pc>=cache->start && pc-cache->start<cache->size)
        return M8_THUMB_CACHE_OK;
    m8_thumb_code_cache_invalidate(cache);
    cache->live_fetch=live_fetch;cache->live_ctx=ctx;
    if(!size || (pc&1) || (size&1) || size>M8_THUMB_CODE_CACHE_BYTES ||
       pc>UINT32_MAX-size || !expected || !live_fetch)
        return M8_THUMB_CACHE_SPAN;
    /* A trusted span cannot cross a target page except for a single wide
     * instruction starting at its last halfword. */
    if((pc&1023u)+size>1024u && !((pc&1023u)==1022u && size==4))
        return M8_THUMB_CACHE_SPAN;
    for(unsigned i=0;i<size;i+=2) {
        if(live_fetch(ctx,pc+i,2,cache->bytes+i))return M8_THUMB_CACHE_FETCH;
        if(memcmp(cache->bytes+i,expected+i,2))return M8_THUMB_CACHE_CHANGED;
    }
    cache->start=pc;cache->size=size;cache->active=1;
    return M8_THUMB_CACHE_OK;
}

int m8_thumb_code_cache_fetch(void *opaque, uint32_t pc, unsigned size,
                            uint8_t *bytes) {
    m8_thumb_code_cache *cache=opaque;
    if(cache->active && pc>=cache->start && size<=cache->size &&
       pc-cache->start<=cache->size-size) {
        memcpy(bytes,cache->bytes+pc-cache->start,size);return 0;
    }
    if(cache->active)m8_thumb_code_cache_invalidate(cache);
    return cache->live_fetch?cache->live_fetch(cache->live_ctx,pc,size,bytes):1;
}

void m8_thumb_code_cache_finish(m8_thumb_code_cache *cache, uint32_t pc,
                               unsigned width, uint32_t actual_pc,
                               unsigned boundary, int completed) {
    if(!cache->active)return;
    if(!completed || boundary || actual_pc!=pc+width ||
       actual_pc<cache->start || actual_pc-cache->start>=cache->size)
        m8_thumb_code_cache_invalidate(cache);
}
