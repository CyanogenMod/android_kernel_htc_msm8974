/*
 * Summit Microelectronics SMB347 Battery Charger Driver
 *
 * Copyright (C) 2011, Intel Corporation
 *
 * Authors: Bruce E. Robertson <bruce.e.robertson@intel.com>
 *          Mika Westerberg <mika.westerberg@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef SMB347_CHARGER_H
#define SMB347_CHARGER_H

#include <linux/types.h>
#include <linux/power_supply.h>

enum {
	
	SMB347_SOFT_TEMP_COMPENSATE_DEFAULT = -1,

	SMB347_SOFT_TEMP_COMPENSATE_NONE,
	SMB347_SOFT_TEMP_COMPENSATE_CURRENT,
	SMB347_SOFT_TEMP_COMPENSATE_VOLTAGE,
};

#define SMB347_TEMP_USE_DEFAULT		-273

enum smb347_chg_enable {
	SMB347_CHG_ENABLE_SW,
	SMB347_CHG_ENABLE_PIN_ACTIVE_LOW,
	SMB347_CHG_ENABLE_PIN_ACTIVE_HIGH,
};

struct smb347_charger_platform_data {
	struct power_supply_info battery_info;
	unsigned int	max_charge_current;
	unsigned int	max_charge_voltage;
	unsigned int	pre_charge_current;
	unsigned int	termination_current;
	unsigned int	pre_to_fast_voltage;
	unsigned int	mains_current_limit;
	unsigned int	usb_hc_current_limit;
	unsigned int	chip_temp_threshold;
	int		soft_cold_temp_limit;
	int		soft_hot_temp_limit;
	int		hard_cold_temp_limit;
	int		hard_hot_temp_limit;
	bool		suspend_on_hard_temp_limit;
	unsigned int	soft_temp_limit_compensation;
	unsigned int	charge_current_compensation;
	bool		use_mains;
	bool		use_usb;
	bool		use_usb_otg;
	int		irq_gpio;
	enum smb347_chg_enable enable_control;
};

#endif 
