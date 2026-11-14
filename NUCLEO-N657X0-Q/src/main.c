#include "stm32n6xx.h"
#include "bsp_uart.h"

#define BAUD_RATE   115200         // baud rate for UART

int main(void)
{
    uart1_init(BAUD_RATE);

    while(1)
    {
        // echo each received character back
        if (uart_rx_ready(&uart1))
        {
            uart_putc(&uart1, uart_getc(&uart1));
        }
    }
}
