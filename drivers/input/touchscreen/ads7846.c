/*
 * ADS7846 based touchscreen and sensor driver
 *
 * Copyright (c) 2005 David Brownell
 * Copyright (c) 2006 Nokia Corporation
 * Various changes: Imre Deak <imre.deak@nokia.com>
 *
 * Using code from:
 *  - corgi_ts.c
 *	Copyright (C) 2004-2005 Richard Purdie
 *  - omap_ts.[hc], ads7846.h, ts_osk.c
 *	Copyright (C) 2002 MontaVista Software
 *	Copyright (C) 2004 Texas Instruments
 *	Copyright (C) 2005 Dirk Behme
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/types.h>
#include <linux/hwmon.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/ads7846.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <asm/irq.h>


#define TS_POLL_DELAY	1	
#define TS_POLL_PERIOD	5	

#define	SAMPLE_BITS	(8  + 16  + 2 )

struct ts_event {
	u16	x;
	u16	y;
	u16	z1, z2;
	bool	ignore;
	u8	x_buf[3];
	u8	y_buf[3];
};

struct ads7846_packet {
	u8			read_x, read_y, read_z1, read_z2, pwrdown;
	u16			dummy;		
	struct ts_event		tc;
	
	u8			read_x_cmd[3], read_y_cmd[3], pwrdown_cmd[3];
};

struct ads7846 {
	struct input_dev	*input;
	char			phys[32];
	char			name[32];

	struct spi_device	*spi;
	struct regulator	*reg;

#if defined(CONFIG_HWMON) || defined(CONFIG_HWMON_MODULE)
	struct attribute_group	*attr_group;
	struct device		*hwmon;
#endif

	u16			model;
	u16			vref_mv;
	u16			vref_delay_usecs;
	u16			x_plate_ohms;
	u16			pressure_max;

	bool			swap_xy;
	bool			use_internal;

	struct ads7846_packet	*packet;

	struct spi_transfer	xfer[18];
	struct spi_message	msg[5];
	int			msg_count;
	wait_queue_head_t	wait;

	bool			pendown;

	int			read_cnt;
	int			read_rep;
	int			last_read;

	u16			debounce_max;
	u16			debounce_tol;
	u16			debounce_rep;

	u16			penirq_recheck_delay_usecs;

	struct mutex		lock;
	bool			stopped;	
	bool			disabled;	
	bool			suspended;	

	int			(*filter)(void *data, int data_idx, int *val);
	void			*filter_data;
	void			(*filter_cleanup)(void *data);
	int			(*get_pendown_state)(void);
	int			gpio_pendown;

	void			(*wait_for_sync)(void);
};

#if	0
#define	CS_CHANGE(xfer)	((xfer).cs_change = 1)
#else
#define	CS_CHANGE(xfer)	((xfer).cs_change = 0)
#endif


#define	ADS_START		(1 << 7)
#define	ADS_A2A1A0_d_y		(1 << 4)	
#define	ADS_A2A1A0_d_z1		(3 << 4)	
#define	ADS_A2A1A0_d_z2		(4 << 4)	
#define	ADS_A2A1A0_d_x		(5 << 4)	
#define	ADS_A2A1A0_temp0	(0 << 4)	
#define	ADS_A2A1A0_vbatt	(2 << 4)	
#define	ADS_A2A1A0_vaux		(6 << 4)	
#define	ADS_A2A1A0_temp1	(7 << 4)	
#define	ADS_8_BIT		(1 << 3)
#define	ADS_12_BIT		(0 << 3)
#define	ADS_SER			(1 << 2)	
#define	ADS_DFR			(0 << 2)	
#define	ADS_PD10_PDOWN		(0 << 0)	
#define	ADS_PD10_ADC_ON		(1 << 0)	
#define	ADS_PD10_REF_ON		(2 << 0)	
#define	ADS_PD10_ALL_ON		(3 << 0)	

#define	MAX_12BIT	((1<<12)-1)

#define	READ_12BIT_DFR(x, adc, vref) (ADS_START | ADS_A2A1A0_d_ ## x \
	| ADS_12_BIT | ADS_DFR | \
	(adc ? ADS_PD10_ADC_ON : 0) | (vref ? ADS_PD10_REF_ON : 0))

#define	READ_Y(vref)	(READ_12BIT_DFR(y,  1, vref))
#define	READ_Z1(vref)	(READ_12BIT_DFR(z1, 1, vref))
#define	READ_Z2(vref)	(READ_12BIT_DFR(z2, 1, vref))

#define	READ_X(vref)	(READ_12BIT_DFR(x,  1, vref))
#define	PWRDOWN		(READ_12BIT_DFR(y,  0, 0))	

#define	READ_12BIT_SER(x) (ADS_START | ADS_A2A1A0_ ## x \
	| ADS_12_BIT | ADS_SER)

#define	REF_ON	(READ_12BIT_DFR(x, 1, 1))
#define	REF_OFF	(READ_12BIT_DFR(y, 0, 0))

static void ads7846_stop(struct ads7846 *ts)
{
	if (!ts->disabled && !ts->suspended) {
		
		ts->stopped = true;
		mb();
		wake_up(&ts->wait);
		disable_irq(ts->spi->irq);
	}
}

static void ads7846_restart(struct ads7846 *ts)
{
	if (!ts->disabled && !ts->suspended) {
		
		ts->stopped = false;
		mb();
		enable_irq(ts->spi->irq);
	}
}

static void __ads7846_disable(struct ads7846 *ts)
{
	ads7846_stop(ts);
	regulator_disable(ts->reg);

}

static void __ads7846_enable(struct ads7846 *ts)
{
	regulator_enable(ts->reg);
	ads7846_restart(ts);
}

static void ads7846_disable(struct ads7846 *ts)
{
	mutex_lock(&ts->lock);

	if (!ts->disabled) {

		if  (!ts->suspended)
			__ads7846_disable(ts);

		ts->disabled = true;
	}

	mutex_unlock(&ts->lock);
}

static void ads7846_enable(struct ads7846 *ts)
{
	mutex_lock(&ts->lock);

	if (ts->disabled) {

		ts->disabled = false;

		if (!ts->suspended)
			__ads7846_enable(ts);
	}

	mutex_unlock(&ts->lock);
}



struct ser_req {
	u8			ref_on;
	u8			command;
	u8			ref_off;
	u16			scratch;
	struct spi_message	msg;
	struct spi_transfer	xfer[6];
	__be16 sample ____cacheline_aligned;
};

struct ads7845_ser_req {
	u8			command[3];
	struct spi_message	msg;
	struct spi_transfer	xfer[2];
	u8 sample[3] ____cacheline_aligned;
};

static int ads7846_read12_ser(struct device *dev, unsigned command)
{
	struct spi_device *spi = to_spi_device(dev);
	struct ads7846 *ts = dev_get_drvdata(dev);
	struct ser_req *req;
	int status;

	req = kzalloc(sizeof *req, GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	spi_message_init(&req->msg);

	
	if (ts->use_internal) {
		req->ref_on = REF_ON;
		req->xfer[0].tx_buf = &req->ref_on;
		req->xfer[0].len = 1;
		spi_message_add_tail(&req->xfer[0], &req->msg);

		req->xfer[1].rx_buf = &req->scratch;
		req->xfer[1].len = 2;

		
		req->xfer[1].delay_usecs = ts->vref_delay_usecs;
		spi_message_add_tail(&req->xfer[1], &req->msg);

		
		command |= ADS_PD10_REF_ON;
	}

	
	command |= ADS_PD10_ADC_ON;

	
	req->command = (u8) command;
	req->xfer[2].tx_buf = &req->command;
	req->xfer[2].len = 1;
	spi_message_add_tail(&req->xfer[2], &req->msg);

	req->xfer[3].rx_buf = &req->sample;
	req->xfer[3].len = 2;
	spi_message_add_tail(&req->xfer[3], &req->msg);

	

	
	req->ref_off = PWRDOWN;
	req->xfer[4].tx_buf = &req->ref_off;
	req->xfer[4].len = 1;
	spi_message_add_tail(&req->xfer[4], &req->msg);

	req->xfer[5].rx_buf = &req->scratch;
	req->xfer[5].len = 2;
	CS_CHANGE(req->xfer[5]);
	spi_message_add_tail(&req->xfer[5], &req->msg);

	mutex_lock(&ts->lock);
	ads7846_stop(ts);
	status = spi_sync(spi, &req->msg);
	ads7846_restart(ts);
	mutex_unlock(&ts->lock);

	if (status == 0) {
		
		status = be16_to_cpu(req->sample);
		status = status >> 3;
		status &= 0x0fff;
	}

	kfree(req);
	return status;
}

static int ads7845_read12_ser(struct device *dev, unsigned command)
{
	struct spi_device *spi = to_spi_device(dev);
	struct ads7846 *ts = dev_get_drvdata(dev);
	struct ads7845_ser_req *req;
	int status;

	req = kzalloc(sizeof *req, GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	spi_message_init(&req->msg);

	req->command[0] = (u8) command;
	req->xfer[0].tx_buf = req->command;
	req->xfer[0].rx_buf = req->sample;
	req->xfer[0].len = 3;
	spi_message_add_tail(&req->xfer[0], &req->msg);

	mutex_lock(&ts->lock);
	ads7846_stop(ts);
	status = spi_sync(spi, &req->msg);
	ads7846_restart(ts);
	mutex_unlock(&ts->lock);

	if (status == 0) {
		
		status = be16_to_cpu(*((u16 *)&req->sample[1]));
		status = status >> 3;
		status &= 0x0fff;
	}

	kfree(req);
	return status;
}

#if defined(CONFIG_HWMON) || defined(CONFIG_HWMON_MODULE)

#define SHOW(name, var, adjust) static ssize_t \
name ## _show(struct device *dev, struct device_attribute *attr, char *buf) \
{ \
	struct ads7846 *ts = dev_get_drvdata(dev); \
	ssize_t v = ads7846_read12_ser(dev, \
			READ_12BIT_SER(var)); \
	if (v < 0) \
		return v; \
	return sprintf(buf, "%u\n", adjust(ts, v)); \
} \
static DEVICE_ATTR(name, S_IRUGO, name ## _show, NULL);


static inline unsigned null_adjust(struct ads7846 *ts, ssize_t v)
{
	return v;
}

SHOW(temp0, temp0, null_adjust)		
SHOW(temp1, temp1, null_adjust)		


static inline unsigned vaux_adjust(struct ads7846 *ts, ssize_t v)
{
	unsigned retval = v;

	
	retval *= ts->vref_mv;
	retval = retval >> 12;

	return retval;
}

static inline unsigned vbatt_adjust(struct ads7846 *ts, ssize_t v)
{
	unsigned retval = vaux_adjust(ts, v);

	
	if (ts->model == 7846)
		retval *= 4;

	return retval;
}

SHOW(in0_input, vaux, vaux_adjust)
SHOW(in1_input, vbatt, vbatt_adjust)

static struct attribute *ads7846_attributes[] = {
	&dev_attr_temp0.attr,
	&dev_attr_temp1.attr,
	&dev_attr_in0_input.attr,
	&dev_attr_in1_input.attr,
	NULL,
};

static struct attribute_group ads7846_attr_group = {
	.attrs = ads7846_attributes,
};

static struct attribute *ads7843_attributes[] = {
	&dev_attr_in0_input.attr,
	&dev_attr_in1_input.attr,
	NULL,
};

static struct attribute_group ads7843_attr_group = {
	.attrs = ads7843_attributes,
};

static struct attribute *ads7845_attributes[] = {
	&dev_attr_in0_input.attr,
	NULL,
};

static struct attribute_group ads7845_attr_group = {
	.attrs = ads7845_attributes,
};

static int ads784x_hwmon_register(struct spi_device *spi, struct ads7846 *ts)
{
	struct device *hwmon;
	int err;

	
	switch (ts->model) {
	case 7846:
		if (!ts->vref_mv) {
			dev_dbg(&spi->dev, "assuming 2.5V internal vREF\n");
			ts->vref_mv = 2500;
			ts->use_internal = true;
		}
		break;
	case 7845:
	case 7843:
		if (!ts->vref_mv) {
			dev_warn(&spi->dev,
				"external vREF for ADS%d not specified\n",
				ts->model);
			return 0;
		}
		break;
	}

	
	switch (ts->model) {
	case 7846:
		ts->attr_group = &ads7846_attr_group;
		break;
	case 7845:
		ts->attr_group = &ads7845_attr_group;
		break;
	case 7843:
		ts->attr_group = &ads7843_attr_group;
		break;
	default:
		dev_dbg(&spi->dev, "ADS%d not recognized\n", ts->model);
		return 0;
	}

	err = sysfs_create_group(&spi->dev.kobj, ts->attr_group);
	if (err)
		return err;

	hwmon = hwmon_device_register(&spi->dev);
	if (IS_ERR(hwmon)) {
		sysfs_remove_group(&spi->dev.kobj, ts->attr_group);
		return PTR_ERR(hwmon);
	}

	ts->hwmon = hwmon;
	return 0;
}

static void ads784x_hwmon_unregister(struct spi_device *spi,
				     struct ads7846 *ts)
{
	if (ts->hwmon) {
		sysfs_remove_group(&spi->dev.kobj, ts->attr_group);
		hwmon_device_unregister(ts->hwmon);
	}
}

#else
static inline int ads784x_hwmon_register(struct spi_device *spi,
					 struct ads7846 *ts)
{
	return 0;
}

static inline void ads784x_hwmon_unregister(struct spi_device *spi,
					    struct ads7846 *ts)
{
}
#endif

static ssize_t ads7846_pen_down_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ads7846 *ts = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", ts->pendown);
}

static DEVICE_ATTR(pen_down, S_IRUGO, ads7846_pen_down_show, NULL);

static ssize_t ads7846_disable_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ads7846 *ts = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", ts->disabled);
}

static ssize_t ads7846_disable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct ads7846 *ts = dev_get_drvdata(dev);
	unsigned int i;
	int err;

	err = kstrtouint(buf, 10, &i);
	if (err)
		return err;

	if (i)
		ads7846_disable(ts);
	else
		ads7846_enable(ts);

	return count;
}

static DEVICE_ATTR(disable, 0664, ads7846_disable_show, ads7846_disable_store);

static struct attribute *ads784x_attributes[] = {
	&dev_attr_pen_down.attr,
	&dev_attr_disable.attr,
	NULL,
};

static struct attribute_group ads784x_attr_group = {
	.attrs = ads784x_attributes,
};


static int get_pendown_state(struct ads7846 *ts)
{
	if (ts->get_pendown_state)
		return ts->get_pendown_state();

	return !gpio_get_value(ts->gpio_pendown);
}

static void null_wait_for_sync(void)
{
}

static int ads7846_debounce_filter(void *ads, int data_idx, int *val)
{
	struct ads7846 *ts = ads;

	if (!ts->read_cnt || (abs(ts->last_read - *val) > ts->debounce_tol)) {
		
		ts->read_rep = 0;
		if (ts->read_cnt < ts->debounce_max) {
			ts->last_read = *val;
			ts->read_cnt++;
			return ADS7846_FILTER_REPEAT;
		} else {
			ts->read_cnt = 0;
			return ADS7846_FILTER_IGNORE;
		}
	} else {
		if (++ts->read_rep > ts->debounce_rep) {
			ts->read_cnt = 0;
			ts->read_rep = 0;
			return ADS7846_FILTER_OK;
		} else {
			
			ts->read_cnt++;
			return ADS7846_FILTER_REPEAT;
		}
	}
}

static int ads7846_no_filter(void *ads, int data_idx, int *val)
{
	return ADS7846_FILTER_OK;
}

static int ads7846_get_value(struct ads7846 *ts, struct spi_message *m)
{
	struct spi_transfer *t =
		list_entry(m->transfers.prev, struct spi_transfer, transfer_list);

	if (ts->model == 7845) {
		return be16_to_cpup((__be16 *)&(((char*)t->rx_buf)[1])) >> 3;
	} else {
		/*
		 * adjust:  on-wire is a must-ignore bit, a BE12 value, then
		 * padding; built from two 8 bit values written msb-first.
		 */
		return be16_to_cpup((__be16 *)t->rx_buf) >> 3;
	}
}

