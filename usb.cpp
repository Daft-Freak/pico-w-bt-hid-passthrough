#include "tusb.h"

#include "usb.hpp"

extern uint8_t desc_configuration[];

static uint8_t *report_data = nullptr;
static int report_len = 0;

static const uint8_t *hid_desc = nullptr;

void usb_init()
{
    // usb init
    tusb_init();
    tud_disconnect();
}

void usb_update()
{
    tud_task();

    // send report
    if(report_data && tud_connected())
    {
        tud_hid_report(0, report_data, report_len);
    
        delete[] report_data;
        report_data = nullptr;
    }
}

void usb_set_connected(bool connected)
{
    if(connected)
        tud_connect();
    else
        tud_disconnect();
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return hid_desc;
}

void usb_set_hid_descriptor(const uint8_t *data, uint16_t len)
{
    // set report descriptor len
    desc_configuration[25] = len;
    desc_configuration[26] = len >> 8;

    hid_desc = data;
}

void usb_queue_report(const uint8_t *data, uint16_t len)
{
    // TODO: actually queue
    if(report_data)
        printf("dropping report!\n");
    else
    {
        report_data = new uint8_t[len];
        report_len = len;
        memcpy(report_data, data, len);
    }
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint8_t len)
{
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
}