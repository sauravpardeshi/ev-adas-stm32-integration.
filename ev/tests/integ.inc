/* ===================== INTEGRATION TESTS (TC-I) ===================== */

static void integ_tests(void){
    /* I01 IMU -> ADAS brake event */
    { setup_sim(); a_t0 = 500; a_dur = 300; a_x = -800; boot(); run_until(1000); evlog_entry_t e; int f = find_event(EV_BRAKE, 0, &e);
      long lat = f ? (long)e.t_ms - 500 : -1;
      tc("TC-I01","Integ","R03","IMU->ADAS: -0.8 g pulse yields one logged BRAKE event within 100 ms", f && lat >= 0 && lat <= 100 && count_events(EV_BRAKE) == 1, "1 event, latency <= 100 ms", "events=%d latency=%ld ms arg=%d mg", count_events(EV_BRAKE), lat, f ? (int)e.arg : 0);
      metric("latency", "brake_event_ms", (double)lat); }
    /* I02 impact chain */
    { setup_sim(); a_t0 = 500; a_dur = 60; a_x = 3200; boot(); run_until(800); evlog_entry_t e; int f = find_event(EV_IMPACT, 0, &e); long lat = f ? (long)e.t_ms - 500 : -1;
      char d[9]; disp(d); char tb[96] = ""; frames_with("EVD,", tb, sizeof tb); unsigned s, sp, so, l, fl; long di, tt; sscanf(tb, "EVD,%u,%u,%u,%ld,%ld,%u,%x", &s, &sp, &so, &di, &tt, &l, &fl);
      int chain = g_veh.crash && sim.gpio[PIN_BUZZER] && sim.gpio[PIN_LED_CRIT] && sim.gpio[PIN_LED_WARN] && d[7] == 'P' && (fl & 1);
      sim_uart_inject("RST\n"); run_until(1000); int cleared = !g_veh.crash && !sim.gpio[PIN_BUZZER];
      tc("TC-I02","Integ","R03,R05,R06","Impact: detect<=100 ms -> latch -> buzzer+LEDs -> display 'P' -> telemetry flag -> RST clears", f && lat <= 100 && chain && cleared, "all true", "lat=%ld ms chain=%d display=[%s] flags=0x%X cleared=%d", lat, chain, d, fl, cleared);
      metric("latency", "impact_event_ms", (double)lat); }
    /* I03 range -> ADAS -> alert chain */
    { setup_sim(); sim.dist_mm = sc_approach; boot(); run_until(4600);
      long tA = first_edge(PIN_LED_ADV, 1, 0), tW = first_edge(PIN_LED_WARN, 1, 0), tC = first_edge(PIN_LED_CRIT, 1, 0);
      long cA = 300 + (3990 - 3000), cW = 300 + (3990 - 2000), cC = 300 + (3990 - 1000);   /* analytic threshold crossings (v = 1 m/s) */
      long lA = tA - cA, lW = tW - cW, lC = tC - cC; int ok = lA >= 0 && lA <= 250 && lW >= 0 && lW <= 250 && lC >= 0 && lC <= 250;
      char d[9]; disp(d);
      tc("TC-I03","Integ","R04","Range->ADAS->alert: ADV/WARN/CRIT LEDs follow TTC thresholds within 250 ms", ok && sim.gpio[PIN_BUZZER], "3 latencies in [0,250] ms, buzzer on", "ADV %ld ms, WARN %ld ms, CRIT %ld ms, display=[%s]", lA, lW, lC, d);
      metric("latency", "adv_ms", (double)lA); metric("latency", "warn_ms", (double)lW); metric("latency", "crit_ms", (double)lC); }
    /* I04 speed gating */
    { setup_sim(); s_spd = 0; s_dmin = 100; sim.dist_mm = sc_approach; boot(); run_until(4600);
      long tC = first_edge(PIN_LED_CRIT, 1, 0); long cC = 300 + (3990 - DIST_CRIT_MM); long lat = tC - cC;
      tc("TC-I04","Integ","R01,R04","Speed gate: parked (0 km/h) -> no ADV/WARN, proximity CRIT only at <=150 mm", count_edges(PIN_LED_ADV, 1) == 0 && count_edges(PIN_LED_WARN, 1) == 0 && tC > 0 && lat >= 0 && lat <= 250,
         "0 ADV, 0 WARN, CRIT within 250 ms of 150 mm", "ADV=%d WARN=%d CRIT latency=%ld ms", count_edges(PIN_LED_ADV, 1), count_edges(PIN_LED_WARN, 1), lat); }
    /* I05 battery consistency */
    { setup_sim();
      boot(); run_until(1500); char d[9]; disp(d); char sb[4] = {d[4], d[5], d[6], 0}; int dsoc = atoi(sb); char tb[96] = ""; frames_with("EVD,", tb, sizeof tb);
      unsigned s, sp, so, l, fl; long di, tt; sscanf(tb, "EVD,%u,%u,%u,%ld,%ld,%u,%x", &s, &sp, &so, &di, &tt, &l, &fl);
      tc("TC-I05","Integ","R02,R05,R06","Battery SoC identical in shared state, 7-seg display and UART telemetry (15.2 V pack)", g_veh.soc_pct == dsoc && dsoc == (int)so && so >= 60 && so <= 80, "equal across 3 views, 60..80 %", "state=%u display=%d telemetry=%u", g_veh.soc_pct, dsoc, so); }
    /* I06 range sensor stuck */
    { setup_sim(); boot(); run_until(500); sim.range_stuck = true; uint32_t t0 = hal_millis(); long tu = -1, td = -1; char d[9];
      while (hal_millis() < 1500) { run_until(hal_millis() + 10); if (tu < 0 && g_veh.level == ADAS_UNAVAIL) tu = hal_millis() - t0; disp(d); if (td < 0 && !strncmp(&d[4], "E02", 3)) td = hal_millis() - t0; }
      run_until(2500); int blink = count_edges(PIN_LED_WARN, 1) >= 2; char tb[96] = ""; frames_with("EVD,", tb, sizeof tb); unsigned s, sp, so, l, fl; long di, tt; sscanf(tb, "EVD,%u,%u,%u,%ld,%ld,%u,%x", &s, &sp, &so, &di, &tt, &l, &fl);
      sim.range_stuck = false; uint32_t t1 = hal_millis(); long tr = -1; while (hal_millis() < t1 + 1500) { run_until(hal_millis() + 10); if (tr < 0 && g_veh.level == ADAS_CLEAR && !(g_veh.faults & F_RANGE)) tr = hal_millis() - t1; }
      tc("TC-I06","Integ","R07","Stuck HC-SR04: ADAS UNAVAILABLE + 'E02' <=400 ms, LED blink, telemetry fault bit, auto-recover <=500 ms", tu >= 0 && tu <= 400 && td >= 0 && td <= 400 && blink && ((fl >> 4) & 2) && l == 4 && tr >= 0 && tr <= 500,
         "levels/codes <=400 ms, recover <=500 ms", "UNAVAIL %ld ms, E02 %ld ms, blink=%d, tlm level=%u flags=0x%X, recover %ld ms", tu, td, blink, l, fl, tr);
      metric("latency", "range_fault_detect_ms", (double)tu); metric("latency", "range_recover_ms", (double)tr); }
    /* I07 IMU NACK -> degraded mode, FCW still works */
    { setup_sim(); sim.dist_mm = sc_approach; s_t0 = 2000; boot(); run_until(500); sim.imu_nack = true; uint32_t t0 = hal_millis(); long tf = -1; char d[9]; long te = -1;
      while (hal_millis() < 1500) { run_until(hal_millis() + 10); if (tf < 0 && (g_veh.faults & F_IMU)) tf = hal_millis() - t0; disp(d); if (te < 0 && !strncmp(&d[4], "E01", 3)) te = hal_millis() - t0; }
      run_until(5300); int fcw_ok = first_edge(PIN_LED_CRIT, 1, 0) > 0;           /* approach started at 2000 ms while IMU is dead */
      sim.imu_nack = false; uint32_t t1 = hal_millis(); long tr = -1; while (hal_millis() < t1 + 1500) { run_until(hal_millis() + 10); if (tr < 0 && !(g_veh.faults & F_IMU)) tr = hal_millis() - t1; }
      tc("TC-I07","Integ","R07","IMU NACK: E01 <=200 ms, FCW still alerts (graceful degradation), IMU auto-recovers <=1 s", tf >= 0 && tf <= 200 && te >= 0 && te <= 200 && fcw_ok && tr >= 0 && tr <= 1000,
         "detect<=200, FCW ok, recover<=1000 ms", "fault %ld ms, E01 %ld ms, FCW=%d, recover %ld ms", tf, te, fcw_ok, tr);
      metric("latency", "imu_fault_detect_ms", (double)tf); metric("latency", "imu_recover_ms", (double)tr); }
    /* I08 stuck I2C bus -> recovery without fault */
    { setup_sim(); boot(); uint32_t rc0 = mpu_recover_count(); run_until(300); sim.i2c_bus_stuck = true; uint32_t seq0 = g_veh.imu_seq; run_until(800);
      uint32_t adv = g_veh.imu_seq - seq0; uint32_t rc = mpu_recover_count() - rc0;
      tc("TC-I08","Integ","R07,R08","Stuck I2C bus: 9-clock recovery restores IMU with no fault and no scheduler miss", !(g_veh.faults) && rc >= 1 && adv >= 20 && sched_total_misses() == 0,
         "no fault, >=1 recovery, >=20 samples/0.5 s, 0 misses", "faults=0x%X recoveries=%u samples=%u misses=%u", g_veh.faults, rc, adv, sched_total_misses()); }
    /* I09 frozen IMU data */
    { setup_sim(); boot(); run_until(300); sim.imu_frozen = true; uint32_t t0 = hal_millis(); long tf = -1, tr = -1;
      while (hal_millis() < t0 + 3000) { run_until(hal_millis() + 10); if (tf < 0 && (g_veh.faults & F_IMU)) tf = hal_millis() - t0; if (tf >= 0 && tr < 0 && !(g_veh.faults & F_IMU)) tr = hal_millis() - t0 - tf; }
      tc("TC-I09","Integ","R07","Frozen IMU data (25 identical frames) flagged <=750 ms, cleared by device reset+re-init <=1 s", tf >= 0 && tf <= 750 && tr >= 0 && tr <= 1000, "detect<=750, recover<=1000 ms", "detect %ld ms, recover %ld ms", tf, tr); }
    /* I10 UART under command load */
    { setup_sim(); sim.dist_mm = sc_approach; boot(); run_until(400); for (int k = 0; k < 3; k++) { sim_uart_inject("LOG?\n"); sim_uart_inject("STATUS?\n"); run_until(hal_millis() + 700); }
      run_until(4000); frame_scan_t fs = scan_frames(0); int logs = frames_with("ACK,LOG", 0, 0);
      tc("TC-I10","Integ","R06,R08","UART under command load: 3x LOG? + STATUS? with telemetry: no drops, no partial frames, no deadline miss", uart_tx_dropped() == 0 && fs.invalid == 0 && logs == 3 && sched_total_misses() == 0,
         "0 drops, 0 invalid, 3 LOG dumps, 0 misses", "drops=%u invalid=%d valid=%d logdumps=%d misses=%u", uart_tx_dropped(), fs.invalid, fs.valid, logs, sched_total_misses()); }
    /* I11 display self-heal */
    { setup_sim(); s_spd = 600; boot(); run_until(600); char good[9], now[9]; disp(good);
      sim.max_reg[0x09] = 0x00; sim.max_reg[0x0B] = 0x03; sim.max_reg[0x08] = 0x05; sim.max_reg[0x02] = 0x0A; uint32_t t0 = hal_millis(); long th = -1; disp(now); int corrupted = strcmp(good, now) != 0;
      while (hal_millis() < t0 + 3000) { run_until(hal_millis() + 10); disp(now); if (th < 0 && !strcmp(good, now)) th = hal_millis() - t0; }
      tc("TC-I11","Integ","R05","Corrupted MAX7219 registers (EMI) are restored by periodic refresh <= 2.1 s", corrupted && th >= 0 && th <= 2100, "healed <= 2100 ms", "corrupted=%d healed after %ld ms, shows [%s]", corrupted, th, now); metric("latency", "display_heal_ms", (double)th); }
    /* I12 IMU unplug / replug */
    { setup_sim(); boot(); run_until(300); sim.mpu_absent = true; uint32_t t0 = hal_millis(); long tf = -1; while (hal_millis() < t0 + 1500) { run_until(hal_millis() + 10); if (tf < 0 && (g_veh.faults & F_IMU)) tf = hal_millis() - t0; }
      sim.mpu_absent = false; uint32_t t1 = hal_millis(); long tr = -1; while (hal_millis() < t1 + 2000) { run_until(hal_millis() + 10); if (tr < 0 && !(g_veh.faults & F_IMU)) tr = hal_millis() - t1; }
      uint32_t s0 = g_veh.imu_seq; run_until(hal_millis() + 200);
      tc("TC-I12","Integ","R07","IMU unplug/replug: fault <=200 ms, hot-plug re-init <=1 s, samples resume", tf >= 0 && tf <= 200 && tr >= 0 && tr <= 1000 && g_veh.imu_seq - s0 >= 8, "detect<=200, recover<=1000, samples resume", "detect %ld ms, recover %ld ms, samples +%u", tf, tr, g_veh.imu_seq - s0); }
    /* I13 battery sense open circuit */
    { setup_sim(); boot(); run_until(500); sim.batt_adc_override = 0; uint32_t t0 = hal_millis(); long tf = -1; char d[9];
      while (hal_millis() < t0 + 800) { run_until(hal_millis() + 10); disp(d); if (tf < 0 && (g_veh.faults & F_BATT) && !strncmp(&d[4], "E03", 3)) tf = hal_millis() - t0; }
      sim.batt_adc_override = -1; uint32_t t1 = hal_millis(); long tr = -1; while (hal_millis() < t1 + 1000) { run_until(hal_millis() + 10); disp(d); if (tr < 0 && !(g_veh.faults & F_BATT) && strncmp(&d[4], "E03", 3)) tr = hal_millis() - t1; }
      tc("TC-I13","Integ","R02,R07","Battery sense line open: E03 within 500 ms (3 bad samples), recovers when restored", tf >= 0 && tf <= 500 && tr >= 0 && tr <= 500, "E03<=500 ms, recover<=500 ms", "E03 after %ld ms, recovered after %ld ms", tf, tr); }
    /* I14 STATUS consistency */
    { setup_sim(); boot(); run_until(300); sim.batt_adc_override = 0; run_until(900); sim_uart_clear(); sim_uart_inject("STATUS?\n"); run_until(1100);
      char b[96] = ""; frames_with("ACK,STATUS", b, sizeof b); char ver[16]; unsigned up, lvl, fl; unsigned long dr, re; int n = sscanf(b, "ACK,STATUS,%15[^,],%u,%u,%x,%lu,%lu", ver, &up, &lvl, &fl, &dr, &re);
      tc("TC-I14","Integ","R06,R07","STATUS? reply matches internal state (version, level, fault mask)", n == 6 && !strcmp(ver, FW_VERSION) && lvl == g_veh.level && fl == g_veh.faults && (fl & F_BATT), "6 fields, equal to state, F_BATT set", "[%s] state level=%u faults=0x%X", b, g_veh.level, g_veh.faults); }
}