static void ads7846_update_value(struct spi_message *m, int val)
{
	struct spi_transfer *t =
		list_entry(m->transfers.prev, struct spi_transfer, transfer_list);

	*(u16 *)t->rx_buf = val;
}

static void ads7846_read_state(struct ads7846 *ts)
{
	struct ads7846_packet *packet = ts->packet;
	struct spi_message *m;
	int msg_idx = 0;
	int val;
	int action;
	int error;

	while (msg_idx < ts->msg_count) {

		ts->wait_for_sync();

		m = &ts->msg[msg_idx];
		error = spi_sync(ts->spi, m);
		if (error) {
			dev_err(&ts->spi->dev, "spi_async --> %d\n", error);
			packet->tc.ignore = true;
			return;
		}

		if (msg_idx < ts->msg_count - 1) {

			val = ads7846_get_value(ts, m);

			action = ts->filter(ts->filter_data, msg_idx, &val);
			switch (action) {
			case ADS7846_FILTER_REPEAT:
				continue;

			case ADS7846_FILTER_IGNORE:
				packet->tc.ignore = true;
				msg_idx = ts->msg_count - 1;
				continue;

			case ADS7846_FILTER_OK:
				ads7846_update_value(m, val);
				packet->tc.ignore = false;
				msg_idx++;
				break;

			default:
				BUG();
			}
		} else {
			msg_idx++;
		}
	}
}

