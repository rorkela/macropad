#include <stdint.h>
#include <string.h>

#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "display.h"
#include "control.h"

#include "defs.h"
#include "font3x7.h"
#include "figures.h"
#define SSD1306_ADDR 0x3C
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

#define CHUNK_SIZE 32
#define NUM_CHUNKS ((SSD1306_WIDTH / CHUNK_SIZE) * (SSD1306_HEIGHT / 8))

static uint8_t display_buffer[SSD1306_WIDTH * (SSD1306_HEIGHT / 8)];
static uint8_t dirty[NUM_CHUNKS];

static display_mode_t current_mode = DISPLAY_NORMAL;

static void i2c_init(void) {
	rcc_periph_clock_enable(RCC_I2C1);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_AFIO);

	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO6);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO7);

	i2c_peripheral_disable(I2C1);
	rcc_periph_reset_pulse(RST_I2C1);

	i2c_set_speed(I2C1, i2c_speed_fm_400k, rcc_apb1_frequency / 1000000);
	i2c_peripheral_enable(I2C1);
}

static void ssd1306_command(uint8_t cmd) {
	while (I2C_SR2(I2C1) & I2C_SR2_BUSY);
	i2c_send_start(I2C1);
	while (!(I2C_SR1(I2C1) & I2C_SR1_SB));
	i2c_send_7bit_address(I2C1, SSD1306_ADDR, I2C_WRITE);
	while (!(I2C_SR1(I2C1) & I2C_SR1_ADDR));
	(void)I2C_SR2(I2C1);
	i2c_send_data(I2C1, 0x00); // Indicate command
	while (!(I2C_SR1(I2C1) & I2C_SR1_TxE));
	i2c_send_data(I2C1, cmd);
	while (!(I2C_SR1(I2C1) & I2C_SR1_TxE));
	i2c_send_stop(I2C1);
}

static void ssd1306_data(uint8_t *data, uint16_t len) {
	while (I2C_SR2(I2C1) & I2C_SR2_BUSY);
	i2c_send_start(I2C1);
	while (!(I2C_SR1(I2C1) & I2C_SR1_SB));
	i2c_send_7bit_address(I2C1, SSD1306_ADDR, I2C_WRITE);
	while (!(I2C_SR1(I2C1) & I2C_SR1_ADDR));
	(void)I2C_SR2(I2C1);
	i2c_send_data(I2C1, 0x40);
	for (uint16_t i = 0; i < len; i++) {
		while (!(I2C_SR1(I2C1) & I2C_SR1_TxE));
		i2c_send_data(I2C1, data[i]);
	}
	while (!(I2C_SR1(I2C1) & I2C_SR1_TxE));
	i2c_send_stop(I2C1);
}

static void ssd1306_init(void) {
	// display off
	ssd1306_command(0xAE);

	// refresh rate
	ssd1306_command(0xD5);
	ssd1306_command(0x10);

	// Number of rows
	ssd1306_command(0xA8);
	ssd1306_command(0x3F);

	// Display offset vertical
	ssd1306_command(0xD3);
	ssd1306_command(0x00);

	// set top of display
	ssd1306_command(0x40);

	// Enable charge pump
	ssd1306_command(0x8D);
	ssd1306_command(0x14);

	// Memory and page addressing
	ssd1306_command(0x20);
	ssd1306_command(0x02);

	// Display orientation
	ssd1306_command(0xA0);
	ssd1306_command(0xC0);

	// com pin hardware config (for display grid)
	ssd1306_command(0xDA);
	ssd1306_command(0x12);

	// Contrast
	ssd1306_command(0x81);
	ssd1306_command(0x22);

	// Set precharge
	ssd1306_command(0xD9);
	ssd1306_command(0xF1);

	// set voltage regulator output
	ssd1306_command(0xDB);
	ssd1306_command(0x40);

	// display mode
	ssd1306_command(0xA4); //normal (0xA5 for test - all pixels on)

	// Normal display mode (1 = white)
	ssd1306_command(0xA6);

	// display on
	ssd1306_command(0xAF);
}

