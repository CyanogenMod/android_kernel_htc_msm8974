/*
 * serial.h: Arch-dep definitions for the Etrax100 serial driver.
 *
 * Copyright (C) 1998-2007 Axis Communications AB
 */

#ifndef _ETRAX_SERIAL_H
#define _ETRAX_SERIAL_H

#include <linux/circ_buf.h>
#include <asm/termios.h>
#include <asm/dma.h>
#include <arch/io_interface_mux.h>


#ifdef __KERNEL__

#define SERIAL_RECV_DESCRIPTORS 8

struct etrax_recv_buffer {
	struct etrax_recv_buffer *next;
	unsigned short length;
	unsigned char error;
	unsigned char pad;

	unsigned char buffer[0];
};

struct e100_serial {
	struct tty_port port;
	int baud;
	volatile u8	*ioport;	
	u32		irq;	

	
	volatile u8 *oclrintradr;	
	volatile u32 *ofirstadr;	
	volatile u8 *ocmdadr;		
	const volatile u8 *ostatusadr;	

	
	volatile u8 *iclrintradr;	
	volatile u32 *ifirstadr;	
	volatile u8 *icmdadr;		
	volatile u32 *idescradr;	

	int flags;	

	u8 rx_ctrl;	
	u8 tx_ctrl;	
	u8 iseteop;	
	int enabled;	

	u8 dma_out_enabled;	
	u8 dma_in_enabled;	

	
	int		dma_owner;
	unsigned int	dma_in_nbr;
	unsigned int	dma_out_nbr;
	unsigned int	dma_in_irq_nbr;
	unsigned int	dma_out_irq_nbr;
	unsigned long	dma_in_irq_flags;
	unsigned long	dma_out_irq_flags;
	char		*dma_in_irq_description;
	char		*dma_out_irq_description;

	enum cris_io_interface io_if;
	char            *io_if_description;

	u8		uses_dma_in;  
	u8		uses_dma_out; 
	u8		forced_eop;   
	int			baud_base;     
	int			custom_divisor; 
	struct etrax_dma_descr	tr_descr;
	struct etrax_dma_descr	rec_descr[SERIAL_RECV_DESCRIPTORS];
	int			cur_rec_descr;

	volatile int		tr_running; 

	struct tty_struct	*tty;
	int			read_status_mask;
	int			ignore_status_mask;
	int			x_char;	
	int			close_delay;
	unsigned short		closing_wait;
	unsigned short		closing_wait2;
	unsigned long		event;
	unsigned long		last_active;
	int			line;
	int			type;  
	int			count;	    
	int			blocked_open; 
	struct circ_buf		xmit;
	struct etrax_recv_buffer *first_recv_buffer;
	struct etrax_recv_buffer *last_recv_buffer;
	unsigned int		recv_cnt;
	unsigned int		max_recv_cnt;

	struct work_struct	work;
	struct async_icount	icount;   
	struct ktermios		normal_termios;
	struct ktermios		callout_termios;
	wait_queue_head_t	open_wait;
	wait_queue_head_t	close_wait;

	unsigned long char_time_usec;       
	unsigned long flush_time_usec;      
	unsigned long last_tx_active_usec;  
	unsigned long last_tx_active;       
	unsigned long last_rx_active_usec;  
	unsigned long last_rx_active;       

	int break_detected_cnt;
	int errorcode;

#ifdef CONFIG_ETRAX_RS485
	struct serial_rs485	rs485;  
#endif
};


#define PORT_ETRAX 1

#define RS_EVENT_WRITE_WAKEUP	0

#endif 

#endif 
