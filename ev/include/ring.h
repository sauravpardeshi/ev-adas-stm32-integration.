#ifndef RING_H
#define RING_H
#include "config.h"
typedef struct { uint8_t *buf; uint16_t size; volatile uint16_t head, tail; } ring_t;
void     ring_init(ring_t *r, uint8_t *mem, uint16_t size);
uint16_t ring_count(const ring_t *r);
uint16_t ring_free(const ring_t *r);
bool     ring_put(ring_t *r, uint8_t b);
int      ring_get(ring_t *r);
#endif
