/**
 *
 * Synaptics Register Mapped Interface (RMI4) SPI Physical Layer Driver.
 * Copyright (C) 2008-2011, Synaptics Incorporated
 *
 */
/*
 * This file is licensed under the GPL2 license.
 *
 *############################################################################
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
 *############################################################################
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/semaphore.h>
#include <linux/spi/spi.h>
#include <linux/input/rmi_platformdata.h>
#include "rmi_spi.h"
#include "rmi_drvr.h"

#define COMM_DEBUG  1 

#define RMI_DEFAULT_BYTE_DELAY_US	0 
#define	SPI_BUFFER_SIZE	32

static u8 *buf;

struct spi_device_instance_data {
	int		instance_no;
	int		irq;
	unsigned int	byte_delay_us;
	struct		rmi_phys_driver rpd;
	struct		spi_device *spidev;
	struct		rmi_spi_platformdata *platformdata;
};

static int spi_xfer(struct spi_device_instance_data *instance_data,
	const u8 *txbuf, unsigned n_tx, u8 *rxbuf, unsigned n_rx)
{
	struct spi_device *spi = instance_data->spidev;
#if COMM_DEBUG
	int i;
#endif
	int status;
	struct spi_message message;
	struct spi_transfer *xfer_list;
	u8 *local_buf;
	int nXfers = 0;
	int xfer_index = 0;

	if ((n_tx + n_rx) > SPI_BUFFER_SIZE)
		return -EINVAL;

	if (n_tx)
		nXfers += 1;
	if (n_rx) {
		if (instance_data->byte_delay_us)
			nXfers += n_rx;
		else
			nXfers += 1;
	}

	xfer_list = kcalloc(nXfers, sizeof(struct spi_transfer), GFP_KERNEL);
	if (!xfer_list)
		return -ENOMEM;

	
	local_buf = kzalloc(SPI_BUFFER_SIZE, GFP_KERNEL);
	if (!local_buf) {
		kfree(xfer_list);
		return -ENOMEM;
	}

	spi_message_init(&message);

	if (n_tx) {
		memset(&xfer_list[0], 0, sizeof(struct spi_transfer));
		xfer_list[0].len = n_tx;
		xfer_list[0].delay_usecs = instance_data->byte_delay_us;
		spi_message_add_tail(&xfer_list[0], &message);
		memcpy(local_buf, txbuf, n_tx);
		xfer_list[0].tx_buf = local_buf;
		xfer_index++;
	}
	if (n_rx) {
		if (instance_data->byte_delay_us) {
			int buffer_offset = n_tx;
			for (; xfer_index < nXfers; xfer_index++) {
				memset(&xfer_list[xfer_index], 0,
						sizeof(struct spi_transfer));
				xfer_list[xfer_index].len = 1;
				xfer_list[xfer_index].delay_usecs =
					instance_data->byte_delay_us;
				xfer_list[xfer_index].rx_buf =
					local_buf + buffer_offset;
				buffer_offset++;
				spi_message_add_tail(&xfer_list[xfer_index],
						&message);
#ifdef CONFIG_ARCH_OMAP
				printk(KERN_INFO "%s: Did you compensate for
					      ARCH_OMAP?", __func__);
	
#else
#endif
			}
		} else {
			memset(&xfer_list[xfer_index], 0, sizeof(struct
						spi_transfer));
#ifdef CONFIG_ARCH_OMAP
			
			xfer_list[xfer_index].len = n_rx-1;
#else
			xfer_list[xfer_index].len = n_rx;
#endif
			xfer_list[xfer_index].rx_buf = local_buf + n_tx;
			spi_message_add_tail(&xfer_list[xfer_index],
					&message);
			xfer_index++;
		}
	}
	printk(KERN_INFO "%s: Ready to go, xfer_index = %d, nXfers = %d.",
			__func__, xfer_index, nXfers);
#if COMM_DEBUG
	printk(KERN_INFO "%s: SPI transmits %d bytes...", __func__, n_tx);
	for (i = 0; i < n_tx; i++)
		printk(KERN_INFO "    0x%02X", local_buf[i]);
#endif

	
	status = spi_sync(spi, &message);
	if (status == 0) {
		memcpy(rxbuf, local_buf + n_tx, n_rx);
		status = message.status;
#if COMM_DEBUG
		if (n_rx) {
			printk(KERN_INFO "%s: SPI received %d bytes...",
					__func__, n_rx);
			for (i = 0; i < n_rx; i++)
				printk(KERN_INFO "    0x%02X", rxbuf[i]);
		}
#endif
	} else {
		printk(KERN_ERR "%s: spi_sync failed with error code %d.",
				__func__, status);
	}

	kfree(local_buf);
	kfree(xfer_list);

	return status;
}

static int
rmi_spi_read(struct rmi_phys_driver *pd, unsigned short address, char *valp)
{
	struct spi_device_instance_data *id =
		container_of(pd, struct spi_device_instance_data, rpd);

	char rxbuf[2];
	int retval;
	unsigned short addr = address;

	addr = ((addr & 0xff00) >> 8);
	address = ((address & 0x00ff) << 8);
	addr |= address;
	addr |= 0x80;		

	retval = spi_xfer(id, (u8 *)&addr, 2, rxbuf, 1);

	*valp = rxbuf[0];

	return retval;
}

static int
rmi_spi_read_multiple(struct rmi_phys_driver *pd, unsigned short address,
	char *valp, int size)
{
	struct spi_device_instance_data *id =
		container_of(pd, struct spi_device_instance_data, rpd);
	int retval;

	unsigned short addr = address;

	addr = ((addr & 0xff00) >> 8);
	address = ((address & 0x00ff) << 8);
	addr |= address;
	addr |= 0x80;		

	retval = spi_xfer(id, (u8 *)&addr, 2, valp, size);

	return retval;
}

/**
 * Write a single register through spi.
 * You can write multiple registers at once, but I made the functions for that
 * seperate for performance reasons.  Writing multiple requires allocation and
 * freeing.
 * \param[in] pd
 * \param[in] address The address at which to start the write.
 * \param[in] data The data to be written.
 * \return one upon success, something else upon error.
 */
