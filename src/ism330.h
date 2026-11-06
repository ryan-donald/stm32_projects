#ifndef ISM330_H
#define ISM330_H

#include <stdint.h>

void ism330_init(void);
void read_data(int16_t *xyz);

#endif