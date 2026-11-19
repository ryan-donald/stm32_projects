#include "bsp.h"
#include "bsp_i2c.h"

static const i2c_config_t i2c2_config = {
    .i2c_registers = I2C2,
    .ker_clk = 64000000UL,             // hsi_div_ck
};

i2c_t i2c2 = {
    .config = &i2c2_config,
};

// I2C on the camera connector
void i2c2_init(uint32_t scl_hz)
{
    // enable clocks for GPIOB and I2C2
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C2EN;

    // register read to make sure the clock is running
    (void)RCC->APB1ENR1;

    // I2C2 kernel clock = hsi_div_ck, clear I2C2SEL and set it to 0b101
    RCC->CCIPR4 = (RCC->CCIPR4 & ~RCC_CCIPR4_I2C2SEL) | RCC_CCIPR4_I2C2SEL_2 | RCC_CCIPR4_I2C2SEL_0;

    // PB10 (SCL), PB11 (SDA) in alternate function mode, open-drain with pull-up, AF4 = I2C2
    GPIOB->OTYPER |= GPIO_OTYPER_OT10 | GPIO_OTYPER_OT11;
    GPIOB->PUPDR = (GPIOB->PUPDR & ~(GPIO_PUPDR_PUPD10 | GPIO_PUPDR_PUPD11))
                 | GPIO_PUPDR_PUPD10_0 | GPIO_PUPDR_PUPD11_0;
    GPIOB->AFR[1] = (GPIOB->AFR[1] & ~(GPIO_AFRH_AFSEL10 | GPIO_AFRH_AFSEL11))
                  | (4 << GPIO_AFRH_AFSEL10_Pos) | (4 << GPIO_AFRH_AFSEL11_Pos);
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE10 | GPIO_MODER_MODE11))
                 | GPIO_MODER_MODE10_1 | GPIO_MODER_MODE11_1;

    i2c_init(&i2c2, scl_hz);
}
