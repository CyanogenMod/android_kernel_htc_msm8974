/*
 * Definitions for MCT (Magic Control Technology) USB-RS232 Converter Driver
 *
 *   Copyright (C) 2000 Wolfgang Grandegger (wolfgang@ces.ch)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 * This driver is for the device MCT USB-RS232 Converter (25 pin, Model No.
 * U232-P25) from Magic Control Technology Corp. (there is also a 9 pin
 * Model No. U232-P9). See http://www.mct.com.tw/products/product_us232.html 
 * for further information. The properties of this device are listed at the end 
 * of this file. This device was used in the Dlink DSB-S25.
 *
 * All of the information about the device was acquired by using SniffUSB
 * on Windows98. The technical details of the reverse engineering are
 * summarized at the end of this file.
 */

#ifndef __LINUX_USB_SERIAL_MCT_U232_H
#define __LINUX_USB_SERIAL_MCT_U232_H

#define MCT_U232_VID	                0x0711	
#define MCT_U232_PID	                0x0210	

#define MCT_U232_SITECOM_PID		0x0230	

#define MCT_U232_DU_H3SP_PID		0x0200	

#define MCT_U232_BELKIN_F5U109_VID	0x050d	
#define MCT_U232_BELKIN_F5U109_PID	0x0109	

#define MCT_U232_SET_REQUEST_TYPE	0x40
#define MCT_U232_GET_REQUEST_TYPE	0xc0

#define MCT_U232_GET_MODEM_STAT_REQUEST	2
#define MCT_U232_GET_MODEM_STAT_SIZE	1

#define MCT_U232_GET_LINE_CTRL_REQUEST	6
#define MCT_U232_GET_LINE_CTRL_SIZE	1

#define MCT_U232_SET_BAUD_RATE_REQUEST	5
#define MCT_U232_SET_BAUD_RATE_SIZE	4

#define MCT_U232_SET_LINE_CTRL_REQUEST	7
#define MCT_U232_SET_LINE_CTRL_SIZE	1

#define MCT_U232_SET_MODEM_CTRL_REQUEST	10
#define MCT_U232_SET_MODEM_CTRL_SIZE	1

#define MCT_U232_SET_UNKNOWN1_REQUEST	11  
#define MCT_U232_SET_UNKNOWN1_SIZE	1

#define MCT_U232_SET_CTS_REQUEST	12
#define MCT_U232_SET_CTS_SIZE		1

#define MCT_U232_MAX_SIZE		4	

static int mct_u232_calculate_baud_rate(struct usb_serial *serial,
					speed_t value, speed_t *result);

#define MCT_U232_SET_BREAK              0x40

#define MCT_U232_PARITY_SPACE		0x38
#define MCT_U232_PARITY_MARK		0x28
#define MCT_U232_PARITY_EVEN		0x18
#define MCT_U232_PARITY_ODD		0x08
#define MCT_U232_PARITY_NONE		0x00

#define MCT_U232_DATA_BITS_5            0x00
#define MCT_U232_DATA_BITS_6            0x01
#define MCT_U232_DATA_BITS_7            0x02
#define MCT_U232_DATA_BITS_8            0x03

#define MCT_U232_STOP_BITS_2            0x04
#define MCT_U232_STOP_BITS_1            0x00

#define MCT_U232_MCR_NONE               0x8     
#define MCT_U232_MCR_RTS                0xa     
#define MCT_U232_MCR_DTR                0x9     

#define MCT_U232_MSR_INDEX              0x0     
#define MCT_U232_MSR_CD                 0x80    
#define MCT_U232_MSR_RI                 0x40    
#define MCT_U232_MSR_DSR                0x20    
#define MCT_U232_MSR_CTS                0x10    
#define MCT_U232_MSR_DCD                0x08    
#define MCT_U232_MSR_DRI                0x04    
#define MCT_U232_MSR_DDSR               0x02    
#define MCT_U232_MSR_DCTS               0x01    

#define MCT_U232_LSR_INDEX	1	
#define MCT_U232_LSR_ERR	0x80	
#define MCT_U232_LSR_TEMT	0x40	
#define MCT_U232_LSR_THRE	0x20	
#define MCT_U232_LSR_BI		0x10	
#define MCT_U232_LSR_FE		0x08	
#define MCT_U232_LSR_OE		0x02	
#define MCT_U232_LSR_PE		0x04	
#define MCT_U232_LSR_OE		0x02	
#define MCT_U232_LSR_DR		0x01	



#endif 

