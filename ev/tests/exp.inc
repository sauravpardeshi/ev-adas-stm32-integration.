/* ===================== INTEGRATION EXPERIMENTS (metrics for the report) ===================== */
static int32_t sc_toggle(uint32_t t){ return (t % 10000) < 5000 ? 1500 : -1; }
static void hook_cmds(uint32_t t){ if (t % 500 == 0) sim_uart_inject("STATUS?\n"); if (t % 2000 == 0) sim_uart_inject("LOG?\n"); }
static void hook_stuck_each_second(uint32_t t){ if (t % 1000 == 0) sim.i2c_bus_stuck = true; }

static void experiments(void){
    /* EXP-1: scheduler timing under mixed load (range target present half the time, command traffic) */
    setup_sim(); sim.dist_mm = sc_toggle; sim.noise_mm = 3; sim.speed_dkmh = sc_speed_profile; boot(); set_hook(hook_cmds); run_until(20000); g_hook = NULL;
    for (uint32_t i = 0; i < sched_task_count(); i++) { const task_t *t = sched_task(i); char k[48];
        snprintf(k, sizeof k, "%s_max_jitter_us", t->name); metric("exp_sched", k, t->max_jitter_us);
        snprintf(k, sizeof k, "%s_max_exec_us", t->name);   metric("exp_sched", k, t->max_exec_us);
        snprintf(k, sizeof k, "%s_misses", t->name);        metric("exp_sched", k, t->misses); }
    metric("exp_sched", "total_misses", sched_total_misses()); metric("exp_sched", "tx_dropped", uart_tx_dropped());
    metric("exp_sched", "range_busy_pct", sim.range_busy_us / 2e5); metric("exp_sched", "uart_busy_pct", sim.uart_busy_us / 2e5);
    { frame_scan_t fs = scan_frames(0); metric("exp_sched", "invalid_frames", fs.invalid); int gaps; metric("exp_sched", "telemetry_frames", telem_continuity(&gaps)); }
    /* EXP-2: I2C bus timeout 5 ms vs 2 ms when the bus is stuck once per second */
    uint32_t tos[2] = {5000, 2000};
    for (int i = 0; i < 2; i++) { setup_sim(); sim.i2c_timeout_us = tos[i]; sim.dist_mm = sc_toggle; boot(); set_hook(hook_stuck_each_second); run_until(20000); g_hook = NULL;
        uint32_t jm = 0, mis = 0; for (uint32_t k = 0; k < sched_task_count(); k++) { const task_t *t = sched_task(k); if (t->period_us <= 10000 && t->max_jitter_us > jm) jm = t->max_jitter_us; mis += t->misses; }
        char k1[40], k2[40], k3[40]; snprintf(k1, sizeof k1, "timeout%u_max_jitter_us", tos[i]); snprintf(k2, sizeof k2, "timeout%u_misses", tos[i]); snprintf(k3, sizeof k3, "timeout%u_fault_E04", tos[i]);
        metric("exp_i2c", k1, jm); metric("exp_i2c", k2, mis); metric("exp_i2c", k3, evlog_total() > 0 ? (double)count_events(EV_FAULT_SET) : 0); }
    /* EXP-3: closing-speed estimation quality: 1000 mm/s approach, 3 mm noise, 1 % spikes + 2 % lost echoes, 20 seeds */
    { double se = 0, mx = 0; int n = 0, big = 0;
      for (uint32_t sd = 1; sd <= 20; sd++) { setup_sim(); s_t0 = 0; s_v = 1000; s_d0 = 3990; sim.dist_mm = sc_approach; sim.noise_mm = 3; sim.spike_permille = 10; sim.lost_echo_permille = 20; sim.seed = 9000 + sd * 31; boot();
          uint32_t last = 0; while (hal_millis() < 3300) { run_until(hal_millis() + 1); if (g_veh.range_seq != last) { last = g_veh.range_seq; if (hal_millis() > 700 && g_veh.target) { double e = (double)g_veh.closing_mmps - 1000.0; se += e * e; if (e < 0) e = -e; if (e > mx) mx = e; if (e > 300) big++; n++; } } } }
      metric("exp_filter", "vel_rms_mmps", n ? __builtin_sqrt(se / n) : 0); metric("exp_filter", "vel_max_abs_err_mmps", mx); metric("exp_filter", "vel_err_gt300_pct", n ? 100.0 * big / n : 0); }
    /* EXP-4: stress false-alarm test: constant gap, 5 % spikes + 5 % lost echoes, 3 x 60 s */
    { uint32_t tot = 0, ev = 0; for (uint32_t sd = 1; sd <= 3; sd++) { setup_sim(); s_gap = 800; s_spd = 600; sim.dist_mm = sc_gap; sim.noise_mm = 3; sim.spike_permille = 50; sim.lost_echo_permille = 50; sim.seed = sd * 104729u; boot(); run_until(60000);
          tot += (uint32_t)(count_edges(PIN_LED_ADV, 1) + count_edges(PIN_LED_WARN, 1) + count_edges(PIN_LED_CRIT, 1)); ev += evlog_total() - 1; }
      metric("exp_filter", "stress_alert_edges_180s", tot); metric("exp_filter", "stress_events_180s", ev); }
}
