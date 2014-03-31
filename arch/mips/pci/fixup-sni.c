/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * SNI specific PCI support for RM200/RM300.
 *
 * Copyright (C) 1997 - 2000, 2003, 04 Ralf Baechle (ralf@linux-mips.org)
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>

#include <asm/mipsregs.h>
#include <asm/sni.h>

#include <irq.h>

#define SCSI	PCIMT_IRQ_SCSI
#define ETH	PCIMT_IRQ_ETHERNET
#define INTA	PCIMT_IRQ_INTA
#define INTB	PCIMT_IRQ_INTB
#define INTC	PCIMT_IRQ_INTC
#define INTD	PCIMT_IRQ_INTD

static char irq_tab_rm200[8][5] __initdata = {
	
	{     0,    0,    0,    0,    0 },	
	{  SCSI, SCSI, SCSI, SCSI, SCSI },	
	{   ETH,  ETH,  ETH,  ETH,  ETH },	
	{  INTB, INTB, INTB, INTB, INTB },	
	{     0,    0,    0,    0,    0 },	
	{     0, INTB, INTC, INTD, INTA },	
	{     0, INTC, INTD, INTA, INTB },	
	{     0, INTD, INTA, INTB, INTC },	
};

static char irq_tab_rm300d[8][5] __initdata = {
	
	{     0,    0,    0,    0,    0 },	
	{  SCSI, SCSI, SCSI, SCSI, SCSI },	
	{     0, INTC, INTD, INTA, INTB },	
	{  INTB, INTB, INTB, INTB, INTB },	
	{     0,    0,    0,    0,    0 },	
	{     0, INTB, INTC, INTD, INTA },	
	{     0, INTC, INTD, INTA, INTB },	
	{     0, INTD, INTA, INTB, INTC },	
};

static char irq_tab_rm300e[5][5] __initdata = {
	
	{     0,    0,    0,    0,    0 },	
	{  SCSI, SCSI, SCSI, SCSI, SCSI },	
	{     0, INTC, INTD, INTA, INTB },	
	{     0, INTD, INTA, INTB, INTC },	
	{     0, INTA, INTB, INTC, INTD },	
};
#undef SCSI
#undef ETH
#undef INTA
#undef INTB
#undef INTC
#undef INTD


#define SCSI0	PCIT_IRQ_SCSI0
#define SCSI1	PCIT_IRQ_SCSI1
#define ETH	PCIT_IRQ_ETHERNET
#define INTA	PCIT_IRQ_INTA
#define INTB	PCIT_IRQ_INTB
#define INTC	PCIT_IRQ_INTC
#define INTD	PCIT_IRQ_INTD

static char irq_tab_pcit[13][5] __initdata = {
	
	{     0,     0,     0,     0,     0 },	
	{ SCSI0, SCSI0, SCSI0, SCSI0, SCSI0 },	
	{ SCSI1, SCSI1, SCSI1, SCSI1, SCSI1 },	
	{   ETH,   ETH,   ETH,   ETH,   ETH },	
	{     0,  INTA,  INTB,  INTC,  INTD },	
	{     0,     0,     0,     0,     0 },	
	{     0,     0,     0,     0,     0 },	
	{     0,     0,     0,     0,     0 },	
	{     0,  INTA,  INTB,  INTC,  INTD },	
	{     0,  INTB,  INTC,  INTD,  INTA },	
	{     0,  INTC,  INTD,  INTA,  INTB },	
	{     0,  INTD,  INTA,  INTB,  INTC },	
	{     0,  INTA,  INTB,  INTC,  INTD },	
};

static char irq_tab_pcit_cplus[13][5] __initdata = {
	
	{     0,     0,     0,     0,     0 },	
	{     0,  INTB,  INTC,  INTD,  INTA },	
	{     0,     0,     0,     0,     0 },	
	{     0,     0,     0,     0,     0 },	
	{     0,  INTA,  INTB,  INTC,  INTD },	
	{     0,  INTB,  INTC,  INTD,  INTA },	
};

static inline int is_rm300_revd(void)
{
	unsigned char csmsr = *(volatile unsigned char *)PCIMT_CSMSR;

	return (csmsr & 0xa0) == 0x20;
}

int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	switch (sni_brd_type) {
	case SNI_BRD_PCI_TOWER_CPLUS:
		if (slot == 4) {
			while (dev && dev->bus->number != 1)
				dev = dev->bus->self;
			if (dev && dev->devfn >= PCI_DEVFN(4, 0))
				slot = 5;
		}
		return irq_tab_pcit_cplus[slot][pin];
	case SNI_BRD_PCI_TOWER:
	        return irq_tab_pcit[slot][pin];

	case SNI_BRD_PCI_MTOWER:
	        if (is_rm300_revd())
		        return irq_tab_rm300d[slot][pin];
	        

	case SNI_BRD_PCI_DESKTOP:
	        return irq_tab_rm200[slot][pin];

	case SNI_BRD_PCI_MTOWER_CPLUS:
	        return irq_tab_rm300e[slot][pin];
	}

	return 0;
}

int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}
