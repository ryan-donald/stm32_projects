#ifndef VL53L9_STREAM_H
#define VL53L9_STREAM_H

#include <stdint.h>
#include "uart.h"
#include "vl53l9.h"

// binary depth packet read by tools/vl53l9_viewer.py, all fields little-endian
//   0  magic A5 5A 'V' '9'
//   4  u8  version
//   5  u8  binning
//   6  u8  width  [zones]
//   7  u8  height [zones]
//   8  u32 sensor frame counter
//  12  u32 frames dropped by the receiver
//  16  u32 receiver errors
//  20  u16 payload length [bytes]
//  22  u16 sensor temperature
//  24  u16 laser driver temperature
//  26  u16 checksum, byte sum of offsets 4..25 and the payload
//  28  payload, 12-bit distance per zone in 2 mm steps, row by row
//      0 = no target or amplitude below min_amplitude, 4095 = 8190 mm or more
//      two zones a, b packed in 3 bytes: a[7:0], a[11:8] | b[3:0] << 4, b[11:4]

#define VL53L9_STREAM_VERSION     5
#define VL53L9_STREAM_HEADER_SIZE 28
#define VL53L9_STREAM_MM_PER_LSB  2     // distance step of the packed values
#define VL53L9_STREAM_PAYLOAD_MAX ((3 * 54 * 42 + 1) / 2)   // payload size for binning 2, the largest grid

void vl53l9_stream_frame(uart_t *uart, const uint8_t *frame, uint8_t binning, vl53l9_dss_mode_t dss,
                         uint16_t min_amplitude, uint32_t dropped, uint32_t errors);

#endif
