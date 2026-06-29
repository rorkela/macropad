#pragma once

#include <stdint.h>

#include "defs.h"

void display_init(void);
void display_clear(void);
void display_char(uint8_t x, uint8_t y, char c);
void display_string(uint8_t x, uint8_t y, const char *str);
void display_hline(uint8_t x1, uint8_t x2, uint8_t pixel_y);
void display_update(void);
void display_get_screen_size(uint8_t* width, uint8_t* height);
void display_layer(macro_layer_t* layer);
