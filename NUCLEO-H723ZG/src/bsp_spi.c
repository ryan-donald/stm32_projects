#include "bsp.h"
#include "bsp_spi.h"

static const spi_config_t spi1_config = {
    .spi_registers = SPI1,
    .ker_clk = 64000000UL,             // per_ck (hsi_ker)
    .cs_port = GPIOD,
    .cs_pin = 14,                      // PD14, Arduino D10
};

spi_t spi1 = {
    .config = &spi1_config,
};

void spi1_init(uint32_t sck_hz)
{
    // enable clocks for GPIOA, GPIOB, GPIOD and SPI1
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIODEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    // dummy register read to make sure the clock is running
    (void)RCC->APB2ENR;

    // SPI1 kernel clock = per_ck (hsi_ker, 64 MHz)
    RCC->D2CCIP1R = (RCC->D2CCIP1R & ~RCC_D2CCIP1R_SPI123SEL) | RCC_D2CCIP1R_SPI123SEL_2;

    // set cs high to prevent false signaling when activating, then make PD14 output
    GPIOD->BSRR = GPIO_BSRR_BS14;
    GPIOD->MODER = (GPIOD->MODER & ~GPIO_MODER_MODE14) | GPIO_MODER_MODE14_0;

    // PA5 (SCK), PA6 (MISO) in alternate function mode, AF5 = SPI1
    GPIOA->MODER = (GPIOA->MODER & ~(GPIO_MODER_MODE5 | GPIO_MODER_MODE6))
                 | GPIO_MODER_MODE5_1 | GPIO_MODER_MODE6_1;
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(GPIO_AFRL_AFSEL5 | GPIO_AFRL_AFSEL6))
                  | (5 << GPIO_AFRL_AFSEL5_Pos) | (5 << GPIO_AFRL_AFSEL6_Pos);

    // PB5 (MOSI) in alternate function mode, AF5 = SPI1
    GPIOB->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE5) | GPIO_MODER_MODE5_1;
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~GPIO_AFRL_AFSEL5) | (5 << GPIO_AFRL_AFSEL5_Pos);

    spi_init(&spi1, sck_hz);
}
