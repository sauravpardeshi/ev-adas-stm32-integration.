/* hal_sim.c - simulated STM32 peripherals with cost model + fault injection.
 * Timing model (all charged to virtual time when the firmware blocks on them):
 *   I2C 400 kHz : 9 clocks/byte = 22.5 us/byte     SPI 1 MHz : 16 us / 16-bit frame
 *   UART 115200 8N1 : 87 us/byte                    ADC : 10 us / conversion
 *   HC-SR04 echo : 5.83 us per mm (343 m/s round trip), 38 ms when no echo
 */
#include "hal_sim.h"
#include "../include/hal.h"
#include <string.h>

sim_t sim;
static uint8_t uart_store[SIM_UART_BUF];

static uint32_t rnd(void){ uint32_t x = sim.seed; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return sim.seed = x; }
static int32_t  rnd_pm(int n){ return n ? (int32_t)(rnd() % (uint32_t)(2*n + 1)) - n : 0; }

static int32_t sc_dist(uint32_t t){ (void)t; return -1; }
static int32_t sc_speed(uint32_t t){ (void)t; return 0; }
static void    sc_accel(uint32_t t, int16_t *x, int16_t *y, int16_t *z){ (void)t; *x = 0; *y = 0; *z = 1000; }
static int32_t sc_batt(uint32_t t){ (void)t; return 15200; }

void sim_reset(void){
    memset(&sim, 0, sizeof sim);
    sim.uart_out = uart_store; sim.dist_mm = sc_dist; sim.speed_dkmh = sc_speed; sim.accel = sc_accel; sim.batt_mv = sc_batt;
    sim.seed = 0x1234ABCDu; sim.who_am_i = 0x68; sim.mpu_reg[0x6B] = 0x40; sim.batt_adc_override = -1;
    sim.reset_cause = 0; sim.i2c_timeout_us = 2000;
}

void sim_advance_us(uint32_t us){
    sim.now_us += us;
    while (sim.uart_irq && sim.uart_next_us <= sim.now_us) {
        int b = sim.uart_pull ? sim.uart_pull() : -1;
        if (b < 0) { sim.uart_irq = false; break; }
        if (sim.uart_out_n < SIM_UART_BUF) sim.uart_out[sim.uart_out_n++] = (uint8_t)b;
        sim.uart_next_us += 87;
    }
    if (sim.wdt_on && (sim.now_us - sim.wdt_last_us) > sim.wdt_to_us) {
        sim.reset_flag = true; sim.wdt_resets++; sim.wdt_last_us = sim.now_us;
    }
}

/* ---- time ---- */
uint32_t hal_micros(void){ return (uint32_t)sim.now_us; }
uint32_t hal_millis(void){ return (uint32_t)(sim.now_us / 1000u); }
void hal_idle(void){ sim_advance_us((uint32_t)(1000u - (sim.now_us % 1000u))); }

