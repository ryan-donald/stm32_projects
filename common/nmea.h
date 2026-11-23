#ifndef NMEA_H
#define NMEA_H

#include <stdbool.h>
#include <stdint.h>
#include "uart.h"

#define NMEA_LINE_SIZE  96              // NMEA max is 82 chars including $ and \r\n
#define NMEA_MAX_FIELDS 20              // fix data frame has 15 fields

// position from the latest fix data frame
typedef struct
{
    uint32_t time_ms;                   // UTC time of day [ms]
    int32_t lat;                        // latitude, north positive [1e-7 deg]
    int32_t lon;                        // longitude, east positive [1e-7 deg]
    int32_t alt_mm;                     // altitude above mean sea level [mm]
    uint32_t hdop;                      // horizontal dilution of precision [x100]
    uint8_t quality;                    // 0 = none, 1 = GNSS, 2 = differential, 6 = dead reckoning
    uint8_t satellites;                 // satellites used in the fix
} nmea_fix_t;

// fixed per receiver
typedef struct
{
    uart_t *uart;
} nmea_config_t;

// runtime state of one receiver
typedef struct
{
    const nmea_config_t *config;

    char line[NMEA_LINE_SIZE];          // data frame being received
    uint32_t line_len;

    nmea_fix_t fix;
    uint32_t fix_count;                 // fix data frames parsed
    uint32_t bad_count;                 // data frames dropped for checksum, format or length
} nmea_t;


bool nmea_poll(nmea_t *gnss);

#endif
