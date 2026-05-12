#include "ring_buffer.h"
#include <string.h>

void rb_init(struct ring_buffer_t *rb) {
  memset(rb->buf, 0, sizeof(rb->buf));
  rb->write = 0;
  rb->read = 0;
}

bool rb_put(struct ring_buffer_t *rb, uint8_t byte) {
  uint32_t next_head = (rb->write + 1u) & RING_BUFFER_MASK;

  if (next_head == rb->read) {
    return false; // full
  }

  rb->buf[rb->write] = byte;
  rb->write = next_head;

  return true;
}

bool rb_get(struct ring_buffer_t *rb, uint8_t *out) {
  if (rb->read == rb->write) {
    return false; // empty
  }

  *out = rb->buf[rb->read];
  rb->read = (rb->read + 1u) & RING_BUFFER_MASK;
  return true;
}

uint32_t rb_available(const struct ring_buffer_t *rb) {
  return (rb->write - rb->read) & RING_BUFFER_MASK;
}

bool rb_is_empty(const struct ring_buffer_t *rb) {
  return rb->write == rb->read;
}
