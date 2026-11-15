#include "bsp.h"
#include "bsp_spi.h"
#include "bsp_ism330.h"

static uint8_t imu_sample[ISM330_SAMPLE_SIZE];            // latest raw temp, gyro, accel registers, written by the interrupt

static const ism330_config_t imu_config = {
    .spi = &spi1,
    .irq = EXTI15_10_IRQn,             // INT1 on PD15, Arduino D9
    .sample = imu_sample,
};

ism330_t imu = {
    .config = &imu_config,
    .sample_count = 0,                 // samples read since the last ism330_read_irq
};

void imu_init(void)
{
    // enable clocks for GPIOD and SYSCFG
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;

    // PD15 (INT1) as input
    GPIOD->MODER &= ~GPIO_MODER_MODE15;

    // EXTI15 from port D, interrupt on rising edge
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~SYSCFG_EXTICR4_EXTI15) | SYSCFG_EXTICR4_EXTI15_PD;
    EXTI->RTSR1 |= EXTI_RTSR1_TR15;
    EXTI_D1->IMR1 |= EXTI_IMR1_IM15;
    
    ism330_init(&imu);
    ism330_irq_init(&imu);
}

void EXTI15_10_IRQHandler(void)
{
    EXTI_D1->PR1 = EXTI_PR1_PR15;
    ism330_irq(&imu);
}
