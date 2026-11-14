#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "bsp.h"

#define RX_BUF_SIZE 64                  // UART receive buffer, must be a power of 2
#define TX_BUF_SIZE 256                 // UART transmit buffer, must be a power of 2

// fixed per port
typedef struct
{
    USART_TypeDef *uart_registers;
    IRQn_Type irq;
    uint32_t ker_clk;                   // kernel clock feeding the baud generator [Hz]

    volatile uint8_t *rx_buf;           // RX_BUF_SIZE bytes
    volatile uint8_t *tx_buf;           // TX_BUF_SIZE bytes
} uart_config_t;

// runtime state of one port
typedef struct
{
    const uart_config_t *config;

    volatile uint32_t rx_head, rx_tail;
    volatile uint32_t tx_head, tx_tail;
} uart_t;


void uart_init(uart_t *uart, uint32_t baud);
void uart_irq(uart_t *uart);
void uart_write(uart_t *uart, const uint8_t *data, uint32_t len);
void uart_putc(uart_t *uart, char c);
void uart_puts(uart_t *uart, const char *s);
void uart_put_int(uart_t *uart, int32_t v);
char uart_getc(uart_t *uart);
int uart_rx_ready(uart_t *uart);

#endif
