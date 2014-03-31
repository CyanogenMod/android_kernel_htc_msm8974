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

#include <brcmu_wifi.h>
#include <brcmu_utils.h>

#include "d11.h"
#include "pub.h"
#include "rate.h"

const u8 rate_info[BRCM_MAXRATE + 1] = {
	
 0x00, 0x00, 0x0a, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x37, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8f, 0x00,
 0x00, 0x00, 0x6e, 0x00, 0x8a, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8e, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x8d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8c
};

const struct brcms_mcs_info mcs_table[MCS_TABLE_SIZE] = {
	
	{6500, 13500, CEIL(6500 * 10, 9), CEIL(13500 * 10, 9), 0x00,
	 BRCM_RATE_6M},
	
	{13000, 27000, CEIL(13000 * 10, 9), CEIL(27000 * 10, 9), 0x08,
	 BRCM_RATE_12M},
	
	{19500, 40500, CEIL(19500 * 10, 9), CEIL(40500 * 10, 9), 0x0A,
	 BRCM_RATE_18M},
	
	{26000, 54000, CEIL(26000 * 10, 9), CEIL(54000 * 10, 9), 0x10,
	 BRCM_RATE_24M},
	
	{39000, 81000, CEIL(39000 * 10, 9), CEIL(81000 * 10, 9), 0x12,
	 BRCM_RATE_36M},
	
	{52000, 108000, CEIL(52000 * 10, 9), CEIL(108000 * 10, 9), 0x19,
	 BRCM_RATE_48M},
	
	{58500, 121500, CEIL(58500 * 10, 9), CEIL(121500 * 10, 9), 0x1A,
	 BRCM_RATE_54M},
	
	{65000, 135000, CEIL(65000 * 10, 9), CEIL(135000 * 10, 9), 0x1C,
	 BRCM_RATE_54M},
	
	{13000, 27000, CEIL(13000 * 10, 9), CEIL(27000 * 10, 9), 0x40,
	 BRCM_RATE_6M},
	
	{26000, 54000, CEIL(26000 * 10, 9), CEIL(54000 * 10, 9), 0x48,
	 BRCM_RATE_12M},
	
	{39000, 81000, CEIL(39000 * 10, 9), CEIL(81000 * 10, 9), 0x4A,
	 BRCM_RATE_18M},
	
	{52000, 108000, CEIL(52000 * 10, 9), CEIL(108000 * 10, 9), 0x50,
	 BRCM_RATE_24M},
	
	{78000, 162000, CEIL(78000 * 10, 9), CEIL(162000 * 10, 9), 0x52,
	 BRCM_RATE_36M},
	
	{104000, 216000, CEIL(104000 * 10, 9), CEIL(216000 * 10, 9), 0x59,
	 BRCM_RATE_48M},
	
	{117000, 243000, CEIL(117000 * 10, 9), CEIL(243000 * 10, 9), 0x5A,
	 BRCM_RATE_54M},
	
	{130000, 270000, CEIL(130000 * 10, 9), CEIL(270000 * 10, 9), 0x5C,
	 BRCM_RATE_54M},
	
	{19500, 40500, CEIL(19500 * 10, 9), CEIL(40500 * 10, 9), 0x80,
	 BRCM_RATE_6M},
	
	{39000, 81000, CEIL(39000 * 10, 9), CEIL(81000 * 10, 9), 0x88,
	 BRCM_RATE_12M},
	
	{58500, 121500, CEIL(58500 * 10, 9), CEIL(121500 * 10, 9), 0x8A,
	 BRCM_RATE_18M},
	
	{78000, 162000, CEIL(78000 * 10, 9), CEIL(162000 * 10, 9), 0x90,
	 BRCM_RATE_24M},
	
	{117000, 243000, CEIL(117000 * 10, 9), CEIL(243000 * 10, 9), 0x92,
	 BRCM_RATE_36M},
	
	{156000, 324000, CEIL(156000 * 10, 9), CEIL(324000 * 10, 9), 0x99,
	 BRCM_RATE_48M},
	
	{175500, 364500, CEIL(175500 * 10, 9), CEIL(364500 * 10, 9), 0x9A,
	 BRCM_RATE_54M},
	
	{195000, 405000, CEIL(195000 * 10, 9), CEIL(405000 * 10, 9), 0x9B,
	 BRCM_RATE_54M},
	
	{26000, 54000, CEIL(26000 * 10, 9), CEIL(54000 * 10, 9), 0xC0,
	 BRCM_RATE_6M},
	
	{52000, 108000, CEIL(52000 * 10, 9), CEIL(108000 * 10, 9), 0xC8,
	 BRCM_RATE_12M},
	
	{78000, 162000, CEIL(78000 * 10, 9), CEIL(162000 * 10, 9), 0xCA,
	 BRCM_RATE_18M},
	
	{104000, 216000, CEIL(104000 * 10, 9), CEIL(216000 * 10, 9), 0xD0,
	 BRCM_RATE_24M},
	
	{156000, 324000, CEIL(156000 * 10, 9), CEIL(324000 * 10, 9), 0xD2,
	 BRCM_RATE_36M},
	
	{208000, 432000, CEIL(208000 * 10, 9), CEIL(432000 * 10, 9), 0xD9,
	 BRCM_RATE_48M},
	
	{234000, 486000, CEIL(234000 * 10, 9), CEIL(486000 * 10, 9), 0xDA,
	 BRCM_RATE_54M},
	
	{260000, 540000, CEIL(260000 * 10, 9), CEIL(540000 * 10, 9), 0xDB,
	 BRCM_RATE_54M},
	
	{0, 6000, 0, CEIL(6000 * 10, 9), 0x00, BRCM_RATE_6M},
};

