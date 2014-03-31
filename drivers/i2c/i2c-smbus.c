/*
 * i2c-smbus.c - SMBus extensions to the I2C protocol
 *
 * Copyright (C) 2008 David Brownell
 * Copyright (C) 2010 Jean Delvare <khali@linux-fr.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/i2c-smbus.h>
#include <linux/slab.h>

struct i2c_smbus_alert {
	unsigned int		alert_edge_triggered:1;
	int			irq;
	struct work_struct	alert;
	struct i2c_client	*ara;		
};

struct alert_data {
	unsigned short		addr;
	u8			flag:1;
};

static int smbus_do_alert(struct device *dev, void *addrp)
{
	struct i2c_client *client = i2c_verify_client(dev);
	struct alert_data *data = addrp;

	if (!client || client->addr != data->addr)
		return 0;
	if (client->flags & I2C_CLIENT_TEN)
		return 0;

	device_lock(dev);
	if (client->driver) {
		if (client->driver->alert)
			client->driver->alert(client, data->flag);
		else
			dev_warn(&client->dev, "no driver alert()!\n");
	} else
		dev_dbg(&client->dev, "alert with no driver\n");
	device_unlock(dev);

	
	return -EBUSY;
}

static void smbus_alert(struct work_struct *work)
{
	struct i2c_smbus_alert *alert;
	struct i2c_client *ara;
	unsigned short prev_addr = 0;	

	alert = container_of(work, struct i2c_smbus_alert, alert);
	ara = alert->ara;

	for (;;) {
		s32 status;
		struct alert_data data;

		status = i2c_smbus_read_byte(ara);
		if (status < 0)
			break;

		data.flag = status & 1;
		data.addr = status >> 1;

		if (data.addr == prev_addr) {
			dev_warn(&ara->dev, "Duplicate SMBALERT# from dev "
				"0x%02x, skipping\n", data.addr);
			break;
		}
		dev_dbg(&ara->dev, "SMBALERT# from dev 0x%02x, flag %d\n",
			data.addr, data.flag);

		
		device_for_each_child(&ara->adapter->dev, &data,
				      smbus_do_alert);
		prev_addr = data.addr;
	}

	
	if (!alert->alert_edge_triggered)
		enable_irq(alert->irq);
}

static irqreturn_t smbalert_irq(int irq, void *d)
{
	struct i2c_smbus_alert *alert = d;

	
	if (!alert->alert_edge_triggered)
		disable_irq_nosync(irq);

	schedule_work(&alert->alert);
	return IRQ_HANDLED;
}

static int smbalert_probe(struct i2c_client *ara,
			  const struct i2c_device_id *id)
{
	struct i2c_smbus_alert_setup *setup = ara->dev.platform_data;
	struct i2c_smbus_alert *alert;
	struct i2c_adapter *adapter = ara->adapter;
	int res;

	alert = kzalloc(sizeof(struct i2c_smbus_alert), GFP_KERNEL);
	if (!alert)
		return -ENOMEM;

	alert->alert_edge_triggered = setup->alert_edge_triggered;
	alert->irq = setup->irq;
	INIT_WORK(&alert->alert, smbus_alert);
	alert->ara = ara;

	if (setup->irq > 0) {
		res = devm_request_irq(&ara->dev, setup->irq, smbalert_irq,
				       0, "smbus_alert", alert);
		if (res) {
			kfree(alert);
			return res;
		}
	}

	i2c_set_clientdata(ara, alert);
	dev_info(&adapter->dev, "supports SMBALERT#, %s trigger\n",
		 setup->alert_edge_triggered ? "edge" : "level");

	return 0;
}

static int smbalert_remove(struct i2c_client *ara)
{
	struct i2c_smbus_alert *alert = i2c_get_clientdata(ara);

	cancel_work_sync(&alert->alert);

	kfree(alert);
	return 0;
}

static const struct i2c_device_id smbalert_ids[] = {
	{ "smbus_alert", 0 },
	{  }
};
MODULE_DEVICE_TABLE(i2c, smbalert_ids);

static struct i2c_driver smbalert_driver = {
	.driver = {
		.name	= "smbus_alert",
	},
	.probe		= smbalert_probe,
	.remove		= smbalert_remove,
	.id_table	= smbalert_ids,
};

struct i2c_client *i2c_setup_smbus_alert(struct i2c_adapter *adapter,
					 struct i2c_smbus_alert_setup *setup)
{
	struct i2c_board_info ara_board_info = {
		I2C_BOARD_INFO("smbus_alert", 0x0c),
		.platform_data = setup,
	};

	return i2c_new_device(adapter, &ara_board_info);
}
EXPORT_SYMBOL_GPL(i2c_setup_smbus_alert);

int i2c_handle_smbus_alert(struct i2c_client *ara)
{
	struct i2c_smbus_alert *alert = i2c_get_clientdata(ara);

	return schedule_work(&alert->alert);
}
EXPORT_SYMBOL_GPL(i2c_handle_smbus_alert);

static int __init i2c_smbus_init(void)
{
	return i2c_add_driver(&smbalert_driver);
}

static void __exit i2c_smbus_exit(void)
{
	i2c_del_driver(&smbalert_driver);
}

module_init(i2c_smbus_init);
module_exit(i2c_smbus_exit);

MODULE_AUTHOR("Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("SMBus protocol extensions support");
MODULE_LICENSE("GPL");
