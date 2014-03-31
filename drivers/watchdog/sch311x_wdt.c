/*
 *	sch311x_wdt.c - Driver for the SCH311x Super-I/O chips
 *			integrated watchdog.
 *
 *	(c) Copyright 2008 Wim Van Sebroeck <wim@iguana.be>.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Neither Wim Van Sebroeck nor Iguana vzw. admit liability nor
 *	provide warranty for any of this software. This material is
 *	provided "AS-IS" and at no charge.
 */


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>		
#include <linux/moduleparam.h>		
#include <linux/types.h>		
#include <linux/errno.h>		
#include <linux/kernel.h>		
#include <linux/miscdevice.h>		
#include <linux/watchdog.h>		
#include <linux/init.h>			
#include <linux/fs.h>			
#include <linux/platform_device.h>	
#include <linux/ioport.h>		
#include <linux/spinlock.h>		
#include <linux/uaccess.h>		
#include <linux/io.h>			

#define DRV_NAME	"sch311x_wdt"

#define RESGEN			0x1d
#define GP60			0x47
#define WDT_TIME_OUT		0x65
#define WDT_VAL			0x66
#define WDT_CFG			0x67
#define WDT_CTRL		0x68

static unsigned long sch311x_wdt_is_open;
static char sch311x_wdt_expect_close;
static struct platform_device *sch311x_wdt_pdev;

static int sch311x_ioports[] = { 0x2e, 0x4e, 0x162e, 0x164e, 0x00 };

static struct {	
	
	unsigned short runtime_reg;
	
	int boot_status;
	
	spinlock_t io_lock;
} sch311x_wdt_data;

static unsigned short force_id;
module_param(force_id, ushort, 0);
MODULE_PARM_DESC(force_id, "Override the detected device ID");

static unsigned short therm_trip;
module_param(therm_trip, ushort, 0);
MODULE_PARM_DESC(therm_trip, "Should a ThermTrip trigger the reset generator");

#define WATCHDOG_TIMEOUT 60		
static int timeout = WATCHDOG_TIMEOUT;	
module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout,
	"Watchdog timeout in seconds. 1<= timeout <=15300, default="
		__MODULE_STRING(WATCHDOG_TIMEOUT) ".");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
	"Watchdog cannot be stopped once started (default="
		__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");


static inline void sch311x_sio_enter(int sio_config_port)
{
	outb(0x55, sio_config_port);
}

static inline void sch311x_sio_exit(int sio_config_port)
{
	outb(0xaa, sio_config_port);
}

static inline int sch311x_sio_inb(int sio_config_port, int reg)
{
	outb(reg, sio_config_port);
	return inb(sio_config_port + 1);
}

static inline void sch311x_sio_outb(int sio_config_port, int reg, int val)
{
	outb(reg, sio_config_port);
	outb(val, sio_config_port + 1);
}


static void sch311x_wdt_set_timeout(int t)
{
	unsigned char timeout_unit = 0x80;

	
	if (t > 255) {
		timeout_unit = 0;
		t /= 60;
	}

	outb(timeout_unit, sch311x_wdt_data.runtime_reg + WDT_TIME_OUT);

	outb(t, sch311x_wdt_data.runtime_reg + WDT_VAL);
}

static void sch311x_wdt_start(void)
{
	spin_lock(&sch311x_wdt_data.io_lock);

	
	sch311x_wdt_set_timeout(timeout);
	
	outb(0x0e, sch311x_wdt_data.runtime_reg + GP60);

	spin_unlock(&sch311x_wdt_data.io_lock);

}

static void sch311x_wdt_stop(void)
{
	spin_lock(&sch311x_wdt_data.io_lock);

	
	outb(0x01, sch311x_wdt_data.runtime_reg + GP60);
	
	sch311x_wdt_set_timeout(0);

	spin_unlock(&sch311x_wdt_data.io_lock);
}

static void sch311x_wdt_keepalive(void)
{
	spin_lock(&sch311x_wdt_data.io_lock);
	sch311x_wdt_set_timeout(timeout);
	spin_unlock(&sch311x_wdt_data.io_lock);
}

static int sch311x_wdt_set_heartbeat(int t)
{
	if (t < 1 || t > (255*60))
		return -EINVAL;

	if (t > 255)
		t = (((t - 1) / 60) + 1) * 60;

	timeout = t;
	return 0;
}

static void sch311x_wdt_get_status(int *status)
{
	unsigned char new_status;

	*status = 0;

	spin_lock(&sch311x_wdt_data.io_lock);

	new_status = inb(sch311x_wdt_data.runtime_reg + WDT_CTRL);
	if (new_status & 0x01)
		*status |= WDIOF_CARDRESET;

	spin_unlock(&sch311x_wdt_data.io_lock);
}


