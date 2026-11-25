/* ===================== SYSTEM TESTS (TC-S) ===================== */
static FILE *trace;
static void hook_trace_s01(uint32_t t){
    if (!trace) return;
    int16_t ax, ay, az; sim.accel(t, &ax, &ay, &az);
    uint32_t w = (uint32_t)sim.range_width;
    fprintf(trace, "%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", t, sim.dist_mm(t), g_veh.target ? g_veh.dist_mm : -1, (int)(w * 10 / 58), g_veh.closing_mmps,
            g_veh.ttc_ms, g_veh.level, sim.gpio[PIN_BUZZER], sim.gpio[PIN_LED_CRIT], ax, sim.speed_dkmh(t));
}
/* emergency scenario: 1.5 m/s approach, driver brakes at t0+1900 ms with 0.8 s linear ramp-down */
static int32_t sc_emerg(uint32_t t){
    if (t < s_t0) return -1;
    double dt = (t - s_t0) / 1000.0, tb = 1.9, v = 1500.0, d;
    if (dt <= tb) d = s_d0 - v * dt; else { double u = dt - tb; if (u > 0.8) u = 0.8; d = s_d0 - v * tb - (v * u - v * u * u / 1.6); }
    return (int32_t)d;
}
static uint32_t s_pulse_t0;
static void sc_emerg_acc(uint32_t t, int16_t *x, int16_t *y, int16_t *z){ *x = (t >= s_pulse_t0 && t < s_pulse_t0 + 800) ? -800 : 0; *y = 0; *z = 1000; }

static int count_levels(int *seq, int max){ evlog_entry_t e; int k = 0; for (uint8_t i = 0; i < evlog_count() && k < max; i++) if (evlog_get(i, &e) && e.code == EV_ADAS_LEVEL) seq[k++] = (int)e.arg; return k; }

static int32_t sc_speed_profile(uint32_t t){ uint32_t s = t / 1000; return 300 + (int32_t)((s * 37) % 500); }
static int32_t sc_batt_discharge(uint32_t t){ double f = t / 220000.0; if (f > 1) f = 1; double v = 16400 - 4400 * f; if ((t / 1000) % 10 >= 8) v -= 100; return (int32_t)v; }
static void hook_batt_trace(uint32_t t){ if (trace && t % 1000 == 0) fprintf(trace, "%u,%d,%u,%u,%d,%d\n", t, sim.batt_mv(t), g_veh.batt_mv, g_veh.soc_pct, g_veh.batt_low, g_veh.batt_crit); }
static void rogue(uint32_t t){ (void)t; sim_advance_us(300000); }
static void hook_cmd_spam(uint32_t t){ if (t % 5000 == 0) sim_uart_inject("STATUS?\n"); }
static void hook_fuzz(uint32_t t){
    if (t % 50) return;
    static uint32_t r = 12345; char b[40]; int n = 0;
    for (int i = 0; i < 25; i++) { r = r * 1664525u + 1013904223u; unsigned x = r >> 24; b[n++] = (char)((x % 7 == 0) ? '\n' : (x % 11 == 0) ? 0xFF : (x < 128 ? 32 + x % 90 : x)); if (b[n-1] == 0) b[n-1] = 1; }
    b[n] = 0; sim_uart_inject(b);
    if (t % 2000 == 0) sim_uart_inject("STATUS?\n");
}
static int false_alarm_run(uint32_t seed, uint32_t dur_ms, uint32_t *events){
    setup_sim(); s_gap = 800; s_spd = 600; sim.dist_mm = sc_gap; sim.noise_mm = 3; sim.spike_permille = 10; sim.lost_echo_permille = 20; sim.seed = seed; boot(); run_until(dur_ms);
    *events = evlog_total() - 1; return count_edges(PIN_LED_ADV, 1) + count_edges(PIN_LED_WARN, 1) + count_edges(PIN_LED_CRIT, 1) + count_edges(PIN_BUZZER, 1);
}

