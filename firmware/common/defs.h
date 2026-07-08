#pragma once

#include <stdint.h>

#define MAGIC 0x87AD91B2

#ifndef FLASH_BASE
#define FLASH_BASE 0x08000000
#endif
#define MAPPINGS_ADDR 0x08007800

#define MAX_LAYER_NAME 10
#define MAX_BINDING_NAME 10
#define NUM_KEYS 9
#define NUM_ENCODERS 2

#define PAYLOAD_SIZE 14

typedef enum: uint8_t {
	USB_TYPE_KEY, USB_TYPE_CNTRL
} usb_type_t;

// for encoder, the payload contains the actions in
// the first 3 bytes, (left rotate, right rotate, press)

typedef struct __attribute__((packed)) {
	usb_type_t usb_type;
	uint8_t payload[PAYLOAD_SIZE];
} usb_action_t;

typedef struct __attribute__((packed)) {
	char layer_name[MAX_LAYER_NAME];
	char binding_names[NUM_KEYS + 2 * (NUM_ENCODERS - 1)][MAX_BINDING_NAME];
	usb_action_t actions[NUM_KEYS + (NUM_ENCODERS - 1)];
} macro_layer_t;

typedef struct __attribute__((packed)) {
	uint32_t magic_number;
	uint8_t total_layers;
	uint8_t reserved[3];
	macro_layer_t layers[];
} mappings_t;
