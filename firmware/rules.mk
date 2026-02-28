ifneq ($(V),1)
Q		:= @
NULL	:= 2>/dev/null
endif

###############################################################################
# Executables

PREFIX	?= arm-none-eabi-

CC		:= $(PREFIX)gcc
LD		:= $(PREFIX)gcc
OBJCOPY	:= $(PREFIX)objcopy
OPT		:= -Os
DEBUG	:= -ggdb3
CSTD	?= -std=c99

OUTPUT_DIR ?= ./bin
BUILD_DIR ?= ./build

###############################################################################
# Source files

OBJS		+= $(BUILD_DIR)/$(BINARY).o

LDFLAGS		+= -L$(OPENCM3_DIR)/lib
LDLIBS		+= -l$(LIBNAME)
LDSCRIPT	?= $(BINARY).ld

ifeq (,$(wildcard $(OPENCM3_DIR)))
$(error OPENCM3_DIR '$(OPENCM3_DIR)' is not a valid directory)
endif

ifeq (,$(wildcard $(OPENCM3_DIR)/lib/lib$(LIBNAME).a))
$(error LIBNAME '$(LIBNAME).a' not found)
endif

###############################################################################
# C flags

TGT_CFLAGS	+= $(OPT) $(CSTD) $(DEBUG)
TGT_CFLAGS	+= $(ARCH_FLAGS)
TGT_CFLAGS	+= -Wextra -Wshadow -Wimplicit-function-declaration
TGT_CFLAGS	+= -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes
TGT_CFLAGS	+= -fno-common -ffunction-sections -fdata-sections

###############################################################################
# C++ flags

TGT_CXXFLAGS	+= $(OPT) $(CXXSTD) $(DEBUG)
TGT_CXXFLAGS	+= $(ARCH_FLAGS)
TGT_CXXFLAGS	+= -Wextra -Wshadow -Wredundant-decls  -Weffc++
TGT_CXXFLAGS	+= -fno-common -ffunction-sections -fdata-sections

###############################################################################
# C & C++ preprocessor common flags

TGT_CPPFLAGS	+= -MD
TGT_CPPFLAGS	+= -Wall -Wundef
TGT_CPPFLAGS	+= $(DEFS)

###############################################################################
# Linker flags

TGT_LDFLAGS		+= --static -nostartfiles
TGT_LDFLAGS		+= -T$(LDSCRIPT)
TGT_LDFLAGS		+= $(ARCH_FLAGS) $(DEBUG)
TGT_LDFLAGS		+= -Wl,-Map=$(BUILD_DIR)/$(BINARY).map -Wl,--cref
TGT_LDFLAGS		+= -Wl,--gc-sections
ifeq ($(V),99)
TGT_LDFLAGS		+= -Wl,--print-gc-sections
endif

###############################################################################
# Used libraries

LDLIBS		+= -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group

###############################################################################
###############################################################################
###############################################################################

all: bin

elf: $(OUTPUT_DIR)/$(BINARY).elf
bin: $(OUTPUT_DIR)/$(BINARY).bin
flash: $(BINARY).flash

$(OUTPUT_DIR)/%.bin: $(OUTPUT_DIR)/%.elf
	@#printf "  OBJCOPY $(*).bin\n"
	$(Q)$(OBJCOPY) -Obinary $< $@

$(OUTPUT_DIR)/%.elf: $(OBJS) $(LDSCRIPT) $(OPENCM3_DIR)/lib/lib$(LIBNAME).a
	@#printf "  CC      $(*).elf\n"
	$(Q)mkdir -p $(OUTPUT_DIR)
	$(Q)$(LD) $(TGT_LDFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.c
	@#printf "  CC      $(*).c\n"
	$(Q)mkdir -p $(BUILD_DIR)
	$(Q)$(CC) $(TGT_CFLAGS) $(CFLAGS) $(TGT_CPPFLAGS) $(CPPFLAGS) -o $@ -c $<

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
