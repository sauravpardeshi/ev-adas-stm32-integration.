/* uart_log.c - framed telemetry (CRC-8), command line interface, TX ring buffer */
#include "uart_log.h"
#include "hal.h"
#include "ring.h"
#include "crc8.h"
#include "evlog.h"
#include "fault.h"
#include <stdio.h>
#include <string.h>

static uint8_t txmem[UART_TX_SIZE]; static ring_t tx;
static uint32_t dropped, rxerr, seq, nframes;
static char line[LINE_MAX]; static uint8_t ll; static bool overflow;
static int8_t dump_idx = -1;

static int tx_pull(void){ return ring_get(&tx); }
void uart_init(void){
    ring_init(&tx, txmem, UART_TX_SIZE); hal_uart_register_tx_source(tx_pull);
    dropped = rxerr = seq = nframes = 0; ll = 0; overflow = false; dump_idx = -1;
}
uint32_t uart_tx_dropped(void){ return dropped; }
uint32_t uart_rx_errors(void){ return rxerr; }
uint32_t telem_seq(void){ return seq; }

bool uart_send_frame(const char *body){
    char f[96]; size_t n = strlen(body);
    if (n > 80) n = 80;
    int len = snprintf(f, sizeof f, "$%.*s*%02X\r\n", (int)n, body, crc8((const uint8_t *)body, n));
    if (ring_free(&tx) < (uint16_t)len) { dropped++; return false; }   /* never queue a partial frame */
    for (int i = 0; i < len; i++) ring_put(&tx, (uint8_t)f[i]);
#if UART_BLOCKING
    int b; while ((b = ring_get(&tx)) >= 0) { uint8_t c = (uint8_t)b; hal_uart_tx_blocking(&c, 1); }
#else
    hal_uart_tx_irq_enable();
#endif
    nframes++; return true;
}

void telem_task(uint32_t now){
    (void)now; const vehicle_t *v = &g_veh; char b[72];
    uint16_t fl = (uint16_t)((v->crash ? 1 : 0) | (v->brake_active ? 2 : 0) | (v->batt_low ? 4 : 0) |
                             (v->batt_crit ? 8 : 0) | (uint16_t)(v->faults << 4));
    snprintf(b, sizeof b, "EVD,%lu,%u,%u,%ld,%ld,%u,%04X", (unsigned long)seq++, v->speed_dkmh, v->soc_pct,
             (long)(v->target ? v->dist_mm : -1), (long)v->ttc_ms, (unsigned)v->level, fl);
    uart_send_frame(b);
}

/* ---------------- command interpreter ---------------- */
static void exec_cmd(const char *c, uint32_t now){
    char b[72];
    if      (!strcmp(c, "STATUS?")) {
        snprintf(b, sizeof b, "ACK,STATUS,%s,%lu,%u,%X,%lu,%lu", FW_VERSION, (unsigned long)(now / 1000),
                 (unsigned)g_veh.level, g_veh.faults, (unsigned long)dropped, (unsigned long)rxerr);
        uart_send_frame(b);
    } else if (!strcmp(c, "LOG?")) { dump_idx = 0; }
    else if (!strcmp(c, "RST"))    { g_veh.crash = false; evlog_add(now, EV_RST_CMD, 0); uart_send_frame("ACK,RST"); }
    else if (!strcmp(c, "HELP"))   { uart_send_frame("ACK,HELP,STATUS?|LOG?|RST"); }
    else                           { rxerr++; uart_send_frame("NAK,UNKNOWN"); }
}

void cli_task(uint32_t now){
    int ch; int guard = 16;                                  /* bounded work per call */
    while (guard-- && (ch = hal_uart_rx_byte()) >= 0) {
        if (ch == '\r' || ch == '\n') {
            if (overflow) { rxerr++; uart_send_frame("NAK,OVERFLOW"); }
            else if (ll) { line[ll] = 0; exec_cmd(line, now); }
            ll = 0; overflow = false;
        } else if (ll < LINE_MAX - 1) line[ll++] = (char)ch;
        else overflow = true;
    }
    /* LOG? streams one entry per call so a 16-entry dump cannot overflow the TX ring */
    if (dump_idx >= 0 && ring_free(&tx) > 64) {
        evlog_entry_t e; char b[72];
        if (evlog_get((uint8_t)dump_idx, &e)) {
            snprintf(b, sizeof b, "LOG,%d,%lu,%u,%ld", dump_idx, (unsigned long)e.t_ms, e.code, (long)e.arg);
            uart_send_frame(b); dump_idx++;
        } else { snprintf(b, sizeof b, "ACK,LOG,%u", evlog_count()); uart_send_frame(b); dump_idx = -1; }
    }
}
