/*
 * Hardware monitoring driver for PMBus devices
 *
 * Copyright (c) 2010, 2011 Ericsson AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/delay.h>
#include <linux/i2c/pmbus.h>
#include "pmbus.h"

#define PMBUS_MAX_INPUT_SENSORS		22	
#define PMBUS_VOUT_SENSORS_PER_PAGE	9	
#define PMBUS_IOUT_SENSORS_PER_PAGE	8	
#define PMBUS_POUT_SENSORS_PER_PAGE	7	
#define PMBUS_MAX_SENSORS_PER_FAN	1	
#define PMBUS_MAX_SENSORS_PER_TEMP	9	

#define PMBUS_MAX_INPUT_BOOLEANS	7	
#define PMBUS_VOUT_BOOLEANS_PER_PAGE	4	
#define PMBUS_IOUT_BOOLEANS_PER_PAGE	3	
#define PMBUS_POUT_BOOLEANS_PER_PAGE	3	
#define PMBUS_MAX_BOOLEANS_PER_FAN	2	
#define PMBUS_MAX_BOOLEANS_PER_TEMP	4	

#define PMBUS_MAX_INPUT_LABELS		4	

#define PB_NUM_STATUS_REG	(PMBUS_PAGES * 6 + 1)

#define PB_STATUS_BASE		0
#define PB_STATUS_VOUT_BASE	(PB_STATUS_BASE + PMBUS_PAGES)
#define PB_STATUS_IOUT_BASE	(PB_STATUS_VOUT_BASE + PMBUS_PAGES)
#define PB_STATUS_FAN_BASE	(PB_STATUS_IOUT_BASE + PMBUS_PAGES)
#define PB_STATUS_FAN34_BASE	(PB_STATUS_FAN_BASE + PMBUS_PAGES)
#define PB_STATUS_INPUT_BASE	(PB_STATUS_FAN34_BASE + PMBUS_PAGES)
#define PB_STATUS_TEMP_BASE	(PB_STATUS_INPUT_BASE + 1)

#define PMBUS_NAME_SIZE		24

struct pmbus_sensor {
	char name[PMBUS_NAME_SIZE];	
	struct sensor_device_attribute attribute;
	u8 page;		
	u16 reg;		
	enum pmbus_sensor_classes class;	
	bool update;		
	int data;		
};

struct pmbus_boolean {
	char name[PMBUS_NAME_SIZE];	
	struct sensor_device_attribute attribute;
};

struct pmbus_label {
	char name[PMBUS_NAME_SIZE];	
	struct sensor_device_attribute attribute;
	char label[PMBUS_NAME_SIZE];	
};

struct pmbus_data {
	struct device *hwmon_dev;

	u32 flags;		

	int exponent;		

	const struct pmbus_driver_info *info;

	int max_attributes;
	int num_attributes;
	struct attribute **attributes;
	struct attribute_group group;

	int max_sensors;
	int num_sensors;
	struct pmbus_sensor *sensors;
	int max_booleans;
	int num_booleans;
	struct pmbus_boolean *booleans;
	int max_labels;
	int num_labels;
	struct pmbus_label *labels;

	struct mutex update_lock;
	bool valid;
	unsigned long last_updated;	

	u8 status[PB_NUM_STATUS_REG];

	u8 currpage;
};

int pmbus_set_page(struct i2c_client *client, u8 page)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	int rv = 0;
	int newpage;

	if (page != data->currpage) {
		rv = i2c_smbus_write_byte_data(client, PMBUS_PAGE, page);
		newpage = i2c_smbus_read_byte_data(client, PMBUS_PAGE);
		if (newpage != page)
			rv = -EIO;
		else
			data->currpage = page;
	}
	return rv;
}
EXPORT_SYMBOL_GPL(pmbus_set_page);

int pmbus_write_byte(struct i2c_client *client, int page, u8 value)
{
	int rv;

	if (page >= 0) {
		rv = pmbus_set_page(client, page);
		if (rv < 0)
			return rv;
	}

	return i2c_smbus_write_byte(client, value);
}
EXPORT_SYMBOL_GPL(pmbus_write_byte);

static int _pmbus_write_byte(struct i2c_client *client, int page, u8 value)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	const struct pmbus_driver_info *info = data->info;
	int status;

	if (info->write_byte) {
		status = info->write_byte(client, page, value);
		if (status != -ENODATA)
			return status;
	}
	return pmbus_write_byte(client, page, value);
}

int pmbus_write_word_data(struct i2c_client *client, u8 page, u8 reg, u16 word)
{
	int rv;

	rv = pmbus_set_page(client, page);
	if (rv < 0)
		return rv;

	return i2c_smbus_write_word_data(client, reg, word);
}
EXPORT_SYMBOL_GPL(pmbus_write_word_data);

static int _pmbus_write_word_data(struct i2c_client *client, int page, int reg,
				  u16 word)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	const struct pmbus_driver_info *info = data->info;
	int status;

	if (info->write_word_data) {
		status = info->write_word_data(client, page, reg, word);
		if (status != -ENODATA)
			return status;
	}
	if (reg >= PMBUS_VIRT_BASE)
		return -ENXIO;
	return pmbus_write_word_data(client, page, reg, word);
}

int pmbus_read_word_data(struct i2c_client *client, u8 page, u8 reg)
{
	int rv;

	rv = pmbus_set_page(client, page);
	if (rv < 0)
		return rv;

	return i2c_smbus_read_word_data(client, reg);
}
EXPORT_SYMBOL_GPL(pmbus_read_word_data);

static int _pmbus_read_word_data(struct i2c_client *client, int page, int reg)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	const struct pmbus_driver_info *info = data->info;
	int status;

	if (info->read_word_data) {
		status = info->read_word_data(client, page, reg);
		if (status != -ENODATA)
			return status;
	}
	if (reg >= PMBUS_VIRT_BASE)
		return -ENXIO;
	return pmbus_read_word_data(client, page, reg);
}

int pmbus_read_byte_data(struct i2c_client *client, int page, u8 reg)
{
	int rv;

	if (page >= 0) {
		rv = pmbus_set_page(client, page);
		if (rv < 0)
			return rv;
	}

	return i2c_smbus_read_byte_data(client, reg);
}
EXPORT_SYMBOL_GPL(pmbus_read_byte_data);

static int _pmbus_read_byte_data(struct i2c_client *client, int page, int reg)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	const struct pmbus_driver_info *info = data->info;
	int status;

	if (info->read_byte_data) {
		status = info->read_byte_data(client, page, reg);
		if (status != -ENODATA)
			return status;
	}
	return pmbus_read_byte_data(client, page, reg);
}

static void pmbus_clear_fault_page(struct i2c_client *client, int page)
{
	_pmbus_write_byte(client, page, PMBUS_CLEAR_FAULTS);
}

void pmbus_clear_faults(struct i2c_client *client)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	int i;

	for (i = 0; i < data->info->pages; i++)
		pmbus_clear_fault_page(client, i);
}
EXPORT_SYMBOL_GPL(pmbus_clear_faults);

static int pmbus_check_status_cml(struct i2c_client *client)
{
	int status, status2;

	status = _pmbus_read_byte_data(client, -1, PMBUS_STATUS_BYTE);
	if (status < 0 || (status & PB_STATUS_CML)) {
		status2 = _pmbus_read_byte_data(client, -1, PMBUS_STATUS_CML);
		if (status2 < 0 || (status2 & PB_CML_FAULT_INVALID_COMMAND))
			return -EIO;
	}
	return 0;
}

bool pmbus_check_byte_register(struct i2c_client *client, int page, int reg)
{
	int rv;
	struct pmbus_data *data = i2c_get_clientdata(client);

	rv = _pmbus_read_byte_data(client, page, reg);
	if (rv >= 0 && !(data->flags & PMBUS_SKIP_STATUS_CHECK))
		rv = pmbus_check_status_cml(client);
	pmbus_clear_fault_page(client, -1);
	return rv >= 0;
}
EXPORT_SYMBOL_GPL(pmbus_check_byte_register);

bool pmbus_check_word_register(struct i2c_client *client, int page, int reg)
{
	int rv;
	struct pmbus_data *data = i2c_get_clientdata(client);

	rv = _pmbus_read_word_data(client, page, reg);
	if (rv >= 0 && !(data->flags & PMBUS_SKIP_STATUS_CHECK))
		rv = pmbus_check_status_cml(client);
	pmbus_clear_fault_page(client, -1);
	return rv >= 0;
}
EXPORT_SYMBOL_GPL(pmbus_check_word_register);

const struct pmbus_driver_info *pmbus_get_driver_info(struct i2c_client *client)
{
	struct pmbus_data *data = i2c_get_clientdata(client);

	return data->info;
}
EXPORT_SYMBOL_GPL(pmbus_get_driver_info);

static struct pmbus_data *pmbus_update_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pmbus_data *data = i2c_get_clientdata(client);
	const struct pmbus_driver_info *info = data->info;

	mutex_lock(&data->update_lock);
	if (time_after(jiffies, data->last_updated + HZ) || !data->valid) {
		int i;

		for (i = 0; i < info->pages; i++)
			data->status[PB_STATUS_BASE + i]
			    = _pmbus_read_byte_data(client, i,
						    PMBUS_STATUS_BYTE);
		for (i = 0; i < info->pages; i++) {
			if (!(info->func[i] & PMBUS_HAVE_STATUS_VOUT))
				continue;
			data->status[PB_STATUS_VOUT_BASE + i]
			  = _pmbus_read_byte_data(client, i, PMBUS_STATUS_VOUT);
		}
		for (i = 0; i < info->pages; i++) {
			if (!(info->func[i] & PMBUS_HAVE_STATUS_IOUT))
				continue;
			data->status[PB_STATUS_IOUT_BASE + i]
			  = _pmbus_read_byte_data(client, i, PMBUS_STATUS_IOUT);
		}
		for (i = 0; i < info->pages; i++) {
			if (!(info->func[i] & PMBUS_HAVE_STATUS_TEMP))
				continue;
			data->status[PB_STATUS_TEMP_BASE + i]
			  = _pmbus_read_byte_data(client, i,
						  PMBUS_STATUS_TEMPERATURE);
		}
		for (i = 0; i < info->pages; i++) {
			if (!(info->func[i] & PMBUS_HAVE_STATUS_FAN12))
				continue;
			data->status[PB_STATUS_FAN_BASE + i]
			  = _pmbus_read_byte_data(client, i,
						  PMBUS_STATUS_FAN_12);
		}

		for (i = 0; i < info->pages; i++) {
			if (!(info->func[i] & PMBUS_HAVE_STATUS_FAN34))
				continue;
			data->status[PB_STATUS_FAN34_BASE + i]
			  = _pmbus_read_byte_data(client, i,
						  PMBUS_STATUS_FAN_34);
		}

		if (info->func[0] & PMBUS_HAVE_STATUS_INPUT)
			data->status[PB_STATUS_INPUT_BASE]
			  = _pmbus_read_byte_data(client, 0,
						  PMBUS_STATUS_INPUT);

		for (i = 0; i < data->num_sensors; i++) {
			struct pmbus_sensor *sensor = &data->sensors[i];

			if (!data->valid || sensor->update)
				sensor->data
				    = _pmbus_read_word_data(client,
							    sensor->page,
							    sensor->reg);
		}
		pmbus_clear_faults(client);
		data->last_updated = jiffies;
		data->valid = 1;
	}
	mutex_unlock(&data->update_lock);
	return data;
}

static long pmbus_reg2data_linear(struct pmbus_data *data,
				  struct pmbus_sensor *sensor)
{
	s16 exponent;
	s32 mantissa;
	long val;

	if (sensor->class == PSC_VOLTAGE_OUT) {	
		exponent = data->exponent;
		mantissa = (u16) sensor->data;
	} else {				
		exponent = ((s16)sensor->data) >> 11;
		mantissa = ((s16)((sensor->data & 0x7ff) << 5)) >> 5;
	}

	val = mantissa;

	
	if (sensor->class != PSC_FAN)
		val = val * 1000L;

	
	if (sensor->class == PSC_POWER)
		val = val * 1000L;

	if (exponent >= 0)
		val <<= exponent;
	else
		val >>= -exponent;

	return val;
}

static long pmbus_reg2data_direct(struct pmbus_data *data,
				  struct pmbus_sensor *sensor)
{
	long val = (s16) sensor->data;
	long m, b, R;

	m = data->info->m[sensor->class];
	b = data->info->b[sensor->class];
	R = data->info->R[sensor->class];

	if (m == 0)
		return 0;

	
	R = -R;
	
	if (sensor->class != PSC_FAN) {
		R += 3;
		b *= 1000;
	}

	
	if (sensor->class == PSC_POWER) {
		R += 3;
		b *= 1000;
	}

	while (R > 0) {
		val *= 10;
		R--;
	}
	while (R < 0) {
		val = DIV_ROUND_CLOSEST(val, 10);
		R++;
	}

	return (val - b) / m;
}

static long pmbus_reg2data_vid(struct pmbus_data *data,
			       struct pmbus_sensor *sensor)
{
	long val = sensor->data;

	if (val < 0x02 || val > 0xb2)
		return 0;
	return DIV_ROUND_CLOSEST(160000 - (val - 2) * 625, 100);
}

static long pmbus_reg2data(struct pmbus_data *data, struct pmbus_sensor *sensor)
{
	long val;

	switch (data->info->format[sensor->class]) {
	case direct:
		val = pmbus_reg2data_direct(data, sensor);
		break;
	case vid:
		val = pmbus_reg2data_vid(data, sensor);
		break;
	case linear:
	default:
		val = pmbus_reg2data_linear(data, sensor);
		break;
	}
	return val;
}

#define MAX_MANTISSA	(1023 * 1000)
#define MIN_MANTISSA	(511 * 1000)

static u16 pmbus_data2reg_linear(struct pmbus_data *data,
				 enum pmbus_sensor_classes class, long val)
{
	s16 exponent = 0, mantissa;
	bool negative = false;

	
	if (val == 0)
		return 0;

	if (class == PSC_VOLTAGE_OUT) {
		
		if (val < 0)
			return 0;

		if (data->exponent < 0)
			val <<= -data->exponent;
		else
			val >>= data->exponent;
		val = DIV_ROUND_CLOSEST(val, 1000);
		return val & 0xffff;
	}

	if (val < 0) {
		negative = true;
		val = -val;
	}

	
	if (class == PSC_POWER)
		val = DIV_ROUND_CLOSEST(val, 1000L);

	if (class == PSC_FAN)
		val = val * 1000;

	
	while (val >= MAX_MANTISSA && exponent < 15) {
		exponent++;
		val >>= 1;
	}
	
	while (val < MIN_MANTISSA && exponent > -15) {
		exponent--;
		val <<= 1;
	}

	
	mantissa = DIV_ROUND_CLOSEST(val, 1000);

	
	if (mantissa > 0x3ff)
		mantissa = 0x3ff;

	
	if (negative)
		mantissa = -mantissa;

	
	return (mantissa & 0x7ff) | ((exponent << 11) & 0xf800);
}

static u16 pmbus_data2reg_direct(struct pmbus_data *data,
				 enum pmbus_sensor_classes class, long val)
{
	long m, b, R;

	m = data->info->m[class];
	b = data->info->b[class];
	R = data->info->R[class];

	
	if (class == PSC_POWER) {
		R -= 3;
		b *= 1000;
	}

	
	if (class != PSC_FAN) {
		R -= 3;		
		b *= 1000;
	}
	val = val * m + b;

	while (R > 0) {
		val *= 10;
		R--;
	}
	while (R < 0) {
		val = DIV_ROUND_CLOSEST(val, 10);
		R++;
	}

	return val;
}

static u16 pmbus_data2reg_vid(struct pmbus_data *data,
			      enum pmbus_sensor_classes class, long val)
{
	val = SENSORS_LIMIT(val, 500, 1600);

	return 2 + DIV_ROUND_CLOSEST((1600 - val) * 100, 625);
}

static u16 pmbus_data2reg(struct pmbus_data *data,
			  enum pmbus_sensor_classes class, long val)
{
	u16 regval;

	switch (data->info->format[class]) {
	case direct:
		regval = pmbus_data2reg_direct(data, class, val);
		break;
	case vid:
		regval = pmbus_data2reg_vid(data, class, val);
		break;
	case linear:
	default:
		regval = pmbus_data2reg_linear(data, class, val);
		break;
	}
	return regval;
}

static int pmbus_get_boolean(struct pmbus_data *data, int index)
{
	u8 s1 = (index >> 24) & 0xff;
	u8 s2 = (index >> 16) & 0xff;
	u8 reg = (index >> 8) & 0xff;
	u8 mask = index & 0xff;
	int ret, status;
	u8 regval;

	status = data->status[reg];
	if (status < 0)
		return status;

	regval = status & mask;
	if (!s1 && !s2)
		ret = !!regval;
	else {
		long v1, v2;
		struct pmbus_sensor *sensor1, *sensor2;

		sensor1 = &data->sensors[s1];
		if (sensor1->data < 0)
			return sensor1->data;
		sensor2 = &data->sensors[s2];
		if (sensor2->data < 0)
			return sensor2->data;

		v1 = pmbus_reg2data(data, sensor1);
		v2 = pmbus_reg2data(data, sensor2);
		ret = !!(regval && v1 >= v2);
	}
	return ret;
}

static ssize_t pmbus_show_boolean(struct device *dev,
				  struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct pmbus_data *data = pmbus_update_device(dev);
	int val;

	val = pmbus_get_boolean(data, attr->index);
	if (val < 0)
		return val;
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t pmbus_show_sensor(struct device *dev,
				 struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct pmbus_data *data = pmbus_update_device(dev);
	struct pmbus_sensor *sensor;

	sensor = &data->sensors[attr->index];
	if (sensor->data < 0)
		return sensor->data;

	return snprintf(buf, PAGE_SIZE, "%ld\n", pmbus_reg2data(data, sensor));
}

static ssize_t pmbus_set_sensor(struct device *dev,
				struct device_attribute *devattr,
				const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct i2c_client *client = to_i2c_client(dev);
	struct pmbus_data *data = i2c_get_clientdata(client);
	struct pmbus_sensor *sensor = &data->sensors[attr->index];
	ssize_t rv = count;
	long val = 0;
	int ret;
	u16 regval;

	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	regval = pmbus_data2reg(data, sensor->class, val);
	ret = _pmbus_write_word_data(client, sensor->page, sensor->reg, regval);
	if (ret < 0)
		rv = ret;
	else
		data->sensors[attr->index].data = regval;
	mutex_unlock(&data->update_lock);
	return rv;
}

static ssize_t pmbus_show_label(struct device *dev,
				struct device_attribute *da, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pmbus_data *data = i2c_get_clientdata(client);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			data->labels[attr->index].label);
}

#define PMBUS_ADD_ATTR(data, _name, _idx, _mode, _type, _show, _set)	\
do {									\
	struct sensor_device_attribute *a				\
	    = &data->_type##s[data->num_##_type##s].attribute;		\
	BUG_ON(data->num_attributes >= data->max_attributes);		\
	sysfs_attr_init(&a->dev_attr.attr);				\
	a->dev_attr.attr.name = _name;					\
	a->dev_attr.attr.mode = _mode;					\
	a->dev_attr.show = _show;					\
	a->dev_attr.store = _set;					\
	a->index = _idx;						\
	data->attributes[data->num_attributes] = &a->dev_attr.attr;	\
	data->num_attributes++;						\
} while (0)

#define PMBUS_ADD_GET_ATTR(data, _name, _type, _idx)			\
	PMBUS_ADD_ATTR(data, _name, _idx, S_IRUGO, _type,		\
		       pmbus_show_##_type,  NULL)

#define PMBUS_ADD_SET_ATTR(data, _name, _type, _idx)			\
	PMBUS_ADD_ATTR(data, _name, _idx, S_IWUSR | S_IRUGO, _type,	\
		       pmbus_show_##_type, pmbus_set_##_type)

static void pmbus_add_boolean(struct pmbus_data *data,
			      const char *name, const char *type, int seq,
			      int idx)
{
	struct pmbus_boolean *boolean;

	BUG_ON(data->num_booleans >= data->max_booleans);

	boolean = &data->booleans[data->num_booleans];

	snprintf(boolean->name, sizeof(boolean->name), "%s%d_%s",
		 name, seq, type);
	PMBUS_ADD_GET_ATTR(data, boolean->name, boolean, idx);
	data->num_booleans++;
}

static void pmbus_add_boolean_reg(struct pmbus_data *data,
				  const char *name, const char *type,
				  int seq, int reg, int bit)
{
	pmbus_add_boolean(data, name, type, seq, (reg << 8) | bit);
}

static void pmbus_add_boolean_cmp(struct pmbus_data *data,
				  const char *name, const char *type,
				  int seq, int i1, int i2, int reg, int mask)
{
	pmbus_add_boolean(data, name, type, seq,
			  (i1 << 24) | (i2 << 16) | (reg << 8) | mask);
}

static void pmbus_add_sensor(struct pmbus_data *data,
			     const char *name, const char *type, int seq,
			     int page, int reg, enum pmbus_sensor_classes class,
			     bool update, bool readonly)
{
	struct pmbus_sensor *sensor;

	BUG_ON(data->num_sensors >= data->max_sensors);

	sensor = &data->sensors[data->num_sensors];
	snprintf(sensor->name, sizeof(sensor->name), "%s%d_%s",
		 name, seq, type);
	sensor->page = page;
	sensor->reg = reg;
	sensor->class = class;
	sensor->update = update;
	if (readonly)
		PMBUS_ADD_GET_ATTR(data, sensor->name, sensor,
				   data->num_sensors);
	else
		PMBUS_ADD_SET_ATTR(data, sensor->name, sensor,
				   data->num_sensors);
	data->num_sensors++;
}

static void pmbus_add_label(struct pmbus_data *data,
			    const char *name, int seq,
			    const char *lstring, int index)
{
	struct pmbus_label *label;

	BUG_ON(data->num_labels >= data->max_labels);

	label = &data->labels[data->num_labels];
	snprintf(label->name, sizeof(label->name), "%s%d_label", name, seq);
	if (!index)
		strncpy(label->label, lstring, sizeof(label->label) - 1);
	else
		snprintf(label->label, sizeof(label->label), "%s%d", lstring,
			 index);

	PMBUS_ADD_GET_ATTR(data, label->name, label, data->num_labels);
	data->num_labels++;
}

static void pmbus_find_max_attr(struct i2c_client *client,
				struct pmbus_data *data)
{
	const struct pmbus_driver_info *info = data->info;
	int page, max_sensors, max_booleans, max_labels;

	max_sensors = PMBUS_MAX_INPUT_SENSORS;
	max_booleans = PMBUS_MAX_INPUT_BOOLEANS;
	max_labels = PMBUS_MAX_INPUT_LABELS;

	for (page = 0; page < info->pages; page++) {
		if (info->func[page] & PMBUS_HAVE_VOUT) {
			max_sensors += PMBUS_VOUT_SENSORS_PER_PAGE;
			max_booleans += PMBUS_VOUT_BOOLEANS_PER_PAGE;
			max_labels++;
		}
		if (info->func[page] & PMBUS_HAVE_IOUT) {
			max_sensors += PMBUS_IOUT_SENSORS_PER_PAGE;
			max_booleans += PMBUS_IOUT_BOOLEANS_PER_PAGE;
			max_labels++;
		}
		if (info->func[page] & PMBUS_HAVE_POUT) {
			max_sensors += PMBUS_POUT_SENSORS_PER_PAGE;
			max_booleans += PMBUS_POUT_BOOLEANS_PER_PAGE;
			max_labels++;
		}
		if (info->func[page] & PMBUS_HAVE_FAN12) {
			max_sensors += 2 * PMBUS_MAX_SENSORS_PER_FAN;
			max_booleans += 2 * PMBUS_MAX_BOOLEANS_PER_FAN;
		}
		if (info->func[page] & PMBUS_HAVE_FAN34) {
			max_sensors += 2 * PMBUS_MAX_SENSORS_PER_FAN;
			max_booleans += 2 * PMBUS_MAX_BOOLEANS_PER_FAN;
		}
		if (info->func[page] & PMBUS_HAVE_TEMP) {
			max_sensors += PMBUS_MAX_SENSORS_PER_TEMP;
			max_booleans += PMBUS_MAX_BOOLEANS_PER_TEMP;
		}
		if (info->func[page] & PMBUS_HAVE_TEMP2) {
			max_sensors += PMBUS_MAX_SENSORS_PER_TEMP;
			max_booleans += PMBUS_MAX_BOOLEANS_PER_TEMP;
		}
		if (info->func[page] & PMBUS_HAVE_TEMP3) {
			max_sensors += PMBUS_MAX_SENSORS_PER_TEMP;
			max_booleans += PMBUS_MAX_BOOLEANS_PER_TEMP;
		}
	}
	data->max_sensors = max_sensors;
	data->max_booleans = max_booleans;
	data->max_labels = max_labels;
	data->max_attributes = max_sensors + max_booleans + max_labels;
}


struct pmbus_limit_attr {
	u16 reg;		
	bool update;		
	bool low;		
	const char *attr;	
	const char *alarm;	
	u32 sbit;		
};

struct pmbus_sensor_attr {
	u8 reg;				
	enum pmbus_sensor_classes class;
	const char *label;		
	bool paged;			
	bool update;			
	bool compare;			
	u32 func;			
	u32 sfunc;			
	int sbase;			
	u32 gbit;			
	const struct pmbus_limit_attr *limit;
	int nlimit;			
};

static bool pmbus_add_limit_attrs(struct i2c_client *client,
				  struct pmbus_data *data,
				  const struct pmbus_driver_info *info,
				  const char *name, int index, int page,
				  int cbase,
				  const struct pmbus_sensor_attr *attr)
{
	const struct pmbus_limit_attr *l = attr->limit;
	int nlimit = attr->nlimit;
	bool have_alarm = false;
	int i, cindex;

	for (i = 0; i < nlimit; i++) {
		if (pmbus_check_word_register(client, page, l->reg)) {
			cindex = data->num_sensors;
			pmbus_add_sensor(data, name, l->attr, index, page,
					 l->reg, attr->class,
					 attr->update || l->update,
					 false);
			if (l->sbit && (info->func[page] & attr->sfunc)) {
				if (attr->compare) {
					pmbus_add_boolean_cmp(data, name,
						l->alarm, index,
						l->low ? cindex : cbase,
						l->low ? cbase : cindex,
						attr->sbase + page, l->sbit);
				} else {
					pmbus_add_boolean_reg(data, name,
						l->alarm, index,
						attr->sbase + page, l->sbit);
				}
				have_alarm = true;
			}
		}
		l++;
	}
	return have_alarm;
}

static void pmbus_add_sensor_attrs_one(struct i2c_client *client,
				       struct pmbus_data *data,
				       const struct pmbus_driver_info *info,
				       const char *name,
				       int index, int page,
				       const struct pmbus_sensor_attr *attr)
{
	bool have_alarm;
	int cbase = data->num_sensors;

	if (attr->label)
		pmbus_add_label(data, name, index, attr->label,
				attr->paged ? page + 1 : 0);
	pmbus_add_sensor(data, name, "input", index, page, attr->reg,
			 attr->class, true, true);
	if (attr->sfunc) {
		have_alarm = pmbus_add_limit_attrs(client, data, info, name,
						   index, page, cbase, attr);
		if (!have_alarm && attr->gbit &&
		    pmbus_check_byte_register(client, page, PMBUS_STATUS_BYTE))
			pmbus_add_boolean_reg(data, name, "alarm", index,
					      PB_STATUS_BASE + page,
					      attr->gbit);
	}
}

static void pmbus_add_sensor_attrs(struct i2c_client *client,
				   struct pmbus_data *data,
				   const char *name,
				   const struct pmbus_sensor_attr *attrs,
				   int nattrs)
{
	const struct pmbus_driver_info *info = data->info;
	int index, i;

	index = 1;
	for (i = 0; i < nattrs; i++) {
		int page, pages;

		pages = attrs->paged ? info->pages : 1;
		for (page = 0; page < pages; page++) {
			if (!(info->func[page] & attrs->func))
				continue;
			pmbus_add_sensor_attrs_one(client, data, info, name,
						   index, page, attrs);
			index++;
		}
		attrs++;
	}
}

static const struct pmbus_limit_attr vin_limit_attrs[] = {
	{
		.reg = PMBUS_VIN_UV_WARN_LIMIT,
		.attr = "min",
		.alarm = "min_alarm",
		.sbit = PB_VOLTAGE_UV_WARNING,
	}, {
		.reg = PMBUS_VIN_UV_FAULT_LIMIT,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_VOLTAGE_UV_FAULT,
	}, {
		.reg = PMBUS_VIN_OV_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_VOLTAGE_OV_WARNING,
	}, {
		.reg = PMBUS_VIN_OV_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_VOLTAGE_OV_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_VIN_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_VIN_MIN,
		.update = true,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_VIN_MAX,
		.update = true,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_VIN_HISTORY,
		.attr = "reset_history",
	},
};

static const struct pmbus_limit_attr vout_limit_attrs[] = {
	{
		.reg = PMBUS_VOUT_UV_WARN_LIMIT,
		.attr = "min",
		.alarm = "min_alarm",
		.sbit = PB_VOLTAGE_UV_WARNING,
	}, {
		.reg = PMBUS_VOUT_UV_FAULT_LIMIT,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_VOLTAGE_UV_FAULT,
	}, {
		.reg = PMBUS_VOUT_OV_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_VOLTAGE_OV_WARNING,
	}, {
		.reg = PMBUS_VOUT_OV_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_VOLTAGE_OV_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_VOUT_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_VOUT_MIN,
		.update = true,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_VOUT_MAX,
		.update = true,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_VOUT_HISTORY,
		.attr = "reset_history",
	}
};

static const struct pmbus_sensor_attr voltage_attributes[] = {
	{
		.reg = PMBUS_READ_VIN,
		.class = PSC_VOLTAGE_IN,
		.label = "vin",
		.func = PMBUS_HAVE_VIN,
		.sfunc = PMBUS_HAVE_STATUS_INPUT,
		.sbase = PB_STATUS_INPUT_BASE,
		.gbit = PB_STATUS_VIN_UV,
		.limit = vin_limit_attrs,
		.nlimit = ARRAY_SIZE(vin_limit_attrs),
	}, {
		.reg = PMBUS_READ_VCAP,
		.class = PSC_VOLTAGE_IN,
		.label = "vcap",
		.func = PMBUS_HAVE_VCAP,
	}, {
		.reg = PMBUS_READ_VOUT,
		.class = PSC_VOLTAGE_OUT,
		.label = "vout",
		.paged = true,
		.func = PMBUS_HAVE_VOUT,
		.sfunc = PMBUS_HAVE_STATUS_VOUT,
		.sbase = PB_STATUS_VOUT_BASE,
		.gbit = PB_STATUS_VOUT_OV,
		.limit = vout_limit_attrs,
		.nlimit = ARRAY_SIZE(vout_limit_attrs),
	}
};


static const struct pmbus_limit_attr iin_limit_attrs[] = {
	{
		.reg = PMBUS_IIN_OC_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_IIN_OC_WARNING,
	}, {
		.reg = PMBUS_IIN_OC_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_IIN_OC_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_IIN_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_IIN_MIN,
		.update = true,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_IIN_MAX,
		.update = true,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_IIN_HISTORY,
		.attr = "reset_history",
	}
};

static const struct pmbus_limit_attr iout_limit_attrs[] = {
	{
		.reg = PMBUS_IOUT_OC_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_IOUT_OC_WARNING,
	}, {
		.reg = PMBUS_IOUT_UC_FAULT_LIMIT,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_IOUT_UC_FAULT,
	}, {
		.reg = PMBUS_IOUT_OC_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_IOUT_OC_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_IOUT_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_IOUT_MIN,
		.update = true,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_IOUT_MAX,
		.update = true,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_IOUT_HISTORY,
		.attr = "reset_history",
	}
};

static const struct pmbus_sensor_attr current_attributes[] = {
	{
		.reg = PMBUS_READ_IIN,
		.class = PSC_CURRENT_IN,
		.label = "iin",
		.func = PMBUS_HAVE_IIN,
		.sfunc = PMBUS_HAVE_STATUS_INPUT,
		.sbase = PB_STATUS_INPUT_BASE,
		.limit = iin_limit_attrs,
		.nlimit = ARRAY_SIZE(iin_limit_attrs),
	}, {
		.reg = PMBUS_READ_IOUT,
		.class = PSC_CURRENT_OUT,
		.label = "iout",
		.paged = true,
		.func = PMBUS_HAVE_IOUT,
		.sfunc = PMBUS_HAVE_STATUS_IOUT,
		.sbase = PB_STATUS_IOUT_BASE,
		.gbit = PB_STATUS_IOUT_OC,
		.limit = iout_limit_attrs,
		.nlimit = ARRAY_SIZE(iout_limit_attrs),
	}
};


static const struct pmbus_limit_attr pin_limit_attrs[] = {
	{
		.reg = PMBUS_PIN_OP_WARN_LIMIT,
		.attr = "max",
		.alarm = "alarm",
		.sbit = PB_PIN_OP_WARNING,
	}, {
		.reg = PMBUS_VIRT_READ_PIN_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_PIN_MAX,
		.update = true,
		.attr = "input_highest",
	}, {
		.reg = PMBUS_VIRT_RESET_PIN_HISTORY,
		.attr = "reset_history",
	}
};

static const struct pmbus_limit_attr pout_limit_attrs[] = {
	{
		.reg = PMBUS_POUT_MAX,
		.attr = "cap",
		.alarm = "cap_alarm",
		.sbit = PB_POWER_LIMITING,
	}, {
		.reg = PMBUS_POUT_OP_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_POUT_OP_WARNING,
	}, {
		.reg = PMBUS_POUT_OP_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_POUT_OP_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_POUT_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_POUT_MAX,
		.update = true,
		.attr = "input_highest",
	}, {
		.reg = PMBUS_VIRT_RESET_POUT_HISTORY,
		.attr = "reset_history",
	}
};

static const struct pmbus_sensor_attr power_attributes[] = {
	{
		.reg = PMBUS_READ_PIN,
		.class = PSC_POWER,
		.label = "pin",
		.func = PMBUS_HAVE_PIN,
		.sfunc = PMBUS_HAVE_STATUS_INPUT,
		.sbase = PB_STATUS_INPUT_BASE,
		.limit = pin_limit_attrs,
		.nlimit = ARRAY_SIZE(pin_limit_attrs),
	}, {
		.reg = PMBUS_READ_POUT,
		.class = PSC_POWER,
		.label = "pout",
		.paged = true,
		.func = PMBUS_HAVE_POUT,
		.sfunc = PMBUS_HAVE_STATUS_IOUT,
		.sbase = PB_STATUS_IOUT_BASE,
		.limit = pout_limit_attrs,
		.nlimit = ARRAY_SIZE(pout_limit_attrs),
	}
};


static const struct pmbus_limit_attr temp_limit_attrs[] = {
	{
		.reg = PMBUS_UT_WARN_LIMIT,
		.low = true,
		.attr = "min",
		.alarm = "min_alarm",
		.sbit = PB_TEMP_UT_WARNING,
	}, {
		.reg = PMBUS_UT_FAULT_LIMIT,
		.low = true,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_TEMP_UT_FAULT,
	}, {
		.reg = PMBUS_OT_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_TEMP_OT_WARNING,
	}, {
		.reg = PMBUS_OT_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_TEMP_OT_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_TEMP_MIN,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_TEMP_AVG,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_TEMP_MAX,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_TEMP_HISTORY,
		.attr = "reset_history",
	}
};

static const struct pmbus_limit_attr temp_limit_attrs2[] = {
	{
		.reg = PMBUS_UT_WARN_LIMIT,
		.low = true,
		.attr = "min",
		.alarm = "min_alarm",
		.sbit = PB_TEMP_UT_WARNING,
	}, {
		.reg = PMBUS_UT_FAULT_LIMIT,
		.low = true,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_TEMP_UT_FAULT,
	}, {
		.reg = PMBUS_OT_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_TEMP_OT_WARNING,
	}, {
		.reg = PMBUS_OT_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_TEMP_OT_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_TEMP2_MIN,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_TEMP2_AVG,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_TEMP2_MAX,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_TEMP2_HISTORY,
		.attr = "reset_history",
	}
};

static const struct pmbus_limit_attr temp_limit_attrs3[] = {
	{
		.reg = PMBUS_UT_WARN_LIMIT,
		.low = true,
		.attr = "min",
		.alarm = "min_alarm",
		.sbit = PB_TEMP_UT_WARNING,
	}, {
		.reg = PMBUS_UT_FAULT_LIMIT,
		.low = true,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_TEMP_UT_FAULT,
	}, {
		.reg = PMBUS_OT_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_TEMP_OT_WARNING,
	}, {
		.reg = PMBUS_OT_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_TEMP_OT_FAULT,
	}
};

static const struct pmbus_sensor_attr temp_attributes[] = {
	{
		.reg = PMBUS_READ_TEMPERATURE_1,
		.class = PSC_TEMPERATURE,
		.paged = true,
		.update = true,
		.compare = true,
		.func = PMBUS_HAVE_TEMP,
		.sfunc = PMBUS_HAVE_STATUS_TEMP,
		.sbase = PB_STATUS_TEMP_BASE,
		.gbit = PB_STATUS_TEMPERATURE,
		.limit = temp_limit_attrs,
		.nlimit = ARRAY_SIZE(temp_limit_attrs),
	}, {
		.reg = PMBUS_READ_TEMPERATURE_2,
		.class = PSC_TEMPERATURE,
		.paged = true,
		.update = true,
		.compare = true,
		.func = PMBUS_HAVE_TEMP2,
		.sfunc = PMBUS_HAVE_STATUS_TEMP,
		.sbase = PB_STATUS_TEMP_BASE,
		.gbit = PB_STATUS_TEMPERATURE,
		.limit = temp_limit_attrs2,
		.nlimit = ARRAY_SIZE(temp_limit_attrs2),
	}, {
		.reg = PMBUS_READ_TEMPERATURE_3,
		.class = PSC_TEMPERATURE,
		.paged = true,
		.update = true,
		.compare = true,
		.func = PMBUS_HAVE_TEMP3,
		.sfunc = PMBUS_HAVE_STATUS_TEMP,
		.sbase = PB_STATUS_TEMP_BASE,
		.gbit = PB_STATUS_TEMPERATURE,
		.limit = temp_limit_attrs3,
		.nlimit = ARRAY_SIZE(temp_limit_attrs3),
	}
};

static const int pmbus_fan_registers[] = {
	PMBUS_READ_FAN_SPEED_1,
	PMBUS_READ_FAN_SPEED_2,
	PMBUS_READ_FAN_SPEED_3,
	PMBUS_READ_FAN_SPEED_4
};

static const int pmbus_fan_config_registers[] = {
	PMBUS_FAN_CONFIG_12,
	PMBUS_FAN_CONFIG_12,
	PMBUS_FAN_CONFIG_34,
	PMBUS_FAN_CONFIG_34
};

static const int pmbus_fan_status_registers[] = {
	PMBUS_STATUS_FAN_12,
	PMBUS_STATUS_FAN_12,
	PMBUS_STATUS_FAN_34,
	PMBUS_STATUS_FAN_34
};

static const u32 pmbus_fan_flags[] = {
	PMBUS_HAVE_FAN12,
	PMBUS_HAVE_FAN12,
	PMBUS_HAVE_FAN34,
	PMBUS_HAVE_FAN34
};

static const u32 pmbus_fan_status_flags[] = {
	PMBUS_HAVE_STATUS_FAN12,
	PMBUS_HAVE_STATUS_FAN12,
	PMBUS_HAVE_STATUS_FAN34,
	PMBUS_HAVE_STATUS_FAN34
};

static void pmbus_add_fan_attributes(struct i2c_client *client,
				     struct pmbus_data *data)
{
	const struct pmbus_driver_info *info = data->info;
	int index = 1;
	int page;

	for (page = 0; page < info->pages; page++) {
		int f;

		for (f = 0; f < ARRAY_SIZE(pmbus_fan_registers); f++) {
			int regval;

			if (!(info->func[page] & pmbus_fan_flags[f]))
				break;

			if (!pmbus_check_word_register(client, page,
						       pmbus_fan_registers[f]))
				break;

			regval = _pmbus_read_byte_data(client, page,
				pmbus_fan_config_registers[f]);
			if (regval < 0 ||
			    (!(regval & (PB_FAN_1_INSTALLED >> ((f & 1) * 4)))))
				continue;

			pmbus_add_sensor(data, "fan", "input", index, page,
					 pmbus_fan_registers[f], PSC_FAN, true,
					 true);

			if ((info->func[page] & pmbus_fan_status_flags[f]) &&
			    pmbus_check_byte_register(client,
					page, pmbus_fan_status_registers[f])) {
				int base;

				if (f > 1)	
					base = PB_STATUS_FAN34_BASE + page;
				else
					base = PB_STATUS_FAN_BASE + page;
				pmbus_add_boolean_reg(data, "fan", "alarm",
					index, base,
					PB_FAN_FAN1_WARNING >> (f & 1));
				pmbus_add_boolean_reg(data, "fan", "fault",
					index, base,
					PB_FAN_FAN1_FAULT >> (f & 1));
			}
			index++;
		}
	}
}

static void pmbus_find_attributes(struct i2c_client *client,
				  struct pmbus_data *data)
{
	
	pmbus_add_sensor_attrs(client, data, "in", voltage_attributes,
			       ARRAY_SIZE(voltage_attributes));

	
	pmbus_add_sensor_attrs(client, data, "curr", current_attributes,
			       ARRAY_SIZE(current_attributes));

	
	pmbus_add_sensor_attrs(client, data, "power", power_attributes,
			       ARRAY_SIZE(power_attributes));

	
	pmbus_add_sensor_attrs(client, data, "temp", temp_attributes,
			       ARRAY_SIZE(temp_attributes));

	
	pmbus_add_fan_attributes(client, data);
}

static int pmbus_identify_common(struct i2c_client *client,
				 struct pmbus_data *data)
{
	int vout_mode = -1;

	if (pmbus_check_byte_register(client, 0, PMBUS_VOUT_MODE))
		vout_mode = _pmbus_read_byte_data(client, 0, PMBUS_VOUT_MODE);
	if (vout_mode >= 0 && vout_mode != 0xff) {
		switch (vout_mode >> 5) {
		case 0:	
			if (data->info->format[PSC_VOLTAGE_OUT] != linear)
				return -ENODEV;

			data->exponent = ((s8)(vout_mode << 3)) >> 3;
			break;
		case 1: 
			if (data->info->format[PSC_VOLTAGE_OUT] != vid)
				return -ENODEV;
			break;
		case 2:	
			if (data->info->format[PSC_VOLTAGE_OUT] != direct)
				return -ENODEV;
			break;
		default:
			return -ENODEV;
		}
	}

	
	pmbus_find_max_attr(client, data);
	pmbus_clear_fault_page(client, 0);
	return 0;
}

int pmbus_do_probe(struct i2c_client *client, const struct i2c_device_id *id,
		   struct pmbus_driver_info *info)
{
	const struct pmbus_platform_data *pdata = client->dev.platform_data;
	struct pmbus_data *data;
	int ret;

	if (!info) {
		dev_err(&client->dev, "Missing chip information");
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WRITE_BYTE
				     | I2C_FUNC_SMBUS_BYTE_DATA
				     | I2C_FUNC_SMBUS_WORD_DATA))
		return -ENODEV;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "No memory to allocate driver data\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);

	
	if (i2c_smbus_read_byte_data(client, PMBUS_STATUS_BYTE) < 0) {
		dev_err(&client->dev, "PMBus status register not found\n");
		return -ENODEV;
	}

	if (pdata)
		data->flags = pdata->flags;
	data->info = info;

	pmbus_clear_faults(client);

	if (info->identify) {
		ret = (*info->identify)(client, info);
		if (ret < 0) {
			dev_err(&client->dev, "Chip identification failed\n");
			return ret;
		}
	}

	if (info->pages <= 0 || info->pages > PMBUS_PAGES) {
		dev_err(&client->dev, "Bad number of PMBus pages: %d\n",
			info->pages);
		return -ENODEV;
	}

	ret = pmbus_identify_common(client, data);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to identify chip capabilities\n");
		return ret;
	}

	ret = -ENOMEM;
	data->sensors = devm_kzalloc(&client->dev, sizeof(struct pmbus_sensor)
				     * data->max_sensors, GFP_KERNEL);
	if (!data->sensors) {
		dev_err(&client->dev, "No memory to allocate sensor data\n");
		return -ENOMEM;
	}

	data->booleans = devm_kzalloc(&client->dev, sizeof(struct pmbus_boolean)
				 * data->max_booleans, GFP_KERNEL);
	if (!data->booleans) {
		dev_err(&client->dev, "No memory to allocate boolean data\n");
		return -ENOMEM;
	}

	data->labels = devm_kzalloc(&client->dev, sizeof(struct pmbus_label)
				    * data->max_labels, GFP_KERNEL);
	if (!data->labels) {
		dev_err(&client->dev, "No memory to allocate label data\n");
		return -ENOMEM;
	}

	data->attributes = devm_kzalloc(&client->dev, sizeof(struct attribute *)
					* data->max_attributes, GFP_KERNEL);
	if (!data->attributes) {
		dev_err(&client->dev, "No memory to allocate attribute data\n");
		return -ENOMEM;
	}

	pmbus_find_attributes(client, data);

	if (!data->num_attributes) {
		dev_err(&client->dev, "No attributes found\n");
		return -ENODEV;
	}

	
	data->group.attrs = data->attributes;
	ret = sysfs_create_group(&client->dev.kobj, &data->group);
	if (ret) {
		dev_err(&client->dev, "Failed to create sysfs entries\n");
		return ret;
	}
	data->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(data->hwmon_dev)) {
		ret = PTR_ERR(data->hwmon_dev);
		dev_err(&client->dev, "Failed to register hwmon device\n");
		goto out_hwmon_device_register;
	}
	return 0;

out_hwmon_device_register:
	sysfs_remove_group(&client->dev.kobj, &data->group);
	return ret;
}
EXPORT_SYMBOL_GPL(pmbus_do_probe);

int pmbus_do_remove(struct i2c_client *client)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, &data->group);
	return 0;
}
EXPORT_SYMBOL_GPL(pmbus_do_remove);

MODULE_AUTHOR("Guenter Roeck");
MODULE_DESCRIPTION("PMBus core driver");
MODULE_LICENSE("GPL");
