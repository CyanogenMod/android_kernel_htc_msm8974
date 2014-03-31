/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2005-2009 Cavium Networks
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/swiotlb.h>

#include <asm/time.h>

#include <asm/octeon/octeon.h>
#include <asm/octeon/cvmx-npi-defs.h>
#include <asm/octeon/cvmx-pci-defs.h>
#include <asm/octeon/pci-octeon.h>

#include <dma-coherence.h>

#define USE_OCTEON_INTERNAL_ARBITER

#define OCTEON_PCI_IOSPACE_BASE     0x80011a0400000000ull
#define OCTEON_PCI_IOSPACE_SIZE     (1ull<<32)

#define OCTEON_PCI_MEMSPACE_OFFSET  (0x00011b0000000000ull)

u64 octeon_bar1_pci_phys;

union octeon_pci_address {
	uint64_t u64;
	struct {
		uint64_t upper:2;
		uint64_t reserved:13;
		uint64_t io:1;
		uint64_t did:5;
		uint64_t subdid:3;
		uint64_t reserved2:4;
		uint64_t endian_swap:2;
		uint64_t reserved3:10;
		uint64_t bus:8;
		uint64_t dev:5;
		uint64_t func:3;
		uint64_t reg:8;
	} s;
};

int __initdata (*octeon_pcibios_map_irq)(const struct pci_dev *dev,
					 u8 slot, u8 pin);
enum octeon_dma_bar_type octeon_dma_bar_type = OCTEON_DMA_BAR_TYPE_INVALID;

int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	if (octeon_pcibios_map_irq)
		return octeon_pcibios_map_irq(dev, slot, pin);
	else
		panic("octeon_pcibios_map_irq not set.");
}


int pcibios_plat_dev_init(struct pci_dev *dev)
{
	uint16_t config;
	uint32_t dconfig;
	int pos;
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, 64 / 4);
	
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, 64);

	
	
	pci_read_config_word(dev, PCI_COMMAND, &config);
	config |= PCI_COMMAND_PARITY | PCI_COMMAND_SERR;
	pci_write_config_word(dev, PCI_COMMAND, config);

	if (dev->subordinate) {
		
		pci_write_config_byte(dev, PCI_SEC_LATENCY_TIMER, 64);
		
		pci_read_config_word(dev, PCI_BRIDGE_CONTROL, &config);
		config |= PCI_BRIDGE_CTL_PARITY | PCI_BRIDGE_CTL_SERR;
		pci_write_config_word(dev, PCI_BRIDGE_CONTROL, config);
	}

	
	pos = pci_find_capability(dev, PCI_CAP_ID_EXP);
	if (pos) {
		
		pci_read_config_word(dev, pos + PCI_EXP_DEVCTL, &config);
		config |= PCI_EXP_DEVCTL_CERE; 
		config |= PCI_EXP_DEVCTL_NFERE; 
		config |= PCI_EXP_DEVCTL_FERE;  
		config |= PCI_EXP_DEVCTL_URRE;  
		pci_write_config_word(dev, pos + PCI_EXP_DEVCTL, config);
	}

	
	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ERR);
	if (pos) {
		
		pci_read_config_dword(dev, pos + PCI_ERR_UNCOR_STATUS,
				      &dconfig);
		pci_write_config_dword(dev, pos + PCI_ERR_UNCOR_STATUS,
				       dconfig);
		
		
		pci_write_config_dword(dev, pos + PCI_ERR_UNCOR_MASK, 0);
		
		
		pci_read_config_dword(dev, pos + PCI_ERR_COR_STATUS, &dconfig);
		pci_write_config_dword(dev, pos + PCI_ERR_COR_STATUS, dconfig);
		
		
		pci_write_config_dword(dev, pos + PCI_ERR_COR_MASK, 0);
		
		pci_read_config_dword(dev, pos + PCI_ERR_CAP, &dconfig);
		
		if (config & PCI_ERR_CAP_ECRC_GENC)
			config |= PCI_ERR_CAP_ECRC_GENE;
		
		if (config & PCI_ERR_CAP_ECRC_CHKC)
			config |= PCI_ERR_CAP_ECRC_CHKE;
		pci_write_config_dword(dev, pos + PCI_ERR_CAP, dconfig);
		
		
		pci_write_config_dword(dev, pos + PCI_ERR_ROOT_COMMAND,
				       PCI_ERR_ROOT_CMD_COR_EN |
				       PCI_ERR_ROOT_CMD_NONFATAL_EN |
				       PCI_ERR_ROOT_CMD_FATAL_EN);
		
		pci_read_config_dword(dev, pos + PCI_ERR_ROOT_STATUS, &dconfig);
		pci_write_config_dword(dev, pos + PCI_ERR_ROOT_STATUS, dconfig);
	}

	dev->dev.archdata.dma_ops = octeon_pci_dma_map_ops;

	return 0;
}

