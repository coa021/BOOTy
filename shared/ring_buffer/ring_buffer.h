#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RING_BUFFER_SIZE 512u
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1u)

struct ring_buffer_t {
  uint8_t buf[RING_BUFFER_SIZE];
  volatile uint32_t write;
  volatile uint32_t read;
};

void rb_init(struct ring_buffer_t *rb);
bool rb_put(struct ring_buffer_t *rb, uint8_t byte);
bool rb_get(struct ring_buffer_t *rb, uint8_t *out);
uint32_t rb_available(const struct ring_buffer_t *rb);
bool rb_is_empty(const struct ring_buffer_t *rb);

#endif // RING_BUFFER_H
