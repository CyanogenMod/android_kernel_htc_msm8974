/* SCTP kernel implementation
 * (C) Copyright IBM Corp. 2001, 2004
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2001 Intel Corp.
 * Copyright (c) 2001 Nokia, Inc.
 *
 * This file is part of the SCTP kernel implementation
 *
 * These are the state tables for the SCTP state machine.
 *
 * This SCTP implementation is free software;
 * you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 *
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 * Written or modified by:
 *    La Monte H.P. Yarroll <piggy@acm.org>
 *    Karl Knutson          <karl@athena.chicago.il.us>
 *    Jon Grimm             <jgrimm@us.ibm.com>
 *    Hui Huang		    <hui.huang@nokia.com>
 *    Daisy Chang	    <daisyc@us.ibm.com>
 *    Ardelle Fan	    <ardelle.fan@intel.com>
 *    Sridhar Samudrala	    <sri@us.ibm.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/skbuff.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

static const sctp_sm_table_entry_t
primitive_event_table[SCTP_NUM_PRIMITIVE_TYPES][SCTP_STATE_NUM_STATES];
static const sctp_sm_table_entry_t
other_event_table[SCTP_NUM_OTHER_TYPES][SCTP_STATE_NUM_STATES];
static const sctp_sm_table_entry_t
timeout_event_table[SCTP_NUM_TIMEOUT_TYPES][SCTP_STATE_NUM_STATES];

static const sctp_sm_table_entry_t *sctp_chunk_event_lookup(sctp_cid_t cid,
							    sctp_state_t state);


static const sctp_sm_table_entry_t bug = {
	.fn = sctp_sf_bug,
	.name = "sctp_sf_bug"
};

#define DO_LOOKUP(_max, _type, _table)					\
({									\
	const sctp_sm_table_entry_t *rtn;				\
									\
	if ((event_subtype._type > (_max))) {				\
		pr_warn("table %p possible attack: event %d exceeds max %d\n", \
			_table, event_subtype._type, _max);		\
	        rtn = &bug;						\
	} else								\
		rtn = &_table[event_subtype._type][(int)state];		\
									\
	rtn;								\
})

const sctp_sm_table_entry_t *sctp_sm_lookup_event(sctp_event_t event_type,
						  sctp_state_t state,
						  sctp_subtype_t event_subtype)
{
	switch (event_type) {
	case SCTP_EVENT_T_CHUNK:
		return sctp_chunk_event_lookup(event_subtype.chunk, state);
	case SCTP_EVENT_T_TIMEOUT:
		return DO_LOOKUP(SCTP_EVENT_TIMEOUT_MAX, timeout,
				 timeout_event_table);
	case SCTP_EVENT_T_OTHER:
		return DO_LOOKUP(SCTP_EVENT_OTHER_MAX, other,
				 other_event_table);
	case SCTP_EVENT_T_PRIMITIVE:
		return DO_LOOKUP(SCTP_EVENT_PRIMITIVE_MAX, primitive,
				 primitive_event_table);
	default:
		
		return &bug;
	}
}

#define TYPE_SCTP_FUNC(func) {.fn = func, .name = #func}

#define TYPE_SCTP_DATA { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ootb), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_data_6_2), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_data_6_2), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_data_fast_4_4), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

#define TYPE_SCTP_INIT { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_1B_init), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_1_siminit), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_1_siminit), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_2_dupinit), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_2_dupinit), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_2_dupinit), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_2_dupinit), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_2_reshutack), \
} 

#define TYPE_SCTP_INIT_ACK { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_3_initack), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_1C_ack), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

#define TYPE_SCTP_SACK { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ootb), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_sack_6_2), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_sack_6_2), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_sack_6_2), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_sack_6_2), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

#define TYPE_SCTP_HEARTBEAT { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ootb), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_beat_8_3), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_beat_8_3), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_beat_8_3), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_beat_8_3), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_beat_8_3), \
	 \
	 \
	TYPE_SCTP_FUNC(sctp_sf_beat_8_3), \
} 

#define TYPE_SCTP_HEARTBEAT_ACK { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ootb), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_violation), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_backbeat_8_3), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_backbeat_8_3), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_backbeat_8_3), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_backbeat_8_3), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

#define TYPE_SCTP_ABORT { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_pdiscard), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_cookie_wait_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_cookie_echoed_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_1_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_shutdown_pending_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_shutdown_sent_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_1_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_shutdown_ack_sent_abort), \
} 

#define TYPE_SCTP_SHUTDOWN { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ootb), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_2_shutdown), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_2_shutdown), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_2_shutdown_ack), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_2_shut_ctsn), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

#define TYPE_SCTP_SHUTDOWN_ACK { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ootb), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_8_5_1_E_sa), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_8_5_1_E_sa), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_violation), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_violation), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_2_final), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_violation), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_2_final), \
} 

#define TYPE_SCTP_ERROR { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ootb), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_cookie_echoed_err), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_operr_notify), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_operr_notify), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_operr_notify), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

#define TYPE_SCTP_COOKIE_ECHO { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_1D_ce), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_4_dupcook), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_4_dupcook), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_4_dupcook), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_4_dupcook), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_4_dupcook), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_4_dupcook), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_2_4_dupcook), \
} 

#define TYPE_SCTP_COOKIE_ACK { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_5_1E_ca), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

#define TYPE_SCTP_ECN_ECNE { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_ecne), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_ecne), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_ecne), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_ecne), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_ecne), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

#define TYPE_SCTP_ECN_CWR { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_ecn_cwr), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_ecn_cwr), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_ecn_cwr), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

#define TYPE_SCTP_SHUTDOWN_COMPLETE { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_4_C), \
} 

static const sctp_sm_table_entry_t chunk_event_table[SCTP_NUM_BASE_CHUNK_TYPES][SCTP_STATE_NUM_STATES] = {
	TYPE_SCTP_DATA,
	TYPE_SCTP_INIT,
	TYPE_SCTP_INIT_ACK,
	TYPE_SCTP_SACK,
	TYPE_SCTP_HEARTBEAT,
	TYPE_SCTP_HEARTBEAT_ACK,
	TYPE_SCTP_ABORT,
	TYPE_SCTP_SHUTDOWN,
	TYPE_SCTP_SHUTDOWN_ACK,
	TYPE_SCTP_ERROR,
	TYPE_SCTP_COOKIE_ECHO,
	TYPE_SCTP_COOKIE_ACK,
	TYPE_SCTP_ECN_ECNE,
	TYPE_SCTP_ECN_CWR,
	TYPE_SCTP_SHUTDOWN_COMPLETE,
}; 

#define TYPE_SCTP_ASCONF { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_asconf), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_asconf), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_asconf), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_asconf), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

#define TYPE_SCTP_ASCONF_ACK { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_asconf_ack), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_asconf_ack), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_asconf_ack), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_asconf_ack), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

static const sctp_sm_table_entry_t addip_chunk_event_table[SCTP_NUM_ADDIP_CHUNK_TYPES][SCTP_STATE_NUM_STATES] = {
	TYPE_SCTP_ASCONF,
	TYPE_SCTP_ASCONF_ACK,
}; 

#define TYPE_SCTP_FWD_TSN { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ootb), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_fwd_tsn), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_fwd_tsn), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_fwd_tsn_fast), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
} 

static const sctp_sm_table_entry_t prsctp_chunk_event_table[SCTP_NUM_PRSCTP_CHUNK_TYPES][SCTP_STATE_NUM_STATES] = {
	TYPE_SCTP_FWD_TSN,
}; 

#define TYPE_SCTP_AUTH { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ootb), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_discard_chunk), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_auth), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_auth), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_auth), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_auth), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_auth), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_eat_auth), \
} 

static const sctp_sm_table_entry_t auth_chunk_event_table[SCTP_NUM_AUTH_CHUNK_TYPES][SCTP_STATE_NUM_STATES] = {
	TYPE_SCTP_AUTH,
}; 

static const sctp_sm_table_entry_t
chunk_event_table_unknown[SCTP_STATE_NUM_STATES] = {
	
	TYPE_SCTP_FUNC(sctp_sf_ootb),
	
	TYPE_SCTP_FUNC(sctp_sf_unk_chunk),
	
	TYPE_SCTP_FUNC(sctp_sf_unk_chunk),
	
	TYPE_SCTP_FUNC(sctp_sf_unk_chunk),
	
	TYPE_SCTP_FUNC(sctp_sf_unk_chunk),
	
	TYPE_SCTP_FUNC(sctp_sf_unk_chunk),
	
	TYPE_SCTP_FUNC(sctp_sf_unk_chunk),
	
	TYPE_SCTP_FUNC(sctp_sf_unk_chunk),
};	


#define TYPE_SCTP_PRIMITIVE_ASSOCIATE  { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_asoc), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_not_impl), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_not_impl), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_not_impl), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_not_impl), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_not_impl), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_not_impl), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_not_impl), \
} 

#define TYPE_SCTP_PRIMITIVE_SHUTDOWN  { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_closed), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_cookie_wait_prm_shutdown), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_cookie_echoed_prm_shutdown),\
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_2_prm_shutdown), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_primitive), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_primitive), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_primitive), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_primitive), \
} 

#define TYPE_SCTP_PRIMITIVE_ABORT  { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_closed), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_cookie_wait_prm_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_cookie_echoed_prm_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_1_prm_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_shutdown_pending_prm_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_shutdown_sent_prm_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_1_prm_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_shutdown_ack_sent_prm_abort), \
} 

#define TYPE_SCTP_PRIMITIVE_SEND  { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_closed), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_send), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_send), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_send), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_shutdown), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_shutdown), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_shutdown), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_shutdown), \
} 

#define TYPE_SCTP_PRIMITIVE_REQUESTHEARTBEAT  { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_closed), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_requestheartbeat),          \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_requestheartbeat),          \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_requestheartbeat),          \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_requestheartbeat),          \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_requestheartbeat),          \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_requestheartbeat),          \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_requestheartbeat),          \
} 

#define TYPE_SCTP_PRIMITIVE_ASCONF { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_closed), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_closed), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_closed), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_asconf), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_asconf), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_asconf), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_prm_asconf), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_error_shutdown), \
} 

static const sctp_sm_table_entry_t primitive_event_table[SCTP_NUM_PRIMITIVE_TYPES][SCTP_STATE_NUM_STATES] = {
	TYPE_SCTP_PRIMITIVE_ASSOCIATE,
	TYPE_SCTP_PRIMITIVE_SHUTDOWN,
	TYPE_SCTP_PRIMITIVE_ABORT,
	TYPE_SCTP_PRIMITIVE_SEND,
	TYPE_SCTP_PRIMITIVE_REQUESTHEARTBEAT,
	TYPE_SCTP_PRIMITIVE_ASCONF,
};

#define TYPE_SCTP_OTHER_NO_PENDING_TSN  { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_no_pending_tsn), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_2_start_shutdown), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_9_2_shutdown_ack), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
}

#define TYPE_SCTP_OTHER_ICMP_PROTO_UNREACH  { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_cookie_wait_icmp_abort), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_ignore_other), \
}

static const sctp_sm_table_entry_t other_event_table[SCTP_NUM_OTHER_TYPES][SCTP_STATE_NUM_STATES] = {
	TYPE_SCTP_OTHER_NO_PENDING_TSN,
	TYPE_SCTP_OTHER_ICMP_PROTO_UNREACH,
};

#define TYPE_SCTP_EVENT_TIMEOUT_NONE { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_bug), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_bug), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_bug), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_bug), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_bug), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_bug), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_bug), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_bug), \
}

#define TYPE_SCTP_EVENT_TIMEOUT_T1_COOKIE { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_bug), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_t1_cookie_timer_expire), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
}

#define TYPE_SCTP_EVENT_TIMEOUT_T1_INIT { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_t1_init_timer_expire), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
}

#define TYPE_SCTP_EVENT_TIMEOUT_T2_SHUTDOWN { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_t2_timer_expire), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_t2_timer_expire), \
}

#define TYPE_SCTP_EVENT_TIMEOUT_T3_RTX { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_6_3_3_rtx), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_6_3_3_rtx), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_6_3_3_rtx), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_6_3_3_rtx), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
}

#define TYPE_SCTP_EVENT_TIMEOUT_T4_RTO { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_t4_timer_expire), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
}

#define TYPE_SCTP_EVENT_TIMEOUT_T5_SHUTDOWN_GUARD { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_t5_timer_expire), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_t5_timer_expire), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
}

#define TYPE_SCTP_EVENT_TIMEOUT_HEARTBEAT { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_sendbeat_8_3), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_sendbeat_8_3), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_sendbeat_8_3), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
}

#define TYPE_SCTP_EVENT_TIMEOUT_SACK { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_6_2_sack), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_6_2_sack), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_do_6_2_sack), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
}

#define TYPE_SCTP_EVENT_TIMEOUT_AUTOCLOSE { \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_autoclose_timer_expire), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
	 \
	TYPE_SCTP_FUNC(sctp_sf_timer_ignore), \
}

static const sctp_sm_table_entry_t timeout_event_table[SCTP_NUM_TIMEOUT_TYPES][SCTP_STATE_NUM_STATES] = {
	TYPE_SCTP_EVENT_TIMEOUT_NONE,
	TYPE_SCTP_EVENT_TIMEOUT_T1_COOKIE,
	TYPE_SCTP_EVENT_TIMEOUT_T1_INIT,
	TYPE_SCTP_EVENT_TIMEOUT_T2_SHUTDOWN,
	TYPE_SCTP_EVENT_TIMEOUT_T3_RTX,
	TYPE_SCTP_EVENT_TIMEOUT_T4_RTO,
	TYPE_SCTP_EVENT_TIMEOUT_T5_SHUTDOWN_GUARD,
	TYPE_SCTP_EVENT_TIMEOUT_HEARTBEAT,
	TYPE_SCTP_EVENT_TIMEOUT_SACK,
	TYPE_SCTP_EVENT_TIMEOUT_AUTOCLOSE,
};

static const sctp_sm_table_entry_t *sctp_chunk_event_lookup(sctp_cid_t cid,
							    sctp_state_t state)
{
	if (state > SCTP_STATE_MAX)
		return &bug;

	if (cid <= SCTP_CID_BASE_MAX)
		return &chunk_event_table[cid][state];

	if (sctp_prsctp_enable) {
		if (cid == SCTP_CID_FWD_TSN)
			return &prsctp_chunk_event_table[0][state];
	}

	if (sctp_addip_enable) {
		if (cid == SCTP_CID_ASCONF)
			return &addip_chunk_event_table[0][state];

		if (cid == SCTP_CID_ASCONF_ACK)
			return &addip_chunk_event_table[1][state];
	}

	if (sctp_auth_enable) {
		if (cid == SCTP_CID_AUTH)
			return &auth_chunk_event_table[0][state];
	}

	return &chunk_event_table_unknown[state];
}
