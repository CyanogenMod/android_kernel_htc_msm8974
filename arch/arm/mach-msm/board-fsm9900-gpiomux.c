/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <mach/board.h>
#include <mach/gpiomux.h>

static struct gpiomux_setting blsp_uart_no_pull_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting blsp_uart_pull_up_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting blsp_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm_blsp_configs[] __initdata = {
	{
		.gpio      = 0,	       
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_uart_no_pull_config,
		},
	},
	{
		.gpio      = 1,	       
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_uart_pull_up_config,
		},
	},
	{
		.gpio      = 2,	       
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},
	{
		.gpio      = 3,	       
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},
	{
		.gpio      = 6,	       
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},
	{
		.gpio      = 7,	       
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},
	{
		.gpio      = 36,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_uart_no_pull_config,
		},
	},
	{
		.gpio      = 37,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_uart_pull_up_config,
		},
	},
	{
		.gpio      = 38,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},
	{
		.gpio      = 39,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &blsp_i2c_config,
		},
	},

};

static struct gpiomux_setting geni_func4_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting geni_func5_config = {
	.func = GPIOMUX_FUNC_5,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config fsm_geni_configs[] __initdata = {
	{
		.gpio      = 8,	       
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},
	{
		.gpio      = 9,	       
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},
	{
		.gpio      = 10,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},
	{
		.gpio      = 11,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},
	{
		.gpio      = 20,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func5_config,
		},
	},
	{
		.gpio      = 21,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func5_config,
		},
	},
	{
		.gpio      = 22,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func5_config,
		},
	},
	{
		.gpio      = 23,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func5_config,
		},
	},
	{
		.gpio      = 30,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},
	{
		.gpio      = 31,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &geni_func4_config,
		},
	},

};

static struct gpiomux_setting dan_spi_func4_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting dan_spi_func1_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm_dan_spi_configs[] __initdata = {
	{
		.gpio      = 12,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 13,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 14,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 15,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 16,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 17,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 18,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 19,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func4_config,
		},
	},
	{
		.gpio      = 81,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func1_config,
		},
	},
	{
		.gpio      = 82,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &dan_spi_func1_config,
		},
	},
};

static struct gpiomux_setting uim_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm_uim_configs[] __initdata = {
	{
		.gpio      = 24,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &uim_config,
		},
	},
	{
		.gpio      = 25,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &uim_config,
		},
	},
	{
		.gpio      = 26,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &uim_config,
		},
	},
	{
		.gpio      = 27,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &uim_config,
		},
	},
};

static struct gpiomux_setting pcie_config = {
	.func = GPIOMUX_FUNC_4,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm_pcie_configs[] __initdata = {
	{
		.gpio      = 28,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &pcie_config,
		},
	},
	{
		.gpio      = 32,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &pcie_config,
		},
	},
};

static struct gpiomux_setting pps_out_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_4MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting pps_in_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gps_clk_in_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gps_nav_tlmm_blank_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct msm_gpiomux_config fsm_gps_configs[] __initdata = {
	{
		.gpio      = 40,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &pps_out_config,
		},
	},
	{
		.gpio      = 41,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &pps_in_config,
		},
	},
	{
		.gpio      = 43,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &gps_clk_in_config,
		},
	},
	{
		.gpio      = 120,      
		.settings = {
			[GPIOMUX_SUSPENDED] = &gps_nav_tlmm_blank_config,
		},
	},
};

static struct gpiomux_setting sd_detect_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sd_wp_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config fsm_sd_configs[] __initdata = {
	{
		.gpio      = 42,       
		.settings = {
			[GPIOMUX_SUSPENDED] = &sd_detect_config,
		},
	},
	{
		.gpio      = 122,      
		.settings = {
			[GPIOMUX_SUSPENDED] = &sd_wp_config,
		},
	},
};

void __init fsm9900_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	msm_gpiomux_install(fsm_blsp_configs, ARRAY_SIZE(fsm_blsp_configs));
	msm_gpiomux_install(fsm_geni_configs, ARRAY_SIZE(fsm_geni_configs));
	msm_gpiomux_install(fsm_dan_spi_configs,
			    ARRAY_SIZE(fsm_dan_spi_configs));
	msm_gpiomux_install(fsm_uim_configs, ARRAY_SIZE(fsm_uim_configs));
	msm_gpiomux_install(fsm_pcie_configs, ARRAY_SIZE(fsm_pcie_configs));
	msm_gpiomux_install(fsm_gps_configs, ARRAY_SIZE(fsm_gps_configs));
	msm_gpiomux_install(fsm_sd_configs, ARRAY_SIZE(fsm_sd_configs));
}