static void ads7846_report_state(struct ads7846 *ts)
{
	struct ads7846_packet *packet = ts->packet;
	unsigned int Rt;
	u16 x, y, z1, z2;

	if (ts->model == 7845) {
		x = *(u16 *)packet->tc.x_buf;
		y = *(u16 *)packet->tc.y_buf;
		z1 = 0;
		z2 = 0;
	} else {
		x = packet->tc.x;
		y = packet->tc.y;
		z1 = packet->tc.z1;
		z2 = packet->tc.z2;
	}

	
	if (x == MAX_12BIT)
		x = 0;

	if (ts->model == 7843) {
		Rt = ts->pressure_max / 2;
	} else if (ts->model == 7845) {
		if (get_pendown_state(ts))
			Rt = ts->pressure_max / 2;
		else
			Rt = 0;
		dev_vdbg(&ts->spi->dev, "x/y: %d/%d, PD %d\n", x, y, Rt);
	} else if (likely(x && z1)) {
		
		Rt = z2;
		Rt -= z1;
		Rt *= x;
		Rt *= ts->x_plate_ohms;
		Rt /= z1;
		Rt = (Rt + 2047) >> 12;
	} else {
		Rt = 0;
	}

	if (packet->tc.ignore || Rt > ts->pressure_max) {
		dev_vdbg(&ts->spi->dev, "ignored %d pressure %d\n",
			 packet->tc.ignore, Rt);
		return;
	}

	if (ts->penirq_recheck_delay_usecs) {
		udelay(ts->penirq_recheck_delay_usecs);
		if (!get_pendown_state(ts))
			Rt = 0;
	}

	if (Rt) {
		struct input_dev *input = ts->input;

		if (ts->swap_xy)
			swap(x, y);

		if (!ts->pendown) {
			input_report_key(input, BTN_TOUCH, 1);
			ts->pendown = true;
			dev_vdbg(&ts->spi->dev, "DOWN\n");
		}

		input_report_abs(input, ABS_X, x);
		input_report_abs(input, ABS_Y, y);
		input_report_abs(input, ABS_PRESSURE, ts->pressure_max - Rt);

		input_sync(input);
		dev_vdbg(&ts->spi->dev, "%4d/%4d/%4d\n", x, y, Rt);
	}
}

