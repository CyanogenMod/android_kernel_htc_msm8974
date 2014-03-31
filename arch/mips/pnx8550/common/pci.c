/*
 *
 * BRIEF MODULE DESCRIPTION
 *
 * Author: source@mvista.com
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <pci.h>
#include <glb.h>
#include <nand.h>

static struct resource pci_io_resource = {
	.start	= PNX8550_PCIIO + 0x1000,	
	.end	= PNX8550_PCIIO + PNX8550_PCIIO_SIZE,
	.name	= "pci IO space",
	.flags	= IORESOURCE_IO
};

static struct resource pci_mem_resource = {
	.start	= PNX8550_PCIMEM,
	.end	= PNX8550_PCIMEM + PNX8550_PCIMEM_SIZE - 1,
	.name	= "pci memory space",
	.flags	= IORESOURCE_MEM
};

extern struct pci_ops pnx8550_pci_ops;

static struct pci_controller pnx8550_controller = {
	.pci_ops	= &pnx8550_pci_ops,
	.io_map_base	= PNX8550_PORT_BASE,
	.io_resource	= &pci_io_resource,
	.mem_resource	= &pci_mem_resource,
};

static inline unsigned long get_system_mem_size(void)
{
	
	unsigned long dram_r0_lo = inl(PCI_BASE | 0x65010);
	
	unsigned long dram_r1_hi = inl(PCI_BASE | 0x65018);

	return dram_r1_hi - dram_r0_lo + 1;
}

static int __init pnx8550_pci_setup(void)
{
	int pci_mem_code;
	int mem_size = get_system_mem_size() >> 20;

	PNX8550_GLB2_ENAB_INTA_O = 0;

	
	if (mem_size >= 128)
		pci_mem_code = SIZE_128M;
	else if (mem_size >= 64)
		pci_mem_code = SIZE_64M;
	else if (mem_size >= 32)
		pci_mem_code = SIZE_32M;
	else
		pci_mem_code = SIZE_16M;

	
	outl(pci_mem_resource.start, PCI_BASE | PCI_BASE1_LO);
	outl(pci_mem_resource.end + 1, PCI_BASE | PCI_BASE1_HI);
	outl(pci_io_resource.start, PCI_BASE | PCI_BASE2_LO);
	outl(pci_io_resource.end, PCI_BASE | PCI_BASE2_HI);

	
	outl(0x00000001, PCI_BASE | PCI_IO);

	
	outl(0xca, PCI_BASE | PCI_UNLOCKREG);

	outl(0x00000000, PCI_BASE | PCI_BASE10);

	outl(0x1be00000, PCI_BASE | PCI_BASE14);  
	outl(PNX8550_NAND_BASE_ADDR, PCI_BASE | PCI_BASE18);  

	outl(PCI_EN_TA |
	     PCI_EN_PCI2MMI |
	     PCI_EN_XIO |
	     PCI_SETUP_BASE18_SIZE(SIZE_32M) |
	     PCI_SETUP_BASE18_EN |
	     PCI_SETUP_BASE14_EN |
	     PCI_SETUP_BASE10_PREF |
	     PCI_SETUP_BASE10_SIZE(pci_mem_code) |
	     PCI_SETUP_CFGMANAGE_EN |
	     PCI_SETUP_PCIARB_EN,
	     PCI_BASE |
	     PCI_SETUP);	
	outl(0x00000000, PCI_BASE | PCI_CTRL);	

	register_pci_controller(&pnx8550_controller);

	return 0;
}

arch_initcall(pnx8550_pci_setup);
