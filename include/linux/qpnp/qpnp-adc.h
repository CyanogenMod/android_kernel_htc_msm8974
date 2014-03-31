/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef __QPNP_ADC_H
#define __QPNP_ADC_H

#include <linux/kernel.h>
#include <linux/list.h>
enum qpnp_vadc_channels {
	USBIN = 0,
	DCIN,
	VCHG_SNS,
	SPARE1_03,
	USB_ID_MV,
	VCOIN,
	VBAT_SNS,
	VSYS,
	DIE_TEMP,
	REF_625MV,
	REF_125V,
	CHG_TEMP,
	SPARE1,
	SPARE2,
	GND_REF,
	VDD_VADC,
	P_MUX1_1_1,
	P_MUX2_1_1,
	P_MUX3_1_1,
	P_MUX4_1_1,
	P_MUX5_1_1,
	P_MUX6_1_1,
	P_MUX7_1_1,
	P_MUX8_1_1,
	P_MUX9_1_1,
	P_MUX10_1_1,
	P_MUX11_1_1,
	P_MUX12_1_1,
	P_MUX13_1_1,
	P_MUX14_1_1,
	P_MUX15_1_1,
	P_MUX16_1_1,
	P_MUX1_1_3,
	P_MUX2_1_3,
	P_MUX3_1_3,
	P_MUX4_1_3,
	P_MUX5_1_3,
	P_MUX6_1_3,
	P_MUX7_1_3,
	P_MUX8_1_3,
	P_MUX9_1_3,
	P_MUX10_1_3,
	P_MUX11_1_3,
	P_MUX12_1_3,
	P_MUX13_1_3,
	P_MUX14_1_3,
	P_MUX15_1_3,
	P_MUX16_1_3,
	LR_MUX1_BATT_THERM,
	LR_MUX2_BAT_ID,
	LR_MUX3_XO_THERM,
	LR_MUX4_AMUX_THM1,
	LR_MUX5_AMUX_THM2,
	LR_MUX6_AMUX_THM3,
	LR_MUX7_HW_ID,
	LR_MUX8_AMUX_THM4,
	LR_MUX9_AMUX_THM5,
	LR_MUX10_USB_ID_LV,
	AMUX_PU1,
	AMUX_PU2,
	LR_MUX3_BUF_XO_THERM_BUF,
	LR_MUX1_PU1_BAT_THERM = 112,
	LR_MUX2_PU1_BAT_ID = 113,
	LR_MUX3_PU1_XO_THERM = 114,
	LR_MUX4_PU1_AMUX_THM1 = 115,
	LR_MUX5_PU1_AMUX_THM2 = 116,
	LR_MUX6_PU1_AMUX_THM3 = 117,
	LR_MUX7_PU1_AMUX_HW_ID = 118,
	LR_MUX8_PU1_AMUX_THM4 = 119,
	LR_MUX9_PU1_AMUX_THM5 = 120,
	LR_MUX10_PU1_AMUX_USB_ID_LV = 121,
	LR_MUX3_BUF_PU1_XO_THERM_BUF = 124,
	LR_MUX1_PU2_BAT_THERM = 176,
	LR_MUX2_PU2_BAT_ID = 177,
	LR_MUX3_PU2_XO_THERM = 178,
	LR_MUX4_PU2_AMUX_THM1 = 179,
	LR_MUX5_PU2_AMUX_THM2 = 180,
	LR_MUX6_PU2_AMUX_THM3 = 181,
	LR_MUX7_PU2_AMUX_HW_ID = 182,
	LR_MUX8_PU2_AMUX_THM4 = 183,
	LR_MUX9_PU2_AMUX_THM5 = 184,
	LR_MUX10_PU2_AMUX_USB_ID_LV = 185,
	LR_MUX3_BUF_PU2_XO_THERM_BUF = 188,
	LR_MUX1_PU1_PU2_BAT_THERM = 240,
	LR_MUX2_PU1_PU2_BAT_ID = 241,
	LR_MUX3_PU1_PU2_XO_THERM = 242,
	LR_MUX4_PU1_PU2_AMUX_THM1 = 243,
	LR_MUX5_PU1_PU2_AMUX_THM2 = 244,
	LR_MUX6_PU1_PU2_AMUX_THM3 = 245,
	LR_MUX7_PU1_PU2_AMUX_HW_ID = 246,
	LR_MUX8_PU1_PU2_AMUX_THM4 = 247,
	LR_MUX9_PU1_PU2_AMUX_THM5 = 248,
	LR_MUX10_PU1_PU2_AMUX_USB_ID_LV = 249,
	LR_MUX3_BUF_PU1_PU2_XO_THERM_BUF = 252,
	ALL_OFF = 255,
	ADC_MAX_NUM,
};

