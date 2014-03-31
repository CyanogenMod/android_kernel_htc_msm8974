/*
 *	intel TCO vendor specific watchdog driver support
 *
 *	(c) Copyright 2006-2009 Wim Van Sebroeck <wim@iguana.be>.
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

#define DRV_NAME	"iTCO_vendor_support"
#define DRV_VERSION	"1.04"

#include <linux/module.h>		
#include <linux/moduleparam.h>		
#include <linux/types.h>		
#include <linux/errno.h>		
#include <linux/kernel.h>		
#include <linux/init.h>			
#include <linux/ioport.h>		
#include <linux/io.h>			

#include "iTCO_vendor.h"

#define	SMI_EN		(acpibase + 0x30) 
#define	TCOBASE		(acpibase + 0x60) 
#define	TCO1_STS	(TCOBASE + 0x04)  

#define SUPERMICRO_OLD_BOARD	1
#define SUPERMICRO_NEW_BOARD	2
#define BROKEN_BIOS		911

static int vendorsupport;
module_param(vendorsupport, int, 0);
MODULE_PARM_DESC(vendorsupport, "iTCO vendor specific support mode, default="
			"0 (none), 1=SuperMicro Pent3, 2=SuperMicro Pent4+, "
							"911=Broken SMI BIOS");



static void supermicro_old_pre_start(unsigned long acpibase)
{
	unsigned long val32;

	
	val32 = inl(SMI_EN);
	val32 &= 0xffffdfff;	
	outl(val32, SMI_EN);	
}

static void supermicro_old_pre_stop(unsigned long acpibase)
{
	unsigned long val32;

	
	val32 = inl(SMI_EN);
	val32 |= 0x00002000;	
	outl(val32, SMI_EN);	
}

/*
 *	Vendor Support: 2
 *	Board: Super Micro Computer Inc. P4SBx, P4DPx
 *	iTCO chipset: ICH4
 *
 *	Code contributed by: R. Seretny <lkpatches@paypc.com>
 *	Documentation obtained by R. Seretny from SuperMicro Technical Support
 *
 *	To enable Watchdog function:
 *	 1. BIOS
 *	  For P4SBx:
 *	  BIOS setup -> Advanced -> Integrated Peripherals -> Watch Dog Feature
 *	  For P4DPx:
 *	  BIOS setup -> Advanced -> I/O Device Configuration -> Watch Dog
 *	 This setting enables or disables Watchdog function. When enabled, the
 *	 default watchdog timer is set to be 5 minutes (about 4m35s). It is
 *	 enough to load and run the OS. The application (service or driver) has
 *	 to take over the control once OS is running up and before watchdog
 *	 expires.
 *
 *	 2. JUMPER
 *	  For P4SBx: JP39
 *	  For P4DPx: JP37
 *	  This jumper is used for safety.  Closed is enabled. This jumper
 *	  prevents user enables watchdog in BIOS by accident.
 *
 *	 To enable Watch Dog function, both BIOS and JUMPER must be enabled.
 *
 *	The documentation lists motherboards P4SBx and P4DPx series as of
 *	20-March-2002. However, this code works flawlessly with much newer
 *	motherboards, such as my X6DHR-8G2 (SuperServer 6014H-82).
 *
 *	The original iTCO driver as written does not actually reset the
 *	watchdog timer on these machines, as a result they reboot after five
 *	minutes.
 *
 *	NOTE: You may leave the Watchdog function disabled in the SuperMicro
 *	BIOS to avoid a "boot-race"... This driver will enable watchdog
 *	functionality even if it's disabled in the BIOS once the /dev/watchdog
 *	file is opened.
 */

#define SM_REGINDEX	0x2e	
#define SM_DATAIO	0x2f	

#define SM_CTLPAGESW	0x07	
#define SM_CTLPAGE	0x08	

#define SM_WATCHENABLE	0x30	

#define SM_WATCHPAGE	0x87	

#define SM_ENDWATCH	0xAA	

#define SM_COUNTMODE	0xf5	
				

#define SM_WATCHTIMER	0xf6	

#define SM_RESETCONTROL	0xf7	
				
				

static void supermicro_new_unlock_watchdog(void)
{
	
	outb(SM_WATCHPAGE, SM_REGINDEX);
	outb(SM_WATCHPAGE, SM_REGINDEX);
	
	outb(SM_CTLPAGESW, SM_REGINDEX);
	outb(SM_CTLPAGE, SM_DATAIO);
}

static void supermicro_new_lock_watchdog(void)
{
	outb(SM_ENDWATCH, SM_REGINDEX);
}

