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
#include "nwtool-usb.h"

#define NWUSB_PACKETSIZE		64

#define NWUSB_GOT_MODEL			0x0001
#define NWUSB_GOT_FIRMWARE		0x0002
#define NWUSB_GOT_SERIAL		0x0004
#define NWUSB_GOT_HWCAPS		0x0008
#define NWUSB_GOT_RIGHTCLICKDELAY	0x0010
#define NWUSB_GOT_DOUBLECLICKTIME	0x0020
#define NWUSB_GOT_REPORTMODE		0x0040
#define NWUSB_GOT_DRAGTHRESHOLD		0x0080
#define NWUSB_GOT_BUZZERTIME		0x0100
#define NWUSB_GOT_BUZZERTONE		0x0200
#define NWUSB_GOT_CALIBRATIONKEY	0x0400
#define NWUSB_GOT_CALIBRATIONPRESSES	0x0800

struct nwusb {
	HIDInterface *hid;
	int got;

	int model;
	int firmware;
	unsigned int serial;
	int hw_caps;
	int rightclick_delay;
	int doubleclick_time;
	int report_mode;
	int drag_threshold;
	int buzzer_time;
	int buzzer_tone;
	int calibration_key;
	int calibration_presses;
};

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

static int nw_usb_send(struct nwusb *nw, void *data, int len)
{
	const int PATH[] = { 0xffa00001, 0xffa00001 };
	char buf[NWUSB_PACKETSIZE];

	if (len > sizeof(buf))
		len = sizeof(buf);

	memcpy(buf, data, len);

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

static void nw_usb_parse(struct nwusb *nw, unsigned char *buf)
{
	unsigned short data16;
	unsigned int data32;

	data16 = (buf[3] << 8) + buf[4];
	data32 = (buf[3] << 24) + (buf[4] << 16) + (buf[5] << 8) + buf[6];

	if (buf[0] != 'C') {
		fprintf(stderr, "Unknown packet type (0x%02x)\n", buf[0]);
		return;
	}

	switch (buf[2]) {
	case 0x10:
		nw->got |= NWUSB_GOT_MODEL; nw->model = data16; break;
	case 0x11:
		nw->got |= NWUSB_GOT_FIRMWARE; nw->firmware = data16; break;
	case 0x12:
		nw->got |= NWUSB_GOT_SERIAL; nw->serial = data32; break;
	case 0x20:
		nw->got |= NWUSB_GOT_HWCAPS; nw->hw_caps = buf[3]; break;
	case 0x21:
		printf("Calibration mode = %u\n", buf[3]);
		break;
	case 0x30:
		nw->got |= NWUSB_GOT_RIGHTCLICKDELAY;
		nw->rightclick_delay = buf[3];
		break;
	case 0x31:
		nw->got |= NWUSB_GOT_DOUBLECLICKTIME;
		nw->doubleclick_time = buf[3];
		break;
	case 0x32:
		nw->got |= NWUSB_GOT_REPORTMODE; nw->report_mode = buf[3];
		break;
	case 0x33:
		nw->got |= NWUSB_GOT_DRAGTHRESHOLD;
		nw->drag_threshold = data16;
		break;
	case 0x34:
		nw->got |= NWUSB_GOT_BUZZERTIME; nw->buzzer_time = buf[3];
		break;
	case 0x35:
		nw->got |= NWUSB_GOT_BUZZERTONE; nw->buzzer_tone = buf[3];
		break;
	case 0x40:
		nw->got |= NWUSB_GOT_CALIBRATIONKEY;
		nw->calibration_key = buf[3];
		break;
	case 0x41:
		nw->got |= NWUSB_GOT_CALIBRATIONPRESSES;
		nw->calibration_presses = buf[3];
		break;
	default:
		printf("unknown 'C' packet (0x%02x)\n", buf[2]);
	}
}

static int nw_usb_poll(struct nwusb *nw, int msg)
{
	unsigned char buf[NWUSB_PACKETSIZE];
	int i;

	nw->got &= ~msg;

	for (i=0; i<10; i++) {
		if (!nw_usb_recv(nw, buf))
			nw_usb_parse(nw, buf);
		if (nw->got & msg)
			return 1;
	}

	return 0;
}

static int nw_usb_get_model(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x10 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_MODEL);
}

