// =============================================================================
//  ili9488.c  -  ILI9488 TFT display driver implementation
// =============================================================================

#include "ili9488.h"
#include "config.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"
#include <string.h>

// DMA channel for pixel transfers
static int s_dma_channel = -1;
static dma_channel_config s_dma_cfg;

// Pixel conversion buffer: worst-case one LVGL render strip
// LVGL_BUF_LINES * DISP_WIDTH * 3 bytes (RGB666)
// 10 * 320 * 3 = 9600 bytes
#define PIXEL_BUF_SIZE  (LVGL_BUF_LINES * DISP_WIDTH * 3)
static uint8_t s_pixel_buf[PIXEL_BUF_SIZE];

// ---------------------------------------------------------------------------
//  ILI9488 command list entry
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t  cmd;
    uint8_t  data[16];
    uint8_t  len;       // 0xFF = end of list, 0xFE = delay 120ms
} ili9488_init_cmd_t;

// ---------------------------------------------------------------------------
//  ILI9488 initialisation sequence
//  Based on the official ILI9488 application notes and Bodmer's TFT_eSPI
// ---------------------------------------------------------------------------
static const ili9488_init_cmd_t s_init_cmds[] = {
    {0xE0, {0x00,0x07,0x0F,0x0D,0x1B,0x0A,0x3C,0x78,0x4A,0x07,0x0E,0x09,0x1B,0x1E,0x0F}, 15}, // Positive Gamma
    {0xE1, {0x00,0x22,0x24,0x06,0x12,0x07,0x36,0x47,0x47,0x06,0x0A,0x07,0x30,0x37,0x0F}, 15}, // Negative Gamma
    {0xC0, {0x10,0x10}, 2},         // Power Control 1: Vreg1out, Verg2out
    {0xC1, {0x41},      1},         // Power Control 2: VGH, VGL
    {0xC5, {0x00,0x22,0x80}, 3},    // VCOM Control
    {0x36, {DISP_MADCTL}, 1},       // Memory Access Control (rotation)
    {0x3A, {0x66},      1},         // Interface Pixel Format: 18-bit (RGB666)
    {0xB0, {0x00},      1},         // Interface Mode Control
    {0xB1, {0xA0},      1},         // Frame Rate: 60 Hz
    {0xB4, {0x02},      1},         // Display Inversion Control: 2-dot
    {0xB6, {0x02,0x02}, 2},         // Display Function Control
    {0xE9, {0x00},      1},         // Set Image Function (disable 24-bit)
    {0xF7, {0xA9,0x51,0x2C,0x02},4},// Adjust Control 3
    {0x11, {0},         0xFE},      // Sleep out + 120 ms delay
    {0x29, {0},         0},         // Display ON
    {0x00, {0},         0xFF}       // End of list
};

// ---------------------------------------------------------------------------
//  Internal helpers
// ---------------------------------------------------------------------------
static inline void cs_select(void)   { gpio_put(PIN_DISP_CS, 0); }
static inline void cs_deselect(void) { gpio_put(PIN_DISP_CS, 1); }
static inline void dc_cmd(void)      { gpio_put(PIN_DISP_DC, 0); }
static inline void dc_data(void)     { gpio_put(PIN_DISP_DC, 1); }

void ili9488_send_cmd(uint8_t cmd)
{
    dc_cmd();
    cs_select();
    spi_write_blocking(SPI_DISP_PORT, &cmd, 1);
    cs_deselect();
}

void ili9488_send_data(const uint8_t *data, size_t len)
{
    dc_data();
    cs_select();
    spi_write_blocking(SPI_DISP_PORT, data, len);
    cs_deselect();
}

static void send_cmd_data(uint8_t cmd, const uint8_t *data, size_t len)
{
    ili9488_send_cmd(cmd);
    if (len > 0) {
        ili9488_send_data(data, len);
    }
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
void ili9488_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    // Column Address Set (CASET)
    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)(x0 & 0xFF);
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)(x1 & 0xFF);
    send_cmd_data(0x2A, data, 4);

    // Row Address Set (RASET)
    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)(y0 & 0xFF);
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)(y1 & 0xFF);
    send_cmd_data(0x2B, data, 4);

    // Memory Write (RAMWR) - pixels follow
    ili9488_send_cmd(0x2C);
}

