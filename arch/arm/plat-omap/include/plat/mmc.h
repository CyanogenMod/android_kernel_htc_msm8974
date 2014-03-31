/*
 * MMC definitions for OMAP2
 *
 * Copyright (C) 2006 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OMAP2_MMC_H
#define __OMAP2_MMC_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/mmc/host.h>

#include <plat/board.h>

#define OMAP15XX_NR_MMC		1
#define OMAP16XX_NR_MMC		2
#define OMAP1_MMC_SIZE		0x080
#define OMAP1_MMC1_BASE		0xfffb7800
#define OMAP1_MMC2_BASE		0xfffb7c00	

#define OMAP24XX_NR_MMC		2
#define OMAP2420_MMC_SIZE	OMAP1_MMC_SIZE
#define OMAP2_MMC1_BASE		0x4809c000

#define OMAP4_MMC_REG_OFFSET	0x100

#define OMAP_MMC_MAX_SLOTS	2

#define OMAP_HSMMC_SUPPORTS_DUAL_VOLT		BIT(0)
#define OMAP_HSMMC_BROKEN_MULTIBLOCK_READ	BIT(1)

struct omap_mmc_dev_attr {
	u8 flags;
};

struct omap_mmc_platform_data {
	
	struct device *dev;

	
	unsigned nr_slots:2;

	unsigned int max_freq;

	
	int (*switch_slot)(struct device *dev, int slot);
	int (*init)(struct device *dev);
	void (*cleanup)(struct device *dev);
	void (*shutdown)(struct device *dev);

	
	int (*suspend)(struct device *dev, int slot);
	int (*resume)(struct device *dev, int slot);

	
	int (*get_context_loss_count)(struct device *dev);

	u64 dma_mask;

	
	u8 controller_flags;

	
	u16 reg_offset;

	struct omap_mmc_slot_data {

		u8  wires;	
		u32 caps;	
		u32 pm_caps;	

		unsigned nomux:1;

		
		unsigned cover:1;

		
		unsigned internal_clock:1;

		
		unsigned nonremovable:1;

		
		unsigned power_saving:1;

		
		unsigned no_off:1;

		
		unsigned no_regulator_off_init:1;

		
		unsigned vcc_aux_disable_is_sleep:1;

		
#define HSMMC_HAS_PBIAS		(1 << 0)
#define HSMMC_HAS_UPDATED_RESET	(1 << 1)
		unsigned features;

		int switch_pin;			
		int gpio_wp;			

		int (*set_bus_mode)(struct device *dev, int slot, int bus_mode);
		int (*set_power)(struct device *dev, int slot,
				 int power_on, int vdd);
		int (*get_ro)(struct device *dev, int slot);
		void (*remux)(struct device *dev, int slot, int power_on);
		
		void (*before_set_reg)(struct device *dev, int slot,
				       int power_on, int vdd);
		
		void (*after_set_reg)(struct device *dev, int slot,
				      int power_on, int vdd);
		
		void (*init_card)(struct mmc_card *card);

		int (*get_cover_state)(struct device *dev, int slot);

		const char *name;
		u32 ocr_mask;

		
		int card_detect_irq;
		int (*card_detect)(struct device *dev, int slot);

		unsigned int ban_openended:1;

	} slots[OMAP_MMC_MAX_SLOTS];
};

extern void omap_mmc_notify_cover_event(struct device *dev, int slot,
					int is_closed);

#if	defined(CONFIG_MMC_OMAP) || defined(CONFIG_MMC_OMAP_MODULE) || \
	defined(CONFIG_MMC_OMAP_HS) || defined(CONFIG_MMC_OMAP_HS_MODULE)
void omap1_init_mmc(struct omap_mmc_platform_data **mmc_data,
				int nr_controllers);
void omap242x_init_mmc(struct omap_mmc_platform_data **mmc_data);
int omap_mmc_add(const char *name, int id, unsigned long base,
				unsigned long size, unsigned int irq,
				struct omap_mmc_platform_data *data);
#else
static inline void omap1_init_mmc(struct omap_mmc_platform_data **mmc_data,
				int nr_controllers)
{
}
static inline void omap242x_init_mmc(struct omap_mmc_platform_data **mmc_data)
{
}
static inline int omap_mmc_add(const char *name, int id, unsigned long base,
				unsigned long size, unsigned int irq,
				struct omap_mmc_platform_data *data)
{
	return 0;
}

#endif
#endif
