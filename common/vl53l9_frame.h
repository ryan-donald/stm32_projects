#ifndef VL53L9_FRAME_H
#define VL53L9_FRAME_H

#include <stdint.h>
#include "vl53l9.h"

#define VL53L9_CSI_DATA_TYPE    0x2A                            // RAW8 data type for every line
#define VL53L9_CSI_LINE_SIZE    100                             // bytes per line
#define VL53L9_CSI_FRAME_MAX    (149 * VL53L9_CSI_LINE_SIZE)    // largest frame, binning 2 with DSS [bytes]


uint32_t vl53l9_frame_lines(uint8_t binning, vl53l9_dss_mode_t dss);

#endif
