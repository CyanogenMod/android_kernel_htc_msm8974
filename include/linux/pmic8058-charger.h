/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#ifndef __PMIC8058_CHARGER_H__
#define __PMIC8058_CHARGER_H__
enum pmic8058_chg_state {
	PMIC8058_CHG_STATE_NONE,
	PMIC8058_CHG_STATE_PWR_CHG,
	PMIC8058_CHG_STATE_ATC,
	PMIC8058_CHG_STATE_PWR_BAT,
	PMIC8058_CHG_STATE_ATC_FAIL,
	PMIC8058_CHG_STATE_AUX_EN,
	PMIC8058_CHG_STATE_PON_AFTER_ATC,
	PMIC8058_CHG_STATE_FAST_CHG,
	PMIC8058_CHG_STATE_TRKL_CHG,
	PMIC8058_CHG_STATE_CHG_FAIL,
	PMIC8058_CHG_STATE_EOC,
	PMIC8058_CHG_STATE_INRUSH_LIMIT,
	PMIC8058_CHG_STATE_USB_SUSPENDED,
	PMIC8058_CHG_STATE_PAUSE_ATC,
	PMIC8058_CHG_STATE_PAUSE_FAST_CHG,
	PMIC8058_CHG_STATE_PAUSE_TRKL_CHG
};

#if defined(CONFIG_BATTERY_MSM8X60) || defined(CONFIG_BATTERY_MSM8X60_MODULE)
int pmic8058_get_charge_batt(void);
int pmic8058_set_charge_batt(int);
enum pmic8058_chg_state pmic8058_get_fsm_state(void);
#else
int pmic8058_get_charge_batt(void)
{
	return -ENXIO;
}
int pmic8058_set_charge_batt(int)
{
	return -ENXIO;
}
enum pmic8058_chg_state pmic8058_get_fsm_state(void)
{
	return -ENXIO;
}
#endif
#endif 
