#ifndef _M68K_IDPROM_H
#define _M68K_IDPROM_H
/*
 * idprom.h: Macros and defines for idprom routines
 *
 * Copyright (C) 1995,1996 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/types.h>

struct idprom {
	u8		id_format;	
	u8		id_machtype;	
	u8		id_ethaddr[6];	
	s32		id_date;	
	u32		id_sernum:24;	
	u8		id_cksum;	
	u8		reserved[16];
};

extern struct idprom *idprom;
extern void idprom_init(void);

#define SUN3_IDPROM_BASE	0x00000000

#endif 
