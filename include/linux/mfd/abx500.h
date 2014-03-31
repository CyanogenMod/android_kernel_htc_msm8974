/*
 * Copyright (C) 2007-2009 ST-Ericsson AB
 * License terms: GNU General Public License (GPL) version 2
 * AB3100 core access functions
 * Author: Linus Walleij <linus.walleij@stericsson.com>
 *
 * ABX500 core access functions.
 * The abx500 interface is used for the Analog Baseband chip
 * ab3100, ab5500, and ab8500.
 *
 * Author: Mattias Wallin <mattias.wallin@stericsson.com>
 * Author: Mattias Nilsson <mattias.i.nilsson@stericsson.com>
 * Author: Bengt Jonsson <bengt.g.jonsson@stericsson.com>
 * Author: Rickard Andersson <rickard.andersson@stericsson.com>
 */

#include <linux/regulator/machine.h>

struct device;

#ifndef MFD_ABX500_H
#define MFD_ABX500_H

#define AB3100_P1A	0xc0
#define AB3100_P1B	0xc1
#define AB3100_P1C	0xc2
#define AB3100_P1D	0xc3
#define AB3100_P1E	0xc4
#define AB3100_P1F	0xc5
#define AB3100_P1G	0xc6
#define AB3100_R2A	0xc7
#define AB3100_R2B	0xc8
#define AB5500_1_0	0x20
#define AB5500_1_1	0x21
#define AB5500_2_0	0x24

#define AB3100_EVENTA1_ONSWA				(0x01<<16)
#define AB3100_EVENTA1_ONSWB				(0x02<<16)
#define AB3100_EVENTA1_ONSWC				(0x04<<16)
#define AB3100_EVENTA1_DCIO				(0x08<<16)
#define AB3100_EVENTA1_OVER_TEMP			(0x10<<16)
#define AB3100_EVENTA1_SIM_OFF				(0x20<<16)
#define AB3100_EVENTA1_VBUS				(0x40<<16)
#define AB3100_EVENTA1_VSET_USB				(0x80<<16)

#define AB3100_EVENTA2_READY_TX				(0x01<<8)
#define AB3100_EVENTA2_READY_RX				(0x02<<8)
#define AB3100_EVENTA2_OVERRUN_ERROR			(0x04<<8)
#define AB3100_EVENTA2_FRAMING_ERROR			(0x08<<8)
#define AB3100_EVENTA2_CHARG_OVERCURRENT		(0x10<<8)
#define AB3100_EVENTA2_MIDR				(0x20<<8)
#define AB3100_EVENTA2_BATTERY_REM			(0x40<<8)
#define AB3100_EVENTA2_ALARM				(0x80<<8)

#define AB3100_EVENTA3_ADC_TRIG5			(0x01)
#define AB3100_EVENTA3_ADC_TRIG4			(0x02)
#define AB3100_EVENTA3_ADC_TRIG3			(0x04)
#define AB3100_EVENTA3_ADC_TRIG2			(0x08)
#define AB3100_EVENTA3_ADC_TRIGVBAT			(0x10)
#define AB3100_EVENTA3_ADC_TRIGVTX			(0x20)
#define AB3100_EVENTA3_ADC_TRIG1			(0x40)
#define AB3100_EVENTA3_ADC_TRIG0			(0x80)

#define AB3100_STR_ONSWA				(0x01)
#define AB3100_STR_ONSWB				(0x02)
#define AB3100_STR_ONSWC				(0x04)
#define AB3100_STR_DCIO					(0x08)
#define AB3100_STR_BOOT_MODE				(0x10)
#define AB3100_STR_SIM_OFF				(0x20)
#define AB3100_STR_BATT_REMOVAL				(0x40)
#define AB3100_STR_VBUS					(0x80)

#define AB3100_NUM_REGULATORS				10

struct ab3100 {
	struct mutex access_mutex;
	struct device *dev;
	struct i2c_client *i2c_client;
	struct i2c_client *testreg_client;
	char chip_name[32];
	u8 chip_id;
	struct blocking_notifier_head event_subscribers;
	u8 startup_events[3];
	bool startup_events_read;
};

struct ab3100_platform_data {
	struct regulator_init_data reg_constraints[AB3100_NUM_REGULATORS];
	u8 reg_initvals[AB3100_NUM_REGULATORS+2];
	int external_voltage;
};

int ab3100_event_register(struct ab3100 *ab3100,
			  struct notifier_block *nb);
int ab3100_event_unregister(struct ab3100 *ab3100,
			    struct notifier_block *nb);

struct abx500_init_settings {
	u8 bank;
	u8 reg;
	u8 setting;
};

enum abx500_adc_therm {
	ABx500_ADC_THERM_BATCTRL,
	ABx500_ADC_THERM_BATTEMP,
};

struct abx500_res_to_temp {
	int temp;
	int resist;
};

struct abx500_v_to_cap {
	int voltage;
	int capacity;
};

struct abx500_fg;

