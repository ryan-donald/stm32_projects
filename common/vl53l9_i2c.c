#include "vl53l9_i2c.h"
#include "vl53l9_platform.h"
#include "delay.h"

// function implementations for ST provided driver

int vl53l9_read(void *const p_dev, uint16_t address, uint8_t *p_values, uint32_t size)
{
    vl53l9_i2c_t *dev = p_dev;
    if (i2c_read_reg16(dev->i2c, dev->address, address, p_values, size) != 0)
    {
        return VL53L9_ERROR_PLATFORM;
    }
    return VL53L9_ERROR_NONE;
}

int vl53l9_write(void *const p_dev, uint16_t address, uint8_t *p_values, uint32_t size)
{
    vl53l9_i2c_t *dev = p_dev;
    if (i2c_write_reg16(dev->i2c, dev->address, address, p_values, size) != 0)
    {
        return VL53L9_ERROR_PLATFORM;
    }
    return VL53L9_ERROR_NONE;
}

// same as vl53l9_read, the driver only uses this for I3C
int vl53l9_read_async(void *const p_dev, uint16_t address, volatile uint8_t *p_values, uint32_t size)
{
    return vl53l9_read(p_dev, address, (uint8_t *)p_values, size);
}

int vl53l9_read8(void *const p_dev, uint16_t address, uint8_t *p_value)
{
    return vl53l9_read(p_dev, address, p_value, 1);
}

int vl53l9_read16(void *const p_dev, uint16_t address, uint16_t *p_value)
{
    uint8_t data[2];
    int ret = vl53l9_read(p_dev, address, data, 2);
    *p_value = data[0] | (data[1] << 8);
    return ret;
}

int vl53l9_read32(void *const p_dev, uint16_t address, uint32_t *p_value)
{
    uint8_t data[4];
    int ret = vl53l9_read(p_dev, address, data, 4);
    *p_value = data[0] | (data[1] << 8) | (data[2] << 16) | ((uint32_t)data[3] << 24);
    return ret;
}

int vl53l9_write8(void *const p_dev, uint16_t address, uint8_t value)
{
    return vl53l9_write(p_dev, address, &value, 1);
}

int vl53l9_write16(void *const p_dev, uint16_t address, uint16_t value)
{
    uint8_t data[2] = { value & 0xFF, value >> 8 };
    return vl53l9_write(p_dev, address, data, 2);
}

int vl53l9_write32(void *const p_dev, uint16_t address, uint32_t value)
{
    uint8_t data[4] = { value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF, value >> 24 };
    return vl53l9_write(p_dev, address, data, 4);
}

int vl53l9_wait_ms(void *const p_dev, uint32_t ms)
{
    (void)p_dev;
    delay_ms(ms);
    return VL53L9_ERROR_NONE;
}

int vl53l9_get_config_vddio(void *const p_dev, vl53l9_vddio_t *voltage)
{
    *voltage = ((vl53l9_i2c_t *)p_dev)->vddio;
    return VL53L9_ERROR_NONE;
}

int vl53l9_get_config_vdda(void *const p_dev, vl53l9_vdda_t *voltage)
{
    *voltage = ((vl53l9_i2c_t *)p_dev)->vdda;
    return VL53L9_ERROR_NONE;
}

int vl53l9_get_config_ext_clock(void *const p_dev, uint32_t *ext_clock)
{
    *ext_clock = ((vl53l9_i2c_t *)p_dev)->ext_clock;
    return VL53L9_ERROR_NONE;
}
