#ifndef M8_HOST_QUEUE_H
#define M8_HOST_QUEUE_H
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
/* Bounded byte FIFO: one writer and one reader. Capacity is a power of two.
 * Whole writes are admitted or refused; published bytes retain their order. */
typedef struct m8_queue {
    unsigned char *data;
    uint32_t capacity;
    _Atomic uint32_t written;
    unsigned char separate_positions[64];
    _Atomic uint32_t read;
} m8_queue;
int m8_queue_init(m8_queue *q, uint32_t capacity);
void m8_queue_close(m8_queue *q);
int m8_queue_write(m8_queue *q, const void *bytes, size_t n);
size_t m8_queue_read(m8_queue *q, void *bytes, size_t n);
uint32_t m8_queue_size(m8_queue *q);
#endif
