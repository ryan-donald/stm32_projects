#include "spi.h"

#define SPI_READ_BIT 0x80       // MSB of addr is read/write.
#define SPI_MBR_MAX  7          // largest divider, kernel clock / 256

// private functions for setting chip select.
static void cs_low(spi_t *spi)  { spi->config->cs_port->BSRR = 1UL << (spi->config->cs_pin + 16); }
static void cs_high(spi_t *spi) { spi->config->cs_port->BSRR = 1UL << spi->config->cs_pin; }

void spi_init(spi_t *spi, uint32_t sck_hz)
{
    SPI_TypeDef *registers = spi->config->spi_registers;

    // smallest divider 2^(mbr + 1) that keeps SCK at or below sck_hz
    uint32_t mbr = 0;
    while (mbr < SPI_MBR_MAX && (spi->config->ker_clk >> (mbr + 1)) > sck_hz)
    {
        mbr++;
    }

    // disable SPI to configure
    registers->CR1 = SPI_CR1_SSI;
    // 8-bit frames, 1-frame FIFO threshold, SCK = kernel clock / 2^(mbr + 1)
    registers->CFG1 = (7 << SPI_CFG1_DSIZE_Pos) | (mbr << SPI_CFG1_MBR_Pos);
    // master, mode 3 (CPOL=1, CPHA=1), software CS management
    registers->CFG2 = SPI_CFG2_MASTER | SPI_CFG2_CPOL | SPI_CFG2_CPHA | SPI_CFG2_SSM;
    // endless transfer: the master clocks out whatever is written to TXDR
    registers->CR2 = 0;

    // enable and start
    registers->CR1 |= SPI_CR1_SPE;
    registers->CR1 |= SPI_CR1_CSTART;
}

// sends one byte and returns the byte clocked in at the same time
static uint8_t spi_transfer(spi_t *spi, uint8_t b)
{
    SPI_TypeDef *registers = spi->config->spi_registers;

    // 8-bit accesses, a 32-bit write to TXDR would queue 4 frames
    while (!(registers->SR & SPI_SR_TXP)) { };
    *(volatile uint8_t *)&registers->TXDR = b;
    while (!(registers->SR & SPI_SR_RXP)) { };
    return *(volatile uint8_t *)&registers->RXDR;
}

// burst read from SPI
void spi_read_regs(spi_t *spi, uint8_t reg, uint8_t *buf, uint32_t len)
{
    // set cs and send address
    cs_low(spi);
    spi_transfer(spi, reg | SPI_READ_BIT);

    // transfer dummy data to device and read return data into buffer
    for (uint32_t i = 0; i < len; i++)
    {
        buf[i] = spi_transfer(spi, 0x00);
    }
    cs_high(spi);
}

// reads a single register
uint8_t spi_read_reg(spi_t *spi, uint8_t reg)
{
    uint8_t val;
    spi_read_regs(spi, reg, &val, 1);
    return val;
}

// writes a single register
void spi_write_reg(spi_t *spi, uint8_t reg, uint8_t val)
{
    cs_low(spi);
    spi_transfer(spi, reg & ~SPI_READ_BIT);
    spi_transfer(spi, val);
    cs_high(spi);
}
