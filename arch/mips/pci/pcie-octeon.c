/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007, 2008, 2009, 2010, 2011 Cavium Networks
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <asm/octeon/octeon.h>
#include <asm/octeon/cvmx-npei-defs.h>
#include <asm/octeon/cvmx-pciercx-defs.h>
#include <asm/octeon/cvmx-pescx-defs.h>
#include <asm/octeon/cvmx-pexp-defs.h>
#include <asm/octeon/cvmx-pemx-defs.h>
#include <asm/octeon/cvmx-dpi-defs.h>
#include <asm/octeon/cvmx-sli-defs.h>
#include <asm/octeon/cvmx-sriox-defs.h>
#include <asm/octeon/cvmx-helper-errata.h>
#include <asm/octeon/pci-octeon.h>

#define MRRS_CN5XXX 0 
#define MPS_CN5XXX  0 
#define MRRS_CN6XXX 3 
#define MPS_CN6XXX  0 

static int pcie_disable;
module_param(pcie_disable, int, S_IRUGO);

static int enable_pcie_14459_war;
static int enable_pcie_bus_num_war[2];

union cvmx_pcie_address {
	uint64_t u64;
	struct {
		uint64_t upper:2;	
		uint64_t reserved_49_61:13;	
		uint64_t io:1;	
		uint64_t did:5;	
		uint64_t subdid:3;	
		uint64_t reserved_36_39:4;	
		uint64_t es:2;	
		uint64_t port:2;	
		uint64_t reserved_29_31:3;	
		uint64_t ty:1;
		
		uint64_t bus:8;
		uint64_t dev:5;
		
		uint64_t func:3;
		uint64_t reg:12;
	} config;
	struct {
		uint64_t upper:2;	
		uint64_t reserved_49_61:13;	
		uint64_t io:1;	
		uint64_t did:5;	
		uint64_t subdid:3;	
		uint64_t reserved_36_39:4;	
		uint64_t es:2;	
		uint64_t port:2;	
		uint64_t address:32;	
	} io;
	struct {
		uint64_t upper:2;	
		uint64_t reserved_49_61:13;	
		uint64_t io:1;	
		uint64_t did:5;	
		uint64_t subdid:3;	
		uint64_t reserved_36_39:4;	
		uint64_t address:36;	
	} mem;
};

static int cvmx_pcie_rc_initialize(int pcie_port);

#include <dma-coherence.h>

/**
 * Return the Core virtual base address for PCIe IO access. IOs are
 * read/written as an offset from this address.
 *
 * @pcie_port: PCIe port the IO is for
 *
 * Returns 64bit Octeon IO base address for read/write
 */
static inline uint64_t cvmx_pcie_get_io_base_address(int pcie_port)
{
	union cvmx_pcie_address pcie_addr;
	pcie_addr.u64 = 0;
	pcie_addr.io.upper = 0;
	pcie_addr.io.io = 1;
	pcie_addr.io.did = 3;
	pcie_addr.io.subdid = 2;
	pcie_addr.io.es = 1;
	pcie_addr.io.port = pcie_port;
	return pcie_addr.u64;
}

static inline uint64_t cvmx_pcie_get_io_size(int pcie_port)
{
	return 1ull << 32;
}

/**
 * Return the Core virtual base address for PCIe MEM access. Memory is
 * read/written as an offset from this address.
 *
 * @pcie_port: PCIe port the IO is for
 *
 * Returns 64bit Octeon IO base address for read/write
 */
static inline uint64_t cvmx_pcie_get_mem_base_address(int pcie_port)
{
	union cvmx_pcie_address pcie_addr;
	pcie_addr.u64 = 0;
	pcie_addr.mem.upper = 0;
	pcie_addr.mem.io = 1;
	pcie_addr.mem.did = 3;
	pcie_addr.mem.subdid = 3 + pcie_port;
	return pcie_addr.u64;
}

static inline uint64_t cvmx_pcie_get_mem_size(int pcie_port)
{
	return 1ull << 36;
}

static uint32_t cvmx_pcie_cfgx_read(int pcie_port, uint32_t cfg_offset)
{
	if (octeon_has_feature(OCTEON_FEATURE_NPEI)) {
		union cvmx_pescx_cfg_rd pescx_cfg_rd;
		pescx_cfg_rd.u64 = 0;
		pescx_cfg_rd.s.addr = cfg_offset;
		cvmx_write_csr(CVMX_PESCX_CFG_RD(pcie_port), pescx_cfg_rd.u64);
		pescx_cfg_rd.u64 = cvmx_read_csr(CVMX_PESCX_CFG_RD(pcie_port));
		return pescx_cfg_rd.s.data;
	} else {
		union cvmx_pemx_cfg_rd pemx_cfg_rd;
		pemx_cfg_rd.u64 = 0;
		pemx_cfg_rd.s.addr = cfg_offset;
		cvmx_write_csr(CVMX_PEMX_CFG_RD(pcie_port), pemx_cfg_rd.u64);
		pemx_cfg_rd.u64 = cvmx_read_csr(CVMX_PEMX_CFG_RD(pcie_port));
		return pemx_cfg_rd.s.data;
	}
}

static void cvmx_pcie_cfgx_write(int pcie_port, uint32_t cfg_offset,
				 uint32_t val)
{
	if (octeon_has_feature(OCTEON_FEATURE_NPEI)) {
		union cvmx_pescx_cfg_wr pescx_cfg_wr;
		pescx_cfg_wr.u64 = 0;
		pescx_cfg_wr.s.addr = cfg_offset;
		pescx_cfg_wr.s.data = val;
		cvmx_write_csr(CVMX_PESCX_CFG_WR(pcie_port), pescx_cfg_wr.u64);
	} else {
		union cvmx_pemx_cfg_wr pemx_cfg_wr;
		pemx_cfg_wr.u64 = 0;
		pemx_cfg_wr.s.addr = cfg_offset;
		pemx_cfg_wr.s.data = val;
		cvmx_write_csr(CVMX_PEMX_CFG_WR(pcie_port), pemx_cfg_wr.u64);
	}
}

static inline uint64_t __cvmx_pcie_build_config_addr(int pcie_port, int bus,
						     int dev, int fn, int reg)
{
	union cvmx_pcie_address pcie_addr;
	union cvmx_pciercx_cfg006 pciercx_cfg006;

	pciercx_cfg006.u32 =
	    cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG006(pcie_port));
	if ((bus <= pciercx_cfg006.s.pbnum) && (dev != 0))
		return 0;

	pcie_addr.u64 = 0;
	pcie_addr.config.upper = 2;
	pcie_addr.config.io = 1;
	pcie_addr.config.did = 3;
	pcie_addr.config.subdid = 1;
	pcie_addr.config.es = 1;
	pcie_addr.config.port = pcie_port;
	pcie_addr.config.ty = (bus > pciercx_cfg006.s.pbnum);
	pcie_addr.config.bus = bus;
	pcie_addr.config.dev = dev;
	pcie_addr.config.func = fn;
	pcie_addr.config.reg = reg;
	return pcie_addr.u64;
}

static uint8_t cvmx_pcie_config_read8(int pcie_port, int bus, int dev,
				      int fn, int reg)
{
	uint64_t address =
	    __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
	if (address)
		return cvmx_read64_uint8(address);
	else
		return 0xff;
}

static uint16_t cvmx_pcie_config_read16(int pcie_port, int bus, int dev,
					int fn, int reg)
{
	uint64_t address =
	    __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
	if (address)
		return le16_to_cpu(cvmx_read64_uint16(address));
	else
		return 0xffff;
}

static uint32_t cvmx_pcie_config_read32(int pcie_port, int bus, int dev,
					int fn, int reg)
{
	uint64_t address =
	    __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
	if (address)
		return le32_to_cpu(cvmx_read64_uint32(address));
	else
		return 0xffffffff;
}

static void cvmx_pcie_config_write8(int pcie_port, int bus, int dev, int fn,
				    int reg, uint8_t val)
{
	uint64_t address =
	    __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
	if (address)
		cvmx_write64_uint8(address, val);
}

