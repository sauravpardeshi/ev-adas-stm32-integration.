#ifndef SENSORS_H
#define SENSORS_H
#include "config.h"
/* speed */
uint16_t speed_from_adc(uint16_t counts);          /* deci-km/h */
void     speed_task(uint32_t now_ms);
/* battery */
uint32_t batt_adc_to_pack_mv(uint16_t counts);
uint8_t  soc_from_cell_mv(uint32_t cell_mv);
void     batt_init(void);
void     batt_task(uint32_t now_ms);
/* imu */
void     imu_init_task(uint32_t now_ms);
void     imu_task(uint32_t now_ms);
#endif
