/* MN10300 On-chip serial port driver definitions
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _MN10300_SERIAL_H
#define _MN10300_SERIAL_H

#ifndef __ASSEMBLY__
#include <linux/serial_core.h>
#include <linux/termios.h>
#endif

#include <asm/page.h>
#include <asm/serial-regs.h>

#define NR_PORTS		3		

#define MNSC_BUFFER_SIZE	+(PAGE_SIZE / 2)

#define MNSCx_RX_AVAIL		0x01
#define MNSCx_RX_OVERF		0x02
#define MNSCx_TX_SPACE		0x04
#define MNSCx_TX_EMPTY		0x08

#ifndef __ASSEMBLY__

struct mn10300_serial_port {
	char			*rx_buffer;	
	unsigned		rx_inp;		
	unsigned		rx_outp;	
	u8			tx_xchar;	
	u8			tx_break;	
	u8			intr_flags;	
	volatile u16		*rx_icr;	
	volatile u16		*tx_icr;	
	int			rx_irq;		
	int			tx_irq;		
	int			tm_irq;		

	const char		*name;		
	const char		*rx_name;	
	const char		*tx_name;	
	const char		*tm_name;	
	unsigned short		type;		
	unsigned char		isconsole;	
	volatile void		*_iobase;	
	volatile u16		*_control;	
	volatile u8		*_status;	
	volatile u8		*_intr;		
	volatile void		*_rxb;		
	volatile void		*_txb;		
	volatile u16		*_tmicr;	
	volatile u8		*_tmxmd;	
	volatile u16		*_tmxbr;	

	struct uart_port	uart;

	unsigned short		rx_brk;		
	u16			tx_cts;		
	int			gdbstub;	

	u8			clock_src;	
#define MNSCx_CLOCK_SRC_IOCLK	0
#define MNSCx_CLOCK_SRC_IOBCLK	1

	u8			div_timer;	
#define MNSCx_DIV_TIMER_16BIT	0
#define MNSCx_DIV_TIMER_8BIT	1

	u16			options;	
#define MNSCx_OPT_CTS		0x0001

	unsigned long		ioclk;		
};

#ifdef CONFIG_MN10300_TTYSM0
extern struct mn10300_serial_port mn10300_serial_port_sif0;
#endif

#ifdef CONFIG_MN10300_TTYSM1
extern struct mn10300_serial_port mn10300_serial_port_sif1;
#endif

#ifdef CONFIG_MN10300_TTYSM2
extern struct mn10300_serial_port mn10300_serial_port_sif2;
#endif

extern struct mn10300_serial_port *mn10300_serial_ports[];

struct mn10300_serial_int {
	struct mn10300_serial_port *port;
	asmlinkage void (*vdma)(void);
};

extern struct mn10300_serial_int mn10300_serial_int_tbl[];

extern asmlinkage void mn10300_serial_vdma_interrupt(void);
extern asmlinkage void mn10300_serial_vdma_rx_handler(void);
extern asmlinkage void mn10300_serial_vdma_tx_handler(void);

#endif 

#if defined(CONFIG_GDBSTUB_ON_TTYSM0)
#define SCgSTR SC0STR
#define SCgRXB SC0RXB
#define SCgRXIRQ SC0RXIRQ
#elif defined(CONFIG_GDBSTUB_ON_TTYSM1)
#define SCgSTR SC1STR
#define SCgRXB SC1RXB
#define SCgRXIRQ SC1RXIRQ
#elif defined(CONFIG_GDBSTUB_ON_TTYSM2)
#define SCgSTR SC2STR
#define SCgRXB SC2RXB
#define SCgRXIRQ SC2RXIRQ
#endif

#endif 
