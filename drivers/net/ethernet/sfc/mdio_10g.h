/****************************************************************************
 * Driver for Solarflare Solarstorm network controllers and boards
 * Copyright 2006-2011 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#ifndef EFX_MDIO_10G_H
#define EFX_MDIO_10G_H

#include <linux/mdio.h>


#include "efx.h"

static inline unsigned efx_mdio_id_rev(u32 id) { return id & 0xf; }
static inline unsigned efx_mdio_id_model(u32 id) { return (id >> 4) & 0x3f; }
extern unsigned efx_mdio_id_oui(u32 id);

static inline int efx_mdio_read(struct efx_nic *efx, int devad, int addr)
{
	return efx->mdio.mdio_read(efx->net_dev, efx->mdio.prtad, devad, addr);
}

static inline void
efx_mdio_write(struct efx_nic *efx, int devad, int addr, int value)
{
	efx->mdio.mdio_write(efx->net_dev, efx->mdio.prtad, devad, addr, value);
}

static inline u32 efx_mdio_read_id(struct efx_nic *efx, int mmd)
{
	u16 id_low = efx_mdio_read(efx, mmd, MDIO_DEVID2);
	u16 id_hi = efx_mdio_read(efx, mmd, MDIO_DEVID1);
	return (id_hi << 16) | (id_low);
}

static inline bool efx_mdio_phyxgxs_lane_sync(struct efx_nic *efx)
{
	int i, lane_status;
	bool sync;

	for (i = 0; i < 2; ++i)
		lane_status = efx_mdio_read(efx, MDIO_MMD_PHYXS,
					    MDIO_PHYXS_LNSTAT);

	sync = !!(lane_status & MDIO_PHYXS_LNSTAT_ALIGN);
	if (!sync)
		netif_dbg(efx, hw, efx->net_dev, "XGXS lane status: %x\n",
			  lane_status);
	return sync;
}

extern const char *efx_mdio_mmd_name(int mmd);

extern int efx_mdio_reset_mmd(struct efx_nic *efx, int mmd,
			      int spins, int spintime);

int efx_mdio_check_mmds(struct efx_nic *efx, unsigned int mmd_mask);

extern bool efx_mdio_links_ok(struct efx_nic *efx, unsigned int mmd_mask);

extern void efx_mdio_transmit_disable(struct efx_nic *efx);

extern void efx_mdio_phy_reconfigure(struct efx_nic *efx);

extern void efx_mdio_set_mmds_lpower(struct efx_nic *efx,
				     int low_power, unsigned int mmd_mask);

extern int efx_mdio_set_settings(struct efx_nic *efx, struct ethtool_cmd *ecmd);

extern void efx_mdio_an_reconfigure(struct efx_nic *efx);

u8 efx_mdio_get_pause(struct efx_nic *efx);

extern int efx_mdio_wait_reset_mmds(struct efx_nic *efx,
				    unsigned int mmd_mask);

static inline void
efx_mdio_set_flag(struct efx_nic *efx, int devad, int addr,
		  int mask, bool state)
{
	mdio_set_flag(&efx->mdio, efx->mdio.prtad, devad, addr, mask, state);
}

extern int efx_mdio_test_alive(struct efx_nic *efx);

#endif 
