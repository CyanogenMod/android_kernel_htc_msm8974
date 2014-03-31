/*
 * Definitions for Belkin USB Serial Adapter Driver
 *
 *  Copyright (C) 2000
 *      William Greathouse (wgreathouse@smva.com)
 *
 *  This program is largely derived from work by the linux-usb group
 *  and associated source files.  Please see the usb/serial files for
 *  individual credits and copyrights.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 * See Documentation/usb/usb-serial.txt for more information on using this
 * driver
 *
 * 12-Mar-2001 gkh
 *	Added GoHubs GO-COM232 device id.
 *
 * 06-Nov-2000 gkh
 *	Added old Belkin and Peracom device ids, which this driver supports
 *
 * 12-Oct-2000 William Greathouse
 *    First cut at supporting Belkin USB Serial Adapter F5U103
 *    I did not have a copy of the original work to support this
 *    adapter, so pardon any stupid mistakes.  All of the information
 *    I am using to write this driver was acquired by using a modified
 *    UsbSnoop on Windows2000.
 *
 */

#ifndef __LINUX_USB_SERIAL_BSA_H
#define __LINUX_USB_SERIAL_BSA_H

#define BELKIN_DOCKSTATION_VID	0x050d	
#define BELKIN_DOCKSTATION_PID	0x1203	

#define BELKIN_SA_VID	0x050d	
#define BELKIN_SA_PID	0x0103	

#define BELKIN_OLD_VID	0x056c	
#define BELKIN_OLD_PID	0x8007	

#define PERACOM_VID	0x0565	
#define PERACOM_PID	0x0001	

#define GOHUBS_VID	0x0921	
#define GOHUBS_PID	0x1000	
#define HANDYLINK_PID	0x1200	

#define BELKIN_SA_SET_BAUDRATE_REQUEST	0  
#define BELKIN_SA_SET_STOP_BITS_REQUEST	1  
#define BELKIN_SA_SET_DATA_BITS_REQUEST	2  
#define BELKIN_SA_SET_PARITY_REQUEST	3  

#define BELKIN_SA_SET_DTR_REQUEST	10 
#define BELKIN_SA_SET_RTS_REQUEST	11 
#define BELKIN_SA_SET_BREAK_REQUEST	12 

#define BELKIN_SA_SET_FLOW_CTRL_REQUEST	16 


#ifdef WHEN_I_LEARN_THIS
#define BELKIN_SA_SET_MAGIC_REQUEST	17 
					   
#define BELKIN_SA_RESET			xx 
#define BELKIN_SA_GET_MODEM_STATUS	xx 
#endif

#define BELKIN_SA_SET_REQUEST_TYPE	0x40

#define BELKIN_SA_BAUD(b)		(230400/b)

#define BELKIN_SA_STOP_BITS(b)		(b-1)

#define BELKIN_SA_DATA_BITS(b)		(b-5)

#define BELKIN_SA_PARITY_NONE		0
#define BELKIN_SA_PARITY_EVEN		1
#define BELKIN_SA_PARITY_ODD		2
#define BELKIN_SA_PARITY_MARK		3
#define BELKIN_SA_PARITY_SPACE		4

#define BELKIN_SA_FLOW_NONE		0x0000	
#define BELKIN_SA_FLOW_OCTS		0x0001	
#define BELKIN_SA_FLOW_ODSR		0x0002	
#define BELKIN_SA_FLOW_IDSR		0x0004	
#define BELKIN_SA_FLOW_IDTR		0x0008	
#define BELKIN_SA_FLOW_IRTS		0x0010	
#define BELKIN_SA_FLOW_ORTS		0x0020	
#define BELKIN_SA_FLOW_ERRSUB		0x0040	
#define BELKIN_SA_FLOW_OXON		0x0080	
#define BELKIN_SA_FLOW_IXON		0x0100	

#define BELKIN_SA_LSR_INDEX		2	
#define BELKIN_SA_LSR_RDR		0x01	
#define BELKIN_SA_LSR_OE		0x02	
#define BELKIN_SA_LSR_PE		0x04	
#define BELKIN_SA_LSR_FE		0x08	
#define BELKIN_SA_LSR_BI		0x10	
#define BELKIN_SA_LSR_THE		0x20	
#define BELKIN_SA_LSR_TE		0x40	
#define BELKIN_SA_LSR_ERR		0x80	

#define BELKIN_SA_MSR_INDEX		3	
#define BELKIN_SA_MSR_DCTS		0x01	
#define BELKIN_SA_MSR_DDSR		0x02	
#define BELKIN_SA_MSR_DRI		0x04	
#define BELKIN_SA_MSR_DCD		0x08	
#define BELKIN_SA_MSR_CTS		0x10	
#define BELKIN_SA_MSR_DSR		0x20	
#define BELKIN_SA_MSR_RI		0x40	
#define BELKIN_SA_MSR_CD		0x80	

#endif 

