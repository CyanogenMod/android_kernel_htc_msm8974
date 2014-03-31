/*
 * PMC-Sierra MSP board specific pci_ops
 *
 * Copyright 2001 MontaVista Software Inc.
 * Copyright 2005-2007 PMC-Sierra, Inc
 *
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 * Much of the code is derived from the original DDB5074 port by
 * Geert Uytterhoeven <geert@sonycom.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#define PCI_COUNTERS	1

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

#if defined(CONFIG_PROC_FS) && defined(PCI_COUNTERS)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif 

#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/byteorder.h>
#if defined(CONFIG_PMC_MSP7120_GW) || defined(CONFIG_PMC_MSP7120_EVAL)
#include <asm/mipsmtregs.h>
#endif

#include <msp_prom.h>
#include <msp_cic_int.h>
#include <msp_pci.h>
#include <msp_regs.h>
#include <msp_regops.h>

#define PCI_ACCESS_READ		0
#define PCI_ACCESS_WRITE	1

#if defined(CONFIG_PROC_FS) && defined(PCI_COUNTERS)
static char proc_init;
extern struct proc_dir_entry *proc_bus_pci_dir;
unsigned int pci_int_count[32];

static void pci_proc_init(void);

static int read_msp_pci_counts(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	int i;
	int len = 0;
	unsigned int intcount, total = 0;

	for (i = 0; i < 32; ++i) {
		intcount = pci_int_count[i];
		if (intcount != 0) {
			len += sprintf(page + len, "[%d] = %u\n", i, intcount);
			total += intcount;
		}
	}

	len += sprintf(page + len, "total = %u\n", total);
	if (len <= off+count)
		*eof = 1;

	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;

	return len;
}

/*****************************************************************************
 *
 *  FUNCTION: gen_pci_cfg_wr
 *  _________________________________________________________________________
 *
 *  DESCRIPTION: Generates a configuration write cycle for debug purposes.
 *               The IDSEL line asserted and location and data written are
 *               immaterial. Just want to be able to prove that a
 *               configuration write can be correctly generated on the
 *               PCI bus.  Intent is that this function by invocable from
 *               the /proc filesystem.
 *
 *  INPUTS:      page    - part of STDOUT calculation
 *               off     - part of STDOUT calculation
 *               count   - part of STDOUT calculation
 *               data    - unused
 *
 *  OUTPUTS:     start   - new start location
 *               eof     - end of file pointer
 *
 *  RETURNS:     len     - STDOUT length
 *
 ****************************************************************************/
