/************************************************************************
 * Copyright 2003 Digi International (www.digi.com)
 *
 * Copyright (C) 2004 IBM Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 * Temple Place - Suite 330, Boston,
 * MA  02111-1307, USA.
 *
 * Contact Information:
 * Scott H Kilau <Scott_Kilau@digi.com>
 * Wendy Xiong   <wendyx@us.ibm.com>
 *
 ***********************************************************************/

#ifndef __JSM_DRIVER_H
#define __JSM_DRIVER_H

#include <linux/kernel.h>
#include <linux/types.h>	
#include <linux/tty.h>
#include <linux/serial_core.h>
#include <linux/device.h>

enum {
	DBG_INIT	= 0x01,
	DBG_BASIC	= 0x02,
	DBG_CORE	= 0x04,
	DBG_OPEN	= 0x08,
	DBG_CLOSE	= 0x10,
	DBG_READ	= 0x20,
	DBG_WRITE	= 0x40,
	DBG_IOCTL	= 0x80,
	DBG_PROC	= 0x100,
	DBG_PARAM	= 0x200,
	DBG_PSCAN	= 0x400,
	DBG_EVENT	= 0x800,
	DBG_DRAIN	= 0x1000,
	DBG_MSIGS	= 0x2000,
	DBG_MGMT	= 0x4000,
	DBG_INTR	= 0x8000,
	DBG_CARR	= 0x10000,
};

