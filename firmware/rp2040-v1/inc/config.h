#pragma once

// =============================================================================
//  config.h  -  Central pin and system configuration
//  Raspberry Pi Pico - Flight Simulator Panel (RP2040)
// =============================================================================

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pico/critical_section.h"

// --- SPI0: ILI9488 Display + HR2046 Touch ------------------------------------
#define SPI_DISP_PORT           spi0
#define SPI_DISP_SPEED_HZ       40000000U   // 40 MHz  (display write)
#define SPI_TOUCH_SPEED_HZ       2000000U   //  2 MHz (touch controller)

#define LED_PIN                 PICO_DEFAULT_LED_PIN

#define PIN_DISP_MISO           16          // T_OUT
#define PIN_DISP_CS             17
#define PIN_DISP_SCK            18
#define PIN_DISP_MOSI           19          // SDI / T_DIN (shared)
#define PIN_DISP_DC             20
#define PIN_DISP_RST            21
#define PIN_TOUCH_CS            22
#define PIN_TOUCH_IRQ           26

// --- SPI1: MCP3208 ADCs ------------------------------------------------------
#define SPI_ADC_PORT            spi1
// MCP3208 max 1.8 MHz @ Vdd = 2.7 V  /  3.6 MHz @ Vdd = 5 V
// With Pico 3.3 V supply -> safe at 1 MHz
#define SPI_ADC_SPEED_HZ         1000000U

#define PIN_ADC_MISO             8
#define PIN_ADC_SCK             10
#define PIN_ADC_MOSI            11
#define PIN_ADC1_CS              6          // Left Panel  (channels 0-3 used)
#define PIN_ADC2_CS              7          // Flightstick (channels 0-1 used)

// --- CD74HC4067 Multiplexer --------------------------------------------------
#define PIN_MUX_S0              12
#define PIN_MUX_S1              13
#define PIN_MUX_S2              14
#define PIN_MUX_S3              15
#define PIN_MUX_SIG             27          // GPIO27 = ADC channel 1
#define MUX_ADC_CHANNEL          1          // passed to adc_select_input()

// Settling time for the sample-and-hold capacitor after channel switch.
// 50 us is safe for typical RC combinations on the ADC input.
#define MUX_SETTLE_US           50

// Number of MUX channels in use
#define MUX_DIGITAL_CHANNELS    10          // Ch 0-9  -> digital (pull-up)
#define MUX_ANALOG_CHANNELS      2          // Ch 10-11 -> analog (joystick)
#define MUX_TOTAL_CHANNELS      12

// ADC mid-scale threshold for digital detection (12-bit ADC -> 0..4095)
#define MUX_DIGITAL_THRESHOLD   2048        // below = pressed

// --- Rotary Encoder (Core 1, interrupt-driven) -------------------------------
#define PIN_ENC_CW               1          // Clockwise pulse      -> GPIO1
#define PIN_ENC_CCW              0          // Counter-clockwise    -> GPIO0
#define PIN_ENC_BTN              2          // Push button          -> GPIO2

// --- Display configuration ---------------------------------------------------
#define DISP_WIDTH             320
#define DISP_HEIGHT            480

// MADCTL register (0x36) - rotation / mirror flags
//   0x48 = MX | BGR       Portrait, cable at top
//   0x88 = MY | BGR       Portrait, cable at bottom  (bottom-to-top)
//   0x28 = MX | MV | BGR  Landscape 90 deg
//   0xE8 = MY|MX|MV|BGR   Landscape 270 deg
#define DISP_MADCTL            0x88

// LVGL render buffer: N lines x DISP_WIDTH pixels x 2 bytes (RGB565)
// 320 x 10 x 2 = 6.25 KB per buffer
#define LVGL_BUF_LINES          40

// --- Inter-core FIFO commands (Core 0 <-> Core 1) ----------------------------
#define FIFO_CMD_SYNC           0x00000001UL  // Core0 -> Core1: request sync
#define FIFO_ACK_SYNC           0x00000002UL  // Core1 -> Core0: sync done

// --- Timing ------------------------------------------------------------------
// Core0 main loop target period (input polling rate)
#define INPUT_LOOP_US           5000U       // 5 ms = 200 Hz

// USB CDC receive buffer size
#define CDC_RX_BUF_SIZE          256

// --- USB HID report ID -------------------------------------------------------
#define HID_REPORT_ID_JOYSTICK   1

static critical_section_t spi_mutex;
