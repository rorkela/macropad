#include <libopencm3/stm32/i2c.h>
#include <string.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "font3x7.h"

#define I2C I2C1
#define SSD1306_ADDR 0x3C
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

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

	i2c_set_speed(I2C, i2c_speed_sm_100k, rcc_apb1_frequency / 1000000);
	i2c_peripheral_enable(I2C);
}

static void ssd1306_command(uint8_t cmd) {
	uint8_t data[2] = {0x00, cmd};
	while (I2C_SR2(I2C) & I2C_SR2_BUSY);
	i2c_send_start(I2C);
	while (!(I2C_SR1(I2C) & I2C_SR1_SB));
	i2c_send_7bit_address(I2C, SSD1306_ADDR, I2C_WRITE);
	while (!(I2C_SR1(I2C) & I2C_SR1_ADDR));
	(void)I2C_SR2(I2C);
	i2c_send_data(I2C, data[0]);
	while (!(I2C_SR1(I2C) & I2C_SR1_BTF));
	i2c_send_data(I2C, data[1]);
	while (!(I2C_SR1(I2C) & I2C_SR1_BTF));
	i2c_send_stop(I2C);
}

static void ssd1306_data(uint8_t *data, uint16_t len) {
	while (I2C_SR2(I2C) & I2C_SR2_BUSY);
	i2c_send_start(I2C);
	while (!(I2C_SR1(I2C) & I2C_SR1_SB));
	i2c_send_7bit_address(I2C, SSD1306_ADDR, I2C_WRITE);
	while (!(I2C_SR1(I2C) & I2C_SR1_ADDR));
	(void)I2C_SR2(I2C);
	i2c_send_data(I2C, 0x40);
	for (uint16_t i = 0; i < len; i++) {
		while (!(I2C_SR1(I2C) & I2C_SR1_BTF));
		i2c_send_data(I2C, data[i]);
	}
	while (!(I2C_SR1(I2C) & I2C_SR1_BTF));
	i2c_send_stop(I2C);
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

static void ssd1306_update(void) {
	for (uint8_t page = 0; page < 8; page++) {
		ssd1306_set_pos(0, page);
		ssd1306_data(&display_buffer[page * 128], 128);
	}
}

static void ssd1306_char(uint8_t x, uint8_t y, char c) {
	if (c < 32 || c > 126) return;
	const uint8_t *glyph = font[c];
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

int main(void) {
	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
	rcc_apb1_frequency = 72000000;
	rcc_apb2_frequency = 72000000;

	i2c_init();
	ssd1306_init();
	ssd1306_clear();

	int c = 32;
	for(int i = 0; i < 8; i++)  {
		for(int j = 0; j < SSD1306_WIDTH/(FONT_WIDTH + 1); j++) {
			ssd1306_char(j*(FONT_WIDTH + 1), i, c);
			c++;
			if(c > 126)
				break;
		}
		if(c > 126)
			break;
	}

	c = 32;
	for(int i = 3; i < 8; i++)  {
		for(int j = 0; j < SSD1306_WIDTH/(FONT_WIDTH + 1); j++) {
			ssd1306_char(j*(FONT_WIDTH + 1), i, c);
			c++;
			if(c > 126)
				break;
		}
		if(c > 126)
			break;
	}

	c = 32;
	for(int i = 6; i < 8; i++)  {
		for(int j = 0; j < SSD1306_WIDTH/(FONT_WIDTH + 1); j++) {
			ssd1306_char(j*(FONT_WIDTH + 1), i, c);
			c++;
			if(c > 126)
				break;
		}
		if(c > 126)
			break;
	}

	ssd1306_update();

	while (1);
}
