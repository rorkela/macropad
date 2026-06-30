#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "keys.h"

const usb_keycode_t layer0_keys[NUM_KEYS] = {
	KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8
};

const usb_keycode_t layer1_keys[NUM_KEYS] = {
	KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I
};

void set_key_action(usb_action_t* action, uint8_t modifiers, usb_keycode_t hid_keycode) {
	action->usb_type = USB_TYPE_KEY;
	memset(action->payload, 0, PAYLOAD_SIZE);
	action->payload[0] = modifiers;

	uint8_t byte_idx = ((hid_keycode >> 8) & 0xFF) + 1;
	uint8_t val  = hid_keycode & 0xFF;
	action->payload[byte_idx] |= val;
}

void set_consumer_action(usb_action_t* action, usb_consumer_t consumer_code) {
	action->usb_type = USB_TYPE_CNTRL;
	memset(action->payload, 0, PAYLOAD_SIZE);
	action->payload[0] = consumer_code;
}

int main(void) {
	uint8_t num_layers = 2;

	size_t total_bin_size = sizeof(mappings_t) + (sizeof(macro_layer_t) * num_layers);
	mappings_t* map = (mappings_t*)calloc(1, total_bin_size);

	if (!map) {
		fprintf(stderr, "Memory allocation error.\n");
		return 1;
	}

	map->magic_number = MAGIC;
	map->total_layers = num_layers;

	macro_layer_t* layer0 = &map->layers[0];
	strncpy(layer0->layer_name, "LYR1", MAX_LAYER_NAME);

	for(int k = 0; k < NUM_KEYS; k++) {
		snprintf(layer0->binding_names[k], MAX_BINDING_NAME, "%c", '0' + k);
		set_key_action(&layer0->actions[k], 0, layer0_keys[k]);
	}

	strncpy(layer0->binding_names[9], "VOL-", MAX_BINDING_NAME);
	strncpy(layer0->binding_names[10], "VOL+", MAX_BINDING_NAME);
	layer0->actions[9].usb_type = USB_TYPE_CNTRL;
	layer0->actions[9].payload[0] = AUDIO_VOL_DOWN;
	layer0->actions[9].payload[1] = AUDIO_VOL_UP;
	layer0->actions[9].payload[2] = AUDIO_MUTE;

	macro_layer_t* layer1 = &map->layers[1];
	strncpy(layer1->layer_name, "LYR2", MAX_LAYER_NAME);

	for(int k = 0; k < NUM_KEYS; k++) {
		snprintf(layer1->binding_names[k], MAX_BINDING_NAME, "%c", 'A' + k);
		set_key_action(&layer1->actions[k], 0, layer1_keys[k]);
	}

	strncpy(layer1->binding_names[9], "BRT-", MAX_BINDING_NAME);
	strncpy(layer1->binding_names[10], "BRT+", MAX_BINDING_NAME);
	layer1->actions[9].usb_type = USB_TYPE_CNTRL;
	layer1->actions[9].payload[0] = DISPLAY_BRIGHTNESS_DOWN;
	layer1->actions[9].payload[1] = DISPLAY_BRIGHTNESS_UP;
	layer1->actions[9].payload[2] = MEDIA_PLAY_PAUSE;

	FILE* outfile = fopen("mappings.bin", "wb");
	if (!outfile) {
		perror("Failed to create output file");
		free(map);
		return 1;
	}

	size_t written = fwrite(map, 1, total_bin_size, outfile);
	fclose(outfile);

	printf("Successfully compiled layout structure!\n");
	printf("Total Binary Payload Size: %zu bytes\n", written);
	printf("Flash Output Location Target: 0x%08X\n", MAPPINGS_ADDR);

	free(map);
	return 0;
}