struct legacy_phycfg {
	u32 rate_ofdm;	
	
	u8 tx_phy_ctl3;
};

#define LEGACY_PHYCFG_TABLE_SIZE	12

static const struct
legacy_phycfg legacy_phycfg_table[LEGACY_PHYCFG_TABLE_SIZE] = {
	{BRCM_RATE_1M, 0x00},	
	{BRCM_RATE_2M, 0x08},	
	{BRCM_RATE_5M5, 0x10},	
	{BRCM_RATE_11M, 0x18},	
	
	{BRCM_RATE_6M, 0x00},
	
	{BRCM_RATE_9M, 0x02},
	
	{BRCM_RATE_12M, 0x08},
	
	{BRCM_RATE_18M, 0x0A},
	
	{BRCM_RATE_24M, 0x10},
	
	{BRCM_RATE_36M, 0x12},
	
	{BRCM_RATE_48M, 0x19},
	
	{BRCM_RATE_54M, 0x1A},
};


const struct brcms_c_rateset cck_ofdm_mimo_rates = {
	12,
	
	{ 0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60,
	
	  0x6c},
	0x00,
	{ 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00}
};

const struct brcms_c_rateset ofdm_mimo_rates = {
	8,
	
	{ 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c},
	0x00,
	{ 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_c_rateset cck_ofdm_40bw_mimo_rates = {
	12,
	
	{ 0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60,
	
	  0x6c},
	0x00,
	{ 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00}
};

static const struct brcms_c_rateset ofdm_40bw_mimo_rates = {
	8,
	
	{ 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c},
	0x00,
	{ 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00}
};

const struct brcms_c_rateset cck_ofdm_rates = {
	12,
	
	{ 0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60,
	
	  0x6c},
	0x00,
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00}
};

const struct brcms_c_rateset gphy_legacy_rates = {
	4,
	
	{ 0x82, 0x84, 0x8b, 0x96},
	0x00,
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00}
};

const struct brcms_c_rateset ofdm_rates = {
	8,
	
	{ 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c},
	0x00,
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00}
};

const struct brcms_c_rateset cck_rates = {
	4,
	
	{ 0x82, 0x84, 0x0b, 0x16},
	0x00,
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00}
};

