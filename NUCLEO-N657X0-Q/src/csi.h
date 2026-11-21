#ifndef CSI_H
#define CSI_H

#include <stddef.h>
#include <stdint.h>
#include "stm32n6xx.h"

// MIPI CSI-2 receiver, DCMIPP pipe0 copies the received data into memory

#define CSI_BUF_ALIGN 32                // buffer alignment for DCMIPP and the D-cache
#define CSI_BUF_COUNT 3                 // one being written, one ready, one being read
#define CSI_PHY_BAND_1000 0x0A          // D-PHY frequency range for 1 Gbit/s per lane

typedef struct
{
    uint8_t volatile capture;          // buffer being writted via DMA from sensor
    uint8_t volatile waiting;          // next available full frame
    uint8_t volatile in_use;           // frame currently being used by the CPU
} csi_buffers;

typedef struct
{
    uint8_t phy_band;                   // D-PHY frequency range for the lane bit rate
    uint16_t osc_freq_target;           // D-PHY DLL target
    uint8_t data_type;                  // CSI-2 data type captured by pipe0 on virtual channel 0
    uint32_t buf_size;                  // bytes per buffer

    uint8_t *buf[CSI_BUF_COUNT];        // frame buffers, CSI_BUF_ALIGN aligned
} csi_config_t;

// runtime state of one receiver
typedef struct
{
    const csi_config_t *config;

    csi_buffers buffers;
    volatile uint8_t new_frame;         // 1 when waiting holds a frame not yet taken by csi_get_frame
    volatile uint32_t frame_count;      // frames captured
    volatile uint32_t errors;           // overrun, limit or AXI errors
} csi_t;


void csi_init(csi_t *csi);
int csi_start(csi_t *csi);
const uint8_t *csi_get_frame(csi_t *csi);
void csi_irq(csi_t *csi);

#endif