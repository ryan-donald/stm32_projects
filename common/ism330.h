#ifndef ISM330_H
#define ISM330_H

#include <stdint.h>
#include "bsp.h"
#include "spi.h"

#define ISM330_SAMPLE_SIZE 14           // raw temp, gyro, accel registers [bytes]

// fixed per sensor
typedef struct
{
    spi_t *spi;
    IRQn_Type irq;                      // EXTI interrupt the board routes INT1 to

    uint8_t *sample;                    // ISM330_SAMPLE_SIZE bytes
} ism330_config_t;

// runtime state of one sensor
typedef struct
{
    const ism330_config_t *config;

    uint32_t sample_count;
} ism330_t;


void ism330_init(ism330_t *imu);
void ism330_read(ism330_t *imu, int16_t *temp, int16_t *gyro, int16_t *accel);
void ism330_irq_init(ism330_t *imu);
void ism330_irq(ism330_t *imu);
uint32_t ism330_read_irq(ism330_t *imu, int16_t *temp, int16_t *gyro, int16_t *accel);

#endif
