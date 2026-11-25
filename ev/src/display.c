/* display.c - 8-digit MAX7219 (SPI) dashboard: [SPD SPD SPD] [ ] [SOC SOC SOC] [ADAS] */
#include "display.h"
#include "hal.h"
#include "fault.h"

static uint8_t shadow[8]; static uint32_t last_full_ms;

static void max_write(uint8_t reg, uint8_t val){
    hal_spi_cs(true);
    hal_spi_xfer16((uint16_t)((reg << 8) | val));
    hal_spi_cs(false);
}
static void max_config(void){
    max_write(0x0C, 0x01);   /* normal operation (not shutdown) */
    max_write(0x0F, 0x00);   /* display-test off                */
    max_write(0x09, 0xFF);   /* code-B decode, all digits       */
    max_write(0x0B, 0x07);   /* scan 8 digits                   */
    max_write(0x0A, 0x08);   /* mid intensity                   */
}
static void put3(uint8_t *o, unsigned val){          /* 3 digits, leading-zero suppression */
    uint8_t h = (uint8_t)(val / 100 % 10), t = (uint8_t)(val / 10 % 10), u = (uint8_t)(val % 10);
    o[0] = h ? h : CB_BLANK;
    o[1] = (h || t) ? t : CB_BLANK;
    o[2] = u;
}
void display_format(const vehicle_t *v, uint8_t out[8]){
    put3(&out[0], v->speed_dkmh / 10u);
    out[3] = CB_BLANK;
    uint8_t code = fault_top_code(v->faults);
    if (code) { out[4] = CB_E; out[5] = 0; out[6] = code; }          /* E0n */
    else put3(&out[4], v->soc_pct);
    if (v->crash) out[7] = CB_P;
    else if (v->level == ADAS_UNAVAIL) out[7] = CB_DASH;
    else out[7] = (uint8_t)v->level;
}
void display_init(void){
    max_config();
    for (int i = 0; i < 8; i++) { shadow[i] = CB_BLANK; max_write((uint8_t)(8 - i), CB_BLANK); }
    last_full_ms = 0;
}
void display_task(uint32_t now){
    uint8_t d[8]; display_format(&g_veh, d);
    bool full = (now - last_full_ms) >= 2000;      /* periodic refresh heals EMI-corrupted registers */
    if (full) { max_config(); last_full_ms = now; }
    for (int i = 0; i < 8; i++)
        if (full || !DISPLAY_DIRTY || d[i] != shadow[i]) { max_write((uint8_t)(8 - i), d[i]); shadow[i] = d[i]; }
}
