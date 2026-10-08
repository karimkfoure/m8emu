#include "queue.h"
#include <stdlib.h>
#include <string.h>
int m8_queue_init(m8_queue *q, uint32_t capacity) {
    if (!capacity || (capacity & (capacity - 1)) || capacity > UINT32_MAX / 2) return -1;
    q->data = malloc(capacity);
    if (!q->data) return -1;
    q->capacity = capacity;
    atomic_init(&q->written, 0); atomic_init(&q->read, 0);
    return 0;
}
void m8_queue_close(m8_queue *q) { free(q->data); q->data = NULL; }
uint32_t m8_queue_size(m8_queue *q) {
    uint32_t r = atomic_load_explicit(&q->read, memory_order_acquire);
    return atomic_load_explicit(&q->written, memory_order_acquire) - r;
}
int m8_queue_write(m8_queue *q, const void *bytes, size_t n) {
    uint32_t w = atomic_load_explicit(&q->written, memory_order_relaxed);
    uint32_t r = atomic_load_explicit(&q->read, memory_order_acquire);
    if (n > q->capacity - (w - r)) return -1;
    uint32_t offset = w & (q->capacity - 1);
    size_t first = q->capacity - offset;
    if (first > n) first = n;
    memcpy(q->data + offset, bytes, first);
    memcpy(q->data, (const unsigned char *)bytes + first, n - first);
    atomic_store_explicit(&q->written, w + (uint32_t)n, memory_order_release);
    return 0;
}
size_t m8_queue_read(m8_queue *q, void *bytes, size_t n) {
    uint32_t r = atomic_load_explicit(&q->read, memory_order_relaxed);
    uint32_t w = atomic_load_explicit(&q->written, memory_order_acquire);
    if (n > w - r) n = w - r;
    uint32_t offset = r & (q->capacity - 1);
    size_t first = q->capacity - offset;
    if (first > n) first = n;
    if (bytes) {
        memcpy(bytes, q->data + offset, first);
        memcpy((unsigned char *)bytes + first, q->data, n - first);
    }
    atomic_store_explicit(&q->read, r + (uint32_t)n, memory_order_release);
    return n;
}
