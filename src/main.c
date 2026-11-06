#include "stm32h7xx.h"
#include "uart.h"
#include "spi.h"
#include "ism330.h"

#define BAUD_RATE   115200         // baud rate for UART
#define LINE_MAX    64             // line limit [char]
#define WHO_AM_I    0x0F           // ISM330DHCX ID register, reads 0x6B
#define PRINT_MS    100            // accelerometer print period [ms]
#define ACCEL_UG    61             // accel sensitivity at +-2 g [ug/LSB]

static volatile uint32_t ticks_ms;

void SysTick_Handler(void)
{
    ticks_ms++;
}

void delay_ms(uint32_t delay)
{
    uint32_t start = ticks_ms;
    while((ticks_ms - start) < delay) { };
}

int main(void)
{

    // enable GPIO for LED
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    (void)RCC->AHB4ENR;
    // clear MODE0 bits [1:0], then set them to 01 (output)
    GPIOB->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE0) | GPIO_MODER_MODE0_0;

    //SysTick interrupt every 1 ms
    SysTick_Config(SystemCoreClock / 1000);

    uart_init(BAUD_RATE);
    spi_init();
    delay_ms(10);
    ism330_init();

    // print the sensor ID as hex to check the SPI wiring
    const char hex[] = "0123456789ABCDEF";
    uint8_t id = spi_read_reg(WHO_AM_I);
    uart_puts("WHO_AM_I = 0x");
    uart_putc(hex[id >> 4]);
    uart_putc(hex[id & 0x0F]);
    uart_puts("\r\n");

    uint32_t last_toggle = ticks_ms;

    char line[LINE_MAX + 1];
    uint32_t line_len = 0;

    int16_t accel[3];
    uint32_t last_print = ticks_ms;


    while (1)
    {

        // // echoes each character, then repeats the whole line on a new line after Enter
        // if (uart_rx_ready())
        // {
        //     char c = uart_getc();

        //     // on newline, print full line received
        //     if (c == '\r')
        //     {
        //         line[line_len] = '\0';
        //         uart_puts("\r\n");
        //         uart_puts(line);
        //         uart_puts("\r\n");
        //         line_len = 0;
        //     }
        //     // if not a newline, echo character and add to line
        //     else if (c != '\n')
        //     {
        //         uart_putc(c);
        //         if (line_len < LINE_MAX)
        //         {
        //             line[line_len++] = c;
        //         }
        //     }
        // }



        // read the accelerometer and print it in mg every PRINT_MS
        if ((ticks_ms - last_print) >= PRINT_MS)
        {
            read_data(accel);
            uart_puts("x=");
            uart_put_int((int32_t)accel[0] * ACCEL_UG / 1000);
            uart_puts("  y=");
            uart_put_int((int32_t)accel[1] * ACCEL_UG / 1000);
            uart_puts("  z=");
            uart_put_int((int32_t)accel[2] * ACCEL_UG / 1000);
            uart_puts(" mg\r\n");
            last_print += PRINT_MS;
        }

        // toggle LED every 1000 ms without blocking
        if ((ticks_ms - last_toggle) >= 1000)
        {
            GPIOB->ODR ^= (1<<0);
            last_toggle += 1000;
        }
    }
}