enum qpnp_iadc_channels {
	INTERNAL_RSENSE = 0,
	EXTERNAL_RSENSE,
	ALT_LEAD_PAIR,
	GAIN_CALIBRATION_17P857MV,
	OFFSET_CALIBRATION_SHORT_CADC_LEADS,
	OFFSET_CALIBRATION_CSP_CSN,
	OFFSET_CALIBRATION_CSP2_CSN2,
	IADC_MUX_NUM,
};

#define QPNP_ADC_625_UV	625000
#define QPNP_ADC_HWMON_NAME_LENGTH				64
#define QPNP_MAX_PROP_NAME_LEN					32

struct qpnp_vadc_chip;

struct qpnp_iadc_chip;

struct qpnp_adc_tm_chip;

enum qpnp_adc_decimation_type {
	DECIMATION_TYPE1 = 0,
	DECIMATION_TYPE2,
	DECIMATION_TYPE3,
	DECIMATION_TYPE4,
	DECIMATION_NONE,
};

enum qpnp_adc_calib_type {
	CALIB_ABSOLUTE = 0,
	CALIB_RATIOMETRIC,
	CALIB_NONE,
};

enum qpnp_adc_channel_scaling_param {
	PATH_SCALING0 = 0,
	PATH_SCALING1,
	PATH_SCALING2,
	PATH_SCALING3,
	PATH_SCALING4,
	PATH_SCALING_NONE,
};

enum qpnp_adc_scale_fn_type {
	SCALE_DEFAULT = 0,
	SCALE_BATT_THERM,
	SCALE_THERM_100K_PULLUP,
	SCALE_PMIC_THERM,
	SCALE_XOTHERM,
	SCALE_THERM_150K_PULLUP,
	SCALE_QRD_BATT_THERM,
	SCALE_QRD_SKUAA_BATT_THERM,
	SCALE_QRD_SKUG_BATT_THERM = 9,
	SCALE_NONE,
};


enum qpnp_adc_tm_rscale_fn_type {
	SCALE_R_VBATT = 0,
	SCALE_RBATT_THERM,
	SCALE_R_USB_ID,
	SCALE_RPMIC_THERM,
	SCALE_RSCALE_NONE,
};

enum qpnp_adc_fast_avg_ctl {
	ADC_FAST_AVG_SAMPLE_1 = 0,
	ADC_FAST_AVG_SAMPLE_2,
	ADC_FAST_AVG_SAMPLE_4,
	ADC_FAST_AVG_SAMPLE_8,
	ADC_FAST_AVG_SAMPLE_16,
	ADC_FAST_AVG_SAMPLE_32,
	ADC_FAST_AVG_SAMPLE_64,
	ADC_FAST_AVG_SAMPLE_128,
	ADC_FAST_AVG_SAMPLE_256,
	ADC_FAST_AVG_SAMPLE_512,
	ADC_FAST_AVG_SAMPLE_NONE,
};

enum qpnp_adc_hw_settle_time {
	ADC_CHANNEL_HW_SETTLE_DELAY_0US = 0,
	ADC_CHANNEL_HW_SETTLE_DELAY_100US,
	ADC_CHANNEL_HW_SETTLE_DELAY_2000US,
	ADC_CHANNEL_HW_SETTLE_DELAY_300US,
	ADC_CHANNEL_HW_SETTLE_DELAY_400US,
	ADC_CHANNEL_HW_SETTLE_DELAY_500US,
	ADC_CHANNEL_HW_SETTLE_DELAY_600US,
	ADC_CHANNEL_HW_SETTLE_DELAY_700US,
	ADC_CHANNEL_HW_SETTLE_DELAY_800US,
	ADC_CHANNEL_HW_SETTLE_DELAY_900US,
	ADC_CHANNEL_HW_SETTLE_DELAY_1MS,
	ADC_CHANNEL_HW_SETTLE_DELAY_2MS,
	ADC_CHANNEL_HW_SETTLE_DELAY_4MS,
	ADC_CHANNEL_HW_SETTLE_DELAY_6MS,
	ADC_CHANNEL_HW_SETTLE_DELAY_8MS,
	ADC_CHANNEL_HW_SETTLE_DELAY_10MS,
	ADC_CHANNEL_HW_SETTLE_NONE,
};

