/*
 * Copyright 2004-2008 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef _BFIN_SIGINFO_H
#define _BFIN_SIGINFO_H

#include <linux/types.h>
#include <asm-generic/siginfo.h>

#define UID16_SIGINFO_COMPAT_NEEDED

#define si_uid16	_sifields._kill._uid

#define ILL_ILLPARAOP	(__SI_FAULT|2)	
#define ILL_ILLEXCPT	(__SI_FAULT|4)	
#define ILL_CPLB_VI	(__SI_FAULT|9)	
#define ILL_CPLB_MISS	(__SI_FAULT|10)	
#define ILL_CPLB_MULHIT	(__SI_FAULT|11)	

#define BUS_OPFETCH	(__SI_FAULT|4)	

#define TRAP_STEP	(__SI_FAULT|1)	
#define TRAP_TRACEFLOW	(__SI_FAULT|2)	
#define TRAP_WATCHPT	(__SI_FAULT|3)	
#define TRAP_ILLTRAP	(__SI_FAULT|4)	

#define SEGV_STACKFLOW	(__SI_FAULT|3)	

#endif
