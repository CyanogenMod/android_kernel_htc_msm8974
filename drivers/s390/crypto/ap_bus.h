/*
 * linux/drivers/s390/crypto/ap_bus.h
 *
 * Copyright (C) 2006 IBM Corporation
 * Author(s): Cornelia Huck <cornelia.huck@de.ibm.com>
 *	      Martin Schwidefsky <schwidefsky@de.ibm.com>
 *	      Ralph Wuerthner <rwuerthn@de.ibm.com>
 *	      Felix Beck <felix.beck@de.ibm.com>
 *	      Holger Dengler <hd@linux.vnet.ibm.com>
 *
 * Adjunct processor bus header file.
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

#ifndef _AP_BUS_H_
#define _AP_BUS_H_

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/types.h>

#define AP_DEVICES 64		
#define AP_DOMAINS 16		
#define AP_MAX_RESET 90		
#define AP_RESET_TIMEOUT (HZ/2)	
#define AP_CONFIG_TIME 30	
#define AP_POLL_TIME 1		

extern int ap_domain_index;

typedef unsigned int ap_qid_t;

#define AP_MKQID(_device,_queue) (((_device) & 63) << 8 | ((_queue) & 15))
#define AP_QID_DEVICE(_qid) (((_qid) >> 8) & 63)
#define AP_QID_QUEUE(_qid) ((_qid) & 15)

struct ap_queue_status {
	unsigned int queue_empty	: 1;
	unsigned int replies_waiting	: 1;
	unsigned int queue_full		: 1;
	unsigned int pad1		: 4;
	unsigned int int_enabled	: 1;
	unsigned int response_code	: 8;
	unsigned int pad2		: 16;
} __packed;

#define AP_QUEUE_STATUS_INVALID \
		{ 1, 1, 1, 0xF, 1, 0xFF, 0xFFFF }

static inline
int ap_queue_status_invalid_test(struct ap_queue_status *status)
{
	struct ap_queue_status invalid = AP_QUEUE_STATUS_INVALID;
	return !(memcmp(status, &invalid, sizeof(struct ap_queue_status)));
}

#define MAX_AP_FACILITY 31

static inline int test_ap_facility(unsigned int function, unsigned int nr)
{
	if (nr > MAX_AP_FACILITY)
		return 0;
	return function & (unsigned int)(0x80000000 >> nr);
}

#define AP_RESPONSE_NORMAL		0x00
#define AP_RESPONSE_Q_NOT_AVAIL		0x01
#define AP_RESPONSE_RESET_IN_PROGRESS	0x02
#define AP_RESPONSE_DECONFIGURED	0x03
#define AP_RESPONSE_CHECKSTOPPED	0x04
#define AP_RESPONSE_BUSY		0x05
#define AP_RESPONSE_INVALID_ADDRESS	0x06
#define AP_RESPONSE_OTHERWISE_CHANGED	0x07
#define AP_RESPONSE_Q_FULL		0x10
#define AP_RESPONSE_NO_PENDING_REPLY	0x10
#define AP_RESPONSE_INDEX_TOO_BIG	0x11
#define AP_RESPONSE_NO_FIRST_PART	0x13
#define AP_RESPONSE_MESSAGE_TOO_BIG	0x15
#define AP_RESPONSE_REQ_FAC_NOT_INST	0x16

#define AP_DEVICE_TYPE_PCICC	3
#define AP_DEVICE_TYPE_PCICA	4
#define AP_DEVICE_TYPE_PCIXCC	5
#define AP_DEVICE_TYPE_CEX2A	6
#define AP_DEVICE_TYPE_CEX2C	7
#define AP_DEVICE_TYPE_CEX3A	8
#define AP_DEVICE_TYPE_CEX3C	9

#define AP_RESET_IGNORE	0	
#define AP_RESET_ARMED	1	
#define AP_RESET_DO	2	

struct ap_device;
struct ap_message;

struct ap_driver {
	struct device_driver driver;
	struct ap_device_id *ids;

	int (*probe)(struct ap_device *);
	void (*remove)(struct ap_device *);
	
	void (*receive)(struct ap_device *, struct ap_message *,
			struct ap_message *);
	int request_timeout;		
};

#define to_ap_drv(x) container_of((x), struct ap_driver, driver)

int ap_driver_register(struct ap_driver *, struct module *, char *);
void ap_driver_unregister(struct ap_driver *);

struct ap_device {
	struct device device;
	struct ap_driver *drv;		
	spinlock_t lock;		
	struct list_head list;		

	ap_qid_t qid;			
	int queue_depth;		
	int device_type;		
	int unregistered;		
	struct timer_list timeout;	
	int reset;			

	int queue_count;		

	struct list_head pendingq;	
	int pendingq_count;		
	struct list_head requestq;	
	int requestq_count;		
	int total_request_count;	

	struct ap_message *reply;	

	void *private;			
};

#define to_ap_dev(x) container_of((x), struct ap_device, device)

struct ap_message {
	struct list_head list;		
	unsigned long long psmid;	
	void *message;			
	size_t length;			

	void *private;			
	unsigned int special:1;		
};

#define AP_DEVICE(dt)					\
	.dev_type=(dt),					\
	.match_flags=AP_DEVICE_ID_MATCH_DEVICE_TYPE,

static inline void ap_init_message(struct ap_message *ap_msg)
{
	ap_msg->psmid = 0;
	ap_msg->length = 0;
	ap_msg->special = 0;
}

int ap_send(ap_qid_t, unsigned long long, void *, size_t);
int ap_recv(ap_qid_t, unsigned long long *, void *, size_t);

void ap_queue_message(struct ap_device *ap_dev, struct ap_message *ap_msg);
void ap_cancel_message(struct ap_device *ap_dev, struct ap_message *ap_msg);
void ap_flush_queue(struct ap_device *ap_dev);

int ap_module_init(void);
void ap_module_exit(void);

int ap_4096_commands_available(ap_qid_t qid);

#endif 
