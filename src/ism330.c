#include "stm32h7xx.h"
#include "spi.h"
#include "ism330.h"

// register addresses
#define STATUS_REG      0x1E     // STATUS_REG
#define CTRL1_XL        0x10     // CTRL1_XL
#define CTRL2_G         0x11     // CTRL2_G
#define CTRL3_C         0x12     // CTRL3_C

#define OUT_TEMP_L      0x20     // OUT_TEMP_L
#define OUT_TEMP_H      0x21     // OUT_TEMP_H

#define OUTX_L_G        0x22     // OUTX_L_G
#define OUTX_H_G        0x23     // OUTX_H_G
#define OUTY_L_G        0x24     // OUTY_L_G
#define OUTY_H_G        0x25     // OUTY_H_G
#define OUTZ_L_G        0x26     // OUTZ_L_G
#define OUTZ_H_G        0x27     // OUTZ_H_G

#define OUTX_L_A        0x28     // OUTX_L_A
#define OUTX_H_A        0x29     // OUTX_H_A
#define OUTY_L_A        0x2A     // OUTY_L_A
#define OUTY_H_A        0x2B     // OUTY_H_A
#define OUTZ_L_A        0x2C     // OUTZ_L_A
#define OUTZ_H_A        0x2D     // OUTZ_H_A

#define WHO_AM_I        0x0F     // WHO_AM_I

void ism330_init(void)
{
    if (spi_read_reg(WHO_AM_I) != 0x6B)
    {
        return;
    }

    // software reset on imu
    spi_write_reg(CTRL3_C, 0x01);

    // wait for the software reset to complete
    while (spi_read_reg(CTRL3_C) & 0x01) { };

    // set block data update so each read if always from the same cycle
    spi_write_reg(CTRL3_C, 0x44);

    // configure accelerometer rate
    spi_write_reg(CTRL1_XL, 0x60);

    // configure gyroscope rate
    spi_write_reg(CTRL2_G, 0x60);
}

// wait for bits 0 and 1 of STATUS_REG to be set, burst read 14 bytes,
// and store in buffers
void read_data(int16_t *temp, int16_t *gyro, int16_t *accel)
{
    uint8_t raw[14];

    while((spi_read_reg(STATUS_REG) & 0x03) != 0x03) { };

    spi_read_regs(OUT_TEMP_L, raw, 14);

    // each value is a little-endian 16-bit value
    // order: temp bytes 0-1, gyro 2-7, accel 8-13
    *temp = (int16_t)(raw[0] | (raw[1] << 8));
    for (uint32_t i = 0; i < 3; i++)
    {
        gyro[i] = (int16_t)(raw[2 * i + 2] | (raw[2 * i + 3] << 8));
        accel[i] = (int16_t)(raw[2 * i + 8] | (raw[2 * i + 9] << 8));
    }
}