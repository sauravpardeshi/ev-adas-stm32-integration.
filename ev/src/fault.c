#include "fault.h"
#include "evlog.h"
static uint8_t bit_index(uint16_t b){ uint8_t i=0; while(b>1){b>>=1;i++;} return i; }
void fault_set(uint16_t bit, uint32_t now){
    if (!(g_veh.faults & bit)) { g_veh.faults |= bit; evlog_add(now, EV_FAULT_SET, bit_index(bit)); }
}
void fault_clear(uint16_t bit, uint32_t now){
    if (g_veh.faults & bit) { g_veh.faults &= (uint16_t)~bit; evlog_add(now, EV_FAULT_CLR, bit_index(bit)); }
}
/* priority: sensing faults before scheduler fault; lowest bit wins */
uint8_t fault_top_code(uint16_t f){
    for (uint8_t i=0;i<4;i++) if (f & (1u<<i)) return (uint8_t)(i+1);
    return 0;
}
