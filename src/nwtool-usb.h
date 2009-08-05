/*
 * nwtool: NextWindow touchscreen utility
 *
 * Copyright (C) 2008 Peter Korsgaard <peter.korsgaard@barco.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef _NWTOOL_USB_H_
#define _NWTOOL_USB_H_

struct nwusb;

struct nwusb *nw_usb_init(int bus_nr, int dev_nr);

void nw_usb_deinit(struct nwusb *nw);

int nw_usb_show_info(struct nwusb *nw);

int nw_usb_set_rightclick_delay(struct nwusb *nw, int ms);

int nw_usb_set_doubleclick_time(struct nwusb *nw, int ms);

int nw_usb_set_drag_threshold(struct nwusb *nw, int value);

int nw_usb_set_report_mode(struct nwusb *nw, int mode);

int nw_usb_set_buzzer_time(struct nwusb *nw, int ms);

int nw_usb_set_buzzer_tone(struct nwusb *nw, int value);

int nw_usb_set_calibration_key(struct nwusb *nw, int key);

int nw_usb_set_calibration_presses(struct nwusb *nw, int value);

int nw_usb_calibrate(struct nwusb *nw, int enable);

#endif /* _NWTOOL_USB_H_ */
