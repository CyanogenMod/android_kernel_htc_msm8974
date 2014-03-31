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
static const char ID_sccs[] = "@(#)rmt.c	2.13 99/07/02 (C) SK " ;
#endif

#define AFLAG	0x10
#define GO_STATE(x)	(smc->mib.m[MAC0].fddiMACRMTState = (x)|AFLAG)
#define ACTIONS_DONE()	(smc->mib.m[MAC0].fddiMACRMTState &= ~AFLAG)
#define ACTIONS(x)	(x|AFLAG)

#define RM0_ISOLATED	0
#define RM1_NON_OP	1		
#define RM2_RING_OP	2		
#define RM3_DETECT	3		
#define RM4_NON_OP_DUP	4		
#define RM5_RING_OP_DUP	5		
#define RM6_DIRECTED	6		
#define RM7_TRACE	7		

#ifdef	DEBUG
static const char * const rmt_states[] = {
	"RM0_ISOLATED","RM1_NON_OP","RM2_RING_OP","RM3_DETECT",
	"RM4_NON_OP_DUP","RM5_RING_OP_DUP","RM6_DIRECTED",
	"RM7_TRACE"
} ;

static const char * const rmt_events[] = {
	"NONE","RM_RING_OP","RM_RING_NON_OP","RM_MY_BEACON",
	"RM_OTHER_BEACON","RM_MY_CLAIM","RM_TRT_EXP","RM_VALID_CLAIM",
	"RM_JOIN","RM_LOOP","RM_DUP_ADDR","RM_ENABLE_FLAG",
	"RM_TIMEOUT_NON_OP","RM_TIMEOUT_T_STUCK",
	"RM_TIMEOUT_ANNOUNCE","RM_TIMEOUT_T_DIRECT",
	"RM_TIMEOUT_D_MAX","RM_TIMEOUT_POLL","RM_TX_STATE_CHANGE"
} ;
#endif



static void rmt_fsm(struct s_smc *smc, int cmd);
static void start_rmt_timer0(struct s_smc *smc, u_long value, int event);
static void start_rmt_timer1(struct s_smc *smc, u_long value, int event);
static void start_rmt_timer2(struct s_smc *smc, u_long value, int event);
static void stop_rmt_timer0(struct s_smc *smc);
static void stop_rmt_timer1(struct s_smc *smc);
static void stop_rmt_timer2(struct s_smc *smc);
static void rmt_dup_actions(struct s_smc *smc);
static void rmt_reinsert_actions(struct s_smc *smc);
static void rmt_leave_actions(struct s_smc *smc);
static void rmt_new_dup_actions(struct s_smc *smc);

#ifndef SUPERNET_3
extern void restart_trt_for_dbcn() ;
#endif 

void rmt_init(struct s_smc *smc)
{
	smc->mib.m[MAC0].fddiMACRMTState = ACTIONS(RM0_ISOLATED) ;
	smc->r.dup_addr_test = DA_NONE ;
	smc->r.da_flag = 0 ;
	smc->mib.m[MAC0].fddiMACMA_UnitdataAvailable = FALSE ;
	smc->r.sm_ma_avail = FALSE ;
	smc->r.loop_avail = 0 ;
	smc->r.bn_flag = 0 ;
	smc->r.jm_flag = 0 ;
	smc->r.no_flag = TRUE ;
}

void rmt(struct s_smc *smc, int event)
{
	int	state ;

	do {
		DB_RMT("RMT : state %s%s",
			(smc->mib.m[MAC0].fddiMACRMTState & AFLAG) ? "ACTIONS " : "",
			rmt_states[smc->mib.m[MAC0].fddiMACRMTState & ~AFLAG]) ;
		DB_RMT(" event %s\n",rmt_events[event],0) ;
		state = smc->mib.m[MAC0].fddiMACRMTState ;
		rmt_fsm(smc,event) ;
		event = 0 ;
	} while (state != smc->mib.m[MAC0].fddiMACRMTState) ;
	rmt_state_change(smc,(int)smc->mib.m[MAC0].fddiMACRMTState) ;
}

