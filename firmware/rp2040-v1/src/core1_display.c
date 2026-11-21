// =============================================================================
//  core1_display.c  -  Core1: display driver, LVGL, touch, encoder (IRQ)
//
//  Execution flow:
//    1. Initialise ILI9488, HR2046, LVGL
//    2. Build UI pages
//    3. Setup encoder GPIO interrupts
//    4. Main loop:
//         a. Wait for FIFO_CMD_SYNC from Core0
//         b. Copy shared data under mutex
//         c. Send FIFO_ACK_SYNC
//         d. Update LVGL widgets
//         e. Call lv_timer_handler()
// =============================================================================

#include "core1_display.h"
#include "ili9488.h"
#include "hr2046.h"
#include "display_pages.h"
#include "shared_data.h"
#include "config.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "lvgl.h"

#include <string.h>

// ---------------------------------------------------------------------------
//  LVGL render buffers (static, Core1 SRAM)
//  Two buffers for double-buffered partial rendering.
//  Each: DISP_WIDTH * LVGL_BUF_LINES * 2 bytes = 320*10*2 = 6400 bytes
// ---------------------------------------------------------------------------
static lv_color_t s_lvgl_buf1[DISP_WIDTH * LVGL_BUF_LINES];
static lv_color_t s_lvgl_buf2[DISP_WIDTH * LVGL_BUF_LINES];

flight_data_t  local_fd;

// ---------------------------------------------------------------------------
//  LVGL tick source  (repeating alarm, 1 ms)
// ---------------------------------------------------------------------------
static bool lvgl_tick_alarm_cb(repeating_timer_t *rt)
{
    (void)rt;
    lv_tick_inc(1);
    return true; // keep repeating
}

// ---------------------------------------------------------------------------
//  Encoder state (modified in IRQ, consumed in main loop)
// ---------------------------------------------------------------------------
static volatile int16_t s_enc_delta   = 0;   // accumulated steps
static volatile bool    s_enc_pressed = false;
static volatile uint32_t s_btn_press_time_ms = 0;
static volatile uint32_t last_enc_time = 0;

// GPIO IRQ handler for encoder and button (runs on Core1)
static void encoder_irq_handler(uint gpio, uint32_t events)
{
    uint32_t now = time_us_32();

    // Ignore everything until 2,5ms passed
    if (now - last_enc_time < 20000) {
        return;
    }
    last_enc_time = now;

    // 2. Button separat behandeln (falls er an PIN_ENC_BTN hängt)
    if (gpio == PIN_ENC_BTN) {
        s_enc_pressed    = true;
        s_btn_press_time_ms = to_ms_since_boot(get_absolute_time());
        return;
    }

    if (gpio == PIN_ENC_CW) {
        if (gpio_get(PIN_ENC_CCW) != 0) {
            s_enc_delta++;
        } else {
            s_enc_delta--;
        }
    } 
    else if (gpio == PIN_ENC_CCW) {
        if (gpio_get(PIN_ENC_CW) != 0) {
            s_enc_delta--;  // Gegen den Uhrzeigersinn
        } else {
            s_enc_delta++;  // Im Uhrzeigersinn
        }
    }
}

// ---------------------------------------------------------------------------
//  Process accumulated encoder events in the main loop
// ---------------------------------------------------------------------------
static void process_encoder(void)
{
    static uint32_t last_poll_time = 0;
    static uint32_t last_click_time = 0;
    uint32_t now = time_us_32();
    static uint8_t accel_factor = 1;
    
    // Alle 3 ms (3000 µs) abfragen - das blockiert nichts und fängt jeden Klick sicher ab
    if (now - last_poll_time < 1000) {
        return;
    }
    last_poll_time = now;

    static int last_state_cw = 1;
    int current_state_cw = gpio_get(PIN_ENC_CW);

    
    if (last_state_cw == 1 && current_state_cw == 0) {
        if (now - last_click_time < 75000) {
            accel_factor += 1;
            if (accel_factor > 5) accel_factor = 5;
        } else {
            accel_factor = 1;
        }
        last_click_time = now;

        if (gpio_get(PIN_ENC_CCW) != 0) {
            // Im Uhrzeigersinn gedreht
            display_pages_encoder_step(accel_factor);
        } else {
            // Gegen den Uhrzeigersinn gedreht
            display_pages_encoder_step(-accel_factor);
        }
    }
    last_state_cw = current_state_cw;

    static int last_state_btn = 1;
    int current_state_btn = gpio_get(PIN_ENC_BTN);

    if (last_state_btn == 1 && current_state_btn == 0) {
        display_pages_encoder_press();
    }
    last_state_btn = current_state_btn;
}

