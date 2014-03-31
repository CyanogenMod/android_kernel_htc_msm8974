
/*
 *  linux/arch/m68knommu/platform/68VZ328/config.c
 *
 *  Copyright (C) 1993 Hamish Macdonald
 *  Copyright (C) 1999 D. Jeff Dionne
 *  Copyright (C) 2001 Georges Menie, Ken Desmet
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */


#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kd.h>
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/rtc.h>

#include <asm/pgtable.h>
#include <asm/machdep.h>
#include <asm/MC68VZ328.h>
#include <asm/bootstd.h>

#ifdef CONFIG_INIT_LCD
#include "bootlogo.h"
#endif


int m68328_hwclk(int set, struct rtc_time *t);

#if defined(CONFIG_DRAGEN2)

static void m68vz328_reset(void)
{
	local_irq_disable();

#ifdef CONFIG_INIT_LCD
	PBDATA |= 0x20;				
	PKDATA |= 0x4;				
	LCKCON = 0;
#endif

	__asm__ __volatile__(
		"reset\n\t"
		"moveal #0x04000000, %a0\n\t"
		"moveal 0(%a0), %sp\n\t"
		"moveal 4(%a0), %a0\n\t"
		"jmp (%a0)"
	);
}

static void init_hardware(char *command, int size)
{
#ifdef CONFIG_DIRECT_IO_ACCESS
	SCR = 0x10;					
#endif

	
	CSGBB = 0x4000;
	CSB = 0x1a1;

	
	
	PKSEL |= PK(3);				
	PKDIR |= PK(3);				
	PKDATA |= PK(3);			

	
	PFSEL |= PF(5);				
	PFDIR |= PF(5);				
	PFDATA &= ~PF(5);			

	
	PFDATA |= PF(5);
	{ int i; for (i = 0; i < 32000; ++i); }
	PFDATA &= ~PF(5);

	
	PDPOL &= ~PD(1);			
	PDIQEG &= ~PD(1);
	PDIRQEN |= PD(1);			

#ifdef CONFIG_INIT_LCD
	
	LSSA = (long) screen_bits;
	LVPW = 0x14;
	LXMAX = 0x140;
	LYMAX = 0xef;
	LRRA = 0;
	LPXCD = 3;
	LPICF = 0x08;
	LPOLCF = 0;
	LCKCON = 0x80;
	PCPDEN = 0xff;
	PCSEL = 0;

	
	PKDIR |= 0x4;
	PKSEL |= 0x4;
	PKDATA &= ~0x4;

	
	PBDIR |= 0x20;
	PBSEL |= 0x20;
	PBDATA &= ~0x20;

	
	PFDIR |= 0x1;
	PFSEL &= ~0x1;
	PWMR = 0x037F;
#endif
}

#elif defined(CONFIG_UCDIMM)

static void m68vz328_reset(void)
{
	local_irq_disable();
	asm volatile (
		"moveal #0x10c00000, %a0;\n\t"
		"moveb #0, 0xFFFFF300;\n\t"
		"moveal 0(%a0), %sp;\n\t"
		"moveal 4(%a0), %a0;\n\t"
		"jmp (%a0);\n"
	);
}

unsigned char *cs8900a_hwaddr;
static int errno;

_bsc0(char *, getserialnum)
_bsc1(unsigned char *, gethwaddr, int, a)
_bsc1(char *, getbenv, char *, a)

static void init_hardware(char *command, int size)
{
	char *p;

	printk(KERN_INFO "uCdimm serial string [%s]\n", getserialnum());
	p = cs8900a_hwaddr = gethwaddr(0);
	printk(KERN_INFO "uCdimm hwaddr %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
		p[0], p[1], p[2], p[3], p[4], p[5]);
	p = getbenv("APPEND");
	if (p)
		strcpy(p, command);
	else
		command[0] = 0;
}

#else

static void m68vz328_reset(void)
{
}

static void init_hardware(char *command, int size)
{
}

#endif

void config_BSP(char *command, int size)
{
	printk(KERN_INFO "68VZ328 DragonBallVZ support (c) 2001 Lineo, Inc.\n");

	init_hardware(command, size);

	mach_hwclk = m68328_hwclk;
	mach_reset = m68vz328_reset;
}