static void rmt_fsm(struct s_smc *smc, int cmd)
{
	if (!smc->r.rm_join && !smc->r.rm_loop &&
		smc->mib.m[MAC0].fddiMACRMTState != ACTIONS(RM0_ISOLATED) &&
		smc->mib.m[MAC0].fddiMACRMTState != RM0_ISOLATED) {
		RS_SET(smc,RS_NORINGOP) ;
		rmt_indication(smc,0) ;
		GO_STATE(RM0_ISOLATED) ;
		return ;
	}

	switch(smc->mib.m[MAC0].fddiMACRMTState) {
	case ACTIONS(RM0_ISOLATED) :
		stop_rmt_timer0(smc) ;
		stop_rmt_timer1(smc) ;
		stop_rmt_timer2(smc) ;

		sm_ma_control(smc,MA_OFFLINE) ;
		smc->mib.m[MAC0].fddiMACMA_UnitdataAvailable = FALSE ;
		smc->r.loop_avail = FALSE ;
		smc->r.sm_ma_avail = FALSE ;
		smc->r.no_flag = TRUE ;
		DB_RMTN(1,"RMT : ISOLATED\n",0,0) ;
		ACTIONS_DONE() ;
		break ;
	case RM0_ISOLATED :
		
		if (smc->r.rm_join || smc->r.rm_loop) {
			sm_ma_control(smc,MA_RESET) ;
			GO_STATE(RM1_NON_OP) ;
			break ;
		}
		break ;
	case ACTIONS(RM1_NON_OP) :
		start_rmt_timer0(smc,smc->s.rmt_t_non_op,RM_TIMEOUT_NON_OP) ;
		stop_rmt_timer1(smc) ;
		stop_rmt_timer2(smc) ;
		sm_ma_control(smc,MA_BEACON) ;
		DB_RMTN(1,"RMT : RING DOWN\n",0,0) ;
		RS_SET(smc,RS_NORINGOP) ;
		smc->r.sm_ma_avail = FALSE ;
		rmt_indication(smc,0) ;
		ACTIONS_DONE() ;
		break ;
	case RM1_NON_OP :
		
		if (cmd == RM_RING_OP) {
			RS_SET(smc,RS_RINGOPCHANGE) ;
			GO_STATE(RM2_RING_OP) ;
			break ;
		}
		
		else if (cmd == RM_TIMEOUT_NON_OP) {
			smc->r.bn_flag = FALSE ;
			smc->r.no_flag = TRUE ;
			GO_STATE(RM3_DETECT) ;
			break ;
		}
		break ;
	case ACTIONS(RM2_RING_OP) :
		stop_rmt_timer0(smc) ;
		stop_rmt_timer1(smc) ;
		stop_rmt_timer2(smc) ;
		smc->r.no_flag = FALSE ;
		if (smc->r.rm_loop)
			smc->r.loop_avail = TRUE ;
		if (smc->r.rm_join) {
			smc->r.sm_ma_avail = TRUE ;
			if (smc->mib.m[MAC0].fddiMACMA_UnitdataEnable)
			smc->mib.m[MAC0].fddiMACMA_UnitdataAvailable = TRUE ;
				else
			smc->mib.m[MAC0].fddiMACMA_UnitdataAvailable = FALSE ;
		}
		DB_RMTN(1,"RMT : RING UP\n",0,0) ;
		RS_CLEAR(smc,RS_NORINGOP) ;
		RS_SET(smc,RS_RINGOPCHANGE) ;
		rmt_indication(smc,1) ;
		smt_stat_counter(smc,0) ;
		ACTIONS_DONE() ;
		break ;
	case RM2_RING_OP :
		
		if (cmd == RM_RING_NON_OP) {
			smc->mib.m[MAC0].fddiMACMA_UnitdataAvailable = FALSE ;
			smc->r.loop_avail = FALSE ;
			RS_SET(smc,RS_RINGOPCHANGE) ;
			GO_STATE(RM1_NON_OP) ;
			break ;
		}
		
		else if (cmd == RM_ENABLE_FLAG) {
			if (smc->mib.m[MAC0].fddiMACMA_UnitdataEnable)
			smc->mib.m[MAC0].fddiMACMA_UnitdataAvailable = TRUE ;
				else
			smc->mib.m[MAC0].fddiMACMA_UnitdataAvailable = FALSE ;
		}
		
		else if (smc->r.dup_addr_test == DA_FAILED) {
			smc->mib.m[MAC0].fddiMACMA_UnitdataAvailable = FALSE ;
			smc->r.loop_avail = FALSE ;
			smc->r.da_flag = TRUE ;
			GO_STATE(RM5_RING_OP_DUP) ;
			break ;
		}
		break ;
	case ACTIONS(RM3_DETECT) :
		start_rmt_timer0(smc,smc->s.mac_d_max*2,RM_TIMEOUT_D_MAX) ;
		start_rmt_timer1(smc,smc->s.rmt_t_stuck,RM_TIMEOUT_T_STUCK) ;
		start_rmt_timer2(smc,smc->s.rmt_t_poll,RM_TIMEOUT_POLL) ;
		sm_mac_check_beacon_claim(smc) ;
		DB_RMTN(1,"RMT : RM3_DETECT\n",0,0) ;
		ACTIONS_DONE() ;
		break ;
	case RM3_DETECT :
		if (cmd == RM_TIMEOUT_POLL) {
			start_rmt_timer2(smc,smc->s.rmt_t_poll,RM_TIMEOUT_POLL);
			sm_mac_check_beacon_claim(smc) ;
			break ;
		}
		if (cmd == RM_TIMEOUT_D_MAX) {
			smc->r.timer0_exp = TRUE ;
		}
		if (cmd == RM_TX_STATE_CHANGE) {
			start_rmt_timer0(smc,
					 smc->s.mac_d_max*2,
					 RM_TIMEOUT_D_MAX) ;
		}
		
		if (cmd == RM_RING_OP) {
			GO_STATE(RM2_RING_OP) ;
			break ;
		}
		
		else if ((cmd == RM_MY_BEACON || cmd == RM_OTHER_BEACON)
			&& smc->r.bn_flag) {
			smc->r.bn_flag = FALSE ;
		}
		
		else if (cmd == RM_TRT_EXP && !smc->r.bn_flag) {
			int	tx ;
			if ((tx =  sm_mac_get_tx_state(smc)) == 4 || tx == 5) {
			DB_RMTN(2,"RMT : DETECT && TRT_EXPIRED && T4/T5\n",0,0);
				smc->r.bn_flag = TRUE ;
				start_rmt_timer1(smc,smc->s.rmt_t_stuck,
					RM_TIMEOUT_T_STUCK) ;
			}

			DB_RMTN(2,
			"RMT : sm_mac_get_tx_state() = %d (bn_flag = %d)\n",
			tx,smc->r.bn_flag) ;
		}
		
		else if (cmd == RM_MY_CLAIM && smc->r.timer0_exp) {
			rmt_new_dup_actions(smc) ;
			GO_STATE(RM4_NON_OP_DUP) ;
			break ;
		}
		
		else if (cmd == RM_MY_BEACON && smc->r.timer0_exp) {
			rmt_new_dup_actions(smc) ;
			GO_STATE(RM4_NON_OP_DUP) ;
			break ;
		}
		
		else if (cmd == RM_VALID_CLAIM) {
			rmt_new_dup_actions(smc) ;
			GO_STATE(RM4_NON_OP_DUP) ;
			break ;
		}
		
		else if (cmd == RM_TIMEOUT_T_STUCK &&
			smc->r.rm_join && smc->r.bn_flag) {
			GO_STATE(RM6_DIRECTED) ;
			break ;
		}
		break ;
	case ACTIONS(RM4_NON_OP_DUP) :
		start_rmt_timer0(smc,smc->s.rmt_t_announce,RM_TIMEOUT_ANNOUNCE);
		start_rmt_timer1(smc,smc->s.rmt_t_stuck,RM_TIMEOUT_T_STUCK) ;
		start_rmt_timer2(smc,smc->s.rmt_t_poll,RM_TIMEOUT_POLL) ;
		sm_mac_check_beacon_claim(smc) ;
		DB_RMTN(1,"RMT : RM4_NON_OP_DUP\n",0,0) ;
		ACTIONS_DONE() ;
		break ;
	case RM4_NON_OP_DUP :
		if (cmd == RM_TIMEOUT_POLL) {
			start_rmt_timer2(smc,smc->s.rmt_t_poll,RM_TIMEOUT_POLL);
			sm_mac_check_beacon_claim(smc) ;
			break ;
		}
		
		if (!smc->r.da_flag) {
			GO_STATE(RM1_NON_OP) ;
			break ;
		}
		
		else if ((cmd == RM_MY_BEACON || cmd == RM_OTHER_BEACON) &&
			smc->r.bn_flag) {
			smc->r.bn_flag = FALSE ;
		}
		
		else if (cmd == RM_TRT_EXP && !smc->r.bn_flag) {
			int	tx ;
			if ((tx =  sm_mac_get_tx_state(smc)) == 4 || tx == 5) {
			DB_RMTN(2,"RMT : NOPDUP && TRT_EXPIRED && T4/T5\n",0,0);
				smc->r.bn_flag = TRUE ;
				start_rmt_timer1(smc,smc->s.rmt_t_stuck,
					RM_TIMEOUT_T_STUCK) ;
			}

			DB_RMTN(2,
			"RMT : sm_mac_get_tx_state() = %d (bn_flag = %d)\n",
			tx,smc->r.bn_flag) ;
		}
		
		else if (cmd == RM_TIMEOUT_ANNOUNCE && !smc->r.bn_flag) {
			rmt_dup_actions(smc) ;
		}
		
		else if (cmd == RM_RING_OP) {
			smc->r.no_flag = FALSE ;
			GO_STATE(RM5_RING_OP_DUP) ;
			break ;
		}
		
		else if (cmd == RM_TIMEOUT_T_STUCK &&
			smc->r.rm_join && smc->r.bn_flag) {
			GO_STATE(RM6_DIRECTED) ;
			break ;
		}
		break ;
	case ACTIONS(RM5_RING_OP_DUP) :
		stop_rmt_timer0(smc) ;
		stop_rmt_timer1(smc) ;
		stop_rmt_timer2(smc) ;
		DB_RMTN(1,"RMT : RM5_RING_OP_DUP\n",0,0) ;
		ACTIONS_DONE() ;
		break;
	case RM5_RING_OP_DUP :
		
		if (smc->r.dup_addr_test == DA_PASSED) {
			smc->r.da_flag = FALSE ;
			GO_STATE(RM2_RING_OP) ;
			break ;
		}
		
		else if (cmd == RM_RING_NON_OP) {
			smc->r.jm_flag = FALSE ;
			smc->r.bn_flag = FALSE ;
			GO_STATE(RM4_NON_OP_DUP) ;
			break ;
		}
		break ;
	case ACTIONS(RM6_DIRECTED) :
		start_rmt_timer0(smc,smc->s.rmt_t_direct,RM_TIMEOUT_T_DIRECT) ;
		stop_rmt_timer1(smc) ;
		start_rmt_timer2(smc,smc->s.rmt_t_poll,RM_TIMEOUT_POLL) ;
		sm_ma_control(smc,MA_DIRECTED) ;
		RS_SET(smc,RS_BEACON) ;
		DB_RMTN(1,"RMT : RM6_DIRECTED\n",0,0) ;
		ACTIONS_DONE() ;
		break ;
	case RM6_DIRECTED :
		
		if (cmd == RM_TIMEOUT_POLL) {
			start_rmt_timer2(smc,smc->s.rmt_t_poll,RM_TIMEOUT_POLL);
			sm_mac_check_beacon_claim(smc) ;
#ifndef SUPERNET_3
			restart_trt_for_dbcn(smc) ;
#endif 
			break ;
		}
		if ((cmd == RM_MY_BEACON || cmd == RM_OTHER_BEACON) &&
			!smc->r.da_flag) {
			smc->r.bn_flag = FALSE ;
			GO_STATE(RM3_DETECT) ;
			break ;
		}
		
		else if ((cmd == RM_MY_BEACON || cmd == RM_OTHER_BEACON) &&
			smc->r.da_flag) {
			smc->r.bn_flag = FALSE ;
			GO_STATE(RM4_NON_OP_DUP) ;
			break ;
		}
		
		else if (cmd == RM_TIMEOUT_T_DIRECT) {
			GO_STATE(RM7_TRACE) ;
			break ;
		}
		break ;
	case ACTIONS(RM7_TRACE) :
		stop_rmt_timer0(smc) ;
		stop_rmt_timer1(smc) ;
		stop_rmt_timer2(smc) ;
		smc->e.trace_prop |= ENTITY_BIT(ENTITY_MAC) ;
		queue_event(smc,EVENT_ECM,EC_TRACE_PROP) ;
		DB_RMTN(1,"RMT : RM7_TRACE\n",0,0) ;
		ACTIONS_DONE() ;
		break ;
	case RM7_TRACE :
		break ;
	default:
		SMT_PANIC(smc,SMT_E0122, SMT_E0122_MSG) ;
		break;
	}
}

