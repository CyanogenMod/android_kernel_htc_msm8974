/*
 * EDAC defs for Marvell MV64x60 bridge chip
 *
 * Author: Dave Jiang <djiang@mvista.com>
 *
 * 2007 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 */
#ifndef _MV64X60_EDAC_H_
#define _MV64X60_EDAC_H_

#define MV64x60_REVISION " Ver: 2.0.0"
#define EDAC_MOD_STR	"MV64x60_edac"

#define mv64x60_printk(level, fmt, arg...) \
	edac_printk(level, "MV64x60", fmt, ##arg)

#define mv64x60_mc_printk(mci, level, fmt, arg...) \
	edac_mc_chipset_printk(mci, level, "MV64x60", fmt, ##arg)

#define MV64x60_CPU_ERR_ADDR_LO		0x00	
#define MV64x60_CPU_ERR_ADDR_HI		0x08	
#define MV64x60_CPU_ERR_DATA_LO		0x00	
#define MV64x60_CPU_ERR_DATA_HI		0x08	
#define MV64x60_CPU_ERR_PARITY		0x10	
#define MV64x60_CPU_ERR_CAUSE		0x18	
#define MV64x60_CPU_ERR_MASK		0x20	

#define MV64x60_CPU_CAUSE_MASK		0x07ffffff

#define MV64X60_SRAM_ERR_CAUSE		0x08	
#define MV64X60_SRAM_ERR_ADDR_LO	0x10	
#define MV64X60_SRAM_ERR_ADDR_HI	0x78	
#define MV64X60_SRAM_ERR_DATA_LO	0x18	
#define MV64X60_SRAM_ERR_DATA_HI	0x20	
#define MV64X60_SRAM_ERR_PARITY		0x28	

#define MV64X60_SDRAM_CONFIG		0x00	
#define MV64X60_SDRAM_ERR_DATA_HI	0x40	
#define MV64X60_SDRAM_ERR_DATA_LO	0x44	
#define MV64X60_SDRAM_ERR_ECC_RCVD	0x48	
#define MV64X60_SDRAM_ERR_ECC_CALC	0x4c	
#define MV64X60_SDRAM_ERR_ADDR		0x50	
#define MV64X60_SDRAM_ERR_ECC_CNTL	0x54	
#define MV64X60_SDRAM_ERR_ECC_ERR_CNT	0x58	

#define MV64X60_SDRAM_REGISTERED	0x20000
#define MV64X60_SDRAM_ECC		0x40000

#ifdef CONFIG_PCI
#define MV64X60_PCIx_ERR_MASK_VAL	0x00a50c24

#define MV64X60_PCI_ERROR_ADDR_LO	0x00
#define MV64X60_PCI_ERROR_ADDR_HI	0x04
#define MV64X60_PCI_ERROR_ATTR		0x08
#define MV64X60_PCI_ERROR_CMD		0x10
#define MV64X60_PCI_ERROR_CAUSE		0x18
#define MV64X60_PCI_ERROR_MASK		0x1c

#define MV64X60_PCI_ERR_SWrPerr		0x0002
#define MV64X60_PCI_ERR_SRdPerr		0x0004
#define	MV64X60_PCI_ERR_MWrPerr		0x0020
#define MV64X60_PCI_ERR_MRdPerr		0x0040

#define MV64X60_PCI_PE_MASK	(MV64X60_PCI_ERR_SWrPerr | \
				MV64X60_PCI_ERR_SRdPerr | \
				MV64X60_PCI_ERR_MWrPerr | \
				MV64X60_PCI_ERR_MRdPerr)

struct mv64x60_pci_pdata {
	int pci_hose;
	void __iomem *pci_vbase;
	char *name;
	int irq;
	int edac_idx;
};

#endif				

struct mv64x60_mc_pdata {
	void __iomem *mc_vbase;
	int total_mem;
	char *name;
	int irq;
	int edac_idx;
};

struct mv64x60_cpu_pdata {
	void __iomem *cpu_vbase[2];
	char *name;
	int irq;
	int edac_idx;
};

struct mv64x60_sram_pdata {
	void __iomem *sram_vbase;
	char *name;
	int irq;
	int edac_idx;
};

#endif