static ssize_t sch311x_wdt_write(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	if (count) {
		if (!nowayout) {
			size_t i;

			sch311x_wdt_expect_close = 0;

			for (i = 0; i != count; i++) {
				char c;
				if (get_user(c, buf + i))
					return -EFAULT;
				if (c == 'V')
					sch311x_wdt_expect_close = 42;
			}
		}
		sch311x_wdt_keepalive();
	}
	return count;
}

static long sch311x_wdt_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	int status;
	int new_timeout;
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	static const struct watchdog_info ident = {
		.options		= WDIOF_KEEPALIVEPING |
					  WDIOF_SETTIMEOUT |
					  WDIOF_MAGICCLOSE,
		.firmware_version	= 1,
		.identity		= DRV_NAME,
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		if (copy_to_user(argp, &ident, sizeof(ident)))
			return -EFAULT;
		break;

	case WDIOC_GETSTATUS:
	{
		sch311x_wdt_get_status(&status);
		return put_user(status, p);
	}
	case WDIOC_GETBOOTSTATUS:
		return put_user(sch311x_wdt_data.boot_status, p);

	case WDIOC_SETOPTIONS:
	{
		int options, retval = -EINVAL;

		if (get_user(options, p))
			return -EFAULT;
		if (options & WDIOS_DISABLECARD) {
			sch311x_wdt_stop();
			retval = 0;
		}
		if (options & WDIOS_ENABLECARD) {
			sch311x_wdt_start();
			retval = 0;
		}
		return retval;
	}
	case WDIOC_KEEPALIVE:
		sch311x_wdt_keepalive();
		break;

	case WDIOC_SETTIMEOUT:
		if (get_user(new_timeout, p))
			return -EFAULT;
		if (sch311x_wdt_set_heartbeat(new_timeout))
			return -EINVAL;
		sch311x_wdt_keepalive();
		
	case WDIOC_GETTIMEOUT:
		return put_user(timeout, p);
	default:
		return -ENOTTY;
	}
	return 0;
}

static int sch311x_wdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &sch311x_wdt_is_open))
		return -EBUSY;
	sch311x_wdt_start();
	return nonseekable_open(inode, file);
}

static int sch311x_wdt_close(struct inode *inode, struct file *file)
{
	if (sch311x_wdt_expect_close == 42) {
		sch311x_wdt_stop();
	} else {
		pr_crit("Unexpected close, not stopping watchdog!\n");
		sch311x_wdt_keepalive();
	}
	clear_bit(0, &sch311x_wdt_is_open);
	sch311x_wdt_expect_close = 0;
	return 0;
}


static const struct file_operations sch311x_wdt_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= sch311x_wdt_write,
	.unlocked_ioctl	= sch311x_wdt_ioctl,
	.open		= sch311x_wdt_open,
	.release	= sch311x_wdt_close,
};

static struct miscdevice sch311x_wdt_miscdev = {
	.minor	= WATCHDOG_MINOR,
	.name	= "watchdog",
	.fops	= &sch311x_wdt_fops,
};


static int __devinit sch311x_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	unsigned char val;
	int err;

	spin_lock_init(&sch311x_wdt_data.io_lock);

	if (!request_region(sch311x_wdt_data.runtime_reg + RESGEN, 1,
								DRV_NAME)) {
		dev_err(dev, "Failed to request region 0x%04x-0x%04x.\n",
			sch311x_wdt_data.runtime_reg + RESGEN,
			sch311x_wdt_data.runtime_reg + RESGEN);
		err = -EBUSY;
		goto exit;
	}

	if (!request_region(sch311x_wdt_data.runtime_reg + GP60, 1, DRV_NAME)) {
		dev_err(dev, "Failed to request region 0x%04x-0x%04x.\n",
			sch311x_wdt_data.runtime_reg + GP60,
			sch311x_wdt_data.runtime_reg + GP60);
		err = -EBUSY;
		goto exit_release_region;
	}

	if (!request_region(sch311x_wdt_data.runtime_reg + WDT_TIME_OUT, 4,
								DRV_NAME)) {
		dev_err(dev, "Failed to request region 0x%04x-0x%04x.\n",
			sch311x_wdt_data.runtime_reg + WDT_TIME_OUT,
			sch311x_wdt_data.runtime_reg + WDT_CTRL);
		err = -EBUSY;
		goto exit_release_region2;
	}

	
	sch311x_wdt_stop();

	
	outb(0, sch311x_wdt_data.runtime_reg + WDT_CFG);

	if (sch311x_wdt_set_heartbeat(timeout)) {
		sch311x_wdt_set_heartbeat(WATCHDOG_TIMEOUT);
		dev_info(dev, "timeout value must be 1<=x<=15300, using %d\n",
			timeout);
	}

	
	sch311x_wdt_get_status(&sch311x_wdt_data.boot_status);

	
	outb(0, sch311x_wdt_data.runtime_reg + RESGEN);
	val = therm_trip ? 0x06 : 0x04;
	outb(val, sch311x_wdt_data.runtime_reg + RESGEN);

	sch311x_wdt_miscdev.parent = dev;

	err = misc_register(&sch311x_wdt_miscdev);
	if (err != 0) {
		dev_err(dev, "cannot register miscdev on minor=%d (err=%d)\n",
							WATCHDOG_MINOR, err);
		goto exit_release_region3;
	}

	dev_info(dev,
		"SMSC SCH311x WDT initialized. timeout=%d sec (nowayout=%d)\n",
		timeout, nowayout);

	return 0;

