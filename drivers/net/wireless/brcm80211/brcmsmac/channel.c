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

#include <linux/types.h>
#include <net/mac80211.h>

#include <defs.h>
#include "pub.h"
#include "phy/phy_hal.h"
#include "main.h"
#include "stf.h"
#include "channel.h"

#define QDB(n) ((n) * BRCMS_TXPWR_DB_FACTOR)

#define  LOCALE_CHAN_01_11	 (1<<0)
#define  LOCALE_CHAN_12_13	 (1<<1)
#define  LOCALE_CHAN_14		 (1<<2)
#define  LOCALE_SET_5G_LOW_JP1   (1<<3)	
#define  LOCALE_SET_5G_LOW_JP2   (1<<4)	
#define  LOCALE_SET_5G_LOW1      (1<<5)	
#define  LOCALE_SET_5G_LOW2      (1<<6)	
#define  LOCALE_SET_5G_LOW3      (1<<7)	
#define  LOCALE_SET_5G_MID1      (1<<8)	
#define  LOCALE_SET_5G_MID2	 (1<<9)	
#define  LOCALE_SET_5G_MID3      (1<<10)	
#define  LOCALE_SET_5G_HIGH1     (1<<11)	
#define  LOCALE_SET_5G_HIGH2     (1<<12)	
#define  LOCALE_SET_5G_HIGH3     (1<<13)	
#define  LOCALE_CHAN_52_140_ALL  (1<<14)
#define  LOCALE_SET_5G_HIGH4     (1<<15)	

#define  LOCALE_CHAN_36_64	(LOCALE_SET_5G_LOW1 | \
				 LOCALE_SET_5G_LOW2 | \
				 LOCALE_SET_5G_LOW3)
#define  LOCALE_CHAN_52_64	(LOCALE_SET_5G_LOW2 | LOCALE_SET_5G_LOW3)
#define  LOCALE_CHAN_100_124	(LOCALE_SET_5G_MID1 | LOCALE_SET_5G_MID2)
#define  LOCALE_CHAN_100_140	(LOCALE_SET_5G_MID1 | LOCALE_SET_5G_MID2 | \
				  LOCALE_SET_5G_MID3 | LOCALE_SET_5G_HIGH1)
#define  LOCALE_CHAN_149_165	(LOCALE_SET_5G_HIGH2 | LOCALE_SET_5G_HIGH3)
#define  LOCALE_CHAN_184_216	LOCALE_SET_5G_HIGH4

#define  LOCALE_CHAN_01_14	(LOCALE_CHAN_01_11 | \
				 LOCALE_CHAN_12_13 | \
				 LOCALE_CHAN_14)

#define  LOCALE_RADAR_SET_NONE		  0
#define  LOCALE_RADAR_SET_1		  1

#define  LOCALE_RESTRICTED_NONE		  0
#define  LOCALE_RESTRICTED_SET_2G_SHORT   1
#define  LOCALE_RESTRICTED_CHAN_165       2
#define  LOCALE_CHAN_ALL_5G		  3
#define  LOCALE_RESTRICTED_JAPAN_LEGACY   4
#define  LOCALE_RESTRICTED_11D_2G	  5
#define  LOCALE_RESTRICTED_11D_5G	  6
#define  LOCALE_RESTRICTED_LOW_HI	  7
#define  LOCALE_RESTRICTED_12_13_14	  8

#define LOCALE_2G_IDX_i			0
#define LOCALE_5G_IDX_11		0
#define LOCALE_MIMO_IDX_bn		0
#define LOCALE_MIMO_IDX_11n		0

#define BRCMS_MAXPWR_TBL_SIZE		6
#define BRCMS_MAXPWR_MIMO_TBL_SIZE	14


#define BAND_5G_PWR_LVLS	5	

#define LC(id)	LOCALE_MIMO_IDX_ ## id

#define LC_2G(id)	LOCALE_2G_IDX_ ## id

#define LC_5G(id)	LOCALE_5G_IDX_ ## id

#define LOCALES(band2, band5, mimo2, mimo5) \
		{LC_2G(band2), LC_5G(band5), LC(mimo2), LC(mimo5)}

#define CHANNEL_POWER_IDX_2G_CCK(c) (((c) < 2) ? 0 : (((c) < 11) ? 1 : 2))
#define CHANNEL_POWER_IDX_2G_OFDM(c) (((c) < 2) ? 3 : (((c) < 11) ? 4 : 5))

#define CHANNEL_POWER_IDX_5G(c) (((c) < 52) ? 0 : \
				 (((c) < 62) ? 1 : \
				 (((c) < 100) ? 2 : \
				 (((c) < 149) ? 3 : 4))))

#define ISDFS_EU(fl)		(((fl) & BRCMS_DFS_EU) == BRCMS_DFS_EU)

struct brcms_cm_band {
	
	u8 locale_flags;
	
	struct brcms_chanvec valid_channels;
	
	const struct brcms_chanvec *restricted_channels;
	
	const struct brcms_chanvec *radar_channels;
	u8 PAD[8];
};

struct locale_mimo_info {
	
	s8 maxpwr20[BRCMS_MAXPWR_MIMO_TBL_SIZE];
	
	s8 maxpwr40[BRCMS_MAXPWR_MIMO_TBL_SIZE];
	u8 flags;
};

struct country_info {
	const u8 locale_2G;	
	const u8 locale_5G;	
	const u8 locale_mimo_2G;	
	const u8 locale_mimo_5G;	
};