const char *octeon_get_pci_interrupts(void)
{
	switch (octeon_bootinfo->board_type) {
	case CVMX_BOARD_TYPE_NAO38:
		
		return "AAAAADABAAAAAAAAAAAAAAAAAAAAAAAA";
	case CVMX_BOARD_TYPE_EBH3100:
	case CVMX_BOARD_TYPE_CN3010_EVB_HS5:
	case CVMX_BOARD_TYPE_CN3005_EVB_HS5:
		return "AAABAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	case CVMX_BOARD_TYPE_BBGW_REF:
		return "AABCD";
	case CVMX_BOARD_TYPE_THUNDER:
	case CVMX_BOARD_TYPE_EBH3000:
	default:
		return "";
	}
}

int __init octeon_pci_pcibios_map_irq(const struct pci_dev *dev,
				      u8 slot, u8 pin)
{
	int irq_num;
	const char *interrupts;
	int dev_num;

	
	interrupts = octeon_get_pci_interrupts();

	dev_num = dev->devfn >> 3;
	if (dev_num < strlen(interrupts))
		irq_num = ((interrupts[dev_num] - 'A' + pin - 1) & 3) +
			OCTEON_IRQ_PCI_INT0;
	else
		irq_num = ((slot + pin - 3) & 3) + OCTEON_IRQ_PCI_INT0;
	return irq_num;
}


static int octeon_read_config(struct pci_bus *bus, unsigned int devfn,
			      int reg, int size, u32 *val)
{
	union octeon_pci_address pci_addr;

	pci_addr.u64 = 0;
	pci_addr.s.upper = 2;
	pci_addr.s.io = 1;
	pci_addr.s.did = 3;
	pci_addr.s.subdid = 1;
	pci_addr.s.endian_swap = 1;
	pci_addr.s.bus = bus->number;
	pci_addr.s.dev = devfn >> 3;
	pci_addr.s.func = devfn & 0x7;
	pci_addr.s.reg = reg;

#if PCI_CONFIG_SPACE_DELAY
	udelay(PCI_CONFIG_SPACE_DELAY);
#endif
	switch (size) {
	case 4:
		*val = le32_to_cpu(cvmx_read64_uint32(pci_addr.u64));
		return PCIBIOS_SUCCESSFUL;
	case 2:
		*val = le16_to_cpu(cvmx_read64_uint16(pci_addr.u64));
		return PCIBIOS_SUCCESSFUL;
	case 1:
		*val = cvmx_read64_uint8(pci_addr.u64);
		return PCIBIOS_SUCCESSFUL;
	}
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}


static int octeon_write_config(struct pci_bus *bus, unsigned int devfn,
			       int reg, int size, u32 val)
{
	union octeon_pci_address pci_addr;

	pci_addr.u64 = 0;
	pci_addr.s.upper = 2;
	pci_addr.s.io = 1;
	pci_addr.s.did = 3;
	pci_addr.s.subdid = 1;
	pci_addr.s.endian_swap = 1;
	pci_addr.s.bus = bus->number;
	pci_addr.s.dev = devfn >> 3;
	pci_addr.s.func = devfn & 0x7;
	pci_addr.s.reg = reg;

#if PCI_CONFIG_SPACE_DELAY
	udelay(PCI_CONFIG_SPACE_DELAY);
#endif
	switch (size) {
	case 4:
		cvmx_write64_uint32(pci_addr.u64, cpu_to_le32(val));
		return PCIBIOS_SUCCESSFUL;
	case 2:
		cvmx_write64_uint16(pci_addr.u64, cpu_to_le16(val));
		return PCIBIOS_SUCCESSFUL;
	case 1:
		cvmx_write64_uint8(pci_addr.u64, val);
		return PCIBIOS_SUCCESSFUL;
	}
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}


