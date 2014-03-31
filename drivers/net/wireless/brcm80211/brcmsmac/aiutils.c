/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * File contents: support functions for PCI/PCIe
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/pci.h>

#include <defs.h>
#include <chipcommon.h>
#include <brcmu_utils.h>
#include <brcm_hw_ids.h>
#include <soc.h>
#include "types.h"
#include "pub.h"
#include "pmu.h"
#include "srom.h"
#include "nicpci.h"
#include "aiutils.h"

 
#define SCC_SS_MASK		0x00000007
 
#define	SCC_SS_LPO		0x00000000
 
#define	SCC_SS_XTAL		0x00000001
 
#define	SCC_SS_PCI		0x00000002
 
#define SCC_LF			0x00000200
 
#define SCC_LP			0x00000400
 
#define SCC_FS			0x00000800
#define SCC_IP			0x00001000
#define SCC_XC			0x00002000
 
#define SCC_XP			0x00004000
 
#define SCC_CD_MASK		0xffff0000
#define SCC_CD_SHIFT		16

 
#define	SYCC_IE			0x00000001
 
#define	SYCC_AE			0x00000002
 
#define	SYCC_FP			0x00000004
 
#define	SYCC_AR			0x00000008
 
#define	SYCC_HR			0x00000010
 
#define SYCC_CD_MASK		0xffff0000
#define SYCC_CD_SHIFT		16

#define CST4329_SPROM_OTP_SEL_MASK	0x00000003
 
#define CST4329_DEFCIS_SEL		0
 
#define CST4329_SPROM_SEL		1
 
#define CST4329_OTP_SEL			2
 
#define CST4329_OTP_PWRDN		3

#define CST4329_SPI_SDIO_MODE_MASK	0x00000004
#define CST4329_SPI_SDIO_MODE_SHIFT	2

#define CCTRL43224_GPIO_TOGGLE          0x8000
 
#define CCTRL_43224A0_12MA_LED_DRIVE    0x00F000F0
 
#define CCTRL_43224B0_12MA_LED_DRIVE    0xF0

#define CST43236_SFLASH_MASK		0x00000040
#define CST43236_OTP_MASK		0x00000080
#define CST43236_HSIC_MASK		0x00000100	
#define CST43236_BP_CLK			0x00000200	
#define CST43236_BOOT_MASK		0x00001800
#define CST43236_BOOT_SHIFT		11
#define CST43236_BOOT_FROM_SRAM		0 
#define CST43236_BOOT_FROM_ROM		1 
#define CST43236_BOOT_FROM_FLASH	2 
#define CST43236_BOOT_FROM_INVALID	3

 
#define CCTRL4331_BT_COEXIST		(1<<0)
 
#define CCTRL4331_SECI			(1<<1)
 
#define CCTRL4331_EXT_LNA		(1<<2)
 
#define CCTRL4331_SPROM_GPIO13_15       (1<<3)
 
#define CCTRL4331_EXTPA_EN		(1<<4)
 
#define CCTRL4331_GPIOCLK_ON_SPROMCS	(1<<5)
 
#define CCTRL4331_PCIE_MDIO_ON_SPROMCS	(1<<6)
 
#define CCTRL4331_EXTPA_ON_GPIO2_5	(1<<7)
 
#define CCTRL4331_OVR_PIPEAUXCLKEN	(1<<8)
 
#define CCTRL4331_OVR_PIPEAUXPWRDOWN	(1<<9)
 
#define CCTRL4331_PCIE_AUXCLKEN		(1<<10)
 
#define CCTRL4331_PCIE_PIPE_PLLDOWN	(1<<11)
 
#define CCTRL4331_BT_SHD0_ON_GPIO4	(1<<16)
 
#define CCTRL4331_BT_SHD1_ON_GPIO5	(1<<17)

 
#define	CST4331_XTAL_FREQ		0x00000001
#define	CST4331_SPROM_PRESENT		0x00000002
#define	CST4331_OTP_PRESENT		0x00000004
#define	CST4331_LDO_RF			0x00000008
#define	CST4331_LDO_PAR			0x00000010

#define	CST4319_SPI_CPULESSUSB		0x00000001
#define	CST4319_SPI_CLK_POL		0x00000002
#define	CST4319_SPI_CLK_PH		0x00000008
 
#define	CST4319_SPROM_OTP_SEL_MASK	0x000000c0
#define	CST4319_SPROM_OTP_SEL_SHIFT	6
 
#define	CST4319_DEFCIS_SEL		0x00000000
 
#define	CST4319_SPROM_SEL		0x00000040
 
#define	CST4319_OTP_SEL			0x00000080
 
#define	CST4319_OTP_PWRDN		0x000000c0
 