/* ---- I2C + MPU-6050 model ---- */
static void build_mpu_frame(void){
    static const int32_t lsb[4] = {16384, 8192, 4096, 2048};
    int32_t l = lsb[(sim.mpu_reg[0x1C] >> 3) & 3];
    int16_t ax, ay, az; sim.accel(hal_millis(), &ax, &ay, &az);
    int32_t r[3] = { (int32_t)ax * l / 1000 + rnd_pm(6), (int32_t)ay * l / 1000 + rnd_pm(6), (int32_t)az * l / 1000 + rnd_pm(6) };
    int32_t temp = 2560 + rnd_pm(3), g[3] = { rnd_pm(4), rnd_pm(4), rnd_pm(4) };
    int32_t w[7] = { r[0], r[1], r[2], temp, g[0], g[1], g[2] };
    for (int i = 0; i < 7; i++) {
        if (i < 3) { if (w[i] > 32767) w[i] = 32767; if (w[i] < -32768) w[i] = -32768; }
        sim.mpu_frame[2*i] = (uint8_t)((w[i] >> 8) & 0xFF); sim.mpu_frame[2*i+1] = (uint8_t)(w[i] & 0xFF);
    }
}
static int i2c_common(uint8_t dev, uint32_t bytes){
    sim.i2c_xfers++;
    if (sim.i2c_bus_stuck) { sim.i2c_busy_us += sim.i2c_timeout_us; sim_advance_us(sim.i2c_timeout_us); return -2; }  /* HAL bus timeout */
    if (dev != MPU6050_ADDR || sim.mpu_absent || sim.imu_nack) { sim.i2c_busy_us += 23; sim_advance_us(23); return -1; }
    if (sim.nack_once > 0) { sim.nack_once--; sim.i2c_busy_us += 23; sim_advance_us(23); return -1; }
    uint32_t c = (bytes * 45u) / 2u; sim.i2c_busy_us += c; sim_advance_us(c); return 0;
}
int hal_i2c_read(uint8_t dev, uint8_t reg, uint8_t *buf, uint8_t n){
    int rc = i2c_common(dev, 3u + n); if (rc) return rc;
    if (reg == 0x75) { buf[0] = sim.who_am_i; return 0; }
    if (reg == 0x3B && n == 14) {
        if (!sim.imu_frozen && !(sim.mpu_reg[0x6B] & 0x40)) build_mpu_frame();
        else if (sim.mpu_reg[0x6B] & 0x40) memset(sim.mpu_frame, 0, 14);
        memcpy(buf, sim.mpu_frame, 14); return 0;
    }
    for (uint8_t i = 0; i < n; i++) buf[i] = sim.mpu_reg[(reg + i) & 0x7F];
    return 0;
}
int hal_i2c_write(uint8_t dev, uint8_t reg, const uint8_t *buf, uint8_t n){
    int rc = i2c_common(dev, 2u + n); if (rc) return rc;
    for (uint8_t i = 0; i < n; i++) {
        if (reg == 0x6B && (buf[i] & 0x80)) { memset(sim.mpu_reg, 0, sizeof sim.mpu_reg); sim.mpu_reg[0x6B] = 0x40; sim.imu_frozen = false; }
        else sim.mpu_reg[(reg + i) & 0x7F] = buf[i];
    }
    return 0;
}
void hal_i2c_bus_recover(void){ sim.i2c_bus_stuck = false; sim.i2c_busy_us += 90; sim_advance_us(90); }

/* ---- SPI + MAX7219 model with protocol checker ---- */
void hal_spi_cs(bool a){
    if (a) { if (sim.spi_cs) sim.spi_proto_errs++; sim.spi_cs = 1; sim.spi_bits = 0; sim.gpio[PIN_SPI_CS] = true; }
    else {
        if (sim.spi_cs) {
            if (sim.spi_bits != 16) sim.spi_proto_errs++;
            else { uint8_t reg = (uint8_t)((sim.spi_word >> 8) & 0xFF);
                   if (reg > 0x0F) sim.spi_proto_errs++; else { sim.max_reg[reg] = (uint8_t)(sim.spi_word & 0xFF); sim.spi_frames++; } }
        }
        sim.spi_cs = 0; sim.gpio[PIN_SPI_CS] = false;
    }
}
void hal_spi_xfer16(uint16_t w){
    if (!sim.spi_cs) sim.spi_proto_errs++;
    sim.spi_word = w; sim.spi_bits += 16; sim.spi_busy_us += 16; sim_advance_us(16);
}
void sim_display_string(char out[9]){
    static const char map[16] = {'0','1','2','3','4','5','6','7','8','9','-','E','H','L','P',' '};
    bool ok = (sim.max_reg[0x0C] & 1) && sim.max_reg[0x09] == 0xFF && sim.max_reg[0x0B] == 0x07;
    for (int d = 8; d >= 1; d--) out[8 - d] = ok ? map[sim.max_reg[d] & 0x0F] : '?';
    out[8] = 0;
}

