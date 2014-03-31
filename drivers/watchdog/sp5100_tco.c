/*
 *	sp5100_tco :	TCO timer driver for sp5100 chipsets
 *
 *	(c) Copyright 2009 Google Inc., All Rights Reserved.
 *
 *	Based on i8xx_tco.c:
 *	(c) Copyright 2000 kernel concepts <nils@kernelconcepts.de>, All Rights
 *	Reserved.
 *				http://www.kernelconcepts.de
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	See AMD Publication 43009 "AMD SB700/710/750 Register Reference Guide"
 */


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include "sp5100_tco.h"

#define TCO_VERSION "0.01"
#define TCO_MODULE_NAME "SP5100 TCO timer"
#define TCO_DRIVER_NAME   TCO_MODULE_NAME ", v" TCO_VERSION

static u32 tcobase_phys;
static void __iomem *tcobase;
static unsigned int pm_iobase;
static DEFINE_SPINLOCK(tco_lock);	
static unsigned long timer_alive;
static char tco_expect_close;
static struct pci_dev *sp5100_tco_pci;

static struct platform_device *sp5100_tco_platform_device;


#define WATCHDOG_HEARTBEAT 60	
static int heartbeat = WATCHDOG_HEARTBEAT;  
module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat, "Watchdog heartbeat in seconds. (default="
		 __MODULE_STRING(WATCHDOG_HEARTBEAT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started"
		" (default=" __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static void tco_timer_start(void)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&tco_lock, flags);
	val = readl(SP5100_WDT_CONTROL(tcobase));
	val |= SP5100_WDT_START_STOP_BIT;
	writel(val, SP5100_WDT_CONTROL(tcobase));
	spin_unlock_irqrestore(&tco_lock, flags);
}

static void tco_timer_stop(void)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&tco_lock, flags);
	val = readl(SP5100_WDT_CONTROL(tcobase));
	val &= ~SP5100_WDT_START_STOP_BIT;
	writel(val, SP5100_WDT_CONTROL(tcobase));
	spin_unlock_irqrestore(&tco_lock, flags);
}

static void tco_timer_keepalive(void)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&tco_lock, flags);
	val = readl(SP5100_WDT_CONTROL(tcobase));
	val |= SP5100_WDT_TRIGGER_BIT;
	writel(val, SP5100_WDT_CONTROL(tcobase));
	spin_unlock_irqrestore(&tco_lock, flags);
}

static int tco_timer_set_heartbeat(int t)
{
	unsigned long flags;

	if (t < 0 || t > 0xffff)
		return -EINVAL;

	
	spin_lock_irqsave(&tco_lock, flags);
	writel(t, SP5100_WDT_COUNT(tcobase));
	spin_unlock_irqrestore(&tco_lock, flags);

	heartbeat = t;
	return 0;
}


static int sp5100_tco_open(struct inode *inode, struct file *file)
{
	
	if (test_and_set_bit(0, &timer_alive))
		return -EBUSY;

	
	tco_timer_start();
	tco_timer_keepalive();
	return nonseekable_open(inode, file);
}

static int sp5100_tco_release(struct inode *inode, struct file *file)
{
	
	if (tco_expect_close == 42) {
		tco_timer_stop();
	} else {
		pr_crit("Unexpected close, not stopping watchdog!\n");
		tco_timer_keepalive();
	}
	clear_bit(0, &timer_alive);
	tco_expect_close = 0;
	return 0;
}

static ssize_t sp5100_tco_write(struct file *file, const char __user *data,
				size_t len, loff_t *ppos)
{
	
	if (len) {
		if (!nowayout) {
			size_t i;

			tco_expect_close = 0;

			for (i = 0; i != len; i++) {
				char c;
				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V')
					tco_expect_close = 42;
			}
		}

		
		tco_timer_keepalive();
	}
	return len;
}

