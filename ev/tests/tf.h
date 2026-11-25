/* tf.h - tiny test framework + simulation harness helpers */
#ifndef TF_H
#define TF_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../port/hal_sim.h"
#include "../include/hal.h"
#include "../include/app.h"
#include "../include/vehicle.h"
#include "../include/evlog.h"
#include "../include/sched.h"
#include "../include/crc8.h"
#include "../include/uart_log.h"

#ifndef TAG
#define TAG "final"
#endif

typedef struct { char id[10], lvl[8], req[24], desc[160], exp[96], meas[240]; int pass; } tc_t;
static tc_t tcs[160]; static int ntc, npass;
static FILE *mf;

static void tc(const char *id, const char *lvl, const char *req, const char *desc, int pass, const char *exp, const char *fmt, ...){
    tc_t *t = &tcs[ntc++]; va_list ap; va_start(ap, fmt);
    snprintf(t->id, sizeof t->id, "%s", id); snprintf(t->lvl, sizeof t->lvl, "%s", lvl);
    snprintf(t->req, sizeof t->req, "%s", req); snprintf(t->desc, sizeof t->desc, "%s", desc);
    snprintf(t->exp, sizeof t->exp, "%s", exp); vsnprintf(t->meas, sizeof t->meas, fmt, ap); va_end(ap);
    t->pass = pass ? 1 : 0; if (pass) npass++;
    printf("[%s] %-7s %s\n", pass ? "PASS" : "FAIL", id, desc);
}
static void metric(const char *grp, const char *key, double v){ if (mf) fprintf(mf, "%s,%s,%.3f\n", grp, key, v); }

/* ---- run harness ---- */
static void (*g_hook)(uint32_t);
static uint32_t hook_next;
static int g_boots;
static void set_hook(void (*h)(uint32_t)){ g_hook = h; hook_next = (hal_millis() / 10 + 1) * 10; }
static void boot(void){ ev_init(); g_boots++; }
static void run_until(uint32_t t_ms){
    while (hal_millis() < t_ms) {
        ev_step();
        if (sim.reset_flag) { sim.reset_flag = false; sim.reset_cause = 1; ev_init(); }
        if (g_hook && hal_millis() >= hook_next) { g_hook(hal_millis()); hook_next += 10; }
    }
}

/* ---- UART frame helpers ---- */
static int frame_ok(const char *s, size_t n, char *body, size_t bsz){
    if (n < 5 || s[0] != '$') return 0;
    const char *st = memchr(s, '*', n); if (!st || (size_t)(s + n - st) != 3) return 0;
    size_t bl = (size_t)(st - s - 1); unsigned crc = 0; if (sscanf(st + 1, "%2x", &crc) != 1) return 0;
    if (crc8((const uint8_t *)s + 1, bl) != crc) return 0;
    if (body) { size_t c = bl < bsz - 1 ? bl : bsz - 1; memcpy(body, s + 1, c); body[c] = 0; }
    return 1;
}
typedef struct { int valid, invalid; } frame_scan_t;
static frame_scan_t scan_frames(uint32_t from){
    frame_scan_t r = {0, 0}; uint32_t i = from;
    while (i < sim.uart_out_n) {
        uint32_t j = i; while (j + 1 < sim.uart_out_n && !(sim.uart_out[j] == '\r' && sim.uart_out[j+1] == '\n')) j++;
        if (j + 1 >= sim.uart_out_n) { if (j > i) r.invalid++; break; }   /* unterminated tail = partial frame */
        if (frame_ok((const char *)sim.uart_out + i, j - i, 0, 0)) r.valid++; else r.invalid++;
        i = j + 2;
    }
    return r;
}
/* returns number of valid frames whose body starts with prefix; copies the last one into out */
static int frames_with(const char *prefix, char *out, size_t osz){
    int cnt = 0; uint32_t i = 0; char body[96]; size_t pl = strlen(prefix);
    while (i < sim.uart_out_n) {
        uint32_t j = i; while (j + 1 < sim.uart_out_n && !(sim.uart_out[j] == '\r' && sim.uart_out[j+1] == '\n')) j++;
        if (j + 1 >= sim.uart_out_n) break;
        if (frame_ok((const char *)sim.uart_out + i, j - i, body, sizeof body) && !strncmp(body, prefix, pl)) { cnt++; if (out) snprintf(out, osz, "%s", body); }
        i = j + 2;
    }
    return cnt;
}
/* telemetry sequence continuity: returns frame count, sets *gaps */
static int telem_continuity(int *gaps){
    int cnt = 0; long last = -1; *gaps = 0; uint32_t i = 0; char body[96];
    while (i < sim.uart_out_n) {
        uint32_t j = i; while (j + 1 < sim.uart_out_n && !(sim.uart_out[j] == '\r' && sim.uart_out[j+1] == '\n')) j++;
        if (j + 1 >= sim.uart_out_n) break;
        if (frame_ok((const char *)sim.uart_out + i, j - i, body, sizeof body) && !strncmp(body, "EVD,", 4)) {
            long s = strtol(body + 4, 0, 10); if (last >= 0 && s != last + 1) (*gaps)++; last = s; cnt++;
        }
        i = j + 2;
    }
    return cnt;
}

