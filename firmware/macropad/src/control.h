#pragma once

#include "defs.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
	NORMAL, LAYER_SWITCH, ERROR=-1
} mode_t;


int control_init(void);
void control_handle_key_event(uint8_t key, bool pressed);
void control_handle_enc_rotate(uint8_t enc, int8_t dir);
void control_handle_enc_press(uint8_t enc, bool pressed);

mode_t get_current_mode(void);
const mappings_t* get_mappings(void);
uint8_t get_current_layer_idx(void);
