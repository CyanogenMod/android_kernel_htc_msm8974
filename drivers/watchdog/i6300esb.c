/*
 *	i6300esb:	Watchdog timer driver for Intel 6300ESB chipset
 *
 *	(c) Copyright 2004 Google Inc.
 *	(c) Copyright 2005 David Härdeman <david@2gen.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	based on i810-tco.c which is in turn based on softdog.c
 *
 *	The timer is implemented in the following I/O controller hubs:
 *	(See the intel documentation on http://developer.intel.com.)
 *	6300ESB chip : document number 300641-004
 *
 *  2004YYZZ Ross Biro
 *	Initial version 0.01
 *  2004YYZZ Ross Biro
 *	Version 0.02
 *  20050210 David Härdeman <david@2gen.com>
 *	Ported driver to kernel 2.6
 */


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define ESB_VERSION "0.05"
#define ESB_MODULE_NAME "i6300ESB timer"
#define ESB_DRIVER_NAME ESB_MODULE_NAME ", v" ESB_VERSION

#define ESB_CONFIG_REG  0x60            
#define ESB_LOCK_REG    0x68            

#define ESB_TIMER1_REG (BASEADDR + 0x00)
#define ESB_TIMER2_REG (BASEADDR + 0x04)
#define ESB_GINTSR_REG (BASEADDR + 0x08)
#define ESB_RELOAD_REG (BASEADDR + 0x0c)

#define ESB_WDT_FUNC    (0x01 << 2)   
#define ESB_WDT_ENABLE  (0x01 << 1)   
#define ESB_WDT_LOCK    (0x01 << 0)   

#define ESB_WDT_REBOOT  (0x01 << 5)   
#define ESB_WDT_FREQ    (0x01 << 2)   
#define ESB_WDT_INTTYPE (0x03 << 0)   

#define ESB_WDT_TIMEOUT (0x01 << 9)    
#define ESB_WDT_RELOAD  (0x01 << 8)    

#define ESB_UNLOCK1     0x80            
#define ESB_UNLOCK2     0x86            

static void __iomem *BASEADDR;
static DEFINE_SPINLOCK(esb_lock); 
static unsigned long timer_alive;
static struct pci_dev *esb_pci;
static unsigned short triggered; 
static char esb_expect_close;

static int cards_found;

#define WATCHDOG_HEARTBEAT 30
static int heartbeat = WATCHDOG_HEARTBEAT;  
module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat,
		"Watchdog heartbeat in seconds. (1<heartbeat<2046, default="
				__MODULE_STRING(WATCHDOG_HEARTBEAT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");


/*
 * Prepare for reloading the timer by unlocking the proper registers.
 * This is performed by first writing 0x80 followed by 0x86 to the
 * reload register. After this the appropriate registers can be written
 * to once before they need to be unlocked again.
 */
static inline void esb_unlock_registers(void)
{
	writew(ESB_UNLOCK1, ESB_RELOAD_REG);
	writew(ESB_UNLOCK2, ESB_RELOAD_REG);
}

static int esb_timer_start(void)
{
	u8 val;

	spin_lock(&esb_lock);
	esb_unlock_registers();
	writew(ESB_WDT_RELOAD, ESB_RELOAD_REG);
	
	val = ESB_WDT_ENABLE | (nowayout ? ESB_WDT_LOCK : 0x00);
	pci_write_config_byte(esb_pci, ESB_LOCK_REG, val);
	spin_unlock(&esb_lock);
	return 0;
}

static int esb_timer_stop(void)
{
	u8 val;

	spin_lock(&esb_lock);
	
	esb_unlock_registers();
	writew(ESB_WDT_RELOAD, ESB_RELOAD_REG);
	
	pci_write_config_byte(esb_pci, ESB_LOCK_REG, 0x0);
	pci_read_config_byte(esb_pci, ESB_LOCK_REG, &val);
	spin_unlock(&esb_lock);

	
	return val & ESB_WDT_ENABLE;
}

static void esb_timer_keepalive(void)
{
	spin_lock(&esb_lock);
	esb_unlock_registers();
	writew(ESB_WDT_RELOAD, ESB_RELOAD_REG);
	
	spin_unlock(&esb_lock);
}

static int esb_timer_set_heartbeat(int time)
{
	u32 val;

	if (time < 0x1 || time > (2 * 0x03ff))
		return -EINVAL;

	spin_lock(&esb_lock);

	val = time << 9;

	
	esb_unlock_registers();
	writel(val, ESB_TIMER1_REG);

	
	esb_unlock_registers();
	writel(val, ESB_TIMER2_REG);

	
	esb_unlock_registers();
	writew(ESB_WDT_RELOAD, ESB_RELOAD_REG);

	

	
	heartbeat = time;
	spin_unlock(&esb_lock);
	return 0;
}


static int esb_open(struct inode *inode, struct file *file)
{
	
	if (test_and_set_bit(0, &timer_alive))
		return -EBUSY;

	
	esb_timer_start();

	return nonseekable_open(inode, file);
}

static int esb_release(struct inode *inode, struct file *file)
{
	
	if (esb_expect_close == 42)
		esb_timer_stop();
	else {
		pr_crit("Unexpected close, not stopping watchdog!\n");
		esb_timer_keepalive();
	}
	clear_bit(0, &timer_alive);
	esb_expect_close = 0;
	return 0;
}

static ssize_t esb_write(struct file *file, const char __user *data,
			  size_t len, loff_t *ppos)
{
	
	if (len) {
		if (!nowayout) {
			size_t i;

			esb_expect_close = 0;

			for (i = 0; i != len; i++) {
				char c;
				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V')
					esb_expect_close = 42;
			}
		}

		
		esb_timer_keepalive();
	}
	return len;
}

static long esb_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
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
		.identity =		ESB_MODULE_NAME,
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(argp, &ident,
					sizeof(ident)) ? -EFAULT : 0;

	case WDIOC_GETSTATUS:
		return put_user(0, p);

	case WDIOC_GETBOOTSTATUS:
		return put_user(triggered, p);

	case WDIOC_SETOPTIONS:
	{
		if (get_user(new_options, p))
			return -EFAULT;

		if (new_options & WDIOS_DISABLECARD) {
			esb_timer_stop();
			retval = 0;
		}

		if (new_options & WDIOS_ENABLECARD) {
			esb_timer_start();
			retval = 0;
		}
		return retval;
	}
	case WDIOC_KEEPALIVE:
		esb_timer_keepalive();
		return 0;

	case WDIOC_SETTIMEOUT:
	{
		if (get_user(new_heartbeat, p))
			return -EFAULT;
		if (esb_timer_set_heartbeat(new_heartbeat))
			return -EINVAL;
		esb_timer_keepalive();
		
	}
	case WDIOC_GETTIMEOUT:
		return put_user(heartbeat, p);
	default:
		return -ENOTTY;
	}
}


