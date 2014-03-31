/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000 Silicon Graphics, Inc.
 * Copyright (C) 2000 by Colin Ngam
 */
#ifndef _ASM_SN_LAUNCH_H
#define _ASM_SN_LAUNCH_H

#include <asm/sn/types.h>
#include <asm/sn/addrs.h>


#define LAUNCH_MAGIC		0xaddbead2addbead3
#ifdef CONFIG_SGI_IP27
#define LAUNCH_SIZEOF		0x100
#define LAUNCH_PADSZ		0xa0
#endif

#define LAUNCH_OFF_MAGIC	0x00	
#define LAUNCH_OFF_BUSY		0x08
#define LAUNCH_OFF_CALL		0x10
#define LAUNCH_OFF_CALLC	0x18
#define LAUNCH_OFF_CALLPARM	0x20
#define LAUNCH_OFF_STACK	0x28
#define LAUNCH_OFF_GP		0x30
#define LAUNCH_OFF_BEVUTLB	0x38
#define LAUNCH_OFF_BEVNORMAL	0x40
#define LAUNCH_OFF_BEVECC	0x48

#define LAUNCH_STATE_DONE	0	
#define LAUNCH_STATE_SENT	1
#define LAUNCH_STATE_RECD	2


#ifndef __ASSEMBLY__

typedef int launch_state_t;
typedef void (*launch_proc_t)(u64 call_parm);

typedef struct launch_s {
	volatile u64		magic;	
	volatile u64		busy;	
	volatile launch_proc_t	call_addr;	
	volatile u64		call_addr_c;	
	volatile u64		call_parm;	
	volatile void *stack_addr;	
	volatile void *gp_addr;		
	volatile char 		*bevutlb;
	volatile char 		*bevnormal;
	volatile char 		*bevecc;
	volatile char		pad[160];	
} launch_t;


#define LAUNCH_SLAVE	(*(void (*)(int nasid, int cpu, \
				    launch_proc_t call_addr, \
				    u64 call_parm, \
				    void *stack_addr, \
				    void *gp_addr)) \
			 IP27PROM_LAUNCHSLAVE)

#define LAUNCH_WAIT	(*(void (*)(int nasid, int cpu, int timeout_msec)) \
			 IP27PROM_WAITSLAVE)

#define LAUNCH_POLL	(*(launch_state_t (*)(int nasid, int cpu)) \
			 IP27PROM_POLLSLAVE)

#define LAUNCH_LOOP	(*(void (*)(void)) \
			 IP27PROM_SLAVELOOP)

#define LAUNCH_FLASH	(*(void (*)(void)) \
			 IP27PROM_FLASHLEDS)

#endif 

#endif 
