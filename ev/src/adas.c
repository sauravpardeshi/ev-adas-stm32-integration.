/* adas.c - forward-collision warning (TTC), harsh-brake and impact detection */
#include "adas.h"
#include "config.h"
#include "evlog.h"

static adas_level_t cur;
static uint8_t esc, dn; static adas_level_t pend_dn;
#if !ESC_VOTE
static adas_level_t pend_up;
#endif
static adas_level_t raws[3]; static uint8_t nraw;
static uint32_t last_range_seq, last_imu_seq;
static uint8_t brake_cnt; static bool brake_armed = true;
static uint32_t warn_onset_ms; static bool warn_open; static uint32_t reaction_ms;

adas_level_t adas_classify(bool target, int32_t d, int32_t v, uint16_t spd, int32_t *ttc_out){
    int32_t ttc = -1; adas_level_t raw = ADAS_CLEAR;
    if (target) {
        if (v >= MIN_CLOSING_MMPS) ttc = (int32_t)(((int64_t)d * 1000) / v);
        if (d <= DIST_CRIT_MM) raw = ADAS_CRIT;                       /* proximity: always on */
        else if (spd >= ADAS_MIN_SPEED_DKMH && ttc >= 0) {
            if      (ttc <= TTC_CRIT_MS) raw = ADAS_CRIT;
            else if (ttc <= TTC_WARN_MS) raw = ADAS_WARN;
            else if (ttc <= TTC_ADV_MS)  raw = ADAS_ADV;
        }
    }
    if (ttc_out) *ttc_out = ttc;
    return raw;
}

void adas_init(void){
    cur = ADAS_CLEAR; esc = dn = 0; nraw = 0; last_range_seq = last_imu_seq = 0;
    brake_cnt = 0; brake_armed = true; warn_open = false; reaction_ms = 0;
    g_veh.level = ADAS_CLEAR; g_veh.ttc_ms = -1; g_veh.crash = false;
}
uint32_t adas_last_reaction_ms(void){ return reaction_ms; }

static void set_level(adas_level_t l, uint32_t now){
    if (l == cur) return;
    adas_level_t old = cur; cur = l; g_veh.level = l;
    evlog_add(now, EV_ADAS_LEVEL, l);
    if (l >= ADAS_WARN && old < ADAS_WARN && l != ADAS_UNAVAIL) { warn_open = true; warn_onset_ms = now; }
    if (l == ADAS_CLEAR) warn_open = false;
}

static void imu_events(uint32_t now){
    vehicle_t *v = &g_veh;
    int64_t m2 = (int64_t)v->ax_mg*v->ax_mg + (int64_t)v->ay_mg*v->ay_mg + (int64_t)v->az_mg*v->az_mg;
    if (!v->crash && m2 > (int64_t)IMPACT_MG * IMPACT_MG) {
        v->crash = true;
        int32_t mag = 0; while ((int64_t)(mag + 1) * (mag + 1) <= m2) mag++;   /* integer sqrt, rare path */
        evlog_add(now, EV_IMPACT, mag);
    }
    if (v->ax_mg <= -BRAKE_MG) {
        if (brake_cnt < 255) brake_cnt++;
        if (brake_cnt >= BRAKE_CONFIRM && brake_armed) {
            brake_armed = false; v->brake_active = true;
            evlog_add(now, EV_BRAKE, -v->ax_mg);
            if (warn_open) { reaction_ms = now - warn_onset_ms; evlog_add(now, EV_DRV_REACT, (int32_t)reaction_ms); warn_open = false; }
        }
    } else if (v->ax_mg > -BRAKE_REARM_MG) { brake_cnt = 0; brake_armed = true; v->brake_active = false; }
    else brake_cnt = 0;
}

void adas_task(uint32_t now){
    vehicle_t *v = &g_veh;
    /* ---- inertial events (only from fresh, healthy IMU data) ---- */
    if (!(v->faults & F_IMU) && v->imu_seq != last_imu_seq) { last_imu_seq = v->imu_seq; imu_events(now); }

    /* ---- FCW: fail-safe if range data is missing or stale ---- */
    bool range_ok = !(v->faults & F_RANGE) && (now - v->range_ts_ms) <= RANGE_STALE_MS;
    if (!range_ok) { esc = dn = 0; nraw = 0; v->ttc_ms = -1; set_level(ADAS_UNAVAIL, now); return; }
    if (cur == ADAS_UNAVAIL) { cur = ADAS_CLEAR; v->level = ADAS_CLEAR; evlog_add(now, EV_ADAS_LEVEL, 0); }

    if (v->range_seq == last_range_seq) return;          /* evaluate once per new range sample */
    last_range_seq = v->range_seq;
    int32_t ttc; adas_level_t raw = adas_classify(v->target, v->dist_mm, v->closing_mmps, v->speed_dkmh, &ttc);
    v->ttc_ms = ttc;
    if (nraw < 3) raws[nraw++] = raw; else { raws[0] = raws[1]; raws[1] = raws[2]; raws[2] = raw; }

    /* ---- escalation ---- */
#if ESC_VOTE
    /* 2-of-last-3 vote: robust to one dropped/odd sample. The new level is the LOWEST of the
       exceeding samples, so a single high outlier can never raise the level on its own. */
    uint8_t up = 0; adas_level_t m = ADAS_UNAVAIL;
    for (uint8_t i = 0; i < nraw; i++) if (raws[i] > cur) { up++; if (raws[i] < m) m = raws[i]; }
    if (up >= ESC_CONFIRM) { dn = 0; set_level(m, now); return; }
    if (raw > cur) { dn = 0; return; }                  /* pending: waiting for a second vote */
#else
    if (raw > cur) {                                    /* 2 consecutive samples */
        dn = 0; pend_up = (esc == 0) ? raw : (raw < pend_up ? raw : pend_up);
        if (++esc >= ESC_CONFIRM) { set_level(pend_up, now); esc = 0; }
        return;
    }
    esc = 0;
#endif
    /* ---- de-escalation: DEESC_CONFIRM consecutive lower samples ---- */
    if (raw < cur) {
        pend_dn = (dn == 0) ? raw : (raw > pend_dn ? raw : pend_dn);
        if (++dn >= DEESC_CONFIRM) { set_level(pend_dn, now); dn = 0; }
    } else dn = 0;
}