struct brcms_cm_info {
	struct brcms_pub *pub;
	struct brcms_c_info *wlc;
	char srom_ccode[BRCM_CNTRY_BUF_SZ];	
	uint srom_regrev;	
	const struct country_info *country;	
	char ccode[BRCM_CNTRY_BUF_SZ];	
	uint regrev;		
	char country_abbrev[BRCM_CNTRY_BUF_SZ];	
	
	struct brcms_cm_band bandstate[MAXBANDS];
	
	
	struct brcms_chanvec quiet_channels;
};

struct locale_info {
	u32 valid_channels;
	
	u8 radar_channels;
	
	u8 restricted_channels;
	
	s8 maxpwr[BRCMS_MAXPWR_TBL_SIZE];
	
	s8 pub_maxpwr[BAND_5G_PWR_LVLS];
	u8 flags;
};



static const struct brcms_chanvec chanvec_none = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec chanvec_all_2G = {
	{0xfe, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec chanvec_all_5G = {
	{0x00, 0x00, 0x00, 0x00, 0x54, 0x55, 0x11, 0x11,
	 0x01, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x11,
	 0x11, 0x11, 0x20, 0x22, 0x22, 0x00, 0x00, 0x11,
	 0x11, 0x11, 0x11, 0x01}
};


static const struct brcms_chanvec radar_set1 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11,  
	 0x01, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x11,  
	 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  
	 0x00, 0x00, 0x00, 0x00}
};


