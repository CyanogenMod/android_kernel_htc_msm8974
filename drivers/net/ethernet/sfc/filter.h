/****************************************************************************
 * Driver for Solarflare Solarstorm network controllers and boards
 * Copyright 2005-2010 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#ifndef EFX_FILTER_H
#define EFX_FILTER_H

#include <linux/types.h>

enum efx_filter_type {
	EFX_FILTER_TCP_FULL = 0,
	EFX_FILTER_TCP_WILD,
	EFX_FILTER_UDP_FULL,
	EFX_FILTER_UDP_WILD,
	EFX_FILTER_MAC_FULL = 4,
	EFX_FILTER_MAC_WILD,
	EFX_FILTER_UC_DEF = 8,
	EFX_FILTER_MC_DEF,
	EFX_FILTER_TYPE_COUNT,		
	EFX_FILTER_UNSPEC = 0xf,
};

enum efx_filter_priority {
	EFX_FILTER_PRI_HINT = 0,
	EFX_FILTER_PRI_MANUAL,
	EFX_FILTER_PRI_REQUIRED,
};

enum efx_filter_flags {
	EFX_FILTER_FLAG_RX_RSS = 0x01,
	EFX_FILTER_FLAG_RX_SCATTER = 0x02,
	EFX_FILTER_FLAG_RX_OVERRIDE_IP = 0x04,
	EFX_FILTER_FLAG_RX = 0x08,
	EFX_FILTER_FLAG_TX = 0x10,
};

struct efx_filter_spec {
	u8	type:4;
	u8	priority:4;
	u8	flags;
	u16	dmaq_id;
	u32	data[3];
};

static inline void efx_filter_init_rx(struct efx_filter_spec *spec,
				      enum efx_filter_priority priority,
				      enum efx_filter_flags flags,
				      unsigned rxq_id)
{
	spec->type = EFX_FILTER_UNSPEC;
	spec->priority = priority;
	spec->flags = EFX_FILTER_FLAG_RX | flags;
	spec->dmaq_id = rxq_id;
}

static inline void efx_filter_init_tx(struct efx_filter_spec *spec,
				      unsigned txq_id)
{
	spec->type = EFX_FILTER_UNSPEC;
	spec->priority = EFX_FILTER_PRI_REQUIRED;
	spec->flags = EFX_FILTER_FLAG_TX;
	spec->dmaq_id = txq_id;
}

extern int efx_filter_set_ipv4_local(struct efx_filter_spec *spec, u8 proto,
				     __be32 host, __be16 port);
extern int efx_filter_get_ipv4_local(const struct efx_filter_spec *spec,
				     u8 *proto, __be32 *host, __be16 *port);
extern int efx_filter_set_ipv4_full(struct efx_filter_spec *spec, u8 proto,
				    __be32 host, __be16 port,
				    __be32 rhost, __be16 rport);
extern int efx_filter_get_ipv4_full(const struct efx_filter_spec *spec,
				    u8 *proto, __be32 *host, __be16 *port,
				    __be32 *rhost, __be16 *rport);
extern int efx_filter_set_eth_local(struct efx_filter_spec *spec,
				    u16 vid, const u8 *addr);
extern int efx_filter_get_eth_local(const struct efx_filter_spec *spec,
				    u16 *vid, u8 *addr);
extern int efx_filter_set_uc_def(struct efx_filter_spec *spec);
extern int efx_filter_set_mc_def(struct efx_filter_spec *spec);
enum {
	EFX_FILTER_VID_UNSPEC = 0xffff,
};

#endif 
