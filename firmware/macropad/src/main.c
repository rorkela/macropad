#include <stddef.h>

#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/scb.h>
#include <stdint.h>

#include "control.h"
#include "defs.h"
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

	if(control_init() != 0)
	{
		flash_unlock();
		flash_erase_page(LAST_PAGE);
		flash_program_word(LAST_PAGE, (uint32_t)MAGIC);
		flash_lock();

		scb_reset_system();
	}

	usb_init();
	input_init();

	usb_run();
}
