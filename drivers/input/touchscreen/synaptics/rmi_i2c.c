/**
 *
 * Synaptics Register Mapped Interface (RMI4) I2C Physical Layer Driver.
 * Copyright (c) 2007-2011, Synaptics Incorporated
 *
 */
/*
 * This file is licensed under the GPL2 license.
 *
 *#############################################################################
 * GPL
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 *#############################################################################
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/input/rmi_platformdata.h>
#include <linux/input/rmi_i2c.h>

#include "rmi_drvr.h"

#define DRIVER_NAME "rmi4_ts"

#define DEVICE_NAME "rmi4_ts"

static DEFINE_MUTEX(page_mutex);


static const struct i2c_device_id rmi_i2c_id_table[] = {
	{ DEVICE_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, rmi_i2c_id_table);


static int device_count;


struct instance_data {
	int instance_no;
	int irq;
	struct rmi_phys_driver rmiphysdrvr;
	struct i2c_client *i2cclient; 
	int page;
};

#if	defined(USE_PAGESELECT)
int
rmi_set_page(struct instance_data *instancedata, unsigned int page)
{
	char txbuf[2];
	int retval;
	txbuf[0] = 0xff;
	txbuf[1] = page;
	retval = i2c_master_send(instancedata->i2cclient, txbuf, 2);
	if (retval != 2) {
		dev_err(&instancedata->i2cclient->dev,
				"%s: Set page failed: %d.", __func__, retval);
	} else {
		retval = 0;
		instancedata->page = page;
	}
	return retval;
}
#else
int
rmi_set_page(struct instance_data *instancedata, unsigned int page)
{
	return 0;
}
#endif

static int
rmi_i2c_read(struct rmi_phys_driver *physdrvr, unsigned short address, char *valp)
{
	struct instance_data *instancedata =
		container_of(physdrvr, struct instance_data, rmiphysdrvr);

	char txbuf[2];
	int retval = 0;
	int retry_count = 0;

	
	mutex_lock(&page_mutex);

	if (((address >> 8) & 0xff) != instancedata->page) {
		
		retval = rmi_set_page(instancedata, ((address >> 8) & 0xff));
		if (retval)
			goto exit;
	}

retry:
	txbuf[0] = address & 0xff;
	retval = i2c_master_send(instancedata->i2cclient, txbuf, 1);

	if (retval != 1) {
		dev_err(&instancedata->i2cclient->dev, "%s: Write fail: %d\n",
				__func__, retval);
		goto exit;
	}
	retval = i2c_master_recv(instancedata->i2cclient, txbuf, 1);

	if (retval != 1) {
		if (++retry_count == 5) {
			dev_err(&instancedata->i2cclient->dev,
					"%s: Read of 0x%04x fail: %d\n",
					__func__, address, retval);
		} else {
			mdelay(10);
			rmi_set_page(instancedata, ((address >> 8) & 0xff));
			goto retry;
		}
	} else {
		retval = 0;
		*valp = txbuf[0];
	}
exit:

	mutex_unlock(&page_mutex);
	return retval;
}

static int
rmi_i2c_read_multiple(struct rmi_phys_driver *physdrvr, unsigned short address,
	char *valp, int size)
{
	struct instance_data *instancedata =
		container_of(physdrvr, struct instance_data, rmiphysdrvr);

	char txbuf[2];
	int retval = 0;
	int retry_count = 0;

	
	mutex_lock(&page_mutex);

	if (((address >> 8) & 0xff) != instancedata->page) {
		
		retval = rmi_set_page(instancedata, ((address >> 8) & 0xff));
		if (retval)
			goto exit;
	}

retry:
	txbuf[0] = address & 0xff;
	retval = i2c_master_send(instancedata->i2cclient, txbuf, 1);

	if (retval != 1) {
		dev_err(&instancedata->i2cclient->dev, "%s: Write fail: %d\n",
				__func__, retval);
		goto exit;
	}
	retval = i2c_master_recv(instancedata->i2cclient, valp, size);

	if (retval != size) {
		if (++retry_count == 5) {
			dev_err(&instancedata->i2cclient->dev,
					"%s: Read of 0x%04x size %d fail: %d\n",
					__func__, address, size, retval);
		} else {
			mdelay(10);
			rmi_set_page(instancedata, ((address >> 8) & 0xff));
			goto retry;
		}
	} else {
		retval = 0;
	}
exit:

	mutex_unlock(&page_mutex);
	return retval;
}


/*
 * Write a single register through i2c.
 * You can write multiple registers at once, but I made the functions for that
 * seperate for performance reasons.  Writing multiple requires allocation and
 * freeing.
 *
 * param[in] pd - The pointer to the rmi_phys_driver struct
 * param[in] address - The address at which to start the write.
 * param[in] data - The data to be written.
 * returns one upon success, something else upon error.
 */
