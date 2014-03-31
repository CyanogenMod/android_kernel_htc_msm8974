/*
 * MMC definitions for OMAP2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct mmc_card;

struct omap2_hsmmc_info {
	u8	mmc;		
	u32	caps;		
	u32	pm_caps;	
	bool	transceiver;	
	bool	ext_clock;	
	bool	cover_only;	
	bool	nonremovable;	
	bool	power_saving;	
	bool	no_off;		
	bool	no_off_init;	
	bool	vcc_aux_disable_is_sleep; 
	bool	deferred;	
	int	gpio_cd;	
	int	gpio_wp;	
	char	*name;		
	struct platform_device *pdev;	
	int	ocr_mask;	
	int	max_freq;	
	
	void (*remux)(struct device *dev, int slot, int power_on);
	
	void (*init_card)(struct mmc_card *card);
};

#if defined(CONFIG_MMC_OMAP_HS) || defined(CONFIG_MMC_OMAP_HS_MODULE)

void omap_hsmmc_init(struct omap2_hsmmc_info *);
void omap_hsmmc_late_init(struct omap2_hsmmc_info *);

#else

static inline void omap_hsmmc_init(struct omap2_hsmmc_info *info)
{
}

static inline void omap_hsmmc_late_init(struct omap2_hsmmc_info *info)
{
}

#endif
