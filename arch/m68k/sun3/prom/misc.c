/*
 * misc.c:  Miscellaneous prom functions that don't belong
 *          anywhere else.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/sun3-head.h>
#include <asm/idprom.h>
#include <asm/openprom.h>
#include <asm/oplib.h>
#include <asm/movs.h>

void
prom_reboot(char *bcommand)
{
	unsigned long flags;
	local_irq_save(flags);
	(*(romvec->pv_reboot))(bcommand);
	local_irq_restore(flags);
}

void
prom_cmdline(void)
{
}

void
prom_halt(void)
{
	unsigned long flags;
again:
	local_irq_save(flags);
	(*(romvec->pv_halt))();
	local_irq_restore(flags);
	goto again; 
}

typedef void (*sfunc_t)(void);

unsigned char
prom_get_idprom(char *idbuf, int num_bytes)
{
	int i, oldsfc;
	GET_SFC(oldsfc);
	SET_SFC(FC_CONTROL);
	for(i=0;i<num_bytes; i++)
	{
		int c;
		GET_CONTROL_BYTE(SUN3_IDPROM_BASE + i, c);
		idbuf[i] = c;
	}
	SET_SFC(oldsfc);
	return idbuf[0];
}

int
prom_version(void)
{
	return romvec->pv_romvers;
}

int
prom_getrev(void)
{
	return prom_rev;
}

int
prom_getprev(void)
{
	return prom_prev;
}
