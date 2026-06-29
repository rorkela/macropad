#include <stddef.h>

#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/scb.h>
#include <stdint.h>

#include "defs.h"
#include "control.h"
#include "display.h"
#include "input.h"
#include "time.h"
#include "usb.h"

static void init(void)
{
	SCB_VTOR = START_ADDR;

	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
}

int main(void)
{
	for (int i=0;i<80000;i++) __asm__ volatile ("nop");
	init();

	time_init();

	display_init();
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

	while(1)
	{
		input_poll();
		display_update();
		usb_run();
	}
}
