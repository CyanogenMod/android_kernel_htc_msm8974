/* ASB2305 Arch-specific PCI declarations
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 * Derived from: arch/i386/kernel/pci-i386.h: (c) 1999 Martin Mares <mj@ucw.cz>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _PCI_ASB2305_H
#define _PCI_ASB2305_H

#undef DEBUG

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

#define PCI_PROBE_BIOS 1
#define PCI_PROBE_CONF1 2
#define PCI_PROBE_CONF2 4
#define PCI_NO_CHECKS 0x400
#define PCI_ASSIGN_ROMS 0x1000
#define PCI_BIOS_IRQ_SCAN 0x2000

extern unsigned int pci_probe;


extern void pcibios_resource_survey(void);


extern int pcibios_last_bus;
extern struct pci_bus *pci_root_bus;
extern struct pci_ops *pci_root_ops;

extern struct irq_routing_table *pcibios_get_irq_routing_table(void);
extern int pcibios_set_irq_routing(struct pci_dev *dev, int pin, int irq);


struct irq_info {
	u8 bus, devfn;			
	struct {
		u8 link;		
		u16 bitmap;		
	} __attribute__((packed)) irq[4];
	u8 slot;			
	u8 rfu;
} __attribute__((packed));

struct irq_routing_table {
	u32 signature;			
	u16 version;			
	u16 size;			
	u8 rtr_bus, rtr_devfn;		
	u16 exclusive_irqs;		
	u16 rtr_vendor, rtr_device;	
	u32 miniport_data;		
	u8 rfu[11];
	u8 checksum;			
	struct irq_info slots[0];
} __attribute__((packed));

extern unsigned int pcibios_irq_mask;

extern void pcibios_irq_init(void);
extern void pcibios_fixup_irqs(void);
extern void pcibios_enable_irq(struct pci_dev *dev);

#endif 
