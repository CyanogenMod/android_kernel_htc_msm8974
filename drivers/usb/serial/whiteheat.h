/*
 * USB ConnectTech WhiteHEAT driver
 *
 *      Copyright (C) 2002
 *          Connect Tech Inc.
 *
 *      Copyright (C) 1999, 2000
 *          Greg Kroah-Hartman (greg@kroah.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 * See Documentation/usb/usb-serial.txt for more information on using this
 * driver
 *
 */

#ifndef __LINUX_USB_SERIAL_WHITEHEAT_H
#define __LINUX_USB_SERIAL_WHITEHEAT_H


#define WHITEHEAT_OPEN			1	
#define WHITEHEAT_CLOSE			2	
#define WHITEHEAT_SETUP_PORT		3	
#define WHITEHEAT_SET_RTS		4	
#define WHITEHEAT_SET_DTR		5	
#define WHITEHEAT_SET_BREAK		6	
#define WHITEHEAT_DUMP			7	
#define WHITEHEAT_STATUS		8	
#define WHITEHEAT_PURGE			9	
#define WHITEHEAT_GET_DTR_RTS		10	
#define WHITEHEAT_GET_HW_INFO		11	
#define WHITEHEAT_REPORT_TX_DONE	12	
#define WHITEHEAT_EVENT			13	
#define WHITEHEAT_ECHO			14	
#define WHITEHEAT_DO_TEST		15	
#define WHITEHEAT_CMD_COMPLETE		16	
#define WHITEHEAT_CMD_FAILURE		17	




struct whiteheat_simple {
	__u8	port;	
};


#define WHITEHEAT_PAR_NONE	'n'	
#define WHITEHEAT_PAR_EVEN	'e'	
#define WHITEHEAT_PAR_ODD	'o'	
#define WHITEHEAT_PAR_SPACE	'0'	
#define WHITEHEAT_PAR_MARK	'1'	

#define WHITEHEAT_SFLOW_NONE	'n'	
#define WHITEHEAT_SFLOW_RX	'r'	
#define WHITEHEAT_SFLOW_TX	't'	
#define WHITEHEAT_SFLOW_RXTX	'b'	

#define WHITEHEAT_HFLOW_NONE		0x00	
#define WHITEHEAT_HFLOW_RTS_TOGGLE	0x01	
#define WHITEHEAT_HFLOW_DTR		0x02	
#define WHITEHEAT_HFLOW_CTS		0x08	
#define WHITEHEAT_HFLOW_DSR		0x10	
#define WHITEHEAT_HFLOW_RTS		0x80	

struct whiteheat_port_settings {
	__u8	port;		
	__u32	baud;		
	__u8	bits;		
	__u8	stop;		
	__u8	parity;		
	__u8	sflow;		
	__u8	xoff;		
	__u8	xon;		
	__u8	hflow;		
	__u8	lloop;		
} __attribute__ ((packed));


#define WHITEHEAT_RTS_OFF	0x00
#define WHITEHEAT_RTS_ON	0x01
#define WHITEHEAT_DTR_OFF	0x00
#define WHITEHEAT_DTR_ON	0x01
#define WHITEHEAT_BREAK_OFF	0x00
#define WHITEHEAT_BREAK_ON	0x01

struct whiteheat_set_rdb {
	__u8	port;		
	__u8	state;		
};


#define WHITEHEAT_DUMP_MEM_DATA		'd'  
#define WHITEHEAT_DUMP_MEM_IDATA	'i'  
#define WHITEHEAT_DUMP_MEM_BDATA	'b'  
#define WHITEHEAT_DUMP_MEM_XDATA	'x'  


struct whiteheat_dump {
	__u8	mem_type;	
	__u16	addr;		
	__u16	length;		
};


#define WHITEHEAT_PURGE_RX	0x01	
#define WHITEHEAT_PURGE_TX	0x02	

struct whiteheat_purge {
	__u8	port;		
	__u8	what;		
};


struct whiteheat_echo {
	__u8	port;		
	__u8	length;		
	__u8	echo_data[61];	
};


#define WHITEHEAT_TEST_UART_RW		0x01  
#define WHITEHEAT_TEST_UART_INTR	0x02  
#define WHITEHEAT_TEST_SETUP_CONT	0x03  
#define WHITEHEAT_TEST_PORT_CONT	0x04  
#define WHITEHEAT_TEST_PORT_DISCONT	0x05  
#define WHITEHEAT_TEST_UART_CLK_START	0x06  
#define WHITEHEAT_TEST_UART_CLK_STOP	0x07  
#define WHITEHEAT_TEST_MODEM_FT		0x08  
#define WHITEHEAT_TEST_ERASE_EEPROM	0x09  
#define WHITEHEAT_TEST_READ_EEPROM	0x0a  
#define WHITEHEAT_TEST_PROGRAM_EEPROM	0x0b  

struct whiteheat_test {
	__u8	port;		
	__u8	test;		
	__u8	info[32];	
};




#define WHITEHEAT_EVENT_MODEM		0x01	
#define WHITEHEAT_EVENT_ERROR		0x02	
#define WHITEHEAT_EVENT_FLOW		0x04	
#define WHITEHEAT_EVENT_CONNECT		0x08	

#define WHITEHEAT_FLOW_NONE		0x00	
#define WHITEHEAT_FLOW_HARD_OUT		0x01	
#define WHITEHEAT_FLOW_HARD_IN		0x02	
#define WHITEHEAT_FLOW_SOFT_OUT		0x04	
#define WHITEHEAT_FLOW_SOFT_IN		0x08	
#define WHITEHEAT_FLOW_TX_DONE		0x80	

struct whiteheat_status_info {
	__u8	port;		
	__u8	event;		
	__u8	modem;		
	__u8	error;		
	__u8	flow;		
	__u8	connect;	
};


struct whiteheat_dr_info {
	__u8	mcr;		
};


struct whiteheat_hw_info {
	__u8	hw_id;		
	__u8	sw_major_rev;	
	__u8	sw_minor_rev;	
	struct whiteheat_hw_eeprom_info {
		__u8	b0;			
		__u8	vendor_id_low;		
		__u8	vendor_id_high;		
		__u8	product_id_low;		
		__u8	product_id_high;	
		__u8	device_id_low;		
		__u8	device_id_high;		
		__u8	not_used_1;
		__u8	serial_number_0;	
		__u8	serial_number_1;	
		__u8	serial_number_2;	
		__u8	serial_number_3;	
		__u8	not_used_2;
		__u8	not_used_3;
		__u8	checksum_low;		
		__u8	checksum_high;		
	} hw_eeprom_info;	
};


struct whiteheat_event_info {
	__u8	port;		
	__u8	event;		
	__u8	info;		
};


#define WHITEHEAT_TEST_FAIL	0x00  
#define WHITEHEAT_TEST_UNKNOWN	0x01  
#define WHITEHEAT_TEST_PASS	0xff  

struct whiteheat_test_info {
	__u8	port;		
	__u8	test;		
	__u8	status;		
	__u8	results[32];	
};


#endif
