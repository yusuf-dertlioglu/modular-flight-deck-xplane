// =============================================================================
//  usb_descriptors.c  -  TinyUSB descriptor definitions
//
//  Composite USB device:
//    Interface 0: HID Joystick  (Pico -> PC)  flight controls
//    Interface 1: CDC ACM       (bidirectional) telemetry / AP commands
//
//  The PC sees:
//    - A standard HID joystick (works in X-Plane without any plugin)
//    - A virtual COM port for the Python telemetry bridge
// =============================================================================

#include "tusb.h"
#include "usb_hid.h"

// ---------------------------------------------------------------------------
//  HID report descriptor
//  Joystick with 7 signed 16-bit axes and 16 buttons.
//
//  Axis mapping (X-Plane default joystick assignment):
//    Axis 0 X   -> Aileron       (ADC2 ch0)
//    Axis 1 Y   -> Elevator      (ADC2 ch1)
//    Axis 2 Z   -> Rudder        (ADC1 ch0)
//    Axis 3 Rx  -> Throttle      (ADC1 ch1)
//    Axis 4 Ry  -> Mixture       (ADC1 ch2)
//    Axis 5 Rz  -> Camera X      (MUX ch11)
//    Axis 6 Vx  -> Camera Y      (MUX ch10)
//
//  Button mapping (X-Plane button assignments):
//    Bit 0  -> Parking brake
//    Bit 1  -> Landing gear toggle
//    Bit 2  -> Flaps up
//    Bit 3  -> Flaps down
//    Bit 4  -> Right brake
//    Bit 5  -> Engine start
//    Bit 6  -> Left brake
//    Bit 7  -> Camera centre
//    Bit 8  -> Camera zoom in
//    Bit 9  -> Camera zoom out
//    Bit 10 -> Brake hold
//    Bit 11 -> AP heading mode  (from display)
//    Bit 12 -> AP altitude mode (from display)
//    Bit 13 -> AP VS mode       (from display)
//    Bit 14 -> AP NAV mode      (from display)
//    Bit 15 -> AP master toggle (from display)
// ---------------------------------------------------------------------------
static const uint8_t hid_report_descriptor[] = {
    HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
    HID_USAGE(HID_USAGE_DESKTOP_JOYSTICK),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),

        HID_REPORT_ID(HID_REPORT_ID_JOYSTICK)

        // --- 7 signed 16-bit axes ----------------------------------------
        HID_USAGE_PAGE(HID_USAGE_PAGE_DESKTOP),
        HID_USAGE(HID_USAGE_DESKTOP_X),        // Aileron
        HID_USAGE(HID_USAGE_DESKTOP_Y),        // Elevator
        HID_USAGE(HID_USAGE_DESKTOP_Z),        // Rudder
        HID_USAGE(HID_USAGE_DESKTOP_RX),       // Throttle
        HID_USAGE(HID_USAGE_DESKTOP_RY),       // Mixture
        HID_USAGE(HID_USAGE_DESKTOP_RZ),       // Camera X
        HID_USAGE(HID_USAGE_DESKTOP_SLIDER),   // Camera Y
        HID_LOGICAL_MIN_N(-32767, 2),
        HID_LOGICAL_MAX_N( 32767, 2),
        HID_REPORT_COUNT(7),
        HID_REPORT_SIZE(16),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),

        // --- 16 buttons --------------------------------------------------
        HID_USAGE_PAGE(HID_USAGE_PAGE_BUTTON),
        HID_USAGE_MIN(1),
        HID_USAGE_MAX(16),
        HID_LOGICAL_MIN(0),
        HID_LOGICAL_MAX(1),
        HID_REPORT_COUNT(16),
        HID_REPORT_SIZE(1),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),

    HID_COLLECTION_END
};

// ---------------------------------------------------------------------------
//  Return the HID report descriptor (called by TinyUSB)
// ---------------------------------------------------------------------------
uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void)itf;
    return hid_report_descriptor;
}

// ---------------------------------------------------------------------------
//  Device descriptor
// ---------------------------------------------------------------------------
static const tusb_desc_device_t device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCafe,   // placeholder - change to your VID
    .idProduct          = 0x4001,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&device_descriptor;
}

// ---------------------------------------------------------------------------
//  Interface numbers and endpoint addresses
// ---------------------------------------------------------------------------
enum {
    ITF_NUM_HID = 0,
    ITF_NUM_CDC_COMM,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

#define EPNUM_HID       0x81    // IN  endpoint for HID reports
#define EPNUM_CDC_NOTIF 0x82    // IN  endpoint for CDC notifications
#define EPNUM_CDC_OUT   0x02    // OUT endpoint for CDC data
#define EPNUM_CDC_IN    0x83    // IN  endpoint for CDC data

// ---------------------------------------------------------------------------
//  Configuration descriptor  (HID + CDC)
// ---------------------------------------------------------------------------
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN          \
                         + TUD_HID_DESC_LEN             \
                         + TUD_CDC_DESC_LEN)

static const uint8_t config_descriptor[] = {
    // Configuration header
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // HID Interface  (report size: 7*2 + 2 = 16 bytes + 1 report-ID byte)
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(hid_report_descriptor), EPNUM_HID,
                       CFG_TUD_HID_EP_BUFSIZE, 5),

    // CDC ACM Interface
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_COMM, 4, EPNUM_CDC_NOTIF, 8,
                       EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return config_descriptor;
}

// ---------------------------------------------------------------------------
//  String descriptors
// ---------------------------------------------------------------------------
static const char *string_descriptors[] = {
    (const char[]){0x09, 0x04},  // 0: supported language = English (0x0409)
    "RP2040 FlightPanel",         // 1: Manufacturer
    "Flight Panel v1.0",          // 2: Product
    "FP0001",                     // 3: Serial number
    "Flight Panel HID",           // 4: HID interface
    "Flight Panel CDC",           // 5: CDC interface
};

static uint16_t s_str_buf[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    uint8_t  chr_count;
    const char *str;

    if (index == 0) {
        memcpy(&s_str_buf[1], string_descriptors[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_descriptors) / sizeof(string_descriptors[0]))
            return NULL;
        str = string_descriptors[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) {
            s_str_buf[1 + i] = (uint16_t)str[i];
        }
    }

    s_str_buf[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return s_str_buf;
}
