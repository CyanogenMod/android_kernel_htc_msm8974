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

#define KERNEL
#include "h/smtstate.h"

#ifndef	lint
static const char ID_sccs[] = "@(#)cfm.c	2.18 98/10/06 (C) SK " ;
#endif

#define AFLAG	0x10
#define GO_STATE(x)	(smc->mib.fddiSMTCF_State = (x)|AFLAG)
#define ACTIONS_DONE()	(smc->mib.fddiSMTCF_State &= ~AFLAG)
#define ACTIONS(x)	(x|AFLAG)

#ifdef	DEBUG
static const char * const cfm_states[] = {
	"SC0_ISOLATED","CF1","CF2","CF3","CF4",
	"SC1_WRAP_A","SC2_WRAP_B","SC5_TRHU_B","SC7_WRAP_S",
	"SC9_C_WRAP_A","SC10_C_WRAP_B","SC11_C_WRAP_S","SC4_THRU_A"
} ;

static const char * const cfm_events[] = {
	"NONE","CF_LOOP_A","CF_LOOP_B","CF_JOIN_A","CF_JOIN_B"
} ;
#endif

static const unsigned char cf_to_ptype[] = {
	TNONE,TNONE,TNONE,TNONE,TNONE,
	TNONE,TB,TB,TS,
	TA,TB,TS,TB
} ;

#define	CEM_PST_DOWN	0
#define	CEM_PST_UP	1
#define	CEM_PST_HOLD	2


static void cfm_fsm(struct s_smc *smc, int cmd);

void cfm_init(struct s_smc *smc)
{
	smc->mib.fddiSMTCF_State = ACTIONS(SC0_ISOLATED) ;
	smc->r.rm_join = 0 ;
	smc->r.rm_loop = 0 ;
	smc->y[PA].scrub = 0 ;
	smc->y[PB].scrub = 0 ;
	smc->y[PA].cem_pst = CEM_PST_DOWN ;
	smc->y[PB].cem_pst = CEM_PST_DOWN ;
}

#define THRU_ENABLED(smc)	(smc->y[PA].pc_mode != PM_TREE && \
				 smc->y[PB].pc_mode != PM_TREE)
static void selection_criteria (struct s_smc *smc, struct s_phy *phy)
{

	switch (phy->mib->fddiPORTMy_Type) {
	case TA:
		if ( !THRU_ENABLED(smc) && smc->y[PB].cf_join ) {
			phy->wc_flag = TRUE ;
		} else {
			phy->wc_flag = FALSE ;
		}

		break;
	case TB:
		
		phy->wc_flag = FALSE ;
		break;
	case TS:
		phy->wc_flag = FALSE ;
		break;
	case TM:
		phy->wc_flag = FALSE ;
		break;
	}

}

void all_selection_criteria(struct s_smc *smc)
{
	struct s_phy	*phy ;
	int		p ;

	for ( p = 0,phy = smc->y ; p < NUMPHYS; p++, phy++ ) {
		
		selection_criteria (smc,phy);
	}
}

static void cem_priv_state(struct s_smc *smc, int event)
{
	int	np;	
	int	i;

	
	if (smc->s.sas != SMT_DAS )
		return ;

	np = event - CF_JOIN;

	if (np != PA && np != PB) {
		return ;
	}
	
	if (smc->y[np].cf_join) {
		smc->y[np].cem_pst = CEM_PST_UP ;
	} else if (!smc->y[np].wc_flag) {
		
		smc->y[np].cem_pst = CEM_PST_DOWN ;
	}

	

	
	for (i = 0 ; i < 2 ; i ++ ) {
		
		if ( smc->y[i].cem_pst == CEM_PST_HOLD && !smc->y[i].wc_flag ) {
			smc->y[i].cem_pst = CEM_PST_DOWN;
			queue_event(smc,(int)(EVENT_PCM+i),PC_START) ;
		}
		if ( smc->y[i].cem_pst == CEM_PST_UP && smc->y[i].wc_flag ) {
			smc->y[i].cem_pst = CEM_PST_HOLD;
			queue_event(smc,(int)(EVENT_PCM+i),PC_START) ;
		}
		if ( smc->y[i].cem_pst == CEM_PST_DOWN && smc->y[i].wc_flag ) {
			smc->y[i].cem_pst = CEM_PST_HOLD;
		}
	}
	return ;
}

