#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(uint32_t baud);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_put_int(int32_t v);
char uart_getc(void);
int uart_rx_ready(void);

#endif
