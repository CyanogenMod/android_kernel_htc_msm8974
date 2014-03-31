/*
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * MyungJoo.Ham <myungjoo.ham@samsung.com>
 *
 * Charger Manager.
 * This framework enables to control and multiple chargers and to
 * monitor charging even in the context of suspend-to-RAM with
 * an interface combining the chargers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
**/

#ifndef _CHARGER_MANAGER_H
#define _CHARGER_MANAGER_H

#include <linux/power_supply.h>

enum data_source {
	CM_FUEL_GAUGE,
	CM_CHARGER_STAT,
};

enum polling_modes {
	CM_POLL_DISABLE = 0,
	CM_POLL_ALWAYS,
	CM_POLL_EXTERNAL_POWER_ONLY,
	CM_POLL_CHARGING_ONLY,
};

struct charger_global_desc {
	char *rtc_name;

	bool (*rtc_only_wakeup)(void);
};

struct charger_desc {
	char *psy_name;

	enum polling_modes polling_mode;
	unsigned int polling_interval_ms;

	unsigned int fullbatt_uV;

	enum data_source battery_present;

	char **psy_charger_stat;

	int num_charger_regulators;
	struct regulator_bulk_data *charger_regulators;

	char *psy_fuel_gauge;

	int (*temperature_out_of_range)(int *mC);
	bool measure_battery_temp;
};

#define PSY_NAME_MAX	30

struct charger_manager {
	struct list_head entry;
	struct device *dev;
	struct charger_desc *desc;

	struct power_supply *fuel_gauge;
	struct power_supply **charger_stat;

	bool charger_enabled;

	int emergency_stop;
	int last_temp_mC;

	char psy_name_buf[PSY_NAME_MAX + 1];
	struct power_supply charger_psy;

	bool status_save_ext_pwr_inserted;
	bool status_save_batt;
};

#ifdef CONFIG_CHARGER_MANAGER
extern int setup_charger_manager(struct charger_global_desc *gd);
extern bool cm_suspend_again(void);
#else
static void __maybe_unused setup_charger_manager(struct charger_global_desc *gd)
{ }

static bool __maybe_unused cm_suspend_again(void)
{
	return false;
}
#endif

#endif 
