#ifndef EVLOG_H
#define EVLOG_H
#include "config.h"
enum { EV_ADAS_LEVEL=1, EV_BRAKE, EV_IMPACT, EV_FAULT_SET, EV_FAULT_CLR, EV_BATT_LOW,
       EV_BATT_CRIT, EV_DRV_REACT, EV_BOOT, EV_RST_CMD };
#define EVLOG_SIZE 16
typedef struct { uint32_t t_ms; uint8_t code; int32_t arg; } evlog_entry_t;
void     evlog_init(void);
void     evlog_add(uint32_t t_ms, uint8_t code, int32_t arg);
uint8_t  evlog_count(void);
uint32_t evlog_total(void);
bool     evlog_get(uint8_t idx_from_oldest, evlog_entry_t *out);
#endif
