
/*
 *	linux/arch/m68knommu/platform/528x/config.c
 *
 *	Sub-architcture dependent initialization code for the Freescale
 *	5280, 5281 and 5282 CPUs.
 *
 *	Copyright (C) 1999-2003, Greg Ungerer (gerg@snapgear.com)
 *	Copyright (C) 2001-2003, SnapGear Inc. (www.snapgear.com)
 */


#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/mcfuart.h>


#if IS_ENABLED(CONFIG_SPI_COLDFIRE_QSPI)

static void __init m528x_qspi_init(void)
{
	
	__raw_writeb(0x07, MCFGPIO_PQSPAR);
}

#endif 


static void __init m528x_uarts_init(void)
{
	u8 port;

	
	port = readb(MCF5282_GPIO_PUAPAR);
	port |= 0x03 | (0x03 << 2);
	writeb(port, MCF5282_GPIO_PUAPAR);
}


static void __init m528x_fec_init(void)
{
	u16 v16;

	
	v16 = readw(MCF_IPSBAR + 0x100056);
	writew(v16 | 0xf00, MCF_IPSBAR + 0x100056);
	writeb(0xc0, MCF_IPSBAR + 0x100058);
}


#ifdef CONFIG_WILDFIRE
void wildfire_halt(void)
{
	writeb(0, 0x30000007);
	writeb(0x2, 0x30000007);
}
#endif

#ifdef CONFIG_WILDFIREMOD
void wildfiremod_halt(void)
{
	printk(KERN_INFO "WildFireMod hibernating...\n");

	
	MCF5282_GPIO_PEPAR &= ~(1 << (5 * 2));

	
	MCF5282_GPIO_DDRE |= (1 << 5);

	
	MCF5282_GPIO_PORTE &= ~(1 << 5);
	MCF5282_GPIO_PORTE |= (1 << 5);

	printk(KERN_EMERG "Failed to hibernate. Halting!\n");
}
#endif

void __init config_BSP(char *commandp, int size)
{
#ifdef CONFIG_WILDFIRE
	mach_halt = wildfire_halt;
#endif
#ifdef CONFIG_WILDFIREMOD
	mach_halt = wildfiremod_halt;
#endif
	mach_sched_init = hw_timer_init;
	m528x_uarts_init();
	m528x_fec_init();
#if IS_ENABLED(CONFIG_SPI_COLDFIRE_QSPI)
	m528x_qspi_init();
#endif
}

