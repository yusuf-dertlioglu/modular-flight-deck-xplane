// =============================================================================
//  main.c  -  Core0 entry point
//
//  Core0 responsibilities:
//    1. Hardware init (SPI1, MUX ADC, GPIO)
//    2. Mutex + shared data init
//    3. USB init (TinyUSB)
//    4. Launch Core1
//    5. Main loop (200 Hz):
//         a. Poll all physical inputs
//         b. Trigger Core1 data sync via FIFO
//         c. Send USB HID joystick report
//         d. Receive USB CDC telemetry from PC
//         e. Forward received flight data to shared memory
// =============================================================================

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "tusb.h"

#include "config.h"
#include "shared_data.h"
#include "inc/mcp3208.h"
#include "inc/mux_hc4067.h"
#include "inc/usb_hid.h"
#include "inc/core1_display.h"
#include "inc/hr2046.h"

#include <string.h>

// ---------------------------------------------------------------------------
//  ADC device instances
// ---------------------------------------------------------------------------
static mcp3208_t s_adc1;   // Left Panel  (PIN_ADC1_CS)
static mcp3208_t s_adc2;   // Flightstick (PIN_ADC2_CS)
static bool LGearSt;

// ---------------------------------------------------------------------------
//  Hardware initialisation for Core0 peripherals
// ---------------------------------------------------------------------------
static void hw_init(void)
{
    
    //stdio_init_all();   // needed internally by SDK even with USB stdio disabled

    // --- SPI1 for MCP3208 ADCs ---
    spi_init(SPI_ADC_PORT, SPI_ADC_SPEED_HZ);
    spi_set_format(SPI_ADC_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_ADC_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_ADC_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_ADC_MOSI, GPIO_FUNC_SPI);

    // --- MCP3208 instances ---
    mcp3208_init(&s_adc1, SPI_ADC_PORT, PIN_ADC1_CS);
    mcp3208_init(&s_adc2, SPI_ADC_PORT, PIN_ADC2_CS);

    // --- CD74HC4067 multiplexer + RP2040 ADC ---
    mux_init();
}

// ---------------------------------------------------------------------------
//  Poll all physical inputs and write results to g_input_state.
//  No mutex needed here because g_input_state is only written by Core0
//  and only read by Core0 (for the HID report).
//  The flight data / AP settings use mutexes because they cross cores.
// ---------------------------------------------------------------------------
static void poll_inputs(void)
{
    input_state_t *in = &g_input_state;

    // ── MUX: digital channels 0-9 (pull-up, active LOW) ──────────────────
    // Select each channel, wait for settle, then read.
    // All channels are read back-to-back; the settle time inside
    // mux_read_digital() provides enough spacing between channels.

    in->parking_brake  = mux_read_digital(0);
    in->gear_down_cmd  = mux_read_digital(1);
    in->flaps_up       = mux_read_digital(2);
    in->flaps_down     = mux_read_digital(3);
    in->brake_right    = mux_read_digital(4);
    in->engine_start   = mux_read_digital(5);
    in->brake_left     = mux_read_digital(6);
    in->cam_center     = mux_read_digital(7);
    in->cam_zoom_in    = mux_read_digital(8);
    in->cam_zoom_out   = mux_read_digital(9);

    // ── MUX: analog channels 10-11 (camera joystick, no pull-up) ─────────
    // Mid-point for an unbiased joystick is ~2048 (half of 4095)
    in->cam_y = mux_read_analog_signed(10, 2048);   // -Y axis
    in->cam_x = mux_read_analog_signed(11, 2048);   //  X axis

    // ── ADC1 (Left Panel) ─────────────────────────────────────────────────
    // ch0: rudder slider  (positive = right; signed, centre at ~2048)
    in->rudder   = mcp3208_read_signed(&s_adc1, 0, 2048);

    // ch1: throttle (unipolar 0-32767)
    in->throttle = mcp3208_read_unsigned(&s_adc1, 1);

    // ch2: mixture  (unipolar 0-32767)
    in->mixture  = mcp3208_read_unsigned(&s_adc1, 2);

    // ch3: brake hold button (pull-up, active LOW -> raw near 0)
    uint16_t brake_raw = mcp3208_read(&s_adc1, 3);
    in->brake_hold = (brake_raw < MUX_DIGITAL_THRESHOLD);

    // ── ADC2 (Flightstick) ────────────────────────────────────────────────
    // ch0: aileron  (signed, centred at 2048)
    in->aileron  = mcp3208_read_signed(&s_adc2, 0, 2048);

    // ch1: elevator (signed, centred at 2048)
    //      Invert so pulling stick back gives positive (nose-up) value
    in->elevator = (int16_t)(-mcp3208_read_signed(&s_adc2, 1, 2048));
}

