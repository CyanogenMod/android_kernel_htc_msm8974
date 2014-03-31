/* gdb-io.h: FR403 GDB I/O port defs
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _GDB_IO_H
#define _GDB_IO_H

#include <asm/serial-regs.h>

#undef UART_RX
#undef UART_TX
#undef UART_DLL
#undef UART_DLM
#undef UART_IER
#undef UART_IIR
#undef UART_FCR
#undef UART_LCR
#undef UART_MCR
#undef UART_LSR
#undef UART_MSR
#undef UART_SCR

#define UART_RX		0*8	
#define UART_TX		0*8	
#define UART_DLL	0*8	
#define UART_DLM	1*8	
#define UART_IER	1*8	
#define UART_IIR	2*8	
#define UART_FCR	2*8	
#define UART_LCR	3*8	
#define UART_MCR	4*8	
#define UART_LSR	5*8	
#define UART_MSR	6*8	
#define UART_SCR	7*8	

#define UART_LCR_DLAB	0x80	
#define UART_LCR_SBC	0x40	
#define UART_LCR_SPAR	0x20	
#define UART_LCR_EPAR	0x10	
#define UART_LCR_PARITY	0x08	
#define UART_LCR_STOP	0x04	
#define UART_LCR_WLEN5  0x00	
#define UART_LCR_WLEN6  0x01	
#define UART_LCR_WLEN7  0x02	
#define UART_LCR_WLEN8  0x03	


#endif 