static void mark_chunk_dirty(uint8_t x, uint8_t y) {
	uint8_t chunk_id = (y * (SSD1306_WIDTH / CHUNK_SIZE)) + (x / CHUNK_SIZE);

	if(chunk_id < NUM_CHUNKS) dirty[chunk_id] = 1;
}

static void mark_all_dirty(void) {
	for(int i = 0; i < NUM_CHUNKS; i++)
		dirty[i] = 1;
}

static void ssd1306_set_pos(uint8_t x, uint8_t y) {
	ssd1306_command(0xB0 + y);
	ssd1306_command((x & 0x0F));
	ssd1306_command(((x & 0xF0) >> 4) | 0x10);
}

static void ssd1306_clear(void) {
	memset(display_buffer, 0, sizeof(display_buffer));
	mark_all_dirty();
}

static void ssd1306_update(uint8_t chunk) {
	uint8_t page = chunk / (SSD1306_WIDTH / CHUNK_SIZE);
	uint8_t offset = (chunk % (SSD1306_WIDTH / CHUNK_SIZE)) * CHUNK_SIZE;

	ssd1306_set_pos(offset, page);
	ssd1306_data(&display_buffer[(page * SSD1306_WIDTH) + offset], CHUNK_SIZE);
}

static void ssd1306_char(uint8_t x, uint8_t y, char c) {
	if (c < 32 || c > 126) return;
	if (x + FONT_WIDTH >= SSD1306_WIDTH) return;
	if (y >= (SSD1306_HEIGHT / 8)) return;

	const uint8_t *glyph = font[(int)c];
	for (uint8_t col = 0; col < FONT_WIDTH; col++) {
		display_buffer[y * SSD1306_WIDTH + x + col] = glyph[col];
		mark_chunk_dirty(x + col, y);
	}
	display_buffer[y * SSD1306_WIDTH + x + FONT_WIDTH] = 0x00;
	mark_chunk_dirty(x + FONT_WIDTH, y);
}

static void ssd1306_string(uint8_t x, uint8_t y, const char *str) {
	uint8_t pos = 0;
	while (*str) {
		ssd1306_char(x + pos * (FONT_WIDTH + 1), y, *str);
		pos++;
		str++;
	}
}

static void ssd1306_char_2x(uint8_t x, uint8_t y, char c) {
	if (c < 32 || c > 126) return;
	if (x + (FONT_WIDTH * 2) >= SSD1306_WIDTH) return;
	if (y >= (SSD1306_HEIGHT / 8) - 1) return;

	const uint8_t *glyph = font[(int)c];
	for (uint8_t col = 0; col < FONT_WIDTH; col++) {
		uint8_t original_col = glyph[col];

		uint8_t page1_bits = 0;
		uint8_t page2_bits = 0;

		for (uint8_t bit = 0; bit < 7; bit++) {
			if (original_col & (1 << bit)) {
				if (bit < 4) {
					page1_bits |= (0x03 << (bit * 2));
				}
				else {
					page2_bits |= (0x03 << ((bit - 4) * 2));
				}
			}
		}

		for (uint8_t h = 0; h < 2; h++) {
			uint8_t target_x = x + (col * 2) + h;
			if (target_x < SSD1306_WIDTH) {
				display_buffer[y * SSD1306_WIDTH + target_x] = page1_bits;
				mark_chunk_dirty(target_x, y);

				display_buffer[(y + 1) * SSD1306_WIDTH + target_x] = page2_bits;
				mark_chunk_dirty(target_x, y + 1);
			}
		}
	}

	uint8_t spacer_x = x + (FONT_WIDTH * 2);
	for (uint8_t h = 0; h < 2; h++) {
		uint8_t tx = spacer_x + h;
		if (tx < SSD1306_WIDTH) {
			display_buffer[y * SSD1306_WIDTH + tx] = 0x00;
			display_buffer[(y + 1) * SSD1306_WIDTH + tx] = 0x00;
			mark_chunk_dirty(tx, y);
			mark_chunk_dirty(tx, y + 1);
		}
	}
}

