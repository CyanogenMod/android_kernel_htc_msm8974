/*
 * dme1737.c - Driver for the SMSC DME1737, Asus A8000, SMSC SCH311x, SCH5027,
 *             and SCH5127 Super-I/O chips integrated hardware monitoring
 *             features.
 * Copyright (c) 2007, 2008, 2009, 2010 Juerg Haefliger <juergh@gmail.com>
 *
 * This driver is an I2C/ISA hybrid, meaning that it uses the I2C bus to access
 * the chip registers if a DME1737, A8000, or SCH5027 is found and the ISA bus
 * if a SCH311x or SCH5127 chip is found. Both types of chips have very
 * similar hardware monitoring capabilities but differ in the way they can be
 * accessed.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon-vid.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/acpi.h>
#include <linux/io.h>

static struct platform_device *pdev;

static bool force_start;
module_param(force_start, bool, 0);
MODULE_PARM_DESC(force_start, "Force the chip to start monitoring inputs");

static unsigned short force_id;
module_param(force_id, ushort, 0);
MODULE_PARM_DESC(force_id, "Override the detected device ID");

static bool probe_all_addr;
module_param(probe_all_addr, bool, 0);
MODULE_PARM_DESC(probe_all_addr, "Include probing of non-standard LPC "
		 "addresses");

static const unsigned short normal_i2c[] = {0x2c, 0x2d, 0x2e, I2C_CLIENT_END};

enum chips { dme1737, sch5027, sch311x, sch5127 };


#define DME1737_REG_IN(ix)		((ix) < 5 ? 0x20 + (ix) : \
					 (ix) < 7 ? 0x94 + (ix) : \
						    0x1f)
#define DME1737_REG_IN_MIN(ix)		((ix) < 5 ? 0x44 + (ix) * 2 \
						  : 0x91 + (ix) * 2)
#define DME1737_REG_IN_MAX(ix)		((ix) < 5 ? 0x45 + (ix) * 2 \
						  : 0x92 + (ix) * 2)

#define DME1737_REG_TEMP(ix)		(0x25 + (ix))
#define DME1737_REG_TEMP_MIN(ix)	(0x4e + (ix) * 2)
#define DME1737_REG_TEMP_MAX(ix)	(0x4f + (ix) * 2)
#define DME1737_REG_TEMP_OFFSET(ix)	((ix) == 0 ? 0x1f \
						   : 0x1c + (ix))

#define DME1737_REG_IN_TEMP_LSB(ix)	(0x84 + (ix))
static const u8 DME1737_REG_IN_LSB[] = {3, 4, 4, 3, 2, 0, 0, 5};
static const u8 DME1737_REG_IN_LSB_SHL[] = {4, 4, 0, 0, 0, 0, 4, 4};
static const u8 DME1737_REG_TEMP_LSB[] = {1, 2, 1};
static const u8 DME1737_REG_TEMP_LSB_SHL[] = {4, 4, 0};

#define DME1737_REG_FAN(ix)		((ix) < 4 ? 0x28 + (ix) * 2 \
						  : 0xa1 + (ix) * 2)
#define DME1737_REG_FAN_MIN(ix)		((ix) < 4 ? 0x54 + (ix) * 2 \
						  : 0xa5 + (ix) * 2)
#define DME1737_REG_FAN_OPT(ix)		((ix) < 4 ? 0x90 + (ix) \
						  : 0xb2 + (ix))
#define DME1737_REG_FAN_MAX(ix)		(0xb4 + (ix)) 

#define DME1737_REG_PWM(ix)		((ix) < 3 ? 0x30 + (ix) \
						  : 0xa1 + (ix))
#define DME1737_REG_PWM_CONFIG(ix)	(0x5c + (ix)) 
#define DME1737_REG_PWM_MIN(ix)		(0x64 + (ix)) 
#define DME1737_REG_PWM_FREQ(ix)	((ix) < 3 ? 0x5f + (ix) \
						  : 0xa3 + (ix))
#define DME1737_REG_PWM_RR(ix)		(0x62 + (ix)) 

#define DME1737_REG_ZONE_LOW(ix)	(0x67 + (ix))
#define DME1737_REG_ZONE_ABS(ix)	(0x6a + (ix))
#define DME1737_REG_ZONE_HYST(ix)	(0x6d + (ix))

#define DME1737_REG_ALARM1		0x41
#define DME1737_REG_ALARM2		0x42
#define DME1737_REG_ALARM3		0x83
static const u8 DME1737_BIT_ALARM_IN[] = {0, 1, 2, 3, 8, 16, 17, 18};
static const u8 DME1737_BIT_ALARM_TEMP[] = {4, 5, 6};
static const u8 DME1737_BIT_ALARM_FAN[] = {10, 11, 12, 13, 22, 23};

#define DME1737_REG_DEVICE		0x3d
#define DME1737_REG_COMPANY		0x3e
#define DME1737_REG_VERSTEP		0x3f
#define DME1737_REG_CONFIG		0x40
#define DME1737_REG_CONFIG2		0x7f
#define DME1737_REG_VID			0x43
#define DME1737_REG_TACH_PWM		0x81


#define DME1737_COMPANY_SMSC	0x5c
#define DME1737_VERSTEP		0x88
#define DME1737_VERSTEP_MASK	0xf8
#define SCH311X_DEVICE		0x8c
#define SCH5027_VERSTEP		0x69
#define SCH5127_DEVICE		0x8e

#define DME1737_ID_1	0x77
#define DME1737_ID_2	0x78
#define SCH3112_ID	0x7c
#define SCH3114_ID	0x7d
#define SCH3116_ID	0x7f
#define SCH5027_ID	0x89
#define SCH5127_ID	0x86

#define DME1737_EXTENT	2

#define HAS_TEMP_OFFSET		(1 << 0)		
#define HAS_VID			(1 << 1)		
#define HAS_ZONE3		(1 << 2)		
#define HAS_ZONE_HYST		(1 << 3)		
#define HAS_PWM_MIN		(1 << 4)		
#define HAS_FAN(ix)		(1 << ((ix) + 5))	
#define HAS_PWM(ix)		(1 << ((ix) + 11))	
#define HAS_IN7			(1 << 17)		


struct dme1737_data {
	struct i2c_client *client;	
	struct device *hwmon_dev;
	const char *name;
	unsigned int addr;		

	struct mutex update_lock;
	int valid;			
	unsigned long last_update;	
	unsigned long last_vbat;	
	enum chips type;
	const int *in_nominal;		

	u8 vid;
	u8 pwm_rr_en;
	u32 has_features;

	
	u16 in[8];
	u8  in_min[8];
	u8  in_max[8];
	s16 temp[3];
	s8  temp_min[3];
	s8  temp_max[3];
	s8  temp_offset[3];
	u8  config;
	u8  config2;
	u8  vrm;
	u16 fan[6];
	u16 fan_min[6];
	u8  fan_max[2];
	u8  fan_opt[6];
	u8  pwm[6];
	u8  pwm_min[3];
	u8  pwm_config[3];
	u8  pwm_acz[3];
	u8  pwm_freq[6];
	u8  pwm_rr[2];
	u8  zone_low[3];
	u8  zone_abs[3];
	u8  zone_hyst[2];
	u32 alarms;
};

static const int IN_NOMINAL_DME1737[] = {5000, 2250, 3300, 5000, 12000, 3300,
					 3300};
static const int IN_NOMINAL_SCH311x[] = {2500, 1500, 3300, 5000, 12000, 3300,
					 3300};
static const int IN_NOMINAL_SCH5027[] = {5000, 2250, 3300, 1125, 1125, 3300,
					 3300};
static const int IN_NOMINAL_SCH5127[] = {2500, 2250, 3300, 1125, 1125, 3300,
					 3300, 1500};
#define IN_NOMINAL(type)	((type) == sch311x ? IN_NOMINAL_SCH311x : \
				 (type) == sch5027 ? IN_NOMINAL_SCH5027 : \
				 (type) == sch5127 ? IN_NOMINAL_SCH5127 : \
				 IN_NOMINAL_DME1737)

static inline int IN_FROM_REG(int reg, int nominal, int res)
{
	return (reg * nominal + (3 << (res - 3))) / (3 << (res - 2));
}

static inline int IN_TO_REG(int val, int nominal)
{
	return SENSORS_LIMIT((val * 192 + nominal / 2) / nominal, 0, 255);
}

static inline int TEMP_FROM_REG(int reg, int res)
{
	return (reg * 1000) >> (res - 8);
}

static inline int TEMP_TO_REG(int val)
{
	return SENSORS_LIMIT((val < 0 ? val - 500 : val + 500) / 1000,
			     -128, 127);
}

static const int TEMP_RANGE[] = {2000, 2500, 3333, 4000, 5000, 6666, 8000,
				 10000, 13333, 16000, 20000, 26666, 32000,
				 40000, 53333, 80000};

static inline int TEMP_RANGE_FROM_REG(int reg)
{
	return TEMP_RANGE[(reg >> 4) & 0x0f];
}

static int TEMP_RANGE_TO_REG(int val, int reg)
{
	int i;

	for (i = 15; i > 0; i--) {
		if (val > (TEMP_RANGE[i] + TEMP_RANGE[i - 1] + 1) / 2)
			break;
	}

	return (reg & 0x0f) | (i << 4);
}

static inline int TEMP_HYST_FROM_REG(int reg, int ix)
{
	return (((ix == 1) ? reg : reg >> 4) & 0x0f) * 1000;
}

static inline int TEMP_HYST_TO_REG(int val, int ix, int reg)
{
	int hyst = SENSORS_LIMIT((val + 500) / 1000, 0, 15);

	return (ix == 1) ? (reg & 0xf0) | hyst : (reg & 0x0f) | (hyst << 4);
}

static inline int FAN_FROM_REG(int reg, int tpc)
{
	if (tpc)
		return tpc * reg;
	else
		return (reg == 0 || reg == 0xffff) ? 0 : 90000 * 60 / reg;
}

static inline int FAN_TO_REG(int val, int tpc)
{
	if (tpc) {
		return SENSORS_LIMIT(val / tpc, 0, 0xffff);
	} else {
		return (val <= 0) ? 0xffff :
			SENSORS_LIMIT(90000 * 60 / val, 0, 0xfffe);
	}
}

static inline int FAN_TPC_FROM_REG(int reg)
{
	return (reg & 0x20) ? 0 : 60 >> (reg & 0x03);
}

static inline int FAN_TYPE_FROM_REG(int reg)
{
	int edge = (reg >> 1) & 0x03;

	return (edge > 0) ? 1 << (edge - 1) : 0;
}

static inline int FAN_TYPE_TO_REG(int val, int reg)
{
	int edge = (val == 4) ? 3 : val;

	return (reg & 0xf9) | (edge << 1);
}

static const int FAN_MAX[] = {0x54, 0x38, 0x2a, 0x21, 0x1c, 0x18, 0x15, 0x12,
			      0x11, 0x0f, 0x0e};

static int FAN_MAX_FROM_REG(int reg)
{
	int i;

	for (i = 10; i > 0; i--) {
		if (reg == FAN_MAX[i])
			break;
	}

	return 1000 + i * 500;
}

static int FAN_MAX_TO_REG(int val)
{
	int i;

	for (i = 10; i > 0; i--) {
		if (val > (1000 + (i - 1) * 500))
			break;
	}

	return FAN_MAX[i];
}

static inline int PWM_EN_FROM_REG(int reg)
{
	static const int en[] = {2, 2, 2, 0, -1, 2, 2, 1};

	return en[(reg >> 5) & 0x07];
}

static inline int PWM_EN_TO_REG(int val, int reg)
{
	int en = (val == 1) ? 7 : 3;

	return (reg & 0x1f) | ((en & 0x07) << 5);
}

static inline int PWM_ACZ_FROM_REG(int reg)
{
	static const int acz[] = {1, 2, 4, 0, 0, 6, 7, 0};

	return acz[(reg >> 5) & 0x07];
}

static inline int PWM_ACZ_TO_REG(int val, int reg)
{
	int acz = (val == 4) ? 2 : val - 1;

	return (reg & 0x1f) | ((acz & 0x07) << 5);
}

static const int PWM_FREQ[] = {11, 15, 22, 29, 35, 44, 59, 88,
			       15000, 20000, 30000, 25000, 0, 0, 0, 0};

static inline int PWM_FREQ_FROM_REG(int reg)
{
	return PWM_FREQ[reg & 0x0f];
}

static int PWM_FREQ_TO_REG(int val, int reg)
{
	int i;

	
	if (val > 27500) {
		i = 10;
	} else if (val > 22500) {
		i = 11;
	} else {
		for (i = 9; i > 0; i--) {
			if (val > (PWM_FREQ[i] + PWM_FREQ[i - 1] + 1) / 2)
				break;
		}
	}

	return (reg & 0xf0) | i;
}

static const u8 PWM_RR[] = {206, 104, 69, 41, 26, 18, 10, 5};

static inline int PWM_RR_FROM_REG(int reg, int ix)
{
	int rr = (ix == 1) ? reg >> 4 : reg;

	return (rr & 0x08) ? PWM_RR[rr & 0x07] : 0;
}

static int PWM_RR_TO_REG(int val, int ix, int reg)
{
	int i;

	for (i = 0; i < 7; i++) {
		if (val > (PWM_RR[i] + PWM_RR[i + 1] + 1) / 2)
			break;
	}

	return (ix == 1) ? (reg & 0x8f) | (i << 4) : (reg & 0xf8) | i;
}

static inline int PWM_RR_EN_FROM_REG(int reg, int ix)
{
	return PWM_RR_FROM_REG(reg, ix) ? 1 : 0;
}

static inline int PWM_RR_EN_TO_REG(int val, int ix, int reg)
{
	int en = (ix == 1) ? 0x80 : 0x08;

	return val ? reg | en : reg & ~en;
}

static inline int PWM_OFF_FROM_REG(int reg, int ix)
{
	return (reg >> (ix + 5)) & 0x01;
}

static inline int PWM_OFF_TO_REG(int val, int ix, int reg)
{
	return (reg & ~(1 << (ix + 5))) | ((val & 0x01) << (ix + 5));
}


static u8 dme1737_read(const struct dme1737_data *data, u8 reg)
{
	struct i2c_client *client = data->client;
	s32 val;

	if (client) { 
		val = i2c_smbus_read_byte_data(client, reg);

		if (val < 0) {
			dev_warn(&client->dev, "Read from register "
				 "0x%02x failed! Please report to the driver "
				 "maintainer.\n", reg);
		}
	} else { 
		outb(reg, data->addr);
		val = inb(data->addr + 1);
	}

	return val;
}

static s32 dme1737_write(const struct dme1737_data *data, u8 reg, u8 val)
{
	struct i2c_client *client = data->client;
	s32 res = 0;

	if (client) { 
		res = i2c_smbus_write_byte_data(client, reg, val);

		if (res < 0) {
			dev_warn(&client->dev, "Write to register "
				 "0x%02x failed! Please report to the driver "
				 "maintainer.\n", reg);
		}
	} else { 
		outb(reg, data->addr);
		outb(val, data->addr + 1);
	}

	return res;
}

static struct dme1737_data *dme1737_update_device(struct device *dev)
{
	struct dme1737_data *data = dev_get_drvdata(dev);
	int ix;
	u8 lsb[6];

	mutex_lock(&data->update_lock);

	
	if (time_after(jiffies, data->last_vbat + 600 * HZ) || !data->valid) {
		dme1737_write(data, DME1737_REG_CONFIG, dme1737_read(data,
						DME1737_REG_CONFIG) | 0x10);
		data->last_vbat = jiffies;
	}

	
	if (time_after(jiffies, data->last_update + HZ) || !data->valid) {
		if (data->has_features & HAS_VID) {
			data->vid = dme1737_read(data, DME1737_REG_VID) &
				0x3f;
		}

		
		for (ix = 0; ix < ARRAY_SIZE(data->in); ix++) {
			if (ix == 7 && !(data->has_features & HAS_IN7))
				continue;
			data->in[ix] = dme1737_read(data,
					DME1737_REG_IN(ix)) << 8;
			data->in_min[ix] = dme1737_read(data,
					DME1737_REG_IN_MIN(ix));
			data->in_max[ix] = dme1737_read(data,
					DME1737_REG_IN_MAX(ix));
		}

		
		for (ix = 0; ix < ARRAY_SIZE(data->temp); ix++) {
			data->temp[ix] = dme1737_read(data,
					DME1737_REG_TEMP(ix)) << 8;
			data->temp_min[ix] = dme1737_read(data,
					DME1737_REG_TEMP_MIN(ix));
			data->temp_max[ix] = dme1737_read(data,
					DME1737_REG_TEMP_MAX(ix));
			if (data->has_features & HAS_TEMP_OFFSET) {
				data->temp_offset[ix] = dme1737_read(data,
						DME1737_REG_TEMP_OFFSET(ix));
			}
		}

		for (ix = 0; ix < ARRAY_SIZE(lsb); ix++) {
			if (ix == 5 && !(data->has_features & HAS_IN7))
				continue;
			lsb[ix] = dme1737_read(data,
					DME1737_REG_IN_TEMP_LSB(ix));
		}
		for (ix = 0; ix < ARRAY_SIZE(data->in); ix++) {
			if (ix == 7 && !(data->has_features & HAS_IN7))
				continue;
			data->in[ix] |= (lsb[DME1737_REG_IN_LSB[ix]] <<
					DME1737_REG_IN_LSB_SHL[ix]) & 0xf0;
		}
		for (ix = 0; ix < ARRAY_SIZE(data->temp); ix++) {
			data->temp[ix] |= (lsb[DME1737_REG_TEMP_LSB[ix]] <<
					DME1737_REG_TEMP_LSB_SHL[ix]) & 0xf0;
		}

		
		for (ix = 0; ix < ARRAY_SIZE(data->fan); ix++) {
			if (!(data->has_features & HAS_FAN(ix)))
				continue;
			data->fan[ix] = dme1737_read(data,
					DME1737_REG_FAN(ix));
			data->fan[ix] |= dme1737_read(data,
					DME1737_REG_FAN(ix) + 1) << 8;
			data->fan_min[ix] = dme1737_read(data,
					DME1737_REG_FAN_MIN(ix));
			data->fan_min[ix] |= dme1737_read(data,
					DME1737_REG_FAN_MIN(ix) + 1) << 8;
			data->fan_opt[ix] = dme1737_read(data,
					DME1737_REG_FAN_OPT(ix));
			
			if (ix > 3) {
				data->fan_max[ix - 4] = dme1737_read(data,
					DME1737_REG_FAN_MAX(ix));
			}
		}

		
		for (ix = 0; ix < ARRAY_SIZE(data->pwm); ix++) {
			if (!(data->has_features & HAS_PWM(ix)))
				continue;
			data->pwm[ix] = dme1737_read(data,
					DME1737_REG_PWM(ix));
			data->pwm_freq[ix] = dme1737_read(data,
					DME1737_REG_PWM_FREQ(ix));
			
			if (ix < 3) {
				data->pwm_config[ix] = dme1737_read(data,
						DME1737_REG_PWM_CONFIG(ix));
				data->pwm_min[ix] = dme1737_read(data,
						DME1737_REG_PWM_MIN(ix));
			}
		}
		for (ix = 0; ix < ARRAY_SIZE(data->pwm_rr); ix++) {
			data->pwm_rr[ix] = dme1737_read(data,
						DME1737_REG_PWM_RR(ix));
		}

		
		for (ix = 0; ix < ARRAY_SIZE(data->zone_low); ix++) {
			
			if ((ix == 2) && !(data->has_features & HAS_ZONE3))
				continue;
			
			if ((ix == 1) && (data->type == sch5127)) {
				data->zone_low[1] = dme1737_read(data,
						DME1737_REG_ZONE_LOW(2));
				data->zone_abs[1] = dme1737_read(data,
						DME1737_REG_ZONE_ABS(2));
			} else {
				data->zone_low[ix] = dme1737_read(data,
						DME1737_REG_ZONE_LOW(ix));
				data->zone_abs[ix] = dme1737_read(data,
						DME1737_REG_ZONE_ABS(ix));
			}
		}
		if (data->has_features & HAS_ZONE_HYST) {
			for (ix = 0; ix < ARRAY_SIZE(data->zone_hyst); ix++) {
				data->zone_hyst[ix] = dme1737_read(data,
						DME1737_REG_ZONE_HYST(ix));
			}
		}

		
		data->alarms = dme1737_read(data,
						DME1737_REG_ALARM1);
		if (data->alarms & 0x80) {
			data->alarms |= dme1737_read(data,
						DME1737_REG_ALARM2) << 8;
			data->alarms |= dme1737_read(data,
						DME1737_REG_ALARM3) << 16;
		}

		if (!data->client) {
			if (data->alarms & 0xff0000)
				dme1737_write(data, DME1737_REG_ALARM3, 0xff);
			if (data->alarms & 0xff00)
				dme1737_write(data, DME1737_REG_ALARM2, 0xff);
			if (data->alarms & 0xff)
				dme1737_write(data, DME1737_REG_ALARM1, 0xff);
		}

		data->last_update = jiffies;
		data->valid = 1;
	}

	mutex_unlock(&data->update_lock);

	return data;
}


#define SYS_IN_INPUT	0
#define SYS_IN_MIN	1
#define SYS_IN_MAX	2
#define SYS_IN_ALARM	3

static ssize_t show_in(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct dme1737_data *data = dme1737_update_device(dev);
	struct sensor_device_attribute_2
		*sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int fn = sensor_attr_2->nr;
	int res;

	switch (fn) {
	case SYS_IN_INPUT:
		res = IN_FROM_REG(data->in[ix], data->in_nominal[ix], 16);
		break;
	case SYS_IN_MIN:
		res = IN_FROM_REG(data->in_min[ix], data->in_nominal[ix], 8);
		break;
	case SYS_IN_MAX:
		res = IN_FROM_REG(data->in_max[ix], data->in_nominal[ix], 8);
		break;
	case SYS_IN_ALARM:
		res = (data->alarms >> DME1737_BIT_ALARM_IN[ix]) & 0x01;
		break;
	default:
		res = 0;
		dev_dbg(dev, "Unknown function %d.\n", fn);
	}

	return sprintf(buf, "%d\n", res);
}

static ssize_t set_in(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct dme1737_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2
		*sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int fn = sensor_attr_2->nr;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	switch (fn) {
	case SYS_IN_MIN:
		data->in_min[ix] = IN_TO_REG(val, data->in_nominal[ix]);
		dme1737_write(data, DME1737_REG_IN_MIN(ix),
			      data->in_min[ix]);
		break;
	case SYS_IN_MAX:
		data->in_max[ix] = IN_TO_REG(val, data->in_nominal[ix]);
		dme1737_write(data, DME1737_REG_IN_MAX(ix),
			      data->in_max[ix]);
		break;
	default:
		dev_dbg(dev, "Unknown function %d.\n", fn);
	}
	mutex_unlock(&data->update_lock);

	return count;
}


#define SYS_TEMP_INPUT			0
#define SYS_TEMP_MIN			1
#define SYS_TEMP_MAX			2
#define SYS_TEMP_OFFSET			3
#define SYS_TEMP_ALARM			4
#define SYS_TEMP_FAULT			5

static ssize_t show_temp(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct dme1737_data *data = dme1737_update_device(dev);
	struct sensor_device_attribute_2
		*sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int fn = sensor_attr_2->nr;
	int res;

	switch (fn) {
	case SYS_TEMP_INPUT:
		res = TEMP_FROM_REG(data->temp[ix], 16);
		break;
	case SYS_TEMP_MIN:
		res = TEMP_FROM_REG(data->temp_min[ix], 8);
		break;
	case SYS_TEMP_MAX:
		res = TEMP_FROM_REG(data->temp_max[ix], 8);
		break;
	case SYS_TEMP_OFFSET:
		res = TEMP_FROM_REG(data->temp_offset[ix], 8);
		break;
	case SYS_TEMP_ALARM:
		res = (data->alarms >> DME1737_BIT_ALARM_TEMP[ix]) & 0x01;
		break;
	case SYS_TEMP_FAULT:
		res = (((u16)data->temp[ix] & 0xff00) == 0x8000);
		break;
	default:
		res = 0;
		dev_dbg(dev, "Unknown function %d.\n", fn);
	}

	return sprintf(buf, "%d\n", res);
}

static ssize_t set_temp(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct dme1737_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2
		*sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int fn = sensor_attr_2->nr;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	switch (fn) {
	case SYS_TEMP_MIN:
		data->temp_min[ix] = TEMP_TO_REG(val);
		dme1737_write(data, DME1737_REG_TEMP_MIN(ix),
			      data->temp_min[ix]);
		break;
	case SYS_TEMP_MAX:
		data->temp_max[ix] = TEMP_TO_REG(val);
		dme1737_write(data, DME1737_REG_TEMP_MAX(ix),
			      data->temp_max[ix]);
		break;
	case SYS_TEMP_OFFSET:
		data->temp_offset[ix] = TEMP_TO_REG(val);
		dme1737_write(data, DME1737_REG_TEMP_OFFSET(ix),
			      data->temp_offset[ix]);
		break;
	default:
		dev_dbg(dev, "Unknown function %d.\n", fn);
	}
	mutex_unlock(&data->update_lock);

	return count;
}


#define SYS_ZONE_AUTO_CHANNELS_TEMP	0
#define SYS_ZONE_AUTO_POINT1_TEMP_HYST	1
#define SYS_ZONE_AUTO_POINT1_TEMP	2
#define SYS_ZONE_AUTO_POINT2_TEMP	3
#define SYS_ZONE_AUTO_POINT3_TEMP	4

static ssize_t show_zone(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct dme1737_data *data = dme1737_update_device(dev);
	struct sensor_device_attribute_2
		*sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int fn = sensor_attr_2->nr;
	int res;

	switch (fn) {
	case SYS_ZONE_AUTO_CHANNELS_TEMP:
		
		if ((ix == 1) && (data->config2 & 0x02))
			res = 4;
		else
			res = 1 << ix;
		break;
	case SYS_ZONE_AUTO_POINT1_TEMP_HYST:
		res = TEMP_FROM_REG(data->zone_low[ix], 8) -
		      TEMP_HYST_FROM_REG(data->zone_hyst[ix == 2], ix);
		break;
	case SYS_ZONE_AUTO_POINT1_TEMP:
		res = TEMP_FROM_REG(data->zone_low[ix], 8);
		break;
	case SYS_ZONE_AUTO_POINT2_TEMP:
		
		res = TEMP_FROM_REG(data->zone_low[ix], 8) +
		      TEMP_RANGE_FROM_REG(data->pwm_freq[ix]);
		break;
	case SYS_ZONE_AUTO_POINT3_TEMP:
		res = TEMP_FROM_REG(data->zone_abs[ix], 8);
		break;
	default:
		res = 0;
		dev_dbg(dev, "Unknown function %d.\n", fn);
	}

	return sprintf(buf, "%d\n", res);
}

static ssize_t set_zone(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct dme1737_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2
		*sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int fn = sensor_attr_2->nr;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	switch (fn) {
	case SYS_ZONE_AUTO_POINT1_TEMP_HYST:
		
		data->zone_low[ix] = dme1737_read(data,
						  DME1737_REG_ZONE_LOW(ix));
		
		data->zone_hyst[ix == 2] = TEMP_HYST_TO_REG(
					TEMP_FROM_REG(data->zone_low[ix], 8) -
					val, ix, dme1737_read(data,
					DME1737_REG_ZONE_HYST(ix == 2)));
		dme1737_write(data, DME1737_REG_ZONE_HYST(ix == 2),
			      data->zone_hyst[ix == 2]);
		break;
	case SYS_ZONE_AUTO_POINT1_TEMP:
		data->zone_low[ix] = TEMP_TO_REG(val);
		dme1737_write(data, DME1737_REG_ZONE_LOW(ix),
			      data->zone_low[ix]);
		break;
	case SYS_ZONE_AUTO_POINT2_TEMP:
		
		data->zone_low[ix] = dme1737_read(data,
						  DME1737_REG_ZONE_LOW(ix));
		data->pwm_freq[ix] = TEMP_RANGE_TO_REG(val -
					TEMP_FROM_REG(data->zone_low[ix], 8),
					dme1737_read(data,
					DME1737_REG_PWM_FREQ(ix)));
		dme1737_write(data, DME1737_REG_PWM_FREQ(ix),
			      data->pwm_freq[ix]);
		break;
	case SYS_ZONE_AUTO_POINT3_TEMP:
		data->zone_abs[ix] = TEMP_TO_REG(val);
		dme1737_write(data, DME1737_REG_ZONE_ABS(ix),
			      data->zone_abs[ix]);
		break;
	default:
		dev_dbg(dev, "Unknown function %d.\n", fn);
	}
	mutex_unlock(&data->update_lock);

	return count;
}


#define SYS_FAN_INPUT	0
#define SYS_FAN_MIN	1
#define SYS_FAN_MAX	2
#define SYS_FAN_ALARM	3
#define SYS_FAN_TYPE	4

static ssize_t show_fan(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct dme1737_data *data = dme1737_update_device(dev);
	struct sensor_device_attribute_2
		*sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int fn = sensor_attr_2->nr;
	int res;

	switch (fn) {
	case SYS_FAN_INPUT:
		res = FAN_FROM_REG(data->fan[ix],
				   ix < 4 ? 0 :
				   FAN_TPC_FROM_REG(data->fan_opt[ix]));
		break;
	case SYS_FAN_MIN:
		res = FAN_FROM_REG(data->fan_min[ix],
				   ix < 4 ? 0 :
				   FAN_TPC_FROM_REG(data->fan_opt[ix]));
		break;
	case SYS_FAN_MAX:
		
		res = FAN_MAX_FROM_REG(data->fan_max[ix - 4]);
		break;
	case SYS_FAN_ALARM:
		res = (data->alarms >> DME1737_BIT_ALARM_FAN[ix]) & 0x01;
		break;
	case SYS_FAN_TYPE:
		
		res = FAN_TYPE_FROM_REG(data->fan_opt[ix]);
		break;
	default:
		res = 0;
		dev_dbg(dev, "Unknown function %d.\n", fn);
	}

	return sprintf(buf, "%d\n", res);
}

static ssize_t set_fan(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct dme1737_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2
		*sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int fn = sensor_attr_2->nr;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	switch (fn) {
	case SYS_FAN_MIN:
		if (ix < 4) {
			data->fan_min[ix] = FAN_TO_REG(val, 0);
		} else {
			
			data->fan_opt[ix] = dme1737_read(data,
						DME1737_REG_FAN_OPT(ix));
			
			data->fan_min[ix] = FAN_TO_REG(val,
					FAN_TPC_FROM_REG(data->fan_opt[ix]));
		}
		dme1737_write(data, DME1737_REG_FAN_MIN(ix),
			      data->fan_min[ix] & 0xff);
		dme1737_write(data, DME1737_REG_FAN_MIN(ix) + 1,
			      data->fan_min[ix] >> 8);
		break;
	case SYS_FAN_MAX:
		
		data->fan_max[ix - 4] = FAN_MAX_TO_REG(val);
		dme1737_write(data, DME1737_REG_FAN_MAX(ix),
			      data->fan_max[ix - 4]);
		break;
	case SYS_FAN_TYPE:
		
		if (!(val == 1 || val == 2 || val == 4)) {
			count = -EINVAL;
			dev_warn(dev, "Fan type value %ld not "
				 "supported. Choose one of 1, 2, or 4.\n",
				 val);
			goto exit;
		}
		data->fan_opt[ix] = FAN_TYPE_TO_REG(val, dme1737_read(data,
					DME1737_REG_FAN_OPT(ix)));
		dme1737_write(data, DME1737_REG_FAN_OPT(ix),
			      data->fan_opt[ix]);
		break;
	default:
		dev_dbg(dev, "Unknown function %d.\n", fn);
	}
exit:
	mutex_unlock(&data->update_lock);

	return count;
}


#define SYS_PWM				0
#define SYS_PWM_FREQ			1
#define SYS_PWM_ENABLE			2
#define SYS_PWM_RAMP_RATE		3
#define SYS_PWM_AUTO_CHANNELS_ZONE	4
#define SYS_PWM_AUTO_PWM_MIN		5
#define SYS_PWM_AUTO_POINT1_PWM		6
#define SYS_PWM_AUTO_POINT2_PWM		7

static ssize_t show_pwm(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct dme1737_data *data = dme1737_update_device(dev);
	struct sensor_device_attribute_2
		*sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int fn = sensor_attr_2->nr;
	int res;

	switch (fn) {
	case SYS_PWM:
		if (PWM_EN_FROM_REG(data->pwm_config[ix]) == 0)
			res = 255;
		else
			res = data->pwm[ix];
		break;
	case SYS_PWM_FREQ:
		res = PWM_FREQ_FROM_REG(data->pwm_freq[ix]);
		break;
	case SYS_PWM_ENABLE:
		if (ix >= 3)
			res = 1; 
		else
			res = PWM_EN_FROM_REG(data->pwm_config[ix]);
		break;
	case SYS_PWM_RAMP_RATE:
		
		res = PWM_RR_FROM_REG(data->pwm_rr[ix > 0], ix);
		break;
	case SYS_PWM_AUTO_CHANNELS_ZONE:
		
		if (PWM_EN_FROM_REG(data->pwm_config[ix]) == 2)
			res = PWM_ACZ_FROM_REG(data->pwm_config[ix]);
		else
			res = data->pwm_acz[ix];
		break;
	case SYS_PWM_AUTO_PWM_MIN:
		
		if (PWM_OFF_FROM_REG(data->pwm_rr[0], ix))
			res = data->pwm_min[ix];
		else
			res = 0;
		break;
	case SYS_PWM_AUTO_POINT1_PWM:
		
		res = data->pwm_min[ix];
		break;
	case SYS_PWM_AUTO_POINT2_PWM:
		
		res = 255; 
		break;
	default:
		res = 0;
		dev_dbg(dev, "Unknown function %d.\n", fn);
	}

	return sprintf(buf, "%d\n", res);
}

static struct attribute *dme1737_pwm_chmod_attr[];
static void dme1737_chmod_file(struct device*, struct attribute*, umode_t);

static ssize_t set_pwm(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct dme1737_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute_2
		*sensor_attr_2 = to_sensor_dev_attr_2(attr);
	int ix = sensor_attr_2->index;
	int fn = sensor_attr_2->nr;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	switch (fn) {
	case SYS_PWM:
		data->pwm[ix] = SENSORS_LIMIT(val, 0, 255);
		dme1737_write(data, DME1737_REG_PWM(ix), data->pwm[ix]);
		break;
	case SYS_PWM_FREQ:
		data->pwm_freq[ix] = PWM_FREQ_TO_REG(val, dme1737_read(data,
						DME1737_REG_PWM_FREQ(ix)));
		dme1737_write(data, DME1737_REG_PWM_FREQ(ix),
			      data->pwm_freq[ix]);
		break;
	case SYS_PWM_ENABLE:
		
		if (val < 0 || val > 2) {
			count = -EINVAL;
			dev_warn(dev, "PWM enable %ld not "
				 "supported. Choose one of 0, 1, or 2.\n",
				 val);
			goto exit;
		}
		
		data->pwm_config[ix] = dme1737_read(data,
						DME1737_REG_PWM_CONFIG(ix));
		if (val == PWM_EN_FROM_REG(data->pwm_config[ix])) {
			
			goto exit;
		}
		
		if (PWM_EN_FROM_REG(data->pwm_config[ix]) == 2) {
			
			data->pwm_acz[ix] = PWM_ACZ_FROM_REG(
							data->pwm_config[ix]);
			
			data->pwm_rr[ix > 0] = dme1737_read(data,
						DME1737_REG_PWM_RR(ix > 0));
			data->pwm_rr_en &= ~(1 << ix);
			if (PWM_RR_EN_FROM_REG(data->pwm_rr[ix > 0], ix)) {
				data->pwm_rr_en |= (1 << ix);
				data->pwm_rr[ix > 0] = PWM_RR_EN_TO_REG(0, ix,
							data->pwm_rr[ix > 0]);
				dme1737_write(data,
					      DME1737_REG_PWM_RR(ix > 0),
					      data->pwm_rr[ix > 0]);
			}
		}
		
		switch (val) {
		case 0:
			
			dme1737_chmod_file(dev, dme1737_pwm_chmod_attr[ix],
					   S_IRUGO);
			
			data->pwm_config[ix] = PWM_EN_TO_REG(0,
							data->pwm_config[ix]);
			dme1737_write(data, DME1737_REG_PWM_CONFIG(ix),
				      data->pwm_config[ix]);
			break;
		case 1:
			
			data->pwm_config[ix] = PWM_EN_TO_REG(1,
							data->pwm_config[ix]);
			dme1737_write(data, DME1737_REG_PWM_CONFIG(ix),
				      data->pwm_config[ix]);
			
			dme1737_chmod_file(dev, dme1737_pwm_chmod_attr[ix],
					   S_IRUGO | S_IWUSR);
			break;
		case 2:
			
			dme1737_chmod_file(dev, dme1737_pwm_chmod_attr[ix],
					   S_IRUGO);
			data->pwm_config[ix] = PWM_ACZ_TO_REG(
							data->pwm_acz[ix],
							data->pwm_config[ix]);
			dme1737_write(data, DME1737_REG_PWM_CONFIG(ix),
				      data->pwm_config[ix]);
			
			if (data->pwm_rr_en & (1 << ix)) {
				data->pwm_rr[ix > 0] = PWM_RR_EN_TO_REG(1, ix,
						dme1737_read(data,
						DME1737_REG_PWM_RR(ix > 0)));
				dme1737_write(data,
					      DME1737_REG_PWM_RR(ix > 0),
					      data->pwm_rr[ix > 0]);
			}
			break;
		}
		break;
	case SYS_PWM_RAMP_RATE:
		
		
		data->pwm_config[ix] = dme1737_read(data,
						DME1737_REG_PWM_CONFIG(ix));
		data->pwm_rr[ix > 0] = dme1737_read(data,
						DME1737_REG_PWM_RR(ix > 0));
		
		if (val > 0) {
			data->pwm_rr[ix > 0] = PWM_RR_TO_REG(val, ix,
							data->pwm_rr[ix > 0]);
		}
		if (PWM_EN_FROM_REG(data->pwm_config[ix]) == 2) {
			data->pwm_rr[ix > 0] = PWM_RR_EN_TO_REG(val > 0, ix,
							data->pwm_rr[ix > 0]);
		}
		dme1737_write(data, DME1737_REG_PWM_RR(ix > 0),
			      data->pwm_rr[ix > 0]);
		break;
	case SYS_PWM_AUTO_CHANNELS_ZONE:
		
		if (!(val == 1 || val == 2 || val == 4 ||
		      val == 6 || val == 7)) {
			count = -EINVAL;
			dev_warn(dev, "PWM auto channels zone %ld "
				 "not supported. Choose one of 1, 2, 4, 6, "
				 "or 7.\n", val);
			goto exit;
		}
		
		data->pwm_config[ix] = dme1737_read(data,
						DME1737_REG_PWM_CONFIG(ix));
		if (PWM_EN_FROM_REG(data->pwm_config[ix]) == 2) {
			data->pwm_config[ix] = PWM_ACZ_TO_REG(val,
						data->pwm_config[ix]);
			dme1737_write(data, DME1737_REG_PWM_CONFIG(ix),
				      data->pwm_config[ix]);
		} else {
			data->pwm_acz[ix] = val;
		}
		break;
	case SYS_PWM_AUTO_PWM_MIN:
		
		
		data->pwm_min[ix] = dme1737_read(data,
						DME1737_REG_PWM_MIN(ix));
		if (val > ((data->pwm_min[ix] + 1) / 2)) {
			data->pwm_rr[0] = PWM_OFF_TO_REG(1, ix,
						dme1737_read(data,
						DME1737_REG_PWM_RR(0)));
		} else {
			data->pwm_rr[0] = PWM_OFF_TO_REG(0, ix,
						dme1737_read(data,
						DME1737_REG_PWM_RR(0)));
		}
		dme1737_write(data, DME1737_REG_PWM_RR(0),
			      data->pwm_rr[0]);
		break;
	case SYS_PWM_AUTO_POINT1_PWM:
		
		data->pwm_min[ix] = SENSORS_LIMIT(val, 0, 255);
		dme1737_write(data, DME1737_REG_PWM_MIN(ix),
			      data->pwm_min[ix]);
		break;
	default:
		dev_dbg(dev, "Unknown function %d.\n", fn);
	}
exit:
	mutex_unlock(&data->update_lock);

	return count;
}


static ssize_t show_vrm(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct dme1737_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", data->vrm);
}

static ssize_t set_vrm(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct dme1737_data *data = dev_get_drvdata(dev);
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	data->vrm = val;
	return count;
}

static ssize_t show_vid(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct dme1737_data *data = dme1737_update_device(dev);

	return sprintf(buf, "%d\n", vid_from_reg(data->vid, data->vrm));
}

static ssize_t show_name(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct dme1737_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", data->name);
}



#define SENSOR_DEVICE_ATTR_IN(ix) \
static SENSOR_DEVICE_ATTR_2(in##ix##_input, S_IRUGO, \
	show_in, NULL, SYS_IN_INPUT, ix); \
static SENSOR_DEVICE_ATTR_2(in##ix##_min, S_IRUGO | S_IWUSR, \
	show_in, set_in, SYS_IN_MIN, ix); \
static SENSOR_DEVICE_ATTR_2(in##ix##_max, S_IRUGO | S_IWUSR, \
	show_in, set_in, SYS_IN_MAX, ix); \
static SENSOR_DEVICE_ATTR_2(in##ix##_alarm, S_IRUGO, \
	show_in, NULL, SYS_IN_ALARM, ix)

SENSOR_DEVICE_ATTR_IN(0);
SENSOR_DEVICE_ATTR_IN(1);
SENSOR_DEVICE_ATTR_IN(2);
SENSOR_DEVICE_ATTR_IN(3);
SENSOR_DEVICE_ATTR_IN(4);
SENSOR_DEVICE_ATTR_IN(5);
SENSOR_DEVICE_ATTR_IN(6);
SENSOR_DEVICE_ATTR_IN(7);


#define SENSOR_DEVICE_ATTR_TEMP(ix) \
static SENSOR_DEVICE_ATTR_2(temp##ix##_input, S_IRUGO, \
	show_temp, NULL, SYS_TEMP_INPUT, ix-1); \
static SENSOR_DEVICE_ATTR_2(temp##ix##_min, S_IRUGO | S_IWUSR, \
	show_temp, set_temp, SYS_TEMP_MIN, ix-1); \
static SENSOR_DEVICE_ATTR_2(temp##ix##_max, S_IRUGO | S_IWUSR, \
	show_temp, set_temp, SYS_TEMP_MAX, ix-1); \
static SENSOR_DEVICE_ATTR_2(temp##ix##_offset, S_IRUGO, \
	show_temp, set_temp, SYS_TEMP_OFFSET, ix-1); \
static SENSOR_DEVICE_ATTR_2(temp##ix##_alarm, S_IRUGO, \
	show_temp, NULL, SYS_TEMP_ALARM, ix-1); \
static SENSOR_DEVICE_ATTR_2(temp##ix##_fault, S_IRUGO, \
	show_temp, NULL, SYS_TEMP_FAULT, ix-1)

SENSOR_DEVICE_ATTR_TEMP(1);
SENSOR_DEVICE_ATTR_TEMP(2);
SENSOR_DEVICE_ATTR_TEMP(3);


#define SENSOR_DEVICE_ATTR_ZONE(ix) \
static SENSOR_DEVICE_ATTR_2(zone##ix##_auto_channels_temp, S_IRUGO, \
	show_zone, NULL, SYS_ZONE_AUTO_CHANNELS_TEMP, ix-1); \
static SENSOR_DEVICE_ATTR_2(zone##ix##_auto_point1_temp_hyst, S_IRUGO, \
	show_zone, set_zone, SYS_ZONE_AUTO_POINT1_TEMP_HYST, ix-1); \
static SENSOR_DEVICE_ATTR_2(zone##ix##_auto_point1_temp, S_IRUGO, \
	show_zone, set_zone, SYS_ZONE_AUTO_POINT1_TEMP, ix-1); \
static SENSOR_DEVICE_ATTR_2(zone##ix##_auto_point2_temp, S_IRUGO, \
	show_zone, set_zone, SYS_ZONE_AUTO_POINT2_TEMP, ix-1); \
static SENSOR_DEVICE_ATTR_2(zone##ix##_auto_point3_temp, S_IRUGO, \
	show_zone, set_zone, SYS_ZONE_AUTO_POINT3_TEMP, ix-1)

SENSOR_DEVICE_ATTR_ZONE(1);
SENSOR_DEVICE_ATTR_ZONE(2);
SENSOR_DEVICE_ATTR_ZONE(3);


#define SENSOR_DEVICE_ATTR_FAN_1TO4(ix) \
static SENSOR_DEVICE_ATTR_2(fan##ix##_input, S_IRUGO, \
	show_fan, NULL, SYS_FAN_INPUT, ix-1); \
static SENSOR_DEVICE_ATTR_2(fan##ix##_min, S_IRUGO | S_IWUSR, \
	show_fan, set_fan, SYS_FAN_MIN, ix-1); \
static SENSOR_DEVICE_ATTR_2(fan##ix##_alarm, S_IRUGO, \
	show_fan, NULL, SYS_FAN_ALARM, ix-1); \
static SENSOR_DEVICE_ATTR_2(fan##ix##_type, S_IRUGO | S_IWUSR, \
	show_fan, set_fan, SYS_FAN_TYPE, ix-1)

SENSOR_DEVICE_ATTR_FAN_1TO4(1);
SENSOR_DEVICE_ATTR_FAN_1TO4(2);
SENSOR_DEVICE_ATTR_FAN_1TO4(3);
SENSOR_DEVICE_ATTR_FAN_1TO4(4);


#define SENSOR_DEVICE_ATTR_FAN_5TO6(ix) \
static SENSOR_DEVICE_ATTR_2(fan##ix##_input, S_IRUGO, \
	show_fan, NULL, SYS_FAN_INPUT, ix-1); \
static SENSOR_DEVICE_ATTR_2(fan##ix##_min, S_IRUGO | S_IWUSR, \
	show_fan, set_fan, SYS_FAN_MIN, ix-1); \
static SENSOR_DEVICE_ATTR_2(fan##ix##_alarm, S_IRUGO, \
	show_fan, NULL, SYS_FAN_ALARM, ix-1); \
static SENSOR_DEVICE_ATTR_2(fan##ix##_max, S_IRUGO | S_IWUSR, \
	show_fan, set_fan, SYS_FAN_MAX, ix-1)

SENSOR_DEVICE_ATTR_FAN_5TO6(5);
SENSOR_DEVICE_ATTR_FAN_5TO6(6);


#define SENSOR_DEVICE_ATTR_PWM_1TO3(ix) \
static SENSOR_DEVICE_ATTR_2(pwm##ix, S_IRUGO, \
	show_pwm, set_pwm, SYS_PWM, ix-1); \
static SENSOR_DEVICE_ATTR_2(pwm##ix##_freq, S_IRUGO, \
	show_pwm, set_pwm, SYS_PWM_FREQ, ix-1); \
static SENSOR_DEVICE_ATTR_2(pwm##ix##_enable, S_IRUGO, \
	show_pwm, set_pwm, SYS_PWM_ENABLE, ix-1); \
static SENSOR_DEVICE_ATTR_2(pwm##ix##_ramp_rate, S_IRUGO, \
	show_pwm, set_pwm, SYS_PWM_RAMP_RATE, ix-1); \
static SENSOR_DEVICE_ATTR_2(pwm##ix##_auto_channels_zone, S_IRUGO, \
	show_pwm, set_pwm, SYS_PWM_AUTO_CHANNELS_ZONE, ix-1); \
static SENSOR_DEVICE_ATTR_2(pwm##ix##_auto_pwm_min, S_IRUGO, \
	show_pwm, set_pwm, SYS_PWM_AUTO_PWM_MIN, ix-1); \
static SENSOR_DEVICE_ATTR_2(pwm##ix##_auto_point1_pwm, S_IRUGO, \
	show_pwm, set_pwm, SYS_PWM_AUTO_POINT1_PWM, ix-1); \
static SENSOR_DEVICE_ATTR_2(pwm##ix##_auto_point2_pwm, S_IRUGO, \
	show_pwm, NULL, SYS_PWM_AUTO_POINT2_PWM, ix-1)

SENSOR_DEVICE_ATTR_PWM_1TO3(1);
SENSOR_DEVICE_ATTR_PWM_1TO3(2);
SENSOR_DEVICE_ATTR_PWM_1TO3(3);


#define SENSOR_DEVICE_ATTR_PWM_5TO6(ix) \
static SENSOR_DEVICE_ATTR_2(pwm##ix, S_IRUGO, \
	show_pwm, set_pwm, SYS_PWM, ix-1); \
static SENSOR_DEVICE_ATTR_2(pwm##ix##_freq, S_IRUGO, \
	show_pwm, set_pwm, SYS_PWM_FREQ, ix-1); \
static SENSOR_DEVICE_ATTR_2(pwm##ix##_enable, S_IRUGO, \
	show_pwm, NULL, SYS_PWM_ENABLE, ix-1)

SENSOR_DEVICE_ATTR_PWM_5TO6(5);
SENSOR_DEVICE_ATTR_PWM_5TO6(6);


static DEVICE_ATTR(vrm, S_IRUGO | S_IWUSR, show_vrm, set_vrm);
static DEVICE_ATTR(cpu0_vid, S_IRUGO, show_vid, NULL);
static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);   

static struct attribute *dme1737_attr[] = {
	
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_min.dev_attr.attr,
	&sensor_dev_attr_in0_max.dev_attr.attr,
	&sensor_dev_attr_in0_alarm.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_min.dev_attr.attr,
	&sensor_dev_attr_in1_max.dev_attr.attr,
	&sensor_dev_attr_in1_alarm.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in2_min.dev_attr.attr,
	&sensor_dev_attr_in2_max.dev_attr.attr,
	&sensor_dev_attr_in2_alarm.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in3_min.dev_attr.attr,
	&sensor_dev_attr_in3_max.dev_attr.attr,
	&sensor_dev_attr_in3_alarm.dev_attr.attr,
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in4_min.dev_attr.attr,
	&sensor_dev_attr_in4_max.dev_attr.attr,
	&sensor_dev_attr_in4_alarm.dev_attr.attr,
	&sensor_dev_attr_in5_input.dev_attr.attr,
	&sensor_dev_attr_in5_min.dev_attr.attr,
	&sensor_dev_attr_in5_max.dev_attr.attr,
	&sensor_dev_attr_in5_alarm.dev_attr.attr,
	&sensor_dev_attr_in6_input.dev_attr.attr,
	&sensor_dev_attr_in6_min.dev_attr.attr,
	&sensor_dev_attr_in6_max.dev_attr.attr,
	&sensor_dev_attr_in6_alarm.dev_attr.attr,
	
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_min.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp1_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_fault.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp2_min.dev_attr.attr,
	&sensor_dev_attr_temp2_max.dev_attr.attr,
	&sensor_dev_attr_temp2_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_fault.dev_attr.attr,
	&sensor_dev_attr_temp3_input.dev_attr.attr,
	&sensor_dev_attr_temp3_min.dev_attr.attr,
	&sensor_dev_attr_temp3_max.dev_attr.attr,
	&sensor_dev_attr_temp3_alarm.dev_attr.attr,
	&sensor_dev_attr_temp3_fault.dev_attr.attr,
	
	&sensor_dev_attr_zone1_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_zone1_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_zone1_auto_point3_temp.dev_attr.attr,
	&sensor_dev_attr_zone1_auto_channels_temp.dev_attr.attr,
	&sensor_dev_attr_zone2_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_zone2_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_zone2_auto_point3_temp.dev_attr.attr,
	&sensor_dev_attr_zone2_auto_channels_temp.dev_attr.attr,
	NULL
};

static const struct attribute_group dme1737_group = {
	.attrs = dme1737_attr,
};

static struct attribute *dme1737_temp_offset_attr[] = {
	&sensor_dev_attr_temp1_offset.dev_attr.attr,
	&sensor_dev_attr_temp2_offset.dev_attr.attr,
	&sensor_dev_attr_temp3_offset.dev_attr.attr,
	NULL
};

static const struct attribute_group dme1737_temp_offset_group = {
	.attrs = dme1737_temp_offset_attr,
};

static struct attribute *dme1737_vid_attr[] = {
	&dev_attr_vrm.attr,
	&dev_attr_cpu0_vid.attr,
	NULL
};

static const struct attribute_group dme1737_vid_group = {
	.attrs = dme1737_vid_attr,
};

static struct attribute *dme1737_zone3_attr[] = {
	&sensor_dev_attr_zone3_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_zone3_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_zone3_auto_point3_temp.dev_attr.attr,
	&sensor_dev_attr_zone3_auto_channels_temp.dev_attr.attr,
	NULL
};

static const struct attribute_group dme1737_zone3_group = {
	.attrs = dme1737_zone3_attr,
};


static struct attribute *dme1737_zone_hyst_attr[] = {
	&sensor_dev_attr_zone1_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_zone2_auto_point1_temp_hyst.dev_attr.attr,
	&sensor_dev_attr_zone3_auto_point1_temp_hyst.dev_attr.attr,
	NULL
};

static const struct attribute_group dme1737_zone_hyst_group = {
	.attrs = dme1737_zone_hyst_attr,
};

static struct attribute *dme1737_in7_attr[] = {
	&sensor_dev_attr_in7_input.dev_attr.attr,
	&sensor_dev_attr_in7_min.dev_attr.attr,
	&sensor_dev_attr_in7_max.dev_attr.attr,
	&sensor_dev_attr_in7_alarm.dev_attr.attr,
	NULL
};

static const struct attribute_group dme1737_in7_group = {
	.attrs = dme1737_in7_attr,
};

static struct attribute *dme1737_pwm1_attr[] = {
	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm1_freq.dev_attr.attr,
	&sensor_dev_attr_pwm1_enable.dev_attr.attr,
	&sensor_dev_attr_pwm1_ramp_rate.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_channels_zone.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_point1_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_point2_pwm.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_pwm2_attr[] = {
	&sensor_dev_attr_pwm2.dev_attr.attr,
	&sensor_dev_attr_pwm2_freq.dev_attr.attr,
	&sensor_dev_attr_pwm2_enable.dev_attr.attr,
	&sensor_dev_attr_pwm2_ramp_rate.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_channels_zone.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_point1_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_point2_pwm.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_pwm3_attr[] = {
	&sensor_dev_attr_pwm3.dev_attr.attr,
	&sensor_dev_attr_pwm3_freq.dev_attr.attr,
	&sensor_dev_attr_pwm3_enable.dev_attr.attr,
	&sensor_dev_attr_pwm3_ramp_rate.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_channels_zone.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_point1_pwm.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_point2_pwm.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_pwm5_attr[] = {
	&sensor_dev_attr_pwm5.dev_attr.attr,
	&sensor_dev_attr_pwm5_freq.dev_attr.attr,
	&sensor_dev_attr_pwm5_enable.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_pwm6_attr[] = {
	&sensor_dev_attr_pwm6.dev_attr.attr,
	&sensor_dev_attr_pwm6_freq.dev_attr.attr,
	&sensor_dev_attr_pwm6_enable.dev_attr.attr,
	NULL
};

static const struct attribute_group dme1737_pwm_group[] = {
	{ .attrs = dme1737_pwm1_attr },
	{ .attrs = dme1737_pwm2_attr },
	{ .attrs = dme1737_pwm3_attr },
	{ .attrs = NULL },
	{ .attrs = dme1737_pwm5_attr },
	{ .attrs = dme1737_pwm6_attr },
};

static struct attribute *dme1737_auto_pwm_min_attr[] = {
	&sensor_dev_attr_pwm1_auto_pwm_min.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_pwm_min.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_pwm_min.dev_attr.attr,
};

static struct attribute *dme1737_fan1_attr[] = {
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan1_min.dev_attr.attr,
	&sensor_dev_attr_fan1_alarm.dev_attr.attr,
	&sensor_dev_attr_fan1_type.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_fan2_attr[] = {
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan2_min.dev_attr.attr,
	&sensor_dev_attr_fan2_alarm.dev_attr.attr,
	&sensor_dev_attr_fan2_type.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_fan3_attr[] = {
	&sensor_dev_attr_fan3_input.dev_attr.attr,
	&sensor_dev_attr_fan3_min.dev_attr.attr,
	&sensor_dev_attr_fan3_alarm.dev_attr.attr,
	&sensor_dev_attr_fan3_type.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_fan4_attr[] = {
	&sensor_dev_attr_fan4_input.dev_attr.attr,
	&sensor_dev_attr_fan4_min.dev_attr.attr,
	&sensor_dev_attr_fan4_alarm.dev_attr.attr,
	&sensor_dev_attr_fan4_type.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_fan5_attr[] = {
	&sensor_dev_attr_fan5_input.dev_attr.attr,
	&sensor_dev_attr_fan5_min.dev_attr.attr,
	&sensor_dev_attr_fan5_alarm.dev_attr.attr,
	&sensor_dev_attr_fan5_max.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_fan6_attr[] = {
	&sensor_dev_attr_fan6_input.dev_attr.attr,
	&sensor_dev_attr_fan6_min.dev_attr.attr,
	&sensor_dev_attr_fan6_alarm.dev_attr.attr,
	&sensor_dev_attr_fan6_max.dev_attr.attr,
	NULL
};

static const struct attribute_group dme1737_fan_group[] = {
	{ .attrs = dme1737_fan1_attr },
	{ .attrs = dme1737_fan2_attr },
	{ .attrs = dme1737_fan3_attr },
	{ .attrs = dme1737_fan4_attr },
	{ .attrs = dme1737_fan5_attr },
	{ .attrs = dme1737_fan6_attr },
};

static struct attribute *dme1737_zone_chmod_attr[] = {
	&sensor_dev_attr_zone1_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_zone1_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_zone1_auto_point3_temp.dev_attr.attr,
	&sensor_dev_attr_zone2_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_zone2_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_zone2_auto_point3_temp.dev_attr.attr,
	NULL
};

static const struct attribute_group dme1737_zone_chmod_group = {
	.attrs = dme1737_zone_chmod_attr,
};


static struct attribute *dme1737_zone3_chmod_attr[] = {
	&sensor_dev_attr_zone3_auto_point1_temp.dev_attr.attr,
	&sensor_dev_attr_zone3_auto_point2_temp.dev_attr.attr,
	&sensor_dev_attr_zone3_auto_point3_temp.dev_attr.attr,
	NULL
};

static const struct attribute_group dme1737_zone3_chmod_group = {
	.attrs = dme1737_zone3_chmod_attr,
};

static struct attribute *dme1737_pwm1_chmod_attr[] = {
	&sensor_dev_attr_pwm1_freq.dev_attr.attr,
	&sensor_dev_attr_pwm1_enable.dev_attr.attr,
	&sensor_dev_attr_pwm1_ramp_rate.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_channels_zone.dev_attr.attr,
	&sensor_dev_attr_pwm1_auto_point1_pwm.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_pwm2_chmod_attr[] = {
	&sensor_dev_attr_pwm2_freq.dev_attr.attr,
	&sensor_dev_attr_pwm2_enable.dev_attr.attr,
	&sensor_dev_attr_pwm2_ramp_rate.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_channels_zone.dev_attr.attr,
	&sensor_dev_attr_pwm2_auto_point1_pwm.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_pwm3_chmod_attr[] = {
	&sensor_dev_attr_pwm3_freq.dev_attr.attr,
	&sensor_dev_attr_pwm3_enable.dev_attr.attr,
	&sensor_dev_attr_pwm3_ramp_rate.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_channels_zone.dev_attr.attr,
	&sensor_dev_attr_pwm3_auto_point1_pwm.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_pwm5_chmod_attr[] = {
	&sensor_dev_attr_pwm5.dev_attr.attr,
	&sensor_dev_attr_pwm5_freq.dev_attr.attr,
	NULL
};
static struct attribute *dme1737_pwm6_chmod_attr[] = {
	&sensor_dev_attr_pwm6.dev_attr.attr,
	&sensor_dev_attr_pwm6_freq.dev_attr.attr,
	NULL
};

static const struct attribute_group dme1737_pwm_chmod_group[] = {
	{ .attrs = dme1737_pwm1_chmod_attr },
	{ .attrs = dme1737_pwm2_chmod_attr },
	{ .attrs = dme1737_pwm3_chmod_attr },
	{ .attrs = NULL },
	{ .attrs = dme1737_pwm5_chmod_attr },
	{ .attrs = dme1737_pwm6_chmod_attr },
};

static struct attribute *dme1737_pwm_chmod_attr[] = {
	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm2.dev_attr.attr,
	&sensor_dev_attr_pwm3.dev_attr.attr,
};


static inline void dme1737_sio_enter(int sio_cip)
{
	outb(0x55, sio_cip);
}

static inline void dme1737_sio_exit(int sio_cip)
{
	outb(0xaa, sio_cip);
}

static inline int dme1737_sio_inb(int sio_cip, int reg)
{
	outb(reg, sio_cip);
	return inb(sio_cip + 1);
}

static inline void dme1737_sio_outb(int sio_cip, int reg, int val)
{
	outb(reg, sio_cip);
	outb(val, sio_cip + 1);
}


static int dme1737_i2c_get_features(int, struct dme1737_data*);

static void dme1737_chmod_file(struct device *dev,
			       struct attribute *attr, umode_t mode)
{
	if (sysfs_chmod_file(&dev->kobj, attr, mode)) {
		dev_warn(dev, "Failed to change permissions of %s.\n",
			 attr->name);
	}
}

static void dme1737_chmod_group(struct device *dev,
				const struct attribute_group *group,
				umode_t mode)
{
	struct attribute **attr;

	for (attr = group->attrs; *attr; attr++)
		dme1737_chmod_file(dev, *attr, mode);
}

static void dme1737_remove_files(struct device *dev)
{
	struct dme1737_data *data = dev_get_drvdata(dev);
	int ix;

	for (ix = 0; ix < ARRAY_SIZE(dme1737_fan_group); ix++) {
		if (data->has_features & HAS_FAN(ix)) {
			sysfs_remove_group(&dev->kobj,
					   &dme1737_fan_group[ix]);
		}
	}

	for (ix = 0; ix < ARRAY_SIZE(dme1737_pwm_group); ix++) {
		if (data->has_features & HAS_PWM(ix)) {
			sysfs_remove_group(&dev->kobj,
					   &dme1737_pwm_group[ix]);
			if ((data->has_features & HAS_PWM_MIN) && ix < 3) {
				sysfs_remove_file(&dev->kobj,
						dme1737_auto_pwm_min_attr[ix]);
			}
		}
	}

	if (data->has_features & HAS_TEMP_OFFSET)
		sysfs_remove_group(&dev->kobj, &dme1737_temp_offset_group);
	if (data->has_features & HAS_VID)
		sysfs_remove_group(&dev->kobj, &dme1737_vid_group);
	if (data->has_features & HAS_ZONE3)
		sysfs_remove_group(&dev->kobj, &dme1737_zone3_group);
	if (data->has_features & HAS_ZONE_HYST)
		sysfs_remove_group(&dev->kobj, &dme1737_zone_hyst_group);
	if (data->has_features & HAS_IN7)
		sysfs_remove_group(&dev->kobj, &dme1737_in7_group);
	sysfs_remove_group(&dev->kobj, &dme1737_group);

	if (!data->client)
		sysfs_remove_file(&dev->kobj, &dev_attr_name.attr);
}

static int dme1737_create_files(struct device *dev)
{
	struct dme1737_data *data = dev_get_drvdata(dev);
	int err, ix;

	
	if (!data->client) {
		err = sysfs_create_file(&dev->kobj, &dev_attr_name.attr);
		if (err)
			goto exit;
	}

	
	err = sysfs_create_group(&dev->kobj, &dme1737_group);
	if (err)
		goto exit_remove;

	
	if (data->has_features & HAS_TEMP_OFFSET) {
		err = sysfs_create_group(&dev->kobj,
					 &dme1737_temp_offset_group);
		if (err)
			goto exit_remove;
	}
	if (data->has_features & HAS_VID) {
		err = sysfs_create_group(&dev->kobj, &dme1737_vid_group);
		if (err)
			goto exit_remove;
	}
	if (data->has_features & HAS_ZONE3) {
		err = sysfs_create_group(&dev->kobj, &dme1737_zone3_group);
		if (err)
			goto exit_remove;
	}
	if (data->has_features & HAS_ZONE_HYST) {
		err = sysfs_create_group(&dev->kobj, &dme1737_zone_hyst_group);
		if (err)
			goto exit_remove;
	}
	if (data->has_features & HAS_IN7) {
		err = sysfs_create_group(&dev->kobj, &dme1737_in7_group);
		if (err)
			goto exit_remove;
	}

	
	for (ix = 0; ix < ARRAY_SIZE(dme1737_fan_group); ix++) {
		if (data->has_features & HAS_FAN(ix)) {
			err = sysfs_create_group(&dev->kobj,
						 &dme1737_fan_group[ix]);
			if (err)
				goto exit_remove;
		}
	}

	
	for (ix = 0; ix < ARRAY_SIZE(dme1737_pwm_group); ix++) {
		if (data->has_features & HAS_PWM(ix)) {
			err = sysfs_create_group(&dev->kobj,
						 &dme1737_pwm_group[ix]);
			if (err)
				goto exit_remove;
			if ((data->has_features & HAS_PWM_MIN) && (ix < 3)) {
				err = sysfs_create_file(&dev->kobj,
						dme1737_auto_pwm_min_attr[ix]);
				if (err)
					goto exit_remove;
			}
		}
	}

	if (data->config & 0x02) {
		dev_info(dev, "Device is locked. Some attributes "
			 "will be read-only.\n");
	} else {
		
		dme1737_chmod_group(dev, &dme1737_zone_chmod_group,
				    S_IRUGO | S_IWUSR);

		
		if (data->has_features & HAS_TEMP_OFFSET) {
			dme1737_chmod_group(dev, &dme1737_temp_offset_group,
					    S_IRUGO | S_IWUSR);
		}
		if (data->has_features & HAS_ZONE3) {
			dme1737_chmod_group(dev, &dme1737_zone3_chmod_group,
					    S_IRUGO | S_IWUSR);
		}
		if (data->has_features & HAS_ZONE_HYST) {
			dme1737_chmod_group(dev, &dme1737_zone_hyst_group,
					    S_IRUGO | S_IWUSR);
		}

		
		for (ix = 0; ix < ARRAY_SIZE(dme1737_pwm_chmod_group); ix++) {
			if (data->has_features & HAS_PWM(ix)) {
				dme1737_chmod_group(dev,
						&dme1737_pwm_chmod_group[ix],
						S_IRUGO | S_IWUSR);
				if ((data->has_features & HAS_PWM_MIN) &&
				    ix < 3) {
					dme1737_chmod_file(dev,
						dme1737_auto_pwm_min_attr[ix],
						S_IRUGO | S_IWUSR);
				}
			}
		}

		
		for (ix = 0; ix < 3; ix++) {
			if ((data->has_features & HAS_PWM(ix)) &&
			    (PWM_EN_FROM_REG(data->pwm_config[ix]) == 1)) {
				dme1737_chmod_file(dev,
						dme1737_pwm_chmod_attr[ix],
						S_IRUGO | S_IWUSR);
			}
		}
	}

	return 0;

exit_remove:
	dme1737_remove_files(dev);
exit:
	return err;
}

static int dme1737_init_device(struct device *dev)
{
	struct dme1737_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ix;
	u8 reg;

	
	data->in_nominal = IN_NOMINAL(data->type);

	data->config = dme1737_read(data, DME1737_REG_CONFIG);
	
	if (!(data->config & 0x01)) {
		if (!force_start) {
			dev_err(dev, "Device is not monitoring. "
				"Use the force_start load parameter to "
				"override.\n");
			return -EFAULT;
		}

		
		data->config |= 0x01;
		dme1737_write(data, DME1737_REG_CONFIG, data->config);
	}
	
	if (!(data->config & 0x04)) {
		dev_err(dev, "Device is not ready.\n");
		return -EFAULT;
	}

	if (client) {   
		data->config2 = dme1737_read(data, DME1737_REG_CONFIG2);
		
		if (data->config2 & 0x04)
			data->has_features |= HAS_FAN(2);

		if (client->addr == 0x2e)
			data->has_features |= HAS_FAN(3) | HAS_PWM(2);

		if (dme1737_i2c_get_features(0x2e, data) &&
		    dme1737_i2c_get_features(0x4e, data)) {
			dev_warn(dev, "Failed to query Super-IO for optional "
				 "features.\n");
		}
	}

	
	data->has_features |= HAS_FAN(0) | HAS_FAN(1) | HAS_PWM(0) | HAS_PWM(1);

	
	switch (data->type) {
	case dme1737:
		data->has_features |= HAS_TEMP_OFFSET | HAS_VID | HAS_ZONE3 |
			HAS_ZONE_HYST | HAS_PWM_MIN;
		break;
	case sch311x:
		data->has_features |= HAS_TEMP_OFFSET | HAS_ZONE3 |
			HAS_ZONE_HYST | HAS_PWM_MIN | HAS_FAN(2) | HAS_PWM(2);
		break;
	case sch5027:
		data->has_features |= HAS_ZONE3;
		break;
	case sch5127:
		data->has_features |= HAS_FAN(2) | HAS_PWM(2) | HAS_IN7;
		break;
	default:
		break;
	}

	dev_info(dev, "Optional features: pwm3=%s, pwm5=%s, pwm6=%s, "
		 "fan3=%s, fan4=%s, fan5=%s, fan6=%s.\n",
		 (data->has_features & HAS_PWM(2)) ? "yes" : "no",
		 (data->has_features & HAS_PWM(4)) ? "yes" : "no",
		 (data->has_features & HAS_PWM(5)) ? "yes" : "no",
		 (data->has_features & HAS_FAN(2)) ? "yes" : "no",
		 (data->has_features & HAS_FAN(3)) ? "yes" : "no",
		 (data->has_features & HAS_FAN(4)) ? "yes" : "no",
		 (data->has_features & HAS_FAN(5)) ? "yes" : "no");

	reg = dme1737_read(data, DME1737_REG_TACH_PWM);
	
	if (client && reg != 0xa4) {   
		dev_warn(dev, "Non-standard fan to pwm mapping: "
			 "fan1->pwm%d, fan2->pwm%d, fan3->pwm%d, "
			 "fan4->pwm%d. Please report to the driver "
			 "maintainer.\n",
			 (reg & 0x03) + 1, ((reg >> 2) & 0x03) + 1,
			 ((reg >> 4) & 0x03) + 1, ((reg >> 6) & 0x03) + 1);
	} else if (!client && reg != 0x24) {   
		dev_warn(dev, "Non-standard fan to pwm mapping: "
			 "fan1->pwm%d, fan2->pwm%d, fan3->pwm%d. "
			 "Please report to the driver maintainer.\n",
			 (reg & 0x03) + 1, ((reg >> 2) & 0x03) + 1,
			 ((reg >> 4) & 0x03) + 1);
	}

	if (!(data->config & 0x02)) {
		for (ix = 0; ix < 3; ix++) {
			data->pwm_config[ix] = dme1737_read(data,
						DME1737_REG_PWM_CONFIG(ix));
			if ((data->has_features & HAS_PWM(ix)) &&
			    (PWM_EN_FROM_REG(data->pwm_config[ix]) == -1)) {
				dev_info(dev, "Switching pwm%d to "
					 "manual mode.\n", ix + 1);
				data->pwm_config[ix] = PWM_EN_TO_REG(1,
							data->pwm_config[ix]);
				dme1737_write(data, DME1737_REG_PWM(ix), 0);
				dme1737_write(data,
					      DME1737_REG_PWM_CONFIG(ix),
					      data->pwm_config[ix]);
			}
		}
	}

	
	data->pwm_acz[0] = 1;	
	data->pwm_acz[1] = 2;	
	data->pwm_acz[2] = 4;	

	
	if (data->has_features & HAS_VID)
		data->vrm = vid_which_vrm();

	return 0;
}


static struct i2c_driver dme1737_i2c_driver;

static int dme1737_i2c_get_features(int sio_cip, struct dme1737_data *data)
{
	int err = 0, reg;
	u16 addr;

	dme1737_sio_enter(sio_cip);

	reg = force_id ? force_id : dme1737_sio_inb(sio_cip, 0x20);
	if (!(reg == DME1737_ID_1 || reg == DME1737_ID_2 ||
	      reg == SCH5027_ID)) {
		err = -ENODEV;
		goto exit;
	}

	
	dme1737_sio_outb(sio_cip, 0x07, 0x0a);

	
	addr = (dme1737_sio_inb(sio_cip, 0x60) << 8) |
		dme1737_sio_inb(sio_cip, 0x61);
	if (!addr) {
		err = -ENODEV;
		goto exit;
	}

	if ((inb(addr + 0x43) & 0x0c) == 0x08) 
		data->has_features |= HAS_FAN(5);
	if ((inb(addr + 0x44) & 0x0c) == 0x08) 
		data->has_features |= HAS_PWM(5);
	if ((inb(addr + 0x45) & 0x0c) == 0x08) 
		data->has_features |= HAS_FAN(4);
	if ((inb(addr + 0x46) & 0x0c) == 0x08) 
		data->has_features |= HAS_PWM(4);

exit:
	dme1737_sio_exit(sio_cip);

	return err;
}

static int dme1737_i2c_detect(struct i2c_client *client,
			      struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	struct device *dev = &adapter->dev;
	u8 company, verstep = 0;
	const char *name;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	company = i2c_smbus_read_byte_data(client, DME1737_REG_COMPANY);
	verstep = i2c_smbus_read_byte_data(client, DME1737_REG_VERSTEP);

	if (company == DME1737_COMPANY_SMSC &&
	    verstep == SCH5027_VERSTEP) {
		name = "sch5027";
	} else if (company == DME1737_COMPANY_SMSC &&
		   (verstep & DME1737_VERSTEP_MASK) == DME1737_VERSTEP) {
		name = "dme1737";
	} else {
		return -ENODEV;
	}

	dev_info(dev, "Found a %s chip at 0x%02x (rev 0x%02x).\n",
		 verstep == SCH5027_VERSTEP ? "SCH5027" : "DME1737",
		 client->addr, verstep);
	strlcpy(info->type, name, I2C_NAME_SIZE);

	return 0;
}

static int dme1737_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct dme1737_data *data;
	struct device *dev = &client->dev;
	int err;

	data = kzalloc(sizeof(struct dme1737_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);
	data->type = id->driver_data;
	data->client = client;
	data->name = client->name;
	mutex_init(&data->update_lock);

	
	err = dme1737_init_device(dev);
	if (err) {
		dev_err(dev, "Failed to initialize device.\n");
		goto exit_kfree;
	}

	
	err = dme1737_create_files(dev);
	if (err) {
		dev_err(dev, "Failed to create sysfs files.\n");
		goto exit_kfree;
	}

	
	data->hwmon_dev = hwmon_device_register(dev);
	if (IS_ERR(data->hwmon_dev)) {
		dev_err(dev, "Failed to register device.\n");
		err = PTR_ERR(data->hwmon_dev);
		goto exit_remove;
	}

	return 0;

exit_remove:
	dme1737_remove_files(dev);
exit_kfree:
	kfree(data);
exit:
	return err;
}

static int dme1737_i2c_remove(struct i2c_client *client)
{
	struct dme1737_data *data = i2c_get_clientdata(client);

	hwmon_device_unregister(data->hwmon_dev);
	dme1737_remove_files(&client->dev);

	kfree(data);
	return 0;
}

static const struct i2c_device_id dme1737_id[] = {
	{ "dme1737", dme1737 },
	{ "sch5027", sch5027 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dme1737_id);

static struct i2c_driver dme1737_i2c_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		.name = "dme1737",
	},
	.probe = dme1737_i2c_probe,
	.remove = dme1737_i2c_remove,
	.id_table = dme1737_id,
	.detect = dme1737_i2c_detect,
	.address_list = normal_i2c,
};


static int __init dme1737_isa_detect(int sio_cip, unsigned short *addr)
{
	int err = 0, reg;
	unsigned short base_addr;

	dme1737_sio_enter(sio_cip);

	reg = force_id ? force_id : dme1737_sio_inb(sio_cip, 0x20);
	if (!(reg == SCH3112_ID || reg == SCH3114_ID || reg == SCH3116_ID ||
	      reg == SCH5127_ID)) {
		err = -ENODEV;
		goto exit;
	}

	
	dme1737_sio_outb(sio_cip, 0x07, 0x0a);

	
	base_addr = (dme1737_sio_inb(sio_cip, 0x60) << 8) |
		     dme1737_sio_inb(sio_cip, 0x61);
	if (!base_addr) {
		pr_err("Base address not set\n");
		err = -ENODEV;
		goto exit;
	}

	*addr = base_addr + 0x70;

exit:
	dme1737_sio_exit(sio_cip);
	return err;
}

static int __init dme1737_isa_device_add(unsigned short addr)
{
	struct resource res = {
		.start	= addr,
		.end	= addr + DME1737_EXTENT - 1,
		.name	= "dme1737",
		.flags	= IORESOURCE_IO,
	};
	int err;

	err = acpi_check_resource_conflict(&res);
	if (err)
		goto exit;

	pdev = platform_device_alloc("dme1737", addr);
	if (!pdev) {
		pr_err("Failed to allocate device\n");
		err = -ENOMEM;
		goto exit;
	}

	err = platform_device_add_resources(pdev, &res, 1);
	if (err) {
		pr_err("Failed to add device resource (err = %d)\n", err);
		goto exit_device_put;
	}

	err = platform_device_add(pdev);
	if (err) {
		pr_err("Failed to add device (err = %d)\n", err);
		goto exit_device_put;
	}

	return 0;

exit_device_put:
	platform_device_put(pdev);
	pdev = NULL;
exit:
	return err;
}

static int __devinit dme1737_isa_probe(struct platform_device *pdev)
{
	u8 company, device;
	struct resource *res;
	struct dme1737_data *data;
	struct device *dev = &pdev->dev;
	int err;

	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (!request_region(res->start, DME1737_EXTENT, "dme1737")) {
		dev_err(dev, "Failed to request region 0x%04x-0x%04x.\n",
			(unsigned short)res->start,
			(unsigned short)res->start + DME1737_EXTENT - 1);
		err = -EBUSY;
		goto exit;
	}

	data = kzalloc(sizeof(struct dme1737_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit_release_region;
	}

	data->addr = res->start;
	platform_set_drvdata(pdev, data);

	
	switch (force_id) {
	case SCH3112_ID:
	case SCH3114_ID:
	case SCH3116_ID:
		data->type = sch311x;
		break;
	case SCH5127_ID:
		data->type = sch5127;
		break;
	default:
		company = dme1737_read(data, DME1737_REG_COMPANY);
		device = dme1737_read(data, DME1737_REG_DEVICE);

		if ((company == DME1737_COMPANY_SMSC) &&
		    (device == SCH311X_DEVICE)) {
			data->type = sch311x;
		} else if ((company == DME1737_COMPANY_SMSC) &&
			   (device == SCH5127_DEVICE)) {
			data->type = sch5127;
		} else {
			err = -ENODEV;
			goto exit_kfree;
		}
	}

	if (data->type == sch5127)
		data->name = "sch5127";
	else
		data->name = "sch311x";

	
	mutex_init(&data->update_lock);

	dev_info(dev, "Found a %s chip at 0x%04x\n",
		 data->type == sch5127 ? "SCH5127" : "SCH311x", data->addr);

	
	err = dme1737_init_device(dev);
	if (err) {
		dev_err(dev, "Failed to initialize device.\n");
		goto exit_kfree;
	}

	
	err = dme1737_create_files(dev);
	if (err) {
		dev_err(dev, "Failed to create sysfs files.\n");
		goto exit_kfree;
	}

	
	data->hwmon_dev = hwmon_device_register(dev);
	if (IS_ERR(data->hwmon_dev)) {
		dev_err(dev, "Failed to register device.\n");
		err = PTR_ERR(data->hwmon_dev);
		goto exit_remove_files;
	}

	return 0;

exit_remove_files:
	dme1737_remove_files(dev);
exit_kfree:
	platform_set_drvdata(pdev, NULL);
	kfree(data);
exit_release_region:
	release_region(res->start, DME1737_EXTENT);
exit:
	return err;
}

static int __devexit dme1737_isa_remove(struct platform_device *pdev)
{
	struct dme1737_data *data = platform_get_drvdata(pdev);

	hwmon_device_unregister(data->hwmon_dev);
	dme1737_remove_files(&pdev->dev);
	release_region(data->addr, DME1737_EXTENT);
	platform_set_drvdata(pdev, NULL);
	kfree(data);

	return 0;
}

static struct platform_driver dme1737_isa_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "dme1737",
	},
	.probe = dme1737_isa_probe,
	.remove = __devexit_p(dme1737_isa_remove),
};


static int __init dme1737_init(void)
{
	int err;
	unsigned short addr;

	err = i2c_add_driver(&dme1737_i2c_driver);
	if (err)
		goto exit;

	if (dme1737_isa_detect(0x2e, &addr) &&
	    dme1737_isa_detect(0x4e, &addr) &&
	    (!probe_all_addr ||
	     (dme1737_isa_detect(0x162e, &addr) &&
	      dme1737_isa_detect(0x164e, &addr)))) {
		
		return 0;
	}

	err = platform_driver_register(&dme1737_isa_driver);
	if (err)
		goto exit_del_i2c_driver;

	
	err = dme1737_isa_device_add(addr);
	if (err)
		goto exit_del_isa_driver;

	return 0;

exit_del_isa_driver:
	platform_driver_unregister(&dme1737_isa_driver);
exit_del_i2c_driver:
	i2c_del_driver(&dme1737_i2c_driver);
exit:
	return err;
}

static void __exit dme1737_exit(void)
{
	if (pdev) {
		platform_device_unregister(pdev);
		platform_driver_unregister(&dme1737_isa_driver);
	}

	i2c_del_driver(&dme1737_i2c_driver);
}

MODULE_AUTHOR("Juerg Haefliger <juergh@gmail.com>");
MODULE_DESCRIPTION("DME1737 sensors");
MODULE_LICENSE("GPL");

module_init(dme1737_init);
module_exit(dme1737_exit);
