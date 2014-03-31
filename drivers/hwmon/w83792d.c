/*
 * w83792d.c - Part of lm_sensors, Linux kernel modules for hardware
 *	       monitoring
 * Copyright (C) 2004, 2005 Winbond Electronics Corp.
 *			    Chunhao Huang <DZShen@Winbond.com.tw>,
 *			    Rudolf Marek <r.marek@assembler.cz>
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
 *
 * Note:
 * 1. This driver is only for 2.6 kernel, 2.4 kernel need a different driver.
 * 2. This driver is only for Winbond W83792D C version device, there
 *     are also some motherboards with B version W83792D device. The
 *     calculation method to in6-in7(measured value, limits) is a little
 *     different between C and B version. C or B version can be identified
 *     by CR[0x49h].
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>

static const unsigned short normal_i2c[] = { 0x2c, 0x2d, 0x2e, 0x2f,
						I2C_CLIENT_END };


static unsigned short force_subclients[4];
module_param_array(force_subclients, short, NULL, 0);
MODULE_PARM_DESC(force_subclients, "List of subclient addresses: "
			"{bus, clientaddr, subclientaddr1, subclientaddr2}");

static bool init;
module_param(init, bool, 0);
MODULE_PARM_DESC(init, "Set to one to force chip initialization");

static const u8 W83792D_REG_IN[9] = {
	0x20,	
	0x21,	
	0x22,	
	0x23,	
	0x24,	
	0x25,	
	0x26,	
	0xB0,	
	0xB1	
};
#define W83792D_REG_LOW_BITS1 0x3E  
#define W83792D_REG_LOW_BITS2 0x3F  
static const u8 W83792D_REG_IN_MAX[9] = {
	0x2B,	
	0x2D,	
	0x2F,	
	0x31,	
	0x33,	
	0x35,	
	0x37,	
	0xB4,	
	0xB6	
};
static const u8 W83792D_REG_IN_MIN[9] = {
	0x2C,	
	0x2E,	
	0x30,	
	0x32,	
	0x34,	
	0x36,	
	0x38,	
	0xB5,	
	0xB7	
};
static const u8 W83792D_REG_FAN[7] = {
	0x28,	
	0x29,	
	0x2A,	
	0xB8,	
	0xB9,	
	0xBA,	
	0xBE	
};
static const u8 W83792D_REG_FAN_MIN[7] = {
	0x3B,	
	0x3C,	
	0x3D,	
	0xBB,	
	0xBC,	
	0xBD,	
	0xBF	
};
#define W83792D_REG_FAN_CFG 0x84	
static const u8 W83792D_REG_FAN_DIV[4] = {
	0x47,	
	0x5B,	
	0x5C,	
	0x9E	
};
static const u8 W83792D_REG_PWM[7] = {
	0x81,	
	0x83,	
	0x94,	
	0xA3,	
	0xA4,	
	0xA5,	
	0xA6	
};
#define W83792D_REG_BANK		0x4E
#define W83792D_REG_TEMP2_CONFIG	0xC2
#define W83792D_REG_TEMP3_CONFIG	0xCA

static const u8 W83792D_REG_TEMP1[3] = {
	0x27,	
	0x39,	
	0x3A,	
};

static const u8 W83792D_REG_TEMP_ADD[2][6] = {
	{ 0xC0,		
	  0xC1,		
	  0xC5,		
	  0xC6,		
	  0xC3,		
	  0xC4 },	
	{ 0xC8,		
	  0xC9,		
	  0xCD,		
	  0xCE,		
	  0xCB,		
	  0xCC }	
};

static const u8 W83792D_REG_THERMAL[3] = {
	0x85,	
	0x86,	
	0x96	
};

static const u8 W83792D_REG_TOLERANCE[3] = {
	0x87,	
	0x87,	
	0x97	
};

static const u8 W83792D_REG_POINTS[3][4] = {
	{ 0x85,		
	  0xE3,		
	  0xE4,		
	  0xE5 },	
	{ 0x86,		
	  0xE6,		
	  0xE7,		
	  0xE8 },	
	{ 0x96,		
	  0xE9,		
	  0xEA,		
	  0xEB }	
};

static const u8 W83792D_REG_LEVELS[3][4] = {
	{ 0x88,		
	  0x88,		
	  0xE0,		
	  0xE0 },	
	{ 0x89,		
	  0x89,		
	  0xE1,		
	  0xE1 },	
	{ 0x98,		
	  0x98,		
	  0xE2,		
	  0xE2 }	
};

#define W83792D_REG_GPIO_EN		0x1A
#define W83792D_REG_CONFIG		0x40
#define W83792D_REG_VID_FANDIV		0x47
#define W83792D_REG_CHIPID		0x49
#define W83792D_REG_WCHIPID		0x58
#define W83792D_REG_CHIPMAN		0x4F
#define W83792D_REG_PIN			0x4B
#define W83792D_REG_I2C_SUBADDR		0x4A

#define W83792D_REG_ALARM1 0xA9		
#define W83792D_REG_ALARM2 0xAA		
#define W83792D_REG_ALARM3 0xAB		
#define W83792D_REG_CHASSIS 0x42	
#define W83792D_REG_CHASSIS_CLR 0x44	

#define W83792D_REG_VID_IN_B		0x17

#define W83792D_REG_VBAT		0x5D
#define W83792D_REG_I2C_ADDR		0x48

#define IN_FROM_REG(nr, val) (((nr) <= 1) ? ((val) * 2) : \
		((((nr) == 6) || ((nr) == 7)) ? ((val) * 6) : ((val) * 4)))
#define IN_TO_REG(nr, val) (((nr) <= 1) ? ((val) / 2) : \
		((((nr) == 6) || ((nr) == 7)) ? ((val) / 6) : ((val) / 4)))

static inline u8
FAN_TO_REG(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((1350000 + rpm * div / 2) / (rpm * div), 1, 254);
}

#define FAN_FROM_REG(val, div)	((val) == 0   ? -1 : \
				((val) == 255 ? 0 : \
						1350000 / ((val) * (div))))

#define TEMP1_TO_REG(val)	(SENSORS_LIMIT(((val) < 0 ? (val)+0x100*1000 \
					: (val)) / 1000, 0, 0xff))
#define TEMP1_FROM_REG(val)	(((val) & 0x80 ? (val)-0x100 : (val)) * 1000)
#define TEMP_ADD_FROM_REG(val1, val2) \
	((((val1) & 0x80 ? (val1)-0x100 \
		: (val1)) * 1000) + ((val2 & 0x80) ? 500 : 0))
#define TEMP_ADD_TO_REG_HIGH(val) \
	(SENSORS_LIMIT(((val) < 0 ? (val)+0x100*1000 \
			: (val)) / 1000, 0, 0xff))
#define TEMP_ADD_TO_REG_LOW(val)	((val%1000) ? 0x80 : 0x00)

#define DIV_FROM_REG(val)		(1 << (val))

static inline u8
DIV_TO_REG(long val)
{
	int i;
	val = SENSORS_LIMIT(val, 1, 128) >> 1;
	for (i = 0; i < 7; i++) {
		if (val == 0)
			break;
		val >>= 1;
	}
	return (u8)i;
}

struct w83792d_data {
	struct device *hwmon_dev;

	struct mutex update_lock;
	char valid;		
	unsigned long last_updated;	

	
	struct i2c_client *lm75[2];

	u8 in[9];		
	u8 in_max[9];		
	u8 in_min[9];		
	u16 low_bits;		
	u8 fan[7];		
	u8 fan_min[7];		
	u8 temp1[3];		
	u8 temp_add[2][6];	
	u8 fan_div[7];		
	u8 pwm[7];		
	u8 pwmenable[3];
	u32 alarms;		
	u8 chassis;		
	u8 chassis_clear;	
	u8 thermal_cruise[3];	
	u8 tolerance[3];	
	u8 sf2_points[3][4];	
	u8 sf2_levels[3][4];	
};

static int w83792d_probe(struct i2c_client *client,
			 const struct i2c_device_id *id);
static int w83792d_detect(struct i2c_client *client,
			  struct i2c_board_info *info);
static int w83792d_remove(struct i2c_client *client);
static struct w83792d_data *w83792d_update_device(struct device *dev);

#ifdef DEBUG
static void w83792d_print_debug(struct w83792d_data *data, struct device *dev);
#endif

static void w83792d_init_client(struct i2c_client *client);

static const struct i2c_device_id w83792d_id[] = {
	{ "w83792d", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, w83792d_id);

static struct i2c_driver w83792d_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name = "w83792d",
	},
	.probe		= w83792d_probe,
	.remove		= w83792d_remove,
	.id_table	= w83792d_id,
	.detect		= w83792d_detect,
	.address_list	= normal_i2c,
};

static inline long in_count_from_reg(int nr, struct w83792d_data *data)
{
	
	return (data->in[nr] << 2) | ((data->low_bits >> (2 * nr)) & 0x03);
}

static inline int w83792d_read_value(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static inline int
w83792d_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static ssize_t show_in(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%ld\n",
		       IN_FROM_REG(nr, in_count_from_reg(nr, data)));
}

#define show_in_reg(reg) \
static ssize_t show_##reg(struct device *dev, struct device_attribute *attr, \
			char *buf) \
{ \
	struct sensor_device_attribute *sensor_attr \
		= to_sensor_dev_attr(attr); \
	int nr = sensor_attr->index; \
	struct w83792d_data *data = w83792d_update_device(dev); \
	return sprintf(buf, "%ld\n", \
		       (long)(IN_FROM_REG(nr, data->reg[nr]) * 4)); \
}

show_in_reg(in_min);
show_in_reg(in_max);

#define store_in_reg(REG, reg) \
static ssize_t store_in_##reg(struct device *dev, \
				struct device_attribute *attr, \
				const char *buf, size_t count) \
{ \
	struct sensor_device_attribute *sensor_attr \
			= to_sensor_dev_attr(attr); \
	int nr = sensor_attr->index; \
	struct i2c_client *client = to_i2c_client(dev); \
	struct w83792d_data *data = i2c_get_clientdata(client); \
	unsigned long val; \
	int err = kstrtoul(buf, 10, &val); \
	if (err) \
		return err; \
	mutex_lock(&data->update_lock); \
	data->in_##reg[nr] = SENSORS_LIMIT(IN_TO_REG(nr, val) / 4, 0, 255); \
	w83792d_write_value(client, W83792D_REG_IN_##REG[nr], \
			    data->in_##reg[nr]); \
	mutex_unlock(&data->update_lock); \
	 \
	return count; \
}
store_in_reg(MIN, min);
store_in_reg(MAX, max);

#define show_fan_reg(reg) \
static ssize_t show_##reg(struct device *dev, struct device_attribute *attr, \
			char *buf) \
{ \
	struct sensor_device_attribute *sensor_attr \
			= to_sensor_dev_attr(attr); \
	int nr = sensor_attr->index - 1; \
	struct w83792d_data *data = w83792d_update_device(dev); \
	return sprintf(buf, "%d\n", \
		FAN_FROM_REG(data->reg[nr], DIV_FROM_REG(data->fan_div[nr]))); \
}

show_fan_reg(fan);
show_fan_reg(fan_min);

static ssize_t
store_fan_min(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index - 1;
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->fan_min[nr] = FAN_TO_REG(val, DIV_FROM_REG(data->fan_div[nr]));
	w83792d_write_value(client, W83792D_REG_FAN_MIN[nr],
				data->fan_min[nr]);
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t
show_fan_div(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%u\n", DIV_FROM_REG(data->fan_div[nr - 1]));
}

static ssize_t
store_fan_div(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index - 1;
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	unsigned long min;
	
	u8 fan_div_reg = 0;
	u8 tmp_fan_div;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	
	mutex_lock(&data->update_lock);
	min = FAN_FROM_REG(data->fan_min[nr],
			   DIV_FROM_REG(data->fan_div[nr]));

	data->fan_div[nr] = DIV_TO_REG(val);

	fan_div_reg = w83792d_read_value(client, W83792D_REG_FAN_DIV[nr >> 1]);
	fan_div_reg &= (nr & 0x01) ? 0x8f : 0xf8;
	tmp_fan_div = (nr & 0x01) ? (((data->fan_div[nr]) << 4) & 0x70)
					: ((data->fan_div[nr]) & 0x07);
	w83792d_write_value(client, W83792D_REG_FAN_DIV[nr >> 1],
					fan_div_reg | tmp_fan_div);

	
	data->fan_min[nr] = FAN_TO_REG(min, DIV_FROM_REG(data->fan_div[nr]));
	w83792d_write_value(client, W83792D_REG_FAN_MIN[nr], data->fan_min[nr]);
	mutex_unlock(&data->update_lock);

	return count;
}


static ssize_t show_temp1(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%d\n", TEMP1_FROM_REG(data->temp1[nr]));
}

static ssize_t store_temp1(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->temp1[nr] = TEMP1_TO_REG(val);
	w83792d_write_value(client, W83792D_REG_TEMP1[nr],
		data->temp1[nr]);
	mutex_unlock(&data->update_lock);

	return count;
}


static ssize_t show_temp23(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct sensor_device_attribute_2 *sensor_attr
	  = to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index;
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%ld\n",
		(long)TEMP_ADD_FROM_REG(data->temp_add[nr][index],
			data->temp_add[nr][index+1]));
}

static ssize_t store_temp23(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sensor_attr
	  = to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index;
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->temp_add[nr][index] = TEMP_ADD_TO_REG_HIGH(val);
	data->temp_add[nr][index+1] = TEMP_ADD_TO_REG_LOW(val);
	w83792d_write_value(client, W83792D_REG_TEMP_ADD[nr][index],
		data->temp_add[nr][index]);
	w83792d_write_value(client, W83792D_REG_TEMP_ADD[nr][index+1],
		data->temp_add[nr][index+1]);
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t
show_alarms_reg(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%d\n", data->alarms);
}

static ssize_t show_alarm(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%d\n", (data->alarms >> nr) & 1);
}

static ssize_t
show_pwm(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%d\n", (data->pwm[nr] & 0x0f) << 4);
}

static ssize_t
show_pwmenable(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index - 1;
	struct w83792d_data *data = w83792d_update_device(dev);
	long pwm_enable_tmp = 1;

	switch (data->pwmenable[nr]) {
	case 0:
		pwm_enable_tmp = 1; 
		break;
	case 1:
		pwm_enable_tmp = 3; 
		break;
	case 2:
		pwm_enable_tmp = 2; 
		break;
	}

	return sprintf(buf, "%ld\n", pwm_enable_tmp);
}

static ssize_t
store_pwm(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;
	val = SENSORS_LIMIT(val, 0, 255) >> 4;

	mutex_lock(&data->update_lock);
	val |= w83792d_read_value(client, W83792D_REG_PWM[nr]) & 0xf0;
	data->pwm[nr] = val;
	w83792d_write_value(client, W83792D_REG_PWM[nr], data->pwm[nr]);
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t
store_pwmenable(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index - 1;
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	u8 fan_cfg_tmp, cfg1_tmp, cfg2_tmp, cfg3_tmp, cfg4_tmp;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	if (val < 1 || val > 3)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	switch (val) {
	case 1:
		data->pwmenable[nr] = 0; 
		break;
	case 2:
		data->pwmenable[nr] = 2; 
		break;
	case 3:
		data->pwmenable[nr] = 1; 
		break;
	}
	cfg1_tmp = data->pwmenable[0];
	cfg2_tmp = (data->pwmenable[1]) << 2;
	cfg3_tmp = (data->pwmenable[2]) << 4;
	cfg4_tmp = w83792d_read_value(client, W83792D_REG_FAN_CFG) & 0xc0;
	fan_cfg_tmp = ((cfg4_tmp | cfg3_tmp) | cfg2_tmp) | cfg1_tmp;
	w83792d_write_value(client, W83792D_REG_FAN_CFG, fan_cfg_tmp);
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t
show_pwm_mode(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%d\n", data->pwm[nr] >> 7);
}

static ssize_t
store_pwm_mode(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;
	if (val > 1)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	data->pwm[nr] = w83792d_read_value(client, W83792D_REG_PWM[nr]);
	if (val) {			
		data->pwm[nr] |= 0x80;
	} else {			
		data->pwm[nr] &= 0x7f;
	}
	w83792d_write_value(client, W83792D_REG_PWM[nr], data->pwm[nr]);
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t
show_chassis(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%d\n", data->chassis);
}

static ssize_t
show_regs_chassis(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	dev_warn(dev,
		 "Attribute %s is deprecated, use intrusion0_alarm instead\n",
		 "chassis");
	return show_chassis(dev, attr, buf);
}

static ssize_t
show_chassis_clear(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%d\n", data->chassis_clear);
}

static ssize_t
store_chassis_clear_legacy(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	unsigned long val;
	int err;
	u8 temp1 = 0, temp2 = 0;

	dev_warn(dev,
		 "Attribute %s is deprecated, use intrusion0_alarm instead\n",
		 "chassis_clear");

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->chassis_clear = SENSORS_LIMIT(val, 0, 1);
	temp1 = ((data->chassis_clear) << 7) & 0x80;
	temp2 = w83792d_read_value(client,
		W83792D_REG_CHASSIS_CLR) & 0x7f;
	w83792d_write_value(client, W83792D_REG_CHASSIS_CLR, temp1 | temp2);
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t
store_chassis_clear(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	unsigned long val;
	u8 reg;

	if (kstrtoul(buf, 10, &val) || val != 0)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	reg = w83792d_read_value(client, W83792D_REG_CHASSIS_CLR);
	w83792d_write_value(client, W83792D_REG_CHASSIS_CLR, reg | 0x80);
	data->valid = 0;		
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t
show_thermal_cruise(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%ld\n", (long)data->thermal_cruise[nr-1]);
}

static ssize_t
store_thermal_cruise(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index - 1;
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	u8 target_tmp = 0, target_mask = 0;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	target_tmp = val;
	target_tmp = target_tmp & 0x7f;
	mutex_lock(&data->update_lock);
	target_mask = w83792d_read_value(client,
					 W83792D_REG_THERMAL[nr]) & 0x80;
	data->thermal_cruise[nr] = SENSORS_LIMIT(target_tmp, 0, 255);
	w83792d_write_value(client, W83792D_REG_THERMAL[nr],
		(data->thermal_cruise[nr]) | target_mask);
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t
show_tolerance(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%ld\n", (long)data->tolerance[nr-1]);
}

static ssize_t
store_tolerance(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index - 1;
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	u8 tol_tmp, tol_mask;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	tol_mask = w83792d_read_value(client,
		W83792D_REG_TOLERANCE[nr]) & ((nr == 1) ? 0x0f : 0xf0);
	tol_tmp = SENSORS_LIMIT(val, 0, 15);
	tol_tmp &= 0x0f;
	data->tolerance[nr] = tol_tmp;
	if (nr == 1)
		tol_tmp <<= 4;
	w83792d_write_value(client, W83792D_REG_TOLERANCE[nr],
		tol_mask | tol_tmp);
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t
show_sf2_point(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sensor_device_attribute_2 *sensor_attr
	  = to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index;
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%ld\n", (long)data->sf2_points[index-1][nr-1]);
}

static ssize_t
store_sf2_point(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sensor_attr
	  = to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr - 1;
	int index = sensor_attr->index - 1;
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	u8 mask_tmp = 0;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->sf2_points[index][nr] = SENSORS_LIMIT(val, 0, 127);
	mask_tmp = w83792d_read_value(client,
					W83792D_REG_POINTS[index][nr]) & 0x80;
	w83792d_write_value(client, W83792D_REG_POINTS[index][nr],
		mask_tmp|data->sf2_points[index][nr]);
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t
show_sf2_level(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sensor_device_attribute_2 *sensor_attr
	  = to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index;
	struct w83792d_data *data = w83792d_update_device(dev);
	return sprintf(buf, "%d\n",
			(((data->sf2_levels[index-1][nr]) * 100) / 15));
}

static ssize_t
store_sf2_level(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sensor_attr
	  = to_sensor_dev_attr_2(attr);
	int nr = sensor_attr->nr;
	int index = sensor_attr->index - 1;
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	u8 mask_tmp = 0, level_tmp = 0;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->sf2_levels[index][nr] = SENSORS_LIMIT((val * 15) / 100, 0, 15);
	mask_tmp = w83792d_read_value(client, W83792D_REG_LEVELS[index][nr])
		& ((nr == 3) ? 0xf0 : 0x0f);
	if (nr == 3)
		level_tmp = data->sf2_levels[index][nr];
	else
		level_tmp = data->sf2_levels[index][nr] << 4;
	w83792d_write_value(client, W83792D_REG_LEVELS[index][nr],
			    level_tmp | mask_tmp);
	mutex_unlock(&data->update_lock);

	return count;
}


static int
w83792d_detect_subclients(struct i2c_client *new_client)
{
	int i, id, err;
	int address = new_client->addr;
	u8 val;
	struct i2c_adapter *adapter = new_client->adapter;
	struct w83792d_data *data = i2c_get_clientdata(new_client);

	id = i2c_adapter_id(adapter);
	if (force_subclients[0] == id && force_subclients[1] == address) {
		for (i = 2; i <= 3; i++) {
			if (force_subclients[i] < 0x48 ||
			    force_subclients[i] > 0x4f) {
				dev_err(&new_client->dev, "invalid subclient "
					"address %d; must be 0x48-0x4f\n",
					force_subclients[i]);
				err = -ENODEV;
				goto ERROR_SC_0;
			}
		}
		w83792d_write_value(new_client, W83792D_REG_I2C_SUBADDR,
					(force_subclients[2] & 0x07) |
					((force_subclients[3] & 0x07) << 4));
	}

	val = w83792d_read_value(new_client, W83792D_REG_I2C_SUBADDR);
	if (!(val & 0x08))
		data->lm75[0] = i2c_new_dummy(adapter, 0x48 + (val & 0x7));
	if (!(val & 0x80)) {
		if ((data->lm75[0] != NULL) &&
			((val & 0x7) == ((val >> 4) & 0x7))) {
			dev_err(&new_client->dev, "duplicate addresses 0x%x, "
				"use force_subclient\n", data->lm75[0]->addr);
			err = -ENODEV;
			goto ERROR_SC_1;
		}
		data->lm75[1] = i2c_new_dummy(adapter,
					      0x48 + ((val >> 4) & 0x7));
	}

	return 0;


ERROR_SC_1:
	if (data->lm75[0] != NULL)
		i2c_unregister_device(data->lm75[0]);
ERROR_SC_0:
	return err;
}

static SENSOR_DEVICE_ATTR(in0_input, S_IRUGO, show_in, NULL, 0);
static SENSOR_DEVICE_ATTR(in1_input, S_IRUGO, show_in, NULL, 1);
static SENSOR_DEVICE_ATTR(in2_input, S_IRUGO, show_in, NULL, 2);
static SENSOR_DEVICE_ATTR(in3_input, S_IRUGO, show_in, NULL, 3);
static SENSOR_DEVICE_ATTR(in4_input, S_IRUGO, show_in, NULL, 4);
static SENSOR_DEVICE_ATTR(in5_input, S_IRUGO, show_in, NULL, 5);
static SENSOR_DEVICE_ATTR(in6_input, S_IRUGO, show_in, NULL, 6);
static SENSOR_DEVICE_ATTR(in7_input, S_IRUGO, show_in, NULL, 7);
static SENSOR_DEVICE_ATTR(in8_input, S_IRUGO, show_in, NULL, 8);
static SENSOR_DEVICE_ATTR(in0_min, S_IWUSR | S_IRUGO,
			show_in_min, store_in_min, 0);
static SENSOR_DEVICE_ATTR(in1_min, S_IWUSR | S_IRUGO,
			show_in_min, store_in_min, 1);
static SENSOR_DEVICE_ATTR(in2_min, S_IWUSR | S_IRUGO,
			show_in_min, store_in_min, 2);
static SENSOR_DEVICE_ATTR(in3_min, S_IWUSR | S_IRUGO,
			show_in_min, store_in_min, 3);
static SENSOR_DEVICE_ATTR(in4_min, S_IWUSR | S_IRUGO,
			show_in_min, store_in_min, 4);
static SENSOR_DEVICE_ATTR(in5_min, S_IWUSR | S_IRUGO,
			show_in_min, store_in_min, 5);
static SENSOR_DEVICE_ATTR(in6_min, S_IWUSR | S_IRUGO,
			show_in_min, store_in_min, 6);
static SENSOR_DEVICE_ATTR(in7_min, S_IWUSR | S_IRUGO,
			show_in_min, store_in_min, 7);
static SENSOR_DEVICE_ATTR(in8_min, S_IWUSR | S_IRUGO,
			show_in_min, store_in_min, 8);
static SENSOR_DEVICE_ATTR(in0_max, S_IWUSR | S_IRUGO,
			show_in_max, store_in_max, 0);
static SENSOR_DEVICE_ATTR(in1_max, S_IWUSR | S_IRUGO,
			show_in_max, store_in_max, 1);
static SENSOR_DEVICE_ATTR(in2_max, S_IWUSR | S_IRUGO,
			show_in_max, store_in_max, 2);
static SENSOR_DEVICE_ATTR(in3_max, S_IWUSR | S_IRUGO,
			show_in_max, store_in_max, 3);
static SENSOR_DEVICE_ATTR(in4_max, S_IWUSR | S_IRUGO,
			show_in_max, store_in_max, 4);
static SENSOR_DEVICE_ATTR(in5_max, S_IWUSR | S_IRUGO,
			show_in_max, store_in_max, 5);
static SENSOR_DEVICE_ATTR(in6_max, S_IWUSR | S_IRUGO,
			show_in_max, store_in_max, 6);
static SENSOR_DEVICE_ATTR(in7_max, S_IWUSR | S_IRUGO,
			show_in_max, store_in_max, 7);
static SENSOR_DEVICE_ATTR(in8_max, S_IWUSR | S_IRUGO,
			show_in_max, store_in_max, 8);
static SENSOR_DEVICE_ATTR_2(temp1_input, S_IRUGO, show_temp1, NULL, 0, 0);
static SENSOR_DEVICE_ATTR_2(temp2_input, S_IRUGO, show_temp23, NULL, 0, 0);
static SENSOR_DEVICE_ATTR_2(temp3_input, S_IRUGO, show_temp23, NULL, 1, 0);
static SENSOR_DEVICE_ATTR_2(temp1_max, S_IRUGO | S_IWUSR,
			show_temp1, store_temp1, 0, 1);
static SENSOR_DEVICE_ATTR_2(temp2_max, S_IRUGO | S_IWUSR, show_temp23,
			store_temp23, 0, 2);
static SENSOR_DEVICE_ATTR_2(temp3_max, S_IRUGO | S_IWUSR, show_temp23,
			store_temp23, 1, 2);
static SENSOR_DEVICE_ATTR_2(temp1_max_hyst, S_IRUGO | S_IWUSR,
			show_temp1, store_temp1, 0, 2);
static SENSOR_DEVICE_ATTR_2(temp2_max_hyst, S_IRUGO | S_IWUSR,
			show_temp23, store_temp23, 0, 4);
static SENSOR_DEVICE_ATTR_2(temp3_max_hyst, S_IRUGO | S_IWUSR,
			show_temp23, store_temp23, 1, 4);
static DEVICE_ATTR(alarms, S_IRUGO, show_alarms_reg, NULL);
static SENSOR_DEVICE_ATTR(in0_alarm, S_IRUGO, show_alarm, NULL, 0);
static SENSOR_DEVICE_ATTR(in1_alarm, S_IRUGO, show_alarm, NULL, 1);
static SENSOR_DEVICE_ATTR(temp1_alarm, S_IRUGO, show_alarm, NULL, 2);
static SENSOR_DEVICE_ATTR(temp2_alarm, S_IRUGO, show_alarm, NULL, 3);
static SENSOR_DEVICE_ATTR(temp3_alarm, S_IRUGO, show_alarm, NULL, 4);
static SENSOR_DEVICE_ATTR(fan1_alarm, S_IRUGO, show_alarm, NULL, 5);
static SENSOR_DEVICE_ATTR(fan2_alarm, S_IRUGO, show_alarm, NULL, 6);
static SENSOR_DEVICE_ATTR(fan3_alarm, S_IRUGO, show_alarm, NULL, 7);
static SENSOR_DEVICE_ATTR(in2_alarm, S_IRUGO, show_alarm, NULL, 8);
static SENSOR_DEVICE_ATTR(in3_alarm, S_IRUGO, show_alarm, NULL, 9);
static SENSOR_DEVICE_ATTR(in4_alarm, S_IRUGO, show_alarm, NULL, 10);
static SENSOR_DEVICE_ATTR(in5_alarm, S_IRUGO, show_alarm, NULL, 11);
static SENSOR_DEVICE_ATTR(in6_alarm, S_IRUGO, show_alarm, NULL, 12);
static SENSOR_DEVICE_ATTR(fan7_alarm, S_IRUGO, show_alarm, NULL, 15);
static SENSOR_DEVICE_ATTR(in7_alarm, S_IRUGO, show_alarm, NULL, 19);
static SENSOR_DEVICE_ATTR(in8_alarm, S_IRUGO, show_alarm, NULL, 20);
static SENSOR_DEVICE_ATTR(fan4_alarm, S_IRUGO, show_alarm, NULL, 21);
static SENSOR_DEVICE_ATTR(fan5_alarm, S_IRUGO, show_alarm, NULL, 22);
static SENSOR_DEVICE_ATTR(fan6_alarm, S_IRUGO, show_alarm, NULL, 23);
static DEVICE_ATTR(chassis, S_IRUGO, show_regs_chassis, NULL);
static DEVICE_ATTR(chassis_clear, S_IRUGO | S_IWUSR,
			show_chassis_clear, store_chassis_clear_legacy);
static DEVICE_ATTR(intrusion0_alarm, S_IRUGO | S_IWUSR,
			show_chassis, store_chassis_clear);
static SENSOR_DEVICE_ATTR(pwm1, S_IWUSR | S_IRUGO, show_pwm, store_pwm, 0);
static SENSOR_DEVICE_ATTR(pwm2, S_IWUSR | S_IRUGO, show_pwm, store_pwm, 1);
static SENSOR_DEVICE_ATTR(pwm3, S_IWUSR | S_IRUGO, show_pwm, store_pwm, 2);
static SENSOR_DEVICE_ATTR(pwm1_enable, S_IWUSR | S_IRUGO,
			show_pwmenable, store_pwmenable, 1);
static SENSOR_DEVICE_ATTR(pwm2_enable, S_IWUSR | S_IRUGO,
			show_pwmenable, store_pwmenable, 2);
static SENSOR_DEVICE_ATTR(pwm3_enable, S_IWUSR | S_IRUGO,
			show_pwmenable, store_pwmenable, 3);
static SENSOR_DEVICE_ATTR(pwm1_mode, S_IWUSR | S_IRUGO,
			show_pwm_mode, store_pwm_mode, 0);
static SENSOR_DEVICE_ATTR(pwm2_mode, S_IWUSR | S_IRUGO,
			show_pwm_mode, store_pwm_mode, 1);
static SENSOR_DEVICE_ATTR(pwm3_mode, S_IWUSR | S_IRUGO,
			show_pwm_mode, store_pwm_mode, 2);
static SENSOR_DEVICE_ATTR(tolerance1, S_IWUSR | S_IRUGO,
			show_tolerance, store_tolerance, 1);
static SENSOR_DEVICE_ATTR(tolerance2, S_IWUSR | S_IRUGO,
			show_tolerance, store_tolerance, 2);
static SENSOR_DEVICE_ATTR(tolerance3, S_IWUSR | S_IRUGO,
			show_tolerance, store_tolerance, 3);
static SENSOR_DEVICE_ATTR(thermal_cruise1, S_IWUSR | S_IRUGO,
			show_thermal_cruise, store_thermal_cruise, 1);
static SENSOR_DEVICE_ATTR(thermal_cruise2, S_IWUSR | S_IRUGO,
			show_thermal_cruise, store_thermal_cruise, 2);
static SENSOR_DEVICE_ATTR(thermal_cruise3, S_IWUSR | S_IRUGO,
			show_thermal_cruise, store_thermal_cruise, 3);
static SENSOR_DEVICE_ATTR_2(sf2_point1_fan1, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 1, 1);
static SENSOR_DEVICE_ATTR_2(sf2_point2_fan1, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 2, 1);
static SENSOR_DEVICE_ATTR_2(sf2_point3_fan1, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 3, 1);
static SENSOR_DEVICE_ATTR_2(sf2_point4_fan1, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 4, 1);
static SENSOR_DEVICE_ATTR_2(sf2_point1_fan2, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 1, 2);
static SENSOR_DEVICE_ATTR_2(sf2_point2_fan2, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 2, 2);
static SENSOR_DEVICE_ATTR_2(sf2_point3_fan2, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 3, 2);
static SENSOR_DEVICE_ATTR_2(sf2_point4_fan2, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 4, 2);
static SENSOR_DEVICE_ATTR_2(sf2_point1_fan3, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 1, 3);
static SENSOR_DEVICE_ATTR_2(sf2_point2_fan3, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 2, 3);
static SENSOR_DEVICE_ATTR_2(sf2_point3_fan3, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 3, 3);
static SENSOR_DEVICE_ATTR_2(sf2_point4_fan3, S_IRUGO | S_IWUSR,
			show_sf2_point, store_sf2_point, 4, 3);
static SENSOR_DEVICE_ATTR_2(sf2_level1_fan1, S_IRUGO | S_IWUSR,
			show_sf2_level, store_sf2_level, 1, 1);
static SENSOR_DEVICE_ATTR_2(sf2_level2_fan1, S_IRUGO | S_IWUSR,
			show_sf2_level, store_sf2_level, 2, 1);
static SENSOR_DEVICE_ATTR_2(sf2_level3_fan1, S_IRUGO | S_IWUSR,
			show_sf2_level, store_sf2_level, 3, 1);
static SENSOR_DEVICE_ATTR_2(sf2_level1_fan2, S_IRUGO | S_IWUSR,
			show_sf2_level, store_sf2_level, 1, 2);
static SENSOR_DEVICE_ATTR_2(sf2_level2_fan2, S_IRUGO | S_IWUSR,
			show_sf2_level, store_sf2_level, 2, 2);
static SENSOR_DEVICE_ATTR_2(sf2_level3_fan2, S_IRUGO | S_IWUSR,
			show_sf2_level, store_sf2_level, 3, 2);
static SENSOR_DEVICE_ATTR_2(sf2_level1_fan3, S_IRUGO | S_IWUSR,
			show_sf2_level, store_sf2_level, 1, 3);
static SENSOR_DEVICE_ATTR_2(sf2_level2_fan3, S_IRUGO | S_IWUSR,
			show_sf2_level, store_sf2_level, 2, 3);
static SENSOR_DEVICE_ATTR_2(sf2_level3_fan3, S_IRUGO | S_IWUSR,
			show_sf2_level, store_sf2_level, 3, 3);
static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, show_fan, NULL, 1);
static SENSOR_DEVICE_ATTR(fan2_input, S_IRUGO, show_fan, NULL, 2);
static SENSOR_DEVICE_ATTR(fan3_input, S_IRUGO, show_fan, NULL, 3);
static SENSOR_DEVICE_ATTR(fan4_input, S_IRUGO, show_fan, NULL, 4);
static SENSOR_DEVICE_ATTR(fan5_input, S_IRUGO, show_fan, NULL, 5);
static SENSOR_DEVICE_ATTR(fan6_input, S_IRUGO, show_fan, NULL, 6);
static SENSOR_DEVICE_ATTR(fan7_input, S_IRUGO, show_fan, NULL, 7);
static SENSOR_DEVICE_ATTR(fan1_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 1);
static SENSOR_DEVICE_ATTR(fan2_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 2);
static SENSOR_DEVICE_ATTR(fan3_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 3);
static SENSOR_DEVICE_ATTR(fan4_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 4);
static SENSOR_DEVICE_ATTR(fan5_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 5);
static SENSOR_DEVICE_ATTR(fan6_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 6);
static SENSOR_DEVICE_ATTR(fan7_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 7);
static SENSOR_DEVICE_ATTR(fan1_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 1);
static SENSOR_DEVICE_ATTR(fan2_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 2);
static SENSOR_DEVICE_ATTR(fan3_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 3);
static SENSOR_DEVICE_ATTR(fan4_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 4);
static SENSOR_DEVICE_ATTR(fan5_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 5);
static SENSOR_DEVICE_ATTR(fan6_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 6);
static SENSOR_DEVICE_ATTR(fan7_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 7);

static struct attribute *w83792d_attributes_fan[4][5] = {
	{
		&sensor_dev_attr_fan4_input.dev_attr.attr,
		&sensor_dev_attr_fan4_min.dev_attr.attr,
		&sensor_dev_attr_fan4_div.dev_attr.attr,
		&sensor_dev_attr_fan4_alarm.dev_attr.attr,
		NULL
	}, {
		&sensor_dev_attr_fan5_input.dev_attr.attr,
		&sensor_dev_attr_fan5_min.dev_attr.attr,
		&sensor_dev_attr_fan5_div.dev_attr.attr,
		&sensor_dev_attr_fan5_alarm.dev_attr.attr,
		NULL
	}, {
		&sensor_dev_attr_fan6_input.dev_attr.attr,
		&sensor_dev_attr_fan6_min.dev_attr.attr,
		&sensor_dev_attr_fan6_div.dev_attr.attr,
		&sensor_dev_attr_fan6_alarm.dev_attr.attr,
		NULL
	}, {
		&sensor_dev_attr_fan7_input.dev_attr.attr,
		&sensor_dev_attr_fan7_min.dev_attr.attr,
		&sensor_dev_attr_fan7_div.dev_attr.attr,
		&sensor_dev_attr_fan7_alarm.dev_attr.attr,
		NULL
	}
};

static const struct attribute_group w83792d_group_fan[4] = {
	{ .attrs = w83792d_attributes_fan[0] },
	{ .attrs = w83792d_attributes_fan[1] },
	{ .attrs = w83792d_attributes_fan[2] },
	{ .attrs = w83792d_attributes_fan[3] },
};

static struct attribute *w83792d_attributes[] = {
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_max.dev_attr.attr,
	&sensor_dev_attr_in0_min.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_max.dev_attr.attr,
	&sensor_dev_attr_in1_min.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in2_max.dev_attr.attr,
	&sensor_dev_attr_in2_min.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in3_max.dev_attr.attr,
	&sensor_dev_attr_in3_min.dev_attr.attr,
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in4_max.dev_attr.attr,
	&sensor_dev_attr_in4_min.dev_attr.attr,
	&sensor_dev_attr_in5_input.dev_attr.attr,
	&sensor_dev_attr_in5_max.dev_attr.attr,
	&sensor_dev_attr_in5_min.dev_attr.attr,
	&sensor_dev_attr_in6_input.dev_attr.attr,
	&sensor_dev_attr_in6_max.dev_attr.attr,
	&sensor_dev_attr_in6_min.dev_attr.attr,
	&sensor_dev_attr_in7_input.dev_attr.attr,
	&sensor_dev_attr_in7_max.dev_attr.attr,
	&sensor_dev_attr_in7_min.dev_attr.attr,
	&sensor_dev_attr_in8_input.dev_attr.attr,
	&sensor_dev_attr_in8_max.dev_attr.attr,
	&sensor_dev_attr_in8_min.dev_attr.attr,
	&sensor_dev_attr_in0_alarm.dev_attr.attr,
	&sensor_dev_attr_in1_alarm.dev_attr.attr,
	&sensor_dev_attr_in2_alarm.dev_attr.attr,
	&sensor_dev_attr_in3_alarm.dev_attr.attr,
	&sensor_dev_attr_in4_alarm.dev_attr.attr,
	&sensor_dev_attr_in5_alarm.dev_attr.attr,
	&sensor_dev_attr_in6_alarm.dev_attr.attr,
	&sensor_dev_attr_in7_alarm.dev_attr.attr,
	&sensor_dev_attr_in8_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp1_max_hyst.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp2_max.dev_attr.attr,
	&sensor_dev_attr_temp2_max_hyst.dev_attr.attr,
	&sensor_dev_attr_temp3_input.dev_attr.attr,
	&sensor_dev_attr_temp3_max.dev_attr.attr,
	&sensor_dev_attr_temp3_max_hyst.dev_attr.attr,
	&sensor_dev_attr_temp1_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_alarm.dev_attr.attr,
	&sensor_dev_attr_temp3_alarm.dev_attr.attr,
	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm1_mode.dev_attr.attr,
	&sensor_dev_attr_pwm1_enable.dev_attr.attr,
	&sensor_dev_attr_pwm2.dev_attr.attr,
	&sensor_dev_attr_pwm2_mode.dev_attr.attr,
	&sensor_dev_attr_pwm2_enable.dev_attr.attr,
	&sensor_dev_attr_pwm3.dev_attr.attr,
	&sensor_dev_attr_pwm3_mode.dev_attr.attr,
	&sensor_dev_attr_pwm3_enable.dev_attr.attr,
	&dev_attr_alarms.attr,
	&dev_attr_chassis.attr,
	&dev_attr_chassis_clear.attr,
	&dev_attr_intrusion0_alarm.attr,
	&sensor_dev_attr_tolerance1.dev_attr.attr,
	&sensor_dev_attr_thermal_cruise1.dev_attr.attr,
	&sensor_dev_attr_tolerance2.dev_attr.attr,
	&sensor_dev_attr_thermal_cruise2.dev_attr.attr,
	&sensor_dev_attr_tolerance3.dev_attr.attr,
	&sensor_dev_attr_thermal_cruise3.dev_attr.attr,
	&sensor_dev_attr_sf2_point1_fan1.dev_attr.attr,
	&sensor_dev_attr_sf2_point2_fan1.dev_attr.attr,
	&sensor_dev_attr_sf2_point3_fan1.dev_attr.attr,
	&sensor_dev_attr_sf2_point4_fan1.dev_attr.attr,
	&sensor_dev_attr_sf2_point1_fan2.dev_attr.attr,
	&sensor_dev_attr_sf2_point2_fan2.dev_attr.attr,
	&sensor_dev_attr_sf2_point3_fan2.dev_attr.attr,
	&sensor_dev_attr_sf2_point4_fan2.dev_attr.attr,
	&sensor_dev_attr_sf2_point1_fan3.dev_attr.attr,
	&sensor_dev_attr_sf2_point2_fan3.dev_attr.attr,
	&sensor_dev_attr_sf2_point3_fan3.dev_attr.attr,
	&sensor_dev_attr_sf2_point4_fan3.dev_attr.attr,
	&sensor_dev_attr_sf2_level1_fan1.dev_attr.attr,
	&sensor_dev_attr_sf2_level2_fan1.dev_attr.attr,
	&sensor_dev_attr_sf2_level3_fan1.dev_attr.attr,
	&sensor_dev_attr_sf2_level1_fan2.dev_attr.attr,
	&sensor_dev_attr_sf2_level2_fan2.dev_attr.attr,
	&sensor_dev_attr_sf2_level3_fan2.dev_attr.attr,
	&sensor_dev_attr_sf2_level1_fan3.dev_attr.attr,
	&sensor_dev_attr_sf2_level2_fan3.dev_attr.attr,
	&sensor_dev_attr_sf2_level3_fan3.dev_attr.attr,
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan1_min.dev_attr.attr,
	&sensor_dev_attr_fan1_div.dev_attr.attr,
	&sensor_dev_attr_fan1_alarm.dev_attr.attr,
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan2_min.dev_attr.attr,
	&sensor_dev_attr_fan2_div.dev_attr.attr,
	&sensor_dev_attr_fan2_alarm.dev_attr.attr,
	&sensor_dev_attr_fan3_input.dev_attr.attr,
	&sensor_dev_attr_fan3_min.dev_attr.attr,
	&sensor_dev_attr_fan3_div.dev_attr.attr,
	&sensor_dev_attr_fan3_alarm.dev_attr.attr,
	NULL
};

static const struct attribute_group w83792d_group = {
	.attrs = w83792d_attributes,
};

static int
w83792d_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	int val1, val2;
	unsigned short address = client->addr;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	if (w83792d_read_value(client, W83792D_REG_CONFIG) & 0x80)
		return -ENODEV;

	val1 = w83792d_read_value(client, W83792D_REG_BANK);
	val2 = w83792d_read_value(client, W83792D_REG_CHIPMAN);
	
	if (!(val1 & 0x07)) {  
		if ((!(val1 & 0x80) && val2 != 0xa3) ||
		    ((val1 & 0x80) && val2 != 0x5c))
			return -ENODEV;
	}
	if (w83792d_read_value(client, W83792D_REG_I2C_ADDR) != address)
		return -ENODEV;

	
	w83792d_write_value(client,
			    W83792D_REG_BANK,
			    (w83792d_read_value(client,
				W83792D_REG_BANK) & 0x78) | 0x80);

	
	val1 = w83792d_read_value(client, W83792D_REG_WCHIPID);
	val2 = w83792d_read_value(client, W83792D_REG_CHIPMAN);
	if (val1 != 0x7a || val2 != 0x5c)
		return -ENODEV;

	strlcpy(info->type, "w83792d", I2C_NAME_SIZE);

	return 0;
}

static int
w83792d_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct w83792d_data *data;
	struct device *dev = &client->dev;
	int i, val1, err;

	data = kzalloc(sizeof(struct w83792d_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto ERROR0;
	}

	i2c_set_clientdata(client, data);
	data->valid = 0;
	mutex_init(&data->update_lock);

	err = w83792d_detect_subclients(client);
	if (err)
		goto ERROR1;

	
	w83792d_init_client(client);

	
	for (i = 0; i < 7; i++) {
		data->fan_min[i] = w83792d_read_value(client,
					W83792D_REG_FAN_MIN[i]);
	}

	
	err = sysfs_create_group(&dev->kobj, &w83792d_group);
	if (err)
		goto ERROR3;

	val1 = w83792d_read_value(client, W83792D_REG_GPIO_EN);

	if (!(val1 & 0x40)) {
		err = sysfs_create_group(&dev->kobj, &w83792d_group_fan[0]);
		if (err)
			goto exit_remove_files;
	}

	if (!(val1 & 0x20)) {
		err = sysfs_create_group(&dev->kobj, &w83792d_group_fan[1]);
		if (err)
			goto exit_remove_files;
	}

	val1 = w83792d_read_value(client, W83792D_REG_PIN);
	if (val1 & 0x40) {
		err = sysfs_create_group(&dev->kobj, &w83792d_group_fan[2]);
		if (err)
			goto exit_remove_files;
	}

	if (val1 & 0x04) {
		err = sysfs_create_group(&dev->kobj, &w83792d_group_fan[3]);
		if (err)
			goto exit_remove_files;
	}

	data->hwmon_dev = hwmon_device_register(dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		goto exit_remove_files;
	}

	return 0;

exit_remove_files:
	sysfs_remove_group(&dev->kobj, &w83792d_group);
	for (i = 0; i < ARRAY_SIZE(w83792d_group_fan); i++)
		sysfs_remove_group(&dev->kobj, &w83792d_group_fan[i]);
ERROR3:
	if (data->lm75[0] != NULL)
		i2c_unregister_device(data->lm75[0]);
	if (data->lm75[1] != NULL)
		i2c_unregister_device(data->lm75[1]);
ERROR1:
	kfree(data);
ERROR0:
	return err;
}

static int
w83792d_remove(struct i2c_client *client)
{
	struct w83792d_data *data = i2c_get_clientdata(client);
	int i;

	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, &w83792d_group);
	for (i = 0; i < ARRAY_SIZE(w83792d_group_fan); i++)
		sysfs_remove_group(&client->dev.kobj,
				   &w83792d_group_fan[i]);

	if (data->lm75[0] != NULL)
		i2c_unregister_device(data->lm75[0]);
	if (data->lm75[1] != NULL)
		i2c_unregister_device(data->lm75[1]);

	kfree(data);
	return 0;
}

static void
w83792d_init_client(struct i2c_client *client)
{
	u8 temp2_cfg, temp3_cfg, vid_in_b;

	if (init)
		w83792d_write_value(client, W83792D_REG_CONFIG, 0x80);

	vid_in_b = w83792d_read_value(client, W83792D_REG_VID_IN_B);
	w83792d_write_value(client, W83792D_REG_VID_IN_B,
			    vid_in_b & 0xbf);

	temp2_cfg = w83792d_read_value(client, W83792D_REG_TEMP2_CONFIG);
	temp3_cfg = w83792d_read_value(client, W83792D_REG_TEMP3_CONFIG);
	w83792d_write_value(client, W83792D_REG_TEMP2_CONFIG,
				temp2_cfg & 0xe6);
	w83792d_write_value(client, W83792D_REG_TEMP3_CONFIG,
				temp3_cfg & 0xe6);

	
	w83792d_write_value(client, W83792D_REG_CONFIG,
			    (w83792d_read_value(client,
						W83792D_REG_CONFIG) & 0xf7)
			    | 0x01);
}

static struct w83792d_data *w83792d_update_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct w83792d_data *data = i2c_get_clientdata(client);
	int i, j;
	u8 reg_array_tmp[4], reg_tmp;

	mutex_lock(&data->update_lock);

	if (time_after
	    (jiffies - data->last_updated, (unsigned long) (HZ * 3))
	    || time_before(jiffies, data->last_updated) || !data->valid) {
		dev_dbg(dev, "Starting device update\n");

		
		for (i = 0; i < 9; i++) {
			data->in[i] = w83792d_read_value(client,
						W83792D_REG_IN[i]);
			data->in_max[i] = w83792d_read_value(client,
						W83792D_REG_IN_MAX[i]);
			data->in_min[i] = w83792d_read_value(client,
						W83792D_REG_IN_MIN[i]);
		}
		data->low_bits = w83792d_read_value(client,
						W83792D_REG_LOW_BITS1) +
				 (w83792d_read_value(client,
						W83792D_REG_LOW_BITS2) << 8);
		for (i = 0; i < 7; i++) {
			
			data->fan[i] = w83792d_read_value(client,
						W83792D_REG_FAN[i]);
			data->fan_min[i] = w83792d_read_value(client,
						W83792D_REG_FAN_MIN[i]);
			
			data->pwm[i] = w83792d_read_value(client,
						W83792D_REG_PWM[i]);
		}

		reg_tmp = w83792d_read_value(client, W83792D_REG_FAN_CFG);
		data->pwmenable[0] = reg_tmp & 0x03;
		data->pwmenable[1] = (reg_tmp>>2) & 0x03;
		data->pwmenable[2] = (reg_tmp>>4) & 0x03;

		for (i = 0; i < 3; i++) {
			data->temp1[i] = w83792d_read_value(client,
							W83792D_REG_TEMP1[i]);
		}
		for (i = 0; i < 2; i++) {
			for (j = 0; j < 6; j++) {
				data->temp_add[i][j] = w83792d_read_value(
					client, W83792D_REG_TEMP_ADD[i][j]);
			}
		}

		
		for (i = 0; i < 4; i++) {
			reg_array_tmp[i] = w83792d_read_value(client,
							W83792D_REG_FAN_DIV[i]);
		}
		data->fan_div[0] = reg_array_tmp[0] & 0x07;
		data->fan_div[1] = (reg_array_tmp[0] >> 4) & 0x07;
		data->fan_div[2] = reg_array_tmp[1] & 0x07;
		data->fan_div[3] = (reg_array_tmp[1] >> 4) & 0x07;
		data->fan_div[4] = reg_array_tmp[2] & 0x07;
		data->fan_div[5] = (reg_array_tmp[2] >> 4) & 0x07;
		data->fan_div[6] = reg_array_tmp[3] & 0x07;

		
		data->alarms = w83792d_read_value(client, W83792D_REG_ALARM1) +
			(w83792d_read_value(client, W83792D_REG_ALARM2) << 8) +
			(w83792d_read_value(client, W83792D_REG_ALARM3) << 16);

		
		data->chassis = (w83792d_read_value(client,
			W83792D_REG_CHASSIS) >> 5) & 0x01;
		data->chassis_clear = (w83792d_read_value(client,
			W83792D_REG_CHASSIS_CLR) >> 7) & 0x01;

		
		for (i = 0; i < 3; i++) {
			data->thermal_cruise[i] =
				w83792d_read_value(client,
				W83792D_REG_THERMAL[i]) & 0x7f;
		}

		
		reg_tmp = w83792d_read_value(client, W83792D_REG_TOLERANCE[0]);
		data->tolerance[0] = reg_tmp & 0x0f;
		data->tolerance[1] = (reg_tmp >> 4) & 0x0f;
		data->tolerance[2] = w83792d_read_value(client,
					W83792D_REG_TOLERANCE[2]) & 0x0f;

		
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 4; j++) {
				data->sf2_points[i][j]
				  = w83792d_read_value(client,
					W83792D_REG_POINTS[i][j]) & 0x7f;
			}
		}

		
		for (i = 0; i < 3; i++) {
			reg_tmp = w83792d_read_value(client,
						W83792D_REG_LEVELS[i][0]);
			data->sf2_levels[i][0] = reg_tmp & 0x0f;
			data->sf2_levels[i][1] = (reg_tmp >> 4) & 0x0f;
			reg_tmp = w83792d_read_value(client,
						W83792D_REG_LEVELS[i][2]);
			data->sf2_levels[i][2] = (reg_tmp >> 4) & 0x0f;
			data->sf2_levels[i][3] = reg_tmp & 0x0f;
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	mutex_unlock(&data->update_lock);

#ifdef DEBUG
	w83792d_print_debug(data, dev);
#endif

	return data;
}

#ifdef DEBUG
static void w83792d_print_debug(struct w83792d_data *data, struct device *dev)
{
	int i = 0, j = 0;
	dev_dbg(dev, "==========The following is the debug message...========\n");
	dev_dbg(dev, "9 set of Voltages: =====>\n");
	for (i = 0; i < 9; i++) {
		dev_dbg(dev, "vin[%d] is: 0x%x\n", i, data->in[i]);
		dev_dbg(dev, "vin[%d] max is: 0x%x\n", i, data->in_max[i]);
		dev_dbg(dev, "vin[%d] min is: 0x%x\n", i, data->in_min[i]);
	}
	dev_dbg(dev, "Low Bit1 is: 0x%x\n", data->low_bits & 0xff);
	dev_dbg(dev, "Low Bit2 is: 0x%x\n", data->low_bits >> 8);
	dev_dbg(dev, "7 set of Fan Counts and Duty Cycles: =====>\n");
	for (i = 0; i < 7; i++) {
		dev_dbg(dev, "fan[%d] is: 0x%x\n", i, data->fan[i]);
		dev_dbg(dev, "fan[%d] min is: 0x%x\n", i, data->fan_min[i]);
		dev_dbg(dev, "pwm[%d]     is: 0x%x\n", i, data->pwm[i]);
	}
	dev_dbg(dev, "3 set of Temperatures: =====>\n");
	for (i = 0; i < 3; i++)
		dev_dbg(dev, "temp1[%d] is: 0x%x\n", i, data->temp1[i]);

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 6; j++) {
			dev_dbg(dev, "temp_add[%d][%d] is: 0x%x\n", i, j,
							data->temp_add[i][j]);
		}
	}

	for (i = 0; i < 7; i++)
		dev_dbg(dev, "fan_div[%d] is: 0x%x\n", i, data->fan_div[i]);

	dev_dbg(dev, "==========End of the debug message...================\n");
	dev_dbg(dev, "\n");
}
#endif

module_i2c_driver(w83792d_driver);

MODULE_AUTHOR("Chunhao Huang @ Winbond <DZShen@Winbond.com.tw>");
MODULE_DESCRIPTION("W83792AD/D driver for linux-2.6");
MODULE_LICENSE("GPL");
