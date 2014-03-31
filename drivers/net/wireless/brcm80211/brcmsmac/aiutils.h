/*
 * Copyright (c) 2011 Broadcom Corporation
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
 */

#ifndef	_BRCM_AIUTILS_H_
#define	_BRCM_AIUTILS_H_

#include <linux/bcma/bcma.h>

#include "types.h"

#define SI_CORE_SIZE		0x1000
#define	SI_MAXCORES		16

#define SI_PCI_DMA_SZ		0x40000000

#define SI_PCIE_DMA_H32		0x80000000

#define	SI_CC_IDX		0

#define	SOCI_AI			1

#define SI_CLK_CTL_ST		0x1e0	

#define	CCS_FORCEALP		0x00000001	
#define	CCS_FORCEHT		0x00000002	
#define	CCS_FORCEILP		0x00000004	
#define	CCS_ALPAREQ		0x00000008	
#define	CCS_HTAREQ		0x00000010	
#define	CCS_FORCEHWREQOFF	0x00000020	
#define CCS_ERSRC_REQ_MASK	0x00000700	
#define CCS_ERSRC_REQ_SHIFT	8
#define	CCS_ALPAVAIL		0x00010000	
#define	CCS_HTAVAIL		0x00020000	
#define CCS_BP_ON_APL		0x00040000	
#define CCS_BP_ON_HT		0x00080000	
#define CCS_ERSRC_STS_MASK	0x07000000	
#define CCS_ERSRC_STS_SHIFT	24

#define	CCS0_HTAVAIL		0x00010000
#define	CCS0_ALPAVAIL		0x00020000


#define FLASH_MIN		0x00020000	

#define	CC_SROM_OTP		0x800	

#define GPIO_ONTIME_SHIFT	16

#define	CLKD_OTP		0x000f0000
#define	CLKD_OTP_SHIFT		16

#define	BCM4717_PKG_ID		9	
#define	BCM4718_PKG_ID		10	
#define BCM43224_FAB_SMIC	0xa	

#define	BCM4716_CHIP_ID		0x4716	
#define	BCM47162_CHIP_ID	47162	
#define	BCM4748_CHIP_ID		0x4748	

#define	LPOMINFREQ		25000	
#define	LPOMAXFREQ		43000	
#define	XTALMINFREQ		19800000	
#define	XTALMAXFREQ		20200000	
#define	PCIMINFREQ		25000000	
#define	PCIMAXFREQ		34000000	

#define	ILP_DIV_5MHZ		0	
#define	ILP_DIV_1MHZ		4	

#define	XTAL			0x1	
#define	PLL			0x2	

#define	CLK_FAST		0	
#define	CLK_DYNAMIC		2	

#define GPIO_DRV_PRIORITY	0	
#define GPIO_APP_PRIORITY	1	
#define GPIO_HI_PRIORITY	2	

#define GPIO_PULLUP		0
#define GPIO_PULLDN		1

#define GPIO_REGEVT		0	
#define GPIO_REGEVT_INTMSK	1	
#define GPIO_REGEVT_INTPOL	2	

#define SI_DEVPATH_BUFSZ	16	

#define	SI_DOATTACH	1
#define SI_PCIDOWN	2
#define SI_PCIUP	3

struct si_pub {
	int ccrev;		
	u32 cccaps;		
	int pmurev;		
	u32 pmucaps;		
	uint boardtype;		
	uint boardvendor;	
	uint chip;		
	uint chiprev;		
	uint chippkg;		
};

struct pci_dev;

struct gpioh_item {
	void *arg;
	bool level;
	void (*handler) (u32 stat, void *arg);
	u32 event;
	struct gpioh_item *next;
};

struct si_info {
	struct si_pub pub;	
	struct bcma_bus *icbus;	
	struct pci_dev *pcibus;	
	struct pcicore_info *pch; 
	struct bcma_device *buscore;
	struct list_head var_list; 

	u32 chipst;		
};



extern struct bcma_device *ai_findcore(struct si_pub *sih,
				       u16 coreid, u16 coreunit);
extern u32 ai_core_cflags(struct bcma_device *core, u32 mask, u32 val);

extern struct si_pub *ai_attach(struct bcma_bus *pbus);
extern void ai_detach(struct si_pub *sih);
extern uint ai_cc_reg(struct si_pub *sih, uint regoff, u32 mask, u32 val);
extern void ai_pci_setup(struct si_pub *sih, uint coremask);
extern void ai_clkctl_init(struct si_pub *sih);
extern u16 ai_clkctl_fast_pwrup_delay(struct si_pub *sih);
extern bool ai_clkctl_cc(struct si_pub *sih, uint mode);
extern int ai_clkctl_xtal(struct si_pub *sih, uint what, bool on);
extern bool ai_deviceremoved(struct si_pub *sih);
extern u32 ai_gpiocontrol(struct si_pub *sih, u32 mask, u32 val,
			     u8 priority);

extern bool ai_is_otp_disabled(struct si_pub *sih);

extern bool ai_is_sprom_available(struct si_pub *sih);

extern void ai_pci_sleep(struct si_pub *sih);
extern void ai_pci_down(struct si_pub *sih);
extern void ai_pci_up(struct si_pub *sih);
extern int ai_pci_fixcfg(struct si_pub *sih);

extern void ai_chipcontrl_epa4331(struct si_pub *sih, bool on);
extern void ai_epa_4313war(struct si_pub *sih);

extern uint ai_get_buscoretype(struct si_pub *sih);
extern uint ai_get_buscorerev(struct si_pub *sih);

static inline int ai_get_ccrev(struct si_pub *sih)
{
	return sih->ccrev;
}

static inline u32 ai_get_cccaps(struct si_pub *sih)
{
	return sih->cccaps;
}

static inline int ai_get_pmurev(struct si_pub *sih)
{
	return sih->pmurev;
}

static inline u32 ai_get_pmucaps(struct si_pub *sih)
{
	return sih->pmucaps;
}

static inline uint ai_get_boardtype(struct si_pub *sih)
{
	return sih->boardtype;
}

static inline uint ai_get_boardvendor(struct si_pub *sih)
{
	return sih->boardvendor;
}

static inline uint ai_get_chip_id(struct si_pub *sih)
{
	return sih->chip;
}

static inline uint ai_get_chiprev(struct si_pub *sih)
{
	return sih->chiprev;
}

static inline uint ai_get_chippkg(struct si_pub *sih)
{
	return sih->chippkg;
}

#endif				
