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
#include <getopt.h>

void nw_usb_test(void);
int nw_serial_forward(char *dev);

int main(int argc, char **argv)
{
	static struct option options[] = {
		{ "usb",		no_argument,		0, 'u' },
		{ "serial",		required_argument,	0, 's' },
		{ "info",		no_argument,	 	0, 'i' },
		{ "rightclick",		required_argument, 	0, 'r' },
		{ "doubleclick",	required_argument,	0, 'd' },
		{ "drag-threshold",	required_argument,	0, 'D' },
		{ "report-mode",	required_argument,	0, 'm' },
		{ "buzzer-time",	required_argument,	0, 'b' },
		{ "buzzer-tone",	required_argument,	0, 't' },
		{ "calibration-key",	required_argument,	0, 'k' },
		{ "calibration-presses", required_argument,	0, 'p' },
		{ "forward", 		no_argument,		0, 'f' },
		{ "calibrate",		no_argument,		0, 'c' },
		{ 0, 0, 0, 0 }
	};
	int c;

	do {
		int c;

		c = getopt_long(argc, argv, "us:ir:d:D:m:b:t:k:p:fc",
				options, 0);

		switch (c) {
		case 'u':
		case 's':
		case 'i':
		case 'r':
		case 'd':
		case 'D':
		case 'm':
		case 'b':
		case 't':
		case 'k':
		case 'p':
		case 'f':
		case 'c':
			break;

		case -1:
			break;

		default:
			fprintf(stderr, "Usage:\n");
			break;
		}

	} while (c != -1);
#ifdef WITH_USB
	nw_usb_test();
#endif
//	nw_serial_forward("/dev/ttyUSB0");
	return 0;
}

