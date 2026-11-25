#include "ring.h"
void ring_init(ring_t *r, uint8_t *mem, uint16_t size){ r->buf=mem; r->size=size; r->head=r->tail=0; }
uint16_t ring_count(const ring_t *r){ return (uint16_t)((r->head + r->size - r->tail) % r->size); }
uint16_t ring_free(const ring_t *r){ return (uint16_t)(r->size - 1 - ring_count(r)); }
bool ring_put(ring_t *r, uint8_t b){
    uint16_t n = (uint16_t)((r->head + 1) % r->size);
    if (n == r->tail) return false;          /* full: caller decides policy */
    r->buf[r->head] = b; r->head = n; return true;
}
int ring_get(ring_t *r){
    if (r->head == r->tail) return -1;
    uint8_t b = r->buf[r->tail]; r->tail = (uint16_t)((r->tail + 1) % r->size); return b;
}