struct abx500_fg_parameters {
	int recovery_sleep_timer;
	int recovery_total_time;
	int init_timer;
	int init_discard_time;
	int init_total_time;
	int high_curr_time;
	int accu_charging;
	int accu_high_curr;
	int high_curr_threshold;
	int lowbat_threshold;
	int overbat_threshold;
	int battok_falling_th_sel0;
	int battok_raising_th_sel1;
	int user_cap_limit;
	int maint_thres;
};

struct abx500_maxim_parameters {
	bool ena_maxi;
	int chg_curr;
	int wait_cycles;
	int charger_curr_step;
};

struct abx500_battery_type {
	int name;
	int resis_high;
	int resis_low;
	int charge_full_design;
	int nominal_voltage;
	int termination_vol;
	int termination_curr;
	int recharge_vol;
	int normal_cur_lvl;
	int normal_vol_lvl;
	int maint_a_cur_lvl;
	int maint_a_vol_lvl;
	int maint_a_chg_timer_h;
	int maint_b_cur_lvl;
	int maint_b_vol_lvl;
	int maint_b_chg_timer_h;
	int low_high_cur_lvl;
	int low_high_vol_lvl;
	int battery_resistance;
	int n_temp_tbl_elements;
	struct abx500_res_to_temp *r_to_t_tbl;
	int n_v_cap_tbl_elements;
	struct abx500_v_to_cap *v_to_cap_tbl;
	int n_batres_tbl_elements;
	struct batres_vs_temp *batres_tbl;
};

struct abx500_bm_capacity_levels {
	int critical;
	int low;
	int normal;
	int high;
	int full;
};

struct abx500_bm_charger_parameters {
	int usb_volt_max;
	int usb_curr_max;
	int ac_volt_max;
	int ac_curr_max;
};

struct abx500_bm_data {
	int temp_under;
	int temp_low;
	int temp_high;
	int temp_over;
	int temp_now;
	int temp_interval_chg;
	int temp_interval_nochg;
	int main_safety_tmr_h;
	int usb_safety_tmr_h;
	int bkup_bat_v;
	int bkup_bat_i;
	bool no_maintenance;
	bool chg_unknown_bat;
	bool enable_overshoot;
	bool auto_trig;
	enum abx500_adc_therm adc_therm;
	int fg_res;
	int n_btypes;
	int batt_id;
	int interval_charging;
	int interval_not_charging;
	int temp_hysteresis;
	int gnd_lift_resistance;
	const struct abx500_maxim_parameters *maxi;
	const struct abx500_bm_capacity_levels *cap_levels;
	const struct abx500_battery_type *bat_type;
	const struct abx500_bm_charger_parameters *chg_params;
	const struct abx500_fg_parameters *fg_params;
};

struct abx500_chargalg_platform_data {
	char **supplied_to;
	size_t num_supplicants;
};

struct abx500_charger_platform_data {
	char **supplied_to;
	size_t num_supplicants;
	bool autopower_cfg;
};

struct abx500_btemp_platform_data {
	char **supplied_to;
	size_t num_supplicants;
};

struct abx500_fg_platform_data {
	char **supplied_to;
	size_t num_supplicants;
};

struct abx500_bm_plat_data {
	struct abx500_bm_data *battery;
	struct abx500_charger_platform_data *charger;
	struct abx500_btemp_platform_data *btemp;
	struct abx500_fg_platform_data *fg;
	struct abx500_chargalg_platform_data *chargalg;
};

int abx500_set_register_interruptible(struct device *dev, u8 bank, u8 reg,
	u8 value);
int abx500_get_register_interruptible(struct device *dev, u8 bank, u8 reg,
	u8 *value);
int abx500_get_register_page_interruptible(struct device *dev, u8 bank,
	u8 first_reg, u8 *regvals, u8 numregs);
int abx500_set_register_page_interruptible(struct device *dev, u8 bank,
	u8 first_reg, u8 *regvals, u8 numregs);
int abx500_mask_and_set_register_interruptible(struct device *dev, u8 bank,
	u8 reg, u8 bitmask, u8 bitvalues);
int abx500_get_chip_id(struct device *dev);
int abx500_event_registers_startup_state_get(struct device *dev, u8 *event);
int abx500_startup_irq_enabled(struct device *dev, unsigned int irq);

struct abx500_ops {
	int (*get_chip_id) (struct device *);
	int (*get_register) (struct device *, u8, u8, u8 *);
	int (*set_register) (struct device *, u8, u8, u8);
	int (*get_register_page) (struct device *, u8, u8, u8 *, u8);
	int (*set_register_page) (struct device *, u8, u8, u8 *, u8);
	int (*mask_and_set_register) (struct device *, u8, u8, u8, u8);
	int (*event_registers_startup_state_get) (struct device *, u8 *);
	int (*startup_irq_enabled) (struct device *, unsigned int);
};

int abx500_register_ops(struct device *core_dev, struct abx500_ops *ops);
void abx500_remove_ops(struct device *dev);
#endif