enum qpnp_vadc_mode_sel {
	ADC_OP_NORMAL_MODE = 0,
	ADC_OP_CONVERSION_SEQUENCER,
	ADC_OP_MEASUREMENT_INTERVAL,
	ADC_OP_MODE_NONE,
};

enum qpnp_vadc_trigger {
	ADC_GSM_PA_ON = 0,
	ADC_TX_GTR_THRES,
	ADC_CAMERA_FLASH_RAMP,
	ADC_DTEST,
	ADC_SEQ_NONE,
};

enum qpnp_vadc_conv_seq_timeout {
	ADC_CONV_SEQ_TIMEOUT_0MS = 0,
	ADC_CONV_SEQ_TIMEOUT_1MS,
	ADC_CONV_SEQ_TIMEOUT_2MS,
	ADC_CONV_SEQ_TIMEOUT_3MS,
	ADC_CONV_SEQ_TIMEOUT_4MS,
	ADC_CONV_SEQ_TIMEOUT_5MS,
	ADC_CONV_SEQ_TIMEOUT_6MS,
	ADC_CONV_SEQ_TIMEOUT_7MS,
	ADC_CONV_SEQ_TIMEOUT_8MS,
	ADC_CONV_SEQ_TIMEOUT_9MS,
	ADC_CONV_SEQ_TIMEOUT_10MS,
	ADC_CONV_SEQ_TIMEOUT_11MS,
	ADC_CONV_SEQ_TIMEOUT_12MS,
	ADC_CONV_SEQ_TIMEOUT_13MS,
	ADC_CONV_SEQ_TIMEOUT_14MS,
	ADC_CONV_SEQ_TIMEOUT_15MS,
	ADC_CONV_SEQ_TIMEOUT_NONE,
};

enum qpnp_adc_conv_seq_holdoff {
	ADC_SEQ_HOLD_25US = 0,
	ADC_SEQ_HOLD_50US,
	ADC_SEQ_HOLD_75US,
	ADC_SEQ_HOLD_100US,
	ADC_SEQ_HOLD_125US,
	ADC_SEQ_HOLD_150US,
	ADC_SEQ_HOLD_175US,
	ADC_SEQ_HOLD_200US,
	ADC_SEQ_HOLD_225US,
	ADC_SEQ_HOLD_250US,
	ADC_SEQ_HOLD_275US,
	ADC_SEQ_HOLD_300US,
	ADC_SEQ_HOLD_325US,
	ADC_SEQ_HOLD_350US,
	ADC_SEQ_HOLD_375US,
	ADC_SEQ_HOLD_400US,
	ADC_SEQ_HOLD_NONE,
};

enum qpnp_adc_conv_seq_state {
	ADC_CONV_SEQ_IDLE = 0,
	ADC_CONV_TRIG_RISE,
	ADC_CONV_TRIG_HOLDOFF,
	ADC_CONV_MEAS_RISE,
	ADC_CONV_TRIG_FALL,
	ADC_CONV_FALL_HOLDOFF,
	ADC_CONV_MEAS_FALL,
	ADC_CONV_ERROR,
	ADC_CONV_NONE,
};

enum qpnp_adc_meas_timer_1 {
	ADC_MEAS1_INTERVAL_0MS = 0,
	ADC_MEAS1_INTERVAL_1P0MS,
	ADC_MEAS1_INTERVAL_2P0MS,
	ADC_MEAS1_INTERVAL_3P9MS,
	ADC_MEAS1_INTERVAL_7P8MS,
	ADC_MEAS1_INTERVAL_15P6MS,
	ADC_MEAS1_INTERVAL_31P3MS,
	ADC_MEAS1_INTERVAL_62P5MS,
	ADC_MEAS1_INTERVAL_125MS,
	ADC_MEAS1_INTERVAL_250MS,
	ADC_MEAS1_INTERVAL_500MS,
	ADC_MEAS1_INTERVAL_1S,
	ADC_MEAS1_INTERVAL_2S,
	ADC_MEAS1_INTERVAL_4S,
	ADC_MEAS1_INTERVAL_8S,
	ADC_MEAS1_INTERVAL_16S,
	ADC_MEAS1_INTERVAL_NONE,
};

