/* vehicle.h - shared "data bus": the single vehicle state record written by
 * sensor tasks and read by ADAS / display / telemetry. Main-loop context only. */
#ifndef VEHICLE_H
#define VEHICLE_H
#include "config.h"
typedef enum { ADAS_CLEAR=0, ADAS_ADV=1, ADAS_WARN=2, ADAS_CRIT=3, ADAS_UNAVAIL=4 } adas_level_t;

/* fault bits -> display codes E01..E04 */
#define F_IMU    (1u<<0)
#define F_RANGE  (1u<<1)
#define F_BATT   (1u<<2)
#define F_SCHED  (1u<<3)

typedef struct {
    int16_t  ax_mg, ay_mg, az_mg;  uint32_t imu_ts_ms,   imu_seq;
    uint16_t speed_dkmh;           uint32_t speed_ts_ms;
    bool     target; int32_t dist_mm, closing_mmps; uint32_t range_ts_ms, range_seq;
    uint16_t batt_mv; uint8_t soc_pct; bool batt_low, batt_crit; uint32_t batt_ts_ms;
    adas_level_t level; int32_t ttc_ms; bool crash; bool brake_active;
    uint16_t faults;
} vehicle_t;
extern vehicle_t g_veh;
#endif
