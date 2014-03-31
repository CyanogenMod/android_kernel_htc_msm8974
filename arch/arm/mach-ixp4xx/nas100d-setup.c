/*
 * arch/arm/mach-ixp4xx/nas100d-setup.c
 *
 * NAS 100d board-setup
 *
 * Copyright (C) 2008 Rod Whitby <rod@whitby.id.au>
 *
 * based on ixdp425-setup.c:
 *      Copyright (C) 2003-2004 MontaVista Software, Inc.
 * based on nas100d-power.c:
 *	Copyright (C) 2005 Tower Technologies
 * based on nas100d-io.c
 *	Copyright (C) 2004 Karen Spearel
 *
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 * Author: Rod Whitby <rod@whitby.id.au>
 * Maintainers: http://www.nslu2-linux.org/
 *
 */
#include <linux/gpio.h>
#include <linux/if_ether.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>
#include <linux/leds.h>
#include <linux/reboot.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/io.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>

#define NAS100D_SDA_PIN		5
#define NAS100D_SCL_PIN		6

#define NAS100D_PB_GPIO         14   
#define NAS100D_RB_GPIO         4    

#define NAS100D_PO_GPIO         12   

#define NAS100D_LED_WLAN_GPIO	0
#define NAS100D_LED_DISK_GPIO	3
#define NAS100D_LED_PWR_GPIO	15

static struct flash_platform_data nas100d_flash_data = {
	.map_name		= "cfi_probe",
	.width			= 2,
};

static struct resource nas100d_flash_resource = {
	.flags			= IORESOURCE_MEM,
};

static struct platform_device nas100d_flash = {
	.name			= "IXP4XX-Flash",
	.id			= 0,
	.dev.platform_data	= &nas100d_flash_data,
	.num_resources		= 1,
	.resource		= &nas100d_flash_resource,
};

static struct i2c_board_info __initdata nas100d_i2c_board_info [] = {
	{
		I2C_BOARD_INFO("pcf8563", 0x51),
	},
};

static struct gpio_led nas100d_led_pins[] = {
	{
		.name		= "nas100d:green:wlan",
		.gpio		= NAS100D_LED_WLAN_GPIO,
		.active_low	= true,
	},
	{
		.name		= "nas100d:blue:power",  
		.gpio		= NAS100D_LED_PWR_GPIO,
		.active_low	= true,
	},
	{
		.name		= "nas100d:yellow:disk",
		.gpio		= NAS100D_LED_DISK_GPIO,
		.active_low	= true,
	},
};

static struct gpio_led_platform_data nas100d_led_data = {
	.num_leds		= ARRAY_SIZE(nas100d_led_pins),
	.leds			= nas100d_led_pins,
};

static struct platform_device nas100d_leds = {
	.name			= "leds-gpio",
	.id			= -1,
	.dev.platform_data	= &nas100d_led_data,
};

static struct i2c_gpio_platform_data nas100d_i2c_gpio_data = {
	.sda_pin		= NAS100D_SDA_PIN,
	.scl_pin		= NAS100D_SCL_PIN,
};

static struct platform_device nas100d_i2c_gpio = {
	.name			= "i2c-gpio",
	.id			= 0,
	.dev	 = {
		.platform_data	= &nas100d_i2c_gpio_data,
	},
};

static struct resource nas100d_uart_resources[] = {
	{
		.start		= IXP4XX_UART1_BASE_PHYS,
		.end		= IXP4XX_UART1_BASE_PHYS + 0x0fff,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= IXP4XX_UART2_BASE_PHYS,
		.end		= IXP4XX_UART2_BASE_PHYS + 0x0fff,
		.flags		= IORESOURCE_MEM,
	}
};

static struct plat_serial8250_port nas100d_uart_data[] = {
	{
		.mapbase	= IXP4XX_UART1_BASE_PHYS,
		.membase	= (char *)IXP4XX_UART1_BASE_VIRT + REG_OFFSET,
		.irq		= IRQ_IXP4XX_UART1,
		.flags		= UPF_BOOT_AUTOCONF,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= IXP4XX_UART_XTAL,
	},
	{
		.mapbase	= IXP4XX_UART2_BASE_PHYS,
		.membase	= (char *)IXP4XX_UART2_BASE_VIRT + REG_OFFSET,
		.irq		= IRQ_IXP4XX_UART2,
		.flags		= UPF_BOOT_AUTOCONF,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= IXP4XX_UART_XTAL,
	},
	{ }
};

