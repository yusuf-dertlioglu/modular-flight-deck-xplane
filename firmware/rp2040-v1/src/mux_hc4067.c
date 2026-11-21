// =============================================================================
//  mux_hc4067.c  -  CD74HC4067 analog multiplexer driver implementation
// =============================================================================

#include "mux_hc4067.h"
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"
#include "pico/time.h"

// Address pins in LSB-first order
static const uint8_t s_addr_pins[4] = {
    PIN_MUX_S0, PIN_MUX_S1, PIN_MUX_S2, PIN_MUX_S3
};

void mux_init(void)
{
    // Configure address pins as outputs
    for (int i = 0; i < 4; i++) {
        gpio_init(s_addr_pins[i]);
        gpio_set_dir(s_addr_pins[i], GPIO_OUT);
        gpio_put(s_addr_pins[i], 0);
    }

    // Configure the signal pin for ADC
    adc_init();
    adc_gpio_init(PIN_MUX_SIG);   // sets GPIO27 to ADC input mode
    adc_select_input(MUX_ADC_CHANNEL);
}

void mux_select_channel(uint8_t channel)
{
    // Write 4-bit binary address to S0..S3
    for (int i = 0; i < 4; i++) {
        gpio_put(s_addr_pins[i], (channel >> i) & 0x01);
    }
    // Wait for the MUX and the ADC sample-hold cap to settle
    sleep_us(MUX_SETTLE_US);
}

uint16_t mux_read_raw(void)
{
    adc_select_input(MUX_ADC_CHANNEL);
    return adc_read();  // 12-bit result [0..4095]
}

bool mux_read_digital(uint8_t channel)
{
    mux_select_channel(channel);
    uint16_t raw = mux_read_raw();
    // Pull-up present: LOW voltage (closed switch) -> raw < threshold
    return (raw < MUX_DIGITAL_THRESHOLD);
}

int16_t mux_read_analog_signed(uint8_t channel, uint16_t mid_raw)
{
    mux_select_channel(channel);
    uint16_t raw = mux_read_raw();

    int32_t shifted = (int32_t)raw - (int32_t)mid_raw;
    int32_t scaled;

    if (shifted >= 0) {
        int32_t range = 4095 - (int32_t)mid_raw;
        if (range <= 0) return 0;
        scaled = (shifted * 32767) / range;
    } else {
        if ((int32_t)mid_raw == 0) return 0;
        scaled = (shifted * 32767) / (int32_t)mid_raw;
    }

    if (scaled >  32767) scaled =  32767;
    if (scaled < -32767) scaled = -32767;

    return (int16_t)scaled;
}