static void cvmx_pcie_config_write16(int pcie_port, int bus, int dev, int fn,
				     int reg, uint16_t val)
{
	uint64_t address =
	    __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
	if (address)
		cvmx_write64_uint16(address, cpu_to_le16(val));
}

static void cvmx_pcie_config_write32(int pcie_port, int bus, int dev, int fn,
				     int reg, uint32_t val)
{
	uint64_t address =
	    __cvmx_pcie_build_config_addr(pcie_port, bus, dev, fn, reg);
	if (address)
		cvmx_write64_uint32(address, cpu_to_le32(val));
}

static void __cvmx_pcie_rc_initialize_config_space(int pcie_port)
{
	union cvmx_pciercx_cfg030 pciercx_cfg030;
	union cvmx_pciercx_cfg070 pciercx_cfg070;
	union cvmx_pciercx_cfg001 pciercx_cfg001;
	union cvmx_pciercx_cfg032 pciercx_cfg032;
	union cvmx_pciercx_cfg006 pciercx_cfg006;
	union cvmx_pciercx_cfg008 pciercx_cfg008;
	union cvmx_pciercx_cfg009 pciercx_cfg009;
	union cvmx_pciercx_cfg010 pciercx_cfg010;
	union cvmx_pciercx_cfg011 pciercx_cfg011;
	union cvmx_pciercx_cfg035 pciercx_cfg035;
	union cvmx_pciercx_cfg075 pciercx_cfg075;
	union cvmx_pciercx_cfg034 pciercx_cfg034;

	
	
	
	

	pciercx_cfg030.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG030(pcie_port));
	if (OCTEON_IS_MODEL(OCTEON_CN5XXX)) {
		pciercx_cfg030.s.mps = MPS_CN5XXX;
		pciercx_cfg030.s.mrrs = MRRS_CN5XXX;
	} else {
		pciercx_cfg030.s.mps = MPS_CN6XXX;
		pciercx_cfg030.s.mrrs = MRRS_CN6XXX;
	}
	pciercx_cfg030.s.ro_en = 1;
	
	pciercx_cfg030.s.ns_en = 1;
	
	pciercx_cfg030.s.ce_en = 1;
	
	pciercx_cfg030.s.nfe_en = 1;
	
	pciercx_cfg030.s.fe_en = 1;
	
	pciercx_cfg030.s.ur_en = 1;
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG030(pcie_port), pciercx_cfg030.u32);


	if (octeon_has_feature(OCTEON_FEATURE_NPEI)) {
		union cvmx_npei_ctl_status2 npei_ctl_status2;
		npei_ctl_status2.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_CTL_STATUS2);
		
		npei_ctl_status2.s.mps = MPS_CN5XXX;
		
		npei_ctl_status2.s.mrrs = MRRS_CN5XXX;
		if (pcie_port)
			npei_ctl_status2.s.c1_b1_s = 3; 
		else
			npei_ctl_status2.s.c0_b1_s = 3; 

		cvmx_write_csr(CVMX_PEXP_NPEI_CTL_STATUS2, npei_ctl_status2.u64);
	} else {
		union cvmx_dpi_sli_prtx_cfg prt_cfg;
		union cvmx_sli_s2m_portx_ctl sli_s2m_portx_ctl;
		prt_cfg.u64 = cvmx_read_csr(CVMX_DPI_SLI_PRTX_CFG(pcie_port));
		prt_cfg.s.mps = MPS_CN6XXX;
		prt_cfg.s.mrrs = MRRS_CN6XXX;
		
		prt_cfg.s.molr = 32;
		cvmx_write_csr(CVMX_DPI_SLI_PRTX_CFG(pcie_port), prt_cfg.u64);

		sli_s2m_portx_ctl.u64 = cvmx_read_csr(CVMX_PEXP_SLI_S2M_PORTX_CTL(pcie_port));
		sli_s2m_portx_ctl.s.mrrs = MRRS_CN6XXX;
		cvmx_write_csr(CVMX_PEXP_SLI_S2M_PORTX_CTL(pcie_port), sli_s2m_portx_ctl.u64);
	}

	
	pciercx_cfg070.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG070(pcie_port));
	pciercx_cfg070.s.ge = 1;	
	pciercx_cfg070.s.ce = 1;	
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG070(pcie_port), pciercx_cfg070.u32);

	pciercx_cfg001.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG001(pcie_port));
	pciercx_cfg001.s.msae = 1;	
	pciercx_cfg001.s.me = 1;	
	pciercx_cfg001.s.i_dis = 1;	
	pciercx_cfg001.s.see = 1;	
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG001(pcie_port), pciercx_cfg001.u32);

	
	
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG066(pcie_port), 0);
	
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG069(pcie_port), 0);


	
	pciercx_cfg032.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG032(pcie_port));
	pciercx_cfg032.s.aslpc = 0; 
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG032(pcie_port), pciercx_cfg032.u32);

	pciercx_cfg006.u32 = 0;
	pciercx_cfg006.s.pbnum = 1;
	pciercx_cfg006.s.sbnum = 1;
	pciercx_cfg006.s.subbnum = 1;
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG006(pcie_port), pciercx_cfg006.u32);


	pciercx_cfg008.u32 = 0;
	pciercx_cfg008.s.mb_addr = 0x100;
	pciercx_cfg008.s.ml_addr = 0;
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG008(pcie_port), pciercx_cfg008.u32);


	pciercx_cfg009.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG009(pcie_port));
	pciercx_cfg010.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG010(pcie_port));
	pciercx_cfg011.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG011(pcie_port));
	pciercx_cfg009.s.lmem_base = 0x100;
	pciercx_cfg009.s.lmem_limit = 0;
	pciercx_cfg010.s.umem_base = 0x100;
	pciercx_cfg011.s.umem_limit = 0;
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG009(pcie_port), pciercx_cfg009.u32);
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG010(pcie_port), pciercx_cfg010.u32);
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG011(pcie_port), pciercx_cfg011.u32);

	pciercx_cfg035.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG035(pcie_port));
	pciercx_cfg035.s.secee = 1; 
	pciercx_cfg035.s.sefee = 1; 
	pciercx_cfg035.s.senfee = 1; 
	pciercx_cfg035.s.pmeie = 1; 
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG035(pcie_port), pciercx_cfg035.u32);

	pciercx_cfg075.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG075(pcie_port));
	pciercx_cfg075.s.cere = 1; 
	pciercx_cfg075.s.nfere = 1; 
	pciercx_cfg075.s.fere = 1; 
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG075(pcie_port), pciercx_cfg075.u32);

	pciercx_cfg034.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG034(pcie_port));
	pciercx_cfg034.s.hpint_en = 1; 
	pciercx_cfg034.s.dlls_en = 1; 
	pciercx_cfg034.s.ccint_en = 1; 
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG034(pcie_port), pciercx_cfg034.u32);
}