void ili9488_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color565)
{
    // Convert RGB565 -> RGB666
    uint8_t r = (uint8_t)((color565 >> 11) & 0x1F) << 3;
    uint8_t g = (uint8_t)((color565 >>  5) & 0x3F) << 2;
    uint8_t b = (uint8_t)((color565      ) & 0x1F) << 3;

    ili9488_set_window(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    // Fill pixel buffer with the converted colour
    uint32_t pixel_count = (uint32_t)w * h;
    uint32_t buf_pixels = PIXEL_BUF_SIZE / 3;

    // Fill one buffer worth of pixels
    for (uint32_t i = 0; i < buf_pixels && i < pixel_count; i++) {
        s_pixel_buf[i * 3]     = r;
        s_pixel_buf[i * 3 + 1] = g;
        s_pixel_buf[i * 3 + 2] = b;
    }

    dc_data();
    cs_select();

    uint32_t remaining = pixel_count;
    while (remaining > 0) {
        uint32_t batch = (remaining < buf_pixels) ? remaining : buf_pixels;

        if (remaining < buf_pixels) {
            // Refill only the needed portion
            for (uint32_t i = 0; i < batch; i++) {
                s_pixel_buf[i * 3]     = r;
                s_pixel_buf[i * 3 + 1] = g;
                s_pixel_buf[i * 3 + 2] = b;
            }
        }
        spi_write_blocking(SPI_DISP_PORT, s_pixel_buf, batch * 3);
        remaining -= batch;
    }

    cs_deselect();
}

void ili9488_init(void)
{
    // --- GPIO setup ---
    gpio_set_function(PIN_DISP_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_DISP_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_DISP_MISO, GPIO_FUNC_SPI);

    gpio_init(PIN_DISP_CS);
    gpio_init(PIN_DISP_DC);
    gpio_init(PIN_DISP_RST);
    gpio_set_dir(PIN_DISP_CS,  GPIO_OUT);
    gpio_set_dir(PIN_DISP_DC,  GPIO_OUT);
    gpio_set_dir(PIN_DISP_RST, GPIO_OUT);

    gpio_put(PIN_DISP_CS,  1);  // deselect
    gpio_put(PIN_DISP_DC,  1);
    gpio_put(PIN_DISP_RST, 1);

    // --- SPI0 init at display speed ---
    spi_init(SPI_DISP_PORT, SPI_DISP_SPEED_HZ);
    spi_set_format(SPI_DISP_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // --- Hardware reset ---
    gpio_put(PIN_DISP_RST, 0);
    sleep_ms(10);
    gpio_put(PIN_DISP_RST, 1);
    sleep_ms(120);

    // --- Run init sequence ---
    for (int i = 0; s_init_cmds[i].len != 0xFF; i++) {
        const ili9488_init_cmd_t *c = &s_init_cmds[i];
        ili9488_send_cmd(c->cmd);
        if (c->len == 0xFE) {
            // Delay command
            sleep_ms(120);
        } else if (c->len > 0) {
            ili9488_send_data(c->data, c->len);
        }
    }

    // --- Prepare DMA channel for pixel transfers ---
    s_dma_channel = dma_claim_unused_channel(true);
    s_dma_cfg = dma_channel_get_default_config(s_dma_channel);
    channel_config_set_transfer_data_size(&s_dma_cfg, DMA_SIZE_8);
    channel_config_set_dreq(&s_dma_cfg, spi_get_dreq(SPI_DISP_PORT, true));
    channel_config_set_read_increment(&s_dma_cfg, true);
    channel_config_set_write_increment(&s_dma_cfg, false);
}

// ---------------------------------------------------------------------------
//  LVGL flush callback
//  LVGL calls this with a rendered rectangle (lv_color_t = RGB565).
//  We convert to RGB666 (3 bytes) and send via DMA.
// ---------------------------------------------------------------------------
void ili9488_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    critical_section_enter_blocking(&spi_mutex);
    
    uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    uint32_t pixel_count = w * h;

    ili9488_set_window((uint16_t)area->x1, (uint16_t)area->y1,
                       (uint16_t)area->x2, (uint16_t)area->y2);

    // Convert RGB565 -> RGB666 into the DMA buffer in chunks
    dc_data();
    cs_select();

    uint32_t remaining = pixel_count;
    const lv_color_t *src = color_map;
    uint32_t buf_pixels = PIXEL_BUF_SIZE / 3;

    while (remaining > 0) {
        uint32_t batch = (remaining < buf_pixels) ? remaining : buf_pixels;

        for (uint32_t i = 0; i < batch; i++) {
            uint16_t c16 = src[i].full;
            s_pixel_buf[i * 3]     = (uint8_t)((c16 >> 11) & 0x1F) << 3;  // R
            s_pixel_buf[i * 3 + 1] = (uint8_t)((c16 >>  5) & 0x3F) << 2;  // G
            s_pixel_buf[i * 3 + 2] = (uint8_t)((c16      ) & 0x1F) << 3;  // B
        }

        // DMA transfer
        dma_channel_configure(
            s_dma_channel, &s_dma_cfg,
            &spi_get_hw(SPI_DISP_PORT)->dr,  // write to SPI TX FIFO
            s_pixel_buf,
            batch * 3,
            true  // start immediately
        );
        dma_channel_wait_for_finish_blocking(s_dma_channel);

        src       += batch;
        remaining -= batch;
    }

    cs_deselect();

    critical_section_exit(&spi_mutex);

    // Notify LVGL that flushing is done
    lv_disp_flush_ready(drv);
}