static int
rmi_i2c_write(struct rmi_phys_driver *physdrvr, unsigned short address, char data)
{
	struct instance_data *instancedata =
		container_of(physdrvr, struct instance_data, rmiphysdrvr);

	unsigned char txbuf[2];
	int retval = 0;

	
	mutex_lock(&page_mutex);

	if (((address >> 8) & 0xff) != instancedata->page) {
		
		retval = rmi_set_page(instancedata, ((address >> 8) & 0xff));
		if (retval)
			goto exit;
	}

	txbuf[0] = address & 0xff;
	txbuf[1] = data;
	retval = i2c_master_send(instancedata->i2cclient, txbuf, 2);

	
	if (retval != 2) {
		dev_err(&instancedata->i2cclient->dev, "%s: Write fail: %d\n",
			__func__, retval);
		goto exit; 
	} else {
		retval = 1;
	}
exit:

	mutex_unlock(&page_mutex);
	return retval;
}

/*
 * Write multiple registers.
 *
 * For fast writes of 16 bytes of less we will re-use a buffer on the stack.
 * For larger writes (like for RMI reflashing) we will need to allocate a
 * temp buffer.
 *
 * param[in] pd - The pointer to the rmi_phys_driver struct
 * param[in] address - The address at which to start the write.
 * param[in] valp - A pointer to a buffer containing the data to be written.
 * param[in] size - The number of bytes to write.
 * returns one upon success, something else upon error.
 */
static int
rmi_i2c_write_multiple(struct rmi_phys_driver *physdrvr, unsigned short address,
	char *valp, int size)
{
	struct instance_data *instancedata =
		container_of(physdrvr, struct instance_data, rmiphysdrvr);

	unsigned char *txbuf;
	unsigned char txbuf_most[17]; 
	int retval = 0;
	int i;

	if (size < sizeof(txbuf_most)) {
		
		txbuf = txbuf_most;
	} else {
		
		txbuf = kzalloc(size + 1, GFP_KERNEL);
		if (!txbuf)
			return -ENOMEM;
	}

	
	for (i = 0; i < size; i++)
		txbuf[i + 1] = valp[i];

	
	mutex_lock(&page_mutex);

	if (((address >> 8) & 0xff) != instancedata->page) {
		
		retval = rmi_set_page(instancedata, ((address >> 8) & 0xff));
		if (retval)
			goto exit;
	}

	txbuf[0] = address & 0xff; 
	retval = i2c_master_send(instancedata->i2cclient, txbuf, size + 1);

	
	if (retval != 1) {
		dev_err(&instancedata->i2cclient->dev, "%s: Write fail: %d\n",
				__func__, retval);
		goto exit;
	}
exit:

	mutex_unlock(&page_mutex);
	if (txbuf != txbuf_most)
		kfree(txbuf);
	return retval;
}

static irqreturn_t
i2c_attn_isr(int irq, void *info)
{
	struct instance_data *instancedata = info;

	disable_irq_nosync(instancedata->irq);

	if (instancedata->rmiphysdrvr.attention) {
		instancedata->rmiphysdrvr.attention(&instancedata->rmiphysdrvr,
			instancedata->instance_no);
	}

	return IRQ_HANDLED;
}

