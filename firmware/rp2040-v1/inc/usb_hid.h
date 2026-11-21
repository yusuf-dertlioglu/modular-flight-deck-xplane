#pragma once

// =============================================================================
//  usb_hid.h  -  USB HID joystick report and CDC telemetry protocol
// =============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "tusb.h"
#include "config.h"

// ---------------------------------------------------------------------------
//  HID joystick report (must match the descriptor in usb_descriptors.c)
//  Total payload: 7 * 2 + 2 = 16 bytes  +  1 byte report-ID  = 17 bytes
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    int16_t  aileron;       // X   [-32767 .. +32767]
    int16_t  elevator;      // Y   [-32767 .. +32767]
    int16_t  rudder;        // Z   [-32767 .. +32767]
    int16_t  throttle;      // Rx  [0      .. +32767]
    int16_t  mixture;       // Ry  [0      .. +32767]
    int16_t  cam_x;         // Rz  [-32767 .. +32767]
    int16_t  cam_y;         // Slider [-32767 .. +32767]
    uint16_t buttons;       // 16 buttons, bit-packed
} hid_joystick_report_t;

// Button bit positions inside hid_joystick_report_t.buttons
#define BTN_PARKING_BRAKE  (1u <<  0)
#define BTN_GEAR_TOGGLE    (1u <<  1)
#define BTN_FLAPS_UP       (1u <<  2)
#define BTN_FLAPS_DOWN     (1u <<  3)
#define BTN_BRAKE_RIGHT    (1u <<  4)
#define BTN_ENGINE_START   (1u <<  5)
#define BTN_BRAKE_LEFT     (1u <<  6)
#define BTN_CAM_CENTER     (1u <<  7)
#define BTN_CAM_ZOOM_IN    (1u <<  8)
#define BTN_CAM_ZOOM_OUT   (1u <<  9)
#define BTN_BRAKE_HOLD     (1u << 10)
#define BTN_AP_HDG         (1u << 11)
#define BTN_AP_ALT         (1u << 12)
#define BTN_AP_VS          (1u << 13)
#define BTN_AP_NAV         (1u << 14)
#define BTN_AP_TOGGLE      (1u << 15)

// ---------------------------------------------------------------------------
//  CDC telemetry packet: PC -> Pico  (X-Plane data -> display)
//
//  The Python bridge on the PC reads X-Plane UDP datarefs and packs
//  them into this struct, then sends it over the CDC serial port.
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  header[2];         // 0xFE, 0xED
    float    airspeed_kts;
    float    altitude_ft;
    float    vspeed_fpm;
    float    heading_deg;
    float    throttle_pct;      // 0.0 - 100.0
    float    mixture_pct;       // 0.0 - 100.0
    float    rudder_pct;        // -100.0 .. +100.0
    uint8_t  gear_down;         // 1 = down
    uint8_t  flaps_pos;         // 0..5
    uint8_t  ap_engaged;
    uint16_t ap_heading;
    int16_t  ap_vspeed;
    uint32_t ap_altitude;
    int8_t   elev_trim;
    uint8_t  ap_modes;          // bit0=HDG bit1=ALT bit2=VS bit3=NAV
    uint8_t  checksum;          // XOR of all bytes after header
} telemetry_packet_t;

// ---------------------------------------------------------------------------
//  CDC command packet: Pico -> PC  (display AP settings -> X-Plane)
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  header[2];         // 0xAB, 0xCD
    uint16_t target_heading;    // degrees
    int16_t  target_vspeed;     // fpm
    uint32_t target_altitude;   // feet
    int8_t   target_elev_trim;  // Elevator Trim [-100...100%]
    uint8_t  modes;             // bit0=HDG bit1=ALT bit2=VS bit3=NAV bit4=AP_TOGGLE
    bool     LGearSt;           //Landing Gear State 0=Down 1=Up
    uint8_t  checksum;
} ap_command_packet_t;

// ---------------------------------------------------------------------------
//  Public interface
// ---------------------------------------------------------------------------

// Send a HID joystick report. Returns true if accepted by TinyUSB.
bool usb_hid_send_report(const hid_joystick_report_t *report);

// Try to receive one telemetry packet from the CDC RX buffer.
// Returns true if a complete, valid packet was received.
// The caller must call this frequently (every loop iteration).
bool usb_cdc_receive_telemetry(telemetry_packet_t *out);

// Send an AP command packet to the PC over CDC.
bool usb_cdc_send_ap_command(const ap_command_packet_t *pkt);

// Compute XOR checksum over a byte array (skips the 2-byte header)
uint8_t usb_checksum(const uint8_t *data, size_t len);
