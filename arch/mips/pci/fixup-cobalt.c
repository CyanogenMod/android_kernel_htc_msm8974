/*
 * Cobalt Qube/Raq PCI support
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1996, 1997, 2002, 2003 by Ralf Baechle
 * Copyright (C) 2001, 2002, 2003 by Liam Davies (ldavies@agile.tv)
 */
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/pci.h>
#include <asm/io.h>
#include <asm/gt64120.h>

#include <cobalt.h>
#include <irq.h>

#define COBALT_PCICONF_CPU	0x06
#define COBALT_PCICONF_ETH0	0x07
#define COBALT_PCICONF_RAQSCSI	0x08
#define COBALT_PCICONF_VIA	0x09
#define COBALT_PCICONF_PCISLOT	0x0A
#define COBALT_PCICONF_ETH1	0x0C

#define VIA_COBALT_BRD_ID_REG  0x94
#define VIA_COBALT_BRD_REG_to_ID(reg)	((unsigned char)(reg) >> 4)

static void qube_raq_galileo_early_fixup(struct pci_dev *dev)
{
	if (dev->devfn == PCI_DEVFN(0, 0) &&
		(dev->class >> 8) == PCI_CLASS_MEMORY_OTHER) {

		dev->class = (PCI_CLASS_BRIDGE_HOST << 8) | (dev->class & 0xff);

		printk(KERN_INFO "Galileo: fixed bridge class\n");
	}
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_MARVELL, PCI_DEVICE_ID_MARVELL_GT64111,
	 qube_raq_galileo_early_fixup);

static void qube_raq_via_bmIDE_fixup(struct pci_dev *dev)
{
	unsigned short cfgword;
	unsigned char lt;

	
	pci_read_config_word(dev, PCI_COMMAND, &cfgword);
	cfgword |= (PCI_COMMAND_FAST_BACK | PCI_COMMAND_MASTER);
	pci_write_config_word(dev, PCI_COMMAND, cfgword);

	
	pci_write_config_byte(dev, 0x40, 0xb);

	
	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lt);
	if (lt < 64)
		pci_write_config_byte(dev, PCI_LATENCY_TIMER, 64);
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, 8);
}

DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C586_1,
	 qube_raq_via_bmIDE_fixup);

static void qube_raq_galileo_fixup(struct pci_dev *dev)
{
	if (dev->devfn != PCI_DEVFN(0, 0))
		return;

	pci_write_config_byte(dev, PCI_LATENCY_TIMER, 64);
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, 8);


 	printk(KERN_INFO "Galileo: revision %u\n", dev->revision);

#if 0
	if (dev->revision >= 0x10) {
		
		GT_WRITE(GT_PCI0_TOR_OFS, 0x4020);
	} else if (dev->revision == 0x1 || dev->revision == 0x2)
#endif
	{
		signed int timeo;
		
		timeo = GT_READ(GT_PCI0_TOR_OFS);
		
		GT_WRITE(GT_PCI0_TOR_OFS,
			(0xff << 16) |		
			(0xff << 8) |		
			0xff);			

		
		GT_WRITE(GT_INTRMASK_OFS, GT_INTR_RETRYCTR0_MSK | GT_READ(GT_INTRMASK_OFS));
	}
}

DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_MARVELL, PCI_DEVICE_ID_MARVELL_GT64111,
	 qube_raq_galileo_fixup);

int cobalt_board_id;

static void qube_raq_via_board_id_fixup(struct pci_dev *dev)
{
	u8 id;
	int retval;

	retval = pci_read_config_byte(dev, VIA_COBALT_BRD_ID_REG, &id);
	if (retval) {
		panic("Cannot read board ID");
		return;
	}

	cobalt_board_id = VIA_COBALT_BRD_REG_to_ID(id);

	printk(KERN_INFO "Cobalt board ID: %d\n", cobalt_board_id);
}

DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C586_0,
	 qube_raq_via_board_id_fixup);

static char irq_tab_qube1[] __initdata = {
  [COBALT_PCICONF_CPU]     = 0,
  [COBALT_PCICONF_ETH0]    = QUBE1_ETH0_IRQ,
  [COBALT_PCICONF_RAQSCSI] = SCSI_IRQ,
  [COBALT_PCICONF_VIA]     = 0,
  [COBALT_PCICONF_PCISLOT] = PCISLOT_IRQ,
  [COBALT_PCICONF_ETH1]    = 0
};

static char irq_tab_cobalt[] __initdata = {
  [COBALT_PCICONF_CPU]     = 0,
  [COBALT_PCICONF_ETH0]    = ETH0_IRQ,
  [COBALT_PCICONF_RAQSCSI] = SCSI_IRQ,
  [COBALT_PCICONF_VIA]     = 0,
  [COBALT_PCICONF_PCISLOT] = PCISLOT_IRQ,
  [COBALT_PCICONF_ETH1]    = ETH1_IRQ
};

static char irq_tab_raq2[] __initdata = {
  [COBALT_PCICONF_CPU]     = 0,
  [COBALT_PCICONF_ETH0]    = ETH0_IRQ,
  [COBALT_PCICONF_RAQSCSI] = RAQ2_SCSI_IRQ,
  [COBALT_PCICONF_VIA]     = 0,
  [COBALT_PCICONF_PCISLOT] = PCISLOT_IRQ,
  [COBALT_PCICONF_ETH1]    = ETH1_IRQ
};

int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	if (cobalt_board_id <= COBALT_BRD_ID_QUBE1)
		return irq_tab_qube1[slot];

	if (cobalt_board_id == COBALT_BRD_ID_RAQ2)
		return irq_tab_raq2[slot];

	return irq_tab_cobalt[slot];
}

int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}
