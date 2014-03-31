/*
 * linux/arch/sh/boards/magicpanel/setup.c
 *
 *  Copyright (C) 2007  Markus Brunner, Mark Jonas
 *
 *  Magic Panel Release 2 board setup
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/smsc911x.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/map.h>
#include <mach/magicpanelr2.h>
#include <asm/heartbeat.h>
#include <cpu/sh7720.h>

#define LAN9115_READY	(__raw_readl(0xA8000084UL) & 0x00000001UL)

static int __init ethernet_reset_finished(void)
{
	int i;

	if (LAN9115_READY)
		return 1;

	for (i = 0; i < 10; ++i) {
		mdelay(10);
		if (LAN9115_READY)
			return 1;
	}

	return 0;
}

static void __init reset_ethernet(void)
{
	
	CLRBITS_OUTB(0x10, PORT_PMDR);

	udelay(200);

	
	SETBITS_OUTB(0x10, PORT_PMDR);
}

static void __init setup_chip_select(void)
{
	
	
	__raw_writel(0x36db0400, CS2BCR);
	
	__raw_writel(0x000003c0, CS2WCR);

	
	
	__raw_writel(0x00000200, CS4BCR);
	
	__raw_writel(0x00100981, CS4WCR);

	
	
	__raw_writel(0x00000200, CS5ABCR);
	
	__raw_writel(0x00100981, CS5AWCR);

	
	
	__raw_writel(0x00000200, CS5BBCR);
	
	__raw_writel(0x00100981, CS5BWCR);

	
	
	__raw_writel(0x00000200, CS6ABCR);
	
	__raw_writel(0x001009C1, CS6AWCR);
}

static void __init setup_port_multiplexing(void)
{
	__raw_writew(0x5555, PORT_PACR);	

	__raw_writew(0x5555, PORT_PBCR);	

	__raw_writew(0x5500, PORT_PCCR);	

	__raw_writew(0x5555, PORT_PDCR);	

	__raw_writew(0x3C00, PORT_PECR);	

	__raw_writew(0x0002, PORT_PFCR);	

	__raw_writew(0x03D5, PORT_PGCR);	

	__raw_writew(0x0050, PORT_PHCR);	

	__raw_writew(0x0000, PORT_PJCR);	

	__raw_writew(0x00FF, PORT_PKCR);	

	__raw_writew(0x0000, PORT_PLCR);	

	__raw_writew(0x5552, PORT_PMCR);	   

#if CONFIG_SH_MAGIC_PANEL_R2_VERSION == 2
	__raw_writeb(0x30, PORT_PMDR);
#elif CONFIG_SH_MAGIC_PANEL_R2_VERSION == 3
	__raw_writeb(0xF0, PORT_PMDR);
#else
#error Unknown revision of PLATFORM_MP_R2
#endif

	__raw_writew(0x0100, PORT_PPCR);	
	__raw_writeb(0x10, PORT_PPDR);

	gpio_request(GPIO_FN_A25, NULL);
	gpio_request(GPIO_FN_A24, NULL);
	gpio_request(GPIO_FN_A23, NULL);
	gpio_request(GPIO_FN_A22, NULL);
	gpio_request(GPIO_FN_A21, NULL);
	gpio_request(GPIO_FN_A20, NULL);
	gpio_request(GPIO_FN_A19, NULL);
	gpio_request(GPIO_FN_A0, NULL);

	__raw_writew(0x0140, PORT_PSCR);	

	__raw_writew(0x0001, PORT_PTCR);	

	__raw_writew(0x0240, PORT_PUCR);	

	__raw_writew(0x0142, PORT_PVCR);	
}

static void __init mpr2_setup(char **cmdline_p)
{
	__raw_writew(0xAABC, PORT_PSELA);
	__raw_writew(0x3C00, PORT_PSELB);
	__raw_writew(0x0000, PORT_PSELC);
	__raw_writew(0x0000, PORT_PSELD);
	
	__raw_writew(0x0101, PORT_UTRCTL);
	
	__raw_writew(0xA5C0, PORT_UCLKCR_W);

	setup_chip_select();

	setup_port_multiplexing();

	reset_ethernet();

	printk(KERN_INFO "Magic Panel Release 2 A.%i\n",
				CONFIG_SH_MAGIC_PANEL_R2_VERSION);

	if (ethernet_reset_finished() == 0)
		printk(KERN_WARNING "Ethernet not ready\n");
}

static struct resource smsc911x_resources[] = {
	[0] = {
		.start		= 0xa8000000,
		.end		= 0xabffffff,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= 35,
		.end		= 35,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct smsc911x_platform_config smsc911x_config = {
	.phy_interface	= PHY_INTERFACE_MODE_MII,
	.irq_polarity	= SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type	= SMSC911X_IRQ_TYPE_OPEN_DRAIN,
	.flags		= SMSC911X_USE_32BIT,
};

static struct platform_device smsc911x_device = {
	.name		= "smsc911x",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(smsc911x_resources),
	.resource	= smsc911x_resources,
	.dev = {
		.platform_data = &smsc911x_config,
	},
};

static struct resource heartbeat_resources[] = {
	[0] = {
		.start	= PA_LED,
		.end	= PA_LED,
		.flags	= IORESOURCE_MEM,
	},
};

static struct heartbeat_data heartbeat_data = {
	.flags		= HEARTBEAT_INVERTED,
};

static struct platform_device heartbeat_device = {
	.name		= "heartbeat",
	.id		= -1,
	.dev	= {
		.platform_data	= &heartbeat_data,
	},
	.num_resources	= ARRAY_SIZE(heartbeat_resources),
	.resource	= heartbeat_resources,
};

static struct mtd_partition mpr2_partitions[] = {
	
	{
		.name = "Bootloader",
		.offset = 0x00000000UL,
		.size = MPR2_MTD_BOOTLOADER_SIZE,
		.mask_flags = MTD_WRITEABLE,
	},
	
	{
		.name = "Kernel",
		.offset = MTDPART_OFS_NXTBLK,
		.size = MPR2_MTD_KERNEL_SIZE,
	},
	
	{
		.name = "Flash_FS",
		.offset = MTDPART_OFS_NXTBLK,
		.size = MTDPART_SIZ_FULL,
	}
};

static struct physmap_flash_data flash_data = {
	.parts		= mpr2_partitions,
	.nr_parts	= ARRAY_SIZE(mpr2_partitions),
	.width		= 2,
};

static struct resource flash_resource = {
	.start		= 0x00000000,
	.end		= 0x2000000UL,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device flash_device = {
	.name		= "physmap-flash",
	.id		= -1,
	.resource	= &flash_resource,
	.num_resources	= 1,
	.dev		= {
		.platform_data = &flash_data,
	},
};


static struct platform_device *mpr2_devices[] __initdata = {
	&heartbeat_device,
	&smsc911x_device,
	&flash_device,
};


static int __init mpr2_devices_setup(void)
{
	return platform_add_devices(mpr2_devices, ARRAY_SIZE(mpr2_devices));
}
device_initcall(mpr2_devices_setup);

static void __init init_mpr2_IRQ(void)
{
	plat_irq_setup_pins(IRQ_MODE_IRQ); 

	irq_set_irq_type(32, IRQ_TYPE_LEVEL_LOW);    
	irq_set_irq_type(33, IRQ_TYPE_LEVEL_LOW);    
	irq_set_irq_type(34, IRQ_TYPE_LEVEL_LOW);    
	irq_set_irq_type(35, IRQ_TYPE_LEVEL_LOW);    
	irq_set_irq_type(36, IRQ_TYPE_EDGE_RISING);  
	irq_set_irq_type(37, IRQ_TYPE_EDGE_FALLING); 

	intc_set_priority(32, 13);		
	intc_set_priority(33, 13);		
	intc_set_priority(34, 13);		
	intc_set_priority(35, 6);		
}


static struct sh_machine_vector mv_mpr2 __initmv = {
	.mv_name		= "mpr2",
	.mv_setup		= mpr2_setup,
	.mv_init_irq		= init_mpr2_IRQ,
};
