/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

#include <linux/qcomwlan_pwrif.h>
#include <linux/export.h>

#define GPIO_WLAN_DEEP_SLEEP_N  230
#define GPIO_WLAN_DEEP_SLEEP_N_DRAGON  82
#define WLAN_RESET_OUT          1
#define WLAN_RESET              0

static const char *id = "WLAN";

int vos_chip_power_qrf8615(int on)
{
	static char wlan_on;
	static const char *vregs_qwlan_name[] = {
		"8058_l20",
		"8058_l8",
		"8901_s4",
		"8901_lvs1",
		"8901_l0",
		"8058_s2",
		"8058_s1",
	};
	static const char *vregs_qwlan_pc_name[] = {
		"8058_l20_pc",
		"8058_l8_pc",
		NULL,
		NULL,
		"8901_l0_pc",
		"8058_s2_pc",
		NULL,
	};
	static const int vregs_qwlan_val_min[] = {
		1800000,
		3050000,
		1225000,
		0,
		1200000,
		1300000,
		500000,
	};
	static const int vregs_qwlan_val_max[] = {
		1800000,
		3050000,
		1225000,
		0,
		1200000,
		1300000,
		1250000,
	};
	static const int vregs_qwlan_peek_current[] = {
		4000,
		150000,
		60000,
		0,
		32000,
		130000,
		0,
	};
	static const bool vregs_is_pin_controlled_default[] = {
		1,
		1,
		0,
		0,
		1,
		1,
		0,
	};
	static const bool vregs_is_pin_controlled_dragon[] = {
		0,
		0,
		0,
		0,
		0,
		1,
		0,
	};
	bool const *vregs_is_pin_controlled;
	static struct regulator *vregs_qwlan[ARRAY_SIZE(vregs_qwlan_name)];
	static struct regulator *vregs_pc_qwlan[ARRAY_SIZE(vregs_qwlan_name)];
	static struct msm_xo_voter *wlan_clock;
	int ret, i, rc = 0;
	unsigned wlan_gpio_deep_sleep = GPIO_WLAN_DEEP_SLEEP_N;

	vregs_is_pin_controlled = vregs_is_pin_controlled_default;

	if (machine_is_msm8x60_dragon()) {
		wlan_gpio_deep_sleep = GPIO_WLAN_DEEP_SLEEP_N_DRAGON;
		vregs_is_pin_controlled = vregs_is_pin_controlled_dragon;
	}
	
	if (on && !wlan_on) {
		rc = gpio_request(wlan_gpio_deep_sleep, "WLAN_DEEP_SLEEP_N");
		if (rc) {
			pr_err("WLAN reset GPIO %d request failed\n",
					wlan_gpio_deep_sleep);
			goto fail;
		}
		rc = gpio_direction_output(wlan_gpio_deep_sleep,
				WLAN_RESET);
		if (rc < 0) {
			pr_err("WLAN reset GPIO %d set output direction failed",
					wlan_gpio_deep_sleep);
			goto fail_gpio_dir_out;
		}

		
		if (wlan_clock == NULL) {
			wlan_clock = msm_xo_get(MSM_XO_TCXO_A0, id);
			if (IS_ERR(wlan_clock)) {
				pr_err("Failed to get TCXO_A0 voter (%ld)\n",
						PTR_ERR(wlan_clock));
				goto fail_gpio_dir_out;
			}
		}

		rc = msm_xo_mode_vote(wlan_clock, MSM_XO_MODE_PIN_CTRL);
		if (rc < 0) {
			pr_err("Configuring TCXO to Pin controllable failed"
					"(%d)\n", rc);
			goto fail_xo_mode_vote;
		}
	} else if (!on && wlan_on) {
		if (wlan_clock != NULL)
			msm_xo_mode_vote(wlan_clock, MSM_XO_MODE_OFF);
		gpio_set_value_cansleep(wlan_gpio_deep_sleep, WLAN_RESET);
		gpio_free(wlan_gpio_deep_sleep);
	}

	
	for (i = 0; i < ARRAY_SIZE(vregs_qwlan_name); i++) {
		if (on && !wlan_on)	{
			vregs_qwlan[i] = regulator_get(NULL,
					vregs_qwlan_name[i]);
			if (IS_ERR(vregs_qwlan[i])) {
				pr_err("regulator get of %s failed (%ld)\n",
						vregs_qwlan_name[i],
						PTR_ERR(vregs_qwlan[i]));
				rc = PTR_ERR(vregs_qwlan[i]);
				goto vreg_get_fail;
			}
			if (vregs_qwlan_val_min[i] || vregs_qwlan_val_max[i]) {
				rc = regulator_set_voltage(vregs_qwlan[i],
						vregs_qwlan_val_min[i],
						vregs_qwlan_val_max[i]);
				if (rc) {
					pr_err("regulator_set_voltage(%s) failed\n",
							vregs_qwlan_name[i]);
					goto vreg_fail;
				}
			}
			
			if (vregs_is_pin_controlled[i]) {
				vregs_pc_qwlan[i] = regulator_get(NULL,
							vregs_qwlan_pc_name[i]);
				if (IS_ERR(vregs_pc_qwlan[i])) {
					pr_err("regulator get of %s failed "
						"(%ld)\n",
						vregs_qwlan_pc_name[i],
						PTR_ERR(vregs_pc_qwlan[i]));
					rc = PTR_ERR(vregs_pc_qwlan[i]);
					goto vreg_fail;
				}
			}

			if (vregs_qwlan_peek_current[i]) {
				rc = regulator_set_optimum_mode(vregs_qwlan[i],
						vregs_qwlan_peek_current[i]);
				if (rc < 0)
					pr_err("vreg %s set optimum mode"
						" failed to %d (%d)\n",
						vregs_qwlan_name[i], rc,
						 vregs_qwlan_peek_current[i]);
			}
			rc = regulator_enable(vregs_qwlan[i]);
			if (rc < 0) {
				pr_err("vreg %s enable failed (%d)\n",
						vregs_qwlan_name[i], rc);
				goto vreg_fail;
			}
			if (vregs_is_pin_controlled[i]) {
				rc = regulator_enable(vregs_pc_qwlan[i]);
				if (rc < 0) {
					pr_err("vreg %s enable failed (%d)\n",
						vregs_qwlan_pc_name[i], rc);
					goto vreg_fail;
				}
			}
		} else if (!on && wlan_on) {

			if (vregs_qwlan_peek_current[i]) {
				rc = regulator_set_optimum_mode(vregs_qwlan[i],
									 1000);
				if (rc < 0)
					pr_info("vreg %s set optimum mode"
								"failed (%d)\n",
						vregs_qwlan_name[i], rc);
				rc = regulator_set_voltage(vregs_qwlan[i], 0 ,
							vregs_qwlan_val_max[i]);
				if (rc)
					pr_err("regulator_set_voltage(%s)"
								"failed (%d)\n",
						vregs_qwlan_name[i], rc);

			}

			if (vregs_is_pin_controlled[i]) {
				rc = regulator_disable(vregs_pc_qwlan[i]);
				if (rc < 0) {
					pr_err("vreg %s disable failed (%d)\n",
						vregs_qwlan_pc_name[i], rc);
					goto vreg_fail;
				}
				regulator_put(vregs_pc_qwlan[i]);
			}

			rc = regulator_disable(vregs_qwlan[i]);
			if (rc < 0) {
				pr_err("vreg %s disable failed (%d)\n",
						vregs_qwlan_name[i], rc);
				goto vreg_fail;
			}
			regulator_put(vregs_qwlan[i]);
		}
	}
	if (on) {
		gpio_set_value_cansleep(wlan_gpio_deep_sleep, WLAN_RESET_OUT);
		wlan_on = true;
	}
	else
		wlan_on = false;
	return 0;

vreg_fail:
	regulator_put(vregs_qwlan[i]);
	if (vregs_is_pin_controlled[i])
		regulator_put(vregs_pc_qwlan[i]);
vreg_get_fail:
	i--;
	while (i >= 0) {
		ret = !on ? regulator_enable(vregs_qwlan[i]) :
			regulator_disable(vregs_qwlan[i]);
		if (ret < 0) {
			pr_err("vreg %s %s failed (%d) in err path\n",
					vregs_qwlan_name[i],
					!on ? "enable" : "disable", ret);
		}
		if (vregs_is_pin_controlled[i]) {
			ret = !on ? regulator_enable(vregs_pc_qwlan[i]) :
				regulator_disable(vregs_pc_qwlan[i]);
			if (ret < 0) {
				pr_err("vreg %s %s failed (%d) in err path\n",
					vregs_qwlan_pc_name[i],
					!on ? "enable" : "disable", ret);
			}
		}
		regulator_put(vregs_qwlan[i]);
		if (vregs_is_pin_controlled[i])
			regulator_put(vregs_pc_qwlan[i]);
		i--;
	}
	if (!on)
		goto fail;
fail_xo_mode_vote:
	msm_xo_put(wlan_clock);
fail_gpio_dir_out:
	gpio_free(wlan_gpio_deep_sleep);
fail:
	return rc;
}
EXPORT_SYMBOL(vos_chip_power_qrf8615);

