/*
 * HP i8042 System Device Controller -- header
 *
 * Copyright (c) 2001 Brian S. Julin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *
 * References:
 * 
 * HP-HIL Technical Reference Manual.  Hewlett Packard Product No. 45918A
 *
 * System Device Controller Microprocessor Firmware Theory of Operation
 * 	for Part Number 1820-4784 Revision B.  Dwg No. A-1820-4784-2
 *
 */

#ifndef _LINUX_HP_SDC_H
#define _LINUX_HP_SDC_H

#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/timer.h>
#if defined(__hppa__)
#include <asm/hardware.h>
#endif


#define HP_SDC_MAX_REG_DELAY 20000

typedef void (hp_sdc_irqhook) (int irq, void *dev_id, 
			       uint8_t status, uint8_t data);

int hp_sdc_request_timer_irq(hp_sdc_irqhook *callback);
int hp_sdc_request_hil_irq(hp_sdc_irqhook *callback);
int hp_sdc_request_cooked_irq(hp_sdc_irqhook *callback);
int hp_sdc_release_timer_irq(hp_sdc_irqhook *callback);
int hp_sdc_release_hil_irq(hp_sdc_irqhook *callback);
int hp_sdc_release_cooked_irq(hp_sdc_irqhook *callback);

typedef struct {
	int actidx;	
	int idx;	
	int endidx;	
	uint8_t *seq;	
	union {
	  hp_sdc_irqhook   *irqhook;	
	  struct semaphore *semaphore;	
	} act;
} hp_sdc_transaction;
int __hp_sdc_enqueue_transaction(hp_sdc_transaction *this);
int hp_sdc_enqueue_transaction(hp_sdc_transaction *this);
int hp_sdc_dequeue_transaction(hp_sdc_transaction *this);

#define HP_SDC_ACT_PRECMD	0x01		
#define HP_SDC_ACT_DATAREG	0x02		
#define HP_SDC_ACT_DATAOUT	0x04		
#define HP_SDC_ACT_POSTCMD      0x08            
#define HP_SDC_ACT_DATAIN	0x10		
#define HP_SDC_ACT_DURING	0x1f
#define HP_SDC_ACT_SEMAPHORE    0x20            
#define HP_SDC_ACT_CALLBACK	0x40		
#define HP_SDC_ACT_DEALLOC	0x80		
#define HP_SDC_ACT_AFTER	0xe0
#define HP_SDC_ACT_DEAD		0x60		

#define HP_SDC_STATUS_IBF	0x02	

#define HP_SDC_STATUS_IRQMASK	0xf0	
#define HP_SDC_STATUS_PERIODIC  0x10    
#define HP_SDC_STATUS_USERTIMER 0x20    
#define HP_SDC_STATUS_TIMER     0x30    
#define HP_SDC_STATUS_REG	0x40	
#define HP_SDC_STATUS_HILCMD    0x50	
#define HP_SDC_STATUS_HILDATA   0x60	
#define HP_SDC_STATUS_PUP	0x70	
#define HP_SDC_STATUS_KCOOKED	0x80	
#define HP_SDC_STATUS_KRPG	0xc0	
#define HP_SDC_STATUS_KMOD_SUP	0x10	
#define HP_SDC_STATUS_KMOD_CUP	0x20	

#define HP_SDC_NMISTATUS_FHS	0x40	


#define HP_SDC_USE		0x02	
#define HP_SDC_IM		0x04	
#define HP_SDC_CFG		0x11	
#define HP_SDC_KBLANGUAGE	0x12	

#define HP_SDC_D0		0x70	
#define HP_SDC_D1		0x71	
#define HP_SDC_D2		0x72	
#define HP_SDC_D3		0x73	
#define HP_SDC_VT1		0x74	
#define HP_SDC_VT2		0x75	
#define HP_SDC_VT3		0x76	
#define HP_SDC_VT4		0x77	
#define HP_SDC_KBN		0x78	
#define HP_SDC_KBC		0x79	
#define HP_SDC_LPS		0x7a	
#define HP_SDC_LPC		0x7b	
#define HP_SDC_RSV  		0x7c	
#define HP_SDC_LPR		0x7d    
#define HP_SDC_XTD		0x7e    
#define HP_SDC_STR		0x7f    