// ---------------------------------------------------------------------------
//  Build a HID report from the current input state + AP settings
// ---------------------------------------------------------------------------
static void build_hid_report(hid_joystick_report_t *report)
{
    const input_state_t *in = &g_input_state;

    report->aileron  = in->aileron;
    report->elevator = in->elevator;
    report->rudder   = in->rudder;
    report->throttle = in->throttle;
    report->mixture  = in->mixture;
    report->cam_x    = in->cam_x;
    report->cam_y    = in->cam_y;

    // Pack digital inputs into button bitmask
    uint16_t btns = 0;
    if (in->parking_brake) btns |= BTN_PARKING_BRAKE;
    if (in->gear_down_cmd) btns |= BTN_GEAR_TOGGLE;
    if (in->flaps_up)      btns |= BTN_FLAPS_UP;
    if (in->flaps_down)    btns |= BTN_FLAPS_DOWN;
    if (in->brake_right)   btns |= BTN_BRAKE_RIGHT;
    if (in->engine_start)  btns |= BTN_ENGINE_START;
    if (in->brake_left)    btns |= BTN_BRAKE_LEFT;
    if (in->cam_center)    btns |= BTN_CAM_CENTER;
    if (in->cam_zoom_in)   btns |= BTN_CAM_ZOOM_IN;
    if (in->cam_zoom_out)  btns |= BTN_CAM_ZOOM_OUT;
    if (in->brake_hold)    btns |= BTN_BRAKE_HOLD;

    // AP commands from display (Core1 via shared memory)
    ap_settings_t ap;
    mutex_enter_blocking(&g_ap_settings_mutex);
    ap = g_ap_settings;
    mutex_exit(&g_ap_settings_mutex);

    /*if (ap.hdg_mode)  btns |= BTN_AP_HDG;
    if (ap.alt_mode)  btns |= BTN_AP_ALT;
    if (ap.vs_mode)   btns |= BTN_AP_VS;
    if (ap.nav_mode)  btns |= BTN_AP_NAV;
    if (ap.ap_toggle) btns |= BTN_AP_TOGGLE;*/

    if(ap.btnpressed==1) btns |= BTN_AP_HDG;
    if(ap.btnpressed==2) btns |= BTN_AP_ALT;
    if(ap.btnpressed==3) btns |= BTN_AP_VS;
    if(ap.btnpressed==4) btns |= BTN_AP_NAV;
    if(ap.btnpressed==5) btns |= BTN_AP_TOGGLE;

    report->buttons = btns;
}

// ---------------------------------------------------------------------------
//  Forward AP settings to PC via CDC if they changed
// ---------------------------------------------------------------------------
static void forward_ap_to_pc(void)
{
    ap_settings_t ap;
    mutex_enter_blocking(&g_ap_settings_mutex);
    ap = g_ap_settings;
    bool changed = ap.changed;
    if (changed) g_ap_settings.changed = false;
    mutex_exit(&g_ap_settings_mutex);

    if(LGearSt!=g_input_state.gear_down_cmd){
        LGearSt=g_input_state.gear_down_cmd;
        changed= true;
    }

    if (!changed) return;

    ap_command_packet_t pkt;
    pkt.header[0]       = 0xAB;
    pkt.header[1]       = 0xCD;
    pkt.target_heading  = ap.target_heading;
    pkt.target_vspeed   = ap.target_vspeed;
    pkt.target_altitude = ap.target_altitude;
    pkt.target_elev_trim= ap.target_elev_trim;
    pkt.modes  = (ap.hdg_mode  ? (1u << 0) : 0u)
               | (ap.alt_mode  ? (1u << 1) : 0u)
               | (ap.vs_mode   ? (1u << 2) : 0u)
               | (ap.nav_mode  ? (1u << 3) : 0u)
               | (ap.ap_toggle ? (1u << 4) : 0u);
    pkt.LGearSt= LGearSt;
    pkt.checksum = usb_checksum((const uint8_t *)&pkt, sizeof(pkt) - 1);

    usb_cdc_send_ap_command(&pkt);
}