#define jsm_printk(nlevel, klevel, pdev, fmt, args...)	\
	if ((DBG_##nlevel & jsm_debug))			\
	dev_printk(KERN_##klevel, pdev->dev, fmt, ## args)

#define	MAXLINES	256
#define MAXPORTS	8
#define MAX_STOPS_SENT	5


#define T_NEO		0000
#define T_CLASSIC	0001
#define T_PCIBUS	0400


#define BD_RUNNING	0x0
#define BD_REASON	0x7f
#define BD_NOTFOUND	0x1
#define BD_NOIOPORT	0x2
#define BD_NOMEM	0x3
#define BD_NOBIOS	0x4
#define BD_NOFEP	0x5
#define BD_FAILED	0x6
#define BD_ALLOCATED	0x7
#define BD_TRIBOOT	0x8
#define BD_BADKME	0x80


#define WRITEBUFLEN	((4096) + 4)

#define JSM_VERSION	"jsm: 1.2-1-INKERNEL"
#define JSM_PARTNUM	"40002438_A-INKERNEL"

struct jsm_board;
struct jsm_channel;

struct board_ops {
	irq_handler_t intr;
	void (*uart_init) (struct jsm_channel *ch);
	void (*uart_off) (struct jsm_channel *ch);
	void (*param) (struct jsm_channel *ch);
	void (*assert_modem_signals) (struct jsm_channel *ch);
	void (*flush_uart_write) (struct jsm_channel *ch);
	void (*flush_uart_read) (struct jsm_channel *ch);
	void (*disable_receiver) (struct jsm_channel *ch);
	void (*enable_receiver) (struct jsm_channel *ch);
	void (*send_break) (struct jsm_channel *ch);
	void (*clear_break) (struct jsm_channel *ch, int);
	void (*send_start_character) (struct jsm_channel *ch);
	void (*send_stop_character) (struct jsm_channel *ch);
	void (*copy_data_from_queue_to_uart) (struct jsm_channel *ch);
	u32 (*get_uart_bytes_left) (struct jsm_channel *ch);
	void (*send_immediate_char) (struct jsm_channel *ch, unsigned char);
};


struct jsm_board
{
	int		boardnum;	

	int		type;		
	u8		rev;		
	struct pci_dev	*pci_dev;
	u32		maxports;	

	spinlock_t	bd_intr_lock;	

	u32		nasync;		

	u32		irq;		

	u64		membase;	
	u64		membase_end;	

	u8	__iomem *re_map_membase;

	u64		iobase;		
	u64		iobase_end;	

	u32		bd_uart_offset;	

	struct jsm_channel *channels[MAXPORTS]; 

	u32		bd_dividend;	

	struct board_ops *bd_ops;

	struct list_head jsm_board_entry;
};

#define CH_PRON		0x0001		
#define CH_STOP		0x0002		
#define CH_STOPI	0x0004		
#define CH_CD		0x0008		
#define CH_FCAR		0x0010		
#define CH_HANGUP	0x0020		

#define CH_RECEIVER_OFF	0x0040		
#define CH_OPENING	0x0080		
#define CH_CLOSING	0x0100		
#define CH_FIFO_ENABLED 0x0200		
#define CH_TX_FIFO_EMPTY 0x0400		
#define CH_TX_FIFO_LWM	0x0800		
#define CH_BREAK_SENDING 0x1000		
#define CH_LOOPBACK 0x2000		
#define CH_BAUD0	0x08000		

#define RQUEUEMASK	0x1FFF		
#define EQUEUEMASK	0x1FFF		
#define RQUEUESIZE	(RQUEUEMASK + 1)
#define EQUEUESIZE	RQUEUESIZE


struct jsm_channel {
	struct uart_port uart_port;
	struct jsm_board	*ch_bd;		

	spinlock_t	ch_lock;	
	wait_queue_head_t ch_flags_wait;

	u32		ch_portnum;	
	u32		ch_open_count;	
	u32		ch_flags;	

	u64		ch_close_delay;	

	tcflag_t	ch_c_iflag;	
	tcflag_t	ch_c_cflag;	
	tcflag_t	ch_c_oflag;	
	tcflag_t	ch_c_lflag;	
	u8		ch_stopc;	
	u8		ch_startc;	

	u8		ch_mostat;	
	u8		ch_mistat;	

	struct neo_uart_struct __iomem *ch_neo_uart;	
	u8		ch_cached_lsr;	

	u8		*ch_rqueue;	
	u16		ch_r_head;	
	u16		ch_r_tail;	

	u8		*ch_equeue;	
	u16		ch_e_head;	
	u16		ch_e_tail;	

	u64		ch_rxcount;	
	u64		ch_txcount;	

	u8		ch_r_tlevel;	
	u8		ch_t_tlevel;	

	u8		ch_r_watermark;	


	u32		ch_stops_sent;	
	u64		ch_err_parity;	
	u64		ch_err_frame;	
	u64		ch_err_break;	
	u64		ch_err_overrun; 

	u64		ch_xon_sends;	
	u64		ch_xoff_sends;	
};



struct neo_uart_struct {
	 u8 txrx;		
	 u8 ier;		
	 u8 isr_fcr;		
	 u8 lcr;		
	 u8 mcr;		
	 u8 lsr;		
	 u8 msr;		
	 u8 spr;		
	 u8 fctr;		
	 u8 efr;		
	 u8 tfifo;		
	 u8 rfifo;		
	 u8 xoffchar1;	
	 u8 xoffchar2;	
	 u8 xonchar1;	
	 u8 xonchar2;	

	 u8 reserved1[0x2ff - 0x200]; 
	 u8 txrxburst[64];	
	 u8 reserved2[0x37f - 0x340]; 
	 u8 rxburst_with_errors[64];	
};

#define	UART_17158_POLL_ADDR_OFFSET	0x80


#define UART_17158_FCTR_RTS_NODELAY	0x00
#define UART_17158_FCTR_RTS_4DELAY	0x01
#define UART_17158_FCTR_RTS_6DELAY	0x02
#define UART_17158_FCTR_RTS_8DELAY	0x03
#define UART_17158_FCTR_RTS_12DELAY	0x12
#define UART_17158_FCTR_RTS_16DELAY	0x05
#define UART_17158_FCTR_RTS_20DELAY	0x13
#define UART_17158_FCTR_RTS_24DELAY	0x06
#define UART_17158_FCTR_RTS_28DELAY	0x14
#define UART_17158_FCTR_RTS_32DELAY	0x07
#define UART_17158_FCTR_RTS_36DELAY	0x16
#define UART_17158_FCTR_RTS_40DELAY	0x08
#define UART_17158_FCTR_RTS_44DELAY	0x09
#define UART_17158_FCTR_RTS_48DELAY	0x10
#define UART_17158_FCTR_RTS_52DELAY	0x11

#define UART_17158_FCTR_RTS_IRDA	0x10
#define UART_17158_FCTR_RS485		0x20
#define UART_17158_FCTR_TRGA		0x00
#define UART_17158_FCTR_TRGB		0x40
#define UART_17158_FCTR_TRGC		0x80
#define UART_17158_FCTR_TRGD		0xC0

#define UART_17158_FCTR_BIT6		0x40
#define UART_17158_FCTR_BIT7		0x80

#define UART_17158_RX_FIFOSIZE		64
#define UART_17158_TX_FIFOSIZE		64

#define UART_17158_IIR_RDI_TIMEOUT	0x0C	
#define UART_17158_IIR_XONXOFF		0x10	
#define UART_17158_IIR_HWFLOW_STATE_CHANGE 0x20	
#define UART_17158_IIR_FIFO_ENABLED	0xC0	

#define UART_17158_RX_LINE_STATUS	0x1	
#define UART_17158_RXRDY_TIMEOUT	0x2	
#define UART_17158_TXRDY		0x3	
#define UART_17158_MSR			0x4	
#define UART_17158_TX_AND_FIFO_CLR	0x40	
#define UART_17158_RX_FIFO_DATA_ERROR	0x80	

#define UART_17158_EFR_ECB	0x10	
#define UART_17158_EFR_IXON	0x2	
#define UART_17158_EFR_IXOFF	0x8	
#define UART_17158_EFR_RTSDTR	0x40	
#define UART_17158_EFR_CTSDSR	0x80	

#define UART_17158_XOFF_DETECT	0x1	
#define UART_17158_XON_DETECT	0x2	

#define UART_17158_IER_RSVD1	0x10	
#define UART_17158_IER_XOFF	0x20	
#define UART_17158_IER_RTSDTR	0x40	
#define UART_17158_IER_CTSDSR	0x80	

#define PCI_DEVICE_NEO_2DB9_PCI_NAME		"Neo 2 - DB9 Universal PCI"
#define PCI_DEVICE_NEO_2DB9PRI_PCI_NAME		"Neo 2 - DB9 Universal PCI - Powered Ring Indicator"
#define PCI_DEVICE_NEO_2RJ45_PCI_NAME		"Neo 2 - RJ45 Universal PCI"
#define PCI_DEVICE_NEO_2RJ45PRI_PCI_NAME	"Neo 2 - RJ45 Universal PCI - Powered Ring Indicator"
#define PCIE_DEVICE_NEO_IBM_PCI_NAME		"Neo 4 - PCI Express - IBM"

extern struct	uart_driver jsm_uart_driver;
extern struct	board_ops jsm_neo_ops;
extern int	jsm_debug;

int jsm_tty_init(struct jsm_board *);
int jsm_uart_port_init(struct jsm_board *);
int jsm_remove_uart_port(struct jsm_board *);
void jsm_input(struct jsm_channel *ch);
void jsm_check_queue_flow_control(struct jsm_channel *ch);

#endif