static int nw_usb_get_firmware(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x11 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_FIRMWARE);
}

static int nw_usb_get_serial(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x12 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_SERIAL);
}

static int nw_usb_get_hw_caps(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x20 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	return nw_usb_poll(nw, NWUSB_GOT_HWCAPS);
}

static int nw_usb_get_rightclick_delay(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x30 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_poll(nw, NWUSB_GOT_RIGHTCLICKDELAY);
	return 0;
}

static int nw_usb_get_doubleclick_time(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x31 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_poll(nw, NWUSB_GOT_DOUBLECLICKTIME);
	return 0;
}

static int nw_usb_get_report_mode(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x32 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_poll(nw, NWUSB_GOT_REPORTMODE);
	return 0;
}

static int nw_usb_get_drag_threshold(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x33 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_poll(nw, NWUSB_GOT_DRAGTHRESHOLD);
	return 0;
}

static int nw_usb_get_buzzer_time(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x34 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_poll(nw, NWUSB_GOT_BUZZERTIME);
	return 0;
}

static int nw_usb_get_buzzer_tone(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x35 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_poll(nw, NWUSB_GOT_BUZZERTONE);
	return 0;
}

static int nw_usb_get_calibration_key(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x40 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_poll(nw, NWUSB_GOT_CALIBRATIONKEY);
	return 0;
}

static int nw_usb_get_calibration_presses(struct nwusb *nw)
{
	unsigned char buf[] = { 'C', 1, 0x41 };
	int ret;

	ret = nw_usb_send(nw, buf, sizeof(buf));
	if (ret)
		return ret;

	nw_usb_poll(nw, NWUSB_GOT_CALIBRATIONPRESSES);
	return 0;
}

struct nwusb *nw_usb_init(void)
{
	struct nwusb *nw;

	nw = calloc(1, sizeof(struct nwusb));
	if (!nw) {
		perror("malloc");
		return 0;
	}

	nw->hid = nw_usb_open(0x1926, 0x0003);
	if (!nw->hid) {
		fprintf(stderr, "Error opening device\n");
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
	nw_usb_get_model(nw);
	nw_usb_get_firmware(nw);
	nw_usb_get_serial(nw);
	nw_usb_get_hw_caps(nw);
	nw_usb_get_rightclick_delay(nw);
	nw_usb_get_doubleclick_time(nw);
	nw_usb_get_report_mode(nw);
	nw_usb_get_drag_threshold(nw);
	nw_usb_get_buzzer_time(nw);
	nw_usb_get_buzzer_tone(nw);
	nw_usb_get_calibration_key(nw);
	nw_usb_get_calibration_presses(nw);

	printf("Version:\t\t%d.%02d\n", nw->firmware>>8, nw->firmware & 0xff);
	printf("Serial:\t\t\t%u\n", nw->serial);
	printf("Model:\t\t\t%d\n", nw->model);
	printf("HW capabilities:\t0x%02x\n", nw->hw_caps);
	printf("Rightclick delay:\t%d ms\n", nw->rightclick_delay*10);
	printf("Doubleclick time:\t%d ms\n", nw->doubleclick_time*10);
	printf("Report mode:\t\t%d\n", nw->report_mode);
	printf("Drag threshold:\t\t%d\n", nw->drag_threshold);
	printf("Buzzer time:\t\t%d ms\n", nw->buzzer_time*10);
	printf("Buzzer tone:\t\t%d\n", nw->buzzer_tone);
	printf("Calibration key:\t%d\n", nw->calibration_key);
	printf("Calibration presses:\t%d\n", nw->calibration_presses);

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