static int __cvmx_pcie_rc_initialize_link_gen1(int pcie_port)
{
	uint64_t start_cycle;
	union cvmx_pescx_ctl_status pescx_ctl_status;
	union cvmx_pciercx_cfg452 pciercx_cfg452;
	union cvmx_pciercx_cfg032 pciercx_cfg032;
	union cvmx_pciercx_cfg448 pciercx_cfg448;

	
	pciercx_cfg452.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG452(pcie_port));
	pescx_ctl_status.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS(pcie_port));
	if (pescx_ctl_status.s.qlm_cfg == 0)
		
		pciercx_cfg452.s.lme = 0xf;
	else
		
		pciercx_cfg452.s.lme = 0x7;
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG452(pcie_port), pciercx_cfg452.u32);

	if (OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X)) {
		union cvmx_pciercx_cfg455 pciercx_cfg455;
		pciercx_cfg455.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG455(pcie_port));
		pciercx_cfg455.s.m_cpl_len_err = 1;
		cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG455(pcie_port), pciercx_cfg455.u32);
	}

	
	if (OCTEON_IS_MODEL(OCTEON_CN52XX) && (pcie_port == 1)) {
		pescx_ctl_status.s.lane_swp = 1;
		cvmx_write_csr(CVMX_PESCX_CTL_STATUS(pcie_port), pescx_ctl_status.u64);
	}

	
	pescx_ctl_status.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS(pcie_port));
	pescx_ctl_status.s.lnk_enb = 1;
	cvmx_write_csr(CVMX_PESCX_CTL_STATUS(pcie_port), pescx_ctl_status.u64);

	if (OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_0))
		__cvmx_helper_errata_qlm_disable_2nd_order_cdr(0);

	
	start_cycle = cvmx_get_cycle();
	do {
		if (cvmx_get_cycle() - start_cycle > 2 * octeon_get_clock_rate()) {
			cvmx_dprintf("PCIe: Port %d link timeout\n", pcie_port);
			return -1;
		}
		cvmx_wait(10000);
		pciercx_cfg032.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG032(pcie_port));
	} while (pciercx_cfg032.s.dlla == 0);

	
	cvmx_write_csr(CVMX_PEXP_NPEI_INT_SUM, cvmx_read_csr(CVMX_PEXP_NPEI_INT_SUM));

	pciercx_cfg448.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG448(pcie_port));
	switch (pciercx_cfg032.s.nlw) {
	case 1:		
		pciercx_cfg448.s.rtl = 1677;
		break;
	case 2:		
		pciercx_cfg448.s.rtl = 867;
		break;
	case 4:		
		pciercx_cfg448.s.rtl = 462;
		break;
	case 8:		
		pciercx_cfg448.s.rtl = 258;
		break;
	}
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG448(pcie_port), pciercx_cfg448.u32);

	return 0;
}

static void __cvmx_increment_ba(union cvmx_sli_mem_access_subidx *pmas)
{
	if (OCTEON_IS_MODEL(OCTEON_CN68XX))
		pmas->cn68xx.ba++;
	else
		pmas->cn63xx.ba++;
}

static int __cvmx_pcie_rc_initialize_gen1(int pcie_port)
{
	int i;
	int base;
	u64 addr_swizzle;
	union cvmx_ciu_soft_prst ciu_soft_prst;
	union cvmx_pescx_bist_status pescx_bist_status;
	union cvmx_pescx_bist_status2 pescx_bist_status2;
	union cvmx_npei_ctl_status npei_ctl_status;
	union cvmx_npei_mem_access_ctl npei_mem_access_ctl;
	union cvmx_npei_mem_access_subidx mem_access_subid;
	union cvmx_npei_dbg_data npei_dbg_data;
	union cvmx_pescx_ctl_status2 pescx_ctl_status2;
	union cvmx_pciercx_cfg032 pciercx_cfg032;
	union cvmx_npei_bar1_indexx bar1_index;

retry:
	npei_ctl_status.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_CTL_STATUS);
	if ((pcie_port == 0) && !npei_ctl_status.s.host_mode) {
		cvmx_dprintf("PCIe: Port %d in endpoint mode\n", pcie_port);
		return -1;
	}

	if (OCTEON_IS_MODEL(OCTEON_CN52XX)) {
		npei_dbg_data.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
		if ((pcie_port == 1) && npei_dbg_data.cn52xx.qlm0_link_width) {
			cvmx_dprintf("PCIe: ERROR: cvmx_pcie_rc_initialize() called on port1, but port1 is disabled\n");
			return -1;
		}
	}

	npei_ctl_status.s.arb = 1;
	
	npei_ctl_status.s.cfg_rtry = 0x20;
	if (OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X)) {
		npei_ctl_status.s.p0_ntags = 0x20;
		npei_ctl_status.s.p1_ntags = 0x20;
	}
	cvmx_write_csr(CVMX_PEXP_NPEI_CTL_STATUS, npei_ctl_status.u64);

	
	if (cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_EBH5200) {
		if (pcie_port == 0) {
			ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
			if (ciu_soft_prst.s.soft_prst == 0) {
				
				ciu_soft_prst.s.soft_prst = 1;
				cvmx_write_csr(CVMX_CIU_SOFT_PRST, ciu_soft_prst.u64);
				ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
				ciu_soft_prst.s.soft_prst = 1;
				cvmx_write_csr(CVMX_CIU_SOFT_PRST1, ciu_soft_prst.u64);
				
				udelay(2000);
			}
			ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
			ciu_soft_prst.s.soft_prst = 0;
			cvmx_write_csr(CVMX_CIU_SOFT_PRST1, ciu_soft_prst.u64);
			ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
			ciu_soft_prst.s.soft_prst = 0;
			cvmx_write_csr(CVMX_CIU_SOFT_PRST, ciu_soft_prst.u64);
		}
	} else {
		if (pcie_port)
			ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
		else
			ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
		if (ciu_soft_prst.s.soft_prst == 0) {
			
			ciu_soft_prst.s.soft_prst = 1;
			if (pcie_port)
				cvmx_write_csr(CVMX_CIU_SOFT_PRST1, ciu_soft_prst.u64);
			else
				cvmx_write_csr(CVMX_CIU_SOFT_PRST, ciu_soft_prst.u64);
			
			udelay(2000);
		}
		if (pcie_port) {
			ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
			ciu_soft_prst.s.soft_prst = 0;
			cvmx_write_csr(CVMX_CIU_SOFT_PRST1, ciu_soft_prst.u64);
		} else {
			ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
			ciu_soft_prst.s.soft_prst = 0;
			cvmx_write_csr(CVMX_CIU_SOFT_PRST, ciu_soft_prst.u64);
		}
	}

	cvmx_wait(400000);

	if (!OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X) && !OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X)) {
		
		pescx_ctl_status2.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS2(pcie_port));
		pescx_ctl_status2.s.pclk_run = 1;
		cvmx_write_csr(CVMX_PESCX_CTL_STATUS2(pcie_port), pescx_ctl_status2.u64);
		if (CVMX_WAIT_FOR_FIELD64(CVMX_PESCX_CTL_STATUS2(pcie_port),
					  union cvmx_pescx_ctl_status2, pclk_run, ==, 1, 10000)) {
			cvmx_dprintf("PCIe: Port %d isn't clocked, skipping.\n", pcie_port);
			return -1;
		}
	}

	pescx_ctl_status2.u64 = cvmx_read_csr(CVMX_PESCX_CTL_STATUS2(pcie_port));
	if (pescx_ctl_status2.s.pcierst) {
		cvmx_dprintf("PCIe: Port %d stuck in reset, skipping.\n", pcie_port);
		return -1;
	}

	pescx_bist_status2.u64 = cvmx_read_csr(CVMX_PESCX_BIST_STATUS2(pcie_port));
	if (pescx_bist_status2.u64) {
		cvmx_dprintf("PCIe: Port %d BIST2 failed. Most likely this port isn't hooked up, skipping.\n",
			     pcie_port);
		return -1;
	}

	
	pescx_bist_status.u64 = cvmx_read_csr(CVMX_PESCX_BIST_STATUS(pcie_port));
	if (pescx_bist_status.u64)
		cvmx_dprintf("PCIe: BIST FAILED for port %d (0x%016llx)\n",
			     pcie_port, CAST64(pescx_bist_status.u64));

	
	__cvmx_pcie_rc_initialize_config_space(pcie_port);

	
	if (__cvmx_pcie_rc_initialize_link_gen1(pcie_port)) {
		cvmx_dprintf("PCIe: Failed to initialize port %d, probably the slot is empty\n",
			     pcie_port);
		return -1;
	}

	
	npei_mem_access_ctl.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_MEM_ACCESS_CTL);
	npei_mem_access_ctl.s.max_word = 0;     
	npei_mem_access_ctl.s.timer = 127;      
	cvmx_write_csr(CVMX_PEXP_NPEI_MEM_ACCESS_CTL, npei_mem_access_ctl.u64);

	
	mem_access_subid.u64 = 0;
	mem_access_subid.s.port = pcie_port; 
	mem_access_subid.s.nmerge = 1;  
	mem_access_subid.s.esr = 1;	
	mem_access_subid.s.esw = 1;	
	mem_access_subid.s.nsr = 0;	
	mem_access_subid.s.nsw = 0;	
	mem_access_subid.s.ror = 0;	
	mem_access_subid.s.row = 0;	
	mem_access_subid.s.ba = 0;	

	for (i = 12 + pcie_port * 4; i < 16 + pcie_port * 4; i++) {
		cvmx_write_csr(CVMX_PEXP_NPEI_MEM_ACCESS_SUBIDX(i), mem_access_subid.u64);
		mem_access_subid.s.ba += 1; 
	}

	for (i = 0; i < 4; i++) {
		cvmx_write_csr(CVMX_PESCX_P2P_BARX_START(i, pcie_port), -1);
		cvmx_write_csr(CVMX_PESCX_P2P_BARX_END(i, pcie_port), -1);
	}

	
	cvmx_write_csr(CVMX_PESCX_P2N_BAR0_START(pcie_port), 0);

	
	cvmx_write_csr(CVMX_PESCX_P2N_BAR1_START(pcie_port), CVMX_PCIE_BAR1_RC_BASE);

	bar1_index.u32 = 0;
	bar1_index.s.addr_idx = (CVMX_PCIE_BAR1_PHYS_BASE >> 22);
	bar1_index.s.ca = 1;       
	bar1_index.s.end_swp = 1;  
	bar1_index.s.addr_v = 1;   

	base = pcie_port ? 16 : 0;

	
