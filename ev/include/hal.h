/* hal.h - hardware abstraction layer. Two ports: port/hal_sim.c (host) and
 * a target port (STM32F103, see report section 3.4). Firmware never touches registers. */
#ifndef HAL_H
#define HAL_H
#include "config.h"

uint32_t hal_micros(void);
uint32_t hal_millis(void);
void     hal_idle(void);                 /* WFI until next SysTick (1 kHz)   */

int  hal_i2c_read (uint8_t dev, uint8_t reg, uint8_t *buf, uint8_t n); /* 0 ok */
int  hal_i2c_write(uint8_t dev, uint8_t reg, const uint8_t *buf, uint8_t n);
void hal_i2c_bus_recover(void);          /* 9 x SCL clocks + STOP            */

void hal_spi_cs(bool asserted);
void hal_spi_xfer16(uint16_t word);      /* blocking, 16 SCK clocks          */

uint16_t hal_adc_read(uint8_t ch);       /* 12-bit                           */
void     hal_gpio_write(uint8_t pin, bool level);

void    hal_range_trigger(void);         /* 10 us TRIG pulse                 */
int32_t hal_range_poll_us(void);         /* -1 pending, else echo width us   */
int32_t hal_range_measure_blocking_us(uint32_t timeout_us); /* -2 timeout    */

void hal_uart_tx_blocking(const uint8_t *p, uint16_t n);
void hal_uart_register_tx_source(int (*pull)(void)); /* ISR pulls bytes      */
void hal_uart_tx_irq_enable(void);
int  hal_uart_rx_byte(void);             /* -1 if none                       */

void    hal_wdt_start(uint32_t timeout_ms);
void    hal_wdt_kick(void);
uint8_t hal_reset_cause(void);           /* 0 power-on, 1 watchdog           */
#endif
