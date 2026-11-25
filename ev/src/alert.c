/* alert.c - buzzer / LED patterns, highest priority first */
#include "alert.h"
#include "hal.h"
#include "vehicle.h"
void alert_task(uint32_t now){
    const vehicle_t *v = &g_veh;
    bool bz = false, adv = false, warn = false, crit = false, batt = false;
    if (v->crash)                       { bz = true; adv = warn = crit = true; }
    else switch (v->level) {
        case ADAS_CRIT:   bz = true;  crit = true; break;
        case ADAS_WARN:   bz = ((now / 250) & 1) == 0; warn = true; break;   /* 2 Hz beep */
        case ADAS_ADV:    adv = true; break;
        case ADAS_UNAVAIL: warn = ((now / 500) & 1) == 0; break;            /* 1 Hz blink: ADAS unavailable */
        default: break;
    }
    if (v->batt_crit)      batt = ((now / 125) & 1) == 0;                    /* 4 Hz */
    else if (v->batt_low)  batt = true;
    hal_gpio_write(PIN_BUZZER, bz);   hal_gpio_write(PIN_LED_ADV, adv);
    hal_gpio_write(PIN_LED_WARN, warn); hal_gpio_write(PIN_LED_CRIT, crit);
    hal_gpio_write(PIN_LED_BATT, batt);
}