static bool brcms_c_rateset_valid(struct brcms_c_rateset *rs, bool check_brate)
{
	uint idx;

	if (!rs->count)
		return false;

	if (!check_brate)
		return true;

	
	for (idx = 0; idx < rs->count; idx++) {
		if (rs->rates[idx] & BRCMS_RATE_FLAG)
			return true;
	}
	return false;
}

void brcms_c_rateset_mcs_upd(struct brcms_c_rateset *rs, u8 txstreams)
{
	int i;
	for (i = txstreams; i < MAX_STREAMS_SUPPORTED; i++)
		rs->mcs[i] = 0;
}

bool
brcms_c_rate_hwrs_filter_sort_validate(struct brcms_c_rateset *rs,
				   const struct brcms_c_rateset *hw_rs,
				   bool check_brate, u8 txstreams)
{
	u8 rateset[BRCM_MAXRATE + 1];
	u8 r;
	uint count;
	uint i;

	memset(rateset, 0, sizeof(rateset));
	count = rs->count;

	for (i = 0; i < count; i++) {
		
		r = (int)rs->rates[i] & BRCMS_RATE_MASK;
		if ((r > BRCM_MAXRATE) || (rate_info[r] == 0))
			continue;
		rateset[r] = rs->rates[i];	
	}

	
	count = 0;
	for (i = 0; i < hw_rs->count; i++) {
		r = hw_rs->rates[i] & BRCMS_RATE_MASK;
		if (rateset[r])
			rs->rates[count++] = rateset[r];
	}

	rs->count = count;

	
	for (i = 0; i < MCSSET_LEN; i++)
		rs->mcs[i] = (rs->mcs[i] & hw_rs->mcs[i]);

	if (brcms_c_rateset_valid(rs, check_brate))
		return true;
	else
		return false;
}

u32 brcms_c_compute_rspec(struct d11rxhdr *rxh, u8 *plcp)
{
	int phy_type;
	u32 rspec = PHY_TXC1_BW_20MHZ << RSPEC_BW_SHIFT;

	phy_type =
	    ((rxh->RxChan & RXS_CHAN_PHYTYPE_MASK) >> RXS_CHAN_PHYTYPE_SHIFT);

	if ((phy_type == PHY_TYPE_N) || (phy_type == PHY_TYPE_SSN) ||
	    (phy_type == PHY_TYPE_LCN) || (phy_type == PHY_TYPE_HT)) {
		switch (rxh->PhyRxStatus_0 & PRXS0_FT_MASK) {
		case PRXS0_CCK:
			rspec =
				cck_phy2mac_rate(
				((struct cck_phy_hdr *) plcp)->signal);
			break;
		case PRXS0_OFDM:
			rspec =
			    ofdm_phy2mac_rate(
				((struct ofdm_phy_hdr *) plcp)->rlpt[0]);
			break;
		case PRXS0_PREN:
			rspec = (plcp[0] & MIMO_PLCP_MCS_MASK) | RSPEC_MIMORATE;
			if (plcp[0] & MIMO_PLCP_40MHZ) {
				
				rspec &= ~RSPEC_BW_MASK;
				rspec |= (PHY_TXC1_BW_40MHZ << RSPEC_BW_SHIFT);
			}
			break;
		case PRXS0_STDN:
			
		default:
			
			break;
		}
		if (plcp3_issgi(plcp[3]))
			rspec |= RSPEC_SHORT_GI;
	} else
	    if ((phy_type == PHY_TYPE_A) || (rxh->PhyRxStatus_0 & PRXS0_OFDM))
		rspec = ofdm_phy2mac_rate(
				((struct ofdm_phy_hdr *) plcp)->rlpt[0]);
	else
		rspec = cck_phy2mac_rate(
				((struct cck_phy_hdr *) plcp)->signal);

	return rspec;
}

void brcms_c_rateset_copy(const struct brcms_c_rateset *src,
			  struct brcms_c_rateset *dst)
{
	memcpy(dst, src, sizeof(struct brcms_c_rateset));
}