enum qpnp_adc_meas_timer_2 {
	ADC_MEAS2_INTERVAL_0MS = 0,
	ADC_MEAS2_INTERVAL_100MS,
	ADC_MEAS2_INTERVAL_200MS,
	ADC_MEAS2_INTERVAL_300MS,
	ADC_MEAS2_INTERVAL_400MS,
	ADC_MEAS2_INTERVAL_500MS,
	ADC_MEAS2_INTERVAL_600MS,
	ADC_MEAS2_INTERVAL_700MS,
	ADC_MEAS2_INTERVAL_800MS,
	ADC_MEAS2_INTERVAL_900MS,
	ADC_MEAS2_INTERVAL_1S,
	ADC_MEAS2_INTERVAL_1P1S,
	ADC_MEAS2_INTERVAL_1P2S,
	ADC_MEAS2_INTERVAL_1P3S,
	ADC_MEAS2_INTERVAL_1P4S,
	ADC_MEAS2_INTERVAL_1P5S,
	ADC_MEAS2_INTERVAL_NONE,
};

enum qpnp_adc_meas_timer_3 {
	ADC_MEAS3_INTERVAL_0S = 0,
	ADC_MEAS3_INTERVAL_1S,
	ADC_MEAS3_INTERVAL_2S,
	ADC_MEAS3_INTERVAL_3S,
	ADC_MEAS3_INTERVAL_4S,
	ADC_MEAS3_INTERVAL_5S,
	ADC_MEAS3_INTERVAL_6S,
	ADC_MEAS3_INTERVAL_7S,
	ADC_MEAS3_INTERVAL_8S,
	ADC_MEAS3_INTERVAL_9S,
	ADC_MEAS3_INTERVAL_10S,
	ADC_MEAS3_INTERVAL_11S,
	ADC_MEAS3_INTERVAL_12S,
	ADC_MEAS3_INTERVAL_13S,
	ADC_MEAS3_INTERVAL_14S,
	ADC_MEAS3_INTERVAL_15S,
	ADC_MEAS3_INTERVAL_NONE,
};

enum qpnp_adc_meas_timer_select {
	ADC_MEAS_TIMER_SELECT1 = 0,
	ADC_MEAS_TIMER_SELECT2,
	ADC_MEAS_TIMER_SELECT3,
	ADC_MEAS_TIMER_NUM,
};

enum qpnp_adc_meas_interval_op_ctl {
	ADC_MEAS_INTERVAL_OP_SINGLE = 0,
	ADC_MEAS_INTERVAL_OP_CONTINUOUS,
	ADC_MEAS_INTERVAL_OP_NONE,
};

enum qpnp_adc_tm_channel_select	{
	QPNP_ADC_TM_M0_ADC_CH_SEL_CTL = 0x48,
	QPNP_ADC_TM_M1_ADC_CH_SEL_CTL = 0x68,
	QPNP_ADC_TM_M2_ADC_CH_SEL_CTL = 0x70,
	QPNP_ADC_TM_M3_ADC_CH_SEL_CTL = 0x78,
	QPNP_ADC_TM_M4_ADC_CH_SEL_CTL = 0x80,
	QPNP_ADC_TM_M5_ADC_CH_SEL_CTL = 0x88,
	QPNP_ADC_TM_M6_ADC_CH_SEL_CTL = 0x90,
	QPNP_ADC_TM_M7_ADC_CH_SEL_CTL = 0x98,
	QPNP_ADC_TM_CH_SELECT_NONE
};

enum qpnp_adc_tm_channel_num {
	QPNP_ADC_TM_CHAN0 = 0,
	QPNP_ADC_TM_CHAN1,
	QPNP_ADC_TM_CHAN2,
	QPNP_ADC_TM_CHAN3,
	QPNP_ADC_TM_CHAN4,
	QPNP_ADC_TM_CHAN5,
	QPNP_ADC_TM_CHAN6,
	QPNP_ADC_TM_CHAN7,
	QPNP_ADC_TM_CHAN_NONE
};

enum qpnp_comp_scheme_type {
	COMP_ID_GF = 0,
	COMP_ID_SMIC,
	COMP_ID_TSMC,
	COMP_ID_NUM,
};

struct qpnp_adc_tm_config {
	int	channel;
	int	adc_code;
	int	high_thr_temp;
	int	low_thr_temp;
	int64_t	high_thr_voltage;
	int64_t	low_thr_voltage;
};

