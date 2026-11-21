#ifndef BSP_VL53L9_H
#define BSP_VL53L9_H

#include "vl53l9.h"
#include "vl53l9_i2c.h"

#define TOF_ERR_CSI -10                // CSI-2 virtual channel did not start

// ranging settings applied by tof_init
typedef struct
{
    vl53l9_context_t context;
    vl53l9_power_mode_t power;
    vl53l9_dss_mode_t dss;              // VL53L9_DSS_SHORT / _LONG matching the context, or VL53L9_DSS_DISABLE
    uint8_t binning;                    // 2 (54x42), 4 (24x20), 6 (18x14), 8 (12x10), 12 (8x6), 24 (4x4)
    uint16_t exposure_ms;
    uint32_t frame_period_us;
} tof_ranging_t;

extern vl53l9_i2c_t tof;
extern const tof_ranging_t tof_ranging;
extern const char *tof_failed_step;

int tof_init(void);

#endif
