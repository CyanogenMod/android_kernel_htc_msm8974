/*
 * arch/arm/mach-u300/i2c.c
 *
 * Copyright (C) 2009 ST-Ericsson AB
 * License terms: GNU General Public License (GPL) version 2
 *
 * Register board i2c devices
 * Author: Linus Walleij <linus.walleij@stericsson.com>
 */
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/mfd/abx500.h>
#include <linux/regulator/machine.h>
#include <linux/amba/bus.h>
#include <mach/irqs.h>


#define LDO_A_SETTING		0x16
#define LDO_C_SETTING		0x10
#define LDO_D_SETTING		0x10
#define LDO_E_SETTING		0x10
#define LDO_E_SLEEP_SETTING	0x00
#define LDO_F_SETTING		0xD0
#define LDO_G_SETTING		0x00
#define LDO_H_SETTING		0x18
#define LDO_K_SETTING		0x00
#define LDO_EXT_SETTING		0x00
#define BUCK_SETTING	0x7D
#define BUCK_SLEEP_SETTING	0xAC

#ifdef CONFIG_AB3100_CORE
static struct regulator_consumer_supply supply_ldo_c[] = {
	{
		.dev_name = "ab3100-codec",
		.supply = "vaudio", 
	},
};

static struct regulator_consumer_supply supply_ldo_d[] = {
	{
		.supply = "vana15", 
	},
};

static struct regulator_consumer_supply supply_ldo_g[] = {
	{
		.dev_name = "mmci",
		.supply = "vmmc", 
	},
};

static struct regulator_consumer_supply supply_ldo_h[] = {
	{
		.dev_name = "xgam_pdi",
		.supply = "vdisp", 
	},
};

static struct regulator_consumer_supply supply_ldo_k[] = {
	{
		.dev_name = "irda",
		.supply = "vir", 
	},
};

static struct regulator_consumer_supply supply_ldo_ext[] = {
	{
		.supply = "vext", 
	},
};

#define LDO_A_VOLTAGE 2750000
#define LDO_C_VOLTAGE 2650000
#define LDO_D_VOLTAGE 2650000

static struct ab3100_platform_data ab3100_plf_data = {
	.reg_constraints = {
		
		{
			.constraints = {
				.name = "vrad",
				.min_uV = LDO_A_VOLTAGE,
				.max_uV = LDO_A_VOLTAGE,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.always_on = 1,
				.boot_on = 1,
			},
		},
		
		{
			.constraints = {
				.min_uV = LDO_C_VOLTAGE,
				.max_uV = LDO_C_VOLTAGE,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
			},
			.num_consumer_supplies = ARRAY_SIZE(supply_ldo_c),
			.consumer_supplies = supply_ldo_c,
		},
		
		{
			.constraints = {
				.min_uV = LDO_D_VOLTAGE,
				.max_uV = LDO_D_VOLTAGE,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			},
			.num_consumer_supplies = ARRAY_SIZE(supply_ldo_d),
			.consumer_supplies = supply_ldo_d,
		},
		
		{
			.constraints = {
				.name = "vio",
				.min_uV = 1800000,
				.max_uV = 1800000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.always_on = 1,
				.boot_on = 1,
			},
		},
		
		{
			.constraints = {
				.name = "vana25",
				.min_uV = 2500000,
				.max_uV = 2500000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.always_on = 1,
				.boot_on = 1,
			},
		},
		
		{
			.constraints = {
				.min_uV = 1500000,
				.max_uV = 2850000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.valid_ops_mask =
				REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
			},
			.num_consumer_supplies = ARRAY_SIZE(supply_ldo_g),
			.consumer_supplies = supply_ldo_g,
		},
		
		{
			.constraints = {
				.min_uV = 1200000,
				.max_uV = 2750000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.valid_ops_mask =
				REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
			},
			.num_consumer_supplies = ARRAY_SIZE(supply_ldo_h),
			.consumer_supplies = supply_ldo_h,
		},
		
		{
			.constraints = {
				.min_uV = 1800000,
				.max_uV = 2750000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.valid_ops_mask =
				REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
			},
			.num_consumer_supplies = ARRAY_SIZE(supply_ldo_k),
			.consumer_supplies = supply_ldo_k,
		},
		{
			.constraints = {
				.min_uV = 0,
				.max_uV = 0,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.valid_ops_mask =
				REGULATOR_CHANGE_STATUS,
			},
			.num_consumer_supplies = ARRAY_SIZE(supply_ldo_ext),
			.consumer_supplies = supply_ldo_ext,
		},
		
		{
			.constraints = {
				.name = "vcore",
				.min_uV = 1200000,
				.max_uV = 1800000,
				.valid_modes_mask = REGULATOR_MODE_NORMAL,
				.valid_ops_mask =
				REGULATOR_CHANGE_VOLTAGE,
				.always_on = 1,
				.boot_on = 1,
			},
		},
	},
	.reg_initvals = {
		LDO_A_SETTING,
		LDO_C_SETTING,
		LDO_E_SETTING,
		LDO_E_SLEEP_SETTING,
		LDO_F_SETTING,
		LDO_G_SETTING,
		LDO_H_SETTING,
		LDO_K_SETTING,
		LDO_EXT_SETTING,
		BUCK_SETTING,
		BUCK_SLEEP_SETTING,
		LDO_D_SETTING,
	},
};
#endif

static struct i2c_board_info __initdata bus0_i2c_board_info[] = {
#ifdef CONFIG_AB3100_CORE
	{
		.type = "ab3100",
		.addr = 0x48,
		.irq = IRQ_U300_IRQ0_EXT,
		.platform_data = &ab3100_plf_data,
	},
#else
	{ },
#endif
};

static struct i2c_board_info __initdata bus1_i2c_board_info[] = {
#ifdef CONFIG_MACH_U300_BS335
	{
		.type = "fwcam",
		.addr = 0x10,
	},
	{
		.type = "fwcam",
		.addr = 0x5d,
	},
#else
	{ },
#endif
};

void __init u300_i2c_register_board_devices(void)
{
	i2c_register_board_info(0, bus0_i2c_board_info,
				ARRAY_SIZE(bus0_i2c_board_info));
	regulator_has_full_constraints();
	i2c_register_board_info(1, bus1_i2c_board_info,
				ARRAY_SIZE(bus1_i2c_board_info));
}
