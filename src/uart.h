#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "stm32h7xx.h"

// fixed per port
typedef struct
{
    USART_TypeDef *uart_registers;
    IRQn_Type irq;

    volatile uint8_t *rx_buf;
} uart_config_t;

// runtime state of one port
typedef struct
{
    const uart_config_t *config;

    volatile uint32_t rx_head, rx_tail;
} uart_t;


extern uart_t uart3;

void uart3_init(uint32_t baud);
void uart_init(uart_t *uart, uint32_t baud);
void uart_irq(uart_t *uart);
void uart_putc(uart_t *uart, char c);
void uart_puts(uart_t *uart, const char *s);
void uart_put_int(uart_t *uart, int32_t v);
char uart_getc(uart_t *uart);
int uart_rx_ready(uart_t *uart);

#endif
