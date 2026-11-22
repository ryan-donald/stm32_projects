#include "stm32n6xx.h"
#include "bsp_clock.h"
#include "bsp_uart.h"
#include "bsp_spi.h"
#include "bsp_ism330.h"
#include "bsp_csi.h"
#include "bsp_vl53l9.h"
#include "vl53l9_frame.h"
#include "vl53l9_stream.h"

#define BAUD_RATE   4000000        // baud rate for UART
#define MIN_AMPLITUDE 64           // zones below this amplitude are sent as no target
#define SPI_SCK_HZ  2000000        // SPI clock for the ISM330 [Hz]
#define WHO_AM_I    0x0F           // ISM330DHCX ID register, reads 0x6B
#define PRINT_MS    100            // sensor print period [ms]
#define ACCEL_UG    61             // accel sensitivity at +-2 g [ug/LSB]
#define GYRO_UDPS   8750           // gyro sensitivity at +-250 dps [udps/LSB]
#define TEMP_LSB    256            // temperature sensitivity [LSB/degC]
#define TEMP_ZERO   25             // temperature at 0 LSB [degC]

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
    clock_init();
    SCB_EnableICache();

    // enable GPIO for LED, green LD on PG0
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOGEN;
    (void)RCC->AHB4ENR;
    // clear MODE0 bits [1:0], then set them to 01 (output)
    GPIOG->MODER = (GPIOG->MODER & ~GPIO_MODER_MODE0) | GPIO_MODER_MODE0_0;

    // SysTick interrupt every 1 ms
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);

    uart1_init(BAUD_RATE);
    spi5_init(SPI_SCK_HZ);
    delay_ms(10);

    // print the sensor ID as hex to check the SPI wiring
    const char hex[] = "0123456789ABCDEF";
    uint8_t id = spi_read_reg(&spi5, WHO_AM_I);
    uart_puts(&uart1, "WHO_AM_I = 0x");
    uart_putc(&uart1, hex[id >> 4]);
    uart_putc(&uart1, hex[id & 0x0F]);
    uart_puts(&uart1, "\r\n");

    imu_init();

    // boot the VL53L9, arm the CSI receiver and start ranging
    int ret = tof_init();
    uart_puts(&uart1, "tof_init = ");
    uart_put_int(&uart1, ret);
    if (ret != 0)
    {
        uart_puts(&uart1, " at ");
        uart_puts(&uart1, tof_failed_step);
    }
    uart_puts(&uart1, "\r\n");

    uint32_t last_frames = ticks_ms;
    uint32_t frames = 0;

    uint32_t last_toggle = ticks_ms;

    int16_t temp;
    int16_t gyro[3];
    int16_t accel[3];
    uint32_t last_print = ticks_ms;

    while(1)
    {
        // print the latest sample as accel in mg, gyro in mdps and temp in mdegC every PRINT_MS,
        // and how many samples the interrupt read since the last print
        if ((ticks_ms - last_print) >= PRINT_MS)
        {
            uint32_t samples = ism330_read_irq(&imu, &temp, gyro, accel);
            uart_puts(&uart1, "n=");
            uart_put_int(&uart1, samples);
            uart_puts(&uart1, "  ax=");
            uart_put_int(&uart1, (int32_t)accel[0] * ACCEL_UG / 1000);
            uart_puts(&uart1, "  ay=");
            uart_put_int(&uart1, (int32_t)accel[1] * ACCEL_UG / 1000);
            uart_puts(&uart1, "  az=");
            uart_put_int(&uart1, (int32_t)accel[2] * ACCEL_UG / 1000);
            uart_puts(&uart1, " mg  gx=");
            uart_put_int(&uart1, (int32_t)gyro[0] * GYRO_UDPS / 1000);
            uart_puts(&uart1, "  gy=");
            uart_put_int(&uart1, (int32_t)gyro[1] * GYRO_UDPS / 1000);
            uart_puts(&uart1, "  gz=");
            uart_put_int(&uart1, (int32_t)gyro[2] * GYRO_UDPS / 1000);
            uart_puts(&uart1, " mdps  t=");
            uart_put_int(&uart1, TEMP_ZERO * 1000 + (int32_t)temp * 1000 / TEMP_LSB);
            uart_puts(&uart1, " mdegC\r\n");
            last_print += PRINT_MS;
        }

        // toggle LED every 1000 ms without blocking
        if ((ticks_ms - last_toggle) >= 1000)
        {
            GPIOG->ODR ^= (1<<0);
            last_toggle += 1000;
        }

        if ((ticks_ms - last_frames) >= 1000)
        {
            uint32_t count = csi.frame_count;
            uart_puts(&uart1, "frames/s=");
            uart_put_int(&uart1, count - frames);
            uart_puts(&uart1, "  errors=");
            uart_put_int(&uart1, csi.errors);
            uart_puts(&uart1, "  bytes=");
            uart_put_int(&uart1, vl53l9_frame_lines(tof_ranging.binning, tof_ranging.dss) * VL53L9_CSI_LINE_SIZE);
            uart_puts(&uart1, "\r\n");
            frames = count;
            last_frames += 1000;
        }

        const uint8_t *frame = csi_get_frame(&csi);
        if (frame != NULL)
        {
            // the frame stays valid until the next csi_get_frame
            vl53l9_stream_frame(&uart1, frame, tof_ranging.binning, tof_ranging.dss, MIN_AMPLITUDE, 0,
                                csi.errors);
        }
    }
}
