/* 68328serial.h: Definitions for the mc68328 serial driver.
 *
 * Copyright (C) 1995       David S. Miller    <davem@caip.rutgers.edu>
 * Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
 * Copyright (C) 1998, 1999 D. Jeff Dionne     <jeff@uclinux.org>
 * Copyright (C) 1999       Vladimir Gurevich  <vgurevic@cisco.com>
 *
 * VZ Support/Fixes             Evan Stawnyczy <e@lineo.ca>
 */

#ifndef _MC683XX_SERIAL_H
#define _MC683XX_SERIAL_H


struct serial_struct {
	int	type;
	int	line;
	int	port;
	int	irq;
	int	flags;
	int	xmit_fifo_size;
	int	custom_divisor;
	int	baud_base;
	unsigned short	close_delay;
	char	reserved_char[2];
	int	hub6;  
	unsigned short	closing_wait; 
	unsigned short	closing_wait2; 
	int	reserved[4];
};

#define S_CLOSING_WAIT_INF	0
#define S_CLOSING_WAIT_NONE	65535

#define S_HUP_NOTIFY 0x0001 
#define S_FOURPORT  0x0002	
#define S_SAK	0x0004	
#define S_SPLIT_TERMIOS 0x0008 

#define S_SPD_MASK	0x0030
#define S_SPD_HI	0x0010	

#define S_SPD_VHI	0x0020  
#define S_SPD_CUST	0x0030  

#define S_SKIP_TEST	0x0040 
#define S_AUTO_IRQ  0x0080 
#define S_SESSION_LOCKOUT 0x0100 
#define S_PGRP_LOCKOUT    0x0200 
#define S_CALLOUT_NOHUP   0x0400 

#define S_FLAGS	0x0FFF	
#define S_USR_MASK 0x0430	

#define S_INITIALIZED	0x80000000 
#define S_CALLOUT_ACTIVE	0x40000000 
#define S_NORMAL_ACTIVE	0x20000000 
#define S_BOOT_AUTOCONF	0x10000000 
#define S_CLOSING		0x08000000 
#define S_CTS_FLOW		0x04000000 
#define S_CHECK_CD		0x02000000 


#ifdef __KERNEL__

#define USTCNT_TX_INTR_MASK (USTCNT_TXEE)


#if defined(CONFIG_M68EZ328) || defined(CONFIG_M68VZ328)
#define USTCNT_RX_INTR_MASK (USTCNT_RXHE | USTCNT_ODEN)
#elif defined(CONFIG_M68328)
#define USTCNT_RX_INTR_MASK (USTCNT_RXRE)
#else
#error Please, define the Rx interrupt events for your CPU
#endif


struct m68k_serial {
	char soft_carrier;  
	char break_abort;   
	char is_cons;       

	unsigned char clk_divisor;  
	int baud;
	int			magic;
	int			baud_base;
	int			port;
	int			irq;
	int			flags; 		
	int			type; 		
	struct tty_struct 	*tty;
	int			read_status_mask;
	int			ignore_status_mask;
	int			timeout;
	int			xmit_fifo_size;
	int			custom_divisor;
	int			x_char;	
	int			close_delay;
	unsigned short		closing_wait;
	unsigned short		closing_wait2;
	unsigned long		event;
	unsigned long		last_active;
	int			line;
	int			count;	    
	int			blocked_open; 
	unsigned char 		*xmit_buf;
	int			xmit_head;
	int			xmit_tail;
	int			xmit_cnt;
	wait_queue_head_t	open_wait;
	wait_queue_head_t	close_wait;
};


#define SERIAL_MAGIC 0x5301

#define SERIAL_XMIT_SIZE 4096

#define RS_EVENT_WRITE_WAKEUP	0

#define NR_PORTS 1
#define UART_IRQ_DEFNS {UART_IRQ_NUM}

#endif 
#endif 
