#ifndef RANGE_H
#define RANGE_H
#include "config.h"
typedef struct { bool valid; int32_t d, v; uint8_t rejects, n; } track_t;   /* d mm, v mm/s (+ve = closing) */
bool    track_update(track_t *t, int32_t z_mm, int32_t dt_ms);               /* z >= RANGE_MAX_MM = no echo */
void    range_init(void);
void    range_task(uint32_t now_ms);
int32_t range_echo_to_mm(int32_t width_us);   /* 10/58 mm per us          */
int32_t median3(int32_t a, int32_t b, int32_t c);
uint32_t range_invalid_count(void);
#endif
