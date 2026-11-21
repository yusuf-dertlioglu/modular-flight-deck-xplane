// =============================================================================
//  usb_hid.c  -  USB HID send and CDC receive/send implementation
// =============================================================================

#include "usb_hid.h"
#include "config.h"
#include "tusb.h"
#include <string.h>

// ---------------------------------------------------------------------------
//  Checksum: XOR of every byte starting after the 2-byte header
// ---------------------------------------------------------------------------
uint8_t usb_checksum(const uint8_t *data, size_t len)
{
    uint8_t csum = 0;
    for (size_t i = 2; i < len; i++) {
        csum ^= data[i];
    }
    return csum;
}

// ---------------------------------------------------------------------------
//  HID report transmission
// ---------------------------------------------------------------------------
bool usb_hid_send_report(const hid_joystick_report_t *report)
{
    if (!tud_hid_ready()) return false;
    return tud_hid_report(HID_REPORT_ID_JOYSTICK, report, sizeof(*report));
}

// ---------------------------------------------------------------------------
//  TinyUSB HID callbacks (required by TinyUSB even if unused)
// ---------------------------------------------------------------------------
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize)
{
    (void)itf; (void)report_id; (void)report_type;
    (void)buffer; (void)bufsize;
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen)
{
    (void)itf; (void)report_id; (void)report_type;
    (void)buffer; (void)reqlen;
    return 0;
}

// ---------------------------------------------------------------------------
//  CDC receive: accumulate bytes until a complete telemetry packet is found
// ---------------------------------------------------------------------------

// Header magic bytes
#define TEL_HDR0  0xFE
#define TEL_HDR1  0xED

static uint8_t  s_rx_buf[CDC_RX_BUF_SIZE];
static uint32_t s_rx_head = 0;   // write position
static uint32_t s_rx_tail = 0;   // read  position

static inline uint32_t rx_used(void) {
    return (s_rx_head - s_rx_tail + CDC_RX_BUF_SIZE) % CDC_RX_BUF_SIZE;
}

bool usb_cdc_receive_telemetry(telemetry_packet_t *out)
{
    // 1. Pull all available CDC bytes into our circular buffer
    if (tud_cdc_available()) {
        uint32_t space = CDC_RX_BUF_SIZE - 1 - rx_used();
        if (space > 0) {
            uint8_t tmp[64];
            uint32_t got = tud_cdc_read(tmp, (space < sizeof(tmp)) ? space : sizeof(tmp));
            for (uint32_t i = 0; i < got; i++) {
                s_rx_buf[s_rx_head] = tmp[i];
                s_rx_head = (s_rx_head + 1) % CDC_RX_BUF_SIZE;
            }
        }
    }

    const uint32_t PKT_LEN = sizeof(telemetry_packet_t);

    // 2. Scan for header in the circular buffer
    while (rx_used() >= PKT_LEN) {
        // Peek at the first two bytes
        uint8_t b0 = s_rx_buf[s_rx_tail];
        uint8_t b1 = s_rx_buf[(s_rx_tail + 1) % CDC_RX_BUF_SIZE];

        if (b0 != TEL_HDR0 || b1 != TEL_HDR1) {
            // Not a header, discard one byte and keep searching
            s_rx_tail = (s_rx_tail + 1) % CDC_RX_BUF_SIZE;
            continue;
        }

        // Found potential header – copy packet to linear buffer for checksum check
        uint8_t linear[sizeof(telemetry_packet_t)];
        for (uint32_t i = 0; i < PKT_LEN; i++) {
            linear[i] = s_rx_buf[(s_rx_tail + i) % CDC_RX_BUF_SIZE];
        }

        telemetry_packet_t *p = (telemetry_packet_t *)linear;
        uint8_t expected_csum = usb_checksum(linear, PKT_LEN - 1);

        if (p->checksum != expected_csum) {
            // Bad checksum - skip header byte and retry
            s_rx_tail = (s_rx_tail + 1) % CDC_RX_BUF_SIZE;
            continue;
        }

        // Valid packet found
        memcpy(out, linear, PKT_LEN);
        s_rx_tail = (s_rx_tail + PKT_LEN) % CDC_RX_BUF_SIZE;
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
//  CDC send: AP command packet to PC
// ---------------------------------------------------------------------------
bool usb_cdc_send_ap_command(const ap_command_packet_t *pkt)
{
    if (!tud_cdc_connected()) return false;
    uint32_t written = tud_cdc_write(pkt, sizeof(*pkt));
    tud_cdc_write_flush();
    return (written == sizeof(*pkt));
}
