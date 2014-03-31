/*
 * DVB USB Linux driver for Anysee E30 DVB-C & DVB-T USB2.0 receiver
 *
 * Copyright (C) 2007 Antti Palosaari <crope@iki.fi>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * TODO:
 * - add smart card reader support for Conditional Access (CA)
 *
 * Card reader in Anysee is nothing more than ISO 7816 card reader.
 * There is no hardware CAM in any Anysee device sold.
 * In my understanding it should be implemented by making own module
 * for ISO 7816 card reader, like dvb_ca_en50221 is implemented. This
 * module registers serial interface that can be used to communicate
 * with any ISO 7816 smart card.
 *
 * Any help according to implement serial smart card reader support
 * is highly welcome!
 */

#ifndef _DVB_USB_ANYSEE_H_
#define _DVB_USB_ANYSEE_H_

#define DVB_USB_LOG_PREFIX "anysee"
#include "dvb-usb.h"
#include "dvb_ca_en50221.h"

#define deb_info(args...) dprintk(dvb_usb_anysee_debug, 0x01, args)
#define deb_xfer(args...) dprintk(dvb_usb_anysee_debug, 0x02, args)
#define deb_rc(args...)   dprintk(dvb_usb_anysee_debug, 0x04, args)
#define deb_reg(args...)  dprintk(dvb_usb_anysee_debug, 0x08, args)
#define deb_i2c(args...)  dprintk(dvb_usb_anysee_debug, 0x10, args)
#define deb_fw(args...)   dprintk(dvb_usb_anysee_debug, 0x20, args)

enum cmd {
	CMD_I2C_READ            = 0x33,
	CMD_I2C_WRITE           = 0x31,
	CMD_REG_READ            = 0xb0,
	CMD_REG_WRITE           = 0xb1,
	CMD_STREAMING_CTRL      = 0x12,
	CMD_LED_AND_IR_CTRL     = 0x16,
	CMD_GET_IR_CODE         = 0x41,
	CMD_GET_HW_INFO         = 0x19,
	CMD_SMARTCARD           = 0x34,
	CMD_CI                  = 0x37,
};

struct anysee_state {
	u8 hw; 
	u8 seq;
	u8 fe_id:1; 
	u8 has_ci:1;
	struct dvb_ca_en50221 ci;
	unsigned long ci_cam_ready; 
};

#define ANYSEE_HW_507T    2 
#define ANYSEE_HW_507CD   6 
#define ANYSEE_HW_507DC  10 
#define ANYSEE_HW_507SI  11 
#define ANYSEE_HW_507FA  15 
#define ANYSEE_HW_508TC  18 
#define ANYSEE_HW_508S2  19 
#define ANYSEE_HW_508T2C 20 
#define ANYSEE_HW_508PTC 21 
#define ANYSEE_HW_508PS2 22 

#define REG_IOA       0x80 
#define REG_IOB       0x90 
#define REG_IOC       0xa0 
#define REG_IOD       0xb0 
#define REG_IOE       0xb1 
#define REG_OEA       0xb2 
#define REG_OEB       0xb3 
#define REG_OEC       0xb4 
#define REG_OED       0xb5 
#define REG_OEE       0xb6 

#endif

