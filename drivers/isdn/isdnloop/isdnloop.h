/* $Id: isdnloop.h,v 1.5.6.3 2001/09/23 22:24:56 kai Exp $
 *
 * Loopback lowlevel module for testing of linklevel.
 *
 * Copyright 1997 by Fritz Elfert (fritz@isdn4linux.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef isdnloop_h
#define isdnloop_h

#define ISDNLOOP_IOCTL_DEBUGVAR  0
#define ISDNLOOP_IOCTL_ADDCARD   1
#define ISDNLOOP_IOCTL_LEASEDCFG 2
#define ISDNLOOP_IOCTL_STARTUP   3

typedef struct isdnloop_cdef {
	char id1[10];
} isdnloop_cdef;

typedef struct isdnloop_sdef {
	int ptype;
	char num[3][20];
} isdnloop_sdef;

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
#include <linux/isdnif.h>

#endif                          

#define ISDNLOOP_FLAGS_B1ACTIVE 1	
#define ISDNLOOP_FLAGS_B2ACTIVE 2	
#define ISDNLOOP_FLAGS_RUNNING  4	
#define ISDNLOOP_FLAGS_RBTIMER  8	
#define ISDNLOOP_TIMER_BCREAD 1 
#define ISDNLOOP_TIMER_DCREAD (HZ/2)	
#define ISDNLOOP_TIMER_ALERTWAIT (10 * HZ)	
#define ISDNLOOP_MAX_SQUEUE 65536	
#define ISDNLOOP_BCH 2          

typedef struct isdnloop_card {
	struct isdnloop_card *next;	
	struct isdnloop_card
	*rcard[ISDNLOOP_BCH];   
	int rch[ISDNLOOP_BCH];  
	int myid;               
	int leased;             
	
	int sil[ISDNLOOP_BCH];  
	char eazlist[ISDNLOOP_BCH][11];
	
	char s0num[3][20];      
	unsigned short flags;   
	int ptype;              
	struct timer_list st_timer;	
	struct timer_list rb_timer;	
	struct timer_list
	c_timer[ISDNLOOP_BCH]; 
	int l2_proto[ISDNLOOP_BCH];	
	isdn_if interface;      
	int iptr;               
	char imsg[60];          
	int optr;               
	char omsg[60];          
	char msg_buf[2048];     
	char *msg_buf_write;    
	char *msg_buf_read;     
	char *msg_buf_end;      
	int sndcount[ISDNLOOP_BCH];	
	struct sk_buff_head
	bqueue[ISDNLOOP_BCH];  
	struct sk_buff_head dqueue;	
	spinlock_t isdnloop_lock;
} isdnloop_card;

#ifdef __KERNEL__
static isdnloop_card *cards = (isdnloop_card *) 0;
#endif                          


#define CID (card->interface.id)

#endif                          
#endif                          
