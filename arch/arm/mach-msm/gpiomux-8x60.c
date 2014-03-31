/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <mach/irqs.h>
#include <asm/mach-types.h>
#include <mach/gpiomux.h>
#include "gpiomux-8x60.h"

static struct gpiomux_setting console_uart = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting spi_active = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting spi_suspended_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting spi_suspended_cs_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting i2c_active = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting i2c_active_gsbi7 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting i2c_suspended_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi8 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting ps_hold = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting msm_snddev_active_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting msm_snddev_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ebi2_a_d = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting ebi2_oe = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting ebi2_we = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting ebi2_cs2 = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting ebi2_cs3 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

#if defined(CONFIG_USB_PEHCI_HCD) || defined(CONFIG_USB_PEHCI_HCD_MODULE)
static struct gpiomux_setting ebi2_cs4 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};
#endif

static struct gpiomux_setting ebi2_adv = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

#if defined(CONFIG_USB_PEHCI_HCD) || defined(CONFIG_USB_PEHCI_HCD_MODULE)
static struct gpiomux_setting usb_isp1763_actv_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting usb_isp1763_susp_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif

static struct gpiomux_setting sdcc1_dat_0_3_cmd_actv_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdcc1_dat_4_7_cmd_actv_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdcc1_clk_actv_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdcc1_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdcc2_dat_0_3_cmd_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdcc2_dat_4_7_cmd_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdcc2_clk_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdcc2_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdcc5_dat_0_3_cmd_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_10MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdcc5_clk_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdcc5_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting aux_pcm_active_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting aux_pcm_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting uart1dm_active = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting uart1dm_suspended = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mi2s_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting mi2s_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting lcdc_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting lcdc_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdp_vsync_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hdmi_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm2ap_status_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting mdm2ap_status_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting cam_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm2ap_sync_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting mdm2ap_sync_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting tm_active = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting tm_suspended = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting tma_active = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting ts_suspended = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdp_vsync_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting hdmi_active_1_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting hdmi_active_2_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting hdmi_active_3_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting pmic_suspended_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting cam_active_1_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting cam_active_2_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting cam_active_3_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting cam_active_4_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting cam_active_5_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_NONE,
};

#ifdef CONFIG_MSM_GSBI9_UART
static struct gpiomux_setting uart9dm_active = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA ,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gsbi9 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
#endif

static struct gpiomux_setting ap2mdm_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm2ap_status_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting mdm2ap_vfr_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting mdm2ap_vfr_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm2ap_errfatal_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ap2mdm_kpdpwr_n_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};


static struct gpiomux_setting mdm2ap_vddmin_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting mdm2ap_vddmin_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm8x60_gsbi_configs[] __initdata = {
	{
		.gpio      = 33,
		.settings = {
			[GPIOMUX_SUSPENDED] = &spi_suspended_config,
			[GPIOMUX_ACTIVE]    = &spi_active,
		},
	},
	{
		.gpio      = 34,
		.settings = {
			[GPIOMUX_SUSPENDED] = &spi_suspended_config,
			[GPIOMUX_ACTIVE]    = &spi_active,
		},
	},
	{
		.gpio      = 35,
		.settings = {
			[GPIOMUX_SUSPENDED] = &spi_suspended_cs_config,
			[GPIOMUX_ACTIVE]    = &spi_active,
		},
	},
	{
		.gpio      = 36,
		.settings = {
			[GPIOMUX_SUSPENDED] = &spi_suspended_config,
			[GPIOMUX_ACTIVE]    = &spi_active,
		},
	},
	{
		.gpio      = 43,
		.settings = {
			[GPIOMUX_SUSPENDED] = &i2c_suspended_config,
			[GPIOMUX_ACTIVE]    = &i2c_active,
		},
	},
	{
		.gpio      = 44,
		.settings = {
			[GPIOMUX_SUSPENDED] = &i2c_suspended_config,
			[GPIOMUX_ACTIVE]    = &i2c_active,
		},
	},
	{
		.gpio      = 47,
		.settings = {
			[GPIOMUX_SUSPENDED] = &i2c_suspended_config,
			[GPIOMUX_ACTIVE]    = &i2c_active,
		},
	},
	{
		.gpio      = 48,
		.settings = {
			[GPIOMUX_SUSPENDED] = &i2c_suspended_config,
			[GPIOMUX_ACTIVE]    = &i2c_active,
		},
	},
	{
		.gpio      = 59,
		.settings = {
			[GPIOMUX_SUSPENDED] = &i2c_suspended_config,
			[GPIOMUX_ACTIVE]    = &i2c_active_gsbi7,
		},
	},
	{
		.gpio      = 60,
		.settings = {
			[GPIOMUX_SUSPENDED] = &i2c_suspended_config,
			[GPIOMUX_ACTIVE]    = &i2c_active_gsbi7,
		},
	},
	{
		.gpio      = 64,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi8,
		},
	},
	{
		.gpio      = 65,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi8,
		},
	},
};

