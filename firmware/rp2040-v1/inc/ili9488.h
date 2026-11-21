#pragma once

// =============================================================================
//  ili9488.h  -  ILI9488 3.5" TFT display driver (SPI, 18-bit color / RGB666)
//
//  ILI9488 does not support 16-bit RGB565 over 4-wire SPI natively.
//  Internally we work with lv_color_t (RGB565), and the flush callback
//  converts each pixel to 3-byte RGB666 before sending it to the display.
//
//  The driver exposes an LVGL flush callback so that LVGL renders directly
//  to the display through its partial-rendering pipeline.
// =============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <lvgl.h>

// -----------------------------------------------------------------------------
//  Hardware initialisation.
//  - Configures SPI0 pins (SCK, MOSI, MISO, CS, DC, RST).
//  - Runs the ILI9488 init command sequence.
//  - Sets rotation according to DISP_MADCTL (config.h).
//  Call from Core1 before lv_init().
// -----------------------------------------------------------------------------
void ili9488_init(void);

// -----------------------------------------------------------------------------
//  LVGL flush callback.
//  Registered with lv_disp_drv_t.flush_cb.
//  Converts RGB565 -> RGB666 and sends the pixel rectangle via SPI DMA.
// -----------------------------------------------------------------------------
void ili9488_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);

// -----------------------------------------------------------------------------
//  Low-level helpers (used internally and by hr2046 calibration if needed)
// -----------------------------------------------------------------------------

// Set the active drawing window on the display
void ili9488_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

// Send a command byte (DC = 0)
void ili9488_send_cmd(uint8_t cmd);

// Send one or more data bytes (DC = 1)
void ili9488_send_data(const uint8_t *data, size_t len);

// Fill a rectangular area with a single RGB565 color (useful for clearing)
void ili9488_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color565);
