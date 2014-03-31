/*
 *  Support for Cogent CSB726
 *
 *  Copyright (c) 2008 Dmitry Eremin-Solenikov
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/sm501.h>
#include <linux/smsc911x.h>
#include <linux/i2c/pxa-i2c.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/csb726.h>
#include <mach/pxa27x.h>
#include <mach/mmc.h>
#include <mach/ohci.h>
#include <mach/audio.h>
#include <mach/smemc.h>

#include "generic.h"
#include "devices.h"


static unsigned long csb726_pin_config[] = {
	GPIO78_nCS_2, 
	GPIO79_nCS_3, 
	GPIO80_nCS_4, 

	GPIO52_GPIO, 
	GPIO53_GPIO, 

	GPIO1_GPIO, 
	GPIO11_GPIO, 
	GPIO9_GPIO, 
	GPIO10_GPIO, 
	GPIO16_PWM0_OUT, 
	GPIO17_PWM1_OUT, 
	GPIO94_GPIO, 
	GPIO95_GPIO, 
	GPIO96_GPIO, 
	GPIO97_GPIO, 
	GPIO15_GPIO, 
	GPIO18_RDY, 

	GPIO0_GPIO, 
	GPIO104_GPIO, 

	GPIO12_GPIO, 

	GPIO13_SSP2_TXD,
	GPIO14_SSP2_SFRM,
	MFP_CFG_OUT(GPIO19, AF1, DRIVE_LOW),
	GPIO22_SSP2_SCLK,

	GPIO81_SSP3_TXD,
	GPIO82_SSP3_RXD,
	GPIO83_SSP3_SFRM,
	GPIO84_SSP3_SCLK,

	GPIO20_GPIO, 
	GPIO32_MMC_CLK,
	GPIO92_MMC_DAT_0,
	GPIO109_MMC_DAT_1,
	GPIO110_MMC_DAT_2,
	GPIO111_MMC_DAT_3,
	GPIO112_MMC_CMD,
	GPIO100_GPIO, 
	GPIO101_GPIO, 

	GPIO28_AC97_BITCLK,
	GPIO29_AC97_SDATA_IN_0,
	GPIO30_AC97_SDATA_OUT,
	GPIO31_AC97_SYNC,
	GPIO113_AC97_nRESET,

	GPIO34_FFUART_RXD,
	GPIO35_FFUART_CTS,
	GPIO36_FFUART_DCD,
	GPIO37_FFUART_DSR,
	GPIO38_FFUART_RI,
	GPIO39_FFUART_TXD,
	GPIO40_FFUART_DTR,
	GPIO41_FFUART_RTS,

	GPIO42_BTUART_RXD,
	GPIO43_BTUART_TXD,
	GPIO44_BTUART_CTS,
	GPIO45_BTUART_RTS,

	GPIO46_STUART_RXD,
	GPIO47_STUART_TXD,

	GPIO48_nPOE,
	GPIO49_nPWE,
	GPIO50_nPIOR,
	GPIO51_nPIOW,
	GPIO54_nPCE_2,
	GPIO55_nPREG,
	GPIO56_nPWAIT,
	GPIO57_nIOIS16, 
	GPIO85_nPCE_1,
	GPIO98_GPIO, 
	GPIO99_GPIO, 
	GPIO103_GPIO, 

	GPIO117_I2C_SCL,
	GPIO118_I2C_SDA,
};

static struct pxamci_platform_data csb726_mci = {
	.detect_delay_ms	= 500,
	.ocr_mask		= MMC_VDD_32_33|MMC_VDD_33_34,
	
	.gpio_card_detect	= CSB726_GPIO_MMC_DETECT,
	.gpio_card_ro		= CSB726_GPIO_MMC_RO,
	.gpio_power		= -1,
};

static struct pxaohci_platform_data csb726_ohci_platform_data = {
	.port_mode	= PMM_NPS_MODE,
	.flags		= ENABLE_PORT1 | NO_OC_PROTECTION,
};

static struct mtd_partition csb726_flash_partitions[] = {
	{
		.name		= "Bootloader",
		.offset		= 0,
		.size		= CSB726_FLASH_uMON,
		.mask_flags	= MTD_WRITEABLE  
	},
	{
		.name		= "root",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
	}
};

static struct physmap_flash_data csb726_flash_data = {
	.width		= 2,
	.parts		= csb726_flash_partitions,
	.nr_parts	= ARRAY_SIZE(csb726_flash_partitions),
};

static struct resource csb726_flash_resources[] = {
	{
		.start          = PXA_CS0_PHYS,
		.end            = PXA_CS0_PHYS + CSB726_FLASH_SIZE - 1 ,
		.flags          = IORESOURCE_MEM,
	}
};

static struct platform_device csb726_flash = {
	.name           = "physmap-flash",
	.dev            = {
		.platform_data  = &csb726_flash_data,
	},
	.resource       = csb726_flash_resources,
	.num_resources  = ARRAY_SIZE(csb726_flash_resources),
};

static struct resource csb726_sm501_resources[] = {
	{
		.start          = PXA_CS4_PHYS,
		.end            = PXA_CS4_PHYS + SZ_8M - 1,
		.flags          = IORESOURCE_MEM,
		.name		= "sm501-localmem",
	},
	{
		.start          = PXA_CS4_PHYS + SZ_64M - SZ_2M,
		.end            = PXA_CS4_PHYS + SZ_64M - 1,
		.flags          = IORESOURCE_MEM,
		.name		= "sm501-regs",
	},
	{
		.start		= CSB726_IRQ_SM501,
		.end		= CSB726_IRQ_SM501,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct sm501_initdata csb726_sm501_initdata = {
	.devices	= SM501_USE_USB_HOST | SM501_USE_UART0 | SM501_USE_UART1,
};

static struct sm501_platdata csb726_sm501_platdata = {
	.init		= &csb726_sm501_initdata,
};

static struct platform_device csb726_sm501 = {
	.name		= "sm501",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(csb726_sm501_resources),
	.resource	= csb726_sm501_resources,
	.dev		= {
		.platform_data = &csb726_sm501_platdata,
	},
};

static struct resource csb726_lan_resources[] = {
	{
		.start	= PXA_CS3_PHYS,
		.end	= PXA_CS3_PHYS + SZ_64K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= CSB726_IRQ_LAN,
		.end	= CSB726_IRQ_LAN,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
	},
};

struct smsc911x_platform_config csb726_lan_config = {
	.irq_polarity	= SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type	= SMSC911X_IRQ_TYPE_PUSH_PULL,
	.flags		= SMSC911X_USE_32BIT,
	.phy_interface	= PHY_INTERFACE_MODE_MII,
};


static struct platform_device csb726_lan = {
	.name		= "smsc911x",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(csb726_lan_resources),
	.resource	= csb726_lan_resources,
	.dev		= {
		.platform_data	= &csb726_lan_config,
	},
};

static struct platform_device *devices[] __initdata = {
	&csb726_flash,
	&csb726_sm501,
	&csb726_lan,
};

static void __init csb726_init(void)
{
	pxa2xx_mfp_config(ARRAY_AND_SIZE(csb726_pin_config));
	__raw_writel((__raw_readl(MSC2) & ~0xffff) | 0x7ff4, MSC2); 

	pxa_set_ffuart_info(NULL);
	pxa_set_btuart_info(NULL);
	pxa_set_stuart_info(NULL);
	pxa_set_i2c_info(NULL);
	pxa27x_set_i2c_power_info(NULL);
	pxa_set_mci_info(&csb726_mci);
	pxa_set_ohci_info(&csb726_ohci_platform_data);
	pxa_set_ac97_info(NULL);

	platform_add_devices(devices, ARRAY_SIZE(devices));
}

MACHINE_START(CSB726, "Cogent CSB726")
	.atag_offset	= 0x100,
	.map_io         = pxa27x_map_io,
	.nr_irqs	= PXA_NR_IRQS,
	.init_irq       = pxa27x_init_irq,
	.handle_irq       = pxa27x_handle_irq,
	.init_machine   = csb726_init,
	.timer          = &pxa_timer,
	.restart	= pxa_restart,
MACHINE_END