void
brcms_c_rateset_filter(struct brcms_c_rateset *src, struct brcms_c_rateset *dst,
		       bool basic_only, u8 rates, uint xmask, bool mcsallow)
{
	uint i;
	uint r;
	uint count;

	count = 0;
	for (i = 0; i < src->count; i++) {
		r = src->rates[i];
		if (basic_only && !(r & BRCMS_RATE_FLAG))
			continue;
		if (rates == BRCMS_RATES_CCK &&
		    is_ofdm_rate((r & BRCMS_RATE_MASK)))
			continue;
		if (rates == BRCMS_RATES_OFDM &&
		    is_cck_rate((r & BRCMS_RATE_MASK)))
			continue;
		dst->rates[count++] = r & xmask;
	}
	dst->count = count;
	dst->htphy_membership = src->htphy_membership;

	if (mcsallow && rates != BRCMS_RATES_CCK)
		memcpy(&dst->mcs[0], &src->mcs[0], MCSSET_LEN);
	else
		brcms_c_rateset_mcs_clear(dst);
}

void
brcms_c_rateset_default(struct brcms_c_rateset *rs_tgt,
			const struct brcms_c_rateset *rs_hw,
			uint phy_type, int bandtype, bool cck_only,
			uint rate_mask, bool mcsallow, u8 bw, u8 txstreams)
{
	const struct brcms_c_rateset *rs_dflt;
	struct brcms_c_rateset rs_sel;
	if ((PHYTYPE_IS(phy_type, PHY_TYPE_HT)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_N)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_LCN)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_SSN))) {
		if (bandtype == BRCM_BAND_5G)
			rs_dflt = (bw == BRCMS_20_MHZ ?
				   &ofdm_mimo_rates : &ofdm_40bw_mimo_rates);
		else
			rs_dflt = (bw == BRCMS_20_MHZ ?
				   &cck_ofdm_mimo_rates :
				   &cck_ofdm_40bw_mimo_rates);
	} else if (PHYTYPE_IS(phy_type, PHY_TYPE_LP)) {
		rs_dflt = (bandtype == BRCM_BAND_5G) ?
			  &ofdm_rates : &cck_ofdm_rates;
	} else if (PHYTYPE_IS(phy_type, PHY_TYPE_A)) {
		rs_dflt = &ofdm_rates;
	} else if (PHYTYPE_IS(phy_type, PHY_TYPE_G)) {
		rs_dflt = &cck_ofdm_rates;
	} else {
		
		rs_dflt = &cck_rates;	
	}

	
	if (!rs_hw)
		rs_hw = rs_dflt;

	brcms_c_rateset_copy(rs_dflt, &rs_sel);
	brcms_c_rateset_mcs_upd(&rs_sel, txstreams);
	brcms_c_rateset_filter(&rs_sel, rs_tgt, false,
			   cck_only ? BRCMS_RATES_CCK : BRCMS_RATES_CCK_OFDM,
			   rate_mask, mcsallow);
	brcms_c_rate_hwrs_filter_sort_validate(rs_tgt, rs_hw, false,
					   mcsallow ? txstreams : 1);
}

s16 brcms_c_rate_legacy_phyctl(uint rate)
{
	uint i;
	for (i = 0; i < LEGACY_PHYCFG_TABLE_SIZE; i++)
		if (rate == legacy_phycfg_table[i].rate_ofdm)
			return legacy_phycfg_table[i].tx_phy_ctl3;

	return -1;
}

void brcms_c_rateset_mcs_clear(struct brcms_c_rateset *rateset)
{
	uint i;
	for (i = 0; i < MCSSET_LEN; i++)
		rateset->mcs[i] = 0;
}

void brcms_c_rateset_mcs_build(struct brcms_c_rateset *rateset, u8 txstreams)
{
	memcpy(&rateset->mcs[0], &cck_ofdm_mimo_rates.mcs[0], MCSSET_LEN);
	brcms_c_rateset_mcs_upd(rateset, txstreams);
}

void brcms_c_rateset_bw_mcs_filter(struct brcms_c_rateset *rateset, u8 bw)
{
	if (bw == BRCMS_40_MHZ)
		setbit(rateset->mcs, 32);
	else
		clrbit(rateset->mcs, 32);
}
