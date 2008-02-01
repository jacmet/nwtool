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
#include <string.h>
#include <hid.h>

#define NWUSB_PACKETSIZE 64

static HIDInterface *nw_usb_open(unsigned short vid, unsigned short pid)
{
	HIDInterface *hid;
	HIDInterfaceMatcher matcher;
	int ret;

	memset(&matcher, 0, sizeof(matcher));
	matcher.vendor_id = vid;
	matcher.product_id = pid;

	ret = hid_init();
	if (ret) {
		fprintf(stderr, "hid_init error (%d)\n", ret);
		return 0;
	}

	hid = hid_new_HIDInterface();
	if (!hid) {
		fprintf(stderr, "new_HID error\n");
		goto err_new_intf;
	}

	/* todo: somehow parse HID description to figure out correct interface
	   number instead */
	ret = hid_force_open(hid, 1, &matcher, 3);
	if (ret)
		goto err_force_open;

	return hid;

err_force_open:
	fprintf(stderr, "hid_force_open error\n");
	hid_delete_HIDInterface(&hid);

err_new_intf:
	hid_cleanup();

	return 0;
}

static void nw_usb_close(HIDInterface *hid)
{
	hid_close(hid);
	hid_delete_HIDInterface(&hid);
	hid_cleanup();
}

static int nw_usb_send(HIDInterface *hid, void *data, int len)
{
	const int PATH[] = { 0xffa00001, 0xffa00001 };
	char buf[NWUSB_PACKETSIZE];

	if (len > sizeof(buf))
		len = sizeof(buf);

	memcpy(buf, data, len);

	/* output HID report to ep0 */
	return hid_set_output_report(hid, PATH, sizeof(PATH)/sizeof(PATH[0]),
				     buf, sizeof(buf));
}

static int nw_usb_recv(HIDInterface *hid, void *data)
{
	return hid_interrupt_read(hid, 2 | USB_ENDPOINT_IN, data,
				  NWUSB_PACKETSIZE, 1000);
}

static int nw_usb_calibrate(HIDInterface *hid, int enable)
{
	unsigned char buf[] = { 'C', 2, 0x21, enable ? 1 : 0 };

	return nw_usb_send(hid, buf, sizeof(buf));
}

/* unit is 1/100 sec, 0 = no right clicks */
static int nw_usb_set_rightclick_delay(HIDInterface *hid, unsigned char value)
{
	unsigned char buf[] = { 'C', 2, 0x30, value };

	return nw_usb_send(hid, buf, sizeof(buf));
}

/* unit is 1/100 sec, 0 = no double clicks */
static int nw_usb_set_doubleclick_time(HIDInterface *hid, unsigned char value)
{
	unsigned char buf[] = { 'C', 2, 0x31, value };

	return nw_usb_send(hid, buf, sizeof(buf));
}

/* bitmask of modes to be used */
static int nw_usb_set_report_mode(HIDInterface *hid, unsigned char value)
{
	unsigned char buf[] = { 'C', 2, 0x32, value };

	return nw_usb_send(hid, buf, sizeof(buf));
}

static int nw_usb_set_drag_threshold(HIDInterface *hid, unsigned short value)
{
	unsigned char buf[] = { 'C', 3, 0x33, value>>8, value&0xff };

	return nw_usb_send(hid, buf, sizeof(buf));
}

/* unit is 1/100 sec, 0 = disabled */
static int nw_usb_set_buzzer_time(HIDInterface *hid, unsigned char value)
{
	unsigned char buf[] = { 'C', 2, 0x34, value };

	return nw_usb_send(hid, buf, sizeof(buf));
}

/* lower values means higher tones */
static int nw_usb_set_buzzer_tone(HIDInterface *hid, unsigned char value)
{
	unsigned char buf[] = { 'C', 2, 0x35, value };

	return nw_usb_send(hid, buf, sizeof(buf));
}

/* 0 = disabled */
static int nw_usb_set_calibration_key(HIDInterface *hid, unsigned char key)
{
	unsigned char buf[] = { 'C', 2, 0x40, key };

	return nw_usb_send(hid, buf, sizeof(buf));
}

static int nw_usb_set_calibration_presses(HIDInterface *hid, unsigned char n)
{
	unsigned char buf[] = { 'C', 2, 0x41, n };

	return nw_usb_send(hid, buf, sizeof(buf));
}

static int nw_usb_hard_reset(HIDInterface *hid)
{
	unsigned char buf[] = { 'T', 1, 'R' };

	return nw_usb_send(hid, buf, sizeof(buf));
}

