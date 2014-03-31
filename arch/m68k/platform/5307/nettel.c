
/*
 *	nettel.c -- startup code support for the NETtel boards
 *
 *	Copyright (C) 2009, Greg Ungerer (gerg@snapgear.com)
 */


#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/nettel.h>


#define	NETTEL_SMC0_ADDR	0x30600300
#define	NETTEL_SMC0_IRQ		29

#define	NETTEL_SMC1_ADDR	0x30600000
#define	NETTEL_SMC1_IRQ		27

#define	SMC91xx_BANKSELECT	14
#define	SMC91xx_BASEADDR	2
#define	SMC91xx_BASEMAC		4


static struct resource nettel_smc91x_0_resources[] = {
	{
		.start		= NETTEL_SMC0_ADDR,
		.end		= NETTEL_SMC0_ADDR + 0x20,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= NETTEL_SMC0_IRQ,
		.end		= NETTEL_SMC0_IRQ,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct resource nettel_smc91x_1_resources[] = {
	{
		.start		= NETTEL_SMC1_ADDR,
		.end		= NETTEL_SMC1_ADDR + 0x20,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= NETTEL_SMC1_IRQ,
		.end		= NETTEL_SMC1_IRQ,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device nettel_smc91x[] = {
	{
		.name			= "smc91x",
		.id			= 0,
		.num_resources		= ARRAY_SIZE(nettel_smc91x_0_resources),
		.resource		= nettel_smc91x_0_resources,
	},
	{
		.name			= "smc91x",
		.id			= 1,
		.num_resources		= ARRAY_SIZE(nettel_smc91x_1_resources),
		.resource		= nettel_smc91x_1_resources,
	},
};

static struct platform_device *nettel_devices[] __initdata = {
	&nettel_smc91x[0],
	&nettel_smc91x[1],
};


static u8 nettel_macdefault[] __initdata = {
	0x00, 0xd0, 0xcf, 0x00, 0x00, 0x01,
};


static void __init nettel_smc91x_setmac(unsigned int ioaddr, unsigned int flashaddr)
{
	u16 *macp;

	macp = (u16 *) flashaddr;
	if ((macp[0] == 0xffff) && (macp[1] == 0xffff) && (macp[2] == 0xffff))
		macp = (u16 *) &nettel_macdefault[0];

	writew(1, NETTEL_SMC0_ADDR + SMC91xx_BANKSELECT);
	writew(macp[0], ioaddr + SMC91xx_BASEMAC);
	writew(macp[1], ioaddr + SMC91xx_BASEMAC + 2);
	writew(macp[2], ioaddr + SMC91xx_BASEMAC + 4);
}



static void __init nettel_smc91x_init(void)
{
	writew(0x00ec, MCF_MBAR + MCFSIM_PADDR);
	mcf_setppdata(0, 0x0080);
	writew(1, NETTEL_SMC0_ADDR + SMC91xx_BANKSELECT);
	writew(0x0067, NETTEL_SMC0_ADDR + SMC91xx_BASEADDR);
	mcf_setppdata(0x0080, 0);

	
	writew(0x1180, MCF_MBAR + MCFSIM_CSCR3);

	
	mcf_autovector(NETTEL_SMC0_IRQ);
	mcf_autovector(NETTEL_SMC1_IRQ);

	
	nettel_smc91x_setmac(NETTEL_SMC0_ADDR, 0xf0006000);
	nettel_smc91x_setmac(NETTEL_SMC1_ADDR, 0xf0006006);
}


static int __init init_nettel(void)
{
	nettel_smc91x_init();
	platform_add_devices(nettel_devices, ARRAY_SIZE(nettel_devices));
	return 0;
}

arch_initcall(init_nettel);