static struct platform_device nas100d_uart = {
	.name			= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev.platform_data	= nas100d_uart_data,
	.num_resources		= 2,
	.resource		= nas100d_uart_resources,
};

static struct eth_plat_info nas100d_plat_eth[] = {
	{
		.phy		= 0,
		.rxq		= 3,
		.txreadyq	= 20,
	}
};

static struct platform_device nas100d_eth[] = {
	{
		.name			= "ixp4xx_eth",
		.id			= IXP4XX_ETH_NPEB,
		.dev.platform_data	= nas100d_plat_eth,
	}
};

static struct platform_device *nas100d_devices[] __initdata = {
	&nas100d_i2c_gpio,
	&nas100d_flash,
	&nas100d_leds,
	&nas100d_eth[0],
};

static void nas100d_power_off(void)
{
	

	
	gpio_line_config(NAS100D_PO_GPIO, IXP4XX_GPIO_OUT);

	
	gpio_line_set(NAS100D_PO_GPIO, IXP4XX_GPIO_HIGH);
}

static int power_button_countdown;

#define PBUTTON_HOLDDOWN_COUNT 4 

static void nas100d_power_handler(unsigned long data);
static DEFINE_TIMER(nas100d_power_timer, nas100d_power_handler, 0, 0);

static void nas100d_power_handler(unsigned long data)
{

	if (gpio_get_value(NAS100D_PB_GPIO)) {

		
		if (power_button_countdown > 0)
			power_button_countdown--;

	} else {

		
		if (power_button_countdown == 0) {
			ctrl_alt_del();

			
			gpio_line_set(NAS100D_LED_PWR_GPIO, IXP4XX_GPIO_LOW);
		} else {
			power_button_countdown = PBUTTON_HOLDDOWN_COUNT;
		}
	}

	mod_timer(&nas100d_power_timer, jiffies + msecs_to_jiffies(500));
}

static irqreturn_t nas100d_reset_handler(int irq, void *dev_id)
{
	
	machine_power_off();

	return IRQ_HANDLED;
}

static void __init nas100d_init(void)
{
	uint8_t __iomem *f;
	int i;

	ixp4xx_sys_init();

	
	*IXP4XX_GPIO_GPCLKR = 0;

	nas100d_flash_resource.start = IXP4XX_EXP_BUS_BASE(0);
	nas100d_flash_resource.end =
		IXP4XX_EXP_BUS_BASE(0) + ixp4xx_exp_bus_size - 1;

	i2c_register_board_info(0, nas100d_i2c_board_info,
				ARRAY_SIZE(nas100d_i2c_board_info));

	(void)platform_device_register(&nas100d_uart);

	platform_add_devices(nas100d_devices, ARRAY_SIZE(nas100d_devices));

	pm_power_off = nas100d_power_off;

	if (request_irq(gpio_to_irq(NAS100D_RB_GPIO), &nas100d_reset_handler,
		IRQF_DISABLED | IRQF_TRIGGER_LOW,
		"NAS100D reset button", NULL) < 0) {

		printk(KERN_DEBUG "Reset Button IRQ %d not available\n",
			gpio_to_irq(NAS100D_RB_GPIO));
	}


	
	gpio_line_config(NAS100D_PB_GPIO, IXP4XX_GPIO_IN);

	
	power_button_countdown = PBUTTON_HOLDDOWN_COUNT;

	mod_timer(&nas100d_power_timer, jiffies + msecs_to_jiffies(500));

	f = ioremap(IXP4XX_EXP_BUS_BASE(0), 0x1000000);
	if (f) {
		for (i = 0; i < 6; i++)
#ifdef __ARMEB__
			nas100d_plat_eth[0].hwaddr[i] = readb(f + 0xFC0FD8 + i);
#else
			nas100d_plat_eth[0].hwaddr[i] = readb(f + 0xFC0FD8 + (i^3));
#endif
		iounmap(f);
	}
	printk(KERN_INFO "NAS100D: Using MAC address %pM for port 0\n",
	       nas100d_plat_eth[0].hwaddr);

}

MACHINE_START(NAS100D, "Iomega NAS 100d")
	
	.atag_offset	= 0x100,
	.map_io		= ixp4xx_map_io,
	.init_early	= ixp4xx_init_early,
	.init_irq	= ixp4xx_init_irq,
	.timer          = &ixp4xx_timer,
	.init_machine	= nas100d_init,
#if defined(CONFIG_PCI)
	.dma_zone_size	= SZ_64M,
#endif
	.restart	= ixp4xx_restart,
MACHINE_END
