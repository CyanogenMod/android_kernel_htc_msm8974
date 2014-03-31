/*
 *	nv_tco 0.01:	TCO timer driver for NV chipsets
 *
 *	(c) Copyright 2005 Google Inc., All Rights Reserved.
 *
 *	Based off i8xx_tco.c:
 *	(c) Copyright 2000 kernel concepts <nils@kernelconcepts.de>, All Rights
 *	Reserved.
 *				http://www.kernelconcepts.de
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	TCO timer driver for NV chipsets
 *	based on softdog.c by Alan Cox <alan@redhat.com>
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
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include "nv_tco.h"

#define TCO_VERSION "0.01"
#define TCO_MODULE_NAME "NV_TCO"
#define TCO_DRIVER_NAME   TCO_MODULE_NAME ", v" TCO_VERSION

static unsigned int tcobase;
static DEFINE_SPINLOCK(tco_lock);	
static unsigned long timer_alive;
static char tco_expect_close;
static struct pci_dev *tco_pci;

static struct platform_device *nv_tco_platform_device;

#define WATCHDOG_HEARTBEAT 30	
static int heartbeat = WATCHDOG_HEARTBEAT;  
module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat, "Watchdog heartbeat in seconds. (2<heartbeat<39, "
			    "default=" __MODULE_STRING(WATCHDOG_HEARTBEAT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started"
		" (default=" __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static inline unsigned char seconds_to_ticks(int seconds)
{
	return (seconds * 10) / 6;
}

static void tco_timer_start(void)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&tco_lock, flags);
	val = inl(TCO_CNT(tcobase));
	val &= ~TCO_CNT_TCOHALT;
	outl(val, TCO_CNT(tcobase));
	spin_unlock_irqrestore(&tco_lock, flags);
}

static void tco_timer_stop(void)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&tco_lock, flags);
	val = inl(TCO_CNT(tcobase));
	val |= TCO_CNT_TCOHALT;
	outl(val, TCO_CNT(tcobase));
	spin_unlock_irqrestore(&tco_lock, flags);
}

static void tco_timer_keepalive(void)
{
	unsigned long flags;

	spin_lock_irqsave(&tco_lock, flags);
	outb(0x01, TCO_RLD(tcobase));
	spin_unlock_irqrestore(&tco_lock, flags);
}

static int tco_timer_set_heartbeat(int t)
{
	int ret = 0;
	unsigned char tmrval;
	unsigned long flags;
	u8 val;

	if (t < 0 || t > 0x3f)
		return -EINVAL;
	tmrval = seconds_to_ticks(t);

	
	if (tmrval > 0x3f || tmrval < 0x04)
		return -EINVAL;

	
	spin_lock_irqsave(&tco_lock, flags);
	val = inb(TCO_TMR(tcobase));
	val &= 0xc0;
	val |= tmrval;
	outb(val, TCO_TMR(tcobase));
	val = inb(TCO_TMR(tcobase));

	if ((val & 0x3f) != tmrval)
		ret = -EINVAL;
	spin_unlock_irqrestore(&tco_lock, flags);

	if (ret)
		return ret;

	heartbeat = t;
	return 0;
}


static int nv_tco_open(struct inode *inode, struct file *file)
{
	
	if (test_and_set_bit(0, &timer_alive))
		return -EBUSY;

	
	tco_timer_keepalive();
	tco_timer_start();
	return nonseekable_open(inode, file);
}

static int nv_tco_release(struct inode *inode, struct file *file)
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

static ssize_t nv_tco_write(struct file *file, const char __user *data,
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

static long nv_tco_ioctl(struct file *file, unsigned int cmd,
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
		return copy_to_user(argp, &ident, sizeof(ident)) ? -EFAULT : 0;
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
			tco_timer_keepalive();
			tco_timer_start();
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


static const struct file_operations nv_tco_fops = {
	.owner =		THIS_MODULE,
	.llseek =		no_llseek,
	.write =		nv_tco_write,
	.unlocked_ioctl =	nv_tco_ioctl,
	.open =			nv_tco_open,
	.release =		nv_tco_release,
};

static struct miscdevice nv_tco_miscdev = {
	.minor =	WATCHDOG_MINOR,
	.name =		"watchdog",
	.fops =		&nv_tco_fops,
};

static DEFINE_PCI_DEVICE_TABLE(tco_pci_tbl) = {
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_NFORCE_MCP51_SMBUS,
	  PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_NVIDIA, PCI_DEVICE_ID_NVIDIA_NFORCE_MCP55_SMBUS,
	  PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, },			
};
MODULE_DEVICE_TABLE(pci, tco_pci_tbl);


static unsigned char __devinit nv_tco_getdevice(void)
{
	struct pci_dev *dev = NULL;
	u32 val;

	
	for_each_pci_dev(dev) {
		if (pci_match_id(tco_pci_tbl, dev) != NULL) {
			tco_pci = dev;
			break;
		}
	}

	if (!tco_pci)
		return 0;

	
	pci_read_config_dword(tco_pci, 0x64, &val);
	val &= 0xffff;
	if (val == 0x0001 || val == 0x0000) {
		
		pr_err("failed to get tcobase address\n");
		return 0;
	}
	val &= 0xff00;
	tcobase = val + 0x40;

	if (!request_region(tcobase, 0x10, "NV TCO")) {
		pr_err("I/O address 0x%04x already in use\n", tcobase);
		return 0;
	}

	
	tco_timer_set_heartbeat(30);

	tco_timer_keepalive();
	tco_timer_stop();

	
	if (!request_region(MCP51_SMI_EN(tcobase), 4, "NV TCO")) {
		pr_err("I/O address 0x%04x already in use\n",
		       MCP51_SMI_EN(tcobase));
		goto out;
	}
	val = inl(MCP51_SMI_EN(tcobase));
	val &= ~MCP51_SMI_EN_TCO;
	outl(val, MCP51_SMI_EN(tcobase));
	val = inl(MCP51_SMI_EN(tcobase));
	release_region(MCP51_SMI_EN(tcobase), 4);
	if (val & MCP51_SMI_EN_TCO) {
		pr_err("Could not disable SMI caused by TCO\n");
		goto out;
	}

	
	pci_read_config_dword(tco_pci, MCP51_SMBUS_SETUP_B, &val);
	val |= MCP51_SMBUS_SETUP_B_TCO_REBOOT;
	pci_write_config_dword(tco_pci, MCP51_SMBUS_SETUP_B, val);
	pci_read_config_dword(tco_pci, MCP51_SMBUS_SETUP_B, &val);
	if (!(val & MCP51_SMBUS_SETUP_B_TCO_REBOOT)) {
		pr_err("failed to reset NO_REBOOT flag, reboot disabled by hardware\n");
		goto out;
	}

	return 1;
out:
	release_region(tcobase, 0x10);
	return 0;
}

static int __devinit nv_tco_init(struct platform_device *dev)
{
	int ret;

	
	if (!nv_tco_getdevice())
		return -ENODEV;

	
	pr_info("Watchdog reboot %sdetected\n",
		inl(TCO_STS(tcobase)) & TCO_STS_TCO2TO_STS ? "" : "not ");

	
	outl(TCO_STS_RESET, TCO_STS(tcobase));

	if (tco_timer_set_heartbeat(heartbeat)) {
		heartbeat = WATCHDOG_HEARTBEAT;
		tco_timer_set_heartbeat(heartbeat);
		pr_info("heartbeat value must be 2<heartbeat<39, using %d\n",
			heartbeat);
	}

	ret = misc_register(&nv_tco_miscdev);
	if (ret != 0) {
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
		       WATCHDOG_MINOR, ret);
		goto unreg_region;
	}

	clear_bit(0, &timer_alive);

	tco_timer_stop();

	pr_info("initialized (0x%04x). heartbeat=%d sec (nowayout=%d)\n",
		tcobase, heartbeat, nowayout);

	return 0;

unreg_region:
	release_region(tcobase, 0x10);
	return ret;
}

static void __devexit nv_tco_cleanup(void)
{
	u32 val;

	
	if (!nowayout)
		tco_timer_stop();

	
	pci_read_config_dword(tco_pci, MCP51_SMBUS_SETUP_B, &val);
	val &= ~MCP51_SMBUS_SETUP_B_TCO_REBOOT;
	pci_write_config_dword(tco_pci, MCP51_SMBUS_SETUP_B, val);
	pci_read_config_dword(tco_pci, MCP51_SMBUS_SETUP_B, &val);
	if (val & MCP51_SMBUS_SETUP_B_TCO_REBOOT) {
		pr_crit("Couldn't unset REBOOT bit.  Machine may soon reset\n");
	}

	
	misc_deregister(&nv_tco_miscdev);
	release_region(tcobase, 0x10);
}

static int __devexit nv_tco_remove(struct platform_device *dev)
{
	if (tcobase)
		nv_tco_cleanup();

	return 0;
}

static void nv_tco_shutdown(struct platform_device *dev)
{
	u32 val;

	tco_timer_stop();

	pci_read_config_dword(tco_pci, MCP51_SMBUS_SETUP_B, &val);
	val &= ~MCP51_SMBUS_SETUP_B_TCO_REBOOT;
	pci_write_config_dword(tco_pci, MCP51_SMBUS_SETUP_B, val);
}

static struct platform_driver nv_tco_driver = {
	.probe		= nv_tco_init,
	.remove		= __devexit_p(nv_tco_remove),
	.shutdown	= nv_tco_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= TCO_MODULE_NAME,
	},
};

static int __init nv_tco_init_module(void)
{
	int err;

	pr_info("NV TCO WatchDog Timer Driver v%s\n", TCO_VERSION);

	err = platform_driver_register(&nv_tco_driver);
	if (err)
		return err;

	nv_tco_platform_device = platform_device_register_simple(
					TCO_MODULE_NAME, -1, NULL, 0);
	if (IS_ERR(nv_tco_platform_device)) {
		err = PTR_ERR(nv_tco_platform_device);
		goto unreg_platform_driver;
	}

	return 0;

unreg_platform_driver:
	platform_driver_unregister(&nv_tco_driver);
	return err;
}

static void __exit nv_tco_cleanup_module(void)
{
	platform_device_unregister(nv_tco_platform_device);
	platform_driver_unregister(&nv_tco_driver);
	pr_info("NV TCO Watchdog Module Unloaded\n");
}

module_init(nv_tco_init_module);
module_exit(nv_tco_cleanup_module);

MODULE_AUTHOR("Mike Waychison");
MODULE_DESCRIPTION("TCO timer driver for NV chipsets");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