enum qpnp_adc_tm_trip_type {
	ADC_TM_TRIP_HIGH_WARM = 0,
	ADC_TM_TRIP_LOW_COOL,
	ADC_TM_TRIP_NUM,
};

enum qpnp_tm_state {
	ADC_TM_HIGH_STATE = 0,
	ADC_TM_COOL_STATE = ADC_TM_HIGH_STATE,
	ADC_TM_LOW_STATE,
	ADC_TM_WARM_STATE = ADC_TM_LOW_STATE,
	ADC_TM_STATE_NUM,
};

enum qpnp_state_request {
	ADC_TM_HIGH_THR_ENABLE = 0,
	ADC_TM_COOL_THR_ENABLE = ADC_TM_HIGH_THR_ENABLE,
	ADC_TM_LOW_THR_ENABLE,
	ADC_TM_WARM_THR_ENABLE = ADC_TM_LOW_THR_ENABLE,
	ADC_TM_HIGH_LOW_THR_ENABLE,
	ADC_TM_HIGH_THR_DISABLE,
	ADC_TM_COOL_THR_DISABLE = ADC_TM_HIGH_THR_DISABLE,
	ADC_TM_LOW_THR_DISABLE,
	ADC_TM_WARM_THR_DISABLE = ADC_TM_LOW_THR_DISABLE,
	ADC_TM_HIGH_LOW_THR_DISABLE,
	ADC_TM_THR_NUM,
};

struct qpnp_adc_tm_btm_param {
	int32_t					high_temp;
	int32_t					low_temp;
	int32_t					high_thr;
	int32_t					low_thr;
	enum qpnp_vadc_channels			channel;
	enum qpnp_state_request			state_request;
	enum qpnp_adc_meas_timer_1		timer_interval;
	enum qpnp_adc_meas_timer_2		timer_interval2;
	enum qpnp_adc_meas_timer_3		timer_interval3;
	void					*btm_ctx;
	void	(*threshold_notification) (enum qpnp_tm_state state,
						void *ctx);
};

struct qpnp_vadc_linear_graph {
	int64_t dy;
	int64_t dx;
	int64_t adc_vref;
	int64_t adc_gnd;
};

struct qpnp_vadc_map_pt {
	int32_t x;
	int32_t y;
};

struct qpnp_vadc_scaling_ratio {
	int32_t num;
	int32_t den;
};

struct qpnp_adc_properties {
	uint32_t	adc_vdd_reference;
	uint32_t	bitresolution;
	bool		bipolar;
};

struct qpnp_vadc_chan_properties {
	uint32_t			offset_gain_numerator;
	uint32_t			offset_gain_denominator;
	uint32_t				high_thr;
	uint32_t				low_thr;
	enum qpnp_adc_meas_timer_select		timer_select;
	enum qpnp_adc_meas_timer_1		meas_interval1;
	enum qpnp_adc_meas_timer_2		meas_interval2;
	enum qpnp_adc_tm_channel_select		tm_channel_select;
	enum qpnp_state_request			state_request;
	struct qpnp_vadc_linear_graph	adc_graph[2];
};

struct qpnp_vadc_result {
	uint32_t	chan;
	int32_t		adc_code;
	int64_t		measurement;
	int64_t		physical;
};

struct qpnp_adc_amux {
	char					*name;
	enum qpnp_vadc_channels			channel_num;
	enum qpnp_adc_channel_scaling_param	chan_path_prescaling;
	enum qpnp_adc_decimation_type		adc_decimation;
	enum qpnp_adc_scale_fn_type		adc_scale_fn;
	enum qpnp_adc_fast_avg_ctl		fast_avg_setup;
	enum qpnp_adc_hw_settle_time		hw_settle_time;
};

static const struct qpnp_vadc_scaling_ratio qpnp_vadc_amux_scaling_ratio[] = {
	{1, 1},
	{1, 3},
	{1, 4},
	{1, 6},
	{1, 20}
};

struct qpnp_vadc_scale_fn {
	int32_t (*chan) (struct qpnp_vadc_chip *, int32_t,
		const struct qpnp_adc_properties *,
		const struct qpnp_vadc_chan_properties *,
		struct qpnp_vadc_result *);
};

struct qpnp_adc_tm_reverse_scale_fn {
	int32_t (*chan) (struct qpnp_vadc_chip *,
		struct qpnp_adc_tm_btm_param *,
		uint32_t *, uint32_t *);
};

