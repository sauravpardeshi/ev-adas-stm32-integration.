/* ===================== UNIT TESTS (TC-U) ===================== */
#include "../include/mpu6050.h"
#include "../include/range.h"
#include "../include/sensors.h"
#include "../include/adas.h"
#include "../include/display.h"
#include "../include/fault.h"
#include "../include/ring.h"

static char code_char(uint8_t c){ static const char m[16] = {'0','1','2','3','4','5','6','7','8','9','-','E','H','L','P',' '}; return m[c & 15]; }
static void fmt_str(const vehicle_t *v, char *o){ uint8_t d[8]; display_format(v, d); for (int i = 0; i < 8; i++) o[i] = code_char(d[i]); o[8] = 0; }
static void feed(uint32_t now, bool tgt, int32_t d, int32_t v){
    g_veh.target = tgt; g_veh.dist_mm = d; g_veh.closing_mmps = v; g_veh.range_ts_ms = now; g_veh.range_seq++; g_veh.speed_dkmh = 400; adas_task(now);
}

static void unit_tests(void){
    /* U01 CRC-8 */
    { uint8_t c = crc8((const uint8_t *)"123456789", 9);
      tc("TC-U01","Unit","R06","CRC-8 (poly 0x07) check value for \"123456789\"", c == 0xF4, "0xF4", "0x%02X", c); }
    /* U02 ring buffer */
    { uint8_t m[8]; ring_t r; ring_init(&r, m, 8); int ok = 1;
      for (int i = 0; i < 7; i++) ok &= ring_put(&r, (uint8_t)i);
      int full_rejected = !ring_put(&r, 99);
      for (int i = 0; i < 7; i++) ok &= (ring_get(&r) == i);
      ok &= (ring_get(&r) == -1);
      for (int k = 0; k < 40; k++) { ring_put(&r, (uint8_t)k); ok &= (ring_get(&r) == k); }   /* wrap-around */
      tc("TC-U02","Unit","R06","Ring buffer: full rejection, FIFO order, wrap-around", ok && full_rejected, "all true", "ok=%d full_rejected=%d", ok, full_rejected); }
    /* U03 MPU init */
    { setup_sim(); memset(&g_veh, 0, sizeof g_veh); int rc = mpu_init();
      tc("TC-U03","Unit","R03","MPU-6050 init wakes device and selects +/-4 g", rc == 0 && sim.mpu_reg[0x6B] == 0 && sim.mpu_reg[0x1C] == 0x08,
         "rc=0 PWR=0x00 CFG=0x08", "rc=%d PWR=0x%02X CFG=0x%02X", rc, sim.mpu_reg[0x6B], sim.mpu_reg[0x1C]); }
    /* U04 MPU conversion */
    { setup_sim(); mpu_init(); mpu_sample_t s; a_t0 = 0; a_dur = 100000; a_x = -800; a_y = 0; a_z = 1000;
      int rc = mpu_read(&s); int e = abs(s.ax_mg + 800) > abs(s.az_mg - 1000) ? abs(s.ax_mg + 800) : abs(s.az_mg - 1000);
      tc("TC-U04","Unit","R03","MPU-6050 raw->mg conversion (ax=-800 mg, az=1000 mg)", rc == 0 && e <= 20, "error <= 20 mg", "ax=%d az=%d err=%d mg", s.ax_mg, s.az_mg, e); }
    /* U05 bad WHO_AM_I */
    { setup_sim(); sim.who_am_i = 0x70; int rc = mpu_init();
      tc("TC-U05","Unit","R07","MPU-6050 init rejects wrong WHO_AM_I", rc == MPU_ERR_ID, "rc=-2", "rc=%d", rc); }
    /* U06 retry */
    { setup_sim(); mpu_init(); mpu_sample_t s; uint32_t r0 = mpu_retry_count(); sim.nack_once = 1; int rc = mpu_read(&s);
      tc("TC-U06","Unit","R07","Transient I2C NACK is retried transparently", rc == 0 && mpu_retry_count() == r0 + 1, "rc=0, 1 retry", "rc=%d retries=%u", rc, mpu_retry_count() - r0); }
    /* U07 SoC LUT */
    { uint32_t in[] = {4200, 3000, 3600, 3650, 3300, 3950, 4300, 2500}; uint8_t ex[] = {100, 0, 40, 47, 10, 85, 100, 0}; int ok = 1; char m[80] = "";
      for (int i = 0; i < 8; i++) { uint8_t g = soc_from_cell_mv(in[i]); ok &= (g == ex[i]); sprintf(m + strlen(m), "%u,", g); }
      tc("TC-U07","Unit","R02","SoC look-up table interpolation and clamping (8 points)", ok, "100,0,40,47,10,85,100,0", "%s", m); }
    /* U08 divider conversion */
    { double worst = 0; uint32_t v[] = {12000, 14000, 16000, 16800};
      for (int i = 0; i < 4; i++) { uint32_t pin = v[i] * 10 / 57; uint16_t c = (uint16_t)(pin * 4095 / 3300); double e = ((double)batt_adc_to_pack_mv(c) - v[i]) * 100.0 / v[i]; if (e < 0) e = -e; if (e > worst) worst = e; }
      tc("TC-U08","Unit","R02","Battery ADC->pack mV conversion error over 12-16.8 V", worst <= 1.5, "<= 1.5 %", "worst %.2f %%", worst); }
    /* U09 speed */
    { uint16_t a = speed_from_adc(0), b = speed_from_adc(2048), c = speed_from_adc(4095);
      tc("TC-U09","Unit","R01","Speed ADC->deci-km/h at 0, mid, full scale", a == 0 && abs(b - 600) <= 10 && c == 1200, "0, 600+/-10, 1200", "%u, %u, %u", a, b, c); }
    /* U10 median3 */
    { int ok = median3(5,100,6) == 6 && median3(9000,10,11) == 11 && median3(1,2,3) == 2 && median3(3,2,1) == 2 && median3(7,7,1) == 7;
      tc("TC-U10","Unit","R04","median-of-3 outlier rejection (5 vectors)", ok, "all true", "ok=%d", ok); }
    /* U11 echo conversion */
    { int32_t mm = range_echo_to_mm(5830);
      tc("TC-U11","Unit","R04","HC-SR04 echo width -> mm (5830 us = 1000 mm)", abs(mm - 1000) <= 10, "1000 +/- 10 mm", "%d mm", mm); }
    /* U12 ADAS classification table */
    { struct { bool t; int32_t d, v; uint16_t s; adas_level_t e; } c[] = {
        {1,3000,1000,400,ADAS_ADV},{1,2000,1000,400,ADAS_WARN},{1,1000,1000,400,ADAS_CRIT},{1,3500,1000,400,ADAS_CLEAR},
        {1,2000,100,400,ADAS_CLEAR},{1,140,0,400,ADAS_CRIT},{1,1500,1000,0,ADAS_CLEAR},{1,140,0,0,ADAS_CRIT},{0,500,1000,400,ADAS_CLEAR} };
      int ok = 0, n = (int)(sizeof c / sizeof c[0]); for (int i = 0; i < n; i++) { int32_t t; ok += (adas_classify(c[i].t, c[i].d, c[i].v, c[i].s, &t) == c[i].e); }
      tc("TC-U12","Unit","R04","FCW classification table (ADV/WARN/CRIT, min-closing, speed gate, proximity)", ok == n, "9/9 correct", "%d/%d", ok, n); }
    /* U13 debounce */
    { setup_sim(); memset(&g_veh, 0, sizeof g_veh); evlog_init(); adas_init(); uint32_t t = 1000; int ok = 1;
      feed(t += 50, 1, 3500, 0); feed(t += 50, 1, 500, 1000); ok &= (g_veh.level == ADAS_CLEAR);   /* single-sample glitch  */
      feed(t += 50, 1, 3500, 0); ok &= (g_veh.level == ADAS_CLEAR);
      feed(t += 50, 1, 500, 1000); feed(t += 50, 1, 500, 1000); ok &= (g_veh.level == ADAS_CRIT);   /* 2 consecutive -> escalate */
      feed(t += 50, 1, 3500, 0); ok &= (g_veh.level == ADAS_CRIT);                                   /* hold for de-escalation */
      feed(t += 50, 1, 3500, 0); feed(t += 50, 1, 3500, 0); feed(t += 50, 1, 3500, 0); ok &= (g_veh.level == ADAS_CLEAR);
      tc("TC-U13","Unit","R04","ADAS debounce: glitch ignored, 2-sample escalate, 4-sample de-escalate", ok, "all true", "ok=%d", ok); }
    /* U14 MAX7219 init */
    { setup_sim(); display_init();
      int ok = sim.max_reg[0x0C] == 1 && sim.max_reg[0x09] == 0xFF && sim.max_reg[0x0B] == 7 && sim.max_reg[0x0A] == 8 && sim.max_reg[0x0F] == 0;
      tc("TC-U14","Unit","R05","MAX7219 init registers (shutdown, decode, scan limit, intensity, test)", ok && sim.spi_proto_errs == 0, "01,FF,07,08,00 / 0 SPI errors", "%02X,%02X,%02X,%02X,%02X / %u errs",
         sim.max_reg[0x0C], sim.max_reg[0x09], sim.max_reg[0x0B], sim.max_reg[0x0A], sim.max_reg[0x0F], sim.spi_proto_errs); }
    /* U15 display formatting */
    { vehicle_t v; char s1[9], s2[9], s3[9], s4[9], s5[9]; memset(&v, 0, sizeof v);
      v.speed_dkmh = 1200; v.soc_pct = 100; v.level = ADAS_CRIT; fmt_str(&v, s1);
      memset(&v, 0, sizeof v); v.speed_dkmh = 0; v.soc_pct = 5; fmt_str(&v, s2);
      v.faults = F_RANGE; fmt_str(&v, s3); v.faults = 0; v.crash = true; fmt_str(&v, s4); v.crash = false; v.level = ADAS_UNAVAIL; v.speed_dkmh = 457; v.soc_pct = 64; fmt_str(&v, s5);
      int ok = !strcmp(s1, "120 1003") && !strcmp(s2, "  0   50") && !strcmp(s3, "  0 E020") && !strcmp(s4, "  0   5P") && !strcmp(s5, " 45  64-");
      tc("TC-U15","Unit","R05","Display digit formatting (zero suppression, E-codes, P/- symbols)", ok, "5/5 strings match", "[%s][%s][%s][%s][%s]", s1, s2, s3, s4, s5); }
    /* U16 SPI protocol checker */
    { setup_sim(); memset(&g_veh, 0, sizeof g_veh); display_init(); display_task(5000); uint32_t errs = sim.spi_proto_errs, fr = sim.spi_frames;
      hal_spi_xfer16(0x0101);                         /* deliberate violation: no chip-select */
      tc("TC-U16","Unit","R05","SPI framing: every frame = CS low, 16 clocks, CS high (checker also catches a violation)", errs == 0 && fr >= 13 && sim.spi_proto_errs == 1,
         "0 errs in normal use; violation detected", "errs=%u frames=%u after_violation=%u", errs, fr, sim.spi_proto_errs); }
    /* U17 telemetry frame */
    { setup_sim(); boot(); run_until(450); char b[96] = ""; int n = frames_with("EVD,", b, sizeof b);
      unsigned seq, spd, soc, lvl, fl; long dist, ttc; int f = sscanf(b, "EVD,%u,%u,%u,%ld,%ld,%u,%x", &seq, &spd, &soc, &dist, &ttc, &lvl, &fl);
      frame_scan_t fs = scan_frames(0);
      tc("TC-U17","Unit","R06","Telemetry frame: 7 fields, valid CRC-8, CRLF terminated", n >= 2 && f == 7 && fs.invalid == 0, ">=2 frames, 7 fields, 0 invalid", "frames=%d fields=%d invalid=%d last=[%s]", n, f, fs.invalid, b); }
    /* U18 command parser */
    { setup_sim(); boot(); run_until(100); sim_uart_clear();
      sim_uart_inject("STATUS?\n"); run_until(200); int a = frames_with("ACK,STATUS", 0, 0);
      sim_uart_inject("FOO\n"); run_until(300); int b = frames_with("NAK,UNKNOWN", 0, 0);
      sim_uart_inject("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"); run_until(400); int c = frames_with("NAK,OVERFLOW", 0, 0);
      sim_uart_inject("HELP\r\n"); run_until(500); int d = frames_with("ACK,HELP", 0, 0);
      tc("TC-U18","Unit","R06","Command parser: STATUS?, unknown command, 44-char overflow line, HELP with CRLF", a == 1 && b == 1 && c == 1 && d == 1, "1,1,1,1", "%d,%d,%d,%d", a, b, c, d); }
    /* U19 fault priority */
    { int ok = fault_top_code(F_RANGE | F_SCHED) == 2 && fault_top_code(F_IMU | F_RANGE) == 1 && fault_top_code(0) == 0 && fault_top_code(F_SCHED) == 4 && fault_top_code(F_BATT | F_SCHED) == 3;
      tc("TC-U19","Unit","R07","Fault priority -> display code (E01 > E02 > E03 > E04)", ok, "all true", "ok=%d", ok); }
    /* U20 event log wrap */
    { evlog_init(); for (int i = 0; i < 20; i++) evlog_add((uint32_t)i, EV_BRAKE, i); evlog_entry_t e0, e15;
      evlog_get(0, &e0); evlog_get(15, &e15); int ok = evlog_count() == 16 && evlog_total() == 20 && e0.arg == 4 && e15.arg == 19 && !evlog_get(16, &e0);
      tc("TC-U20","Unit","R06","Event log keeps the newest 16 of 20 entries in order", ok, "count=16 oldest=4 newest=19", "count=%u oldest=%d newest=%d", evlog_count(), (int)e0.arg, (int)e15.arg); }
    /* U21 tracker spike rejection */
    { track_t t; memset(&t, 0, sizeof t); int32_t d = 3000; for (int i = 0; i < 10; i++) { track_update(&t, d, 50); d -= 50; }
      int32_t before = t.d; track_update(&t, 600, 50); int32_t after = t.d; /* gross outlier at 600 mm while truth ~2500 */
      tc("TC-U21","Unit","R04","Alpha-beta tracker coasts through an 800+ mm outlier (gate 300 mm)", abs(after - (before - 50)) <= 25, "coasts within 25 mm of prediction", "before=%d after=%d (pred %d)", before, after, before - 50); }
    /* U22 tracker convergence and re-acquisition */
    { track_t t; memset(&t, 0, sizeof t); int32_t d = 3500; for (int i = 0; i < 12; i++) { track_update(&t, d, 50); d -= 50; }   /* 1000 mm/s */
      int okv = abs(t.v - 1000) <= 100; int ok = okv; for (int i = 0; i < 4; i++) track_update(&t, 600, 50);     /* 4 consecutive: genuine jump */
      ok &= (t.valid && abs(t.d - 600) <= 5); for (int i = 0; i < 5; i++) track_update(&t, 9999, 50); ok &= !t.valid;
      tc("TC-U22","Unit","R04","Tracker: velocity converges (1000+/-100 mm/s), accepts a persistent jump, drops target on persistent no-echo", ok, "all true", "v=%d valid_after_far=%d", t.v, t.valid); }
}