static const struct file_operations esb_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.write = esb_write,
	.unlocked_ioctl = esb_ioctl,
	.open = esb_open,
	.release = esb_release,
};

static struct miscdevice esb_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &esb_fops,
};

static DEFINE_PCI_DEVICE_TABLE(esb_pci_tbl) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_ESB_9), },
	{ 0, },                 
};
MODULE_DEVICE_TABLE(pci, esb_pci_tbl);


static unsigned char __devinit esb_getdevice(struct pci_dev *pdev)
{
	if (pci_enable_device(pdev)) {
		pr_err("failed to enable device\n");
		goto err_devput;
	}

	if (pci_request_region(pdev, 0, ESB_MODULE_NAME)) {
		pr_err("failed to request region\n");
		goto err_disable;
	}

	BASEADDR = pci_ioremap_bar(pdev, 0);
	if (BASEADDR == NULL) {
		
		pr_err("failed to get BASEADDR\n");
		goto err_release;
	}

	
	esb_pci = pdev;
	return 1;

err_release:
	pci_release_region(pdev, 0);
err_disable:
	pci_disable_device(pdev);
err_devput:
	return 0;
}

static void __devinit esb_initdevice(void)
{
	u8 val1;
	u16 val2;

	pci_write_config_word(esb_pci, ESB_CONFIG_REG, 0x0003);

	
	pci_read_config_byte(esb_pci, ESB_LOCK_REG, &val1);
	if (val1 & ESB_WDT_LOCK)
		pr_warn("nowayout already set\n");

	
	pci_write_config_byte(esb_pci, ESB_LOCK_REG, 0x00);

	
	esb_unlock_registers();
	val2 = readw(ESB_RELOAD_REG);
	if (val2 & ESB_WDT_TIMEOUT)
		triggered = WDIOF_CARDRESET;

	
	esb_unlock_registers();
	writew((ESB_WDT_TIMEOUT | ESB_WDT_RELOAD), ESB_RELOAD_REG);

	
	esb_timer_set_heartbeat(heartbeat);
}

static int __devinit esb_probe(struct pci_dev *pdev,
		const struct pci_device_id *ent)
{
	int ret;

	cards_found++;
	if (cards_found == 1)
		pr_info("Intel 6300ESB WatchDog Timer Driver v%s\n",
			ESB_VERSION);

	if (cards_found > 1) {
		pr_err("This driver only supports 1 device\n");
		return -ENODEV;
	}

	
	if (!esb_getdevice(pdev) || esb_pci == NULL)
		return -ENODEV;

	if (heartbeat < 0x1 || heartbeat > 2 * 0x03ff) {
		heartbeat = WATCHDOG_HEARTBEAT;
		pr_info("heartbeat value must be 1<heartbeat<2046, using %d\n",
			heartbeat);
	}

	
	esb_initdevice();

	
	ret = misc_register(&esb_miscdev);
	if (ret != 0) {
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
		       WATCHDOG_MINOR, ret);
		goto err_unmap;
	}
	pr_info("initialized (0x%p). heartbeat=%d sec (nowayout=%d)\n",
		BASEADDR, heartbeat, nowayout);
	return 0;

err_unmap:
	iounmap(BASEADDR);
	pci_release_region(esb_pci, 0);
	pci_disable_device(esb_pci);
	esb_pci = NULL;
	return ret;
}

static void __devexit esb_remove(struct pci_dev *pdev)
{
	
	if (!nowayout)
		esb_timer_stop();

	
	misc_deregister(&esb_miscdev);
	iounmap(BASEADDR);
	pci_release_region(esb_pci, 0);
	pci_disable_device(esb_pci);
	esb_pci = NULL;
}

static void esb_shutdown(struct pci_dev *pdev)
{
	esb_timer_stop();
}

static struct pci_driver esb_driver = {
	.name		= ESB_MODULE_NAME,
	.id_table	= esb_pci_tbl,
	.probe          = esb_probe,
	.remove         = __devexit_p(esb_remove),
	.shutdown       = esb_shutdown,
};

static int __init watchdog_init(void)
{
	return pci_register_driver(&esb_driver);
}

static void __exit watchdog_cleanup(void)
{
	pci_unregister_driver(&esb_driver);
	pr_info("Watchdog Module Unloaded\n");
}

module_init(watchdog_init);
module_exit(watchdog_cleanup);

MODULE_AUTHOR("Ross Biro and David Härdeman");
MODULE_DESCRIPTION("Watchdog driver for Intel 6300ESB chipsets");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
