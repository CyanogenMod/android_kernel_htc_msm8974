#ifndef ____ASM_ARCH_SDRC_H
#define ____ASM_ARCH_SDRC_H

/*
 * OMAP2/3 SDRC/SMS register definitions
 *
 * Copyright (C) 2007-2008 Texas Instruments, Inc.
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Tony Lindgren
 * Paul Walmsley
 * Richard Woodruff
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#define SDRC_SYSCONFIG		0x010
#define SDRC_CS_CFG		0x040
#define SDRC_SHARING		0x044
#define SDRC_ERR_TYPE		0x04C
#define SDRC_DLLA_CTRL		0x060
#define SDRC_DLLA_STATUS	0x064
#define SDRC_DLLB_CTRL		0x068
#define SDRC_DLLB_STATUS	0x06C
#define SDRC_POWER		0x070
#define SDRC_MCFG_0		0x080
#define SDRC_MR_0		0x084
#define SDRC_EMR2_0		0x08c
#define SDRC_ACTIM_CTRL_A_0	0x09c
#define SDRC_ACTIM_CTRL_B_0	0x0a0
#define SDRC_RFR_CTRL_0		0x0a4
#define SDRC_MANUAL_0		0x0a8
#define SDRC_MCFG_1		0x0B0
#define SDRC_MR_1		0x0B4
#define SDRC_EMR2_1		0x0BC
#define SDRC_ACTIM_CTRL_A_1	0x0C4
#define SDRC_ACTIM_CTRL_B_1	0x0C8
#define SDRC_RFR_CTRL_1		0x0D4
#define SDRC_MANUAL_1		0x0D8

#define SDRC_POWER_AUTOCOUNT_SHIFT	8
#define SDRC_POWER_AUTOCOUNT_MASK	(0xffff << SDRC_POWER_AUTOCOUNT_SHIFT)
#define SDRC_POWER_CLKCTRL_SHIFT	4
#define SDRC_POWER_CLKCTRL_MASK		(0x3 << SDRC_POWER_CLKCTRL_SHIFT)
#define SDRC_SELF_REFRESH_ON_AUTOCOUNT	(0x2 << SDRC_POWER_CLKCTRL_SHIFT)

#define SDRC_RFR_CTRL_165MHz	(0x00044c00 | 1)
#define SDRC_RFR_CTRL_133MHz	(0x0003de00 | 1)
#define SDRC_RFR_CTRL_100MHz	(0x0002da01 | 1)
#define SDRC_RFR_CTRL_110MHz	(0x0002da01 | 1) 
#define SDRC_RFR_CTRL_BYPASS	(0x00005000 | 1) 



#define OMAP242X_SMS_REGADDR(reg)					\
		(void __iomem *)OMAP2_L3_IO_ADDRESS(OMAP2420_SMS_BASE + reg)
#define OMAP243X_SMS_REGADDR(reg)					\
		(void __iomem *)OMAP2_L3_IO_ADDRESS(OMAP243X_SMS_BASE + reg)
#define OMAP343X_SMS_REGADDR(reg)					\
		(void __iomem *)OMAP2_L3_IO_ADDRESS(OMAP343X_SMS_BASE + reg)


#define SMS_SYSCONFIG			0x010
#define SMS_ROT_CONTROL(context)	(0x180 + 0x10 * context)
#define SMS_ROT_SIZE(context)		(0x184 + 0x10 * context)
#define SMS_ROT_PHYSICAL_BA(context)	(0x188 + 0x10 * context)


#ifndef __ASSEMBLER__

struct omap_sdrc_params {
	unsigned long rate;
	u32 actim_ctrla;
	u32 actim_ctrlb;
	u32 rfr_ctrl;
	u32 mr;
};

#if defined(CONFIG_ARCH_OMAP2) || defined(CONFIG_ARCH_OMAP3)
void omap2_sdrc_init(struct omap_sdrc_params *sdrc_cs0,
			    struct omap_sdrc_params *sdrc_cs1);
#else
static inline void __init omap2_sdrc_init(struct omap_sdrc_params *sdrc_cs0,
					  struct omap_sdrc_params *sdrc_cs1) {};
#endif

int omap2_sdrc_get_params(unsigned long r,
			  struct omap_sdrc_params **sdrc_cs0,
			  struct omap_sdrc_params **sdrc_cs1);
void omap2_sms_save_context(void);
void omap2_sms_restore_context(void);

void omap2_sms_write_rot_control(u32 val, unsigned ctx);
void omap2_sms_write_rot_size(u32 val, unsigned ctx);
void omap2_sms_write_rot_physical_ba(u32 val, unsigned ctx);

#ifdef CONFIG_ARCH_OMAP2

struct memory_timings {
	u32 m_type;		
	u32 dll_mode;		
	u32 slow_dll_ctrl;	
	u32 fast_dll_ctrl;	
	u32 base_cs;		
};

extern void omap2xxx_sdrc_init_params(u32 force_lock_to_unlock_mode);
struct omap_sdrc_params *rx51_get_sdram_timings(void);

u32 omap2xxx_sdrc_dll_is_unlocked(void);
u32 omap2xxx_sdrc_reprogram(u32 level, u32 force);

#endif  

#endif  

#endif
