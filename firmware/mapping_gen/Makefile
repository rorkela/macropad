V?=0
ifeq ($(V),0)
Q	:= @
NULL	:= 2>/dev/null
endif

PROJECT := mapping-gen

SRC_DIR := src
INC_DIR := ../common

INCLUDES    += -I$(INC_DIR)

CFLAGS += $(INCLUDES)

BUILD_DIR  ?= build
OUTPUT_DIR ?= bin

CC      := gcc

CFILES := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(CFILES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

all: $(OUTPUT_DIR)/$(PROJECT)

$(OUTPUT_DIR)/$(PROJECT): $(OBJS)
	@#printf "  CC      $(*)\n"
	$(Q)mkdir -p $(OUTPUT_DIR)
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@#printf "  CC      $(*).c\n"
	$(Q)mkdir -p $(BUILD_DIR)
	$(Q)$(CC) $(CFLAGS) -o $@ -c $<

clean:
	@#printf "  CLEAN\n"
	$(Q)$(RM) -r $(OUTPUT_DIR) $(BUILD_DIR)

.PHONY: clean all
