/*
 * nwtool: NextWindow touchscreen utility
 *
 * Copyright (C) 2008 Peter Korsgaard <peter.korsgaard@barco.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hid.h>
#include "nwtool-usb.h"

/*#define NWUSB_VERBOSE 1 */

#define NWUSB_PACKETSIZE		64

#define NWUSB_GOT_MODEL			1
#define NWUSB_GOT_FIRMWARE		2
#define NWUSB_GOT_SERIAL		3
#define NWUSB_GOT_HWCAPS		4
#define NWUSB_GOT_RIGHTCLICKDELAY	5
#define NWUSB_GOT_DOUBLECLICKTIME	6
#define NWUSB_GOT_REPORTMODE		7
#define NWUSB_GOT_DRAGTHRESHOLD		8
#define NWUSB_GOT_BUZZERTIME		9
#define NWUSB_GOT_BUZZERTONE		10
#define NWUSB_GOT_CALIBRATIONKEY	11
#define NWUSB_GOT_CALIBRATIONPRESSES	12

struct nwusb {
	HIDInterface *hid;
	int bus_nr;
	int dev_nr;
};

static bool nw_usb_match(struct usb_dev_handle const *usbdev,
			 void *custom, unsigned int len)
{
	struct nwusb *nw = custom;
	struct usb_device const *dev;
	int id;

	dev = usb_device((usb_dev_handle*)usbdev);
	id = strtol(dev->bus->dirname, NULL, 10);
	if (id != nw->bus_nr)
		return 0;

	if (nw->dev_nr != -1) {
		id = strtol(dev->filename, NULL, 10);
		return id == nw->dev_nr;
	} else {
		return 1;
	}
}

static int nw_usb_open(unsigned short vid, unsigned short pid, struct nwusb *nw)
{
	HIDInterfaceMatcher matcher;
	int ret;

	memset(&matcher, 0, sizeof(matcher));
	matcher.vendor_id = vid;
	matcher.product_id = pid;

	if (nw->bus_nr != -1) {
		matcher.matcher_fn = nw_usb_match;
		matcher.custom_data = nw;
	}

	ret = hid_init();
	if (ret) {
		fprintf(stderr, "hid_init error (%d)\n", ret);
		return 0;
	}

	nw->hid = hid_new_HIDInterface();
	if (!nw->hid) {
		fprintf(stderr, "new_HID error\n");
		goto err_new_intf;
	}

	/* todo: somehow parse HID description to figure out correct interface
	   number instead */
	ret = hid_force_open(nw->hid, 1, &matcher, 3);
	if (ret)
		goto err_force_open;

	return 0;

err_force_open:
	hid_delete_HIDInterface(&nw->hid);

err_new_intf:
	hid_cleanup();
	nw->hid = NULL;

	return ret;
}

static void nw_usb_close(HIDInterface *hid)
{
	hid_close(hid);
	hid_delete_HIDInterface(&hid);
	hid_cleanup();
}

static int nw_usb_send(struct nwusb *nw, void *data, int len)
{
	const int PATH[] = { 0xffa00001, 0xffa00001 };
	char buf[NWUSB_PACKETSIZE];

	if (len > sizeof(buf))
		len = sizeof(buf);

	memcpy(buf, data, len);

#ifdef NWUSB_VERBOSE
	{
		int i;
		printf("sending ");
		for (i=0; i<6; i++)
			printf("%02x ", (unsigned char)buf[i]);
		printf("\n");
	}
#endif /* NWUSB_VERBOSE */

	/* output HID report to ep0 */
	return hid_set_output_report(nw->hid, PATH,
				     sizeof(PATH)/sizeof(PATH[0]),
				     buf, sizeof(buf));
}

static int nw_usb_recv(struct nwusb *nw, void *data)
{
	return hid_interrupt_read(nw->hid, 2 | USB_ENDPOINT_IN, data,
				  NWUSB_PACKETSIZE, 1000);
}

static int nw_usb_hard_reset(struct nwusb *nw)
{
	unsigned char buf[] = { 'T', 1, 'R' };

	return nw_usb_send(nw, buf, sizeof(buf));
}

static int nw_usb_restore_factory_defaults(struct nwusb *nw)
{
	unsigned char buf[] = { 'T', 1, 'L' };

	return nw_usb_send(nw, buf, sizeof(buf));
}