static long sp5100_tco_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	int new_options, retval = -EINVAL;
	int new_heartbeat;
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	static const struct watchdog_info ident = {
		.options =		WDIOF_SETTIMEOUT |
					WDIOF_KEEPALIVEPING |
					WDIOF_MAGICCLOSE,
		.firmware_version =	0,
		.identity =		TCO_MODULE_NAME,
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(argp, &ident,
			sizeof(ident)) ? -EFAULT : 0;
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, p);
	case WDIOC_SETOPTIONS:
		if (get_user(new_options, p))
			return -EFAULT;
		if (new_options & WDIOS_DISABLECARD) {
			tco_timer_stop();
			retval = 0;
		}
		if (new_options & WDIOS_ENABLECARD) {
			tco_timer_start();
			tco_timer_keepalive();
			retval = 0;
		}
		return retval;
	case WDIOC_KEEPALIVE:
		tco_timer_keepalive();
		return 0;
	case WDIOC_SETTIMEOUT:
		if (get_user(new_heartbeat, p))
			return -EFAULT;
		if (tco_timer_set_heartbeat(new_heartbeat))
			return -EINVAL;
		tco_timer_keepalive();
		
	case WDIOC_GETTIMEOUT:
		return put_user(heartbeat, p);
	default:
		return -ENOTTY;
	}
}


static const struct file_operations sp5100_tco_fops = {
	.owner =		THIS_MODULE,
	.llseek =		no_llseek,
	.write =		sp5100_tco_write,
	.unlocked_ioctl =	sp5100_tco_ioctl,
	.open =			sp5100_tco_open,
	.release =		sp5100_tco_release,
};

static struct miscdevice sp5100_tco_miscdev = {
	.minor =	WATCHDOG_MINOR,
	.name =		"watchdog",
	.fops =		&sp5100_tco_fops,
};

static DEFINE_PCI_DEVICE_TABLE(sp5100_tco_pci_tbl) = {
	{ PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_SBX00_SMBUS, PCI_ANY_ID,
	  PCI_ANY_ID, },
	{ 0, },			
};
MODULE_DEVICE_TABLE(pci, sp5100_tco_pci_tbl);


static unsigned char __devinit sp5100_tco_setupdevice(void)
{
	struct pci_dev *dev = NULL;
	u32 val;

	
	for_each_pci_dev(dev) {
		if (pci_match_id(sp5100_tco_pci_tbl, dev) != NULL) {
			sp5100_tco_pci = dev;
			break;
		}
	}

	if (!sp5100_tco_pci)
		return 0;

	
	pm_iobase = SP5100_IO_PM_INDEX_REG;
	if (!request_region(pm_iobase, SP5100_PM_IOPORTS_SIZE, "SP5100 TCO")) {
		pr_err("I/O address 0x%04x already in use\n", pm_iobase);
		goto exit;
	}

	
	outb(SP5100_PM_WATCHDOG_BASE3, SP5100_IO_PM_INDEX_REG);
	val = inb(SP5100_IO_PM_DATA_REG);
	outb(SP5100_PM_WATCHDOG_BASE2, SP5100_IO_PM_INDEX_REG);
	val = val << 8 | inb(SP5100_IO_PM_DATA_REG);
	outb(SP5100_PM_WATCHDOG_BASE1, SP5100_IO_PM_INDEX_REG);
	val = val << 8 | inb(SP5100_IO_PM_DATA_REG);
	outb(SP5100_PM_WATCHDOG_BASE0, SP5100_IO_PM_INDEX_REG);
	
	val = val << 8 | (inb(SP5100_IO_PM_DATA_REG) & 0xf8);

	if (!request_mem_region_exclusive(val, SP5100_WDT_MEM_MAP_SIZE,
								"SP5100 TCO")) {
		pr_err("mmio address 0x%04x already in use\n", val);
		goto unreg_region;
	}
	tcobase_phys = val;

	tcobase = ioremap(val, SP5100_WDT_MEM_MAP_SIZE);
	if (tcobase == 0) {
		pr_err("failed to get tcobase address\n");
		goto unreg_mem_region;
	}

	
	pci_read_config_dword(sp5100_tco_pci,
			      SP5100_PCI_WATCHDOG_MISC_REG,
			      &val);

	val |= SP5100_PCI_WATCHDOG_DECODE_EN;

	pci_write_config_dword(sp5100_tco_pci,
			       SP5100_PCI_WATCHDOG_MISC_REG,
			       val);

	
	outb(SP5100_PM_WATCHDOG_CONTROL, SP5100_IO_PM_INDEX_REG);
	val = inb(SP5100_IO_PM_DATA_REG);
	val |= SP5100_PM_WATCHDOG_SECOND_RES;
	val &= ~SP5100_PM_WATCHDOG_DISABLE;
	outb(val, SP5100_IO_PM_DATA_REG);

	
	val = readl(SP5100_WDT_CONTROL(tcobase));
	val &= ~SP5100_PM_WATCHDOG_ACTION_RESET;
	writel(val, SP5100_WDT_CONTROL(tcobase));

	
	tco_timer_set_heartbeat(heartbeat);

	tco_timer_stop();

	
	return 1;

unreg_mem_region:
	release_mem_region(tcobase_phys, SP5100_WDT_MEM_MAP_SIZE);
unreg_region:
	release_region(pm_iobase, SP5100_PM_IOPORTS_SIZE);
exit:
	return 0;
}

