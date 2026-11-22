#ifndef VL53L9_FRAME_H
#define VL53L9_FRAME_H

#include <stdint.h>
#include "vl53l9.h"

#define VL53L9_CSI_DATA_TYPE    0x2A                            // RAW8 data type for every line
#define VL53L9_CSI_LINE_SIZE    100                             // bytes per line
#define VL53L9_CSI_FRAME_MAX    (149 * VL53L9_CSI_LINE_SIZE)    // largest frame, binning 2 with DSS [bytes]

// fields of the status line used by the stream
typedef struct
{
    uint32_t frame_counter;
    uint16_t temperature;               // sensor temperature
    uint16_t laser_temperature;         // laser driver temperature
} vl53l9_frame_status_t;


uint32_t vl53l9_frame_lines(uint8_t binning, vl53l9_dss_mode_t dss);
void vl53l9_frame_zones(uint8_t binning, uint32_t *width, uint32_t *height);
uint16_t vl53l9_frame_distance(const uint8_t *frame, uint32_t zone);
uint16_t vl53l9_frame_amplitude(const uint8_t *frame, uint8_t binning, uint32_t zone);
void vl53l9_frame_status(const uint8_t *frame, uint8_t binning, vl53l9_dss_mode_t dss,
                         vl53l9_frame_status_t *status);

#endif
