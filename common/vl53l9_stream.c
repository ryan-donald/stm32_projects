#include <string.h>
#include "vl53l9_stream.h"
#include "vl53l9_frame.h"

static uint8_t payload[VL53L9_STREAM_PAYLOAD_MAX];   // packed distances of the frame being sent

// sends the distances of one frame as a packet, see vl53l9_stream.h
void vl53l9_stream_frame(uart_t *uart, const uint8_t *frame, uint8_t binning, vl53l9_dss_mode_t dss,
                         uint16_t min_amplitude, uint32_t dropped, uint32_t errors)
{
    uint32_t width, height;
    vl53l9_frame_zones(binning, &width, &height);
    uint32_t zones = width * height;
    uint16_t len = (3 * zones + 1) / 2;     // two 12-bit zones per 3 bytes

    for(uint16_t zone = 0, i = 0; i < len; i+=3, zone += 2)
    {
        uint16_t a_amp = vl53l9_frame_amplitude(frame, binning, zone);
        uint16_t b_amp = vl53l9_frame_amplitude(frame, binning, zone + 1);
        uint16_t a, b;

        // prefilter low amplitude zones
        if (a_amp < min_amplitude) 
        {
            a = 0u;
        }
        else
        {
            a = vl53l9_frame_distance(frame, zone);
        }

        if (b_amp < min_amplitude) 
        {
            b = 0u;
        }
        else
        {
            b = vl53l9_frame_distance(frame, zone + 1);
        }

        // output is in 2mm steps
        a = (a + VL53L9_STREAM_MM_PER_LSB / 2) / VL53L9_STREAM_MM_PER_LSB;
        b = (b + VL53L9_STREAM_MM_PER_LSB / 2) / VL53L9_STREAM_MM_PER_LSB;
        if (a > 0xFFF) {a = 0xFFF;}
        if (b > 0xFFF) {b = 0xFFF;}

        payload[i] = a & 0x00FF;
        payload[i+1] = ((0x0F00 & a) >> 8) | ((0x000F & b) << 4);
        payload[i+2] = (0x0FF & (b>>4));
    }

    vl53l9_frame_status_t status;
    vl53l9_frame_status(frame, binning, dss, &status);

    uint8_t header[VL53L9_STREAM_HEADER_SIZE];
    header[0] = 0xA5;
    header[1] = 0x5A;
    header[2] = 'V';
    header[3] = '9';
    header[4] = VL53L9_STREAM_VERSION;
    header[5] = binning;
    header[6] = width;
    header[7] = height;

    // little-endian fields copied straight from the little-endian CPU's variables
    memcpy(&header[8], &status.frame_counter, 4);
    memcpy(&header[12], &dropped, 4);
    memcpy(&header[16], &errors, 4);
    memcpy(&header[20], &len, 2);
    memcpy(&header[22], &status.temperature, 2);
    memcpy(&header[24], &status.laser_temperature, 2);

    // checksum of the header after the magic, and the payload
    uint16_t sum = 0;
    for (uint32_t i = 4; i < 26; i++)
    {
        sum += header[i];
    }
    for (uint32_t i = 0; i < len; i++)
    {
        sum += payload[i];
    }
    memcpy(&header[26], &sum, 2);

    uart_write(uart, header, VL53L9_STREAM_HEADER_SIZE);
    uart_write(uart, payload, len);

}
