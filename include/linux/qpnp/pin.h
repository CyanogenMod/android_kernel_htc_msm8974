/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#define QPNP_PIN_MODE_DIG_IN			0
#define QPNP_PIN_MODE_DIG_OUT			1
#define QPNP_PIN_MODE_DIG_IN_OUT		2
#define QPNP_PIN_MODE_BIDIR			3
#define QPNP_PIN_MODE_AIN			4
#define QPNP_PIN_MODE_AOUT			5
#define QPNP_PIN_MODE_SINK			6

#define QPNP_PIN_INVERT_DISABLE			0
#define QPNP_PIN_INVERT_ENABLE			1

#define QPNP_PIN_OUT_BUF_CMOS			0
#define QPNP_PIN_OUT_BUF_OPEN_DRAIN_NMOS	1
#define QPNP_PIN_OUT_BUF_OPEN_DRAIN_PMOS	2

#define QPNP_PIN_VIN0				0
#define QPNP_PIN_VIN1				1
#define QPNP_PIN_VIN2				2
#define QPNP_PIN_VIN3				3
#define QPNP_PIN_VIN4				4
#define QPNP_PIN_VIN5				5
#define QPNP_PIN_VIN6				6
#define QPNP_PIN_VIN7				7

#define QPNP_PIN_GPIO_PULL_UP_30		0
#define QPNP_PIN_GPIO_PULL_UP_1P5		1
#define QPNP_PIN_GPIO_PULL_UP_31P5		2
#define QPNP_PIN_GPIO_PULL_UP_1P5_30		3
#define QPNP_PIN_GPIO_PULL_DN			4
#define QPNP_PIN_GPIO_PULL_NO			5

#define QPNP_PIN_MPP_PULL_UP_0P6KOHM		0
#define QPNP_PIN_MPP_PULL_UP_OPEN		1
#define QPNP_PIN_MPP_PULL_UP_10KOHM		2
#define QPNP_PIN_MPP_PULL_UP_30KOHM		3

#define QPNP_PIN_OUT_STRENGTH_LOW		1
#define QPNP_PIN_OUT_STRENGTH_MED		2
#define QPNP_PIN_OUT_STRENGTH_HIGH		3

#define QPNP_PIN_SEL_FUNC_CONSTANT		0
#define QPNP_PIN_SEL_FUNC_PAIRED		1
#define QPNP_PIN_SEL_FUNC_1			2
#define QPNP_PIN_SEL_FUNC_2			3
#define QPNP_PIN_SEL_DTEST1			4
#define QPNP_PIN_SEL_DTEST2			5
#define QPNP_PIN_SEL_DTEST3			6
#define QPNP_PIN_SEL_DTEST4			7

#define QPNP_PIN_MASTER_DISABLE			0
#define QPNP_PIN_MASTER_ENABLE			1

#define QPNP_PIN_AOUT_1V25			0
#define QPNP_PIN_AOUT_0V625			1
#define QPNP_PIN_AOUT_0V3125			2
#define QPNP_PIN_AOUT_MPP			3
#define QPNP_PIN_AOUT_ABUS1			4
#define QPNP_PIN_AOUT_ABUS2			5
#define QPNP_PIN_AOUT_ABUS3			6
#define QPNP_PIN_AOUT_ABUS4			7

#define QPNP_PIN_AIN_AMUX_CH5			0
#define QPNP_PIN_AIN_AMUX_CH6			1
#define QPNP_PIN_AIN_AMUX_CH7			2
#define QPNP_PIN_AIN_AMUX_CH8			3
#define QPNP_PIN_AIN_AMUX_ABUS1			4
#define QPNP_PIN_AIN_AMUX_ABUS2			5
#define QPNP_PIN_AIN_AMUX_ABUS3			6
#define QPNP_PIN_AIN_AMUX_ABUS4			7

#define QPNP_PIN_CS_OUT_5MA			0
#define QPNP_PIN_CS_OUT_10MA			1
#define QPNP_PIN_CS_OUT_15MA			2
#define QPNP_PIN_CS_OUT_20MA			3
#define QPNP_PIN_CS_OUT_25MA			4
#define QPNP_PIN_CS_OUT_30MA			5
#define QPNP_PIN_CS_OUT_35MA			6
#define QPNP_PIN_CS_OUT_40MA			7

struct qpnp_pin_cfg {
	int mode;
	int output_type;
	int invert;
	int pull;
	int vin_sel;
	int out_strength;
	int src_sel;
	int master_en;
	int aout_ref;
	int ain_route;
	int cs_out;
};

int qpnp_pin_config(int gpio, struct qpnp_pin_cfg *param);

int qpnp_pin_map(const char *name, uint32_t pmic_pin);

#ifdef CONFIG_HTC_POWER_DEBUG
int qpnp_pin_dump(struct seq_file *m, int curr_len, char *gpio_buffer);
#endif
