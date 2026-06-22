MEMORY
{
	rom (rx) : ORIGIN = FLASH_ORIGIN, LENGTH = FLASH_SIZE
	ram (rwx) : ORIGIN = RAM_ORIGIN, LENGTH = RAM_SIZE
}

INCLUDE cortex-m-generic.ld
