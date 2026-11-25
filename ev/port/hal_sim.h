/* hal_sim.h - host-side simulated hardware ("software-in-the-loop") */
#ifndef HAL_SIM_H
#define HAL_SIM_H
#include "../include/config.h"
#define SIM_UART_BUF 262144
typedef struct { uint32_t t_us; uint8_t pin; uint8_t level; } gpio_edge_t;
typedef struct {
    uint64_t now_us;
    /* scenario sources (functions of simulated time) */
    int32_t (*dist_mm)(uint32_t t_ms);                  /* <0 or >4000: no target */
    int32_t (*speed_dkmh)(uint32_t t_ms);
    void    (*accel)(uint32_t t_ms, int16_t *ax, int16_t *ay, int16_t *az);  /* mg */
    int32_t (*batt_mv)(uint32_t t_ms);                  /* pack voltage */
    /* impairments / fault injection */
    int  noise_mm, spike_permille, lost_echo_permille;
    bool imu_nack, i2c_bus_stuck, imu_frozen, range_stuck, mpu_absent;
    int  batt_adc_override;                             /* -1 none, else forced counts */
    uint8_t who_am_i; int nack_once; uint32_t i2c_timeout_us;                    /* transient NACK counter */
    /* device models / observation */
    uint8_t mpu_reg[128]; uint8_t mpu_frame[14];
    uint8_t max_reg[16]; uint32_t spi_frames, spi_proto_errs; int spi_cs, spi_bits; uint16_t spi_word;
    bool gpio[PIN_COUNT]; gpio_edge_t edges[16384]; uint32_t n_edges;
    uint8_t *uart_out; uint32_t uart_out_n;             /* everything transmitted */
    char uart_rx[4096]; uint32_t rx_head, rx_tail;
    uint32_t i2c_xfers, i2c_busy_us, spi_busy_us, uart_busy_us, range_busy_us, adc_busy_us;
    uint32_t wdt_resets; bool reset_flag; uint8_t reset_cause;
    /* range */
    bool range_pending; uint64_t range_ready_us; int32_t range_width;
    /* uart tx isr */
    int (*uart_pull)(void); bool uart_irq; uint64_t uart_next_us;
    /* wdt */
    bool wdt_on; uint32_t wdt_to_us; uint64_t wdt_last_us;
    uint32_t seed;
} sim_t;
extern sim_t sim;
void sim_reset(void);
void sim_advance_us(uint32_t us);
void sim_uart_inject(const char *s);
void sim_uart_clear(void);
void sim_display_string(char out[9]);   /* decoded digits d8..d1 */
#endif
