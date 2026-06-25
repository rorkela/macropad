#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "defs.h"

void usb_init(void);
void usb_run(void);
void usb_send(usb_action_t action, bool pressed);
