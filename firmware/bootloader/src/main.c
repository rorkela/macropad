#include <stdint.h>

#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/flash.h>

#include "defs.h"
#include "dfu.h"

static void jumpToProgram(void)
{
	uint32_t msp = *(volatile uint32_t*)END_ADDR;
	if ((msp & 0xFF000000) == 0x20000000)
	{
		flash_unlock();
		flash_erase_page(LAST_PAGE);
		flash_lock();

		rcc_periph_clock_disable(RCC_GPIOB);
		rcc_osc_on(RCC_HSI);
		rcc_wait_for_osc_ready(RCC_HSI);
		rcc_set_sysclk_source(RCC_HSI);

		uint32_t jump_addr = *(volatile uint32_t*)(END_ADDR + 4);

		SCB_VTOR = END_ADDR;

		__asm__ volatile("msr msp, %0" : : "r" (msp) : );
		void (*app_reset_handler)(void) = (void (*)(void))jump_addr;
		app_reset_handler();
	}
}

static void init(void)
{
	SCB_VTOR = START_ADDR;

	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

	rcc_periph_clock_enable(RCC_GPIOB);

	if (!gpio_get(GPIOB, GPIO2) && *((uint32_t*)LAST_PAGE) != MAGIC) jumpToProgram();
}

int main(void)
{
	init();
	dfu_init();
	dfu_run();
}
