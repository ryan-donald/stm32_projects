#ifndef SPI_H
#define SPI_H

#include <stdint.h>

void spi_init(void);
uint8_t spi_read_reg(uint8_t reg);
void spi_read_regs(uint8_t reg, uint8_t *buf, uint32_t len);
void spi_write_reg(uint8_t reg, uint8_t val);

#endif