static int
rmi_i2c_probe(struct i2c_client *client, const struct i2c_device_id *dev_id)
{

	struct instance_data *instancedata;
	int retval = 0;
	int irqtype = 0;

	struct rmi_i2c_platformdata *platformdata;
	struct rmi_sensordata *sensordata;

	if (client == NULL) {
		printk(KERN_ERR "%s: Invalid NULL client received.", __func__);
		return -EINVAL;
	}

	printk(KERN_DEBUG "%s: Probing i2c RMI device, addr: 0x%02x", __func__, client->addr);


	
	instancedata = kzalloc(sizeof(*instancedata), GFP_KERNEL);
	if (!instancedata) {
		dev_err(&client->dev,
			"%s: Out of memory trying to allocate instance_data.\n",
			__func__);
		return -ENOMEM;
	}

	instancedata->rmiphysdrvr.name           = DRIVER_NAME;
	instancedata->rmiphysdrvr.write          = rmi_i2c_write;
	instancedata->rmiphysdrvr.read           = rmi_i2c_read;
	instancedata->rmiphysdrvr.write_multiple = rmi_i2c_write_multiple;
	instancedata->rmiphysdrvr.read_multiple  = rmi_i2c_read_multiple;
	instancedata->rmiphysdrvr.module         = THIS_MODULE;

	instancedata->rmiphysdrvr.polling_required = true;

	instancedata->page = 0xffff; 

	platformdata = client->dev.platform_data;
	if (platformdata == NULL) {
		printk(KERN_ERR "%s: CONFIGURATION ERROR - platform data is NULL.", __func__);
		return -EINVAL;
	}
	sensordata = platformdata->sensordata;

	if (platformdata->delay_ms > 0)
		mdelay(platformdata->delay_ms);

	if (sensordata && sensordata->rmi_sensor_setup) {
		retval = sensordata->rmi_sensor_setup();
		if (retval) {
			printk(KERN_ERR "%s: sensor setup failed with code %d.", __func__, retval);
			return retval;
		}
	}

	printk(KERN_DEBUG "%s: sensor addr: 0x%02x irq: 0x%x type: %d",
		__func__, platformdata->i2c_address, platformdata->irq, platformdata->irq_type);
	if (client->addr != platformdata->i2c_address) {
		printk(KERN_ERR "%s: CONFIGURATION ERROR - client I2C address 0x%02x doesn't match platform data address 0x%02x.", __func__, client->addr, platformdata->i2c_address);
		return -EINVAL;
	}

	instancedata->instance_no = device_count++;

	dev_set_name(&client->dev,
		"rmi4-i2c%d", instancedata->instance_no);

	if (platformdata->irq) {
		instancedata->irq = platformdata->irq;
		switch (platformdata->irq_type) {
		case IORESOURCE_IRQ_HIGHEDGE:
			irqtype = IRQF_TRIGGER_RISING;
			break;
		case IORESOURCE_IRQ_LOWEDGE:
			irqtype = IRQF_TRIGGER_FALLING;
			break;
		case IORESOURCE_IRQ_HIGHLEVEL:
			irqtype = IRQF_TRIGGER_HIGH;
			break;
		case IORESOURCE_IRQ_LOWLEVEL:
			irqtype = IRQF_TRIGGER_LOW;
			break;
		default:
			dev_warn(&client->dev,
				"%s: Invalid IRQ flags in platform data.\n",
				__func__);
			kfree(instancedata);
			return -ENXIO;
		}

		instancedata->rmiphysdrvr.polling_required = false;
		instancedata->rmiphysdrvr.irq = instancedata->irq;

	} else {
		instancedata->rmiphysdrvr.polling_required = true;
		dev_info(&client->dev,
				"%s: No IRQ info given. Polling required.\n",
				__func__);
	}

	i2c_set_clientdata(client, instancedata);

	instancedata->i2cclient = client;

	retval = rmi_register_sensor(&instancedata->rmiphysdrvr, platformdata->sensordata);
	if (retval) {
		dev_err(&client->dev, "%s: Failed to Register %s sensor drivers\n",
				__func__, instancedata->rmiphysdrvr.name);
		i2c_set_clientdata(client, NULL);
		kfree(instancedata);
		return retval;
	}

	if (instancedata->rmiphysdrvr.polling_required == false) {
		retval = request_irq(instancedata->irq, i2c_attn_isr,
				irqtype, "rmi_i2c", instancedata);
		if (retval) {
			dev_err(&client->dev, "%s: failed to obtain IRQ %d. Result: %d.",
				__func__, instancedata->irq, retval);
			dev_info(&client->dev, "%s: Reverting to polling.\n", __func__);
			instancedata->rmiphysdrvr.polling_required = true;
			
		} else {
			dev_dbg(&client->dev, "%s: got irq.\n", __func__);
		}
	}

	dev_dbg(&client->dev, "%s: Successfully registered %s sensor driver.\n",
			__func__, instancedata->rmiphysdrvr.name);

	printk(KERN_INFO "%s: Successfully registered %s sensor driver.\n", __func__, instancedata->rmiphysdrvr.name);

	return retval;
}

static int
rmi_i2c_remove(struct i2c_client *client)
{
	struct instance_data *instancedata =
		i2c_get_clientdata(client);

	dev_dbg(&client->dev, "%s: Unregistering phys driver %s\n", __func__,
		instancedata->rmiphysdrvr.name);

	rmi_unregister_sensors(&instancedata->rmiphysdrvr);

	dev_dbg(&client->dev, "%s: Unregistered phys driver %s\n",
			__func__, instancedata->rmiphysdrvr.name);

	if (instancedata->irq)
		free_irq(instancedata->irq, instancedata);

	kfree(instancedata);
	dev_dbg(&client->dev, "%s: Remove successful\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
static int
rmi_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	
	return 0;
}

static int
rmi_i2c_resume(struct i2c_client *client)
{
	
	return 0;
}
#else
#define rmi_i2c_suspend	NULL
#define rmi_i2c_resume	NULL
#endif

static struct i2c_driver rmi_i2c_driver = {
	.probe		= rmi_i2c_probe,
	.remove		= rmi_i2c_remove,
	.suspend	= rmi_i2c_suspend,
	.resume		= rmi_i2c_resume,
	.driver = {
		.name  = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.id_table	= rmi_i2c_id_table,
};

static int __init rmi_phys_i2c_init(void)
{
	return i2c_add_driver(&rmi_i2c_driver);
}

static void __exit rmi_phys_i2c_exit(void)
{
	i2c_del_driver(&rmi_i2c_driver);
}


module_init(rmi_phys_i2c_init);
module_exit(rmi_phys_i2c_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("RMI4 Driver I2C Physical Layer");
MODULE_LICENSE("GPL");