#ifdef __MIPSEB__
	addr_swizzle = 4;
#else
	addr_swizzle = 0;
#endif
	for (i = 0; i < 16; i++) {
		cvmx_write64_uint32((CVMX_PEXP_NPEI_BAR1_INDEXX(base) ^ addr_swizzle),
				    bar1_index.u32);
		base++;
		
		bar1_index.s.addr_idx += (((1ull << 28) / 16ull) >> 22);
	}

	cvmx_write_csr(CVMX_PESCX_P2N_BAR2_START(pcie_port), 0);

	if (pcie_port) {
		union cvmx_npei_ctl_port1 npei_ctl_port;
		npei_ctl_port.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_CTL_PORT1);
		npei_ctl_port.s.bar2_enb = 1;
		npei_ctl_port.s.bar2_esx = 1;
		npei_ctl_port.s.bar2_cax = 0;
		npei_ctl_port.s.ptlp_ro = 1;
		npei_ctl_port.s.ctlp_ro = 1;
		npei_ctl_port.s.wait_com = 0;
		npei_ctl_port.s.waitl_com = 0;
		cvmx_write_csr(CVMX_PEXP_NPEI_CTL_PORT1, npei_ctl_port.u64);
	} else {
		union cvmx_npei_ctl_port0 npei_ctl_port;
		npei_ctl_port.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_CTL_PORT0);
		npei_ctl_port.s.bar2_enb = 1;
		npei_ctl_port.s.bar2_esx = 1;
		npei_ctl_port.s.bar2_cax = 0;
		npei_ctl_port.s.ptlp_ro = 1;
		npei_ctl_port.s.ctlp_ro = 1;
		npei_ctl_port.s.wait_com = 0;
		npei_ctl_port.s.waitl_com = 0;
		cvmx_write_csr(CVMX_PEXP_NPEI_CTL_PORT0, npei_ctl_port.u64);
	}

	if (OCTEON_IS_MODEL(OCTEON_CN56XX_PASS2_X) ||
	    OCTEON_IS_MODEL(OCTEON_CN52XX_PASS2_X) ||
	    OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X) ||
	    OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X)) {
		union cvmx_npei_dbg_data dbg_data;
		int old_in_fif_p_count;
		int in_fif_p_count;
		int out_p_count;
		int in_p_offset = (OCTEON_IS_MODEL(OCTEON_CN52XX_PASS1_X) || OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_X)) ? 4 : 1;
		int i;

		uint64_t write_address = (cvmx_pcie_get_mem_base_address(pcie_port) + 0x100000) | (1ull<<63);

		i = in_p_offset;
		while (i--) {
			cvmx_write64_uint32(write_address, 0);
			cvmx_wait(10000);
		}

		cvmx_write_csr(CVMX_PEXP_NPEI_DBG_SELECT, (pcie_port) ? 0xd7fc : 0xcffc);
		cvmx_read_csr(CVMX_PEXP_NPEI_DBG_SELECT);
		do {
			dbg_data.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
			old_in_fif_p_count = dbg_data.s.data & 0xff;
			cvmx_write64_uint32(write_address, 0);
			cvmx_wait(10000);
			dbg_data.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
			in_fif_p_count = dbg_data.s.data & 0xff;
		} while (in_fif_p_count != ((old_in_fif_p_count+1) & 0xff));

		
		in_fif_p_count = (in_fif_p_count + in_p_offset) & 0xff;

		
		cvmx_write_csr(CVMX_PEXP_NPEI_DBG_SELECT, (pcie_port) ? 0xd00f : 0xc80f);
		cvmx_read_csr(CVMX_PEXP_NPEI_DBG_SELECT);
		dbg_data.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
		out_p_count = (dbg_data.s.data>>1) & 0xff;

		
		if (out_p_count != in_fif_p_count) {
			cvmx_dprintf("PCIe: Port %d aligning TLP counters as workaround to maintain ordering\n", pcie_port);
			while (in_fif_p_count != 0) {
				cvmx_write64_uint32(write_address, 0);
				cvmx_wait(10000);
				in_fif_p_count = (in_fif_p_count + 1) & 0xff;
			}
			if ((cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_EBH5200) &&
				(pcie_port == 1))
				cvmx_pcie_rc_initialize(0);
			
			goto retry;
		}
	}

	
	pciercx_cfg032.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG032(pcie_port));
	cvmx_dprintf("PCIe: Port %d link active, %d lanes\n", pcie_port, pciercx_cfg032.s.nlw);

	return 0;
}

static int __cvmx_pcie_rc_initialize_link_gen2(int pcie_port)
{
	uint64_t start_cycle;
	union cvmx_pemx_ctl_status pem_ctl_status;
	union cvmx_pciercx_cfg032 pciercx_cfg032;
	union cvmx_pciercx_cfg448 pciercx_cfg448;

	
	pem_ctl_status.u64 = cvmx_read_csr(CVMX_PEMX_CTL_STATUS(pcie_port));
	pem_ctl_status.s.lnk_enb = 1;
	cvmx_write_csr(CVMX_PEMX_CTL_STATUS(pcie_port), pem_ctl_status.u64);

	
	start_cycle = cvmx_get_cycle();
	do {
		if (cvmx_get_cycle() - start_cycle >  octeon_get_clock_rate())
			return -1;
		cvmx_wait(10000);
		pciercx_cfg032.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG032(pcie_port));
	} while ((pciercx_cfg032.s.dlla == 0) || (pciercx_cfg032.s.lt == 1));

	pciercx_cfg448.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG448(pcie_port));
	switch (pciercx_cfg032.s.nlw) {
	case 1: 
		pciercx_cfg448.s.rtl = 1677;
		break;
	case 2: 
		pciercx_cfg448.s.rtl = 867;
		break;
	case 4: 
		pciercx_cfg448.s.rtl = 462;
		break;
	case 8: 
		pciercx_cfg448.s.rtl = 258;
		break;
	}
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG448(pcie_port), pciercx_cfg448.u32);

	return 0;
}


