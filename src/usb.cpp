#include <cstdio>
#include "pico/stdlib.h"
#include "tusb.h"
#include "usb.h"

static const uint8_t desc_hid_report[] =
{
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
};

const uint8_t* tud_hid_descriptor_report_cb(uint8_t instance)
{
    printf("tud_hid_descriptor_report_cb(%i)\n", instance);
    return desc_hid_report;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    printf("tud_hid_get_report_cb\n");
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    printf("tud_hid_set_report_cb\n");
}

static const char* string_desc_arr[] =
{
    (const char[]) { 0x09, 0x04 },    // 0: is supported language is English (0x0409)
    "bruelltuete.com",                // 1: Manufacturer
    "Jiggle mouse",                   // 2: Product
    "1",                              // 3: Serials, should use chip ID
};

const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    static uint16_t _desc_str[32];
    int chr_count = 0;

    printf("tud_descriptor_string_cb(%i)\n", index);
    if (index == 0)
    {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    }
    else
    {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if (index >= count_of(string_desc_arr))
            return NULL;

        const char* str = string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31)
            chr_count = 31;

        // Convert ASCII string into UTF-16
        for (int i = 0; i < chr_count; i++)
            _desc_str[1 + i] = str[i];
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2 * chr_count + 2);

    return _desc_str;
}

enum
{
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID   0x81

static const uint8_t desc_configuration[] =
{
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 150),
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_MOUSE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 5)
};

const uint8_t* tud_descriptor_configuration_cb(uint8_t index)
{
    printf("tud_descriptor_configuration_cb\n");
    // This example use the same configuration for both high and full speed mode
    return desc_configuration;
}

// FIXME: copy pasta from example code
/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )        // e.g. CFG_TUD_VENDOR is defined in tusb_config.h
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4))

static const tusb_desc_device_t desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,              // which version of usb spec
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xCafe,      // cafe == red hat usb dev kit, picoprobe = 2e8a
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    // indices into string_desc_arr for string descriptors.
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

const uint8_t* tud_descriptor_device_cb()
{
    printf("tud_descriptor_device_cb\n");
    return (const uint8_t*) &desc_device;
}
