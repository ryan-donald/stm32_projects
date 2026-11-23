#ifndef BSP_UART_H
#define BSP_UART_H

#include "uart.h"

extern uart_t uart1;
extern uart_t uart3;

void uart1_init(uint32_t baud);
void uart3_init(uint32_t baud);

#endif
