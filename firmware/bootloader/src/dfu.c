#include <stdint.h>
#include <string.h>
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

#include "dfu.h"
#include "defs.h"

static usbd_device *usbd_dev;

const struct usb_device_descriptor dev_descr = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0100,
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

#define DFU_TRANSFER_SIZE 1024

const struct usb_dfu_descriptor dfu_func = {
	.bLength = sizeof(struct usb_dfu_descriptor),
	.bDescriptorType = DFU_FUNCTIONAL,
	.bmAttributes = USB_DFU_CAN_DOWNLOAD | USB_DFU_WILL_DETACH,
	.wDetachTimeout = 255,
	.wTransferSize = DFU_TRANSFER_SIZE,
	.bcdDFUVersion = 0x0101
};

const struct usb_interface_descriptor app_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 0,
	.bInterfaceClass = USB_CLASS_DFU,
	.bInterfaceSubClass = 0x01,
	.bInterfaceProtocol = 0x02,
	.iInterface = 4,

	.endpoint = NULL,

	.extra = &dfu_func,
	.extralen = sizeof(dfu_func),
};

const struct usb_interface_descriptor mapping_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 1,
	.bNumEndpoints = 0,
	.bInterfaceClass = USB_CLASS_DFU,
	.bInterfaceSubClass = 0x01,
	.bInterfaceProtocol = 0x02,
	.iInterface = 5,

	.endpoint = NULL,

	.extra = &dfu_func,
	.extralen = sizeof(dfu_func),
};

const struct usb_interface_descriptor altsettings_list[] = {
	app_iface, mapping_iface
};

static uint8_t current_altsetting = 0;

const struct usb_interface ifaces_dev[] = {{
	.num_altsetting = 2,
	.cur_altsetting = &current_altsetting,
	.altsetting = altsettings_list,
}};

const struct usb_config_descriptor config_dev = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0, // Auto calculate
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = USB_CONFIG_ATTR_DEFAULT | USB_CONFIG_ATTR_SELF_POWERED,
	.bMaxPower = 0x32,

	.interface = ifaces_dev,
};

#define ADDR_STR(addr) STR(addr)
#define STR(str) #str

static const char *usb_strings[] = {
	"Macropad DFU updater",
	"Macropad",
	"0.1",
	"Application Firmware @ 0x" ADDR_STR(END_ADDR),
	"Key Mappings @ 0x" ADDR_STR(MAPPINGS_ADDR),
};

uint8_t usbd_control_buffer[DFU_TRANSFER_SIZE];

static enum dfu_state dfuState = STATE_DFU_IDLE;
static enum dfu_status dfuStatus = DFU_STATUS_OK;

static struct {
	uint8_t buf[sizeof(usbd_control_buffer)];
	uint16_t len;
	uint16_t blocknum;
} prog;

static void get_status(uint32_t *pollTimeout)
{
	switch(dfuState) {
		case STATE_DFU_DNLOAD_SYNC:
			dfuState = STATE_DFU_DNBUSY;
			*pollTimeout = 100;
			dfuStatus = DFU_STATUS_OK;
			break;
		case STATE_DFU_MANIFEST_SYNC:
			dfuState = STATE_DFU_MANIFEST;
			dfuStatus = DFU_STATUS_OK;
			break;
		default:
			dfuStatus = DFU_STATUS_OK;
	}
}