struct qpnp_iadc_calib {
	enum qpnp_iadc_channels		channel;
	uint16_t			offset_raw;
	uint16_t			gain_raw;
	uint32_t			ideal_offset_uv;
	uint32_t			ideal_gain_nv;
	uint32_t			offset_uv;
	uint32_t			gain_uv;
};

struct qpnp_iadc_result {
	int32_t				result_uv;
	int32_t				result_ua;
};

/**
 * struct qpnp_adc_drv - QPNP ADC device structure.
 * @spmi - spmi device for ADC peripheral.
 * @offset - base offset for the ADC peripheral.
 * @adc_prop - ADC properties specific to the ADC peripheral.
 * @amux_prop - AMUX properties representing the ADC peripheral.
 * @adc_channels - ADC channel properties for the ADC peripheral.
 * @adc_irq_eoc - End of Conversion IRQ.
 * @adc_irq_fifo_not_empty - Conversion sequencer request written
 *			to FIFO when not empty.
 * @adc_irq_conv_seq_timeout - Conversion sequencer trigger timeout.
 * @adc_high_thr_irq - Output higher than high threshold set for measurement.
 * @adc_low_thr_irq - Output lower than low threshold set for measurement.
 * @adc_lock - ADC lock for access to the peripheral.
 * @adc_rslt_completion - ADC result notification after interrupt
 *			  is received.
 * @calib - Internal rsens calibration values for gain and offset.
 */
struct qpnp_adc_drv {
	struct spmi_device		*spmi;
	uint8_t				slave;
	uint16_t			offset;
	struct qpnp_adc_properties	*adc_prop;
	struct qpnp_adc_amux_properties	*amux_prop;
	struct qpnp_adc_amux		*adc_channels;
	int				adc_irq_eoc;
	int				adc_irq_fifo_not_empty;
	int				adc_irq_conv_seq_timeout;
	int				adc_high_thr_irq;
	int				adc_low_thr_irq;
	struct mutex			adc_lock;
	struct completion		adc_rslt_completion;
	struct qpnp_iadc_calib		calib;
};

struct qpnp_adc_amux_properties {
	uint32_t			amux_channel;
	uint32_t			decimation;
	uint32_t			mode_sel;
	uint32_t			hw_settle_time;
	uint32_t			fast_avg_setup;
	enum qpnp_vadc_trigger		trigger_channel;
	struct qpnp_vadc_chan_properties	chan_prop[0];
};

#if defined(CONFIG_SENSORS_QPNP_ADC_VOLTAGE)				\
			|| defined(CONFIG_SENSORS_QPNP_ADC_VOLTAGE_MODULE)
int32_t qpnp_vadc_read(struct qpnp_vadc_chip *dev,
				enum qpnp_vadc_channels channel,
				struct qpnp_vadc_result *result);

int32_t qpnp_vadc_conv_seq_request(struct qpnp_vadc_chip *dev,
			enum qpnp_vadc_trigger trigger_channel,
			enum qpnp_vadc_channels channel,
			struct qpnp_vadc_result *result);

int32_t qpnp_vadc_check_result(int32_t *data);

int32_t qpnp_adc_get_devicetree_data(struct spmi_device *spmi,
					struct qpnp_adc_drv *adc_qpnp);

