/* app.c - system integration: wires every module into the scheduler */
#include "app.h"
#include "hal.h"
#include "sched.h"
#include "vehicle.h"
#include "fault.h"
#include "evlog.h"
#include "sensors.h"
#include "range.h"
#include "adas.h"
#include "alert.h"
#include "display.h"
#include "uart_log.h"
#include <string.h>

vehicle_t g_veh;

static void superv_task(uint32_t now){            /* clears the scheduler fault after 2 s without misses */
    if ((g_veh.faults & F_SCHED) && (now - sched_last_miss_ms()) >= 2000) fault_clear(F_SCHED, now);
}

void ev_init(void){
    memset(&g_veh, 0, sizeof g_veh);
    evlog_init(); sched_init();
    uint32_t now = hal_millis();
    evlog_add(now, EV_BOOT, hal_reset_cause());
    uart_init(); range_init(); adas_init(); batt_init();
    imu_init_task(now);                           /* sets F_IMU if the sensor is absent */
    display_init();
    /* ---- task table, in priority order (cooperative) ---- */
    sched_add("imu",     imu_task,     P_IMU_MS,     0);
    sched_add("speed",   speed_task,   P_SPEED_MS,   1);
    sched_add("range",   range_task,   P_RANGE_POLL, 0);
    sched_add("adas",    adas_task,    P_ADAS_MS,    2);
    sched_add("alert",   alert_task,   P_ALERT_MS,   3);
    sched_add("batt",    batt_task,    P_BATT_MS,    4);
    sched_add("cli",     cli_task,     P_CLI_MS,     5);
    sched_add("display", display_task, P_DISPLAY_MS, 6);
    sched_add("telem",   telem_task,   P_TELEM_MS,   7);
    sched_add("superv",  superv_task,  P_SUPERV_MS,  8);
    hal_wdt_start(WDT_TIMEOUT_MS);
}

void ev_step(void){
    bool ran = sched_run();
    if (!sched_hung_round()) hal_wdt_kick();      /* withhold the kick if any task blocked too long */
    if (!ran) hal_idle();
}
