#include "bsp.h"
#include "bsp_spi.h"
#include "bsp_ism330.h"

#define EXTI_PORT_D 3                  // EXTICR port index, PA = 0

static uint8_t imu_sample[ISM330_SAMPLE_SIZE];            // latest raw temp, gyro, accel registers, written by the interrupt

static const ism330_config_t imu_config = {
    .spi = &spi5,
    .irq = EXTI7_IRQn,                 // INT1 on PD7, Arduino D9
    .sample = imu_sample,
};

ism330_t imu = {
    .config = &imu_config,
    .sample_count = 0,                 // samples read since the last ism330_read_irq
};

void imu_init(void)
{
    // enable clock for GPIOD
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;
    (void)RCC->AHB4ENR;

    // PD7 (INT1) as input
    GPIOD->MODER &= ~GPIO_MODER_MODE7;

    // EXTI7 from port D, interrupt on rising edge
    EXTI->EXTICR[1] = (EXTI->EXTICR[1] & ~EXTI_EXTICR2_EXTI7) | (EXTI_PORT_D << EXTI_EXTICR2_EXTI7_Pos);
    EXTI->RTSR1 |= EXTI_RTSR1_RT7;
    EXTI->IMR1 |= EXTI_IMR1_IM7;

    ism330_init(&imu);
    ism330_irq_init(&imu);
}

void EXTI7_IRQHandler(void)
{
    EXTI->RPR1 = EXTI_RPR1_RPIF7;
    ism330_irq(&imu);
}