/* ---- GPIO edge helpers (times in ms) ---- */
static long first_edge(uint8_t pin, int level, uint32_t after_ms){
    for (uint32_t i = 0; i < sim.n_edges; i++)
        if (sim.edges[i].pin == pin && sim.edges[i].level == level && sim.edges[i].t_us / 1000u >= after_ms) return (long)(sim.edges[i].t_us / 1000u);
    return -1;
}
static int count_edges(uint8_t pin, int level){
    int c = 0; for (uint32_t i = 0; i < sim.n_edges; i++) if (sim.edges[i].pin == pin && sim.edges[i].level == level) c++; return c;
}
static bool find_event(uint8_t code, int nth, evlog_entry_t *out){
    evlog_entry_t e; int k = 0;
    for (uint8_t i = 0; i < evlog_count(); i++) if (evlog_get(i, &e) && e.code == code && k++ == nth) { *out = e; return true; }
    return false;
}
static int count_events(uint8_t code){ evlog_entry_t e; int k = 0; for (uint8_t i = 0; i < evlog_count(); i++) if (evlog_get(i, &e) && e.code == code) k++; return k; }

/* ---- scenario library ---- */
static int32_t  s_d0, s_v, s_dmin; static uint32_t s_t0;            /* linear approach  */
static int32_t sc_approach(uint32_t t){ if (t < s_t0) return -1; int64_t d = s_d0 - (int64_t)s_v * (t - s_t0) / 1000; return d < s_dmin ? s_dmin : (int32_t)d; }
static int32_t s_gap;                                                /* constant gap     */
static int32_t sc_gap(uint32_t t){ (void)t; return s_gap; }
static int32_t s_spd = 400;
static int32_t sc_spd(uint32_t t){ (void)t; return s_spd; }
static uint32_t a_t0, a_dur; static int16_t a_x, a_y, a_z = 1000;    /* accel pulse      */
static void sc_acc(uint32_t t, int16_t *x, int16_t *y, int16_t *z){
    if (t >= a_t0 && t < a_t0 + a_dur) { *x = a_x; *y = a_y; *z = a_z; } else { *x = 0; *y = 0; *z = 1000; }
}
static void scenario_reset(void){ s_d0 = 3990; s_v = 1000; s_dmin = 100; s_t0 = 300; s_gap = 800; s_spd = 400; a_t0 = 0xFFFFFF00u; a_dur = 0; a_x = 0; a_y = 0; a_z = 1000; }
static void setup_sim(void){ sim_reset(); scenario_reset(); sim.speed_dkmh = sc_spd; sim.accel = sc_acc; }
static void disp(char *b){ sim_display_string(b); }
#endif
