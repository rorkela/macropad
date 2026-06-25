#include <stddef.h>
#include <stdint.h>

#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/stm32/timer.h>

#include "defs.h"
#include "input.h"
#include "control.h"

typedef struct {
	uint32_t port;
	uint16_t pin;
	bool is_pressed;
	uint8_t counter;
	bool changed;
} switch_t;

typedef struct {
	uint32_t tim;
	uint16_t last_value;
	uint8_t counter;
	int8_t dir;
	bool changed;
} enc_t;

static switch_t switches[NUM_KEYS] = {
	{GPIOB, GPIO3, false, 0, false},
	{GPIOB, GPIO4, false, 0, false},
	{GPIOB, GPIO5, false, 0, false},
	{GPIOB, GPIO8, false, 0, false},
	{GPIOB, GPIO9, false, 0, false},
	{GPIOB, GPIO11, false, 0, false},
	{GPIOB, GPIO12, false, 0, false},
	{GPIOB, GPIO13, false, 0, false},
	{GPIOB, GPIO14, false, 0, false},
};

static switch_t enc_buttons[NUM_ENCODERS] = {
	{GPIOA, GPIO10, false, 0, false},
	{GPIOA, GPIO2, false, 0, false}
};

static enc_t enc_rot[NUM_ENCODERS] = {
	{TIM1, 0, 0, 0, false},
	{TIM2, 0, 0, 0, false}
};

static void configure_timer_hardware(uint32_t tim)
{
	switch(tim)
	{
		case TIM1:
			rcc_periph_clock_enable(RCC_GPIOA);
			rcc_periph_clock_enable(RCC_TIM1);
			gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO8 | GPIO9);
			break;
		case TIM2:
			rcc_periph_clock_enable(RCC_GPIOA);
			rcc_periph_clock_enable(RCC_TIM2);
			gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO0 | GPIO1);
			break;
		default:
			break;
	}
}

static void encoder_init(void)
{
	for(int i = 0; i < NUM_ENCODERS; i++) {
		uint32_t timer = enc_rot[i].tim;
		configure_timer_hardware(timer);

		timer_set_period(timer, 0xFFFF);

		timer_ic_set_filter(timer, TIM_IC1, TIM_IC_CK_INT_N_8);
		timer_ic_set_filter(timer, TIM_IC2, TIM_IC_CK_INT_N_8);

		timer_ic_set_polarity(timer, TIM_IC1, TIM_IC_FALLING);
		timer_ic_set_polarity(timer, TIM_IC2, TIM_IC_FALLING);

		timer_slave_set_mode(timer, TIM_SMCR_SMS_EM3);
		timer_enable_counter(timer);
	}

	for(int i = 0; i < NUM_ENCODERS; i++) {
		enc_rot[i].last_value = timer_get_counter(enc_rot[i].tim);
	}
}

static void switches_init(void)
{
	rcc_periph_clock_enable(RCC_AFIO);
	gpio_primary_remap(AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON, 0);

	for(int i = 0; i < NUM_KEYS; i++) {
		rcc_periph_clock_enable(switches[i].port);
		gpio_set_mode(
				switches[i].port,
				GPIO_MODE_INPUT,
				GPIO_CNF_INPUT_PULL_UPDOWN,
				switches[i].pin);
		gpio_set(switches[i].port, switches[i].pin);
	}

	for(int i = 0; i < NUM_ENCODERS; i++) {
		rcc_periph_clock_enable(enc_buttons[i].port);
		gpio_set_mode(
				enc_buttons[i].port,
				GPIO_MODE_INPUT,
				GPIO_CNF_INPUT_FLOAT,
				enc_buttons[i].pin);
	}
}

static int8_t get_encoder_value(int enc)
{
	uint16_t current_val = timer_get_counter(enc_rot[enc].tim);
	int16_t raw_delta = (int16_t)(current_val - enc_rot[enc].last_value);

	int16_t clicks = raw_delta / 4;
	int8_t dir = (clicks > 0)? 1 : ((clicks < 0)? -1: 0);

	if(dir != 0) enc_rot[enc].last_value += (clicks * 4);

	return dir;
}

void input_init(void)
{
	encoder_init();
	switches_init();

	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
	systick_set_reload(8999);
	systick_interrupt_enable();
	systick_counter_enable();
}

void input_poll(void)
{
	for(int i = 0; i < NUM_KEYS; i++) {
		if(switches[i].changed) {
			bool state;

			CM_ATOMIC_BLOCK() {
				switches[i].changed = false;
				state = switches[i].is_pressed;
			}
			control_handle_key_event(i, state);
		}
	}

	for(int i = 0; i < NUM_ENCODERS; i++) {
		if(enc_buttons[i].changed) {
			bool state;

			CM_ATOMIC_BLOCK() {
				enc_buttons[i].changed = false;
				state = enc_buttons[i].is_pressed;
			}

			control_handle_enc_press(i, state);
		}
		if(enc_rot[i].changed) {
			int8_t dir;

			CM_ATOMIC_BLOCK() {
				enc_rot[i].changed = false;
				dir = enc_rot[i].dir;
			}

			control_handle_enc_rotate(i, dir);
		}
	}
}

#define DEBOUNCE_TIME 10
#define ENCODER_HOLD_TIME 30

static void sys_tick_handler(void)
{
	for(int i = 0; i < NUM_KEYS; i++) {
		bool raw = (gpio_get(switches[i].port, switches[i].pin) == 0);

		if(raw != switches[i].is_pressed) {
			switches[i].counter++;
			if(switches[i].counter >= DEBOUNCE_TIME) {
				switches[i].is_pressed = raw;
				switches[i].counter = 0;
				switches[i].changed = true;
			}
		} else {
			switches[i].counter = 0;
		}
	}

	for(int i = 0; i < NUM_ENCODERS; i++) {
		bool raw = (gpio_get(enc_buttons[i].port, enc_buttons[i].pin) == 0);

		if(raw != enc_buttons[i].is_pressed) {
			enc_buttons[i].counter++;
			if(enc_buttons[i].counter >= DEBOUNCE_TIME) {
				enc_buttons[i].is_pressed = raw;
				enc_buttons[i].counter = 0;
				enc_buttons[i].changed = true;
			}
		} else {
			enc_buttons[i].counter = 0;
		}
	}

	for(int i = 0; i < NUM_ENCODERS; i++) {
		int8_t dir = get_encoder_value(i);

		if(dir != enc_rot[i].dir) {
			enc_rot[i].dir = dir;
			enc_rot[i].changed = true;
			enc_rot[i].counter = ENCODER_HOLD_TIME;
		}
		else if(enc_rot[i].counter > 0) {
			enc_rot[i].counter--;

			if(enc_rot[i].counter == 0) {
				enc_rot[i].dir = 0;
				enc_rot[i].changed = true;
			}
		}
	}
}
