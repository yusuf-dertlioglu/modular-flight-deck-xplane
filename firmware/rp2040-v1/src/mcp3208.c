// =============================================================================
//  mcp3208.c  -  MCP3208 12-bit SPI ADC driver implementation
// =============================================================================

#include "mcp3208.h"
#include "config.h"
#include "hardware/gpio.h"
#include <stddef.h>

// -----------------------------------------------------------------------------
//  MCP3208 SPI frame (24 bits / 3 bytes, single-ended mode)
//
//  TX byte 0: [0 0 0 0 0 1 1 D2]   start=1, SGL=1, D2
//  TX byte 1: [D1 D0 0 0 0 0 0 0]  D1, D0, then padding
//  TX byte 2: [0 0 0 0 0 0 0 0]    clocks for the 12 result bits
//
//  RX byte 0: don't care
//  RX byte 1: [x x x x B11 B10 B9 B8]  upper 4 bits in bits [3:0]
//  RX byte 2: [B7 B6 B5 B4 B3 B2 B1 B0] lower 8 bits
//
//  Result = ((rx[1] & 0x0F) << 8) | rx[2]
// -----------------------------------------------------------------------------

void mcp3208_init(mcp3208_t *dev, spi_inst_t *spi, uint cs_pin)
{
    dev->spi    = spi;
    dev->cs_pin = cs_pin;

    gpio_init(cs_pin);
    gpio_set_dir(cs_pin, GPIO_OUT);
    gpio_put(cs_pin, 1);  // deselect
}

uint16_t mcp3208_read(const mcp3208_t *dev, uint8_t channel)
{
    if (channel > 7) return 0;

    uint8_t tx[3] = {
        (uint8_t)(0x06 | (channel >> 2)),    // start + SGL + D2
        (uint8_t)((channel & 0x03) << 6),    // D1, D0
        0x00
    };
    uint8_t rx[3] = {0, 0, 0};

    gpio_put(dev->cs_pin, 0);
    spi_write_read_blocking(dev->spi, tx, rx, 3);
    gpio_put(dev->cs_pin, 1);

    uint16_t result = (uint16_t)(((rx[1] & 0x0F) << 8) | rx[2]);
    return result;
}

int16_t mcp3208_read_signed(const mcp3208_t *dev, uint8_t channel, uint16_t mid_raw)
{
    uint16_t raw = mcp3208_read(dev, channel);

    // Map [0..4095] relative to mid_raw into [-32767..+32767]
    int32_t shifted = (int32_t)raw - (int32_t)mid_raw;
    int32_t scaled;

    if (shifted >= 0) {
        // positive half: [0 .. (4095 - mid_raw)] -> [0 .. 32767]
        int32_t range = 4095 - (int32_t)mid_raw;
        if (range <= 0) return 0;
        scaled = (shifted * 32767) / range;
    } else {
        // negative half: [-mid_raw .. 0] -> [-32767 .. 0]
        if ((int32_t)mid_raw == 0) return 0;
        scaled = (shifted * 32767) / (int32_t)mid_raw;
    }

    if (scaled >  32767) scaled =  32767;
    if (scaled < -32767) scaled = -32767;

    return (int16_t)scaled;
}

int16_t mcp3208_read_unsigned(const mcp3208_t *dev, uint8_t channel)
{
    uint16_t raw = mcp3208_read(dev, channel);
    // Map [0..4095] -> [0..32767]
    int16_t val = (int16_t)((uint32_t)raw * 32767u / 4095u);
    return val;
}