static int gen_pci_cfg_wr(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	unsigned char where = 0; 
	unsigned char bus_num = 0; 
	unsigned char dev_fn = 0xF; 
	u32 wr_data = 0xFF00AA00; 
	struct msp_pci_regs *preg = (void *)PCI_BASE_REG;
	int len = 0;
	unsigned long value;
	int intr;

	len += sprintf(page + len, "PMC MSP PCI: Beginning\n");

	if (proc_init == 0) {
		pci_proc_init();
		proc_init = ~0;
	}

	len += sprintf(page + len, "PMC MSP PCI: Before Cfg Wr\n");


	
	preg->if_status = ~(BPCI_IFSTATUS_BC0F | BPCI_IFSTATUS_BC1F);

	
	preg->config_addr = BPCI_CFGADDR_ENABLE |
		(bus_num << BPCI_CFGADDR_BUSNUM_SHF) |
		(dev_fn << BPCI_CFGADDR_FUNCTNUM_SHF) |
		(where & 0xFC);

	value = cpu_to_le32(wr_data);

	
	*PCI_CONFIG_SPACE_REG = value;

	intr = preg->if_status;

	len += sprintf(page + len, "PMC MSP PCI: After Cfg Wr\n");

	
	if (len <= off+count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;

	return len;
}

static void pci_proc_init(void)
{
	create_proc_read_entry("pmc_msp_pci_rd_cnt", 0, NULL,
				read_msp_pci_counts, NULL);
	create_proc_read_entry("pmc_msp_pci_cfg_wr", 0, NULL,
				gen_pci_cfg_wr, NULL);
}
#endif 

static DEFINE_SPINLOCK(bpci_lock);

static struct resource pci_io_resource = {
	.name	= "pci IO space",
	.start	= 0x04,
	.end	= 0x0FFF,
	.flags	= IORESOURCE_IO	
};

static struct resource pci_mem_resource = {
	.name	= "pci memory space",
	.start	= MSP_PCI_SPACE_BASE,
	.end	= MSP_PCI_SPACE_END,
	.flags	= IORESOURCE_MEM	 
};

static irqreturn_t bpci_interrupt(int irq, void *dev_id)
{
	struct msp_pci_regs *preg = (void *)PCI_BASE_REG;
	unsigned int stat = preg->if_status;

#if defined(CONFIG_PROC_FS) && defined(PCI_COUNTERS)
	int i;
	for (i = 0; i < 32; ++i) {
		if ((1 << i) & stat)
			++pci_int_count[i];
	}
#endif 

	

	
	preg->if_status = stat;

	return IRQ_HANDLED;
}

int msp_pcibios_config_access(unsigned char access_type,
				struct pci_bus *bus,
				unsigned int devfn,
				unsigned char where,
				u32 *data)
{
	struct msp_pci_regs *preg = (void *)PCI_BASE_REG;
	unsigned char bus_num = bus->number;
	unsigned char dev_fn = (unsigned char)devfn;
	unsigned long flags;
	unsigned long intr;
	unsigned long value;
	static char pciirqflag;
	int ret;
#if defined(CONFIG_PMC_MSP7120_GW) || defined(CONFIG_PMC_MSP7120_EVAL)
	unsigned int	vpe_status;
#endif

#if defined(CONFIG_PROC_FS) && defined(PCI_COUNTERS)
	if (proc_init == 0) {
		pci_proc_init();
		proc_init = ~0;
	}
#endif 

	if (pciirqflag == 0) {
		ret = request_irq(MSP_INT_PCI,
				bpci_interrupt,
				IRQF_SHARED,
				"PMC MSP PCI Host",
				preg);
		if (ret != 0)
			return ret;
		pciirqflag = ~0;
	}

#if defined(CONFIG_PMC_MSP7120_GW) || defined(CONFIG_PMC_MSP7120_EVAL)
	local_irq_save(flags);
	vpe_status = dvpe();
#else
	spin_lock_irqsave(&bpci_lock, flags);
#endif

	preg->if_status = ~(BPCI_IFSTATUS_BC0F | BPCI_IFSTATUS_BC1F);

	
	preg->config_addr = BPCI_CFGADDR_ENABLE	|
		(bus_num << BPCI_CFGADDR_BUSNUM_SHF) |
		(dev_fn << BPCI_CFGADDR_FUNCTNUM_SHF) |
		(where & 0xFC);

	
	if (access_type == PCI_ACCESS_WRITE) {
		value = cpu_to_le32(*data);
		*PCI_CONFIG_SPACE_REG = value;
	} else {
		
		value = le32_to_cpu(*PCI_CONFIG_SPACE_REG);
		*data = value;
	}

	intr = preg->if_status;

	
	preg->config_addr = 0;

	
	if (intr & ~(BPCI_IFSTATUS_BC0F | BPCI_IFSTATUS_BC1F)) {
		
		preg->if_status = ~(BPCI_IFSTATUS_BC0F | BPCI_IFSTATUS_BC1F);

#if defined(CONFIG_PMC_MSP7120_GW) || defined(CONFIG_PMC_MSP7120_EVAL)
		evpe(vpe_status);
		local_irq_restore(flags);
#else
		spin_unlock_irqrestore(&bpci_lock, flags);
#endif

		return -1;
	}

#if defined(CONFIG_PMC_MSP7120_GW) || defined(CONFIG_PMC_MSP7120_EVAL)
	evpe(vpe_status);
	local_irq_restore(flags);
#else
	spin_unlock_irqrestore(&bpci_lock, flags);
#endif

	return PCIBIOS_SUCCESSFUL;
}

static int
msp_pcibios_read_config_byte(struct pci_bus *bus,
				unsigned int devfn,
				int where,
				u32 *val)
{
	u32 data = 0;

	if (msp_pcibios_config_access(PCI_ACCESS_READ, bus, devfn,
					where, &data)) {
		*val = 0xFFFFFFFF;
		return -1;
	}

	*val = (data >> ((where & 3) << 3)) & 0x0ff;

	return PCIBIOS_SUCCESSFUL;
}

static int
msp_pcibios_read_config_word(struct pci_bus *bus,
				unsigned int devfn,
				int where,
				u32 *val)
{
	u32 data = 0;

	if ((where & 3) == 3) {
		*val = 0xFFFFFFFF;
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}

	if (msp_pcibios_config_access(PCI_ACCESS_READ, bus, devfn,
					where, &data)) {
		*val = 0xFFFFFFFF;
		return -1;
	}

	*val = (data >> ((where & 3) << 3)) & 0x0ffff;

	return PCIBIOS_SUCCESSFUL;
}

static int
msp_pcibios_read_config_dword(struct pci_bus *bus,
				unsigned int devfn,
				int where,
				u32 *val)
{
	u32 data = 0;

	
	if (where & 3) {
		*val = 0xFFFFFFFF;
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}

	if (msp_pcibios_config_access(PCI_ACCESS_READ, bus, devfn,
					where, &data)) {
		*val = 0xFFFFFFFF;
		return -1;
	}

	*val = data;

	return PCIBIOS_SUCCESSFUL;
}

static int
msp_pcibios_write_config_byte(struct pci_bus *bus,
				unsigned int devfn,
				int where,
				u8 val)
{
	u32 data = 0;

	
	if (msp_pcibios_config_access(PCI_ACCESS_READ, bus, devfn,
					where, &data))
		return -1;

	
	data = (data & ~(0xff << ((where & 3) << 3))) |
			(val << ((where & 3) << 3));

	
	if (msp_pcibios_config_access(PCI_ACCESS_WRITE, bus, devfn,
					where, &data))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}

static int
msp_pcibios_write_config_word(struct pci_bus *bus,
				unsigned int devfn,
				int where,
				u16 val)
{
	u32 data = 0;

	
	if ((where & 3) == 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	
	if (msp_pcibios_config_access(PCI_ACCESS_READ, bus, devfn,
					where, &data))
		return -1;

	
	data = (data & ~(0xffff << ((where & 3) << 3))) |
			(val << ((where & 3) << 3));

	
	if (msp_pcibios_config_access(PCI_ACCESS_WRITE, bus, devfn,
					where, &data))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}

static int
msp_pcibios_write_config_dword(struct pci_bus *bus,
				unsigned int devfn,
				int where,
				u32 val)
{
	
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	
	if (msp_pcibios_config_access(PCI_ACCESS_WRITE, bus, devfn,
					where, &val))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}

int
msp_pcibios_read_config(struct pci_bus *bus,
			unsigned int	devfn,
			int where,
			int size,
			u32 *val)
{
	if (size == 1) {
		if (msp_pcibios_read_config_byte(bus, devfn, where, val)) {
			return -1;
		}
	} else if (size == 2) {
		if (msp_pcibios_read_config_word(bus, devfn, where, val)) {
			return -1;
		}
	} else if (size == 4) {
		if (msp_pcibios_read_config_dword(bus, devfn, where, val)) {
			return -1;
		}
	} else {
		*val = 0xFFFFFFFF;
		return -1;
	}

	return PCIBIOS_SUCCESSFUL;
}

int
msp_pcibios_write_config(struct pci_bus *bus,
			unsigned int devfn,
			int where,
			int size,
			u32 val)
{
	if (size == 1) {
		if (msp_pcibios_write_config_byte(bus, devfn,
						where, (u8)(0xFF & val))) {
			return -1;
		}
	} else if (size == 2) {
		if (msp_pcibios_write_config_word(bus, devfn,
						where, (u16)(0xFFFF & val))) {
			return -1;
		}
	} else if (size == 4) {
		if (msp_pcibios_write_config_dword(bus, devfn, where, val)) {
			return -1;
		}
	} else {
		return -1;
	}

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops msp_pci_ops = {
	.read = msp_pcibios_read_config,
	.write = msp_pcibios_write_config
};

static struct pci_controller msp_pci_controller = {
	.pci_ops	= &msp_pci_ops,
	.mem_resource	= &pci_mem_resource,
	.mem_offset	= 0,
	.io_map_base	= MSP_PCI_IOSPACE_BASE,
	.io_resource	= &pci_io_resource,
	.io_offset	= 0
};

void __init msp_pci_init(void)
{
	struct msp_pci_regs *preg = (void *)PCI_BASE_REG;
	u32 id;

	
	id = read_reg32(PCI_JTAG_DEVID_REG, 0xFFFF) >> 12;

	
	if (!MSP_HAS_PCI(id)) {
		printk(KERN_WARNING "PCI: No PCI; id reads as %x\n", id);
		goto no_pci;
	}

	*(unsigned long *)QFLUSH_REG_1 = 3;

	
	preg->if_status	= ~0;		
	preg->config_addr = 0;		
	preg->oatran	= MSP_PCI_OATRAN; 
	preg->if_mask	= 0xF8BF87C0;	

	
	set_io_port_base(MSP_PCI_IOSPACE_BASE);

	
	register_pci_controller(&msp_pci_controller);

	return;

no_pci:
	
	printk(KERN_WARNING "PCI: no host PCI bus detected\n");
}
