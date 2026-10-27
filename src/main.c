#include "stm32h7xx.h"

volatile uint32_t ticks_ms;

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
    GPIOB->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE0) | GPIO_MODER_MODE0_0;

    //SysTick interrupt every 1 ms
    SysTick_Config(SystemCoreClock / 1000);

    while (1)
    {
        GPIOB->ODR ^= (1<<0);
        delay_ms(1000);
    }
}