static void rmt_dup_actions(struct s_smc *smc)
{
	if (smc->r.jm_flag) {
	}
	else {
		if (smc->s.rmt_dup_mac_behavior) {
			SMT_ERR_LOG(smc,SMT_E0138, SMT_E0138_MSG) ;
                        rmt_reinsert_actions(smc) ;
		}
		else {
			SMT_ERR_LOG(smc,SMT_E0135, SMT_E0135_MSG) ;
			rmt_leave_actions(smc) ;
		}
	}
}

static void rmt_reinsert_actions(struct s_smc *smc)
{
	queue_event(smc,EVENT_ECM,EC_DISCONNECT) ;
	queue_event(smc,EVENT_ECM,EC_CONNECT) ;
}

static void rmt_new_dup_actions(struct s_smc *smc)
{
	smc->r.da_flag = TRUE ;
	smc->r.bn_flag = FALSE ;
	smc->r.jm_flag = FALSE ;
	if (smc->s.rmt_dup_mac_behavior) {
		SMT_ERR_LOG(smc,SMT_E0138, SMT_E0138_MSG) ;
		rmt_reinsert_actions(smc) ;
	}
	else {
		SMT_ERR_LOG(smc,SMT_E0135, SMT_E0135_MSG) ;
		rmt_leave_actions(smc) ;
	}
}


