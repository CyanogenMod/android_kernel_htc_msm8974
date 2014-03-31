/*
 *  Copyright (C) 2010 Christian Glindkamp <christian.glindkamp@taskit.de>
 *                     taskit GmbH
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

#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/w1-gpio.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/board.h>
#include <mach/at91sam9_smc.h>
#include <mach/stamp9g20.h>

#include "sam9_smc.h"
#include "generic.h"


static void __init pcontrol_g20_init_early(void)
{
	stamp9g20_init_early();

	
	at91_register_uart(AT91SAM9260_ID_US0, 1, ATMEL_UART_CTS
						| ATMEL_UART_RTS);

	
	at91_register_uart(AT91SAM9260_ID_US1, 2, ATMEL_UART_CTS
						| ATMEL_UART_RTS);

	
	at91_register_uart(AT91SAM9260_ID_US4, 3, 0);
}

static struct sam9_smc_config __initdata pcontrol_smc_config[2] = { {
	.ncs_read_setup		= 16,
	.nrd_setup		= 18,
	.ncs_write_setup	= 16,
	.nwe_setup		= 18,

	.ncs_read_pulse		= 63,
	.nrd_pulse		= 55,
	.ncs_write_pulse	= 63,
	.nwe_pulse		= 55,

	.read_cycle		= 127,
	.write_cycle		= 127,

	.mode			= AT91_SMC_READMODE | AT91_SMC_WRITEMODE
			| AT91_SMC_EXNWMODE_DISABLE | AT91_SMC_BAT_SELECT
			| AT91_SMC_DBW_8 | AT91_SMC_PS_4
			| AT91_SMC_TDFMODE,
	.tdf_cycles		= 3,
}, {
	.ncs_read_setup		= 0,
	.nrd_setup		= 0,
	.ncs_write_setup	= 0,
	.nwe_setup		= 1,

	.ncs_read_pulse		= 8,
	.nrd_pulse		= 8,
	.ncs_write_pulse	= 5,
	.nwe_pulse		= 4,

	.read_cycle		= 8,
	.write_cycle		= 7,

	.mode			= AT91_SMC_READMODE | AT91_SMC_WRITEMODE
			| AT91_SMC_EXNWMODE_DISABLE | AT91_SMC_BAT_SELECT
			| AT91_SMC_DBW_16 | AT91_SMC_PS_8
			| AT91_SMC_TDFMODE,
	.tdf_cycles		= 1,
} };

static void __init add_device_pcontrol(void)
{
	
	sam9_smc_configure(0, 4, &pcontrol_smc_config[0]);
	
	sam9_smc_configure(0, 7, &pcontrol_smc_config[1]);
}


static struct at91_usbh_data __initdata usbh_data = {
	.ports		= 2,
	.vbus_pin	= {-EINVAL, -EINVAL},
	.overcurrent_pin= {-EINVAL, -EINVAL},
};


static struct at91_udc_data __initdata pcontrol_g20_udc_data = {
	.vbus_pin	= AT91_PIN_PA22,	
	.pullup_pin	= AT91_PIN_PA4,		
};


static struct macb_platform_data __initdata macb_data = {
	.phy_irq_pin	= AT91_PIN_PA28,
	.is_rmii	= 1,
};


static struct i2c_board_info __initdata pcontrol_g20_i2c_devices[] = {
{		
	I2C_BOARD_INFO("24c64", 0x50)
}, {		
	I2C_BOARD_INFO("lan9303", 0x0a)
}, };


static struct gpio_led pcontrol_g20_leds[] = {
	{
		.name			= "LED1",	
		.gpio			= AT91_PIN_PB18,
		.active_low		= 1,
		.default_trigger	= "none",	
	}, {
		.name			= "LED2",	
		.gpio			= AT91_PIN_PB19,
		.active_low		= 1,
		.default_trigger	= "mmc0",	
	}, {
		.name			= "LED3",	
		.gpio			= AT91_PIN_PB20,
		.active_low		= 1,
		.default_trigger	= "heartbeat",	
	}, {
		.name			= "LED4",	
		.gpio			= AT91_PIN_PC6,
		.active_low		= 1,
		.default_trigger	= "none",	
	}, {
		.name			= "LED5",	
		.gpio			= AT91_PIN_PC7,
		.active_low		= 1,
		.default_trigger	= "none",	
	}, {
		.name			= "LED6",	
		.gpio			= AT91_PIN_PC9,
		.active_low		= 1,
		.default_trigger	= "none",	
	}
};


static struct spi_board_info pcontrol_g20_spi_devices[] = {
	{
		.modalias	= "spidev",	
		.chip_select	= 1,
		.max_speed_hz	= 50 * 1000 * 1000,
		.bus_num	= 0,
	}, {
		.modalias	= "spidev",	
		.chip_select	= 0,
		.max_speed_hz	= 50 * 1000 * 1000,
		.bus_num	= 1,
	},
};


static void __init pcontrol_g20_board_init(void)
{
	stamp9g20_board_init();
	at91_add_device_usbh(&usbh_data);
	at91_add_device_eth(&macb_data);
	at91_add_device_i2c(pcontrol_g20_i2c_devices,
		ARRAY_SIZE(pcontrol_g20_i2c_devices));
	add_device_pcontrol();
	at91_add_device_spi(pcontrol_g20_spi_devices,
		ARRAY_SIZE(pcontrol_g20_spi_devices));
	at91_add_device_udc(&pcontrol_g20_udc_data);
	at91_gpio_leds(pcontrol_g20_leds,
		ARRAY_SIZE(pcontrol_g20_leds));
	
	at91_set_gpio_output(AT91_PIN_PB31, 1);
}


MACHINE_START(PCONTROL_G20, "PControl G20")
	
	.timer		= &at91sam926x_timer,
	.map_io		= at91_map_io,
	.init_early	= pcontrol_g20_init_early,
	.init_irq	= at91_init_irq_default,
	.init_machine	= pcontrol_g20_board_init,
MACHINE_END