void cfm(struct s_smc *smc, int event)
{
	int	state ;		
	int	cond ;
	int	oldstate ;

	
	
	
	
	

	all_selection_criteria (smc);

	
	
	cem_priv_state (smc, event);

	oldstate = smc->mib.fddiSMTCF_State ;
	do {
		DB_CFM("CFM : state %s%s",
			(smc->mib.fddiSMTCF_State & AFLAG) ? "ACTIONS " : "",
			cfm_states[smc->mib.fddiSMTCF_State & ~AFLAG]) ;
		DB_CFM(" event %s\n",cfm_events[event],0) ;
		state = smc->mib.fddiSMTCF_State ;
		cfm_fsm(smc,event) ;
		event = 0 ;
	} while (state != smc->mib.fddiSMTCF_State) ;

#ifndef	SLIM_SMT
	cond = FALSE ;
	if (	(smc->mib.fddiSMTCF_State == SC9_C_WRAP_A &&
		smc->y[PA].pc_mode == PM_PEER) 	||
		(smc->mib.fddiSMTCF_State == SC10_C_WRAP_B &&
		smc->y[PB].pc_mode == PM_PEER) 	||
		(smc->mib.fddiSMTCF_State == SC11_C_WRAP_S &&
		smc->y[PS].pc_mode == PM_PEER &&
		smc->y[PS].mib->fddiPORTNeighborType != TS ) ) {
			cond = TRUE ;
	}
	if (cond != smc->mib.fddiSMTPeerWrapFlag)
		smt_srf_event(smc,SMT_COND_SMT_PEER_WRAP,0,cond) ;

#if	0
	if (smc->mib.fddiSMTCF_State != oldstate) {
		smt_srf_event(smc,SMT_EVENT_MAC_PATH_CHANGE,INDEX_MAC,0) ;
	}
#endif
#endif	

	smc->mib.m[MAC0].fddiMACDownstreamPORTType =
		cf_to_ptype[smc->mib.fddiSMTCF_State] ;
	cfm_state_change(smc,(int)smc->mib.fddiSMTCF_State) ;
}

