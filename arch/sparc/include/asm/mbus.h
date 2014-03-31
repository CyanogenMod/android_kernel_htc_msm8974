/*
 * mbus.h:  Various defines for MBUS modules.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SPARC_MBUS_H
#define _SPARC_MBUS_H

#include <asm/ross.h>    
#include <asm/cypress.h> 
#include <asm/viking.h>  

enum mbus_module {
	HyperSparc        = 0,
	Cypress           = 1,
	Cypress_vE        = 2,
	Cypress_vD        = 3,
	Swift_ok          = 4,
	Swift_bad_c       = 5,
	Swift_lots_o_bugs = 6,
	Tsunami           = 7,
	Viking_12         = 8,
	Viking_2x         = 9,
	Viking_30         = 10,
	Viking_35         = 11,
	Viking_new        = 12,
	TurboSparc	  = 13,
	SRMMU_INVAL_MOD   = 14,
};

extern enum mbus_module srmmu_modtype;
extern unsigned int viking_rev, swift_rev, cypress_rev;

#define HWBUG_COPYBACK_BROKEN        0x00000001
#define HWBUG_ASIFLUSH_BROKEN        0x00000002
#define HWBUG_VACFLUSH_BITROT        0x00000004
#define HWBUG_KERN_ACCBROKEN         0x00000008
#define HWBUG_KERN_CBITBROKEN        0x00000010
#define HWBUG_MODIFIED_BITROT        0x00000020
#define HWBUG_PC_BADFAULT_ADDR       0x00000040
#define HWBUG_SUPERSCALAR_BAD        0x00000080
#define HWBUG_PACINIT_BITROT         0x00000100

#define MBUS_VIKING        0x4   
#define MBUS_LSI           0x3   
#define MBUS_ROSS          0x1   
#define MBUS_FMI           0x0   

#define ROSS_604_REV_CDE        0x0   
#define ROSS_604_REV_F          0x1   
#define ROSS_605                0xf   
#define ROSS_605_REV_B          0xe   

#define VIKING_REV_12           0x1   
#define VIKING_REV_2            0x2   
#define VIKING_REV_30           0x3   
#define VIKING_REV_35           0x4   

#define LSI_L64815		0x0

#define FMI_AURORA		0x4   
#define FMI_TURBO		0x5   


#define TBR_ID_SHIFT            20

static inline int get_cpuid(void)
{
	register int retval;
	__asm__ __volatile__("rd %%tbr, %0\n\t"
			     "srl %0, %1, %0\n\t" :
			     "=r" (retval) :
			     "i" (TBR_ID_SHIFT));
	return (retval & 3);
}

static inline int get_modid(void)
{
	return (get_cpuid() | 0x8);
}

	
#endif 