#define HP_SDC_USE_LOOP		0x04	

#define HP_SDC_IM_MASK          0x1f    
#define HP_SDC_IM_FH		0x10	
#define HP_SDC_IM_PT		0x08	
#define HP_SDC_IM_TIMERS	0x04	
#define HP_SDC_IM_RESET		0x02	
#define HP_SDC_IM_HIL		0x01	

#define HP_SDC_CFG_ROLLOVER	0x08	
#define HP_SDC_CFG_KBD		0x10	
#define HP_SDC_CFG_NEW		0x20	
#define HP_SDC_CFG_KBD_OLD	0x03	
#define HP_SDC_CFG_KBD_NEW	0x07	
#define HP_SDC_CFG_REV		0x40	
#define HP_SDC_CFG_IDPROM	0x80	

#define HP_SDC_LPS_NDEV		0x07	
#define HP_SDC_LPS_ACSUCC	0x08	
#define HP_SDC_LPS_ACFAIL	0x80	

#define HP_SDC_LPC_APE_IPF	0x01	
#define HP_SDC_LPC_ARCONERR	0x02	
#define HP_SDC_LPC_ARCQUIET	0x03	
#define HP_SDC_LPC_COOK		0x10	
#define HP_SDC_LPC_RC		0x80	

#define HP_SDC_XTD_REV		0x07	
#define HP_SDC_XTD_REV_STRINGS(val, str) \
switch (val) {						\
	case 0x1: str = "1820-3712"; break;		\
	case 0x2: str = "1820-4379"; break;		\
	case 0x3: str = "1820-4784"; break;		\
	default: str = "unknown";			\
};
#define HP_SDC_XTD_BEEPER	0x08	
#define HP_SDC_XTD_BBRTC	0x20	

#define HP_SDC_CMD_LOAD_RT	0x31	
#define HP_SDC_CMD_LOAD_FHS	0x36	
#define HP_SDC_CMD_LOAD_MT	0x38	
#define HP_SDC_CMD_LOAD_DT	0x3B	
#define HP_SDC_CMD_LOAD_CT	0x3E	

#define HP_SDC_CMD_SET_IM	0x40    

#define HP_SDC_CMD_READ_RAM	0x00	
#define HP_SDC_CMD_READ_USE	0x02	
#define HP_SDC_CMD_READ_IM	0x04	
#define HP_SDC_CMD_READ_KCC	0x11	
#define HP_SDC_CMD_READ_KLC	0x12	
#define HP_SDC_CMD_READ_T1	0x13	
#define HP_SDC_CMD_READ_T2	0x14	
#define HP_SDC_CMD_READ_T3	0x15	
#define HP_SDC_CMD_READ_T4	0x16	
#define HP_SDC_CMD_READ_T5	0x17	
#define HP_SDC_CMD_READ_D0	0xf0	
#define HP_SDC_CMD_READ_D1	0xf1	
#define HP_SDC_CMD_READ_D2	0xf2	
#define HP_SDC_CMD_READ_D3	0xf3	
#define HP_SDC_CMD_READ_VT1	0xf4	
#define HP_SDC_CMD_READ_VT2	0xf5	
#define HP_SDC_CMD_READ_VT3	0xf6	
#define HP_SDC_CMD_READ_VT4	0xf7	
#define HP_SDC_CMD_READ_KBN	0xf8	
#define HP_SDC_CMD_READ_KBC	0xf9	
#define HP_SDC_CMD_READ_LPS	0xfa	
#define HP_SDC_CMD_READ_LPC	0xfb	
#define HP_SDC_CMD_READ_RSV	0xfc	
#define HP_SDC_CMD_READ_LPR	0xfd	
#define HP_SDC_CMD_READ_XTD	0xfe	
#define HP_SDC_CMD_READ_STR	0xff	