static void cfm_fsm(struct s_smc *smc, int cmd)
{
	switch(smc->mib.fddiSMTCF_State) {
	case ACTIONS(SC0_ISOLATED) :
		smc->mib.p[PA].fddiPORTCurrentPath = MIB_PATH_ISOLATED ;
		smc->mib.p[PB].fddiPORTCurrentPath = MIB_PATH_ISOLATED ;
		smc->mib.p[PA].fddiPORTMACPlacement = 0 ;
		smc->mib.p[PB].fddiPORTMACPlacement = 0 ;
		smc->mib.fddiSMTStationStatus = MIB_SMT_STASTA_SEPA ;
		config_mux(smc,MUX_ISOLATE) ;	
		smc->r.rm_loop = FALSE ;
		smc->r.rm_join = FALSE ;
		queue_event(smc,EVENT_RMT,RM_JOIN) ;
		
		ACTIONS_DONE() ;
		DB_CFMN(1,"CFM : %s\n",cfm_states[smc->mib.fddiSMTCF_State],0) ;
		break;
	case SC0_ISOLATED :
		
		
		if (smc->s.sas && (smc->y[PA].cf_join || smc->y[PA].cf_loop ||
				smc->y[PB].cf_join || smc->y[PB].cf_loop)) {
			GO_STATE(SC11_C_WRAP_S) ;
			break ;
		}
		
		if ((smc->y[PA].cem_pst == CEM_PST_UP && smc->y[PA].cf_join &&
		     !smc->y[PA].wc_flag) || smc->y[PA].cf_loop) {
			GO_STATE(SC9_C_WRAP_A) ;
			break ;
		}
		
		if ((smc->y[PB].cem_pst == CEM_PST_UP && smc->y[PB].cf_join &&
		     !smc->y[PB].wc_flag) || smc->y[PB].cf_loop) {
			GO_STATE(SC10_C_WRAP_B) ;
			break ;
		}
		break ;
	case ACTIONS(SC9_C_WRAP_A) :
		smc->mib.p[PA].fddiPORTCurrentPath = MIB_PATH_CONCATENATED ;
		smc->mib.p[PB].fddiPORTCurrentPath = MIB_PATH_ISOLATED ;
		smc->mib.p[PA].fddiPORTMACPlacement = INDEX_MAC ;
		smc->mib.p[PB].fddiPORTMACPlacement = 0 ;
		smc->mib.fddiSMTStationStatus = MIB_SMT_STASTA_CON ;
		config_mux(smc,MUX_WRAPA) ;		
		if (smc->y[PA].cf_loop) {
			smc->r.rm_join = FALSE ;
			smc->r.rm_loop = TRUE ;
			queue_event(smc,EVENT_RMT,RM_LOOP) ;
		}
		if (smc->y[PA].cf_join) {
			smc->r.rm_loop = FALSE ;
			smc->r.rm_join = TRUE ;
			queue_event(smc,EVENT_RMT,RM_JOIN) ;
		}
		ACTIONS_DONE() ;
		DB_CFMN(1,"CFM : %s\n",cfm_states[smc->mib.fddiSMTCF_State],0) ;
		break ;
	case SC9_C_WRAP_A :
		
		if ( (smc->y[PA].wc_flag || !smc->y[PA].cf_join) &&
		      !smc->y[PA].cf_loop ) {
			GO_STATE(SC0_ISOLATED) ;
			break ;
		}
		
		else if ( (smc->y[PB].cf_loop && smc->y[PA].cf_join &&
			   smc->y[PA].cem_pst == CEM_PST_UP) ||
			  ((smc->y[PB].cf_loop ||
			   (smc->y[PB].cf_join &&
			    smc->y[PB].cem_pst == CEM_PST_UP)) &&
			    (smc->y[PA].pc_mode == PM_TREE ||
			     smc->y[PB].pc_mode == PM_TREE))) {
			smc->y[PA].scrub = TRUE ;
			GO_STATE(SC10_C_WRAP_B) ;
			break ;
		}
		
		else if (!smc->s.attach_s &&
			  smc->y[PA].cf_join &&
			  smc->y[PA].cem_pst == CEM_PST_UP &&
			  smc->y[PA].pc_mode == PM_PEER && smc->y[PB].cf_join &&
			  smc->y[PB].cem_pst == CEM_PST_UP &&
			  smc->y[PB].pc_mode == PM_PEER) {
			smc->y[PA].scrub = TRUE ;
			smc->y[PB].scrub = TRUE ;
			GO_STATE(SC4_THRU_A) ;
			break ;
		}
		
		else if ( smc->s.attach_s &&
			  smc->y[PA].cf_join &&
			  smc->y[PA].cem_pst == CEM_PST_UP &&
			  smc->y[PA].pc_mode == PM_PEER &&
			  smc->y[PB].cf_join &&
			  smc->y[PB].cem_pst == CEM_PST_UP &&
			  smc->y[PB].pc_mode == PM_PEER) {
			smc->y[PA].scrub = TRUE ;
			smc->y[PB].scrub = TRUE ;
			GO_STATE(SC5_THRU_B) ;
			break ;
		}
		break ;
	case ACTIONS(SC10_C_WRAP_B) :
		smc->mib.p[PA].fddiPORTCurrentPath = MIB_PATH_ISOLATED ;
		smc->mib.p[PB].fddiPORTCurrentPath = MIB_PATH_CONCATENATED ;
		smc->mib.p[PA].fddiPORTMACPlacement = 0 ;
		smc->mib.p[PB].fddiPORTMACPlacement = INDEX_MAC ;
		smc->mib.fddiSMTStationStatus = MIB_SMT_STASTA_CON ;
		config_mux(smc,MUX_WRAPB) ;		
		if (smc->y[PB].cf_loop) {
			smc->r.rm_join = FALSE ;
			smc->r.rm_loop = TRUE ;
			queue_event(smc,EVENT_RMT,RM_LOOP) ;
		}
		if (smc->y[PB].cf_join) {
			smc->r.rm_loop = FALSE ;
			smc->r.rm_join = TRUE ;
			queue_event(smc,EVENT_RMT,RM_JOIN) ;
		}
		ACTIONS_DONE() ;
		DB_CFMN(1,"CFM : %s\n",cfm_states[smc->mib.fddiSMTCF_State],0) ;
		break ;
	case SC10_C_WRAP_B :
		
		if ( !smc->y[PB].cf_join && !smc->y[PB].cf_loop ) {
			GO_STATE(SC0_ISOLATED) ;
			break ;
		}
		
		else if ( smc->y[PA].cf_loop && smc->y[PA].pc_mode == PM_PEER &&
			  smc->y[PB].cf_join && smc->y[PB].pc_mode == PM_PEER) {
			smc->y[PB].scrub = TRUE ;
			GO_STATE(SC9_C_WRAP_A) ;
			break ;
		}
		
		else if (!smc->s.attach_s &&
			 smc->y[PA].cf_join && smc->y[PA].pc_mode == PM_PEER &&
			 smc->y[PB].cf_join && smc->y[PB].pc_mode == PM_PEER) {
			smc->y[PA].scrub = TRUE ;
			smc->y[PB].scrub = TRUE ;
			GO_STATE(SC4_THRU_A) ;
			break ;
		}
		
		else if ( smc->s.attach_s &&
			 smc->y[PA].cf_join && smc->y[PA].pc_mode == PM_PEER &&
			 smc->y[PB].cf_join && smc->y[PB].pc_mode == PM_PEER) {
			smc->y[PA].scrub = TRUE ;
			smc->y[PB].scrub = TRUE ;
			GO_STATE(SC5_THRU_B) ;
			break ;
		}
		break ;
	case ACTIONS(SC4_THRU_A) :
		smc->mib.p[PA].fddiPORTCurrentPath = MIB_PATH_THRU ;
		smc->mib.p[PB].fddiPORTCurrentPath = MIB_PATH_THRU ;
		smc->mib.p[PA].fddiPORTMACPlacement = 0 ;
		smc->mib.p[PB].fddiPORTMACPlacement = INDEX_MAC ;
		smc->mib.fddiSMTStationStatus = MIB_SMT_STASTA_THRU ;
		config_mux(smc,MUX_THRUA) ;		
		smc->r.rm_loop = FALSE ;
		smc->r.rm_join = TRUE ;
		queue_event(smc,EVENT_RMT,RM_JOIN) ;
		ACTIONS_DONE() ;
		DB_CFMN(1,"CFM : %s\n",cfm_states[smc->mib.fddiSMTCF_State],0) ;
		break ;
	case SC4_THRU_A :
		
		if (smc->y[PB].wc_flag || !smc->y[PB].cf_join) {
			smc->y[PA].scrub = TRUE ;
			GO_STATE(SC9_C_WRAP_A) ;
			break ;
		}
		
		else if (!smc->y[PA].cf_join || smc->y[PA].wc_flag) {
			smc->y[PB].scrub = TRUE ;
			GO_STATE(SC10_C_WRAP_B) ;
			break ;
		}
		
		else if (smc->s.attach_s) {
			smc->y[PB].scrub = TRUE ;
			GO_STATE(SC5_THRU_B) ;
			break ;
		}
		break ;
	case ACTIONS(SC5_THRU_B) :
		smc->mib.p[PA].fddiPORTCurrentPath = MIB_PATH_THRU ;
		smc->mib.p[PB].fddiPORTCurrentPath = MIB_PATH_THRU ;
		smc->mib.p[PA].fddiPORTMACPlacement = INDEX_MAC ;
		smc->mib.p[PB].fddiPORTMACPlacement = 0 ;
		smc->mib.fddiSMTStationStatus = MIB_SMT_STASTA_THRU ;
		config_mux(smc,MUX_THRUB) ;		
		smc->r.rm_loop = FALSE ;
		smc->r.rm_join = TRUE ;
		queue_event(smc,EVENT_RMT,RM_JOIN) ;
		ACTIONS_DONE() ;
		DB_CFMN(1,"CFM : %s\n",cfm_states[smc->mib.fddiSMTCF_State],0) ;
		break ;
	case SC5_THRU_B :
		
		if (!smc->y[PB].cf_join || smc->y[PB].wc_flag) {
			smc->y[PA].scrub = TRUE ;
			GO_STATE(SC9_C_WRAP_A) ;
			break ;
		}
		
		else if (!smc->y[PA].cf_join || smc->y[PA].wc_flag) {
			smc->y[PB].scrub = TRUE ;
			GO_STATE(SC10_C_WRAP_B) ;
			break ;
		}
		
		else if (!smc->s.attach_s) {
			smc->y[PA].scrub = TRUE ;
			GO_STATE(SC4_THRU_A) ;
			break ;
		}
		break ;
	case ACTIONS(SC11_C_WRAP_S) :
		smc->mib.p[PS].fddiPORTCurrentPath = MIB_PATH_CONCATENATED ;
		smc->mib.p[PS].fddiPORTMACPlacement = INDEX_MAC ;
		smc->mib.fddiSMTStationStatus = MIB_SMT_STASTA_CON ;
		config_mux(smc,MUX_WRAPS) ;		
		if (smc->y[PA].cf_loop || smc->y[PB].cf_loop) {
			smc->r.rm_join = FALSE ;
			smc->r.rm_loop = TRUE ;
			queue_event(smc,EVENT_RMT,RM_LOOP) ;
		}
		if (smc->y[PA].cf_join || smc->y[PB].cf_join) {
			smc->r.rm_loop = FALSE ;
			smc->r.rm_join = TRUE ;
			queue_event(smc,EVENT_RMT,RM_JOIN) ;
		}
		ACTIONS_DONE() ;
		DB_CFMN(1,"CFM : %s\n",cfm_states[smc->mib.fddiSMTCF_State],0) ;
		break ;
	case SC11_C_WRAP_S :
		
		if ( !smc->y[PA].cf_join && !smc->y[PA].cf_loop &&
		     !smc->y[PB].cf_join && !smc->y[PB].cf_loop) {
			GO_STATE(SC0_ISOLATED) ;
			break ;
		}
		break ;
	default:
		SMT_PANIC(smc,SMT_E0106, SMT_E0106_MSG) ;
		break;
	}
}

