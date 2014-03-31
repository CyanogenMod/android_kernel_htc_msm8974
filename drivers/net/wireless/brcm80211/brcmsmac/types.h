/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _BRCM_TYPES_H_
#define _BRCM_TYPES_H_

#include <linux/types.h>
#include <linux/io.h>

#define WL_CHAN_FREQ_RANGE_2G      0
#define WL_CHAN_FREQ_RANGE_5GL     1
#define WL_CHAN_FREQ_RANGE_5GM     2
#define WL_CHAN_FREQ_RANGE_5GH     3


#define	BFL_PACTRL		0x00000002
#define	BFL_NOPLLDOWN		0x00000020
#define BFL_FEM			0x00000800
#define BFL_EXTLNA		0x00001000
#define BFL_NOPA		0x00010000
#define BFL_BUCKBOOST		0x00200000
#define BFL_FEM_BT		0x00400000
#define BFL_NOCBUCK		0x00800000
#define BFL_PALDO		0x02000000
#define BFL_EXTLNA_5GHz		0x10000000


#define BFL2_RXBB_INT_REG_DIS	0x00000001
#define BFL2_APLL_WAR		0x00000002
#define BFL2_TXPWRCTRL_EN	0x00000004
#define BFL2_2X4_DIV		0x00000008
#define BFL2_5G_PWRGAIN		0x00000010
#define BFL2_PCIEWAR_OVR	0x00000020
#define BFL2_LEGACY		0x00000080
#define BFL2_SKWRKFEM_BRD	0x00000100
#define BFL2_SPUR_WAR		0x00000200
#define BFL2_GPLL_WAR		0x00000400
#define BFL2_SINGLEANT_CCK	0x00001000
#define BFL2_2G_SPUR_WAR	0x00002000
#define BFL2_GPLL_WAR2	        0x00010000
#define BFL2_IPALVLSHIFT_3P3    0x00020000
#define BFL2_INTERNDET_TXIQCAL  0x00040000
#define BFL2_XTALBUFOUTEN       0x00080000


#define	BOARD_GPIO_PACTRL	0x200
#define BOARD_GPIO_12		0x1000
#define BOARD_GPIO_13		0x2000

#define D11CONF		0x0fffffb0	

#define NCONF		0x000001ff	

#define LCNCONF		0x00000007	

#define SSLPNCONF	0x0000000f	



#define CONF_HAS(config, val)	((config) & (1 << (val)))
#define CONF_MSK(config, mask)	((config) & (mask))
#define MSK_RANGE(low, hi)	((1 << ((hi)+1)) - (1 << (low)))
#define CONF_RANGE(config, low, hi) (CONF_MSK(config, MSK_RANGE(low, high)))

#define CONF_IS(config, val)	((config) == (1 << (val)))
#define CONF_GE(config, val)	((config) & (0-(1 << (val))))
#define CONF_GT(config, val)	((config) & (0-2*(1 << (val))))
#define CONF_LT(config, val)	((config) & ((1 << (val))-1))
#define CONF_LE(config, val)	((config) & (2*(1 << (val))-1))


#define NCONF_HAS(val)	CONF_HAS(NCONF, val)
#define NCONF_MSK(mask)	CONF_MSK(NCONF, mask)
#define NCONF_IS(val)	CONF_IS(NCONF, val)
#define NCONF_GE(val)	CONF_GE(NCONF, val)
#define NCONF_GT(val)	CONF_GT(NCONF, val)
#define NCONF_LT(val)	CONF_LT(NCONF, val)
#define NCONF_LE(val)	CONF_LE(NCONF, val)

#define LCNCONF_HAS(val)	CONF_HAS(LCNCONF, val)
#define LCNCONF_MSK(mask)	CONF_MSK(LCNCONF, mask)
#define LCNCONF_IS(val)		CONF_IS(LCNCONF, val)
#define LCNCONF_GE(val)		CONF_GE(LCNCONF, val)
#define LCNCONF_GT(val)		CONF_GT(LCNCONF, val)
#define LCNCONF_LT(val)		CONF_LT(LCNCONF, val)
#define LCNCONF_LE(val)		CONF_LE(LCNCONF, val)

#define D11CONF_HAS(val) CONF_HAS(D11CONF, val)
#define D11CONF_MSK(mask) CONF_MSK(D11CONF, mask)
#define D11CONF_IS(val)	CONF_IS(D11CONF, val)
#define D11CONF_GE(val)	CONF_GE(D11CONF, val)
#define D11CONF_GT(val)	CONF_GT(D11CONF, val)
#define D11CONF_LT(val)	CONF_LT(D11CONF, val)
#define D11CONF_LE(val)	CONF_LE(D11CONF, val)

#define PHYCONF_HAS(val) CONF_HAS(PHYTYPE, val)
#define PHYCONF_IS(val)	CONF_IS(PHYTYPE, val)

