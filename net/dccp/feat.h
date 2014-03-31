#ifndef _DCCP_FEAT_H
#define _DCCP_FEAT_H
/*
 *  net/dccp/feat.h
 *
 *  Feature negotiation for the DCCP protocol (RFC 4340, section 6)
 *  Copyright (c) 2008 Gerrit Renker <gerrit@erg.abdn.ac.uk>
 *  Copyright (c) 2005 Andrea Bittau <a.bittau@cs.ucl.ac.uk>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/types.h>
#include "dccp.h"

#define DCCPF_ACK_RATIO_MAX	0xFFFF
#define DCCPF_SEQ_WMIN		32
#define DCCPF_SEQ_WMAX		0x3FFFFFFFFFFFull
#define DCCP_FEAT_MAX_SP_VALS	(DCCP_SINGLE_OPT_MAXLEN - 2)

enum dccp_feat_type {
	FEAT_AT_RX   = 1,	
	FEAT_AT_TX   = 2,	
	FEAT_SP      = 4,	
	FEAT_NN	     = 8,	
	FEAT_UNKNOWN = 0xFF	
};

enum dccp_feat_state {
	FEAT_DEFAULT = 0,	
	FEAT_INITIALISING,	
	FEAT_CHANGING,		
	FEAT_UNSTABLE,		
	FEAT_STABLE		
};

typedef union {
	u64 nn;
	struct {
		u8	*vec;
		u8	len;
	}   sp;
} dccp_feat_val;

struct dccp_feat_entry {
	dccp_feat_val           val;
	enum dccp_feat_state    state:8;
	u8                      feat_num;

	bool			needs_mandatory,
				needs_confirm,
				empty_confirm,
				is_local;

	struct list_head	node;
};

static inline u8 dccp_feat_genopt(struct dccp_feat_entry *entry)
{
	if (entry->needs_confirm)
		return entry->is_local ? DCCPO_CONFIRM_L : DCCPO_CONFIRM_R;
	return entry->is_local ? DCCPO_CHANGE_L : DCCPO_CHANGE_R;
}

struct ccid_dependency {
	u8	dependent_feat;
	bool	is_local:1,
		is_mandatory:1;
	u8	val;
};

extern unsigned long sysctl_dccp_sequence_window;
extern int	     sysctl_dccp_rx_ccid;
extern int	     sysctl_dccp_tx_ccid;

extern int  dccp_feat_init(struct sock *sk);
extern void dccp_feat_initialise_sysctls(void);
extern int  dccp_feat_register_sp(struct sock *sk, u8 feat, u8 is_local,
				  u8 const *list, u8 len);
extern int  dccp_feat_parse_options(struct sock *, struct dccp_request_sock *,
				    u8 mand, u8 opt, u8 feat, u8 *val, u8 len);
extern int  dccp_feat_clone_list(struct list_head const *, struct list_head *);

#define DCCP_OPTVAL_MAXLEN	6

extern void dccp_encode_value_var(const u64 value, u8 *to, const u8 len);
extern u64  dccp_decode_value_var(const u8 *bf, const u8 len);
extern u64  dccp_feat_nn_get(struct sock *sk, u8 feat);

extern int  dccp_insert_option_mandatory(struct sk_buff *skb);
extern int  dccp_insert_fn_opt(struct sk_buff *skb, u8 type, u8 feat,
			       u8 *val, u8 len, bool repeat_first);
#endif 
