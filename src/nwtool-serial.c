/*
 * nwtool: NextWindow touchscreen utility
 *
 * Copyright (C) 2008 Peter Korsgaard <peter.korsgaard@barco.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include "nwtool-serial.h"

#define NW_SER_BAUDRATE	B115200
#define NW_SER_BUFSIZE	256

struct nwserial {
	int fd;
	struct termios orig_tio;
	unsigned char buf[NW_SER_BUFSIZE];
	int buf_pos;
	int footer_pos;
	uint32_t serial;
	uint32_t version;
	int ufd; /* uinput node */
};

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
		return -1;
	}

	if (ioctl(fd, UI_SET_PHYS, "ttySx"))
		perror("UI_SET_PHYS");

	for (i=0; i< sizeof(ev_bits)/sizeof(ev_bits[0]); i++)
		if (ioctl(fd, UI_SET_EVBIT, ev_bits[i])) {
			perror("UI_SET_EVBIT");
			close(fd);
			return -1;
		}

	for (i=0; i< sizeof(key_bits)/sizeof(key_bits[0]); i++)
		if (ioctl(fd, UI_SET_KEYBIT, key_bits[i])) {
			perror("UI_SET_KEYBIT");
			close(fd);
			return -1;
		}

	for (i=0; i< sizeof(abs_bits)/sizeof(abs_bits[0]); i++)
		if (ioctl(fd, UI_SET_ABSBIT, abs_bits[i])) {
			perror("UI_SET_ABSBIT");
			close(fd);
			return -1;
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
		return -1;
	}

	if (ioctl(fd, UI_DEV_CREATE, 0)) {
		perror("UI_DEV_CREATE");
		close(fd);
		return -1;
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

static void nw_serial_handle_packet(struct nwserial *nw)
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

	memcpy(&xi, nw->buf+0, sizeof(xi));
	xi = ntohl(xi); x = *(float*)&xi;
	memcpy(&yi, nw->buf+4, sizeof(yi));
	yi = ntohl(yi); y = *(float*)&yi;
	type = nw->buf[8];

	switch (type) {
	case 0x75:
		fprintf(stderr, "USB cable connected, please disconnect\n");
		break;

	case 0x73: /* ts info */
		nw->serial  = xi;
		nw->version = yi;
		break;

	case 0x6b: /* calibration status */
		break;

	case 0x00:
	case 0x01:
	case 0x02:
	case 0x0a:
	case 0x0b:
	case 0x0c:
		key = type % 10;
		printf("Action %s LCD, x=%.0f, y=%.0f %s (%u)\n",
		       (type >= 0x0a) ? "outside" : "inside", x, y,
		       key ? (key==2) ? "right" : "left" : "", key);
		if (nw->ufd != -1)
			nw_uinput_action(nw->ufd, (int)x, (int)y, key);
		break;

	default:
		fprintf(stderr, "Unknown packet 0x%02x, x=%u, y=%u\n",
			type, xi, yi);
		break;
	}
}

static int nw_serial_process(struct nwserial *nw)
{
	static const char footer[] = "<END>\r";
	int length;

	length = read(nw->fd, &nw->buf[nw->buf_pos],
		      sizeof(nw->buf) - nw->buf_pos);
	if (length == -1) {
		perror("read");
		return 1;
	}

	while (length) {
		if (nw->buf_pos >= sizeof(nw->buf)) {
			fprintf(stderr, "Overflow, resetting buffer\n");
			nw->buf_pos = nw->footer_pos = 0;
		}

		nw->buf_pos++;

		if (nw->buf[nw->buf_pos-1] == footer[nw->footer_pos]) {
			nw->footer_pos++;

			if (nw->footer_pos == sizeof(footer)-1) {
				nw_serial_handle_packet(nw);
				memmove(nw->buf, &nw->buf[nw->buf_pos],
					sizeof(nw->buf) - nw->buf_pos);
				nw->buf_pos = nw->footer_pos = 0;
			}
		} else {
			nw->footer_pos = 0;
		}

		length--;
	}

	return 0;
}

static int nw_serial_get_info(struct nwserial *nw)
{
	int i;

	nw->serial = nw->version = 0xdeadbeef;

	if (write(nw->fd, "nwgs\r", 5) == -1) {
		perror("write");
		return 1;
	}

	for (i=0; i<10; i++) {
		fd_set fds;
		struct timeval tv;

		FD_ZERO(&fds);
		FD_SET(nw->fd, &fds);

		tv.tv_sec = 0;
		tv.tv_usec = 1000;

		if (select(nw->fd + 1, &fds, 0, 0, &tv) == -1) {
			perror("select");
			return 1;
		}

		if (FD_ISSET(nw->fd, &fds)) {
			if (nw_serial_process(nw))
				return 1;

			if (nw->serial != 0xdeadbeef
			    || nw->version != 0xdeadbeef)
				break;
		}
	}

	return (nw->serial == 0xdeadbeef && nw->version == 0xdeadbeef);
}

struct nwserial *nw_serial_init(char *device)
{
	struct nwserial *nw;
	struct termios tio;
	int fd;

	nw = calloc(1, sizeof(struct nwserial));
	if (!nw) {
		perror("malloc");
		return 0;
	}

	nw->fd = open(device, O_RDWR);
	if (nw->fd == -1) {
		perror(device);
		free(nw);
		return 0;
	}

	tcgetattr(fd, &nw->orig_tio);
	tio = nw->orig_tio;
	tio.c_lflag &= ~(ICANON|ECHO);
	tio.c_iflag &= ~(IXON|ICRNL);
	tio.c_oflag &= ~(ONLCR);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

	cfsetispeed(&tio, NW_SER_BAUDRATE);
	cfsetospeed(&tio, NW_SER_BAUDRATE);

	if (tcsetattr(nw->fd, TCSANOW, &tio)) {
		perror("tcsetattr");
		close(nw->fd);
		free(nw);
		return 0;
	}

	nw->ufd = -1;

	return nw;
}

void nw_serial_deinit(struct nwserial *nw)
{
	tcsetattr(nw->fd, TCSANOW, &nw->orig_tio);
	close(nw->fd);
	free(nw);
}

int nw_serial_show_info(struct nwserial *nw)
{
	if (nw_serial_get_info(nw)) {
		fprintf(stderr, "Error getting info\n");
		return 1;
	}

	printf("Version:\t%u.%02u\nSerial:\t\t%u\n",
	       nw->version>>24, (nw->version>>16)&0xff, nw->serial);

	return 0;
}

int nw_serial_calibrate(struct nwserial *nw, int enable)
{	int n;

	n = write(nw->fd, enable ? "nwk1\r" : "nwk0\r", 5);
	if (n == - 1) {
		perror("write");
		return 1;
	}

	return 0;
}

int nw_serial_forward(struct nwserial *nw)
{
	nw->ufd = nw_uinput_open();

	if (nw->ufd == -1)
		return 1;

	while (1) {
		nw_serial_process(nw);
	}

	nw_uinput_close(nw->ufd);

	return 0;
}

