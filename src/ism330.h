#ifndef ISM330_H
#define ISM330_H

#include <stdint.h>

void ism330_init(void);
void read_data(int16_t *temp, int16_t *gyro, int16_t *accel);
void ism330_irq_init(void);
uint32_t read_data_irq(int16_t *temp, int16_t *gyro, int16_t *accel);

#endif