/*
 *  linux/arch/arm/mach-integrator/pci_v3.c
 *
 *  PCI functions for V3 host PCI bridge
 *
 *  Copyright (C) 1999 ARM Limited
 *  Copyright (C) 2000-2001 Deep Blue Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <mach/platform.h>
#include <mach/irqs.h>

#include <asm/signal.h>
#include <asm/mach/pci.h>
#include <asm/irq_regs.h>

#include <asm/hardware/pci_v3.h>


#define v3_writeb(o,v) __raw_writeb(v, PCI_V3_VADDR + (unsigned int)(o))
#define v3_readb(o)    (__raw_readb(PCI_V3_VADDR + (unsigned int)(o)))

#define v3_writew(o,v) __raw_writew(v, PCI_V3_VADDR + (unsigned int)(o))
#define v3_readw(o)    (__raw_readw(PCI_V3_VADDR + (unsigned int)(o)))

#define v3_writel(o,v) __raw_writel(v, PCI_V3_VADDR + (unsigned int)(o))
#define v3_readl(o)    (__raw_readl(PCI_V3_VADDR + (unsigned int)(o)))

static DEFINE_RAW_SPINLOCK(v3_lock);

#define PCI_BUS_NONMEM_START	0x00000000
#define PCI_BUS_NONMEM_SIZE	SZ_256M

#define PCI_BUS_PREMEM_START	PCI_BUS_NONMEM_START + PCI_BUS_NONMEM_SIZE
#define PCI_BUS_PREMEM_SIZE	SZ_256M

#if PCI_BUS_NONMEM_START & 0x000fffff
#error PCI_BUS_NONMEM_START must be megabyte aligned
#endif
#if PCI_BUS_PREMEM_START & 0x000fffff
#error PCI_BUS_PREMEM_START must be megabyte aligned
#endif

#undef V3_LB_BASE_PREFETCH
#define V3_LB_BASE_PREFETCH 0

static unsigned long v3_open_config_window(struct pci_bus *bus,
					   unsigned int devfn, int offset)
{
	unsigned int address, mapaddress, busnr;

	busnr = bus->number;

	if (offset > 255)
		BUG();
	if (busnr > 255)
		BUG();
	if (devfn > 255)
		BUG();

	if (busnr == 0) {
		int slot = PCI_SLOT(devfn);

		address = PCI_FUNC(devfn) << 8;
		mapaddress = V3_LB_MAP_TYPE_CONFIG;

		if (slot > 12)
			mapaddress |= 1 << (slot - 5);
		else
			address |= 1 << (slot + 11);
	} else {
		mapaddress = V3_LB_MAP_TYPE_CONFIG | V3_LB_MAP_AD_LOW_EN;
		address = (busnr << 16) | (devfn << 8);
	}

	v3_writel(V3_LB_BASE0, v3_addr_to_lb_base(PHYS_PCI_MEM_BASE) |
			V3_LB_BASE_ADR_SIZE_512MB | V3_LB_BASE_ENABLE);

	v3_writel(V3_LB_BASE1, v3_addr_to_lb_base(PHYS_PCI_CONFIG_BASE) |
			V3_LB_BASE_ADR_SIZE_16MB | V3_LB_BASE_ENABLE);
	v3_writew(V3_LB_MAP1, mapaddress);

	return PCI_CONFIG_VADDR + address + offset;
}

static void v3_close_config_window(void)
{
	v3_writel(V3_LB_BASE1, v3_addr_to_lb_base(PHYS_PCI_MEM_BASE + SZ_256M) |
			V3_LB_BASE_ADR_SIZE_256MB | V3_LB_BASE_PREFETCH |
			V3_LB_BASE_ENABLE);
	v3_writew(V3_LB_MAP1, v3_addr_to_lb_map(PCI_BUS_PREMEM_START) |
			V3_LB_MAP_TYPE_MEM_MULTIPLE);

	v3_writel(V3_LB_BASE0, v3_addr_to_lb_base(PHYS_PCI_MEM_BASE) |
			V3_LB_BASE_ADR_SIZE_256MB | V3_LB_BASE_ENABLE);
}

static int v3_read_config(struct pci_bus *bus, unsigned int devfn, int where,
			  int size, u32 *val)
{
	unsigned long addr;
	unsigned long flags;
	u32 v;

	raw_spin_lock_irqsave(&v3_lock, flags);
	addr = v3_open_config_window(bus, devfn, where);

	switch (size) {
	case 1:
		v = __raw_readb(addr);
		break;

	case 2:
		v = __raw_readw(addr);
		break;

	default:
		v = __raw_readl(addr);
		break;
	}

	v3_close_config_window();
	raw_spin_unlock_irqrestore(&v3_lock, flags);

	*val = v;
	return PCIBIOS_SUCCESSFUL;
}

static int v3_write_config(struct pci_bus *bus, unsigned int devfn, int where,
			   int size, u32 val)
{
	unsigned long addr;
	unsigned long flags;

	raw_spin_lock_irqsave(&v3_lock, flags);
	addr = v3_open_config_window(bus, devfn, where);

	switch (size) {
	case 1:
		__raw_writeb((u8)val, addr);
		__raw_readb(addr);
		break;

	case 2:
		__raw_writew((u16)val, addr);
		__raw_readw(addr);
		break;

	case 4:
		__raw_writel(val, addr);
		__raw_readl(addr);
		break;
	}

	v3_close_config_window();
	raw_spin_unlock_irqrestore(&v3_lock, flags);

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops pci_v3_ops = {
	.read	= v3_read_config,
	.write	= v3_write_config,
};

static struct resource non_mem = {
	.name	= "PCI non-prefetchable",
	.start	= PHYS_PCI_MEM_BASE + PCI_BUS_NONMEM_START,
	.end	= PHYS_PCI_MEM_BASE + PCI_BUS_NONMEM_START + PCI_BUS_NONMEM_SIZE - 1,
	.flags	= IORESOURCE_MEM,
};

static struct resource pre_mem = {
	.name	= "PCI prefetchable",
	.start	= PHYS_PCI_MEM_BASE + PCI_BUS_PREMEM_START,
	.end	= PHYS_PCI_MEM_BASE + PCI_BUS_PREMEM_START + PCI_BUS_PREMEM_SIZE - 1,
	.flags	= IORESOURCE_MEM | IORESOURCE_PREFETCH,
};

static int __init pci_v3_setup_resources(struct pci_sys_data *sys)
{
	if (request_resource(&iomem_resource, &non_mem)) {
		printk(KERN_ERR "PCI: unable to allocate non-prefetchable "
		       "memory region\n");
		return -EBUSY;
	}
	if (request_resource(&iomem_resource, &pre_mem)) {
		release_resource(&non_mem);
		printk(KERN_ERR "PCI: unable to allocate prefetchable "
		       "memory region\n");
		return -EBUSY;
	}

	pci_add_resource_offset(&sys->resources,
				&ioport_resource, sys->io_offset);
	pci_add_resource_offset(&sys->resources, &non_mem, sys->mem_offset);
	pci_add_resource_offset(&sys->resources, &pre_mem, sys->mem_offset);

	return 1;
}

#define SC_PCI     IO_ADDRESS(INTEGRATOR_SC_PCIENABLE)
#define SC_LBFADDR IO_ADDRESS(INTEGRATOR_SC_BASE + 0x20)
#define SC_LBFCODE IO_ADDRESS(INTEGRATOR_SC_BASE + 0x24)

static int
v3_pci_fault(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
	unsigned long pc = instruction_pointer(regs);
	unsigned long instr = *(unsigned long *)pc;
#if 0
	char buf[128];

	sprintf(buf, "V3 fault: addr 0x%08lx, FSR 0x%03x, PC 0x%08lx [%08lx] LBFADDR=%08x LBFCODE=%02x ISTAT=%02x\n",
		addr, fsr, pc, instr, __raw_readl(SC_LBFADDR), __raw_readl(SC_LBFCODE) & 255,
		v3_readb(V3_LB_ISTAT));
	printk(KERN_DEBUG "%s", buf);
#endif

	v3_writeb(V3_LB_ISTAT, 0);
	__raw_writel(3, SC_PCI);

	if ((instr & 0x0c100000) == 0x04100000) {
		int reg = (instr >> 12) & 15;
		unsigned long val;

		if (instr & 0x00400000)
			val = 255;
		else
			val = -1;

		regs->uregs[reg] = val;
		regs->ARM_pc += 4;
		return 0;
	}

	if ((instr & 0x0e100090) == 0x00100090) {
		int reg = (instr >> 12) & 15;

		regs->uregs[reg] = -1;
		regs->ARM_pc += 4;
		return 0;
	}

	return 1;
}

static irqreturn_t v3_irq(int dummy, void *devid)
{
#ifdef CONFIG_DEBUG_LL
	struct pt_regs *regs = get_irq_regs();
	unsigned long pc = instruction_pointer(regs);
	unsigned long instr = *(unsigned long *)pc;
	char buf[128];
	extern void printascii(const char *);

	sprintf(buf, "V3 int %d: pc=0x%08lx [%08lx] LBFADDR=%08x LBFCODE=%02x "
		"ISTAT=%02x\n", IRQ_AP_V3INT, pc, instr,
		__raw_readl(SC_LBFADDR),
		__raw_readl(SC_LBFCODE) & 255,
		v3_readb(V3_LB_ISTAT));
	printascii(buf);
#endif

	v3_writew(V3_PCI_STAT, 0xf000);
	v3_writeb(V3_LB_ISTAT, 0);
	__raw_writel(3, SC_PCI);

#ifdef CONFIG_DEBUG_LL
	if ((instr & 0x0c100000) == 0x04100000) {
		int reg = (instr >> 16) & 15;
		sprintf(buf, "   reg%d = %08lx\n", reg, regs->uregs[reg]);
		printascii(buf);
	}
#endif
	return IRQ_HANDLED;
}

int __init pci_v3_setup(int nr, struct pci_sys_data *sys)
{
	int ret = 0;

	if (nr == 0) {
		sys->mem_offset = PHYS_PCI_MEM_BASE;
		ret = pci_v3_setup_resources(sys);
	}

	return ret;
}

struct pci_bus * __init pci_v3_scan_bus(int nr, struct pci_sys_data *sys)
{
	return pci_scan_root_bus(NULL, sys->busnr, &pci_v3_ops, sys,
				 &sys->resources);
}

void __init pci_v3_preinit(void)
{
	unsigned long flags;
	unsigned int temp;
	int ret;

	pcibios_min_io = 0x6000;
	pcibios_min_mem = 0x00100000;

	hook_fault_code(4, v3_pci_fault, SIGBUS, 0, "external abort on linefetch");
	hook_fault_code(6, v3_pci_fault, SIGBUS, 0, "external abort on linefetch");
	hook_fault_code(8, v3_pci_fault, SIGBUS, 0, "external abort on non-linefetch");
	hook_fault_code(10, v3_pci_fault, SIGBUS, 0, "external abort on non-linefetch");

	raw_spin_lock_irqsave(&v3_lock, flags);

	if (v3_readw(V3_SYSTEM) & V3_SYSTEM_M_LOCK)
		v3_writew(V3_SYSTEM, 0xa05f);

	v3_writel(V3_LB_BASE0, v3_addr_to_lb_base(PHYS_PCI_MEM_BASE) |
			V3_LB_BASE_ADR_SIZE_256MB | V3_LB_BASE_ENABLE);
	v3_writew(V3_LB_MAP0, v3_addr_to_lb_map(PCI_BUS_NONMEM_START) |
			V3_LB_MAP_TYPE_MEM);

	v3_writel(V3_LB_BASE1, v3_addr_to_lb_base(PHYS_PCI_MEM_BASE + SZ_256M) |
			V3_LB_BASE_ADR_SIZE_256MB | V3_LB_BASE_PREFETCH |
			V3_LB_BASE_ENABLE);
	v3_writew(V3_LB_MAP1, v3_addr_to_lb_map(PCI_BUS_PREMEM_START) |
			V3_LB_MAP_TYPE_MEM_MULTIPLE);

	v3_writel(V3_LB_BASE2, v3_addr_to_lb_base2(PHYS_PCI_IO_BASE) |
			V3_LB_BASE_ENABLE);
	v3_writew(V3_LB_MAP2, v3_addr_to_lb_map2(0));

	temp = v3_readw(V3_PCI_CFG) & ~V3_PCI_CFG_M_I2O_EN;
	temp |= V3_PCI_CFG_M_IO_REG_DIS | V3_PCI_CFG_M_IO_DIS;
	v3_writew(V3_PCI_CFG, temp);

	printk(KERN_DEBUG "FIFO_CFG: %04x  FIFO_PRIO: %04x\n",
		v3_readw(V3_FIFO_CFG), v3_readw(V3_FIFO_PRIORITY));

	v3_writew(V3_FIFO_PRIORITY, 0x0a0a);

	temp = v3_readw(V3_SYSTEM) | V3_SYSTEM_M_LOCK;
	v3_writew(V3_SYSTEM, temp);

	v3_writeb(V3_LB_ISTAT, 0);
	v3_writew(V3_LB_CFG, v3_readw(V3_LB_CFG) | (1 << 10));
	v3_writeb(V3_LB_IMASK, 0x28);
	__raw_writel(3, SC_PCI);

	ret = request_irq(IRQ_AP_V3INT, v3_irq, 0, "V3", NULL);
	if (ret)
		printk(KERN_ERR "PCI: unable to grab PCI error "
		       "interrupt: %d\n", ret);

	raw_spin_unlock_irqrestore(&v3_lock, flags);
}

void __init pci_v3_postinit(void)
{
	unsigned int pci_cmd;

	pci_cmd = PCI_COMMAND_MEMORY |
		  PCI_COMMAND_MASTER | PCI_COMMAND_INVALIDATE;

	v3_writew(V3_PCI_CMD, pci_cmd);

	v3_writeb(V3_LB_ISTAT, ~0x40);
	v3_writeb(V3_LB_IMASK, 0x68);

#if 0
	ret = request_irq(IRQ_AP_LBUSTIMEOUT, lb_timeout, 0, "bus timeout", NULL);
	if (ret)
		printk(KERN_ERR "PCI: unable to grab local bus timeout "
		       "interrupt: %d\n", ret);
#endif

	register_isa_ports(PHYS_PCI_MEM_BASE, PHYS_PCI_IO_BASE, 0);
}
