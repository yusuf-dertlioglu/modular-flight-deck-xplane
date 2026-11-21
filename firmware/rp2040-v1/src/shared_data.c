// =============================================================================
//  shared_data.c  -  Global shared variable definitions
//  Initialised by Core0 before Core1 is launched.
// =============================================================================

#include "shared_data.h"

// Zeroed by default (BSS segment)
flight_data_t  g_flight_data;
input_state_t  g_input_state;
ap_settings_t  g_ap_settings;

mutex_t        g_flight_data_mutex;
mutex_t        g_ap_settings_mutex;