static void system_tests(void){
    /* S01 emergency braking, end to end, with trace */
    { setup_sim(); s_t0 = 500; sim.dist_mm = sc_emerg; s_pulse_t0 = 500 + 1900; sim.accel = sc_emerg_acc; sim.noise_mm = 2; sim.spike_permille = 5; sim.lost_echo_permille = 10;
      trace = fopen("out/trace_s01_" TAG ".csv", "w"); if (trace) fprintf(trace, "t,dist_true,dist_filt,dist_raw,closing,ttc,level,buzzer,led_crit,ax,speed\n");
      boot(); set_hook(hook_trace_s01); int saw_buzz_in_crit = 0, d1_ok = 0; char d[9];
      while (hal_millis() < 9000) { run_until(hal_millis() + 10); if (g_veh.level == ADAS_CRIT) { saw_buzz_in_crit |= sim.gpio[PIN_BUZZER]; disp(d); d1_ok |= (d[7] == '3'); } }
      if (trace) { fclose(trace); trace = NULL; } g_hook = NULL;
      int seq[16]; int n = count_levels(seq, 16); int hasADV = -1, hasWARN = -1, hasCRIT = -1, clearEnd = 0;
      for (int i = 0; i < n; i++) { if (seq[i] == 1 && hasADV < 0) hasADV = i; if (seq[i] == 2 && hasWARN < 0) hasWARN = i; if (seq[i] == 3 && hasCRIT < 0) hasCRIT = i; }
      clearEnd = (n > 0 && seq[n - 1] == 0) && g_veh.level == ADAS_CLEAR;
      int order = hasADV >= 0 && hasWARN > hasADV && hasCRIT > hasWARN;
      evlog_entry_t eb, er; int fb = find_event(EV_BRAKE, 0, &eb), fr = find_event(EV_DRV_REACT, 0, &er);
      char tb[96]; int tf = frames_with("EVD,", tb, sizeof tb); int gaps; telem_continuity(&gaps); frame_scan_t fs = scan_frames(0);
      int saw_l3 = 0; { uint32_t i = 0; char body[96]; while (i < sim.uart_out_n) { uint32_t j = i; while (j + 1 < sim.uart_out_n && !(sim.uart_out[j] == '\r' && sim.uart_out[j+1] == '\n')) j++; if (j + 1 >= sim.uart_out_n) break;
          if (frame_ok((const char *)sim.uart_out + i, j - i, body, sizeof body) && !strncmp(body, "EVD,", 4)) { unsigned s, sp, so, l, fl; long di, tt; if (sscanf(body, "EVD,%u,%u,%u,%ld,%ld,%u,%x", &s, &sp, &so, &di, &tt, &l, &fl) == 7 && l == 3) saw_l3 = 1; } i = j + 2; } }
      tc("TC-S01","System","R03,R04,R05,R06","Emergency braking: ADV->WARN->CRIT in order, buzzer+display in CRIT, brake + driver-reaction logged, returns to CLEAR",
         order && saw_buzz_in_crit && d1_ok && fb && fr && clearEnd && saw_l3 && fs.invalid == 0 && gaps == 0 && sched_total_misses() == 0, "all true",
         "order=%d buzzer=%d display=%d brake=%d reaction=%ld ms clear=%d tlm_l3=%d invalid=%d gaps=%d misses=%u", order, saw_buzz_in_crit, d1_ok, fb, fr ? (long)er.arg : -1, clearEnd, saw_l3, fs.invalid, gaps, sched_total_misses());
      (void)tf; if (fr) metric("scenario", "driver_reaction_ms", (double)er.arg); }
    /* S02 nominal drive */
    { setup_sim(); sim.speed_dkmh = sc_speed_profile; sim.noise_mm = 3; sim.spike_permille = 10; sim.lost_echo_permille = 20; sim.seed = 777; boot(); run_until(60000);
      int gaps; int n = telem_continuity(&gaps); frame_scan_t fs = scan_frames(0);
      tc("TC-S02","System","R04,R06,R08","Nominal 60 s drive, no obstacle: zero events/alerts, 5 Hz telemetry +/-5 %, no gaps, no misses",
         evlog_total() == 1 && count_edges(PIN_BUZZER, 1) == 0 && n >= 285 && n <= 315 && gaps == 0 && fs.invalid == 0 && sched_total_misses() == 0 && g_veh.faults == 0, "1 event (boot), 285..315 frames", "events=%u frames=%d gaps=%d invalid=%d misses=%u faults=0x%X", evlog_total(), n, gaps, fs.invalid, sched_total_misses(), g_veh.faults); }
    /* S03 false alarms: constant-gap following with noise + spikes, 3 seeds x 60 s */
    { uint32_t tot_alerts = 0, tot_events = 0; for (uint32_t sd = 1; sd <= 3; sd++) { uint32_t ev; tot_alerts += (uint32_t)false_alarm_run(sd * 7919u, 60000, &ev); tot_events += ev; }
      tc("TC-S03","System","R04","False-alarm test: following at constant 800 mm gap, 3 mm noise, 1 % spikes, 2 % lost echoes, 3 x 60 s", tot_alerts == 0 && tot_events == 0, "0 alerts, 0 events", "alerts=%u events=%u", tot_alerts, tot_events);
      metric("filter", "false_alarm_edges_180s", (double)tot_alerts); metric("filter", "false_alarm_events_180s", (double)tot_events); }
    /* S04 collision with UART log evidence */
    { setup_sim(); s_t0 = 300; s_v = 1500; s_dmin = 100; sim.dist_mm = sc_approach; a_t0 = 300 + (3990 - 100) * 1000 / 1500 + 100; a_dur = 80; a_x = -3500; boot(); run_until(a_t0 + 300);
      evlog_entry_t e; int f = find_event(EV_IMPACT, 0, &e); long lat = f ? (long)e.t_ms - (long)a_t0 : -1; int latched_during = g_veh.crash && sim.gpio[PIN_BUZZER];
      run_until(a_t0 + 2500); int still = g_veh.crash && sim.gpio[PIN_BUZZER]; sim_uart_clear(); sim_uart_inject("LOG?\n"); run_until(hal_millis() + 600);
      int logs = frames_with("LOG,", 0, 0) , impact_in_log = 0; { uint32_t i = 0; char body[96]; while (i < sim.uart_out_n) { uint32_t j = i; while (j + 1 < sim.uart_out_n && !(sim.uart_out[j] == '\r' && sim.uart_out[j+1] == '\n')) j++; if (j + 1 >= sim.uart_out_n) break;
            if (frame_ok((const char *)sim.uart_out + i, j - i, body, sizeof body) && !strncmp(body, "LOG,", 4)) { int ix, c; unsigned long t; long a; if (sscanf(body, "LOG,%d,%lu,%d,%ld", &ix, &t, &c, &a) == 4 && c == EV_IMPACT && a > 3300 && a < 3700) impact_in_log = 1; } i = j + 2; } }
      sim_uart_inject("RST\n"); run_until(hal_millis() + 300);
      tc("TC-S04","System","R03,R06","Collision at 150 mm: crash latched <=100 ms, held 2.5 s, evidence readable via LOG?, cleared by RST", f && lat <= 100 && latched_during && still && impact_in_log && logs >= 3 && !g_veh.crash, "all true", "lat=%ld ms latched=%d held=%d log_entries=%d impact_logged=%d cleared=%d", lat, latched_during, still, logs, impact_in_log, !g_veh.crash); }
    /* S05 battery discharge 16.4 V -> 12.0 V with load-sag pulses */
    { setup_sim(); sim.batt_mv = sc_batt_discharge; trace = fopen("out/trace_batt_" TAG ".csv", "w"); if (trace) fprintf(trace, "t,pack_true_mv,pack_meas_mv,soc,low,crit\n"); boot(); set_hook(hook_batt_trace); run_until(230000);
      if (trace) { fclose(trace); trace = NULL; } g_hook = NULL; evlog_entry_t e1, e2; int fl = find_event(EV_BATT_LOW, 0, &e1), fc = find_event(EV_BATT_CRIT, 0, &e2);
      tc("TC-S05","System","R02","Battery discharge 16.4->12.0 V with 100 mV load-sag pulses: LOW and CRIT each raised exactly once (no chatter), final SoC 0 %", fl && fc && count_events(EV_BATT_LOW) == 1 && count_events(EV_BATT_CRIT) == 1 && g_veh.soc_pct == 0 && g_veh.batt_crit,
         "1 LOW, 1 CRIT, SoC=0", "LOW@%lus (SoC %d%%) CRIT@%lus (SoC %d%%) events %d/%d final SoC %u%%", fl ? e1.t_ms / 1000 : 0, fl ? (int)e1.arg : -1, fc ? e2.t_ms / 1000 : 0, fc ? (int)e2.arg : -1, count_events(EV_BATT_LOW), count_events(EV_BATT_CRIT), g_veh.soc_pct); }
    /* S06 fault storm */
    { setup_sim(); s_t0 = 100000; sim.dist_mm = sc_approach; boot(); run_until(1000); sim.imu_nack = true; sim.range_stuck = true; sim.batt_adc_override = 0; sim_uart_clear();
      for (int k = 0; k < 5; k++) { sim_uart_inject("STATUS?\n"); run_until(hal_millis() + 300); } run_until(2600); char d[9]; disp(d);
      int storm = (g_veh.faults & (F_IMU | F_RANGE | F_BATT)) == (F_IMU | F_RANGE | F_BATT); int topE01 = !strncmp(&d[4], "E01", 3) && d[7] == '-'; int fr = frames_with("EVD,", 0, 0);
      frame_scan_t fs = scan_frames(0); uint32_t mis = sched_total_misses(), wd = sim.wdt_resets;
      sim.imu_nack = false; sim.range_stuck = false; sim.batt_adc_override = -1; run_until(hal_millis() + 2000); disp(d);
      int rec = g_veh.faults == 0 && g_veh.level == ADAS_CLEAR && strncmp(&d[4], "E0", 2) != 0;
      tc("TC-S06","System","R07,R08","Fault storm (IMU + range + battery sense) 1.6 s: all 3 flagged, E01 shown, UART alive, no WDT/misses; full recovery <=2 s after removal",
         storm && topE01 && fr >= 7 && fs.invalid == 0 && mis == 0 && wd == 0 && rec, "all true", "storm=%d E01=%d frames=%d invalid=%d misses=%u wdt=%u recovered=%d display=[%s]", storm, topE01, fr, fs.invalid, mis, wd, rec, d); }
    /* S07 watchdog */
    { setup_sim(); boot(); run_until(3000); int w0 = (int)sim.wdt_resets; sched_add("rogue", rogue, 1000, 500); run_until(3600); uint32_t after = hal_millis();
      int resets = (int)sim.wdt_resets - w0; sim_uart_clear(); run_until(after + 400); evlog_entry_t e; int f = find_event(EV_BOOT, 0, &e); char d[9]; disp(d);
      int tfirst = frames_with("EVD,", 0, 0);
      tc("TC-S07","System","R09","Watchdog: 300 ms task hang -> reset, boot cause WDT logged, display & telemetry back within 400 ms, no reset in normal 3 s run", w0 == 0 && resets == 1 && f && e.arg == 1 && tfirst >= 1 && d[0] != '?',
         "0 resets normal, 1 reset after hang, cause=1", "normal_resets=%d hang_resets=%d boot_cause=%d frames_after=%d display=[%s]", w0, resets, f ? (int)e.arg : -1, tfirst, d); }
    /* S08/S09 10-minute soak + load budget */
    { setup_sim(); sim.speed_dkmh = sc_speed_profile; sim.noise_mm = 3; sim.spike_permille = 10; sim.lost_echo_permille = 20; sim.seed = 4242; boot(); set_hook(hook_cmd_spam); run_until(600000); g_hook = NULL;
      int gaps; int n = telem_continuity(&gaps); frame_scan_t fs = scan_frames(0); uint32_t jmax = 0, jcrit = 0; FILE *tf = fopen("out/task_stats_" TAG ".csv", "w"); if (tf) fprintf(tf, "task,period_ms,runs,max_exec_us,mean_exec_us,max_jitter_us,misses\n");
      for (uint32_t i = 0; i < sched_task_count(); i++) { const task_t *t = sched_task(i); if (t->max_jitter_us > jmax) jmax = t->max_jitter_us; if (t->period_us <= 10000 && t->max_jitter_us > jcrit) jcrit = t->max_jitter_us;
          if (tf) fprintf(tf, "%s,%u,%u,%u,%u,%u,%u\n", t->name, t->period_us / 1000, t->runs, t->max_exec_us, t->runs ? (uint32_t)(t->sum_exec_us / t->runs) : 0, t->max_jitter_us, t->misses); }
      if (tf) fclose(tf);
      tc("TC-S08","System","R06,R08","10-minute soak: 0 deadline misses, 0 TX drops, 0 WDT resets, 3000 telemetry frames +/-1 %, no sequence gaps, no faults",
         sched_total_misses() == 0 && uart_tx_dropped() == 0 && sim.wdt_resets == 0 && n >= 2970 && n <= 3030 && gaps == 0 && fs.invalid == 0 && g_veh.faults == 0, "all true", "misses=%u drops=%u wdt=%u frames=%d gaps=%d invalid=%d faults=0x%X", sched_total_misses(), uart_tx_dropped(), sim.wdt_resets, n, gaps, fs.invalid, g_veh.faults);
      tc("TC-S09","System","R08","Start jitter of 10 ms safety tasks (adas, alert, range, cli) <= 2 ms over the soak", jcrit <= 2000, "<= 2000 us", "max jitter %u us (all tasks %u us)", jcrit, jmax);
      double busy = (double)(sim.i2c_busy_us + sim.spi_busy_us + sim.adc_busy_us + sim.range_busy_us + (UART_BLOCKING ? sim.uart_busy_us : 0)) / 600000000.0 * 100.0;
      tc("TC-S10","System","R08","Peripheral-blocking CPU time (I2C+SPI+ADC+range[+UART if polled]) <= 5 % of wall time", busy <= 5.0, "<= 5 %", "%.2f %% (I2C %.2f, SPI %.3f, ADC %.3f, range %.3f)", busy,
         sim.i2c_busy_us / 6e6, sim.spi_busy_us / 6e6, sim.adc_busy_us / 6e6, sim.range_busy_us / 6e6);
      metric("soak", "busy_pct", busy); metric("soak", "spi_frames_per_s", sim.spi_frames / 600.0); metric("soak", "i2c_pct", sim.i2c_busy_us / 6e6); }
    /* S11 UART fuzz */
    { setup_sim(); sim.speed_dkmh = sc_speed_profile; boot(); set_hook(hook_fuzz); run_until(20000); g_hook = NULL; frame_scan_t fs = scan_frames(0); int gaps; int n = telem_continuity(&gaps);
      tc("TC-S11","System","R06","UART fuzz: 10 000 random/binary bytes at 500 B/s during operation: no crash, every output frame valid, telemetry >= 95 % delivered",
         fs.invalid == 0 && n >= 95 && sched_total_misses() == 0 && sim.wdt_resets == 0, "0 invalid frames, >=95/100 telemetry", "valid=%d invalid=%d telemetry=%d dropped=%u rx_errors=%u misses=%u", fs.valid, fs.invalid, n, uart_tx_dropped(), uart_rx_errors(), sched_total_misses()); }
    /* S12 alert latency distribution */
    { double sum = 0, mx = -1e9, mn = 1e9; int N = 40; double lat[40]; int ok = 1;
      for (int k = 0; k < N; k++) { setup_sim(); s_t0 = 300 + (uint32_t)k * 7; s_v = 1000; sim.dist_mm = sc_approach; sim.noise_mm = 3; sim.spike_permille = 10; sim.lost_echo_permille = 20; sim.seed = 500u + (uint32_t)k; boot();
          run_until(s_t0 + 3600); long t = first_edge(PIN_LED_CRIT, 1, 0); long cross = (long)s_t0 + (3990 - 1000); if (t < 0) { ok = 0; lat[k] = 9999; } else lat[k] = (double)(t - cross); sum += lat[k]; if (lat[k] > mx) mx = lat[k]; if (lat[k] < mn) mn = lat[k]; }
      double srt[40]; memcpy(srt, lat, sizeof srt); for (int i = 0; i < N; i++) for (int j = i + 1; j < N; j++) if (srt[j] < srt[i]) { double t = srt[i]; srt[i] = srt[j]; srt[j] = t; }
      double p95 = srt[(int)(0.95 * N) - 1];
      tc("TC-S12","System","R04","CRIT alert latency over 40 randomised trials (3 mm noise, 1 % spikes, random phase): worst case <= 250 ms", ok && mx <= 250, "max <= 250 ms", "min %.0f / mean %.0f / p95 %.0f / max %.0f ms", mn, sum / N, p95, mx);
      FILE *lf = fopen("out/latency_" TAG ".csv", "w"); if (lf) { fprintf(lf, "trial,latency_ms\n"); for (int k = 0; k < N; k++) fprintf(lf, "%d,%.0f\n", k, lat[k]); fclose(lf); } }
    /* S13 boot time */
    { setup_sim(); boot(); uint32_t t0 = hal_millis(); long td = -1, tt = -1; char d[9];
      while (hal_millis() < t0 + 1000) { run_until(hal_millis() + 1); disp(d); if (td < 0 && strcmp(d, "????????") && d[0] != '?') td = hal_millis() - t0; if (tt < 0 && frames_with("EVD,", 0, 0) >= 1) tt = hal_millis() - t0; }
      tc("TC-S13","System","R05,R06","Cold boot: display configured and first valid telemetry frame <= 500 ms after init", td >= 0 && td <= 500 && tt >= 0 && tt <= 500, "<= 500 ms", "display %ld ms, telemetry %ld ms", td, tt); metric("latency", "boot_first_frame_ms", (double)tt); }
}
