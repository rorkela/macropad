#include <stdlib.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/hid.h>
#include <libopencm3/usb/dfu.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/flash.h>

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
	systick_set_reload(99999);
	systick_interrupt_enable();
	systick_counter_enable();
}

int main(void)
{
	rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_HSI_48MHZ]);

	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	gpio_clear(GPIOA, GPIO12);
	for (unsigned i = 0; i < 800000; i++) {
		__asm__("nop");
	}

	usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config, usb_strings, 3, usbd_control_buffer, sizeof(usbd_control_buffer));
	usbd_register_set_config_callback(usbd_dev, set_config);

	while (1)
		usbd_poll(usbd_dev);
}

void sys_tick_handler(void)
{
	static uint16_t ticks = 0;
	uint8_t report1[14] = {0};
	uint8_t report2[1] = {0};

	ticks++;

	if(ticks > 1) {
		ticks = 0;

		report1[1] = 0xF0;
		report1[2] = 0xFF;
		report1[3] = 0xFF;
		report1[4] = 0x3F;
		report1[6] = (1 << 0x04);
		report2[0] = 0x00;

		usbd_ep_write_packet(usbd_dev, 0x81, report1, 14);
		usbd_ep_write_packet(usbd_dev, 0x82, report2, 1);
		return;
	}
	report1[1] = 0x00;
	report2[0] = 0x00;
	usbd_ep_write_packet(usbd_dev, 0x81, report1, 14);
	usbd_ep_write_packet(usbd_dev, 0x82, report2, 1);
}
