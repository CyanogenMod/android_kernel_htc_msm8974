/*
 *  linux/drivers/s390/crypto/zcrypt_api.h
 *
 *  zcrypt 2.1.0
 *
 *  Copyright (C)  2001, 2006 IBM Corporation
 *  Author(s): Robert Burroughs
 *	       Eric Rossman (edrossma@us.ibm.com)
 *	       Cornelia Huck <cornelia.huck@de.ibm.com>
 *
 *  Hotplug & misc device support: Jochen Roehrig (roehrig@de.ibm.com)
 *  Major cleanup & driver split: Martin Schwidefsky <schwidefsky@de.ibm.com>
 *				  Ralph Wuerthner <rwuerthn@de.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _ZCRYPT_API_H_
#define _ZCRYPT_API_H_

#include "ap_bus.h"
#include <asm/zcrypt.h>

#define ICAZ90STATUS		_IOR(ZCRYPT_IOCTL_MAGIC, 0x10, struct ica_z90_status)
#define Z90STAT_PCIXCCCOUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x43, int)

struct ica_z90_status {
	int totalcount;
	int leedslitecount; 
	int leeds2count;    
	
	int requestqWaitCount;
	int pendingqWaitCount;
	int totalOpenCount;
	int cryptoDomain;
	
	
	unsigned char status[64];
	
	unsigned char qdepth[64];
};

/**
 * device type for an actual device is either PCICA, PCICC, PCIXCC_MCL2,
 * PCIXCC_MCL3, CEX2C, or CEX2A
 *
 * NOTE: PCIXCC_MCL3 refers to a PCIXCC with May 2004 version of Licensed
 *	 Internal Code (LIC) (EC J12220 level 29).
 *	 PCIXCC_MCL2 refers to any LIC before this level.
 */
#define ZCRYPT_PCICA		1
#define ZCRYPT_PCICC		2
#define ZCRYPT_PCIXCC_MCL2	3
#define ZCRYPT_PCIXCC_MCL3	4
#define ZCRYPT_CEX2C		5
#define ZCRYPT_CEX2A		6
#define ZCRYPT_CEX3C		7
#define ZCRYPT_CEX3A		8

#define ZCRYPT_RNG_BUFFER_SIZE	4096

struct zcrypt_device;

struct zcrypt_ops {
	long (*rsa_modexpo)(struct zcrypt_device *, struct ica_rsa_modexpo *);
	long (*rsa_modexpo_crt)(struct zcrypt_device *,
				struct ica_rsa_modexpo_crt *);
	long (*send_cprb)(struct zcrypt_device *, struct ica_xcRB *);
	long (*rng)(struct zcrypt_device *, char *);
};

struct zcrypt_device {
	struct list_head list;		
	spinlock_t lock;		
	struct kref refcount;		
	struct ap_device *ap_dev;	
	struct zcrypt_ops *ops;		
	int online;			

	int user_space_type;		
	char *type_string;		
	int min_mod_size;		
	int max_mod_size;		
	int short_crt;			
	int speed_rating;		

	int request_count;		

	struct ap_message reply;	
	int max_exp_bit_length;
};

struct zcrypt_device *zcrypt_device_alloc(size_t);
void zcrypt_device_free(struct zcrypt_device *);
void zcrypt_device_get(struct zcrypt_device *);
int zcrypt_device_put(struct zcrypt_device *);
int zcrypt_device_register(struct zcrypt_device *);
void zcrypt_device_unregister(struct zcrypt_device *);
int zcrypt_api_init(void);
void zcrypt_api_exit(void);

#endif 
