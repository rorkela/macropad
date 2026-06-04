#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/hid.h>
#include <libopencm3/usb/dfu.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/timer.h>

typedef struct {
	struct usb_hid_descriptor hid_descriptor;
	struct {
		uint8_t bReportDescriptorType;
		uint16_t wDescriptorLength;
	} __attribute__((packed)) hid_report;
} __attribute__((packed)) hid_function_t;

#define HID_FUNCTION(report_len) {							\
	.hid_descriptor = {										\
		.bLength = sizeof(hid_function_t),					\
		.bDescriptorType = USB_DT_HID,						\
		.bcdHID = 0x0111,									\
		.bCountryCode = 0,									\
		.bNumDescriptors = 1,								\
	},														\
	.hid_report = {											\
		.bReportDescriptorType = USB_DT_REPORT,				\
		.wDescriptorLength = report_len,					\
	}}

static const uint8_t poll_interval = 0x0a;

static usbd_device *usbd_dev;

const struct usb_device_descriptor dev_descr = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0x1209,
	.idProduct = 0x1253,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

static const uint8_t keyboard_report_descr[] = {
	0x05, 0x01,        // Usage Page (Generic Desktop)
	0x09, 0x06,        // Usage (Keyboard)
	0xA1, 0x01,        // Collection (Application)

	// Modifiers
	0x75, 0x01,        //   Report Size (1)
	0x95, 0x08,        //   Report Count (8)
	0x05, 0x07,        //   Usage Page (Keyboard)
	0x19, 0xe0,        //   Usage Minimum (224)
	0x29, 0xe7,        //   Usage Maximum (231)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x01,        //   Logical Maximum (1)
	0x81, 0x02,        //   Input (Data,Var,Abs)

	// Keys
	0x75, 0x01,        //   Report Size (1)
	0x95, 0x68,        //   Report Count (104)
	0x05, 0x07,        //   Usage Page (Keyboard)
	0x19, 0x00,        //   Usage Minimum (0)
	0x29, 0x67,        //   Usage Maximum (103)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x01,		   //   Logical Maximum (1)
	0x81, 0x02,        //   Input (Data,Ary,Abs)

	0xc0               // End Collection
};

static const uint8_t cntrl_report_descr[] = {
	0x05, 0x0C,        // Usage Page (Consumer)
	0x09, 0x01,        // Usage (Consumer control)
	0xA1, 0x01,        // Collection (Application)

	0x75, 0x08,        //   Report Size (1)
	0x95, 0x01,        //   Report Count (2)
	0x05, 0x0C,        //   Usage Page (Consumer)
	0x19, 0x00,        //   Usage Minimum (0)
	0x29, 0xFF,        //   Usage Maximum (255)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0xFF,		   //   Logical Maximum (255)
	0x81, 0x00,        //   Input (Data,Ary,Abs)

	0xc0               // End Collection
};

static const hid_function_t keyboard_function = HID_FUNCTION(sizeof(keyboard_report_descr));

const struct usb_endpoint_descriptor hid_endpoint = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_ENDPOINT_ADDR_IN(0x01),
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 16,
	.bInterval = poll_interval,
};

const struct usb_interface_descriptor hid_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_HID,
	.bInterfaceSubClass = USB_HID_SUBCLASS_BOOT_INTERFACE,
	.bInterfaceProtocol = USB_HID_INTERFACE_PROTOCOL_KEYBOARD,
	.iInterface = 0,

	.endpoint = &hid_endpoint,

	.extra = &keyboard_function,
	.extralen = sizeof(keyboard_function),
};

static const hid_function_t cntrl_function = HID_FUNCTION(sizeof(cntrl_report_descr));

const struct usb_endpoint_descriptor cntrl_endpoint = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_ENDPOINT_ADDR_IN(0x02),
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 8,
	.bInterval = poll_interval,
};

const struct usb_interface_descriptor cntrl_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_HID,
	.bInterfaceSubClass = USB_HID_SUBCLASS_NO,
	.bInterfaceProtocol = USB_HID_INTERFACE_PROTOCOL_NONE,
	.iInterface = 0,
	.endpoint = &cntrl_endpoint,

	.extra = &cntrl_function,
	.extralen = sizeof(cntrl_function),
};

static const struct usb_dfu_descriptor dfu_func_runtime = {
	.bLength = sizeof(struct usb_dfu_descriptor),
	.bDescriptorType = DFU_FUNCTIONAL,
	.bmAttributes = USB_DFU_CAN_DOWNLOAD | USB_DFU_WILL_DETACH,
	.wDetachTimeout = 255,
	.wTransferSize = 1024,
	.bcdDFUVersion = 0x011A,
};

const struct usb_interface_descriptor dfu_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 2,
	.bAlternateSetting = 0,
	.bNumEndpoints = 0,
	.bInterfaceClass = USB_CLASS_DFU,
	.bInterfaceSubClass = 1,
	.bInterfaceProtocol = 1,
	.iInterface = 0,
	.endpoint = NULL,

	.extra = &dfu_func_runtime,
	.extralen = sizeof(dfu_func_runtime),
};

