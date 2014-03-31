#ifndef _DCCP_LI_HIST_
#define _DCCP_LI_HIST_
/*
 *  Copyright (c) 2007   The University of Aberdeen, Scotland, UK
 *  Copyright (c) 2005-7 The University of Waikato, Hamilton, New Zealand.
 *  Copyright (c) 2005-7 Ian McDonald <ian.mcdonald@jandi.co.nz>
 *  Copyright (c) 2005 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 */
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/slab.h>

#define NINTERVAL	8
#define LIH_SIZE	(NINTERVAL + 1)

struct tfrc_loss_interval {
	u64		 li_seqno:48,
			 li_ccval:4,
			 li_is_closed:1;
	u32		 li_length;
};

struct tfrc_loss_hist {
	struct tfrc_loss_interval	*ring[LIH_SIZE];
	u8				counter;
	u32				i_mean;
};

static inline void tfrc_lh_init(struct tfrc_loss_hist *lh)
{
	memset(lh, 0, sizeof(struct tfrc_loss_hist));
}

static inline u8 tfrc_lh_is_initialised(struct tfrc_loss_hist *lh)
{
	return lh->counter > 0;
}

static inline u8 tfrc_lh_length(struct tfrc_loss_hist *lh)
{
	return min(lh->counter, (u8)LIH_SIZE);
}

struct tfrc_rx_hist;

extern int  tfrc_lh_interval_add(struct tfrc_loss_hist *, struct tfrc_rx_hist *,
				 u32 (*first_li)(struct sock *), struct sock *);
extern u8   tfrc_lh_update_i_mean(struct tfrc_loss_hist *lh, struct sk_buff *);
extern void tfrc_lh_cleanup(struct tfrc_loss_hist *lh);

#endif 