static void supermicro_new_pre_start(unsigned int heartbeat)
{
	unsigned int val;

	supermicro_new_unlock_watchdog();

	
	outb(SM_COUNTMODE, SM_REGINDEX);
	val = inb(SM_DATAIO);
	val &= 0xF7;
	outb(val, SM_DATAIO);

	
	outb(SM_WATCHTIMER, SM_REGINDEX);
	outb((heartbeat & 255), SM_DATAIO);

	
	outb(SM_RESETCONTROL, SM_REGINDEX);
	val = inb(SM_DATAIO);
	val &= 0x3f;
	outb(val, SM_DATAIO);

	
	outb(SM_WATCHENABLE, SM_REGINDEX);
	val = inb(SM_DATAIO);
	val |= 0x01;
	outb(val, SM_DATAIO);

	supermicro_new_lock_watchdog();
}

static void supermicro_new_pre_stop(void)
{
	unsigned int val;

	supermicro_new_unlock_watchdog();

	
	outb(SM_WATCHENABLE, SM_REGINDEX);
	val = inb(SM_DATAIO);
	val &= 0xFE;
	outb(val, SM_DATAIO);

	supermicro_new_lock_watchdog();
}

static void supermicro_new_pre_set_heartbeat(unsigned int heartbeat)
{
	supermicro_new_unlock_watchdog();

	
	outb(SM_WATCHTIMER, SM_REGINDEX);
	outb((heartbeat & 255), SM_DATAIO);

	supermicro_new_lock_watchdog();
}


static void broken_bios_start(unsigned long acpibase)
{
	unsigned long val32;

	val32 = inl(SMI_EN);
	val32 &= 0xffffdffe;
	outl(val32, SMI_EN);
}

static void broken_bios_stop(unsigned long acpibase)
{
	unsigned long val32;

	val32 = inl(SMI_EN);
	val32 |= 0x00002001;
	outl(val32, SMI_EN);
}


void iTCO_vendor_pre_start(unsigned long acpibase,
			   unsigned int heartbeat)
{
	switch (vendorsupport) {
	case SUPERMICRO_OLD_BOARD:
		supermicro_old_pre_start(acpibase);
		break;
	case SUPERMICRO_NEW_BOARD:
		supermicro_new_pre_start(heartbeat);
		break;
	case BROKEN_BIOS:
		broken_bios_start(acpibase);
		break;
	}
}
EXPORT_SYMBOL(iTCO_vendor_pre_start);

void iTCO_vendor_pre_stop(unsigned long acpibase)
{
	switch (vendorsupport) {
	case SUPERMICRO_OLD_BOARD:
		supermicro_old_pre_stop(acpibase);
		break;
	case SUPERMICRO_NEW_BOARD:
		supermicro_new_pre_stop();
		break;
	case BROKEN_BIOS:
		broken_bios_stop(acpibase);
		break;
	}
}
EXPORT_SYMBOL(iTCO_vendor_pre_stop);

void iTCO_vendor_pre_keepalive(unsigned long acpibase, unsigned int heartbeat)
{
	if (vendorsupport == SUPERMICRO_NEW_BOARD)
		supermicro_new_pre_set_heartbeat(heartbeat);
}
EXPORT_SYMBOL(iTCO_vendor_pre_keepalive);

void iTCO_vendor_pre_set_heartbeat(unsigned int heartbeat)
{
	if (vendorsupport == SUPERMICRO_NEW_BOARD)
		supermicro_new_pre_set_heartbeat(heartbeat);
}
EXPORT_SYMBOL(iTCO_vendor_pre_set_heartbeat);

int iTCO_vendor_check_noreboot_on(void)
{
	switch (vendorsupport) {
	case SUPERMICRO_OLD_BOARD:
		return 0;
	default:
		return 1;
	}
}
EXPORT_SYMBOL(iTCO_vendor_check_noreboot_on);

static int __init iTCO_vendor_init_module(void)
{
	pr_info("vendor-support=%d\n", vendorsupport);
	return 0;
}

static void __exit iTCO_vendor_exit_module(void)
{
	pr_info("Module Unloaded\n");
}

module_init(iTCO_vendor_init_module);
module_exit(iTCO_vendor_exit_module);

MODULE_AUTHOR("Wim Van Sebroeck <wim@iguana.be>, "
		"R. Seretny <lkpatches@paypc.com>");
MODULE_DESCRIPTION("Intel TCO Vendor Specific WatchDog Timer Driver Support");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");