const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = &hid_iface,
},
{
	.num_altsetting = 1,
	.altsetting = &cntrl_iface,
},
{
	.num_altsetting = 1,
	.altsetting = &dfu_iface,
}};

const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0, // Auto calculate
	.bNumInterfaces = 3,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = USB_CONFIG_ATTR_DEFAULT | USB_CONFIG_ATTR_SELF_POWERED,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

static const char *usb_strings[] = {
	"oiSOIJDF",
	"HID Demo",
	"DEMO",
};

uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes control_request(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
			void (**complete)(usbd_device *, struct usb_setup_data *))
{
	(void)complete;
	(void)dev;

	if((req->bmRequestType != (USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE)) ||
	   (req->bRequest != USB_REQ_GET_DESCRIPTOR) ||
	   (req->wValue != (USB_DT_REPORT << 8)))
		return USBD_REQ_NOTSUPP;

	if(req->wIndex == 0) {
		*buf = (uint8_t *)keyboard_report_descr;
		*len = sizeof(keyboard_report_descr);
	}
	else if(req->wIndex == 1) {
		*buf = (uint8_t *)cntrl_report_descr;
		*len = sizeof(cntrl_report_descr);
	}
	else {
		return USBD_REQ_NOTSUPP;
	}

	return USBD_REQ_HANDLED;
}

#define DFU_FLAG_PAGE 0x08010000

typedef enum {
	SW_1,
	SW_2,
	SW_3,
	SW_4,
	SW_5,
	SW_6,
	SW_7,
	SW_8,
	SW_9,
} switch_id_t;

typedef struct {
	uint32_t port;
	uint16_t pin;
	bool is_pressed;
	uint8_t counter;
	uint8_t keycode;
} switch_t;

#define NUM_SWITCHES 9

static switch_t switches[NUM_SWITCHES] = {
	[SW_1] = {GPIOB, GPIO3, false, 0, 0x04},
	[SW_2] = {GPIOB, GPIO4, false, 0, 0x05},
	[SW_3] = {GPIOB, GPIO5, false, 0, 0x06},
	[SW_4] = {GPIOB, GPIO8, false, 0, 0x07},
	[SW_5] = {GPIOB, GPIO9, false, 0, 0x08},
	[SW_6] = {GPIOB, GPIO11, false, 0, 0x09},
	[SW_7] = {GPIOB, GPIO12, false, 0, 0x0a},
	[SW_8] = {GPIOB, GPIO13, false, 0, 0x0b},
	[SW_9] = {GPIOB, GPIO14, false, 0, 0x0c},
};

static switch_t enc_buttons[2] = {
	{GPIOA, GPIO10, false, 0, 0x0d},
	{GPIOA, GPIO2, false, 0, 0x0e}
};

static void dfu_detach_complete(usbd_device *dev, struct usb_setup_data *req)
{
	(void)req;
	(void)dev;

	usbd_disconnect(dev, 1);

	flash_unlock();
	uint16_t retain = 0x0000;
	flash_program_half_word(DFU_FLAG_PAGE, retain);
	flash_lock();

	scb_reset_system();
}

static enum usbd_request_return_codes dfu_control_request(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
			void (**complete)(usbd_device *, struct usb_setup_data *))
{
	(void)buf;
	(void)len;
	(void)dev;

	if ((req->bmRequestType &0x60) != USB_REQ_TYPE_CLASS || (req->bRequest != DFU_DETACH))
		return USBD_REQ_NOTSUPP;

	*complete = dfu_detach_complete;

	return USBD_REQ_HANDLED;
}

static void set_config(usbd_device *dev, uint16_t wValue)
{
	(void)wValue;
	(void)dev;

	usbd_ep_setup(dev, 0x81, USB_ENDPOINT_ATTR_INTERRUPT, 14, NULL);
	usbd_ep_setup(dev, 0x82, USB_ENDPOINT_ATTR_INTERRUPT, 1, NULL);

	usbd_register_control_callback(
				dev,
				USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				control_request);

	usbd_register_control_callback(
				dev,
				USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				dfu_control_request);

	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
	systick_set_reload(8999);
	systick_interrupt_enable();
	systick_counter_enable();
}

static uint16_t last_enc1_value = 0;
static uint16_t last_enc2_value = 0;

