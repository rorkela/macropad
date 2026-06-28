#pragma once

#include <stdint.h>

void display_init(void);
void display_clear(void);
void display_char(uint8_t x, uint8_t y, char c);
void display_string(uint8_t x, uint8_t y, const char *str);
void display_update(void);
void display_get_screen_size(uint8_t* width, uint8_t* height);
