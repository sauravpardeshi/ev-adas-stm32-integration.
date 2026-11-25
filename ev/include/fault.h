#ifndef FAULT_H
#define FAULT_H
#include "vehicle.h"
void    fault_set(uint16_t bit, uint32_t now_ms);
void    fault_clear(uint16_t bit, uint32_t now_ms);
uint8_t fault_top_code(uint16_t faults);   /* 0 = none, else 1..4 (E01..E04) */
#endif
