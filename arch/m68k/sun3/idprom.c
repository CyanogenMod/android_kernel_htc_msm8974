/*
 * idprom.c: Routines to load the idprom into kernel addresses and
 *           interpret the data contained within.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Sun3/3x models added by David Monro (davidm@psrg.cs.usyd.edu.au)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/string.h>

#include <asm/oplib.h>
#include <asm/idprom.h>
#include <asm/machines.h>  

struct idprom *idprom;
EXPORT_SYMBOL(idprom);

static struct idprom idprom_buffer;

static struct Sun_Machine_Models Sun_Machines[NUM_SUN_MACHINES] = {
    { .name = "Sun 3/160 Series",	.id_machtype = (SM_SUN3 | SM_3_160) },
    { .name = "Sun 3/50",		.id_machtype = (SM_SUN3 | SM_3_50) },
    { .name = "Sun 3/260 Series",	.id_machtype = (SM_SUN3 | SM_3_260) },
    { .name = "Sun 3/110 Series",	.id_machtype = (SM_SUN3 | SM_3_110) },
    { .name = "Sun 3/60",		.id_machtype = (SM_SUN3 | SM_3_60) },
    { .name = "Sun 3/E",		.id_machtype = (SM_SUN3 | SM_3_E) },
    { .name = "Sun 3/460 Series",	.id_machtype = (SM_SUN3X | SM_3_460) },
    { .name = "Sun 3/80",		.id_machtype = (SM_SUN3X | SM_3_80) },
};

static void __init display_system_type(unsigned char machtype)
{
	register int i;

	for (i = 0; i < NUM_SUN_MACHINES; i++) {
		if(Sun_Machines[i].id_machtype == machtype) {
			if (machtype != (SM_SUN4M_OBP | 0x00))
				printk("TYPE: %s\n", Sun_Machines[i].name);
			else {
#if 0
				prom_getproperty(prom_root_node, "banner-name",
						 sysname, sizeof(sysname));
				printk("TYPE: %s\n", sysname);
#endif
			}
			return;
		}
	}

	prom_printf("IDPROM: Bogus id_machtype value, 0x%x\n", machtype);
	prom_halt();
}

void sun3_get_model(unsigned char* model)
{
	register int i;

	for (i = 0; i < NUM_SUN_MACHINES; i++) {
		if(Sun_Machines[i].id_machtype == idprom->id_machtype) {
		        strcpy(model, Sun_Machines[i].name);
			return;
		}
	}
}



static unsigned char __init calc_idprom_cksum(struct idprom *idprom)
{
	unsigned char cksum, i, *ptr = (unsigned char *)idprom;

	for (i = cksum = 0; i <= 0x0E; i++)
		cksum ^= *ptr++;

	return cksum;
}

void __init idprom_init(void)
{
	prom_get_idprom((char *) &idprom_buffer, sizeof(idprom_buffer));

	idprom = &idprom_buffer;

	if (idprom->id_format != 0x01)  {
		prom_printf("IDPROM: Unknown format type!\n");
		prom_halt();
	}

	if (idprom->id_cksum != calc_idprom_cksum(idprom)) {
		prom_printf("IDPROM: Checksum failure (nvram=%x, calc=%x)!\n",
			    idprom->id_cksum, calc_idprom_cksum(idprom));
		prom_halt();
	}

	display_system_type(idprom->id_machtype);

	printk("Ethernet address: %x:%x:%x:%x:%x:%x\n",
		    idprom->id_ethaddr[0], idprom->id_ethaddr[1],
		    idprom->id_ethaddr[2], idprom->id_ethaddr[3],
		    idprom->id_ethaddr[4], idprom->id_ethaddr[5]);
}
