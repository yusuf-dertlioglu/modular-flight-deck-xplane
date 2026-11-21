#pragma once

// =============================================================================
//  hr2046.h  -  HR2046 / XPT2046-compatible resistive touch controller driver
//
//  The HR2046 is pin- and protocol-compatible with the XPT2046.
//  It shares SPI0 with the ILI9488 display but uses a separate CS pin
//  (PIN_TOUCH_CS) and a lower SPI clock speed (SPI_TOUCH_SPEED_HZ).
//
//  Touch IRQ (PIN_TOUCH_IRQ) is polled rather than interrupt-driven because
//  the display driver also runs on Core1 and LVGL handles the read period.
// =============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <lvgl.h>

// Calibration constants (map raw ADC counts to display pixels).
// Adjust these values after a physical calibration run.
#define TOUCH_X_MIN     280 //380
#define TOUCH_X_MAX    3800 //3800
#define TOUCH_Y_MIN     440 //700
#define TOUCH_Y_MAX    3650 //3800

// Minimum pressure reading to consider a touch valid
#define TOUCH_PRESSURE_MIN  200

// -----------------------------------------------------------------------------
//  Initialise the touch controller.
//  The SPI bus (spi0) must already be initialised by ili9488_init().
//  This function configures the CS and IRQ pins.
// -----------------------------------------------------------------------------
void hr2046_init(void);

// -----------------------------------------------------------------------------
//  LVGL input device read callback.
//  Registered with lv_indev_drv_t.read_cb.
//  Reads X/Y/pressure from the HR2046 and fills the LVGL data structure.
// -----------------------------------------------------------------------------
void hr2046_read(lv_indev_drv_t *drv, lv_indev_data_t *data);

extern uint16_t raw_x;
extern uint16_t raw_y;
extern int16_t px;
extern int16_t py;
