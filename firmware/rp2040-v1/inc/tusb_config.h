#pragma once

// =============================================================================
//  tusb_config.h  -  TinyUSB configuration for composite HID + CDC device
// =============================================================================

// --- Controller --------------------------------------------------------------
#define CFG_TUSB_MCU                OPT_MCU_RP2040
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS                 OPT_OS_NONE
#endif

// --- Device classes ----------------------------------------------------------
#define CFG_TUD_HID                 1    // HID joystick
#define CFG_TUD_CDC                 1    // Virtual COM port (telemetry)
#define CFG_TUD_MSC                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

// --- Buffer sizes ------------------------------------------------------------
#define CFG_TUD_CDC_RX_BUFSIZE      256
#define CFG_TUD_CDC_TX_BUFSIZE      64
#define CFG_TUD_HID_EP_BUFSIZE      64

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          __attribute__((aligned(4)))