#define	CST4319_SDIO_USB_MODE		0x00000100
#define	CST4319_REMAP_SEL_MASK		0x00000600
#define	CST4319_ILPDIV_EN		0x00000800
#define	CST4319_XTAL_PD_POL		0x00001000
#define	CST4319_LPO_SEL			0x00002000
#define	CST4319_RES_INIT_MODE		0x0000c000
 
#define	CST4319_PALDO_EXTPNP		0x00010000
#define	CST4319_CBUCK_MODE_MASK		0x00060000
#define CST4319_CBUCK_MODE_BURST	0x00020000
#define CST4319_CBUCK_MODE_LPBURST	0x00060000
#define	CST4319_RCAL_VALID		0x01000000
#define	CST4319_RCAL_VALUE_MASK		0x3e000000
#define	CST4319_RCAL_VALUE_SHIFT	25

#define	CST4336_SPI_MODE_MASK		0x00000001
#define	CST4336_SPROM_PRESENT		0x00000002
#define	CST4336_OTP_PRESENT		0x00000004
#define	CST4336_ARMREMAP_0		0x00000008
#define	CST4336_ILPDIV_EN_MASK		0x00000010
#define	CST4336_ILPDIV_EN_SHIFT		4
#define	CST4336_XTAL_PD_POL_MASK	0x00000020
#define	CST4336_XTAL_PD_POL_SHIFT	5
#define	CST4336_LPO_SEL_MASK		0x00000040
#define	CST4336_LPO_SEL_SHIFT		6
#define	CST4336_RES_INIT_MODE_MASK	0x00000180
#define	CST4336_RES_INIT_MODE_SHIFT	7
#define	CST4336_CBUCK_MODE_MASK		0x00000600
#define	CST4336_CBUCK_MODE_SHIFT	9

#define	CST4313_SPROM_PRESENT			1
#define	CST4313_OTP_PRESENT			2
#define	CST4313_SPROM_OTP_SEL_MASK		0x00000002
#define	CST4313_SPROM_OTP_SEL_SHIFT		0

 
#define CCTRL_4313_12MA_LED_DRIVE    0x00000007

#define	MFGID_ARM		0x43b
#define	MFGID_BRCM		0x4bf
#define	MFGID_MIPS		0x4a7

#define	ER_EROMENTRY		0x000
#define	ER_REMAPCONTROL		0xe00
#define	ER_REMAPSELECT		0xe04
#define	ER_MASTERSELECT		0xe10
#define	ER_ITCR			0xf00
#define	ER_ITIP			0xf04

#define	ER_TAG			0xe
#define	ER_TAG1			0x6
#define	ER_VALID		1
#define	ER_CI			0
#define	ER_MP			2
#define	ER_ADD			4
#define	ER_END			0xe
#define	ER_BAD			0xffffffff

#define	CIA_MFG_MASK		0xfff00000
#define	CIA_MFG_SHIFT		20
#define	CIA_CID_MASK		0x000fff00
#define	CIA_CID_SHIFT		8
#define	CIA_CCL_MASK		0x000000f0
#define	CIA_CCL_SHIFT		4

#define	CIB_REV_MASK		0xff000000
#define	CIB_REV_SHIFT		24
#define	CIB_NSW_MASK		0x00f80000
#define	CIB_NSW_SHIFT		19
#define	CIB_NMW_MASK		0x0007c000
#define	CIB_NMW_SHIFT		14
#define	CIB_NSP_MASK		0x00003e00
#define	CIB_NSP_SHIFT		9
#define	CIB_NMP_MASK		0x000001f0
#define	CIB_NMP_SHIFT		4

#define	AD_ADDR_MASK		0xfffff000
#define	AD_SP_MASK		0x00000f00
#define	AD_SP_SHIFT		8
#define	AD_ST_MASK		0x000000c0
#define	AD_ST_SHIFT		6
#define	AD_ST_SLAVE		0x00000000
#define	AD_ST_BRIDGE		0x00000040
#define	AD_ST_SWRAP		0x00000080
#define	AD_ST_MWRAP		0x000000c0
#define	AD_SZ_MASK		0x00000030
#define	AD_SZ_SHIFT		4
#define	AD_SZ_4K		0x00000000
#define	AD_SZ_8K		0x00000010
#define	AD_SZ_16K		0x00000020
#define	AD_SZ_SZD		0x00000030
#define	AD_AG32			0x00000008
#define	AD_ADDR_ALIGN		0x00000fff
#define	AD_SZ_BASE		0x00001000	

#define	SD_SZ_MASK		0xfffff000
#define	SD_SG32			0x00000008
#define	SD_SZ_ALIGN		0x00000fff

