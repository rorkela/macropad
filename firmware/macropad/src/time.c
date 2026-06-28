#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>

#include "time.h"

static volatile uint32_t system_millis = 0;

void time_init(void) {
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_set_frequency(1000, 72000000);
	systick_interrupt_enable();
	systick_counter_enable();
}

uint32_t get_millis(void) {
	return system_millis;
}

void sys_tick_handler(void) {
	system_millis++;
}