int32_t qpnp_adc_scale_default(struct qpnp_vadc_chip *dev,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
int32_t qpnp_adc_scale_pmic_therm(struct qpnp_vadc_chip *dev,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
int32_t qpnp_adc_scale_batt_therm(struct qpnp_vadc_chip *dev,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
int32_t qpnp_adc_scale_qrd_batt_therm(struct qpnp_vadc_chip *dev,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
int32_t qpnp_adc_scale_qrd_skuaa_batt_therm(struct qpnp_vadc_chip *dev,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
int32_t qpnp_adc_scale_qrd_skug_batt_therm(struct qpnp_vadc_chip *dev,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
int32_t qpnp_adc_scale_batt_id(struct qpnp_vadc_chip *dev, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
int32_t qpnp_adc_tdkntcg_therm(struct qpnp_vadc_chip *dev, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
int32_t qpnp_adc_scale_therm_pu1(struct qpnp_vadc_chip *dev, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
int32_t qpnp_adc_scale_therm_pu2(struct qpnp_vadc_chip *dev, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt);
struct qpnp_vadc_chip *qpnp_get_vadc(struct device *dev, const char *name);
static inline int32_t qpnp_adc_tm_scaler(struct qpnp_adc_tm_config *tm_config,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop)
{ return -ENXIO; }
int32_t qpnp_get_vadc_gain_and_offset(struct qpnp_vadc_chip *dev,
			struct qpnp_vadc_linear_graph *param,
			enum qpnp_adc_calib_type calib_type);
int32_t qpnp_adc_scale_millidegc_pmic_voltage_thr(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold);
int32_t qpnp_adc_btm_scaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold);
int32_t qpnp_adc_tm_scale_therm_voltage_pu2(struct qpnp_vadc_chip *dev,
				struct qpnp_adc_tm_config *param);
int32_t qpnp_adc_tm_scale_voltage_therm_pu2(struct qpnp_vadc_chip *dev,
				uint32_t reg, int64_t *result);
int32_t qpnp_adc_usb_scaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold);
int32_t qpnp_adc_vbatt_rscaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold);
int32_t qpnp_vadc_iadc_sync_request(struct qpnp_vadc_chip *dev,
				enum qpnp_vadc_channels channel);
int32_t qpnp_vadc_iadc_sync_complete_request(struct qpnp_vadc_chip *dev,
	enum qpnp_vadc_channels channel, struct qpnp_vadc_result *result);
int32_t qpnp_vbat_sns_comp_result(struct qpnp_vadc_chip *dev,
						int64_t *result);
#else
static inline int32_t qpnp_vadc_read(struct qpnp_vadc_chip *dev,
				uint32_t channel,
				struct qpnp_vadc_result *result)
{ return -ENXIO; }
static inline int32_t qpnp_vadc_conv_seq_request(struct qpnp_vadc_chip *dev,
			enum qpnp_vadc_trigger trigger_channel,
			enum qpnp_vadc_channels channel,
			struct qpnp_vadc_result *result)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_default(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_pmic_therm(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_batt_therm(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_qrd_batt_therm(
			struct qpnp_vadc_chip *vadc, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_qrd_skuaa_batt_therm(
			struct qpnp_vadc_chip *vadc, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_qrd_skug_batt_therm(
			struct qpnp_vadc_chip *vadc, int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_batt_id(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tdkntcg_therm(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_therm_pu1(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_therm_pu2(struct qpnp_vadc_chip *vadc,
			int32_t adc_code,
			const struct qpnp_adc_properties *adc_prop,
			const struct qpnp_vadc_chan_properties *chan_prop,
			struct qpnp_vadc_result *chan_rslt)
{ return -ENXIO; }
static inline struct qpnp_vadc_chip *qpnp_get_vadc(struct device *dev,
							const char *name)
{ return ERR_PTR(-ENXIO); }
static inline int32_t qpnp_get_vadc_gain_and_offset(struct qpnp_vadc_chip *dev,
			struct qpnp_vadc_linear_graph *param,
			enum qpnp_adc_calib_type calib_type)
{ return -ENXIO; }
static inline int32_t qpnp_adc_usb_scaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{ return -ENXIO; }
static inline int32_t qpnp_adc_vbatt_rscaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{ return -ENXIO; }
static inline int32_t qpnp_adc_btm_scaler(struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{ return -ENXIO; }
static inline int32_t qpnp_adc_scale_millidegc_pmic_voltage_thr(
		struct qpnp_vadc_chip *dev,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tm_scale_therm_voltage_pu2(
				struct qpnp_vadc_chip *dev,
				struct qpnp_adc_tm_config *param)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tm_scale_voltage_therm_pu2(
				struct qpnp_vadc_chip *dev,
				uint32_t reg, int64_t *result)
{ return -ENXIO; }
static inline int32_t qpnp_vadc_iadc_sync_request(struct qpnp_vadc_chip *dev,
				enum qpnp_vadc_channels channel)
{ return -ENXIO; }
static inline int32_t qpnp_vadc_iadc_sync_complete_request(
				struct qpnp_vadc_chip *dev,
				enum qpnp_vadc_channels channel,
				struct qpnp_vadc_result *result)
{ return -ENXIO; }
static inline int32_t qpnp_vbat_sns_comp_result(struct qpnp_vadc_chip *dev,
						int64_t *result)
{ return -ENXIO; }
#endif

#if defined(CONFIG_SENSORS_QPNP_ADC_CURRENT)				\
			|| defined(CONFIG_SENSORS_QPNP_ADC_CURRENT_MODULE)
int32_t qpnp_iadc_read(struct qpnp_iadc_chip *dev,
				enum qpnp_iadc_channels channel,
				struct qpnp_iadc_result *result);
int32_t qpnp_iadc_get_rsense(struct qpnp_iadc_chip *dev, int32_t *rsense);
int32_t qpnp_iadc_get_gain_and_offset(struct qpnp_iadc_chip *dev,
					struct qpnp_iadc_calib *result);
struct qpnp_iadc_chip *qpnp_get_iadc(struct device *dev, const char *name);
int32_t qpnp_iadc_vadc_sync_read(struct qpnp_iadc_chip *dev,
	enum qpnp_iadc_channels i_channel, struct qpnp_iadc_result *i_result,
	enum qpnp_vadc_channels v_channel, struct qpnp_vadc_result *v_result);
int32_t qpnp_iadc_calibrate_for_trim(struct qpnp_iadc_chip *dev,
						bool batfet_closed);
int32_t qpnp_iadc_comp_result(struct qpnp_iadc_chip *dev, int64_t *result);
int qpnp_iadc_skip_calibration(struct qpnp_iadc_chip *dev);
int qpnp_iadc_resume_calibration(struct qpnp_iadc_chip *dev);
#else
static inline int32_t qpnp_iadc_read(struct qpnp_iadc_chip *iadc,
	enum qpnp_iadc_channels channel, struct qpnp_iadc_result *result)
{ return -ENXIO; }
static inline int32_t qpnp_iadc_get_rsense(struct qpnp_iadc_chip *iadc,
							int32_t *rsense)
{ return -ENXIO; }
static inline int32_t qpnp_iadc_get_gain_and_offset(struct qpnp_iadc_chip *iadc,
				struct qpnp_iadc_calib *result)
{ return -ENXIO; }
static inline struct qpnp_iadc_chip *qpnp_get_iadc(struct device *dev,
							const char *name)
{ return ERR_PTR(-ENXIO); }
static inline int32_t qpnp_iadc_vadc_sync_read(struct qpnp_iadc_chip *iadc,
	enum qpnp_iadc_channels i_channel, struct qpnp_iadc_result *i_result,
	enum qpnp_vadc_channels v_channel, struct qpnp_vadc_result *v_result)
{ return -ENXIO; }
static inline int32_t qpnp_iadc_calibrate_for_trim(struct qpnp_iadc_chip *iadc,
							bool batfet_closed)
{ return -ENXIO; }
static inline int32_t qpnp_iadc_comp_result(struct qpnp_iadc_chip *iadc,
						int64_t *result, int32_t sign)
{ return -ENXIO; }
#endif

#if defined(CONFIG_THERMAL_QPNP_ADC_TM)				\
			|| defined(CONFIG_THERMAL_QPNP_ADC_TM_MODULE)
int32_t qpnp_adc_tm_usbid_configure(struct qpnp_adc_tm_chip *chip,
					struct qpnp_adc_tm_btm_param *param);
int32_t qpnp_adc_tm_usbid_end(struct qpnp_adc_tm_chip *chip);
int32_t qpnp_adc_tm_channel_measure(struct qpnp_adc_tm_chip *chip,
					struct qpnp_adc_tm_btm_param *param);
int32_t qpnp_adc_tm_disable_chan_meas(struct qpnp_adc_tm_chip *chip,
					struct qpnp_adc_tm_btm_param *param);
struct qpnp_adc_tm_chip *qpnp_get_adc_tm(struct device *dev, const char *name);
#else
static inline int32_t qpnp_adc_tm_usbid_configure(
			struct qpnp_adc_tm_chip *chip,
			struct qpnp_adc_tm_btm_param *param)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tm_usbid_end(struct qpnp_adc_tm_chip *chip)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tm_channel_measure(
					struct qpnp_adc_tm_chip *chip,
					struct qpnp_adc_tm_btm_param *param)
{ return -ENXIO; }
static inline int32_t qpnp_adc_tm_disable_chan_meas(
					struct qpnp_adc_tm_chip *chip)
{ return -ENXIO; }
static inline struct qpnp_adc_tm_chip *qpnp_get_adc_tm(struct device *dev,
							const char *name)
{ return ERR_PTR(-ENXIO); }
#endif

#endif