#define	PCI_CFG_GPIO_SCS	0x10
#define PCI_CFG_GPIO_XTAL	0x40
#define PCI_CFG_GPIO_PLL	0x80

#define PLL_DELAY		150	
#define FREF_DELAY		200	
#define	XTAL_ON_DELAY		1000	

#define	AIRC_RESET		1

#define	NOREV		-1	

#define DEFAULT_GPIO_ONTIME	10	
#define DEFAULT_GPIO_OFFTIME	90	

#define	SRC_START		0x80000000
#define	SRC_BUSY		0x80000000
#define	SRC_OPCODE		0x60000000
#define	SRC_OP_READ		0x00000000
#define	SRC_OP_WRITE		0x20000000
#define	SRC_OP_WRDIS		0x40000000
#define	SRC_OP_WREN		0x60000000
#define	SRC_OTPSEL		0x00000010
#define	SRC_LOCK		0x00000008
#define	SRC_SIZE_MASK		0x00000006
#define	SRC_SIZE_1K		0x00000000
#define	SRC_SIZE_4K		0x00000002
#define	SRC_SIZE_16K		0x00000004
#define	SRC_SIZE_SHIFT		1
#define	SRC_PRESENT		0x00000001

#define GPIO_CTRL_EPA_EN_MASK 0x40

#define DEFAULT_GPIOTIMERVAL \
	((DEFAULT_GPIO_ONTIME << GPIO_ONTIME_SHIFT) | DEFAULT_GPIO_OFFTIME)

#define	BADIDX		(SI_MAXCORES + 1)

#define	IS_SIM(chippkg)	\
	((chippkg == HDLSIM_PKG_ID) || (chippkg == HWSIM_PKG_ID))

#define PCI(sih)	(ai_get_buscoretype(sih) == PCI_CORE_ID)
#define PCIE(sih)	(ai_get_buscoretype(sih) == PCIE_CORE_ID)

#define PCI_FORCEHT(sih) (PCIE(sih) && (ai_get_chip_id(sih) == BCM4716_CHIP_ID))

#ifdef DEBUG
#define	SI_MSG(fmt, ...)	pr_debug(fmt, ##__VA_ARGS__)
#else
#define	SI_MSG(fmt, ...)	no_printk(fmt, ##__VA_ARGS__)
#endif				

#define	GOODCOREADDR(x, b) \
	(((x) >= (b)) && ((x) < ((b) + SI_MAXCORES * SI_CORE_SIZE)) && \
		IS_ALIGNED((x), SI_CORE_SIZE))

struct aidmp {
	u32 oobselina30;	
	u32 oobselina74;	
	u32 PAD[6];
	u32 oobselinb30;	
	u32 oobselinb74;	
	u32 PAD[6];
	u32 oobselinc30;	
	u32 oobselinc74;	
	u32 PAD[6];
	u32 oobselind30;	
	u32 oobselind74;	
	u32 PAD[38];
	u32 oobselouta30;	
	u32 oobselouta74;	
	u32 PAD[6];
	u32 oobseloutb30;	
	u32 oobseloutb74;	
	u32 PAD[6];
	u32 oobseloutc30;	
	u32 oobseloutc74;	
	u32 PAD[6];
	u32 oobseloutd30;	
	u32 oobseloutd74;	
	u32 PAD[38];
	u32 oobsynca;	
	u32 oobseloutaen;	
	u32 PAD[6];
	u32 oobsyncb;	
	u32 oobseloutben;	
	u32 PAD[6];
	u32 oobsyncc;	
	u32 oobseloutcen;	
	u32 PAD[6];
	u32 oobsyncd;	
	u32 oobseloutden;	
	u32 PAD[38];
	u32 oobaextwidth;	
	u32 oobainwidth;	
	u32 oobaoutwidth;	
	u32 PAD[5];
	u32 oobbextwidth;	
	u32 oobbinwidth;	
	u32 oobboutwidth;	
	u32 PAD[5];
	u32 oobcextwidth;	
	u32 oobcinwidth;	
	u32 oobcoutwidth;	
	u32 PAD[5];
	u32 oobdextwidth;	
	u32 oobdinwidth;	
	u32 oobdoutwidth;	
	u32 PAD[37];
	u32 ioctrlset;	
	u32 ioctrlclear;	
	u32 ioctrl;		
	u32 PAD[61];
	u32 iostatus;	
	u32 PAD[127];
	u32 ioctrlwidth;	
	u32 iostatuswidth;	
	u32 PAD[62];
	u32 resetctrl;	
	u32 resetstatus;	
	u32 resetreadid;	
	u32 resetwriteid;	
	u32 PAD[60];
	u32 errlogctrl;	
	u32 errlogdone;	
	u32 errlogstatus;	
	u32 errlogaddrlo;	
	u32 errlogaddrhi;	
	u32 errlogid;	
	u32 errloguser;	
	u32 errlogflags;	
	u32 PAD[56];
	u32 intstatus;	
	u32 PAD[127];
	u32 config;		
	u32 PAD[63];
	u32 itcr;		
	u32 PAD[3];
	u32 itipooba;	
	u32 itipoobb;	
	u32 itipoobc;	
	u32 itipoobd;	
	u32 PAD[4];
	u32 itipoobaout;	
	u32 itipoobbout;	
	u32 itipoobcout;	
	u32 itipoobdout;	
	u32 PAD[4];
	u32 itopooba;	
	u32 itopoobb;	
	u32 itopoobc;	
	u32 itopoobd;	
	u32 PAD[4];
	u32 itopoobain;	
	u32 itopoobbin;	
	u32 itopoobcin;	
	u32 itopoobdin;	
	u32 PAD[4];
	u32 itopreset;	
	u32 PAD[15];
	u32 peripherialid4;	
	u32 peripherialid5;	
	u32 peripherialid6;	
	u32 peripherialid7;	
	u32 peripherialid0;	
	u32 peripherialid1;	
	u32 peripherialid2;	
	u32 peripherialid3;	
	u32 componentid0;	
	u32 componentid1;	
	u32 componentid2;	
	u32 componentid3;	
};