static int nw_usb_parse(struct nwusb *nw, unsigned char *buf,
			unsigned int *result)
{
	unsigned short data16;
	unsigned int data32;
	int got;

	data16 = (buf[3] << 8) + buf[4];
	data32 = (buf[3] << 24) + (buf[4] << 16) + (buf[5] << 8) + buf[6];

	if (!result)
		return 0;

	if (buf[0] != 'C') {
		fprintf(stderr, "Unknown packet type (0x%02x)\n", buf[0]);
		return 0;
	}

	switch (buf[2]) {
	case 0x10: got = NWUSB_GOT_MODEL; *result = data16; break;
	case 0x11: got = NWUSB_GOT_FIRMWARE; *result = data16; break;
	case 0x12: got = NWUSB_GOT_SERIAL; *result = data32; break;
	case 0x20: got = NWUSB_GOT_HWCAPS; *result = buf[3]; break;
	case 0x21: break; /* calibration mode */
	case 0x30: got = NWUSB_GOT_RIGHTCLICKDELAY; *result = buf[3]; break;
	case 0x31: got = NWUSB_GOT_DOUBLECLICKTIME; *result = buf[3]; break;
	case 0x32: got = NWUSB_GOT_REPORTMODE; *result = buf[3]; break;
	case 0x33: got = NWUSB_GOT_DRAGTHRESHOLD; *result = data16; break;
	case 0x34: got = NWUSB_GOT_BUZZERTIME; *result = buf[3]; break;
	case 0x35: got = NWUSB_GOT_BUZZERTONE; *result = buf[3]; break;
	case 0x40: got = NWUSB_GOT_CALIBRATIONKEY; *result = buf[3]; break;
	case 0x41: got = NWUSB_GOT_CALIBRATIONPRESSES; *result = buf[3]; break;

	default:
		fprintf(stderr, "unknown 'C' packet (0x%02x)\n", buf[2]);
		got = 0;
	}

	return got;
}

static int nw_usb_poll(struct nwusb *nw, int msg, unsigned int *result)
{
	unsigned char buf[NWUSB_PACKETSIZE];
	int i;

	for (i=0; i<10; i++) {
		if (!nw_usb_recv(nw, buf))
			if (nw_usb_parse(nw, buf, result) == msg)
				return 1;
	}

	return 0;
}

static int nw_usb_get_model(struct nwusb *nw, unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x10 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_MODEL, result);
}

static int nw_usb_get_firmware(struct nwusb *nw, unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x11 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_FIRMWARE, result);
}

static int nw_usb_get_serial(struct nwusb *nw, unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x12 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_SERIAL, result);
}

static int nw_usb_get_hw_caps(struct nwusb *nw, unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x20 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_HWCAPS, result);
}

static int nw_usb_get_rightclick_delay(struct nwusb *nw, unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x30 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_RIGHTCLICKDELAY, result);
}

static int nw_usb_get_doubleclick_time(struct nwusb *nw, unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x31 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_DOUBLECLICKTIME, result);
}

static int nw_usb_get_report_mode(struct nwusb *nw, unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x32 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_REPORTMODE, result);
}

static int nw_usb_get_drag_threshold(struct nwusb *nw, unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x33 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_DRAGTHRESHOLD, result);
}

static int nw_usb_get_buzzer_time(struct nwusb *nw, unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x34 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_BUZZERTIME, result);
}

static int nw_usb_get_buzzer_tone(struct nwusb *nw, unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x35 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_BUZZERTONE, result);
}

static int nw_usb_get_calibration_key(struct nwusb *nw, unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x40 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_CALIBRATIONKEY, result);
}

static int nw_usb_get_calibration_presses(struct nwusb *nw,
					  unsigned int *result)
{
	unsigned char buf[] = { 'C', 1, 0x41 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_CALIBRATIONPRESSES, result);
}

struct nwusb *nw_usb_init(int bus_nr, int dev_nr)
{
	struct nwusb *nw;
	int ret;

	nw = calloc(1, sizeof(struct nwusb));
	if (!nw) {
		perror("malloc");
		return 0;
	}

	nw->bus_nr = bus_nr;
	nw->dev_nr = dev_nr;

	ret = nw_usb_open(0x1926, 0x0001, nw);
	if (ret == HID_RET_DEVICE_NOT_FOUND) {
		ret = nw_usb_open(0x1926, 0x0003, nw);
	}

	switch (ret) {
	case 0:
		break;

	case HID_RET_DEVICE_NOT_FOUND:
		fprintf(stderr, "Error: No touchscreen detected\n");
		break;

	case HID_RET_FAIL_DETACH_DRIVER:
		fprintf(stderr, "Error accessing touchscreen, are you root?\n");
		break;

	default:
		fprintf(stderr, "Error opening device (%d)\n", ret);
		break;
	}

	if (ret) {
		free(nw);
		return 0;
	}

	return nw;
}

void nw_usb_deinit(struct nwusb *nw)
{
	nw_usb_close(nw->hid);
	free(nw);
}

