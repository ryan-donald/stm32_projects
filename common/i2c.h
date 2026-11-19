#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include "bsp.h"

typedef struct
{
    I2C_TypeDef *i2c_registers;
    uint32_t ker_clk;                   // kernel clock feeding the timing generator [Hz]
} i2c_config_t;

typedef struct
{
    const i2c_config_t *config;
} i2c_t;


void i2c_init(i2c_t *i2c, uint32_t scl_hz);
int i2c_write_reg16(i2c_t *i2c, uint8_t addr, uint16_t reg, const uint8_t *data, uint32_t len);
int i2c_read_reg16(i2c_t *i2c, uint8_t addr, uint16_t reg, uint8_t *buf, uint32_t len);

#endif
