/******************************************************************************
 *
 *	(C)Copyright 1998,1999 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

#ifndef	_SCMECM_
#define _SCMECM_

#if	defined(PCI) && !defined(OSDEF)
#define	OSDEF
#endif

#ifdef	PCI
#ifndef	SUPERNET_3
#define	SUPERNET_3
#endif
#ifndef	TAG_MODE
#define	TAG_MODE
#endif
#endif

#ifdef	OSDEF
#include "osdef1st.h"
#endif	
#ifdef	OEM_CONCEPT
#include "oemdef.h"
#endif	
#include "smt.h"
#include "cmtdef.h"
#include "fddimib.h"
#include "targethw.h"		
#include "targetos.h"		
#ifdef	ESS
#include "sba.h"
#endif

struct event_queue {
	u_short	class ;			
	u_short	event ;			
} ;

#ifdef	CONCENTRATOR
#define MAX_EVENT	128
#else	
#define MAX_EVENT	64
#endif	

struct s_queue {

	struct event_queue ev_queue[MAX_EVENT];
	struct event_queue *ev_put ;
	struct event_queue *ev_get ;
} ;

struct s_ecm {
	u_char path_test ;		
	u_char sb_flag ;		
	u_char DisconnectFlag ;		
	u_char ecm_line_state ;		
	u_long trace_prop ;		
	char	ec_pad[2] ;
	struct smt_timer ecm_timer ;	
} ;


struct s_rmt {
	u_char dup_addr_test ;		
	u_char da_flag ;		
	u_char loop_avail ;		
	u_char sm_ma_avail ;		
	u_char no_flag ;		
	u_char bn_flag ;		
	u_char jm_flag ;		
	u_char rm_join ;		
	u_char rm_loop ;		

	long fast_rm_join ;		
	struct smt_timer rmt_timer0 ;	
	struct smt_timer rmt_timer1 ;	
	struct smt_timer rmt_timer2 ;	
	u_char timer0_exp ;		
	u_char timer1_exp ;		
	u_char timer2_exp ;		

	u_char rm_pad1[1] ;
} ;

struct s_cfm {
	u_char cf_state;		
	u_char cf_pad[3] ;
} ;

#ifdef	CONCENTRATOR
struct s_cem {
	int	ce_state ;	
	int	ce_port ;	
	int	ce_type ;	
} ;

struct s_c_ring {
	struct s_c_ring	*c_next ;
	char		c_entity ;
} ;

struct mib_path_config {
	u_long	fddimibPATHConfigSMTIndex;
	u_long	fddimibPATHConfigPATHIndex;
	u_long	fddimibPATHConfigTokenOrder;
	u_long	fddimibPATHConfigResourceType;
#define SNMP_RES_TYPE_MAC	2	
#define SNMP_RES_TYPE_PORT	4	
	u_long	fddimibPATHConfigResourceIndex;
	u_long	fddimibPATHConfigCurrentPath;
#define SNMP_PATH_ISOLATED	1	
#define SNMP_PATH_LOCAL		2	
#define SNMP_PATH_SECONDARY	3	
#define SNMP_PATH_PRIMARY	4	
#define SNMP_PATH_CONCATENATED	5	
#define SNMP_PATH_THRU		6	
};


#endif

#define PCM_DISABLED	0
#define PCM_CONNECTING	1
#define PCM_STANDBY	2
#define PCM_ACTIVE	3

struct s_pcm {
	u_char	pcm_pad[3] ;
} ;

struct s_phy {
	
	struct fddi_mib_p	*mib ;

	u_char np ;		
	u_char cf_join ;
	u_char cf_loop ;
	u_char wc_flag ;	
	u_char pc_mode ;	
	u_char pc_lem_fail ;	
	u_char lc_test ;
	u_char scrub ;		
	char phy_name ;
	u_char pmd_type[2] ;	
#define PMD_SK_CONN	0	
#define PMD_SK_PMD	1	
	u_char pmd_scramble ;	

	
	u_char curr_ls ;	
	u_char ls_flag ;
	u_char rc_flag ;
	u_char tc_flag ;
	u_char td_flag ;
	u_char bitn ;
	u_char tr_flag ;	
	u_char twisted ;	
	u_char t_val[NUMBITS] ;	
	u_char r_val[NUMBITS] ;	
	u_long t_next[NUMBITS] ;
	struct smt_timer pcm_timer0 ;
	struct smt_timer pcm_timer1 ;
	struct smt_timer pcm_timer2 ;
	u_char timer0_exp ;
	u_char timer1_exp ;
	u_char timer2_exp ;
	u_char pcm_pad1[1] ;
	int	cem_pst ;	
	struct lem_counter lem ;
#ifdef	AMDPLC
	struct s_plc	plc ;
#endif
} ;

struct s_timer {
	struct smt_timer	*st_queue ;
	struct smt_timer	st_fast ;
} ;

#define SMT_EVENT_BASE			1
#define SMT_EVENT_MAC_PATH_CHANGE	(SMT_EVENT_BASE+0)
#define SMT_EVENT_MAC_NEIGHBOR_CHANGE	(SMT_EVENT_BASE+1)
#define SMT_EVENT_PORT_PATH_CHANGE	(SMT_EVENT_BASE+2)
#define SMT_EVENT_PORT_CONNECTION	(SMT_EVENT_BASE+3)

#define SMT_IS_CONDITION(x)			((x)>=SMT_COND_BASE)

#define SMT_COND_BASE		(SMT_EVENT_PORT_CONNECTION+1)
#define SMT_COND_SMT_PEER_WRAP		(SMT_COND_BASE+0)
#define SMT_COND_SMT_HOLD		(SMT_COND_BASE+1)
#define SMT_COND_MAC_FRAME_ERROR	(SMT_COND_BASE+2)
#define SMT_COND_MAC_DUP_ADDR		(SMT_COND_BASE+3)
#define SMT_COND_MAC_NOT_COPIED		(SMT_COND_BASE+4)
#define SMT_COND_PORT_EB_ERROR		(SMT_COND_BASE+5)
#define SMT_COND_PORT_LER		(SMT_COND_BASE+6)

#define SR0_WAIT	0
#define SR1_HOLDOFF	1
#define SR2_DISABLED	2

struct s_srf {
	u_long	SRThreshold ;			
	u_char	RT_Flag ;			
	u_char	sr_state ;			
	u_char	any_report ;			
	u_long	TSR ;				
	u_short	ring_status ;			
} ;

#define RS_RES15	(1<<15)			
#define RS_HARDERROR	(1<<14)			
#define RS_SOFTERROR	(1<<13)			
#define RS_BEACON	(1<<12)			
#define RS_PATHTEST	(1<<11)			
#define RS_SELFTEST	(1<<10)			
#define RS_RES9		(1<< 9)			
#define RS_DISCONNECT	(1<< 8)			
#define RS_RES7		(1<< 7)			
#define RS_DUPADDR	(1<< 6)			
#define RS_NORINGOP	(1<< 5)			
#define RS_VERSION	(1<< 4)			
#define RS_STUCKBYPASSS	(1<< 3)			
#define RS_EVENT	(1<< 2)			
#define RS_RINGOPCHANGE	(1<< 1)			
#define RS_RES0		(1<< 0)			

#define RS_SET(smc,bit) \
	ring_status_indication(smc,smc->srf.ring_status |= bit)
#define RS_CLEAR(smc,bit)	\
	ring_status_indication(smc,smc->srf.ring_status &= ~bit)

#define RS_CLEAR_EVENT	(0xffff & ~(RS_NORINGOP))

#ifndef AIX_EVENT
#define AIX_EVENT(smc,opt0,opt1,opt2,opt3)	
#endif

struct s_srf_evc {
	u_char	evc_code ;			
	u_char	evc_index ;			
	u_char	evc_rep_required ;		
	u_short	evc_para ;			
	u_char	*evc_cond_state ;		
	u_char	*evc_multiple ;			
} ;

#define SMT_MAX_TEST		5
#define SMT_TID_NIF		0		
#define SMT_TID_NIF_TEST	1		
#define SMT_TID_ECF_UNA		2		
#define SMT_TID_ECF_DNA		3		
#define SMT_TID_ECF		4		

struct smt_values {
	u_long		smt_tvu ;		
	u_long		smt_tvd ;		
	u_long		smt_tid ;		
	u_long		pend[SMT_MAX_TEST] ;	
	u_long		uniq_time ;		
	u_short		uniq_ticks  ;		
	u_short		please_reconnect ;	
	u_long		smt_last_lem ;
	u_long		smt_last_notify ;
	struct smt_timer	smt_timer ;	
	u_long		last_tok_time[NUMMACS];	
} ;

#define SMT_DAS	0			
#define SMT_SAS	1			
#define SMT_NAC	2			

struct smt_config {
	u_char	attach_s ;		
	u_char	sas ;			
	u_char	build_ring_map ;	
	u_char	numphys ;		
	u_char	sc_pad[1] ;

	u_long	pcm_tb_min ;		
	u_long	pcm_tb_max ;		
	u_long	pcm_c_min ;		
	u_long	pcm_t_out ;		
	u_long	pcm_tl_min ;		
	u_long	pcm_lc_short ;		
	u_long	pcm_lc_medium ;		
	u_long	pcm_lc_long ;		
	u_long	pcm_lc_extended ;	
	u_long	pcm_t_next_9 ;		
	u_long	pcm_ns_max ;		

	u_long	ecm_i_max ;		
	u_long	ecm_in_max ;		
	u_long	ecm_td_min ;		
	u_long	ecm_test_done ;		
	u_long	ecm_check_poll ;	

	u_long	rmt_t_non_op ;		
	u_long	rmt_t_stuck ;		
	u_long	rmt_t_direct ;		
	u_long	rmt_t_jam ;		
	u_long	rmt_t_announce ;	
	u_long	rmt_t_poll ;		
	u_long  rmt_dup_mac_behavior ;  
	u_long	mac_d_max ;		

	u_long lct_short ;		
	u_long lct_medium ;		
	u_long lct_long ;		
	u_long lct_extended ;		
} ;

#ifdef	DEBUG
struct	smt_debug {
	int	d_smtf ;
	int	d_smt ;
	int	d_ecm ;
	int	d_rmt ;
	int	d_cfm ;
	int	d_pcm ;
	int	d_plc ;
#ifdef	ESS
	int	d_ess ;
#endif
#ifdef	SBA
	int	d_sba ;
#endif
	struct	os_debug	d_os;	
} ;

#ifndef	DEBUG_BRD
extern	struct	smt_debug	debug;	
#endif	

#endif	

struct s_smc {
	struct s_smt_os	os ;		
	struct s_smt_hw	hw ;		

	struct smt_config s ;		
	struct smt_values sm ;		
	struct s_ecm	e ;		
	struct s_rmt	r ;		
	struct s_cfm	cf ;		
#ifdef	CONCENTRATOR
	struct s_cem	ce[NUMPHYS] ;	
	struct s_c_ring	cr[NUMPHYS+NUMMACS] ;
#endif
	struct s_pcm	p ;		
	struct s_phy	y[NUMPHYS] ;	
	struct s_queue	q ;		
	struct s_timer	t ;		
	struct s_srf srf ;		
	struct s_srf_evc evcs[6+NUMPHYS*4] ;
	struct fddi_mib	mib ;		
#ifdef	SBA
	struct s_sba	sba ;		
#endif
#ifdef	ESS
	struct s_ess	ess ;		
#endif
#if	defined(DEBUG) && defined(DEBUG_BRD)
	
	struct smt_debug	debug;	
#endif	
} ;

extern const struct fddi_addr fddi_broadcast;

extern void all_selection_criteria(struct s_smc *smc);
extern void card_stop(struct s_smc *smc);
extern void init_board(struct s_smc *smc, u_char *mac_addr);
extern int init_fplus(struct s_smc *smc);
extern void init_plc(struct s_smc *smc);
extern int init_smt(struct s_smc *smc, u_char * mac_addr);
extern void mac1_irq(struct s_smc *smc, u_short stu, u_short stl);
extern void mac2_irq(struct s_smc *smc, u_short code_s2u, u_short code_s2l);
extern void mac3_irq(struct s_smc *smc, u_short code_s3u, u_short code_s3l);
extern int pcm_status_twisted(struct s_smc *smc);
extern void plc1_irq(struct s_smc *smc);
extern void plc2_irq(struct s_smc *smc);
extern void read_address(struct s_smc *smc, u_char * mac_addr);
extern void timer_irq(struct s_smc *smc);

#endif	