static int __cvmx_pcie_rc_initialize_gen2(int pcie_port)
{
	int i;
	union cvmx_ciu_soft_prst ciu_soft_prst;
	union cvmx_mio_rst_ctlx mio_rst_ctl;
	union cvmx_pemx_bar_ctl pemx_bar_ctl;
	union cvmx_pemx_ctl_status pemx_ctl_status;
	union cvmx_pemx_bist_status pemx_bist_status;
	union cvmx_pemx_bist_status2 pemx_bist_status2;
	union cvmx_pciercx_cfg032 pciercx_cfg032;
	union cvmx_pciercx_cfg515 pciercx_cfg515;
	union cvmx_sli_ctl_portx sli_ctl_portx;
	union cvmx_sli_mem_access_ctl sli_mem_access_ctl;
	union cvmx_sli_mem_access_subidx mem_access_subid;
	union cvmx_sriox_status_reg sriox_status_reg;
	union cvmx_pemx_bar1_indexx bar1_index;

	if (octeon_has_feature(OCTEON_FEATURE_SRIO)) {
		
		if (OCTEON_IS_MODEL(OCTEON_CN66XX)) {
			union cvmx_mio_qlmx_cfg qlmx_cfg;
			qlmx_cfg.u64 = cvmx_read_csr(CVMX_MIO_QLMX_CFG(pcie_port));

			if (qlmx_cfg.s.qlm_spd == 15) {
				pr_notice("PCIe: Port %d is disabled, skipping.\n", pcie_port);
				return -1;
			}

			switch (qlmx_cfg.s.qlm_spd) {
			case 0x1: 
			case 0x3: 
			case 0x4: 
			case 0x6: 
				pr_notice("PCIe: Port %d is SRIO, skipping.\n", pcie_port);
				return -1;
			case 0x9: 
				pr_notice("PCIe: Port %d is SGMII, skipping.\n", pcie_port);
				return -1;
			case 0xb: 
				pr_notice("PCIe: Port %d is XAUI, skipping.\n", pcie_port);
				return -1;
			case 0x0: 
			case 0x8: 
			case 0x2: 
			case 0xa: 
				break;
			default:
				pr_notice("PCIe: Port %d is unknown, skipping.\n", pcie_port);
				return -1;
			}
		} else {
			sriox_status_reg.u64 = cvmx_read_csr(CVMX_SRIOX_STATUS_REG(pcie_port));
			if (sriox_status_reg.s.srio) {
				pr_notice("PCIe: Port %d is SRIO, skipping.\n", pcie_port);
				return -1;
			}
		}
	}

#if 0
    
	pr_notice("PCIE : init for pcie analyzer.\n");
	cvmx_helper_qlm_jtag_init();
	cvmx_helper_qlm_jtag_shift_zeros(pcie_port, 85);
	cvmx_helper_qlm_jtag_shift(pcie_port, 1, 1);
	cvmx_helper_qlm_jtag_shift_zeros(pcie_port, 300-86);
	cvmx_helper_qlm_jtag_shift_zeros(pcie_port, 85);
	cvmx_helper_qlm_jtag_shift(pcie_port, 1, 1);
	cvmx_helper_qlm_jtag_shift_zeros(pcie_port, 300-86);
	cvmx_helper_qlm_jtag_shift_zeros(pcie_port, 85);
	cvmx_helper_qlm_jtag_shift(pcie_port, 1, 1);
	cvmx_helper_qlm_jtag_shift_zeros(pcie_port, 300-86);
	cvmx_helper_qlm_jtag_shift_zeros(pcie_port, 85);
	cvmx_helper_qlm_jtag_shift(pcie_port, 1, 1);
	cvmx_helper_qlm_jtag_shift_zeros(pcie_port, 300-86);
	cvmx_helper_qlm_jtag_update(pcie_port);
#endif

	
	mio_rst_ctl.u64 = cvmx_read_csr(CVMX_MIO_RST_CTLX(pcie_port));
	if (!mio_rst_ctl.s.host_mode) {
		pr_notice("PCIe: Port %d in endpoint mode.\n", pcie_port);
		return -1;
	}

	
	if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_0)) {
		if (pcie_port) {
			union cvmx_ciu_qlm1 ciu_qlm;
			ciu_qlm.u64 = cvmx_read_csr(CVMX_CIU_QLM1);
			ciu_qlm.s.txbypass = 1;
			ciu_qlm.s.txdeemph = 5;
			ciu_qlm.s.txmargin = 0x17;
			cvmx_write_csr(CVMX_CIU_QLM1, ciu_qlm.u64);
		} else {
			union cvmx_ciu_qlm0 ciu_qlm;
			ciu_qlm.u64 = cvmx_read_csr(CVMX_CIU_QLM0);
			ciu_qlm.s.txbypass = 1;
			ciu_qlm.s.txdeemph = 5;
			ciu_qlm.s.txmargin = 0x17;
			cvmx_write_csr(CVMX_CIU_QLM0, ciu_qlm.u64);
		}
	}
	
	if (pcie_port)
		ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
	else
		ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
	if (ciu_soft_prst.s.soft_prst == 0) {
		
		ciu_soft_prst.s.soft_prst = 1;
		if (pcie_port)
			cvmx_write_csr(CVMX_CIU_SOFT_PRST1, ciu_soft_prst.u64);
		else
			cvmx_write_csr(CVMX_CIU_SOFT_PRST, ciu_soft_prst.u64);
		
		udelay(2000);
	}
	if (pcie_port) {
		ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST1);
		ciu_soft_prst.s.soft_prst = 0;
		cvmx_write_csr(CVMX_CIU_SOFT_PRST1, ciu_soft_prst.u64);
	} else {
		ciu_soft_prst.u64 = cvmx_read_csr(CVMX_CIU_SOFT_PRST);
		ciu_soft_prst.s.soft_prst = 0;
		cvmx_write_csr(CVMX_CIU_SOFT_PRST, ciu_soft_prst.u64);
	}

	
	udelay(1000);

	if (CVMX_WAIT_FOR_FIELD64(CVMX_MIO_RST_CTLX(pcie_port), union cvmx_mio_rst_ctlx, rst_done, ==, 1, 10000)) {
		pr_notice("PCIe: Port %d stuck in reset, skipping.\n", pcie_port);
		return -1;
	}

	
	pemx_bist_status.u64 = cvmx_read_csr(CVMX_PEMX_BIST_STATUS(pcie_port));
	if (pemx_bist_status.u64)
		pr_notice("PCIe: BIST FAILED for port %d (0x%016llx)\n", pcie_port, CAST64(pemx_bist_status.u64));
	pemx_bist_status2.u64 = cvmx_read_csr(CVMX_PEMX_BIST_STATUS2(pcie_port));
	
	if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_X))
		pemx_bist_status2.u64 &= ~0x3full;
	if (pemx_bist_status2.u64)
		pr_notice("PCIe: BIST2 FAILED for port %d (0x%016llx)\n", pcie_port, CAST64(pemx_bist_status2.u64));

	
	__cvmx_pcie_rc_initialize_config_space(pcie_port);

	
	pciercx_cfg515.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG515(pcie_port));
	pciercx_cfg515.s.dsc = 1;
	cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG515(pcie_port), pciercx_cfg515.u32);

	
	if (__cvmx_pcie_rc_initialize_link_gen2(pcie_port)) {
		union cvmx_pciercx_cfg031 pciercx_cfg031;
		pciercx_cfg031.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG031(pcie_port));
		pciercx_cfg031.s.mls = 1;
		cvmx_pcie_cfgx_write(pcie_port, CVMX_PCIERCX_CFG031(pcie_port), pciercx_cfg031.u32);
		if (__cvmx_pcie_rc_initialize_link_gen2(pcie_port)) {
			pr_notice("PCIe: Link timeout on port %d, probably the slot is empty\n", pcie_port);
			return -1;
		}
	}

	
	sli_mem_access_ctl.u64 = cvmx_read_csr(CVMX_PEXP_SLI_MEM_ACCESS_CTL);
	sli_mem_access_ctl.s.max_word = 0;	
	sli_mem_access_ctl.s.timer = 127;	
	cvmx_write_csr(CVMX_PEXP_SLI_MEM_ACCESS_CTL, sli_mem_access_ctl.u64);

	
	mem_access_subid.u64 = 0;
	mem_access_subid.s.port = pcie_port; 
	mem_access_subid.s.nmerge = 0;  
	mem_access_subid.s.esr = 1;     
	mem_access_subid.s.esw = 1;     
	mem_access_subid.s.wtype = 0;   
	mem_access_subid.s.rtype = 0;   
	
	if (OCTEON_IS_MODEL(OCTEON_CN68XX))
		mem_access_subid.cn68xx.ba = 0;
	else
		mem_access_subid.cn63xx.ba = 0;

	for (i = 12 + pcie_port * 4; i < 16 + pcie_port * 4; i++) {
		cvmx_write_csr(CVMX_PEXP_SLI_MEM_ACCESS_SUBIDX(i), mem_access_subid.u64);
		
		__cvmx_increment_ba(&mem_access_subid);
	}

	for (i = 0; i < 4; i++) {
		cvmx_write_csr(CVMX_PEMX_P2P_BARX_START(i, pcie_port), -1);
		cvmx_write_csr(CVMX_PEMX_P2P_BARX_END(i, pcie_port), -1);
	}

	
	cvmx_write_csr(CVMX_PEMX_P2N_BAR0_START(pcie_port), 0);

	cvmx_write_csr(CVMX_PEMX_P2N_BAR2_START(pcie_port), 0);

	pemx_bar_ctl.u64 = cvmx_read_csr(CVMX_PEMX_BAR_CTL(pcie_port));
	pemx_bar_ctl.s.bar1_siz = 3;  
	pemx_bar_ctl.s.bar2_enb = 1;
	pemx_bar_ctl.s.bar2_esx = 1;
	pemx_bar_ctl.s.bar2_cax = 0;
	cvmx_write_csr(CVMX_PEMX_BAR_CTL(pcie_port), pemx_bar_ctl.u64);
	sli_ctl_portx.u64 = cvmx_read_csr(CVMX_PEXP_SLI_CTL_PORTX(pcie_port));
	sli_ctl_portx.s.ptlp_ro = 1;
	sli_ctl_portx.s.ctlp_ro = 1;
	sli_ctl_portx.s.wait_com = 0;
	sli_ctl_portx.s.waitl_com = 0;
	cvmx_write_csr(CVMX_PEXP_SLI_CTL_PORTX(pcie_port), sli_ctl_portx.u64);

	
	cvmx_write_csr(CVMX_PEMX_P2N_BAR1_START(pcie_port), CVMX_PCIE_BAR1_RC_BASE);

	bar1_index.u64 = 0;
	bar1_index.s.addr_idx = (CVMX_PCIE_BAR1_PHYS_BASE >> 22);
	bar1_index.s.ca = 1;       
	bar1_index.s.end_swp = 1;  
	bar1_index.s.addr_v = 1;   

	for (i = 0; i < 16; i++) {
		cvmx_write_csr(CVMX_PEMX_BAR1_INDEXX(i, pcie_port), bar1_index.u64);
		
		bar1_index.s.addr_idx += (((1ull << 28) / 16ull) >> 22);
	}

	pemx_ctl_status.u64 = cvmx_read_csr(CVMX_PEMX_CTL_STATUS(pcie_port));
	pemx_ctl_status.s.cfg_rtry = 250 * 5000000 / 0x10000;
	cvmx_write_csr(CVMX_PEMX_CTL_STATUS(pcie_port), pemx_ctl_status.u64);

	
	pciercx_cfg032.u32 = cvmx_pcie_cfgx_read(pcie_port, CVMX_PCIERCX_CFG032(pcie_port));
	pr_notice("PCIe: Port %d link active, %d lanes, speed gen%d\n", pcie_port, pciercx_cfg032.s.nlw, pciercx_cfg032.s.ls);

	return 0;
}

