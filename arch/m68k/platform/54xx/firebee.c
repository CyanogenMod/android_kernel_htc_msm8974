
/*
 *	firebee.c -- extra startup code support for the FireBee boards
 *
 *	Copyright (C) 2011, Greg Ungerer (gerg@snapgear.com)
 */


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>


#define	FLASH_PHYS_ADDR		0xe0000000	
#define	FLASH_PHYS_SIZE		0x00800000	

#define	PART_BOOT_START		0x00000000	
#define	PART_BOOT_SIZE		0x00040000	
#define	PART_IMAGE_START	0x00040000	
#define	PART_IMAGE_SIZE		0x006c0000	
#define	PART_FPGA_START		0x00700000	
#define	PART_FPGA_SIZE		0x00100000	

static struct mtd_partition firebee_flash_parts[] = {
	{
		.name	= "dBUG",
		.offset	= PART_BOOT_START,
		.size	= PART_BOOT_SIZE,
	},
	{
		.name	= "FPGA",
		.offset	= PART_FPGA_START,
		.size	= PART_FPGA_SIZE,
	},
	{
		.name	= "image",
		.offset	= PART_IMAGE_START,
		.size	= PART_IMAGE_SIZE,
	},
};

static struct physmap_flash_data firebee_flash_data = {
	.width		= 2,
	.nr_parts	= ARRAY_SIZE(firebee_flash_parts),
	.parts		= firebee_flash_parts,
};

static struct resource firebee_flash_resource = {
	.start		= FLASH_PHYS_ADDR,
	.end		= FLASH_PHYS_ADDR + FLASH_PHYS_SIZE,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device firebee_flash = {
	.name		= "physmap-flash",
	.id		= 0,
	.dev		= {
		.platform_data = &firebee_flash_data,
	},
	.num_resources	= 1,
	.resource	= &firebee_flash_resource,
};


static int __init init_firebee(void)
{
	platform_device_register(&firebee_flash);
	return 0;
}

arch_initcall(init_firebee);

