
/*
 *  linux/arch/m68knommu/platform/520x/config.c
 *
 *  Copyright (C) 2005,      Freescale (www.freescale.com)
 *  Copyright (C) 2005,      Intec Automation (mike@steroidmicros.com)
 *  Copyright (C) 1999-2007, Greg Ungerer (gerg@snapgear.com)
 *  Copyright (C) 2001-2003, SnapGear Inc. (www.snapgear.com)
 */


#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/io.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/mcfuart.h>


#if IS_ENABLED(CONFIG_SPI_COLDFIRE_QSPI)

static void __init m520x_qspi_init(void)
{
	u16 par;
	
	writeb(0x3f, MCF_GPIO_PAR_QSPI);
	
	par = readw(MCF_GPIO_PAR_UART);
	par &= 0x00ff;
	writew(par, MCF_GPIO_PAR_UART);
}

#endif 


static void __init m520x_uarts_init(void)
{
	u16 par;
	u8 par2;

	
	par = readw(MCF_GPIO_PAR_UART);
	par |= MCF_GPIO_PAR_UART_PAR_UTXD0 | MCF_GPIO_PAR_UART_PAR_URXD0;
	par |= MCF_GPIO_PAR_UART_PAR_UTXD1 | MCF_GPIO_PAR_UART_PAR_URXD1;
	writew(par, MCF_GPIO_PAR_UART);

	
	par2 = readb(MCF_GPIO_PAR_FECI2C);
	par2 &= ~0x0F;
	par2 |= MCF_GPIO_PAR_FECI2C_PAR_SCL_UTXD2 |
		MCF_GPIO_PAR_FECI2C_PAR_SDA_URXD2;
	writeb(par2, MCF_GPIO_PAR_FECI2C);
}


static void __init m520x_fec_init(void)
{
	u8 v;

	
	v = readb(MCF_GPIO_PAR_FEC);
	writeb(v | 0xf0, MCF_GPIO_PAR_FEC);

	v = readb(MCF_GPIO_PAR_FECI2C);
	writeb(v | 0x0f, MCF_GPIO_PAR_FECI2C);
}


void __init config_BSP(char *commandp, int size)
{
	mach_sched_init = hw_timer_init;
	m520x_uarts_init();
	m520x_fec_init();
#if IS_ENABLED(CONFIG_SPI_COLDFIRE_QSPI)
	m520x_qspi_init();
#endif
}