int qcomwlan_pmic_xo_core_force_enable(int on)
{
	static struct msm_xo_voter *wlan_ps;
	int rc = 0;

	if (wlan_ps == NULL) {
		wlan_ps = msm_xo_get(MSM_XO_CORE, id);
		if (IS_ERR(wlan_ps)) {
			pr_err("Failed to get XO CORE voter (%ld)\n",
					PTR_ERR(wlan_ps));
			goto fail;
		}
	}

	if (on)
		rc = msm_xo_mode_vote(wlan_ps, MSM_XO_MODE_ON);
	else
		rc = msm_xo_mode_vote(wlan_ps, MSM_XO_MODE_OFF);

	if (rc < 0) {
		pr_err("XO Core %s failed (%d)\n",
			on ? "enable" : "disable", rc);
		goto fail_xo_mode_vote;
	}
	return 0;
fail_xo_mode_vote:
	msm_xo_put(wlan_ps);
fail:
	return rc;
}
EXPORT_SYMBOL(qcomwlan_pmic_xo_core_force_enable);



int qcomwlan_freq_change_1p3v_supply(enum rpm_vreg_freq freq)
{
	return rpm_vreg_set_frequency(RPM_VREG_ID_PM8058_S2, freq);
}
EXPORT_SYMBOL(qcomwlan_freq_change_1p3v_supply);
