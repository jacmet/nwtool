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
#include "nwtool-usb.h"
#include "nwtool-serial.h"

#define NW_NEED_SERIAL	1
#define NW_NEED_USB	1

static void usage(void)
{
	fprintf(stderr, "usage: nwtool [OPTION] ...\n"
		"  -h, --help\t\t\tshow usage info\n"
		"  -v, --version\t\t\tshow version info\n"
		"  -s, --serial <device>\t\taccess touchscreen over serial\n"
#ifdef WITH_USB
		"  -u[<n>], --usb[=<n>]\t\taccess touchscreen over USB [on bus nr n]\n"
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
		"  -c, --calibrate\t\tput touchscreen in calibration mode\n"
		"  -C, --cancel-calibration\tput touchscreen out of "
		"calibration mode\n");

	exit(1);
}

#ifdef WITH_USB
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
#endif /* WITH_USB */

static void missing(int need)
{
#ifndef WITH_USB
	need &= ~NW_NEED_USB;
#endif /* WITH_USB */

	if (need == (NW_NEED_USB|NW_NEED_SERIAL))
		fprintf(stderr, "-u or -s options required\n");
	else
		fprintf(stderr, "%s option required\n",
			need == NW_NEED_USB ? "-u" : "-s");
	usage();
}

int main(int argc, char **argv)
{
	static const struct option options[] = {
		{ "help",		no_argument,	 	0, 'h' },
		{ "version",		no_argument,	 	0, 'v' },
		{ "serial",		required_argument,	0, 's' },
		{ "usb",		optional_argument,	0, 'u' },
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
		{ "cancel-calibration",	no_argument,		0, 'C' },
		{ 0, 0, 0, 0 }
	};
	int c, usb_bus_nr = -1;
	struct nwusb *usb = 0;
	struct nwserial *ser = 0;

	do {
		c = getopt_long(argc, argv, "hvu::s:ir:d:D:m:b:t:k:p:fcC",
				options, 0);

		switch (c) {
		case 'v':
			printf("nwtool version " VERSION ", (C) 2008 "
			       "Peter Korsgaard <peter.korsgaard@barco.com>\n");
			exit(0);
			break;

		case 's':
			if (usb) {
				fprintf(stderr, "Only one of -u | -s options "
					"allowed\n");
				usage();
			}

			ser = nw_serial_init(optarg);
			if (!ser)
				usage();
			break;

#ifdef WITH_USB
		case 'u':
			if (ser) {
				fprintf(stderr, "Only one of -u | -s options "
					"allowed\n");
				usage();
			}

			if (optarg)
				usb_bus_nr = parse_nr(optarg);

			usb = nw_usb_init(usb_bus_nr);
			if (!usb)
				usage();
			break;
#endif /* WITH_USB */

		case 'i':
			if (ser)
				nw_serial_show_info(ser);
#ifdef WITH_USB
			else if (usb)
				nw_usb_show_info(usb);
#endif /* WITH_USB */
			else
				missing(NW_NEED_USB|NW_NEED_SERIAL);
			break;
#ifdef WITH_USB
		case 'r':
			if (usb)
				nw_usb_set_rightclick_delay(usb,
							    parse_nr(optarg));
			else
				missing(NW_NEED_USB);
			break;

		case 'd':
			if (usb)
				nw_usb_set_doubleclick_time(usb,
							    parse_nr(optarg));
			else
				missing(NW_NEED_USB);
			break;

		case 'D':
			if (usb)
				nw_usb_set_drag_threshold(usb,
							  parse_nr(optarg));
			else
				missing(NW_NEED_USB);
			break;

		case 'm':
			if (usb)
				nw_usb_set_report_mode(usb, parse_nr(optarg));
			else
				missing(NW_NEED_USB);
			break;

		case 'b':
			if (usb)
				nw_usb_set_buzzer_time(usb, parse_nr(optarg));
			else
				missing(NW_NEED_USB);
			break;

		case 't':
			if (usb)
				nw_usb_set_buzzer_tone(usb, parse_nr(optarg));
			else
				missing(NW_NEED_USB);
			break;

		case 'k':
			if (usb)
				nw_usb_set_calibration_key(usb,
							   parse_nr(optarg));
			else
				missing(NW_NEED_USB);
			break;

		case 'p':
			if (usb)
				nw_usb_set_calibration_presses(
					usb, parse_nr(optarg));
			else
				missing(NW_NEED_USB);
			break;

#endif /* WITH_USB */
		case 'f':
			if (ser)
				nw_serial_forward(ser);
			else
				missing(NW_NEED_SERIAL);
			break;

		case 'c':
		case 'C':
			if (ser)
				nw_serial_calibrate(ser, c == 'c');
#ifdef WITH_USB
			else if (usb)
				nw_usb_calibrate(usb, c == 'c');
#endif /* WITH_USB */
			else
				missing(NW_NEED_USB|NW_NEED_SERIAL);
			break;

		case -1:
			break;

		default:
			usage();
			break;
		}

	} while (c != -1);

	if (ser)
		nw_serial_deinit(ser);
#ifdef WITH_USB
	else if (usb)
		nw_usb_deinit(usb);
#endif /* WITH_USB */
	else
		usage();

	return 0;
}