static struct msm_gpiomux_config msm8x60_fluid_gsbi_configs[] __initdata = {
	{
		.gpio      = 70,
		.settings = {
			[GPIOMUX_SUSPENDED] = &spi_suspended_config,
			[GPIOMUX_ACTIVE]    = &spi_active,
		},
	},
	{
		.gpio      = 72,
		.settings = {
			[GPIOMUX_SUSPENDED] = &spi_suspended_cs_config,
			[GPIOMUX_ACTIVE]    = &spi_active,
		},
	},
	{
		.gpio      = 73,
		.settings = {
			[GPIOMUX_SUSPENDED] = &spi_suspended_config,
			[GPIOMUX_ACTIVE]    = &spi_active,
		},
	},
};

static struct msm_gpiomux_config msm8x60_ebi2_configs[] __initdata = {
	{
		.gpio      = 40,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_cs2,
		},
	},
	{
		.gpio      = 123,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 124,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 125,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 126,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 127,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 128,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 129,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 130,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
#if defined(CONFIG_USB_PEHCI_HCD) || defined(CONFIG_USB_PEHCI_HCD_MODULE)
	
	{
		.gpio      = 132,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_cs4,
		},
	},
#endif
	{
		.gpio      = 133,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_cs3,
		},
	},
	{
		.gpio      = 135,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 136,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 137,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 138,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 139,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 140,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 141,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 142,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 143,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 144,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 145,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 146,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 147,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 148,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 149,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 150,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_a_d,
		},
	},
	{
		.gpio      = 151,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_oe,
		},
	},
	{
		.gpio      = 153,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_adv,
		},
	},
	{
		.gpio      = 157,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ebi2_we,
		},
	},
};

#if defined(CONFIG_USB_PEHCI_HCD) || defined(CONFIG_USB_PEHCI_HCD_MODULE)
static struct msm_gpiomux_config msm8x60_isp_usb_configs[] __initdata = {
	{
		.gpio      = 117,
		.settings = {
			[GPIOMUX_ACTIVE]    = &usb_isp1763_actv_cfg,
			[GPIOMUX_SUSPENDED] = &usb_isp1763_susp_cfg,
		},
	},
	{
		.gpio      = 152,
		.settings = {
			[GPIOMUX_ACTIVE]    = &usb_isp1763_actv_cfg,
			[GPIOMUX_SUSPENDED] = &usb_isp1763_susp_cfg,
		},
	},

};
#endif

static struct msm_gpiomux_config msm8x60_uart_configs[] __initdata = {
	{ 
		.gpio      = 53,
		.settings = {
			[GPIOMUX_ACTIVE]    = &uart1dm_active,
			[GPIOMUX_SUSPENDED] = &uart1dm_suspended,
		},
	},
	{ 
		.gpio      = 54,
		.settings = {
			[GPIOMUX_ACTIVE]    = &uart1dm_active,
			[GPIOMUX_SUSPENDED] = &uart1dm_suspended,
		},
	},
	{ 
		.gpio      = 55,
		.settings = {
			[GPIOMUX_ACTIVE]    = &uart1dm_active,
			[GPIOMUX_SUSPENDED] = &uart1dm_suspended,
		},
	},
	{ 
		.gpio      = 56,
		.settings = {
			[GPIOMUX_ACTIVE]    = &uart1dm_active,
			[GPIOMUX_SUSPENDED] = &uart1dm_suspended,
		},
	},
	{
		.gpio      = 115,
		.settings = {
			[GPIOMUX_SUSPENDED] = &console_uart,
		},
	},
	{
		.gpio      = 116,
		.settings = {
			[GPIOMUX_SUSPENDED] = &console_uart,
		},
	},
#if !defined(CONFIG_USB_PEHCI_HCD) && !defined(CONFIG_USB_PEHCI_HCD_MODULE)
	
