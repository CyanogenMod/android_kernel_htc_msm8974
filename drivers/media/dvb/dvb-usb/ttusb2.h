/* DVB USB compliant linux driver for Technotrend DVB USB boxes and clones
 * (e.g. Pinnacle 400e DVB-S USB2.0).
 *
 * Copyright (c) 2002 Holger Waechtler <holger@convergence.de>
 * Copyright (c) 2003 Felix Domke <tmbinc@elitedvb.net>
 * Copyright (C) 2005-6 Patrick Boettcher <pb@linuxtv.de>
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the Free
 *	Software Foundation, version 2.
 *
 * see Documentation/dvb/README.dvb-usb for more information
 */
#ifndef _DVB_USB_TTUSB2_H_
#define _DVB_USB_TTUSB2_H_


#define CMD_DSP_DOWNLOAD    0x13

#define CMD_DSP_BOOT        0x14

#define CMD_POWER           0x15

#define CMD_LNB             0x16

#define CMD_GET_VERSION     0x17

#define CMD_DISEQC          0x18

#define CMD_PID_ENABLE      0x22

#define CMD_PID_DISABLE     0x23

#define CMD_FILTER_ENABLE   0x24

#define CMD_FILTER_DISABLE  0x25

#define CMD_GET_DSP_VERSION 0x26

#define CMD_I2C_XFER        0x31

#define CMD_I2C_BITRATE     0x32

#endif
