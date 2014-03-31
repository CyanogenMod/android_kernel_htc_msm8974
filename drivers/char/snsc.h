/*
 * SN Platform system controller communication support
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2006 Silicon Graphics, Inc. All rights reserved.
 */


#ifndef _SN_SYSCTL_H_
#define _SN_SYSCTL_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <asm/sn/types.h>

#define CHUNKSIZE 127

struct subch_data_s {
	nasid_t sd_nasid;	
	int sd_subch;		
	spinlock_t sd_rlock;	
	spinlock_t sd_wlock;	
	wait_queue_head_t sd_rq;	
	wait_queue_head_t sd_wq;	
	struct semaphore sd_rbs;	
	struct semaphore sd_wbs;	

	char sd_rb[CHUNKSIZE];	
	char sd_wb[CHUNKSIZE];	
};

struct sysctl_data_s {
	struct cdev scd_cdev;	
	nasid_t scd_nasid;	
};


#define IR_ARG_INT              0x00    
#define IR_ARG_ASCII            0x01    
#define IR_ARG_UNKNOWN          0x80    
#define IR_ARG_UNKNOWN_LENGTH_MASK	0x7f


#define EV_CLASS_MASK		0xf000ul
#define EV_SEVERITY_MASK	0x0f00ul
#define EV_COMPONENT_MASK	0x00fful

#define EV_CLASS_POWER		0x1000ul
#define EV_CLASS_FAN		0x2000ul
#define EV_CLASS_TEMP		0x3000ul
#define EV_CLASS_ENV		0x4000ul
#define EV_CLASS_TEST_FAULT	0x5000ul
#define EV_CLASS_TEST_WARNING	0x6000ul
#define EV_CLASS_PWRD_NOTIFY	0x8000ul

#define ENV_PWRDN_PEND		0x4101ul

#define EV_SEVERITY_POWER_STABLE	0x0000ul
#define EV_SEVERITY_POWER_LOW_WARNING	0x0100ul
#define EV_SEVERITY_POWER_HIGH_WARNING	0x0200ul
#define EV_SEVERITY_POWER_HIGH_FAULT	0x0300ul
#define EV_SEVERITY_POWER_LOW_FAULT	0x0400ul

#define EV_SEVERITY_FAN_STABLE		0x0000ul
#define EV_SEVERITY_FAN_WARNING		0x0100ul
#define EV_SEVERITY_FAN_FAULT		0x0200ul

#define EV_SEVERITY_TEMP_STABLE		0x0000ul
#define EV_SEVERITY_TEMP_ADVISORY	0x0100ul
#define EV_SEVERITY_TEMP_CRITICAL	0x0200ul
#define EV_SEVERITY_TEMP_FAULT		0x0300ul

void scdrv_event_init(struct sysctl_data_s *);

#endif 
