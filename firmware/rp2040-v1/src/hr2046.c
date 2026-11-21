// =============================================================================
//  hr2046.c  -  HR2046 / XPT2046 touch controller driver implementation
// =============================================================================

#include "hr2046.h"
#include "config.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
//  XPT2046 / HR2046 command bytes
//  Format: [S=1][A2][A1][A0][MODE=0][SER/DFR][PD1][PD0]
//    A2:A0  channel select
//    MODE=0 -> 12-bit conversion
//    SER=1  -> single-ended
//    PD1:0  -> power-down mode (00 = auto power-down between samples)
// ---------------------------------------------------------------------------
#define CMD_READ_X   0xD0   // Channel 5 (X position), single-ended
#define CMD_READ_Y   0x90   // Channel 1 (Y position), single-ended
#define CMD_READ_Z1  0xB0   // Channel 3 (pressure Z1)
#define CMD_READ_Z2  0xC0   // Channel 4 (pressure Z2)

// Number of samples to average per read
#define TOUCH_SAMPLES  1

uint16_t raw_x;
uint16_t raw_y;
int16_t px;
int16_t py;

// ---------------------------------------------------------------------------
//  SPI helper: send one command byte, receive 12-bit result
//  The HR2046 returns data MSB-first in the following 2 bytes.
// ---------------------------------------------------------------------------
/*static uint16_t touch_transfer(uint8_t cmd)
{
    spi_set_baudrate(SPI_DISP_PORT, SPI_TOUCH_SPEED_HZ);

    gpio_put(PIN_DISP_CS, 1);
    gpio_put(PIN_TOUCH_CS, 0);
    
    uint8_t tx[3] = {cmd, 0x00, 0x00};
    uint8_t rx[3] = {0,   0,    0};

    // Switch to touch SPI speed
    spi_write_read_blocking(SPI_DISP_PORT, tx, rx, 3);
    gpio_put(PIN_TOUCH_CS, 1);

    // Restore display SPI speed
    spi_set_baudrate(SPI_DISP_PORT, SPI_DISP_SPEED_HZ);

    // 12-bit result is in bits [14:3] of the 24-bit response
    return (uint16_t)((((uint16_t)rx[1] << 8) | rx[2]) >> 3);
}*/

static uint16_t touch_transfer(uint8_t cmd)
{
    // Sperrt den Bus für andere Cores / Interrupts
    critical_section_enter_blocking(&spi_mutex);

    gpio_put(PIN_DISP_CS, 1);
    spi_set_baudrate(SPI_DISP_PORT, SPI_TOUCH_SPEED_HZ);

    gpio_put(PIN_TOUCH_CS, 0);
    sleep_us(1);

    uint8_t tx[3] = {cmd, 0x00, 0x00};
    uint8_t rx[3] = {0, 0, 0};
    spi_write_read_blocking(SPI_DISP_PORT, tx, rx, 3);

    gpio_put(PIN_TOUCH_CS, 1);
    sleep_us(2);

    spi_set_baudrate(SPI_DISP_PORT, SPI_DISP_SPEED_HZ);

    // Sperre wieder freigeben
    critical_section_exit(&spi_mutex);

    return (uint16_t)((((uint16_t)rx[1] << 8) | rx[2]) >> 3);
}

// ---------------------------------------------------------------------------
//  Read and average TOUCH_SAMPLES raw values for one channel
// ---------------------------------------------------------------------------
static uint16_t touch_read_avg(uint8_t cmd)
{
    uint32_t sum = 0;
    for (int i = 0; i < TOUCH_SAMPLES; i++) {
        sum += touch_transfer(cmd);
    }
    return (uint16_t)(sum / TOUCH_SAMPLES);
}

// ---------------------------------------------------------------------------
//  Map a raw ADC value to a display pixel coordinate
// ---------------------------------------------------------------------------
static int16_t map_touch(uint16_t raw, uint16_t raw_min, uint16_t raw_max,
                          int16_t px_max)
{
    if (raw <= raw_min) return 0;
    if (raw >= raw_max) return px_max;
    return (int16_t)((uint32_t)(raw - raw_min) * (uint32_t)px_max /
                     (uint32_t)(raw_max - raw_min));
}

// ---------------------------------------------------------------------------
void hr2046_init(void)
{
    gpio_init(PIN_TOUCH_CS);
    gpio_set_dir(PIN_TOUCH_CS, GPIO_OUT);
    gpio_put(PIN_TOUCH_CS, 1);  // deselect

    gpio_init(PIN_TOUCH_IRQ);
    gpio_set_dir(PIN_TOUCH_IRQ, GPIO_IN);
    gpio_pull_up(PIN_TOUCH_IRQ);
}

// ---------------------------------------------------------------------------
void hr2046_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    // Quick IRQ check: T_IRQ goes LOW when the panel is touched
    /*if (gpio_get(PIN_TOUCH_IRQ)) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }*/

    // Read X, Y and pressure
    //raw_x = touch_read_avg(CMD_READ_X);
    //raw_y  = touch_read_avg(CMD_READ_Y);
    raw_x = touch_transfer(CMD_READ_X);
    raw_y  = touch_transfer(CMD_READ_Y);
    uint16_t raw_z1 = touch_transfer(CMD_READ_Z1);
    uint16_t raw_z2 = touch_transfer(CMD_READ_Z2);

    // Pressure estimate: z = (x/4096) * (z2/z1 - 1)
    // A simple threshold on z1 alone is also sufficient
    if (raw_z1 < TOUCH_PRESSURE_MIN || raw_x == 0 || raw_y == 0 || raw_x >= 4095 || raw_y >= 4095) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    (void)raw_z2;

    // Map raw coordinates to display pixels.
    // The axis orientation depends on MADCTL rotation; swap/invert as needed.
#if (DISP_MADCTL == 0x88)
    // Portrait, cable at bottom: X raw maps to display X, Y raw maps to display Y
    // but the axes may be swapped or inverted depending on the panel.
    // Adjust TOUCH_X_MIN/MAX and TOUCH_Y_MIN/MAX in hr2046.h after calibration.
    px = map_touch(raw_x, TOUCH_X_MIN, TOUCH_X_MAX, DISP_HEIGHT  - 1);
    py = map_touch(raw_y, TOUCH_Y_MIN, TOUCH_Y_MAX, DISP_WIDTH - 1);
#else
    int16_t px = map_touch(raw_x, TOUCH_X_MIN, TOUCH_X_MAX, DISP_WIDTH  - 1);
    int16_t py = map_touch(raw_y, TOUCH_Y_MIN, TOUCH_Y_MAX, DISP_HEIGHT - 1);
#endif

    //data->point.x = px;
    //data->point.y = py;
    data->point.x = py;
    data->point.y = px;

    data->state   = LV_INDEV_STATE_PRESSED;

    /*printf("rawX=%u rawY=%u px=%d py=%d\n",
       raw_x,
       raw_y,
       px,
       py);*/
}