static int nw_usb_restore_factory_defaults(HIDInterface *hid)
{
	unsigned char buf[] = { 'T', 1, 'L' };

	return nw_usb_send(hid, buf, sizeof(buf));
}

static void nw_usb_parse(unsigned char *buf)
{
	unsigned short data16;
	unsigned int data32;

	data16 = (buf[3] << 8) + buf[4];
	data32 = (buf[3] << 24) + (buf[4] << 16) + (buf[5] << 8) + buf[6];

	switch (buf[0]) {
	case 'C':
		switch (buf[2]) {
		case 0x10:
			printf("Model = %u\n", data16);
			break;
		case 0x11:
			printf("Firmware = %u.%02u\n", buf[3], buf[4]);
			break;
		case 0x12:
			printf("Serial = %u\n", data32);
			break;
		case 0x20:
			printf("HW caps = 0x%02x\n", buf[3]);
			break;
		case 0x21:
			printf("Calibration mode = %u\n", buf[3]);
			break;
		case 0x30:
			printf("Right click delay = %u ms\n", buf[3] * 10);
			break;
		case 0x31:
			printf("Double click time = %u ms\n", buf[3] * 10);
			break;
		case 0x32:
			printf("Report mode = 0x%02x\n", buf[3]);
			break;
		case 0x33:
			printf("Drag threshold = %u\n", data16);
			break;
		case 0x34:
			printf("Buzzer time = %u ms\n", buf[3] * 10);
			break;
		case 0x35:
			printf("Buzzer tone = %u\n", buf[3]);
			break;
		case 0x40:
			printf("Calibration key = %u\n", buf[3]);
			break;
		case 0x41:
			printf("Calibration presses = %u\n", buf[3]);
			break;
		default:
			printf("unknown 'C' packet\n");
		}
		break;

	default:
		printf("Unknown packet (%u)\n", buf[0]);
		break;
	}
}

static void nw_usb_process(HIDInterface *hid)
{
	unsigned char buf[NWUSB_PACKETSIZE];

	if (!nw_usb_recv(hid, buf)) {
		int i;
		printf("Got packet: ");
		for (i=0; i<10; i++)
			printf("%02x ", buf[i]);
		printf("\n");
		nw_usb_parse(buf);
	}
}

static int nw_usb_get_model(HIDInterface *hid)
{
	unsigned char buf[] = { 'C', 1, 0x10 };
	int ret;

	ret = nw_usb_send(hid, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_process(hid);
	return 0;
}

static int nw_usb_get_firmware(HIDInterface *hid)
{
	unsigned char buf[] = { 'C', 1, 0x11 };
	int ret;

	ret = nw_usb_send(hid, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_process(hid);
	return 0;
}

static int nw_usb_get_serial(HIDInterface *hid)
{
	unsigned char buf[] = { 'C', 1, 0x12 };
	int ret;

	ret = nw_usb_send(hid, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_process(hid);
	return 0;
}

static int nw_usb_get_calibration_key(HIDInterface *hid)
{
	unsigned char buf[] = { 'C', 1, 0x40 };
	int ret;

	ret = nw_usb_send(hid, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_process(hid);
	return 0;
}

static int nw_usb_get_calibration_presses(HIDInterface *hid)
{
	unsigned char buf[] = { 'C', 1, 0x41 };
	int ret;

	ret = nw_usb_send(hid, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_process(hid);
	return 0;
}

void nw_usb_test(void)
{
	HIDInterface *hid;
	int ret;

	hid = nw_usb_open(0x1926, 0x0003);
	if (!hid) {
		fprintf(stderr, "Error opening device\n");
		return;
	}
#if 0
	ret = nw_usb_calibrate(hid, 0);
	if (ret) {
		fprintf(stderr, "Error sending calibrate cmd (%d)\n", ret);
	}
#endif
	nw_usb_get_model(hid);
	nw_usb_get_firmware(hid);
	nw_usb_get_serial(hid);
	nw_usb_get_calibration_key(hid);
	nw_usb_get_calibration_presses(hid);

	ret = nw_usb_set_buzzer_time(hid, 50);
	if (ret) {
		fprintf(stderr, "Error setting buzzer time (%d)\n", ret);
	}

	nw_usb_process(hid);
	nw_usb_process(hid);
	nw_usb_process(hid);

	ret = nw_usb_set_buzzer_tone(hid, 15);
	if (ret) {
		fprintf(stderr, "Error setting buzzer tone (%d)\n", ret);
	}

	nw_usb_process(hid);
	nw_usb_process(hid);
	nw_usb_process(hid);

	nw_usb_close(hid);
}
