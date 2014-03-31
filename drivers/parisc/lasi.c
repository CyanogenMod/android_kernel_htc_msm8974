/*
 *	LASI Device Driver
 *
 *	(c) Copyright 1999 Red Hat Software
 *	Portions (c) Copyright 1999 The Puffin Group Inc.
 *	Portions (c) Copyright 1999 Hewlett-Packard
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *	by Alan Cox <alan@redhat.com> and 
 * 	   Alex deVries <alex@onefishtwo.ca>
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/types.h>

#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/led.h>

#include "gsc.h"


#define LASI_VER	0xC008	

#define LASI_IO_CONF	0x7FFFE	
#define LASI_IO_CONF2	0x7FFFF	

static void lasi_choose_irq(struct parisc_device *dev, void *ctrl)
{
	int irq;

	switch (dev->id.sversion) {
		case 0x74:	irq =  7; break; 
		case 0x7B:	irq = 13; break; 
		case 0x81:	irq = 14; break; 
		case 0x82:	irq =  9; break; 
		case 0x83:	irq = 20; break; 
		case 0x84:	irq = 26; break; 
		case 0x87:	irq = 18; break; 
		case 0x8A:	irq =  8; break; 
		case 0x8C:	irq =  5; break; 
		case 0x8D:	irq = (dev->hw_path == 13) ? 16 : 17; break;
						 
		default: 	return;		 
	}

	gsc_asic_assign_irq(ctrl, irq, &dev->irq);
}

static void __init
lasi_init_irq(struct gsc_asic *this_lasi)
{
	unsigned long lasi_base = this_lasi->hpa;

	
	gsc_writel(0x00000000, lasi_base+OFFSET_IMR);

	
	gsc_readl(lasi_base+OFFSET_IRR);


	
		
	if(pdc_add_valid(lasi_base+0x4004) == PDC_OK)
		gsc_writel(0xFFFFFFFF, lasi_base+0x4004);	
		 
		
	gsc_writel(0xFFFFFFFF, lasi_base+0x7000);	
	gsc_writel(0xFFFFFFFF, lasi_base+0x8000);	
	gsc_writel(0xFFFFFFFF, lasi_base+0xA000);	
	
}



#ifndef CONFIG_CHASSIS_LCD_LED

#define lasi_led_init(x)	

#else

static void __init lasi_led_init(unsigned long lasi_hpa)
{
	unsigned long datareg;

	switch (CPU_HVERSION) {
 
	case 0x600:		
	case 0x601:		
	case 0x602:		
	case 0x603:		
	case 0x604:		
	case 0x605:		
		datareg = lasi_hpa + 0x0000C000;
		gsc_writeb(0, datareg);
		return;   

	
	case 0x60A:		
	case 0x60B:		
	case 0x60C:		
	case 0x60D:		
	case 0x60E:		
		datareg = lasi_hpa - 0x00020000;
		break;

	default:
		datareg = lasi_hpa + 0x0000C000;
		break;
	}

	register_led_driver(DISPLAY_MODEL_LASI, LED_CMD_REG_NONE, datareg);
}
#endif


static unsigned long lasi_power_off_hpa __read_mostly;

static void lasi_power_off(void)
{
	unsigned long datareg;

	
	datareg = lasi_power_off_hpa + 0x0000C000;

	
	gsc_writel(0x02, datareg);
}

static int __init lasi_init_chip(struct parisc_device *dev)
{
	extern void (*chassis_power_off)(void);
	struct gsc_asic *lasi;
	struct gsc_irq gsc_irq;
	int ret;

	lasi = kzalloc(sizeof(*lasi), GFP_KERNEL);
	if (!lasi)
		return -ENOMEM;

	lasi->name = "Lasi";
	lasi->hpa = dev->hpa.start;

	
	lasi->version = gsc_readl(lasi->hpa + LASI_VER) & 0xf;
	printk(KERN_INFO "%s version %d at 0x%lx found.\n",
		lasi->name, lasi->version, lasi->hpa);

	 
	lasi_led_init(lasi->hpa);

	
	lasi_init_irq(lasi);

	
	dev->irq = gsc_alloc_irq(&gsc_irq);
	if (dev->irq < 0) {
		printk(KERN_ERR "%s(): cannot get GSC irq\n",
				__func__);
		kfree(lasi);
		return -EBUSY;
	}

	lasi->eim = ((u32) gsc_irq.txn_addr) | gsc_irq.txn_data;

	ret = request_irq(gsc_irq.irq, gsc_asic_intr, 0, "lasi", lasi);
	if (ret < 0) {
		kfree(lasi);
		return ret;
	}

	
	gsc_writel(lasi->eim, lasi->hpa + OFFSET_IAR);

	
	ret = gsc_common_setup(dev, lasi);
	if (ret) {
		kfree(lasi);
		return ret;
	}    

	gsc_fixup_irqs(dev, lasi, lasi_choose_irq);

	
	lasi_power_off_hpa = lasi->hpa;
	chassis_power_off = lasi_power_off;
	
	return ret;
}

static struct parisc_device_id lasi_tbl[] = {
	{ HPHW_BA, HVERSION_REV_ANY_ID, HVERSION_ANY_ID, 0x00081 },
	{ 0, }
};

struct parisc_driver lasi_driver = {
	.name =		"lasi",
	.id_table =	lasi_tbl,
	.probe =	lasi_init_chip,
};
