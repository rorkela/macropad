######################################################
# Stuff to define in per module Makefile (if needed) #
######################################################

# LIB_NAME and its corresponding DEFS are necessary based on what you are building for
# PROJECT is necesary for the output name

SRC_DIR  ?= src
INC_DIR  ?= include

OOCD           ?= openocd
OOCD_INTERFACE ?= stlink
OOCD_TARGET    ?= stm32f1x

FP_FLAGS   ?= -msoft-float
ARCH_FLAGS ?= -mthumb -mcpu=cortex-m3 $(FP_FLAGS)

BUILD_DIR  ?= build
OUTPUT_DIR ?= bin

FLASH_BASE   ?= 0x08000000
FLASH_OFFSET ?= 0K
FLASH_SIZE   ?= 64K
RAM_BASE     ?= 0x20000000
RAM_SIZE     ?= 20K
ROM_SIZE     ?= 64K

###############
# Dont change #
###############

V?=0
ifeq ($(V),0)
Q	:= @
NULL	:= 2>/dev/null
endif

PREFIX  := arm-none-eabi-

CC      := $(PREFIX)gcc
LD      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump

OPT  ?= -Os
CSTD ?= -std=c99

ROOT_DIR    := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
OPENCM3_DIR := $(ROOT_DIR)/libopencm3
OPENCM3_INC := $(OPENCM3_DIR)/include

INCLUDES    += -I$(OPENCM3_INC) -I$(SRC_DIR) -I$(INC_DIR)

LDSCRIPT ?= $(ROOT_DIR)/common.ld

CFILES := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(CFILES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

OPENCM3_LIB := $(OPENCM3_DIR)/lib/lib$(LIBNAME).a

TGT_CFLAGS	+= $(OPT) $(CSTD) -ggdb3
TGT_CFLAGS	+= $(ARCH_FLAGS) $(INCLUDES)
TGT_CFLAGS	+= -Wextra -Wshadow -Wimplicit-function-declaration
TGT_CFLAGS	+= -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes
TGT_CFLAGS	+= -Wall -Wundef
TGT_CFLAGS	+= $(DEFS)
TGT_CFLAGS	+= -fno-common -ffunction-sections -fdata-sections

TGT_LDFLAGS		+= --static -nostartfiles
TGT_LDFLAGS		+= -T$(LDSCRIPT) -L$(OPENCM3_DIR)/lib
TGT_LDFLAGS		+= $(ARCH_FLAGS)
TGT_LDFLAGS		+= -Wl,--gc-sections
ifeq ($(V),99)
TGT_LDFLAGS		+= -Wl,--print-gc-sections
endif

LDLIBS		+= -Wl,--start-group -l$(LIBNAME) -lc -lgcc -lnosys -Wl,--end-group

OFFSET_BYTES := $(shell echo $$(($(subst K,*1024,$(FLASH_OFFSET)))))
FLASH_BYTES  := $(shell echo $$(($(subst K,*1024,$(FLASH_SIZE)))))
ROM_BYTES    := $(shell echo $$(($(subst K,*1024,$(ROM_SIZE)))))
START_ADDR   := $(shell printf "0x%08X" $$(($(FLASH_BASE) + $(OFFSET_BYTES))))
END_ADDR     := $(shell printf "0x%08X" $$(($(START_ADDR) + $(FLASH_BYTES))))
LAST_ADDR    := $(shell printf "0x%08X" $$(($(FLASH_BASE) + $(ROM_BYTES))))
LAST_PAGE    := $(shell printf "0x%08X" $$(($(LAST_ADDR) - 1024)))

# Set SCB_VTOR to this
TGT_CFLAGS += -DSTART_ADDR=$(START_ADDR)
TGT_CFLAGS += -DEND_ADDR=$(END_ADDR)
TGT_CFLAGS += -DLAST_PAGE=$(LAST_PAGE)

TGT_LDFLAGS += -Wl,--defsym,FLASH_ORIGIN=$(START_ADDR)
TGT_LDFLAGS += -Wl,--defsym,FLASH_SIZE=$(FLASH_SIZE)
TGT_LDFLAGS += -Wl,--defsym,RAM_ORIGIN=$(RAM_BASE)
TGT_LDFLAGS += -Wl,--defsym,RAM_SIZE=$(RAM_SIZE)

$(info ========================================)
$(info BUILDING PROJECT: $(PROJECT))
$(info Flash Origin:     $(START_ADDR))
$(info Flash Size:       $(FLASH_SIZE))
$(info RAM Origin:       $(RAM_BASE))
$(info RAM Size:         $(RAM_SIZE))
$(info Start Addr:       $(START_ADDR))
$(info End Addr:         $(END_ADDR))
$(info Last Page:        $(LAST_PAGE))
$(info ========================================)
$(info)

all: elf bin

elf: $(OUTPUT_DIR)/$(PROJECT).elf
bin: $(OUTPUT_DIR)/$(PROJECT).bin
flash: $(PROJECT).flash

$(OUTPUT_DIR)/%.bin: $(OUTPUT_DIR)/%.elf
	@#printf "  OBJCOPY $(*).bin\n"
	$(Q)$(OBJCOPY) -O binary $< $@

$(OUTPUT_DIR)/$(PROJECT).elf: $(OBJS) $(LDSCRIPT) $(OPENCM3_LIB)
	@#printf "  CC      $(*).elf\n"
	$(Q)mkdir -p $(OUTPUT_DIR)
	$(Q)$(LD) $(TGT_LDFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@#printf "  CC      $(*).c\n"
	$(Q)mkdir -p $(BUILD_DIR)
	$(Q)$(CC) $(TGT_CFLAGS) $(CFLAGS) -o $@ -c $<

clean:
	@#printf "  CLEAN\n"
	$(Q)$(RM) -r $(OUTPUT_DIR) $(BUILD_DIR)

ifeq ($(OOCD_FILE),)
%.flash: $(OUTPUT_DIR)/%.elf
	@printf "  FLASH   $<\n"
	(echo "halt; program $(realpath $(*).elf) verify reset" | nc -4 localhost 4444 2>/dev/null) || \
		$(OOCD) -f interface/$(OOCD_INTERFACE).cfg \
		-f target/$(OOCD_TARGET).cfg \
		-c "program $< verify reset exit" \
		$(NULL)
else
%.flash: $(OUTPUT_DIR)/%.elf
	@printf "  FLASH   $<\n"
	(echo "halt; program $(realpath $(*).elf) verify reset" | nc -4 localhost 4444 2>/dev/null) || \
		$(OOCD) -f $(OOCD_FILE) \
		-c "program $< verify reset exit" \
		$(NULL)
endif

.PHONY: clean elf bin map