	{
		.gpio      = 117,
		.settings = {
			[GPIOMUX_SUSPENDED] = &console_uart,
		},
	},
#endif
	{
		.gpio      = 118,
		.settings = {
			[GPIOMUX_SUSPENDED] = &console_uart,
		},
	},
};

#ifdef CONFIG_MSM_GSBI9_UART
static struct msm_gpiomux_config msm8x60_charm_uart_configs[] __initdata = {
	{ 
		.gpio	   = 66,
		.settings = {
			[GPIOMUX_ACTIVE]    = &uart9dm_active,
			[GPIOMUX_SUSPENDED] = &gsbi9,
		},
	},
	{ 
		.gpio	   = 67,
		.settings = {
			[GPIOMUX_ACTIVE]    = &uart9dm_active,
			[GPIOMUX_SUSPENDED] = &gsbi9,
		},
	},
};
#endif

static struct msm_gpiomux_config msm8x60_ts_configs[] __initdata = {
	{
		
		.gpio = 58,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ts_suspended,
		},
	},
};

static struct msm_gpiomux_config msm8x60_tmg200_configs[] __initdata = {
	{
		.gpio = 61,
		.settings = {
			[GPIOMUX_ACTIVE] = &tm_active,
			[GPIOMUX_SUSPENDED] = &tm_suspended,
		},
	},
};

static struct msm_gpiomux_config msm8x60_tma300_configs[] __initdata = {
	{
		.gpio = 61,
		.settings = {
			[GPIOMUX_ACTIVE]    = &tma_active,
			[GPIOMUX_SUSPENDED] = &tm_suspended,
		},
	},
};

static struct msm_gpiomux_config msm8x60_aux_pcm_configs[] __initdata = {
	{
		.gpio = 111,
		.settings = {
			[GPIOMUX_ACTIVE]    = &aux_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &aux_pcm_suspend_config,
		},
	},
	{
		.gpio = 112,
		.settings = {
			[GPIOMUX_ACTIVE]    = &aux_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &aux_pcm_suspend_config,
		},
	},
	{
		.gpio = 113,
		.settings = {
			[GPIOMUX_ACTIVE]    = &aux_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &aux_pcm_suspend_config,
		},
	},
	{
		.gpio = 114,
		.settings = {
			[GPIOMUX_ACTIVE]    = &aux_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &aux_pcm_suspend_config,
		},
	},
};

static struct msm_gpiomux_config msm8x60_sdc_configs[] __initdata = {
	
	{
		.gpio = 159,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc1_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc1_suspend_config,
		},
	},
	
	{
		.gpio = 160,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc1_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc1_suspend_config,
		},
	},
	
	{
		.gpio = 161,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc1_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc1_suspend_config,
		},
	},
	
	{
		.gpio = 162,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc1_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc1_suspend_config,
		},
	},
	
	{
		.gpio = 163,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc1_dat_4_7_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc1_suspend_config,
		},
	},
	
	{
		.gpio = 164,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc1_dat_4_7_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc1_suspend_config,
		},
	},
	
	{
		.gpio = 165,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc1_dat_4_7_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc1_suspend_config,
		},
	},
	
	{
		.gpio = 166,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc1_dat_4_7_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc1_suspend_config,
		},
	},
	
	{
		.gpio = 167,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc1_clk_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc1_suspend_config,
		},
	},
	
	{
		.gpio = 168,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc1_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc1_suspend_config,
		},
	},
};

static struct msm_gpiomux_config msm8x60_charm_sdc_configs[] __initdata = {
	
	{
		.gpio = 95,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc5_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc5_suspend_config,
		},
	},
	
	{
		.gpio = 96,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc5_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc5_suspend_config,
		},
	},
	
	{
		.gpio = 97,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc5_clk_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc5_suspend_config,
		},
	},
	
	{
		.gpio = 98,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc5_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc5_suspend_config,
		},
	},
	
	{
		.gpio = 99,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc5_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc5_suspend_config,
		},
	},
	
	{
		.gpio = 100,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc5_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc5_suspend_config,
		},
	},
	
	{
		.gpio = 129,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mdm2ap_sync_active_cfg,
			[GPIOMUX_SUSPENDED] = &mdm2ap_sync_suspend_cfg,
		},
	},

	
	{
		.gpio = 140,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mdm2ap_vddmin_active_cfg,
			[GPIOMUX_SUSPENDED] = &mdm2ap_vddmin_suspend_cfg,
		},
	},
	
	{
		.gpio = 143,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_config,
		},
	},
	
	{
		.gpio = 144,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_config,
		},
	},
	
	{
		.gpio = 145,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_config,
		},
	},
	
	{
		.gpio = 146,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_config,
		},
	},
	
	{
		.gpio = 147,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_dat_4_7_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_config,
		},
	},
	
	{
		.gpio = 148,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_dat_4_7_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_config,
		},
	},
	
	{
		.gpio = 149,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_dat_4_7_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_config,
		},
	},
	
	{
		.gpio = 150,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_dat_4_7_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_config,
		},
	},
	
	{
		.gpio = 151,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_dat_0_3_cmd_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_config,
		},
	},

	
	{
		.gpio = 152,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdcc2_clk_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdcc2_suspend_config,
		},
	},
};

