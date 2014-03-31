/*
 * PCBIT-D device driver definitions
 *
 * Copyright (C) 1996 Universidade de Lisboa
 *
 * Written by Pedro Roque Marques (roque@di.fc.ul.pt)
 *
 * This software may be used and distributed according to the terms of
 * the GNU General Public License, incorporated herein by reference.
 */

#ifndef PCBIT_H
#define PCBIT_H

#include <linux/workqueue.h>

#define MAX_PCBIT_CARDS 4


#define BLOCK_TIMER

#ifdef __KERNEL__

struct pcbit_chan {
	unsigned short id;
	unsigned short callref;                   
	unsigned char  proto;                     
	unsigned char  queued;                    
	unsigned char  layer2link;                
	unsigned char  snum;                      
	unsigned short s_refnum;
	unsigned short r_refnum;
	unsigned short fsm_state;
	struct timer_list fsm_timer;
#ifdef BLOCK_TIMER
	struct timer_list block_timer;
#endif
};

struct msn_entry {
	char *msn;
	struct msn_entry *next;
};

struct pcbit_dev {
	

	volatile unsigned char __iomem *sh_mem;		
	unsigned long ph_mem;
	unsigned int irq;
	unsigned int id;
	unsigned int interrupt;			
	spinlock_t lock;
	

	struct msn_entry *msn_list;		

	isdn_if *dev_if;

	ushort ll_hdrlen;
	ushort hl_hdrlen;

	
	unsigned char l2_state;

	struct frame_buf *read_queue;
	struct frame_buf *read_frame;
	struct frame_buf *write_queue;

	
	wait_queue_head_t set_running_wq;
	struct timer_list set_running_timer;

	struct timer_list error_recover_timer;

	struct work_struct qdelivery;

	u_char w_busy;
	u_char r_busy;

	volatile unsigned char __iomem *readptr;
	volatile unsigned char __iomem *writeptr;

	ushort loadptr;

	unsigned short fsize[8];		

	unsigned char send_seq;
	unsigned char rcv_seq;
	unsigned char unack_seq;

	unsigned short free;

	

	struct pcbit_chan *b1;
	struct pcbit_chan *b2;
};

#define STATS_TIMER (10 * HZ)
#define ERRTIME     (HZ / 10)

#define MAXBUFSIZE  1534
#define MRU   MAXBUFSIZE

#define STATBUF_LEN 2048

#endif 


struct pcbit_ioctl {
	union {
		struct byte_op {
			ushort addr;
			ushort value;
		} rdp_byte;
		unsigned long l2_status;
	} info;
};



#define PCBIT_IOCTL_GETSTAT  0x01    
#define PCBIT_IOCTL_LWMODE   0x02    
#define PCBIT_IOCTL_STRLOAD  0x03    
#define PCBIT_IOCTL_ENDLOAD  0x04    
#define PCBIT_IOCTL_SETBYTE  0x05    
#define PCBIT_IOCTL_GETBYTE  0x06    
#define PCBIT_IOCTL_RUNNING  0x07    
#define PCBIT_IOCTL_WATCH188 0x08    
#define PCBIT_IOCTL_PING188  0x09    
#define PCBIT_IOCTL_FWMODE   0x0A    
#define PCBIT_IOCTL_STOP     0x0B    
#define PCBIT_IOCTL_APION    0x0C    

#ifndef __KERNEL__

#define PCBIT_GETSTAT  (PCBIT_IOCTL_GETSTAT  + IIOCDRVCTL)
#define PCBIT_LWMODE   (PCBIT_IOCTL_LWMODE   + IIOCDRVCTL)
#define PCBIT_STRLOAD  (PCBIT_IOCTL_STRLOAD  + IIOCDRVCTL)
#define PCBIT_ENDLOAD  (PCBIT_IOCTL_ENDLOAD  + IIOCDRVCTL)
#define PCBIT_SETBYTE  (PCBIT_IOCTL_SETBYTE  + IIOCDRVCTL)
#define PCBIT_GETBYTE  (PCBIT_IOCTL_GETBYTE  + IIOCDRVCTL)
#define PCBIT_RUNNING  (PCBIT_IOCTL_RUNNING  + IIOCDRVCTL)
#define PCBIT_WATCH188 (PCBIT_IOCTL_WATCH188 + IIOCDRVCTL)
#define PCBIT_PING188  (PCBIT_IOCTL_PING188  + IIOCDRVCTL)
#define PCBIT_FWMODE   (PCBIT_IOCTL_FWMODE   + IIOCDRVCTL)
#define PCBIT_STOP     (PCBIT_IOCTL_STOP     + IIOCDRVCTL)
#define PCBIT_APION    (PCBIT_IOCTL_APION    + IIOCDRVCTL)

#define MAXSUPERLINE 3000

#endif

#define L2_DOWN     0
#define L2_LOADING  1
#define L2_LWMODE   2
#define L2_FWMODE   3
#define L2_STARTING 4
#define L2_RUNNING  5
#define L2_ERROR    6

void pcbit_deliver(struct work_struct *work);
int pcbit_init_dev(int board, int mem_base, int irq);
void pcbit_terminate(int board);
void pcbit_l3_receive(struct pcbit_dev *dev, ulong msg, struct sk_buff *skb,
		      ushort hdr_len, ushort refnum);
void pcbit_state_change(struct pcbit_dev *dev, struct pcbit_chan *chan,
			unsigned short i, unsigned short ev, unsigned short f);

#endif