static int
rmi_spi_write(struct rmi_phys_driver *pd, unsigned short address, char data)
{
	struct spi_device_instance_data *id =
		container_of(pd, struct spi_device_instance_data, rpd);
	unsigned char txbuf[4];
	int retval;

	txbuf[2]  = data;
	txbuf[1]  = address;
	txbuf[0]  = address>>8;

	retval = spi_xfer(id, txbuf, 3, NULL, 0);
	return retval ? 0 : 1;
}

/**
 * Write multiple registers.
 * \param[in] pd
 * \param[in] address The address at which to start the write.
 * \param[in] valp A pointer to a buffer containing the data to be written.
 * \param[in] size The number of bytes to write.
 * \return one upon success, something else upon error.
 */
static int
rmi_spi_write_multiple(struct rmi_phys_driver *pd, unsigned short address,
	char *valp, int size)
{
	struct spi_device_instance_data *id =
		container_of(pd, struct spi_device_instance_data, rpd);
	unsigned char txbuf[32];
	int retval;
	int i;

	txbuf[1]  = address;
	txbuf[0]  = address>>8;

	for (i = 0; i < size; i++)
		txbuf[i + 2] = valp[i];

	retval = spi_xfer(id, txbuf, size+2, NULL, 0);

	return retval ? 0 : 1;
}

static irqreturn_t spi_attn_isr(int irq, void *info)
{
	struct spi_device_instance_data *instance_data = info;
	disable_irq_nosync(instance_data->irq);
	if (instance_data->rpd.attention)
		instance_data->rpd.attention(&instance_data->rpd,
				instance_data->instance_no);
	return IRQ_HANDLED;
}

static int sensor_count;