static bool ai_ispcie(struct si_info *sii)
{
	u8 cap_ptr;

	cap_ptr =
	    pcicore_find_pci_capability(sii->pcibus, PCI_CAP_ID_EXP, NULL,
					NULL);
	if (!cap_ptr)
		return false;

	return true;
}

static bool ai_buscore_prep(struct si_info *sii)
{
	
	if (!ai_ispcie(sii))
		ai_clkctl_xtal(&sii->pub, XTAL | PLL, ON);
	return true;
}

static bool
ai_buscore_setup(struct si_info *sii, struct bcma_device *cc)
{
	struct bcma_device *pci = NULL;
	struct bcma_device *pcie = NULL;
	struct bcma_device *core;


	
	if (cc->bus->nr_cores == 0)
		return false;

	
	sii->pub.ccrev = cc->id.rev;

	
	if (ai_get_ccrev(&sii->pub) >= 11)
		sii->chipst = bcma_read32(cc, CHIPCREGOFFS(chipstatus));

	
	sii->pub.cccaps = bcma_read32(cc, CHIPCREGOFFS(capabilities));

	
	if (ai_get_cccaps(&sii->pub) & CC_CAP_PMU) {
		sii->pub.pmucaps = bcma_read32(cc,
					       CHIPCREGOFFS(pmucapabilities));
		sii->pub.pmurev = sii->pub.pmucaps & PCAP_REV_MASK;
	}

	
	list_for_each_entry(core, &cc->bus->cores, list) {
		uint cid, crev;

		cid = core->id.id;
		crev = core->id.rev;

		if (cid == PCI_CORE_ID) {
			pci = core;
		} else if (cid == PCIE_CORE_ID) {
			pcie = core;
		}
	}

	if (pci && pcie) {
		if (ai_ispcie(sii))
			pci = NULL;
		else
			pcie = NULL;
	}
	if (pci) {
		sii->buscore = pci;
	} else if (pcie) {
		sii->buscore = pcie;
	}

	
	if (!sii->pch) {
		sii->pch = pcicore_init(&sii->pub, sii->icbus->drv_pci.core);
		if (sii->pch == NULL)
			return false;
	}
	if (ai_pci_fixcfg(&sii->pub))
		return false;

	return true;
}

static __used void ai_nvram_process(struct si_info *sii)
{
	uint w = 0;

	
	pci_read_config_dword(sii->pcibus, PCI_SUBSYSTEM_VENDOR_ID, &w);

	sii->pub.boardvendor = w & 0xffff;
	sii->pub.boardtype = (w >> 16) & 0xffff;
}

static struct si_info *ai_doattach(struct si_info *sii,
				   struct bcma_bus *pbus)
{
	struct si_pub *sih = &sii->pub;
	u32 w, savewin;
	struct bcma_device *cc;
	uint socitype;

	savewin = 0;

	sii->icbus = pbus;
	sii->pcibus = pbus->host_pci;

	
	cc = pbus->drv_cc.core;

	
	if (!ai_buscore_prep(sii))
		return NULL;

	w = bcma_read32(cc, CHIPCREGOFFS(chipid));
	socitype = (w & CID_TYPE_MASK) >> CID_TYPE_SHIFT;
	
	sih->chip = w & CID_ID_MASK;
	sih->chiprev = (w & CID_REV_MASK) >> CID_REV_SHIFT;
	sih->chippkg = (w & CID_PKG_MASK) >> CID_PKG_SHIFT;

	
	if (socitype != SOCI_AI)
		return NULL;

