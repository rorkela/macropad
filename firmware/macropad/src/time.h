#pragma once

#include <stdint.h>

void time_init(void);
uint32_t get_millis(void);
void sys_tick_handler(void);
