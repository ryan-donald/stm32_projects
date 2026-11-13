#ifndef UART_H
#define UART_H

#include <stdint.h>

typedef struct 
{
    USART_TypeDef *uart_registers;
    IRQn_Type irq;

    volatile uint8_t *rx_buf;
    volatile uint32_t rx_head, rx_tail;
} uart_t;


extern uart_t uart3;

void uart3_init(uint32_t baud);
void uart_init(uart_t *uart_config, uint32_t baud);
void uart_irq(uart_t *uart_config);
void uart_putc(uart_t *uart_config, char c);
void uart_puts(uart_t *uart_config, const char *s);
void uart_put_int(uart_t *uart_config, int32_t v);
char uart_getc(uart_t *uart_config);
int uart_rx_ready(uart_t *uart_config);

#endif