static void getstatus_complete(usbd_device *dev, struct usb_setup_data *req)
{
	(void)req;
	(void)dev;

	switch (dfuState)
	{
		case STATE_DFU_DNBUSY:
			flash_unlock();

			uint32_t target_start = (current_altsetting == 0)? END_ADDR : MAPPINGS_ADDR;

			uint32_t baseAddr = target_start + (prog.blocknum * dfu_func.wTransferSize);

			for(uint32_t offset = 0; offset < dfu_func.wTransferSize; offset += 1024) {
				flash_erase_page(baseAddr + offset);
			}

			for(int i = 0; i < (prog.len & ~1); i += 2)
			{
				uint16_t data = prog.buf[i] | (prog.buf[i+1] << 8);
				flash_program_half_word(baseAddr + i, data);
			}

			if(prog.len & 1)
			{
				uint16_t data = prog.buf[prog.len - 1] | 0xFF00;
				flash_program_half_word(baseAddr + prog.len - 1, data);
			}

			flash_lock();
			dfuState = STATE_DFU_DNLOAD_IDLE;
			return;
		case STATE_DFU_MANIFEST:
			flash_unlock();
			flash_erase_page(LAST_PAGE);
			flash_lock();

			scb_reset_system();
		default:
			return;
	}
}

static enum usbd_request_return_codes control_request(
		usbd_device *dev,
		struct usb_setup_data *req,
		uint8_t **buf, uint16_t *len,
		void (**complete)(usbd_device *, struct usb_setup_data *))
{
	(void)complete;
	(void)dev;

	if((req->bmRequestType & 0x60) != USB_REQ_TYPE_CLASS)
		return USBD_REQ_NOTSUPP;

	switch (req->bRequest)
	{
		case DFU_DNLOAD:
			if(dfuState != STATE_DFU_IDLE && dfuState != STATE_DFU_DNLOAD_IDLE)
			{
				dfuStatus = DFU_STATUS_ERR_NOTDONE;
				dfuState = STATE_DFU_ERROR;
			}
			else {
				prog.blocknum = req->wValue;
				prog.len = req->wLength;
				if(prog.len == 0) {
					dfuState = STATE_DFU_MANIFEST_SYNC;
				}
				else
				{
					memcpy(prog.buf, *buf, prog.len);
					dfuState = STATE_DFU_DNLOAD_SYNC;
				}
			}
			return USBD_REQ_HANDLED;
		case DFU_GETSTATE:
			(*buf)[0] = dfuState;
			*len = 1;
			return USBD_REQ_HANDLED;
		case DFU_GETSTATUS:
			uint32_t pollInterval = 0;
			get_status(&pollInterval);
			(*buf)[0] = dfuStatus;
			(*buf)[1] = pollInterval & 0xFF;
			(*buf)[2] = (pollInterval >> 8) & 0xFF;
			(*buf)[3] = (pollInterval >> 16) & 0xFF;
			(*buf)[4] = dfuState;
			(*buf)[5] = 0;
			*len = 6;
			*complete = getstatus_complete;
			return USBD_REQ_HANDLED;
		case DFU_CLRSTATUS:
			if(dfuState == STATE_DFU_ERROR)
			{
				dfuStatus = DFU_STATUS_OK;	
				dfuState = STATE_DFU_IDLE;	
			}
			return USBD_REQ_HANDLED;
		case DFU_ABORT:
			dfuStatus = DFU_STATUS_OK;	
			dfuState = STATE_DFU_IDLE;
			return USBD_REQ_HANDLED;
		default:
			return USBD_REQ_NOTSUPP;
	}

	return USBD_REQ_HANDLED;
}

static void set_config(usbd_device *dev, uint16_t wValue)
{
	(void)wValue;
	(void)dev;

	usbd_register_control_callback(
			dev,
			USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
			USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
			control_request);
}

void dfu_init(void) {
	rcc_periph_clock_enable(RCC_GPIOA);

	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	gpio_clear(GPIOA, GPIO12);
	for (unsigned i = 0; i < 800000; i++) __asm__("nop");

	usbd_dev = usbd_init(
			&st_usbfs_v1_usb_driver,
			&dev_descr,
			&config_dev,
			usb_strings,
			5,
			usbd_control_buffer,
			sizeof(usbd_control_buffer));
	usbd_register_set_config_callback(usbd_dev, set_config);
}

void dfu_run(void) {
	while(1)
		usbd_poll(usbd_dev);
}
