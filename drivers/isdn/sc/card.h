/* $Id: card.h,v 1.1.10.1 2001/09/23 22:24:59 kai Exp $
 *
 * Driver parameters for SpellCaster ISA ISDN adapters
 *
 * Copyright (C) 1996  SpellCaster Telecommunications Inc.
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * For more information, please contact gpl-info@spellcast.com or write:
 *
 *     SpellCaster Telecommunications Inc.
 *     5621 Finch Avenue East, Unit #3
 *     Scarborough, Ontario  Canada
 *     M1B 2T9
 *     +1 (416) 297-8565
 *     +1 (416) 297-6433 Facsimile
 */

#ifndef CARD_H
#define CARD_H

#include <linux/timer.h>
#include <linux/time.h>
#include <linux/isdnif.h>
#include <linux/irqreturn.h>
#include "message.h"
#include "scioc.h"

#define CHECKRESET_TIME		msecs_to_jiffies(4000)

#define CHECKSTAT_TIME		msecs_to_jiffies(8000)

#define SAR_TIMEOUT		msecs_to_jiffies(10000)

#define IS_VALID_CARD(x)	((x >= 0) && (x <= cinst))

typedef struct {
	int l2_proto;
	int l3_proto;
	char dn[50];
	unsigned long first_sendbuf;	
	unsigned int num_sendbufs;	
	unsigned int free_sendbufs;	
	unsigned int next_sendbuf;	
	char eazlist[50];		
	char sillist[50];		
	int eazclear;			
} bchan;

typedef struct {
	int model;
	int driverId;			
	char devicename[20];		
	isdn_if *card;			
	bchan *channel;			
	char nChannels;			
	unsigned int interrupt;		
	int iobase;			
	int ioport[MAX_IO_REGS];	
	int shmem_pgport;		
	int shmem_magic;		
	unsigned int rambase;		
	unsigned int ramsize;		
	RspMessage async_msg;		
	int want_async_messages;	
	unsigned char seq_no;		
	struct timer_list reset_timer;	
	struct timer_list stat_timer;	
	unsigned char nphystat;		
	unsigned char phystat;		
	HWConfig_pl hwconfig;		
	char load_ver[11];		
	char proc_ver[11];		
	int StartOnReset;		
	int EngineUp;			
	int trace_mode;			
	spinlock_t lock;		
} board;


extern board *sc_adapter[];
extern int cinst;

void memcpy_toshmem(int card, void *dest, const void *src, size_t n);
void memcpy_fromshmem(int card, void *dest, const void *src, size_t n);
int get_card_from_id(int driver);
int indicate_status(int card, int event, ulong Channel, char *Data);
irqreturn_t interrupt_handler(int interrupt, void *cardptr);
int sndpkt(int devId, int channel, int ack, struct sk_buff *data);
void rcvpkt(int card, RspMessage *rcvmsg);
int command(isdn_ctrl *cmd);
int reset(int card);
int startproc(int card);
int send_and_receive(int card, unsigned int procid, unsigned char type,
		     unsigned char class, unsigned char code,
		     unsigned char link, unsigned char data_len,
		     unsigned char *data,  RspMessage *mesgdata, int timeout);
void flushreadfifo(int card);
int sendmessage(int card, unsigned int procid, unsigned int type,
		unsigned int class, unsigned int code, unsigned int link,
		unsigned int data_len, unsigned int *data);
int receivemessage(int card, RspMessage *rspmsg);
int sc_ioctl(int card, scs_ioctl *data);
int setup_buffers(int card, int c);
void sc_check_reset(unsigned long data);
void check_phystat(unsigned long data);

#endif 
