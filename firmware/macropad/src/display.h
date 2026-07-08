#pragma once

#include <stdint.h>

typedef enum {
	DISPLAY_NORMAL, DISPLAY_LAYER_MENU
} display_mode_t;

void display_init(void);
void display_clear(void);
void display_char(uint8_t x, uint8_t y, char c);
void display_string(uint8_t x, uint8_t y, const char *str);
void display_hline(uint8_t x1, uint8_t x2, uint8_t pixel_y);
void display_get_screen_size(uint8_t* width, uint8_t* height);
void display_iit(uint8_t x, uint8_t y);

void display_update_all(void);
void display_update_chunk(void);

void display_set_mode(display_mode_t mode);
void display_cycle_mode(void);
void display_layer(void);
