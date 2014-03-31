/*
 *  Probe module for 8250/16550-type PCI serial ports.
 *
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o.
 *
 *  Copyright (C) 2001 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/tty.h>
#include <linux/serial_core.h>
#include <linux/8250_pci.h>
#include <linux/bitops.h>

#include <asm/byteorder.h>
#include <asm/io.h>

#include "8250.h"

#undef SERIAL_DEBUG_PCI

struct pci_serial_quirk {
	u32	vendor;
	u32	device;
	u32	subvendor;
	u32	subdevice;
	int	(*probe)(struct pci_dev *dev);
	int	(*init)(struct pci_dev *dev);
	int	(*setup)(struct serial_private *,
			 const struct pciserial_board *,
			 struct uart_port *, int);
	void	(*exit)(struct pci_dev *dev);
};

#define PCI_NUM_BAR_RESOURCES	6

struct serial_private {
	struct pci_dev		*dev;
	unsigned int		nr;
	void __iomem		*remapped_bar[PCI_NUM_BAR_RESOURCES];
	struct pci_serial_quirk	*quirk;
	int			line[0];
};

static int pci_default_setup(struct serial_private*,
	  const struct pciserial_board*, struct uart_port*, int);

static void moan_device(const char *str, struct pci_dev *dev)
{
	printk(KERN_WARNING
	       "%s: %s\n"
	       "Please send the output of lspci -vv, this\n"
	       "message (0x%04x,0x%04x,0x%04x,0x%04x), the\n"
	       "manufacturer and name of serial board or\n"
	       "modem board to rmk+serial@arm.linux.org.uk.\n",
	       pci_name(dev), str, dev->vendor, dev->device,
	       dev->subsystem_vendor, dev->subsystem_device);
}

static int
setup_port(struct serial_private *priv, struct uart_port *port,
	   int bar, int offset, int regshift)
{
	struct pci_dev *dev = priv->dev;
	unsigned long base, len;

	if (bar >= PCI_NUM_BAR_RESOURCES)
		return -EINVAL;

	base = pci_resource_start(dev, bar);

	if (pci_resource_flags(dev, bar) & IORESOURCE_MEM) {
		len =  pci_resource_len(dev, bar);

		if (!priv->remapped_bar[bar])
			priv->remapped_bar[bar] = ioremap_nocache(base, len);
		if (!priv->remapped_bar[bar])
			return -ENOMEM;

		port->iotype = UPIO_MEM;
		port->iobase = 0;
		port->mapbase = base + offset;
		port->membase = priv->remapped_bar[bar] + offset;
		port->regshift = regshift;
	} else {
		port->iotype = UPIO_PORT;
		port->iobase = base + offset;
		port->mapbase = 0;
		port->membase = NULL;
		port->regshift = 0;
	}
	return 0;
}

static int addidata_apci7800_setup(struct serial_private *priv,
				const struct pciserial_board *board,
				struct uart_port *port, int idx)
{
	unsigned int bar = 0, offset = board->first_offset;
	bar = FL_GET_BASE(board->flags);

	if (idx < 2) {
		offset += idx * board->uart_offset;
	} else if ((idx >= 2) && (idx < 4)) {
		bar += 1;
		offset += ((idx - 2) * board->uart_offset);
	} else if ((idx >= 4) && (idx < 6)) {
		bar += 2;
		offset += ((idx - 4) * board->uart_offset);
	} else if (idx >= 6) {
		bar += 3;
		offset += ((idx - 6) * board->uart_offset);
	}

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

static int
afavlab_setup(struct serial_private *priv, const struct pciserial_board *board,
	      struct uart_port *port, int idx)
{
	unsigned int bar, offset = board->first_offset;

	bar = FL_GET_BASE(board->flags);
	if (idx < 4)
		bar += idx;
	else {
		bar = 4;
		offset += (idx - 4) * board->uart_offset;
	}

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

static int pci_hp_diva_init(struct pci_dev *dev)
{
	int rc = 0;

	switch (dev->subsystem_device) {
	case PCI_DEVICE_ID_HP_DIVA_TOSCA1:
	case PCI_DEVICE_ID_HP_DIVA_HALFDOME:
	case PCI_DEVICE_ID_HP_DIVA_KEYSTONE:
	case PCI_DEVICE_ID_HP_DIVA_EVEREST:
		rc = 3;
		break;
	case PCI_DEVICE_ID_HP_DIVA_TOSCA2:
		rc = 2;
		break;
	case PCI_DEVICE_ID_HP_DIVA_MAESTRO:
		rc = 4;
		break;
	case PCI_DEVICE_ID_HP_DIVA_POWERBAR:
	case PCI_DEVICE_ID_HP_DIVA_HURRICANE:
		rc = 1;
		break;
	}

	return rc;
}

static int
pci_hp_diva_setup(struct serial_private *priv,
		const struct pciserial_board *board,
		struct uart_port *port, int idx)
{
	unsigned int offset = board->first_offset;
	unsigned int bar = FL_GET_BASE(board->flags);

	switch (priv->dev->subsystem_device) {
	case PCI_DEVICE_ID_HP_DIVA_MAESTRO:
		if (idx == 3)
			idx++;
		break;
	case PCI_DEVICE_ID_HP_DIVA_EVEREST:
		if (idx > 0)
			idx++;
		if (idx > 2)
			idx++;
		break;
	}
	if (idx > 2)
		offset = 0x18;

	offset += idx * board->uart_offset;

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

static int pci_inteli960ni_init(struct pci_dev *dev)
{
	unsigned long oldval;

	if (!(dev->subsystem_device & 0x1000))
		return -ENODEV;

	
	pci_read_config_dword(dev, 0x44, (void *)&oldval);
	if (oldval == 0x00001000L) { 
		printk(KERN_DEBUG "Local i960 firmware missing");
		return -ENODEV;
	}
	return 0;
}

static int pci_plx9050_init(struct pci_dev *dev)
{
	u8 irq_config;
	void __iomem *p;

	if ((pci_resource_flags(dev, 0) & IORESOURCE_MEM) == 0) {
		moan_device("no memory in bar 0", dev);
		return 0;
	}

	irq_config = 0x41;
	if (dev->vendor == PCI_VENDOR_ID_PANACOM ||
	    dev->subsystem_vendor == PCI_SUBVENDOR_ID_EXSYS)
		irq_config = 0x43;

	if ((dev->vendor == PCI_VENDOR_ID_PLX) &&
	    (dev->device == PCI_DEVICE_ID_PLX_ROMULUS))
		irq_config = 0x5b;
	p = ioremap_nocache(pci_resource_start(dev, 0), 0x80);
	if (p == NULL)
		return -ENOMEM;
	writel(irq_config, p + 0x4c);

	readl(p + 0x4c);
	iounmap(p);

	return 0;
}

static void __devexit pci_plx9050_exit(struct pci_dev *dev)
{
	u8 __iomem *p;

	if ((pci_resource_flags(dev, 0) & IORESOURCE_MEM) == 0)
		return;

	p = ioremap_nocache(pci_resource_start(dev, 0), 0x80);
	if (p != NULL) {
		writel(0, p + 0x4c);

		readl(p + 0x4c);
		iounmap(p);
	}
}

#define NI8420_INT_ENABLE_REG	0x38
#define NI8420_INT_ENABLE_BIT	0x2000

static void __devexit pci_ni8420_exit(struct pci_dev *dev)
{
	void __iomem *p;
	unsigned long base, len;
	unsigned int bar = 0;

	if ((pci_resource_flags(dev, bar) & IORESOURCE_MEM) == 0) {
		moan_device("no memory in bar", dev);
		return;
	}

	base = pci_resource_start(dev, bar);
	len =  pci_resource_len(dev, bar);
	p = ioremap_nocache(base, len);
	if (p == NULL)
		return;

	
	writel(readl(p + NI8420_INT_ENABLE_REG) & ~(NI8420_INT_ENABLE_BIT),
	       p + NI8420_INT_ENABLE_REG);
	iounmap(p);
}


#define MITE_IOWBSR1	0xc4
#define MITE_IOWCR1	0xf4
#define MITE_LCIMR1	0x08
#define MITE_LCIMR2	0x10

#define MITE_LCIMR2_CLR_CPU_IE	(1 << 30)

static void __devexit pci_ni8430_exit(struct pci_dev *dev)
{
	void __iomem *p;
	unsigned long base, len;
	unsigned int bar = 0;

	if ((pci_resource_flags(dev, bar) & IORESOURCE_MEM) == 0) {
		moan_device("no memory in bar", dev);
		return;
	}

	base = pci_resource_start(dev, bar);
	len =  pci_resource_len(dev, bar);
	p = ioremap_nocache(base, len);
	if (p == NULL)
		return;

	
	writel(MITE_LCIMR2_CLR_CPU_IE, p + MITE_LCIMR2);
	iounmap(p);
}

static int
sbs_setup(struct serial_private *priv, const struct pciserial_board *board,
		struct uart_port *port, int idx)
{
	unsigned int bar, offset = board->first_offset;

	bar = 0;

	if (idx < 4) {
		
		offset += idx * board->uart_offset;
	} else if (idx < 8) {
		
		offset += idx * board->uart_offset + 0xC00;
	} else 
		return 1;

	return setup_port(priv, port, bar, offset, board->reg_shift);
}


#define OCT_REG_CR_OFF		0x500

static int sbs_init(struct pci_dev *dev)
{
	u8 __iomem *p;

	p = pci_ioremap_bar(dev, 0);

	if (p == NULL)
		return -ENOMEM;
	
	writeb(0x10, p + OCT_REG_CR_OFF);
	udelay(50);
	writeb(0x0, p + OCT_REG_CR_OFF);

	
	writeb(0x4, p + OCT_REG_CR_OFF);
	iounmap(p);

	return 0;
}


static void __devexit sbs_exit(struct pci_dev *dev)
{
	u8 __iomem *p;

	p = pci_ioremap_bar(dev, 0);
	
	if (p != NULL)
		writeb(0, p + OCT_REG_CR_OFF);
	iounmap(p);
}


#define PCI_DEVICE_ID_SIIG_1S_10x (PCI_DEVICE_ID_SIIG_1S_10x_550 & 0xfffc)
#define PCI_DEVICE_ID_SIIG_2S_10x (PCI_DEVICE_ID_SIIG_2S_10x_550 & 0xfff8)

static int pci_siig10x_init(struct pci_dev *dev)
{
	u16 data;
	void __iomem *p;

	switch (dev->device & 0xfff8) {
	case PCI_DEVICE_ID_SIIG_1S_10x:	
		data = 0xffdf;
		break;
	case PCI_DEVICE_ID_SIIG_2S_10x:	
		data = 0xf7ff;
		break;
	default:			
		data = 0xfffb;
		break;
	}

	p = ioremap_nocache(pci_resource_start(dev, 0), 0x80);
	if (p == NULL)
		return -ENOMEM;

	writew(readw(p + 0x28) & data, p + 0x28);
	readw(p + 0x28);
	iounmap(p);
	return 0;
}

#define PCI_DEVICE_ID_SIIG_2S_20x (PCI_DEVICE_ID_SIIG_2S_20x_550 & 0xfffc)
#define PCI_DEVICE_ID_SIIG_2S1P_20x (PCI_DEVICE_ID_SIIG_2S1P_20x_550 & 0xfffc)

static int pci_siig20x_init(struct pci_dev *dev)
{
	u8 data;

	
	pci_read_config_byte(dev, 0x6f, &data);
	pci_write_config_byte(dev, 0x6f, data & 0xef);

	
	if (((dev->device & 0xfffc) == PCI_DEVICE_ID_SIIG_2S_20x) ||
	    ((dev->device & 0xfffc) == PCI_DEVICE_ID_SIIG_2S1P_20x)) {
		pci_read_config_byte(dev, 0x73, &data);
		pci_write_config_byte(dev, 0x73, data & 0xef);
	}
	return 0;
}

static int pci_siig_init(struct pci_dev *dev)
{
	unsigned int type = dev->device & 0xff00;

	if (type == 0x1000)
		return pci_siig10x_init(dev);
	else if (type == 0x2000)
		return pci_siig20x_init(dev);

	moan_device("Unknown SIIG card", dev);
	return -ENODEV;
}

static int pci_siig_setup(struct serial_private *priv,
			  const struct pciserial_board *board,
			  struct uart_port *port, int idx)
{
	unsigned int bar = FL_GET_BASE(board->flags) + idx, offset = 0;

	if (idx > 3) {
		bar = 4;
		offset = (idx - 4) * 8;
	}

	return setup_port(priv, port, bar, offset, 0);
}

static const unsigned short timedia_single_port[] = {
	0x4025, 0x4027, 0x4028, 0x5025, 0x5027, 0
};

static const unsigned short timedia_dual_port[] = {
	0x0002, 0x4036, 0x4037, 0x4038, 0x4078, 0x4079, 0x4085,
	0x4088, 0x4089, 0x5037, 0x5078, 0x5079, 0x5085, 0x6079,
	0x7079, 0x8079, 0x8137, 0x8138, 0x8237, 0x8238, 0x9079,
	0x9137, 0x9138, 0x9237, 0x9238, 0xA079, 0xB079, 0xC079,
	0xD079, 0
};

static const unsigned short timedia_quad_port[] = {
	0x4055, 0x4056, 0x4095, 0x4096, 0x5056, 0x8156, 0x8157,
	0x8256, 0x8257, 0x9056, 0x9156, 0x9157, 0x9158, 0x9159,
	0x9256, 0x9257, 0xA056, 0xA157, 0xA158, 0xA159, 0xB056,
	0xB157, 0
};

static const unsigned short timedia_eight_port[] = {
	0x4065, 0x4066, 0x5065, 0x5066, 0x8166, 0x9066, 0x9166,
	0x9167, 0x9168, 0xA066, 0xA167, 0xA168, 0
};

static const struct timedia_struct {
	int num;
	const unsigned short *ids;
} timedia_data[] = {
	{ 1, timedia_single_port },
	{ 2, timedia_dual_port },
	{ 4, timedia_quad_port },
	{ 8, timedia_eight_port }
};

static int pci_timedia_probe(struct pci_dev *dev)
{
	if ((dev->subsystem_device & 0x00f0) >= 0x70) {
		dev_info(&dev->dev,
			"ignoring Timedia subdevice %04x for parport_serial\n",
			dev->subsystem_device);
		return -ENODEV;
	}

	return 0;
}

static int pci_timedia_init(struct pci_dev *dev)
{
	const unsigned short *ids;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(timedia_data); i++) {
		ids = timedia_data[i].ids;
		for (j = 0; ids[j]; j++)
			if (dev->subsystem_device == ids[j])
				return timedia_data[i].num;
	}
	return 0;
}

static int
pci_timedia_setup(struct serial_private *priv,
		  const struct pciserial_board *board,
		  struct uart_port *port, int idx)
{
	unsigned int bar = 0, offset = board->first_offset;

	switch (idx) {
	case 0:
		bar = 0;
		break;
	case 1:
		offset = board->uart_offset;
		bar = 0;
		break;
	case 2:
		bar = 1;
		break;
	case 3:
		offset = board->uart_offset;
		
	case 4: 
	case 5: 
	case 6: 
	case 7: 
		bar = idx - 2;
	}

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

static int
titan_400l_800l_setup(struct serial_private *priv,
		      const struct pciserial_board *board,
		      struct uart_port *port, int idx)
{
	unsigned int bar, offset = board->first_offset;

	switch (idx) {
	case 0:
		bar = 1;
		break;
	case 1:
		bar = 2;
		break;
	default:
		bar = 4;
		offset = (idx - 2) * board->uart_offset;
	}

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

static int pci_xircom_init(struct pci_dev *dev)
{
	msleep(100);
	return 0;
}

static int pci_ni8420_init(struct pci_dev *dev)
{
	void __iomem *p;
	unsigned long base, len;
	unsigned int bar = 0;

	if ((pci_resource_flags(dev, bar) & IORESOURCE_MEM) == 0) {
		moan_device("no memory in bar", dev);
		return 0;
	}

	base = pci_resource_start(dev, bar);
	len =  pci_resource_len(dev, bar);
	p = ioremap_nocache(base, len);
	if (p == NULL)
		return -ENOMEM;

	
	writel(readl(p + NI8420_INT_ENABLE_REG) | NI8420_INT_ENABLE_BIT,
	       p + NI8420_INT_ENABLE_REG);

	iounmap(p);
	return 0;
}

#define MITE_IOWBSR1_WSIZE	0xa
#define MITE_IOWBSR1_WIN_OFFSET	0x800
#define MITE_IOWBSR1_WENAB	(1 << 7)
#define MITE_LCIMR1_IO_IE_0	(1 << 24)
#define MITE_LCIMR2_SET_CPU_IE	(1 << 31)
#define MITE_IOWCR1_RAMSEL_MASK	0xfffffffe

static int pci_ni8430_init(struct pci_dev *dev)
{
	void __iomem *p;
	unsigned long base, len;
	u32 device_window;
	unsigned int bar = 0;

	if ((pci_resource_flags(dev, bar) & IORESOURCE_MEM) == 0) {
		moan_device("no memory in bar", dev);
		return 0;
	}

	base = pci_resource_start(dev, bar);
	len =  pci_resource_len(dev, bar);
	p = ioremap_nocache(base, len);
	if (p == NULL)
		return -ENOMEM;

	
	device_window = ((base + MITE_IOWBSR1_WIN_OFFSET) & 0xffffff00)
	                | MITE_IOWBSR1_WENAB | MITE_IOWBSR1_WSIZE;
	writel(device_window, p + MITE_IOWBSR1);

	
	writel((readl(p + MITE_IOWCR1) & MITE_IOWCR1_RAMSEL_MASK),
	       p + MITE_IOWCR1);

	
	writel(MITE_LCIMR1_IO_IE_0, p + MITE_LCIMR1);

	
	writel(MITE_LCIMR2_SET_CPU_IE, p + MITE_LCIMR2);

	iounmap(p);
	return 0;
}

#define NI8430_PORTCON	0x0f
#define NI8430_PORTCON_TXVR_ENABLE	(1 << 3)

static int
pci_ni8430_setup(struct serial_private *priv,
		 const struct pciserial_board *board,
		 struct uart_port *port, int idx)
{
	void __iomem *p;
	unsigned long base, len;
	unsigned int bar, offset = board->first_offset;

	if (idx >= board->num_ports)
		return 1;

	bar = FL_GET_BASE(board->flags);
	offset += idx * board->uart_offset;

	base = pci_resource_start(priv->dev, bar);
	len =  pci_resource_len(priv->dev, bar);
	p = ioremap_nocache(base, len);

	
	writeb(readb(p + offset + NI8430_PORTCON) | NI8430_PORTCON_TXVR_ENABLE,
	       p + offset + NI8430_PORTCON);

	iounmap(p);

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

static int pci_netmos_9900_setup(struct serial_private *priv,
				const struct pciserial_board *board,
				struct uart_port *port, int idx)
{
	unsigned int bar;

	if ((priv->dev->subsystem_device & 0xff00) == 0x3000) {
		bar = 3 * idx;

		return setup_port(priv, port, bar, 0, board->reg_shift);
	} else {
		return pci_default_setup(priv, board, port, idx);
	}
}

static int pci_netmos_9900_numports(struct pci_dev *dev)
{
	unsigned int c = dev->class;
	unsigned int pi;
	unsigned short sub_serports;

	pi = (c & 0xff);

	if (pi == 2) {
		return 1;
	} else if ((pi == 0) &&
			   (dev->device == PCI_DEVICE_ID_NETMOS_9900)) {
		sub_serports = dev->subsystem_device & 0xf;
		if (sub_serports > 0) {
			return sub_serports;
		} else {
			printk(KERN_NOTICE "NetMos/Mostech serial driver ignoring port on ambiguous config.\n");
			return 0;
		}
	}

	moan_device("unknown NetMos/Mostech program interface", dev);
	return 0;
}

static int pci_netmos_init(struct pci_dev *dev)
{
	
	unsigned int num_serial = dev->subsystem_device & 0xf;

	if ((dev->device == PCI_DEVICE_ID_NETMOS_9901) ||
		(dev->device == PCI_DEVICE_ID_NETMOS_9865))
		return 0;

	if (dev->subsystem_vendor == PCI_VENDOR_ID_IBM &&
			dev->subsystem_device == 0x0299)
		return 0;

	switch (dev->device) { 
		case PCI_DEVICE_ID_NETMOS_9904:
		case PCI_DEVICE_ID_NETMOS_9912:
		case PCI_DEVICE_ID_NETMOS_9922:
		case PCI_DEVICE_ID_NETMOS_9900:
			num_serial = pci_netmos_9900_numports(dev);
			break;

		default:
			if (num_serial == 0 ) {
				moan_device("unknown NetMos/Mostech device", dev);
			}
	}

	if (num_serial == 0)
		return -ENODEV;

	return num_serial;
}


#define ITE_887x_MISCR		0x9c
#define ITE_887x_INTCBAR	0x78
#define ITE_887x_UARTBAR	0x7c
#define ITE_887x_PS0BAR		0x10
#define ITE_887x_POSIO0		0x60

#define ITE_887x_IOSIZE		32
#define ITE_887x_POSIO_IOSIZE_8		(3 << 24)
#define ITE_887x_POSIO_IOSIZE_32	(5 << 24)
#define ITE_887x_POSIO_SPEED		(3 << 29)
#define ITE_887x_POSIO_ENABLE		(1 << 31)

static int pci_ite887x_init(struct pci_dev *dev)
{
	
	static const short inta_addr[] = { 0x2a0, 0x2c0, 0x220, 0x240, 0x1e0,
							0x200, 0x280, 0 };
	int ret, i, type;
	struct resource *iobase = NULL;
	u32 miscr, uartbar, ioport;

	
	i = 0;
	while (inta_addr[i] && iobase == NULL) {
		iobase = request_region(inta_addr[i], ITE_887x_IOSIZE,
								"ite887x");
		if (iobase != NULL) {
			
			pci_write_config_dword(dev, ITE_887x_POSIO0,
				ITE_887x_POSIO_ENABLE | ITE_887x_POSIO_SPEED |
				ITE_887x_POSIO_IOSIZE_32 | inta_addr[i]);
			
			pci_write_config_dword(dev, ITE_887x_INTCBAR,
								inta_addr[i]);
			ret = inb(inta_addr[i]);
			if (ret != 0xff) {
				
				break;
			}
			release_region(iobase->start, ITE_887x_IOSIZE);
			iobase = NULL;
		}
		i++;
	}

	if (!inta_addr[i]) {
		printk(KERN_ERR "ite887x: could not find iobase\n");
		return -ENODEV;
	}

	
	type = inb(iobase->start + 0x18) & 0x0f;

	switch (type) {
	case 0x2:	
	case 0xa:	
		ret = 0;
		break;
	case 0xe:	
		ret = 2;
		break;
	case 0x6:	
		ret = 1;
		break;
	case 0x8:	
		ret = 2;
		break;
	default:
		moan_device("Unknown ITE887x", dev);
		ret = -ENODEV;
	}

	
	for (i = 0; i < ret; i++) {
		
		pci_read_config_dword(dev, ITE_887x_PS0BAR + (0x4 * (i + 1)),
								&ioport);
		ioport &= 0x0000FF00;	
		pci_write_config_dword(dev, ITE_887x_POSIO0 + (0x4 * (i + 1)),
			ITE_887x_POSIO_ENABLE | ITE_887x_POSIO_SPEED |
			ITE_887x_POSIO_IOSIZE_8 | ioport);

		
		pci_read_config_dword(dev, ITE_887x_UARTBAR, &uartbar);
		uartbar &= ~(0xffff << (16 * i));	
		uartbar |= (ioport << (16 * i));	
		pci_write_config_dword(dev, ITE_887x_UARTBAR, uartbar);

		
		pci_read_config_dword(dev, ITE_887x_MISCR, &miscr);
		
		miscr &= ~(0xf << (12 - 4 * i));
		
		miscr |= 1 << (23 - i);
		
		pci_write_config_dword(dev, ITE_887x_MISCR, miscr);
	}

	if (ret <= 0) {
		
		release_region(iobase->start, ITE_887x_IOSIZE);
	}

	return ret;
}

static void __devexit pci_ite887x_exit(struct pci_dev *dev)
{
	u32 ioport;
	
	pci_read_config_dword(dev, ITE_887x_POSIO0, &ioport);
	ioport &= 0xffff;
	release_region(ioport, ITE_887x_IOSIZE);
}

static int pci_oxsemi_tornado_init(struct pci_dev *dev)
{
	u8 __iomem *p;
	unsigned long deviceID;
	unsigned int  number_uarts = 0;

	
	if (dev->vendor == PCI_VENDOR_ID_OXSEMI &&
	    (dev->device & 0xF000) != 0xC000)
		return 0;

	p = pci_iomap(dev, 0, 5);
	if (p == NULL)
		return -ENOMEM;

	deviceID = ioread32(p);
	
	if (deviceID == 0x07000200) {
		number_uarts = ioread8(p + 4);
		printk(KERN_DEBUG
			"%d ports detected on Oxford PCI Express device\n",
								number_uarts);
	}
	pci_iounmap(dev, p);
	return number_uarts;
}

static int
pci_default_setup(struct serial_private *priv,
		  const struct pciserial_board *board,
		  struct uart_port *port, int idx)
{
	unsigned int bar, offset = board->first_offset, maxnr;

	bar = FL_GET_BASE(board->flags);
	if (board->flags & FL_BASE_BARS)
		bar += idx;
	else
		offset += idx * board->uart_offset;

	maxnr = (pci_resource_len(priv->dev, bar) - board->first_offset) >>
		(board->reg_shift + 3);

	if (board->flags & FL_REGION_SZ_CAP && idx >= maxnr)
		return 1;

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

static int
ce4100_serial_setup(struct serial_private *priv,
		  const struct pciserial_board *board,
		  struct uart_port *port, int idx)
{
	int ret;

	ret = setup_port(priv, port, 0, 0, board->reg_shift);
	port->iotype = UPIO_MEM32;
	port->type = PORT_XSCALE;
	port->flags = (port->flags | UPF_FIXED_PORT | UPF_FIXED_TYPE);
	port->regshift = 2;

	return ret;
}

static int
pci_omegapci_setup(struct serial_private *priv,
		      const struct pciserial_board *board,
		      struct uart_port *port, int idx)
{
	return setup_port(priv, port, 2, idx * 8, 0);
}

static int skip_tx_en_setup(struct serial_private *priv,
			const struct pciserial_board *board,
			struct uart_port *port, int idx)
{
	port->flags |= UPF_NO_TXEN_TEST;
	printk(KERN_DEBUG "serial8250: skipping TxEn test for device "
			  "[%04x:%04x] subsystem [%04x:%04x]\n",
			  priv->dev->vendor,
			  priv->dev->device,
			  priv->dev->subsystem_vendor,
			  priv->dev->subsystem_device);

	return pci_default_setup(priv, board, port, idx);
}

static int kt_serial_setup(struct serial_private *priv,
			   const struct pciserial_board *board,
			   struct uart_port *port, int idx)
{
	port->flags |= UPF_BUG_THRE;
	return skip_tx_en_setup(priv, board, port, idx);
}

static int pci_eg20t_init(struct pci_dev *dev)
{
#if defined(CONFIG_SERIAL_PCH_UART) || defined(CONFIG_SERIAL_PCH_UART_MODULE)
	return -ENODEV;
#else
	return 0;
#endif
}

static int
pci_xr17c154_setup(struct serial_private *priv,
		  const struct pciserial_board *board,
		  struct uart_port *port, int idx)
{
	port->flags |= UPF_EXAR_EFR;
	return pci_default_setup(priv, board, port, idx);
}

#define PCI_VENDOR_ID_SBSMODULARIO	0x124B
#define PCI_SUBVENDOR_ID_SBSMODULARIO	0x124B
#define PCI_DEVICE_ID_OCTPRO		0x0001
#define PCI_SUBDEVICE_ID_OCTPRO232	0x0108
#define PCI_SUBDEVICE_ID_OCTPRO422	0x0208
#define PCI_SUBDEVICE_ID_POCTAL232	0x0308
#define PCI_SUBDEVICE_ID_POCTAL422	0x0408
#define PCI_VENDOR_ID_ADVANTECH		0x13fe
#define PCI_DEVICE_ID_INTEL_CE4100_UART 0x2e66
#define PCI_DEVICE_ID_ADVANTECH_PCI3620	0x3620
#define PCI_DEVICE_ID_TITAN_200I	0x8028
#define PCI_DEVICE_ID_TITAN_400I	0x8048
#define PCI_DEVICE_ID_TITAN_800I	0x8088
#define PCI_DEVICE_ID_TITAN_800EH	0xA007
#define PCI_DEVICE_ID_TITAN_800EHB	0xA008
#define PCI_DEVICE_ID_TITAN_400EH	0xA009
#define PCI_DEVICE_ID_TITAN_100E	0xA010
#define PCI_DEVICE_ID_TITAN_200E	0xA012
#define PCI_DEVICE_ID_TITAN_400E	0xA013
#define PCI_DEVICE_ID_TITAN_800E	0xA014
#define PCI_DEVICE_ID_TITAN_200EI	0xA016
#define PCI_DEVICE_ID_TITAN_200EISI	0xA017
#define PCI_DEVICE_ID_TITAN_400V3	0xA310
#define PCI_DEVICE_ID_TITAN_410V3	0xA312
#define PCI_DEVICE_ID_TITAN_800V3	0xA314
#define PCI_DEVICE_ID_TITAN_800V3B	0xA315
#define PCI_DEVICE_ID_OXSEMI_16PCI958	0x9538
#define PCIE_DEVICE_ID_NEO_2_OX_IBM	0x00F6
#define PCI_DEVICE_ID_PLX_CRONYX_OMEGA	0xc001
#define PCI_DEVICE_ID_INTEL_PATSBURG_KT 0x1d3d

#define PCI_SUBDEVICE_ID_UNKNOWN_0x1584	0x1584

static struct pci_serial_quirk pci_serial_quirks[] __refdata = {
	{
		.vendor         = PCI_VENDOR_ID_ADDIDATA_OLD,
		.device         = PCI_DEVICE_ID_ADDIDATA_APCI7800,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.setup          = addidata_apci7800_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_AFAVLAB,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= afavlab_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_HP,
		.device		= PCI_DEVICE_ID_HP_DIVA,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_hp_diva_init,
		.setup		= pci_hp_diva_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_80960_RP,
		.subvendor	= 0xe4bf,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_inteli960ni_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_8257X_SOL,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= skip_tx_en_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_82573L_SOL,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= skip_tx_en_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_82573E_SOL,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= skip_tx_en_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_CE4100_UART,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= ce4100_serial_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_PATSBURG_KT,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= kt_serial_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_ITE,
		.device		= PCI_DEVICE_ID_ITE_8872,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ite887x_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ite887x_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI23216,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI2328,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI2324,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI2322,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI2324I,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI2322I,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8420_23216,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8420_2328,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8420_2324,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8420_2322,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8422_2324,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8422_2322,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_ni8420_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8430_init,
		.setup		= pci_ni8430_setup,
		.exit		= __devexit_p(pci_ni8430_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_PANACOM,
		.device		= PCI_DEVICE_ID_PANACOM_QUADMODEM,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_plx9050_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_plx9050_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_PANACOM,
		.device		= PCI_DEVICE_ID_PANACOM_DUALMODEM,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_plx9050_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_plx9050_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_PLX,
		.device		= PCI_DEVICE_ID_PLX_9030,
		.subvendor	= PCI_SUBVENDOR_ID_PERLE,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_default_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_PLX,
		.device		= PCI_DEVICE_ID_PLX_9050,
		.subvendor	= PCI_SUBVENDOR_ID_EXSYS,
		.subdevice	= PCI_SUBDEVICE_ID_EXSYS_4055,
		.init		= pci_plx9050_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_plx9050_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_PLX,
		.device		= PCI_DEVICE_ID_PLX_9050,
		.subvendor	= PCI_SUBVENDOR_ID_KEYSPAN,
		.subdevice	= PCI_SUBDEVICE_ID_KEYSPAN_SX2,
		.init		= pci_plx9050_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_plx9050_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_PLX,
		.device		= PCI_DEVICE_ID_PLX_9050,
		.subvendor	= PCI_VENDOR_ID_PLX,
		.subdevice	= PCI_SUBDEVICE_ID_UNKNOWN_0x1584,
		.init		= pci_plx9050_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_plx9050_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_PLX,
		.device		= PCI_DEVICE_ID_PLX_ROMULUS,
		.subvendor	= PCI_VENDOR_ID_PLX,
		.subdevice	= PCI_DEVICE_ID_PLX_ROMULUS,
		.init		= pci_plx9050_init,
		.setup		= pci_default_setup,
		.exit		= __devexit_p(pci_plx9050_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_SBSMODULARIO,
		.device		= PCI_DEVICE_ID_OCTPRO,
		.subvendor	= PCI_SUBVENDOR_ID_SBSMODULARIO,
		.subdevice	= PCI_SUBDEVICE_ID_OCTPRO232,
		.init		= sbs_init,
		.setup		= sbs_setup,
		.exit		= __devexit_p(sbs_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_SBSMODULARIO,
		.device		= PCI_DEVICE_ID_OCTPRO,
		.subvendor	= PCI_SUBVENDOR_ID_SBSMODULARIO,
		.subdevice	= PCI_SUBDEVICE_ID_OCTPRO422,
		.init		= sbs_init,
		.setup		= sbs_setup,
		.exit		= __devexit_p(sbs_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_SBSMODULARIO,
		.device		= PCI_DEVICE_ID_OCTPRO,
		.subvendor	= PCI_SUBVENDOR_ID_SBSMODULARIO,
		.subdevice	= PCI_SUBDEVICE_ID_POCTAL232,
		.init		= sbs_init,
		.setup		= sbs_setup,
		.exit		= __devexit_p(sbs_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_SBSMODULARIO,
		.device		= PCI_DEVICE_ID_OCTPRO,
		.subvendor	= PCI_SUBVENDOR_ID_SBSMODULARIO,
		.subdevice	= PCI_SUBDEVICE_ID_POCTAL422,
		.init		= sbs_init,
		.setup		= sbs_setup,
		.exit		= __devexit_p(sbs_exit),
	},
	{
		.vendor		= PCI_VENDOR_ID_SIIG,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_siig_init,
		.setup		= pci_siig_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_TITAN,
		.device		= PCI_DEVICE_ID_TITAN_400L,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= titan_400l_800l_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_TITAN,
		.device		= PCI_DEVICE_ID_TITAN_800L,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= titan_400l_800l_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_TIMEDIA,
		.device		= PCI_DEVICE_ID_TIMEDIA_1889,
		.subvendor	= PCI_VENDOR_ID_TIMEDIA,
		.subdevice	= PCI_ANY_ID,
		.probe		= pci_timedia_probe,
		.init		= pci_timedia_init,
		.setup		= pci_timedia_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_TIMEDIA,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_timedia_setup,
	},
	{
		.vendor = PCI_VENDOR_ID_EXAR,
		.device = PCI_DEVICE_ID_EXAR_XR17C152,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_xr17c154_setup,
	},
	{
		.vendor = PCI_VENDOR_ID_EXAR,
		.device = PCI_DEVICE_ID_EXAR_XR17C154,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_xr17c154_setup,
	},
	{
		.vendor = PCI_VENDOR_ID_EXAR,
		.device = PCI_DEVICE_ID_EXAR_XR17C158,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_xr17c154_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_XIRCOM,
		.device		= PCI_DEVICE_ID_XIRCOM_X3201_MDM,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_xircom_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_NETMOS,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_netmos_init,
		.setup		= pci_netmos_9900_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_OXSEMI,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_MAINPINE,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_DIGI,
		.device		= PCIE_DEVICE_ID_NEO_2_OX_IBM,
		.subvendor		= PCI_SUBVENDOR_ID_IBM,
		.subdevice		= PCI_ANY_ID,
		.init			= pci_oxsemi_tornado_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = PCI_VENDOR_ID_INTEL,
		.device         = 0x8811,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = PCI_VENDOR_ID_INTEL,
		.device         = 0x8812,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = PCI_VENDOR_ID_INTEL,
		.device         = 0x8813,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = PCI_VENDOR_ID_INTEL,
		.device         = 0x8814,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = 0x10DB,
		.device         = 0x8027,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = 0x10DB,
		.device         = 0x8028,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = 0x10DB,
		.device         = 0x8029,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = 0x10DB,
		.device         = 0x800C,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = 0x10DB,
		.device         = 0x800D,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_PLX,
		.device		= PCI_DEVICE_ID_PLX_CRONYX_OMEGA,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_omegapci_setup,
	 },
	{
		.vendor		= PCI_ANY_ID,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_default_setup,
	}
};

static inline int quirk_id_matches(u32 quirk_id, u32 dev_id)
{
	return quirk_id == PCI_ANY_ID || quirk_id == dev_id;
}

static struct pci_serial_quirk *find_quirk(struct pci_dev *dev)
{
	struct pci_serial_quirk *quirk;

	for (quirk = pci_serial_quirks; ; quirk++)
		if (quirk_id_matches(quirk->vendor, dev->vendor) &&
		    quirk_id_matches(quirk->device, dev->device) &&
		    quirk_id_matches(quirk->subvendor, dev->subsystem_vendor) &&
		    quirk_id_matches(quirk->subdevice, dev->subsystem_device))
			break;
	return quirk;
}

static inline int get_pci_irq(struct pci_dev *dev,
				const struct pciserial_board *board)
{
	if (board->flags & FL_NOIRQ)
		return 0;
	else
		return dev->irq;
}

enum pci_board_num_t {
	pbn_default = 0,

	pbn_b0_1_115200,
	pbn_b0_2_115200,
	pbn_b0_4_115200,
	pbn_b0_5_115200,
	pbn_b0_8_115200,

	pbn_b0_1_921600,
	pbn_b0_2_921600,
	pbn_b0_4_921600,

	pbn_b0_2_1130000,

	pbn_b0_4_1152000,

	pbn_b0_2_1843200,
	pbn_b0_4_1843200,

	pbn_b0_2_1843200_200,
	pbn_b0_4_1843200_200,
	pbn_b0_8_1843200_200,

	pbn_b0_1_4000000,

	pbn_b0_bt_1_115200,
	pbn_b0_bt_2_115200,
	pbn_b0_bt_4_115200,
	pbn_b0_bt_8_115200,

	pbn_b0_bt_1_460800,
	pbn_b0_bt_2_460800,
	pbn_b0_bt_4_460800,

	pbn_b0_bt_1_921600,
	pbn_b0_bt_2_921600,
	pbn_b0_bt_4_921600,
	pbn_b0_bt_8_921600,

	pbn_b1_1_115200,
	pbn_b1_2_115200,
	pbn_b1_4_115200,
	pbn_b1_8_115200,
	pbn_b1_16_115200,

	pbn_b1_1_921600,
	pbn_b1_2_921600,
	pbn_b1_4_921600,
	pbn_b1_8_921600,

	pbn_b1_2_1250000,

	pbn_b1_bt_1_115200,
	pbn_b1_bt_2_115200,
	pbn_b1_bt_4_115200,

	pbn_b1_bt_2_921600,

	pbn_b1_1_1382400,
	pbn_b1_2_1382400,
	pbn_b1_4_1382400,
	pbn_b1_8_1382400,

	pbn_b2_1_115200,
	pbn_b2_2_115200,
	pbn_b2_4_115200,
	pbn_b2_8_115200,

	pbn_b2_1_460800,
	pbn_b2_4_460800,
	pbn_b2_8_460800,
	pbn_b2_16_460800,

	pbn_b2_1_921600,
	pbn_b2_4_921600,
	pbn_b2_8_921600,

	pbn_b2_8_1152000,

	pbn_b2_bt_1_115200,
	pbn_b2_bt_2_115200,
	pbn_b2_bt_4_115200,

	pbn_b2_bt_2_921600,
	pbn_b2_bt_4_921600,

	pbn_b3_2_115200,
	pbn_b3_4_115200,
	pbn_b3_8_115200,

	pbn_b4_bt_2_921600,
	pbn_b4_bt_4_921600,
	pbn_b4_bt_8_921600,

	pbn_panacom,
	pbn_panacom2,
	pbn_panacom4,
	pbn_exsys_4055,
	pbn_plx_romulus,
	pbn_oxsemi,
	pbn_oxsemi_1_4000000,
	pbn_oxsemi_2_4000000,
	pbn_oxsemi_4_4000000,
	pbn_oxsemi_8_4000000,
	pbn_intel_i960,
	pbn_sgi_ioc3,
	pbn_computone_4,
	pbn_computone_6,
	pbn_computone_8,
	pbn_sbsxrsio,
	pbn_exar_XR17C152,
	pbn_exar_XR17C154,
	pbn_exar_XR17C158,
	pbn_exar_ibm_saturn,
	pbn_pasemi_1682M,
	pbn_ni8430_2,
	pbn_ni8430_4,
	pbn_ni8430_8,
	pbn_ni8430_16,
	pbn_ADDIDATA_PCIe_1_3906250,
	pbn_ADDIDATA_PCIe_2_3906250,
	pbn_ADDIDATA_PCIe_4_3906250,
	pbn_ADDIDATA_PCIe_8_3906250,
	pbn_ce4100_1_115200,
	pbn_omegapci,
	pbn_NETMOS9900_2s_115200,
};


static struct pciserial_board pci_boards[] __devinitdata = {
	[pbn_default] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_1_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_2_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_4_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_5_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 5,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_8_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_1_921600] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b0_2_921600] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b0_4_921600] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},

	[pbn_b0_2_1130000] = {
		.flags          = FL_BASE0,
		.num_ports      = 2,
		.base_baud      = 1130000,
		.uart_offset    = 8,
	},

	[pbn_b0_4_1152000] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 1152000,
		.uart_offset	= 8,
	},

	[pbn_b0_2_1843200] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 1843200,
		.uart_offset	= 8,
	},
	[pbn_b0_4_1843200] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 1843200,
		.uart_offset	= 8,
	},

	[pbn_b0_2_1843200_200] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 1843200,
		.uart_offset	= 0x200,
	},
	[pbn_b0_4_1843200_200] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 1843200,
		.uart_offset	= 0x200,
	},
	[pbn_b0_8_1843200_200] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 1843200,
		.uart_offset	= 0x200,
	},
	[pbn_b0_1_4000000] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 4000000,
		.uart_offset	= 8,
	},

	[pbn_b0_bt_1_115200] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_2_115200] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_4_115200] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_8_115200] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b0_bt_1_460800] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 1,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_2_460800] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_4_460800] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},

	[pbn_b0_bt_1_921600] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 1,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_2_921600] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_4_921600] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_8_921600] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 8,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},

	[pbn_b1_1_115200] = {
		.flags		= FL_BASE1,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_2_115200] = {
		.flags		= FL_BASE1,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_4_115200] = {
		.flags		= FL_BASE1,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_8_115200] = {
		.flags		= FL_BASE1,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_16_115200] = {
		.flags		= FL_BASE1,
		.num_ports	= 16,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b1_1_921600] = {
		.flags		= FL_BASE1,
		.num_ports	= 1,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b1_2_921600] = {
		.flags		= FL_BASE1,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b1_4_921600] = {
		.flags		= FL_BASE1,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b1_8_921600] = {
		.flags		= FL_BASE1,
		.num_ports	= 8,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b1_2_1250000] = {
		.flags		= FL_BASE1,
		.num_ports	= 2,
		.base_baud	= 1250000,
		.uart_offset	= 8,
	},

	[pbn_b1_bt_1_115200] = {
		.flags		= FL_BASE1|FL_BASE_BARS,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_bt_2_115200] = {
		.flags		= FL_BASE1|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_bt_4_115200] = {
		.flags		= FL_BASE1|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b1_bt_2_921600] = {
		.flags		= FL_BASE1|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},

	[pbn_b1_1_1382400] = {
		.flags		= FL_BASE1,
		.num_ports	= 1,
		.base_baud	= 1382400,
		.uart_offset	= 8,
	},
	[pbn_b1_2_1382400] = {
		.flags		= FL_BASE1,
		.num_ports	= 2,
		.base_baud	= 1382400,
		.uart_offset	= 8,
	},
	[pbn_b1_4_1382400] = {
		.flags		= FL_BASE1,
		.num_ports	= 4,
		.base_baud	= 1382400,
		.uart_offset	= 8,
	},
	[pbn_b1_8_1382400] = {
		.flags		= FL_BASE1,
		.num_ports	= 8,
		.base_baud	= 1382400,
		.uart_offset	= 8,
	},

	[pbn_b2_1_115200] = {
		.flags		= FL_BASE2,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b2_2_115200] = {
		.flags		= FL_BASE2,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b2_4_115200] = {
		.flags          = FL_BASE2,
		.num_ports      = 4,
		.base_baud      = 115200,
		.uart_offset    = 8,
	},
	[pbn_b2_8_115200] = {
		.flags		= FL_BASE2,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b2_1_460800] = {
		.flags		= FL_BASE2,
		.num_ports	= 1,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},
	[pbn_b2_4_460800] = {
		.flags		= FL_BASE2,
		.num_ports	= 4,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},
	[pbn_b2_8_460800] = {
		.flags		= FL_BASE2,
		.num_ports	= 8,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},
	[pbn_b2_16_460800] = {
		.flags		= FL_BASE2,
		.num_ports	= 16,
		.base_baud	= 460800,
		.uart_offset	= 8,
	 },

	[pbn_b2_1_921600] = {
		.flags		= FL_BASE2,
		.num_ports	= 1,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b2_4_921600] = {
		.flags		= FL_BASE2,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b2_8_921600] = {
		.flags		= FL_BASE2,
		.num_ports	= 8,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},

	[pbn_b2_8_1152000] = {
		.flags		= FL_BASE2,
		.num_ports	= 8,
		.base_baud	= 1152000,
		.uart_offset	= 8,
	},

	[pbn_b2_bt_1_115200] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b2_bt_2_115200] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b2_bt_4_115200] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b2_bt_2_921600] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b2_bt_4_921600] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},

	[pbn_b3_2_115200] = {
		.flags		= FL_BASE3,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b3_4_115200] = {
		.flags		= FL_BASE3,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b3_8_115200] = {
		.flags		= FL_BASE3,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b4_bt_2_921600] = {
		.flags		= FL_BASE4,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b4_bt_4_921600] = {
		.flags		= FL_BASE4,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b4_bt_8_921600] = {
		.flags		= FL_BASE4,
		.num_ports	= 8,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},


	[pbn_panacom] = {
		.flags		= FL_BASE2,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 0x400,
		.reg_shift	= 7,
	},
	[pbn_panacom2] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 0x400,
		.reg_shift	= 7,
	},
	[pbn_panacom4] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 0x400,
		.reg_shift	= 7,
	},

	[pbn_exsys_4055] = {
		.flags		= FL_BASE2,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	
	[pbn_plx_romulus] = {
		.flags		= FL_BASE2,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8 << 2,
		.reg_shift	= 2,
		.first_offset	= 0x03,
	},

	[pbn_oxsemi] = {
		.flags		= FL_BASE0|FL_REGION_SZ_CAP,
		.num_ports	= 32,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_oxsemi_1_4000000] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 4000000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_oxsemi_2_4000000] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 4000000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_oxsemi_4_4000000] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 4000000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_oxsemi_8_4000000] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 4000000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},


	[pbn_intel_i960] = {
		.flags		= FL_BASE0,
		.num_ports	= 32,
		.base_baud	= 921600,
		.uart_offset	= 8 << 2,
		.reg_shift	= 2,
		.first_offset	= 0x10000,
	},
	[pbn_sgi_ioc3] = {
		.flags		= FL_BASE0|FL_NOIRQ,
		.num_ports	= 1,
		.base_baud	= 458333,
		.uart_offset	= 8,
		.reg_shift	= 0,
		.first_offset	= 0x20178,
	},

	[pbn_computone_4] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 0x40,
		.reg_shift	= 2,
		.first_offset	= 0x200,
	},
	[pbn_computone_6] = {
		.flags		= FL_BASE0,
		.num_ports	= 6,
		.base_baud	= 921600,
		.uart_offset	= 0x40,
		.reg_shift	= 2,
		.first_offset	= 0x200,
	},
	[pbn_computone_8] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 921600,
		.uart_offset	= 0x40,
		.reg_shift	= 2,
		.first_offset	= 0x200,
	},
	[pbn_sbsxrsio] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 460800,
		.uart_offset	= 256,
		.reg_shift	= 4,
	},
	[pbn_exar_XR17C152] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 0x200,
	},
	[pbn_exar_XR17C154] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 0x200,
	},
	[pbn_exar_XR17C158] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 921600,
		.uart_offset	= 0x200,
	},
	[pbn_exar_ibm_saturn] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 921600,
		.uart_offset	= 0x200,
	},

	[pbn_pasemi_1682M] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 8333333,
	},
	[pbn_ni8430_16] = {
		.flags		= FL_BASE0,
		.num_ports	= 16,
		.base_baud	= 3686400,
		.uart_offset	= 0x10,
		.first_offset	= 0x800,
	},
	[pbn_ni8430_8] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 3686400,
		.uart_offset	= 0x10,
		.first_offset	= 0x800,
	},
	[pbn_ni8430_4] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 3686400,
		.uart_offset	= 0x10,
		.first_offset	= 0x800,
	},
	[pbn_ni8430_2] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 3686400,
		.uart_offset	= 0x10,
		.first_offset	= 0x800,
	},
	[pbn_ADDIDATA_PCIe_1_3906250] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 3906250,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_ADDIDATA_PCIe_2_3906250] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 3906250,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_ADDIDATA_PCIe_4_3906250] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 3906250,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_ADDIDATA_PCIe_8_3906250] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 3906250,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_ce4100_1_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 921600,
		.reg_shift      = 2,
	},
	[pbn_omegapci] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 0x200,
	},
	[pbn_NETMOS9900_2s_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 115200,
	},
};

static const struct pci_device_id softmodem_blacklist[] = {
	{ PCI_VDEVICE(AL, 0x5457), }, 
	{ PCI_VDEVICE(MOTOROLA, 0x3052), }, 
	{ PCI_DEVICE(0x1543, 0x3052), }, 
};

static int __devinit
serial_pci_guess_board(struct pci_dev *dev, struct pciserial_board *board)
{
	const struct pci_device_id *blacklist;
	int num_iomem, num_port, first_port = -1, i;

	if ((((dev->class >> 8) != PCI_CLASS_COMMUNICATION_SERIAL) &&
	     ((dev->class >> 8) != PCI_CLASS_COMMUNICATION_MODEM)) ||
	    (dev->class & 0xff) > 6)
		return -ENODEV;

	for (blacklist = softmodem_blacklist;
	     blacklist < softmodem_blacklist + ARRAY_SIZE(softmodem_blacklist);
	     blacklist++) {
		if (dev->vendor == blacklist->vendor &&
		    dev->device == blacklist->device)
			return -ENODEV;
	}

	num_iomem = num_port = 0;
	for (i = 0; i < PCI_NUM_BAR_RESOURCES; i++) {
		if (pci_resource_flags(dev, i) & IORESOURCE_IO) {
			num_port++;
			if (first_port == -1)
				first_port = i;
		}
		if (pci_resource_flags(dev, i) & IORESOURCE_MEM)
			num_iomem++;
	}

	if (num_iomem <= 1 && num_port == 1) {
		board->flags = first_port;
		board->num_ports = pci_resource_len(dev, first_port) / 8;
		return 0;
	}

	first_port = -1;
	num_port = 0;
	for (i = 0; i < PCI_NUM_BAR_RESOURCES; i++) {
		if (pci_resource_flags(dev, i) & IORESOURCE_IO &&
		    pci_resource_len(dev, i) == 8 &&
		    (first_port == -1 || (first_port + num_port) == i)) {
			num_port++;
			if (first_port == -1)
				first_port = i;
		}
	}

	if (num_port > 1) {
		board->flags = first_port | FL_BASE_BARS;
		board->num_ports = num_port;
		return 0;
	}

	return -ENODEV;
}

static inline int
serial_pci_matches(const struct pciserial_board *board,
		   const struct pciserial_board *guessed)
{
	return
	    board->num_ports == guessed->num_ports &&
	    board->base_baud == guessed->base_baud &&
	    board->uart_offset == guessed->uart_offset &&
	    board->reg_shift == guessed->reg_shift &&
	    board->first_offset == guessed->first_offset;
}

struct serial_private *
pciserial_init_ports(struct pci_dev *dev, const struct pciserial_board *board)
{
	struct uart_port serial_port;
	struct serial_private *priv;
	struct pci_serial_quirk *quirk;
	int rc, nr_ports, i;

	nr_ports = board->num_ports;

	quirk = find_quirk(dev);

	if (quirk->init) {
		rc = quirk->init(dev);
		if (rc < 0) {
			priv = ERR_PTR(rc);
			goto err_out;
		}
		if (rc)
			nr_ports = rc;
	}

	priv = kzalloc(sizeof(struct serial_private) +
		       sizeof(unsigned int) * nr_ports,
		       GFP_KERNEL);
	if (!priv) {
		priv = ERR_PTR(-ENOMEM);
		goto err_deinit;
	}

	priv->dev = dev;
	priv->quirk = quirk;

	memset(&serial_port, 0, sizeof(struct uart_port));
	serial_port.flags = UPF_SKIP_TEST | UPF_BOOT_AUTOCONF | UPF_SHARE_IRQ;
	serial_port.uartclk = board->base_baud * 16;
	serial_port.irq = get_pci_irq(dev, board);
	serial_port.dev = &dev->dev;

	for (i = 0; i < nr_ports; i++) {
		if (quirk->setup(priv, board, &serial_port, i))
			break;

#ifdef SERIAL_DEBUG_PCI
		printk(KERN_DEBUG "Setup PCI port: port %lx, irq %d, type %d\n",
		       serial_port.iobase, serial_port.irq, serial_port.iotype);
#endif

		priv->line[i] = serial8250_register_port(&serial_port);
		if (priv->line[i] < 0) {
			printk(KERN_WARNING "Couldn't register serial port %s: %d\n", pci_name(dev), priv->line[i]);
			break;
		}
	}
	priv->nr = i;
	return priv;

err_deinit:
	if (quirk->exit)
		quirk->exit(dev);
err_out:
	return priv;
}
EXPORT_SYMBOL_GPL(pciserial_init_ports);

void pciserial_remove_ports(struct serial_private *priv)
{
	struct pci_serial_quirk *quirk;
	int i;

	for (i = 0; i < priv->nr; i++)
		serial8250_unregister_port(priv->line[i]);

	for (i = 0; i < PCI_NUM_BAR_RESOURCES; i++) {
		if (priv->remapped_bar[i])
			iounmap(priv->remapped_bar[i]);
		priv->remapped_bar[i] = NULL;
	}

	quirk = find_quirk(priv->dev);
	if (quirk->exit)
		quirk->exit(priv->dev);

	kfree(priv);
}
EXPORT_SYMBOL_GPL(pciserial_remove_ports);

void pciserial_suspend_ports(struct serial_private *priv)
{
	int i;

	for (i = 0; i < priv->nr; i++)
		if (priv->line[i] >= 0)
			serial8250_suspend_port(priv->line[i]);
}
EXPORT_SYMBOL_GPL(pciserial_suspend_ports);

void pciserial_resume_ports(struct serial_private *priv)
{
	int i;

	if (priv->quirk->init)
		priv->quirk->init(priv->dev);

	for (i = 0; i < priv->nr; i++)
		if (priv->line[i] >= 0)
			serial8250_resume_port(priv->line[i]);
}
EXPORT_SYMBOL_GPL(pciserial_resume_ports);

static int __devinit
pciserial_init_one(struct pci_dev *dev, const struct pci_device_id *ent)
{
	struct pci_serial_quirk *quirk;
	struct serial_private *priv;
	const struct pciserial_board *board;
	struct pciserial_board tmp;
	int rc;

	quirk = find_quirk(dev);
	if (quirk->probe) {
		rc = quirk->probe(dev);
		if (rc)
			return rc;
	}

	if (ent->driver_data >= ARRAY_SIZE(pci_boards)) {
		printk(KERN_ERR "pci_init_one: invalid driver_data: %ld\n",
			ent->driver_data);
		return -EINVAL;
	}

	board = &pci_boards[ent->driver_data];

	rc = pci_enable_device(dev);
	pci_save_state(dev);
	if (rc)
		return rc;

	if (ent->driver_data == pbn_default) {
		memcpy(&tmp, board, sizeof(struct pciserial_board));
		board = &tmp;

		rc = serial_pci_guess_board(dev, &tmp);
		if (rc)
			goto disable;
	} else {
		memcpy(&tmp, &pci_boards[pbn_default],
		       sizeof(struct pciserial_board));
		rc = serial_pci_guess_board(dev, &tmp);
		if (rc == 0 && serial_pci_matches(board, &tmp))
			moan_device("Redundant entry in serial pci_table.",
				    dev);
	}

	priv = pciserial_init_ports(dev, board);
	if (!IS_ERR(priv)) {
		pci_set_drvdata(dev, priv);
		return 0;
	}

	rc = PTR_ERR(priv);

 disable:
	pci_disable_device(dev);
	return rc;
}

static void __devexit pciserial_remove_one(struct pci_dev *dev)
{
	struct serial_private *priv = pci_get_drvdata(dev);

	pci_set_drvdata(dev, NULL);

	pciserial_remove_ports(priv);

	pci_disable_device(dev);
}

#ifdef CONFIG_PM
static int pciserial_suspend_one(struct pci_dev *dev, pm_message_t state)
{
	struct serial_private *priv = pci_get_drvdata(dev);

	if (priv)
		pciserial_suspend_ports(priv);

	pci_save_state(dev);
	pci_set_power_state(dev, pci_choose_state(dev, state));
	return 0;
}

static int pciserial_resume_one(struct pci_dev *dev)
{
	int err;
	struct serial_private *priv = pci_get_drvdata(dev);

	pci_set_power_state(dev, PCI_D0);
	pci_restore_state(dev);

	if (priv) {
		err = pci_enable_device(dev);
		
		if (err)
			printk(KERN_ERR "pciserial: Unable to re-enable ports, trying to continue.\n");
		pciserial_resume_ports(priv);
	}
	return 0;
}
#endif

static struct pci_device_id serial_pci_tbl[] = {
	
	{	PCI_VENDOR_ID_ADVANTECH, PCI_DEVICE_ID_ADVANTECH_PCI3620,
		PCI_DEVICE_ID_ADVANTECH_PCI3620, 0x0001, 0, 0,
		pbn_b2_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V960,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_232, 0, 0,
		pbn_b1_8_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V960,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_232, 0, 0,
		pbn_b1_4_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V960,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH2_232, 0, 0,
		pbn_b1_2_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_232, 0, 0,
		pbn_b1_8_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_232, 0, 0,
		pbn_b1_4_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH2_232, 0, 0,
		pbn_b1_2_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_485, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_485_4_4, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_485, 0, 0,
		pbn_b1_4_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_485_2_2, 0, 0,
		pbn_b1_4_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH2_485, 0, 0,
		pbn_b1_2_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_485_2_6, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH081101V1, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH041101V1, 0, 0,
		pbn_b1_4_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH2_20MHZ, 0, 0,
		pbn_b1_2_1250000 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_TITAN_2, 0, 0,
		pbn_b0_2_1843200 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_TITAN_4, 0, 0,
		pbn_b0_4_1843200 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_VENDOR_ID_AFAVLAB,
		PCI_SUBDEVICE_ID_AFAVLAB_P061, 0, 0,
		pbn_b0_4_1152000 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C152,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_2_232, 0, 0,
		pbn_b0_2_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C154,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_4_232, 0, 0,
		pbn_b0_4_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C158,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_8_232, 0, 0,
		pbn_b0_8_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C152,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_1_1, 0, 0,
		pbn_b0_2_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C154,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_2_2, 0, 0,
		pbn_b0_4_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C158,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_4_4, 0, 0,
		pbn_b0_8_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C152,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_2, 0, 0,
		pbn_b0_2_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C154,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_4, 0, 0,
		pbn_b0_4_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C158,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_8, 0, 0,
		pbn_b0_8_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C152,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_2_485, 0, 0,
		pbn_b0_2_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C154,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_4_485, 0, 0,
		pbn_b0_4_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C158,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_PCI_UART_8_485, 0, 0,
		pbn_b0_8_1843200_200 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C152,
		PCI_VENDOR_ID_IBM, PCI_SUBDEVICE_ID_IBM_SATURN_SERIAL_ONE_PORT,
		0, 0, pbn_exar_ibm_saturn },

	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_U530,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_1_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_UCOMM2,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_UCOMM422,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_4_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_UCOMM232,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_COMM4,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_4_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_COMM8,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_8_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_7803,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_8_460800 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_UCOMM8,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_8_115200 },

	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_GTEK_SERIAL2,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_115200 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_SPCOM200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_921600 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_SPCOM800,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_8_921600 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_1077,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_4_921600 },
	
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_VENDOR_ID_PLX,
		PCI_SUBDEVICE_ID_UNKNOWN_0x1584, 0, 0,
		pbn_b0_4_115200 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_KEYSPAN,
		PCI_SUBDEVICE_ID_KEYSPAN_SX2, 0, 0,
		pbn_panacom },
	{	PCI_VENDOR_ID_PANACOM, PCI_DEVICE_ID_PANACOM_QUADMODEM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_panacom4 },
	{	PCI_VENDOR_ID_PANACOM, PCI_DEVICE_ID_PANACOM_DUALMODEM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_panacom2 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9030,
		PCI_VENDOR_ID_ESDGMBH,
		PCI_DEVICE_ID_ESDGMBH_CPCIASIO4, 0, 0,
		pbn_b2_4_115200 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST4, 0, 0,
		pbn_b2_4_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST8, 0, 0,
		pbn_b2_8_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST16, 0, 0,
		pbn_b2_16_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST16FMC, 0, 0,
		pbn_b2_16_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIRAS,
		PCI_SUBDEVICE_ID_CHASE_PCIRAS4, 0, 0,
		pbn_b2_4_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIRAS,
		PCI_SUBDEVICE_ID_CHASE_PCIRAS8, 0, 0,
		pbn_b2_8_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_EXSYS,
		PCI_SUBDEVICE_ID_EXSYS_4055, 0, 0,
		pbn_exsys_4055 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_ROMULUS,
		0x10b5, 0x106a, 0, 0,
		pbn_plx_romulus },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_QSC100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_4_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_DSC100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_2_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_ESC100D,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_8_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_ESC100M,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_8_115200 },
	{	PCI_VENDOR_ID_SPECIALIX, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_VENDOR_ID_SPECIALIX, PCI_SUBDEVICE_ID_SPECIALIX_SPEED4,
		0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_SUBVENDOR_ID_SIIG, PCI_SUBDEVICE_ID_SIIG_QUARTET_SERIAL,
		0, 0,
		pbn_b0_4_1152000 },
	{	PCI_VENDOR_ID_OXSEMI, 0x9505,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_921600 },

	{	PCI_VENDOR_ID_OXSEMI, 0x950a,
		PCI_SUBVENDOR_ID_SIIG, PCI_SUBDEVICE_ID_SIIG_DUAL_SERIAL, 0, 0,
		pbn_b0_2_115200 },
	{	PCI_VENDOR_ID_OXSEMI, 0x950a,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_2_1130000 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_C950,
		PCI_VENDOR_ID_OXSEMI, PCI_SUBDEVICE_ID_OXSEMI_C950, 0, 0,
		pbn_b0_1_921600 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_115200 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI952,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_921600 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI958,
		PCI_ANY_ID , PCI_ANY_ID, 0, 0,
		pbn_b2_8_1152000 },

	{	PCI_VENDOR_ID_OXSEMI, 0xc101,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc105,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc11b,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc11f,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc120,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc124,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc138,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc13d,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc140,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc141,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc144,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc145,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc158,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_2_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc15d,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_2_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc208,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_4_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc20d,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_4_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc308,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_8_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc30d,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_8_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc40b,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc40f,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc41b,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc41f,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc42b,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc42f,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc43b,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc43f,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc44b,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc44f,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc45b,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc45f,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc46b,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc46f,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc47b,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc47f,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc48b,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc48f,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc49b,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc49f,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4ab,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4af,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4bb,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4bf,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4cb,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4cf,    
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_MAINPINE, 0x4000,	
		PCI_VENDOR_ID_MAINPINE, 0x4001, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_MAINPINE, 0x4000,	
		PCI_VENDOR_ID_MAINPINE, 0x4002, 0, 0,
		pbn_oxsemi_2_4000000 },
	{	PCI_VENDOR_ID_MAINPINE, 0x4000,	
		PCI_VENDOR_ID_MAINPINE, 0x4004, 0, 0,
		pbn_oxsemi_4_4000000 },
	{	PCI_VENDOR_ID_MAINPINE, 0x4000,	
		PCI_VENDOR_ID_MAINPINE, 0x4008, 0, 0,
		pbn_oxsemi_8_4000000 },

	{	PCI_VENDOR_ID_DIGI, PCIE_DEVICE_ID_NEO_2_OX_IBM,
		PCI_SUBVENDOR_ID_IBM, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_2_4000000 },

	{	PCI_VENDOR_ID_SBSMODULARIO, PCI_DEVICE_ID_OCTPRO,
		PCI_SUBVENDOR_ID_SBSMODULARIO, PCI_SUBDEVICE_ID_OCTPRO232, 0, 0,
		pbn_sbsxrsio },
	{	PCI_VENDOR_ID_SBSMODULARIO, PCI_DEVICE_ID_OCTPRO,
		PCI_SUBVENDOR_ID_SBSMODULARIO, PCI_SUBDEVICE_ID_OCTPRO422, 0, 0,
		pbn_sbsxrsio },
	{	PCI_VENDOR_ID_SBSMODULARIO, PCI_DEVICE_ID_OCTPRO,
		PCI_SUBVENDOR_ID_SBSMODULARIO, PCI_SUBDEVICE_ID_POCTAL232, 0, 0,
		pbn_sbsxrsio },
	{	PCI_VENDOR_ID_SBSMODULARIO, PCI_DEVICE_ID_OCTPRO,
		PCI_SUBVENDOR_ID_SBSMODULARIO, PCI_SUBDEVICE_ID_POCTAL422, 0, 0,
		pbn_sbsxrsio },

	{	PCI_VENDOR_ID_ATT, PCI_DEVICE_ID_ATT_VENUS_MODEM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_1_115200 },

	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_2_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_100L,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_1_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200L,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_2_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400L,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800L,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200I,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b4_bt_2_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400I,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b4_bt_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800I,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b4_bt_8_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400EH,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800EH,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800EHB,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_100E,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200E,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_2_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400E,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_4_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800E,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_8_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200EI,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_2_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200EISI,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_2_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400V3,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_410V3,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800V3,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800V3B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },

	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_10x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_460800 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_10x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_460800 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_10x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_460800 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_10x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_10x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_10x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_10x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_10x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_10x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_20x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_20x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_20x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_20x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_20x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_20x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_20x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_20x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_20x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_8S_20x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_8S_20x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_8S_20x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_921600 },

	{	PCI_VENDOR_ID_COMPUTONE, PCI_DEVICE_ID_COMPUTONE_PG,
		PCI_SUBVENDOR_ID_COMPUTONE, PCI_SUBDEVICE_ID_COMPUTONE_PG4,
		0, 0, pbn_computone_4 },
	{	PCI_VENDOR_ID_COMPUTONE, PCI_DEVICE_ID_COMPUTONE_PG,
		PCI_SUBVENDOR_ID_COMPUTONE, PCI_SUBDEVICE_ID_COMPUTONE_PG8,
		0, 0, pbn_computone_8 },
	{	PCI_VENDOR_ID_COMPUTONE, PCI_DEVICE_ID_COMPUTONE_PG,
		PCI_SUBVENDOR_ID_COMPUTONE, PCI_SUBDEVICE_ID_COMPUTONE_PG6,
		0, 0, pbn_computone_6 },

	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI95N,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi },
	{	PCI_VENDOR_ID_TIMEDIA, PCI_DEVICE_ID_TIMEDIA_1889,
		PCI_VENDOR_ID_TIMEDIA, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_1_921600 },

	{	PCI_VENDOR_ID_AFAVLAB, PCI_DEVICE_ID_AFAVLAB_P028,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_115200 },
	{	PCI_VENDOR_ID_AFAVLAB, PCI_DEVICE_ID_AFAVLAB_P030,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_115200 },

	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_DSERIAL,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUATRO_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUATRO_B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUATTRO_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUATTRO_B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_OCTO_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_OCTO_B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_PORT_PLUS,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUAD_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUAD_B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_SSERIAL,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_1_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_PORT_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_1_460800 },

	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF0,
		0x1204, 0x0004, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF0,
		0x1208, 0x0004, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF1,
		0x1208, 0x0004, 0, 0,
		pbn_b0_4_921600 },

	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF2,
		0x1204, 0x0004, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF2,
		0x1208, 0x0004, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF3,
		0x1208, 0x0004, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_DELL, PCI_DEVICE_ID_DELL_RAC4,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_1_1382400 },

	{	PCI_VENDOR_ID_DELL, PCI_DEVICE_ID_DELL_RACIII,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_1_1382400 },

	{	PCI_VENDOR_ID_MORETON, PCI_DEVICE_ID_RASTEL_2PORT,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_115200 },

	{	PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_80960_RP,
		0xE4BF, PCI_ANY_ID, 0, 0,
		pbn_intel_i960 },

	{	PCI_VENDOR_ID_XIRCOM, PCI_DEVICE_ID_XIRCOM_X3201_MDM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_115200 },
	{	PCI_VENDOR_ID_XIRCOM, PCI_DEVICE_ID_XIRCOM_RBM56G,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_115200 },


	{	PCI_VENDOR_ID_ROCKWELL, 0x1004,
		0x1048, 0x1500, 0, 0,
		pbn_b1_1_115200 },

	{	PCI_VENDOR_ID_SGI, PCI_DEVICE_ID_SGI_IOC3,
		0xFF00, 0, 0, 0,
		pbn_sgi_ioc3 },

	{	PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_DIVA,
		PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_DIVA_RMP3, 0, 0,
		pbn_b1_1_115200 },
	{	PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_DIVA,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_5_115200 },
	{	PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_DIVA_AUX,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_115200 },

	{	PCI_VENDOR_ID_DCI, PCI_DEVICE_ID_DCI_PCCOM2,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b3_2_115200 },
	{	PCI_VENDOR_ID_DCI, PCI_DEVICE_ID_DCI_PCCOM4,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b3_4_115200 },
	{	PCI_VENDOR_ID_DCI, PCI_DEVICE_ID_DCI_PCCOM8,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b3_8_115200 },

	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C152,
		PCI_ANY_ID, PCI_ANY_ID,
		0,
		0, pbn_exar_XR17C152 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C154,
		PCI_ANY_ID, PCI_ANY_ID,
		0,
		0, pbn_exar_XR17C154 },
	{	PCI_VENDOR_ID_EXAR, PCI_DEVICE_ID_EXAR_XR17C158,
		PCI_ANY_ID, PCI_ANY_ID,
		0,
		0, pbn_exar_XR17C158 },

	{	PCI_VENDOR_ID_TOPIC, PCI_DEVICE_ID_TOPIC_TP560,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_115200 },
	{	PCI_VENDOR_ID_ITE, PCI_DEVICE_ID_ITE_8872,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b1_bt_1_115200 },

	{	PCI_VENDOR_ID_INTASHIELD, PCI_DEVICE_ID_INTASHIELD_IS200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,	
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, PCI_DEVICE_ID_INTASHIELD_IS400,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,    
		pbn_b2_4_115200 },
	{       PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9030,
		PCI_SUBVENDOR_ID_PERLE, PCI_SUBDEVICE_ID_PCI_RAS4,
		0, 0, pbn_b2_4_921600 },
	{       PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9030,
		PCI_SUBVENDOR_ID_PERLE, PCI_SUBDEVICE_ID_PCI_RAS8,
		0, 0, pbn_b2_8_921600 },


	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0200,
		0, 0, pbn_b0_2_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0300,
		0, 0, pbn_b0_4_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0400,
		0, 0, pbn_b0_2_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0500,
		0, 0, pbn_b0_4_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0600,
		0, 0, pbn_b0_2_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0700,
		0, 0, pbn_b0_4_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0800,
		0, 0, pbn_b0_8_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0C00,
		0, 0, pbn_b0_2_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0D00,
		0, 0, pbn_b0_4_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x1D00,
		0, 0, pbn_b0_8_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2000,
		0, 0, pbn_b0_1_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2100,
		0, 0, pbn_b0_1_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2200,
		0, 0, pbn_b0_2_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2300,
		0, 0, pbn_b0_2_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2400,
		0, 0, pbn_b0_4_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2500,
		0, 0, pbn_b0_4_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2600,
		0, 0, pbn_b0_8_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2700,
		0, 0, pbn_b0_8_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3000,
		0, 0, pbn_b0_1_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3100,
		0, 0, pbn_b0_1_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3200,
		0, 0, pbn_b0_2_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3300,
		0, 0, pbn_b0_2_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3400,
		0, 0, pbn_b0_4_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3500,
		0, 0, pbn_b0_4_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3C00,
		0, 0, pbn_b0_8_115200 },
	{	
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3D00,
		0, 0, pbn_b0_8_115200 },


	{	PCI_VENDOR_ID_PASEMI, 0xa004,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_pasemi_1682M },

	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI23216,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_16_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI2328,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_8_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_4_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_2_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI2324I,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_4_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI2322I,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_2_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8420_23216,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_16_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8420_2328,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_8_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8420_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_4_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8420_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_2_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8422_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_4_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8422_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_2_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8430_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_2 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8430_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_2 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8430_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_4 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8430_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_4 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8430_2328,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_8 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8430_2328,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_8 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8430_23216,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_16 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8430_23216,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_16 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8432_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_2 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8432_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_2 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8432_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_4 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8432_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_4 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7500,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_4_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7420,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_2_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7300,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_ADDIDATA_OLD,
		PCI_DEVICE_ID_ADDIDATA_APCI7800,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b1_8_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7500_2,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_4_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7420_2,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_2_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7300_2,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7500_3,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_4_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7420_3,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_2_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7300_3,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7800_3,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_8_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCIe7500,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_ADDIDATA_PCIe_4_3906250 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCIe7420,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_ADDIDATA_PCIe_2_3906250 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCIe7300,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_ADDIDATA_PCIe_1_3906250 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCIe7800,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_ADDIDATA_PCIe_8_3906250 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9835,
		PCI_VENDOR_ID_IBM, 0x0299,
		0, 0, pbn_b0_bt_2_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9901,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	
	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9912,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9922,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9904,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9900,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9900,
		0xA000, 0x3002,
		0, 0, pbn_NETMOS9900_2s_115200 },


	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9865,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9865,
		0xA000, 0x3002,
		0, 0, pbn_b0_bt_2_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9865,
		0xA000, 0x3004,
		0, 0, pbn_b0_bt_4_115200 },
	
	{	PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_CE4100_UART,
		PCI_ANY_ID,  PCI_ANY_ID, 0, 0,
		pbn_ce4100_1_115200 },

	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_CRONYX_OMEGA,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_omegapci },

	{	PCI_ANY_ID, PCI_ANY_ID,
		PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_COMMUNICATION_SERIAL << 8,
		0xffff00, pbn_default },
	{	PCI_ANY_ID, PCI_ANY_ID,
		PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_COMMUNICATION_MODEM << 8,
		0xffff00, pbn_default },
	{	PCI_ANY_ID, PCI_ANY_ID,
		PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_COMMUNICATION_MULTISERIAL << 8,
		0xffff00, pbn_default },
	{ 0, }
};

static pci_ers_result_t serial8250_io_error_detected(struct pci_dev *dev,
						pci_channel_state_t state)
{
	struct serial_private *priv = pci_get_drvdata(dev);

	if (state == pci_channel_io_perm_failure)
		return PCI_ERS_RESULT_DISCONNECT;

	if (priv)
		pciserial_suspend_ports(priv);

	pci_disable_device(dev);

	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t serial8250_io_slot_reset(struct pci_dev *dev)
{
	int rc;

	rc = pci_enable_device(dev);

	if (rc)
		return PCI_ERS_RESULT_DISCONNECT;

	pci_restore_state(dev);
	pci_save_state(dev);

	return PCI_ERS_RESULT_RECOVERED;
}

static void serial8250_io_resume(struct pci_dev *dev)
{
	struct serial_private *priv = pci_get_drvdata(dev);

	if (priv)
		pciserial_resume_ports(priv);
}

static struct pci_error_handlers serial8250_err_handler = {
	.error_detected = serial8250_io_error_detected,
	.slot_reset = serial8250_io_slot_reset,
	.resume = serial8250_io_resume,
};

static struct pci_driver serial_pci_driver = {
	.name		= "serial",
	.probe		= pciserial_init_one,
	.remove		= __devexit_p(pciserial_remove_one),
#ifdef CONFIG_PM
	.suspend	= pciserial_suspend_one,
	.resume		= pciserial_resume_one,
#endif
	.id_table	= serial_pci_tbl,
	.err_handler	= &serial8250_err_handler,
};

static int __init serial8250_pci_init(void)
{
	return pci_register_driver(&serial_pci_driver);
}

static void __exit serial8250_pci_exit(void)
{
	pci_unregister_driver(&serial_pci_driver);
}

module_init(serial8250_pci_init);
module_exit(serial8250_pci_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Generic 8250/16x50 PCI serial probe module");
MODULE_DEVICE_TABLE(pci, serial_pci_tbl);