void encoder_init() {
	rcc_periph_clock_enable(RCC_GPIOA);

	rcc_periph_clock_enable(RCC_TIM1);
	rcc_periph_clock_enable(RCC_TIM2);

	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO0 | GPIO1);
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO8 | GPIO9);

	timer_set_period(TIM1, 0xFFFF);

	timer_ic_set_filter(TIM1, TIM_IC1, TIM_IC_CK_INT_N_8);
	timer_ic_set_filter(TIM1, TIM_IC2, TIM_IC_CK_INT_N_8);

	timer_ic_set_polarity(TIM1, TIM_IC1, TIM_IC_FALLING);
	timer_ic_set_polarity(TIM1, TIM_IC2, TIM_IC_FALLING);

	timer_slave_set_mode(TIM1, TIM_SMCR_SMS_EM3);
	timer_enable_counter(TIM1);

	timer_set_period(TIM2, 0xFFFF);

	timer_ic_set_filter(TIM2, TIM_IC1, TIM_IC_CK_INT_N_8);
	timer_ic_set_filter(TIM2, TIM_IC2, TIM_IC_CK_INT_N_8);

	timer_ic_set_polarity(TIM2, TIM_IC1, TIM_IC_FALLING);
	timer_ic_set_polarity(TIM2, TIM_IC2, TIM_IC_FALLING);

	timer_slave_set_mode(TIM2, TIM_SMCR_SMS_EM3);
	timer_enable_counter(TIM2);

	last_enc1_value = timer_get_counter(TIM1);
	last_enc2_value = timer_get_counter(TIM2);
}

int16_t get_encoder_value(int timer_peripheral) {
	uint16_t current_val = timer_get_counter(timer_peripheral);
	int16_t delta = 0;

	switch (timer_peripheral) {
		case TIM1:
			delta = (int16_t)(current_val - last_enc1_value);
			last_enc1_value = current_val;
			return delta;
		case TIM2:
			delta = (int16_t)(current_val - last_enc2_value);
			last_enc2_value = current_val;
			return delta;
		default:
			return 0;
	}
}

void switches_init(void) {
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);

	for(int i = 0; i < NUM_SWITCHES; i++) {
		gpio_set_mode(switches[i].port, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, switches[i].pin);
		gpio_set(switches[i].port, switches[i].pin);
	}

	for(int i = 0; i < 2; i++) {
		gpio_set_mode(enc_buttons[i].port, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, enc_buttons[i].pin);
	}
}

int main(void)
{
	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	gpio_clear(GPIOA, GPIO12);
	for (unsigned i = 0; i < 800000; i++) {
		__asm__("nop");
	}

	encoder_init();
	switches_init();

	usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config, usb_strings, 3, usbd_control_buffer, sizeof(usbd_control_buffer));
	usbd_register_set_config_callback(usbd_dev, set_config);

	while (1)
		usbd_poll(usbd_dev);
}

#define DEBOUNCE_TIME 10

void sys_tick_handler(void)
{
	uint8_t report1[14] = {0};
	uint8_t report2[1] = {0};

	for(int i = 0; i < NUM_SWITCHES; i++) {
		bool raw = (gpio_get(switches[i].port, switches[i].pin) == 0);

		if(raw != switches[i].is_pressed) {
			switches[i].counter++;
			if(switches[i].counter >= DEBOUNCE_TIME) {
				switches[i].is_pressed = raw;
				switches[i].counter = 0;
			}
		} else {
			switches[i].counter = 0;
		}

		if(switches[i].is_pressed) {
			uint8_t key = switches[i].keycode;
			uint8_t byte = 1 + (key / 8);
			uint8_t bit = key % 8;
			report1[byte] |= (1 << bit);
		}
	}

	for(int i = 0; i < 2; i++) {
		bool raw = (gpio_get(enc_buttons[i].port, enc_buttons[i].pin) == 0);

		if(raw != enc_buttons[i].is_pressed) {
			switches[i].counter++;
			if(enc_buttons[i].counter >= DEBOUNCE_TIME) {
				enc_buttons[i].is_pressed = raw;
				enc_buttons[i].counter = 0;
			}
		} else {
			enc_buttons[i].counter = 0;
		}

		if(enc_buttons[i].is_pressed) {
			uint8_t key = enc_buttons[i].keycode;
			uint8_t byte = 1 + (key / 8);
			uint8_t bit = key % 8;
			report1[byte] |= (1 << bit);
		}
	}

	int16_t enc1_delta = get_encoder_value(TIM1);
	int16_t enc2_delta = get_encoder_value(TIM2);

	if(enc1_delta > 0)      report2[0] = 0xE9;
	else if(enc1_delta < 0) report2[0] = 0xEA;
	else if(enc2_delta > 0) report2[0] = 0xE9;
	else if(enc2_delta < 0) report2[0] = 0xEA;
	else					report2[0] = 0x00;

	static uint8_t prev_report1[14] = {0};
	static uint8_t prev_report2[1] = {0};

	if (memcmp(report1, prev_report1, 14) != 0) {
		usbd_ep_write_packet(usbd_dev, 0x81, report1, 14);
		memcpy(prev_report1, report1, 14);
	}

	if (memcmp(report2, prev_report2, 1) != 0) {
		usbd_ep_write_packet(usbd_dev, 0x82, report2, 1);
		memcpy(prev_report2, report2, 1);
	}
}
