/*
 * drivers/media/radio/si470x/radio-si470x-i2c.c
 *
 * I2C driver for radios with Silicon Labs Si470x FM Radio Receivers
 *
 * Copyright (c) 2009 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#define DRIVER_AUTHOR "Joonyoung Shim <jy0922.shim@samsung.com>";
#define DRIVER_CARD "Silicon Labs Si470x FM Radio Receiver"
#define DRIVER_DESC "I2C radio driver for Si470x FM Radio Receivers"
#define DRIVER_VERSION "1.0.2"

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#include "radio-si470x.h"


static const struct i2c_device_id si470x_i2c_id[] = {
	
	{ "si470x", 0 },
	
	{ }
};
MODULE_DEVICE_TABLE(i2c, si470x_i2c_id);




static int radio_nr = -1;
module_param(radio_nr, int, 0444);
MODULE_PARM_DESC(radio_nr, "Radio Nr");

static unsigned int rds_buf = 100;
module_param(rds_buf, uint, 0444);
MODULE_PARM_DESC(rds_buf, "RDS buffer entries: *100*");

static unsigned short max_rds_errors = 1;
module_param(max_rds_errors, ushort, 0644);
MODULE_PARM_DESC(max_rds_errors, "RDS maximum block errors: *1*");




#define WRITE_REG_NUM		8
#define WRITE_INDEX(i)		(i + 0x02)

#define READ_REG_NUM		RADIO_REGISTER_NUM
#define READ_INDEX(i)		((i + RADIO_REGISTER_NUM - 0x0a) % READ_REG_NUM)




int si470x_get_register(struct si470x_device *radio, int regnr)
{
	u16 buf[READ_REG_NUM];
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, I2C_M_RD, sizeof(u16) * READ_REG_NUM,
			(void *)buf },
	};

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	radio->registers[regnr] = __be16_to_cpu(buf[READ_INDEX(regnr)]);

	return 0;
}


int si470x_set_register(struct si470x_device *radio, int regnr)
{
	int i;
	u16 buf[WRITE_REG_NUM];
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, 0, sizeof(u16) * WRITE_REG_NUM,
			(void *)buf },
	};

	for (i = 0; i < WRITE_REG_NUM; i++)
		buf[i] = __cpu_to_be16(radio->registers[WRITE_INDEX(i)]);

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	return 0;
}




static int si470x_get_all_registers(struct si470x_device *radio)
{
	int i;
	u16 buf[READ_REG_NUM];
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, I2C_M_RD, sizeof(u16) * READ_REG_NUM,
			(void *)buf },
	};

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	for (i = 0; i < READ_REG_NUM; i++)
		radio->registers[i] = __be16_to_cpu(buf[READ_INDEX(i)]);

	return 0;
}




int si470x_disconnect_check(struct si470x_device *radio)
{
	return 0;
}




int si470x_fops_open(struct file *file)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	radio->users++;

	if (radio->users == 1) {
		
		retval = si470x_start(radio);
		if (retval < 0)
			goto done;

		
		radio->registers[SYSCONFIG1] |= SYSCONFIG1_RDSIEN;
		radio->registers[SYSCONFIG1] |= SYSCONFIG1_STCIEN;
		radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_GPIO2;
		radio->registers[SYSCONFIG1] |= 0x1 << 2;
		retval = si470x_set_register(radio, SYSCONFIG1);
	}

done:
	mutex_unlock(&radio->lock);
	return retval;
}


int si470x_fops_release(struct file *file)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	
	if (!radio)
		return -ENODEV;

	mutex_lock(&radio->lock);
	radio->users--;
	if (radio->users == 0)
		
		retval = si470x_stop(radio);

	mutex_unlock(&radio->lock);

	return retval;
}




int si470x_vidioc_querycap(struct file *file, void *priv,
		struct v4l2_capability *capability)
{
	strlcpy(capability->driver, DRIVER_NAME, sizeof(capability->driver));
	strlcpy(capability->card, DRIVER_CARD, sizeof(capability->card));
	capability->capabilities = V4L2_CAP_HW_FREQ_SEEK |
		V4L2_CAP_TUNER | V4L2_CAP_RADIO;

	return 0;
}




static irqreturn_t si470x_i2c_interrupt(int irq, void *dev_id)
{
	struct si470x_device *radio = dev_id;
	unsigned char regnr;
	unsigned char blocknum;
	unsigned short bler; 
	unsigned short rds;
	unsigned char tmpbuf[3];
	int retval = 0;

	
	retval = si470x_get_register(radio, STATUSRSSI);
	if (retval < 0)
		goto end;

	if (radio->registers[STATUSRSSI] & STATUSRSSI_STC)
		complete(&radio->completion);

	
	if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
		goto end;

	
	for (regnr = 1; regnr < RDS_REGISTER_NUM; regnr++) {
		retval = si470x_get_register(radio, STATUSRSSI + regnr);
		if (retval < 0)
			goto end;
	}

	
	if ((radio->registers[STATUSRSSI] & STATUSRSSI_RDSR) == 0)
		
		goto end;

	for (blocknum = 0; blocknum < 4; blocknum++) {
		switch (blocknum) {
		default:
			bler = (radio->registers[STATUSRSSI] &
					STATUSRSSI_BLERA) >> 9;
			rds = radio->registers[RDSA];
			break;
		case 1:
			bler = (radio->registers[READCHAN] &
					READCHAN_BLERB) >> 14;
			rds = radio->registers[RDSB];
			break;
		case 2:
			bler = (radio->registers[READCHAN] &
					READCHAN_BLERC) >> 12;
			rds = radio->registers[RDSC];
			break;
		case 3:
			bler = (radio->registers[READCHAN] &
					READCHAN_BLERD) >> 10;
			rds = radio->registers[RDSD];
			break;
		};

		
		put_unaligned_le16(rds, &tmpbuf);
		tmpbuf[2] = blocknum;		
		tmpbuf[2] |= blocknum << 3;	
		if (bler > max_rds_errors)
			tmpbuf[2] |= 0x80;	
		else if (bler > 0)
			tmpbuf[2] |= 0x40;	

		
		memcpy(&radio->buffer[radio->wr_index], &tmpbuf, 3);
		radio->wr_index += 3;

		
		if (radio->wr_index >= radio->buf_size)
			radio->wr_index = 0;

		
		if (radio->wr_index == radio->rd_index) {
			
			radio->rd_index += 3;
			if (radio->rd_index >= radio->buf_size)
				radio->rd_index = 0;
		}
	}

	if (radio->wr_index != radio->rd_index)
		wake_up_interruptible(&radio->read_queue);

end:
	return IRQ_HANDLED;
}


static int __devinit si470x_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct si470x_device *radio;
	int retval = 0;
	unsigned char version_warning = 0;

	
	radio = kzalloc(sizeof(struct si470x_device), GFP_KERNEL);
	if (!radio) {
		retval = -ENOMEM;
		goto err_initial;
	}

	radio->users = 0;
	radio->client = client;
	mutex_init(&radio->lock);

	
	radio->videodev = video_device_alloc();
	if (!radio->videodev) {
		retval = -ENOMEM;
		goto err_radio;
	}
	memcpy(radio->videodev, &si470x_viddev_template,
			sizeof(si470x_viddev_template));
	video_set_drvdata(radio->videodev, radio);

	
	radio->registers[POWERCFG] = POWERCFG_ENABLE;
	if (si470x_set_register(radio, POWERCFG) < 0) {
		retval = -EIO;
		goto err_video;
	}
	msleep(110);

	
	if (si470x_get_all_registers(radio) < 0) {
		retval = -EIO;
		goto err_video;
	}
	dev_info(&client->dev, "DeviceID=0x%4.4hx ChipID=0x%4.4hx\n",
			radio->registers[DEVICEID], radio->registers[CHIPID]);
	if ((radio->registers[CHIPID] & CHIPID_FIRMWARE) < RADIO_FW_VERSION) {
		dev_warn(&client->dev,
			"This driver is known to work with "
			"firmware version %hu,\n", RADIO_FW_VERSION);
		dev_warn(&client->dev,
			"but the device has firmware version %hu.\n",
			radio->registers[CHIPID] & CHIPID_FIRMWARE);
		version_warning = 1;
	}

	
	if (version_warning == 1) {
		dev_warn(&client->dev,
			"If you have some trouble using this driver,\n");
		dev_warn(&client->dev,
			"please report to V4L ML at "
			"linux-media@vger.kernel.org\n");
	}

	
	si470x_set_freq(radio, 87.5 * FREQ_MUL); 

	
	radio->buf_size = rds_buf * 3;
	radio->buffer = kmalloc(radio->buf_size, GFP_KERNEL);
	if (!radio->buffer) {
		retval = -EIO;
		goto err_video;
	}

	
	radio->wr_index = 0;
	radio->rd_index = 0;
	init_waitqueue_head(&radio->read_queue);

	
	radio->stci_enabled = true;
	init_completion(&radio->completion);

	retval = request_threaded_irq(client->irq, NULL, si470x_i2c_interrupt,
			IRQF_TRIGGER_FALLING, DRIVER_NAME, radio);
	if (retval) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_rds;
	}

	
	retval = video_register_device(radio->videodev, VFL_TYPE_RADIO,
			radio_nr);
	if (retval) {
		dev_warn(&client->dev, "Could not register video device\n");
		goto err_all;
	}
	i2c_set_clientdata(client, radio);

	return 0;
err_all:
	free_irq(client->irq, radio);
err_rds:
	kfree(radio->buffer);
err_video:
	video_device_release(radio->videodev);
err_radio:
	kfree(radio);
err_initial:
	return retval;
}


static __devexit int si470x_i2c_remove(struct i2c_client *client)
{
	struct si470x_device *radio = i2c_get_clientdata(client);

	free_irq(client->irq, radio);
	video_unregister_device(radio->videodev);
	kfree(radio);

	return 0;
}


#ifdef CONFIG_PM
static int si470x_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct si470x_device *radio = i2c_get_clientdata(client);

	
	radio->registers[POWERCFG] |= POWERCFG_DISABLE;
	if (si470x_set_register(radio, POWERCFG) < 0)
		return -EIO;

	return 0;
}


static int si470x_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct si470x_device *radio = i2c_get_clientdata(client);

	
	radio->registers[POWERCFG] |= POWERCFG_ENABLE;
	if (si470x_set_register(radio, POWERCFG) < 0)
		return -EIO;
	msleep(110);

	return 0;
}

static SIMPLE_DEV_PM_OPS(si470x_i2c_pm, si470x_i2c_suspend, si470x_i2c_resume);
#endif


static struct i2c_driver si470x_i2c_driver = {
	.driver = {
		.name		= "si470x",
		.owner		= THIS_MODULE,
#ifdef CONFIG_PM
		.pm		= &si470x_i2c_pm,
#endif
	},
	.probe			= si470x_i2c_probe,
	.remove			= __devexit_p(si470x_i2c_remove),
	.id_table		= si470x_i2c_id,
};

module_i2c_driver(si470x_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
