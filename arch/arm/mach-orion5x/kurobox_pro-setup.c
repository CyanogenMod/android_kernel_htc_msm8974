/*
 * arch/arm/mach-orion5x/kurobox_pro-setup.c
 *
 * Maintainer: Ronen Shitrit <rshitrit@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/nand.h>
#include <linux/mv643xx_eth.h>
#include <linux/i2c.h>
#include <linux/serial_reg.h>
#include <linux/ata_platform.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/pci.h>
#include <mach/orion5x.h>
#include <plat/orion_nand.h>
#include "common.h"
#include "mpp.h"



#define KUROBOX_PRO_NOR_BOOT_BASE	0xf4000000
#define KUROBOX_PRO_NOR_BOOT_SIZE	SZ_256K


#define KUROBOX_PRO_NAND_BASE		0xfc000000
#define KUROBOX_PRO_NAND_SIZE		SZ_2M


static struct mtd_partition kurobox_pro_nand_parts[] = {
	{
		.name	= "uImage",
		.offset	= 0,
		.size	= SZ_4M,
	}, {
		.name	= "rootfs",
		.offset	= SZ_4M,
		.size	= SZ_64M,
	}, {
		.name	= "extra",
		.offset	= SZ_4M + SZ_64M,
		.size	= SZ_256M - (SZ_4M + SZ_64M),
	},
};

static struct resource kurobox_pro_nand_resource = {
	.flags		= IORESOURCE_MEM,
	.start		= KUROBOX_PRO_NAND_BASE,
	.end		= KUROBOX_PRO_NAND_BASE + KUROBOX_PRO_NAND_SIZE - 1,
};

static struct orion_nand_data kurobox_pro_nand_data = {
	.parts		= kurobox_pro_nand_parts,
	.nr_parts	= ARRAY_SIZE(kurobox_pro_nand_parts),
	.cle		= 0,
	.ale		= 1,
	.width		= 8,
};

static struct platform_device kurobox_pro_nand_flash = {
	.name		= "orion_nand",
	.id		= -1,
	.dev		= {
		.platform_data	= &kurobox_pro_nand_data,
	},
	.resource	= &kurobox_pro_nand_resource,
	.num_resources	= 1,
};


static struct physmap_flash_data kurobox_pro_nor_flash_data = {
	.width		= 1,
};

static struct resource kurobox_pro_nor_flash_resource = {
	.flags			= IORESOURCE_MEM,
	.start			= KUROBOX_PRO_NOR_BOOT_BASE,
	.end			= KUROBOX_PRO_NOR_BOOT_BASE + KUROBOX_PRO_NOR_BOOT_SIZE - 1,
};

static struct platform_device kurobox_pro_nor_flash = {
	.name			= "physmap-flash",
	.id			= 0,
	.dev		= {
		.platform_data	= &kurobox_pro_nor_flash_data,
	},
	.num_resources		= 1,
	.resource		= &kurobox_pro_nor_flash_resource,
};


static int __init kurobox_pro_pci_map_irq(const struct pci_dev *dev, u8 slot,
	u8 pin)
{
	int irq;

	irq = orion5x_pci_map_irq(dev, slot, pin);
	if (irq != -1)
		return irq;

	return -1;
}

static struct hw_pci kurobox_pro_pci __initdata = {
	.nr_controllers	= 2,
	.swizzle	= pci_std_swizzle,
	.setup		= orion5x_pci_sys_setup,
	.scan		= orion5x_pci_sys_scan_bus,
	.map_irq	= kurobox_pro_pci_map_irq,
};

static int __init kurobox_pro_pci_init(void)
{
	if (machine_is_kurobox_pro()) {
		orion5x_pci_disable();
		pci_common_init(&kurobox_pro_pci);
	}

	return 0;
}

subsys_initcall(kurobox_pro_pci_init);


static struct mv643xx_eth_platform_data kurobox_pro_eth_data = {
	.phy_addr	= MV643XX_ETH_PHY_ADDR(8),
};

static struct i2c_board_info __initdata kurobox_pro_i2c_rtc = {
	I2C_BOARD_INFO("rs5c372a", 0x32),
};

static struct mv_sata_platform_data kurobox_pro_sata_data = {
	.n_ports	= 2,
};


#define UART1_REG(x)	(UART1_VIRT_BASE + ((UART_##x) << 2))

static int kurobox_pro_miconread(unsigned char *buf, int count)
{
	int i;
	int timeout;

	for (i = 0; i < count; i++) {
		timeout = 10;

		while (!(readl(UART1_REG(LSR)) & UART_LSR_DR)) {
			if (--timeout == 0)
				break;
			udelay(1000);
		}

		if (timeout == 0)
			break;
		buf[i] = readl(UART1_REG(RX));
	}

	
	return i;
}

static int kurobox_pro_miconwrite(const unsigned char *buf, int count)
{
	int i = 0;

	while (count--) {
		while (!(readl(UART1_REG(LSR)) & UART_LSR_THRE))
			barrier();
		writel(buf[i++], UART1_REG(TX));
	}

	return 0;
}

static int kurobox_pro_miconsend(const unsigned char *data, int count)
{
	int i;
	unsigned char checksum = 0;
	unsigned char recv_buf[40];
	unsigned char send_buf[40];
	unsigned char correct_ack[3];
	int retry = 2;

	
	for (i = 0; i < count; i++)
		checksum -=  data[i];

	do {
		
		kurobox_pro_miconwrite(data, count);

		
		kurobox_pro_miconwrite(&checksum, 1);

		if (kurobox_pro_miconread(recv_buf, sizeof(recv_buf)) <= 3) {
			printk(KERN_ERR ">%s: receive failed.\n", __func__);

			
			memset(&send_buf, 0xff, sizeof(send_buf));
			kurobox_pro_miconwrite(send_buf, sizeof(send_buf));

			
			mdelay(100);
			kurobox_pro_miconread(recv_buf, sizeof(recv_buf));
		} else {
			
			correct_ack[0] = 0x01;
			correct_ack[1] = data[1];
			correct_ack[2] = 0x00;

			
			if ((recv_buf[0] + recv_buf[1] + recv_buf[2] +
			     recv_buf[3]) & 0xFF) {
				printk(KERN_ERR ">%s: Checksum Error : "
					"Received data[%02x, %02x, %02x, %02x]"
					"\n", __func__, recv_buf[0],
					recv_buf[1], recv_buf[2], recv_buf[3]);
			} else {
				
				if (correct_ack[0] == recv_buf[0] &&
				    correct_ack[1] == recv_buf[1] &&
				    correct_ack[2] == recv_buf[2]) {
					
					mdelay(10);

					
					return 0;
				}
			}
			
			printk(KERN_ERR ">%s: Error : NAK or Illegal Data "
					"Received\n", __func__);
		}
	} while (retry--);

	
	mdelay(10);

	return -1;
}

static void kurobox_pro_power_off(void)
{
	const unsigned char watchdogkill[]	= {0x01, 0x35, 0x00};
	const unsigned char shutdownwait[]	= {0x00, 0x0c};
	const unsigned char poweroff[]		= {0x00, 0x06};
	
	const unsigned divisor = ((orion5x_tclk + (8 * 38400)) / (16 * 38400));

	pr_info("%s: triggering power-off...\n", __func__);

	
	writel(0x83, UART1_REG(LCR));
	writel(divisor & 0xff, UART1_REG(DLL));
	writel((divisor >> 8) & 0xff, UART1_REG(DLM));
	writel(0x1b, UART1_REG(LCR));
	writel(0x00, UART1_REG(IER));
	writel(0x07, UART1_REG(FCR));
	writel(0x00, UART1_REG(MCR));

	
	kurobox_pro_miconsend(watchdogkill, sizeof(watchdogkill)) ;
	kurobox_pro_miconsend(shutdownwait, sizeof(shutdownwait)) ;
	kurobox_pro_miconsend(poweroff, sizeof(poweroff));
}

static unsigned int kurobox_pro_mpp_modes[] __initdata = {
	MPP0_UNUSED,
	MPP1_UNUSED,
	MPP2_GPIO,		
	MPP3_GPIO,		
	MPP4_UNUSED,
	MPP5_UNUSED,
	MPP6_NAND,		
	MPP7_NAND,		
	MPP8_UNUSED,
	MPP9_UNUSED,
	MPP10_UNUSED,
	MPP11_UNUSED,
	MPP12_SATA_LED,		
	MPP13_SATA_LED,		
	MPP14_SATA_LED,		
	MPP15_SATA_LED,		
	MPP16_UART,		
	MPP17_UART,		
	MPP18_UART,		
	MPP19_UART,		
	0,
};

static void __init kurobox_pro_init(void)
{
	orion5x_init();

	orion5x_mpp_conf(kurobox_pro_mpp_modes);

	orion5x_ehci0_init();
	orion5x_ehci1_init();
	orion5x_eth_init(&kurobox_pro_eth_data);
	orion5x_i2c_init();
	orion5x_sata_init(&kurobox_pro_sata_data);
	orion5x_uart0_init();
	orion5x_uart1_init();
	orion5x_xor_init();

	orion5x_setup_dev_boot_win(KUROBOX_PRO_NOR_BOOT_BASE,
				   KUROBOX_PRO_NOR_BOOT_SIZE);
	platform_device_register(&kurobox_pro_nor_flash);

	if (machine_is_kurobox_pro()) {
		orion5x_setup_dev0_win(KUROBOX_PRO_NAND_BASE,
				       KUROBOX_PRO_NAND_SIZE);
		platform_device_register(&kurobox_pro_nand_flash);
	}

	i2c_register_board_info(0, &kurobox_pro_i2c_rtc, 1);

	
	pm_power_off = kurobox_pro_power_off;
}

#ifdef CONFIG_MACH_KUROBOX_PRO
MACHINE_START(KUROBOX_PRO, "Buffalo/Revogear Kurobox Pro")
	
	.atag_offset	= 0x100,
	.init_machine	= kurobox_pro_init,
	.map_io		= orion5x_map_io,
	.init_early	= orion5x_init_early,
	.init_irq	= orion5x_init_irq,
	.timer		= &orion5x_timer,
	.fixup		= tag_fixup_mem32,
	.restart	= orion5x_restart,
MACHINE_END
#endif

#ifdef CONFIG_MACH_LINKSTATION_PRO
MACHINE_START(LINKSTATION_PRO, "Buffalo Linkstation Pro/Live")
	
	.atag_offset	= 0x100,
	.init_machine	= kurobox_pro_init,
	.map_io		= orion5x_map_io,
	.init_early	= orion5x_init_early,
	.init_irq	= orion5x_init_irq,
	.timer		= &orion5x_timer,
	.fixup		= tag_fixup_mem32,
	.restart	= orion5x_restart,
MACHINE_END
#endif
