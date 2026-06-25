#pragma once

#include <stdint.h>
#include <stdbool.h>

void usb_init(void);
void usb_run(void);
void usb_send_key(const uint8_t* payload);
void usb_send_cntrl(const uint8_t* payload);
