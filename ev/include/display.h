#ifndef DISPLAY_H
#define DISPLAY_H
#include "vehicle.h"
void display_init(void);
void display_task(uint32_t now_ms);
/* pure formatter (unit-testable): out[0]=digit8 (left) ... out[7]=digit1 (right), MAX7219 code-B values */
void display_format(const vehicle_t *v, uint8_t out[8]);
#define CB_BLANK 0x0F
#define CB_DASH  0x0A
#define CB_E     0x0B
#define CB_P     0x0E
#endif
