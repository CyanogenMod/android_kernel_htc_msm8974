/******************************************************************************
 *
 *	(C)Copyright 1998,1999 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	See the file "skfddi.c" for further information.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/


#include "h/types.h"
#include "h/fddi.h"
#include "h/smc.h"

#ifndef	lint
static const char ID_sccs[] = "@(#)smtinit.c	1.15 97/05/06 (C) SK " ;
#endif

void init_fddi_driver(struct s_smc *smc, u_char *mac_addr);

#if defined(DEBUG) && !defined(DEBUG_BRD)
struct smt_debug debug;
#endif

#ifndef MULT_OEM
#define OEMID(smc,i)	oem_id[i]
	extern u_char	oem_id[] ;
#else	
#define OEMID(smc,i)	smc->hw.oem_id->oi_mark[i]
	extern struct s_oem_ids	oem_ids[] ;
#endif	

static void set_oem_spec_val(struct s_smc *smc)
{
	struct fddi_mib *mib ;

	mib = &smc->mib ;

	if (OEMID(smc,0) == 'I') {
		mib->fddiSMTConnectionPolicy = POLICY_MM ;
	}
}

int init_smt(struct s_smc *smc, u_char *mac_addr)
{
	int	p ;

#if defined(DEBUG) && !defined(DEBUG_BRD)
	debug.d_smt = 0 ;
	debug.d_smtf = 0 ;
	debug.d_rmt = 0 ;
	debug.d_ecm = 0 ;
	debug.d_pcm = 0 ;
	debug.d_cfm = 0 ;

	debug.d_plc = 0 ;
#ifdef	ESS
	debug.d_ess = 0 ;
#endif
#ifdef	SBA
	debug.d_sba = 0 ;
#endif
#endif	

	
	for ( p = 0; p < NUMPHYS; p ++ ) {
		smc->y[p].mib = & smc->mib.p[p] ;
	}

	set_oem_spec_val(smc) ;	
	(void) smt_set_mac_opvalues(smc) ;
	init_fddi_driver(smc,mac_addr) ;	
	smt_fixup_mib(smc) ;		

	ev_init(smc) ;			
#ifndef	SLIM_SMT
	smt_init_evc(smc) ;		
#endif	
	smt_timer_init(smc) ;		
	smt_agent_init(smc) ;		

	pcm_init(smc) ;			
	ecm_init(smc) ;			
	cfm_init(smc) ;			
	rmt_init(smc) ;			

	for (p = 0 ; p < NUMPHYS ; p++) {
		pcm(smc,p,0) ;		
	}
	ecm(smc,0) ;			
	cfm(smc,0) ;			
	rmt(smc,0) ;			

	smt_agent_task(smc) ;		

        PNMI_INIT(smc) ;                

	return 0;
}