static void ssd1306_string_2x(uint8_t x, uint8_t y, const char *str) {
	if(str == NULL) return;

	uint8_t pos = 0;
	while (*str) {
		ssd1306_char_2x(x + pos * 2 * (FONT_WIDTH + 1), y, *str);
		pos++;
		str++;
	}
}

static void ssd1306_hline(uint8_t x1, uint8_t x2, uint8_t pixel_y){
	if(pixel_y > SSD1306_HEIGHT) return;
	if(x1 >= SSD1306_WIDTH || x1 > x2) return;
	if(x2 >= SSD1306_WIDTH) x2 = SSD1306_WIDTH - 1;

	uint8_t page = pixel_y / 8;
	uint8_t bitmask = (1 << (pixel_y % 8));
	for (int x = x1; x <= x2; x++) {
		display_buffer[page * SSD1306_WIDTH + x] |= bitmask;
		mark_chunk_dirty(x, page);
	}
}

void display_init(void) {
	i2c_init();
	ssd1306_init();
	ssd1306_clear();
}

void display_clear(void) {
	ssd1306_clear();
}

void display_char(uint8_t x, uint8_t y, char c) {
	ssd1306_char(x, y, c);
}

void display_string(uint8_t x, uint8_t y, const char *str) {
	ssd1306_string(x, y, str);
}

void display_hline(uint8_t x1, uint8_t x2, uint8_t pixel_y) {
	ssd1306_hline(x1, x2, pixel_y);
}

void display_iit(uint8_t x, uint8_t y) {
	if (x + IITH_WIDTH >= SSD1306_WIDTH) return;
	if (y + IITH_HEIGHT_8 >= SSD1306_HEIGHT/8) return;
	for (uint8_t row = 0; row < IITH_HEIGHT_8; row++) {
		for (uint8_t col = 0; col < IITH_WIDTH; col++) {
			display_buffer[(y+row) * SSD1306_WIDTH + x + col] = iith[row][col];
			mark_chunk_dirty(x + col, y+row);
		}
	}
}

void display_get_screen_size(uint8_t* width, uint8_t* height) {
	*width = SSD1306_WIDTH;
	*height = SSD1306_HEIGHT / 8;
}

void display_update(void) {
	for(int i = 0; i < NUM_CHUNKS; i++) {
		if (dirty[i]) {
			ssd1306_update(i);
			dirty[i] = 0;
			break;
		}
	}
}

void display_set_mode(display_mode_t mode) {
	current_mode = mode;
}

void display_cycle_mode(void) {
	current_mode = (current_mode == DISPLAY_NORMAL)? DISPLAY_LAYER_MENU:DISPLAY_NORMAL;
}

void display_layer(void) {
	ssd1306_clear();

	uint8_t current_layer = get_current_layer_idx();
	uint8_t total_layers = get_num_layers();
	const macro_layer_t* layer;

	switch(current_mode) {
		case DISPLAY_NORMAL:
			layer = get_layer(current_layer);

			ssd1306_hline(0, SSD1306_WIDTH, 9);
			ssd1306_string(0, 0, layer->layer_name);

			for(int i = 0; i < 3; i++) {
				for(int j = 0; j < 3; j++) {
					ssd1306_string(j*44, 2 + i*2, layer->binding_names[j*3 + i]);
				}
			}
			break;
		case DISPLAY_LAYER_MENU:
			if(current_layer > 0)
				ssd1306_string(0, 1, get_layer_name(current_layer - 1));
			if(current_layer < (total_layers - 1))
				ssd1306_string(0, 7, get_layer_name(current_layer + 1));

			ssd1306_string_2x(0, 3, get_layer_name(current_layer));
			break;
		default:
			break;
	}
}