static const struct brcms_chanvec restricted_set_japan_legacy = {
	{0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec restricted_set_2g_short = {
	{0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec restricted_chan_165 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec restricted_low_hi = {
	{0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x01, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x20, 0x22, 0x22, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec restricted_set_12_13_14 = {
	{0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};


static const struct brcms_chanvec *g_table_radar_set[] = {
	&chanvec_none,
	&radar_set1
};

static const struct brcms_chanvec *g_table_restricted_chan[] = {
	&chanvec_none,		
	&restricted_set_2g_short,
	&restricted_chan_165,
	&chanvec_all_5G,
	&restricted_set_japan_legacy,
	&chanvec_all_2G,	
	&chanvec_all_5G,	
	&restricted_low_hi,
	&restricted_set_12_13_14
};

static const struct brcms_chanvec locale_2g_01_11 = {
	{0xfe, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_2g_12_13 = {
	{0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_2g_14 = {
	{0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_LOW_JP1 = {
	{0x00, 0x00, 0x00, 0x00, 0x54, 0x55, 0x01, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_LOW_JP2 = {
	{0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_LOW1 = {
	{0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x01, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_LOW2 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_LOW3 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
	 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_MID1 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_MID2 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_MID3 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_HIGH1 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x10, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_HIGH2 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x20, 0x22, 0x02, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_HIGH3 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_52_140_ALL = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11,
	 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	 0x11, 0x11, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_chanvec locale_5g_HIGH4 = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
	 0x11, 0x11, 0x11, 0x11}
};

static const struct brcms_chanvec *g_table_locale_base[] = {
	&locale_2g_01_11,
	&locale_2g_12_13,
	&locale_2g_14,
	&locale_5g_LOW_JP1,
	&locale_5g_LOW_JP2,
	&locale_5g_LOW1,
	&locale_5g_LOW2,
	&locale_5g_LOW3,
	&locale_5g_MID1,
	&locale_5g_MID2,
	&locale_5g_MID3,
	&locale_5g_HIGH1,
	&locale_5g_HIGH2,
	&locale_5g_HIGH3,
	&locale_5g_52_140_ALL,
	&locale_5g_HIGH4
};

static void brcms_c_locale_add_channels(struct brcms_chanvec *target,
				    const struct brcms_chanvec *channels)
{
	u8 i;
	for (i = 0; i < sizeof(struct brcms_chanvec); i++)
		target->vec[i] |= channels->vec[i];
}

static void brcms_c_locale_get_channels(const struct locale_info *locale,
				    struct brcms_chanvec *channels)
{
	u8 i;

	memset(channels, 0, sizeof(struct brcms_chanvec));

	for (i = 0; i < ARRAY_SIZE(g_table_locale_base); i++) {
		if (locale->valid_channels & (1 << i))
			brcms_c_locale_add_channels(channels,
						g_table_locale_base[i]);
	}
}

static const struct locale_info locale_i = {	
	LOCALE_CHAN_01_11 | LOCALE_CHAN_12_13,
	LOCALE_RADAR_SET_NONE,
	LOCALE_RESTRICTED_SET_2G_SHORT,
	{QDB(19), QDB(19), QDB(19),
	 QDB(19), QDB(19), QDB(19)},
	{20, 20, 20, 0},
	BRCMS_EIRP
};

static const struct locale_info locale_11 = {
	
	LOCALE_CHAN_36_64 | LOCALE_CHAN_100_140 | LOCALE_CHAN_149_165,
	LOCALE_RADAR_SET_1,
	LOCALE_RESTRICTED_NONE,
	{QDB(21), QDB(21), QDB(21), QDB(21), QDB(21)},
	{23, 23, 23, 30, 30},
	BRCMS_EIRP | BRCMS_DFS_EU
};

static const struct locale_info *g_locale_2g_table[] = {
	&locale_i
};

static const struct locale_info *g_locale_5g_table[] = {
	&locale_11
};

static const struct locale_mimo_info locale_bn = {
	{QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	 QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	 QDB(13), QDB(13), QDB(13)},
	{0, 0, QDB(13), QDB(13), QDB(13),
	 QDB(13), QDB(13), QDB(13), QDB(13), QDB(13),
	 QDB(13), 0, 0},
	0
};

static const struct locale_mimo_info *g_mimo_2g_table[] = {
	&locale_bn
};

static const struct locale_mimo_info locale_11n = {
	{  50, 50, 50, QDB(15), QDB(15)},
	{QDB(14), QDB(15), QDB(15), QDB(15), QDB(15)},
	0
};

static const struct locale_mimo_info *g_mimo_5g_table[] = {
	&locale_11n
};

static const struct {
	char abbrev[BRCM_CNTRY_BUF_SZ];	
	struct country_info country;
} cntry_locales[] = {
	{
	"X2", LOCALES(i, 11, bn, 11n)},	
};

#ifdef SUPPORT_40MHZ
struct chan20_info {
	u8 sb;
	u8 adj_sbs;
};

struct chan20_info chan20_info[] = {
	
 {1, (CH_UPPER_SB | CH_EWA_VALID)},
 {2, (CH_UPPER_SB | CH_EWA_VALID)},
 {3, (CH_UPPER_SB | CH_EWA_VALID)},
 {4, (CH_UPPER_SB | CH_EWA_VALID)},
 {5, (CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
 {6, (CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
 {7, (CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
 {8, (CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
 {9, (CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
 {10, (CH_LOWER_SB | CH_EWA_VALID)},
 {11, (CH_LOWER_SB | CH_EWA_VALID)},
 {12, (CH_LOWER_SB)},
 {13, (CH_LOWER_SB)},
 {14, (CH_LOWER_SB)},

 {34, (CH_UPPER_SB)},
 {38, (CH_LOWER_SB)},
 {42, (CH_LOWER_SB)},
 {46, (CH_LOWER_SB)},

 {36, (CH_UPPER_SB | CH_EWA_VALID)},
 {40, (CH_LOWER_SB | CH_EWA_VALID)},
 {44, (CH_UPPER_SB | CH_EWA_VALID)},
 {48, (CH_LOWER_SB | CH_EWA_VALID)},
 {52, (CH_UPPER_SB | CH_EWA_VALID)},
 {56, (CH_LOWER_SB | CH_EWA_VALID)},
 {60, (CH_UPPER_SB | CH_EWA_VALID)},
 {64, (CH_LOWER_SB | CH_EWA_VALID)},

 {100, (CH_UPPER_SB | CH_EWA_VALID)},
 {104, (CH_LOWER_SB | CH_EWA_VALID)},
 {108, (CH_UPPER_SB | CH_EWA_VALID)},
 {112, (CH_LOWER_SB | CH_EWA_VALID)},
 {116, (CH_UPPER_SB | CH_EWA_VALID)},
 {120, (CH_LOWER_SB | CH_EWA_VALID)},
 {124, (CH_UPPER_SB | CH_EWA_VALID)},
 {128, (CH_LOWER_SB | CH_EWA_VALID)},
 {132, (CH_UPPER_SB | CH_EWA_VALID)},
 {136, (CH_LOWER_SB | CH_EWA_VALID)},
 {140, (CH_LOWER_SB)},

 {149, (CH_UPPER_SB | CH_EWA_VALID)},
 {153, (CH_LOWER_SB | CH_EWA_VALID)},
 {157, (CH_UPPER_SB | CH_EWA_VALID)},
 {161, (CH_LOWER_SB | CH_EWA_VALID)},
 {165, (CH_LOWER_SB)},

 {184, (CH_UPPER_SB)},
 {188, (CH_LOWER_SB)},
 {192, (CH_UPPER_SB)},
 {196, (CH_LOWER_SB)},
 {200, (CH_UPPER_SB)},
 {204, (CH_LOWER_SB)},
 {208, (CH_UPPER_SB)},
 {212, (CH_LOWER_SB)},
 {216, (CH_LOWER_SB)}
};
#endif				

static const struct locale_info *brcms_c_get_locale_2g(u8 locale_idx)
{
	if (locale_idx >= ARRAY_SIZE(g_locale_2g_table))
		return NULL; 

	return g_locale_2g_table[locale_idx];
}

static const struct locale_info *brcms_c_get_locale_5g(u8 locale_idx)
{
	if (locale_idx >= ARRAY_SIZE(g_locale_5g_table))
		return NULL; 

	return g_locale_5g_table[locale_idx];
}

static const struct locale_mimo_info *brcms_c_get_mimo_2g(u8 locale_idx)
{
	if (locale_idx >= ARRAY_SIZE(g_mimo_2g_table))
		return NULL;

	return g_mimo_2g_table[locale_idx];
}

static const struct locale_mimo_info *brcms_c_get_mimo_5g(u8 locale_idx)
{
	if (locale_idx >= ARRAY_SIZE(g_mimo_5g_table))
		return NULL;

	return g_mimo_5g_table[locale_idx];
}

static int
brcms_c_country_aggregate_map(struct brcms_cm_info *wlc_cm, const char *ccode,
			  char *mapped_ccode, uint *mapped_regrev)
{
	return false;
}

static const struct country_info *
brcms_c_country_lookup_direct(const char *ccode, uint regrev)
{
	uint size, i;

	
	

	if (regrev > 0)
		return NULL;

	
	size = ARRAY_SIZE(cntry_locales);
	for (i = 0; i < size; i++) {
		if (strcmp(ccode, cntry_locales[i].abbrev) == 0)
			return &cntry_locales[i].country;
	}
	return NULL;
}

static const struct country_info *
brcms_c_countrycode_map(struct brcms_cm_info *wlc_cm, const char *ccode,
			char *mapped_ccode, uint *mapped_regrev)
{
	struct brcms_c_info *wlc = wlc_cm->wlc;
	const struct country_info *country;
	uint srom_regrev = wlc_cm->srom_regrev;
	const char *srom_ccode = wlc_cm->srom_ccode;
	int mapped;

	
	if (strlen(ccode) > (BRCM_CNTRY_BUF_SZ - 1)) {
		wiphy_err(wlc->wiphy, "wl%d: %s: ccode \"%s\" too long for "
			  "match\n", wlc->pub->unit, __func__, ccode);
		return NULL;
	}

	
	strncpy(mapped_ccode, ccode, BRCM_CNTRY_BUF_SZ);
	*mapped_regrev = 0;

	if (!strcmp(srom_ccode, ccode)) {
		*mapped_regrev = srom_regrev;
		mapped = 0;
		wiphy_err(wlc->wiphy, "srom_code == ccode %s\n", __func__);
	} else {
		mapped =
		    brcms_c_country_aggregate_map(wlc_cm, ccode, mapped_ccode,
					      mapped_regrev);
	}

	
	country = brcms_c_country_lookup_direct(mapped_ccode, *mapped_regrev);

	
	if (country == NULL && *mapped_regrev != 0) {
		*mapped_regrev = 0;
		country =
		    brcms_c_country_lookup_direct(mapped_ccode, *mapped_regrev);
	}

	return country;
}

static const struct country_info *
brcms_c_country_lookup(struct brcms_c_info *wlc, const char *ccode)
{
	const struct country_info *country;
	char mapped_ccode[BRCM_CNTRY_BUF_SZ];
	uint mapped_regrev;

	country = brcms_c_countrycode_map(wlc->cmi, ccode, mapped_ccode,
					  &mapped_regrev);

	return country;
}

static void brcms_c_quiet_channels_reset(struct brcms_cm_info *wlc_cm)
{
	struct brcms_c_info *wlc = wlc_cm->wlc;
	uint i, j;
	struct brcms_band *band;
	const struct brcms_chanvec *chanvec;

	memset(&wlc_cm->quiet_channels, 0, sizeof(struct brcms_chanvec));

	band = wlc->band;
	for (i = 0; i < wlc->pub->_nbands;
	     i++, band = wlc->bandstate[OTHERBANDUNIT(wlc)]) {

		
		chanvec = wlc_cm->bandstate[band->bandunit].restricted_channels;
		for (j = 0; j < sizeof(struct brcms_chanvec); j++)
			wlc_cm->quiet_channels.vec[j] |= chanvec->vec[j];

	}
}

static bool brcms_c_valid_channel20(struct brcms_cm_info *wlc_cm, uint val)
{
	struct brcms_c_info *wlc = wlc_cm->wlc;

	return ((val < MAXCHANNEL) &&
		isset(wlc_cm->bandstate[wlc->band->bandunit].valid_channels.vec,
		      val));
}

static bool brcms_c_valid_channel20_in_band(struct brcms_cm_info *wlc_cm,
					    uint bandunit, uint val)
{
	return ((val < MAXCHANNEL)
		&& isset(wlc_cm->bandstate[bandunit].valid_channels.vec, val));
}

static bool brcms_c_valid_channel20_db(struct brcms_cm_info *wlc_cm, uint val)
{
	struct brcms_c_info *wlc = wlc_cm->wlc;

	return brcms_c_valid_channel20(wlc->cmi, val) ||
		(!wlc->bandlocked
		 && brcms_c_valid_channel20_in_band(wlc->cmi,
						    OTHERBANDUNIT(wlc), val));
}

static bool brcms_c_japan_ccode(const char *ccode)
{
	return (ccode[0] == 'J' &&
		(ccode[1] == 'P' || (ccode[1] >= '1' && ccode[1] <= '9')));
}

static bool brcms_c_japan(struct brcms_c_info *wlc)
{
	return brcms_c_japan_ccode(wlc->cmi->country_abbrev);
}

static void
brcms_c_channel_min_txpower_limits_with_local_constraint(
		struct brcms_cm_info *wlc_cm, struct txpwr_limits *txpwr,
		u8 local_constraint_qdbm)
{
	int j;

	
	for (j = 0; j < WL_TX_POWER_CCK_NUM; j++)
		txpwr->cck[j] = min(txpwr->cck[j], local_constraint_qdbm);

	
	for (j = 0; j < WL_TX_POWER_OFDM_NUM; j++)
		txpwr->ofdm[j] = min(txpwr->ofdm[j], local_constraint_qdbm);

	
	for (j = 0; j < BRCMS_NUM_RATES_OFDM; j++)
		txpwr->ofdm_cdd[j] =
		    min(txpwr->ofdm_cdd[j], local_constraint_qdbm);

	
	for (j = 0; j < BRCMS_NUM_RATES_OFDM; j++)
		txpwr->ofdm_40_siso[j] =
		    min(txpwr->ofdm_40_siso[j], local_constraint_qdbm);

	
	for (j = 0; j < BRCMS_NUM_RATES_OFDM; j++)
		txpwr->ofdm_40_cdd[j] =
		    min(txpwr->ofdm_40_cdd[j], local_constraint_qdbm);

	
	for (j = 0; j < BRCMS_NUM_RATES_MCS_1_STREAM; j++)
		txpwr->mcs_20_siso[j] =
		    min(txpwr->mcs_20_siso[j], local_constraint_qdbm);

	
	for (j = 0; j < BRCMS_NUM_RATES_MCS_1_STREAM; j++)
		txpwr->mcs_20_cdd[j] =
		    min(txpwr->mcs_20_cdd[j], local_constraint_qdbm);

	
	for (j = 0; j < BRCMS_NUM_RATES_MCS_1_STREAM; j++)
		txpwr->mcs_20_stbc[j] =
		    min(txpwr->mcs_20_stbc[j], local_constraint_qdbm);

	
	for (j = 0; j < BRCMS_NUM_RATES_MCS_2_STREAM; j++)
		txpwr->mcs_20_mimo[j] =
		    min(txpwr->mcs_20_mimo[j], local_constraint_qdbm);

	
	for (j = 0; j < BRCMS_NUM_RATES_MCS_1_STREAM; j++)
		txpwr->mcs_40_siso[j] =
		    min(txpwr->mcs_40_siso[j], local_constraint_qdbm);

	
	for (j = 0; j < BRCMS_NUM_RATES_MCS_1_STREAM; j++)
		txpwr->mcs_40_cdd[j] =
		    min(txpwr->mcs_40_cdd[j], local_constraint_qdbm);

	
	for (j = 0; j < BRCMS_NUM_RATES_MCS_1_STREAM; j++)
		txpwr->mcs_40_stbc[j] =
		    min(txpwr->mcs_40_stbc[j], local_constraint_qdbm);

	
	for (j = 0; j < BRCMS_NUM_RATES_MCS_2_STREAM; j++)
		txpwr->mcs_40_mimo[j] =
		    min(txpwr->mcs_40_mimo[j], local_constraint_qdbm);

	
	txpwr->mcs32 = min(txpwr->mcs32, local_constraint_qdbm);

}

static void brcms_c_channels_commit(struct brcms_cm_info *wlc_cm)
{
	struct brcms_c_info *wlc = wlc_cm->wlc;
	uint chan;
	struct txpwr_limits txpwr;

	
	for (chan = 0; chan < MAXCHANNEL; chan++) {
		if (brcms_c_valid_channel20_db(wlc->cmi, chan))
			break;
	}
	if (chan == MAXCHANNEL)
		chan = INVCHANNEL;

	if (chan == INVCHANNEL) {
		mboolset(wlc->pub->radio_disabled, WL_RADIO_COUNTRY_DISABLE);
		wiphy_err(wlc->wiphy, "wl%d: %s: no valid channel for \"%s\" "
			  "nbands %d bandlocked %d\n", wlc->pub->unit,
			  __func__, wlc_cm->country_abbrev, wlc->pub->_nbands,
			  wlc->bandlocked);
	} else if (mboolisset(wlc->pub->radio_disabled,
			      WL_RADIO_COUNTRY_DISABLE)) {
		mboolclr(wlc->pub->radio_disabled, WL_RADIO_COUNTRY_DISABLE);
	}

	if (wlc->pub->_nbands > 1 || wlc->band->bandtype == BRCM_BAND_2G)
		wlc_phy_chanspec_ch14_widefilter_set(wlc->band->pi,
						     brcms_c_japan(wlc) ? true :
						     false);

	if (wlc->pub->up && chan != INVCHANNEL) {
		brcms_c_channel_reg_limits(wlc_cm, wlc->chanspec, &txpwr);
		brcms_c_channel_min_txpower_limits_with_local_constraint(wlc_cm,
			&txpwr, BRCMS_TXPWR_MAX);
		wlc_phy_txpower_limit_set(wlc->band->pi, &txpwr, wlc->chanspec);
	}
}

static int
brcms_c_channels_init(struct brcms_cm_info *wlc_cm,
		      const struct country_info *country)
{
	struct brcms_c_info *wlc = wlc_cm->wlc;
	uint i, j;
	struct brcms_band *band;
	const struct locale_info *li;
	struct brcms_chanvec sup_chan;
	const struct locale_mimo_info *li_mimo;

	band = wlc->band;
	for (i = 0; i < wlc->pub->_nbands;
	     i++, band = wlc->bandstate[OTHERBANDUNIT(wlc)]) {

		li = (band->bandtype == BRCM_BAND_5G) ?
		    brcms_c_get_locale_5g(country->locale_5G) :
		    brcms_c_get_locale_2g(country->locale_2G);
		wlc_cm->bandstate[band->bandunit].locale_flags = li->flags;
		li_mimo = (band->bandtype == BRCM_BAND_5G) ?
		    brcms_c_get_mimo_5g(country->locale_mimo_5G) :
		    brcms_c_get_mimo_2g(country->locale_mimo_2G);

		
		wlc_cm->bandstate[band->bandunit].locale_flags |=
		    li_mimo->flags;

		wlc_cm->bandstate[band->bandunit].restricted_channels =
		    g_table_restricted_chan[li->restricted_channels];
		wlc_cm->bandstate[band->bandunit].radar_channels =
		    g_table_radar_set[li->radar_channels];

		wlc_phy_chanspec_band_validch(band->pi, band->bandtype,
					      &sup_chan);
		brcms_c_locale_get_channels(li,
					&wlc_cm->bandstate[band->bandunit].
					valid_channels);
		for (j = 0; j < sizeof(struct brcms_chanvec); j++)
			wlc_cm->bandstate[band->bandunit].valid_channels.
			    vec[j] &= sup_chan.vec[j];
	}

	brcms_c_quiet_channels_reset(wlc_cm);
	brcms_c_channels_commit(wlc_cm);

	return 0;
}

static void
brcms_c_set_country_common(struct brcms_cm_info *wlc_cm,
		       const char *country_abbrev,
		       const char *ccode, uint regrev,
		       const struct country_info *country)
{
	const struct locale_info *locale;
	struct brcms_c_info *wlc = wlc_cm->wlc;
	char prev_country_abbrev[BRCM_CNTRY_BUF_SZ];

	
	wlc_cm->country = country;

	memset(&prev_country_abbrev, 0, BRCM_CNTRY_BUF_SZ);
	strncpy(prev_country_abbrev, wlc_cm->country_abbrev,
		BRCM_CNTRY_BUF_SZ - 1);

	strncpy(wlc_cm->country_abbrev, country_abbrev, BRCM_CNTRY_BUF_SZ - 1);
	strncpy(wlc_cm->ccode, ccode, BRCM_CNTRY_BUF_SZ - 1);
	wlc_cm->regrev = regrev;

	if ((wlc->pub->_n_enab & SUPPORT_11N) !=
	    wlc->protection->nmode_user)
		brcms_c_set_nmode(wlc);

	brcms_c_stf_ss_update(wlc, wlc->bandstate[BAND_2G_INDEX]);
	brcms_c_stf_ss_update(wlc, wlc->bandstate[BAND_5G_INDEX]);
	
	locale = brcms_c_get_locale_2g(country->locale_2G);
	if (locale && (locale->flags & BRCMS_NO_OFDM))
		brcms_c_set_gmode(wlc, GMODE_LEGACY_B, false);
	else
		brcms_c_set_gmode(wlc, wlc->protection->gmode_user, false);

	brcms_c_channels_init(wlc_cm, country);

	return;
}

static int
brcms_c_set_countrycode_rev(struct brcms_cm_info *wlc_cm,
			const char *country_abbrev,
			const char *ccode, int regrev)
{
	const struct country_info *country;
	char mapped_ccode[BRCM_CNTRY_BUF_SZ];
	uint mapped_regrev;

	if (regrev == -1) {
		country =
		    brcms_c_countrycode_map(wlc_cm, ccode, mapped_ccode,
					&mapped_regrev);
	} else {
		
		country = brcms_c_country_lookup_direct(ccode, regrev);
		strncpy(mapped_ccode, ccode, BRCM_CNTRY_BUF_SZ);
		mapped_regrev = regrev;
	}

	if (country == NULL)
		return -EINVAL;

	
	brcms_c_set_country_common(wlc_cm, country_abbrev, mapped_ccode,
			       mapped_regrev, country);

	return 0;
}

static int
brcms_c_set_countrycode(struct brcms_cm_info *wlc_cm, const char *ccode)
{
	char country_abbrev[BRCM_CNTRY_BUF_SZ];
	strncpy(country_abbrev, ccode, BRCM_CNTRY_BUF_SZ);
	return brcms_c_set_countrycode_rev(wlc_cm, country_abbrev, ccode, -1);
}

struct brcms_cm_info *brcms_c_channel_mgr_attach(struct brcms_c_info *wlc)
{
	struct brcms_cm_info *wlc_cm;
	char country_abbrev[BRCM_CNTRY_BUF_SZ];
	const struct country_info *country;
	struct brcms_pub *pub = wlc->pub;
	char *ccode;

	BCMMSG(wlc->wiphy, "wl%d\n", wlc->pub->unit);

	wlc_cm = kzalloc(sizeof(struct brcms_cm_info), GFP_ATOMIC);
	if (wlc_cm == NULL)
		return NULL;
	wlc_cm->pub = pub;
	wlc_cm->wlc = wlc;
	wlc->cmi = wlc_cm;

	
	ccode = getvar(wlc->hw->sih, BRCMS_SROM_CCODE);
	if (ccode)
		strncpy(wlc->pub->srom_ccode, ccode, BRCM_CNTRY_BUF_SZ - 1);

	memset(country_abbrev, 0, BRCM_CNTRY_BUF_SZ);
	strncpy(country_abbrev, "X2", sizeof(country_abbrev) - 1);
	country = brcms_c_country_lookup(wlc, country_abbrev);

	
	strncpy(wlc->country_default, country_abbrev, BRCM_CNTRY_BUF_SZ - 1);

	
	strncpy(wlc->autocountry_default, "X2", BRCM_CNTRY_BUF_SZ - 1);

	brcms_c_set_countrycode(wlc_cm, country_abbrev);

	return wlc_cm;
}

void brcms_c_channel_mgr_detach(struct brcms_cm_info *wlc_cm)
{
	kfree(wlc_cm);
}

u8
brcms_c_channel_locale_flags_in_band(struct brcms_cm_info *wlc_cm,
				     uint bandunit)
{
	return wlc_cm->bandstate[bandunit].locale_flags;
}

static bool
brcms_c_quiet_chanspec(struct brcms_cm_info *wlc_cm, u16 chspec)
{
	return (wlc_cm->wlc->pub->_n_enab & SUPPORT_11N) &&
		CHSPEC_IS40(chspec) ?
		(isset(wlc_cm->quiet_channels.vec,
		       lower_20_sb(CHSPEC_CHANNEL(chspec))) ||
		 isset(wlc_cm->quiet_channels.vec,
		       upper_20_sb(CHSPEC_CHANNEL(chspec)))) :
		isset(wlc_cm->quiet_channels.vec, CHSPEC_CHANNEL(chspec));
}

void
brcms_c_channel_set_chanspec(struct brcms_cm_info *wlc_cm, u16 chanspec,
			 u8 local_constraint_qdbm)
{
	struct brcms_c_info *wlc = wlc_cm->wlc;
	struct txpwr_limits txpwr;

	brcms_c_channel_reg_limits(wlc_cm, chanspec, &txpwr);

	brcms_c_channel_min_txpower_limits_with_local_constraint(
		wlc_cm, &txpwr, local_constraint_qdbm
	);

	brcms_b_set_chanspec(wlc->hw, chanspec,
			      (brcms_c_quiet_chanspec(wlc_cm, chanspec) != 0),
			      &txpwr);
}

void
brcms_c_channel_reg_limits(struct brcms_cm_info *wlc_cm, u16 chanspec,
		       struct txpwr_limits *txpwr)
{
	struct brcms_c_info *wlc = wlc_cm->wlc;
	uint i;
	uint chan;
	int maxpwr;
	int delta;
	const struct country_info *country;
	struct brcms_band *band;
	const struct locale_info *li;
	int conducted_max = BRCMS_TXPWR_MAX;
	int conducted_ofdm_max = BRCMS_TXPWR_MAX;
	const struct locale_mimo_info *li_mimo;
	int maxpwr20, maxpwr40;
	int maxpwr_idx;
	uint j;

	memset(txpwr, 0, sizeof(struct txpwr_limits));

	if (!brcms_c_valid_chanspec_db(wlc_cm, chanspec)) {
		country = brcms_c_country_lookup(wlc, wlc->autocountry_default);
		if (country == NULL)
			return;
	} else {
		country = wlc_cm->country;
	}

	chan = CHSPEC_CHANNEL(chanspec);
	band = wlc->bandstate[chspec_bandunit(chanspec)];
	li = (band->bandtype == BRCM_BAND_5G) ?
	    brcms_c_get_locale_5g(country->locale_5G) :
	    brcms_c_get_locale_2g(country->locale_2G);

	li_mimo = (band->bandtype == BRCM_BAND_5G) ?
	    brcms_c_get_mimo_5g(country->locale_mimo_5G) :
	    brcms_c_get_mimo_2g(country->locale_mimo_2G);

	if (li->flags & BRCMS_EIRP) {
		delta = band->antgain;
	} else {
		delta = 0;
		if (band->antgain > QDB(6))
			delta = band->antgain - QDB(6);	
	}

	if (li == &locale_i) {
		conducted_max = QDB(22);
		conducted_ofdm_max = QDB(22);
	}

	
	if (band->bandtype == BRCM_BAND_2G) {
		maxpwr = li->maxpwr[CHANNEL_POWER_IDX_2G_CCK(chan)];

		maxpwr = maxpwr - delta;
		maxpwr = max(maxpwr, 0);
		maxpwr = min(maxpwr, conducted_max);

		for (i = 0; i < BRCMS_NUM_RATES_CCK; i++)
			txpwr->cck[i] = (u8) maxpwr;
	}

	
	if (band->bandtype == BRCM_BAND_2G)
		maxpwr = li->maxpwr[CHANNEL_POWER_IDX_2G_OFDM(chan)];
	else
		maxpwr = li->maxpwr[CHANNEL_POWER_IDX_5G(chan)];

	maxpwr = maxpwr - delta;
	maxpwr = max(maxpwr, 0);
	maxpwr = min(maxpwr, conducted_ofdm_max);

	
	if (band->bandtype == BRCM_BAND_2G)
		maxpwr = min_t(int, maxpwr, txpwr->cck[0]);

	for (i = 0; i < BRCMS_NUM_RATES_OFDM; i++)
		txpwr->ofdm[i] = (u8) maxpwr;

	for (i = 0; i < BRCMS_NUM_RATES_OFDM; i++) {
		txpwr->ofdm_40_siso[i] = 0;

		txpwr->ofdm_cdd[i] = (u8) maxpwr;

		txpwr->ofdm_40_cdd[i] = 0;
	}

	
	if (li_mimo->flags & BRCMS_EIRP) {
		delta = band->antgain;
	} else {
		delta = 0;
		if (band->antgain > QDB(6))
			delta = band->antgain - QDB(6);	
	}

	if (band->bandtype == BRCM_BAND_2G)
		maxpwr_idx = (chan - 1);
	else
		maxpwr_idx = CHANNEL_POWER_IDX_5G(chan);

	maxpwr20 = li_mimo->maxpwr20[maxpwr_idx];
	maxpwr40 = li_mimo->maxpwr40[maxpwr_idx];

	maxpwr20 = maxpwr20 - delta;
	maxpwr20 = max(maxpwr20, 0);
	maxpwr40 = maxpwr40 - delta;
	maxpwr40 = max(maxpwr40, 0);

	
	for (i = 0; i < BRCMS_NUM_RATES_MCS_1_STREAM; i++) {

		txpwr->mcs_20_siso[i] = txpwr->ofdm[i];
		txpwr->mcs_40_siso[i] = 0;
	}

	
	for (i = 0; i < BRCMS_NUM_RATES_MCS_1_STREAM; i++) {
		txpwr->mcs_20_cdd[i] = (u8) maxpwr20;
		txpwr->mcs_40_cdd[i] = (u8) maxpwr40;
	}

	if (li_mimo == &locale_bn) {
		if (li_mimo == &locale_bn) {
			maxpwr20 = QDB(16);
			maxpwr40 = 0;

			if (chan >= 3 && chan <= 11)
				maxpwr40 = QDB(16);
		}

		for (i = 0; i < BRCMS_NUM_RATES_MCS_1_STREAM; i++) {
			txpwr->mcs_20_siso[i] = (u8) maxpwr20;
			txpwr->mcs_40_siso[i] = (u8) maxpwr40;
		}
	}

	
	for (i = 0; i < BRCMS_NUM_RATES_MCS_1_STREAM; i++) {
		txpwr->mcs_20_stbc[i] = 0;
		txpwr->mcs_40_stbc[i] = 0;
	}

	
	for (i = 0; i < BRCMS_NUM_RATES_MCS_2_STREAM; i++) {
		txpwr->mcs_20_mimo[i] = (u8) maxpwr20;
		txpwr->mcs_40_mimo[i] = (u8) maxpwr40;
	}

	
	txpwr->mcs32 = (u8) maxpwr40;

	for (i = 0, j = 0; i < BRCMS_NUM_RATES_OFDM; i++, j++) {
		if (txpwr->ofdm_40_cdd[i] == 0)
			txpwr->ofdm_40_cdd[i] = txpwr->mcs_40_cdd[j];
		if (i == 0) {
			i = i + 1;
			if (txpwr->ofdm_40_cdd[i] == 0)
				txpwr->ofdm_40_cdd[i] = txpwr->mcs_40_cdd[j];
		}
	}

	for (i = 0; i < BRCMS_NUM_RATES_MCS_1_STREAM; i++) {
		if (txpwr->mcs_40_siso[i] == 0)
			txpwr->mcs_40_siso[i] = txpwr->mcs_40_cdd[i];
	}

	for (i = 0, j = 0; i < BRCMS_NUM_RATES_OFDM; i++, j++) {
		if (txpwr->ofdm_40_siso[i] == 0)
			txpwr->ofdm_40_siso[i] = txpwr->mcs_40_siso[j];
		if (i == 0) {
			i = i + 1;
			if (txpwr->ofdm_40_siso[i] == 0)
				txpwr->ofdm_40_siso[i] = txpwr->mcs_40_siso[j];
		}
	}

	for (i = 0; i < BRCMS_NUM_RATES_MCS_1_STREAM; i++) {
		if (txpwr->mcs_20_stbc[i] == 0)
			txpwr->mcs_20_stbc[i] = txpwr->mcs_20_cdd[i];

		if (txpwr->mcs_40_stbc[i] == 0)
			txpwr->mcs_40_stbc[i] = txpwr->mcs_40_cdd[i];
	}

	return;
}

static bool brcms_c_chspec_malformed(u16 chanspec)
{
	
	if (!CHSPEC_IS5G(chanspec) && !CHSPEC_IS2G(chanspec))
		return true;
	
	if (!CHSPEC_IS40(chanspec) && !CHSPEC_IS20(chanspec))
		return true;

	
	if (CHSPEC_IS20(chanspec)) {
		if (!CHSPEC_SB_NONE(chanspec))
			return true;
	} else if (!CHSPEC_SB_UPPER(chanspec) && !CHSPEC_SB_LOWER(chanspec)) {
		return true;
	}

	return false;
}

static bool
brcms_c_valid_chanspec_ext(struct brcms_cm_info *wlc_cm, u16 chspec,
			   bool dualband)
{
	struct brcms_c_info *wlc = wlc_cm->wlc;
	u8 channel = CHSPEC_CHANNEL(chspec);

	
	if (brcms_c_chspec_malformed(chspec)) {
		wiphy_err(wlc->wiphy, "wl%d: malformed chanspec 0x%x\n",
			wlc->pub->unit, chspec);
		return false;
	}

	if (CHANNEL_BANDUNIT(wlc_cm->wlc, channel) !=
	    chspec_bandunit(chspec))
		return false;

	
	if (CHSPEC_IS20(chspec)) {
		if (dualband)
			return brcms_c_valid_channel20_db(wlc_cm->wlc->cmi,
							  channel);
		else
			return brcms_c_valid_channel20(wlc_cm->wlc->cmi,
						       channel);
	}
#ifdef SUPPORT_40MHZ
	if (BRCMS_ISNPHY(wlc->band) || BRCMS_ISSSLPNPHY(wlc->band)) {
		u8 upper_sideband = 0, idx;
		u8 num_ch20_entries =
		    sizeof(chan20_info) / sizeof(struct chan20_info);

		if (!VALID_40CHANSPEC_IN_BAND(wlc, chspec_bandunit(chspec)))
			return false;

		if (dualband) {
			if (!brcms_c_valid_channel20_db(wlc->cmi,
							lower_20_sb(channel)) ||
			    !brcms_c_valid_channel20_db(wlc->cmi,
							upper_20_sb(channel)))
				return false;
		} else {
			if (!brcms_c_valid_channel20(wlc->cmi,
						     lower_20_sb(channel)) ||
			    !brcms_c_valid_channel20(wlc->cmi,
						     upper_20_sb(channel)))
				return false;
		}

		
		for (idx = 0; idx < num_ch20_entries; idx++) {
			if (chan20_info[idx].sb == lower_20_sb(channel))
				upper_sideband = chan20_info[idx].adj_sbs;
		}
		
		if ((upper_sideband & (CH_UPPER_SB | CH_EWA_VALID)) ==
		    (CH_UPPER_SB | CH_EWA_VALID))
			return true;
		return false;
	}
#endif				

	return false;
}

bool brcms_c_valid_chanspec_db(struct brcms_cm_info *wlc_cm, u16 chspec)
{
	return brcms_c_valid_chanspec_ext(wlc_cm, chspec, true);
}
