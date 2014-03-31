#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/mtd/physmap.h>
#include <linux/spi/flash.h>
#include <linux/spi/spi.h>
#include <linux/spi/orion_spi.h>
#include <linux/serial_reg.h>
#include <mach/kirkwood.h>
#include "common.h"



struct mtd_partition qnap_tsx1x_partitions[] = {
	{
		.name		= "U-Boot",
		.size		= 0x00080000,
		.offset		= 0,
		.mask_flags	= MTD_WRITEABLE,
	}, {
		.name		= "Kernel",
		.size		= 0x00200000,
		.offset		= 0x00200000,
	}, {
		.name		= "RootFS1",
		.size		= 0x00900000,
		.offset		= 0x00400000,
	}, {
		.name		= "RootFS2",
		.size		= 0x00300000,
		.offset		= 0x00d00000,
	}, {
		.name		= "U-Boot Config",
		.size		= 0x00040000,
		.offset		= 0x00080000,
	}, {
		.name		= "NAS Config",
		.size		= 0x00140000,
		.offset		= 0x000c0000,
	},
};

const struct flash_platform_data qnap_tsx1x_flash = {
	.type		= "m25p128",
	.name		= "spi_flash",
	.parts		= qnap_tsx1x_partitions,
	.nr_parts	= ARRAY_SIZE(qnap_tsx1x_partitions),
};

struct spi_board_info __initdata qnap_tsx1x_spi_slave_info[] = {
	{
		.modalias	= "m25p80",
		.platform_data	= &qnap_tsx1x_flash,
		.irq		= -1,
		.max_speed_hz	= 20000000,
		.bus_num	= 0,
		.chip_select	= 0,
	},
};

void __init qnap_tsx1x_register_flash(void)
{
	spi_register_board_info(qnap_tsx1x_spi_slave_info,
				ARRAY_SIZE(qnap_tsx1x_spi_slave_info));
	kirkwood_spi_init();
}



#define UART1_REG(x)	(UART1_VIRT_BASE + ((UART_##x) << 2))

void qnap_tsx1x_power_off(void)
{
	
	const unsigned divisor = ((kirkwood_tclk + (8 * 19200)) / (16 * 19200));

	pr_info("%s: triggering power-off...\n", __func__);

	
	writel(0x83, UART1_REG(LCR));
	writel(divisor & 0xff, UART1_REG(DLL));
	writel((divisor >> 8) & 0xff, UART1_REG(DLM));
	writel(0x03, UART1_REG(LCR));
	writel(0x00, UART1_REG(IER));
	writel(0x00, UART1_REG(FCR));
	writel(0x00, UART1_REG(MCR));

	
	writel('A', UART1_REG(TX));
}