static void rmt_leave_actions(struct s_smc *smc)
{
	queue_event(smc,EVENT_ECM,EC_DISCONNECT) ;
}

static void start_rmt_timer0(struct s_smc *smc, u_long value, int event)
{
	smc->r.timer0_exp = FALSE ;		
	smt_timer_start(smc,&smc->r.rmt_timer0,value,EV_TOKEN(EVENT_RMT,event));
}

static void start_rmt_timer1(struct s_smc *smc, u_long value, int event)
{
	smc->r.timer1_exp = FALSE ;	
	smt_timer_start(smc,&smc->r.rmt_timer1,value,EV_TOKEN(EVENT_RMT,event));
}

static void start_rmt_timer2(struct s_smc *smc, u_long value, int event)
{
	smc->r.timer2_exp = FALSE ;		
	smt_timer_start(smc,&smc->r.rmt_timer2,value,EV_TOKEN(EVENT_RMT,event));
}

static void stop_rmt_timer0(struct s_smc *smc)
{
	if (smc->r.rmt_timer0.tm_active)
		smt_timer_stop(smc,&smc->r.rmt_timer0) ;
}

static void stop_rmt_timer1(struct s_smc *smc)
{
	if (smc->r.rmt_timer1.tm_active)
		smt_timer_stop(smc,&smc->r.rmt_timer1) ;
}

static void stop_rmt_timer2(struct s_smc *smc)
{
	if (smc->r.rmt_timer2.tm_active)
		smt_timer_stop(smc,&smc->r.rmt_timer2) ;
}

