#include "bsp.h"
#include "bsp_spi.h"

static const spi_config_t spi5_config = {
    .spi_registers = SPI5,
    .ker_clk = 64000000UL,             // hsi_div_ck
    .cs_port = GPIOA,
    .cs_pin = 3,                       // PA3, Arduino D10
};

spi_t spi5 = {
    .config = &spi5_config,
};

void spi5_init(uint32_t sck_hz)
{
    // enable clocks for GPIOA, GPIOE, GPIOG and SPI5
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOGEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI5EN;

    // dummy register read to make sure the clock is running
    (void)RCC->APB2ENR;

    // SPI5 kernel clock = hsi_div_ck, clear SPI5SEL and set it to 0b101
    RCC->CCIPR9 = (RCC->CCIPR9 & ~RCC_CCIPR9_SPI5SEL)
                | RCC_CCIPR9_SPI5SEL_2 | RCC_CCIPR9_SPI5SEL_0;

    // set cs high to prevent false signaling when activating, then make PA3 output
    GPIOA->BSRR = GPIO_BSRR_BS3;
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODE3) | GPIO_MODER_MODE3_0;

    // PE15 (SCK, Arduino D13) in alternate function mode, AF5 = SPI5
    GPIOE->MODER = (GPIOE->MODER & ~GPIO_MODER_MODE15) | GPIO_MODER_MODE15_1;
    GPIOE->AFR[1] = (GPIOE->AFR[1] & ~GPIO_AFRH_AFSEL15) | (5 << GPIO_AFRH_AFSEL15_Pos);

    // PG1 (MISO, Arduino D12), PG2 (MOSI, Arduino D11) in alternate function mode, AF5 = SPI5
    GPIOG->MODER = (GPIOG->MODER & ~(GPIO_MODER_MODE1 | GPIO_MODER_MODE2))
                 | GPIO_MODER_MODE1_1 | GPIO_MODER_MODE2_1;
    GPIOG->AFR[0] = (GPIOG->AFR[0] & ~(GPIO_AFRL_AFSEL1 | GPIO_AFRL_AFSEL2))
                  | (5 << GPIO_AFRL_AFSEL1_Pos) | (5 << GPIO_AFRL_AFSEL2_Pos);

    spi_init(&spi5, sck_hz);
}
