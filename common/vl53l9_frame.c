#include <string.h>
#include "vl53l9_frame.h"

uint32_t vl53l9_frame_lines(uint8_t binning, vl53l9_dss_mode_t dss)
{
    switch (binning)
    {
    case 2:  return dss ? 149 : 138;
    case 4:  return dss ? 39 : 36;
    case 6:  return dss ? 18 : 17;
    case 8:  return 9;
    case 12: return dss ? 6 : 5;
    case 24: return dss ? 3 : 2;
    default: return 0;
    }
}

// zone grid size, binning 4 and 12 include extra rows with no data
void vl53l9_frame_zones(uint8_t binning, uint32_t *width, uint32_t *height)
{
    switch (binning)
    {
    case 2:  *width = 54; *height = 42; break;
    case 4:  *width = 24; *height = 24; break;
    case 6:  *width = 18; *height = 14; break;
    case 8:  *width = 12; *height = 10; break;
    case 12: *width = 8;  *height = 8;  break;
    case 24: *width = 4;  *height = 4;  break;
    default: *width = 0;  *height = 0;  break;
    }
}

// distance of a zone [mm]
uint16_t vl53l9_frame_distance(const uint8_t *frame, uint32_t zone)
{
    uint16_t distance;
    memcpy(&distance, &frame[zone*2], sizeof(distance));
    return distance & 0x7FFF;
}

// signal amplitude of a zone
uint16_t vl53l9_frame_amplitude(const uint8_t *frame, uint8_t binning, uint32_t zone)
{
    uint32_t width, height;
    vl53l9_frame_zones(binning, &width, &height);

    uint16_t amplitude;
    memcpy(&amplitude, &frame[2*width*height + 2*zone], sizeof(amplitude));
    return amplitude;
}

// reads the fields of the status line
void vl53l9_frame_status(const uint8_t *frame, uint8_t binning, vl53l9_dss_mode_t dss,
                         vl53l9_frame_status_t *status)
{
    uint32_t status_idx = (vl53l9_frame_lines(binning, dss) - 1) * VL53L9_CSI_LINE_SIZE;

    // little-endian fields copied straight into the little-endian CPU's variables
    memcpy(&status->frame_counter, &frame[status_idx], sizeof(status->frame_counter));
    memcpy(&status->temperature, &frame[status_idx + 4], sizeof(status->temperature));
    memcpy(&status->laser_temperature, &frame[status_idx + 6], sizeof(status->laser_temperature));
}