static int cvmx_pcie_rc_initialize(int pcie_port)
{
	int result;
	if (octeon_has_feature(OCTEON_FEATURE_NPEI))
		result = __cvmx_pcie_rc_initialize_gen1(pcie_port);
	else
		result = __cvmx_pcie_rc_initialize_gen2(pcie_port);
	return result;
}


int __init octeon_pcie_pcibios_map_irq(const struct pci_dev *dev,
				       u8 slot, u8 pin)
{
	if (strstr(octeon_board_type_string(), "EBH5600") &&
	    dev->bus && dev->bus->parent) {
		while (dev->bus && dev->bus->parent)
			dev = to_pci_dev(dev->bus->bridge);
		if ((dev->bus->number == 1) &&
		    (dev->vendor == 0x10b5) && (dev->device == 0x8114)) {
			pin = ((pin - 3) & 3) + 1;
		}
	}
	return pin - 1 + OCTEON_IRQ_PCI_INT0;
}

static  void set_cfg_read_retry(u32 retry_cnt)
{
	union cvmx_pemx_ctl_status pemx_ctl;
	pemx_ctl.u64 = cvmx_read_csr(CVMX_PEMX_CTL_STATUS(1));
	pemx_ctl.s.cfg_rtry = retry_cnt;
	cvmx_write_csr(CVMX_PEMX_CTL_STATUS(1), pemx_ctl.u64);
}


static u32 disable_cfg_read_retry(void)
{
	u32 retry_cnt;

	union cvmx_pemx_ctl_status pemx_ctl;
	pemx_ctl.u64 = cvmx_read_csr(CVMX_PEMX_CTL_STATUS(1));
	retry_cnt =  pemx_ctl.s.cfg_rtry;
	pemx_ctl.s.cfg_rtry = 0;
	cvmx_write_csr(CVMX_PEMX_CTL_STATUS(1), pemx_ctl.u64);
	return retry_cnt;
}

static int is_cfg_retry(void)
{
	union cvmx_pemx_int_sum pemx_int_sum;
	pemx_int_sum.u64 = cvmx_read_csr(CVMX_PEMX_INT_SUM(1));
	if (pemx_int_sum.s.crs_dr)
		return 1;
	return 0;
}

