// =============================================================================
//  lv_conf.h  -  LVGL v8 configuration for RP2040 / ILI9488 320x480
//  Place this file in the project root so LVGL can find it automatically.
// =============================================================================

#if 1  // Set to "1" to enable content

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// --- Color depth -------------------------------------------------------------
// Use RGB565 internally; the ILI9488 flush callback converts to RGB666 on-the-fly
#define LV_COLOR_DEPTH          16
//#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565

// --- Memory ------------------------------------------------------------------
// Static internal heap used by LVGL (widgets, styles, animations, ...)
// RP2040 has 264 KB SRAM total; 48 KB for LVGL leaves enough for stack + buffers
#define LV_MEM_CUSTOM           0
#define LV_MEM_SIZE             (48u * 1024u)

// --- Display resolution ------------------------------------------------------
#define LV_HOR_RES_MAX          320
#define LV_VER_RES_MAX          480

// --- Render buffer -----------------------------------------------------------
// Double-buffered partial rendering (defined in core1_display.c)
#define LV_DISP_DEF_REFR_PERIOD  20   // ms  (~50 fps max)

// --- Input device ------------------------------------------------------------
#define LV_INDEV_DEF_READ_PERIOD 30   // ms  touch polling interval

// --- Logging -----------------------------------------------------------------
#define LV_USE_LOG              0     // Disable LVGL log output

// --- Asserts -----------------------------------------------------------------
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_ASSERT_OBJ       0
#define LV_USE_ASSERT_STYLE     0

// --- GPU ---------------------------------------------------------------------
#define LV_USE_GPU_RP2040_PIO_DMA  0  // No PIO-DMA GPU

// --- Performance monitor (disable for production) ----------------------------
#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      0

// --- Fonts -------------------------------------------------------------------
// Built-in fonts (Montserrat subset)
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_32   1
#define LV_FONT_MONTSERRAT_48   1

#define LV_FONT_DEFAULT         &lv_font_montserrat_16

// --- Widgets (enable only what we use) ---------------------------------------
#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_BTN              1
#define LV_USE_BTNMATRIX        1
#define LV_USE_CANVAS           0
#define LV_USE_CHECKBOX         0
#define LV_USE_CHART            0
#define LV_USE_COLORWHEEL       0
#define LV_USE_DROPDOWN         1
#define LV_USE_IMG              1
#define LV_USE_IMGBTN           0
#define LV_USE_KEYBOARD         0
#define LV_USE_LABEL            1
#define LV_USE_LED              1
#define LV_USE_LINE             1
#define LV_USE_LIST             0
#define LV_USE_MENU             0
#define LV_USE_METER            0
#define LV_USE_MSGBOX           0
#define LV_USE_ROLLER           0
#define LV_USE_SLIDER           1
#define LV_USE_SPINBOX          1
#define LV_USE_SPINNER          0
#define LV_USE_SWITCH           1
#define LV_USE_TABLE            0
#define LV_USE_TABVIEW          1
#define LV_USE_TEXTAREA         1
#define LV_USE_TILEVIEW         0
#define LV_USE_WIN              0

// --- Animations --------------------------------------------------------------
#define LV_USE_ANIMATION        1
#define LV_USE_FLEX             1
#define LV_USE_GRID             1

// --- Themes ------------------------------------------------------------------
#define LV_USE_THEME_DEFAULT    1
#define LV_USE_THEME_BASIC      0
#define LV_USE_THEME_MONO       0

// Default theme settings (dark mode, primary color = cyan)
#define LV_THEME_DEFAULT_DARK   1
#define LV_THEME_DEFAULT_GROW   1

// --- Misc --------------------------------------------------------------------
#define LV_SPRINTF_CUSTOM       0
#define LV_USE_USER_DATA        1
#define LV_DRAW_COMPLEX         1

#endif  // LV_CONF_H
#endif  // 1
