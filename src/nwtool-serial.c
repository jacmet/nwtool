/*
 * nwtool: NextWindow touchscreen utility
 *
 * Copyright (C) 2008 Peter Korsgaard <peter.korsgaard@barco.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#define NW_BAUDRATE B115200

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/uinput.h>

static struct termios orig_tio;

static int nw_serial_open(char *dev)
{
	struct termios tio;
	int fd;

	fd = open(dev, O_RDWR);
	if (fd == -1) {
		perror(dev);
		return 0;
	}

	tcgetattr(fd, &orig_tio);
	tio = orig_tio;
	tio.c_lflag &= ~(ICANON|ECHO);
	tio.c_iflag &= ~(IXON|ICRNL);
	tio.c_oflag &= ~(ONLCR);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

	cfsetispeed(&tio, NW_BAUDRATE);
	cfsetospeed(&tio, NW_BAUDRATE);

	if (tcsetattr(fd, TCSANOW, &tio)) {
		perror("tcsetattr");
		close(fd);
		return 0;
	}

	return fd;
}

static void nw_serial_close(int fd)
{
	tcsetattr(fd, TCSANOW, &orig_tio);
	close(fd);
}

static int nw_uinput_open(void)
{
	struct uinput_user_dev uinput;
	static const int ev_bits[] = { EV_SYN, EV_KEY, EV_ABS };
	/* protocol doesn't actually provide touch info, but we pretend to
	   support it anyway as otherwise the input device will get taken by
	   joydev (kernel thinks it's a joystick) */
	static const int key_bits[] = { BTN_LEFT, BTN_RIGHT, BTN_TOUCH };
	static const int abs_bits[] = { ABS_X, ABS_Y };
	int fd, i;

	fd = open("/dev/uinput", O_RDWR);
	if (fd == -1)
		fd = open("/dev/input/uinput", O_RDWR);

	if (fd == -1) {
		perror("/dev/uinput");
		return 0;
	}

	if (ioctl(fd, UI_SET_PHYS, "ttySx"))
		perror("UI_SET_PHYS");

	for (i=0; i< sizeof(ev_bits)/sizeof(ev_bits[0]); i++)
		if (ioctl(fd, UI_SET_EVBIT, ev_bits[i])) {
			perror("UI_SET_EVBIT");
			close(fd);
			return 0;
		}

	for (i=0; i< sizeof(key_bits)/sizeof(key_bits[0]); i++)
		if (ioctl(fd, UI_SET_KEYBIT, key_bits[i])) {
			perror("UI_SET_KEYBIT");
			close(fd);
			return 0;
		}

	for (i=0; i< sizeof(abs_bits)/sizeof(abs_bits[0]); i++)
		if (ioctl(fd, UI_SET_ABSBIT, abs_bits[i])) {
			perror("UI_SET_ABSBIT");
			close(fd);
			return 0;
		}

	memset(&uinput, 0, sizeof(uinput));
	strcpy(uinput.name, "NextWindow");
	uinput.id.bustype = BUS_RS232;
	uinput.id.vendor  = 0;
	uinput.id.product = 0;
	uinput.id.version = 0;
	uinput.absmin[ABS_X]  = uinput.absmin[ABS_Y]  = 0;
	uinput.absmax[ABS_X]  = uinput.absmax[ABS_Y]  = 32767;
	uinput.absfuzz[ABS_X] = uinput.absfuzz[ABS_Y] = 0;
	uinput.absflat[ABS_X] = uinput.absflat[ABS_Y] = 0;

	if (write(fd, &uinput, sizeof(uinput)) != sizeof(uinput)) {
		perror("uinput write");
		close(fd);
		return 0;
	}

	if (ioctl(fd, UI_DEV_CREATE, 0)) {
		perror("UI_DEV_CREATE");
		close(fd);
		return 0;
	}

	return fd;
}

static void nw_uinput_close(int fd)
{
	if (ioctl(fd, UI_DEV_DESTROY, 0))
		perror("UI_DEV_CREATE");

	close(fd);
}