#define HP_SDC_CMD_SET_ARD	0xA0	
#define HP_SDC_CMD_SET_ARR	0xA2	
#define HP_SDC_CMD_SET_BELL	0xA3	
#define HP_SDC_CMD_SET_RPGR	0xA6	
#define HP_SDC_CMD_SET_RTMS	0xAD	
#define HP_SDC_CMD_SET_RTD	0xAF	
#define HP_SDC_CMD_SET_FHS	0xB2	
#define HP_SDC_CMD_SET_MT	0xB4	
#define HP_SDC_CMD_SET_DT	0xB7	
#define HP_SDC_CMD_SET_CT	0xBA	
#define HP_SDC_CMD_SET_RAMP	0xC1	
#define HP_SDC_CMD_SET_D0	0xe0	
#define HP_SDC_CMD_SET_D1	0xe1	
#define HP_SDC_CMD_SET_D2	0xe2	
#define HP_SDC_CMD_SET_D3	0xe3	
#define HP_SDC_CMD_SET_VT1	0xe4	
#define HP_SDC_CMD_SET_VT2	0xe5	
#define HP_SDC_CMD_SET_VT3	0xe6	
#define HP_SDC_CMD_SET_VT4	0xe7	
#define HP_SDC_CMD_SET_KBN	0xe8	
#define HP_SDC_CMD_SET_KBC	0xe9	
#define HP_SDC_CMD_SET_LPS	0xea	
#define HP_SDC_CMD_SET_LPC	0xeb	
#define HP_SDC_CMD_SET_RSV	0xec	
#define HP_SDC_CMD_SET_LPR	0xed	
#define HP_SDC_CMD_SET_XTD	0xee	
#define HP_SDC_CMD_SET_STR	0xef	

#define HP_SDC_CMD_DO_RTCW	0xc2	
#define HP_SDC_CMD_DO_RTCR	0xc3	
#define HP_SDC_CMD_DO_BEEP	0xc4	
#define HP_SDC_CMD_DO_HIL	0xc5	

#define HP_SDC_DATA		0x40	
#define HP_SDC_HIL_CMD		0x50	
#define HP_SDC_HIL_R1MASK	0x0f	
#define HP_SDC_HIL_AUTO		0x10	   
#define HP_SDC_HIL_ISERR	0x80	
#define HP_SDC_HIL_RC_DONE	0x80	
#define HP_SDC_HIL_ERR		0x81	
#define HP_SDC_HIL_TO		0x82	
#define HP_SDC_HIL_RC		0x84	
#define HP_SDC_HIL_DAT		0x60	


typedef struct {
	rwlock_t	ibf_lock;
	rwlock_t	lock;		
	rwlock_t	rtq_lock;	
	rwlock_t	hook_lock;	

	unsigned int	irq, nmi;	
	unsigned long	base_io, status_io, data_io; 

	uint8_t		im;		
	int		set_im; 	

	int		ibf;		
	uint8_t		wi;		
	uint8_t		r7[4];          
	uint8_t		r11, r7e;	

	hp_sdc_irqhook	*timer, *reg, *hil, *pup, *cooked;

#define HP_SDC_QUEUE_LEN 16
	hp_sdc_transaction *tq[HP_SDC_QUEUE_LEN]; 

	int		rcurr, rqty;	
	struct timeval	rtv;		
	int		wcurr;		

	int		dev_err;	
#if defined(__hppa__)
	struct parisc_device	*dev;
#elif defined(__mc68000__)
	void		*dev;
#else
#error No support for device registration on this arch yet.
#endif

	struct timer_list kicker;	
	struct tasklet_struct	task;

} hp_i8042_sdc;

#endif 