	SI_MSG("Found chip type AI (0x%08x)\n", w);
	if (!ai_buscore_setup(sii, cc))
		goto exit;

	
	if (srom_var_init(&sii->pub))
		goto exit;

	ai_nvram_process(sii);

	
	bcma_write32(cc, CHIPCREGOFFS(gpiopullup), 0);
	bcma_write32(cc, CHIPCREGOFFS(gpiopulldown), 0);

	
	if (ai_get_cccaps(sih) & CC_CAP_PMU) {
		si_pmu_init(sih);
		(void)si_pmu_measure_alpclk(sih);
		si_pmu_res_init(sih);
	}

	
	w = getintvar(sih, BRCMS_SROM_LEDDC);
	if (w == 0)
		w = DEFAULT_GPIOTIMERVAL;
	ai_cc_reg(sih, offsetof(struct chipcregs, gpiotimerval),
		  ~0, w);

	if (PCIE(sih))
		pcicore_attach(sii->pch, SI_DOATTACH);

	if (ai_get_chip_id(sih) == BCM43224_CHIP_ID) {
		if (ai_get_chiprev(sih) == 0) {
			SI_MSG("Applying 43224A0 WARs\n");
			ai_cc_reg(sih, offsetof(struct chipcregs, chipcontrol),
				  CCTRL43224_GPIO_TOGGLE,
				  CCTRL43224_GPIO_TOGGLE);
			si_pmu_chipcontrol(sih, 0, CCTRL_43224A0_12MA_LED_DRIVE,
					   CCTRL_43224A0_12MA_LED_DRIVE);
		}
		if (ai_get_chiprev(sih) >= 1) {
			SI_MSG("Applying 43224B0+ WARs\n");
			si_pmu_chipcontrol(sih, 0, CCTRL_43224B0_12MA_LED_DRIVE,
					   CCTRL_43224B0_12MA_LED_DRIVE);
		}
	}

	if (ai_get_chip_id(sih) == BCM4313_CHIP_ID) {
		SI_MSG("Applying 4313 WARs\n");
		si_pmu_chipcontrol(sih, 0, CCTRL_4313_12MA_LED_DRIVE,
				   CCTRL_4313_12MA_LED_DRIVE);
	}

	return sii;

 exit:
	if (sii->pch)
		pcicore_deinit(sii->pch);
	sii->pch = NULL;

	return NULL;
}

struct si_pub *
ai_attach(struct bcma_bus *pbus)
{
	struct si_info *sii;

	
	sii = kzalloc(sizeof(struct si_info), GFP_ATOMIC);
	if (sii == NULL)
		return NULL;

	if (ai_doattach(sii, pbus) == NULL) {
		kfree(sii);
		return NULL;
	}

	return (struct si_pub *) sii;
}

void ai_detach(struct si_pub *sih)
{
	struct si_info *sii;

	struct si_pub *si_local = NULL;
	memcpy(&si_local, &sih, sizeof(struct si_pub **));

	sii = (struct si_info *)sih;

	if (sii == NULL)
		return;

	if (sii->pch)
		pcicore_deinit(sii->pch);
	sii->pch = NULL;

	srom_free_vars(sih);
	kfree(sii);
}

struct bcma_device *ai_findcore(struct si_pub *sih, u16 coreid, u16 coreunit)
{
	struct bcma_device *core;
	struct si_info *sii;
	uint found;

	sii = (struct si_info *)sih;

	found = 0;

	list_for_each_entry(core, &sii->icbus->cores, list)
		if (core->id.id == coreid) {
			if (found == coreunit)
				return core;
			found++;
		}

	return NULL;
}

uint ai_cc_reg(struct si_pub *sih, uint regoff, u32 mask, u32 val)
{
	struct bcma_device *cc;
	u32 w;
	struct si_info *sii;

	sii = (struct si_info *)sih;
	cc = sii->icbus->drv_cc.core;

	
	if (mask || val) {
		bcma_maskset32(cc, regoff, ~mask, val);
	}

	
	w = bcma_read32(cc, regoff);

	return w;
}

static uint ai_slowclk_src(struct si_pub *sih, struct bcma_device *cc)
{
	struct si_info *sii;
	u32 val;

	sii = (struct si_info *)sih;
	if (ai_get_ccrev(&sii->pub) < 6) {
		pci_read_config_dword(sii->pcibus, PCI_GPIO_OUT,
				      &val);
		if (val & PCI_CFG_GPIO_SCS)
			return SCC_SS_PCI;
		return SCC_SS_XTAL;
	} else if (ai_get_ccrev(&sii->pub) < 10) {
		return bcma_read32(cc, CHIPCREGOFFS(slow_clk_ctl)) &
		       SCC_SS_MASK;
	} else			
		return SCC_SS_XTAL;
}