static void nw_uinput_action(int fd, int x, int y, int button)
{
	struct input_event ev[5];
	int i;

	memset(&ev, 0, sizeof(ev));

	ev[0].type  = EV_ABS;
	ev[0].code  = ABS_X;
	ev[0].value = x;

	ev[1].type  = EV_ABS;
	ev[1].code  = ABS_Y;
	ev[1].value = y;

	ev[2].type  = EV_KEY;
	ev[2].code  = BTN_LEFT;
	ev[2].value = (button == 1);

	ev[3].type  = EV_KEY;
	ev[3].code  = BTN_RIGHT;
	ev[3].value = (button == 2);

	ev[4].type  = EV_SYN;
	ev[4].code  = SYN_REPORT;
	ev[4].value = 0;

	/* kernel requires seperate write(2) syscall for each event */
	for (i=0; i<sizeof(ev)/sizeof(ev[0]); i++)
		if (write(fd, &ev[i], sizeof(ev[i])) != sizeof(ev[i]))
			perror("uinput_action");
}

static void nw_handle_packet(void *packet, int fd)
{
	float x, y;
	uint32_t xi, yi;
	unsigned char type, key;

	/* format is:
	   b0..3: X-coordinate, big endian IEEE float
	   b4..7: Y-coordinate, big endian IEEE float
	   b8: signal/type byte
	   b9..14: footer '<END>\r'
	*/

	memcpy(&xi, packet+0, sizeof(xi));
	xi = ntohl(xi); x = *(float*)&xi;
	memcpy(&yi, packet+4, sizeof(yi));
	yi = ntohl(yi); y = *(float*)&yi;
	type = *(unsigned char*)(packet+8);

	switch (type) {
	case 0x75:
		fprintf(stderr, "USB cable connected, please disconnect\n");
		break;

	case 0x73: /* model info */
		printf("Model info, serial=%u, version=%u.%02u, unknown=0x%04x\n",
		       xi, yi>>24, (yi>>16)&0xff, yi&0xffff);
		break;

	case 0x6b: /* calibation status */
		printf("Calibration status: %f\n", y);
		break;

	case 0x00:
	case 0x01:
	case 0x02:
	case 0x0a:
	case 0x0b:
	case 0x0c:
		key = (type >= 0x0a) ? type-0x0a : type;
		printf("Action %s LCD, x=%f, y=%f %s (%u)\n",
		       (type >= 0x0a) ? "outside" : "inside", x, y,
		       key ? (key==2) ? "right" : "left" : "", key);
		nw_uinput_action(fd, (int)x, (int)y, key);
		break;

	default:
		fprintf(stderr, "Unknown packet 0x%02x, x=%u, y=%u\n",
			type, xi, yi);
		break;
	}
}

static char buf[256];
static int buf_pos, footer_pos;

static const char footer[] = "<END>\r";

static void nw_serial_parse(char *data, int length, int fd)
{
	while (length) {
		if (buf_pos >= sizeof(buf)) {
			fprintf(stderr, "Overflow, resetting buffer\n");
			buf_pos = footer_pos = 0;
		}

		buf[buf_pos++] = *data++;

		if (buf[buf_pos-1] == footer[footer_pos]) {
			footer_pos++;

			if (footer_pos == sizeof(footer)-1) {
				nw_handle_packet(buf, fd);
				buf_pos = footer_pos = 0;
			}
		} else {
			footer_pos = 0;
		}

		length--;
	}
}

static int nw_serial_process(int infd, int outfd)
{
	char data[256];
	int n;

	n = read(infd, data, sizeof(data));
	if (n == -1) {
		perror("read");
		return 1;
	}

	nw_serial_parse(data, n, outfd);
	return 0;
}

static int nw_serial_get_info(int infd, int outfd)
{
	int n;

	n = write(infd, "nwgs\r", 5);
	if (n == - 1) {
		perror("write");
		return 1;
	}

	/* todo: loop until reply received or timeout */
	return nw_serial_process(infd, outfd);
}

static int nw_serial_calibrate(int infd, int outfd, int enable)

{	int n;

	n = write(infd, enable ? "nwk1\r" : "nwk0\r", 5);
	if (n == - 1) {
		perror("write");
		return 1;
	}

	/* todo: loop until reply received or timeout */
	return nw_serial_process(infd, outfd);
}
