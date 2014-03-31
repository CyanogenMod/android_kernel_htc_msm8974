/*
 * linux/arch/arm/mach-at91/board-qil-a9260.c
 *
 *  Copyright (C) 2005 SAN People
 *  Copyright (C) 2006 Atmel
 *  Copyright (C) 2007 Calao-systems
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/clk.h>

#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/irq.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/at91sam9_smc.h>
#include <mach/at91_shdwc.h>

#include "sam9_smc.h"
#include "generic.h"


static void __init ek_init_early(void)
{
	
	at91_initialize(12000000);

	
	at91_register_uart(0, 0, 0);

	
	at91_register_uart(AT91SAM9260_ID_US0, 1, ATMEL_UART_CTS | ATMEL_UART_RTS
			   | ATMEL_UART_DTR | ATMEL_UART_DSR | ATMEL_UART_DCD
			   | ATMEL_UART_RI);

	
	at91_register_uart(AT91SAM9260_ID_US1, 2, ATMEL_UART_CTS | ATMEL_UART_RTS);

	
	at91_register_uart(AT91SAM9260_ID_US2, 3, ATMEL_UART_CTS | ATMEL_UART_RTS);

	
	at91_set_serial_console(1);

}

static struct at91_usbh_data __initdata ek_usbh_data = {
	.ports		= 2,
	.vbus_pin	= {-EINVAL, -EINVAL},
	.overcurrent_pin= {-EINVAL, -EINVAL},
};

static struct at91_udc_data __initdata ek_udc_data = {
	.vbus_pin	= AT91_PIN_PC5,
	.pullup_pin	= -EINVAL,		
};

static struct spi_board_info ek_spi_devices[] = {
#if defined(CONFIG_RTC_DRV_M41T94)
	{	
		.modalias	= "m41t94",
		.chip_select	= 0,
		.max_speed_hz	= 1 * 1000 * 1000,
		.bus_num	= 0,
	}
#endif
};

static struct macb_platform_data __initdata ek_macb_data = {
	.phy_irq_pin	= AT91_PIN_PA31,
	.is_rmii	= 1,
};

static struct mtd_partition __initdata ek_nand_partition[] = {
	{
		.name	= "Uboot & Kernel",
		.offset	= 0,
		.size	= SZ_16M,
	},
	{
		.name	= "Root FS",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= 120 * SZ_1M,
	},
	{
		.name	= "FS",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= 120 * SZ_1M,
	},
};

static struct atmel_nand_data __initdata ek_nand_data = {
	.ale		= 21,
	.cle		= 22,
	.det_pin	= -EINVAL,
	.rdy_pin	= AT91_PIN_PC13,
	.enable_pin	= AT91_PIN_PC14,
	.ecc_mode	= NAND_ECC_SOFT,
	.on_flash_bbt	= 1,
	.parts		= ek_nand_partition,
	.num_parts	= ARRAY_SIZE(ek_nand_partition),
};

static struct sam9_smc_config __initdata ek_nand_smc_config = {
	.ncs_read_setup		= 0,
	.nrd_setup		= 1,
	.ncs_write_setup	= 0,
	.nwe_setup		= 1,

	.ncs_read_pulse		= 3,
	.nrd_pulse		= 3,
	.ncs_write_pulse	= 3,
	.nwe_pulse		= 3,

	.read_cycle		= 5,
	.write_cycle		= 5,

	.mode			= AT91_SMC_READMODE | AT91_SMC_WRITEMODE | AT91_SMC_EXNWMODE_DISABLE | AT91_SMC_DBW_8,
	.tdf_cycles		= 2,
};

static void __init ek_add_device_nand(void)
{
	
	sam9_smc_configure(0, 3, &ek_nand_smc_config);

	at91_add_device_nand(&ek_nand_data);
}

static struct at91_mmc_data __initdata ek_mmc_data = {
	.slot_b		= 0,
	.wire4		= 1,
	.det_pin	= -EINVAL,
	.wp_pin		= -EINVAL,
	.vcc_pin	= -EINVAL,
};

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
static struct gpio_keys_button ek_buttons[] = {
	{	
		.code		= KEY_ENTER,
		.gpio		= AT91_PIN_PB10,
		.active_low	= 1,
		.desc		= "user_pb",
		.wakeup		= 1,
	}
};

static struct gpio_keys_platform_data ek_button_data = {
	.buttons	= ek_buttons,
	.nbuttons	= ARRAY_SIZE(ek_buttons),
};

static struct platform_device ek_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &ek_button_data,
	}
};

static void __init ek_add_device_buttons(void)
{
	at91_set_GPIO_periph(AT91_PIN_PB10, 1);	
	at91_set_deglitch(AT91_PIN_PB10, 1);

	platform_device_register(&ek_button_device);
}
#else
static void __init ek_add_device_buttons(void) {}
#endif

static struct gpio_led ek_leds[] = {
	{	
		.name			= "user_led",
		.gpio			= AT91_PIN_PB21,
		.active_low		= 0,
		.default_trigger	= "heartbeat",
	}
};

static void __init ek_board_init(void)
{
	
	at91_add_device_serial();
	
	at91_add_device_usbh(&ek_usbh_data);
	
	at91_add_device_udc(&ek_udc_data);
	
	at91_add_device_spi(ek_spi_devices, ARRAY_SIZE(ek_spi_devices));
	
	ek_add_device_nand();
	
	at91_add_device_i2c(NULL, 0);
	
	at91_add_device_eth(&ek_macb_data);
	
	at91_add_device_mmc(0, &ek_mmc_data);
	
	ek_add_device_buttons();
	
	at91_gpio_leds(ek_leds, ARRAY_SIZE(ek_leds));
	
	at91_shdwc_write(AT91_SHDW_MR, AT91_SHDW_CPTWK0_(10) | AT91_SHDW_WKMODE0_LOW
				| AT91_SHDW_RTTWKEN);
}

MACHINE_START(QIL_A9260, "CALAO QIL_A9260")
	
	.timer		= &at91sam926x_timer,
	.map_io		= at91_map_io,
	.init_early	= ek_init_early,
	.init_irq	= at91_init_irq_default,
	.init_machine	= ek_board_init,
MACHINE_END
