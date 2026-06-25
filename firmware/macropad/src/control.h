#pragma once

#include <stdint.h>
#include <stdbool.h>

void control_init(void);
void control_handle_key_event(uint8_t key, bool pressed);
void control_handle_enc_rotate(uint8_t enc, uint8_t dir);
void control_handle_enc_press(uint8_t enc, bool pressed);
