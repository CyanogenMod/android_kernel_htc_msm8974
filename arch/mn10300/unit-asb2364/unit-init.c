/* ASB2364 initialisation
 *
 * Copyright (C) 2002 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/setup.h>
#include <asm/processor.h>
#include <asm/irq.h>
#include <asm/intctl-regs.h>
#include <asm/serial-regs.h>
#include <unit/fpga-regs.h>
#include <unit/serial.h>
#include <unit/smsc911x.h>

#define TTYS0_SERIAL_IER	__SYSREG(SERIAL_PORT0_BASE_ADDRESS + UART_IER * 2, u8)
#define LAN_IRQ_CFG		__SYSREG(SMSC911X_BASE + 0x54, u32)
#define LAN_INT_EN		__SYSREG(SMSC911X_BASE + 0x5c, u32)

asmlinkage void __init unit_init(void)
{
	
	TTYS0_SERIAL_IER = 0;
	SC0RXICR = 0;
	SC0TXICR = 0;
	SC1RXICR = 0;
	SC1TXICR = 0;
	SC2RXICR = 0;
	SC2TXICR = 0;

	
	ASB2364_FPGA_REG_RESET_LAN = 0x0000;
	SyncExBus();
	ASB2364_FPGA_REG_RESET_UART = 0x0000;
	SyncExBus();
	ASB2364_FPGA_REG_RESET_I2C = 0x0000;
	SyncExBus();
	ASB2364_FPGA_REG_RESET_USB = 0x0000;
	SyncExBus();
	ASB2364_FPGA_REG_RESET_AV = 0x0000;
	SyncExBus();

	

	
	

	
	SET_XIRQ_TRIGGER(1, XIRQ_TRIGGER_LOWLEVEL);

	
	

#if defined(CONFIG_EXT_SERIAL_IRQ_LEVEL) &&	\
    defined(CONFIG_ETHERNET_IRQ_LEVEL) &&	\
    (CONFIG_EXT_SERIAL_IRQ_LEVEL != CONFIG_ETHERNET_IRQ_LEVEL)
# error CONFIG_EXT_SERIAL_IRQ_LEVEL != CONFIG_ETHERNET_IRQ_LEVEL
#endif

#if defined(CONFIG_EXT_SERIAL_IRQ_LEVEL)
	set_intr_level(XIRQ1, NUM2GxICR_LEVEL(CONFIG_EXT_SERIAL_IRQ_LEVEL));
#elif defined(CONFIG_ETHERNET_IRQ_LEVEL)
	set_intr_level(XIRQ1, NUM2GxICR_LEVEL(CONFIG_ETHERNET_IRQ_LEVEL));
#endif
}

asmlinkage void __init unit_setup(void)
{
	ASB2364_FPGA_REG_RESET_LAN = 0x0001;
	SyncExBus();
	ASB2364_FPGA_REG_RESET_UART = 0x0001;
	SyncExBus();
	ASB2364_FPGA_REG_RESET_I2C = 0x0001;
	SyncExBus();
	ASB2364_FPGA_REG_RESET_USB = 0x0001;
	SyncExBus();
	ASB2364_FPGA_REG_RESET_AV = 0x0001;
	SyncExBus();

	LAN_IRQ_CFG = 0;
	LAN_INT_EN = 0;
}

void __init unit_init_IRQ(void)
{
	unsigned int extnum;

	for (extnum = 0 ; extnum < NR_XIRQS ; extnum++) {
		switch (GET_XIRQ_TRIGGER(extnum)) {
		case XIRQ_TRIGGER_HILEVEL:
		case XIRQ_TRIGGER_LOWLEVEL:
			mn10300_set_lateack_irq_type(XIRQ2IRQ(extnum));
			break;
		default:
			break;
		}
	}

#define IRQCTL	__SYSREG(0xd5000090, u32)
	IRQCTL |= 0x02;

	irq_fpga_init();
}