static struct msm_gpiomux_config msm8x60_snd_configs[] __initdata = {
	{
		.gpio = 108,
		.settings = {
			[GPIOMUX_ACTIVE]    = &msm_snddev_active_config,
			[GPIOMUX_SUSPENDED] = &msm_snddev_suspend_config,
		},
	},
	{
		.gpio = 109,
		.settings = {
			[GPIOMUX_ACTIVE]    = &msm_snddev_active_config,
			[GPIOMUX_SUSPENDED] = &msm_snddev_suspend_config,
		},
	},
};

static struct msm_gpiomux_config msm8x60_mi2s_configs[] __initdata = {
	
	{
		.gpio = 101,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mi2s_active_cfg,
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
		},
	},
	
	{
		.gpio = 102,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mi2s_active_cfg,
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
		},
	},
	
	{
		.gpio = 103,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mi2s_active_cfg,
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
		},
	},
	
	{
		.gpio = 107,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mi2s_active_cfg,
			[GPIOMUX_SUSPENDED] = &mi2s_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config msm8x60_lcdc_configs[] __initdata = {
	
	{
		.gpio = 0,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 1,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 2,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 3,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 4,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 5,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 6,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 7,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 8,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 9,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 10,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 11,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 12,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 13,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 14,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 15,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 16,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 17,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 18,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 19,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 20,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 21,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 22,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 23,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 25,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 26,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
	
	{
		.gpio = 27,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcdc_active_cfg,
			[GPIOMUX_SUSPENDED] = &lcdc_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config msm8x60_mdp_vsync_configs[] __initdata = {
	{
		.gpio = 28,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mdp_vsync_active_cfg,
			[GPIOMUX_SUSPENDED] = &mdp_vsync_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config msm8x60_hdmi_configs[] __initdata = {
	{
		.gpio = 169,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 170,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 171,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 172,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_3_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
};


static struct msm_gpiomux_config msm8x60_pmic_configs[] __initdata = {
	{
		.gpio = 88,
		.settings = {
			[GPIOMUX_SUSPENDED] = &pmic_suspended_cfg,
		},
	},
	{
		.gpio = 91,
		.settings = {
			[GPIOMUX_SUSPENDED] = &pmic_suspended_cfg,
		},
	},
};

static struct msm_gpiomux_config msm8x60_common_configs[] __initdata = {
	
	{
		.gpio = 77,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mdm2ap_status_active_cfg,
			[GPIOMUX_SUSPENDED] = &mdm2ap_status_suspend_cfg,
		},
	},
	
	{
		.gpio      = 92,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ps_hold,
		},
	},
};

static struct msm_gpiomux_config msm8x60_cam_configs[] __initdata = {
	{
		.gpio = 29,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &cam_suspend_cfg,
		},
	},
	{
		.gpio = 30,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &cam_suspend_cfg,
		},
	},
	{
		.gpio = 31,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &cam_suspend_cfg,
		},
	},
	{
		.gpio = 32,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_5_cfg,
			[GPIOMUX_SUSPENDED] = &cam_suspend_cfg,
		},
	},
	{
		.gpio = 42,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &cam_suspend_cfg,
		},
	},
	{
		.gpio = 47,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_3_cfg,
			[GPIOMUX_SUSPENDED] = &cam_suspend_cfg,
		},
	},
	{
		.gpio = 48,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_3_cfg,
			[GPIOMUX_SUSPENDED] = &cam_suspend_cfg,
		},
	},
	{
		.gpio = 105,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_4_cfg,
			[GPIOMUX_SUSPENDED] = &cam_suspend_cfg,
		},
	},
	{
		.gpio = 106,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_active_4_cfg,
			[GPIOMUX_SUSPENDED] = &cam_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config msm8x60_charm_configs[] __initdata = {
	
	{
		.gpio = 135,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	
	{
		.gpio = 94,
		.settings = {
			[GPIOMUX_ACTIVE] = &mdm2ap_vfr_active_cfg,
			[GPIOMUX_SUSPENDED] = &mdm2ap_vfr_suspend_cfg,
		}
	},
	
	{
		.gpio = 136,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	
	{
		.gpio = 134,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_status_cfg,
		}
	},
	
	{
		.gpio = 40,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	
	{
		.gpio = 133,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_errfatal_cfg,
		}
	},
	
	{
		.gpio = 93,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	
	{
		.gpio = 132,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_kpdpwr_n_cfg,
		}
	},
	
	{
		.gpio = 131,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_kpdpwr_n_cfg,
		}
	}
};

struct msm_gpiomux_configs
msm8x60_surf_ffa_gpiomux_cfgs[] __initdata = {
	{msm8x60_gsbi_configs, ARRAY_SIZE(msm8x60_gsbi_configs)},
	{msm8x60_ebi2_configs, ARRAY_SIZE(msm8x60_ebi2_configs)},
	{msm8x60_uart_configs, ARRAY_SIZE(msm8x60_uart_configs)},
#if defined(CONFIG_USB_PEHCI_HCD) || defined(CONFIG_USB_PEHCI_HCD_MODULE)
	{msm8x60_isp_usb_configs, ARRAY_SIZE(msm8x60_isp_usb_configs)},
#endif
	{msm8x60_ts_configs, ARRAY_SIZE(msm8x60_ts_configs)},
	{msm8x60_aux_pcm_configs, ARRAY_SIZE(msm8x60_aux_pcm_configs)},
	{msm8x60_sdc_configs, ARRAY_SIZE(msm8x60_sdc_configs)},
	{msm8x60_snd_configs, ARRAY_SIZE(msm8x60_snd_configs)},
	{msm8x60_mi2s_configs, ARRAY_SIZE(msm8x60_mi2s_configs)},
	{msm8x60_lcdc_configs, ARRAY_SIZE(msm8x60_lcdc_configs)},
	{msm8x60_mdp_vsync_configs, ARRAY_SIZE(msm8x60_mdp_vsync_configs)},
	{msm8x60_hdmi_configs, ARRAY_SIZE(msm8x60_hdmi_configs)},
	{msm8x60_pmic_configs, ARRAY_SIZE(msm8x60_pmic_configs)},
	{msm8x60_common_configs, ARRAY_SIZE(msm8x60_common_configs)},
	{msm8x60_cam_configs, ARRAY_SIZE(msm8x60_cam_configs)},
	{msm8x60_tmg200_configs, ARRAY_SIZE(msm8x60_tmg200_configs)},
	{NULL, 0},
};

struct msm_gpiomux_configs
msm8x60_fluid_gpiomux_cfgs[] __initdata = {
	{msm8x60_gsbi_configs, ARRAY_SIZE(msm8x60_gsbi_configs)},
	{msm8x60_fluid_gsbi_configs, ARRAY_SIZE(msm8x60_fluid_gsbi_configs)},
	{msm8x60_ebi2_configs, ARRAY_SIZE(msm8x60_ebi2_configs)},
	{msm8x60_uart_configs, ARRAY_SIZE(msm8x60_uart_configs)},
	{msm8x60_ts_configs, ARRAY_SIZE(msm8x60_ts_configs)},
	{msm8x60_aux_pcm_configs, ARRAY_SIZE(msm8x60_aux_pcm_configs)},
	{msm8x60_sdc_configs, ARRAY_SIZE(msm8x60_sdc_configs)},
	{msm8x60_snd_configs, ARRAY_SIZE(msm8x60_snd_configs)},
	{msm8x60_mi2s_configs, ARRAY_SIZE(msm8x60_mi2s_configs)},
	{msm8x60_lcdc_configs, ARRAY_SIZE(msm8x60_lcdc_configs)},
	{msm8x60_mdp_vsync_configs, ARRAY_SIZE(msm8x60_mdp_vsync_configs)},
	{msm8x60_hdmi_configs, ARRAY_SIZE(msm8x60_hdmi_configs)},
	{msm8x60_pmic_configs, ARRAY_SIZE(msm8x60_pmic_configs)},
	{msm8x60_common_configs, ARRAY_SIZE(msm8x60_common_configs)},
	{msm8x60_cam_configs, ARRAY_SIZE(msm8x60_cam_configs)},
	{msm8x60_tma300_configs, ARRAY_SIZE(msm8x60_tma300_configs)},
	{NULL, 0},
};

struct msm_gpiomux_configs
msm8x60_charm_gpiomux_cfgs[] __initdata = {
	{msm8x60_gsbi_configs, ARRAY_SIZE(msm8x60_gsbi_configs)},
	{msm8x60_uart_configs, ARRAY_SIZE(msm8x60_uart_configs)},
#ifdef CONFIG_MSM_GSBI9_UART
	{msm8x60_charm_uart_configs, ARRAY_SIZE(msm8x60_charm_uart_configs)},
#endif
	{msm8x60_ts_configs, ARRAY_SIZE(msm8x60_ts_configs)},
	{msm8x60_aux_pcm_configs, ARRAY_SIZE(msm8x60_aux_pcm_configs)},
	{msm8x60_sdc_configs, ARRAY_SIZE(msm8x60_sdc_configs)},
	{msm8x60_snd_configs, ARRAY_SIZE(msm8x60_snd_configs)},
	{msm8x60_mi2s_configs, ARRAY_SIZE(msm8x60_mi2s_configs)},
	{msm8x60_lcdc_configs, ARRAY_SIZE(msm8x60_lcdc_configs)},
	{msm8x60_mdp_vsync_configs, ARRAY_SIZE(msm8x60_mdp_vsync_configs)},
	{msm8x60_hdmi_configs, ARRAY_SIZE(msm8x60_hdmi_configs)},
	{msm8x60_pmic_configs, ARRAY_SIZE(msm8x60_pmic_configs)},
	{msm8x60_common_configs, ARRAY_SIZE(msm8x60_common_configs)},
	{msm8x60_cam_configs, ARRAY_SIZE(msm8x60_cam_configs)},
	{msm8x60_tmg200_configs, ARRAY_SIZE(msm8x60_tmg200_configs)},
	{msm8x60_charm_sdc_configs, ARRAY_SIZE(msm8x60_charm_sdc_configs)},
	{msm8x60_charm_configs, ARRAY_SIZE(msm8x60_charm_configs)},
	{NULL, 0},
};

struct msm_gpiomux_configs
msm8x60_dragon_gpiomux_cfgs[] __initdata = {
	{msm8x60_gsbi_configs, ARRAY_SIZE(msm8x60_gsbi_configs)},
	{msm8x60_ebi2_configs, ARRAY_SIZE(msm8x60_ebi2_configs)},
	{msm8x60_uart_configs, ARRAY_SIZE(msm8x60_uart_configs)},
#if defined(CONFIG_USB_PEHCI_HCD) || defined(CONFIG_USB_PEHCI_HCD_MODULE)
	{msm8x60_isp_usb_configs, ARRAY_SIZE(msm8x60_isp_usb_configs)},
#endif
	{msm8x60_ts_configs, ARRAY_SIZE(msm8x60_ts_configs)},
	{msm8x60_aux_pcm_configs, ARRAY_SIZE(msm8x60_aux_pcm_configs)},
	{msm8x60_sdc_configs, ARRAY_SIZE(msm8x60_sdc_configs)},
	{msm8x60_snd_configs, ARRAY_SIZE(msm8x60_snd_configs)},
	{msm8x60_mi2s_configs, ARRAY_SIZE(msm8x60_mi2s_configs)},
	{msm8x60_lcdc_configs, ARRAY_SIZE(msm8x60_lcdc_configs)},
	{msm8x60_mdp_vsync_configs, ARRAY_SIZE(msm8x60_mdp_vsync_configs)},
	{msm8x60_hdmi_configs, ARRAY_SIZE(msm8x60_hdmi_configs)},
	{msm8x60_pmic_configs, ARRAY_SIZE(msm8x60_pmic_configs)},
	{msm8x60_common_configs, ARRAY_SIZE(msm8x60_common_configs)},
	{msm8x60_cam_configs, ARRAY_SIZE(msm8x60_cam_configs)},
	{msm8x60_tmg200_configs, ARRAY_SIZE(msm8x60_tmg200_configs)},
	{NULL, 0},
};

void __init msm8x60_init_gpiomux(struct msm_gpiomux_configs *cfgs)
{
	int rc;

	rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc) {
		pr_err("%s failure: %d\n", __func__, rc);
		return;
	}

	while (cfgs->cfg) {
		msm_gpiomux_install(cfgs->cfg, cfgs->ncfg);
		++cfgs;
	}
}