static struct pci_ops octeon_pci_ops = {
	octeon_read_config,
	octeon_write_config,
};

static struct resource octeon_pci_mem_resource = {
	.start = 0,
	.end = 0,
	.name = "Octeon PCI MEM",
	.flags = IORESOURCE_MEM,
};

static struct resource octeon_pci_io_resource = {
	.start = 0x4000,
	.end = OCTEON_PCI_IOSPACE_SIZE - 1,
	.name = "Octeon PCI IO",
	.flags = IORESOURCE_IO,
};

static struct pci_controller octeon_pci_controller = {
	.pci_ops = &octeon_pci_ops,
	.mem_resource = &octeon_pci_mem_resource,
	.mem_offset = OCTEON_PCI_MEMSPACE_OFFSET,
	.io_resource = &octeon_pci_io_resource,
	.io_offset = 0,
	.io_map_base = OCTEON_PCI_IOSPACE_BASE,
};


static void octeon_pci_initialize(void)
{
	union cvmx_pci_cfg01 cfg01;
	union cvmx_npi_ctl_status ctl_status;
	union cvmx_pci_ctl_status_2 ctl_status_2;
	union cvmx_pci_cfg19 cfg19;
	union cvmx_pci_cfg16 cfg16;
	union cvmx_pci_cfg22 cfg22;
	union cvmx_pci_cfg56 cfg56;

	
	cvmx_write_csr(CVMX_CIU_SOFT_PRST, 0x1);
	cvmx_read_csr(CVMX_CIU_SOFT_PRST);

	udelay(2000);		

	ctl_status.u64 = 0;	
	ctl_status.s.max_word = 1;
	ctl_status.s.timer = 1;
	cvmx_write_csr(CVMX_NPI_CTL_STATUS, ctl_status.u64);

	cvmx_write_csr(CVMX_CIU_SOFT_PRST, 0x4);
	cvmx_read_csr(CVMX_CIU_SOFT_PRST);

	udelay(2000);		

	ctl_status_2.u32 = 0;
	ctl_status_2.s.tsr_hwm = 1;	
	ctl_status_2.s.bar2pres = 1;	
	ctl_status_2.s.bar2_enb = 1;
	ctl_status_2.s.bar2_cax = 1;	
	ctl_status_2.s.bar2_esx = 1;
	ctl_status_2.s.pmo_amod = 1;	
	if (octeon_dma_bar_type == OCTEON_DMA_BAR_TYPE_BIG) {
		
		ctl_status_2.s.bb1_hole = OCTEON_PCI_BAR1_HOLE_BITS;
		ctl_status_2.s.bb1_siz = 1;  
		ctl_status_2.s.bb_ca = 1;    
		ctl_status_2.s.bb_es = 1;    
		ctl_status_2.s.bb1 = 1;      
		ctl_status_2.s.bb0 = 1;      
	}

	octeon_npi_write32(CVMX_NPI_PCI_CTL_STATUS_2, ctl_status_2.u32);
	udelay(2000);		

	ctl_status_2.u32 = octeon_npi_read32(CVMX_NPI_PCI_CTL_STATUS_2);
	pr_notice("PCI Status: %s %s-bit\n",
		  ctl_status_2.s.ap_pcix ? "PCI-X" : "PCI",
		  ctl_status_2.s.ap_64ad ? "64" : "32");

	if (OCTEON_IS_MODEL(OCTEON_CN58XX) || OCTEON_IS_MODEL(OCTEON_CN50XX)) {
		union cvmx_pci_cnt_reg cnt_reg_start;
		union cvmx_pci_cnt_reg cnt_reg_end;
		unsigned long cycles, pci_clock;

		cnt_reg_start.u64 = cvmx_read_csr(CVMX_NPI_PCI_CNT_REG);
		cycles = read_c0_cvmcount();
		udelay(1000);
		cnt_reg_end.u64 = cvmx_read_csr(CVMX_NPI_PCI_CNT_REG);
		cycles = read_c0_cvmcount() - cycles;
		pci_clock = (cnt_reg_end.s.pcicnt - cnt_reg_start.s.pcicnt) /
			    (cycles / (mips_hpt_frequency / 1000000));
		pr_notice("PCI Clock: %lu MHz\n", pci_clock);
	}

	if (ctl_status_2.s.ap_pcix) {
		cfg19.u32 = 0;
		cfg19.s.tdomc = 4;
		cfg19.s.mdrrmc = 2;
		cfg19.s.mrbcm = 1;
		octeon_npi_write32(CVMX_NPI_PCI_CFG19, cfg19.u32);
	}


	cfg01.u32 = 0;
	cfg01.s.msae = 1;	
	cfg01.s.me = 1;		
	cfg01.s.pee = 1;	
	cfg01.s.see = 1;	
	cfg01.s.fbbe = 1;	

	octeon_npi_write32(CVMX_NPI_PCI_CFG01, cfg01.u32);

#ifdef USE_OCTEON_INTERNAL_ARBITER
	{
		union cvmx_npi_pci_int_arb_cfg pci_int_arb_cfg;

		pci_int_arb_cfg.u64 = 0;
		pci_int_arb_cfg.s.en = 1;	
		cvmx_write_csr(CVMX_NPI_PCI_INT_ARB_CFG, pci_int_arb_cfg.u64);
	}
#endif	

	/*
	 * Preferably written to 1 to set MLTD. [RDSATI,TRTAE,
	 * TWTAE,TMAE,DPPMR -> must be zero. TILT -> must not be set to
	 * 1..7.
	 */
	cfg16.u32 = 0;
	cfg16.s.mltd = 1;	
	octeon_npi_write32(CVMX_NPI_PCI_CFG16, cfg16.u32);

	/*
	 * Should be written to 0x4ff00. MTTV -> must be zero.
	 * FLUSH -> must be 1. MRV -> should be 0xFF.
	 */
	cfg22.u32 = 0;
	
	cfg22.s.mrv = 0xff;
	cfg22.s.flush = 1;
	octeon_npi_write32(CVMX_NPI_PCI_CFG22, cfg22.u32);

	/*
	 * MOST Indicates the maximum number of outstanding splits (in -1
	 * notation) when OCTEON is in PCI-X mode.  PCI-X performance is
	 * affected by the MOST selection.  Should generally be written
	 * with one of 0x3be807, 0x2be807, 0x1be807, or 0x0be807,
	 * depending on the desired MOST of 3, 2, 1, or 0, respectively.
	 */
	cfg56.u32 = 0;
	cfg56.s.pxcid = 7;	
	cfg56.s.ncp = 0xe8;	
	cfg56.s.dpere = 1;	
	cfg56.s.roe = 1;	
	cfg56.s.mmbc = 1;	
	cfg56.s.most = 3;	

	octeon_npi_write32(CVMX_NPI_PCI_CFG56, cfg56.u32);

	octeon_npi_write32(CVMX_NPI_PCI_READ_CMD_6, 0x21);
	octeon_npi_write32(CVMX_NPI_PCI_READ_CMD_C, 0x31);
	octeon_npi_write32(CVMX_NPI_PCI_READ_CMD_E, 0x31);
}


