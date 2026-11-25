/* range.c - HC-SR04 acquisition, outlier rejection, closing-speed estimate */
#include "range.h"
#include "hal.h"
#include "vehicle.h"
#include "fault.h"
#include <string.h>

static int32_t iabs(int32_t x){ return x < 0 ? -x : x; }

/* alpha-beta tracker (alpha = 1/2, beta = 1/6) with innovation gating:
 * an echo that disagrees with the prediction by more than TRACK_GATE_MM is treated as an outlier and the
 * track coasts on its prediction; TRACK_MAX_REJ+1 consecutive outliers are accepted as a genuine change. */
bool track_update(track_t *t, int32_t z, int32_t dt){
    bool far = z >= RANGE_MAX_MM;
    if (dt <= 0 || dt > 200) t->valid = false;                 /* gap in data: restart */
    if (!t->valid) { if (far) return false; t->valid = true; t->d = z; t->v = 0; t->n = 1; t->rejects = 0; return true; }
    int32_t pred = t->d - t->v * dt / 1000, innov = z - pred;
    bool outlier = far || iabs(innov) > TRACK_GATE_MM;
    if (outlier) {
        if (++t->rejects > TRACK_MAX_REJ) {
            if (far) { t->valid = false; return false; }
            t->d = z; t->v = 0; t->n = 1; t->rejects = 0; return true;      /* new target / jump */
        }
        t->d = pred < RANGE_MIN_MM ? RANGE_MIN_MM : pred; return true;      /* coast */
    }
    t->rejects = 0;
    if (t->n == 1) { t->v = (t->d - z) * 1000 / dt; t->d = z; t->n = 2; return true; }   /* two-point start */
    t->d = pred + innov / 2;
    t->v = t->v - innov * 1000 / (6 * dt);
    return true;
}

#if RANGE_FILTER == 2
static int32_t win[3];
#endif
static uint8_t wn;
static bool     have_prev; static int32_t prev_d; static uint32_t prev_ts;
static int32_t  vel;                       /* closing speed, mm/s */
static track_t  trk;
static uint8_t  timeouts; static uint32_t invalid;
static uint32_t next_trig, t_trig; static bool waiting;

int32_t range_echo_to_mm(int32_t w){ return (w * 10) / 58; }
int32_t median3(int32_t a, int32_t b, int32_t c){
    if (a > b) { int32_t t=a; a=b; b=t; }
    if (b > c) { int32_t t=b; b=c; c=t; }
    if (a > b) { b=a; }
    return b;
}
uint32_t range_invalid_count(void){ return invalid; }

void range_init(void){ memset(&trk, 0, sizeof trk); wn=0; have_prev=false; vel=0; timeouts=0; waiting=false; next_trig=0; }

static void reset_filter(void){ wn=0; have_prev=false; vel=0; memset(&trk, 0, sizeof trk); }

static void handle_measurement(uint32_t ts, int32_t width_us, uint32_t now){
    if (width_us < 0) {                         /* echo never completed */
        if (++timeouts >= 3) { fault_set(F_RANGE, now); g_veh.target = false; reset_filter(); }
        return;
    }
    timeouts = 0; fault_clear(F_RANGE, now);
    int32_t raw;
    if (width_us > (RANGE_MAX_MM * 58) / 10) raw = RANGE_FAR_MM;      /* nothing within 4 m */
    else { raw = range_echo_to_mm(width_us); if (raw < RANGE_MIN_MM) { invalid++; return; } }

    int32_t d; bool target; int32_t dt = (int32_t)(ts - prev_ts);
#if RANGE_FILTER == 1
    target = track_update(&trk, raw, have_prev ? dt : 0);
    d = trk.valid ? trk.d : RANGE_FAR_MM; vel = (target && trk.n >= 2) ? trk.v : 0;
#else
#if RANGE_FILTER == 2
    if (wn < 3) win[wn++] = raw; else { win[0]=win[1]; win[1]=win[2]; win[2]=raw; }
    d = (wn < 3) ? raw : median3(win[0], win[1], win[2]);
#else
    d = raw;
#endif
    target = d < RANGE_MAX_MM;
    if (target && have_prev && prev_d < RANGE_MAX_MM && dt > 0) {
        int32_t v_raw = ((prev_d - d) * 1000) / dt;                  /* +ve = closing */
#if RANGE_FILTER == 2
        vel = (vel + v_raw) / 2;
#else
        vel = v_raw;
#endif
    } else vel = 0;
#endif
    prev_d = d; prev_ts = ts; have_prev = true;

    g_veh.target = target; g_veh.dist_mm = d; g_veh.closing_mmps = vel;
    g_veh.range_ts_ms = ts; g_veh.range_seq++;
}

void range_task(uint32_t now){
#if RANGE_BLOCKING
    if ((int32_t)(now - next_trig) >= 0) {
        next_trig = now + P_RANGE_MS;
        int32_t w = hal_range_measure_blocking_us(RANGE_ECHO_TIMEOUT_MS * 1000u);
        handle_measurement(now, w, hal_millis());
    }
#else
    if (!waiting) {
        if ((int32_t)(now - next_trig) >= 0) {
            hal_range_trigger(); t_trig = now; waiting = true;
            next_trig += P_RANGE_MS; if ((int32_t)(now - next_trig) > 0) next_trig = now + P_RANGE_MS;
        }
    } else {
        int32_t w = hal_range_poll_us();
        if (w >= 0)                                     { handle_measurement(t_trig, w, now); waiting = false; }
        else if ((now - t_trig) > RANGE_ECHO_TIMEOUT_MS){ handle_measurement(t_trig, -1, now); waiting = false; }
    }
#endif
}
