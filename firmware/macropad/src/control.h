#pragma once

#include "defs.h"
#include <stdint.h>
#include <stdbool.h>

int control_init(void);
void control_handle_key_event(uint8_t key, bool pressed);
void control_handle_enc_rotate(uint8_t enc, int8_t dir);
void control_handle_enc_press(uint8_t enc, bool pressed);

const mappings_t* get_mappings(void);
const macro_layer_t* get_layer(uint8_t layer);
uint8_t get_current_layer_idx(void);
uint8_t get_num_layers(void);
const char* get_layer_name(uint8_t layer);