static int __devinit sp5100_tco_init(struct platform_device *dev)
{
	int ret;
	u32 val;

	if (!sp5100_tco_setupdevice())
		return -ENODEV;

	
	pr_info("Watchdog reboot %sdetected\n",
		readl(SP5100_WDT_CONTROL(tcobase)) & SP5100_PM_WATCHDOG_FIRED ?
		"" : "not ");

	
	val = readl(SP5100_WDT_CONTROL(tcobase));
	val &= ~SP5100_PM_WATCHDOG_FIRED;
	writel(val, SP5100_WDT_CONTROL(tcobase));

	if (tco_timer_set_heartbeat(heartbeat)) {
		heartbeat = WATCHDOG_HEARTBEAT;
		tco_timer_set_heartbeat(heartbeat);
	}

	ret = misc_register(&sp5100_tco_miscdev);
	if (ret != 0) {
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
		       WATCHDOG_MINOR, ret);
		goto exit;
	}

	clear_bit(0, &timer_alive);

	pr_info("initialized (0x%p). heartbeat=%d sec (nowayout=%d)\n",
		tcobase, heartbeat, nowayout);

	return 0;

exit:
	iounmap(tcobase);
	release_mem_region(tcobase_phys, SP5100_WDT_MEM_MAP_SIZE);
	release_region(pm_iobase, SP5100_PM_IOPORTS_SIZE);
	return ret;
}

static void __devexit sp5100_tco_cleanup(void)
{
	
	if (!nowayout)
		tco_timer_stop();

	
	misc_deregister(&sp5100_tco_miscdev);
	iounmap(tcobase);
	release_mem_region(tcobase_phys, SP5100_WDT_MEM_MAP_SIZE);
	release_region(pm_iobase, SP5100_PM_IOPORTS_SIZE);
}

static int __devexit sp5100_tco_remove(struct platform_device *dev)
{
	if (tcobase)
		sp5100_tco_cleanup();
	return 0;
}

static void sp5100_tco_shutdown(struct platform_device *dev)
{
	tco_timer_stop();
}

static struct platform_driver sp5100_tco_driver = {
	.probe		= sp5100_tco_init,
	.remove		= __devexit_p(sp5100_tco_remove),
	.shutdown	= sp5100_tco_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= TCO_MODULE_NAME,
	},
};

static int __init sp5100_tco_init_module(void)
{
	int err;

	pr_info("SP5100 TCO WatchDog Timer Driver v%s\n", TCO_VERSION);

	err = platform_driver_register(&sp5100_tco_driver);
	if (err)
		return err;

	sp5100_tco_platform_device = platform_device_register_simple(
					TCO_MODULE_NAME, -1, NULL, 0);
	if (IS_ERR(sp5100_tco_platform_device)) {
		err = PTR_ERR(sp5100_tco_platform_device);
		goto unreg_platform_driver;
	}

	return 0;

unreg_platform_driver:
	platform_driver_unregister(&sp5100_tco_driver);
	return err;
}

static void __exit sp5100_tco_cleanup_module(void)
{
	platform_device_unregister(sp5100_tco_platform_device);
	platform_driver_unregister(&sp5100_tco_driver);
	pr_info("SP5100 TCO Watchdog Module Unloaded\n");
}

module_init(sp5100_tco_init_module);
module_exit(sp5100_tco_cleanup_module);

MODULE_AUTHOR("Priyanka Gupta");
MODULE_DESCRIPTION("TCO timer driver for SP5100 chipset");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
