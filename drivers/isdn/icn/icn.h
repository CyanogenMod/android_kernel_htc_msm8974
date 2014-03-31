/* $Id: icn.h,v 1.30.6.5 2001/09/23 22:24:55 kai Exp $
 *
 * ISDN lowlevel-module for the ICN active ISDN-Card.
 *
 * Copyright 1994 by Fritz Elfert (fritz@isdn4linux.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef icn_h
#define icn_h

#define ICN_IOCTL_SETMMIO   0
#define ICN_IOCTL_GETMMIO   1
#define ICN_IOCTL_SETPORT   2
#define ICN_IOCTL_GETPORT   3
#define ICN_IOCTL_LOADBOOT  4
#define ICN_IOCTL_LOADPROTO 5
#define ICN_IOCTL_LEASEDCFG 6
#define ICN_IOCTL_GETDOUBLE 7
#define ICN_IOCTL_DEBUGVAR  8
#define ICN_IOCTL_ADDCARD   9

typedef struct icn_cdef {
	int port;
	char id1[10];
	char id2[10];
} icn_cdef;

#if defined(__KERNEL__) || defined(__DEBUGVAR__)

#ifdef __KERNEL__

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
#include <linux/isdnif.h>

#endif                          

#ifdef ICN_DEBUG_PORT
#define OUTB_P(v, p) {printk(KERN_DEBUG "icn: outb_p(0x%02x,0x%03x)\n", v, p); outb_p(v, p);}
#else
#define OUTB_P outb
#endif

#define ICN_BASEADDR 0x320
#define ICN_PORTLEN (0x04)
#define ICN_MEMADDR 0x0d0000

#define ICN_FLAGS_B1ACTIVE 1    
#define ICN_FLAGS_B2ACTIVE 2    
#define ICN_FLAGS_RUNNING  4    
#define ICN_FLAGS_RBTIMER  8    

#define ICN_BOOT_TIMEOUT1  1000 

#define ICN_TIMER_BCREAD (HZ / 100)	
#define ICN_TIMER_DCREAD (HZ / 2) 

#define ICN_CODE_STAGE1 4096    
#define ICN_CODE_STAGE2 65536   

#define ICN_MAX_SQUEUE 8000     
#define ICN_FRAGSIZE (250)      
#define ICN_BCH 2               


#define SHM_DCTL_OFFSET (0)     
#define SHM_CCTL_OFFSET (0x1d2) 
#define SHM_CBUF_OFFSET (0x200) 
#define SHM_DBUF_OFFSET (0x2000)	

typedef struct {
	unsigned char length;   
	unsigned char endflag;  
	unsigned char data[ICN_FRAGSIZE];	
	
	char unused[0x100 - ICN_FRAGSIZE - 2];
} frag_buf;

typedef union {
	struct {
		unsigned char scns;	
		unsigned char scnr;	
		unsigned char ecns;	
		unsigned char ecnr;	
		char unused[6];
		unsigned short fuell1;	
	} data_control;
	struct {
		char unused[SHM_CCTL_OFFSET];
		unsigned char iopc_i;	
		unsigned char iopc_o;	
		unsigned char pcio_i;	
		unsigned char pcio_o;	
	} comm_control;
	struct {
		char unused[SHM_CBUF_OFFSET];
		unsigned char pcio_buf[0x100];	
		unsigned char iopc_buf[0x100];	
	} comm_buffers;
	struct {
		char unused[SHM_DBUF_OFFSET];
		frag_buf receive_buf[0x10];
		frag_buf send_buf[0x10];
	} data_buffers;
} icn_shmem;

typedef struct icn_card {
	struct icn_card *next;  
	struct icn_card *other; 
	unsigned short port;    
	int myid;               
	int rvalid;             
	int leased;             
				
	unsigned short flags;   
	int doubleS0;           
	int secondhalf;         
	int fw_rev;             
	int ptype;              
	struct timer_list st_timer;   
	struct timer_list rb_timer;   
	u_char rcvbuf[ICN_BCH][4096]; 
	int rcvidx[ICN_BCH];    
	int l2_proto[ICN_BCH];  
	isdn_if interface;      
	int iptr;               
	char imsg[60];          
	char msg_buf[2048];     
	char *msg_buf_write;    
	char *msg_buf_read;     
	char *msg_buf_end;      
	int sndcount[ICN_BCH];  
	int xlen[ICN_BCH];      
	struct sk_buff *xskb[ICN_BCH]; 
	struct sk_buff_head spqueue[ICN_BCH];  
	char regname[35];       
	u_char xmit_lock[ICN_BCH]; 
	spinlock_t lock;        
} icn_card;

typedef struct icn_dev {
	spinlock_t devlock;     
	unsigned long memaddr;	
	icn_shmem __iomem *shmem;       
	int mvalid;             
	int channel;            
	struct icn_card *mcard; 
	int chanlock;           
	int firstload;          
} icn_dev;

typedef icn_dev *icn_devptr;

#ifdef __KERNEL__

static icn_card *cards = (icn_card *) 0;
static u_char chan2bank[] =
{0, 4, 8, 12};                  

static icn_dev dev;

#endif                          


#define ICN_CFG    (card->port)
#define ICN_MAPRAM (card->port + 1)
#define ICN_RUN    (card->port + 2)
#define ICN_BANK   (card->port + 3)

#define sbfree (((readb(&dev.shmem->data_control.scns) + 1) & 0xf) !=	\
		readb(&dev.shmem->data_control.scnr))

#define sbnext (writeb((readb(&dev.shmem->data_control.scns) + 1) & 0xf,	\
		       &dev.shmem->data_control.scns))

#define sbuf_n dev.shmem->data_control.scns
#define sbuf_d dev.shmem->data_buffers.send_buf[readb(&sbuf_n)].data
#define sbuf_l dev.shmem->data_buffers.send_buf[readb(&sbuf_n)].length
#define sbuf_f dev.shmem->data_buffers.send_buf[readb(&sbuf_n)].endflag

#define rbavl  (readb(&dev.shmem->data_control.ecnr) != \
		readb(&dev.shmem->data_control.ecns))

#define rbnext (writeb((readb(&dev.shmem->data_control.ecnr) + 1) & 0xf,	\
		       &dev.shmem->data_control.ecnr))

#define rbuf_n dev.shmem->data_control.ecnr
#define rbuf_d dev.shmem->data_buffers.receive_buf[readb(&rbuf_n)].data
#define rbuf_l dev.shmem->data_buffers.receive_buf[readb(&rbuf_n)].length
#define rbuf_f dev.shmem->data_buffers.receive_buf[readb(&rbuf_n)].endflag

#define cmd_o (dev.shmem->comm_control.pcio_o)
#define cmd_i (dev.shmem->comm_control.pcio_i)

#define cmd_free ((readb(&cmd_i) >= readb(&cmd_o)) ?		\
		  0x100 - readb(&cmd_i) + readb(&cmd_o) :	\
		  readb(&cmd_o) - readb(&cmd_i))

#define msg_o (dev.shmem->comm_control.iopc_o)
#define msg_i (dev.shmem->comm_control.iopc_i)

#define msg_avail ((readb(&msg_o) > readb(&msg_i)) ?		\
		   0x100 - readb(&msg_o) + readb(&msg_i) :	\
		   readb(&msg_i) - readb(&msg_o))

#define CID (card->interface.id)

#endif                          
#endif                          
