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
