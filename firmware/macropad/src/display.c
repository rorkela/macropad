#include <libopencm3/stm32/i2c.h>
#include <stdint.h>
#include <string.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "display.h"
#include "time.h"

#include "font3x7.h"

#define SSD1306_ADDR 0x3C

static const uint8_t SSD1306_WIDTH = 128;
static const uint8_t SSD1306_HEIGHT = 64;

static uint8_t display_buffer[128 * 8];

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

	i2c_set_speed(I2C1, i2c_speed_sm_100k, rcc_apb1_frequency / 1000000);
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
	while (!(I2C_SR1(I2C1) & I2C_SR1_BTF));
	i2c_send_data(I2C1, cmd);
	while (!(I2C_SR1(I2C1) & I2C_SR1_BTF));
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
		while (!(I2C_SR1(I2C1) & I2C_SR1_BTF));
		i2c_send_data(I2C1, data[i]);
	}
	while (!(I2C_SR1(I2C1) & I2C_SR1_BTF));
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

static void ssd1306_set_pos(uint8_t x, uint8_t y) {
	ssd1306_command(0xB0 + y);
	ssd1306_command((x & 0x0F));
	ssd1306_command(((x & 0xF0) >> 4) | 0x10);
}

static void ssd1306_clear(void) {
	memset(display_buffer, 0, sizeof(display_buffer));
}

static void ssd1306_update(uint8_t page) {
	ssd1306_set_pos(0, page);
	ssd1306_data(&display_buffer[page * 128], 128);
}

static void ssd1306_char(uint8_t x, uint8_t y, char c) {
	if (c < 32 || c > 126) return;
	const uint8_t *glyph = font[(int)c];
	for (uint8_t col = 0; col < FONT_WIDTH; col++) {
		display_buffer[y * 128 + x + col] = glyph[col];
	}
	display_buffer[y * 128 + x + FONT_WIDTH] = 0x00;
}

static void ssd1306_string(uint8_t x, uint8_t y, const char *str) {
	uint8_t pos = 0;
	while (*str) {
		ssd1306_char(x + pos * (FONT_WIDTH + 1), y, *str);
		pos++;
		str++;
	}
}

static uint32_t last_display_millis = 0;
static uint8_t current_page = 0;

void display_init(void) {
	rcc_apb1_frequency = 72000000;
	rcc_apb2_frequency = 72000000;

	i2c_init();
	ssd1306_init();
	ssd1306_clear();

	last_display_millis = get_millis();
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

void display_get_screen_size(uint8_t* width, uint8_t* height) {
	*width = SSD1306_WIDTH;
	*height = SSD1306_HEIGHT / 8;
}

void display_update(void) {
	uint32_t current_time_millis = get_millis();
	if(current_time_millis - last_display_millis > 5) {
		last_display_millis = current_time_millis;
		ssd1306_update(current_page);
		current_page++;
		if(current_page >= 8) current_page = 0;
	}
}
