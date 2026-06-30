#include <stdint.h>
#include <stddef.h>

#include "defs.h"
#include "display.h"
#include "control.h"
#include "usb.h"

const mappings_t* mappings;
const macro_layer_t* current_layer;
static uint8_t current_layer_idx;

static const uint8_t zero[PAYLOAD_SIZE] = {0};

const mappings_t* get_mappings(void) {
	return mappings;
}

const macro_layer_t* get_layer(uint8_t layer) {
	if(layer >= mappings->total_layers)
		return NULL;

	return &mappings->layers[layer];
}

uint8_t get_current_layer_idx(void) {
	return current_layer_idx;
}

uint8_t get_num_layers(void) {
	return mappings->total_layers;
}

const char* get_layer_name(uint8_t layer) {
	if(layer >= mappings->total_layers)
		return NULL;

	return mappings->layers[layer].layer_name;
}

int control_init(void) {
	mappings = (mappings_t*)MAPPINGS_ADDR;

	if(mappings == NULL)
		return -1;

	if(mappings->magic_number != MAGIC)
	{
		mappings = NULL;
		return -2;
	}

	if(mappings->total_layers < 1)
	{
		mappings = NULL;
		return -3;
	}

	current_layer = &mappings->layers[0];
	current_layer_idx = 0;

	display_layer();

	return 0;
}

void control_handle_key_event(uint8_t key, bool pressed)
{
	if(key >= NUM_KEYS)
		return;

	const usb_action_t* action = &current_layer->actions[key];

	const uint8_t* payload;

	if(pressed)
		payload = action->payload;
	else
		payload = zero;

	switch (action->usb_type)
	{
		case USB_TYPE_KEY:
			usb_send_key(payload);
			break;
		case USB_TYPE_CNTRL:
			usb_send_cntrl(payload);
			break;
		default:
			break;
	}
}

void control_handle_enc_press(uint8_t enc, bool pressed)
{
	if(enc >= NUM_ENCODERS)
		return;

	if(enc == 0 && pressed) {
		display_cycle_mode();
		display_layer();

		return;
	}

	const usb_action_t* action = &current_layer->actions[NUM_KEYS + (enc - 1)];

	const uint8_t* payload;

	if(pressed)
		payload = (action->payload + 2);
	else
		payload = zero;

	switch (action->usb_type)
	{
		case USB_TYPE_KEY:
			break;
		case USB_TYPE_CNTRL:
			usb_send_cntrl(payload);
			break;
		default:
			break;
	}
}

void control_handle_enc_rotate(uint8_t enc, int8_t dir)
{
	if(enc >= NUM_ENCODERS)
		return;

	if(enc == 0) {
		if(dir == 1) {
			if(current_layer_idx < (mappings->total_layers - 1))
				current_layer_idx++;
		}
		else if (dir == -1) {
			if(current_layer_idx > 0)
				current_layer_idx--;
		}

		current_layer = &mappings->layers[current_layer_idx];
		display_layer();
		return;
	}

	const usb_action_t* action = &current_layer->actions[NUM_KEYS + (enc - 1)];

	const uint8_t* payload;

	if(dir == 1)
		payload = action->payload;
	else if (dir == -1)
		payload = (action->payload + 1);
	else
		payload = zero;

	switch (action->usb_type)
	{
		case USB_TYPE_KEY:
			break;
		case USB_TYPE_CNTRL:
			usb_send_cntrl(payload);
			break;
		default:
			break;
	}
}
