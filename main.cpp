#include <cstdio>

#include "pico/stdlib.h"

#include "pico/cyw43_arch.h"

#include "btstack.h"

// btstack and tinyusb define conflicting hid enums...
// TODO: avoid this
#define hid_report_type_t usb_hid_report_type_t
#define HID_REPORT_TYPE_INVALID USB_HID_REPORT_TYPE_INVALID
#define HID_REPORT_TYPE_INPUT USB_HID_REPORT_TYPE_INPUT
#define HID_REPORT_TYPE_OUTPUT USB_HID_REPORT_TYPE_OUTPUT
#define HID_REPORT_TYPE_FEATURE USB_HID_REPORT_TYPE_FEATURE

#include "tusb.h"

#undef hid_report_type_t
#undef HID_REPORT_TYPE_INVALID
#undef HID_REPORT_TYPE_INPUT
#undef HID_REPORT_TYPE_OUTPUT
#undef HID_REPORT_TYPE_FEATURE

#include "usb_descriptors.h"

// bt
static btstack_packet_callback_registration_t hci_event_callback_registration;

static void bt_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    if(packet_type == HCI_EVENT_PACKET)
    {
        switch(hci_event_packet_get_type(packet))
        {
            case BTSTACK_EVENT_STATE:
                if(btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                    bd_addr_t local_addr;
                    gap_local_bd_addr(local_addr);
                    printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
                }
                break;
            default:
                break;
        }
    }
}

// usb
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint8_t len)
{
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, usb_hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, usb_hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
}

int main()
{
    stdio_init_all();

    // bt init
    if(cyw43_arch_init())
    {
        printf("failed to initialise cyw43_arch\n");
        return -1;
    }

    // enabled EIR
    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);

    hci_event_callback_registration.callback = &bt_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);
        
    // usb init
    tusb_init();

    while(true)
    {
        tud_task();

        //tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));

        sleep_ms(1);
    }

    return 0;
}
