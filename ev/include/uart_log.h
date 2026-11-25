#ifndef UART_LOG_H
#define UART_LOG_H
#include "vehicle.h"
void     uart_init(void);
bool     uart_send_frame(const char *body);      /* wraps as $body*CRC\r\n; all-or-nothing */
void     telem_task(uint32_t now_ms);
void     cli_task(uint32_t now_ms);
uint32_t uart_tx_dropped(void);
uint32_t uart_rx_errors(void);
uint32_t telem_seq(void);
#endif
