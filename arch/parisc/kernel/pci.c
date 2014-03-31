/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997, 1998 Ralf Baechle
 * Copyright (C) 1999 SuSE GmbH
 * Copyright (C) 1999-2001 Hewlett-Packard Company
 * Copyright (C) 1999-2001 Grant Grundler
 */
#include <linux/eisa.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/types.h>

#include <asm/io.h>
#include <asm/superio.h>

#define DEBUG_RESOURCES 0
#define DEBUG_CONFIG 0

#if DEBUG_CONFIG
# define DBGC(x...)	printk(KERN_DEBUG x)
#else
# define DBGC(x...)
#endif


#if DEBUG_RESOURCES
#define DBG_RES(x...)	printk(KERN_DEBUG x)
#else
#define DBG_RES(x...)
#endif


struct pci_port_ops *pci_port __read_mostly;
struct pci_bios_ops *pci_bios __read_mostly;

static int pci_hba_count __read_mostly;

#define PCI_HBA_MAX 32
static struct pci_hba_data *parisc_pci_hba[PCI_HBA_MAX] __read_mostly;




#ifdef CONFIG_EISA
#define EISA_IN(size) if (EISA_bus && (b == 0)) return eisa_in##size(addr)
#define EISA_OUT(size) if (EISA_bus && (b == 0)) return eisa_out##size(d, addr)
#else
#define EISA_IN(size)
#define EISA_OUT(size)
#endif

#define PCI_PORT_IN(type, size) \
u##size in##type (int addr) \
{ \
	int b = PCI_PORT_HBA(addr); \
	EISA_IN(size); \
	if (!parisc_pci_hba[b]) return (u##size) -1; \
	return pci_port->in##type(parisc_pci_hba[b], PCI_PORT_ADDR(addr)); \
} \
EXPORT_SYMBOL(in##type);

PCI_PORT_IN(b,  8)
PCI_PORT_IN(w, 16)
PCI_PORT_IN(l, 32)


#define PCI_PORT_OUT(type, size) \
void out##type (u##size d, int addr) \
{ \
	int b = PCI_PORT_HBA(addr); \
	EISA_OUT(size); \
	if (!parisc_pci_hba[b]) return; \
	pci_port->out##type(parisc_pci_hba[b], PCI_PORT_ADDR(addr), d); \
} \
EXPORT_SYMBOL(out##type);

PCI_PORT_OUT(b,  8)
PCI_PORT_OUT(w, 16)
PCI_PORT_OUT(l, 32)



static int __init pcibios_init(void)
{
	if (!pci_bios)
		return -1;

	if (pci_bios->init) {
		pci_bios->init();
	} else {
		printk(KERN_WARNING "pci_bios != NULL but init() is!\n");
	}

	
	pci_cache_line_size = pci_dfl_cache_line_size;

	return 0;
}


void pcibios_fixup_bus(struct pci_bus *bus)
{
	if (pci_bios->fixup_bus) {
		pci_bios->fixup_bus(bus);
	} else {
		printk(KERN_WARNING "pci_bios != NULL but fixup_bus() is!\n");
	}
}


char *pcibios_setup(char *str)
{
	return str;
}

void pcibios_set_master(struct pci_dev *dev)
{
	u8 lat;

	
	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lat);
	if (lat >= 16) return;

	pci_write_config_word(dev, PCI_CACHE_LINE_SIZE,
			      (0x80 << 8) | pci_cache_line_size);
}


void __init pcibios_init_bus(struct pci_bus *bus)
{
	struct pci_dev *dev = bus->self;
	unsigned short bridge_ctl;

	
	if (!dev || (dev->class >> 8) != PCI_CLASS_BRIDGE_PCI)
		return;

	pci_write_config_byte(dev, PCI_SEC_LATENCY_TIMER, 32);

	pci_read_config_word(dev, PCI_BRIDGE_CONTROL, &bridge_ctl);
	bridge_ctl |= PCI_BRIDGE_CTL_PARITY | PCI_BRIDGE_CTL_SERR;
	pci_write_config_word(dev, PCI_BRIDGE_CONTROL, bridge_ctl);
}

resource_size_t pcibios_align_resource(void *data, const struct resource *res,
				resource_size_t size, resource_size_t alignment)
{
	resource_size_t mask, align, start = res->start;

	DBG_RES("pcibios_align_resource(%s, (%p) [%lx,%lx]/%x, 0x%lx, 0x%lx)\n",
		pci_name(((struct pci_dev *) data)),
		res->parent, res->start, res->end,
		(int) res->flags, size, alignment);

	
	align = (res->flags & IORESOURCE_IO) ? PCIBIOS_MIN_IO : PCIBIOS_MIN_MEM;

	
	mask = max(alignment, align) - 1;
	start += mask;
	start &= ~mask;

	return start;
}


int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	int err;
	u16 cmd, old_cmd;

	err = pci_enable_resources(dev, mask);
	if (err < 0)
		return err;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;

	cmd |= (PCI_COMMAND_SERR | PCI_COMMAND_PARITY);

#if 0
	
	if (dev->bus->bridge_ctl & PCI_BRIDGE_CTL_FAST_BACK)
		cmd |= PCI_COMMAND_FAST_BACK;
#endif

	if (cmd != old_cmd) {
		dev_info(&dev->dev, "enabling SERR and PARITY (%04x -> %04x)\n",
			old_cmd, cmd);
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	return 0;
}


void pcibios_register_hba(struct pci_hba_data *hba)
{
	if (pci_hba_count >= PCI_HBA_MAX) {
		printk(KERN_ERR "PCI: Too many Host Bus Adapters\n");
		return;
	}

	parisc_pci_hba[pci_hba_count] = hba;
	hba->hba_num = pci_hba_count++;
}

subsys_initcall(pcibios_init);
