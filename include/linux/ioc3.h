/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2005 Stanislaw Skowronek <skylark@linux-mips.org>
 */

#ifndef _LINUX_IOC3_H
#define _LINUX_IOC3_H

#include <asm/sn/ioc3.h>

#define IOC3_MAX_SUBMODULES	32

#define IOC3_CLASS_NONE		0
#define IOC3_CLASS_BASE_IP27	1
#define IOC3_CLASS_BASE_IP30	2
#define IOC3_CLASS_MENET_123	3
#define IOC3_CLASS_MENET_4	4
#define IOC3_CLASS_CADDUO	5
#define IOC3_CLASS_SERIAL	6

struct ioc3_driver_data {
	struct list_head list;
	int id;				
	
	unsigned long pma;		
	struct ioc3 __iomem *vma;	
	struct pci_dev *pdev;		
	
	int dual_irq;			
	int irq_io, irq_eth;		
	
	spinlock_t gpio_lock;
	unsigned int gpdr_shadow;
	
	char nic_part[32];
	char nic_serial[16];
	char nic_mac[6];
	
	int class;
	void *data[IOC3_MAX_SUBMODULES];	
	int active[IOC3_MAX_SUBMODULES];	
	spinlock_t ir_lock;	
};

struct ioc3_submodule {
	char *name;		
	struct module *owner;	
	int ethernet;		
	int (*probe) (struct ioc3_submodule *, struct ioc3_driver_data *);
	int (*remove) (struct ioc3_submodule *, struct ioc3_driver_data *);
	int id;			
	
	unsigned int irq_mask;	
	int reset_mask;		
	int (*intr) (struct ioc3_submodule *, struct ioc3_driver_data *, unsigned int);
	
	void *data;		
};


#define IOC3_W_IES		0
#define IOC3_W_IEC		1

extern int ioc3_register_submodule(struct ioc3_submodule *);
extern void ioc3_unregister_submodule(struct ioc3_submodule *);
extern void ioc3_enable(struct ioc3_submodule *, struct ioc3_driver_data *, unsigned int);
extern void ioc3_ack(struct ioc3_submodule *, struct ioc3_driver_data *, unsigned int);
extern void ioc3_disable(struct ioc3_submodule *, struct ioc3_driver_data *, unsigned int);
extern void ioc3_gpcr_set(struct ioc3_driver_data *, unsigned int);
extern void ioc3_write_ireg(struct ioc3_driver_data *idd, uint32_t value, int reg);

#endif