// ---------------------------------------------------------------------------
//  Sync with Core0 via FIFO and update display data
// ---------------------------------------------------------------------------
static void sync_with_core0(void)
{
    // Wait for FIFO_CMD_SYNC from Core0
    uint32_t cmd = multicore_fifo_pop_blocking();
    if (cmd != FIFO_CMD_SYNC) return;

    // --- Read flight data (Core0 writes, we read) ---
    //flight_data_t local_fd;
    mutex_enter_blocking(&g_flight_data_mutex);
    memcpy(&local_fd, &g_flight_data, sizeof(flight_data_t));
    mutex_exit(&g_flight_data_mutex);

    // --- Push AP settings to Core0 if changed ---
    ap_settings_t local_ap;
    if (display_pages_get_ap_settings(&local_ap)) {
        mutex_enter_blocking(&g_ap_settings_mutex);
        memcpy(&g_ap_settings, &local_ap, sizeof(ap_settings_t));
        mutex_exit(&g_ap_settings_mutex);
    }

    // Acknowledge sync to Core0
    multicore_fifo_push_blocking(FIFO_ACK_SYNC);

    static uint32_t last_update_ms = 0;
    uint32_t now = lv_tick_get(); 

    if (now - last_update_ms < 500) {
        return; // Noch keine 100 ms rum -> sofort raus, SPI-Bus bleibt frei für den Touch!
    }
    last_update_ms = now;

    // --- Update LVGL widgets with fresh data ---
    display_pages_update_flight(&local_fd);
    display_pages_update_ap(&local_fd);
}

// ---------------------------------------------------------------------------
//  Core1 entry point
// ---------------------------------------------------------------------------
void core1_entry(void)
{
    static critical_section_t spi_mutex;
    
    // 1. Initialise display hardware
    ili9488_init();

    // 2. Initialise touch controller
    //    SPI0 is already configured by ili9488_init()
    hr2046_init();

    // 3. Initialise LVGL
    lv_init();

    // Register display driver
    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf,
                           s_lvgl_buf1, s_lvgl_buf2,
                           DISP_WIDTH * LVGL_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = DISP_WIDTH;
    disp_drv.ver_res   = DISP_HEIGHT;
    disp_drv.flush_cb  = ili9488_flush;
    disp_drv.draw_buf  = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Register touch input driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = hr2046_read;
    lv_indev_drv_register(&indev_drv);

    // 4. Build UI
    display_pages_create();

    // 5. Setup LVGL tick timer (1 ms, uses Core1's alarm pool)
    static repeating_timer_t tick_timer;
    add_repeating_timer_ms(1, lvgl_tick_alarm_cb, NULL, &tick_timer);

    // 6. Setup encoder GPIO interrupts on Core1
    gpio_init(PIN_ENC_CW);
    gpio_init(PIN_ENC_CCW);
    gpio_init(PIN_ENC_BTN);
    gpio_set_dir(PIN_ENC_CW,  GPIO_IN);
    gpio_set_dir(PIN_ENC_CCW, GPIO_IN);
    gpio_set_dir(PIN_ENC_BTN, GPIO_IN);
    gpio_pull_up(PIN_ENC_CW);
    gpio_pull_up(PIN_ENC_CCW);
    gpio_pull_up(PIN_ENC_BTN);

    // Calling gpio_set_irq_enabled_with_callback from Core1 registers
    // the IRQ handler on Core1's NVIC
    /*gpio_set_irq_enabled_with_callback(PIN_ENC_CW,
        GPIO_IRQ_EDGE_FALL, true, encoder_irq_handler);
    gpio_set_irq_enabled_with_callback(PIN_ENC_CCW,
        GPIO_IRQ_EDGE_FALL, true, encoder_irq_handler);
    gpio_set_irq_enabled_with_callback(PIN_ENC_BTN,
        GPIO_IRQ_EDGE_FALL, true, encoder_irq_handler);*/

    // 7. Main loop
    while (true) {
        
        // Wait for sync trigger from Core0, then update data and display
        sync_with_core0();

        // Process encoder inputs accumulated since last loop
        process_encoder();

        // Let LVGL render pending changes
        lv_timer_handler();
    }
}