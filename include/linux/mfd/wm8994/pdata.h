/*
 * include/linux/mfd/wm8994/pdata.h -- Platform data for WM8994
 *
 * Copyright 2009 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __MFD_WM8994_PDATA_H__
#define __MFD_WM8994_PDATA_H__

#define WM8994_NUM_LDO   2
#define WM8994_NUM_GPIO 11

struct wm8994_ldo_pdata {
	
	int enable;

	const struct regulator_init_data *init_data;
};

#define WM8994_CONFIGURE_GPIO 0x10000

#define WM8994_DRC_REGS 5
#define WM8994_EQ_REGS  20
#define WM8958_MBC_CUTOFF_REGS 20
#define WM8958_MBC_COEFF_REGS  48
#define WM8958_MBC_COMBINED_REGS 56
#define WM8958_VSS_HPF_REGS 2
#define WM8958_VSS_REGS 148
#define WM8958_ENH_EQ_REGS 32

struct wm8994_drc_cfg {
        const char *name;
        u16 regs[WM8994_DRC_REGS];
};

struct wm8994_retune_mobile_cfg {
        const char *name;
        unsigned int rate;
        u16 regs[WM8994_EQ_REGS];
};

struct wm8958_mbc_cfg {
	const char *name;
	u16 cutoff_regs[WM8958_MBC_CUTOFF_REGS];
	u16 coeff_regs[WM8958_MBC_COEFF_REGS];

	
	u16 combined_regs[WM8958_MBC_COMBINED_REGS];
};

struct wm8958_vss_hpf_cfg {
	const char *name;
	u16 regs[WM8958_VSS_HPF_REGS];
};

struct wm8958_vss_cfg {
	const char *name;
	u16 regs[WM8958_VSS_REGS];
};

struct wm8958_enh_eq_cfg {
	const char *name;
	u16 regs[WM8958_ENH_EQ_REGS];
};

struct wm8958_micd_rate {
	int sysclk;
	bool idle;
	int start;
	int rate;
};

struct wm8994_pdata {
	int gpio_base;

	int gpio_defaults[WM8994_NUM_GPIO];

	struct wm8994_ldo_pdata ldo[WM8994_NUM_LDO];

	int irq_base;  

        int num_drc_cfgs;
        struct wm8994_drc_cfg *drc_cfgs;

        int num_retune_mobile_cfgs;
        struct wm8994_retune_mobile_cfg *retune_mobile_cfgs;

	int num_mbc_cfgs;
	struct wm8958_mbc_cfg *mbc_cfgs;

	int num_vss_cfgs;
	struct wm8958_vss_cfg *vss_cfgs;

	int num_vss_hpf_cfgs;
	struct wm8958_vss_hpf_cfg *vss_hpf_cfgs;

	int num_enh_eq_cfgs;
	struct wm8958_enh_eq_cfg *enh_eq_cfgs;

	int num_micd_rates;
	struct wm8958_micd_rate *micd_rates;

        
        unsigned int lineout1_diff:1;
        unsigned int lineout2_diff:1;

        
        unsigned int lineout1fb:1;
        unsigned int lineout2fb:1;

	int micdet_irq;

        
        unsigned int micbias1_lvl:1;
        unsigned int micbias2_lvl:1;

        
        unsigned int jd_scthr:2;
        unsigned int jd_thr:2;

	
	unsigned int jd_ext_cap:1;

	
	int micbias[2];

	
	u16 micd_lvl_sel;

	bool ldo_ena_always_driven;

	bool spkmode_pu;
};

#endif