int cfm_get_mac_input(struct s_smc *smc)
{
	return (smc->mib.fddiSMTCF_State == SC10_C_WRAP_B ||
		smc->mib.fddiSMTCF_State == SC5_THRU_B) ? PB : PA;
}

int cfm_get_mac_output(struct s_smc *smc)
{
	return (smc->mib.fddiSMTCF_State == SC10_C_WRAP_B ||
		smc->mib.fddiSMTCF_State == SC4_THRU_A) ? PB : PA;
}

static char path_iso[] = {
	0,0,	0,RES_PORT,	0,PA + INDEX_PORT,	0,PATH_ISO,
	0,0,	0,RES_MAC,	0,INDEX_MAC,		0,PATH_ISO,
	0,0,	0,RES_PORT,	0,PB + INDEX_PORT,	0,PATH_ISO
} ;

static char path_wrap_a[] = {
	0,0,	0,RES_PORT,	0,PA + INDEX_PORT,	0,PATH_PRIM,
	0,0,	0,RES_MAC,	0,INDEX_MAC,		0,PATH_PRIM,
	0,0,	0,RES_PORT,	0,PB + INDEX_PORT,	0,PATH_ISO
} ;

static char path_wrap_b[] = {
	0,0,	0,RES_PORT,	0,PB + INDEX_PORT,	0,PATH_PRIM,
	0,0,	0,RES_MAC,	0,INDEX_MAC,		0,PATH_PRIM,
	0,0,	0,RES_PORT,	0,PA + INDEX_PORT,	0,PATH_ISO
} ;