static int octeon_pcie_read_config(unsigned int pcie_port, struct pci_bus *bus,
				   unsigned int devfn, int reg, int size,
				   u32 *val)
{
	union octeon_cvmemctl cvmmemctl;
	union octeon_cvmemctl cvmmemctl_save;
	int bus_number = bus->number;
	int cfg_retry = 0;
	int retry_cnt = 0;
	int max_retry_cnt = 10;
	u32 cfg_retry_cnt = 0;

	cvmmemctl_save.u64 = 0;
	BUG_ON(pcie_port >= ARRAY_SIZE(enable_pcie_bus_num_war));
	if (bus->parent == NULL) {
		if (enable_pcie_bus_num_war[pcie_port])
			bus_number = 0;
		else {
			union cvmx_pciercx_cfg006 pciercx_cfg006;
			pciercx_cfg006.u32 = cvmx_pcie_cfgx_read(pcie_port,
					     CVMX_PCIERCX_CFG006(pcie_port));
			if (pciercx_cfg006.s.pbnum != bus_number) {
				pciercx_cfg006.s.pbnum = bus_number;
				pciercx_cfg006.s.sbnum = bus_number;
				pciercx_cfg006.s.subbnum = bus_number;
				cvmx_pcie_cfgx_write(pcie_port,
					    CVMX_PCIERCX_CFG006(pcie_port),
					    pciercx_cfg006.u32);
			}
		}
	}

	if ((bus->parent == NULL) && (devfn >> 3 != 0))
		return PCIBIOS_FUNC_NOT_SUPPORTED;

	if (OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1) ||
	    OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_1)) {
		if ((bus->parent == NULL) && (devfn >= 2))
			return PCIBIOS_FUNC_NOT_SUPPORTED;
#if 1
		
		if (bus_number == 2)
			return PCIBIOS_FUNC_NOT_SUPPORTED;
#elif 0
		if ((bus_number == 2) && (devfn >> 3 != 2))
			return PCIBIOS_FUNC_NOT_SUPPORTED;
#elif 0
		if ((bus_number == 2) && (devfn >> 3 != 3))
			return PCIBIOS_FUNC_NOT_SUPPORTED;
#elif 0
		
		if ((bus_number == 2) &&
		    !((devfn == (2 << 3)) || (devfn == (3 << 3))))
			return PCIBIOS_FUNC_NOT_SUPPORTED;
#endif

#if 0
		
		if ((bus_number == 4) &&
		    !((devfn >> 3 >= 1) && (devfn >> 3 <= 4)))
			return PCIBIOS_FUNC_NOT_SUPPORTED;
		
		if ((bus_number == 5) && (devfn >> 3 != 0))
			return PCIBIOS_FUNC_NOT_SUPPORTED;
		
		if ((bus_number == 6) && (devfn >> 3 != 0))
			return PCIBIOS_FUNC_NOT_SUPPORTED;
		
		if ((bus_number == 7) && (devfn >> 3 != 0))
			return PCIBIOS_FUNC_NOT_SUPPORTED;
		
		if ((bus_number == 8) && (devfn >> 3 != 0))
			return PCIBIOS_FUNC_NOT_SUPPORTED;
#endif

		cvmmemctl_save.u64 = __read_64bit_c0_register($11, 7);
		cvmmemctl.u64 = cvmmemctl_save.u64;
		cvmmemctl.s.didtto = 2;
		__write_64bit_c0_register($11, 7, cvmmemctl.u64);
	}

	if ((OCTEON_IS_MODEL(OCTEON_CN63XX)) && (enable_pcie_14459_war))
		cfg_retry_cnt = disable_cfg_read_retry();

	pr_debug("pcie_cfg_rd port=%d b=%d devfn=0x%03x reg=0x%03x"
		 " size=%d ", pcie_port, bus_number, devfn, reg, size);
	do {
		switch (size) {
		case 4:
			*val = cvmx_pcie_config_read32(pcie_port, bus_number,
				devfn >> 3, devfn & 0x7, reg);
		break;
		case 2:
			*val = cvmx_pcie_config_read16(pcie_port, bus_number,
				devfn >> 3, devfn & 0x7, reg);
		break;
		case 1:
			*val = cvmx_pcie_config_read8(pcie_port, bus_number,
				devfn >> 3, devfn & 0x7, reg);
		break;
		default:
			if (OCTEON_IS_MODEL(OCTEON_CN63XX))
				set_cfg_read_retry(cfg_retry_cnt);
			return PCIBIOS_FUNC_NOT_SUPPORTED;
		}
		if ((OCTEON_IS_MODEL(OCTEON_CN63XX)) &&
			(enable_pcie_14459_war)) {
			cfg_retry = is_cfg_retry();
			retry_cnt++;
			if (retry_cnt > max_retry_cnt) {
				pr_err(" pcie cfg_read retries failed. retry_cnt=%d\n",
				       retry_cnt);
				cfg_retry = 0;
			}
		}
	} while (cfg_retry);

	if ((OCTEON_IS_MODEL(OCTEON_CN63XX)) && (enable_pcie_14459_war))
		set_cfg_read_retry(cfg_retry_cnt);
	pr_debug("val=%08x  : tries=%02d\n", *val, retry_cnt);
	if (OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1) ||
	    OCTEON_IS_MODEL(OCTEON_CN56XX_PASS1_1))
		write_c0_cvmmemctl(cvmmemctl_save.u64);
	return PCIBIOS_SUCCESSFUL;
}

static int octeon_pcie0_read_config(struct pci_bus *bus, unsigned int devfn,
				    int reg, int size, u32 *val)
{
	return octeon_pcie_read_config(0, bus, devfn, reg, size, val);
}

static int octeon_pcie1_read_config(struct pci_bus *bus, unsigned int devfn,
				    int reg, int size, u32 *val)
{
	return octeon_pcie_read_config(1, bus, devfn, reg, size, val);
}

static int octeon_dummy_read_config(struct pci_bus *bus, unsigned int devfn,
				    int reg, int size, u32 *val)
{
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}

static int octeon_pcie_write_config(unsigned int pcie_port, struct pci_bus *bus,
				    unsigned int devfn, int reg,
				    int size, u32 val)
{
	int bus_number = bus->number;

	BUG_ON(pcie_port >= ARRAY_SIZE(enable_pcie_bus_num_war));

	if ((bus->parent == NULL) && (enable_pcie_bus_num_war[pcie_port]))
		bus_number = 0;

	pr_debug("pcie_cfg_wr port=%d b=%d devfn=0x%03x"
		 " reg=0x%03x size=%d val=%08x\n", pcie_port, bus_number, devfn,
		 reg, size, val);


	switch (size) {
	case 4:
		cvmx_pcie_config_write32(pcie_port, bus_number, devfn >> 3,
					 devfn & 0x7, reg, val);
		break;
	case 2:
		cvmx_pcie_config_write16(pcie_port, bus_number, devfn >> 3,
					 devfn & 0x7, reg, val);
		break;
	case 1:
		cvmx_pcie_config_write8(pcie_port, bus_number, devfn >> 3,
					devfn & 0x7, reg, val);
		break;
	default:
		return PCIBIOS_FUNC_NOT_SUPPORTED;
	}
#if PCI_CONFIG_SPACE_DELAY
	udelay(PCI_CONFIG_SPACE_DELAY);
#endif
	return PCIBIOS_SUCCESSFUL;
}

static int octeon_pcie0_write_config(struct pci_bus *bus, unsigned int devfn,
				     int reg, int size, u32 val)
{
	return octeon_pcie_write_config(0, bus, devfn, reg, size, val);
}

static int octeon_pcie1_write_config(struct pci_bus *bus, unsigned int devfn,
				     int reg, int size, u32 val)
{
	return octeon_pcie_write_config(1, bus, devfn, reg, size, val);
}

static int octeon_dummy_write_config(struct pci_bus *bus, unsigned int devfn,
				     int reg, int size, u32 val)
{
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}

static struct pci_ops octeon_pcie0_ops = {
	octeon_pcie0_read_config,
	octeon_pcie0_write_config,
};

static struct resource octeon_pcie0_mem_resource = {
	.name = "Octeon PCIe0 MEM",
	.flags = IORESOURCE_MEM,
};

static struct resource octeon_pcie0_io_resource = {
	.name = "Octeon PCIe0 IO",
	.flags = IORESOURCE_IO,
};

static struct pci_controller octeon_pcie0_controller = {
	.pci_ops = &octeon_pcie0_ops,
	.mem_resource = &octeon_pcie0_mem_resource,
	.io_resource = &octeon_pcie0_io_resource,
};

static struct pci_ops octeon_pcie1_ops = {
	octeon_pcie1_read_config,
	octeon_pcie1_write_config,
};

static struct resource octeon_pcie1_mem_resource = {
	.name = "Octeon PCIe1 MEM",
	.flags = IORESOURCE_MEM,
};

static struct resource octeon_pcie1_io_resource = {
	.name = "Octeon PCIe1 IO",
	.flags = IORESOURCE_IO,
};

static struct pci_controller octeon_pcie1_controller = {
	.pci_ops = &octeon_pcie1_ops,
	.mem_resource = &octeon_pcie1_mem_resource,
	.io_resource = &octeon_pcie1_io_resource,
};

static struct pci_ops octeon_dummy_ops = {
	octeon_dummy_read_config,
	octeon_dummy_write_config,
};

static struct resource octeon_dummy_mem_resource = {
	.name = "Virtual PCIe MEM",
	.flags = IORESOURCE_MEM,
};

static struct resource octeon_dummy_io_resource = {
	.name = "Virtual PCIe IO",
	.flags = IORESOURCE_IO,
};

static struct pci_controller octeon_dummy_controller = {
	.pci_ops = &octeon_dummy_ops,
	.mem_resource = &octeon_dummy_mem_resource,
	.io_resource = &octeon_dummy_io_resource,
};

static int device_needs_bus_num_war(uint32_t deviceid)
{
#define IDT_VENDOR_ID 0x111d

	if ((deviceid  & 0xffff) == IDT_VENDOR_ID)
		return 1;
	return 0;
}

