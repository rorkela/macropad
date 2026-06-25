#include <stddef.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/scb.h>

#include "input.h"
#include "usb.h"

static void init(void)
{
	SCB_VTOR = START_ADDR;

	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
}

int main(void)
{
	init();

	usb_init();
	input_init();

	usb_run();
}