static int __devinit rmi_spi_probe(struct spi_device *spi)
{
	struct spi_device_instance_data *instance_data;
	int retval;
	struct rmi_spi_platformdata *platformdata;
	struct rmi_sensordata *sensordata;
	int irqtype = 0;

	printk(KERN_INFO "Probing RMI4 SPI device\n");

	spi->bits_per_word = 8;

	spi->mode = SPI_MODE_3;

	retval = spi_setup(spi);
	if (retval < 0) {
		printk(KERN_ERR "%s: spi_setup failed with %d.", __func__,
				retval);
		return retval;
	}

	buf = kzalloc(SPI_BUFFER_SIZE, GFP_KERNEL);
	if (!buf) {
		printk(KERN_ERR "%s: Failed to allocate memory for spi
				buffer.", __func__);
		return -ENOMEM;
	}

	instance_data = kzalloc(sizeof(*instance_data), GFP_KERNEL);
	if (!instance_data) {
		printk(KERN_ERR "%s: Failer to allocate memory for instance
				data.", __func__);
		return -ENOMEM;
	}

	instance_data->byte_delay_us      = RMI_DEFAULT_BYTE_DELAY_US;
	instance_data->spidev             = spi;
	instance_data->rpd.name           = RMI4_SPI_DRIVER_NAME;
	instance_data->rpd.write          = rmi_spi_write;
	instance_data->rpd.read           = rmi_spi_read;
	instance_data->rpd.write_multiple = rmi_spi_write_multiple;
	instance_data->rpd.read_multiple  = rmi_spi_read_multiple;
	instance_data->rpd.module         = THIS_MODULE;
	
	instance_data->rpd.polling_required = true;

	platformdata = spi->dev.platform_data;
	if (platformdata == NULL) {
		printk(KERN_ERR "%s: CONFIGURATION ERROR - platform data
				is NULL.", __func__);
		return -EINVAL;
	}

	instance_data->platformdata = platformdata;
	sensordata = platformdata->sensordata;

	if (sensordata && sensordata->rmi_sensor_setup) {
		retval = sensordata->rmi_sensor_setup();
		if (retval) {
			printk(KERN_ERR "%s: sensor setup failed with
					code %d.", __func__, retval);
			kfree(instance_data);
			return retval;
		}
	}

	
	if (platformdata->chip == RMI_SUPPORT) {
		instance_data->instance_no = sensor_count;
		sensor_count++;

		dev_set_name(&spi->dev, "%s%d", RMI4_SPI_DEVICE_NAME,
						instance_data->instance_no);
		if (platformdata->irq) {
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
				dev_warn(&spi->dev, "%s: Invalid IRQ flags
					   in platform data.", __func__);
				retval = -ENXIO;
				goto error_exit;
			}
			instance_data->rpd.polling_required = false;
		} else {
			instance_data->rpd.polling_required = true;
			dev_info(&spi->dev, "%s: No IRQ info given.
				      Polling required.", __func__);
		}
	}

	
	if (instance_data)
		spi_set_drvdata(spi, instance_data);

	retval = rmi_register_sensor(&instance_data->rpd,
			platformdata->sensordata);
	if (retval) {
		printk(KERN_ERR "%s: sensor registration failed with code
				%d.", __func__, retval);
		goto error_exit;
	}

	if (instance_data->rpd.polling_required == false) {
		instance_data->irq = platformdata->irq;
		retval = request_irq(platformdata->irq, spi_attn_isr,
				irqtype, dev_name(&spi->dev), instance_data);
		if (retval) {
			dev_err(&spi->dev, "%s: failed to obtain IRQ %d.
					Result: %d.", __func__,
					platformdata->irq, retval);
			dev_info(&spi->dev, "%s: Reverting to polling.\n",
					__func__);
			instance_data->rpd.polling_required = true;
			instance_data->irq = 0;
		} else {
			dev_dbg(&spi->dev, "%s: got irq.\n", __func__);
			instance_data->rpd.irq = instance_data->irq;
		}
	}

	printk(KERN_INFO "%s: Successfully Registered %s.",
			__func__, instance_data->rpd.name);

	return 0;

error_exit:
	if (sensordata && sensordata->rmi_sensor_teardown)
		sensordata->rmi_sensor_teardown();
	if (instance_data->irq)
		free_irq(instance_data->irq, instance_data);
	kfree(instance_data);
	return retval;
}

static int rmi_spi_suspend(struct spi_device *spi, pm_message_t message)
{
	printk(KERN_INFO "%s: Suspending...", __func__);
	return 0;
}

static int rmi_spi_resume(struct spi_device *spi)
{
	printk(KERN_INFO "%s: Resuming...", __func__);
	return 0;
}

static int __devexit rmi_spi_remove(struct spi_device *spi)
{
	struct spi_device_instance_data *id = spi_get_drvdata(spi);

	printk(KERN_INFO "%s: RMI SPI device removed.", __func__);

	rmi_spi_suspend(spi, PMSG_SUSPEND);

	rmi_unregister_sensors(&id->rpd);

	if (id) {
		if (id->irq)
			free_irq(id->irq, id);
		kfree(id);
	}

	return 0;
}

static struct spi_driver rmi_spi_driver = {
	.driver = {
		.name  = RMI4_SPI_DRIVER_NAME,
		.bus   = &spi_bus_type,
		.owner = THIS_MODULE,
	},
	.probe    = rmi_spi_probe,
	.remove   = __devexit_p(rmi_spi_remove),
	.suspend  = rmi_spi_suspend,
	.resume   = rmi_spi_resume,
};

static int
rmi_spi_plat_probe(struct platform_device *dev)
{
	struct rmi_spi_platformdata *platform_data = dev->dev.platform_data;

	printk(KERN_INFO "%s: Platform driver probe.", __func__);

	if (!platform_data) {
		printk(KERN_ERR "A platform device must contain
				rmi_spi_platformdata\n");
		return -ENXIO;
	}

	return spi_register_driver(&rmi_spi_driver);
}

static int
rmi_spi_plat_remove(struct platform_device *dev)
{
	printk(KERN_INFO "%s: Platform driver removed.", __func__);
	spi_unregister_driver(&rmi_spi_driver);
	return 0;
}

static struct platform_driver rmi_spi_platform_driver = {
	.driver		= {
		.name	= RMI4_SPI_DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= rmi_spi_plat_probe,
	.remove		= __devexit_p(rmi_spi_plat_remove),
};

static int __init rmi_spi_init(void)
{
	int retval;

	printk(KERN_INFO "%s: RMI SPI physical layer initialization.",
							__func__);
	retval = spi_register_driver(&rmi_spi_driver);
	if (retval < 0) {
		printk(KERN_ERR "%s: Failed to register spi driver, code
				= %d.", __func__, retval);
		return retval;
	}
	printk(KERN_INFO "%s:    result = %d", __func__, retval);
	return retval;
}
module_init(rmi_spi_init);

static void __exit rmi_spi_exit(void)
{
	printk(KERN_INFO "%s: RMI SPI physical layer exits.", __func__);
	kfree(buf);
	buf = NULL;
	platform_driver_unregister(&rmi_spi_platform_driver);
}
module_exit(rmi_spi_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("RMI4 Driver SPI Physical Layer");
/** Standard driver module information - the license under which this module
 * is included in the kernel.
 */
MODULE_LICENSE("GPL");
