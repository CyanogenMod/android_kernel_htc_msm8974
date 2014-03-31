/* Copyright (c) 2009, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/uaccess.h>

#include <mach/pmic.h>


static int debug_lp_mode_control(char *buf, int size)
{
	enum switch_cmd	cmd;
	enum vreg_lp_id	id;
	int	cnt;


	cnt = sscanf(buf, "%u %u", &cmd, &id);
	if (cnt < 2) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d", __func__, cnt);
		return -EINVAL;
	}

	if (pmic_lp_mode_control(cmd, id) < 0)
		return -EFAULT;

	return size;
}

static int debug_vreg_set_level(char *buf, int size)
{
	enum vreg_id vreg;
	int	level;
	int	cnt;

	cnt = sscanf(buf, "%u %u", &vreg, &level);
	if (cnt < 2) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d", __func__, cnt);
		return -EINVAL;
	}
	if (pmic_vreg_set_level(vreg, level) < 0)
		return -EFAULT;

	return size;
}

static int debug_vreg_pull_down_switch(char *buf, int size)
{
	enum switch_cmd	cmd;
	enum vreg_pdown_id id;
	int	cnt;

	cnt = sscanf(buf, "%u %u", &cmd, &id);
	if (cnt < 2) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d", __func__, cnt);
		return -EINVAL;
	}
	if (pmic_vreg_pull_down_switch(cmd, id) < 0)
		return -EFAULT;

	return size;
}

static int debug_secure_mpp_control_digital_output(char *buf, int size)
{
	enum mpp_which which;
	enum mpp_dlogic_level level;
	enum mpp_dlogic_out_ctrl out;
	int	cnt;

	cnt = sscanf(buf, "%u %u %u", &which, &level, &out);
	if (cnt < 3) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}

	if (pmic_secure_mpp_control_digital_output(which, level, out) < 0)
		return -EFAULT;

	return size;
}

static int debug_secure_mpp_config_i_sink(char *buf, int size)
{
	enum mpp_which which;
	enum mpp_i_sink_level level;
	enum mpp_i_sink_switch onoff;
	int	cnt;

	cnt = sscanf(buf, "%u %u %u", &which, &level, &onoff);
	if (cnt < 3) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}

	if (pmic_secure_mpp_config_i_sink(which, level, onoff) < 0)
		return -EFAULT;

	return size;
}

static int debug_secure_mpp_config_digital_input(char *buf, int size)
{
	enum mpp_which which;
	enum mpp_dlogic_level level;
	enum mpp_dlogic_in_dbus dbus;
	int	cnt;

	cnt = sscanf(buf, "%u %u %u", &which, &level, &dbus);
	if (cnt < 3) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_secure_mpp_config_digital_input(which, level, dbus) < 0)
		return -EFAULT;

	return size;
}

static int debug_rtc_start(char *buf, int size)
{
	uint time;
	struct rtc_time	*hal;
	int	cnt;

	cnt = sscanf(buf, "%d", &time);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	hal = (struct rtc_time 	*)&time;
	if (pmic_rtc_start(hal) < 0)
		return -EFAULT;

	return size;
}

static int debug_rtc_stop(char *buf, int size)
{
	if (pmic_rtc_stop() < 0)
		return -EFAULT;

	return size;
}

static int debug_rtc_get_time(char *buf, int size)
{
	uint time;
	struct rtc_time *hal;

	hal = (struct rtc_time 	*)&time;
	if (pmic_rtc_get_time(hal) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", time);
}

static int	debug_rtc_alarm_ndx;

int debug_rtc_enable_alarm(char *buf, int size)
{
	enum rtc_alarm alarm;
	struct rtc_time	*hal;
	uint time;
	int	cnt;


	cnt = sscanf(buf, "%u %u", &alarm, &time);
	if (cnt < 2) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	hal = (struct rtc_time 	*)&time;

	if (pmic_rtc_enable_alarm(alarm, hal) < 0)
		return -EFAULT;

	debug_rtc_alarm_ndx = alarm;
	return size;
}

static int debug_rtc_disable_alarm(char *buf, int size)
{

	enum rtc_alarm alarm;
	int	cnt;

	cnt = sscanf(buf, "%u", &alarm);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_rtc_disable_alarm(alarm) < 0)
		return -EFAULT;

	return size;
}

static int debug_rtc_get_alarm_time(char *buf, int size)
{
	uint time;
	struct rtc_time	*hal;

	hal = (struct rtc_time 	*)&time;
	if (pmic_rtc_get_alarm_time(debug_rtc_alarm_ndx, hal) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", time);
}
static int debug_rtc_get_alarm_status(char *buf, int size)
{
	int	status;;

	if (pmic_rtc_get_alarm_status(&status) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", status);

}

static int debug_rtc_set_time_adjust(char *buf, int size)
{
	uint adjust;
	int	cnt;

	cnt = sscanf(buf, "%d", &adjust);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_rtc_set_time_adjust(adjust) < 0)
		return -EFAULT;

	return size;
}

static int debug_rtc_get_time_adjust(char *buf, int size)
{
	int	adjust;;

	if (pmic_rtc_get_time_adjust(&adjust) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", adjust);
}

static int debug_set_led_intensity(char *buf, int size)
{
	enum ledtype type;
	int	level;
	int	cnt;

	cnt = sscanf(buf, "%u %d", &type, &level);
	if (cnt < 2) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_set_led_intensity(type, level) < 0)
		return -EFAULT;

	return size;
}

static int debug_flash_led_set_current(char *buf, int size)
{
	int	milliamps;
	int	cnt;

	cnt = sscanf(buf, "%d", &milliamps);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_flash_led_set_current(milliamps) < 0)
		return -EFAULT;

	return size;
}
static int debug_flash_led_set_mode(char *buf, int size)
{

	uint mode;
	int	cnt;

	cnt = sscanf(buf, "%d", &mode);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_flash_led_set_mode(mode) < 0)
		return -EFAULT;

	return size;
}

static int debug_flash_led_set_polarity(char *buf, int size)
{
	int	pol;
	int	cnt;

	cnt = sscanf(buf, "%d", &pol);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_flash_led_set_polarity(pol) < 0)
		return -EFAULT;

	return size;
}

static int debug_speaker_cmd(char *buf, int size)
{
	int	cmd;
	int	cnt;

	cnt = sscanf(buf, "%d", &cmd);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_speaker_cmd(cmd) < 0)
		return -EFAULT;

	return size;
}
static int debug_set_speaker_gain(char *buf, int size)
{
	int	gain;
	int	cnt;

	cnt = sscanf(buf, "%d", &gain);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_set_speaker_gain(gain) < 0)
		return -EFAULT;

	return size;
}

static int debug_mic_en(char *buf, int size)
{
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_mic_en(enable) < 0)
		return -EFAULT;

	return size;
}

static int debug_mic_is_en(char *buf, int size)
{
	int	enabled;

	if (pmic_mic_is_en(&enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}

static int debug_mic_set_volt(char *buf, int size)
{
	int	vol;
	int	cnt;

	cnt = sscanf(buf, "%d", &vol);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_mic_set_volt(vol) < 0)
		return -EFAULT;

	return size;
}

static int debug_mic_get_volt(char *buf, int size)
{
	uint vol;

	if (pmic_mic_get_volt(&vol) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", vol);
}

static int debug_spkr_en_right_chan(char *buf, int size)
{
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_en_right_chan(enable) < 0)
		return -EFAULT;

	return size;
}

static int debug_spkr_is_right_chan_en(char *buf, int size)
{
	int	enabled;

	if (pmic_spkr_is_right_chan_en(&enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}
static int debug_spkr_en_left_chan(char *buf, int size)
{
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_en_left_chan(enable) < 0)
		return -EFAULT;

	return size;
}

static int debug_spkr_is_left_chan_en(char *buf, int size)
{
	int	enabled;

	if (pmic_spkr_is_left_chan_en(&enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}

static int debug_set_spkr_configuration(char *buf, int size)
{

	struct spkr_config_mode	cfg;
	int	cnt;

	cnt = sscanf(buf, "%d %d %d %d %d %d %d %d",
		    &cfg.is_right_chan_en,
		    &cfg.is_left_chan_en,
		    &cfg.is_right_left_chan_added,
		    &cfg.is_stereo_en,
		    &cfg.is_usb_with_hpf_20hz,
		    &cfg.is_mux_bypassed,
		    &cfg.is_hpf_en,
		    &cfg.is_sink_curr_from_ref_volt_cir_en);

	if (cnt < 8) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}

	if (pmic_set_spkr_configuration(&cfg) < 0)
		return -EFAULT;

	return size;
}

static int debug_get_spkr_configuration(char *buf, int size)
{
	struct spkr_config_mode cfg;

	if (pmic_get_spkr_configuration(&cfg) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d %d %d %d %d %d %d %d\n",
				cfg.is_right_chan_en,
				cfg.is_left_chan_en,
				cfg.is_right_left_chan_added,
				cfg.is_stereo_en,
				cfg.is_usb_with_hpf_20hz,
				cfg.is_mux_bypassed,
				cfg.is_hpf_en,
				cfg.is_sink_curr_from_ref_volt_cir_en);

}

static int debug_set_speaker_delay(char *buf, int size)
{
	int	delay;
	int	cnt;

	cnt = sscanf(buf, "%d", &delay);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_set_speaker_delay(delay) < 0)
		return -EFAULT;

	return size;
}

static int debug_speaker_1k6_zin_enable(char *buf, int size)
{
	uint enable;
	int	cnt;

	cnt = sscanf(buf, "%u", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_speaker_1k6_zin_enable(enable) < 0)
		return -EFAULT;

	return size;
}

static int debug_spkr_set_mux_hpf_corner_freq(char *buf, int size)
{
	int	freq;
	int	cnt;

	cnt = sscanf(buf, "%d", &freq);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_set_mux_hpf_corner_freq(freq) < 0)
		return -EFAULT;

	return size;
}

static int debug_spkr_get_mux_hpf_corner_freq(char *buf, int size)
{
	uint freq;

	if (pmic_spkr_get_mux_hpf_corner_freq(&freq) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", freq);
}

static int debug_spkr_add_right_left_chan(char *buf, int size)
{
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_add_right_left_chan(enable) < 0)
		return -EFAULT;

	return size;
}

static int debug_spkr_is_right_left_chan_added(char *buf, int size)
{
	int	enabled;

	if (pmic_spkr_is_right_left_chan_added(&enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}

static int debug_spkr_en_stereo(char *buf, int size)
{
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_en_stereo(enable) < 0)
		return -EFAULT;

	return size;
}
static int debug_spkr_is_stereo_en(char *buf, int size)
{
	int	enabled;

	if (pmic_spkr_is_stereo_en(&enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}

static int debug_spkr_select_usb_with_hpf_20hz(char *buf, int size)
{
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_select_usb_with_hpf_20hz(enable) < 0)
		return -EFAULT;

	return size;
}
static int debug_spkr_is_usb_with_hpf_20hz(char *buf, int size)
{
	int	enabled;

	if (pmic_spkr_is_usb_with_hpf_20hz(&enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}

static int debug_spkr_bypass_mux(char *buf, int size)
{
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_bypass_mux(enable) < 0)
		return -EFAULT;

	return size;
}
static int debug_spkr_is_mux_bypassed(char *buf, int size)
{
	int	enabled;

	if (pmic_spkr_is_mux_bypassed(&enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}

static int debug_spkr_en_hpf(char *buf, int size)
{
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_en_hpf(enable) < 0)
		return -EFAULT;

	return size;
}
static int debug_spkr_is_hpf_en(char *buf, int size)
{
	int	enabled;

	if (pmic_spkr_is_hpf_en(&enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}

static int debug_spkr_en_sink_curr_from_ref_volt_cir(char *buf, int size)
{
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_en_sink_curr_from_ref_volt_cir(enable) < 0)
		return -EFAULT;

	return size;
}

static int debug_spkr_is_sink_curr_from_ref_volt_cir_en(char *buf, int size)
{
	int	enabled;

	if (pmic_spkr_is_sink_curr_from_ref_volt_cir_en(&enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}

static int debug_vib_mot_set_volt(char *buf, int size)
{
	int vol;
	int	cnt;

	cnt = sscanf(buf, "%d", &vol);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_vib_mot_set_volt(vol) < 0)
		return -EFAULT;

	return size;
}
static int debug_vib_mot_set_mode(char *buf, int size)
{
	int mode;
	int	cnt;

	cnt = sscanf(buf, "%d", &mode);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_vib_mot_set_mode(mode) < 0)
		return -EFAULT;

	return size;
}

static int debug_vib_mot_set_polarity(char *buf, int size)
{
	int	pol;
	int	cnt;

	cnt = sscanf(buf, "%d", &pol);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_vib_mot_set_polarity(pol) < 0)
		return -EFAULT;

	return size;
}
static int debug_vid_en(char *buf, int size)
{
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_vid_en(enable) < 0)
		return -EFAULT;

	return size;
}
static int debug_vid_is_en(char *buf, int size)
{
	int	enabled;

	if (pmic_vid_is_en(&enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}

static int debug_vid_load_detect_en(char *buf, int size)
{
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d", &enable);
	if (cnt < 1) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_vid_load_detect_en(enable) < 0)
		return -EFAULT;

	return size;
}

static enum spkr_left_right debug_spkr_left_right = LEFT_SPKR;

static int debug_spkr_en(char *buf, int size)
{
	int	left_right;
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d %d", &left_right, &enable);
	if (cnt < 2) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_en(left_right, enable) >= 0) {
		debug_spkr_left_right = left_right;
		return size;
	}
	return -EFAULT;
}

static int debug_spkr_is_en(char *buf, int size)
{
	int	enabled;

	if (pmic_spkr_is_en(debug_spkr_left_right, &enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}

static int debug_spkr_set_gain(char *buf, int size)
{
	int	left_right;
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d %d", &left_right, &enable);
	if (cnt < 2) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_set_gain(left_right, enable) >= 0) {
		debug_spkr_left_right = left_right;
		return size;
	}
	return -EFAULT;
}

static int debug_spkr_get_gain(char *buf, int size)
{
	uint gain;

	if (pmic_spkr_get_gain(debug_spkr_left_right, &gain) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", gain);
}
static int debug_spkr_set_delay(char *buf, int size)
{
	int	left_right;
	int	delay;
	int	cnt;

	cnt = sscanf(buf, "%d %d", &left_right, &delay);
	if (cnt < 2) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_set_delay(left_right, delay) >= 0) {
		debug_spkr_left_right = left_right;
		return size;
	}
	return -EFAULT;
}

static int debug_spkr_get_delay(char *buf, int size)
{
	uint delay;

	if (pmic_spkr_get_delay(debug_spkr_left_right, &delay) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", delay);
}

static int debug_spkr_en_mute(char *buf, int size)
{
	int	left_right;
	int	enable;
	int	cnt;

	cnt = sscanf(buf, "%d %d", &left_right, &enable);
	if (cnt < 2) {
		printk(KERN_ERR "%s: sscanf failed cnt=%d" , __func__, cnt);
		return -EINVAL;
	}
	if (pmic_spkr_en_mute(left_right, enable) >= 0) {
		debug_spkr_left_right = left_right;
		return size;
	}
	return -EFAULT;
}

static int debug_spkr_is_mute_en(char *buf, int size)
{
	int	enabled;

	if (pmic_spkr_is_mute_en(debug_spkr_left_right, &enabled) < 0)
		return -EFAULT;

	return snprintf(buf, size, "%d\n", enabled);
}


struct	pmic_debug_desc {
	int (*get) (char *, int);
	int (*set) (char *, int);
};

struct pmic_debug_desc pmic_debug[] = {
	{NULL, NULL},	
	{NULL, NULL}, 
	{NULL, debug_lp_mode_control}, 
	{NULL, debug_vreg_set_level}, 
	{NULL, debug_vreg_pull_down_switch}, 
	{NULL, debug_secure_mpp_control_digital_output},
				
	
	{NULL, debug_secure_mpp_config_i_sink},
	{NULL, debug_rtc_start}, 
	{NULL, debug_rtc_stop}, 
	{debug_rtc_get_time, NULL}, 
	{NULL, debug_rtc_enable_alarm}, 
	{NULL , debug_rtc_disable_alarm}, 
	{debug_rtc_get_alarm_time, NULL}, 
	{debug_rtc_get_alarm_status, NULL}, 
	{NULL, debug_rtc_set_time_adjust}, 
	{debug_rtc_get_time_adjust, NULL}, 
	{NULL, debug_set_led_intensity}, 
	{NULL, debug_flash_led_set_current}, 
	{NULL, debug_flash_led_set_mode}, 
	{NULL, debug_flash_led_set_polarity}, 
	{NULL, debug_speaker_cmd}, 
	{NULL, debug_set_speaker_gain}, 
	{NULL, debug_vib_mot_set_volt}, 
	{NULL, debug_vib_mot_set_mode}, 
	{NULL, debug_vib_mot_set_polarity}, 
	{NULL, debug_vid_en}, 
	{debug_vid_is_en, NULL}, 
	{NULL, debug_vid_load_detect_en}, 
	{NULL, debug_mic_en}, 
	{debug_mic_is_en, NULL}, 
	{NULL, debug_mic_set_volt}, 
	{debug_mic_get_volt, NULL}, 
	{NULL, debug_spkr_en_right_chan}, 
	{debug_spkr_is_right_chan_en, NULL}, 
	{NULL, debug_spkr_en_left_chan}, 
	{debug_spkr_is_left_chan_en, NULL}, 
	{NULL, debug_set_spkr_configuration}, 
	{debug_get_spkr_configuration, NULL}, 
	{debug_spkr_get_gain, NULL}, 
	{debug_spkr_is_en, NULL}, 
	{NULL, debug_spkr_en_mute}, 
	{debug_spkr_is_mute_en, NULL}, 
	{NULL, debug_spkr_set_delay}, 
	{debug_spkr_get_delay, NULL}, 
	
	{NULL, debug_secure_mpp_config_digital_input},
	{NULL, debug_set_speaker_delay}, 
	{NULL, debug_speaker_1k6_zin_enable}, 
	
	{NULL, debug_spkr_set_mux_hpf_corner_freq},
	
	{debug_spkr_get_mux_hpf_corner_freq, NULL},
	
	{debug_spkr_is_right_left_chan_added, NULL},
	{NULL, debug_spkr_en_stereo}, 
	{debug_spkr_is_stereo_en, NULL}, 
	 
	{NULL, debug_spkr_select_usb_with_hpf_20hz},
	
	{debug_spkr_is_usb_with_hpf_20hz, NULL},
	{NULL, debug_spkr_bypass_mux}, 
	{debug_spkr_is_mux_bypassed, NULL}, 
	{NULL, debug_spkr_en_hpf}, 
	{ debug_spkr_is_hpf_en, NULL}, 
	
	{NULL, debug_spkr_en_sink_curr_from_ref_volt_cir},
	
	{debug_spkr_is_sink_curr_from_ref_volt_cir_en, NULL},
	
	{NULL, debug_spkr_add_right_left_chan},
	{NULL, debug_spkr_set_gain}, 
	{NULL , debug_spkr_en}, 
};


#define PROC_END (sizeof(pmic_debug)/sizeof(struct pmic_debug_desc))


#define PMIC_DEBUG_BUF	512

static int	debug_proc;		

static char	debug_buf[PMIC_DEBUG_BUF];

static int proc_index_set(void *data, u64 val)
{
	int	ndx;

	ndx = (int)val;

	if (ndx >= 0 && ndx <= PROC_END)
		debug_proc = ndx;

	return 0;
}

static int proc_index_get(void *data, u64 *val)
{
	*val = (u64)debug_proc;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(
			proc_index_fops,
			proc_index_get,
			proc_index_set,
			"%llu\n");


static int pmic_debugfs_open(struct inode *inode, struct file *file)
{
	
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	return 0;
}

static int pmic_debugfs_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t pmic_debugfs_write(
	struct file *file,
	const char __user *buff,
	size_t count,
	loff_t *ppos)
{
	struct pmic_debug_desc *pd;
	int len = 0;

	printk(KERN_INFO "%s: proc=%d count=%d *ppos=%d\n",
			__func__, debug_proc, count, (uint)*ppos);

	if (count > sizeof(debug_buf))
		return -EFAULT;

	if (copy_from_user(debug_buf, buff, count))
		return -EFAULT;


	debug_buf[count] = 0;	

	pd = &pmic_debug[debug_proc];

	if (pd->set) {
		len = pd->set(debug_buf, count);
		printk(KERN_INFO "%s: len=%d\n", __func__, len);
		return len;
	}

	return 0;
}

static ssize_t pmic_debugfs_read(
	struct file *file,
	char __user *buff,
	size_t count,
	loff_t *ppos)
{
	struct pmic_debug_desc *pd;
	int len = 0;

	printk(KERN_INFO "%s: proc=%d count=%d *ppos=%d\n",
			__func__, debug_proc, count, (uint)*ppos);

	pd = &pmic_debug[debug_proc];

	if (*ppos)
		return 0;	

	if (pd->get) {
		len = pd->get(debug_buf, sizeof(debug_buf));
		if (len > 0) {
			if (len > count)
				len = count;
			if (copy_to_user(buff, debug_buf, len))
				return -EFAULT;
		}
	}

	printk(KERN_INFO "%s: len=%d\n", __func__, len);

	if (len < 0)
		return 0;

	*ppos += len;	

	return len;
}

static const struct file_operations pmic_debugfs_fops = {
	.open = pmic_debugfs_open,
	.release = pmic_debugfs_release,
	.read = pmic_debugfs_read,
	.write = pmic_debugfs_write,
};

static int __init pmic_debugfs_init(void)
{
	struct dentry *dent = debugfs_create_dir("pmic", NULL);

	if (IS_ERR(dent)) {
		printk(KERN_ERR "%s(%d): debugfs_create_dir fail, error %ld\n",
			__FILE__, __LINE__, PTR_ERR(dent));
		return -1;
	}

	if (debugfs_create_file("index", 0644, dent, 0, &proc_index_fops)
			== NULL) {
		printk(KERN_ERR "%s(%d): debugfs_create_file: index fail\n",
			__FILE__, __LINE__);
		return -1;
	}

	if (debugfs_create_file("debug", 0644, dent, 0, &pmic_debugfs_fops)
			== NULL) {
		printk(KERN_ERR "%s(%d): debugfs_create_file: debug fail\n",
			__FILE__, __LINE__);
		return -1;
	}

	debug_proc = 0;
	debug_rtc_alarm_ndx = 0;

	return 0;
}

late_initcall(pmic_debugfs_init);
