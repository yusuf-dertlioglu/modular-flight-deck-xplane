#pragma once

// =============================================================================
//  display_pages.h  -  LVGL UI pages for the flight panel display
//
//  Page 0 (tab "FLIGHT"):   Real-time flight data from X-Plane
//  Page 1 (tab "AUTOPILOT"): Autopilot settings, adjustable with encoder
//
//  The encoder selects which AP parameter to edit; rotating changes the value.
//  Touch input is handled natively by LVGL (buttons, tabs).
// =============================================================================

#include <lvgl.h>
#include "shared_data.h"

// -----------------------------------------------------------------------------
//  Call once from Core1 after lv_init() to build all screens.
// -----------------------------------------------------------------------------
void display_pages_create(void);

static void show_page(uint8_t page);

static void nav_btn_cb(lv_event_t *e);
static void bigbtn_click_event_cb(lv_event_t *e);

static void ap_row_click_event_cb(lv_event_t *e);

// -----------------------------------------------------------------------------
//  Update the flight data page with fresh data from Core0.
//  Call from Core1's main loop after acquiring g_flight_data_mutex.
// -----------------------------------------------------------------------------
void display_pages_update_flight(const flight_data_t *fd);

// -----------------------------------------------------------------------------
//  Update the autopilot page to reflect the current AP state from X-Plane.
//  Call from Core1's main loop after acquiring g_flight_data_mutex.
// -----------------------------------------------------------------------------
void display_pages_update_ap(const flight_data_t *fd);

// -----------------------------------------------------------------------------
//  Encoder interface (called from Core1's encoder interrupt / main loop)
// -----------------------------------------------------------------------------

// Rotate the encoder one step clockwise (+1) or counter-clockwise (-1)
void display_pages_encoder_step(int8_t direction);

// Short press of the encoder button: cycle AP parameter selection
void display_pages_encoder_press(void);

// Long press of the encoder button (>500 ms): toggle AP master
void display_pages_encoder_long_press(void);

// -----------------------------------------------------------------------------
//  Read back current AP settings so Core0 can forward them to the PC.
//  Returns true if settings have changed since the last call.
// -----------------------------------------------------------------------------
bool display_pages_get_ap_settings(ap_settings_t *out);

extern flight_data_t local_fd;