static uint ai_slowclk_freq(struct si_pub *sih, bool max_freq,
			    struct bcma_device *cc)
{
	u32 slowclk;
	uint div;

	slowclk = ai_slowclk_src(sih, cc);
	if (ai_get_ccrev(sih) < 6) {
		if (slowclk == SCC_SS_PCI)
			return max_freq ? (PCIMAXFREQ / 64)
				: (PCIMINFREQ / 64);
		else
			return max_freq ? (XTALMAXFREQ / 32)
				: (XTALMINFREQ / 32);
	} else if (ai_get_ccrev(sih) < 10) {
		div = 4 *
		    (((bcma_read32(cc, CHIPCREGOFFS(slow_clk_ctl)) &
		      SCC_CD_MASK) >> SCC_CD_SHIFT) + 1);
		if (slowclk == SCC_SS_LPO)
			return max_freq ? LPOMAXFREQ : LPOMINFREQ;
		else if (slowclk == SCC_SS_XTAL)
			return max_freq ? (XTALMAXFREQ / div)
				: (XTALMINFREQ / div);
		else if (slowclk == SCC_SS_PCI)
			return max_freq ? (PCIMAXFREQ / div)
				: (PCIMINFREQ / div);
	} else {
		
		div = bcma_read32(cc, CHIPCREGOFFS(system_clk_ctl));
		div = 4 * ((div >> SYCC_CD_SHIFT) + 1);
		return max_freq ? XTALMAXFREQ : (XTALMINFREQ / div);
	}
	return 0;
}

static void
ai_clkctl_setdelay(struct si_pub *sih, struct bcma_device *cc)
{
	uint slowmaxfreq, pll_delay, slowclk;
	uint pll_on_delay, fref_sel_delay;

	pll_delay = PLL_DELAY;


	slowclk = ai_slowclk_src(sih, cc);
	if (slowclk != SCC_SS_XTAL)
		pll_delay += XTAL_ON_DELAY;

	
	slowmaxfreq =
	    ai_slowclk_freq(sih,
			    (ai_get_ccrev(sih) >= 10) ? false : true, cc);

	pll_on_delay = ((slowmaxfreq * pll_delay) + 999999) / 1000000;
	fref_sel_delay = ((slowmaxfreq * FREF_DELAY) + 999999) / 1000000;

	bcma_write32(cc, CHIPCREGOFFS(pll_on_delay), pll_on_delay);
	bcma_write32(cc, CHIPCREGOFFS(fref_sel_delay), fref_sel_delay);
}

void ai_clkctl_init(struct si_pub *sih)
{
	struct bcma_device *cc;

	if (!(ai_get_cccaps(sih) & CC_CAP_PWR_CTL))
		return;

	cc = ai_findcore(sih, BCMA_CORE_CHIPCOMMON, 0);
	if (cc == NULL)
		return;

	
	if (ai_get_ccrev(sih) >= 10)
		bcma_maskset32(cc, CHIPCREGOFFS(system_clk_ctl), SYCC_CD_MASK,
			       (ILP_DIV_1MHZ << SYCC_CD_SHIFT));

	ai_clkctl_setdelay(sih, cc);
}

u16 ai_clkctl_fast_pwrup_delay(struct si_pub *sih)
{
	struct si_info *sii;
	struct bcma_device *cc;
	uint slowminfreq;
	u16 fpdelay;

	sii = (struct si_info *)sih;
	if (ai_get_cccaps(sih) & CC_CAP_PMU) {
		fpdelay = si_pmu_fast_pwrup_delay(sih);
		return fpdelay;
	}

	if (!(ai_get_cccaps(sih) & CC_CAP_PWR_CTL))
		return 0;

	fpdelay = 0;
	cc = ai_findcore(sih, CC_CORE_ID, 0);
	if (cc) {
		slowminfreq = ai_slowclk_freq(sih, false, cc);
		fpdelay = (((bcma_read32(cc, CHIPCREGOFFS(pll_on_delay)) + 2)
			    * 1000000) + (slowminfreq - 1)) / slowminfreq;
	}
	return fpdelay;
}

