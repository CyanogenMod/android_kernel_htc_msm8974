/*
 *	Copyright (C) 2009 Martin Fuzzey <mfuzzey@gmail.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MX21_USBH
#define __ASM_ARCH_MX21_USBH

enum mx21_usbh_xcvr {
	
	MX21_USBXCVR_TXDIF_RXDIF = 0,
	MX21_USBXCVR_TXDIF_RXSE = 1,
	MX21_USBXCVR_TXSE_RXDIF = 2,
	MX21_USBXCVR_TXSE_RXSE = 3,
};

struct mx21_usbh_platform_data {
	enum mx21_usbh_xcvr host_xcvr; 
	enum mx21_usbh_xcvr otg_xcvr; 
	u16 	enable_host1:1,
		enable_host2:1,
		enable_otg_host:1, 
		host1_xcverless:1, 
		host1_txenoe:1, 
		otg_ext_xcvr:1, 
		unused:10;
};

#endif 
