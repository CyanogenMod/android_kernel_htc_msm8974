/*
 * TI DaVinci EVM board support
 *
 * Author: Kevin Hilman, Deep Root Systems, LLC
 *
 * 2007 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/videodev2.h>
#include <media/tvp514x.h>
#include <linux/spi/spi.h>
#include <linux/spi/eeprom.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/i2c.h>
#include <mach/serial.h>
#include <mach/nand.h>
#include <mach/mmc.h>
#include <mach/usb.h>

#include "davinci.h"

#define NAND_BLOCK_SIZE		SZ_128K

static struct mtd_partition davinci_nand_partitions[] = {
	{
		
		.name		= "bootloader",
		.offset		= 0,
		.size		= 15 * NAND_BLOCK_SIZE,
		.mask_flags	= MTD_WRITEABLE, 
	}, {
		
		.name		= "params",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 1 * NAND_BLOCK_SIZE,
		.mask_flags	= 0,
	}, {
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_4M,
		.mask_flags	= 0,
	}, {
		.name		= "filesystem1",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_512M,
		.mask_flags	= 0,
	}, {
		.name		= "filesystem2",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= 0,
	}
	
};

static struct davinci_nand_pdata davinci_nand_data = {
	.mask_chipsel		= BIT(14),
	.parts			= davinci_nand_partitions,
	.nr_parts		= ARRAY_SIZE(davinci_nand_partitions),
	.ecc_mode		= NAND_ECC_HW,
	.bbt_options		= NAND_BBT_USE_FLASH,
	.ecc_bits		= 4,
};

static struct resource davinci_nand_resources[] = {
	{
		.start		= DM355_ASYNC_EMIF_DATA_CE0_BASE,
		.end		= DM355_ASYNC_EMIF_DATA_CE0_BASE + SZ_32M - 1,
		.flags		= IORESOURCE_MEM,
	}, {
		.start		= DM355_ASYNC_EMIF_CONTROL_BASE,
		.end		= DM355_ASYNC_EMIF_CONTROL_BASE + SZ_4K - 1,
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device davinci_nand_device = {
	.name			= "davinci_nand",
	.id			= 0,

	.num_resources		= ARRAY_SIZE(davinci_nand_resources),
	.resource		= davinci_nand_resources,

	.dev			= {
		.platform_data	= &davinci_nand_data,
	},
};

static struct davinci_i2c_platform_data i2c_pdata = {
	.bus_freq	= 400	,
	.bus_delay	= 0	,
	.sda_pin        = 15,
	.scl_pin        = 14,
};

static struct snd_platform_data dm355_evm_snd_data;

static int dm355evm_mmc_gpios = -EINVAL;

static void dm355evm_mmcsd_gpios(unsigned gpio)
{
	gpio_request(gpio + 0, "mmc0_ro");
	gpio_request(gpio + 1, "mmc0_cd");
	gpio_request(gpio + 2, "mmc1_ro");
	gpio_request(gpio + 3, "mmc1_cd");


	dm355evm_mmc_gpios = gpio;
}

static struct i2c_board_info dm355evm_i2c_info[] = {
	{	I2C_BOARD_INFO("dm355evm_msp", 0x25),
		.platform_data = dm355evm_mmcsd_gpios,
	},
	
	{ I2C_BOARD_INFO("tlv320aic33", 0x1b), },
};

static void __init evm_init_i2c(void)
{
	davinci_init_i2c(&i2c_pdata);

	gpio_request(5, "dm355evm_msp");
	gpio_direction_input(5);
	dm355evm_i2c_info[0].irq = gpio_to_irq(5);

	i2c_register_board_info(1, dm355evm_i2c_info,
			ARRAY_SIZE(dm355evm_i2c_info));
}

static struct resource dm355evm_dm9000_rsrc[] = {
	{
		
		.start	= 0x04014000,
		.end	= 0x04014001,
		.flags	= IORESOURCE_MEM,
	}, {
		
		.start	= 0x04014002,
		.end	= 0x04014003,
		.flags	= IORESOURCE_MEM,
	}, {
		.flags	= IORESOURCE_IRQ
			| IORESOURCE_IRQ_HIGHEDGE ,
	},
};

static struct platform_device dm355evm_dm9000 = {
	.name		= "dm9000",
	.id		= -1,
	.resource	= dm355evm_dm9000_rsrc,
	.num_resources	= ARRAY_SIZE(dm355evm_dm9000_rsrc),
};

static struct tvp514x_platform_data tvp5146_pdata = {
	.clk_polarity = 0,
	.hs_polarity = 1,
	.vs_polarity = 1
};

#define TVP514X_STD_ALL	(V4L2_STD_NTSC | V4L2_STD_PAL)
static struct v4l2_input tvp5146_inputs[] = {
	{
		.index = 0,
		.name = "Composite",
		.type = V4L2_INPUT_TYPE_CAMERA,
		.std = TVP514X_STD_ALL,
	},
	{
		.index = 1,
		.name = "S-Video",
		.type = V4L2_INPUT_TYPE_CAMERA,
		.std = TVP514X_STD_ALL,
	},
};

static struct vpfe_route tvp5146_routes[] = {
	{
		.input = INPUT_CVBS_VI2B,
		.output = OUTPUT_10BIT_422_EMBEDDED_SYNC,
	},
	{
		.input = INPUT_SVIDEO_VI2C_VI1C,
		.output = OUTPUT_10BIT_422_EMBEDDED_SYNC,
	},
};

static struct vpfe_subdev_info vpfe_sub_devs[] = {
	{
		.name = "tvp5146",
		.grp_id = 0,
		.num_inputs = ARRAY_SIZE(tvp5146_inputs),
		.inputs = tvp5146_inputs,
		.routes = tvp5146_routes,
		.can_route = 1,
		.ccdc_if_params = {
			.if_type = VPFE_BT656,
			.hdpol = VPFE_PINPOL_POSITIVE,
			.vdpol = VPFE_PINPOL_POSITIVE,
		},
		.board_info = {
			I2C_BOARD_INFO("tvp5146", 0x5d),
			.platform_data = &tvp5146_pdata,
		},
	}
};

static struct vpfe_config vpfe_cfg = {
	.num_subdevs = ARRAY_SIZE(vpfe_sub_devs),
	.i2c_adapter_id = 1,
	.sub_devs = vpfe_sub_devs,
	.card_name = "DM355 EVM",
	.ccdc = "DM355 CCDC",
};

static struct platform_device *davinci_evm_devices[] __initdata = {
	&dm355evm_dm9000,
	&davinci_nand_device,
};

static struct davinci_uart_config uart_config __initdata = {
	.enabled_uarts = (1 << 0),
};

static void __init dm355_evm_map_io(void)
{
	
	dm355_set_vpfe_config(&vpfe_cfg);
	dm355_init();
}

static int dm355evm_mmc_get_cd(int module)
{
	if (!gpio_is_valid(dm355evm_mmc_gpios))
		return -ENXIO;
	
	return !gpio_get_value_cansleep(dm355evm_mmc_gpios + 2 * module + 1);
}

static int dm355evm_mmc_get_ro(int module)
{
	if (!gpio_is_valid(dm355evm_mmc_gpios))
		return -ENXIO;
	
	return gpio_get_value_cansleep(dm355evm_mmc_gpios + 2 * module + 0);
}

static struct davinci_mmc_config dm355evm_mmc_config = {
	.get_cd		= dm355evm_mmc_get_cd,
	.get_ro		= dm355evm_mmc_get_ro,
	.wires		= 4,
	.max_freq       = 50000000,
	.caps           = MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED,
	.version	= MMC_CTLR_VERSION_1,
};

#ifdef CONFIG_USB_MUSB_PERIPHERAL
#define USB_ID_VALUE	0	
#else
#define USB_ID_VALUE	1	
#endif

static struct spi_eeprom at25640a = {
	.byte_len	= SZ_64K / 8,
	.name		= "at25640a",
	.page_size	= 32,
	.flags		= EE_ADDR2,
};

static struct spi_board_info dm355_evm_spi_info[] __initconst = {
	{
		.modalias	= "at25",
		.platform_data	= &at25640a,
		.max_speed_hz	= 10 * 1000 * 1000,	
		.bus_num	= 0,
		.chip_select	= 0,
		.mode		= SPI_MODE_0,
	},
};

static __init void dm355_evm_init(void)
{
	struct clk *aemif;

	gpio_request(1, "dm9000");
	gpio_direction_input(1);
	dm355evm_dm9000_rsrc[2].start = gpio_to_irq(1);

	aemif = clk_get(&dm355evm_dm9000.dev, "aemif");
	if (IS_ERR(aemif))
		WARN("%s: unable to get AEMIF clock\n", __func__);
	else
		clk_enable(aemif);

	platform_add_devices(davinci_evm_devices,
			     ARRAY_SIZE(davinci_evm_devices));
	evm_init_i2c();
	davinci_serial_init(&uart_config);


	gpio_request(2, "usb_id_toggle");
	gpio_direction_output(2, USB_ID_VALUE);
	
	davinci_setup_usb(1000, 8);

	davinci_setup_mmc(0, &dm355evm_mmc_config);
	davinci_setup_mmc(1, &dm355evm_mmc_config);

	dm355_init_spi0(BIT(0), dm355_evm_spi_info,
			ARRAY_SIZE(dm355_evm_spi_info));

	
	dm355_init_asp1(ASP1_TX_EVT_EN | ASP1_RX_EVT_EN, &dm355_evm_snd_data);
}

MACHINE_START(DAVINCI_DM355_EVM, "DaVinci DM355 EVM")
	.atag_offset  = 0x100,
	.map_io	      = dm355_evm_map_io,
	.init_irq     = davinci_irq_init,
	.timer	      = &davinci_timer,
	.init_machine = dm355_evm_init,
	.dma_zone_size	= SZ_128M,
	.restart	= davinci_restart,
MACHINE_END
