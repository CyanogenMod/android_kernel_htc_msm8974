/*
 * Freescale Lite5200 board support
 *
 * Written by: Grant Likely <grant.likely@secretlab.ca>
 *
 * Copyright (C) Secret Lab Technologies Ltd. 2006. All rights reserved.
 * Copyright (C) Freescale Semicondutor, Inc. 2006. All rights reserved.
 *
 * Description:
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#undef DEBUG

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/root_dev.h>
#include <linux/initrd.h>
#include <asm/time.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/prom.h>
#include <asm/mpc52xx.h>


static struct of_device_id mpc5200_cdm_ids[] __initdata = {
	{ .compatible = "fsl,mpc5200-cdm", },
	{ .compatible = "mpc5200-cdm", },
	{}
};

static struct of_device_id mpc5200_gpio_ids[] __initdata = {
	{ .compatible = "fsl,mpc5200-gpio", },
	{ .compatible = "mpc5200-gpio", },
	{}
};

static void __init
lite5200_fix_clock_config(void)
{
	struct device_node *np;
	struct mpc52xx_cdm  __iomem *cdm;
	
	np = of_find_matching_node(NULL, mpc5200_cdm_ids);
	cdm = of_iomap(np, 0);
	of_node_put(np);
	if (!cdm) {
		printk(KERN_ERR "%s() failed; expect abnormal behaviour\n",
		       __func__);
		return;
	}

	
	out_8(&cdm->ext_48mhz_en, 0x00);
	out_8(&cdm->fd_enable, 0x01);
	if (in_be32(&cdm->rstcfg) & 0x40)	
		out_be16(&cdm->fd_counters, 0x0001);
	else
		out_be16(&cdm->fd_counters, 0x5555);

	
	iounmap(cdm);
}

static void __init
lite5200_fix_port_config(void)
{
	struct device_node *np;
	struct mpc52xx_gpio __iomem *gpio;
	u32 port_config;

	np = of_find_matching_node(NULL, mpc5200_gpio_ids);
	gpio = of_iomap(np, 0);
	of_node_put(np);
	if (!gpio) {
		printk(KERN_ERR "%s() failed. expect abnormal behavior\n",
		       __func__);
		return;
	}

	
	port_config = in_be32(&gpio->port_config);

	port_config &= ~0x00800000;	

	port_config &= ~0x00007000;	
	port_config |=  0x00001000;	

	port_config &= ~0x03000000;	
	port_config |=  0x01000000;

	pr_debug("port_config: old:%x new:%x\n",
	         in_be32(&gpio->port_config), port_config);
	out_be32(&gpio->port_config, port_config);

	
	iounmap(gpio);
}

#ifdef CONFIG_PM
static void lite5200_suspend_prepare(void __iomem *mbar)
{
	u8 pin = 1;	
	u8 level = 0;	
	mpc52xx_set_wakeup_gpio(pin, level);


	out_be32(mbar + 0x1048, in_be32(mbar + 0x1048) & ~0x300);
	
	out_be32(mbar + 0x1050, 0x00000001);
}

static void lite5200_resume_finish(void __iomem *mbar)
{
	
	out_be32(mbar + 0x1050, 0x00010000);
}
#endif

static void __init lite5200_setup_arch(void)
{
	if (ppc_md.progress)
		ppc_md.progress("lite5200_setup_arch()", 0);

	
	mpc52xx_map_common_devices();

	
	mpc5200_setup_xlb_arbiter();

	
	lite5200_fix_clock_config();
	lite5200_fix_port_config();

#ifdef CONFIG_PM
	mpc52xx_suspend.board_suspend_prepare = lite5200_suspend_prepare;
	mpc52xx_suspend.board_resume_finish = lite5200_resume_finish;
	lite5200_pm_init();
#endif

	mpc52xx_setup_pci();
}

static const char *board[] __initdata = {
	"fsl,lite5200",
	"fsl,lite5200b",
	NULL,
};

static int __init lite5200_probe(void)
{
	return of_flat_dt_match(of_get_flat_dt_root(), board);
}

define_machine(lite5200) {
	.name 		= "lite5200",
	.probe 		= lite5200_probe,
	.setup_arch 	= lite5200_setup_arch,
	.init		= mpc52xx_declare_of_platform_devices,
	.init_IRQ 	= mpc52xx_init_irq,
	.get_irq 	= mpc52xx_get_irq,
	.restart	= mpc52xx_restart,
	.calibrate_decr	= generic_calibrate_decr,
};
