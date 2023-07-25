#pragma once

void usb_init();

void usb_update();

void usb_set_connected(bool connected);

void usb_set_hid_descriptor(const uint8_t *data, uint16_t len);
void usb_queue_report(const uint8_t *data, uint16_t len);