exit_release_region3:
	release_region(sch311x_wdt_data.runtime_reg + WDT_TIME_OUT, 4);
exit_release_region2:
	release_region(sch311x_wdt_data.runtime_reg + GP60, 1);
exit_release_region:
	release_region(sch311x_wdt_data.runtime_reg + RESGEN, 1);
	sch311x_wdt_data.runtime_reg = 0;
exit:
	return err;
}

static int __devexit sch311x_wdt_remove(struct platform_device *pdev)
{
	
	if (!nowayout)
		sch311x_wdt_stop();

	
	misc_deregister(&sch311x_wdt_miscdev);
	release_region(sch311x_wdt_data.runtime_reg + WDT_TIME_OUT, 4);
	release_region(sch311x_wdt_data.runtime_reg + GP60, 1);
	release_region(sch311x_wdt_data.runtime_reg + RESGEN, 1);
	sch311x_wdt_data.runtime_reg = 0;
	return 0;
}

static void sch311x_wdt_shutdown(struct platform_device *dev)
{
	
	sch311x_wdt_stop();
}

static struct platform_driver sch311x_wdt_driver = {
	.probe		= sch311x_wdt_probe,
	.remove		= __devexit_p(sch311x_wdt_remove),
	.shutdown	= sch311x_wdt_shutdown,
	.driver		= {
		.owner = THIS_MODULE,
		.name = DRV_NAME,
	},
};

static int __init sch311x_detect(int sio_config_port, unsigned short *addr)
{
	int err = 0, reg;
	unsigned short base_addr;
	unsigned char dev_id;

	sch311x_sio_enter(sio_config_port);

	reg = force_id ? force_id : sch311x_sio_inb(sio_config_port, 0x20);
	if (!(reg == 0x7c || reg == 0x7d || reg == 0x7f)) {
		err = -ENODEV;
		goto exit;
	}
	dev_id = reg == 0x7c ? 2 : reg == 0x7d ? 4 : 6;

	
	sch311x_sio_outb(sio_config_port, 0x07, 0x0a);

	
	if ((sch311x_sio_inb(sio_config_port, 0x30) & 0x01) == 0)
		pr_info("Seems that LDN 0x0a is not active...\n");

	
	base_addr = (sch311x_sio_inb(sio_config_port, 0x60) << 8) |
			   sch311x_sio_inb(sio_config_port, 0x61);
	if (!base_addr) {
		pr_err("Base address not set\n");
		err = -ENODEV;
		goto exit;
	}
	*addr = base_addr;

	pr_info("Found an SMSC SCH311%d chip at 0x%04x\n", dev_id, base_addr);

exit:
	sch311x_sio_exit(sio_config_port);
	return err;
}

static int __init sch311x_wdt_init(void)
{
	int err, i, found = 0;
	unsigned short addr = 0;

	for (i = 0; !found && sch311x_ioports[i]; i++)
		if (sch311x_detect(sch311x_ioports[i], &addr) == 0)
			found++;

	if (!found)
		return -ENODEV;

	sch311x_wdt_data.runtime_reg = addr;

	err = platform_driver_register(&sch311x_wdt_driver);
	if (err)
		return err;

	sch311x_wdt_pdev = platform_device_register_simple(DRV_NAME, addr,
								NULL, 0);

	if (IS_ERR(sch311x_wdt_pdev)) {
		err = PTR_ERR(sch311x_wdt_pdev);
		goto unreg_platform_driver;
	}

	return 0;

unreg_platform_driver:
	platform_driver_unregister(&sch311x_wdt_driver);
	return err;
}

static void __exit sch311x_wdt_exit(void)
{
	platform_device_unregister(sch311x_wdt_pdev);
	platform_driver_unregister(&sch311x_wdt_driver);
}

module_init(sch311x_wdt_init);
module_exit(sch311x_wdt_exit);

MODULE_AUTHOR("Wim Van Sebroeck <wim@iguana.be>");
MODULE_DESCRIPTION("SMSC SCH311x WatchDog Timer Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);

