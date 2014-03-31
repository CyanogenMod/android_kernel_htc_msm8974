/*
 *      NS pc87413-wdt Watchdog Timer driver for Linux 2.6.x.x
 *
 *      This code is based on wdt.c with original copyright.
 *
 *      (C) Copyright 2006 Sven Anders, <anders@anduras.de>
 *                     and Marcus Junker, <junker@anduras.de>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Neither Sven Anders, Marcus Junker nor ANDURAS AG
 *      admit liability nor provide warranty for any of this software.
 *      This material is provided "AS-IS" and at no charge.
 *
 *      Release 1.1
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/uaccess.h>



#define DEFAULT_TIMEOUT     1		
#define MAX_TIMEOUT         255

#define VERSION             "1.1"
#define MODNAME             "pc87413 WDT"
#define DPFX                MODNAME " - DEBUG: "

#define WDT_INDEX_IO_PORT   (io+0)	
#define WDT_DATA_IO_PORT    (WDT_INDEX_IO_PORT+1)
#define SWC_LDN             0x04
#define SIOCFG2             0x22	
#define WDCTL               0x10	
#define WDTO                0x11	
#define WDCFG               0x12	

#define IO_DEFAULT	0x2E		

static int io = IO_DEFAULT;
static int swc_base_addr = -1;

static int timeout = DEFAULT_TIMEOUT;	
static unsigned long timer_enabled;	

static char expect_close;		

static DEFINE_SPINLOCK(io_lock);	

static bool nowayout = WATCHDOG_NOWAYOUT;



static inline void pc87413_select_wdt_out(void)
{
	unsigned int cr_data = 0;

	

	outb_p(SIOCFG2, WDT_INDEX_IO_PORT);

	cr_data = inb(WDT_DATA_IO_PORT);

	cr_data |= 0x80; 
	outb_p(SIOCFG2, WDT_INDEX_IO_PORT);

	outb_p(cr_data, WDT_DATA_IO_PORT);

#ifdef DEBUG
	pr_info(DPFX
		"Select multiple pin,pin55,as WDT output: Bit7 to 1: %d\n",
								cr_data);
#endif
}


static inline void pc87413_enable_swc(void)
{
	unsigned int cr_data = 0;

	

	outb_p(0x07, WDT_INDEX_IO_PORT);	
	outb_p(SWC_LDN, WDT_DATA_IO_PORT);

	outb_p(0x30, WDT_INDEX_IO_PORT);	
	cr_data = inb(WDT_DATA_IO_PORT);
	cr_data |= 0x01;			
	outb_p(0x30, WDT_INDEX_IO_PORT);
	outb_p(cr_data, WDT_DATA_IO_PORT);	

#ifdef DEBUG
	pr_info(DPFX "pc87413 - Enable SWC functions\n");
#endif
}


static void pc87413_get_swc_base_addr(void)
{
	unsigned char addr_l, addr_h = 0;

	

	outb_p(0x60, WDT_INDEX_IO_PORT);	
	addr_h = inb(WDT_DATA_IO_PORT);

	outb_p(0x61, WDT_INDEX_IO_PORT);	

	addr_l = inb(WDT_DATA_IO_PORT);

	swc_base_addr = (addr_h << 8) + addr_l;
#ifdef DEBUG
	pr_info(DPFX
		"Read SWC I/O Base Address: low %d, high %d, res %d\n",
						addr_l, addr_h, swc_base_addr);
#endif
}


static inline void pc87413_swc_bank3(void)
{
	
	outb_p(inb(swc_base_addr + 0x0f) | 0x03, swc_base_addr + 0x0f);
#ifdef DEBUG
	pr_info(DPFX "Select Bank3 of SWC\n");
#endif
}


static inline void pc87413_programm_wdto(char pc87413_time)
{
	
	outb_p(pc87413_time, swc_base_addr + WDTO);
#ifdef DEBUG
	pr_info(DPFX "Set WDTO to %d minutes\n", pc87413_time);
#endif
}


static inline void pc87413_enable_wden(void)
{
	
	outb_p(inb(swc_base_addr + WDCTL) | 0x01, swc_base_addr + WDCTL);
#ifdef DEBUG
	pr_info(DPFX "Enable WDEN\n");
#endif
}

static inline void pc87413_enable_sw_wd_tren(void)
{
	
	outb_p(inb(swc_base_addr + WDCFG) | 0x80, swc_base_addr + WDCFG);
#ifdef DEBUG
	pr_info(DPFX "Enable SW_WD_TREN\n");
#endif
}


static inline void pc87413_disable_sw_wd_tren(void)
{
	
	outb_p(inb(swc_base_addr + WDCFG) & 0x7f, swc_base_addr + WDCFG);
#ifdef DEBUG
	pr_info(DPFX "pc87413 - Disable SW_WD_TREN\n");
#endif
}


static inline void pc87413_enable_sw_wd_trg(void)
{
	
	outb_p(inb(swc_base_addr + WDCTL) | 0x80, swc_base_addr + WDCTL);
#ifdef DEBUG
	pr_info(DPFX "pc87413 - Enable SW_WD_TRG\n");
#endif
}


static inline void pc87413_disable_sw_wd_trg(void)
{
	
	outb_p(inb(swc_base_addr + WDCTL) & 0x7f, swc_base_addr + WDCTL);
#ifdef DEBUG
	pr_info(DPFX "Disable SW_WD_TRG\n");
#endif
}



static void pc87413_enable(void)
{
	spin_lock(&io_lock);

	pc87413_swc_bank3();
	pc87413_programm_wdto(timeout);
	pc87413_enable_wden();
	pc87413_enable_sw_wd_tren();
	pc87413_enable_sw_wd_trg();

	spin_unlock(&io_lock);
}


static void pc87413_disable(void)
{
	spin_lock(&io_lock);

	pc87413_swc_bank3();
	pc87413_disable_sw_wd_tren();
	pc87413_disable_sw_wd_trg();
	pc87413_programm_wdto(0);

	spin_unlock(&io_lock);
}


static void pc87413_refresh(void)
{
	spin_lock(&io_lock);

	pc87413_swc_bank3();
	pc87413_disable_sw_wd_tren();
	pc87413_disable_sw_wd_trg();
	pc87413_programm_wdto(timeout);
	pc87413_enable_wden();
	pc87413_enable_sw_wd_tren();
	pc87413_enable_sw_wd_trg();

	spin_unlock(&io_lock);
}



static int pc87413_open(struct inode *inode, struct file *file)
{
	

	if (test_and_set_bit(0, &timer_enabled))
		return -EBUSY;

	if (nowayout)
		__module_get(THIS_MODULE);

	
	pc87413_refresh();

	pr_info("Watchdog enabled. Timeout set to %d minute(s).\n", timeout);

	return nonseekable_open(inode, file);
}


static int pc87413_release(struct inode *inode, struct file *file)
{
	

	if (expect_close == 42) {
		pc87413_disable();
		pr_info("Watchdog disabled, sleeping again...\n");
	} else {
		pr_crit("Unexpected close, not stopping watchdog!\n");
		pc87413_refresh();
	}
	clear_bit(0, &timer_enabled);
	expect_close = 0;
	return 0;
}



static int pc87413_status(void)
{
	  return 0; 
}


static ssize_t pc87413_write(struct file *file, const char __user *data,
			     size_t len, loff_t *ppos)
{
	
	if (len) {
		if (!nowayout) {
			size_t i;

			
			expect_close = 0;

			for (i = 0; i != len; i++) {
				char c;
				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 42;
			}
		}

		
		pc87413_refresh();
	}
	return len;
}


static long pc87413_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	int new_timeout;

	union {
		struct watchdog_info __user *ident;
		int __user *i;
	} uarg;

	static const struct watchdog_info ident = {
		.options          = WDIOF_KEEPALIVEPING |
				    WDIOF_SETTIMEOUT |
				    WDIOF_MAGICCLOSE,
		.firmware_version = 1,
		.identity         = "PC87413(HF/F) watchdog",
	};

	uarg.i = (int __user *)arg;

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(uarg.ident, &ident,
					sizeof(ident)) ? -EFAULT : 0;
	case WDIOC_GETSTATUS:
		return put_user(pc87413_status(), uarg.i);
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, uarg.i);
	case WDIOC_SETOPTIONS:
	{
		int options, retval = -EINVAL;
		if (get_user(options, uarg.i))
			return -EFAULT;
		if (options & WDIOS_DISABLECARD) {
			pc87413_disable();
			retval = 0;
		}
		if (options & WDIOS_ENABLECARD) {
			pc87413_enable();
			retval = 0;
		}
		return retval;
	}
	case WDIOC_KEEPALIVE:
		pc87413_refresh();
#ifdef DEBUG
		pr_info(DPFX "keepalive\n");
#endif
		return 0;
	case WDIOC_SETTIMEOUT:
		if (get_user(new_timeout, uarg.i))
			return -EFAULT;
		
		new_timeout /= 60;
		if (new_timeout < 0 || new_timeout > MAX_TIMEOUT)
			return -EINVAL;
		timeout = new_timeout;
		pc87413_refresh();
		
	case WDIOC_GETTIMEOUT:
		new_timeout = timeout * 60;
		return put_user(new_timeout, uarg.i);
	default:
		return -ENOTTY;
	}
}



static int pc87413_notify_sys(struct notifier_block *this,
			      unsigned long code,
			      void *unused)
{
	if (code == SYS_DOWN || code == SYS_HALT)
		
		pc87413_disable();
	return NOTIFY_DONE;
}


static const struct file_operations pc87413_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= pc87413_write,
	.unlocked_ioctl	= pc87413_ioctl,
	.open		= pc87413_open,
	.release	= pc87413_release,
};

static struct notifier_block pc87413_notifier = {
	.notifier_call  = pc87413_notify_sys,
};

static struct miscdevice pc87413_miscdev = {
	.minor          = WATCHDOG_MINOR,
	.name           = "watchdog",
	.fops           = &pc87413_fops,
};



static int __init pc87413_init(void)
{
	int ret;

	pr_info("Version " VERSION " at io 0x%X\n",
							WDT_INDEX_IO_PORT);

	if (!request_muxed_region(io, 2, MODNAME))
		return -EBUSY;

	ret = register_reboot_notifier(&pc87413_notifier);
	if (ret != 0) {
		pr_err("cannot register reboot notifier (err=%d)\n", ret);
	}

	ret = misc_register(&pc87413_miscdev);
	if (ret != 0) {
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
		       WATCHDOG_MINOR, ret);
		goto reboot_unreg;
	}
	pr_info("initialized. timeout=%d min\n", timeout);

	pc87413_select_wdt_out();
	pc87413_enable_swc();
	pc87413_get_swc_base_addr();

	if (!request_region(swc_base_addr, 0x20, MODNAME)) {
		pr_err("cannot request SWC region at 0x%x\n", swc_base_addr);
		ret = -EBUSY;
		goto misc_unreg;
	}

	pc87413_enable();

	release_region(io, 2);
	return 0;

misc_unreg:
	misc_deregister(&pc87413_miscdev);
reboot_unreg:
	unregister_reboot_notifier(&pc87413_notifier);
	release_region(io, 2);
	return ret;
}


static void __exit pc87413_exit(void)
{
	
	if (!nowayout) {
		pc87413_disable();
		pr_info("Watchdog disabled\n");
	}

	misc_deregister(&pc87413_miscdev);
	unregister_reboot_notifier(&pc87413_notifier);
	release_region(swc_base_addr, 0x20);

	pr_info("watchdog component driver removed\n");
}

module_init(pc87413_init);
module_exit(pc87413_exit);

MODULE_AUTHOR("Sven Anders <anders@anduras.de>, "
		"Marcus Junker <junker@anduras.de>,");
MODULE_DESCRIPTION("PC87413 WDT driver");
MODULE_LICENSE("GPL");

MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);

module_param(io, int, 0);
MODULE_PARM_DESC(io, MODNAME " I/O port (default: "
					__MODULE_STRING(IO_DEFAULT) ").");

module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout,
		"Watchdog timeout in minutes (default="
				__MODULE_STRING(DEFAULT_TIMEOUT) ").");

module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

