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
#define INQUIRY_INTERVAL 5
#define MAX_ATTRIBUTE_VALUE_SIZE 300

static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint8_t hid_descriptor_storage[MAX_ATTRIBUTE_VALUE_SIZE];

// TODO: only HID_PROTOCOL_MODE_REPORT works on Switch Pro Controller (bug?)
static hid_protocol_mode_t hid_host_report_mode = HID_PROTOCOL_MODE_REPORT;//_WITH_FALLBACK_TO_BOOT;
static uint16_t hid_host_cid = 0;

enum class ConnectionState
{
    Scan,
    StartConnection,
    Connecting,
    Connected
};

static ConnectionState state = ConnectionState::Scan;

static bd_addr_t connect_addr;
static bool have_addr = false;

static void start_scan()
{
    state = ConnectionState::Scan;
    have_addr = false;
    gap_inquiry_start(INQUIRY_INTERVAL);
}

static void bt_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    if(packet_type == HCI_EVENT_PACKET)
    {
        switch(hci_event_packet_get_type(packet))
        {
            case BTSTACK_EVENT_STATE:
                if(btstack_event_state_get_state(packet) == HCI_STATE_WORKING)
                {
                    bd_addr_t local_addr;
                    gap_local_bd_addr(local_addr);
                    printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));

                    start_scan();
                }
                break;

            // gap inquery events
            case GAP_EVENT_INQUIRY_RESULT:
            {
                bd_addr_t addr;
                const char *name = nullptr;

                gap_event_inquiry_result_get_bd_addr(packet, addr);

                auto cod = gap_event_inquiry_result_get_class_of_device(packet);

                // TODO: else request?
                if(gap_event_inquiry_result_get_name_available(packet))
                    name = (const char *)gap_event_inquiry_result_get_name(packet);

                printf("Found %s CoD %06X name %s\n", bd_addr_to_str(addr), cod, name ? name : "??");

                if(!have_addr)
                {
                    // try to connect to everything
                    // TODO: probably not the best idea
                    int majorClass = (cod >> 8) & 0x1F;
                    if(majorClass == 0b00101) // peripheral
                    {
                        memcpy(connect_addr, addr, sizeof(bd_addr_t));
                        have_addr = true;
                    }
                }

                break;
            }

            case GAP_EVENT_INQUIRY_COMPLETE:
                printf("inquiry complete\n");

                if(have_addr)
                    state = ConnectionState::StartConnection;
                else if(state == ConnectionState::Scan)
                    start_scan(); // scan again
                break;

            // hid event
            case HCI_EVENT_HID_META:
            {
                switch(hci_event_hid_meta_get_subevent_code(packet))
                {
                    case HID_SUBEVENT_INCOMING_CONNECTION:
                        printf("incoming conn\n");
                        hid_host_decline_connection(hid_subevent_incoming_connection_get_hid_cid(packet));
                        break;

                    case HID_SUBEVENT_CONNECTION_OPENED:
                    {
                        auto status = hid_subevent_connection_opened_get_status(packet);

                        if(status == ERROR_CODE_SUCCESS)
                        {
                            printf("connected\n");
                            state = ConnectionState::Connected;
                        }
                        else
                        {
                            // drop key if security error
                            // TODO: is this a hack?
                            if(status == L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY)
                            {
                                printf("drop key?\n");
                                gap_drop_link_key_for_bd_addr(connect_addr);
                            }

                            // TODO: try different device?
                            printf("Connection failed: %x\n", status);
                            start_scan();
                        }

                        break;
                    }

                    case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
                    {
                        auto status = hid_subevent_descriptor_available_get_status(packet);
                        if(status == ERROR_CODE_SUCCESS)
                            printf("got descriptor\n");
                        break;
                    }

                    case HID_SUBEVENT_REPORT:
                    {
                        auto report = hid_subevent_report_get_report(packet);
                        auto len = hid_subevent_report_get_report_len(packet);

                        printf("got report len %i\n", len);
                        break;
                    }

                    case HID_SUBEVENT_CONNECTION_CLOSED:
                    {
                        printf("disconnected\n");

                        start_scan();
                        break;
                    }
                }
                break;
            }

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

    l2cap_init();

#ifdef ENABLE_BLE
    sm_init();
#endif

    // HID host
    hid_host_init(hid_descriptor_storage, sizeof(hid_descriptor_storage));
    hid_host_register_packet_handler(bt_packet_handler);

    // Allow sniff mode requests by HID device and support role switch
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // try to become master on incoming connections
    hci_set_master_slave_policy(HCI_ROLE_MASTER);

    // enable EIR
    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);

    hci_event_callback_registration.callback = &bt_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);
        
    // usb init
    tusb_init();

    while(true)
    {
        if(state == ConnectionState::StartConnection)
        {
            printf("connecting to %s...\n", bd_addr_to_str(connect_addr));
            hid_host_connect(connect_addr, hid_host_report_mode, &hid_host_cid);
            state = ConnectionState::Connecting;
        }

        tud_task();

        //tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));

        sleep_ms(1);
    }

    return 0;
}
