#include "i2c.h"

#define I2C_TICK_HZ     16000000UL      // timing clock after the prescaler [Hz]
#define I2C_NBYTES_MAX  255             // largest NBYTES
#define I2C_TIMEOUT     1000000UL       // timeout
#define I2C_ERRORS      (I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO)

void i2c_init(i2c_t *i2c, uint32_t scl_hz)
{
    I2C_TypeDef *registers = i2c->config->i2c_registers;

    // prescaler that brings the kernel clock down to about 16 MHz
    uint32_t presc = (i2c->config->ker_clk + I2C_TICK_HZ - 1) / I2C_TICK_HZ - 1;
    uint32_t ticks = (i2c->config->ker_clk / (presc + 1)) / scl_hz;

    // SCL low for 55 % of the period and high for 45 %
    uint32_t low = (ticks * 55 + 99) / 100;
    uint32_t high = ticks - low;

    // disable I2C to configure
    registers->CR1 = 0;
    // data setup time 4 ticks, data hold time 1 tick, then the SCL high and low times
    registers->TIMINGR = (presc << I2C_TIMINGR_PRESC_Pos) | (3 << I2C_TIMINGR_SCLDEL_Pos)
                       | (1 << I2C_TIMINGR_SDADEL_Pos) | ((high - 1) << I2C_TIMINGR_SCLH_Pos)
                       | ((low - 1) << I2C_TIMINGR_SCLL_Pos);
    registers->CR1 = I2C_CR1_PE;
}

// clears the error flags and resets I2C
static int i2c_fail(I2C_TypeDef *registers)
{
    registers->ICR = I2C_ICR_NACKCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_STOPCF;
    registers->CR1 &= ~I2C_CR1_PE;
    while (registers->CR1 & I2C_CR1_PE) { };
    registers->CR1 |= I2C_CR1_PE;
    return -1;
}

// waits for a flag, returns -1 on failure
static int i2c_wait(I2C_TypeDef *registers, uint32_t flag)
{
    for (uint32_t i = 0; i < I2C_TIMEOUT; i++)
    {
        uint32_t isr = registers->ISR;
        if (isr & I2C_ERRORS)
        {
            return i2c_fail(registers);
        }
        if (isr & flag)
        {
            return 0;
        }
    }
    return i2c_fail(registers);
}

// sets NBYTES for the next chunk, with RELOAD if more chunks follow
static uint32_t i2c_chunk(uint32_t remaining, uint32_t end)
{
    if (remaining > I2C_NBYTES_MAX)
    {
        return (I2C_NBYTES_MAX << I2C_CR2_NBYTES_Pos) | I2C_CR2_RELOAD;
    }
    return (remaining << I2C_CR2_NBYTES_Pos) | end;
}

// writes to a specified device + register
int i2c_write_reg16(i2c_t *i2c, uint8_t addr, uint16_t reg, const uint8_t *data, uint32_t len)
{
    I2C_TypeDef *registers = i2c->config->i2c_registers;
    uint32_t remaining = len + 2;
    uint32_t chunk_left = (remaining > I2C_NBYTES_MAX) ? I2C_NBYTES_MAX : remaining;

    registers->CR2 = ((uint32_t)addr << 1) | i2c_chunk(remaining, I2C_CR2_AUTOEND) | I2C_CR2_START;

    for (uint32_t i = 0; i < len + 2; i++)
    {
        // setup next chunk
        if (chunk_left == 0)
        {
            if (i2c_wait(registers, I2C_ISR_TCR) != 0)
            {
                return -1;
            }
            registers->CR2 = ((uint32_t)addr << 1) | i2c_chunk(remaining, I2C_CR2_AUTOEND);
            chunk_left = (remaining > I2C_NBYTES_MAX) ? I2C_NBYTES_MAX : remaining;
        }

        if (i2c_wait(registers, I2C_ISR_TXIS) != 0)
        {
            return -1;
        }

        // register address high byte, low byte, then the data
        if (i == 0)
        {
            registers->TXDR = reg >> 8;
        }
        else if (i == 1)
        {
            registers->TXDR = reg & 0xFF;
        }
        else
        {
            registers->TXDR = data[i - 2];
        }
        chunk_left--;
        remaining--;
    }

    // wait for the stop sent after the last byte
    if (i2c_wait(registers, I2C_ISR_STOPF) != 0)
    {
        return -1;
    }
    registers->ICR = I2C_ICR_STOPCF;
    return 0;
}

// reads from a specified device + register
int i2c_read_reg16(i2c_t *i2c, uint8_t addr, uint16_t reg, uint8_t *buf, uint32_t len)
{
    // tell the device which register we want to read from
    I2C_TypeDef *registers = i2c->config->i2c_registers;

    registers->CR2 = ((uint32_t)addr << 1) | (2 << I2C_CR2_NBYTES_Pos) | I2C_CR2_START;
    if (i2c_wait(registers, I2C_ISR_TXIS) != 0)
    {
        return -1;
    }
    registers->TXDR = reg >> 8;
    if (i2c_wait(registers, I2C_ISR_TXIS) != 0)
    {
        return -1;
    }
    registers->TXDR = reg & 0xFF;
    if (i2c_wait(registers, I2C_ISR_TC) != 0)
    {
        return -1;
    }

    // read from the device the register data we specified prior
    uint32_t remaining = len;
    uint32_t chunk_left = (remaining > I2C_NBYTES_MAX) ? I2C_NBYTES_MAX : remaining;
    registers->CR2 = ((uint32_t)addr << 1) | I2C_CR2_RD_WRN | i2c_chunk(remaining, I2C_CR2_AUTOEND)
                   | I2C_CR2_START;

    for (uint32_t i = 0; i < len; i++)
    {
        // setup next chunk
        if (chunk_left == 0)
        {
            if (i2c_wait(registers, I2C_ISR_TCR) != 0)
            {
                return -1;
            }
            registers->CR2 = ((uint32_t)addr << 1) | I2C_CR2_RD_WRN | i2c_chunk(remaining, I2C_CR2_AUTOEND);
            chunk_left = (remaining > I2C_NBYTES_MAX) ? I2C_NBYTES_MAX : remaining;
        }

        if (i2c_wait(registers, I2C_ISR_RXNE) != 0)
        {
            return -1;
        }
        buf[i] = registers->RXDR;
        chunk_left--;
        remaining--;
    }

    if (i2c_wait(registers, I2C_ISR_STOPF) != 0)
    {
        return -1;
    }
    registers->ICR = I2C_ICR_STOPCF;
    return 0;
}
