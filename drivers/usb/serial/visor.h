/*
 * USB HandSpring Visor driver
 *
 *	Copyright (C) 1999 - 2003
 *	    Greg Kroah-Hartman (greg@kroah.com)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 * See Documentation/usb/usb-serial.txt for more information on using this
 * driver.
 *
 */

#ifndef __LINUX_USB_SERIAL_VISOR_H
#define __LINUX_USB_SERIAL_VISOR_H


#define HANDSPRING_VENDOR_ID		0x082d
#define HANDSPRING_VISOR_ID		0x0100
#define HANDSPRING_TREO_ID		0x0200
#define HANDSPRING_TREO600_ID		0x0300

#define PALM_VENDOR_ID			0x0830
#define PALM_M500_ID			0x0001
#define PALM_M505_ID			0x0002
#define PALM_M515_ID			0x0003
#define PALM_I705_ID			0x0020
#define PALM_M125_ID			0x0040
#define PALM_M130_ID			0x0050
#define PALM_TUNGSTEN_T_ID		0x0060
#define PALM_TREO_650			0x0061
#define PALM_TUNGSTEN_Z_ID		0x0031
#define PALM_ZIRE_ID			0x0070
#define PALM_M100_ID			0x0080

#define GSPDA_VENDOR_ID		0x115e
#define GSPDA_XPLORE_M68_ID		0xf100

#define SONY_VENDOR_ID			0x054C
#define SONY_CLIE_3_5_ID		0x0038
#define SONY_CLIE_4_0_ID		0x0066
#define SONY_CLIE_S360_ID		0x0095
#define SONY_CLIE_4_1_ID		0x009A
#define SONY_CLIE_NX60_ID		0x00DA
#define SONY_CLIE_NZ90V_ID		0x00E9
#define SONY_CLIE_UX50_ID		0x0144
#define SONY_CLIE_TJ25_ID		0x0169

#define ACER_VENDOR_ID			0x0502
#define ACER_S10_ID			0x0001

#define SAMSUNG_VENDOR_ID		0x04E8
#define SAMSUNG_SCH_I330_ID		0x8001
#define SAMSUNG_SPH_I500_ID		0x6601

#define TAPWAVE_VENDOR_ID		0x12EF
#define TAPWAVE_ZODIAC_ID		0x0100

#define GARMIN_VENDOR_ID		0x091E
#define GARMIN_IQUE_3600_ID		0x0004

#define ACEECA_VENDOR_ID		0x4766
#define ACEECA_MEZ1000_ID		0x0001

#define KYOCERA_VENDOR_ID		0x0C88
#define KYOCERA_7135_ID			0x0021

#define FOSSIL_VENDOR_ID		0x0E67
#define FOSSIL_ABACUS_ID		0x0002


#define VISOR_REQUEST_BYTES_AVAILABLE		0x01

#define VISOR_CLOSE_NOTIFICATION		0x02

#define VISOR_GET_CONNECTION_INFORMATION	0x03


struct visor_connection_info {
	__le16	num_ports;
	struct {
		__u8	port_function_id;
		__u8	port;
	} connections[2];
};


#define VISOR_ENDPOINT_1		0x01
#define VISOR_ENDPOINT_2		0x02

#define VISOR_FUNCTION_GENERIC		0x00
#define VISOR_FUNCTION_DEBUGGER		0x01
#define VISOR_FUNCTION_HOTSYNC		0x02
#define VISOR_FUNCTION_CONSOLE		0x03
#define VISOR_FUNCTION_REMOTE_FILE_SYS	0x04


#define PALM_GET_EXT_CONNECTION_INFORMATION	0x04

struct palm_ext_connection_info {
	__u8 num_ports;
	__u8 endpoint_numbers_different;
	__le16 reserved1;
	struct {
		__u32 port_function_id;
		__u8 port;
		__u8 end_point_info;
		__le16 reserved;
	} connections[2];
};

#endif

