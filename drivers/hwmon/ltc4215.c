/*
 * Driver for Linear Technology LTC4215 I2C Hot Swap Controller
 *
 * Copyright (C) 2009 Ira W. Snyder <iws@ovro.caltech.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * Datasheet:
 * http://www.linear.com/pc/downloadDocument.do?navId=H0,C1,C1003,C1006,C1163,P17572,D12697
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

enum ltc4215_cmd {
	LTC4215_CONTROL			= 0x00, 
	LTC4215_ALERT			= 0x01, 
	LTC4215_STATUS			= 0x02, 
	LTC4215_FAULT			= 0x03, 
	LTC4215_SENSE			= 0x04, 
	LTC4215_SOURCE			= 0x05, 
	LTC4215_ADIN			= 0x06, 
};

struct ltc4215_data {
	struct device *hwmon_dev;

	struct mutex update_lock;
	bool valid;
	unsigned long last_updated; 

	
	u8 regs[7];
};

static struct ltc4215_data *ltc4215_update_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltc4215_data *data = i2c_get_clientdata(client);
	s32 val;
	int i;

	mutex_lock(&data->update_lock);

	
	if (time_after(jiffies, data->last_updated + HZ / 10) || !data->valid) {

		dev_dbg(&client->dev, "Starting ltc4215 update\n");

		
		for (i = 0; i < ARRAY_SIZE(data->regs); i++) {
			val = i2c_smbus_read_byte_data(client, i);
			if (unlikely(val < 0))
				data->regs[i] = 0;
			else
				data->regs[i] = val;
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	mutex_unlock(&data->update_lock);

	return data;
}

static int ltc4215_get_voltage(struct device *dev, u8 reg)
{
	struct ltc4215_data *data = ltc4215_update_device(dev);
	const u8 regval = data->regs[reg];
	u32 voltage = 0;

	switch (reg) {
	case LTC4215_SENSE:
		
		voltage = regval * 151 / 1000;
		break;
	case LTC4215_SOURCE:
		
		voltage = regval * 605 / 10;
		break;
	case LTC4215_ADIN:
		voltage = regval * 482 * 125 / 1000;
		break;
	default:
		
		WARN_ON_ONCE(1);
		break;
	}

	return voltage;
}

static unsigned int ltc4215_get_current(struct device *dev)
{
	struct ltc4215_data *data = ltc4215_update_device(dev);


	
	const unsigned int voltage = data->regs[LTC4215_SENSE] * 151;

	
	const unsigned int curr = voltage / 4;

	return curr;
}

static ssize_t ltc4215_show_voltage(struct device *dev,
				    struct device_attribute *da,
				    char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	const int voltage = ltc4215_get_voltage(dev, attr->index);

	return snprintf(buf, PAGE_SIZE, "%d\n", voltage);
}

static ssize_t ltc4215_show_current(struct device *dev,
				    struct device_attribute *da,
				    char *buf)
{
	const unsigned int curr = ltc4215_get_current(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", curr);
}

static ssize_t ltc4215_show_power(struct device *dev,
				  struct device_attribute *da,
				  char *buf)
{
	const unsigned int curr = ltc4215_get_current(dev);
	const int output_voltage = ltc4215_get_voltage(dev, LTC4215_ADIN);

	
	const unsigned int power = abs(output_voltage * curr);

	return snprintf(buf, PAGE_SIZE, "%u\n", power);
}

static ssize_t ltc4215_show_alarm(struct device *dev,
					  struct device_attribute *da,
					  char *buf)
{
	struct sensor_device_attribute_2 *attr = to_sensor_dev_attr_2(da);
	struct ltc4215_data *data = ltc4215_update_device(dev);
	const u8 reg = data->regs[attr->index];
	const u32 mask = attr->nr;

	return snprintf(buf, PAGE_SIZE, "%u\n", (reg & mask) ? 1 : 0);
}


#define LTC4215_VOLTAGE(name, ltc4215_cmd_idx) \
	static SENSOR_DEVICE_ATTR(name, S_IRUGO, \
	ltc4215_show_voltage, NULL, ltc4215_cmd_idx)

#define LTC4215_CURRENT(name) \
	static SENSOR_DEVICE_ATTR(name, S_IRUGO, \
	ltc4215_show_current, NULL, 0);

#define LTC4215_POWER(name) \
	static SENSOR_DEVICE_ATTR(name, S_IRUGO, \
	ltc4215_show_power, NULL, 0);

#define LTC4215_ALARM(name, mask, reg) \
	static SENSOR_DEVICE_ATTR_2(name, S_IRUGO, \
	ltc4215_show_alarm, NULL, (mask), reg)


LTC4215_CURRENT(curr1_input);
LTC4215_ALARM(curr1_max_alarm,	(1 << 2),	LTC4215_STATUS);

LTC4215_POWER(power1_input);

LTC4215_VOLTAGE(in1_input,			LTC4215_ADIN);
LTC4215_ALARM(in1_max_alarm,	(1 << 0),	LTC4215_STATUS);
LTC4215_ALARM(in1_min_alarm,	(1 << 1),	LTC4215_STATUS);

LTC4215_VOLTAGE(in2_input,			LTC4215_SOURCE);
LTC4215_ALARM(in2_min_alarm,	(1 << 3),	LTC4215_STATUS);

static struct attribute *ltc4215_attributes[] = {
	&sensor_dev_attr_curr1_input.dev_attr.attr,
	&sensor_dev_attr_curr1_max_alarm.dev_attr.attr,

	&sensor_dev_attr_power1_input.dev_attr.attr,

	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_max_alarm.dev_attr.attr,
	&sensor_dev_attr_in1_min_alarm.dev_attr.attr,

	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in2_min_alarm.dev_attr.attr,

	NULL,
};

static const struct attribute_group ltc4215_group = {
	.attrs = ltc4215_attributes,
};

static int ltc4215_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	struct ltc4215_data *data;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto out_kzalloc;
	}

	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);

	
	i2c_smbus_write_byte_data(client, LTC4215_FAULT, 0x00);

	
	ret = sysfs_create_group(&client->dev.kobj, &ltc4215_group);
	if (ret)
		goto out_sysfs_create_group;

	data->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(data->hwmon_dev)) {
		ret = PTR_ERR(data->hwmon_dev);
		goto out_hwmon_device_register;
	}

	return 0;

out_hwmon_device_register:
	sysfs_remove_group(&client->dev.kobj, &ltc4215_group);
out_sysfs_create_group:
	kfree(data);
out_kzalloc:
	return ret;
}

static int ltc4215_remove(struct i2c_client *client)
{
	struct ltc4215_data *data = i2c_get_clientdata(client);

	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, &ltc4215_group);

	kfree(data);

	return 0;
}

static const struct i2c_device_id ltc4215_id[] = {
	{ "ltc4215", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ltc4215_id);

static struct i2c_driver ltc4215_driver = {
	.driver = {
		.name	= "ltc4215",
	},
	.probe		= ltc4215_probe,
	.remove		= ltc4215_remove,
	.id_table	= ltc4215_id,
};

module_i2c_driver(ltc4215_driver);

MODULE_AUTHOR("Ira W. Snyder <iws@ovro.caltech.edu>");
MODULE_DESCRIPTION("LTC4215 driver");
MODULE_LICENSE("GPL");