int nw_usb_show_info(struct nwusb *nw)
{
	unsigned int val;

	if (nw_usb_get_firmware(nw, &val))
		printf("Version:\t\t%d.%02d\n", val>>8, val & 0xff);
	else
		fprintf(stderr, "Error reading firmware version\n");

	if (nw_usb_get_serial(nw, &val))
		printf("Serial:\t\t\t%u\n", val);
	else
		fprintf(stderr, "Error reading serial number\n");

	if (nw_usb_get_model(nw, &val))
		printf("Model:\t\t\t%d\n", val);
	else
		fprintf(stderr, "Error reading model\n");

	if (nw_usb_get_hw_caps(nw, &val))
		printf("HW capabilities:\t0x%02x\n", val);
	else
		fprintf(stderr, "Error reading HW capabilities\n");

	if (nw_usb_get_rightclick_delay(nw, &val))
		printf("Rightclick delay:\t%d ms\n", val*10);
	else
		fprintf(stderr, "Error reading rightclick delay\n");

	if (nw_usb_get_doubleclick_time(nw, &val))
		printf("Doubleclick time:\t%d ms\n", val*10);
	else
		fprintf(stderr, "Error reading doubleclick delay\n");

	if (nw_usb_get_report_mode(nw, &val))
		printf("Report mode:\t\t%d\n", val);
	else
		fprintf(stderr, "Error reading report mode\n");

	if (nw_usb_get_drag_threshold(nw, &val))
		printf("Drag threshold:\t\t%d\n", val);
	else
		fprintf(stderr, "Error reading drag threshold\n");

	if (nw_usb_get_buzzer_time(nw, &val))
		printf("Buzzer time:\t\t%d ms\n", val*10);
	else
		fprintf(stderr, "Error reading buzzer time\n");

	if (nw_usb_get_buzzer_tone(nw, &val))
		printf("Buzzer tone:\t\t%d\n", val);
	else
		fprintf(stderr, "Error reading buzzer tone\n");

	if (nw_usb_get_calibration_key(nw, &val))
		printf("Calibration key:\t%d\n", val);
	else
		fprintf(stderr, "Error reading calibration key\n");

	if (nw_usb_get_calibration_presses(nw, &val))
		printf("Calibration presses:\t%d\n", val);
	else
		fprintf(stderr, "Error reading calibration presses\n");

	return 0;
}

int nw_usb_calibrate(struct nwusb *nw, int enable)
{
	unsigned char buf[] = { 'C', 2, 0x21, enable ? 1 : 0 };

	return nw_usb_send(nw, buf, sizeof(buf));
}

/* unit is 1/100 sec, 0 = no right clicks */
int nw_usb_set_rightclick_delay(struct nwusb *nw, int ms)
{
	unsigned char buf[] = { 'C', 2, 0x30, ms/10};

	return nw_usb_send(nw, buf, sizeof(buf));
}

/* unit is 1/100 sec, 0 = no double clicks */
int nw_usb_set_doubleclick_time(struct nwusb *nw, int ms)
{
	unsigned char buf[] = { 'C', 2, 0x31, ms/10 };

	return nw_usb_send(nw, buf, sizeof(buf));
}

int nw_usb_set_drag_threshold(struct nwusb *nw, int value)
{
	unsigned char buf[] = { 'C', 3, 0x33, value>>8, value&0xff };

	return nw_usb_send(nw, buf, sizeof(buf));
}

/* bitmask of modes to be used */
int nw_usb_set_report_mode(struct nwusb *nw, int mode)
{
	unsigned char buf[] = { 'C', 2, 0x32, mode };

	return nw_usb_send(nw, buf, sizeof(buf));
}

/* unit is 1/100 sec, 0 = disabled */
int nw_usb_set_buzzer_time(struct nwusb *nw, int ms)
{
	unsigned char buf[] = { 'C', 2, 0x34, ms/10 };

	return nw_usb_send(nw, buf, sizeof(buf));
}

/* lower values means higher tones */
int nw_usb_set_buzzer_tone(struct nwusb *nw, int value)
{
	unsigned char buf[] = { 'C', 2, 0x35, value };

	return nw_usb_send(nw, buf, sizeof(buf));
}

/* 0 = disabled */
int nw_usb_set_calibration_key(struct nwusb *nw, int key)
{
	unsigned char buf[] = { 'C', 2, 0x40, key };

	return nw_usb_send(nw, buf, sizeof(buf));
}

int nw_usb_set_calibration_presses(struct nwusb *nw, int value)
{
	unsigned char buf[] = { 'C', 2, 0x41, value };

	return nw_usb_send(nw, buf, sizeof(buf));
}
