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

 
#ifndef _SBA_
#define _SBA_

#include "mbuf.h"
#include "sba_def.h"

#ifdef	SBA

struct timer_cell {
	struct timer_cell	*next_ptr ;
	struct timer_cell	*prev_ptr ;
	u_long			start_time ;
	struct s_sba_node_vars	*node_var ;
} ;

struct s_sba_node_vars {
	u_char			change_resp_flag ;
	u_char			report_resp_flag ;
	u_char			change_req_flag ;
	u_char			report_req_flag ;
	long			change_amount ;
	long			node_overhead ;
	long			node_payload ;
	u_long			node_status ;
	u_char			deallocate_status ;
	u_char			timer_state ;
	u_short			report_cnt ;
	long			lastrep_req_tranid ;
	struct fddi_addr	mac_address ;
	struct s_sba_sessions 	*node_sessions ;
	struct timer_cell	timer ;
} ;

struct s_sba_sessions {
	u_long			deallocate_status ;
	long			session_overhead ;
	u_long			min_segment_size ;
	long			session_payload ;
	u_long			session_status ;
	u_long			sba_category ;
	long			lastchg_req_tranid ;
	u_short			session_id ;
	u_char			class ;
	u_char			fddi2 ;
	u_long			max_t_neg ;
	struct s_sba_sessions	*next_session ;
} ;

struct s_sba {

	struct s_sba_node_vars	node[MAX_NODES] ;
	struct s_sba_sessions	session[MAX_SESSIONS] ;

	struct s_sba_sessions	*free_session ;	
						

	struct timer_cell	*tail_timer ;	

	long	total_payload ;		
	long	total_overhead ;	
	long	sba_allocatable ;	

	long		msg_path_index ;	
	long		msg_sba_pl_req ;	
	long		msg_sba_ov_req ;	
	long		msg_mib_pl ;		
	long		msg_mib_ov ;		
	long		msg_category ;		
	u_long		msg_max_t_neg ;		
	u_long		msg_min_seg_siz ;	
	struct smt_header	*sm ;		
	struct fddi_addr	*msg_alloc_addr ;	

	u_long	sba_t_neg ;		
	long	sba_max_alloc ;			

	short	sba_next_state ;	
	char	sba_command ;		
	u_char	sba_available ;		
} ;

#endif	

struct s_ess {

	u_char	sync_bw_available ;	
	u_char	local_sba_active ;	
	char	raf_act_timer_poll ;	
	char	timer_count ;		

	SMbuf	*sba_reply_pend ;	
	
	long	sync_bw ;		
	u_long	alloc_trans_id ;	
} ;
#endif
