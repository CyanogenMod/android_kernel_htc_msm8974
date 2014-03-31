/* $Id: act2000.h,v 1.8.6.3 2001/09/23 22:24:32 kai Exp $
 *
 * ISDN lowlevel-module for the IBM ISDN-S0 Active 2000.
 *
 * Author       Fritz Elfert
 * Copyright    by Fritz Elfert      <fritz@isdn4linux.de>
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * Thanks to Friedemann Baitinger and IBM Germany
 *
 */

#ifndef act2000_h
#define act2000_h

#include <linux/compiler.h>

#define ACT2000_IOCTL_SETPORT    1
#define ACT2000_IOCTL_GETPORT    2
#define ACT2000_IOCTL_SETIRQ     3
#define ACT2000_IOCTL_GETIRQ     4
#define ACT2000_IOCTL_SETBUS     5
#define ACT2000_IOCTL_GETBUS     6
#define ACT2000_IOCTL_SETPROTO   7
#define ACT2000_IOCTL_GETPROTO   8
#define ACT2000_IOCTL_SETMSN     9
#define ACT2000_IOCTL_GETMSN    10
#define ACT2000_IOCTL_LOADBOOT  11
#define ACT2000_IOCTL_ADDCARD   12

#define ACT2000_IOCTL_TEST      98
#define ACT2000_IOCTL_DEBUGVAR  99

#define ACT2000_BUS_ISA          1
#define ACT2000_BUS_MCA          2
#define ACT2000_BUS_PCMCIA       3

typedef struct act2000_cdef {
	int bus;
	int port;
	int irq;
	char id[10];
} act2000_cdef;

typedef struct act2000_ddef {
	int length;             
	char __user *buffer;    
} act2000_ddef;

typedef struct act2000_fwid {
	char isdn[4];
	char revlen[2];
	char revision[504];
} act2000_fwid;

#if defined(__KERNEL__) || defined(__DEBUGVAR__)

#ifdef __KERNEL__

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/ioport.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/isdnif.h>

#endif                           

#define ACT2000_PORTLEN        8

#define ACT2000_FLAGS_RUNNING  1 
#define ACT2000_FLAGS_PVALID   2 
#define ACT2000_FLAGS_IVALID   4 
#define ACT2000_FLAGS_LOADED   8 

#define ACT2000_BCH            2 

#define ACT2000_STATE_NULL     0
#define ACT2000_STATE_ICALL    1
#define ACT2000_STATE_OCALL    2
#define ACT2000_STATE_IWAIT    3
#define ACT2000_STATE_OWAIT    4
#define ACT2000_STATE_IBWAIT   5
#define ACT2000_STATE_OBWAIT   6
#define ACT2000_STATE_BWAIT    7
#define ACT2000_STATE_BHWAIT   8
#define ACT2000_STATE_BHWAIT2  9
#define ACT2000_STATE_DHWAIT  10
#define ACT2000_STATE_DHWAIT2 11
#define ACT2000_STATE_BSETUP  12
#define ACT2000_STATE_ACTIVE  13

#define ACT2000_MAX_QUEUED  8000 

#define ACT2000_LOCK_TX 0
#define ACT2000_LOCK_RX 1

typedef struct act2000_chan {
	unsigned short callref;          
	unsigned short fsm_state;        
	unsigned short eazmask;          
	short queued;                    
	unsigned short plci;
	unsigned short ncci;
	unsigned char  l2prot;           
	unsigned char  l3prot;           
} act2000_chan;

typedef struct msn_entry {
	char eaz;
	char msn[16];
	struct msn_entry *next;
} msn_entry;

typedef struct irq_data_isa {
	__u8           *rcvptr;
	__u16           rcvidx;
	__u16           rcvlen;
	struct sk_buff *rcvskb;
	__u8            rcvignore;
	__u8            rcvhdr[8];
} irq_data_isa;

typedef union act2000_irq_data {
	irq_data_isa isa;
} act2000_irq_data;

typedef struct act2000_card {
	unsigned short port;		
	unsigned short irq;		
	u_char ptype;			
	u_char bus;			
	struct act2000_card *next;	
	spinlock_t lock;		
	int myid;			
	unsigned long flags;		
	unsigned long ilock;		
	struct sk_buff_head rcvq;	
	struct sk_buff_head sndq;	
	struct sk_buff_head ackq;	
	u_char *ack_msg;		
	__u16 need_b3ack;		
	struct sk_buff *sbuf;		
	struct timer_list ptimer;	
	struct work_struct snd_tq;	
	struct work_struct rcv_tq;	
	struct work_struct poll_tq;	
	msn_entry *msn_list;
	unsigned short msgnum;		
	spinlock_t mnlock;		
	act2000_chan bch[ACT2000_BCH];	
	char   status_buf[256];		
	char   *status_buf_read;
	char   *status_buf_write;
	char   *status_buf_end;
	act2000_irq_data idat;		
	isdn_if interface;		
	char regname[35];		
} act2000_card;

static inline void act2000_schedule_tx(act2000_card *card)
{
	schedule_work(&card->snd_tq);
}

static inline void act2000_schedule_rx(act2000_card *card)
{
	schedule_work(&card->rcv_tq);
}

static inline void act2000_schedule_poll(act2000_card *card)
{
	schedule_work(&card->poll_tq);
}

extern char *act2000_find_eaz(act2000_card *, char);

#endif                          
#endif                          
