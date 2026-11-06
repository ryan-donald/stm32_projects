#include "stm32h7xx.h"
#include "spi.h"
#include "ism330.h"

// register addresses
#define STATUS_REG      0x1E     // STATUS_REG
#define CTRL1_XL        0x10     // CTRL1_XL
#define CTRL2_G         0x11     // CTRL2_G
#define CTRL3_C         0x12     // CTRL3_C

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

// wait for bit 0 of STATUS_REG to be set, burst read 6 bytes, and store in xyz
void read_data(int16_t *xyz)
{
    uint8_t raw[6];

    while((spi_read_reg(STATUS_REG) & 0x01) != 1) { };

    spi_read_regs(OUTX_L_A, raw, 6);

    // each axis is a little-endian 16-bit value
    for (uint32_t i = 0; i < 3; i++)
    {
        xyz[i] = (int16_t)(raw[2 * i] | (raw[2 * i + 1] << 8));
    }
}