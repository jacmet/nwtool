/*
 * nwtool: NextWindow touchscreen utility
 *
 * Copyright (C) 2008 Peter Korsgaard <peter.korsgaard@barco.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef _NWTOOL_SERIAL_H_
#define _NWTOOL_SERIAL_H_

struct nwserial;

struct nwserial *nw_serial_init(char *device);

void nw_serial_deinit(struct nwserial *nw);

int nw_serial_show_info(struct nwserial *nw);

int nw_serial_calibrate(struct nwserial *nw, int enable);

int nw_serial_forward(struct nwserial *nw);

#endif /* _NWTOOL_SERIAL_H_ */
