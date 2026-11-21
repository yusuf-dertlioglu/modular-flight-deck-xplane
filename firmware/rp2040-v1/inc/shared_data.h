#pragma once

// =============================================================================
//  shared_data.h  -  Data structures shared between Core 0 and Core 1
//
//  Access rules:
//    g_flight_data   -> written by Core0 (from USB CDC), read by Core1 (display)
//    g_input_state   -> written by Core0 (polling),     read by Core0 (USB HID)
//    g_ap_settings   -> written by Core1 (display/UI),  read by Core0 (USB CDC)
//
//  Each structure is protected by its own mutex from pico_sync.
//  The FIFO is used only for triggering the sync handshake.
// =============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "pico/sync.h"

// -----------------------------------------------------------------------------
//  Flight data: X-Plane -> PC script -> USB CDC -> Core0 -> Core1 (display)
// -----------------------------------------------------------------------------
typedef struct {
    float    airspeed_kts;      // Indicated airspeed  [knots]
    float    altitude_ft;       // Altitude            [feet]
    float    vspeed_fpm;        // Vertical speed      [feet/min]
    float    heading_deg;       // Magnetic heading    [degrees 0-359]
    float    pitch_deg;         // Pitch angle         [degrees, + = nose up]
    float    roll_deg;          // Roll angle          [degrees, + = right]
    float    throttle_pct;      // Throttle position   [0.0 - 100.0 %]
    float    mixture_pct;       // Mixture             [0.0 - 100.0 %]
    float    rudder_pct;        // Rudder position     [-100.0 .. +100.0 %]
    uint8_t  gear_down;         // Landing gear state  [1 = down, 0 = up]
    uint8_t  flaps_pos;         // Flaps Pos %         [0..100]
    uint8_t  ap_engaged;        // Autopilot master    [1 = on]
    uint16_t ap_heading;        // AP heading bug      [degrees]
    int16_t  ap_vspeed;         // AP vertical speed   [fpm]
    uint32_t ap_altitude;       // AP target altitude  [feet]
    int8_t   elev_trim;         // Elevator Trim [-100...100%]
    uint8_t  ap_hdg_mode;       // HDG mode active
    uint8_t  ap_alt_mode;       // ALT mode active
    uint8_t  ap_vs_mode;        // VS  mode active
    uint8_t  ap_nav_mode;       // NAV mode active
    bool     valid;             // True once first packet received from PC
} flight_data_t;

// -----------------------------------------------------------------------------
//  Input state: physical controls -> Core0 polling -> USB HID report
// -----------------------------------------------------------------------------
typedef struct {
    // Analog axes, normalised to [-32767 .. +32767]
    int16_t  aileron;           // Flightstick X  (ADC2 ch0)
    int16_t  elevator;          // Flightstick Y  (ADC2 ch1)
    int16_t  rudder;            // Rudder slider  (ADC1 ch0)
    // Throttle and mixture use [0 .. 32767] (unipolar)
    int16_t  throttle;          // Throttle slider (ADC1 ch1)
    int16_t  mixture;           // Mixture slider  (ADC1 ch2)
    // Camera joystick via MUX
    int16_t  cam_x;             // Camera X (MUX ch11)
    int16_t  cam_y;             // Camera Y (MUX ch10)

    // Digital inputs (true = active/pressed)
    bool     parking_brake;     // MUX ch0
    bool     gear_down_cmd;     // MUX ch1
    bool     flaps_up;          // MUX ch2
    bool     flaps_down;        // MUX ch3
    bool     brake_right;       // MUX ch4
    bool     engine_start;      // MUX ch5
    bool     brake_left;        // MUX ch6
    bool     cam_center;        // MUX ch7
    bool     cam_zoom_in;       // MUX ch8
    bool     cam_zoom_out;      // MUX ch9
    bool     brake_hold;        // ADC1 ch3  (held brake button)
} input_state_t;

// -----------------------------------------------------------------------------
//  Autopilot settings: Core1 (display/encoder) -> Core0 -> USB CDC -> X-Plane
// -----------------------------------------------------------------------------
typedef struct {
    uint16_t target_heading;    // Heading bug    [0 - 359 degrees]
    int16_t  target_vspeed;     // Vertical speed [-6000 .. +6000 fpm]
    uint32_t target_altitude;   // Target altitude [feet]
    int8_t   target_elev_trim;  // Elevator Trim [-100...100%]
    uint8_t  btnpressed;        // Pressed Button
    bool     hdg_mode;          // HDG hold active
    bool     alt_mode;          // ALT hold active
    bool     vs_mode;           // VS  mode active
    bool     nav_mode;          // NAV mode active
    bool     ap_toggle;         // Request to toggle AP master on/off
    bool     changed;           // Dirty flag: new data ready for Core0
} ap_settings_t;

// -----------------------------------------------------------------------------
//  Global shared instances  (defined in shared_data.c)
// -----------------------------------------------------------------------------
extern flight_data_t  g_flight_data;
extern input_state_t  g_input_state;
extern ap_settings_t  g_ap_settings;

extern mutex_t        g_flight_data_mutex;
extern mutex_t        g_ap_settings_mutex;
