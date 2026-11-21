#pragma once

// =============================================================================
//  mcp3208.h  -  Driver for MCP3208 8-channel 12-bit SPI ADC
//
//  Two instances are used:
//    ADC1 (PIN_ADC1_CS) -> Left Panel  : rudder, throttle, mixture, brake_hold
//    ADC2 (PIN_ADC2_CS) -> Flightstick : X-axis, Y-axis
//
//  SPI must be initialised by the caller before calling mcp3208_init().
// =============================================================================

#include <stdint.h>
#include "hardware/spi.h"

// Represents one MCP3208 chip instance
typedef struct {
    spi_inst_t *spi;    // SPI peripheral (spi0 or spi1)
    uint        cs_pin; // Chip-select GPIO (active low)
} mcp3208_t;

// -----------------------------------------------------------------------------
//  Initialise a MCP3208 instance.
//  Sets cs_pin as output and drives it high (deselected).
// -----------------------------------------------------------------------------
void mcp3208_init(mcp3208_t *dev, spi_inst_t *spi, uint cs_pin);

// -----------------------------------------------------------------------------
//  Read a single-ended channel (0-7).
//  Returns a 12-bit value [0 .. 4095].
//  Returns 0 on error.
// -----------------------------------------------------------------------------
uint16_t mcp3208_read(const mcp3208_t *dev, uint8_t channel);

// -----------------------------------------------------------------------------
//  Convenience: read and normalise to a signed range [-32767 .. +32767].
//  mid_raw  - raw ADC value that maps to 0 (centre), typically 2048.
// -----------------------------------------------------------------------------
int16_t mcp3208_read_signed(const mcp3208_t *dev, uint8_t channel, uint16_t mid_raw);

// -----------------------------------------------------------------------------
//  Convenience: read and normalise to an unsigned range [0 .. 32767].
//  Useful for sliders / throttle / mixture.
// -----------------------------------------------------------------------------
int16_t mcp3208_read_unsigned(const mcp3208_t *dev, uint8_t channel);
