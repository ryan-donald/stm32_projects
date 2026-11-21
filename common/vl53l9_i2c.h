#ifndef VL53L9_I2C_H
#define VL53L9_I2C_H

#include <stdint.h>
#include "i2c.h"
#include "vl53l9.h"

// connects the ST VL53L9 driver to the I2C driver

typedef struct
{
    i2c_t *i2c;
    uint8_t address;                    // 7-bit I2C address
    uint32_t ext_clock;                 // AP_CLK frequency [Hz]
    vl53l9_vdda_t vdda;
    vl53l9_vddio_t vddio;
} vl53l9_i2c_t;

#endif