int ai_clkctl_xtal(struct si_pub *sih, uint what, bool on)
{
	struct si_info *sii;
	u32 in, out, outen;

	sii = (struct si_info *)sih;

	
	if (PCIE(sih))
		return -1;

	pci_read_config_dword(sii->pcibus, PCI_GPIO_IN, &in);
	pci_read_config_dword(sii->pcibus, PCI_GPIO_OUT, &out);
	pci_read_config_dword(sii->pcibus, PCI_GPIO_OUTEN, &outen);

	if (on && (in & PCI_CFG_GPIO_XTAL))
		return 0;

	if (what & XTAL)
		outen |= PCI_CFG_GPIO_XTAL;
	if (what & PLL)
		outen |= PCI_CFG_GPIO_PLL;

	if (on) {
		
		if (what & XTAL) {
			out |= PCI_CFG_GPIO_XTAL;
			if (what & PLL)
				out |= PCI_CFG_GPIO_PLL;
			pci_write_config_dword(sii->pcibus,
					       PCI_GPIO_OUT, out);
			pci_write_config_dword(sii->pcibus,
					       PCI_GPIO_OUTEN, outen);
			udelay(XTAL_ON_DELAY);
		}

		
		if (what & PLL) {
			out &= ~PCI_CFG_GPIO_PLL;
			pci_write_config_dword(sii->pcibus,
					       PCI_GPIO_OUT, out);
			mdelay(2);
		}
	} else {
		if (what & XTAL)
			out &= ~PCI_CFG_GPIO_XTAL;
		if (what & PLL)
			out |= PCI_CFG_GPIO_PLL;
		pci_write_config_dword(sii->pcibus,
				       PCI_GPIO_OUT, out);
		pci_write_config_dword(sii->pcibus,
				       PCI_GPIO_OUTEN, outen);
	}

	return 0;
}

static bool _ai_clkctl_cc(struct si_info *sii, uint mode)
{
	struct bcma_device *cc;
	u32 scc;

	
	if (ai_get_ccrev(&sii->pub) < 6)
		return false;

	cc = ai_findcore(&sii->pub, BCMA_CORE_CHIPCOMMON, 0);

	if (!(ai_get_cccaps(&sii->pub) & CC_CAP_PWR_CTL) &&
	    (ai_get_ccrev(&sii->pub) < 20))
		return mode == CLK_FAST;

	switch (mode) {
	case CLK_FAST:		
		if (ai_get_ccrev(&sii->pub) < 10) {
			ai_clkctl_xtal(&sii->pub, XTAL, ON);
			bcma_maskset32(cc, CHIPCREGOFFS(slow_clk_ctl),
				       (SCC_XC | SCC_FS | SCC_IP), SCC_IP);
		} else if (ai_get_ccrev(&sii->pub) < 20) {
			bcma_set32(cc, CHIPCREGOFFS(system_clk_ctl), SYCC_HR);
		} else {
			bcma_set32(cc, CHIPCREGOFFS(clk_ctl_st), CCS_FORCEHT);
		}

		
		if (ai_get_cccaps(&sii->pub) & CC_CAP_PMU) {
			u32 htavail = CCS_HTAVAIL;
			SPINWAIT(((bcma_read32(cc, CHIPCREGOFFS(clk_ctl_st)) &
				   htavail) == 0), PMU_MAX_TRANSITION_DLY);
		} else {
			udelay(PLL_DELAY);
		}
		break;

	case CLK_DYNAMIC:	
		if (ai_get_ccrev(&sii->pub) < 10) {
			scc = bcma_read32(cc, CHIPCREGOFFS(slow_clk_ctl));
			scc &= ~(SCC_FS | SCC_IP | SCC_XC);
			if ((scc & SCC_SS_MASK) != SCC_SS_XTAL)
				scc |= SCC_XC;
			bcma_write32(cc, CHIPCREGOFFS(slow_clk_ctl), scc);

			if (scc & SCC_XC)
				ai_clkctl_xtal(&sii->pub, XTAL, OFF);
		} else if (ai_get_ccrev(&sii->pub) < 20) {
			
			bcma_mask32(cc, CHIPCREGOFFS(system_clk_ctl), ~SYCC_HR);
		} else {
			bcma_mask32(cc, CHIPCREGOFFS(clk_ctl_st), ~CCS_FORCEHT);
		}
		break;

	default:
		break;
	}

	return mode == CLK_FAST;
}

bool ai_clkctl_cc(struct si_pub *sih, uint mode)
{
	struct si_info *sii;

	sii = (struct si_info *)sih;

	
	if (ai_get_ccrev(sih) < 6)
		return false;

	if (PCI_FORCEHT(sih))
		return mode == CLK_FAST;

	return _ai_clkctl_cc(sii, mode);
}

void ai_pci_up(struct si_pub *sih)
{
	struct si_info *sii;

	sii = (struct si_info *)sih;

	if (PCI_FORCEHT(sih))
		_ai_clkctl_cc(sii, CLK_FAST);

	if (PCIE(sih))
		pcicore_up(sii->pch, SI_PCIUP);

}

void ai_pci_sleep(struct si_pub *sih)
{
	struct si_info *sii;

	sii = (struct si_info *)sih;

	pcicore_sleep(sii->pch);
}

