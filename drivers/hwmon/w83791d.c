/*
 * w83791d.c - Part of lm_sensors, Linux kernel modules for hardware
 *	       monitoring
 *
 * Copyright (C) 2006-2007 Charles Spirakis <bezaur@gmail.com>
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


#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-vid.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>

#define NUMBER_OF_VIN		10
#define NUMBER_OF_FANIN		5
#define NUMBER_OF_TEMPIN	3
#define NUMBER_OF_PWM		5

static const unsigned short normal_i2c[] = { 0x2c, 0x2d, 0x2e, 0x2f,
						I2C_CLIENT_END };


static unsigned short force_subclients[4];
module_param_array(force_subclients, short, NULL, 0);
MODULE_PARM_DESC(force_subclients, "List of subclient addresses: "
			"{bus, clientaddr, subclientaddr1, subclientaddr2}");

static bool reset;
module_param(reset, bool, 0);
MODULE_PARM_DESC(reset, "Set to one to force a hardware chip reset");

static bool init;
module_param(init, bool, 0);
MODULE_PARM_DESC(init, "Set to one to force extra software initialization");

static const u8 W83791D_REG_IN[NUMBER_OF_VIN] = {
	0x20,			
	0x21,			
	0x22,			
	0x23,			
	0x24,			
	0x25,			
	0x26,			
	0xB0,			
	0xB1,			
	0xB2			
};

static const u8 W83791D_REG_IN_MAX[NUMBER_OF_VIN] = {
	0x2B,			
	0x2D,			
	0x2F,			
	0x31,			
	0x33,			
	0x35,			
	0x37,			
	0xB4,			
	0xB6,			
	0xB8			
};
static const u8 W83791D_REG_IN_MIN[NUMBER_OF_VIN] = {
	0x2C,			
	0x2E,			
	0x30,			
	0x32,			
	0x34,			
	0x36,			
	0x38,			
	0xB5,			
	0xB7,			
	0xB9			
};
static const u8 W83791D_REG_FAN[NUMBER_OF_FANIN] = {
	0x28,			
	0x29,			
	0x2A,			
	0xBA,			
	0xBB,			
};
static const u8 W83791D_REG_FAN_MIN[NUMBER_OF_FANIN] = {
	0x3B,			
	0x3C,			
	0x3D,			
	0xBC,			
	0xBD,			
};

static const u8 W83791D_REG_PWM[NUMBER_OF_PWM] = {
	0x81,			
	0x83,			
	0x94,			
	0xA0,			
	0xA1,			
};

static const u8 W83791D_REG_TEMP_TARGET[3] = {
	0x85,			
	0x86,			
	0x96,			
};

static const u8 W83791D_REG_TEMP_TOL[2] = {
	0x87,			
	0x97,			
};

static const u8 W83791D_REG_FAN_CFG[2] = {
	0x84,			
	0x95,			
};

static const u8 W83791D_REG_FAN_DIV[3] = {
	0x47,			
	0x4b,			
	0x5C,			
};

#define W83791D_REG_BANK		0x4E
#define W83791D_REG_TEMP2_CONFIG	0xC2
#define W83791D_REG_TEMP3_CONFIG	0xCA

static const u8 W83791D_REG_TEMP1[3] = {
	0x27,			
	0x39,			
	0x3A,			
};

static const u8 W83791D_REG_TEMP_ADD[2][6] = {
	{0xC0,			
	 0xC1,			
	 0xC5,			
	 0xC6,			
	 0xC3,			
	 0xC4},			
	{0xC8,			
	 0xC9,			
	 0xCD,			
	 0xCE,			
	 0xCB,			
	 0xCC}			
};

#define W83791D_REG_BEEP_CONFIG		0x4D

static const u8 W83791D_REG_BEEP_CTRL[3] = {
	0x56,			
	0x57,			
	0xA3,			
};

#define W83791D_REG_GPIO		0x15
#define W83791D_REG_CONFIG		0x40
#define W83791D_REG_VID_FANDIV		0x47
#define W83791D_REG_DID_VID4		0x49
#define W83791D_REG_WCHIPID		0x58
#define W83791D_REG_CHIPMAN		0x4F
#define W83791D_REG_PIN			0x4B
#define W83791D_REG_I2C_SUBADDR		0x4A

#define W83791D_REG_ALARM1 0xA9	
#define W83791D_REG_ALARM2 0xAA	
#define W83791D_REG_ALARM3 0xAB	

#define W83791D_REG_VBAT		0x5D
#define W83791D_REG_I2C_ADDR		0x48

static inline int w83791d_read(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static inline int w83791d_write(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

#define IN_TO_REG(val)		(SENSORS_LIMIT((((val) + 8) / 16), 0, 255))
#define IN_FROM_REG(val)	((val) * 16)

static u8 fan_to_reg(long rpm, int div)
{
	if (rpm == 0)
		return 255;
	rpm = SENSORS_LIMIT(rpm, 1, 1000000);
	return SENSORS_LIMIT((1350000 + rpm * div / 2) / (rpm * div), 1, 254);
}

#define FAN_FROM_REG(val, div)	((val) == 0 ? -1 : \
				((val) == 255 ? 0 : \
					1350000 / ((val) * (div))))

#define TEMP1_FROM_REG(val)	((val) * 1000)
#define TEMP1_TO_REG(val)	((val) <= -128000 ? -128 : \
				 (val) >= 127000 ? 127 : \
				 (val) < 0 ? ((val) - 500) / 1000 : \
				 ((val) + 500) / 1000)

#define TEMP23_FROM_REG(val)	((val) / 128 * 500)
#define TEMP23_TO_REG(val)	((val) <= -128000 ? 0x8000 : \
				 (val) >= 127500 ? 0x7F80 : \
				 (val) < 0 ? ((val) - 250) / 500 * 128 : \
				 ((val) + 250) / 500 * 128)

#define TARGET_TEMP_TO_REG(val)		((val) < 0 ? 0 : \
					(val) >= 127000 ? 127 : \
					((val) + 500) / 1000)

#define TOL_TEMP_TO_REG(val)		((val) < 0 ? 0 : \
					(val) >= 15000 ? 15 : \
					((val) + 500) / 1000)

#define BEEP_MASK_TO_REG(val)		((val) & 0xffffff)
#define BEEP_MASK_FROM_REG(val)		((val) & 0xffffff)

#define DIV_FROM_REG(val)		(1 << (val))

static u8 div_to_reg(int nr, long val)
{
	int i;

	
	val = SENSORS_LIMIT(val, 1, 128) >> 1;
	for (i = 0; i < 7; i++) {
		if (val == 0)
			break;
		val >>= 1;
	}
	return (u8) i;
}

struct w83791d_data {
	struct device *hwmon_dev;
	struct mutex update_lock;

	char valid;			
	unsigned long last_updated;	

	
	struct i2c_client *lm75[2];

	
	u8 in[NUMBER_OF_VIN];		
	u8 in_max[NUMBER_OF_VIN];	
	u8 in_min[NUMBER_OF_VIN];	

	
	u8 fan[NUMBER_OF_FANIN];	
	u8 fan_min[NUMBER_OF_FANIN];	
	u8 fan_div[NUMBER_OF_FANIN];	

	

	s8 temp1[3];		
	s16 temp_add[2][3];	

	
	u8 pwm[5];		
	u8 pwm_enable[3];	

	u8 temp_target[3];	
	u8 temp_tolerance[3];	

	
	u32 alarms;		
	u8 beep_enable;		
	u32 beep_mask;		
	u8 vid;			
	u8 vrm;			
};

static int w83791d_probe(struct i2c_client *client,
			 const struct i2c_device_id *id);
static int w83791d_detect(struct i2c_client *client,
			  struct i2c_board_info *info);
static int w83791d_remove(struct i2c_client *client);

static int w83791d_read(struct i2c_client *client, u8 reg);
static int w83791d_write(struct i2c_client *client, u8 reg, u8 value);
static struct w83791d_data *w83791d_update_device(struct device *dev);

#ifdef DEBUG
static void w83791d_print_debug(struct w83791d_data *data, struct device *dev);
#endif

static void w83791d_init_client(struct i2c_client *client);

static const struct i2c_device_id w83791d_id[] = {
	{ "w83791d", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, w83791d_id);

static struct i2c_driver w83791d_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name = "w83791d",
	},
	.probe		= w83791d_probe,
	.remove		= w83791d_remove,
	.id_table	= w83791d_id,
	.detect		= w83791d_detect,
	.address_list	= normal_i2c,
};

#define show_in_reg(reg) \
static ssize_t show_##reg(struct device *dev, struct device_attribute *attr, \
			char *buf) \
{ \
	struct sensor_device_attribute *sensor_attr = \
						to_sensor_dev_attr(attr); \
	struct w83791d_data *data = w83791d_update_device(dev); \
	int nr = sensor_attr->index; \
	return sprintf(buf, "%d\n", IN_FROM_REG(data->reg[nr])); \
}

show_in_reg(in);
show_in_reg(in_min);
show_in_reg(in_max);

#define store_in_reg(REG, reg) \
static ssize_t store_in_##reg(struct device *dev, \
				struct device_attribute *attr, \
				const char *buf, size_t count) \
{ \
	struct sensor_device_attribute *sensor_attr = \
						to_sensor_dev_attr(attr); \
	struct i2c_client *client = to_i2c_client(dev); \
	struct w83791d_data *data = i2c_get_clientdata(client); \
	int nr = sensor_attr->index; \
	unsigned long val; \
	int err = kstrtoul(buf, 10, &val); \
	if (err) \
		return err; \
	mutex_lock(&data->update_lock); \
	data->in_##reg[nr] = IN_TO_REG(val); \
	w83791d_write(client, W83791D_REG_IN_##REG[nr], data->in_##reg[nr]); \
	mutex_unlock(&data->update_lock); \
	 \
	return count; \
}
store_in_reg(MIN, min);
store_in_reg(MAX, max);

static struct sensor_device_attribute sda_in_input[] = {
	SENSOR_ATTR(in0_input, S_IRUGO, show_in, NULL, 0),
	SENSOR_ATTR(in1_input, S_IRUGO, show_in, NULL, 1),
	SENSOR_ATTR(in2_input, S_IRUGO, show_in, NULL, 2),
	SENSOR_ATTR(in3_input, S_IRUGO, show_in, NULL, 3),
	SENSOR_ATTR(in4_input, S_IRUGO, show_in, NULL, 4),
	SENSOR_ATTR(in5_input, S_IRUGO, show_in, NULL, 5),
	SENSOR_ATTR(in6_input, S_IRUGO, show_in, NULL, 6),
	SENSOR_ATTR(in7_input, S_IRUGO, show_in, NULL, 7),
	SENSOR_ATTR(in8_input, S_IRUGO, show_in, NULL, 8),
	SENSOR_ATTR(in9_input, S_IRUGO, show_in, NULL, 9),
};

static struct sensor_device_attribute sda_in_min[] = {
	SENSOR_ATTR(in0_min, S_IWUSR | S_IRUGO, show_in_min, store_in_min, 0),
	SENSOR_ATTR(in1_min, S_IWUSR | S_IRUGO, show_in_min, store_in_min, 1),
	SENSOR_ATTR(in2_min, S_IWUSR | S_IRUGO, show_in_min, store_in_min, 2),
	SENSOR_ATTR(in3_min, S_IWUSR | S_IRUGO, show_in_min, store_in_min, 3),
	SENSOR_ATTR(in4_min, S_IWUSR | S_IRUGO, show_in_min, store_in_min, 4),
	SENSOR_ATTR(in5_min, S_IWUSR | S_IRUGO, show_in_min, store_in_min, 5),
	SENSOR_ATTR(in6_min, S_IWUSR | S_IRUGO, show_in_min, store_in_min, 6),
	SENSOR_ATTR(in7_min, S_IWUSR | S_IRUGO, show_in_min, store_in_min, 7),
	SENSOR_ATTR(in8_min, S_IWUSR | S_IRUGO, show_in_min, store_in_min, 8),
	SENSOR_ATTR(in9_min, S_IWUSR | S_IRUGO, show_in_min, store_in_min, 9),
};

static struct sensor_device_attribute sda_in_max[] = {
	SENSOR_ATTR(in0_max, S_IWUSR | S_IRUGO, show_in_max, store_in_max, 0),
	SENSOR_ATTR(in1_max, S_IWUSR | S_IRUGO, show_in_max, store_in_max, 1),
	SENSOR_ATTR(in2_max, S_IWUSR | S_IRUGO, show_in_max, store_in_max, 2),
	SENSOR_ATTR(in3_max, S_IWUSR | S_IRUGO, show_in_max, store_in_max, 3),
	SENSOR_ATTR(in4_max, S_IWUSR | S_IRUGO, show_in_max, store_in_max, 4),
	SENSOR_ATTR(in5_max, S_IWUSR | S_IRUGO, show_in_max, store_in_max, 5),
	SENSOR_ATTR(in6_max, S_IWUSR | S_IRUGO, show_in_max, store_in_max, 6),
	SENSOR_ATTR(in7_max, S_IWUSR | S_IRUGO, show_in_max, store_in_max, 7),
	SENSOR_ATTR(in8_max, S_IWUSR | S_IRUGO, show_in_max, store_in_max, 8),
	SENSOR_ATTR(in9_max, S_IWUSR | S_IRUGO, show_in_max, store_in_max, 9),
};


static ssize_t show_beep(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sensor_device_attribute *sensor_attr =
						to_sensor_dev_attr(attr);
	struct w83791d_data *data = w83791d_update_device(dev);
	int bitnr = sensor_attr->index;

	return sprintf(buf, "%d\n", (data->beep_mask >> bitnr) & 1);
}

static ssize_t store_beep(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr =
						to_sensor_dev_attr(attr);
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	int bitnr = sensor_attr->index;
	int bytenr = bitnr / 8;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	val = val ? 1 : 0;

	mutex_lock(&data->update_lock);

	data->beep_mask &= ~(0xff << (bytenr * 8));
	data->beep_mask |= w83791d_read(client, W83791D_REG_BEEP_CTRL[bytenr])
		<< (bytenr * 8);

	data->beep_mask &= ~(1 << bitnr);
	data->beep_mask |= val << bitnr;

	w83791d_write(client, W83791D_REG_BEEP_CTRL[bytenr],
		(data->beep_mask >> (bytenr * 8)) & 0xff);

	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t show_alarm(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sensor_device_attribute *sensor_attr =
						to_sensor_dev_attr(attr);
	struct w83791d_data *data = w83791d_update_device(dev);
	int bitnr = sensor_attr->index;

	return sprintf(buf, "%d\n", (data->alarms >> bitnr) & 1);
}

static struct sensor_device_attribute sda_in_beep[] = {
	SENSOR_ATTR(in0_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 0),
	SENSOR_ATTR(in1_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 13),
	SENSOR_ATTR(in2_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 2),
	SENSOR_ATTR(in3_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 3),
	SENSOR_ATTR(in4_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 8),
	SENSOR_ATTR(in5_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 9),
	SENSOR_ATTR(in6_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 10),
	SENSOR_ATTR(in7_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 16),
	SENSOR_ATTR(in8_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 17),
	SENSOR_ATTR(in9_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 14),
};

static struct sensor_device_attribute sda_in_alarm[] = {
	SENSOR_ATTR(in0_alarm, S_IRUGO, show_alarm, NULL, 0),
	SENSOR_ATTR(in1_alarm, S_IRUGO, show_alarm, NULL, 1),
	SENSOR_ATTR(in2_alarm, S_IRUGO, show_alarm, NULL, 2),
	SENSOR_ATTR(in3_alarm, S_IRUGO, show_alarm, NULL, 3),
	SENSOR_ATTR(in4_alarm, S_IRUGO, show_alarm, NULL, 8),
	SENSOR_ATTR(in5_alarm, S_IRUGO, show_alarm, NULL, 9),
	SENSOR_ATTR(in6_alarm, S_IRUGO, show_alarm, NULL, 10),
	SENSOR_ATTR(in7_alarm, S_IRUGO, show_alarm, NULL, 19),
	SENSOR_ATTR(in8_alarm, S_IRUGO, show_alarm, NULL, 20),
	SENSOR_ATTR(in9_alarm, S_IRUGO, show_alarm, NULL, 14),
};

#define show_fan_reg(reg) \
static ssize_t show_##reg(struct device *dev, struct device_attribute *attr, \
				char *buf) \
{ \
	struct sensor_device_attribute *sensor_attr = \
						to_sensor_dev_attr(attr); \
	struct w83791d_data *data = w83791d_update_device(dev); \
	int nr = sensor_attr->index; \
	return sprintf(buf, "%d\n", \
		FAN_FROM_REG(data->reg[nr], DIV_FROM_REG(data->fan_div[nr]))); \
}

show_fan_reg(fan);
show_fan_reg(fan_min);

static ssize_t store_fan_min(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	int nr = sensor_attr->index;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->fan_min[nr] = fan_to_reg(val, DIV_FROM_REG(data->fan_div[nr]));
	w83791d_write(client, W83791D_REG_FAN_MIN[nr], data->fan_min[nr]);
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t show_fan_div(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct w83791d_data *data = w83791d_update_device(dev);
	return sprintf(buf, "%u\n", DIV_FROM_REG(data->fan_div[nr]));
}

static ssize_t store_fan_div(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	int nr = sensor_attr->index;
	unsigned long min;
	u8 tmp_fan_div;
	u8 fan_div_reg;
	u8 vbat_reg;
	int indx = 0;
	u8 keep_mask = 0;
	u8 new_shift = 0;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	
	min = FAN_FROM_REG(data->fan_min[nr], DIV_FROM_REG(data->fan_div[nr]));

	mutex_lock(&data->update_lock);
	data->fan_div[nr] = div_to_reg(nr, val);

	switch (nr) {
	case 0:
		indx = 0;
		keep_mask = 0xcf;
		new_shift = 4;
		break;
	case 1:
		indx = 0;
		keep_mask = 0x3f;
		new_shift = 6;
		break;
	case 2:
		indx = 1;
		keep_mask = 0x3f;
		new_shift = 6;
		break;
	case 3:
		indx = 2;
		keep_mask = 0xf8;
		new_shift = 0;
		break;
	case 4:
		indx = 2;
		keep_mask = 0x8f;
		new_shift = 4;
		break;
#ifdef DEBUG
	default:
		dev_warn(dev, "store_fan_div: Unexpected nr seen: %d\n", nr);
		count = -EINVAL;
		goto err_exit;
#endif
	}

	fan_div_reg = w83791d_read(client, W83791D_REG_FAN_DIV[indx])
			& keep_mask;
	tmp_fan_div = (data->fan_div[nr] << new_shift) & ~keep_mask;

	w83791d_write(client, W83791D_REG_FAN_DIV[indx],
				fan_div_reg | tmp_fan_div);

	
	if (nr < 3) {
		keep_mask = ~(1 << (nr + 5));
		vbat_reg = w83791d_read(client, W83791D_REG_VBAT)
				& keep_mask;
		tmp_fan_div = (data->fan_div[nr] << (3 + nr)) & ~keep_mask;
		w83791d_write(client, W83791D_REG_VBAT,
				vbat_reg | tmp_fan_div);
	}

	
	data->fan_min[nr] = fan_to_reg(min, DIV_FROM_REG(data->fan_div[nr]));
	w83791d_write(client, W83791D_REG_FAN_MIN[nr], data->fan_min[nr]);

#ifdef DEBUG
err_exit:
#endif
	mutex_unlock(&data->update_lock);

	return count;
}

static struct sensor_device_attribute sda_fan_input[] = {
	SENSOR_ATTR(fan1_input, S_IRUGO, show_fan, NULL, 0),
	SENSOR_ATTR(fan2_input, S_IRUGO, show_fan, NULL, 1),
	SENSOR_ATTR(fan3_input, S_IRUGO, show_fan, NULL, 2),
	SENSOR_ATTR(fan4_input, S_IRUGO, show_fan, NULL, 3),
	SENSOR_ATTR(fan5_input, S_IRUGO, show_fan, NULL, 4),
};

static struct sensor_device_attribute sda_fan_min[] = {
	SENSOR_ATTR(fan1_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 0),
	SENSOR_ATTR(fan2_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 1),
	SENSOR_ATTR(fan3_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 2),
	SENSOR_ATTR(fan4_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 3),
	SENSOR_ATTR(fan5_min, S_IWUSR | S_IRUGO,
			show_fan_min, store_fan_min, 4),
};

static struct sensor_device_attribute sda_fan_div[] = {
	SENSOR_ATTR(fan1_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 0),
	SENSOR_ATTR(fan2_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 1),
	SENSOR_ATTR(fan3_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 2),
	SENSOR_ATTR(fan4_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 3),
	SENSOR_ATTR(fan5_div, S_IWUSR | S_IRUGO,
			show_fan_div, store_fan_div, 4),
};

static struct sensor_device_attribute sda_fan_beep[] = {
	SENSOR_ATTR(fan1_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 6),
	SENSOR_ATTR(fan2_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 7),
	SENSOR_ATTR(fan3_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 11),
	SENSOR_ATTR(fan4_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 21),
	SENSOR_ATTR(fan5_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 22),
};

static struct sensor_device_attribute sda_fan_alarm[] = {
	SENSOR_ATTR(fan1_alarm, S_IRUGO, show_alarm, NULL, 6),
	SENSOR_ATTR(fan2_alarm, S_IRUGO, show_alarm, NULL, 7),
	SENSOR_ATTR(fan3_alarm, S_IRUGO, show_alarm, NULL, 11),
	SENSOR_ATTR(fan4_alarm, S_IRUGO, show_alarm, NULL, 21),
	SENSOR_ATTR(fan5_alarm, S_IRUGO, show_alarm, NULL, 22),
};

static ssize_t show_pwm(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct w83791d_data *data = w83791d_update_device(dev);
	return sprintf(buf, "%u\n", data->pwm[nr]);
}

static ssize_t store_pwm(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	int nr = sensor_attr->index;
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&data->update_lock);
	data->pwm[nr] = SENSORS_LIMIT(val, 0, 255);
	w83791d_write(client, W83791D_REG_PWM[nr], data->pwm[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

static struct sensor_device_attribute sda_pwm[] = {
	SENSOR_ATTR(pwm1, S_IWUSR | S_IRUGO,
			show_pwm, store_pwm, 0),
	SENSOR_ATTR(pwm2, S_IWUSR | S_IRUGO,
			show_pwm, store_pwm, 1),
	SENSOR_ATTR(pwm3, S_IWUSR | S_IRUGO,
			show_pwm, store_pwm, 2),
	SENSOR_ATTR(pwm4, S_IWUSR | S_IRUGO,
			show_pwm, store_pwm, 3),
	SENSOR_ATTR(pwm5, S_IWUSR | S_IRUGO,
			show_pwm, store_pwm, 4),
};

static ssize_t show_pwmenable(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct w83791d_data *data = w83791d_update_device(dev);
	return sprintf(buf, "%u\n", data->pwm_enable[nr] + 1);
}

static ssize_t store_pwmenable(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	int nr = sensor_attr->index;
	unsigned long val;
	u8 reg_cfg_tmp;
	u8 reg_idx = 0;
	u8 val_shift = 0;
	u8 keep_mask = 0;

	int ret = kstrtoul(buf, 10, &val);

	if (ret || val < 1 || val > 3)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	data->pwm_enable[nr] = val - 1;
	switch (nr) {
	case 0:
		reg_idx = 0;
		val_shift = 2;
		keep_mask = 0xf3;
		break;
	case 1:
		reg_idx = 0;
		val_shift = 4;
		keep_mask = 0xcf;
		break;
	case 2:
		reg_idx = 1;
		val_shift = 2;
		keep_mask = 0xf3;
		break;
	}

	reg_cfg_tmp = w83791d_read(client, W83791D_REG_FAN_CFG[reg_idx]);
	reg_cfg_tmp = (reg_cfg_tmp & keep_mask) |
					data->pwm_enable[nr] << val_shift;

	w83791d_write(client, W83791D_REG_FAN_CFG[reg_idx], reg_cfg_tmp);
	mutex_unlock(&data->update_lock);

	return count;
}
static struct sensor_device_attribute sda_pwmenable[] = {
	SENSOR_ATTR(pwm1_enable, S_IWUSR | S_IRUGO,
			show_pwmenable, store_pwmenable, 0),
	SENSOR_ATTR(pwm2_enable, S_IWUSR | S_IRUGO,
			show_pwmenable, store_pwmenable, 1),
	SENSOR_ATTR(pwm3_enable, S_IWUSR | S_IRUGO,
			show_pwmenable, store_pwmenable, 2),
};

static ssize_t show_temp_target(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct w83791d_data *data = w83791d_update_device(dev);
	int nr = sensor_attr->index;
	return sprintf(buf, "%d\n", TEMP1_FROM_REG(data->temp_target[nr]));
}

static ssize_t store_temp_target(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	int nr = sensor_attr->index;
	unsigned long val;
	u8 target_mask;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&data->update_lock);
	data->temp_target[nr] = TARGET_TEMP_TO_REG(val);
	target_mask = w83791d_read(client,
				W83791D_REG_TEMP_TARGET[nr]) & 0x80;
	w83791d_write(client, W83791D_REG_TEMP_TARGET[nr],
				data->temp_target[nr] | target_mask);
	mutex_unlock(&data->update_lock);
	return count;
}

static struct sensor_device_attribute sda_temp_target[] = {
	SENSOR_ATTR(temp1_target, S_IWUSR | S_IRUGO,
			show_temp_target, store_temp_target, 0),
	SENSOR_ATTR(temp2_target, S_IWUSR | S_IRUGO,
			show_temp_target, store_temp_target, 1),
	SENSOR_ATTR(temp3_target, S_IWUSR | S_IRUGO,
			show_temp_target, store_temp_target, 2),
};

static ssize_t show_temp_tolerance(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct w83791d_data *data = w83791d_update_device(dev);
	int nr = sensor_attr->index;
	return sprintf(buf, "%d\n", TEMP1_FROM_REG(data->temp_tolerance[nr]));
}

static ssize_t store_temp_tolerance(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	int nr = sensor_attr->index;
	unsigned long val;
	u8 target_mask;
	u8 reg_idx = 0;
	u8 val_shift = 0;
	u8 keep_mask = 0;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	switch (nr) {
	case 0:
		reg_idx = 0;
		val_shift = 0;
		keep_mask = 0xf0;
		break;
	case 1:
		reg_idx = 0;
		val_shift = 4;
		keep_mask = 0x0f;
		break;
	case 2:
		reg_idx = 1;
		val_shift = 0;
		keep_mask = 0xf0;
		break;
	}

	mutex_lock(&data->update_lock);
	data->temp_tolerance[nr] = TOL_TEMP_TO_REG(val);
	target_mask = w83791d_read(client,
			W83791D_REG_TEMP_TOL[reg_idx]) & keep_mask;
	w83791d_write(client, W83791D_REG_TEMP_TOL[reg_idx],
			(data->temp_tolerance[nr] << val_shift) | target_mask);
	mutex_unlock(&data->update_lock);
	return count;
}

static struct sensor_device_attribute sda_temp_tolerance[] = {
	SENSOR_ATTR(temp1_tolerance, S_IWUSR | S_IRUGO,
			show_temp_tolerance, store_temp_tolerance, 0),
	SENSOR_ATTR(temp2_tolerance, S_IWUSR | S_IRUGO,
			show_temp_tolerance, store_temp_tolerance, 1),
	SENSOR_ATTR(temp3_tolerance, S_IWUSR | S_IRUGO,
			show_temp_tolerance, store_temp_tolerance, 2),
};

static ssize_t show_temp1(struct device *dev, struct device_attribute *devattr,
				char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct w83791d_data *data = w83791d_update_device(dev);
	return sprintf(buf, "%d\n", TEMP1_FROM_REG(data->temp1[attr->index]));
}

static ssize_t store_temp1(struct device *dev, struct device_attribute *devattr,
				const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	int nr = attr->index;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->temp1[nr] = TEMP1_TO_REG(val);
	w83791d_write(client, W83791D_REG_TEMP1[nr], data->temp1[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

static ssize_t show_temp23(struct device *dev, struct device_attribute *devattr,
				char *buf)
{
	struct sensor_device_attribute_2 *attr = to_sensor_dev_attr_2(devattr);
	struct w83791d_data *data = w83791d_update_device(dev);
	int nr = attr->nr;
	int index = attr->index;
	return sprintf(buf, "%d\n", TEMP23_FROM_REG(data->temp_add[nr][index]));
}

static ssize_t store_temp23(struct device *dev,
				struct device_attribute *devattr,
				const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *attr = to_sensor_dev_attr_2(devattr);
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	long val;
	int err;
	int nr = attr->nr;
	int index = attr->index;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->temp_add[nr][index] = TEMP23_TO_REG(val);
	w83791d_write(client, W83791D_REG_TEMP_ADD[nr][index * 2],
				data->temp_add[nr][index] >> 8);
	w83791d_write(client, W83791D_REG_TEMP_ADD[nr][index * 2 + 1],
				data->temp_add[nr][index] & 0x80);
	mutex_unlock(&data->update_lock);

	return count;
}

static struct sensor_device_attribute_2 sda_temp_input[] = {
	SENSOR_ATTR_2(temp1_input, S_IRUGO, show_temp1, NULL, 0, 0),
	SENSOR_ATTR_2(temp2_input, S_IRUGO, show_temp23, NULL, 0, 0),
	SENSOR_ATTR_2(temp3_input, S_IRUGO, show_temp23, NULL, 1, 0),
};

static struct sensor_device_attribute_2 sda_temp_max[] = {
	SENSOR_ATTR_2(temp1_max, S_IRUGO | S_IWUSR,
			show_temp1, store_temp1, 0, 1),
	SENSOR_ATTR_2(temp2_max, S_IRUGO | S_IWUSR,
			show_temp23, store_temp23, 0, 1),
	SENSOR_ATTR_2(temp3_max, S_IRUGO | S_IWUSR,
			show_temp23, store_temp23, 1, 1),
};

static struct sensor_device_attribute_2 sda_temp_max_hyst[] = {
	SENSOR_ATTR_2(temp1_max_hyst, S_IRUGO | S_IWUSR,
			show_temp1, store_temp1, 0, 2),
	SENSOR_ATTR_2(temp2_max_hyst, S_IRUGO | S_IWUSR,
			show_temp23, store_temp23, 0, 2),
	SENSOR_ATTR_2(temp3_max_hyst, S_IRUGO | S_IWUSR,
			show_temp23, store_temp23, 1, 2),
};

static struct sensor_device_attribute sda_temp_beep[] = {
	SENSOR_ATTR(temp1_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 4),
	SENSOR_ATTR(temp2_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 5),
	SENSOR_ATTR(temp3_beep, S_IWUSR | S_IRUGO, show_beep, store_beep, 1),
};

static struct sensor_device_attribute sda_temp_alarm[] = {
	SENSOR_ATTR(temp1_alarm, S_IRUGO, show_alarm, NULL, 4),
	SENSOR_ATTR(temp2_alarm, S_IRUGO, show_alarm, NULL, 5),
	SENSOR_ATTR(temp3_alarm, S_IRUGO, show_alarm, NULL, 13),
};

static ssize_t show_alarms_reg(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct w83791d_data *data = w83791d_update_device(dev);
	return sprintf(buf, "%u\n", data->alarms);
}

static DEVICE_ATTR(alarms, S_IRUGO, show_alarms_reg, NULL);


#define GLOBAL_BEEP_ENABLE_SHIFT	15
#define GLOBAL_BEEP_ENABLE_MASK		(1 << GLOBAL_BEEP_ENABLE_SHIFT)

static ssize_t show_beep_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct w83791d_data *data = w83791d_update_device(dev);
	return sprintf(buf, "%d\n", data->beep_enable);
}

static ssize_t show_beep_mask(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct w83791d_data *data = w83791d_update_device(dev);
	return sprintf(buf, "%d\n", BEEP_MASK_FROM_REG(data->beep_mask));
}


static ssize_t store_beep_mask(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	int i;
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);

	data->beep_mask = BEEP_MASK_TO_REG(val) & ~GLOBAL_BEEP_ENABLE_MASK;
	data->beep_mask |= (data->beep_enable << GLOBAL_BEEP_ENABLE_SHIFT);

	val = data->beep_mask;

	for (i = 0; i < 3; i++) {
		w83791d_write(client, W83791D_REG_BEEP_CTRL[i], (val & 0xff));
		val >>= 8;
	}

	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t store_beep_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);

	data->beep_enable = val ? 1 : 0;

	
	data->beep_mask &= ~GLOBAL_BEEP_ENABLE_MASK;
	data->beep_mask |= (data->beep_enable << GLOBAL_BEEP_ENABLE_SHIFT);

	val = (data->beep_mask >> 8) & 0xff;

	w83791d_write(client, W83791D_REG_BEEP_CTRL[1], val);

	mutex_unlock(&data->update_lock);

	return count;
}

static struct sensor_device_attribute sda_beep_ctrl[] = {
	SENSOR_ATTR(beep_enable, S_IRUGO | S_IWUSR,
			show_beep_enable, store_beep_enable, 0),
	SENSOR_ATTR(beep_mask, S_IRUGO | S_IWUSR,
			show_beep_mask, store_beep_mask, 1)
};

static ssize_t show_vid_reg(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct w83791d_data *data = w83791d_update_device(dev);
	return sprintf(buf, "%d\n", vid_from_reg(data->vid, data->vrm));
}

static DEVICE_ATTR(cpu0_vid, S_IRUGO, show_vid_reg, NULL);

static ssize_t show_vrm_reg(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct w83791d_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", data->vrm);
}

static ssize_t store_vrm_reg(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct w83791d_data *data = dev_get_drvdata(dev);
	unsigned long val;
	int err;


	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	data->vrm = val;
	return count;
}

static DEVICE_ATTR(vrm, S_IRUGO | S_IWUSR, show_vrm_reg, store_vrm_reg);

#define IN_UNIT_ATTRS(X) \
	&sda_in_input[X].dev_attr.attr,	\
	&sda_in_min[X].dev_attr.attr,	\
	&sda_in_max[X].dev_attr.attr,	\
	&sda_in_beep[X].dev_attr.attr,	\
	&sda_in_alarm[X].dev_attr.attr

#define FAN_UNIT_ATTRS(X) \
	&sda_fan_input[X].dev_attr.attr,	\
	&sda_fan_min[X].dev_attr.attr,		\
	&sda_fan_div[X].dev_attr.attr,		\
	&sda_fan_beep[X].dev_attr.attr,		\
	&sda_fan_alarm[X].dev_attr.attr

#define TEMP_UNIT_ATTRS(X) \
	&sda_temp_input[X].dev_attr.attr,	\
	&sda_temp_max[X].dev_attr.attr,		\
	&sda_temp_max_hyst[X].dev_attr.attr,	\
	&sda_temp_beep[X].dev_attr.attr,	\
	&sda_temp_alarm[X].dev_attr.attr

static struct attribute *w83791d_attributes[] = {
	IN_UNIT_ATTRS(0),
	IN_UNIT_ATTRS(1),
	IN_UNIT_ATTRS(2),
	IN_UNIT_ATTRS(3),
	IN_UNIT_ATTRS(4),
	IN_UNIT_ATTRS(5),
	IN_UNIT_ATTRS(6),
	IN_UNIT_ATTRS(7),
	IN_UNIT_ATTRS(8),
	IN_UNIT_ATTRS(9),
	FAN_UNIT_ATTRS(0),
	FAN_UNIT_ATTRS(1),
	FAN_UNIT_ATTRS(2),
	TEMP_UNIT_ATTRS(0),
	TEMP_UNIT_ATTRS(1),
	TEMP_UNIT_ATTRS(2),
	&dev_attr_alarms.attr,
	&sda_beep_ctrl[0].dev_attr.attr,
	&sda_beep_ctrl[1].dev_attr.attr,
	&dev_attr_cpu0_vid.attr,
	&dev_attr_vrm.attr,
	&sda_pwm[0].dev_attr.attr,
	&sda_pwm[1].dev_attr.attr,
	&sda_pwm[2].dev_attr.attr,
	&sda_pwmenable[0].dev_attr.attr,
	&sda_pwmenable[1].dev_attr.attr,
	&sda_pwmenable[2].dev_attr.attr,
	&sda_temp_target[0].dev_attr.attr,
	&sda_temp_target[1].dev_attr.attr,
	&sda_temp_target[2].dev_attr.attr,
	&sda_temp_tolerance[0].dev_attr.attr,
	&sda_temp_tolerance[1].dev_attr.attr,
	&sda_temp_tolerance[2].dev_attr.attr,
	NULL
};

static const struct attribute_group w83791d_group = {
	.attrs = w83791d_attributes,
};

static struct attribute *w83791d_attributes_fanpwm45[] = {
	FAN_UNIT_ATTRS(3),
	FAN_UNIT_ATTRS(4),
	&sda_pwm[3].dev_attr.attr,
	&sda_pwm[4].dev_attr.attr,
	NULL
};

static const struct attribute_group w83791d_group_fanpwm45 = {
	.attrs = w83791d_attributes_fanpwm45,
};

static int w83791d_detect_subclients(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	struct w83791d_data *data = i2c_get_clientdata(client);
	int address = client->addr;
	int i, id, err;
	u8 val;

	id = i2c_adapter_id(adapter);
	if (force_subclients[0] == id && force_subclients[1] == address) {
		for (i = 2; i <= 3; i++) {
			if (force_subclients[i] < 0x48 ||
			    force_subclients[i] > 0x4f) {
				dev_err(&client->dev,
					"invalid subclient "
					"address %d; must be 0x48-0x4f\n",
					force_subclients[i]);
				err = -ENODEV;
				goto error_sc_0;
			}
		}
		w83791d_write(client, W83791D_REG_I2C_SUBADDR,
					(force_subclients[2] & 0x07) |
					((force_subclients[3] & 0x07) << 4));
	}

	val = w83791d_read(client, W83791D_REG_I2C_SUBADDR);
	if (!(val & 0x08))
		data->lm75[0] = i2c_new_dummy(adapter, 0x48 + (val & 0x7));
	if (!(val & 0x80)) {
		if ((data->lm75[0] != NULL) &&
				((val & 0x7) == ((val >> 4) & 0x7))) {
			dev_err(&client->dev,
				"duplicate addresses 0x%x, "
				"use force_subclient\n",
				data->lm75[0]->addr);
			err = -ENODEV;
			goto error_sc_1;
		}
		data->lm75[1] = i2c_new_dummy(adapter,
					      0x48 + ((val >> 4) & 0x7));
	}

	return 0;


error_sc_1:
	if (data->lm75[0] != NULL)
		i2c_unregister_device(data->lm75[0]);
error_sc_0:
	return err;
}


static int w83791d_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	int val1, val2;
	unsigned short address = client->addr;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	if (w83791d_read(client, W83791D_REG_CONFIG) & 0x80)
		return -ENODEV;

	val1 = w83791d_read(client, W83791D_REG_BANK);
	val2 = w83791d_read(client, W83791D_REG_CHIPMAN);
	
	if (!(val1 & 0x07)) {
		if ((!(val1 & 0x80) && val2 != 0xa3) ||
		    ((val1 & 0x80) && val2 != 0x5c)) {
			return -ENODEV;
		}
	}
	if (w83791d_read(client, W83791D_REG_I2C_ADDR) != address)
		return -ENODEV;

	
	val1 = w83791d_read(client, W83791D_REG_BANK) & 0x78;
	w83791d_write(client, W83791D_REG_BANK, val1 | 0x80);

	
	val1 = w83791d_read(client, W83791D_REG_WCHIPID);
	val2 = w83791d_read(client, W83791D_REG_CHIPMAN);
	if (val1 != 0x71 || val2 != 0x5c)
		return -ENODEV;

	strlcpy(info->type, "w83791d", I2C_NAME_SIZE);

	return 0;
}

static int w83791d_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct w83791d_data *data;
	struct device *dev = &client->dev;
	int i, err;
	u8 has_fanpwm45;

#ifdef DEBUG
	int val1;
	val1 = w83791d_read(client, W83791D_REG_DID_VID4);
	dev_dbg(dev, "Device ID version: %d.%d (0x%02x)\n",
			(val1 >> 5) & 0x07, (val1 >> 1) & 0x0f, val1);
#endif

	data = kzalloc(sizeof(struct w83791d_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto error0;
	}

	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);

	err = w83791d_detect_subclients(client);
	if (err)
		goto error1;

	
	w83791d_init_client(client);

	for (i = 0; i < NUMBER_OF_FANIN; i++)
		data->fan_min[i] = w83791d_read(client, W83791D_REG_FAN_MIN[i]);

	
	err = sysfs_create_group(&client->dev.kobj, &w83791d_group);
	if (err)
		goto error3;

	
	has_fanpwm45 = w83791d_read(client, W83791D_REG_GPIO) & 0x10;
	if (has_fanpwm45) {
		err = sysfs_create_group(&client->dev.kobj,
					 &w83791d_group_fanpwm45);
		if (err)
			goto error4;
	}

	
	data->hwmon_dev = hwmon_device_register(dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		goto error5;
	}

	return 0;

error5:
	if (has_fanpwm45)
		sysfs_remove_group(&client->dev.kobj, &w83791d_group_fanpwm45);
error4:
	sysfs_remove_group(&client->dev.kobj, &w83791d_group);
error3:
	if (data->lm75[0] != NULL)
		i2c_unregister_device(data->lm75[0]);
	if (data->lm75[1] != NULL)
		i2c_unregister_device(data->lm75[1]);
error1:
	kfree(data);
error0:
	return err;
}

static int w83791d_remove(struct i2c_client *client)
{
	struct w83791d_data *data = i2c_get_clientdata(client);

	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, &w83791d_group);

	if (data->lm75[0] != NULL)
		i2c_unregister_device(data->lm75[0]);
	if (data->lm75[1] != NULL)
		i2c_unregister_device(data->lm75[1]);

	kfree(data);
	return 0;
}

static void w83791d_init_client(struct i2c_client *client)
{
	struct w83791d_data *data = i2c_get_clientdata(client);
	u8 tmp;
	u8 old_beep;

	if (reset || init) {
		
		old_beep = w83791d_read(client, W83791D_REG_BEEP_CONFIG);

		if (reset) {
			
			w83791d_write(client, W83791D_REG_CONFIG, 0x80);
		}

		
		w83791d_write(client, W83791D_REG_BEEP_CONFIG, old_beep | 0x80);

		
		tmp = w83791d_read(client, W83791D_REG_BEEP_CTRL[1]);
		w83791d_write(client, W83791D_REG_BEEP_CTRL[1], tmp & 0xef);

		if (init) {
			
			tmp = w83791d_read(client, W83791D_REG_TEMP2_CONFIG);
			if (tmp & 1) {
				w83791d_write(client, W83791D_REG_TEMP2_CONFIG,
					tmp & 0xfe);
			}

			tmp = w83791d_read(client, W83791D_REG_TEMP3_CONFIG);
			if (tmp & 1) {
				w83791d_write(client, W83791D_REG_TEMP3_CONFIG,
					tmp & 0xfe);
			}

			
			tmp = w83791d_read(client, W83791D_REG_CONFIG) & 0xf7;
			w83791d_write(client, W83791D_REG_CONFIG, tmp | 0x01);
		}
	}

	data->vrm = vid_which_vrm();
}

static struct w83791d_data *w83791d_update_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct w83791d_data *data = i2c_get_clientdata(client);
	int i, j;
	u8 reg_array_tmp[3];
	u8 vbat_reg;

	mutex_lock(&data->update_lock);

	if (time_after(jiffies, data->last_updated + (HZ * 3))
			|| !data->valid) {
		dev_dbg(dev, "Starting w83791d device update\n");

		
		for (i = 0; i < NUMBER_OF_VIN; i++) {
			data->in[i] = w83791d_read(client,
						W83791D_REG_IN[i]);
			data->in_max[i] = w83791d_read(client,
						W83791D_REG_IN_MAX[i]);
			data->in_min[i] = w83791d_read(client,
						W83791D_REG_IN_MIN[i]);
		}

		
		for (i = 0; i < NUMBER_OF_FANIN; i++) {
			
			data->fan[i] = w83791d_read(client,
						W83791D_REG_FAN[i]);
			data->fan_min[i] = w83791d_read(client,
						W83791D_REG_FAN_MIN[i]);
		}

		
		for (i = 0; i < 3; i++) {
			reg_array_tmp[i] = w83791d_read(client,
						W83791D_REG_FAN_DIV[i]);
		}
		data->fan_div[0] = (reg_array_tmp[0] >> 4) & 0x03;
		data->fan_div[1] = (reg_array_tmp[0] >> 6) & 0x03;
		data->fan_div[2] = (reg_array_tmp[1] >> 6) & 0x03;
		data->fan_div[3] = reg_array_tmp[2] & 0x07;
		data->fan_div[4] = (reg_array_tmp[2] >> 4) & 0x07;

		vbat_reg = w83791d_read(client, W83791D_REG_VBAT);
		for (i = 0; i < 3; i++)
			data->fan_div[i] |= (vbat_reg >> (3 + i)) & 0x04;

		
		for (i = 0; i < NUMBER_OF_PWM; i++) {
			data->pwm[i] =  w83791d_read(client,
						W83791D_REG_PWM[i]);
		}

		
		for (i = 0; i < 2; i++) {
			reg_array_tmp[i] = w83791d_read(client,
						W83791D_REG_FAN_CFG[i]);
		}
		data->pwm_enable[0] = (reg_array_tmp[0] >> 2) & 0x03;
		data->pwm_enable[1] = (reg_array_tmp[0] >> 4) & 0x03;
		data->pwm_enable[2] = (reg_array_tmp[1] >> 2) & 0x03;

		
		for (i = 0; i < 3; i++) {
			data->temp_target[i] = w83791d_read(client,
				W83791D_REG_TEMP_TARGET[i]) & 0x7f;
		}

		
		for (i = 0; i < 2; i++) {
			reg_array_tmp[i] = w83791d_read(client,
					W83791D_REG_TEMP_TOL[i]);
		}
		data->temp_tolerance[0] = reg_array_tmp[0] & 0x0f;
		data->temp_tolerance[1] = (reg_array_tmp[0] >> 4) & 0x0f;
		data->temp_tolerance[2] = reg_array_tmp[1] & 0x0f;

		
		for (i = 0; i < 3; i++) {
			data->temp1[i] = w83791d_read(client,
						W83791D_REG_TEMP1[i]);
		}

		
		for (i = 0; i < 2; i++) {
			for (j = 0; j < 3; j++) {
				data->temp_add[i][j] =
					(w83791d_read(client,
					W83791D_REG_TEMP_ADD[i][j * 2]) << 8) |
					w83791d_read(client,
					W83791D_REG_TEMP_ADD[i][j * 2 + 1]);
			}
		}

		
		data->alarms =
			w83791d_read(client, W83791D_REG_ALARM1) +
			(w83791d_read(client, W83791D_REG_ALARM2) << 8) +
			(w83791d_read(client, W83791D_REG_ALARM3) << 16);

		
		data->beep_mask =
			w83791d_read(client, W83791D_REG_BEEP_CTRL[0]) +
			(w83791d_read(client, W83791D_REG_BEEP_CTRL[1]) << 8) +
			(w83791d_read(client, W83791D_REG_BEEP_CTRL[2]) << 16);

		
		data->beep_enable =
			(data->beep_mask >> GLOBAL_BEEP_ENABLE_SHIFT) & 0x01;

		
		i = w83791d_read(client, W83791D_REG_VID_FANDIV);
		data->vid = i & 0x0f;
		data->vid |= (w83791d_read(client, W83791D_REG_DID_VID4) & 0x01)
				<< 4;

		data->last_updated = jiffies;
		data->valid = 1;
	}

	mutex_unlock(&data->update_lock);

#ifdef DEBUG
	w83791d_print_debug(data, dev);
#endif

	return data;
}

#ifdef DEBUG
static void w83791d_print_debug(struct w83791d_data *data, struct device *dev)
{
	int i = 0, j = 0;

	dev_dbg(dev, "======Start of w83791d debug values======\n");
	dev_dbg(dev, "%d set of Voltages: ===>\n", NUMBER_OF_VIN);
	for (i = 0; i < NUMBER_OF_VIN; i++) {
		dev_dbg(dev, "vin[%d] is:     0x%02x\n", i, data->in[i]);
		dev_dbg(dev, "vin[%d] min is: 0x%02x\n", i, data->in_min[i]);
		dev_dbg(dev, "vin[%d] max is: 0x%02x\n", i, data->in_max[i]);
	}
	dev_dbg(dev, "%d set of Fan Counts/Divisors: ===>\n", NUMBER_OF_FANIN);
	for (i = 0; i < NUMBER_OF_FANIN; i++) {
		dev_dbg(dev, "fan[%d] is:     0x%02x\n", i, data->fan[i]);
		dev_dbg(dev, "fan[%d] min is: 0x%02x\n", i, data->fan_min[i]);
		dev_dbg(dev, "fan_div[%d] is: 0x%02x\n", i, data->fan_div[i]);
	}

	dev_dbg(dev, "%d set of Temperatures: ===>\n", NUMBER_OF_TEMPIN);
	for (i = 0; i < 3; i++)
		dev_dbg(dev, "temp1[%d] is: 0x%02x\n", i, (u8) data->temp1[i]);
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 3; j++) {
			dev_dbg(dev, "temp_add[%d][%d] is: 0x%04x\n", i, j,
				(u16) data->temp_add[i][j]);
		}
	}

	dev_dbg(dev, "Misc Information: ===>\n");
	dev_dbg(dev, "alarm is:     0x%08x\n", data->alarms);
	dev_dbg(dev, "beep_mask is: 0x%08x\n", data->beep_mask);
	dev_dbg(dev, "beep_enable is: %d\n", data->beep_enable);
	dev_dbg(dev, "vid is: 0x%02x\n", data->vid);
	dev_dbg(dev, "vrm is: 0x%02x\n", data->vrm);
	dev_dbg(dev, "=======End of w83791d debug values========\n");
	dev_dbg(dev, "\n");
}
#endif

module_i2c_driver(w83791d_driver);

MODULE_AUTHOR("Charles Spirakis <bezaur@gmail.com>");
MODULE_DESCRIPTION("W83791D driver");
MODULE_LICENSE("GPL");
