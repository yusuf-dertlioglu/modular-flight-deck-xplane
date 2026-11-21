#pragma once

// =============================================================================
//  mux_hc4067.h  -  Driver for CD74HC4067 16-channel analog multiplexer
//
//  The selected channel is connected to the SIG (common I/O) pin.
//  Channel address is set via four digital pins S0-S3.
//
//  The RP2040 ADC reads SIG via GPIO27 (ADC channel 1).
//  Digital channels (with pull-up) are detected by comparing the ADC value
//  against MUX_DIGITAL_THRESHOLD defined in config.h.
// =============================================================================

#include <stdint.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------
//  Initialise the multiplexer GPIOs and the RP2040 ADC.
//  Call once from Core0 before the main loop.
// -----------------------------------------------------------------------------
void mux_init(void);

// -----------------------------------------------------------------------------
//  Select a channel (0-15) and wait for the signal to settle.
//  After this call the ADC can be read immediately.
// -----------------------------------------------------------------------------
void mux_select_channel(uint8_t channel);

// -----------------------------------------------------------------------------
//  Read the currently selected channel as a raw 12-bit ADC value [0..4095].
// -----------------------------------------------------------------------------
uint16_t mux_read_raw(void);

// -----------------------------------------------------------------------------
//  Read a digital channel (pull-up resistor):
//  Returns true  when the contact is CLOSED (input pulled to GND).
//  Returns false when the contact is OPEN   (input at 3.3 V via pull-up).
// -----------------------------------------------------------------------------
bool mux_read_digital(uint8_t channel);

// -----------------------------------------------------------------------------
//  Read an analog channel and return a signed value [-32767 .. +32767].
//  mid_raw : raw ADC value that represents the centre position (usually 2048).
// -----------------------------------------------------------------------------
int16_t mux_read_analog_signed(uint8_t channel, uint16_t mid_raw);
