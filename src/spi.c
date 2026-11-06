#include "stm32h7xx.h"
#include "spi.h"

#define SPI_READ_BIT 0x80       // MSB of addr is read/write.
#define SPI_MBR_DIV32 4         // MBR = 0b100, 64 MHz / 32 = 2 MHz SCK

// private functions for setting chip select.
static void cs_low(void)  { GPIOD->BSRR = GPIO_BSRR_BR14; }
static void cs_high(void) { GPIOD->BSRR = GPIO_BSRR_BS14; }

void spi_init(void)
{
    // enable clocks for GPIOA, GPIOB, GPIOD and SPI1
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIODEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    // dummy register read to make sure the clock is running
    (void)RCC->APB2ENR;

    // SPI1 kernel clock = per_ck (hsi_ker, 64 MHz)
    RCC->D2CCIP1R = (RCC->D2CCIP1R & ~RCC_D2CCIP1R_SPI123SEL) | RCC_D2CCIP1R_SPI123SEL_2;

    // set cs high to prevent false signaling when activating, then make PD14 output
    cs_high();
    GPIOD->MODER = (GPIOD->MODER & ~GPIO_MODER_MODE14) | GPIO_MODER_MODE14_0;

    // PA5 (SCK), PA6 (MISO) in alternate function mode, AF5 = SPI1
    GPIOA->MODER = (GPIOA->MODER & ~(GPIO_MODER_MODE5 | GPIO_MODER_MODE6))
                 | GPIO_MODER_MODE5_1 | GPIO_MODER_MODE6_1;
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(GPIO_AFRL_AFSEL5 | GPIO_AFRL_AFSEL6))
                  | (5 << GPIO_AFRL_AFSEL5_Pos) | (5 << GPIO_AFRL_AFSEL6_Pos);

    // PB5 (MOSI) in alternate function mode, AF5 = SPI1
    GPIOB->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE5) | GPIO_MODER_MODE5_1;
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~GPIO_AFRL_AFSEL5) | (5 << GPIO_AFRL_AFSEL5_Pos);

    // disable SPI to configure
    SPI1->CR1 = SPI_CR1_SSI;
    // 8-bit frames, 1-frame FIFO threshold, SCK = kernel clock / 32
    SPI1->CFG1 = (7 << SPI_CFG1_DSIZE_Pos) | (SPI_MBR_DIV32 << SPI_CFG1_MBR_Pos);
    // master, mode 3 (CPOL=1, CPHA=1), software CS management
    SPI1->CFG2 = SPI_CFG2_MASTER | SPI_CFG2_CPOL | SPI_CFG2_CPHA | SPI_CFG2_SSM;
    // endless transfer: the master clocks out whatever is written to TXDR
    SPI1->CR2 = 0;

    // enable and start
    SPI1->CR1 |= SPI_CR1_SPE;
    SPI1->CR1 |= SPI_CR1_CSTART;
}

// sends one byte and returns the byte clocked in at the same time
static uint8_t spi_transfer(uint8_t b)
{
    // 8-bit accesses, a 32-bit write to TXDR would queue 4 frames
    while (!(SPI1->SR & SPI_SR_TXP)) { };
    *(volatile uint8_t *)&SPI1->TXDR = b;
    while (!(SPI1->SR & SPI_SR_RXP)) { };
    return *(volatile uint8_t *)&SPI1->RXDR;
}

// burst read from SPI
void spi_read_regs(uint8_t reg, uint8_t *buf, uint32_t len)
{
    // set cs and send address
    cs_low();
    spi_transfer(reg | SPI_READ_BIT);

    // transfer dummy data to device and read return data into buffer
    for (uint32_t i = 0; i < len; i++)
    {
        buf[i] = spi_transfer(0x00);
    }
    cs_high();
}

// reads a single register
uint8_t spi_read_reg(uint8_t reg)
{
    uint8_t val;
    spi_read_regs(reg, &val, 1);
    return val;
}

// writes a single register
void spi_write_reg(uint8_t reg, uint8_t val)
{
    cs_low();
    spi_transfer(reg & ~SPI_READ_BIT);
    spi_transfer(val);
    cs_high();
}
