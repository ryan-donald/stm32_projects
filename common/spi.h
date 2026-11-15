#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include "bsp.h"

// fixed per bus
typedef struct
{
    SPI_TypeDef *spi_registers;
    uint32_t ker_clk;                   // kernel clock feeding the baud divider [Hz]

    GPIO_TypeDef *cs_port;
    uint32_t cs_pin;                    // chip select pin number
} spi_config_t;

// runtime state of one bus
typedef struct
{
    const spi_config_t *config;
} spi_t;


void spi_init(spi_t *spi, uint32_t sck_hz);
uint8_t spi_read_reg(spi_t *spi, uint8_t reg);
void spi_read_regs(spi_t *spi, uint8_t reg, uint8_t *buf, uint32_t len);
void spi_write_reg(spi_t *spi, uint8_t reg, uint8_t val);

#endif