#define NREV_IS(var, val) \
	(NCONF_HAS(val) && (NCONF_IS(val) || ((var) == (val))))

#define NREV_GE(var, val) \
	(NCONF_GE(val) && (!NCONF_LT(val) || ((var) >= (val))))

#define NREV_GT(var, val) \
	(NCONF_GT(val) && (!NCONF_LE(val) || ((var) > (val))))

#define NREV_LT(var, val) \
	(NCONF_LT(val) && (!NCONF_GE(val) || ((var) < (val))))

#define NREV_LE(var, val) \
	(NCONF_LE(val) && (!NCONF_GT(val) || ((var) <= (val))))

#define LCNREV_IS(var, val) \
	(LCNCONF_HAS(val) && (LCNCONF_IS(val) || ((var) == (val))))

#define LCNREV_GE(var, val) \
	(LCNCONF_GE(val) && (!LCNCONF_LT(val) || ((var) >= (val))))

#define LCNREV_GT(var, val) \
	(LCNCONF_GT(val) && (!LCNCONF_LE(val) || ((var) > (val))))

#define LCNREV_LT(var, val) \
	(LCNCONF_LT(val) && (!LCNCONF_GE(val) || ((var) < (val))))

#define LCNREV_LE(var, val) \
	(LCNCONF_LE(val) && (!LCNCONF_GT(val) || ((var) <= (val))))

#define D11REV_IS(var, val) \
	(D11CONF_HAS(val) && (D11CONF_IS(val) || ((var) == (val))))

#define D11REV_GE(var, val) \
	(D11CONF_GE(val) && (!D11CONF_LT(val) || ((var) >= (val))))

#define D11REV_GT(var, val) \
	(D11CONF_GT(val) && (!D11CONF_LE(val) || ((var) > (val))))

#define D11REV_LT(var, val) \
	(D11CONF_LT(val) && (!D11CONF_GE(val) || ((var) < (val))))

#define D11REV_LE(var, val) \
	(D11CONF_LE(val) && (!D11CONF_GT(val) || ((var) <= (val))))

#define PHYTYPE_IS(var, val)\
	(PHYCONF_HAS(val) && (PHYCONF_IS(val) || ((var) == (val))))


#define _PHYCONF_N (1 << PHY_TYPE_N)
#define _PHYCONF_LCN (1 << PHY_TYPE_LCN)
#define _PHYCONF_SSLPN (1 << PHY_TYPE_SSN)

#define PHYTYPE (_PHYCONF_N | _PHYCONF_LCN | _PHYCONF_SSLPN)

#define PHYTYPE_11N_CAP(phytype) \
	(PHYTYPE_IS(phytype, PHY_TYPE_N) ||	\
	 PHYTYPE_IS(phytype, PHY_TYPE_LCN) || \
	 PHYTYPE_IS(phytype, PHY_TYPE_SSN))

#define BRCMS_ISNPHY(band)		PHYTYPE_IS((band)->phytype, PHY_TYPE_N)
#define BRCMS_ISLCNPHY(band)	PHYTYPE_IS((band)->phytype, PHY_TYPE_LCN)
#define BRCMS_ISSSLPNPHY(band)	PHYTYPE_IS((band)->phytype, PHY_TYPE_SSN)

#define BRCMS_PHY_11N_CAP(band)	PHYTYPE_11N_CAP((band)->phytype)


#define BCMMSG(dev, fmt, args...)		\
do {						\
	if (brcm_msg_level & LOG_TRACE_VAL)	\
		wiphy_err(dev, "%s: " fmt, __func__, ##args);	\
} while (0)

#ifdef CONFIG_BCM47XX
#define bcma_wflush16(c, o, v) \
	({ bcma_write16(c, o, v); (void)bcma_read16(c, o); })
#else
#define bcma_wflush16(c, o, v)	bcma_write16(c, o, v)
#endif				


#define mboolset(mb, bit)		((mb) |= (bit))
#define mboolclr(mb, bit)		((mb) &= ~(bit))
#define mboolisset(mb, bit)		(((mb) & (bit)) != 0)
#define	mboolmaskset(mb, mask, val)	((mb) = (((mb) & ~(mask)) | (val)))

#define CEIL(x, y)		(((x) + ((y)-1)) / (y))

struct wiphy;
struct ieee80211_sta;
struct ieee80211_tx_queue_params;
struct brcms_info;
struct brcms_c_info;
struct brcms_hardware;
struct brcms_txq_info;
struct brcms_band;
struct dma_pub;
struct si_pub;
struct tx_status;
struct d11rxhdr;
struct txpwr_limits;

struct brcmu_iovar {
	const char *name;	
	u16 varid;	
	u16 flags;	
	u16 type;	
	u16 minlen;	
};

extern u32 brcm_msg_level;

#endif				
