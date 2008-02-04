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
#include <getopt.h>

void nw_usb_test(void);
int nw_serial_forward(char *dev);

static void usage(void)
{
	fprintf(stderr, "usage: nwtool [OPTION] ...\n"
		"  -s, --serial <device>\t\taccess touchscreen over serial\n"
#ifdef WITH_USB
		"  -u, --usb\t\t\taccess touchscreen over USB\n"
		"  -i, --info\t\t\tdisplay info and current settings\n"
		"  -r, --rightclick\t\tset rightclick delay to <ms>\n"
		"  -d, --doubleclick\t\tset doubleclick time to <ms>\n"
		"  -D, --drag-threshold\t\tset drag threshold to <value>\n"
		"  -m, --report-mode\t\tset reporting mode to <mode>\n"
		"  -b, --buzzer-time\t\tset buzzer time to <ms>\n"
		"  -t, --buzzer-tone\t\tset buzzer tone to <value>\n"
		"  -k, --calibration-key\t\tset calibration key to <value>\n"
		"  -p, --calibration-presses\tset nr of calibration presses\n"
#endif
		"  -f, --forward\t\t\tforward touchscreen events to kernel\n"
		"  -c, --calibrate\t\tput touchscreen in calibration mode\n");

	exit(1);
}

static int parse_nr(char *arg)
{
	long val;
	char *endp;

	val = strtol(arg, &endp, 0);
	if (*endp) {
		fprintf(stderr, "invalid number '%s'\n", arg);
		usage();
	}

	return val;
}

int main(int argc, char **argv)
{
	static const struct option options[] = {
		{ "serial",		required_argument,	0, 's' },
		{ "usb",		no_argument,		0, 'u' },
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
		c = getopt_long(argc, argv, "us:ir:d:D:m:b:t:k:p:fc",
				options, 0);

		switch (c) {
		case 's':

#ifdef WITH_USB
		case 'u':
		case 'i':
		case 'r':
		case 'd':
		case 'D':
		case 'm':
		case 'b':
		case 't':
		case 'k':
		case 'p':
#endif /* WITH_USB */
		case 'f':
		case 'c':
			break;

		case -1:
			break;

		default:
			usage();
			break;
		}

	} while (c != -1);
#ifdef WITH_USB
	nw_usb_test();
#endif
//	nw_serial_forward("/dev/ttyUSB0");
	return 0;
}