static char path_thru[] = {
	0,0,	0,RES_PORT,	0,PA + INDEX_PORT,	0,PATH_PRIM,
	0,0,	0,RES_MAC,	0,INDEX_MAC,		0,PATH_PRIM,
	0,0,	0,RES_PORT,	0,PB + INDEX_PORT,	0,PATH_PRIM
} ;

static char path_wrap_s[] = {
	0,0,	0,RES_PORT,	0,PS + INDEX_PORT,	0,PATH_PRIM,
	0,0,	0,RES_MAC,	0,INDEX_MAC,		0,PATH_PRIM,
} ;

static char path_iso_s[] = {
	0,0,	0,RES_PORT,	0,PS + INDEX_PORT,	0,PATH_ISO,
	0,0,	0,RES_MAC,	0,INDEX_MAC,		0,PATH_ISO,
} ;

int cem_build_path(struct s_smc *smc, char *to, int path_index)
{
	char	*path ;
	int	len ;

	switch (smc->mib.fddiSMTCF_State) {
	default :
	case SC0_ISOLATED :
		path = smc->s.sas ? path_iso_s : path_iso ;
		len = smc->s.sas ? sizeof(path_iso_s) :  sizeof(path_iso) ;
		break ;
	case SC9_C_WRAP_A :
		path = path_wrap_a ;
		len = sizeof(path_wrap_a) ;
		break ;
	case SC10_C_WRAP_B :
		path = path_wrap_b ;
		len = sizeof(path_wrap_b) ;
		break ;
	case SC4_THRU_A :
		path = path_thru ;
		len = sizeof(path_thru) ;
		break ;
	case SC11_C_WRAP_S :
		path = path_wrap_s ;
		len = sizeof(path_wrap_s) ;
		break ;
	}
	memcpy(to,path,len) ;

	LINT_USE(path_index);

	return len;
}
