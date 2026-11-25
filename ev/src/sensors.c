/* sensors.c - speed (ADC), battery (ADC + SoC) and IMU task glue */
#include "sensors.h"
#include "hal.h"
#include "vehicle.h"
#include "fault.h"
#include "evlog.h"
#include "mpu6050.h"

/* ---------------- speed ---------------- */
uint16_t speed_from_adc(uint16_t c){
    uint32_t d = ((uint32_t)c * 1200u + 2047u) / 4095u;      /* 0..120.0 km/h */
    return (uint16_t)(d < 10 ? 0 : d);                       /* 1 km/h dead-band */
}
void speed_task(uint32_t now){
    static uint16_t h[4]; static uint8_t i, n;
    h[i] = hal_adc_read(ADC_CH_SPEED); i = (uint8_t)((i + 1) & 3); if (n < 4) n++;
    uint32_t s = 0; for (uint8_t k = 0; k < n; k++) s += h[k];
    g_veh.speed_dkmh = speed_from_adc((uint16_t)(s / n)); g_veh.speed_ts_ms = now;
}

/* ---------------- battery (4S Li-ion, 47k/10k divider) ---------------- */
uint32_t batt_adc_to_pack_mv(uint16_t c){ return ((uint32_t)c * 3300u / 4095u) * 57u / 10u; }
static const struct { uint16_t mv; uint8_t soc; } soc_lut[] = {
    {3000,0},{3300,10},{3600,40},{3700,55},{3800,70},{3900,80},{4000,90},{4200,100} };
uint8_t soc_from_cell_mv(uint32_t mv){
    const int n = (int)(sizeof soc_lut / sizeof soc_lut[0]);
    if (mv <= soc_lut[0].mv) return 0;
    if (mv >= soc_lut[n-1].mv) return 100;
    for (int i = 1; i < n; i++) if (mv <= soc_lut[i].mv) {
        uint32_t x0=soc_lut[i-1].mv, x1=soc_lut[i].mv, y0=soc_lut[i-1].soc, y1=soc_lut[i].soc;
        return (uint8_t)(y0 + (mv - x0) * (y1 - y0) / (x1 - x0));
    }
    return 100;
}
static uint32_t bfilt; static bool bfirst; static uint8_t bad;
void batt_init(void){ bfirst = true; bad = 0; g_veh.soc_pct = 0; }
void batt_task(uint32_t now){
    uint16_t c = hal_adc_read(ADC_CH_BATT);
    if (c < 20 || c > 4075) {                      /* open / shorted sense line */
        if (++bad >= 3) fault_set(F_BATT, now);
        return;
    }
    bad = 0; fault_clear(F_BATT, now);
    uint32_t mv = batt_adc_to_pack_mv(c);
    if (bfirst) { bfilt = mv; bfirst = false; } else bfilt = (bfilt * 7 + mv) / 8;
    g_veh.batt_mv = (uint16_t)bfilt; g_veh.batt_ts_ms = now;
    g_veh.soc_pct = soc_from_cell_mv(bfilt / 4);
    /* hysteresis so load sag cannot chatter the warning */
    if (!g_veh.batt_low  && g_veh.soc_pct <  SOC_LOW_PCT)  { g_veh.batt_low  = true;  evlog_add(now, EV_BATT_LOW,  g_veh.soc_pct); }
    if ( g_veh.batt_low  && g_veh.soc_pct >= SOC_LOW_CLR)  { g_veh.batt_low  = false; }
    if (!g_veh.batt_crit && g_veh.soc_pct <  SOC_CRIT_PCT) { g_veh.batt_crit = true;  evlog_add(now, EV_BATT_CRIT, g_veh.soc_pct); }
    if ( g_veh.batt_crit && g_veh.soc_pct >= SOC_CRIT_CLR) { g_veh.batt_crit = false; }
}

/* ---------------- IMU task glue ---------------- */
static uint8_t ifail; static uint32_t last_reinit;
void imu_init_task(uint32_t now){
    if (mpu_init() == MPU_OK) { fault_clear(F_IMU, now); ifail = 0; }
    else fault_set(F_IMU, now);
    last_reinit = now;
}
void imu_task(uint32_t now){
    if (g_veh.faults & F_IMU) {                    /* degraded: try to re-initialise periodically */
        if (now - last_reinit >= IMU_REINIT_MS) imu_init_task(now);
        return;
    }
    mpu_sample_t s; int rc = mpu_read(&s);
    if (rc == MPU_OK) {
        ifail = 0; g_veh.ax_mg = s.ax_mg; g_veh.ay_mg = s.ay_mg; g_veh.az_mg = s.az_mg;
        g_veh.imu_ts_ms = now; g_veh.imu_seq++;
    } else if (rc == MPU_ERR_FROZEN) { fault_set(F_IMU, now); last_reinit = now; }
    else if (++ifail >= 3) { fault_set(F_IMU, now); last_reinit = now; }
}