void ai_pci_down(struct si_pub *sih)
{
	struct si_info *sii;

	sii = (struct si_info *)sih;

	
	if (PCI_FORCEHT(sih))
		_ai_clkctl_cc(sii, CLK_DYNAMIC);

	pcicore_down(sii->pch, SI_PCIDOWN);
}

void ai_pci_setup(struct si_pub *sih, uint coremask)
{
	struct si_info *sii;
	u32 w;

	sii = (struct si_info *)sih;

	if (PCIE(sih) || (PCI(sih) && (ai_get_buscorerev(sih) >= 6))) {
		
		pci_read_config_dword(sii->pcibus, PCI_INT_MASK, &w);
		w |= (coremask << PCI_SBIM_SHIFT);
		pci_write_config_dword(sii->pcibus, PCI_INT_MASK, w);
	}

	if (PCI(sih)) {
		pcicore_pci_setup(sii->pch);
	}
}

int ai_pci_fixcfg(struct si_pub *sih)
{
	struct si_info *sii = (struct si_info *)sih;

	
	
	pcicore_fixcfg(sii->pch);
	pcicore_hwup(sii->pch);
	return 0;
}

u32 ai_gpiocontrol(struct si_pub *sih, u32 mask, u32 val, u8 priority)
{
	uint regoff;

	regoff = offsetof(struct chipcregs, gpiocontrol);
	return ai_cc_reg(sih, regoff, mask, val);
}

void ai_chipcontrl_epa4331(struct si_pub *sih, bool on)
{
	struct bcma_device *cc;
	u32 val;

	cc = ai_findcore(sih, CC_CORE_ID, 0);

	if (on) {
		if (ai_get_chippkg(sih) == 9 || ai_get_chippkg(sih) == 0xb)
			
			bcma_set32(cc, CHIPCREGOFFS(chipcontrol),
				   CCTRL4331_EXTPA_EN |
				   CCTRL4331_EXTPA_ON_GPIO2_5);
		else
			
			bcma_set32(cc, CHIPCREGOFFS(chipcontrol),
				   CCTRL4331_EXTPA_EN);
	} else {
		val &= ~(CCTRL4331_EXTPA_EN | CCTRL4331_EXTPA_ON_GPIO2_5);
		bcma_mask32(cc, CHIPCREGOFFS(chipcontrol),
			    ~(CCTRL4331_EXTPA_EN | CCTRL4331_EXTPA_ON_GPIO2_5));
	}
}

void ai_epa_4313war(struct si_pub *sih)
{
	struct bcma_device *cc;

	cc = ai_findcore(sih, CC_CORE_ID, 0);

	
	bcma_set32(cc, CHIPCREGOFFS(gpiocontrol), GPIO_CTRL_EPA_EN_MASK);
}

bool ai_deviceremoved(struct si_pub *sih)
{
	u32 w;
	struct si_info *sii;

	sii = (struct si_info *)sih;

	pci_read_config_dword(sii->pcibus, PCI_VENDOR_ID, &w);
	if ((w & 0xFFFF) != PCI_VENDOR_ID_BROADCOM)
		return true;

	return false;
}

bool ai_is_sprom_available(struct si_pub *sih)
{
	struct si_info *sii = (struct si_info *)sih;

	if (ai_get_ccrev(sih) >= 31) {
		struct bcma_device *cc;
		u32 sromctrl;

		if ((ai_get_cccaps(sih) & CC_CAP_SROM) == 0)
			return false;

		cc = ai_findcore(sih, BCMA_CORE_CHIPCOMMON, 0);
		sromctrl = bcma_read32(cc, CHIPCREGOFFS(sromcontrol));
		return sromctrl & SRC_PRESENT;
	}

	switch (ai_get_chip_id(sih)) {
	case BCM4313_CHIP_ID:
		return (sii->chipst & CST4313_SPROM_PRESENT) != 0;
	default:
		return true;
	}
}

bool ai_is_otp_disabled(struct si_pub *sih)
{
	struct si_info *sii = (struct si_info *)sih;

	switch (ai_get_chip_id(sih)) {
	case BCM4313_CHIP_ID:
		return (sii->chipst & CST4313_OTP_PRESENT) == 0;
		
	case BCM43224_CHIP_ID:
	case BCM43225_CHIP_ID:
	default:
		return false;
	}
}

uint ai_get_buscoretype(struct si_pub *sih)
{
	struct si_info *sii = (struct si_info *)sih;
	return sii->buscore->id.id;
}

uint ai_get_buscorerev(struct si_pub *sih)
{
	struct si_info *sii = (struct si_info *)sih;
	return sii->buscore->id.rev;
}