// ---------------------------------------------------------------------------
//  Receive telemetry from PC and update shared flight data
// ---------------------------------------------------------------------------
static void receive_telemetry(void)
{
    telemetry_packet_t pkt;
    if (!usb_cdc_receive_telemetry(&pkt)) return;

    mutex_enter_blocking(&g_flight_data_mutex);

    g_flight_data.airspeed_kts  = pkt.airspeed_kts;
    g_flight_data.altitude_ft   = pkt.altitude_ft;
    g_flight_data.vspeed_fpm    = pkt.vspeed_fpm;
    g_flight_data.heading_deg   = pkt.heading_deg;
    g_flight_data.throttle_pct  = pkt.throttle_pct;
    g_flight_data.mixture_pct   = pkt.mixture_pct;
    g_flight_data.rudder_pct    = pkt.rudder_pct;
    g_flight_data.gear_down     = pkt.gear_down;
    g_flight_data.flaps_pos     = pkt.flaps_pos;
    g_flight_data.ap_engaged    = pkt.ap_engaged;
    g_flight_data.ap_heading    = pkt.ap_heading;
    g_flight_data.ap_vspeed     = pkt.ap_vspeed;
    g_flight_data.ap_altitude   = pkt.ap_altitude;
    g_flight_data.elev_trim     = pkt.elev_trim;
    g_flight_data.ap_hdg_mode   = (pkt.ap_modes >> 0) & 1;
    g_flight_data.ap_alt_mode   = (pkt.ap_modes >> 1) & 1;
    g_flight_data.ap_vs_mode    = (pkt.ap_modes >> 2) & 1;
    g_flight_data.ap_nav_mode   = (pkt.ap_modes >> 3) & 1;
    g_flight_data.valid         = true;

    mutex_exit(&g_flight_data_mutex);
}

// ---------------------------------------------------------------------------
//  Main
// ---------------------------------------------------------------------------
int main(void)
{
    // 1. Initialise hardware
    hw_init();

    // 2. Initialise shared data mutexes
    mutex_init(&g_flight_data_mutex);
    mutex_init(&g_ap_settings_mutex);

    // 3. Initialise TinyUSB
    tusb_init();

    critical_section_init(&spi_mutex);

    // 4. Launch Core1 (display + touch + encoder)
    multicore_launch_core1(core1_entry);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    bool ledon= true;
    uint32_t ledtimer= 0;

    // 5. Main loop
    uint32_t last_us = time_us_32();

    while (true) {

        // Keep TinyUSB running
        tud_task();

        uint32_t now_us = time_us_32();
        last_us = now_us;

        // a. Poll all physical inputs
        poll_inputs();

        // b. Signal Core1 to sync and wait for acknowledgement
        multicore_fifo_push_blocking(FIFO_CMD_SYNC);
        uint32_t ack = multicore_fifo_pop_blocking();
        (void)ack;  // FIFO_ACK_SYNC expected

        // c. Send HID joystick report to X-Plane
        hid_joystick_report_t report;
        build_hid_report(&report);
        usb_hid_send_report(&report);

        // d. Forward any AP setting changes to PC via CDC
        forward_ap_to_pc();

        // e. Receive telemetry from PC and update shared flight data
        receive_telemetry();
        
        if(time_us_32()-ledtimer > 1000000){
            ledon= !ledon;
            gpio_put(LED_PIN, ledon);
            ledtimer= time_us_32();
        }
    }
}