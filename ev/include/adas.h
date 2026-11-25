#ifndef ADAS_H
#define ADAS_H
#include "vehicle.h"
adas_level_t adas_classify(bool target, int32_t dist_mm, int32_t closing_mmps,
                           uint16_t speed_dkmh, int32_t *ttc_ms_out);
void adas_init(void);
void adas_task(uint32_t now_ms);
uint32_t adas_last_reaction_ms(void);
#endif
