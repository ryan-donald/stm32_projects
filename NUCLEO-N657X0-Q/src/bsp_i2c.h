#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "i2c.h"

extern i2c_t i2c2;

void i2c2_init(uint32_t scl_hz);

#endif
