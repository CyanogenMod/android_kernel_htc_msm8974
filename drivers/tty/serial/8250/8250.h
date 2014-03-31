/*
 *  Driver for 8250/16550-type serial ports
 *
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o.
 *
 *  Copyright (C) 2001 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/serial_8250.h>

struct uart_8250_port {
	struct uart_port	port;
	struct timer_list	timer;		
	struct list_head	list;		
	unsigned short		capabilities;	
	unsigned short		bugs;		
	unsigned int		tx_loadsz;	
	unsigned char		acr;
	unsigned char		ier;
	unsigned char		lcr;
	unsigned char		mcr;
	unsigned char		mcr_mask;	
	unsigned char		mcr_force;	
	unsigned char		cur_iotype;	

#define LSR_SAVE_FLAGS UART_LSR_BRK_ERROR_BITS
	unsigned char		lsr_saved_flags;
#define MSR_SAVE_FLAGS UART_MSR_ANY_DELTA
	unsigned char		msr_saved_flags;
};

struct old_serial_port {
	unsigned int uart;
	unsigned int baud_base;
	unsigned int port;
	unsigned int irq;
	unsigned int flags;
	unsigned char hub6;
	unsigned char io_type;
	unsigned char *iomem_base;
	unsigned short iomem_reg_shift;
	unsigned long irqflags;
};

struct serial8250_config {
	const char	*name;
	unsigned short	fifo_size;
	unsigned short	tx_loadsz;
	unsigned char	fcr;
	unsigned int	flags;
};

#define UART_CAP_FIFO	(1 << 8)	
#define UART_CAP_EFR	(1 << 9)	
#define UART_CAP_SLEEP	(1 << 10)	
#define UART_CAP_AFE	(1 << 11)	
#define UART_CAP_UUE	(1 << 12)	
#define UART_CAP_RTOIE	(1 << 13)	

#define UART_BUG_QUOT	(1 << 0)	
#define UART_BUG_TXEN	(1 << 1)	
#define UART_BUG_NOMSR	(1 << 2)	
#define UART_BUG_THRE	(1 << 3)	

#define PROBE_RSA	(1 << 0)
#define PROBE_ANY	(~0)

#define HIGH_BITS_OFFSET ((sizeof(long)-sizeof(int))*8)

#ifdef CONFIG_SERIAL_8250_SHARE_IRQ
#define SERIAL8250_SHARE_IRQS 1
#else
#define SERIAL8250_SHARE_IRQS 0
#endif

static inline int serial_in(struct uart_8250_port *up, int offset)
{
	return up->port.serial_in(&up->port, offset);
}

static inline void serial_out(struct uart_8250_port *up, int offset, int value)
{
	up->port.serial_out(&up->port, offset, value);
}

#if defined(__alpha__) && !defined(CONFIG_PCI)
#define ALPHA_KLUDGE_MCR  (UART_MCR_OUT2 | UART_MCR_OUT1)
#elif defined(CONFIG_SBC8560)
#define ALPHA_KLUDGE_MCR (UART_MCR_OUT2)
#else
#define ALPHA_KLUDGE_MCR 0
#endif
