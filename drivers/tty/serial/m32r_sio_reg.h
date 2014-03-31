/*
 * m32r_sio_reg.h
 *
 * Copyright (C) 1992, 1994 by Theodore Ts'o.
 * Copyright (C) 2004  Hirokazu Takata <takata at linux-m32r.org>
 *
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL)
 *
 * These are the UART port assignments, expressed as offsets from the base
 * register.  These assignments should hold for any serial port based on
 * a 8250, 16450, or 16550(A).
 */

#ifndef _M32R_SIO_REG_H
#define _M32R_SIO_REG_H


#ifdef CONFIG_SERIAL_M32R_PLDSIO

#define SIOCR		0x000
#define SIOMOD0		0x002
#define SIOMOD1		0x004
#define SIOSTS		0x006
#define SIOTRCR		0x008
#define SIOBAUR		0x00a
#define SIOTXB		0x00c
#define SIORXB		0x00e

#define UART_RX		((unsigned long) PLD_ESIO0RXB)
				
#define UART_TX		((unsigned long) PLD_ESIO0TXB)
				
#define UART_DLL	0	
#define UART_TRG	0	

#define UART_DLM	0	
#define UART_IER	((unsigned long) PLD_ESIO0INTCR)
				
#define UART_FCTR	0	

#define UART_IIR	0	
#define UART_FCR	0	
#define UART_EFR	0	
				

#define UART_LCR	0	
#define UART_MCR	0	
#define UART_LSR	((unsigned long) PLD_ESIO0STS)
				
#define UART_MSR	0	
#define UART_SCR	0	
#define UART_EMSR	0	

#else 

#define SIOCR		0x000
#define SIOMOD0		0x004
#define SIOMOD1		0x008
#define SIOSTS		0x00c
#define SIOTRCR		0x010
#define SIOBAUR		0x014
#define SIORBAUR	0x018
#define SIOTXB		0x01c
#define SIORXB		0x020

#define UART_RX		M32R_SIO0_RXB_PORTL	
#define UART_TX		M32R_SIO0_TXB_PORTL	
#define UART_DLL	0	
#define UART_TRG	0	

#define UART_DLM	0	
#define UART_IER	M32R_SIO0_TRCR_PORTL	
#define UART_FCTR	0	

#define UART_IIR	0	
#define UART_FCR	0	
#define UART_EFR	0	
				

#define UART_LCR	0	
#define UART_MCR	0	
#define UART_LSR	M32R_SIO0_STS_PORTL	
#define UART_MSR	0	
#define UART_SCR	0	
#define UART_EMSR	0	

#endif 

#define UART_EMPTY	(UART_LSR_TEMT | UART_LSR_THRE)

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

#define UART_LSR_TEMT	0x02	
#define UART_LSR_THRE	0x01	
#define UART_LSR_BI	0x00	
#define UART_LSR_FE	0x80	
#define UART_LSR_PE	0x40	
#define UART_LSR_OE	0x20	
#define UART_LSR_DR	0x04	

#define UART_IIR_NO_INT	0x01	
#define UART_IIR_ID	0x06	

#define UART_IIR_MSI	0x00	
#define UART_IIR_THRI	0x02	
#define UART_IIR_RDI	0x04	
#define UART_IIR_RLSI	0x06	

#define UART_IER_MSI	0x00	
#define UART_IER_RLSI	0x08	
#define UART_IER_THRI	0x03	
#define UART_IER_RDI	0x04	

#endif 
