/****************************************************************************
 * Driver for Solarflare Solarstorm network controllers and boards
 * Copyright 2007-2009 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#ifndef EFX_ENUM_H
#define EFX_ENUM_H

enum efx_loopback_mode {
	LOOPBACK_NONE = 0,
	LOOPBACK_DATA = 1,
	LOOPBACK_GMAC = 2,
	LOOPBACK_XGMII = 3,
	LOOPBACK_XGXS = 4,
	LOOPBACK_XAUI = 5,
	LOOPBACK_GMII = 6,
	LOOPBACK_SGMII = 7,
	LOOPBACK_XGBR = 8,
	LOOPBACK_XFI = 9,
	LOOPBACK_XAUI_FAR = 10,
	LOOPBACK_GMII_FAR = 11,
	LOOPBACK_SGMII_FAR = 12,
	LOOPBACK_XFI_FAR = 13,
	LOOPBACK_GPHY = 14,
	LOOPBACK_PHYXS = 15,
	LOOPBACK_PCS = 16,
	LOOPBACK_PMAPMD = 17,
	LOOPBACK_XPORT = 18,
	LOOPBACK_XGMII_WS = 19,
	LOOPBACK_XAUI_WS = 20,
	LOOPBACK_XAUI_WS_FAR = 21,
	LOOPBACK_XAUI_WS_NEAR = 22,
	LOOPBACK_GMII_WS = 23,
	LOOPBACK_XFI_WS = 24,
	LOOPBACK_XFI_WS_FAR = 25,
	LOOPBACK_PHYXS_WS = 26,
	LOOPBACK_MAX
};
#define LOOPBACK_TEST_MAX LOOPBACK_PMAPMD

#define LOOPBACKS_INTERNAL ((1 << LOOPBACK_DATA) |		\
			    (1 << LOOPBACK_GMAC) |		\
			    (1 << LOOPBACK_XGMII)|		\
			    (1 << LOOPBACK_XGXS) |		\
			    (1 << LOOPBACK_XAUI) |		\
			    (1 << LOOPBACK_GMII) |		\
			    (1 << LOOPBACK_SGMII) |		\
			    (1 << LOOPBACK_SGMII) |		\
			    (1 << LOOPBACK_XGBR) |		\
			    (1 << LOOPBACK_XFI) |		\
			    (1 << LOOPBACK_XAUI_FAR) |		\
			    (1 << LOOPBACK_GMII_FAR) |		\
			    (1 << LOOPBACK_SGMII_FAR) |		\
			    (1 << LOOPBACK_XFI_FAR) |		\
			    (1 << LOOPBACK_XGMII_WS) |		\
			    (1 << LOOPBACK_XAUI_WS) |		\
			    (1 << LOOPBACK_XAUI_WS_FAR) |	\
			    (1 << LOOPBACK_XAUI_WS_NEAR) |	\
			    (1 << LOOPBACK_GMII_WS) |		\
			    (1 << LOOPBACK_XFI_WS) |		\
			    (1 << LOOPBACK_XFI_WS_FAR))

#define LOOPBACKS_WS ((1 << LOOPBACK_XGMII_WS) |		\
		      (1 << LOOPBACK_XAUI_WS) |			\
		      (1 << LOOPBACK_XAUI_WS_FAR) |		\
		      (1 << LOOPBACK_XAUI_WS_NEAR) |		\
		      (1 << LOOPBACK_GMII_WS) |			\
		      (1 << LOOPBACK_XFI_WS) |			\
		      (1 << LOOPBACK_XFI_WS_FAR) |		\
		      (1 << LOOPBACK_PHYXS_WS))

#define LOOPBACKS_EXTERNAL(_efx)					\
	((_efx)->loopback_modes & ~LOOPBACKS_INTERNAL &			\
	 ~(1 << LOOPBACK_NONE))

#define LOOPBACK_MASK(_efx)			\
	(1 << (_efx)->loopback_mode)

#define LOOPBACK_INTERNAL(_efx)				\
	(!!(LOOPBACKS_INTERNAL & LOOPBACK_MASK(_efx)))

#define LOOPBACK_EXTERNAL(_efx)				\
	(!!(LOOPBACK_MASK(_efx) & LOOPBACKS_EXTERNAL(_efx)))

#define LOOPBACK_CHANGED(_from, _to, _mask)				\
	(!!((LOOPBACK_MASK(_from) ^ LOOPBACK_MASK(_to)) & (_mask)))

#define LOOPBACK_OUT_OF(_from, _to, _mask)				\
	((LOOPBACK_MASK(_from) & (_mask)) && !(LOOPBACK_MASK(_to) & (_mask)))


enum reset_type {
	RESET_TYPE_INVISIBLE = 0,
	RESET_TYPE_ALL = 1,
	RESET_TYPE_WORLD = 2,
	RESET_TYPE_DISABLE = 3,
	RESET_TYPE_MAX_METHOD,
	RESET_TYPE_TX_WATCHDOG,
	RESET_TYPE_INT_ERROR,
	RESET_TYPE_RX_RECOVERY,
	RESET_TYPE_RX_DESC_FETCH,
	RESET_TYPE_TX_DESC_FETCH,
	RESET_TYPE_TX_SKIP,
	RESET_TYPE_MC_FAILURE,
	RESET_TYPE_MAX,
};

#endif 
