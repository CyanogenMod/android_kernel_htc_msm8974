/*
 * Copyright (C) Freescale Semicondutor, Inc. 2006. All rights reserved.
 *
 * Author: Li Yang <LeoLi@freescale.com>
 *	   Yin Olivia <Hong-hua.Yin@freescale.com>
 *
 * Description:
 * MPC8360E MDS board specific routines.
 *
 * Changelog:
 * Jun 21, 2006	Initial version
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/reboot.h>
#include <linux/pci.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/root_dev.h>
#include <linux/initrd.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>

#include <linux/atomic.h>
#include <asm/time.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/ipic.h>
#include <asm/irq.h>
#include <asm/prom.h>
#include <asm/udbg.h>
#include <sysdev/fsl_soc.h>
#include <sysdev/fsl_pci.h>
#include <sysdev/simple_gpio.h>
#include <asm/qe.h>
#include <asm/qe_ic.h>

#include "mpc83xx.h"

#undef DEBUG
#ifdef DEBUG
#define DBG(fmt...) udbg_printf(fmt)
#else
#define DBG(fmt...)
#endif

static void __init mpc836x_mds_setup_arch(void)
{
	struct device_node *np;
	u8 __iomem *bcsr_regs = NULL;

	if (ppc_md.progress)
		ppc_md.progress("mpc836x_mds_setup_arch()", 0);

	
	np = of_find_node_by_name(NULL, "bcsr");
	if (np) {
		struct resource res;

		of_address_to_resource(np, 0, &res);
		bcsr_regs = ioremap(res.start, resource_size(&res));
		of_node_put(np);
	}

	mpc83xx_setup_pci();

#ifdef CONFIG_QUICC_ENGINE
	qe_reset();

	if ((np = of_find_node_by_name(NULL, "par_io")) != NULL) {
		par_io_init(np);
		of_node_put(np);

		for (np = NULL; (np = of_find_node_by_name(np, "ucc")) != NULL;)
			par_io_of_config(np);
#ifdef CONFIG_QE_USB
		
		par_io_config_pin(1,  2, 1, 0, 3, 0); 
		par_io_config_pin(1,  3, 1, 0, 3, 0); 
		par_io_config_pin(1,  8, 1, 0, 1, 0); 
		par_io_config_pin(1, 10, 2, 0, 3, 0); 
		par_io_config_pin(1,  9, 2, 1, 3, 0); 
		par_io_config_pin(1, 11, 2, 1, 3, 0); 
		par_io_config_pin(2, 20, 2, 0, 1, 0); 
#endif 
	}

	if ((np = of_find_compatible_node(NULL, "network", "ucc_geth"))
			!= NULL){
		uint svid;

		
#define BCSR9_GETHRST 0x20
		clrbits8(&bcsr_regs[9], BCSR9_GETHRST);
		udelay(1000);
		setbits8(&bcsr_regs[9], BCSR9_GETHRST);

		
		svid = mfspr(SPRN_SVR);
		if (svid == 0x80480021) {
			void __iomem *immap;

			immap = ioremap(get_immrbase() + 0x14a8, 8);

			setbits32(immap, 0x0c003000);

			clrsetbits_be32(immap + 4, 0xff0, 0xaa0);

			iounmap(immap);
		}

		iounmap(bcsr_regs);
		of_node_put(np);
	}
#endif				
}

machine_device_initcall(mpc836x_mds, mpc83xx_declare_of_platform_devices);

#ifdef CONFIG_QE_USB
static int __init mpc836x_usb_cfg(void)
{
	u8 __iomem *bcsr;
	struct device_node *np;
	const char *mode;
	int ret = 0;

	np = of_find_compatible_node(NULL, NULL, "fsl,mpc8360mds-bcsr");
	if (!np)
		return -ENODEV;

	bcsr = of_iomap(np, 0);
	of_node_put(np);
	if (!bcsr)
		return -ENOMEM;

	np = of_find_compatible_node(NULL, NULL, "fsl,mpc8323-qe-usb");
	if (!np) {
		ret = -ENODEV;
		goto err;
	}

#define BCSR8_TSEC1M_MASK	(0x3 << 6)
#define BCSR8_TSEC1M_RGMII	(0x0 << 6)
#define BCSR8_TSEC2M_MASK	(0x3 << 4)
#define BCSR8_TSEC2M_RGMII	(0x0 << 4)
	clrsetbits_8(&bcsr[8], BCSR8_TSEC1M_MASK | BCSR8_TSEC2M_MASK,
			       BCSR8_TSEC1M_RGMII | BCSR8_TSEC2M_RGMII);

#define BCSR13_USBMASK	0x0f
#define BCSR13_nUSBEN	0x08 
#define BCSR13_USBSPEED	0x04 
#define BCSR13_USBMODE	0x02 
#define BCSR13_nUSBVCC	0x01 

	clrsetbits_8(&bcsr[13], BCSR13_USBMASK, BCSR13_USBSPEED);

	mode = of_get_property(np, "mode", NULL);
	if (mode && !strcmp(mode, "peripheral")) {
		setbits8(&bcsr[13], BCSR13_nUSBVCC);
		qe_usb_clock_set(QE_CLK21, 48000000);
	} else {
		setbits8(&bcsr[13], BCSR13_USBMODE);
		simple_gpiochip_init("fsl,mpc8360mds-bcsr-gpio");
	}

	of_node_put(np);
err:
	iounmap(bcsr);
	return ret;
}
machine_arch_initcall(mpc836x_mds, mpc836x_usb_cfg);
#endif 

static int __init mpc836x_mds_probe(void)
{
        unsigned long root = of_get_flat_dt_root();

        return of_flat_dt_is_compatible(root, "MPC836xMDS");
}

define_machine(mpc836x_mds) {
	.name		= "MPC836x MDS",
	.probe		= mpc836x_mds_probe,
	.setup_arch	= mpc836x_mds_setup_arch,
	.init_IRQ	= mpc83xx_ipic_and_qe_init_IRQ,
	.get_irq	= ipic_get_irq,
	.restart	= mpc83xx_restart,
	.time_init	= mpc83xx_time_init,
	.calibrate_decr	= generic_calibrate_decr,
	.progress	= udbg_progress,
};
