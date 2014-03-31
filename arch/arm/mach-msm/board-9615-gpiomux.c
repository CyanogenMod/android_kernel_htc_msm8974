/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/gpio.h>
#include <mach/gpiomux.h>
#include <mach/board.h>
#include "board-9615.h"

static struct gpiomux_setting ps_hold = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting slimbus = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_KEEPER,
};

static struct gpiomux_setting gsbi4 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi5 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi3 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi3_cs1_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

#ifdef CONFIG_LTC4088_CHARGER
static struct gpiomux_setting ltc4088_chg_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
#endif

static struct gpiomux_setting sdcc2_clk_actv_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdcc2_cmd_data_0_3_actv_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdcc2_suspend_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting cdc_mclk = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

#ifdef CONFIG_FB_MSM_EBI2
static struct gpiomux_setting ebi2_lcdc_a_d = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ebi2_lcdc_cs = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting ebi2_lcdc_rs = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif

static struct gpiomux_setting wlan_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting wlan_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting tabla_reset = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config msm9615_audio_codec_configs[] __initdata = {
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_SUSPENDED] = &cdc_mclk,
		},
	},
	{
		.gpio	= 84,		
		.settings = {
			[GPIOMUX_SUSPENDED] = &tabla_reset,
		},
	}
};

static struct msm_gpiomux_config msm9615_sdcc2_configs[] __initdata = {
	{
		
		.gpio      = 25,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_cfg,
		},
	},
	{
		
		.gpio      = 26,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_cmd_data_0_3_actv_cfg,
		},
	},
	{
		
		.gpio      = 27,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_cfg,
		},
	},
	{
		
		.gpio      = 28,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_cfg,
		},
	},
	{
		
		.gpio      = 29,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_cfg,
		},
	},
	{
		
		.gpio      = 30,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_clk_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_cfg,
		},
	},
};

struct msm_gpiomux_config msm9615_ps_hold_config[] __initdata = {
	{
		.gpio = 83,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ps_hold,
		},
	},
};

static struct gpiomux_setting sd_card_det = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

struct msm_gpiomux_config sd_card_det_config[] __initdata = {
	{
		.gpio = 80,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sd_card_det,
			[GPIOMUX_SUSPENDED] = &sd_card_det,
		},
	},
};

#ifdef CONFIG_LTC4088_CHARGER
static struct msm_gpiomux_config
	msm9615_ltc4088_charger_config[] __initdata = {
	{
		.gpio = 4,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ltc4088_chg_cfg,
		},
	},
	{
		.gpio = 6,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ltc4088_chg_cfg,
		},
	},
	{
		.gpio = 7,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ltc4088_chg_cfg,
		},
	},
};
#endif

struct msm_gpiomux_config msm9615_gsbi_configs[] __initdata = {
	{
		.gpio      = 8,		
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3,
		},
	},
	{
		.gpio      = 9,		
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3,
		},
	},
	{
		.gpio      = 10,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3,
		},
	},
	{
		.gpio      = 11,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3,
		},
	},
	{
		.gpio      = 12,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 13,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 14,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 15,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 16,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5,
			[GPIOMUX_ACTIVE] = &gsbi5,
		},
	},
	{
		.gpio      = 17,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5,
		},
	},
	{
		
		.gpio      = 19,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3_cs1_config,
		},
	},
};

static struct msm_gpiomux_config msm9615_slimbus_configs[] __initdata = {
	{
		.gpio      = 20,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
	{
		.gpio      = 23,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
};

#ifdef CONFIG_FB_MSM_EBI2
static struct msm_gpiomux_config msm9615_ebi2_lcdc_configs[] __initdata = {
	{
		.gpio      = 21,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_lcdc_a_d,
		},
	},
	{
		.gpio      = 22,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_lcdc_cs,
		},
	},
	{
		.gpio      = 24,	
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_lcdc_rs,
		},
	},
};
#endif

static struct msm_gpiomux_config msm9615_wlan_configs[] __initdata = {
	{
		.gpio      = 21,
		.settings = {
			[GPIOMUX_ACTIVE] = &wlan_active_config,
			[GPIOMUX_SUSPENDED] = &wlan_suspend_config,
		},
	},
};


int __init msm9615_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc) {
		pr_err(KERN_ERR "msm_gpiomux_init failed %d\n", rc);
		return rc;
	}
	msm_gpiomux_install(msm9615_gsbi_configs,
			ARRAY_SIZE(msm9615_gsbi_configs));

	msm_gpiomux_install(msm9615_slimbus_configs,
			ARRAY_SIZE(msm9615_slimbus_configs));

	msm_gpiomux_install(msm9615_ps_hold_config,
			ARRAY_SIZE(msm9615_ps_hold_config));
	msm_gpiomux_install(sd_card_det_config,
			ARRAY_SIZE(sd_card_det_config));
	msm_gpiomux_install(msm9615_sdcc2_configs,
			ARRAY_SIZE(msm9615_sdcc2_configs));
#ifdef CONFIG_LTC4088_CHARGER
	msm_gpiomux_install(msm9615_ltc4088_charger_config,
			ARRAY_SIZE(msm9615_ltc4088_charger_config));
#endif
	msm_gpiomux_install(msm9615_audio_codec_configs,
			ARRAY_SIZE(msm9615_audio_codec_configs));

	msm_gpiomux_install(msm9615_wlan_configs,
			ARRAY_SIZE(msm9615_wlan_configs));

#ifdef CONFIG_FB_MSM_EBI2
	msm_gpiomux_install(msm9615_ebi2_lcdc_configs,
			ARRAY_SIZE(msm9615_ebi2_lcdc_configs));
#endif

	return 0;
}