static int __init octeon_pcie_setup(void)
{
	int result;
	int host_mode;
	int srio_war15205 = 0, port;
	union cvmx_sli_ctl_portx sli_ctl_portx;
	union cvmx_sriox_status_reg sriox_status_reg;

	
	if (!octeon_has_feature(OCTEON_FEATURE_PCIE))
		return 0;

	
	if (octeon_is_simulation())
		return 0;

	
	if (pcie_disable)
		return 0;

	
	octeon_pcibios_map_irq = octeon_pcie_pcibios_map_irq;

	set_io_port_base(CVMX_ADD_IO_SEG(cvmx_pcie_get_io_base_address(0)));
	ioport_resource.start = 0;
	ioport_resource.end =
		cvmx_pcie_get_io_base_address(1) -
		cvmx_pcie_get_io_base_address(0) + cvmx_pcie_get_io_size(1) - 1;

	octeon_dummy_controller.io_map_base = -1;
	octeon_dummy_controller.mem_resource->start = (1ull<<48);
	octeon_dummy_controller.mem_resource->end = (1ull<<48);
	register_pci_controller(&octeon_dummy_controller);

	if (octeon_has_feature(OCTEON_FEATURE_NPEI)) {
		union cvmx_npei_ctl_status npei_ctl_status;
		npei_ctl_status.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_CTL_STATUS);
		host_mode = npei_ctl_status.s.host_mode;
		octeon_dma_bar_type = OCTEON_DMA_BAR_TYPE_PCIE;
	} else {
		union cvmx_mio_rst_ctlx mio_rst_ctl;
		mio_rst_ctl.u64 = cvmx_read_csr(CVMX_MIO_RST_CTLX(0));
		host_mode = mio_rst_ctl.s.host_mode;
		octeon_dma_bar_type = OCTEON_DMA_BAR_TYPE_PCIE2;
	}

	if (host_mode) {
		pr_notice("PCIe: Initializing port 0\n");
		
		if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_X) ||
			OCTEON_IS_MODEL(OCTEON_CN63XX_PASS2_0)) {
			sriox_status_reg.u64 = cvmx_read_csr(CVMX_SRIOX_STATUS_REG(0));
			if (sriox_status_reg.s.srio) {
				srio_war15205 += 1;      
				port = 0;
			}
		}
		result = cvmx_pcie_rc_initialize(0);
		if (result == 0) {
			uint32_t device0;
			
			octeon_pcie0_controller.mem_offset =
				cvmx_pcie_get_mem_base_address(0);
			
			octeon_pcie0_controller.io_map_base =
				CVMX_ADD_IO_SEG(cvmx_pcie_get_io_base_address
						(0));
			octeon_pcie0_controller.io_offset = 0;
			octeon_pcie0_controller.mem_resource->start =
				cvmx_pcie_get_mem_base_address(0) +
				(4ul << 30) - (OCTEON_PCI_BAR1_HOLE_SIZE << 20);
			octeon_pcie0_controller.mem_resource->end =
				cvmx_pcie_get_mem_base_address(0) +
				cvmx_pcie_get_mem_size(0) - 1;
			octeon_pcie0_controller.io_resource->start = 4 << 10;
			octeon_pcie0_controller.io_resource->end =
				cvmx_pcie_get_io_size(0) - 1;
			msleep(100); 
			register_pci_controller(&octeon_pcie0_controller);
			device0 = cvmx_pcie_config_read32(0, 0, 0, 0, 0);
			enable_pcie_bus_num_war[0] =
				device_needs_bus_num_war(device0);
		}
	} else {
		pr_notice("PCIe: Port 0 in endpoint mode, skipping.\n");
		
		if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_X) ||
			OCTEON_IS_MODEL(OCTEON_CN63XX_PASS2_0)) {
			srio_war15205 += 1;
			port = 0;
		}
	}

	if (octeon_has_feature(OCTEON_FEATURE_NPEI)) {
		host_mode = 1;
		
		if (OCTEON_IS_MODEL(OCTEON_CN52XX)) {
			union cvmx_npei_dbg_data dbg_data;
			dbg_data.u64 = cvmx_read_csr(CVMX_PEXP_NPEI_DBG_DATA);
			if (dbg_data.cn52xx.qlm0_link_width)
				host_mode = 0;
		}
	} else {
		union cvmx_mio_rst_ctlx mio_rst_ctl;
		mio_rst_ctl.u64 = cvmx_read_csr(CVMX_MIO_RST_CTLX(1));
		host_mode = mio_rst_ctl.s.host_mode;
	}

	if (host_mode) {
		pr_notice("PCIe: Initializing port 1\n");
		
		if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_X) ||
			OCTEON_IS_MODEL(OCTEON_CN63XX_PASS2_0)) {
			sriox_status_reg.u64 = cvmx_read_csr(CVMX_SRIOX_STATUS_REG(1));
			if (sriox_status_reg.s.srio) {
				srio_war15205 += 1;      
				port = 1;
			}
		}
		result = cvmx_pcie_rc_initialize(1);
		if (result == 0) {
			uint32_t device0;
			
			octeon_pcie1_controller.mem_offset =
				cvmx_pcie_get_mem_base_address(1);
			octeon_pcie1_controller.io_map_base =
				CVMX_ADD_IO_SEG(cvmx_pcie_get_io_base_address(0));
			
			octeon_pcie1_controller.io_offset =
				cvmx_pcie_get_io_base_address(1) -
				cvmx_pcie_get_io_base_address(0);
			octeon_pcie1_controller.mem_resource->start =
				cvmx_pcie_get_mem_base_address(1) + (4ul << 30) -
				(OCTEON_PCI_BAR1_HOLE_SIZE << 20);
			octeon_pcie1_controller.mem_resource->end =
				cvmx_pcie_get_mem_base_address(1) +
				cvmx_pcie_get_mem_size(1) - 1;
			octeon_pcie1_controller.io_resource->start =
				cvmx_pcie_get_io_base_address(1) -
				cvmx_pcie_get_io_base_address(0);
			octeon_pcie1_controller.io_resource->end =
				octeon_pcie1_controller.io_resource->start +
				cvmx_pcie_get_io_size(1) - 1;
			msleep(100); 
			register_pci_controller(&octeon_pcie1_controller);
			device0 = cvmx_pcie_config_read32(1, 0, 0, 0, 0);
			enable_pcie_bus_num_war[1] =
				device_needs_bus_num_war(device0);
		}
	} else {
		pr_notice("PCIe: Port 1 not in root complex mode, skipping.\n");
		
		if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_X) ||
			OCTEON_IS_MODEL(OCTEON_CN63XX_PASS2_0)) {
			srio_war15205 += 1;
			port = 1;
		}
	}

	if (OCTEON_IS_MODEL(OCTEON_CN63XX_PASS1_X) ||
		OCTEON_IS_MODEL(OCTEON_CN63XX_PASS2_0)) {
		if (srio_war15205 == 1) {
			sli_ctl_portx.u64 = cvmx_read_csr(CVMX_PEXP_SLI_CTL_PORTX(port));
			sli_ctl_portx.s.inta_map = 1;
			sli_ctl_portx.s.intb_map = 1;
			sli_ctl_portx.s.intc_map = 1;
			sli_ctl_portx.s.intd_map = 1;
			cvmx_write_csr(CVMX_PEXP_SLI_CTL_PORTX(port), sli_ctl_portx.u64);

			sli_ctl_portx.u64 = cvmx_read_csr(CVMX_PEXP_SLI_CTL_PORTX(!port));
			sli_ctl_portx.s.inta_map = 0;
			sli_ctl_portx.s.intb_map = 0;
			sli_ctl_portx.s.intc_map = 0;
			sli_ctl_portx.s.intd_map = 0;
			cvmx_write_csr(CVMX_PEXP_SLI_CTL_PORTX(!port), sli_ctl_portx.u64);
		}
	}

	octeon_pci_dma_init();

	return 0;
}
arch_initcall(octeon_pcie_setup);
