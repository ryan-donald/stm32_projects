#include "stm32n6xx.h"
#include "bsp_uart.h"
#include "bsp_spi.h"
#include "bsp_ism330.h"

#define BAUD_RATE   115200         // baud rate for UART
#define SPI_SCK_HZ  2000000        // SPI clock for the ISM330 [Hz]
#define WHO_AM_I    0x0F           // ISM330DHCX ID register, reads 0x6B

int main(void)
{
    uart1_init(BAUD_RATE);
    spi5_init(SPI_SCK_HZ);

    // print the sensor ID as hex to check the SPI wiring
    const char hex[] = "0123456789ABCDEF";
    uint8_t id = spi_read_reg(&spi5, WHO_AM_I);
    uart_puts(&uart1, "WHO_AM_I = 0x");
    uart_putc(&uart1, hex[id >> 4]);
    uart_putc(&uart1, hex[id & 0x0F]);
    uart_puts(&uart1, "\r\n");

    imu_init();

    while(1)
    {
        // echo each received character back
        if (uart_rx_ready(&uart1))
        {
            uart_putc(&uart1, uart_getc(&uart1));
        }
    }
}
