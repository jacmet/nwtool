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

void nw_usb_test(void);
int nw_serial_forward(char *dev);

int main(void)
{
#ifdef WITH_USB
	nw_usb_test();
#endif
//	nw_serial_forward("/dev/ttyUSB0");
	return 0;
}

