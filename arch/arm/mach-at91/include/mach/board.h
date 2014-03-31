/*
 * arch/arm/mach-at91/include/mach/board.h
 *
 *  Copyright (C) 2005 HP Labs
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


#ifndef __ASM_ARCH_BOARD_H
#define __ASM_ARCH_BOARD_H

#include <linux/mtd/partitions.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/spi/spi.h>
#include <linux/usb/atmel_usba_udc.h>
#include <linux/atmel-mci.h>
#include <sound/atmel-ac97c.h>
#include <linux/serial.h>
#include <linux/platform_data/macb.h>
#include <linux/platform_data/atmel.h>

 
struct at91_udc_data {
	int	vbus_pin;		
	u8	vbus_active_low;	
	u8	vbus_polled;		
	int	pullup_pin;		
	u8	pullup_active_low;	
};
extern void __init at91_add_device_udc(struct at91_udc_data *data);

 
extern void __init at91_add_device_usba(struct usba_platform_data *data);

 
struct at91_cf_data {
	int	irq_pin;		
	int	det_pin;		
	int	vcc_pin;		
	int	rst_pin;		
	u8	chipselect;		
	u8	flags;
#define AT91_CF_TRUE_IDE	0x01
#define AT91_IDE_SWAP_A0_A2	0x02
};
extern void __init at91_add_device_cf(struct at91_cf_data *data);

 
  
struct at91_mmc_data {
	int		det_pin;	
	unsigned	slot_b:1;	
	unsigned	wire4:1;	
	int		wp_pin;		
	int		vcc_pin;	
};
extern void __init at91_add_device_mmc(short mmc_id, struct at91_mmc_data *data);

  
extern void __init at91_add_device_mci(short mmc_id, struct mci_platform_data *data);

extern void __init at91_add_device_eth(struct macb_platform_data *data);

 
#define AT91_MAX_USBH_PORTS	3
struct at91_usbh_data {
	int		vbus_pin[AT91_MAX_USBH_PORTS];	
	int             overcurrent_pin[AT91_MAX_USBH_PORTS];
	u8		ports;				
	u8              overcurrent_supported;
	u8              vbus_pin_active_low[AT91_MAX_USBH_PORTS];
	u8              overcurrent_status[AT91_MAX_USBH_PORTS];
	u8              overcurrent_changed[AT91_MAX_USBH_PORTS];
};
extern void __init at91_add_device_usbh(struct at91_usbh_data *data);
extern void __init at91_add_device_usbh_ohci(struct at91_usbh_data *data);
extern void __init at91_add_device_usbh_ehci(struct at91_usbh_data *data);

extern void __init at91_add_device_nand(struct atmel_nand_data *data);

 
#if defined(CONFIG_ARCH_AT91SAM9G45)
extern void __init at91_add_device_i2c(short i2c_id, struct i2c_board_info *devices, int nr_devices);
#else
extern void __init at91_add_device_i2c(struct i2c_board_info *devices, int nr_devices);
#endif

 
extern void __init at91_add_device_spi(struct spi_board_info *devices, int nr_devices);

 
#define ATMEL_UART_CTS	0x01
#define ATMEL_UART_RTS	0x02
#define ATMEL_UART_DSR	0x04
#define ATMEL_UART_DTR	0x08
#define ATMEL_UART_DCD	0x10
#define ATMEL_UART_RI	0x20

extern void __init at91_register_uart(unsigned id, unsigned portnr, unsigned pins);
extern void __init at91_set_serial_console(unsigned portnr);

extern struct platform_device *atmel_default_console_device;

struct atmel_uart_data {
	int			num;		
	short			use_dma_tx;	
	short			use_dma_rx;	
	void __iomem		*regs;		
	struct serial_rs485	rs485;		
};
extern void __init at91_add_device_serial(void);

#define AT91_PWM0	0
#define AT91_PWM1	1
#define AT91_PWM2	2
#define AT91_PWM3	3

extern void __init at91_add_device_pwm(u32 mask);

#define ATMEL_SSC_TK	0x01
#define ATMEL_SSC_TF	0x02
#define ATMEL_SSC_TD	0x04
#define ATMEL_SSC_TX	(ATMEL_SSC_TK | ATMEL_SSC_TF | ATMEL_SSC_TD)

#define ATMEL_SSC_RK	0x10
#define ATMEL_SSC_RF	0x20
#define ATMEL_SSC_RD	0x40
#define ATMEL_SSC_RX	(ATMEL_SSC_RK | ATMEL_SSC_RF | ATMEL_SSC_RD)

extern void __init at91_add_device_ssc(unsigned id, unsigned pins);

 
struct atmel_lcdfb_info;
extern void __init at91_add_device_lcdc(struct atmel_lcdfb_info *data);

 
extern void __init at91_add_device_ac97(struct ac97c_platform_data *data);

 
struct isi_platform_data;
extern void __init at91_add_device_isi(struct isi_platform_data *data,
		bool use_pck_as_mck);

 
struct at91_tsadcc_data {
	unsigned int    adc_clock;
	u8		pendet_debounce;
	u8		ts_sample_hold_time;
};
extern void __init at91_add_device_tsadcc(struct at91_tsadcc_data *data);

struct at91_can_data {
	void (*transceiver_switch)(int on);
};
extern void __init at91_add_device_can(struct at91_can_data *data);

 
extern void __init at91_init_leds(u8 cpu_led, u8 timer_led);
extern void __init at91_gpio_leds(struct gpio_led *leds, int nr);
extern void __init at91_pwm_leds(struct gpio_led *leds, int nr);

extern int at91_suspend_entering_slow_clock(void);

#endif