static int __init octeon_pci_setup(void)
{
	union cvmx_npi_mem_access_subidx mem_access;
	int index;

	
	if (octeon_has_feature(OCTEON_FEATURE_PCIE))
		return 0;

	
	octeon_pcibios_map_irq = octeon_pci_pcibios_map_irq;

	
	if (OCTEON_IS_MODEL(OCTEON_CN31XX) ||
	    OCTEON_IS_MODEL(OCTEON_CN38XX_PASS2) ||
	    OCTEON_IS_MODEL(OCTEON_CN38XX_PASS1))
		octeon_dma_bar_type = OCTEON_DMA_BAR_TYPE_SMALL;
	else
		octeon_dma_bar_type = OCTEON_DMA_BAR_TYPE_BIG;

	
	set_io_port_base(OCTEON_PCI_IOSPACE_BASE);
	ioport_resource.start = 0;
	ioport_resource.end = OCTEON_PCI_IOSPACE_SIZE - 1;
	if (!octeon_is_pci_host()) {
		pr_notice("Not in host mode, PCI Controller not initialized\n");
		return 0;
	}

	pr_notice("%s Octeon big bar support\n",
		  (octeon_dma_bar_type ==
		  OCTEON_DMA_BAR_TYPE_BIG) ? "Enabling" : "Disabling");

	octeon_pci_initialize();

	mem_access.u64 = 0;
	mem_access.s.esr = 1;	
	mem_access.s.esw = 1;	
	mem_access.s.nsr = 0;	
	mem_access.s.nsw = 0;	
	mem_access.s.ror = 0;	
	mem_access.s.row = 0;	
	mem_access.s.ba = 0;	
	cvmx_write_csr(CVMX_NPI_MEM_ACCESS_SUBID3, mem_access.u64);

	octeon_npi_write32(CVMX_NPI_PCI_CFG08,
			   (u32)(OCTEON_BAR2_PCI_ADDRESS & 0xffffffffull));
	octeon_npi_write32(CVMX_NPI_PCI_CFG09,
			   (u32)(OCTEON_BAR2_PCI_ADDRESS >> 32));

	if (octeon_dma_bar_type == OCTEON_DMA_BAR_TYPE_BIG) {
		
		octeon_npi_write32(CVMX_NPI_PCI_CFG04, 0);
		octeon_npi_write32(CVMX_NPI_PCI_CFG05, 0);

		octeon_npi_write32(CVMX_NPI_PCI_CFG06, 2ul << 30);
		octeon_npi_write32(CVMX_NPI_PCI_CFG07, 0);

		
		octeon_bar1_pci_phys = 0x80000000ull;
		for (index = 0; index < 32; index++) {
			union cvmx_pci_bar1_indexx bar1_index;

			bar1_index.u32 = 0;
			
			bar1_index.s.addr_idx =
				(octeon_bar1_pci_phys >> 22) + index;
			
			bar1_index.s.ca = 1;
			
			bar1_index.s.end_swp = 1;
			
			bar1_index.s.addr_v = 1;
			octeon_npi_write32(CVMX_NPI_PCI_BAR1_INDEXX(index),
					   bar1_index.u32);
		}

		
		octeon_pci_mem_resource.start =
			OCTEON_PCI_MEMSPACE_OFFSET + (4ul << 30) -
			(OCTEON_PCI_BAR1_HOLE_SIZE << 20);
		octeon_pci_mem_resource.end =
			octeon_pci_mem_resource.start + (1ul << 30);
	} else {
		
		octeon_npi_write32(CVMX_NPI_PCI_CFG04, 128ul << 20);
		octeon_npi_write32(CVMX_NPI_PCI_CFG05, 0);

		
		octeon_npi_write32(CVMX_NPI_PCI_CFG06, 0);
		octeon_npi_write32(CVMX_NPI_PCI_CFG07, 0);

		
		octeon_bar1_pci_phys =
			virt_to_phys(octeon_swiotlb) & ~((1ull << 22) - 1);

		for (index = 0; index < 32; index++) {
			union cvmx_pci_bar1_indexx bar1_index;

			bar1_index.u32 = 0;
			
			bar1_index.s.addr_idx =
				(octeon_bar1_pci_phys >> 22) + index;
			
			bar1_index.s.ca = 1;
			
			bar1_index.s.end_swp = 1;
			
			bar1_index.s.addr_v = 1;
			octeon_npi_write32(CVMX_NPI_PCI_BAR1_INDEXX(index),
					   bar1_index.u32);
		}

		
		octeon_pci_mem_resource.start =
			OCTEON_PCI_MEMSPACE_OFFSET + (128ul << 20) +
			(4ul << 10);
		octeon_pci_mem_resource.end =
			octeon_pci_mem_resource.start + (1ul << 30);
	}

	register_pci_controller(&octeon_pci_controller);

	cvmx_write_csr(CVMX_NPI_PCI_INT_SUM2, -1);

	octeon_pci_dma_init();

	return 0;
}

arch_initcall(octeon_pci_setup);