static irqreturn_t ads7846_hard_irq(int irq, void *handle)
{
	struct ads7846 *ts = handle;

	return get_pendown_state(ts) ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}


static irqreturn_t ads7846_irq(int irq, void *handle)
{
	struct ads7846 *ts = handle;

	
	msleep(TS_POLL_DELAY);

	while (!ts->stopped && get_pendown_state(ts)) {

		
		ads7846_read_state(ts);

		if (!ts->stopped)
			ads7846_report_state(ts);

		wait_event_timeout(ts->wait, ts->stopped,
				   msecs_to_jiffies(TS_POLL_PERIOD));
	}

	if (ts->pendown) {
		struct input_dev *input = ts->input;

		input_report_key(input, BTN_TOUCH, 0);
		input_report_abs(input, ABS_PRESSURE, 0);
		input_sync(input);

		ts->pendown = false;
		dev_vdbg(&ts->spi->dev, "UP\n");
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_PM_SLEEP
static int ads7846_suspend(struct device *dev)
{
	struct ads7846 *ts = dev_get_drvdata(dev);

	mutex_lock(&ts->lock);

	if (!ts->suspended) {

		if (!ts->disabled)
			__ads7846_disable(ts);

		if (device_may_wakeup(&ts->spi->dev))
			enable_irq_wake(ts->spi->irq);

		ts->suspended = true;
	}

	mutex_unlock(&ts->lock);

	return 0;
}

static int ads7846_resume(struct device *dev)
{
	struct ads7846 *ts = dev_get_drvdata(dev);

	mutex_lock(&ts->lock);

	if (ts->suspended) {

		ts->suspended = false;

		if (device_may_wakeup(&ts->spi->dev))
			disable_irq_wake(ts->spi->irq);

		if (!ts->disabled)
			__ads7846_enable(ts);
	}

	mutex_unlock(&ts->lock);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(ads7846_pm, ads7846_suspend, ads7846_resume);

static int __devinit ads7846_setup_pendown(struct spi_device *spi, struct ads7846 *ts)
{
	struct ads7846_platform_data *pdata = spi->dev.platform_data;
	int err;


	if (pdata->get_pendown_state) {
		ts->get_pendown_state = pdata->get_pendown_state;
	} else if (gpio_is_valid(pdata->gpio_pendown)) {

		err = gpio_request_one(pdata->gpio_pendown, GPIOF_IN,
				       "ads7846_pendown");
		if (err) {
			dev_err(&spi->dev,
				"failed to request/setup pendown GPIO%d: %d\n",
				pdata->gpio_pendown, err);
			return err;
		}

		ts->gpio_pendown = pdata->gpio_pendown;

	} else {
		dev_err(&spi->dev, "no get_pendown_state nor gpio_pendown?\n");
		return -EINVAL;
	}

	return 0;
}

static void __devinit ads7846_setup_spi_msg(struct ads7846 *ts,
				const struct ads7846_platform_data *pdata)
{
	struct spi_message *m = &ts->msg[0];
	struct spi_transfer *x = ts->xfer;
	struct ads7846_packet *packet = ts->packet;
	int vref = pdata->keep_vref_on;

	if (ts->model == 7873) {
		ts->model = 7846;
		vref = 0;
	}

	ts->msg_count = 1;
	spi_message_init(m);
	m->context = ts;

	if (ts->model == 7845) {
		packet->read_y_cmd[0] = READ_Y(vref);
		packet->read_y_cmd[1] = 0;
		packet->read_y_cmd[2] = 0;
		x->tx_buf = &packet->read_y_cmd[0];
		x->rx_buf = &packet->tc.y_buf[0];
		x->len = 3;
		spi_message_add_tail(x, m);
	} else {
		
		packet->read_y = READ_Y(vref);
		x->tx_buf = &packet->read_y;
		x->len = 1;
		spi_message_add_tail(x, m);

		x++;
		x->rx_buf = &packet->tc.y;
		x->len = 2;
		spi_message_add_tail(x, m);
	}

	if (pdata->settle_delay_usecs) {
		x->delay_usecs = pdata->settle_delay_usecs;

		x++;
		x->tx_buf = &packet->read_y;
		x->len = 1;
		spi_message_add_tail(x, m);

		x++;
		x->rx_buf = &packet->tc.y;
		x->len = 2;
		spi_message_add_tail(x, m);
	}

	ts->msg_count++;
	m++;
	spi_message_init(m);
	m->context = ts;

	if (ts->model == 7845) {
		x++;
		packet->read_x_cmd[0] = READ_X(vref);
		packet->read_x_cmd[1] = 0;
		packet->read_x_cmd[2] = 0;
		x->tx_buf = &packet->read_x_cmd[0];
		x->rx_buf = &packet->tc.x_buf[0];
		x->len = 3;
		spi_message_add_tail(x, m);
	} else {
		
		x++;
		packet->read_x = READ_X(vref);
		x->tx_buf = &packet->read_x;
		x->len = 1;
		spi_message_add_tail(x, m);

		x++;
		x->rx_buf = &packet->tc.x;
		x->len = 2;
		spi_message_add_tail(x, m);
	}

	
	if (pdata->settle_delay_usecs) {
		x->delay_usecs = pdata->settle_delay_usecs;

		x++;
		x->tx_buf = &packet->read_x;
		x->len = 1;
		spi_message_add_tail(x, m);

		x++;
		x->rx_buf = &packet->tc.x;
		x->len = 2;
		spi_message_add_tail(x, m);
	}

	
	if (ts->model == 7846) {
		ts->msg_count++;
		m++;
		spi_message_init(m);
		m->context = ts;

		x++;
		packet->read_z1 = READ_Z1(vref);
		x->tx_buf = &packet->read_z1;
		x->len = 1;
		spi_message_add_tail(x, m);

		x++;
		x->rx_buf = &packet->tc.z1;
		x->len = 2;
		spi_message_add_tail(x, m);

		
		if (pdata->settle_delay_usecs) {
			x->delay_usecs = pdata->settle_delay_usecs;

			x++;
			x->tx_buf = &packet->read_z1;
			x->len = 1;
			spi_message_add_tail(x, m);

			x++;
			x->rx_buf = &packet->tc.z1;
			x->len = 2;
			spi_message_add_tail(x, m);
		}

		ts->msg_count++;
		m++;
		spi_message_init(m);
		m->context = ts;

		x++;
		packet->read_z2 = READ_Z2(vref);
		x->tx_buf = &packet->read_z2;
		x->len = 1;
		spi_message_add_tail(x, m);

		x++;
		x->rx_buf = &packet->tc.z2;
		x->len = 2;
		spi_message_add_tail(x, m);

		
		if (pdata->settle_delay_usecs) {
			x->delay_usecs = pdata->settle_delay_usecs;

			x++;
			x->tx_buf = &packet->read_z2;
			x->len = 1;
			spi_message_add_tail(x, m);

			x++;
			x->rx_buf = &packet->tc.z2;
			x->len = 2;
			spi_message_add_tail(x, m);
		}
	}

	
	ts->msg_count++;
	m++;
	spi_message_init(m);
	m->context = ts;

	if (ts->model == 7845) {
		x++;
		packet->pwrdown_cmd[0] = PWRDOWN;
		packet->pwrdown_cmd[1] = 0;
		packet->pwrdown_cmd[2] = 0;
		x->tx_buf = &packet->pwrdown_cmd[0];
		x->len = 3;
	} else {
		x++;
		packet->pwrdown = PWRDOWN;
		x->tx_buf = &packet->pwrdown;
		x->len = 1;
		spi_message_add_tail(x, m);

		x++;
		x->rx_buf = &packet->dummy;
		x->len = 2;
	}

	CS_CHANGE(*x);
	spi_message_add_tail(x, m);
}

static int __devinit ads7846_probe(struct spi_device *spi)
{
	struct ads7846 *ts;
	struct ads7846_packet *packet;
	struct input_dev *input_dev;
	struct ads7846_platform_data *pdata = spi->dev.platform_data;
	unsigned long irq_flags;
	int err;

	if (!spi->irq) {
		dev_dbg(&spi->dev, "no IRQ?\n");
		return -ENODEV;
	}

	if (!pdata) {
		dev_dbg(&spi->dev, "no platform data?\n");
		return -ENODEV;
	}

	
	if (spi->max_speed_hz > (125000 * SAMPLE_BITS)) {
		dev_dbg(&spi->dev, "f(sample) %d KHz?\n",
				(spi->max_speed_hz/SAMPLE_BITS)/1000);
		return -EINVAL;
	}

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	err = spi_setup(spi);
	if (err < 0)
		return err;

	ts = kzalloc(sizeof(struct ads7846), GFP_KERNEL);
	packet = kzalloc(sizeof(struct ads7846_packet), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ts || !packet || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}

	dev_set_drvdata(&spi->dev, ts);

	ts->packet = packet;
	ts->spi = spi;
	ts->input = input_dev;
	ts->vref_mv = pdata->vref_mv;
	ts->swap_xy = pdata->swap_xy;

	mutex_init(&ts->lock);
	init_waitqueue_head(&ts->wait);

	ts->model = pdata->model ? : 7846;
	ts->vref_delay_usecs = pdata->vref_delay_usecs ? : 100;
	ts->x_plate_ohms = pdata->x_plate_ohms ? : 400;
	ts->pressure_max = pdata->pressure_max ? : ~0;

	if (pdata->filter != NULL) {
		if (pdata->filter_init != NULL) {
			err = pdata->filter_init(pdata, &ts->filter_data);
			if (err < 0)
				goto err_free_mem;
		}
		ts->filter = pdata->filter;
		ts->filter_cleanup = pdata->filter_cleanup;
	} else if (pdata->debounce_max) {
		ts->debounce_max = pdata->debounce_max;
		if (ts->debounce_max < 2)
			ts->debounce_max = 2;
		ts->debounce_tol = pdata->debounce_tol;
		ts->debounce_rep = pdata->debounce_rep;
		ts->filter = ads7846_debounce_filter;
		ts->filter_data = ts;
	} else {
		ts->filter = ads7846_no_filter;
	}

	err = ads7846_setup_pendown(spi, ts);
	if (err)
		goto err_cleanup_filter;

	if (pdata->penirq_recheck_delay_usecs)
		ts->penirq_recheck_delay_usecs =
				pdata->penirq_recheck_delay_usecs;

	ts->wait_for_sync = pdata->wait_for_sync ? : null_wait_for_sync;

	snprintf(ts->phys, sizeof(ts->phys), "%s/input0", dev_name(&spi->dev));
	snprintf(ts->name, sizeof(ts->name), "ADS%d Touchscreen", ts->model);

	input_dev->name = ts->name;
	input_dev->phys = ts->phys;
	input_dev->dev.parent = &spi->dev;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(input_dev, ABS_X,
			pdata->x_min ? : 0,
			pdata->x_max ? : MAX_12BIT,
			0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			pdata->y_min ? : 0,
			pdata->y_max ? : MAX_12BIT,
			0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE,
			pdata->pressure_min, pdata->pressure_max, 0, 0);

	ads7846_setup_spi_msg(ts, pdata);

	ts->reg = regulator_get(&spi->dev, "vcc");
	if (IS_ERR(ts->reg)) {
		err = PTR_ERR(ts->reg);
		dev_err(&spi->dev, "unable to get regulator: %d\n", err);
		goto err_free_gpio;
	}

	err = regulator_enable(ts->reg);
	if (err) {
		dev_err(&spi->dev, "unable to enable regulator: %d\n", err);
		goto err_put_regulator;
	}

	irq_flags = pdata->irq_flags ? : IRQF_TRIGGER_FALLING;
	irq_flags |= IRQF_ONESHOT;

	err = request_threaded_irq(spi->irq, ads7846_hard_irq, ads7846_irq,
				   irq_flags, spi->dev.driver->name, ts);
	if (err && !pdata->irq_flags) {
		dev_info(&spi->dev,
			"trying pin change workaround on irq %d\n", spi->irq);
		irq_flags |= IRQF_TRIGGER_RISING;
		err = request_threaded_irq(spi->irq,
				  ads7846_hard_irq, ads7846_irq,
				  irq_flags, spi->dev.driver->name, ts);
	}

	if (err) {
		dev_dbg(&spi->dev, "irq %d busy?\n", spi->irq);
		goto err_disable_regulator;
	}

	err = ads784x_hwmon_register(spi, ts);
	if (err)
		goto err_free_irq;

	dev_info(&spi->dev, "touchscreen, irq %d\n", spi->irq);

	if (ts->model == 7845)
		ads7845_read12_ser(&spi->dev, PWRDOWN);
	else
		(void) ads7846_read12_ser(&spi->dev, READ_12BIT_SER(vaux));

	err = sysfs_create_group(&spi->dev.kobj, &ads784x_attr_group);
	if (err)
		goto err_remove_hwmon;

	err = input_register_device(input_dev);
	if (err)
		goto err_remove_attr_group;

	device_init_wakeup(&spi->dev, pdata->wakeup);

	return 0;

 err_remove_attr_group:
	sysfs_remove_group(&spi->dev.kobj, &ads784x_attr_group);
 err_remove_hwmon:
	ads784x_hwmon_unregister(spi, ts);
 err_free_irq:
	free_irq(spi->irq, ts);
 err_disable_regulator:
	regulator_disable(ts->reg);
 err_put_regulator:
	regulator_put(ts->reg);
 err_free_gpio:
	if (!ts->get_pendown_state)
		gpio_free(ts->gpio_pendown);
 err_cleanup_filter:
	if (ts->filter_cleanup)
		ts->filter_cleanup(ts->filter_data);
 err_free_mem:
	input_free_device(input_dev);
	kfree(packet);
	kfree(ts);
	return err;
}

static int __devexit ads7846_remove(struct spi_device *spi)
{
	struct ads7846 *ts = dev_get_drvdata(&spi->dev);

	device_init_wakeup(&spi->dev, false);

	sysfs_remove_group(&spi->dev.kobj, &ads784x_attr_group);

	ads7846_disable(ts);
	free_irq(ts->spi->irq, ts);

	input_unregister_device(ts->input);

	ads784x_hwmon_unregister(spi, ts);

	regulator_disable(ts->reg);
	regulator_put(ts->reg);

	if (!ts->get_pendown_state) {
		gpio_free(ts->gpio_pendown);
	}

	if (ts->filter_cleanup)
		ts->filter_cleanup(ts->filter_data);

	kfree(ts->packet);
	kfree(ts);

	dev_dbg(&spi->dev, "unregistered touchscreen\n");

	return 0;
}

static struct spi_driver ads7846_driver = {
	.driver = {
		.name	= "ads7846",
		.owner	= THIS_MODULE,
		.pm	= &ads7846_pm,
	},
	.probe		= ads7846_probe,
	.remove		= __devexit_p(ads7846_remove),
};

module_spi_driver(ads7846_driver);

MODULE_DESCRIPTION("ADS7846 TouchScreen Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:ads7846");