/* ---- ADC ---- */
uint16_t hal_adc_read(uint8_t ch){
    sim.adc_busy_us += 10; sim_advance_us(10);
    int32_t c = 0, t = (int32_t)hal_millis();
    if (ch == ADC_CH_SPEED) c = sim.speed_dkmh(t) * 4095 / 1200 + rnd_pm(2);
    else if (ch == ADC_CH_BATT) {
        if (sim.batt_adc_override >= 0) return (uint16_t)sim.batt_adc_override;
        int32_t pin_mv = sim.batt_mv(t) * 10 / 57; c = pin_mv * 4095 / 3300 + rnd_pm(2);
    }
    if (c < 0) c = 0;
    if (c > 4095) c = 4095;
    return (uint16_t)c;
}

/* ---- GPIO ---- */
void hal_gpio_write(uint8_t pin, bool lvl){
    if (pin >= PIN_COUNT || sim.gpio[pin] == lvl) return;
    sim.gpio[pin] = lvl;
    if (sim.n_edges < 16384) { sim.edges[sim.n_edges].t_us = (uint32_t)sim.now_us; sim.edges[sim.n_edges].pin = pin; sim.edges[sim.n_edges].level = lvl; sim.n_edges++; }
}

/* ---- HC-SR04 model ---- */
static int32_t make_width(void){
    int32_t d = sim.dist_mm(hal_millis()); int32_t w;
    if (d < 0 || d > RANGE_MAX_MM) w = 38000;
    else { w = (d * 58) / 10 + rnd_pm((sim.noise_mm * 58) / 10); }
    uint32_t r = rnd() % 1000;
    if (r < (uint32_t)sim.spike_permille) { uint32_t k = rnd() % 3; w = (k == 0) ? 100 : (k == 1) ? (int32_t)(300 + rnd() % 22000) : 38000; }
    else if (r < (uint32_t)(sim.spike_permille + sim.lost_echo_permille)) w = 38000;
    if (w < 0) w = 0;
    return w;
}
void hal_range_trigger(void){
    sim.range_busy_us += 10; sim_advance_us(10);
    sim.range_pending = true;
    if (sim.range_stuck) { sim.range_ready_us = ~0ull; return; }
    sim.range_width = make_width(); sim.range_ready_us = sim.now_us + (uint64_t)sim.range_width;
}
int32_t hal_range_poll_us(void){
    if (!sim.range_pending || sim.now_us < sim.range_ready_us) return -1;
    sim.range_pending = false; return sim.range_width;
}
int32_t hal_range_measure_blocking_us(uint32_t timeout_us){
    if (sim.range_stuck) { sim.range_busy_us += timeout_us; sim_advance_us(timeout_us); return -2; }
    int32_t w = make_width();
    if ((uint32_t)w > timeout_us) { sim.range_busy_us += timeout_us; sim_advance_us(timeout_us); return -2; }
    sim.range_busy_us += (uint32_t)w + 10; sim_advance_us((uint32_t)w + 10); return w;
}

/* ---- UART ---- */
void hal_uart_tx_blocking(const uint8_t *p, uint16_t n){
    for (uint16_t i = 0; i < n; i++) { if (sim.uart_out_n < SIM_UART_BUF) sim.uart_out[sim.uart_out_n++] = p[i]; }
    sim.uart_busy_us += 87u * n; sim_advance_us(87u * n);
}
void hal_uart_register_tx_source(int (*pull)(void)){ sim.uart_pull = pull; }
void hal_uart_tx_irq_enable(void){ if (!sim.uart_irq) { sim.uart_irq = true; sim.uart_next_us = sim.now_us + 87; } }
int  hal_uart_rx_byte(void){ if (sim.rx_tail == sim.rx_head) return -1; return (uint8_t)sim.uart_rx[sim.rx_tail++ % sizeof sim.uart_rx]; }
void sim_uart_inject(const char *s){ while (*s) sim.uart_rx[sim.rx_head++ % sizeof sim.uart_rx] = *s++; }
void sim_uart_clear(void){ sim.uart_out_n = 0; }

/* ---- watchdog ---- */
void hal_wdt_start(uint32_t ms){ sim.wdt_on = true; sim.wdt_to_us = ms * 1000u; sim.wdt_last_us = sim.now_us; }
void hal_wdt_kick(void){ sim.wdt_last_us = sim.now_us; }
uint8_t hal_reset_cause(void){ return sim.reset_cause; }
