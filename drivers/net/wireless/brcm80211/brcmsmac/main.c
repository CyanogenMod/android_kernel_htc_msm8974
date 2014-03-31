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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/pci_ids.h>
#include <linux/if_ether.h>
#include <net/mac80211.h>
#include <brcm_hw_ids.h>
#include <aiutils.h>
#include <chipcommon.h>
#include "rate.h"
#include "scb.h"
#include "phy/phy_hal.h"
#include "channel.h"
#include "antsel.h"
#include "stf.h"
#include "ampdu.h"
#include "mac80211_if.h"
#include "ucode_loader.h"
#include "main.h"
#include "soc.h"

#define ALLPRIO				-1

#define TIMER_INTERVAL_WATCHDOG		1000
#define TIMER_INTERVAL_RADIOCHK		800

#define BEACON_INTERVAL_DEFAULT		100

#define WL_11N_2x2			1
#define WL_11N_3x3			3
#define WL_11N_4x4			4

#define EDCF_ACI_MASK			0x60
#define EDCF_ACI_SHIFT			5
#define EDCF_ECWMIN_MASK		0x0f
#define EDCF_ECWMAX_SHIFT		4
#define EDCF_AIFSN_MASK			0x0f
#define EDCF_AIFSN_MAX			15
#define EDCF_ECWMAX_MASK		0xf0

#define EDCF_AC_BE_TXOP_STA		0x0000
#define EDCF_AC_BK_TXOP_STA		0x0000
#define EDCF_AC_VO_ACI_STA		0x62
#define EDCF_AC_VO_ECW_STA		0x32
#define EDCF_AC_VI_ACI_STA		0x42
#define EDCF_AC_VI_ECW_STA		0x43
#define EDCF_AC_BK_ECW_STA		0xA4
#define EDCF_AC_VI_TXOP_STA		0x005e
#define EDCF_AC_VO_TXOP_STA		0x002f
#define EDCF_AC_BE_ACI_STA		0x03
#define EDCF_AC_BE_ECW_STA		0xA4
#define EDCF_AC_BK_ACI_STA		0x27
#define EDCF_AC_VO_TXOP_AP		0x002f

#define EDCF_TXOP2USEC(txop)		((txop) << 5)
#define EDCF_ECW2CW(exp)		((1 << (exp)) - 1)

#define APHY_SYMBOL_TIME		4
#define APHY_PREAMBLE_TIME		16
#define APHY_SIGNAL_TIME		4
#define APHY_SIFS_TIME			16
#define APHY_SERVICE_NBITS		16
#define APHY_TAIL_NBITS			6
#define BPHY_SIFS_TIME			10
#define BPHY_PLCP_SHORT_TIME		96

#define PREN_PREAMBLE			24
#define PREN_MM_EXT			12
#define PREN_PREAMBLE_EXT		4

#define DOT11_MAC_HDR_LEN		24
#define DOT11_ACK_LEN			10
#define DOT11_BA_LEN			4
#define DOT11_OFDM_SIGNAL_EXTENSION	6
#define DOT11_MIN_FRAG_LEN		256
#define DOT11_RTS_LEN			16
#define DOT11_CTS_LEN			10
#define DOT11_BA_BITMAP_LEN		128
#define DOT11_MIN_BEACON_PERIOD		1
#define DOT11_MAX_BEACON_PERIOD		0xFFFF
#define DOT11_MAXNUMFRAGS		16
#define DOT11_MAX_FRAG_LEN		2346

#define BPHY_PLCP_TIME			192
#define RIFS_11N_TIME			2

#define BCN_TMPL_LEN			512

#define BRCMS_BSS_HT			0x0020	

#define BRCMS_HWRXOFF			38

#define RFDISABLE_DEFAULT		10000000

#define BRCMS_TEMPSENSE_PERIOD		10	

#define _BRCMS_PREC_NONE		0	
#define _BRCMS_PREC_BK			2	
#define _BRCMS_PREC_BE			4	
#define _BRCMS_PREC_EE			6	
#define _BRCMS_PREC_CL			8	
#define _BRCMS_PREC_VI			10	
#define _BRCMS_PREC_VO			12	
#define _BRCMS_PREC_NC			14	

#define SYNTHPU_DLY_APHY_US		3700
#define SYNTHPU_DLY_BPHY_US		1050
#define SYNTHPU_DLY_NPHY_US		2048
#define SYNTHPU_DLY_LPPHY_US		300

#define ANTCNT				10	

#define EDCF_SHORT_S			0
#define EDCF_SFB_S			4
#define EDCF_LONG_S			8
#define EDCF_LFB_S			12
#define EDCF_SHORT_M			BITFIELD_MASK(4)
#define EDCF_SFB_M			BITFIELD_MASK(4)
#define EDCF_LONG_M			BITFIELD_MASK(4)
#define EDCF_LFB_M			BITFIELD_MASK(4)

#define RETRY_SHORT_DEF			7	
#define RETRY_SHORT_MAX			255	
#define RETRY_LONG_DEF			4	
#define RETRY_SHORT_FB			3	
#define RETRY_LONG_FB			2	

#define APHY_CWMIN			15
#define PHY_CWMAX			1023

#define EDCF_AIFSN_MIN			1

#define FRAGNUM_MASK			0xF

#define APHY_SLOT_TIME			9
#define BPHY_SLOT_TIME			20

#define WL_SPURAVOID_OFF		0
#define WL_SPURAVOID_ON1		1
#define WL_SPURAVOID_ON2		2

#define BRCMS_USE_COREFLAGS		0xffffffff

#define BRCMS_PLCP_AUTO			-1
#define BRCMS_PLCP_SHORT		0
#define BRCMS_PLCP_LONG			1

#define BRCMS_PROTECTION_AUTO		-1
#define BRCMS_PROTECTION_OFF		0
#define BRCMS_PROTECTION_ON		1
#define BRCMS_PROTECTION_MMHDR_ONLY	2
#define BRCMS_PROTECTION_CTS_ONLY	3

#define BRCMS_PROTECTION_CTL_OFF	0
#define BRCMS_PROTECTION_CTL_LOCAL	1
#define BRCMS_PROTECTION_CTL_OVERLAP	2

#define BRCMS_N_PROTECTION_OFF		0
#define BRCMS_N_PROTECTION_OPTIONAL	1
#define BRCMS_N_PROTECTION_20IN40	2
#define BRCMS_N_PROTECTION_MIXEDMODE	3

#define BRCMS_N_BW_20ALL		0
#define BRCMS_N_BW_40ALL		1
#define BRCMS_N_BW_20IN2G_40IN5G	2

#define BRCMS_N_SGI_20			0x01
#define BRCMS_N_SGI_40			0x02

#define NRATE_MCS_INUSE			0x00000080
#define NRATE_RATE_MASK			0x0000007f
#define NRATE_STF_MASK			0x0000ff00
#define NRATE_STF_SHIFT			8
#define NRATE_OVERRIDE_MCS_ONLY		0x40000000
#define NRATE_SGI_MASK			0x00800000	
#define NRATE_SGI_SHIFT			23		
#define NRATE_LDPC_CODING		0x00400000	
#define NRATE_LDPC_SHIFT		22		

#define NRATE_STF_SISO			0		
#define NRATE_STF_CDD			1		
#define NRATE_STF_STBC			2		
#define NRATE_STF_SDM			3		

#define MAX_DMA_SEGS			4

#define NTXD				256
#define NRXD				256

#define NRXBUFPOST			32

#define BRCMS_DATAHIWAT			50

#define RXBND				8
#define TXSBND				8

struct brcms_c_bit_desc {
	u32 bit;
	const char *name;
};


#define XMTFIFOTBL_STARTREV	20

struct d11init {
	__le16 addr;
	__le16 size;
	__le32 value;
};

struct edcf_acparam {
	u8 ACI;
	u8 ECW;
	u16 TXOP;
} __packed;

const u8 prio2fifo[NUMPRIO] = {
	TX_AC_BE_FIFO,		
	TX_AC_BK_FIFO,		
	TX_AC_BK_FIFO,		
	TX_AC_BE_FIFO,		
	TX_AC_VI_FIFO,		
	TX_AC_VI_FIFO,		
	TX_AC_VO_FIFO,		
	TX_AC_VO_FIFO		
};

uint brcm_msg_level =
#if defined(DEBUG)
	LOG_ERROR_VAL;
#else
	0;
#endif				

static const u8 wme_fifo2ac[] = {
	IEEE80211_AC_BK,
	IEEE80211_AC_BE,
	IEEE80211_AC_VI,
	IEEE80211_AC_VO,
	IEEE80211_AC_BE,
	IEEE80211_AC_BE
};

static const u8 wme_ac2fifo[] = {
	TX_AC_VO_FIFO,
	TX_AC_VI_FIFO,
	TX_AC_BE_FIFO,
	TX_AC_BK_FIFO
};

const u8 wlc_prio2prec_map[] = {
	_BRCMS_PREC_BE,		
	_BRCMS_PREC_BK,		
	_BRCMS_PREC_NONE,		
	_BRCMS_PREC_EE,		
	_BRCMS_PREC_CL,		
	_BRCMS_PREC_VI,		
	_BRCMS_PREC_VO,		
	_BRCMS_PREC_NC,		
};

static const u16 xmtfifo_sz[][NFIFO] = {
	
	{20, 192, 192, 21, 17, 5},
	
	{9, 58, 22, 14, 14, 5},
	
	{20, 192, 192, 21, 17, 5},
	
	{20, 192, 192, 21, 17, 5},
	
	{9, 58, 22, 14, 14, 5},
};

#ifdef DEBUG
static const char * const fifo_names[] = {
	"AC_BK", "AC_BE", "AC_VI", "AC_VO", "BCMC", "ATIM" };
#else
static const char fifo_names[6][0];
#endif

#ifdef DEBUG
static struct brcms_c_info *wlc_info_dbg = (struct brcms_c_info *) (NULL);
#endif

static u8 brcms_basic_rate(struct brcms_c_info *wlc, u32 rspec)
{
	if (is_mcs_rate(rspec))
		return wlc->band->basic_rate[mcs_table[rspec & RSPEC_RATE_MASK]
		       .leg_ofdm];
	return wlc->band->basic_rate[rspec & RSPEC_RATE_MASK];
}

static u16 frametype(u32 rspec, u8 mimoframe)
{
	if (is_mcs_rate(rspec))
		return mimoframe;
	return is_cck_rate(rspec) ? FT_CCK : FT_OFDM;
}

static u16 get_sifs(struct brcms_band *band)
{
	return band->bandtype == BRCM_BAND_5G ? APHY_SIFS_TIME :
				 BPHY_SIFS_TIME;
}

static bool brcms_deviceremoved(struct brcms_c_info *wlc)
{
	u32 macctrl;

	if (!wlc->hw->clk)
		return ai_deviceremoved(wlc->hw->sih);
	macctrl = bcma_read32(wlc->hw->d11core,
			      D11REGOFFS(maccontrol));
	return (macctrl & (MCTL_PSM_JMP_0 | MCTL_IHR_EN)) != MCTL_IHR_EN;
}

static s16 brcms_txpktpendtot(struct brcms_c_info *wlc)
{
	return wlc->core->txpktpend[0] + wlc->core->txpktpend[1] +
	       wlc->core->txpktpend[2] + wlc->core->txpktpend[3];
}

static bool brcms_is_mband_unlocked(struct brcms_c_info *wlc)
{
	return wlc->pub->_nbands > 1 && !wlc->bandlocked;
}

static int brcms_chspec_bw(u16 chanspec)
{
	if (CHSPEC_IS40(chanspec))
		return BRCMS_40_MHZ;
	if (CHSPEC_IS20(chanspec))
		return BRCMS_20_MHZ;

	return BRCMS_10_MHZ;
}

static void brcms_c_bsscfg_mfree(struct brcms_bss_cfg *cfg)
{
	if (cfg == NULL)
		return;

	kfree(cfg->current_bss);
	kfree(cfg);
}

static void brcms_c_detach_mfree(struct brcms_c_info *wlc)
{
	if (wlc == NULL)
		return;

	brcms_c_bsscfg_mfree(wlc->bsscfg);
	kfree(wlc->pub);
	kfree(wlc->modulecb);
	kfree(wlc->default_bss);
	kfree(wlc->protection);
	kfree(wlc->stf);
	kfree(wlc->bandstate[0]);
	kfree(wlc->corestate->macstat_snapshot);
	kfree(wlc->corestate);
	kfree(wlc->hw->bandstate[0]);
	kfree(wlc->hw);

	
	kfree(wlc);
	wlc = NULL;
}

static struct brcms_bss_cfg *brcms_c_bsscfg_malloc(uint unit)
{
	struct brcms_bss_cfg *cfg;

	cfg = kzalloc(sizeof(struct brcms_bss_cfg), GFP_ATOMIC);
	if (cfg == NULL)
		goto fail;

	cfg->current_bss = kzalloc(sizeof(struct brcms_bss_info), GFP_ATOMIC);
	if (cfg->current_bss == NULL)
		goto fail;

	return cfg;

 fail:
	brcms_c_bsscfg_mfree(cfg);
	return NULL;
}

static struct brcms_c_info *
brcms_c_attach_malloc(uint unit, uint *err, uint devid)
{
	struct brcms_c_info *wlc;

	wlc = kzalloc(sizeof(struct brcms_c_info), GFP_ATOMIC);
	if (wlc == NULL) {
		*err = 1002;
		goto fail;
	}

	
	wlc->pub = kzalloc(sizeof(struct brcms_pub), GFP_ATOMIC);
	if (wlc->pub == NULL) {
		*err = 1003;
		goto fail;
	}
	wlc->pub->wlc = wlc;

	

	wlc->hw = kzalloc(sizeof(struct brcms_hardware), GFP_ATOMIC);
	if (wlc->hw == NULL) {
		*err = 1005;
		goto fail;
	}
	wlc->hw->wlc = wlc;

	wlc->hw->bandstate[0] =
		kzalloc(sizeof(struct brcms_hw_band) * MAXBANDS, GFP_ATOMIC);
	if (wlc->hw->bandstate[0] == NULL) {
		*err = 1006;
		goto fail;
	} else {
		int i;

		for (i = 1; i < MAXBANDS; i++)
			wlc->hw->bandstate[i] = (struct brcms_hw_band *)
			    ((unsigned long)wlc->hw->bandstate[0] +
			     (sizeof(struct brcms_hw_band) * i));
	}

	wlc->modulecb =
		kzalloc(sizeof(struct modulecb) * BRCMS_MAXMODULES, GFP_ATOMIC);
	if (wlc->modulecb == NULL) {
		*err = 1009;
		goto fail;
	}

	wlc->default_bss = kzalloc(sizeof(struct brcms_bss_info), GFP_ATOMIC);
	if (wlc->default_bss == NULL) {
		*err = 1010;
		goto fail;
	}

	wlc->bsscfg = brcms_c_bsscfg_malloc(unit);
	if (wlc->bsscfg == NULL) {
		*err = 1011;
		goto fail;
	}

	wlc->protection = kzalloc(sizeof(struct brcms_protection),
				  GFP_ATOMIC);
	if (wlc->protection == NULL) {
		*err = 1016;
		goto fail;
	}

	wlc->stf = kzalloc(sizeof(struct brcms_stf), GFP_ATOMIC);
	if (wlc->stf == NULL) {
		*err = 1017;
		goto fail;
	}

	wlc->bandstate[0] =
		kzalloc(sizeof(struct brcms_band)*MAXBANDS, GFP_ATOMIC);
	if (wlc->bandstate[0] == NULL) {
		*err = 1025;
		goto fail;
	} else {
		int i;

		for (i = 1; i < MAXBANDS; i++)
			wlc->bandstate[i] = (struct brcms_band *)
				((unsigned long)wlc->bandstate[0]
				+ (sizeof(struct brcms_band)*i));
	}

	wlc->corestate = kzalloc(sizeof(struct brcms_core), GFP_ATOMIC);
	if (wlc->corestate == NULL) {
		*err = 1026;
		goto fail;
	}

	wlc->corestate->macstat_snapshot =
		kzalloc(sizeof(struct macstat), GFP_ATOMIC);
	if (wlc->corestate->macstat_snapshot == NULL) {
		*err = 1027;
		goto fail;
	}

	return wlc;

 fail:
	brcms_c_detach_mfree(wlc);
	return NULL;
}

static void brcms_b_update_slot_timing(struct brcms_hardware *wlc_hw,
					bool shortslot)
{
	struct bcma_device *core = wlc_hw->d11core;

	if (shortslot) {
		
		bcma_write16(core, D11REGOFFS(ifs_slot), 0x0207);
		brcms_b_write_shm(wlc_hw, M_DOT11_SLOT, APHY_SLOT_TIME);
	} else {
		
		bcma_write16(core, D11REGOFFS(ifs_slot), 0x0212);
		brcms_b_write_shm(wlc_hw, M_DOT11_SLOT, BPHY_SLOT_TIME);
	}
}

static uint brcms_c_calc_frame_time(struct brcms_c_info *wlc, u32 ratespec,
				    u8 preamble_type, uint mac_len)
{
	uint nsyms, dur = 0, Ndps, kNdps;
	uint rate = rspec2rate(ratespec);

	if (rate == 0) {
		wiphy_err(wlc->wiphy, "wl%d: WAR: using rate of 1 mbps\n",
			  wlc->pub->unit);
		rate = BRCM_RATE_1M;
	}

	BCMMSG(wlc->wiphy, "wl%d: rspec 0x%x, preamble_type %d, len%d\n",
		 wlc->pub->unit, ratespec, preamble_type, mac_len);

	if (is_mcs_rate(ratespec)) {
		uint mcs = ratespec & RSPEC_RATE_MASK;
		int tot_streams = mcs_2_txstreams(mcs) + rspec_stc(ratespec);

		dur = PREN_PREAMBLE + (tot_streams * PREN_PREAMBLE_EXT);
		if (preamble_type == BRCMS_MM_PREAMBLE)
			dur += PREN_MM_EXT;
		
		kNdps = mcs_2_rate(mcs, rspec_is40mhz(ratespec),
				   rspec_issgi(ratespec)) * 4;

		if (rspec_stc(ratespec) == 0)
			nsyms =
			    CEIL((APHY_SERVICE_NBITS + 8 * mac_len +
				  APHY_TAIL_NBITS) * 1000, kNdps);
		else
			
			nsyms =
			    2 *
			    CEIL((APHY_SERVICE_NBITS + 8 * mac_len +
				  APHY_TAIL_NBITS) * 1000, 2 * kNdps);

		dur += APHY_SYMBOL_TIME * nsyms;
		if (wlc->band->bandtype == BRCM_BAND_2G)
			dur += DOT11_OFDM_SIGNAL_EXTENSION;
	} else if (is_ofdm_rate(rate)) {
		dur = APHY_PREAMBLE_TIME;
		dur += APHY_SIGNAL_TIME;
		
		Ndps = rate * 2;
		
		nsyms =
		    CEIL((APHY_SERVICE_NBITS + 8 * mac_len + APHY_TAIL_NBITS),
			 Ndps);
		dur += APHY_SYMBOL_TIME * nsyms;
		if (wlc->band->bandtype == BRCM_BAND_2G)
			dur += DOT11_OFDM_SIGNAL_EXTENSION;
	} else {
		mac_len = mac_len * 8 * 2;
		
		dur = (mac_len + rate - 1) / rate;
		if (preamble_type & BRCMS_SHORT_PREAMBLE)
			dur += BPHY_PLCP_SHORT_TIME;
		else
			dur += BPHY_PLCP_TIME;
	}
	return dur;
}

static void brcms_c_write_inits(struct brcms_hardware *wlc_hw,
				const struct d11init *inits)
{
	struct bcma_device *core = wlc_hw->d11core;
	int i;
	uint offset;
	u16 size;
	u32 value;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	for (i = 0; inits[i].addr != cpu_to_le16(0xffff); i++) {
		size = le16_to_cpu(inits[i].size);
		offset = le16_to_cpu(inits[i].addr);
		value = le32_to_cpu(inits[i].value);
		if (size == 2)
			bcma_write16(core, offset, value);
		else if (size == 4)
			bcma_write32(core, offset, value);
		else
			break;
	}
}

static void brcms_c_write_mhf(struct brcms_hardware *wlc_hw, u16 *mhfs)
{
	u8 idx;
	u16 addr[] = {
		M_HOST_FLAGS1, M_HOST_FLAGS2, M_HOST_FLAGS3, M_HOST_FLAGS4,
		M_HOST_FLAGS5
	};

	for (idx = 0; idx < MHFMAX; idx++)
		brcms_b_write_shm(wlc_hw, addr[idx], mhfs[idx]);
}

static void brcms_c_ucode_bsinit(struct brcms_hardware *wlc_hw)
{
	struct wiphy *wiphy = wlc_hw->wlc->wiphy;
	struct brcms_ucode *ucode = &wlc_hw->wlc->wl->ucode;

	
	brcms_c_write_mhf(wlc_hw, wlc_hw->band->mhfs);

	
	if (D11REV_IS(wlc_hw->corerev, 23)) {
		if (BRCMS_ISNPHY(wlc_hw->band))
			brcms_c_write_inits(wlc_hw, ucode->d11n0bsinitvals16);
		else
			wiphy_err(wiphy, "%s: wl%d: unsupported phy in corerev"
				  " %d\n", __func__, wlc_hw->unit,
				  wlc_hw->corerev);
	} else {
		if (D11REV_IS(wlc_hw->corerev, 24)) {
			if (BRCMS_ISLCNPHY(wlc_hw->band))
				brcms_c_write_inits(wlc_hw,
						    ucode->d11lcn0bsinitvals24);
			else
				wiphy_err(wiphy, "%s: wl%d: unsupported phy in"
					  " core rev %d\n", __func__,
					  wlc_hw->unit, wlc_hw->corerev);
		} else {
			wiphy_err(wiphy, "%s: wl%d: unsupported corerev %d\n",
				__func__, wlc_hw->unit, wlc_hw->corerev);
		}
	}
}

static void brcms_b_core_ioctl(struct brcms_hardware *wlc_hw, u32 m, u32 v)
{
	struct bcma_device *core = wlc_hw->d11core;
	u32 ioctl = bcma_aread32(core, BCMA_IOCTL) & ~m;

	bcma_awrite32(core, BCMA_IOCTL, ioctl | v);
}

static void brcms_b_core_phy_clk(struct brcms_hardware *wlc_hw, bool clk)
{
	BCMMSG(wlc_hw->wlc->wiphy, "wl%d: clk %d\n", wlc_hw->unit, clk);

	wlc_hw->phyclk = clk;

	if (OFF == clk) {	

		brcms_b_core_ioctl(wlc_hw, (SICF_PRST | SICF_FGC | SICF_GMODE),
				   (SICF_PRST | SICF_FGC));
		udelay(1);
		brcms_b_core_ioctl(wlc_hw, (SICF_PRST | SICF_FGC), SICF_PRST);
		udelay(1);

	} else {		

		brcms_b_core_ioctl(wlc_hw, (SICF_PRST | SICF_FGC), SICF_FGC);
		udelay(1);
		brcms_b_core_ioctl(wlc_hw, SICF_FGC, 0);
		udelay(1);

	}
}

static void brcms_c_setxband(struct brcms_hardware *wlc_hw, uint bandunit)
{
	BCMMSG(wlc_hw->wlc->wiphy, "wl%d: bandunit %d\n", wlc_hw->unit,
		bandunit);

	wlc_hw->band = wlc_hw->bandstate[bandunit];

	wlc_hw->wlc->band = wlc_hw->wlc->bandstate[bandunit];

	
	if (wlc_hw->sbclk && !wlc_hw->noreset) {
		u32 gmode = 0;

		if (bandunit == 0)
			gmode = SICF_GMODE;

		brcms_b_core_ioctl(wlc_hw, SICF_GMODE, gmode);
	}
}

static u32 brcms_c_setband_inact(struct brcms_c_info *wlc, uint bandunit)
{
	struct brcms_hardware *wlc_hw = wlc->hw;
	u32 macintmask;
	u32 macctrl;

	BCMMSG(wlc->wiphy, "wl%d\n", wlc_hw->unit);
	macctrl = bcma_read32(wlc_hw->d11core,
			      D11REGOFFS(maccontrol));
	WARN_ON((macctrl & MCTL_EN_MAC) != 0);

	
	macintmask = brcms_intrsoff(wlc->wl);

	
	wlc_phy_switch_radio(wlc_hw->band->pi, OFF);

	brcms_b_core_phy_clk(wlc_hw, OFF);

	brcms_c_setxband(wlc_hw, bandunit);

	return macintmask;
}

static bool
brcms_c_dotxstatus(struct brcms_c_info *wlc, struct tx_status *txs)
{
	struct sk_buff *p;
	uint queue;
	struct d11txh *txh;
	struct scb *scb = NULL;
	bool free_pdu;
	int tx_rts, tx_frame_count, tx_rts_count;
	uint totlen, supr_status;
	bool lastframe;
	struct ieee80211_hdr *h;
	u16 mcl;
	struct ieee80211_tx_info *tx_info;
	struct ieee80211_tx_rate *txrate;
	int i;

	if (!(txs->status & TX_STATUS_AMPDU)
	    && (txs->status & TX_STATUS_INTERMEDIATE)) {
		BCMMSG(wlc->wiphy, "INTERMEDIATE but not AMPDU\n");
		return false;
	}

	queue = txs->frameid & TXFID_QUEUE_MASK;
	if (queue >= NFIFO) {
		p = NULL;
		goto fatal;
	}

	p = dma_getnexttxp(wlc->hw->di[queue], DMA_RANGE_TRANSMITTED);
	if (p == NULL)
		goto fatal;

	txh = (struct d11txh *) (p->data);
	mcl = le16_to_cpu(txh->MacTxControlLow);

	if (txs->phyerr) {
		if (brcm_msg_level & LOG_ERROR_VAL) {
			wiphy_err(wlc->wiphy, "phyerr 0x%x, rate 0x%x\n",
				  txs->phyerr, txh->MainRates);
			brcms_c_print_txdesc(txh);
		}
		brcms_c_print_txstatus(txs);
	}

	if (txs->frameid != le16_to_cpu(txh->TxFrameID))
		goto fatal;
	tx_info = IEEE80211_SKB_CB(p);
	h = (struct ieee80211_hdr *)((u8 *) (txh + 1) + D11_PHY_HDR_LEN);

	if (tx_info->control.sta)
		scb = &wlc->pri_scb;

	if (tx_info->flags & IEEE80211_TX_CTL_AMPDU) {
		brcms_c_ampdu_dotxstatus(wlc->ampdu, scb, p, txs);
		return false;
	}

	supr_status = txs->status & TX_STATUS_SUPR_MASK;
	if (supr_status == TX_STATUS_SUPR_BADCH)
		BCMMSG(wlc->wiphy,
		       "%s: Pkt tx suppressed, possibly channel %d\n",
		       __func__, CHSPEC_CHANNEL(wlc->default_bss->chanspec));

	tx_rts = le16_to_cpu(txh->MacTxControlLow) & TXC_SENDRTS;
	tx_frame_count =
	    (txs->status & TX_STATUS_FRM_RTX_MASK) >> TX_STATUS_FRM_RTX_SHIFT;
	tx_rts_count =
	    (txs->status & TX_STATUS_RTS_RTX_MASK) >> TX_STATUS_RTS_RTX_SHIFT;

	lastframe = !ieee80211_has_morefrags(h->frame_control);

	if (!lastframe) {
		wiphy_err(wlc->wiphy, "Not last frame!\n");
	} else {
		u16 sfbl,	
		    lfbl,	
		    fbl;

		if (queue < IEEE80211_NUM_ACS) {
			sfbl = GFIELD(wlc->wme_retries[wme_fifo2ac[queue]],
				      EDCF_SFB);
			lfbl = GFIELD(wlc->wme_retries[wme_fifo2ac[queue]],
				      EDCF_LFB);
		} else {
			sfbl = wlc->SFBL;
			lfbl = wlc->LFBL;
		}

		txrate = tx_info->status.rates;
		if (txrate[0].flags & IEEE80211_TX_RC_USE_RTS_CTS)
			fbl = lfbl;
		else
			fbl = sfbl;

		ieee80211_tx_info_clear_status(tx_info);

		if ((tx_frame_count > fbl) && (txrate[1].idx >= 0)) {
			txrate[0].count = fbl;
			txrate[1].count = tx_frame_count - fbl;
		} else {
			txrate[0].count = tx_frame_count;
			txrate[1].idx = -1;
			txrate[1].count = 0;
		}

		
		for (i = 2; i < IEEE80211_TX_MAX_RATES; i++) {
			txrate[i].idx = -1;
			txrate[i].count = 0;
		}

		if (txs->status & TX_STATUS_ACK_RCV)
			tx_info->flags |= IEEE80211_TX_STAT_ACK;
	}

	totlen = p->len;
	free_pdu = true;

	brcms_c_txfifo_complete(wlc, queue, 1);

	if (lastframe) {
		
		skb_pull(p, D11_PHY_HDR_LEN);
		skb_pull(p, D11_TXH_LEN);
		ieee80211_tx_status_irqsafe(wlc->pub->ieee_hw, p);
	} else {
		wiphy_err(wlc->wiphy, "%s: Not last frame => not calling "
			  "tx_status\n", __func__);
	}

	return false;

 fatal:
	if (p)
		brcmu_pkt_buf_free_skb(p);

	return true;

}

static bool
brcms_b_txstatus(struct brcms_hardware *wlc_hw, bool bound, bool *fatal)
{
	bool morepending = false;
	struct brcms_c_info *wlc = wlc_hw->wlc;
	struct bcma_device *core;
	struct tx_status txstatus, *txs;
	u32 s1, s2;
	uint n = 0;
	uint max_tx_num = bound ? TXSBND : -1;

	BCMMSG(wlc->wiphy, "wl%d\n", wlc_hw->unit);

	txs = &txstatus;
	core = wlc_hw->d11core;
	*fatal = false;
	s1 = bcma_read32(core, D11REGOFFS(frmtxstatus));
	while (!(*fatal)
	       && (s1 & TXS_V)) {

		if (s1 == 0xffffffff) {
			wiphy_err(wlc->wiphy, "wl%d: %s: dead chip\n",
				wlc_hw->unit, __func__);
			return morepending;
		}
		s2 = bcma_read32(core, D11REGOFFS(frmtxstatus2));

		txs->status = s1 & TXS_STATUS_MASK;
		txs->frameid = (s1 & TXS_FID_MASK) >> TXS_FID_SHIFT;
		txs->sequence = s2 & TXS_SEQ_MASK;
		txs->phyerr = (s2 & TXS_PTX_MASK) >> TXS_PTX_SHIFT;
		txs->lasttxtime = 0;

		*fatal = brcms_c_dotxstatus(wlc_hw->wlc, txs);

		
		if (++n >= max_tx_num)
			break;
		s1 = bcma_read32(core, D11REGOFFS(frmtxstatus));
	}

	if (*fatal)
		return 0;

	if (n >= max_tx_num)
		morepending = true;

	if (!pktq_empty(&wlc->pkt_queue->q))
		brcms_c_send_q(wlc);

	return morepending;
}

static void brcms_c_tbtt(struct brcms_c_info *wlc)
{
	if (!wlc->bsscfg->BSS)
		wlc->qvalid |= MCMD_DIRFRMQVAL;
}

static void
brcms_c_mhfdef(struct brcms_c_info *wlc, u16 *mhfs, u16 mhf2_init)
{
	struct brcms_hardware *wlc_hw = wlc->hw;

	memset(mhfs, 0, MHFMAX * sizeof(u16));

	mhfs[MHF2] |= mhf2_init;

	
	if (wlc_hw->boardflags & BFL_NOPLLDOWN)
		mhfs[MHF1] |= MHF1_FORCEFASTCLK;

	if (BRCMS_ISNPHY(wlc_hw->band) && NREV_LT(wlc_hw->band->phyrev, 2)) {
		mhfs[MHF2] |= MHF2_NPHY40MHZ_WAR;
		mhfs[MHF1] |= MHF1_IQSWAP_WAR;
	}
}

static uint
dmareg(uint direction, uint fifonum)
{
	if (direction == DMA_TX)
		return offsetof(struct d11regs, fifo64regs[fifonum].dmaxmt);
	return offsetof(struct d11regs, fifo64regs[fifonum].dmarcv);
}

static bool brcms_b_attach_dmapio(struct brcms_c_info *wlc, uint j, bool wme)
{
	uint i;
	char name[8];
	u16 pio_mhf2 = 0;
	struct brcms_hardware *wlc_hw = wlc->hw;
	uint unit = wlc_hw->unit;
	struct wiphy *wiphy = wlc->wiphy;

	
	snprintf(name, sizeof(name), "wl%d", unit);

	if (wlc_hw->di[0] == NULL) {	
		int dma_attach_err = 0;

		wlc_hw->di[0] = dma_attach(name, wlc_hw->sih, wlc_hw->d11core,
					   (wme ? dmareg(DMA_TX, 0) : 0),
					   dmareg(DMA_RX, 0),
					   (wme ? NTXD : 0), NRXD,
					   RXBUFSZ, -1, NRXBUFPOST,
					   BRCMS_HWRXOFF, &brcm_msg_level);
		dma_attach_err |= (NULL == wlc_hw->di[0]);

		wlc_hw->di[1] = dma_attach(name, wlc_hw->sih, wlc_hw->d11core,
					   dmareg(DMA_TX, 1), 0,
					   NTXD, 0, 0, -1, 0, 0,
					   &brcm_msg_level);
		dma_attach_err |= (NULL == wlc_hw->di[1]);

		wlc_hw->di[2] = dma_attach(name, wlc_hw->sih, wlc_hw->d11core,
					   dmareg(DMA_TX, 2), 0,
					   NTXD, 0, 0, -1, 0, 0,
					   &brcm_msg_level);
		dma_attach_err |= (NULL == wlc_hw->di[2]);
		wlc_hw->di[3] = dma_attach(name, wlc_hw->sih, wlc_hw->d11core,
					   dmareg(DMA_TX, 3),
					   0, NTXD, 0, 0, -1,
					   0, 0, &brcm_msg_level);
		dma_attach_err |= (NULL == wlc_hw->di[3]);

		if (dma_attach_err) {
			wiphy_err(wiphy, "wl%d: wlc_attach: dma_attach failed"
				  "\n", unit);
			return false;
		}

		
		for (i = 0; i < NFIFO; i++)
			if (wlc_hw->di[i])
				wlc_hw->txavail[i] =
				    (uint *) dma_getvar(wlc_hw->di[i],
							"&txavail");
	}

	
	brcms_c_mhfdef(wlc, wlc_hw->band->mhfs, pio_mhf2);

	return true;
}

static void brcms_b_detach_dmapio(struct brcms_hardware *wlc_hw)
{
	uint j;

	for (j = 0; j < NFIFO; j++) {
		if (wlc_hw->di[j]) {
			dma_detach(wlc_hw->di[j]);
			wlc_hw->di[j] = NULL;
		}
	}
}

static void brcms_b_info_init(struct brcms_hardware *wlc_hw)
{
	struct brcms_c_info *wlc = wlc_hw->wlc;

	
	wlc->defmacintmask = DEF_MACINTMASK;

	
	wlc_hw->shortslot = false;

	wlc_hw->SFBL = RETRY_SHORT_FB;
	wlc_hw->LFBL = RETRY_LONG_FB;

	
	wlc_hw->SRL = RETRY_SHORT_DEF;
	wlc_hw->LRL = RETRY_LONG_DEF;
	wlc_hw->chanspec = ch20mhz_chspec(1);
}

static void brcms_b_wait_for_wake(struct brcms_hardware *wlc_hw)
{
	
	udelay(40);

	
	SPINWAIT((brcms_b_read_shm(wlc_hw, M_UCODE_DBGST) ==
		  DBGST_ASLEEP), wlc_hw->wlc->fastpwrup_dly);
}

static void brcms_b_clkctl_clk(struct brcms_hardware *wlc_hw, uint mode)
{
	if (ai_get_cccaps(wlc_hw->sih) & CC_CAP_PMU) {

		if (wlc_hw->clk) {
			if (mode == CLK_FAST) {
				bcma_set32(wlc_hw->d11core,
					   D11REGOFFS(clk_ctl_st),
					   CCS_FORCEHT);

				udelay(64);

				SPINWAIT(
				    ((bcma_read32(wlc_hw->d11core,
				      D11REGOFFS(clk_ctl_st)) &
				      CCS_HTAVAIL) == 0),
				      PMU_MAX_TRANSITION_DLY);
				WARN_ON(!(bcma_read32(wlc_hw->d11core,
					D11REGOFFS(clk_ctl_st)) &
					CCS_HTAVAIL));
			} else {
				if ((ai_get_pmurev(wlc_hw->sih) == 0) &&
				    (bcma_read32(wlc_hw->d11core,
					D11REGOFFS(clk_ctl_st)) &
					(CCS_FORCEHT | CCS_HTAREQ)))
					SPINWAIT(
					    ((bcma_read32(wlc_hw->d11core,
					      offsetof(struct d11regs,
						       clk_ctl_st)) &
					      CCS_HTAVAIL) == 0),
					      PMU_MAX_TRANSITION_DLY);
				bcma_mask32(wlc_hw->d11core,
					D11REGOFFS(clk_ctl_st),
					~CCS_FORCEHT);
			}
		}
		wlc_hw->forcefastclk = (mode == CLK_FAST);
	} else {


		wlc_hw->forcefastclk = ai_clkctl_cc(wlc_hw->sih, mode);

		
		if (wlc_hw->forcefastclk && wlc_hw->clk)
			WARN_ON(!(bcma_aread32(wlc_hw->d11core, BCMA_IOST) &
				  SISF_FCLKA));

		if (wlc_hw->forcefastclk)
			mboolset(wlc_hw->wake_override,
				 BRCMS_WAKE_OVERRIDE_FORCEFAST);
		else
			mboolclr(wlc_hw->wake_override,
				 BRCMS_WAKE_OVERRIDE_FORCEFAST);
	}
}

void
brcms_b_mhf(struct brcms_hardware *wlc_hw, u8 idx, u16 mask, u16 val,
	     int bands)
{
	u16 save;
	u16 addr[MHFMAX] = {
		M_HOST_FLAGS1, M_HOST_FLAGS2, M_HOST_FLAGS3, M_HOST_FLAGS4,
		M_HOST_FLAGS5
	};
	struct brcms_hw_band *band;

	if ((val & ~mask) || idx >= MHFMAX)
		return; 

	switch (bands) {
	case BRCM_BAND_AUTO:
	case BRCM_BAND_ALL:
		band = wlc_hw->band;
		break;
	case BRCM_BAND_5G:
		band = wlc_hw->bandstate[BAND_5G_INDEX];
		break;
	case BRCM_BAND_2G:
		band = wlc_hw->bandstate[BAND_2G_INDEX];
		break;
	default:
		band = NULL;	
	}

	if (band) {
		save = band->mhfs[idx];
		band->mhfs[idx] = (band->mhfs[idx] & ~mask) | val;

		if (wlc_hw->clk && (band->mhfs[idx] != save)
		    && (band == wlc_hw->band))
			brcms_b_write_shm(wlc_hw, addr[idx],
					   (u16) band->mhfs[idx]);
	}

	if (bands == BRCM_BAND_ALL) {
		wlc_hw->bandstate[0]->mhfs[idx] =
		    (wlc_hw->bandstate[0]->mhfs[idx] & ~mask) | val;
		wlc_hw->bandstate[1]->mhfs[idx] =
		    (wlc_hw->bandstate[1]->mhfs[idx] & ~mask) | val;
	}
}

static void brcms_c_mctrl_reset(struct brcms_hardware *wlc_hw)
{
	
	wlc_hw->maccontrol = 0;
	wlc_hw->suspended_fifos = 0;
	wlc_hw->wake_override = 0;
	wlc_hw->mute_override = 0;
	brcms_b_mctrl(wlc_hw, ~0, MCTL_IHR_EN | MCTL_WAKE);
}

static void brcms_c_mctrl_write(struct brcms_hardware *wlc_hw)
{
	u32 maccontrol = wlc_hw->maccontrol;

	
	if (wlc_hw->wake_override)
		maccontrol |= MCTL_WAKE;

	
	if (wlc_hw->mute_override) {
		maccontrol &= ~(MCTL_AP);
		maccontrol |= MCTL_INFRA;
	}

	bcma_write32(wlc_hw->d11core, D11REGOFFS(maccontrol),
		     maccontrol);
}

void brcms_b_mctrl(struct brcms_hardware *wlc_hw, u32 mask, u32 val)
{
	u32 maccontrol;
	u32 new_maccontrol;

	if (val & ~mask)
		return; 
	maccontrol = wlc_hw->maccontrol;
	new_maccontrol = (maccontrol & ~mask) | val;

	
	if (new_maccontrol == maccontrol)
		return;

	
	wlc_hw->maccontrol = new_maccontrol;

	
	brcms_c_mctrl_write(wlc_hw);
}

void brcms_c_ucode_wake_override_set(struct brcms_hardware *wlc_hw,
				 u32 override_bit)
{
	if (wlc_hw->wake_override || (wlc_hw->maccontrol & MCTL_WAKE)) {
		mboolset(wlc_hw->wake_override, override_bit);
		return;
	}

	mboolset(wlc_hw->wake_override, override_bit);

	brcms_c_mctrl_write(wlc_hw);
	brcms_b_wait_for_wake(wlc_hw);
}

void brcms_c_ucode_wake_override_clear(struct brcms_hardware *wlc_hw,
				   u32 override_bit)
{
	mboolclr(wlc_hw->wake_override, override_bit);

	if (wlc_hw->wake_override || (wlc_hw->maccontrol & MCTL_WAKE))
		return;

	brcms_c_mctrl_write(wlc_hw);
}

static void brcms_c_ucode_mute_override_set(struct brcms_hardware *wlc_hw)
{
	wlc_hw->mute_override = 1;

	if ((wlc_hw->maccontrol & (MCTL_AP | MCTL_INFRA)) == MCTL_INFRA)
		return;

	brcms_c_mctrl_write(wlc_hw);
}

static void brcms_c_ucode_mute_override_clear(struct brcms_hardware *wlc_hw)
{
	if (wlc_hw->mute_override == 0)
		return;

	wlc_hw->mute_override = 0;

	if ((wlc_hw->maccontrol & (MCTL_AP | MCTL_INFRA)) == MCTL_INFRA)
		return;

	brcms_c_mctrl_write(wlc_hw);
}

static void
brcms_b_set_addrmatch(struct brcms_hardware *wlc_hw, int match_reg_offset,
		       const u8 *addr)
{
	struct bcma_device *core = wlc_hw->d11core;
	u16 mac_l;
	u16 mac_m;
	u16 mac_h;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d: brcms_b_set_addrmatch\n",
		 wlc_hw->unit);

	mac_l = addr[0] | (addr[1] << 8);
	mac_m = addr[2] | (addr[3] << 8);
	mac_h = addr[4] | (addr[5] << 8);

	
	bcma_write16(core, D11REGOFFS(rcm_ctl),
		     RCM_INC_DATA | match_reg_offset);
	bcma_write16(core, D11REGOFFS(rcm_mat_data), mac_l);
	bcma_write16(core, D11REGOFFS(rcm_mat_data), mac_m);
	bcma_write16(core, D11REGOFFS(rcm_mat_data), mac_h);
}

void
brcms_b_write_template_ram(struct brcms_hardware *wlc_hw, int offset, int len,
			    void *buf)
{
	struct bcma_device *core = wlc_hw->d11core;
	u32 word;
	__le32 word_le;
	__be32 word_be;
	bool be_bit;
	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	bcma_write32(core, D11REGOFFS(tplatewrptr), offset);

	be_bit = (bcma_read32(core, D11REGOFFS(maccontrol)) & MCTL_BIGEND) != 0;

	while (len > 0) {
		memcpy(&word, buf, sizeof(u32));

		if (be_bit) {
			word_be = cpu_to_be32(word);
			word = *(u32 *)&word_be;
		} else {
			word_le = cpu_to_le32(word);
			word = *(u32 *)&word_le;
		}

		bcma_write32(core, D11REGOFFS(tplatewrdata), word);

		buf = (u8 *) buf + sizeof(u32);
		len -= sizeof(u32);
	}
}

static void brcms_b_set_cwmin(struct brcms_hardware *wlc_hw, u16 newmin)
{
	wlc_hw->band->CWmin = newmin;

	bcma_write32(wlc_hw->d11core, D11REGOFFS(objaddr),
		     OBJADDR_SCR_SEL | S_DOT11_CWMIN);
	(void)bcma_read32(wlc_hw->d11core, D11REGOFFS(objaddr));
	bcma_write32(wlc_hw->d11core, D11REGOFFS(objdata), newmin);
}

static void brcms_b_set_cwmax(struct brcms_hardware *wlc_hw, u16 newmax)
{
	wlc_hw->band->CWmax = newmax;

	bcma_write32(wlc_hw->d11core, D11REGOFFS(objaddr),
		     OBJADDR_SCR_SEL | S_DOT11_CWMAX);
	(void)bcma_read32(wlc_hw->d11core, D11REGOFFS(objaddr));
	bcma_write32(wlc_hw->d11core, D11REGOFFS(objdata), newmax);
}

void brcms_b_bw_set(struct brcms_hardware *wlc_hw, u16 bw)
{
	bool fastclk;

	
	fastclk = wlc_hw->forcefastclk;
	if (!fastclk)
		brcms_b_clkctl_clk(wlc_hw, CLK_FAST);

	wlc_phy_bw_state_set(wlc_hw->band->pi, bw);

	brcms_b_phy_reset(wlc_hw);
	wlc_phy_init(wlc_hw->band->pi, wlc_phy_chanspec_get(wlc_hw->band->pi));

	
	if (!fastclk)
		brcms_b_clkctl_clk(wlc_hw, CLK_DYNAMIC);
}

static void brcms_b_upd_synthpu(struct brcms_hardware *wlc_hw)
{
	u16 v;
	struct brcms_c_info *wlc = wlc_hw->wlc;
	

	if (BRCMS_ISLCNPHY(wlc->band))
		v = SYNTHPU_DLY_LPPHY_US;
	else if (BRCMS_ISNPHY(wlc->band) && (NREV_GE(wlc->band->phyrev, 3)))
		v = SYNTHPU_DLY_NPHY_US;
	else
		v = SYNTHPU_DLY_BPHY_US;

	brcms_b_write_shm(wlc_hw, M_SYNTHPU_DLY, v);
}

static void brcms_c_ucode_txant_set(struct brcms_hardware *wlc_hw)
{
	u16 phyctl;
	u16 phytxant = wlc_hw->bmac_phytxant;
	u16 mask = PHY_TXC_ANT_MASK;

	
	phyctl = brcms_b_read_shm(wlc_hw, M_CTXPRS_BLK + C_CTX_PCTLWD_POS);
	phyctl = (phyctl & ~mask) | phytxant;
	brcms_b_write_shm(wlc_hw, M_CTXPRS_BLK + C_CTX_PCTLWD_POS, phyctl);

	
	phyctl = brcms_b_read_shm(wlc_hw, M_RSP_PCTLWD);
	phyctl = (phyctl & ~mask) | phytxant;
	brcms_b_write_shm(wlc_hw, M_RSP_PCTLWD, phyctl);
}

static u16 brcms_b_ofdm_ratetable_offset(struct brcms_hardware *wlc_hw,
					 u8 rate)
{
	uint i;
	u8 plcp_rate = 0;
	struct plcp_signal_rate_lookup {
		u8 rate;
		u8 signal_rate;
	};
	
	const struct plcp_signal_rate_lookup rate_lookup[] = {
		{BRCM_RATE_6M, 0xB},
		{BRCM_RATE_9M, 0xF},
		{BRCM_RATE_12M, 0xA},
		{BRCM_RATE_18M, 0xE},
		{BRCM_RATE_24M, 0x9},
		{BRCM_RATE_36M, 0xD},
		{BRCM_RATE_48M, 0x8},
		{BRCM_RATE_54M, 0xC}
	};

	for (i = 0; i < ARRAY_SIZE(rate_lookup); i++) {
		if (rate == rate_lookup[i].rate) {
			plcp_rate = rate_lookup[i].signal_rate;
			break;
		}
	}

	return 2 * brcms_b_read_shm(wlc_hw, M_RT_DIRMAP_A + (plcp_rate * 2));
}

static void brcms_upd_ofdm_pctl1_table(struct brcms_hardware *wlc_hw)
{
	u8 rate;
	u8 rates[8] = {
		BRCM_RATE_6M, BRCM_RATE_9M, BRCM_RATE_12M, BRCM_RATE_18M,
		BRCM_RATE_24M, BRCM_RATE_36M, BRCM_RATE_48M, BRCM_RATE_54M
	};
	u16 entry_ptr;
	u16 pctl1;
	uint i;

	if (!BRCMS_PHY_11N_CAP(wlc_hw->band))
		return;

	
	for (i = 0; i < ARRAY_SIZE(rates); i++) {
		rate = rates[i];

		entry_ptr = brcms_b_ofdm_ratetable_offset(wlc_hw, rate);

		
		pctl1 =
		    brcms_b_read_shm(wlc_hw, entry_ptr + M_RT_OFDM_PCTL1_POS);

		
		pctl1 &= ~PHY_TXC1_MODE_MASK;
		pctl1 |= (wlc_hw->hw_stf_ss_opmode << PHY_TXC1_MODE_SHIFT);

		
		brcms_b_write_shm(wlc_hw, entry_ptr + M_RT_OFDM_PCTL1_POS,
				   pctl1);
	}
}

static void brcms_b_bsinit(struct brcms_c_info *wlc, u16 chanspec)
{
	struct brcms_hardware *wlc_hw = wlc->hw;

	BCMMSG(wlc->wiphy, "wl%d: bandunit %d\n", wlc_hw->unit,
		wlc_hw->band->bandunit);

	brcms_c_ucode_bsinit(wlc_hw);

	wlc_phy_init(wlc_hw->band->pi, chanspec);

	brcms_c_ucode_txant_set(wlc_hw);

	brcms_b_set_cwmin(wlc_hw, wlc_hw->band->CWmin);
	brcms_b_set_cwmax(wlc_hw, wlc_hw->band->CWmax);

	brcms_b_update_slot_timing(wlc_hw,
				   wlc_hw->band->bandtype == BRCM_BAND_5G ?
				   true : wlc_hw->shortslot);

	
	brcms_b_write_shm(wlc_hw, M_PHYTYPE, (u16) wlc_hw->band->phytype);
	brcms_b_write_shm(wlc_hw, M_PHYVER, (u16) wlc_hw->band->phyrev);

	brcms_upd_ofdm_pctl1_table(wlc_hw);

	brcms_b_upd_synthpu(wlc_hw);
}

void brcms_b_core_phypll_reset(struct brcms_hardware *wlc_hw)
{
	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	ai_cc_reg(wlc_hw->sih, offsetof(struct chipcregs, chipcontrol_addr),
		  ~0, 0);
	udelay(1);
	ai_cc_reg(wlc_hw->sih, offsetof(struct chipcregs, chipcontrol_data),
		  0x4, 0);
	udelay(1);
	ai_cc_reg(wlc_hw->sih, offsetof(struct chipcregs, chipcontrol_data),
		  0x4, 4);
	udelay(1);
	ai_cc_reg(wlc_hw->sih, offsetof(struct chipcregs, chipcontrol_data),
		  0x4, 0);
	udelay(1);
}

void brcms_b_phyclk_fgc(struct brcms_hardware *wlc_hw, bool clk)
{
	
	if (!BRCMS_ISNPHY(wlc_hw->band))
		return;

	if (ON == clk)
		brcms_b_core_ioctl(wlc_hw, SICF_FGC, SICF_FGC);
	else
		brcms_b_core_ioctl(wlc_hw, SICF_FGC, 0);

}

void brcms_b_macphyclk_set(struct brcms_hardware *wlc_hw, bool clk)
{
	if (ON == clk)
		brcms_b_core_ioctl(wlc_hw, SICF_MPCLKE, SICF_MPCLKE);
	else
		brcms_b_core_ioctl(wlc_hw, SICF_MPCLKE, 0);
}

void brcms_b_phy_reset(struct brcms_hardware *wlc_hw)
{
	struct brcms_phy_pub *pih = wlc_hw->band->pi;
	u32 phy_bw_clkbits;
	bool phy_in_reset = false;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	if (pih == NULL)
		return;

	phy_bw_clkbits = wlc_phy_clk_bwbits(wlc_hw->band->pi);

	
	if (BRCMS_ISNPHY(wlc_hw->band) && NREV_GE(wlc_hw->band->phyrev, 3) &&
	    NREV_LE(wlc_hw->band->phyrev, 4)) {
		
		brcms_b_core_ioctl(wlc_hw, SICF_BWMASK, phy_bw_clkbits);

		udelay(1);

		
		brcms_b_core_phypll_reset(wlc_hw);

		
		brcms_b_core_ioctl(wlc_hw, (SICF_PRST | SICF_PCLKE),
				   (SICF_PRST | SICF_PCLKE));
		phy_in_reset = true;
	} else {
		brcms_b_core_ioctl(wlc_hw,
				   (SICF_PRST | SICF_PCLKE | SICF_BWMASK),
				   (SICF_PRST | SICF_PCLKE | phy_bw_clkbits));
	}

	udelay(2);
	brcms_b_core_phy_clk(wlc_hw, ON);

	if (pih)
		wlc_phy_anacore(pih, ON);
}

static void brcms_b_setband(struct brcms_hardware *wlc_hw, uint bandunit,
			    u16 chanspec) {
	struct brcms_c_info *wlc = wlc_hw->wlc;
	u32 macintmask;

	
	if (!bcma_core_is_enabled(wlc_hw->d11core)) {
		bcma_core_enable(wlc_hw->d11core, 0);
		brcms_c_mctrl_reset(wlc_hw);
	}

	macintmask = brcms_c_setband_inact(wlc, bandunit);

	if (!wlc_hw->up)
		return;

	brcms_b_core_phy_clk(wlc_hw, ON);

	
	brcms_b_bsinit(wlc, chanspec);

	if (wlc->macintstatus)
		wlc->macintstatus = MI_DMAINT;

	
	brcms_intrsrestore(wlc->wl, macintmask);

	
	WARN_ON((bcma_read32(wlc_hw->d11core, D11REGOFFS(maccontrol)) &
		 MCTL_EN_MAC) != 0);
}

static bool brcms_c_isgoodchip(struct brcms_hardware *wlc_hw)
{

	
	if (!CONF_HAS(D11CONF, wlc_hw->corerev)) {
		wiphy_err(wlc_hw->wlc->wiphy, "unsupported core rev %d\n",
			  wlc_hw->corerev);
		return false;
	}

	return true;
}

static bool brcms_c_validboardtype(struct brcms_hardware *wlc_hw)
{
	uint boardrev = wlc_hw->boardrev;

	
	uint brt = (boardrev & 0xf000) >> 12;
	uint b0 = (boardrev & 0xf00) >> 8;
	uint b1 = (boardrev & 0xf0) >> 4;
	uint b2 = boardrev & 0xf;

	
	if (ai_get_boardvendor(wlc_hw->sih) != PCI_VENDOR_ID_BROADCOM)
		return true;

	
	if (boardrev == 0)
		return false;

	if (boardrev <= 0xff)
		return true;

	if ((brt > 2) || (brt == 0) || (b0 > 9) || (b0 == 0) || (b1 > 9)
		|| (b2 > 9))
		return false;

	return true;
}

static char *brcms_c_get_macaddr(struct brcms_hardware *wlc_hw)
{
	enum brcms_srom_id var_id = BRCMS_SROM_MACADDR;
	char *macaddr;

	
	macaddr = getvar(wlc_hw->sih, var_id);
	if (macaddr != NULL)
		return macaddr;

	if (wlc_hw->_nbands > 1)
		var_id = BRCMS_SROM_ET1MACADDR;
	else
		var_id = BRCMS_SROM_IL0MACADDR;

	macaddr = getvar(wlc_hw->sih, var_id);
	if (macaddr == NULL)
		wiphy_err(wlc_hw->wlc->wiphy, "wl%d: wlc_get_macaddr: macaddr "
			  "getvar(%d) not found\n", wlc_hw->unit, var_id);

	return macaddr;
}

static void brcms_b_xtal(struct brcms_hardware *wlc_hw, bool want)
{
	BCMMSG(wlc_hw->wlc->wiphy, "wl%d: want %d\n", wlc_hw->unit, want);

	if (!want && wlc_hw->pllreq)
		return;

	if (wlc_hw->sih)
		ai_clkctl_xtal(wlc_hw->sih, XTAL | PLL, want);

	wlc_hw->sbclk = want;
	if (!wlc_hw->sbclk) {
		wlc_hw->clk = false;
		if (wlc_hw->band && wlc_hw->band->pi)
			wlc_phy_hw_clk_state_upd(wlc_hw->band->pi, false);
	}
}

static bool brcms_b_radio_read_hwdisabled(struct brcms_hardware *wlc_hw)
{
	bool v, clk, xtal;
	u32 flags = 0;

	xtal = wlc_hw->sbclk;
	if (!xtal)
		brcms_b_xtal(wlc_hw, ON);

	
	clk = wlc_hw->clk;
	if (!clk) {
		flags |= SICF_PCLKE;


		bcma_core_enable(wlc_hw->d11core, flags);
		brcms_c_mctrl_reset(wlc_hw);
	}

	v = ((bcma_read32(wlc_hw->d11core,
			  D11REGOFFS(phydebug)) & PDBG_RFD) != 0);

	
	if (!clk)
		bcma_core_disable(wlc_hw->d11core, 0);

	if (!xtal)
		brcms_b_xtal(wlc_hw, OFF);

	return v;
}

static bool wlc_dma_rxreset(struct brcms_hardware *wlc_hw, uint fifo)
{
	struct dma_pub *di = wlc_hw->di[fifo];
	return dma_rxreset(di);
}

void brcms_b_corereset(struct brcms_hardware *wlc_hw, u32 flags)
{
	uint i;
	bool fastclk;

	if (flags == BRCMS_USE_COREFLAGS)
		flags = (wlc_hw->band->pi ? wlc_hw->band->core_flags : 0);

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	
	fastclk = wlc_hw->forcefastclk;
	if (!fastclk)
		brcms_b_clkctl_clk(wlc_hw, CLK_FAST);

	
	if (bcma_core_is_enabled(wlc_hw->d11core)) {
		for (i = 0; i < NFIFO; i++)
			if ((wlc_hw->di[i]) && (!dma_txreset(wlc_hw->di[i])))
				wiphy_err(wlc_hw->wlc->wiphy, "wl%d: %s: "
					  "dma_txreset[%d]: cannot stop dma\n",
					   wlc_hw->unit, __func__, i);

		if ((wlc_hw->di[RX_FIFO])
		    && (!wlc_dma_rxreset(wlc_hw, RX_FIFO)))
			wiphy_err(wlc_hw->wlc->wiphy, "wl%d: %s: dma_rxreset"
				  "[%d]: cannot stop dma\n",
				  wlc_hw->unit, __func__, RX_FIFO);
	}
	
	if (wlc_hw->noreset) {
		wlc_hw->wlc->macintstatus = 0;	
		brcms_b_mctrl(wlc_hw, MCTL_PSM_RUN | MCTL_EN_MAC, 0);
		return;
	}

	flags |= SICF_PCLKE;

	wlc_hw->clk = false;
	bcma_core_enable(wlc_hw->d11core, flags);
	wlc_hw->clk = true;
	if (wlc_hw->band && wlc_hw->band->pi)
		wlc_phy_hw_clk_state_upd(wlc_hw->band->pi, true);

	brcms_c_mctrl_reset(wlc_hw);

	if (ai_get_cccaps(wlc_hw->sih) & CC_CAP_PMU)
		brcms_b_clkctl_clk(wlc_hw, CLK_FAST);

	brcms_b_phy_reset(wlc_hw);

	
	brcms_b_core_phypll_ctl(wlc_hw, true);

	
	wlc_hw->wlc->macintstatus = 0;

	
	if (!fastclk)
		brcms_b_clkctl_clk(wlc_hw, CLK_DYNAMIC);
}

static void brcms_b_corerev_fifofixup(struct brcms_hardware *wlc_hw)
{
	struct bcma_device *core = wlc_hw->d11core;
	u16 fifo_nu;
	u16 txfifo_startblk = TXFIFO_START_BLK, txfifo_endblk;
	u16 txfifo_def, txfifo_def1;
	u16 txfifo_cmd;

	
	txfifo_startblk = TXFIFO_START_BLK;

	
	for (fifo_nu = 0; fifo_nu < NFIFO; fifo_nu++) {

		txfifo_endblk = txfifo_startblk + wlc_hw->xmtfifo_sz[fifo_nu];
		txfifo_def = (txfifo_startblk & 0xff) |
		    (((txfifo_endblk - 1) & 0xff) << TXFIFO_FIFOTOP_SHIFT);
		txfifo_def1 = ((txfifo_startblk >> 8) & 0x1) |
		    ((((txfifo_endblk -
			1) >> 8) & 0x1) << TXFIFO_FIFOTOP_SHIFT);
		txfifo_cmd =
		    TXFIFOCMD_RESET_MASK | (fifo_nu << TXFIFOCMD_FIFOSEL_SHIFT);

		bcma_write16(core, D11REGOFFS(xmtfifocmd), txfifo_cmd);
		bcma_write16(core, D11REGOFFS(xmtfifodef), txfifo_def);
		bcma_write16(core, D11REGOFFS(xmtfifodef1), txfifo_def1);

		bcma_write16(core, D11REGOFFS(xmtfifocmd), txfifo_cmd);

		txfifo_startblk += wlc_hw->xmtfifo_sz[fifo_nu];
	}
	brcms_b_write_shm(wlc_hw, M_FIFOSIZE0,
			   wlc_hw->xmtfifo_sz[TX_AC_BE_FIFO]);
	brcms_b_write_shm(wlc_hw, M_FIFOSIZE1,
			   wlc_hw->xmtfifo_sz[TX_AC_VI_FIFO]);
	brcms_b_write_shm(wlc_hw, M_FIFOSIZE2,
			   ((wlc_hw->xmtfifo_sz[TX_AC_VO_FIFO] << 8) | wlc_hw->
			    xmtfifo_sz[TX_AC_BK_FIFO]));
	brcms_b_write_shm(wlc_hw, M_FIFOSIZE3,
			   ((wlc_hw->xmtfifo_sz[TX_ATIM_FIFO] << 8) | wlc_hw->
			    xmtfifo_sz[TX_BCMC_FIFO]));
}


void brcms_b_switch_macfreq(struct brcms_hardware *wlc_hw, u8 spurmode)
{
	struct bcma_device *core = wlc_hw->d11core;

	if ((ai_get_chip_id(wlc_hw->sih) == BCM43224_CHIP_ID) ||
	    (ai_get_chip_id(wlc_hw->sih) == BCM43225_CHIP_ID)) {
		if (spurmode == WL_SPURAVOID_ON2) {	
			bcma_write16(core, D11REGOFFS(tsf_clk_frac_l), 0x2082);
			bcma_write16(core, D11REGOFFS(tsf_clk_frac_h), 0x8);
		} else if (spurmode == WL_SPURAVOID_ON1) {	
			bcma_write16(core, D11REGOFFS(tsf_clk_frac_l), 0x5341);
			bcma_write16(core, D11REGOFFS(tsf_clk_frac_h), 0x8);
		} else {	
			bcma_write16(core, D11REGOFFS(tsf_clk_frac_l), 0x8889);
			bcma_write16(core, D11REGOFFS(tsf_clk_frac_h), 0x8);
		}
	} else if (BRCMS_ISLCNPHY(wlc_hw->band)) {
		if (spurmode == WL_SPURAVOID_ON1) {	
			bcma_write16(core, D11REGOFFS(tsf_clk_frac_l), 0x7CE0);
			bcma_write16(core, D11REGOFFS(tsf_clk_frac_h), 0xC);
		} else {	
			bcma_write16(core, D11REGOFFS(tsf_clk_frac_l), 0xCCCD);
			bcma_write16(core, D11REGOFFS(tsf_clk_frac_h), 0xC);
		}
	}
}

static void brcms_c_gpio_init(struct brcms_c_info *wlc)
{
	struct brcms_hardware *wlc_hw = wlc->hw;
	u32 gc, gm;

	
	brcms_b_mctrl(wlc_hw, MCTL_GPOUT_SEL_MASK, 0);


	gc = gm = 0;

	
	if (wlc_hw->antsel_type == ANTSEL_2x3) {
		
		brcms_b_mhf(wlc_hw, MHF3, MHF3_ANTSEL_EN,
			     MHF3_ANTSEL_EN, BRCM_BAND_ALL);
		brcms_b_mhf(wlc_hw, MHF3, MHF3_ANTSEL_MODE,
			     MHF3_ANTSEL_MODE, BRCM_BAND_ALL);

		
		wlc_phy_antsel_init(wlc_hw->band->pi, false);

	} else if (wlc_hw->antsel_type == ANTSEL_2x4) {
		gm |= gc |= (BOARD_GPIO_12 | BOARD_GPIO_13);
		bcma_set16(wlc_hw->d11core, D11REGOFFS(psm_gpio_oe),
			   (BOARD_GPIO_12 | BOARD_GPIO_13));
		bcma_set16(wlc_hw->d11core, D11REGOFFS(psm_gpio_out),
			   (BOARD_GPIO_12 | BOARD_GPIO_13));

		
		brcms_b_mhf(wlc_hw, MHF3, MHF3_ANTSEL_EN,
			     MHF3_ANTSEL_EN, BRCM_BAND_ALL);
		brcms_b_mhf(wlc_hw, MHF3, MHF3_ANTSEL_MODE, 0,
			     BRCM_BAND_ALL);

		
		brcms_b_write_shm(wlc_hw, M_ANTSEL_CLKDIV,
				   ANTSEL_CLKDIV_4MHZ);
	}

	if (wlc_hw->boardflags & BFL_PACTRL)
		gm |= gc |= BOARD_GPIO_PACTRL;

	
	ai_gpiocontrol(wlc_hw->sih, gm, gc, GPIO_DRV_PRIORITY);
}

static void brcms_ucode_write(struct brcms_hardware *wlc_hw,
			      const __le32 ucode[], const size_t nbytes)
{
	struct bcma_device *core = wlc_hw->d11core;
	uint i;
	uint count;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	count = (nbytes / sizeof(u32));

	bcma_write32(core, D11REGOFFS(objaddr),
		     OBJADDR_AUTO_INC | OBJADDR_UCM_SEL);
	(void)bcma_read32(core, D11REGOFFS(objaddr));
	for (i = 0; i < count; i++)
		bcma_write32(core, D11REGOFFS(objdata), le32_to_cpu(ucode[i]));

}

static void brcms_ucode_download(struct brcms_hardware *wlc_hw)
{
	struct brcms_c_info *wlc;
	struct brcms_ucode *ucode = &wlc_hw->wlc->wl->ucode;

	wlc = wlc_hw->wlc;

	if (wlc_hw->ucode_loaded)
		return;

	if (D11REV_IS(wlc_hw->corerev, 23)) {
		if (BRCMS_ISNPHY(wlc_hw->band)) {
			brcms_ucode_write(wlc_hw, ucode->bcm43xx_16_mimo,
					  ucode->bcm43xx_16_mimosz);
			wlc_hw->ucode_loaded = true;
		} else
			wiphy_err(wlc->wiphy, "%s: wl%d: unsupported phy in "
				  "corerev %d\n",
				  __func__, wlc_hw->unit, wlc_hw->corerev);
	} else if (D11REV_IS(wlc_hw->corerev, 24)) {
		if (BRCMS_ISLCNPHY(wlc_hw->band)) {
			brcms_ucode_write(wlc_hw, ucode->bcm43xx_24_lcn,
					  ucode->bcm43xx_24_lcnsz);
			wlc_hw->ucode_loaded = true;
		} else {
			wiphy_err(wlc->wiphy, "%s: wl%d: unsupported phy in "
				  "corerev %d\n",
				  __func__, wlc_hw->unit, wlc_hw->corerev);
		}
	}
}

void brcms_b_txant_set(struct brcms_hardware *wlc_hw, u16 phytxant)
{
	
	wlc_hw->bmac_phytxant = phytxant;

	
	if (!wlc_hw->up)
		return;
	brcms_c_ucode_txant_set(wlc_hw);

}

u16 brcms_b_get_txant(struct brcms_hardware *wlc_hw)
{
	return (u16) wlc_hw->wlc->stf->txant;
}

void brcms_b_antsel_type_set(struct brcms_hardware *wlc_hw, u8 antsel_type)
{
	wlc_hw->antsel_type = antsel_type;

	
	wlc_phy_antsel_type_set(wlc_hw->band->pi, antsel_type);
}

static void brcms_b_fifoerrors(struct brcms_hardware *wlc_hw)
{
	bool fatal = false;
	uint unit;
	uint intstatus, idx;
	struct bcma_device *core = wlc_hw->d11core;
	struct wiphy *wiphy = wlc_hw->wlc->wiphy;

	unit = wlc_hw->unit;

	for (idx = 0; idx < NFIFO; idx++) {
		
		intstatus =
			bcma_read32(core,
				    D11REGOFFS(intctrlregs[idx].intstatus)) &
			I_ERRORS;
		if (!intstatus)
			continue;

		BCMMSG(wlc_hw->wlc->wiphy, "wl%d: intstatus%d 0x%x\n",
			unit, idx, intstatus);

		if (intstatus & I_RO) {
			wiphy_err(wiphy, "wl%d: fifo %d: receive fifo "
				  "overflow\n", unit, idx);
			fatal = true;
		}

		if (intstatus & I_PC) {
			wiphy_err(wiphy, "wl%d: fifo %d: descriptor error\n",
				 unit, idx);
			fatal = true;
		}

		if (intstatus & I_PD) {
			wiphy_err(wiphy, "wl%d: fifo %d: data error\n", unit,
				  idx);
			fatal = true;
		}

		if (intstatus & I_DE) {
			wiphy_err(wiphy, "wl%d: fifo %d: descriptor protocol "
				  "error\n", unit, idx);
			fatal = true;
		}

		if (intstatus & I_RU)
			wiphy_err(wiphy, "wl%d: fifo %d: receive descriptor "
				  "underflow\n", idx, unit);

		if (intstatus & I_XU) {
			wiphy_err(wiphy, "wl%d: fifo %d: transmit fifo "
				  "underflow\n", idx, unit);
			fatal = true;
		}

		if (fatal) {
			brcms_fatal_error(wlc_hw->wlc->wl); 
			break;
		} else
			bcma_write32(core,
				     D11REGOFFS(intctrlregs[idx].intstatus),
				     intstatus);
	}
}

void brcms_c_intrson(struct brcms_c_info *wlc)
{
	struct brcms_hardware *wlc_hw = wlc->hw;
	wlc->macintmask = wlc->defmacintmask;
	bcma_write32(wlc_hw->d11core, D11REGOFFS(macintmask), wlc->macintmask);
}

u32 brcms_c_intrsoff(struct brcms_c_info *wlc)
{
	struct brcms_hardware *wlc_hw = wlc->hw;
	u32 macintmask;

	if (!wlc_hw->clk)
		return 0;

	macintmask = wlc->macintmask;	

	bcma_write32(wlc_hw->d11core, D11REGOFFS(macintmask), 0);
	(void)bcma_read32(wlc_hw->d11core, D11REGOFFS(macintmask));
	udelay(1);		
	wlc->macintmask = 0;

	
	return wlc->macintstatus ? 0 : macintmask;
}

void brcms_c_intrsrestore(struct brcms_c_info *wlc, u32 macintmask)
{
	struct brcms_hardware *wlc_hw = wlc->hw;
	if (!wlc_hw->clk)
		return;

	wlc->macintmask = macintmask;
	bcma_write32(wlc_hw->d11core, D11REGOFFS(macintmask), wlc->macintmask);
}

static void brcms_b_tx_fifo_suspend(struct brcms_hardware *wlc_hw,
				    uint tx_fifo)
{
	u8 fifo = 1 << tx_fifo;

	

	
	if ((wlc_hw->suspended_fifos & fifo) == fifo)
		return;

	
	if (wlc_hw->suspended_fifos == 0)
		brcms_c_ucode_wake_override_set(wlc_hw,
						BRCMS_WAKE_OVERRIDE_TXFIFO);

	wlc_hw->suspended_fifos |= fifo;

	if (wlc_hw->di[tx_fifo]) {
		if (BRCMS_PHY_11N_CAP(wlc_hw->band))
			brcms_c_suspend_mac_and_wait(wlc_hw->wlc);

		dma_txsuspend(wlc_hw->di[tx_fifo]);

		if (BRCMS_PHY_11N_CAP(wlc_hw->band))
			brcms_c_enable_mac(wlc_hw->wlc);
	}
}

static void brcms_b_tx_fifo_resume(struct brcms_hardware *wlc_hw,
				   uint tx_fifo)
{
	
	if (wlc_hw->di[tx_fifo])
		dma_txresume(wlc_hw->di[tx_fifo]);

	
	if (wlc_hw->suspended_fifos == 0)
		return;
	else {
		wlc_hw->suspended_fifos &= ~(1 << tx_fifo);
		if (wlc_hw->suspended_fifos == 0)
			brcms_c_ucode_wake_override_clear(wlc_hw,
						BRCMS_WAKE_OVERRIDE_TXFIFO);
	}
}

static void brcms_b_mute(struct brcms_hardware *wlc_hw, bool mute_tx)
{
	static const u8 null_ether_addr[ETH_ALEN] = {0, 0, 0, 0, 0, 0};

	if (mute_tx) {
		
		brcms_b_tx_fifo_suspend(wlc_hw, TX_DATA_FIFO);
		brcms_b_tx_fifo_suspend(wlc_hw, TX_CTL_FIFO);
		brcms_b_tx_fifo_suspend(wlc_hw, TX_AC_BK_FIFO);
		brcms_b_tx_fifo_suspend(wlc_hw, TX_AC_VI_FIFO);

		
		brcms_b_set_addrmatch(wlc_hw, RCM_MAC_OFFSET,
				       null_ether_addr);
	} else {
		
		brcms_b_tx_fifo_resume(wlc_hw, TX_DATA_FIFO);
		brcms_b_tx_fifo_resume(wlc_hw, TX_CTL_FIFO);
		brcms_b_tx_fifo_resume(wlc_hw, TX_AC_BK_FIFO);
		brcms_b_tx_fifo_resume(wlc_hw, TX_AC_VI_FIFO);

		
		brcms_b_set_addrmatch(wlc_hw, RCM_MAC_OFFSET,
				       wlc_hw->etheraddr);
	}

	wlc_phy_mute_upd(wlc_hw->band->pi, mute_tx, 0);

	if (mute_tx)
		brcms_c_ucode_mute_override_set(wlc_hw);
	else
		brcms_c_ucode_mute_override_clear(wlc_hw);
}

void
brcms_c_mute(struct brcms_c_info *wlc, bool mute_tx)
{
	brcms_b_mute(wlc->hw, mute_tx);
}

static inline u32 wlc_intstatus(struct brcms_c_info *wlc, bool in_isr)
{
	struct brcms_hardware *wlc_hw = wlc->hw;
	struct bcma_device *core = wlc_hw->d11core;
	u32 macintstatus;

	
	macintstatus = bcma_read32(core, D11REGOFFS(macintstatus));

	BCMMSG(wlc->wiphy, "wl%d: macintstatus: 0x%x\n", wlc_hw->unit,
		 macintstatus);

	
	if (brcms_deviceremoved(wlc))
		return -1;

	if (macintstatus == 0xffffffff)
		return 0;

	
	macintstatus &= (in_isr ? wlc->macintmask : wlc->defmacintmask);

	
	if (macintstatus == 0)
		return 0;

	
	bcma_write32(core, D11REGOFFS(macintmask), 0);
	(void)bcma_read32(core, D11REGOFFS(macintmask));
	wlc->macintmask = 0;

	
	bcma_write32(core, D11REGOFFS(macintstatus), macintstatus);

	
	if (macintstatus & MI_DMAINT)
		bcma_write32(core, D11REGOFFS(intctrlregs[RX_FIFO].intstatus),
			     DEF_RXINTMASK);

	return macintstatus;
}

bool brcms_c_intrsupd(struct brcms_c_info *wlc)
{
	u32 macintstatus;

	
	macintstatus = wlc_intstatus(wlc, false);

	
	if (macintstatus == 0xffffffff)
		return false;

	
	wlc->macintstatus |= macintstatus;

	return true;
}

bool brcms_c_isr(struct brcms_c_info *wlc, bool *wantdpc)
{
	struct brcms_hardware *wlc_hw = wlc->hw;
	u32 macintstatus;

	*wantdpc = false;

	if (!wlc_hw->up || !wlc->macintmask)
		return false;

	
	macintstatus = wlc_intstatus(wlc, true);

	if (macintstatus == 0xffffffff)
		wiphy_err(wlc->wiphy, "DEVICEREMOVED detected in the ISR code"
			  " path\n");

	
	if (macintstatus == 0)
		return false;

	*wantdpc = true;

	
	wlc->macintstatus = macintstatus;

	return true;

}

void brcms_c_suspend_mac_and_wait(struct brcms_c_info *wlc)
{
	struct brcms_hardware *wlc_hw = wlc->hw;
	struct bcma_device *core = wlc_hw->d11core;
	u32 mc, mi;
	struct wiphy *wiphy = wlc->wiphy;

	BCMMSG(wlc->wiphy, "wl%d: bandunit %d\n", wlc_hw->unit,
		wlc_hw->band->bandunit);

	wlc_hw->mac_suspend_depth++;
	if (wlc_hw->mac_suspend_depth > 1)
		return;

	
	brcms_c_ucode_wake_override_set(wlc_hw, BRCMS_WAKE_OVERRIDE_MACSUSPEND);

	mc = bcma_read32(core, D11REGOFFS(maccontrol));

	if (mc == 0xffffffff) {
		wiphy_err(wiphy, "wl%d: %s: dead chip\n", wlc_hw->unit,
			  __func__);
		brcms_down(wlc->wl);
		return;
	}
	WARN_ON(mc & MCTL_PSM_JMP_0);
	WARN_ON(!(mc & MCTL_PSM_RUN));
	WARN_ON(!(mc & MCTL_EN_MAC));

	mi = bcma_read32(core, D11REGOFFS(macintstatus));
	if (mi == 0xffffffff) {
		wiphy_err(wiphy, "wl%d: %s: dead chip\n", wlc_hw->unit,
			  __func__);
		brcms_down(wlc->wl);
		return;
	}
	WARN_ON(mi & MI_MACSSPNDD);

	brcms_b_mctrl(wlc_hw, MCTL_EN_MAC, 0);

	SPINWAIT(!(bcma_read32(core, D11REGOFFS(macintstatus)) & MI_MACSSPNDD),
		 BRCMS_MAX_MAC_SUSPEND);

	if (!(bcma_read32(core, D11REGOFFS(macintstatus)) & MI_MACSSPNDD)) {
		wiphy_err(wiphy, "wl%d: wlc_suspend_mac_and_wait: waited %d uS"
			  " and MI_MACSSPNDD is still not on.\n",
			  wlc_hw->unit, BRCMS_MAX_MAC_SUSPEND);
		wiphy_err(wiphy, "wl%d: psmdebug 0x%08x, phydebug 0x%08x, "
			  "psm_brc 0x%04x\n", wlc_hw->unit,
			  bcma_read32(core, D11REGOFFS(psmdebug)),
			  bcma_read32(core, D11REGOFFS(phydebug)),
			  bcma_read16(core, D11REGOFFS(psm_brc)));
	}

	mc = bcma_read32(core, D11REGOFFS(maccontrol));
	if (mc == 0xffffffff) {
		wiphy_err(wiphy, "wl%d: %s: dead chip\n", wlc_hw->unit,
			  __func__);
		brcms_down(wlc->wl);
		return;
	}
	WARN_ON(mc & MCTL_PSM_JMP_0);
	WARN_ON(!(mc & MCTL_PSM_RUN));
	WARN_ON(mc & MCTL_EN_MAC);
}

void brcms_c_enable_mac(struct brcms_c_info *wlc)
{
	struct brcms_hardware *wlc_hw = wlc->hw;
	struct bcma_device *core = wlc_hw->d11core;
	u32 mc, mi;

	BCMMSG(wlc->wiphy, "wl%d: bandunit %d\n", wlc_hw->unit,
		wlc->band->bandunit);

	wlc_hw->mac_suspend_depth--;
	if (wlc_hw->mac_suspend_depth > 0)
		return;

	mc = bcma_read32(core, D11REGOFFS(maccontrol));
	WARN_ON(mc & MCTL_PSM_JMP_0);
	WARN_ON(mc & MCTL_EN_MAC);
	WARN_ON(!(mc & MCTL_PSM_RUN));

	brcms_b_mctrl(wlc_hw, MCTL_EN_MAC, MCTL_EN_MAC);
	bcma_write32(core, D11REGOFFS(macintstatus), MI_MACSSPNDD);

	mc = bcma_read32(core, D11REGOFFS(maccontrol));
	WARN_ON(mc & MCTL_PSM_JMP_0);
	WARN_ON(!(mc & MCTL_EN_MAC));
	WARN_ON(!(mc & MCTL_PSM_RUN));

	mi = bcma_read32(core, D11REGOFFS(macintstatus));
	WARN_ON(mi & MI_MACSSPNDD);

	brcms_c_ucode_wake_override_clear(wlc_hw,
					  BRCMS_WAKE_OVERRIDE_MACSUSPEND);
}

void brcms_b_band_stf_ss_set(struct brcms_hardware *wlc_hw, u8 stf_mode)
{
	wlc_hw->hw_stf_ss_opmode = stf_mode;

	if (wlc_hw->clk)
		brcms_upd_ofdm_pctl1_table(wlc_hw);
}

static bool brcms_b_validate_chip_access(struct brcms_hardware *wlc_hw)
{
	struct bcma_device *core = wlc_hw->d11core;
	u32 w, val;
	struct wiphy *wiphy = wlc_hw->wlc->wiphy;

	BCMMSG(wiphy, "wl%d\n", wlc_hw->unit);

	

	bcma_write32(core, D11REGOFFS(objaddr), OBJADDR_SHM_SEL | 0);
	(void)bcma_read32(core, D11REGOFFS(objaddr));
	w = bcma_read32(core, D11REGOFFS(objdata));

	
	bcma_write32(core, D11REGOFFS(objaddr), OBJADDR_SHM_SEL | 0);
	(void)bcma_read32(core, D11REGOFFS(objaddr));
	bcma_write32(core, D11REGOFFS(objdata), (u32) 0xaa5555aa);

	bcma_write32(core, D11REGOFFS(objaddr), OBJADDR_SHM_SEL | 0);
	(void)bcma_read32(core, D11REGOFFS(objaddr));
	val = bcma_read32(core, D11REGOFFS(objdata));
	if (val != (u32) 0xaa5555aa) {
		wiphy_err(wiphy, "wl%d: validate_chip_access: SHM = 0x%x, "
			  "expected 0xaa5555aa\n", wlc_hw->unit, val);
		return false;
	}

	bcma_write32(core, D11REGOFFS(objaddr), OBJADDR_SHM_SEL | 0);
	(void)bcma_read32(core, D11REGOFFS(objaddr));
	bcma_write32(core, D11REGOFFS(objdata), (u32) 0x55aaaa55);

	bcma_write32(core, D11REGOFFS(objaddr), OBJADDR_SHM_SEL | 0);
	(void)bcma_read32(core, D11REGOFFS(objaddr));
	val = bcma_read32(core, D11REGOFFS(objdata));
	if (val != (u32) 0x55aaaa55) {
		wiphy_err(wiphy, "wl%d: validate_chip_access: SHM = 0x%x, "
			  "expected 0x55aaaa55\n", wlc_hw->unit, val);
		return false;
	}

	bcma_write32(core, D11REGOFFS(objaddr), OBJADDR_SHM_SEL | 0);
	(void)bcma_read32(core, D11REGOFFS(objaddr));
	bcma_write32(core, D11REGOFFS(objdata), w);

	
	bcma_write32(core, D11REGOFFS(tsf_cfpstart), 0);

	w = bcma_read32(core, D11REGOFFS(maccontrol));
	if ((w != (MCTL_IHR_EN | MCTL_WAKE)) &&
	    (w != (MCTL_IHR_EN | MCTL_GMODE | MCTL_WAKE))) {
		wiphy_err(wiphy, "wl%d: validate_chip_access: maccontrol = "
			  "0x%x, expected 0x%x or 0x%x\n", wlc_hw->unit, w,
			  (MCTL_IHR_EN | MCTL_WAKE),
			  (MCTL_IHR_EN | MCTL_GMODE | MCTL_WAKE));
		return false;
	}

	return true;
}

#define PHYPLL_WAIT_US	100000

void brcms_b_core_phypll_ctl(struct brcms_hardware *wlc_hw, bool on)
{
	struct bcma_device *core = wlc_hw->d11core;
	u32 tmp;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	tmp = 0;

	if (on) {
		if ((ai_get_chip_id(wlc_hw->sih) == BCM4313_CHIP_ID)) {
			bcma_set32(core, D11REGOFFS(clk_ctl_st),
				   CCS_ERSRC_REQ_HT |
				   CCS_ERSRC_REQ_D11PLL |
				   CCS_ERSRC_REQ_PHYPLL);
			SPINWAIT((bcma_read32(core, D11REGOFFS(clk_ctl_st)) &
				  CCS_ERSRC_AVAIL_HT) != CCS_ERSRC_AVAIL_HT,
				 PHYPLL_WAIT_US);

			tmp = bcma_read32(core, D11REGOFFS(clk_ctl_st));
			if ((tmp & CCS_ERSRC_AVAIL_HT) != CCS_ERSRC_AVAIL_HT)
				wiphy_err(wlc_hw->wlc->wiphy, "%s: turn on PHY"
					  " PLL failed\n", __func__);
		} else {
			bcma_set32(core, D11REGOFFS(clk_ctl_st),
				   tmp | CCS_ERSRC_REQ_D11PLL |
				   CCS_ERSRC_REQ_PHYPLL);
			SPINWAIT((bcma_read32(core, D11REGOFFS(clk_ctl_st)) &
				  (CCS_ERSRC_AVAIL_D11PLL |
				   CCS_ERSRC_AVAIL_PHYPLL)) !=
				 (CCS_ERSRC_AVAIL_D11PLL |
				  CCS_ERSRC_AVAIL_PHYPLL), PHYPLL_WAIT_US);

			tmp = bcma_read32(core, D11REGOFFS(clk_ctl_st));
			if ((tmp &
			     (CCS_ERSRC_AVAIL_D11PLL | CCS_ERSRC_AVAIL_PHYPLL))
			    !=
			    (CCS_ERSRC_AVAIL_D11PLL | CCS_ERSRC_AVAIL_PHYPLL))
				wiphy_err(wlc_hw->wlc->wiphy, "%s: turn on "
					  "PHY PLL failed\n", __func__);
		}
	} else {
		bcma_mask32(core, D11REGOFFS(clk_ctl_st),
			    ~CCS_ERSRC_REQ_PHYPLL);
		(void)bcma_read32(core, D11REGOFFS(clk_ctl_st));
	}
}

static void brcms_c_coredisable(struct brcms_hardware *wlc_hw)
{
	bool dev_gone;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	dev_gone = brcms_deviceremoved(wlc_hw->wlc);

	if (dev_gone)
		return;

	if (wlc_hw->noreset)
		return;

	
	wlc_phy_switch_radio(wlc_hw->band->pi, OFF);

	
	wlc_phy_anacore(wlc_hw->band->pi, OFF);

	
	brcms_b_core_phypll_ctl(wlc_hw, false);

	wlc_hw->clk = false;
	bcma_core_disable(wlc_hw->d11core, 0);
	wlc_phy_hw_clk_state_upd(wlc_hw->band->pi, false);
}

static void brcms_c_flushqueues(struct brcms_c_info *wlc)
{
	struct brcms_hardware *wlc_hw = wlc->hw;
	uint i;

	
	for (i = 0; i < NFIFO; i++)
		if (wlc_hw->di[i]) {
			dma_txreclaim(wlc_hw->di[i], DMA_RANGE_ALL);
			wlc->core->txpktpend[i] = 0;
			BCMMSG(wlc->wiphy, "pktpend fifo %d clrd\n", i);
		}

	
	dma_rxreclaim(wlc_hw->di[RX_FIFO]);
}

static u16
brcms_b_read_objmem(struct brcms_hardware *wlc_hw, uint offset, u32 sel)
{
	struct bcma_device *core = wlc_hw->d11core;
	u16 objoff = D11REGOFFS(objdata);

	bcma_write32(core, D11REGOFFS(objaddr), sel | (offset >> 2));
	(void)bcma_read32(core, D11REGOFFS(objaddr));
	if (offset & 2)
		objoff += 2;

	return bcma_read16(core, objoff);
}

static void
brcms_b_write_objmem(struct brcms_hardware *wlc_hw, uint offset, u16 v,
		     u32 sel)
{
	struct bcma_device *core = wlc_hw->d11core;
	u16 objoff = D11REGOFFS(objdata);

	bcma_write32(core, D11REGOFFS(objaddr), sel | (offset >> 2));
	(void)bcma_read32(core, D11REGOFFS(objaddr));
	if (offset & 2)
		objoff += 2;

	bcma_write16(core, objoff, v);
}

u16 brcms_b_read_shm(struct brcms_hardware *wlc_hw, uint offset)
{
	return brcms_b_read_objmem(wlc_hw, offset, OBJADDR_SHM_SEL);
}

void brcms_b_write_shm(struct brcms_hardware *wlc_hw, uint offset, u16 v)
{
	brcms_b_write_objmem(wlc_hw, offset, v, OBJADDR_SHM_SEL);
}

void
brcms_b_copyto_objmem(struct brcms_hardware *wlc_hw, uint offset,
		      const void *buf, int len, u32 sel)
{
	u16 v;
	const u8 *p = (const u8 *)buf;
	int i;

	if (len <= 0 || (offset & 1) || (len & 1))
		return;

	for (i = 0; i < len; i += 2) {
		v = p[i] | (p[i + 1] << 8);
		brcms_b_write_objmem(wlc_hw, offset + i, v, sel);
	}
}

void
brcms_b_copyfrom_objmem(struct brcms_hardware *wlc_hw, uint offset, void *buf,
			 int len, u32 sel)
{
	u16 v;
	u8 *p = (u8 *) buf;
	int i;

	if (len <= 0 || (offset & 1) || (len & 1))
		return;

	for (i = 0; i < len; i += 2) {
		v = brcms_b_read_objmem(wlc_hw, offset + i, sel);
		p[i] = v & 0xFF;
		p[i + 1] = (v >> 8) & 0xFF;
	}
}

static void brcms_c_copyto_shm(struct brcms_c_info *wlc, uint offset,
			const void *buf, int len)
{
	brcms_b_copyto_objmem(wlc->hw, offset, buf, len, OBJADDR_SHM_SEL);
}

static void brcms_b_retrylimit_upd(struct brcms_hardware *wlc_hw,
				   u16 SRL, u16 LRL)
{
	wlc_hw->SRL = SRL;
	wlc_hw->LRL = LRL;

	
	if (wlc_hw->up) {
		bcma_write32(wlc_hw->d11core, D11REGOFFS(objaddr),
			     OBJADDR_SCR_SEL | S_DOT11_SRC_LMT);
		(void)bcma_read32(wlc_hw->d11core, D11REGOFFS(objaddr));
		bcma_write32(wlc_hw->d11core, D11REGOFFS(objdata), wlc_hw->SRL);
		bcma_write32(wlc_hw->d11core, D11REGOFFS(objaddr),
			     OBJADDR_SCR_SEL | S_DOT11_LRC_LMT);
		(void)bcma_read32(wlc_hw->d11core, D11REGOFFS(objaddr));
		bcma_write32(wlc_hw->d11core, D11REGOFFS(objdata), wlc_hw->LRL);
	}
}

static void brcms_b_pllreq(struct brcms_hardware *wlc_hw, bool set, u32 req_bit)
{
	if (set) {
		if (mboolisset(wlc_hw->pllreq, req_bit))
			return;

		mboolset(wlc_hw->pllreq, req_bit);

		if (mboolisset(wlc_hw->pllreq, BRCMS_PLLREQ_FLIP)) {
			if (!wlc_hw->sbclk)
				brcms_b_xtal(wlc_hw, ON);
		}
	} else {
		if (!mboolisset(wlc_hw->pllreq, req_bit))
			return;

		mboolclr(wlc_hw->pllreq, req_bit);

		if (mboolisset(wlc_hw->pllreq, BRCMS_PLLREQ_FLIP)) {
			if (wlc_hw->sbclk)
				brcms_b_xtal(wlc_hw, OFF);
		}
	}
}

static void brcms_b_antsel_set(struct brcms_hardware *wlc_hw, u32 antsel_avail)
{
	wlc_hw->antsel_avail = antsel_avail;
}

static bool brcms_c_ps_allowed(struct brcms_c_info *wlc)
{
	struct brcms_bss_cfg *cfg = wlc->bsscfg;

	
	if (!wlc->pub->associated)
		return false;

	
	if (wlc->filter_flags & FIF_PROMISC_IN_BSS)
		return false;

	if (cfg->associated) {
		if (!cfg->BSS)
			return false;

		return false;
	}

	return true;
}

static void brcms_c_statsupd(struct brcms_c_info *wlc)
{
	int i;
	struct macstat macstats;
#ifdef DEBUG
	u16 delta;
	u16 rxf0ovfl;
	u16 txfunfl[NFIFO];
#endif				

	
	if (!wlc->pub->up)
		return;

#ifdef DEBUG
	
	rxf0ovfl = wlc->core->macstat_snapshot->rxf0ovfl;

	
	for (i = 0; i < NFIFO; i++)
		txfunfl[i] = wlc->core->macstat_snapshot->txfunfl[i];
#endif				

	
	brcms_b_copyfrom_objmem(wlc->hw, M_UCODE_MACSTAT, &macstats,
				sizeof(struct macstat), OBJADDR_SHM_SEL);

#ifdef DEBUG
	
	delta = (u16) (wlc->core->macstat_snapshot->rxf0ovfl - rxf0ovfl);
	if (delta)
		wiphy_err(wlc->wiphy, "wl%d: %u rx fifo 0 overflows!\n",
			  wlc->pub->unit, delta);

	
	for (i = 0; i < NFIFO; i++) {
		delta =
		    (u16) (wlc->core->macstat_snapshot->txfunfl[i] -
			      txfunfl[i]);
		if (delta)
			wiphy_err(wlc->wiphy, "wl%d: %u tx fifo %d underflows!"
				  "\n", wlc->pub->unit, delta, i);
	}
#endif				

	
	for (i = 0; i < NFIFO; i++) {
		if (wlc->hw->di[i])
			dma_counterreset(wlc->hw->di[i]);
	}
}

static void brcms_b_reset(struct brcms_hardware *wlc_hw)
{
	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	
	if (!brcms_deviceremoved(wlc_hw->wlc))
		brcms_b_corereset(wlc_hw, BRCMS_USE_COREFLAGS);

	
	brcms_c_flushqueues(wlc_hw->wlc);
}

void brcms_c_reset(struct brcms_c_info *wlc)
{
	BCMMSG(wlc->wiphy, "wl%d\n", wlc->pub->unit);

	
	brcms_c_statsupd(wlc);

	
	memset((char *)wlc->core->macstat_snapshot, 0,
		sizeof(struct macstat));

	brcms_b_reset(wlc->hw);
}

static u16 brcms_c_init_chanspec(struct brcms_c_info *wlc)
{
	u16 chanspec =
	    1 | WL_CHANSPEC_BW_20 | WL_CHANSPEC_CTL_SB_NONE |
	    WL_CHANSPEC_BAND_2G;

	return chanspec;
}

void brcms_c_init_scb(struct scb *scb)
{
	int i;

	memset(scb, 0, sizeof(struct scb));
	scb->flags = SCB_WMECAP | SCB_HTCAP;
	for (i = 0; i < NUMPRIO; i++) {
		scb->seqnum[i] = 0;
		scb->seqctl[i] = 0xFFFF;
	}

	scb->seqctl_nonqos = 0xFFFF;
	scb->magic = SCB_MAGIC;
}

static void brcms_b_coreinit(struct brcms_c_info *wlc)
{
	struct brcms_hardware *wlc_hw = wlc->hw;
	struct bcma_device *core = wlc_hw->d11core;
	u32 sflags;
	u32 bcnint_us;
	uint i = 0;
	bool fifosz_fixup = false;
	int err = 0;
	u16 buf[NFIFO];
	struct wiphy *wiphy = wlc->wiphy;
	struct brcms_ucode *ucode = &wlc_hw->wlc->wl->ucode;

	BCMMSG(wlc->wiphy, "wl%d\n", wlc_hw->unit);

	
	brcms_b_mctrl(wlc_hw, ~0, (MCTL_IHR_EN | MCTL_PSM_JMP_0 | MCTL_WAKE));

	brcms_ucode_download(wlc_hw);
	fifosz_fixup = true;

	
	bcma_write32(core, D11REGOFFS(macintstatus), -1);
	brcms_b_mctrl(wlc_hw, ~0,
		       (MCTL_IHR_EN | MCTL_INFRA | MCTL_PSM_RUN | MCTL_WAKE));

	
	SPINWAIT(((bcma_read32(core, D11REGOFFS(macintstatus)) &
		   MI_MACSSPNDD) == 0), 1000 * 1000);
	if ((bcma_read32(core, D11REGOFFS(macintstatus)) & MI_MACSSPNDD) == 0)
		wiphy_err(wiphy, "wl%d: wlc_coreinit: ucode did not self-"
			  "suspend!\n", wlc_hw->unit);

	brcms_c_gpio_init(wlc);

	sflags = bcma_aread32(core, BCMA_IOST);

	if (D11REV_IS(wlc_hw->corerev, 23)) {
		if (BRCMS_ISNPHY(wlc_hw->band))
			brcms_c_write_inits(wlc_hw, ucode->d11n0initvals16);
		else
			wiphy_err(wiphy, "%s: wl%d: unsupported phy in corerev"
				  " %d\n", __func__, wlc_hw->unit,
				  wlc_hw->corerev);
	} else if (D11REV_IS(wlc_hw->corerev, 24)) {
		if (BRCMS_ISLCNPHY(wlc_hw->band))
			brcms_c_write_inits(wlc_hw, ucode->d11lcn0initvals24);
		else
			wiphy_err(wiphy, "%s: wl%d: unsupported phy in corerev"
				  " %d\n", __func__, wlc_hw->unit,
				  wlc_hw->corerev);
	} else {
		wiphy_err(wiphy, "%s: wl%d: unsupported corerev %d\n",
			  __func__, wlc_hw->unit, wlc_hw->corerev);
	}

	
	if (fifosz_fixup)
		brcms_b_corerev_fifofixup(wlc_hw);

	
	buf[TX_AC_BE_FIFO] = brcms_b_read_shm(wlc_hw, M_FIFOSIZE0);
	if (buf[TX_AC_BE_FIFO] != wlc_hw->xmtfifo_sz[TX_AC_BE_FIFO]) {
		i = TX_AC_BE_FIFO;
		err = -1;
	}
	buf[TX_AC_VI_FIFO] = brcms_b_read_shm(wlc_hw, M_FIFOSIZE1);
	if (buf[TX_AC_VI_FIFO] != wlc_hw->xmtfifo_sz[TX_AC_VI_FIFO]) {
		i = TX_AC_VI_FIFO;
		err = -1;
	}
	buf[TX_AC_BK_FIFO] = brcms_b_read_shm(wlc_hw, M_FIFOSIZE2);
	buf[TX_AC_VO_FIFO] = (buf[TX_AC_BK_FIFO] >> 8) & 0xff;
	buf[TX_AC_BK_FIFO] &= 0xff;
	if (buf[TX_AC_BK_FIFO] != wlc_hw->xmtfifo_sz[TX_AC_BK_FIFO]) {
		i = TX_AC_BK_FIFO;
		err = -1;
	}
	if (buf[TX_AC_VO_FIFO] != wlc_hw->xmtfifo_sz[TX_AC_VO_FIFO]) {
		i = TX_AC_VO_FIFO;
		err = -1;
	}
	buf[TX_BCMC_FIFO] = brcms_b_read_shm(wlc_hw, M_FIFOSIZE3);
	buf[TX_ATIM_FIFO] = (buf[TX_BCMC_FIFO] >> 8) & 0xff;
	buf[TX_BCMC_FIFO] &= 0xff;
	if (buf[TX_BCMC_FIFO] != wlc_hw->xmtfifo_sz[TX_BCMC_FIFO]) {
		i = TX_BCMC_FIFO;
		err = -1;
	}
	if (buf[TX_ATIM_FIFO] != wlc_hw->xmtfifo_sz[TX_ATIM_FIFO]) {
		i = TX_ATIM_FIFO;
		err = -1;
	}
	if (err != 0)
		wiphy_err(wiphy, "wlc_coreinit: txfifo mismatch: ucode size %d"
			  " driver size %d index %d\n", buf[i],
			  wlc_hw->xmtfifo_sz[i], i);

	
	WARN_ON(bcma_read32(core, D11REGOFFS(maccontrol)) == 0xffffffff);

	

	
	brcms_b_write_shm(wlc_hw, M_MBURST_SIZE, MAXTXFRAMEBURST);
	brcms_b_write_shm(wlc_hw, M_MAX_ANTCNT, ANTCNT);

	
	bcma_write32(core, D11REGOFFS(intrcvlazy[0]), (1 << IRL_FC_SHIFT));

	
	brcms_b_mctrl(wlc_hw,
		       (MCTL_INFRA | MCTL_DISCARD_PMQ | MCTL_AP),
		       (MCTL_INFRA | MCTL_DISCARD_PMQ));

	
	bcnint_us = 0x8000 << 10;
	bcma_write32(core, D11REGOFFS(tsf_cfprep),
		     (bcnint_us << CFPREP_CBI_SHIFT));
	bcma_write32(core, D11REGOFFS(tsf_cfpstart), bcnint_us);
	bcma_write32(core, D11REGOFFS(macintstatus), MI_GP1);

	
	bcma_write32(core, D11REGOFFS(intctrlregs[RX_FIFO].intmask),
		     DEF_RXINTMASK);

	
	brcms_b_macphyclk_set(wlc_hw, ON);

	
	wlc->fastpwrup_dly = ai_clkctl_fast_pwrup_delay(wlc_hw->sih);
	bcma_write16(core, D11REGOFFS(scc_fastpwrup_dly), wlc->fastpwrup_dly);

	
	brcms_b_write_shm(wlc_hw, M_MACHW_VER, (u16) wlc_hw->corerev);

	
	brcms_b_write_shm(wlc_hw, M_MACHW_CAP_L,
			   (u16) (wlc_hw->machwcap & 0xffff));
	brcms_b_write_shm(wlc_hw, M_MACHW_CAP_H,
			   (u16) ((wlc_hw->
				      machwcap >> 16) & 0xffff));

	
	bcma_write32(core, D11REGOFFS(objaddr),
		     OBJADDR_SCR_SEL | S_DOT11_SRC_LMT);
	(void)bcma_read32(core, D11REGOFFS(objaddr));
	bcma_write32(core, D11REGOFFS(objdata), wlc_hw->SRL);
	bcma_write32(core, D11REGOFFS(objaddr),
		     OBJADDR_SCR_SEL | S_DOT11_LRC_LMT);
	(void)bcma_read32(core, D11REGOFFS(objaddr));
	bcma_write32(core, D11REGOFFS(objdata), wlc_hw->LRL);

	
	brcms_b_write_shm(wlc_hw, M_SFRMTXCNTFBRTHSD, wlc_hw->SFBL);
	brcms_b_write_shm(wlc_hw, M_LFRMTXCNTFBRTHSD, wlc_hw->LFBL);

	bcma_mask16(core, D11REGOFFS(ifs_ctl), 0x0FFF);
	bcma_write16(core, D11REGOFFS(ifs_aifsn), EDCF_AIFSN_MIN);

	
	for (i = 0; i < NFIFO; i++) {
		if (wlc_hw->di[i])
			dma_txinit(wlc_hw->di[i]);
	}

	
	dma_rxinit(wlc_hw->di[RX_FIFO]);
	dma_rxfill(wlc_hw->di[RX_FIFO]);
}

void
static brcms_b_init(struct brcms_hardware *wlc_hw, u16 chanspec) {
	u32 macintmask;
	bool fastclk;
	struct brcms_c_info *wlc = wlc_hw->wlc;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	
	fastclk = wlc_hw->forcefastclk;
	if (!fastclk)
		brcms_b_clkctl_clk(wlc_hw, CLK_FAST);

	
	macintmask = brcms_intrsoff(wlc->wl);

	
	brcms_c_setxband(wlc_hw, chspec_bandunit(chanspec));
	wlc_phy_chanspec_radio_set(wlc_hw->band->pi, chanspec);

	
	wlc_phy_cal_init(wlc_hw->band->pi);

	
	brcms_b_coreinit(wlc);

	
	brcms_b_bsinit(wlc, chanspec);

	
	brcms_intrsrestore(wlc->wl, macintmask);

	mboolset(wlc_hw->wake_override, BRCMS_WAKE_OVERRIDE_MACSUSPEND);

	wlc_hw->mac_suspend_depth = 1;

	
	if (!fastclk)
		brcms_b_clkctl_clk(wlc_hw, CLK_DYNAMIC);
}

static void brcms_c_set_phy_chanspec(struct brcms_c_info *wlc,
				     u16 chanspec)
{
	
	wlc->chanspec = chanspec;

	
	brcms_c_channel_set_chanspec(wlc->cmi, chanspec, BRCMS_TXPWR_MAX);

	if (wlc->stf->ss_algosel_auto)
		brcms_c_stf_ss_algo_channel_get(wlc, &wlc->stf->ss_algo_channel,
					    chanspec);

	brcms_c_stf_ss_update(wlc, wlc->band);
}

static void
brcms_default_rateset(struct brcms_c_info *wlc, struct brcms_c_rateset *rs)
{
	brcms_c_rateset_default(rs, NULL, wlc->band->phytype,
		wlc->band->bandtype, false, BRCMS_RATE_MASK_FULL,
		(bool) (wlc->pub->_n_enab & SUPPORT_11N),
		brcms_chspec_bw(wlc->default_bss->chanspec),
		wlc->stf->txstreams);
}

static void brcms_c_rate_lookup_init(struct brcms_c_info *wlc,
			      struct brcms_c_rateset *rateset)
{
	u8 rate;
	u8 mandatory;
	u8 cck_basic = 0;
	u8 ofdm_basic = 0;
	u8 *br = wlc->band->basic_rate;
	uint i;

	
	memset(br, 0, BRCM_MAXRATE + 1);

	for (i = 0; i < rateset->count; i++) {
		
		if (!(rateset->rates[i] & BRCMS_RATE_FLAG))
			continue;

		
		rate = (rateset->rates[i] & BRCMS_RATE_MASK);

		if (rate > BRCM_MAXRATE) {
			wiphy_err(wlc->wiphy, "brcms_c_rate_lookup_init: "
				  "invalid rate 0x%X in rate set\n",
				  rateset->rates[i]);
			continue;
		}

		br[rate] = rate;
	}


	for (i = 0; i < wlc->band->hw_rateset.count; i++) {
		rate = wlc->band->hw_rateset.rates[i];

		if (br[rate] != 0) {
			if (is_ofdm_rate(rate))
				ofdm_basic = rate;
			else
				cck_basic = rate;

			continue;
		}


		br[rate] = is_ofdm_rate(rate) ? ofdm_basic : cck_basic;

		if (br[rate] != 0)
			continue;

		if (is_ofdm_rate(rate)) {
			if (rate >= BRCM_RATE_24M)
				mandatory = BRCM_RATE_24M;
			else if (rate >= BRCM_RATE_12M)
				mandatory = BRCM_RATE_12M;
			else
				mandatory = BRCM_RATE_6M;
		} else {
			
			mandatory = rate;
		}

		br[rate] = mandatory;
	}
}

static void brcms_c_bandinit_ordered(struct brcms_c_info *wlc,
				     u16 chanspec)
{
	struct brcms_c_rateset default_rateset;
	uint parkband;
	uint i, band_order[2];

	BCMMSG(wlc->wiphy, "wl%d\n", wlc->pub->unit);
	if (wlc->bandlocked || wlc->pub->_nbands == 1) {
		
		parkband = wlc->band->bandunit;
		band_order[0] = band_order[1] = parkband;
	} else {
		
		parkband = chspec_bandunit(chanspec);

		
		band_order[0] = parkband ^ 1;
		band_order[1] = parkband;
	}

	
	for (i = 0; i < wlc->pub->_nbands; i++) {
		uint j = band_order[i];

		wlc->band = wlc->bandstate[j];

		brcms_default_rateset(wlc, &default_rateset);

		
		brcms_c_rateset_filter(&default_rateset, &wlc->band->hw_rateset,
				   false, BRCMS_RATES_CCK_OFDM, BRCMS_RATE_MASK,
				   (bool) (wlc->pub->_n_enab & SUPPORT_11N));

		
		brcms_c_rate_lookup_init(wlc, &default_rateset);
	}

	
	brcms_c_set_phy_chanspec(wlc, chanspec);
}

void brcms_c_mac_promisc(struct brcms_c_info *wlc, uint filter_flags)
{
	u32 promisc_bits = 0;

	wlc->filter_flags = filter_flags;

	if (filter_flags & (FIF_PROMISC_IN_BSS | FIF_OTHER_BSS))
		promisc_bits |= MCTL_PROMISC;

	if (filter_flags & FIF_BCN_PRBRESP_PROMISC)
		promisc_bits |= MCTL_BCNS_PROMISC;

	if (filter_flags & FIF_FCSFAIL)
		promisc_bits |= MCTL_KEEPBADFCS;

	if (filter_flags & (FIF_CONTROL | FIF_PSPOLL))
		promisc_bits |= MCTL_KEEPCONTROL;

	brcms_b_mctrl(wlc->hw,
		MCTL_PROMISC | MCTL_BCNS_PROMISC |
		MCTL_KEEPCONTROL | MCTL_KEEPBADFCS,
		promisc_bits);
}

static void brcms_c_ucode_mac_upd(struct brcms_c_info *wlc)
{
	if (wlc->home_chanspec == wlc_phy_chanspec_get(wlc->band->pi)) {
		if (wlc->pub->associated) {
			if (BRCMS_PHY_11N_CAP(wlc->band))
				brcms_b_write_shm(wlc->hw,
						M_BCN_TXTSF_OFFSET, 0);
		}
	} else {
		
	}
}

static void brcms_c_write_rate_shm(struct brcms_c_info *wlc, u8 rate,
				   u8 basic_rate)
{
	u8 phy_rate, index;
	u8 basic_phy_rate, basic_index;
	u16 dir_table, basic_table;
	u16 basic_ptr;

	
	dir_table = is_ofdm_rate(basic_rate) ? M_RT_DIRMAP_A : M_RT_DIRMAP_B;

	
	basic_table = is_ofdm_rate(rate) ? M_RT_BBRSMAP_A : M_RT_BBRSMAP_B;

	phy_rate = rate_info[rate] & BRCMS_RATE_MASK;
	basic_phy_rate = rate_info[basic_rate] & BRCMS_RATE_MASK;
	index = phy_rate & 0xf;
	basic_index = basic_phy_rate & 0xf;

	basic_ptr = brcms_b_read_shm(wlc->hw, (dir_table + basic_index * 2));

	brcms_b_write_shm(wlc->hw, (basic_table + index * 2), basic_ptr);
}

static const struct brcms_c_rateset *
brcms_c_rateset_get_hwrs(struct brcms_c_info *wlc)
{
	const struct brcms_c_rateset *rs_dflt;

	if (BRCMS_PHY_11N_CAP(wlc->band)) {
		if (wlc->band->bandtype == BRCM_BAND_5G)
			rs_dflt = &ofdm_mimo_rates;
		else
			rs_dflt = &cck_ofdm_mimo_rates;
	} else if (wlc->band->gmode)
		rs_dflt = &cck_ofdm_rates;
	else
		rs_dflt = &cck_rates;

	return rs_dflt;
}

static void brcms_c_set_ratetable(struct brcms_c_info *wlc)
{
	const struct brcms_c_rateset *rs_dflt;
	struct brcms_c_rateset rs;
	u8 rate, basic_rate;
	uint i;

	rs_dflt = brcms_c_rateset_get_hwrs(wlc);

	brcms_c_rateset_copy(rs_dflt, &rs);
	brcms_c_rateset_mcs_upd(&rs, wlc->stf->txstreams);

	
	for (i = 0; i < rs.count; i++) {
		rate = rs.rates[i] & BRCMS_RATE_MASK;

		basic_rate = brcms_basic_rate(wlc, rate);
		if (basic_rate == 0)
			basic_rate = rs.rates[0] & BRCMS_RATE_MASK;

		brcms_c_write_rate_shm(wlc, rate, basic_rate);
	}
}

static void brcms_c_bsinit(struct brcms_c_info *wlc)
{
	BCMMSG(wlc->wiphy, "wl%d: bandunit %d\n",
		 wlc->pub->unit, wlc->band->bandunit);

	
	brcms_c_set_ratetable(wlc);

	
	brcms_c_ucode_mac_upd(wlc);

	
	brcms_c_antsel_init(wlc->asi);

}

static int
brcms_c_duty_cycle_set(struct brcms_c_info *wlc, int duty_cycle, bool isOFDM,
		   bool writeToShm)
{
	int idle_busy_ratio_x_16 = 0;
	uint offset =
	    isOFDM ? M_TX_IDLE_BUSY_RATIO_X_16_OFDM :
	    M_TX_IDLE_BUSY_RATIO_X_16_CCK;
	if (duty_cycle > 100 || duty_cycle < 0) {
		wiphy_err(wlc->wiphy, "wl%d:  duty cycle value off limit\n",
			  wlc->pub->unit);
		return -EINVAL;
	}
	if (duty_cycle)
		idle_busy_ratio_x_16 = (100 - duty_cycle) * 16 / duty_cycle;
	
	if (writeToShm)
		brcms_b_write_shm(wlc->hw, offset, (u16) idle_busy_ratio_x_16);

	if (isOFDM)
		wlc->tx_duty_cycle_ofdm = (u16) duty_cycle;
	else
		wlc->tx_duty_cycle_cck = (u16) duty_cycle;

	return 0;
}

static void brcms_c_tx_prec_map_init(struct brcms_c_info *wlc)
{
	wlc->tx_prec_map = BRCMS_PREC_BMP_ALL;
	memset(wlc->fifo2prec_map, 0, NFIFO * sizeof(u16));

	wlc->fifo2prec_map[TX_AC_BK_FIFO] = BRCMS_PREC_BMP_AC_BK;
	wlc->fifo2prec_map[TX_AC_BE_FIFO] = BRCMS_PREC_BMP_AC_BE;
	wlc->fifo2prec_map[TX_AC_VI_FIFO] = BRCMS_PREC_BMP_AC_VI;
	wlc->fifo2prec_map[TX_AC_VO_FIFO] = BRCMS_PREC_BMP_AC_VO;
}

static void
brcms_c_txflowcontrol_signal(struct brcms_c_info *wlc,
			     struct brcms_txq_info *qi, bool on, int prio)
{
	
}

static void brcms_c_txflowcontrol_reset(struct brcms_c_info *wlc)
{
	struct brcms_txq_info *qi;

	for (qi = wlc->tx_queues; qi != NULL; qi = qi->next) {
		if (qi->stopped) {
			brcms_c_txflowcontrol_signal(wlc, qi, OFF, ALLPRIO);
			qi->stopped = 0;
		}
	}
}

static void brcms_c_set_ps_ctrl(struct brcms_c_info *wlc)
{
	u32 v1, v2;
	bool hps;
	bool awake_before;

	hps = brcms_c_ps_allowed(wlc);

	BCMMSG(wlc->wiphy, "wl%d: hps %d\n", wlc->pub->unit, hps);

	v1 = bcma_read32(wlc->hw->d11core, D11REGOFFS(maccontrol));
	v2 = MCTL_WAKE;
	if (hps)
		v2 |= MCTL_HPS;

	brcms_b_mctrl(wlc->hw, MCTL_WAKE | MCTL_HPS, v2);

	awake_before = ((v1 & MCTL_WAKE) || ((v1 & MCTL_HPS) == 0));

	if (!awake_before)
		brcms_b_wait_for_wake(wlc->hw);
}

static int brcms_c_set_mac(struct brcms_bss_cfg *bsscfg)
{
	int err = 0;
	struct brcms_c_info *wlc = bsscfg->wlc;

	
	brcms_c_set_addrmatch(wlc, RCM_MAC_OFFSET, bsscfg->cur_etheraddr);

	brcms_c_ampdu_macaddr_upd(wlc);

	return err;
}

static void brcms_c_set_bssid(struct brcms_bss_cfg *bsscfg)
{
	
	brcms_c_set_addrmatch(bsscfg->wlc, RCM_BSSID_OFFSET, bsscfg->BSSID);
}

static void brcms_b_set_shortslot(struct brcms_hardware *wlc_hw, bool shortslot)
{
	wlc_hw->shortslot = shortslot;

	if (wlc_hw->band->bandtype == BRCM_BAND_2G && wlc_hw->up) {
		brcms_c_suspend_mac_and_wait(wlc_hw->wlc);
		brcms_b_update_slot_timing(wlc_hw, shortslot);
		brcms_c_enable_mac(wlc_hw->wlc);
	}
}

static void brcms_c_switch_shortslot(struct brcms_c_info *wlc, bool shortslot)
{
	
	if (wlc->shortslot_override != BRCMS_SHORTSLOT_AUTO)
		shortslot = (wlc->shortslot_override == BRCMS_SHORTSLOT_ON);

	if (wlc->shortslot == shortslot)
		return;

	wlc->shortslot = shortslot;

	brcms_b_set_shortslot(wlc->hw, shortslot);
}

static void brcms_c_set_home_chanspec(struct brcms_c_info *wlc, u16 chanspec)
{
	if (wlc->home_chanspec != chanspec) {
		wlc->home_chanspec = chanspec;

		if (wlc->bsscfg->associated)
			wlc->bsscfg->current_bss->chanspec = chanspec;
	}
}

void
brcms_b_set_chanspec(struct brcms_hardware *wlc_hw, u16 chanspec,
		      bool mute_tx, struct txpwr_limits *txpwr)
{
	uint bandunit;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d: 0x%x\n", wlc_hw->unit, chanspec);

	wlc_hw->chanspec = chanspec;

	
	if (wlc_hw->_nbands > 1) {
		bandunit = chspec_bandunit(chanspec);
		if (wlc_hw->band->bandunit != bandunit) {
			if (wlc_hw->up) {
				wlc_phy_chanspec_radio_set(wlc_hw->
							   bandstate[bandunit]->
							   pi, chanspec);
				brcms_b_setband(wlc_hw, bandunit, chanspec);
			} else {
				brcms_c_setxband(wlc_hw, bandunit);
			}
		}
	}

	wlc_phy_initcal_enable(wlc_hw->band->pi, !mute_tx);

	if (!wlc_hw->up) {
		if (wlc_hw->clk)
			wlc_phy_txpower_limit_set(wlc_hw->band->pi, txpwr,
						  chanspec);
		wlc_phy_chanspec_radio_set(wlc_hw->band->pi, chanspec);
	} else {
		wlc_phy_chanspec_set(wlc_hw->band->pi, chanspec);
		wlc_phy_txpower_limit_set(wlc_hw->band->pi, txpwr, chanspec);

		
		brcms_b_mute(wlc_hw, mute_tx);
	}
}

static void brcms_c_setband(struct brcms_c_info *wlc,
					   uint bandunit)
{
	wlc->band = wlc->bandstate[bandunit];

	if (!wlc->pub->up)
		return;

	
	brcms_c_set_ps_ctrl(wlc);

	
	brcms_c_bsinit(wlc);
}

static void brcms_c_set_chanspec(struct brcms_c_info *wlc, u16 chanspec)
{
	uint bandunit;
	bool switchband = false;
	u16 old_chanspec = wlc->chanspec;

	if (!brcms_c_valid_chanspec_db(wlc->cmi, chanspec)) {
		wiphy_err(wlc->wiphy, "wl%d: %s: Bad channel %d\n",
			  wlc->pub->unit, __func__, CHSPEC_CHANNEL(chanspec));
		return;
	}

	
	if (wlc->pub->_nbands > 1) {
		bandunit = chspec_bandunit(chanspec);
		if (wlc->band->bandunit != bandunit || wlc->bandinit_pending) {
			switchband = true;
			if (wlc->bandlocked) {
				wiphy_err(wlc->wiphy, "wl%d: %s: chspec %d "
					  "band is locked!\n",
					  wlc->pub->unit, __func__,
					  CHSPEC_CHANNEL(chanspec));
				return;
			}
			brcms_c_setband(wlc, bandunit);
		}
	}

	
	brcms_c_set_phy_chanspec(wlc, chanspec);

	
	if (brcms_chspec_bw(old_chanspec) != brcms_chspec_bw(chanspec)) {
		brcms_c_antsel_init(wlc->asi);

		brcms_c_rateset_bw_mcs_filter(&wlc->band->hw_rateset,
			wlc->band->mimo_cap_40 ? brcms_chspec_bw(chanspec) : 0);
	}

	
	brcms_c_ucode_mac_upd(wlc);
}

void brcms_c_beacon_phytxctl_txant_upd(struct brcms_c_info *wlc,
				       u32 bcn_rspec)
{
	u16 phyctl;
	u16 phytxant = wlc->stf->phytxant;
	u16 mask = PHY_TXC_ANT_MASK;

	
	if (BRCMS_PHY_11N_CAP(wlc->band))
		phytxant = brcms_c_stf_phytxchain_sel(wlc, bcn_rspec);

	phyctl = brcms_b_read_shm(wlc->hw, M_BCN_PCTLWD);
	phyctl = (phyctl & ~mask) | phytxant;
	brcms_b_write_shm(wlc->hw, M_BCN_PCTLWD, phyctl);
}

void brcms_c_protection_upd(struct brcms_c_info *wlc, uint idx, int val)
{
	BCMMSG(wlc->wiphy, "idx %d, val %d\n", idx, val);

	switch (idx) {
	case BRCMS_PROT_G_SPEC:
		wlc->protection->_g = (bool) val;
		break;
	case BRCMS_PROT_G_OVR:
		wlc->protection->g_override = (s8) val;
		break;
	case BRCMS_PROT_G_USER:
		wlc->protection->gmode_user = (u8) val;
		break;
	case BRCMS_PROT_OVERLAP:
		wlc->protection->overlap = (s8) val;
		break;
	case BRCMS_PROT_N_USER:
		wlc->protection->nmode_user = (s8) val;
		break;
	case BRCMS_PROT_N_CFG:
		wlc->protection->n_cfg = (s8) val;
		break;
	case BRCMS_PROT_N_CFG_OVR:
		wlc->protection->n_cfg_override = (s8) val;
		break;
	case BRCMS_PROT_N_NONGF:
		wlc->protection->nongf = (bool) val;
		break;
	case BRCMS_PROT_N_NONGF_OVR:
		wlc->protection->nongf_override = (s8) val;
		break;
	case BRCMS_PROT_N_PAM_OVR:
		wlc->protection->n_pam_override = (s8) val;
		break;
	case BRCMS_PROT_N_OBSS:
		wlc->protection->n_obss = (bool) val;
		break;

	default:
		break;
	}

}

static void brcms_c_ht_update_sgi_rx(struct brcms_c_info *wlc, int val)
{
	if (wlc->pub->up) {
		brcms_c_update_beacon(wlc);
		brcms_c_update_probe_resp(wlc, true);
	}
}

static void brcms_c_ht_update_ldpc(struct brcms_c_info *wlc, s8 val)
{
	wlc->stf->ldpc = val;

	if (wlc->pub->up) {
		brcms_c_update_beacon(wlc);
		brcms_c_update_probe_resp(wlc, true);
		wlc_phy_ldpc_override_set(wlc->band->pi, (val ? true : false));
	}
}

void brcms_c_wme_setparams(struct brcms_c_info *wlc, u16 aci,
		       const struct ieee80211_tx_queue_params *params,
		       bool suspend)
{
	int i;
	struct shm_acparams acp_shm;
	u16 *shm_entry;

	
	if (!wlc->clk) {
		wiphy_err(wlc->wiphy, "wl%d: %s : no-clock\n", wlc->pub->unit,
			  __func__);
		return;
	}

	memset((char *)&acp_shm, 0, sizeof(struct shm_acparams));
	
	acp_shm.txop = params->txop;
	
	wlc->edcf_txop[aci & 0x3] = acp_shm.txop =
	    EDCF_TXOP2USEC(acp_shm.txop);
	acp_shm.aifs = (params->aifs & EDCF_AIFSN_MASK);

	if (aci == IEEE80211_AC_VI && acp_shm.txop == 0
	    && acp_shm.aifs < EDCF_AIFSN_MAX)
		acp_shm.aifs++;

	if (acp_shm.aifs < EDCF_AIFSN_MIN
	    || acp_shm.aifs > EDCF_AIFSN_MAX) {
		wiphy_err(wlc->wiphy, "wl%d: edcf_setparams: bad "
			  "aifs %d\n", wlc->pub->unit, acp_shm.aifs);
	} else {
		acp_shm.cwmin = params->cw_min;
		acp_shm.cwmax = params->cw_max;
		acp_shm.cwcur = acp_shm.cwmin;
		acp_shm.bslots =
			bcma_read16(wlc->hw->d11core, D11REGOFFS(tsf_random)) &
			acp_shm.cwcur;
		acp_shm.reggap = acp_shm.bslots + acp_shm.aifs;
		
		acp_shm.status = brcms_b_read_shm(wlc->hw, (M_EDCF_QINFO +
						  wme_ac2fifo[aci] *
						  M_EDCF_QLEN +
						  M_EDCF_STATUS_OFF));
		acp_shm.status |= WME_STATUS_NEWAC;

		
		shm_entry = (u16 *) &acp_shm;
		for (i = 0; i < (int)sizeof(struct shm_acparams); i += 2)
			brcms_b_write_shm(wlc->hw,
					  M_EDCF_QINFO +
					  wme_ac2fifo[aci] * M_EDCF_QLEN + i,
					  *shm_entry++);
	}

	if (suspend) {
		brcms_c_suspend_mac_and_wait(wlc);
		brcms_c_enable_mac(wlc);
	}
}

static void brcms_c_edcf_setparams(struct brcms_c_info *wlc, bool suspend)
{
	u16 aci;
	int i_ac;
	struct ieee80211_tx_queue_params txq_pars;
	static const struct edcf_acparam default_edcf_acparams[] = {
		 {EDCF_AC_BE_ACI_STA, EDCF_AC_BE_ECW_STA, EDCF_AC_BE_TXOP_STA},
		 {EDCF_AC_BK_ACI_STA, EDCF_AC_BK_ECW_STA, EDCF_AC_BK_TXOP_STA},
		 {EDCF_AC_VI_ACI_STA, EDCF_AC_VI_ECW_STA, EDCF_AC_VI_TXOP_STA},
		 {EDCF_AC_VO_ACI_STA, EDCF_AC_VO_ECW_STA, EDCF_AC_VO_TXOP_STA}
	}; 
	const struct edcf_acparam *edcf_acp = &default_edcf_acparams[0];

	for (i_ac = 0; i_ac < IEEE80211_NUM_ACS; i_ac++, edcf_acp++) {
		
		aci = (edcf_acp->ACI & EDCF_ACI_MASK) >> EDCF_ACI_SHIFT;

		
		txq_pars.txop = edcf_acp->TXOP;
		txq_pars.aifs = edcf_acp->ACI;

		
		txq_pars.cw_min = EDCF_ECW2CW(edcf_acp->ECW & EDCF_ECWMIN_MASK);
		
		txq_pars.cw_max = EDCF_ECW2CW((edcf_acp->ECW & EDCF_ECWMAX_MASK)
					    >> EDCF_ECWMAX_SHIFT);
		brcms_c_wme_setparams(wlc, aci, &txq_pars, suspend);
	}

	if (suspend) {
		brcms_c_suspend_mac_and_wait(wlc);
		brcms_c_enable_mac(wlc);
	}
}

static void brcms_c_radio_monitor_start(struct brcms_c_info *wlc)
{
	
	if (wlc->radio_monitor)
		return;

	wlc->radio_monitor = true;
	brcms_b_pllreq(wlc->hw, true, BRCMS_PLLREQ_RADIO_MON);
	brcms_add_timer(wlc->radio_timer, TIMER_INTERVAL_RADIOCHK, true);
}

static bool brcms_c_radio_monitor_stop(struct brcms_c_info *wlc)
{
	if (!wlc->radio_monitor)
		return true;

	wlc->radio_monitor = false;
	brcms_b_pllreq(wlc->hw, false, BRCMS_PLLREQ_RADIO_MON);
	return brcms_del_timer(wlc->radio_timer);
}

static void brcms_c_radio_hwdisable_upd(struct brcms_c_info *wlc)
{
	if (wlc->pub->hw_off)
		return;

	if (brcms_b_radio_read_hwdisabled(wlc->hw))
		mboolset(wlc->pub->radio_disabled, WL_RADIO_HW_DISABLE);
	else
		mboolclr(wlc->pub->radio_disabled, WL_RADIO_HW_DISABLE);
}

bool brcms_c_check_radio_disabled(struct brcms_c_info *wlc)
{
	brcms_c_radio_hwdisable_upd(wlc);

	return mboolisset(wlc->pub->radio_disabled, WL_RADIO_HW_DISABLE) ?
			true : false;
}

static void brcms_c_radio_timer(void *arg)
{
	struct brcms_c_info *wlc = (struct brcms_c_info *) arg;

	if (brcms_deviceremoved(wlc)) {
		wiphy_err(wlc->wiphy, "wl%d: %s: dead chip\n", wlc->pub->unit,
			__func__);
		brcms_down(wlc->wl);
		return;
	}

	brcms_c_radio_hwdisable_upd(wlc);
}

static void brcms_b_watchdog(void *arg)
{
	struct brcms_c_info *wlc = (struct brcms_c_info *) arg;
	struct brcms_hardware *wlc_hw = wlc->hw;

	BCMMSG(wlc->wiphy, "wl%d\n", wlc_hw->unit);

	if (!wlc_hw->up)
		return;

	
	wlc_hw->now++;

	
	brcms_b_fifoerrors(wlc_hw);

	
	dma_rxfill(wlc->hw->di[RX_FIFO]);

	wlc_phy_watchdog(wlc_hw->band->pi);
}

static void brcms_c_watchdog(void *arg)
{
	struct brcms_c_info *wlc = (struct brcms_c_info *) arg;

	BCMMSG(wlc->wiphy, "wl%d\n", wlc->pub->unit);

	if (!wlc->pub->up)
		return;

	if (brcms_deviceremoved(wlc)) {
		wiphy_err(wlc->wiphy, "wl%d: %s: dead chip\n", wlc->pub->unit,
			  __func__);
		brcms_down(wlc->wl);
		return;
	}

	
	wlc->pub->now++;

	brcms_c_radio_hwdisable_upd(wlc);
	
	if (wlc->pub->radio_disabled)
		return;

	brcms_b_watchdog(wlc);

	if ((wlc->pub->now % SW_TIMER_MAC_STAT_UPD) == 0)
		brcms_c_statsupd(wlc);

	if (BRCMS_ISNPHY(wlc->band) &&
	    ((wlc->pub->now - wlc->tempsense_lasttime) >=
	     BRCMS_TEMPSENSE_PERIOD)) {
		wlc->tempsense_lasttime = wlc->pub->now;
		brcms_c_tempsense_upd(wlc);
	}
}

static void brcms_c_watchdog_by_timer(void *arg)
{
	brcms_c_watchdog(arg);
}

static bool brcms_c_timers_init(struct brcms_c_info *wlc, int unit)
{
	wlc->wdtimer = brcms_init_timer(wlc->wl, brcms_c_watchdog_by_timer,
		wlc, "watchdog");
	if (!wlc->wdtimer) {
		wiphy_err(wlc->wiphy, "wl%d:  wl_init_timer for wdtimer "
			  "failed\n", unit);
		goto fail;
	}

	wlc->radio_timer = brcms_init_timer(wlc->wl, brcms_c_radio_timer,
		wlc, "radio");
	if (!wlc->radio_timer) {
		wiphy_err(wlc->wiphy, "wl%d:  wl_init_timer for radio_timer "
			  "failed\n", unit);
		goto fail;
	}

	return true;

 fail:
	return false;
}

static void brcms_c_info_init(struct brcms_c_info *wlc, int unit)
{
	int i;

	
	wlc->chanspec = ch20mhz_chspec(1);

	
	wlc->shortslot = false;
	wlc->shortslot_override = BRCMS_SHORTSLOT_AUTO;

	brcms_c_protection_upd(wlc, BRCMS_PROT_G_OVR, BRCMS_PROTECTION_AUTO);
	brcms_c_protection_upd(wlc, BRCMS_PROT_G_SPEC, false);

	brcms_c_protection_upd(wlc, BRCMS_PROT_N_CFG_OVR,
			       BRCMS_PROTECTION_AUTO);
	brcms_c_protection_upd(wlc, BRCMS_PROT_N_CFG, BRCMS_N_PROTECTION_OFF);
	brcms_c_protection_upd(wlc, BRCMS_PROT_N_NONGF_OVR,
			       BRCMS_PROTECTION_AUTO);
	brcms_c_protection_upd(wlc, BRCMS_PROT_N_NONGF, false);
	brcms_c_protection_upd(wlc, BRCMS_PROT_N_PAM_OVR, AUTO);

	brcms_c_protection_upd(wlc, BRCMS_PROT_OVERLAP,
			       BRCMS_PROTECTION_CTL_OVERLAP);

	
	wlc->include_legacy_erp = true;

	wlc->stf->ant_rx_ovr = ANT_RX_DIV_DEF;
	wlc->stf->txant = ANT_TX_DEF;

	wlc->prb_resp_timeout = BRCMS_PRB_RESP_TIMEOUT;

	wlc->usr_fragthresh = DOT11_DEFAULT_FRAG_LEN;
	for (i = 0; i < NFIFO; i++)
		wlc->fragthresh[i] = DOT11_DEFAULT_FRAG_LEN;
	wlc->RTSThresh = DOT11_DEFAULT_RTS_LEN;

	
	wlc->SFBL = RETRY_SHORT_FB;
	wlc->LFBL = RETRY_LONG_FB;

	
	wlc->SRL = RETRY_SHORT_DEF;
	wlc->LRL = RETRY_LONG_DEF;

	
	wlc->pub->_ampdu = AMPDU_AGG_HOST;
	wlc->pub->bcmerror = 0;
}

static uint brcms_c_attach_module(struct brcms_c_info *wlc)
{
	uint err = 0;
	uint unit;
	unit = wlc->pub->unit;

	wlc->asi = brcms_c_antsel_attach(wlc);
	if (wlc->asi == NULL) {
		wiphy_err(wlc->wiphy, "wl%d: attach: antsel_attach "
			  "failed\n", unit);
		err = 44;
		goto fail;
	}

	wlc->ampdu = brcms_c_ampdu_attach(wlc);
	if (wlc->ampdu == NULL) {
		wiphy_err(wlc->wiphy, "wl%d: attach: ampdu_attach "
			  "failed\n", unit);
		err = 50;
		goto fail;
	}

	if ((brcms_c_stf_attach(wlc) != 0)) {
		wiphy_err(wlc->wiphy, "wl%d: attach: stf_attach "
			  "failed\n", unit);
		err = 68;
		goto fail;
	}
 fail:
	return err;
}

struct brcms_pub *brcms_c_pub(struct brcms_c_info *wlc)
{
	return wlc->pub;
}

static int brcms_b_attach(struct brcms_c_info *wlc, struct bcma_device *core,
			  uint unit, bool piomode)
{
	struct brcms_hardware *wlc_hw;
	char *macaddr = NULL;
	uint err = 0;
	uint j;
	bool wme = false;
	struct shared_phy_params sha_params;
	struct wiphy *wiphy = wlc->wiphy;
	struct pci_dev *pcidev = core->bus->host_pci;

	BCMMSG(wlc->wiphy, "wl%d: vendor 0x%x device 0x%x\n", unit,
	       pcidev->vendor,
	       pcidev->device);

	wme = true;

	wlc_hw = wlc->hw;
	wlc_hw->wlc = wlc;
	wlc_hw->unit = unit;
	wlc_hw->band = wlc_hw->bandstate[0];
	wlc_hw->_piomode = piomode;

	
	brcms_b_info_init(wlc_hw);

	wlc_hw->sih = ai_attach(core->bus);
	if (wlc_hw->sih == NULL) {
		wiphy_err(wiphy, "wl%d: brcms_b_attach: si_attach failed\n",
			  unit);
		err = 11;
		goto fail;
	}

	
	if (!brcms_c_chipmatch(pcidev->vendor, pcidev->device)) {
		wiphy_err(wiphy, "wl%d: brcms_b_attach: Unsupported "
			"vendor/device (0x%x/0x%x)\n",
			 unit, pcidev->vendor, pcidev->device);
		err = 12;
		goto fail;
	}

	wlc_hw->vendorid = pcidev->vendor;
	wlc_hw->deviceid = pcidev->device;

	wlc_hw->d11core = core;
	wlc_hw->corerev = core->id.rev;

	
	if (!brcms_c_isgoodchip(wlc_hw)) {
		err = 13;
		goto fail;
	}

	
	ai_clkctl_init(wlc_hw->sih);

	brcms_b_clkctl_clk(wlc_hw, CLK_FAST);
	brcms_b_corereset(wlc_hw, BRCMS_USE_COREFLAGS);

	if (!brcms_b_validate_chip_access(wlc_hw)) {
		wiphy_err(wiphy, "wl%d: brcms_b_attach: validate_chip_access "
			"failed\n", unit);
		err = 14;
		goto fail;
	}

	
	j = getintvar(wlc_hw->sih, BRCMS_SROM_BOARDREV);
	
	if (j == BOARDREV_PROMOTABLE)
		j = BOARDREV_PROMOTED;
	wlc_hw->boardrev = (u16) j;
	if (!brcms_c_validboardtype(wlc_hw)) {
		wiphy_err(wiphy, "wl%d: brcms_b_attach: Unsupported Broadcom "
			  "board type (0x%x)" " or revision level (0x%x)\n",
			  unit, ai_get_boardtype(wlc_hw->sih),
			  wlc_hw->boardrev);
		err = 15;
		goto fail;
	}
	wlc_hw->sromrev = (u8) getintvar(wlc_hw->sih, BRCMS_SROM_REV);
	wlc_hw->boardflags = (u32) getintvar(wlc_hw->sih,
					     BRCMS_SROM_BOARDFLAGS);
	wlc_hw->boardflags2 = (u32) getintvar(wlc_hw->sih,
					      BRCMS_SROM_BOARDFLAGS2);

	if (wlc_hw->boardflags & BFL_NOPLLDOWN)
		brcms_b_pllreq(wlc_hw, true, BRCMS_PLLREQ_SHARED);

	
	if (wlc_hw->deviceid == BCM43224_D11N_ID ||
	    wlc_hw->deviceid == BCM43224_D11N_ID_VEN1)
		
		wlc_hw->_nbands = 2;
	else
		wlc_hw->_nbands = 1;

	if ((ai_get_chip_id(wlc_hw->sih) == BCM43225_CHIP_ID))
		wlc_hw->_nbands = 1;

	wlc->vendorid = wlc_hw->vendorid;
	wlc->deviceid = wlc_hw->deviceid;
	wlc->pub->sih = wlc_hw->sih;
	wlc->pub->corerev = wlc_hw->corerev;
	wlc->pub->sromrev = wlc_hw->sromrev;
	wlc->pub->boardrev = wlc_hw->boardrev;
	wlc->pub->boardflags = wlc_hw->boardflags;
	wlc->pub->boardflags2 = wlc_hw->boardflags2;
	wlc->pub->_nbands = wlc_hw->_nbands;

	wlc_hw->physhim = wlc_phy_shim_attach(wlc_hw, wlc->wl, wlc);

	if (wlc_hw->physhim == NULL) {
		wiphy_err(wiphy, "wl%d: brcms_b_attach: wlc_phy_shim_attach "
			"failed\n", unit);
		err = 25;
		goto fail;
	}

	
	sha_params.sih = wlc_hw->sih;
	sha_params.physhim = wlc_hw->physhim;
	sha_params.unit = unit;
	sha_params.corerev = wlc_hw->corerev;
	sha_params.vid = wlc_hw->vendorid;
	sha_params.did = wlc_hw->deviceid;
	sha_params.chip = ai_get_chip_id(wlc_hw->sih);
	sha_params.chiprev = ai_get_chiprev(wlc_hw->sih);
	sha_params.chippkg = ai_get_chippkg(wlc_hw->sih);
	sha_params.sromrev = wlc_hw->sromrev;
	sha_params.boardtype = ai_get_boardtype(wlc_hw->sih);
	sha_params.boardrev = wlc_hw->boardrev;
	sha_params.boardflags = wlc_hw->boardflags;
	sha_params.boardflags2 = wlc_hw->boardflags2;

	
	wlc_hw->phy_sh = wlc_phy_shared_attach(&sha_params);
	if (!wlc_hw->phy_sh) {
		err = 16;
		goto fail;
	}

	
	for (j = 0; j < wlc_hw->_nbands; j++) {

		brcms_c_setxband(wlc_hw, j);

		wlc_hw->band->bandunit = j;
		wlc_hw->band->bandtype = j ? BRCM_BAND_5G : BRCM_BAND_2G;
		wlc->band->bandunit = j;
		wlc->band->bandtype = j ? BRCM_BAND_5G : BRCM_BAND_2G;
		wlc->core->coreidx = core->core_index;

		wlc_hw->machwcap = bcma_read32(core, D11REGOFFS(machwcap));
		wlc_hw->machwcap_backup = wlc_hw->machwcap;

		
		wlc_hw->xmtfifo_sz =
		    xmtfifo_sz[(wlc_hw->corerev - XMTFIFOTBL_STARTREV)];

		
		wlc_hw->band->pi =
			wlc_phy_attach(wlc_hw->phy_sh, core,
				       wlc_hw->band->bandtype,
				       wlc->wiphy);
		if (wlc_hw->band->pi == NULL) {
			wiphy_err(wiphy, "wl%d: brcms_b_attach: wlc_phy_"
				  "attach failed\n", unit);
			err = 17;
			goto fail;
		}

		wlc_phy_machwcap_set(wlc_hw->band->pi, wlc_hw->machwcap);

		wlc_phy_get_phyversion(wlc_hw->band->pi, &wlc_hw->band->phytype,
				       &wlc_hw->band->phyrev,
				       &wlc_hw->band->radioid,
				       &wlc_hw->band->radiorev);
		wlc_hw->band->abgphy_encore =
		    wlc_phy_get_encore(wlc_hw->band->pi);
		wlc->band->abgphy_encore = wlc_phy_get_encore(wlc_hw->band->pi);
		wlc_hw->band->core_flags =
		    wlc_phy_get_coreflags(wlc_hw->band->pi);

		
		if (BRCMS_ISNPHY(wlc_hw->band)) {
			if (NCONF_HAS(wlc_hw->band->phyrev))
				goto good_phy;
			else
				goto bad_phy;
		} else if (BRCMS_ISLCNPHY(wlc_hw->band)) {
			if (LCNCONF_HAS(wlc_hw->band->phyrev))
				goto good_phy;
			else
				goto bad_phy;
		} else {
 bad_phy:
			wiphy_err(wiphy, "wl%d: brcms_b_attach: unsupported "
				  "phy type/rev (%d/%d)\n", unit,
				  wlc_hw->band->phytype, wlc_hw->band->phyrev);
			err = 18;
			goto fail;
		}

 good_phy:
		wlc->band->pi = wlc_hw->band->pi;
		wlc->band->phytype = wlc_hw->band->phytype;
		wlc->band->phyrev = wlc_hw->band->phyrev;
		wlc->band->radioid = wlc_hw->band->radioid;
		wlc->band->radiorev = wlc_hw->band->radiorev;

		
		wlc_hw->band->CWmin = APHY_CWMIN;
		wlc_hw->band->CWmax = PHY_CWMAX;

		if (!brcms_b_attach_dmapio(wlc, j, wme)) {
			err = 19;
			goto fail;
		}
	}

	
	brcms_c_coredisable(wlc_hw);

	
	ai_pci_down(wlc_hw->sih);

	
	brcms_b_xtal(wlc_hw, OFF);


	
	macaddr = brcms_c_get_macaddr(wlc_hw);
	if (macaddr == NULL) {
		wiphy_err(wiphy, "wl%d: brcms_b_attach: macaddr not found\n",
			  unit);
		err = 21;
		goto fail;
	}
	if (!mac_pton(macaddr, wlc_hw->etheraddr) ||
	    is_broadcast_ether_addr(wlc_hw->etheraddr) ||
	    is_zero_ether_addr(wlc_hw->etheraddr)) {
		wiphy_err(wiphy, "wl%d: brcms_b_attach: bad macaddr %s\n",
			  unit, macaddr);
		err = 22;
		goto fail;
	}

	BCMMSG(wlc->wiphy, "deviceid 0x%x nbands %d board 0x%x macaddr: %s\n",
	       wlc_hw->deviceid, wlc_hw->_nbands, ai_get_boardtype(wlc_hw->sih),
	       macaddr);

	return err;

 fail:
	wiphy_err(wiphy, "wl%d: brcms_b_attach: failed with err %d\n", unit,
		  err);
	return err;
}

static void brcms_c_attach_antgain_init(struct brcms_c_info *wlc)
{
	uint unit;
	unit = wlc->pub->unit;

	if ((wlc->band->antgain == -1) && (wlc->pub->sromrev == 1)) {
		
		wlc->band->antgain = 8;
	} else if (wlc->band->antgain == -1) {
		wiphy_err(wlc->wiphy, "wl%d: %s: Invalid antennas available in"
			  " srom, using 2dB\n", unit, __func__);
		wlc->band->antgain = 8;
	} else {
		s8 gain, fract;
		gain = wlc->band->antgain & 0x3f;
		gain <<= 2;	
		gain >>= 2;
		fract = (wlc->band->antgain & 0xc0) >> 6;
		wlc->band->antgain = 4 * gain + fract;
	}
}

static bool brcms_c_attach_stf_ant_init(struct brcms_c_info *wlc)
{
	int aa;
	uint unit;
	int bandtype;
	struct si_pub *sih = wlc->hw->sih;

	unit = wlc->pub->unit;
	bandtype = wlc->band->bandtype;

	
	if (bandtype == BRCM_BAND_5G)
		aa = (s8) getintvar(sih, BRCMS_SROM_AA5G);
	else
		aa = (s8) getintvar(sih, BRCMS_SROM_AA2G);

	if ((aa < 1) || (aa > 15)) {
		wiphy_err(wlc->wiphy, "wl%d: %s: Invalid antennas available in"
			  " srom (0x%x), using 3\n", unit, __func__, aa);
		aa = 3;
	}

	
	if (aa == 1) {
		wlc->stf->ant_rx_ovr = ANT_RX_DIV_FORCE_0;
		wlc->stf->txant = ANT_TX_FORCE_0;
	} else if (aa == 2) {
		wlc->stf->ant_rx_ovr = ANT_RX_DIV_FORCE_1;
		wlc->stf->txant = ANT_TX_FORCE_1;
	} else {
	}

	
	if (bandtype == BRCM_BAND_5G)
		wlc->band->antgain = (s8) getintvar(sih, BRCMS_SROM_AG1);
	else
		wlc->band->antgain = (s8) getintvar(sih, BRCMS_SROM_AG0);

	brcms_c_attach_antgain_init(wlc);

	return true;
}

static void brcms_c_bss_default_init(struct brcms_c_info *wlc)
{
	u16 chanspec;
	struct brcms_band *band;
	struct brcms_bss_info *bi = wlc->default_bss;

	
	memset((char *)(bi), 0, sizeof(struct brcms_bss_info));
	bi->beacon_period = BEACON_INTERVAL_DEFAULT;

	chanspec = ch20mhz_chspec(1);
	wlc->home_chanspec = bi->chanspec = chanspec;

	
	band = wlc->band;
	if (wlc->pub->_nbands > 1 &&
	    band->bandunit != chspec_bandunit(chanspec))
		band = wlc->bandstate[OTHERBANDUNIT(wlc)];

	
	brcms_c_rateset_default(&bi->rateset, NULL, band->phytype,
		band->bandtype, false, BRCMS_RATE_MASK_FULL,
		(bool) (wlc->pub->_n_enab & SUPPORT_11N),
		brcms_chspec_bw(chanspec), wlc->stf->txstreams);

	if (wlc->pub->_n_enab & SUPPORT_11N)
		bi->flags |= BRCMS_BSS_HT;
}

static struct brcms_txq_info *brcms_c_txq_alloc(struct brcms_c_info *wlc)
{
	struct brcms_txq_info *qi, *p;

	qi = kzalloc(sizeof(struct brcms_txq_info), GFP_ATOMIC);
	if (qi != NULL) {
		brcmu_pktq_init(&qi->q, BRCMS_PREC_COUNT,
			  2 * BRCMS_DATAHIWAT + PKTQ_LEN_DEFAULT);

		
		p = wlc->tx_queues;
		if (p == NULL) {
			wlc->tx_queues = qi;
		} else {
			while (p->next != NULL)
				p = p->next;
			p->next = qi;
		}
	}
	return qi;
}

static void brcms_c_txq_free(struct brcms_c_info *wlc,
			     struct brcms_txq_info *qi)
{
	struct brcms_txq_info *p;

	if (qi == NULL)
		return;

	
	p = wlc->tx_queues;
	if (p == qi)
		wlc->tx_queues = p->next;
	else {
		while (p != NULL && p->next != qi)
			p = p->next;
		if (p != NULL)
			p->next = p->next->next;
	}

	kfree(qi);
}

static void brcms_c_update_mimo_band_bwcap(struct brcms_c_info *wlc, u8 bwcap)
{
	uint i;
	struct brcms_band *band;

	for (i = 0; i < wlc->pub->_nbands; i++) {
		band = wlc->bandstate[i];
		if (band->bandtype == BRCM_BAND_5G) {
			if ((bwcap == BRCMS_N_BW_40ALL)
			    || (bwcap == BRCMS_N_BW_20IN2G_40IN5G))
				band->mimo_cap_40 = true;
			else
				band->mimo_cap_40 = false;
		} else {
			if (bwcap == BRCMS_N_BW_40ALL)
				band->mimo_cap_40 = true;
			else
				band->mimo_cap_40 = false;
		}
	}
}

static void brcms_c_timers_deinit(struct brcms_c_info *wlc)
{
	
	if (wlc->wdtimer) {
		brcms_free_timer(wlc->wdtimer);
		wlc->wdtimer = NULL;
	}
	if (wlc->radio_timer) {
		brcms_free_timer(wlc->radio_timer);
		wlc->radio_timer = NULL;
	}
}

static void brcms_c_detach_module(struct brcms_c_info *wlc)
{
	if (wlc->asi) {
		brcms_c_antsel_detach(wlc->asi);
		wlc->asi = NULL;
	}

	if (wlc->ampdu) {
		brcms_c_ampdu_detach(wlc->ampdu);
		wlc->ampdu = NULL;
	}

	brcms_c_stf_detach(wlc);
}

static int brcms_b_detach(struct brcms_c_info *wlc)
{
	uint i;
	struct brcms_hw_band *band;
	struct brcms_hardware *wlc_hw = wlc->hw;
	int callbacks;

	callbacks = 0;

	if (wlc_hw->sih) {
		ai_pci_sleep(wlc_hw->sih);
	}

	brcms_b_detach_dmapio(wlc_hw);

	band = wlc_hw->band;
	for (i = 0; i < wlc_hw->_nbands; i++) {
		if (band->pi) {
			
			wlc_phy_detach(band->pi);
			band->pi = NULL;
		}
		band = wlc_hw->bandstate[OTHERBANDUNIT(wlc)];
	}

	
	kfree(wlc_hw->phy_sh);

	wlc_phy_shim_detach(wlc_hw->physhim);

	if (wlc_hw->sih) {
		ai_detach(wlc_hw->sih);
		wlc_hw->sih = NULL;
	}

	return callbacks;

}

uint brcms_c_detach(struct brcms_c_info *wlc)
{
	uint callbacks = 0;

	if (wlc == NULL)
		return 0;

	BCMMSG(wlc->wiphy, "wl%d\n", wlc->pub->unit);

	callbacks += brcms_b_detach(wlc);

	
	if (!brcms_c_radio_monitor_stop(wlc))
		callbacks++;

	brcms_c_channel_mgr_detach(wlc->cmi);

	brcms_c_timers_deinit(wlc);

	brcms_c_detach_module(wlc);


	while (wlc->tx_queues != NULL)
		brcms_c_txq_free(wlc, wlc->tx_queues);

	brcms_c_detach_mfree(wlc);
	return callbacks;
}

static void brcms_c_ap_upd(struct brcms_c_info *wlc)
{
	
	wlc->PLCPHdr_override = BRCMS_PLCP_SHORT;
}

static void brcms_b_hw_up(struct brcms_hardware *wlc_hw)
{
	if (wlc_hw->wlc->pub->hw_up)
		return;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	brcms_b_xtal(wlc_hw, ON);
	ai_clkctl_init(wlc_hw->sih);
	brcms_b_clkctl_clk(wlc_hw, CLK_FAST);

	ai_pci_fixcfg(wlc_hw->sih);


	wlc_phy_por_inform(wlc_hw->band->pi);

	wlc_hw->ucode_loaded = false;
	wlc_hw->wlc->pub->hw_up = true;

	if ((wlc_hw->boardflags & BFL_FEM)
	    && (ai_get_chip_id(wlc_hw->sih) == BCM4313_CHIP_ID)) {
		if (!
		    (wlc_hw->boardrev >= 0x1250
		     && (wlc_hw->boardflags & BFL_FEM_BT)))
			ai_epa_4313war(wlc_hw->sih);
	}
}

static int brcms_b_up_prep(struct brcms_hardware *wlc_hw)
{
	uint coremask;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	brcms_b_xtal(wlc_hw, ON);
	ai_clkctl_init(wlc_hw->sih);
	brcms_b_clkctl_clk(wlc_hw, CLK_FAST);

	coremask = (1 << wlc_hw->wlc->core->coreidx);

	ai_pci_setup(wlc_hw->sih, coremask);

	if (brcms_b_radio_read_hwdisabled(wlc_hw)) {
		
		ai_pci_down(wlc_hw->sih);
		brcms_b_xtal(wlc_hw, OFF);
		return -ENOMEDIUM;
	}

	ai_pci_up(wlc_hw->sih);

	
	brcms_b_corereset(wlc_hw, BRCMS_USE_COREFLAGS);

	return 0;
}

static int brcms_b_up_finish(struct brcms_hardware *wlc_hw)
{
	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	wlc_hw->up = true;
	wlc_phy_hw_state_upd(wlc_hw->band->pi, true);

	
	brcms_b_clkctl_clk(wlc_hw, CLK_DYNAMIC);
	brcms_intrson(wlc_hw->wlc->wl);
	return 0;
}

static void brcms_c_wme_retries_write(struct brcms_c_info *wlc)
{
	int ac;

	
	if (!wlc->clk)
		return;

	for (ac = 0; ac < IEEE80211_NUM_ACS; ac++)
		brcms_b_write_shm(wlc->hw, M_AC_TXLMT_ADDR(ac),
				  wlc->wme_retries[ac]);
}

int brcms_c_up(struct brcms_c_info *wlc)
{
	BCMMSG(wlc->wiphy, "wl%d\n", wlc->pub->unit);

	
	if (wlc->pub->hw_off || brcms_deviceremoved(wlc))
		return -ENOMEDIUM;

	if (!wlc->pub->hw_up) {
		brcms_b_hw_up(wlc->hw);
		wlc->pub->hw_up = true;
	}

	if ((wlc->pub->boardflags & BFL_FEM)
	    && (ai_get_chip_id(wlc->hw->sih) == BCM4313_CHIP_ID)) {
		if (wlc->pub->boardrev >= 0x1250
		    && (wlc->pub->boardflags & BFL_FEM_BT))
			brcms_b_mhf(wlc->hw, MHF5, MHF5_4313_GPIOCTRL,
				MHF5_4313_GPIOCTRL, BRCM_BAND_ALL);
		else
			brcms_b_mhf(wlc->hw, MHF4, MHF4_EXTPA_ENABLE,
				    MHF4_EXTPA_ENABLE, BRCM_BAND_ALL);
	}

	if (!wlc->pub->radio_disabled) {
		int status = brcms_b_up_prep(wlc->hw);
		if (status == -ENOMEDIUM) {
			if (!mboolisset
			    (wlc->pub->radio_disabled, WL_RADIO_HW_DISABLE)) {
				struct brcms_bss_cfg *bsscfg = wlc->bsscfg;
				mboolset(wlc->pub->radio_disabled,
					 WL_RADIO_HW_DISABLE);

				if (bsscfg->enable && bsscfg->BSS)
					wiphy_err(wlc->wiphy, "wl%d: up"
						  ": rfdisable -> "
						  "bsscfg_disable()\n",
						   wlc->pub->unit);
			}
		}
	}

	if (wlc->pub->radio_disabled) {
		brcms_c_radio_monitor_start(wlc);
		return 0;
	}

	
	wlc->clk = true;

	brcms_c_radio_monitor_stop(wlc);

	
	brcms_b_mhf(wlc->hw, MHF1, MHF1_EDCF, MHF1_EDCF, BRCM_BAND_ALL);

	brcms_init(wlc->wl);
	wlc->pub->up = true;

	if (wlc->bandinit_pending) {
		brcms_c_suspend_mac_and_wait(wlc);
		brcms_c_set_chanspec(wlc, wlc->default_bss->chanspec);
		wlc->bandinit_pending = false;
		brcms_c_enable_mac(wlc);
	}

	brcms_b_up_finish(wlc->hw);

	
	brcms_c_wme_retries_write(wlc);

	
	brcms_add_timer(wlc->wdtimer, TIMER_INTERVAL_WATCHDOG, true);
	wlc->WDarmed = true;

	
	brcms_c_stf_phy_txant_upd(wlc);
	
	brcms_c_ht_update_ldpc(wlc, wlc->stf->ldpc);

	return 0;
}

static uint brcms_c_down_del_timer(struct brcms_c_info *wlc)
{
	uint callbacks = 0;

	return callbacks;
}

static int brcms_b_bmac_down_prep(struct brcms_hardware *wlc_hw)
{
	bool dev_gone;
	uint callbacks = 0;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	if (!wlc_hw->up)
		return callbacks;

	dev_gone = brcms_deviceremoved(wlc_hw->wlc);

	
	if (dev_gone)
		wlc_hw->wlc->macintmask = 0;
	else {
		
		brcms_intrsoff(wlc_hw->wlc->wl);

		
		brcms_b_clkctl_clk(wlc_hw, CLK_FAST);
	}
	
	callbacks += wlc_phy_down(wlc_hw->band->pi);

	return callbacks;
}

static int brcms_b_down_finish(struct brcms_hardware *wlc_hw)
{
	uint callbacks = 0;
	bool dev_gone;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);

	if (!wlc_hw->up)
		return callbacks;

	wlc_hw->up = false;
	wlc_phy_hw_state_upd(wlc_hw->band->pi, false);

	dev_gone = brcms_deviceremoved(wlc_hw->wlc);

	if (dev_gone) {
		wlc_hw->sbclk = false;
		wlc_hw->clk = false;
		wlc_phy_hw_clk_state_upd(wlc_hw->band->pi, false);

		
		brcms_c_flushqueues(wlc_hw->wlc);
	} else {

		
		if (bcma_core_is_enabled(wlc_hw->d11core)) {
			if (bcma_read32(wlc_hw->d11core,
					D11REGOFFS(maccontrol)) & MCTL_EN_MAC)
				brcms_c_suspend_mac_and_wait(wlc_hw->wlc);
			callbacks += brcms_reset(wlc_hw->wlc->wl);
			brcms_c_coredisable(wlc_hw);
		}

		
		if (!wlc_hw->noreset) {
			ai_pci_down(wlc_hw->sih);
			brcms_b_xtal(wlc_hw, OFF);
		}
	}

	return callbacks;
}

uint brcms_c_down(struct brcms_c_info *wlc)
{

	uint callbacks = 0;
	int i;
	bool dev_gone = false;
	struct brcms_txq_info *qi;

	BCMMSG(wlc->wiphy, "wl%d\n", wlc->pub->unit);

	
	if (wlc->going_down) {
		wiphy_err(wlc->wiphy, "wl%d: %s: Driver going down so return"
			  "\n", wlc->pub->unit, __func__);
		return 0;
	}
	if (!wlc->pub->up)
		return callbacks;

	wlc->going_down = true;

	callbacks += brcms_b_bmac_down_prep(wlc->hw);

	dev_gone = brcms_deviceremoved(wlc);

	
	for (i = 0; i < BRCMS_MAXMODULES; i++) {
		if (wlc->modulecb[i].down_fn)
			callbacks +=
			    wlc->modulecb[i].down_fn(wlc->modulecb[i].hdl);
	}

	
	if (wlc->WDarmed) {
		if (!brcms_del_timer(wlc->wdtimer))
			callbacks++;
		wlc->WDarmed = false;
	}
	
	callbacks += brcms_c_down_del_timer(wlc);

	wlc->pub->up = false;

	wlc_phy_mute_upd(wlc->band->pi, false, PHY_MUTE_ALL);

	
	brcms_c_txflowcontrol_reset(wlc);

	
	for (qi = wlc->tx_queues; qi != NULL; qi = qi->next)
		brcmu_pktq_flush(&qi->q, true, NULL, NULL);

	callbacks += brcms_b_down_finish(wlc->hw);

	
	wlc->clk = false;

	wlc->going_down = false;
	return callbacks;
}

int brcms_c_set_gmode(struct brcms_c_info *wlc, u8 gmode, bool config)
{
	int ret = 0;
	uint i;
	struct brcms_c_rateset rs;
	
	
	s8 shortslot = BRCMS_SHORTSLOT_AUTO;
	bool shortslot_restrict = false; 
	bool ofdm_basic = false;	
	
	int preamble = BRCMS_PLCP_LONG;
	bool preamble_restrict = false;	
	struct brcms_band *band;

	if ((wlc->pub->_n_enab & SUPPORT_11N) && gmode == GMODE_LEGACY_B)
		return -ENOTSUPP;

	
	if (wlc->band->bandtype == BRCM_BAND_2G)
		band = wlc->band;
	else if ((wlc->pub->_nbands > 1) &&
		 (wlc->bandstate[OTHERBANDUNIT(wlc)]->bandtype == BRCM_BAND_2G))
		band = wlc->bandstate[OTHERBANDUNIT(wlc)];
	else
		return -EINVAL;

	
	if ((brcms_c_channel_locale_flags_in_band(wlc->cmi, band->bandunit) &
	     BRCMS_NO_OFDM) && (gmode != GMODE_LEGACY_B))
		return -EINVAL;

	
	if (config)
		brcms_c_protection_upd(wlc, BRCMS_PROT_G_USER, gmode);

	
	memset(&rs, 0, sizeof(struct brcms_c_rateset));

	switch (gmode) {
	case GMODE_LEGACY_B:
		shortslot = BRCMS_SHORTSLOT_OFF;
		brcms_c_rateset_copy(&gphy_legacy_rates, &rs);

		break;

	case GMODE_LRS:
		break;

	case GMODE_AUTO:
		
		break;

	case GMODE_ONLY:
		ofdm_basic = true;
		preamble = BRCMS_PLCP_SHORT;
		preamble_restrict = true;
		break;

	case GMODE_PERFORMANCE:
		shortslot = BRCMS_SHORTSLOT_ON;
		shortslot_restrict = true;
		ofdm_basic = true;
		preamble = BRCMS_PLCP_SHORT;
		preamble_restrict = true;
		break;

	default:
		
		wiphy_err(wlc->wiphy, "wl%d: %s: invalid gmode %d\n",
			  wlc->pub->unit, __func__, gmode);
		return -ENOTSUPP;
	}

	band->gmode = gmode;

	wlc->shortslot_override = shortslot;

	
	if (!rs.count)
		brcms_c_rateset_copy(&cck_ofdm_rates, &rs);

	if (ofdm_basic) {
		for (i = 0; i < rs.count; i++) {
			if (rs.rates[i] == BRCM_RATE_6M
			    || rs.rates[i] == BRCM_RATE_12M
			    || rs.rates[i] == BRCM_RATE_24M)
				rs.rates[i] |= BRCMS_RATE_FLAG;
		}
	}

	
	wlc->default_bss->rateset.count = rs.count;
	memcpy(wlc->default_bss->rateset.rates, rs.rates,
	       sizeof(wlc->default_bss->rateset.rates));

	return ret;
}

int brcms_c_set_nmode(struct brcms_c_info *wlc)
{
	uint i;
	s32 nmode = AUTO;

	if (wlc->stf->txstreams == WL_11N_3x3)
		nmode = WL_11N_3x3;
	else
		nmode = WL_11N_2x2;

	
	brcms_c_set_gmode(wlc, GMODE_AUTO, true);
	if (nmode == WL_11N_3x3)
		wlc->pub->_n_enab = SUPPORT_HT;
	else
		wlc->pub->_n_enab = SUPPORT_11N;
	wlc->default_bss->flags |= BRCMS_BSS_HT;
	
	brcms_c_rateset_mcs_build(&wlc->default_bss->rateset,
			      wlc->stf->txstreams);
	for (i = 0; i < wlc->pub->_nbands; i++)
		memcpy(wlc->bandstate[i]->hw_rateset.mcs,
		       wlc->default_bss->rateset.mcs, MCSSET_LEN);

	return 0;
}

static int
brcms_c_set_internal_rateset(struct brcms_c_info *wlc,
			     struct brcms_c_rateset *rs_arg)
{
	struct brcms_c_rateset rs, new;
	uint bandunit;

	memcpy(&rs, rs_arg, sizeof(struct brcms_c_rateset));

	
	if ((rs.count == 0) || (rs.count > BRCMS_NUMRATES))
		return -EINVAL;

	
	bandunit = wlc->band->bandunit;
	memcpy(&new, &rs, sizeof(struct brcms_c_rateset));
	if (brcms_c_rate_hwrs_filter_sort_validate
	    (&new, &wlc->bandstate[bandunit]->hw_rateset, true,
	     wlc->stf->txstreams))
		goto good;

	
	if (brcms_is_mband_unlocked(wlc)) {
		bandunit = OTHERBANDUNIT(wlc);
		memcpy(&new, &rs, sizeof(struct brcms_c_rateset));
		if (brcms_c_rate_hwrs_filter_sort_validate(&new,
						       &wlc->
						       bandstate[bandunit]->
						       hw_rateset, true,
						       wlc->stf->txstreams))
			goto good;
	}

	return -EBADE;

 good:
	
	memcpy(&wlc->default_bss->rateset, &new,
	       sizeof(struct brcms_c_rateset));
	memcpy(&wlc->bandstate[bandunit]->defrateset, &new,
	       sizeof(struct brcms_c_rateset));
	return 0;
}

static void brcms_c_ofdm_rateset_war(struct brcms_c_info *wlc)
{
	u8 r;
	bool war = false;

	if (wlc->bsscfg->associated)
		r = wlc->bsscfg->current_bss->rateset.rates[0];
	else
		r = wlc->default_bss->rateset.rates[0];

	wlc_phy_ofdm_rateset_war(wlc->band->pi, war);
}

int brcms_c_set_channel(struct brcms_c_info *wlc, u16 channel)
{
	u16 chspec = ch20mhz_chspec(channel);

	if (channel < 0 || channel > MAXCHANNEL)
		return -EINVAL;

	if (!brcms_c_valid_chanspec_db(wlc->cmi, chspec))
		return -EINVAL;


	if (!wlc->pub->up && brcms_is_mband_unlocked(wlc)) {
		if (wlc->band->bandunit != chspec_bandunit(chspec))
			wlc->bandinit_pending = true;
		else
			wlc->bandinit_pending = false;
	}

	wlc->default_bss->chanspec = chspec;
	if (wlc->pub->up && (wlc_phy_chanspec_get(wlc->band->pi) != chspec)) {
		brcms_c_set_home_chanspec(wlc, chspec);
		brcms_c_suspend_mac_and_wait(wlc);
		brcms_c_set_chanspec(wlc, chspec);
		brcms_c_enable_mac(wlc);
	}
	return 0;
}

int brcms_c_set_rate_limit(struct brcms_c_info *wlc, u16 srl, u16 lrl)
{
	int ac;

	if (srl < 1 || srl > RETRY_SHORT_MAX ||
	    lrl < 1 || lrl > RETRY_SHORT_MAX)
		return -EINVAL;

	wlc->SRL = srl;
	wlc->LRL = lrl;

	brcms_b_retrylimit_upd(wlc->hw, wlc->SRL, wlc->LRL);

	for (ac = 0; ac < IEEE80211_NUM_ACS; ac++) {
		wlc->wme_retries[ac] =	SFIELD(wlc->wme_retries[ac],
					       EDCF_SHORT,  wlc->SRL);
		wlc->wme_retries[ac] =	SFIELD(wlc->wme_retries[ac],
					       EDCF_LONG, wlc->LRL);
	}
	brcms_c_wme_retries_write(wlc);

	return 0;
}

void brcms_c_get_current_rateset(struct brcms_c_info *wlc,
				 struct brcm_rateset *currs)
{
	struct brcms_c_rateset *rs;

	if (wlc->pub->associated)
		rs = &wlc->bsscfg->current_bss->rateset;
	else
		rs = &wlc->default_bss->rateset;

	
	currs->count = rs->count;
	memcpy(&currs->rates, &rs->rates, rs->count);
}

int brcms_c_set_rateset(struct brcms_c_info *wlc, struct brcm_rateset *rs)
{
	struct brcms_c_rateset internal_rs;
	int bcmerror;

	if (rs->count > BRCMS_NUMRATES)
		return -ENOBUFS;

	memset(&internal_rs, 0, sizeof(struct brcms_c_rateset));

	
	internal_rs.count = rs->count;
	memcpy(&internal_rs.rates, &rs->rates, internal_rs.count);

	
	if (wlc->pub->_n_enab & SUPPORT_11N) {
		struct brcms_bss_info *mcsset_bss;
		if (wlc->bsscfg->associated)
			mcsset_bss = wlc->bsscfg->current_bss;
		else
			mcsset_bss = wlc->default_bss;
		memcpy(internal_rs.mcs, &mcsset_bss->rateset.mcs[0],
		       MCSSET_LEN);
	}

	bcmerror = brcms_c_set_internal_rateset(wlc, &internal_rs);
	if (!bcmerror)
		brcms_c_ofdm_rateset_war(wlc);

	return bcmerror;
}

int brcms_c_set_beacon_period(struct brcms_c_info *wlc, u16 period)
{
	if (period < DOT11_MIN_BEACON_PERIOD ||
	    period > DOT11_MAX_BEACON_PERIOD)
		return -EINVAL;

	wlc->default_bss->beacon_period = period;
	return 0;
}

u16 brcms_c_get_phy_type(struct brcms_c_info *wlc, int phyidx)
{
	return wlc->band->phytype;
}

void brcms_c_set_shortslot_override(struct brcms_c_info *wlc, s8 sslot_override)
{
	wlc->shortslot_override = sslot_override;

	if (wlc->band->bandtype == BRCM_BAND_5G)
		return;

	if (wlc->pub->up && wlc->pub->associated) {
		
	} else if (wlc->pub->up) {
		
		brcms_c_switch_shortslot(wlc, false);
	} else {
		if (wlc->shortslot_override == BRCMS_SHORTSLOT_AUTO)
			wlc->shortslot = false;
		else
			wlc->shortslot =
			    (wlc->shortslot_override ==
			     BRCMS_SHORTSLOT_ON);
	}
}

int brcms_c_module_register(struct brcms_pub *pub,
			    const char *name, struct brcms_info *hdl,
			    int (*d_fn)(void *handle))
{
	struct brcms_c_info *wlc = (struct brcms_c_info *) pub->wlc;
	int i;

	
	for (i = 0; i < BRCMS_MAXMODULES; i++) {
		if (wlc->modulecb[i].name[0] == '\0') {
			strncpy(wlc->modulecb[i].name, name,
				sizeof(wlc->modulecb[i].name) - 1);
			wlc->modulecb[i].hdl = hdl;
			wlc->modulecb[i].down_fn = d_fn;
			return 0;
		}
	}

	return -ENOSR;
}

int brcms_c_module_unregister(struct brcms_pub *pub, const char *name,
			      struct brcms_info *hdl)
{
	struct brcms_c_info *wlc = (struct brcms_c_info *) pub->wlc;
	int i;

	if (wlc == NULL)
		return -ENODATA;

	for (i = 0; i < BRCMS_MAXMODULES; i++) {
		if (!strcmp(wlc->modulecb[i].name, name) &&
		    (wlc->modulecb[i].hdl == hdl)) {
			memset(&wlc->modulecb[i], 0, sizeof(struct modulecb));
			return 0;
		}
	}

	
	return -ENODATA;
}

void brcms_c_print_txstatus(struct tx_status *txs)
{
	pr_debug("\ntxpkt (MPDU) Complete\n");

	pr_debug("FrameID: %04x   TxStatus: %04x\n", txs->frameid, txs->status);

	pr_debug("[15:12]  %d  frame attempts\n",
		  (txs->status & TX_STATUS_FRM_RTX_MASK) >>
		 TX_STATUS_FRM_RTX_SHIFT);
	pr_debug(" [11:8]  %d  rts attempts\n",
		 (txs->status & TX_STATUS_RTS_RTX_MASK) >>
		 TX_STATUS_RTS_RTX_SHIFT);
	pr_debug("    [7]  %d  PM mode indicated\n",
		 txs->status & TX_STATUS_PMINDCTD ? 1 : 0);
	pr_debug("    [6]  %d  intermediate status\n",
		 txs->status & TX_STATUS_INTERMEDIATE ? 1 : 0);
	pr_debug("    [5]  %d  AMPDU\n",
		 txs->status & TX_STATUS_AMPDU ? 1 : 0);
	pr_debug("  [4:2]  %d  Frame Suppressed Reason (%s)\n",
		 (txs->status & TX_STATUS_SUPR_MASK) >> TX_STATUS_SUPR_SHIFT,
		 (const char *[]) {
			"None",
			"PMQ Entry",
			"Flush request",
			"Previous frag failure",
			"Channel mismatch",
			"Lifetime Expiry",
			"Underflow"
		 } [(txs->status & TX_STATUS_SUPR_MASK) >>
		    TX_STATUS_SUPR_SHIFT]);
	pr_debug("    [1]  %d  acked\n",
		 txs->status & TX_STATUS_ACK_RCV ? 1 : 0);

	pr_debug("LastTxTime: %04x Seq: %04x PHYTxStatus: %04x RxAckRSSI: %04x RxAckSQ: %04x\n",
		 txs->lasttxtime, txs->sequence, txs->phyerr,
		 (txs->ackphyrxsh & PRXS1_JSSI_MASK) >> PRXS1_JSSI_SHIFT,
		 (txs->ackphyrxsh & PRXS1_SQ_MASK) >> PRXS1_SQ_SHIFT);
}

bool brcms_c_chipmatch(u16 vendor, u16 device)
{
	if (vendor != PCI_VENDOR_ID_BROADCOM) {
		pr_err("unknown vendor id %04x\n", vendor);
		return false;
	}

	if (device == BCM43224_D11N_ID_VEN1)
		return true;
	if ((device == BCM43224_D11N_ID) || (device == BCM43225_D11N2G_ID))
		return true;
	if (device == BCM4313_D11N2G_ID)
		return true;
	if ((device == BCM43236_D11N_ID) || (device == BCM43236_D11N2G_ID))
		return true;

	pr_err("unknown device id %04x\n", device);
	return false;
}

#if defined(DEBUG)
void brcms_c_print_txdesc(struct d11txh *txh)
{
	u16 mtcl = le16_to_cpu(txh->MacTxControlLow);
	u16 mtch = le16_to_cpu(txh->MacTxControlHigh);
	u16 mfc = le16_to_cpu(txh->MacFrameControl);
	u16 tfest = le16_to_cpu(txh->TxFesTimeNormal);
	u16 ptcw = le16_to_cpu(txh->PhyTxControlWord);
	u16 ptcw_1 = le16_to_cpu(txh->PhyTxControlWord_1);
	u16 ptcw_1_Fbr = le16_to_cpu(txh->PhyTxControlWord_1_Fbr);
	u16 ptcw_1_Rts = le16_to_cpu(txh->PhyTxControlWord_1_Rts);
	u16 ptcw_1_FbrRts = le16_to_cpu(txh->PhyTxControlWord_1_FbrRts);
	u16 mainrates = le16_to_cpu(txh->MainRates);
	u16 xtraft = le16_to_cpu(txh->XtraFrameTypes);
	u8 *iv = txh->IV;
	u8 *ra = txh->TxFrameRA;
	u16 tfestfb = le16_to_cpu(txh->TxFesTimeFallback);
	u8 *rtspfb = txh->RTSPLCPFallback;
	u16 rtsdfb = le16_to_cpu(txh->RTSDurFallback);
	u8 *fragpfb = txh->FragPLCPFallback;
	u16 fragdfb = le16_to_cpu(txh->FragDurFallback);
	u16 mmodelen = le16_to_cpu(txh->MModeLen);
	u16 mmodefbrlen = le16_to_cpu(txh->MModeFbrLen);
	u16 tfid = le16_to_cpu(txh->TxFrameID);
	u16 txs = le16_to_cpu(txh->TxStatus);
	u16 mnmpdu = le16_to_cpu(txh->MaxNMpdus);
	u16 mabyte = le16_to_cpu(txh->MaxABytes_MRT);
	u16 mabyte_f = le16_to_cpu(txh->MaxABytes_FBR);
	u16 mmbyte = le16_to_cpu(txh->MinMBytes);

	u8 *rtsph = txh->RTSPhyHeader;
	struct ieee80211_rts rts = txh->rts_frame;

	
	brcmu_dbg_hex_dump(txh, sizeof(struct d11txh) + 48,
			   "Raw TxDesc + plcp header:\n");

	pr_debug("TxCtlLow: %04x ", mtcl);
	pr_debug("TxCtlHigh: %04x ", mtch);
	pr_debug("FC: %04x ", mfc);
	pr_debug("FES Time: %04x\n", tfest);
	pr_debug("PhyCtl: %04x%s ", ptcw,
	       (ptcw & PHY_TXC_SHORT_HDR) ? " short" : "");
	pr_debug("PhyCtl_1: %04x ", ptcw_1);
	pr_debug("PhyCtl_1_Fbr: %04x\n", ptcw_1_Fbr);
	pr_debug("PhyCtl_1_Rts: %04x ", ptcw_1_Rts);
	pr_debug("PhyCtl_1_Fbr_Rts: %04x\n", ptcw_1_FbrRts);
	pr_debug("MainRates: %04x ", mainrates);
	pr_debug("XtraFrameTypes: %04x ", xtraft);
	pr_debug("\n");

	print_hex_dump_bytes("SecIV:", DUMP_PREFIX_OFFSET, iv, sizeof(txh->IV));
	print_hex_dump_bytes("RA:", DUMP_PREFIX_OFFSET,
			     ra, sizeof(txh->TxFrameRA));

	pr_debug("Fb FES Time: %04x ", tfestfb);
	print_hex_dump_bytes("Fb RTS PLCP:", DUMP_PREFIX_OFFSET,
			     rtspfb, sizeof(txh->RTSPLCPFallback));
	pr_debug("RTS DUR: %04x ", rtsdfb);
	print_hex_dump_bytes("PLCP:", DUMP_PREFIX_OFFSET,
			     fragpfb, sizeof(txh->FragPLCPFallback));
	pr_debug("DUR: %04x", fragdfb);
	pr_debug("\n");

	pr_debug("MModeLen: %04x ", mmodelen);
	pr_debug("MModeFbrLen: %04x\n", mmodefbrlen);

	pr_debug("FrameID:     %04x\n", tfid);
	pr_debug("TxStatus:    %04x\n", txs);

	pr_debug("MaxNumMpdu:  %04x\n", mnmpdu);
	pr_debug("MaxAggbyte:  %04x\n", mabyte);
	pr_debug("MaxAggbyte_fb:  %04x\n", mabyte_f);
	pr_debug("MinByte:     %04x\n", mmbyte);

	print_hex_dump_bytes("RTS PLCP:", DUMP_PREFIX_OFFSET,
			     rtsph, sizeof(txh->RTSPhyHeader));
	print_hex_dump_bytes("RTS Frame:", DUMP_PREFIX_OFFSET,
			     (u8 *)&rts, sizeof(txh->rts_frame));
	pr_debug("\n");
}
#endif				

#if defined(DEBUG)
static int
brcms_c_format_flags(const struct brcms_c_bit_desc *bd, u32 flags, char *buf,
		     int len)
{
	int i;
	char *p = buf;
	char hexstr[16];
	int slen = 0, nlen = 0;
	u32 bit;
	const char *name;

	if (len < 2 || !buf)
		return 0;

	buf[0] = '\0';

	for (i = 0; flags != 0; i++) {
		bit = bd[i].bit;
		name = bd[i].name;
		if (bit == 0 && flags != 0) {
			
			snprintf(hexstr, 16, "0x%X", flags);
			name = hexstr;
			flags = 0;	
		} else if ((flags & bit) == 0)
			continue;
		flags &= ~bit;
		nlen = strlen(name);
		slen += nlen;
		
		if (flags != 0)
			slen += 1;
		
		if (len <= slen)
			break;
		
		strncpy(p, name, nlen + 1);
		p += nlen;
		
		if (flags != 0)
			p += snprintf(p, 2, " ");
		len -= slen;
	}

	
	if (flags != 0) {
		if (len < 2)
			p -= 2 - len;	
		p += snprintf(p, 2, ">");
	}

	return (int)(p - buf);
}
#endif				

#if defined(DEBUG)
void brcms_c_print_rxh(struct d11rxhdr *rxh)
{
	u16 len = rxh->RxFrameSize;
	u16 phystatus_0 = rxh->PhyRxStatus_0;
	u16 phystatus_1 = rxh->PhyRxStatus_1;
	u16 phystatus_2 = rxh->PhyRxStatus_2;
	u16 phystatus_3 = rxh->PhyRxStatus_3;
	u16 macstatus1 = rxh->RxStatus1;
	u16 macstatus2 = rxh->RxStatus2;
	char flagstr[64];
	char lenbuf[20];
	static const struct brcms_c_bit_desc macstat_flags[] = {
		{RXS_FCSERR, "FCSErr"},
		{RXS_RESPFRAMETX, "Reply"},
		{RXS_PBPRES, "PADDING"},
		{RXS_DECATMPT, "DeCr"},
		{RXS_DECERR, "DeCrErr"},
		{RXS_BCNSENT, "Bcn"},
		{0, NULL}
	};

	brcmu_dbg_hex_dump(rxh, sizeof(struct d11rxhdr), "Raw RxDesc:\n");

	brcms_c_format_flags(macstat_flags, macstatus1, flagstr, 64);

	snprintf(lenbuf, sizeof(lenbuf), "0x%x", len);

	pr_debug("RxFrameSize:     %6s (%d)%s\n", lenbuf, len,
	       (rxh->PhyRxStatus_0 & PRXS0_SHORTH) ? " short preamble" : "");
	pr_debug("RxPHYStatus:     %04x %04x %04x %04x\n",
	       phystatus_0, phystatus_1, phystatus_2, phystatus_3);
	pr_debug("RxMACStatus:     %x %s\n", macstatus1, flagstr);
	pr_debug("RXMACaggtype:    %x\n",
	       (macstatus2 & RXS_AGGTYPE_MASK));
	pr_debug("RxTSFTime:       %04x\n", rxh->RxTSFTime);
}
#endif				

u16 brcms_b_rate_shm_offset(struct brcms_hardware *wlc_hw, u8 rate)
{
	u16 table_ptr;
	u8 phy_rate, index;

	
	if (is_ofdm_rate(rate))
		table_ptr = M_RT_DIRMAP_A;
	else
		table_ptr = M_RT_DIRMAP_B;

	phy_rate = rate_info[rate] & BRCMS_RATE_MASK;
	index = phy_rate & 0xf;

	return 2 * brcms_b_read_shm(wlc_hw, table_ptr + (index * 2));
}

static bool
brcms_c_prec_enq_head(struct brcms_c_info *wlc, struct pktq *q,
		      struct sk_buff *pkt, int prec, bool head)
{
	struct sk_buff *p;
	int eprec = -1;		

	
	if (pktq_pfull(q, prec))
		eprec = prec;
	else if (pktq_full(q)) {
		p = brcmu_pktq_peek_tail(q, &eprec);
		if (eprec > prec) {
			wiphy_err(wlc->wiphy, "%s: Failing: eprec %d > prec %d"
				  "\n", __func__, eprec, prec);
			return false;
		}
	}

	
	if (eprec >= 0) {
		bool discard_oldest;

		discard_oldest = ac_bitmap_tst(0, eprec);

		
		if (eprec == prec && !discard_oldest) {
			wiphy_err(wlc->wiphy, "%s: No where to go, prec == %d"
				  "\n", __func__, prec);
			return false;
		}

		
		p = discard_oldest ? brcmu_pktq_pdeq(q, eprec) :
			brcmu_pktq_pdeq_tail(q, eprec);
		brcmu_pkt_buf_free_skb(p);
	}

	
	if (head)
		p = brcmu_pktq_penq_head(q, prec, pkt);
	else
		p = brcmu_pktq_penq(q, prec, pkt);

	return true;
}

static bool brcms_c_prec_enq(struct brcms_c_info *wlc, struct pktq *q,
		      struct sk_buff *pkt, int prec)
{
	return brcms_c_prec_enq_head(wlc, q, pkt, prec, false);
}

void brcms_c_txq_enq(struct brcms_c_info *wlc, struct scb *scb,
		     struct sk_buff *sdu, uint prec)
{
	struct brcms_txq_info *qi = wlc->pkt_queue;	
	struct pktq *q = &qi->q;
	int prio;

	prio = sdu->priority;

	if (!brcms_c_prec_enq(wlc, q, sdu, prec)) {
		brcmu_pkt_buf_free_skb(sdu);
	}
}

static inline u16
bcmc_fid_generate(struct brcms_c_info *wlc, struct brcms_bss_cfg *bsscfg,
		  struct d11txh *txh)
{
	u16 frameid;

	frameid = le16_to_cpu(txh->TxFrameID) & ~(TXFID_SEQ_MASK |
						  TXFID_QUEUE_MASK);
	frameid |=
	    (((wlc->
	       mc_fid_counter++) << TXFID_SEQ_SHIFT) & TXFID_SEQ_MASK) |
	    TX_BCMC_FIFO;

	return frameid;
}

static uint
brcms_c_calc_ack_time(struct brcms_c_info *wlc, u32 rspec,
		      u8 preamble_type)
{
	uint dur = 0;

	BCMMSG(wlc->wiphy, "wl%d: rspec 0x%x, preamble_type %d\n",
		wlc->pub->unit, rspec, preamble_type);
	rspec = brcms_basic_rate(wlc, rspec);
	
	dur =
	    brcms_c_calc_frame_time(wlc, rspec, preamble_type,
				(DOT11_ACK_LEN + FCS_LEN));
	return dur;
}

static uint
brcms_c_calc_cts_time(struct brcms_c_info *wlc, u32 rspec,
		      u8 preamble_type)
{
	BCMMSG(wlc->wiphy, "wl%d: ratespec 0x%x, preamble_type %d\n",
		wlc->pub->unit, rspec, preamble_type);
	return brcms_c_calc_ack_time(wlc, rspec, preamble_type);
}

static uint
brcms_c_calc_ba_time(struct brcms_c_info *wlc, u32 rspec,
		     u8 preamble_type)
{
	BCMMSG(wlc->wiphy, "wl%d: rspec 0x%x, "
		 "preamble_type %d\n", wlc->pub->unit, rspec, preamble_type);
	rspec = brcms_basic_rate(wlc, rspec);
	
	return brcms_c_calc_frame_time(wlc, rspec, preamble_type,
				   (DOT11_BA_LEN + DOT11_BA_BITMAP_LEN +
				    FCS_LEN));
}

static u16
brcms_c_compute_frame_dur(struct brcms_c_info *wlc, u32 rate,
		      u8 preamble_type, uint next_frag_len)
{
	u16 dur, sifs;

	sifs = get_sifs(wlc->band);

	dur = sifs;
	dur += (u16) brcms_c_calc_ack_time(wlc, rate, preamble_type);

	if (next_frag_len) {
		
		dur *= 2;
		
		dur += sifs;
		dur +=
		    (u16) brcms_c_calc_frame_time(wlc, rate, preamble_type,
						 next_frag_len);
	}
	return dur;
}

static uint
brcms_c_calc_frame_len(struct brcms_c_info *wlc, u32 ratespec,
		   u8 preamble_type, uint dur)
{
	uint nsyms, mac_len, Ndps, kNdps;
	uint rate = rspec2rate(ratespec);

	BCMMSG(wlc->wiphy, "wl%d: rspec 0x%x, preamble_type %d, dur %d\n",
		 wlc->pub->unit, ratespec, preamble_type, dur);

	if (is_mcs_rate(ratespec)) {
		uint mcs = ratespec & RSPEC_RATE_MASK;
		int tot_streams = mcs_2_txstreams(mcs) + rspec_stc(ratespec);
		dur -= PREN_PREAMBLE + (tot_streams * PREN_PREAMBLE_EXT);
		
		if (wlc->band->bandtype == BRCM_BAND_2G)
			dur -= DOT11_OFDM_SIGNAL_EXTENSION;
		
		kNdps =	mcs_2_rate(mcs, rspec_is40mhz(ratespec),
				   rspec_issgi(ratespec)) * 4;
		nsyms = dur / APHY_SYMBOL_TIME;
		mac_len =
		    ((nsyms * kNdps) -
		     ((APHY_SERVICE_NBITS + APHY_TAIL_NBITS) * 1000)) / 8000;
	} else if (is_ofdm_rate(ratespec)) {
		dur -= APHY_PREAMBLE_TIME;
		dur -= APHY_SIGNAL_TIME;
		
		Ndps = rate * 2;
		nsyms = dur / APHY_SYMBOL_TIME;
		mac_len =
		    ((nsyms * Ndps) -
		     (APHY_SERVICE_NBITS + APHY_TAIL_NBITS)) / 8;
	} else {
		if (preamble_type & BRCMS_SHORT_PREAMBLE)
			dur -= BPHY_PLCP_SHORT_TIME;
		else
			dur -= BPHY_PLCP_TIME;
		mac_len = dur * rate;
		
		mac_len = mac_len / 8 / 2;
	}
	return mac_len;
}

static bool brcms_c_valid_rate(struct brcms_c_info *wlc, u32 rspec, int band,
		    bool verbose)
{
	struct brcms_c_rateset *hw_rateset;
	uint i;

	if ((band == BRCM_BAND_AUTO) || (band == wlc->band->bandtype))
		hw_rateset = &wlc->band->hw_rateset;
	else if (wlc->pub->_nbands > 1)
		hw_rateset = &wlc->bandstate[OTHERBANDUNIT(wlc)]->hw_rateset;
	else
		
		return false;

	
	if (is_mcs_rate(rspec)) {
		if ((rspec & RSPEC_RATE_MASK) >= MCS_TABLE_SIZE)
			goto error;

		return isset(hw_rateset->mcs, (rspec & RSPEC_RATE_MASK));
	}

	for (i = 0; i < hw_rateset->count; i++)
		if (hw_rateset->rates[i] == rspec2rate(rspec))
			return true;
 error:
	if (verbose)
		wiphy_err(wlc->wiphy, "wl%d: valid_rate: rate spec 0x%x "
			  "not in hw_rateset\n", wlc->pub->unit, rspec);

	return false;
}

static u32
mac80211_wlc_set_nrate(struct brcms_c_info *wlc, struct brcms_band *cur_band,
		       u32 int_val)
{
	u8 stf = (int_val & NRATE_STF_MASK) >> NRATE_STF_SHIFT;
	u8 rate = int_val & NRATE_RATE_MASK;
	u32 rspec;
	bool ismcs = ((int_val & NRATE_MCS_INUSE) == NRATE_MCS_INUSE);
	bool issgi = ((int_val & NRATE_SGI_MASK) >> NRATE_SGI_SHIFT);
	bool override_mcs_only = ((int_val & NRATE_OVERRIDE_MCS_ONLY)
				  == NRATE_OVERRIDE_MCS_ONLY);
	int bcmerror = 0;

	if (!ismcs)
		return (u32) rate;

	
	if ((wlc->pub->_n_enab & SUPPORT_11N) && ismcs) {
		
		if (stf > PHY_TXC1_MODE_SDM) {
			wiphy_err(wlc->wiphy, "wl%d: %s: Invalid stf\n",
				  wlc->pub->unit, __func__);
			bcmerror = -EINVAL;
			goto done;
		}

		
		if (rate == 32) {
			if (!CHSPEC_IS40(wlc->home_chanspec) ||
			    ((stf != PHY_TXC1_MODE_SISO)
			     && (stf != PHY_TXC1_MODE_CDD))) {
				wiphy_err(wlc->wiphy, "wl%d: %s: Invalid mcs "
					  "32\n", wlc->pub->unit, __func__);
				bcmerror = -EINVAL;
				goto done;
			}
			
		} else if (rate > HIGHEST_SINGLE_STREAM_MCS) {
			
			if (stf != PHY_TXC1_MODE_SDM) {
				BCMMSG(wlc->wiphy, "wl%d: enabling "
				       "SDM mode for mcs %d\n",
				       wlc->pub->unit, rate);
				stf = PHY_TXC1_MODE_SDM;
			}
		} else {
			if ((stf > PHY_TXC1_MODE_STBC) ||
			    (!BRCMS_STBC_CAP_PHY(wlc)
			     && (stf == PHY_TXC1_MODE_STBC))) {
				wiphy_err(wlc->wiphy, "wl%d: %s: Invalid STBC"
					  "\n", wlc->pub->unit, __func__);
				bcmerror = -EINVAL;
				goto done;
			}
		}
	} else if (is_ofdm_rate(rate)) {
		if ((stf != PHY_TXC1_MODE_CDD) && (stf != PHY_TXC1_MODE_SISO)) {
			wiphy_err(wlc->wiphy, "wl%d: %s: Invalid OFDM\n",
				  wlc->pub->unit, __func__);
			bcmerror = -EINVAL;
			goto done;
		}
	} else if (is_cck_rate(rate)) {
		if ((cur_band->bandtype != BRCM_BAND_2G)
		    || (stf != PHY_TXC1_MODE_SISO)) {
			wiphy_err(wlc->wiphy, "wl%d: %s: Invalid CCK\n",
				  wlc->pub->unit, __func__);
			bcmerror = -EINVAL;
			goto done;
		}
	} else {
		wiphy_err(wlc->wiphy, "wl%d: %s: Unknown rate type\n",
			  wlc->pub->unit, __func__);
		bcmerror = -EINVAL;
		goto done;
	}
	
	if ((stf != PHY_TXC1_MODE_SISO) && (wlc->stf->txstreams == 1)) {
		wiphy_err(wlc->wiphy, "wl%d: %s: SISO antenna but !SISO "
			  "request\n", wlc->pub->unit, __func__);
		bcmerror = -EINVAL;
		goto done;
	}

	rspec = rate;
	if (ismcs) {
		rspec |= RSPEC_MIMORATE;
		
		if (stf == PHY_TXC1_MODE_STBC) {
			u8 stc;
			stc = 1;	
			rspec |= (stc << RSPEC_STC_SHIFT);
		}
	}

	rspec |= (stf << RSPEC_STF_SHIFT);

	if (override_mcs_only)
		rspec |= RSPEC_OVERRIDE_MCS_ONLY;

	if (issgi)
		rspec |= RSPEC_SHORT_GI;

	if ((rate != 0)
	    && !brcms_c_valid_rate(wlc, rspec, cur_band->bandtype, true))
		return rate;

	return rspec;
done:
	return rate;
}


static void brcms_c_cck_plcp_set(struct brcms_c_info *wlc, int rate_500,
			     uint length, u8 *plcp)
{
	u16 usec = 0;
	u8 le = 0;

	switch (rate_500) {
	case BRCM_RATE_1M:
		usec = length << 3;
		break;
	case BRCM_RATE_2M:
		usec = length << 2;
		break;
	case BRCM_RATE_5M5:
		usec = (length << 4) / 11;
		if ((length << 4) - (usec * 11) > 0)
			usec++;
		break;
	case BRCM_RATE_11M:
		usec = (length << 3) / 11;
		if ((length << 3) - (usec * 11) > 0) {
			usec++;
			if ((usec * 11) - (length << 3) >= 8)
				le = D11B_PLCP_SIGNAL_LE;
		}
		break;

	default:
		wiphy_err(wlc->wiphy,
			  "brcms_c_cck_plcp_set: unsupported rate %d\n",
			  rate_500);
		rate_500 = BRCM_RATE_1M;
		usec = length << 3;
		break;
	}
	
	plcp[0] = rate_500 * 5;	
	
	plcp[1] = (u8) (le | D11B_PLCP_SIGNAL_LOCKED);
	
	plcp[2] = usec & 0xff;
	plcp[3] = (usec >> 8) & 0xff;
	
	plcp[4] = 0;
	plcp[5] = 0;
}

static void brcms_c_compute_mimo_plcp(u32 rspec, uint length, u8 *plcp)
{
	u8 mcs = (u8) (rspec & RSPEC_RATE_MASK);
	plcp[0] = mcs;
	if (rspec_is40mhz(rspec) || (mcs == 32))
		plcp[0] |= MIMO_PLCP_40MHZ;
	BRCMS_SET_MIMO_PLCP_LEN(plcp, length);
	plcp[3] = rspec_mimoplcp3(rspec); 
	plcp[3] |= 0x7; 
	plcp[4] = 0; 
	plcp[5] = 0;
}

static void
brcms_c_compute_ofdm_plcp(u32 rspec, u32 length, u8 *plcp)
{
	u8 rate_signal;
	u32 tmp = 0;
	int rate = rspec2rate(rspec);

	rate_signal = rate_info[rate] & BRCMS_RATE_MASK;
	memset(plcp, 0, D11_PHY_HDR_LEN);
	D11A_PHY_HDR_SRATE((struct ofdm_phy_hdr *) plcp, rate_signal);

	tmp = (length & 0xfff) << 5;
	plcp[2] |= (tmp >> 16) & 0xff;
	plcp[1] |= (tmp >> 8) & 0xff;
	plcp[0] |= tmp & 0xff;
}

static void brcms_c_compute_cck_plcp(struct brcms_c_info *wlc, u32 rspec,
				 uint length, u8 *plcp)
{
	int rate = rspec2rate(rspec);

	brcms_c_cck_plcp_set(wlc, rate, length, plcp);
}

static void
brcms_c_compute_plcp(struct brcms_c_info *wlc, u32 rspec,
		     uint length, u8 *plcp)
{
	if (is_mcs_rate(rspec))
		brcms_c_compute_mimo_plcp(rspec, length, plcp);
	else if (is_ofdm_rate(rspec))
		brcms_c_compute_ofdm_plcp(rspec, length, plcp);
	else
		brcms_c_compute_cck_plcp(wlc, rspec, length, plcp);
}

u16
brcms_c_compute_rtscts_dur(struct brcms_c_info *wlc, bool cts_only,
			   u32 rts_rate,
			   u32 frame_rate, u8 rts_preamble_type,
			   u8 frame_preamble_type, uint frame_len, bool ba)
{
	u16 dur, sifs;

	sifs = get_sifs(wlc->band);

	if (!cts_only) {
		
		dur = 3 * sifs;
		dur +=
		    (u16) brcms_c_calc_cts_time(wlc, rts_rate,
					       rts_preamble_type);
	} else {
		
		dur = 2 * sifs;
	}

	dur +=
	    (u16) brcms_c_calc_frame_time(wlc, frame_rate, frame_preamble_type,
					 frame_len);
	if (ba)
		dur +=
		    (u16) brcms_c_calc_ba_time(wlc, frame_rate,
					      BRCMS_SHORT_PREAMBLE);
	else
		dur +=
		    (u16) brcms_c_calc_ack_time(wlc, frame_rate,
					       frame_preamble_type);
	return dur;
}

static u16 brcms_c_phytxctl1_calc(struct brcms_c_info *wlc, u32 rspec)
{
	u16 phyctl1 = 0;
	u16 bw;

	if (BRCMS_ISLCNPHY(wlc->band)) {
		bw = PHY_TXC1_BW_20MHZ;
	} else {
		bw = rspec_get_bw(rspec);
		
		if (bw < PHY_TXC1_BW_20MHZ) {
			wiphy_err(wlc->wiphy, "phytxctl1_calc: bw %d is "
				  "not supported yet, set to 20L\n", bw);
			bw = PHY_TXC1_BW_20MHZ;
		}
	}

	if (is_mcs_rate(rspec)) {
		uint mcs = rspec & RSPEC_RATE_MASK;

		
		phyctl1 = rspec_phytxbyte2(rspec);
		
		phyctl1 |= (mcs_table[mcs].tx_phy_ctl3 << 8);
	} else if (is_cck_rate(rspec) && !BRCMS_ISLCNPHY(wlc->band)
		   && !BRCMS_ISSSLPNPHY(wlc->band)) {
		
		phyctl1 = (bw | (rspec_stf(rspec) << PHY_TXC1_MODE_SHIFT));
	} else {		
		s16 phycfg;
		
		phycfg = brcms_c_rate_legacy_phyctl(rspec2rate(rspec));
		if (phycfg == -1) {
			wiphy_err(wlc->wiphy, "phytxctl1_calc: wrong "
				  "legacy OFDM/CCK rate\n");
			phycfg = 0;
		}
		
		phyctl1 =
		    (bw | (phycfg << 8) |
		     (rspec_stf(rspec) << PHY_TXC1_MODE_SHIFT));
	}
	return phyctl1;
}

static u16
brcms_c_d11hdrs_mac80211(struct brcms_c_info *wlc, struct ieee80211_hw *hw,
		     struct sk_buff *p, struct scb *scb, uint frag,
		     uint nfrags, uint queue, uint next_frag_len)
{
	struct ieee80211_hdr *h;
	struct d11txh *txh;
	u8 *plcp, plcp_fallback[D11_PHY_HDR_LEN];
	int len, phylen, rts_phylen;
	u16 mch, phyctl, xfts, mainrates;
	u16 seq = 0, mcl = 0, status = 0, frameid = 0;
	u32 rspec[2] = { BRCM_RATE_1M, BRCM_RATE_1M };
	u32 rts_rspec[2] = { BRCM_RATE_1M, BRCM_RATE_1M };
	bool use_rts = false;
	bool use_cts = false;
	bool use_rifs = false;
	bool short_preamble[2] = { false, false };
	u8 preamble_type[2] = { BRCMS_LONG_PREAMBLE, BRCMS_LONG_PREAMBLE };
	u8 rts_preamble_type[2] = { BRCMS_LONG_PREAMBLE, BRCMS_LONG_PREAMBLE };
	u8 *rts_plcp, rts_plcp_fallback[D11_PHY_HDR_LEN];
	struct ieee80211_rts *rts = NULL;
	bool qos;
	uint ac;
	bool hwtkmic = false;
	u16 mimo_ctlchbw = PHY_TXC1_BW_20MHZ;
#define ANTCFG_NONE 0xFF
	u8 antcfg = ANTCFG_NONE;
	u8 fbantcfg = ANTCFG_NONE;
	uint phyctl1_stf = 0;
	u16 durid = 0;
	struct ieee80211_tx_rate *txrate[2];
	int k;
	struct ieee80211_tx_info *tx_info;
	bool is_mcs;
	u16 mimo_txbw;
	u8 mimo_preamble_type;

	
	h = (struct ieee80211_hdr *)(p->data);
	qos = ieee80211_is_data_qos(h->frame_control);

	
	len = p->len;
	phylen = len + FCS_LEN;

	
	tx_info = IEEE80211_SKB_CB(p);

	
	plcp = skb_push(p, D11_PHY_HDR_LEN);

	
	txh = (struct d11txh *) skb_push(p, D11_TXH_LEN);
	memset(txh, 0, D11_TXH_LEN);

	
	if (tx_info->flags & IEEE80211_TX_CTL_ASSIGN_SEQ) {
		
		if (queue == TX_BCMC_FIFO) {
			wiphy_err(wlc->wiphy, "wl%d: %s: ASSERT queue == "
				  "TX_BCMC!\n", wlc->pub->unit, __func__);
			frameid = bcmc_fid_generate(wlc, NULL, txh);
		} else {
			
			if (tx_info->flags & IEEE80211_TX_CTL_FIRST_FRAGMENT)
				scb->seqnum[p->priority]++;

			
			seq = le16_to_cpu(h->seq_ctrl) & FRAGNUM_MASK;
			seq |= (scb->seqnum[p->priority] << SEQNUM_SHIFT);
			h->seq_ctrl = cpu_to_le16(seq);

			frameid = ((seq << TXFID_SEQ_SHIFT) & TXFID_SEQ_MASK) |
			    (queue & TXFID_QUEUE_MASK);
		}
	}
	frameid |= queue & TXFID_QUEUE_MASK;

	
	if (ieee80211_is_beacon(h->frame_control))
		mcl |= TXC_IGNOREPMQ;

	txrate[0] = tx_info->control.rates;
	txrate[1] = txrate[0] + 1;

	if (txrate[1]->idx < 0)
		txrate[1] = txrate[0];

	for (k = 0; k < hw->max_rates; k++) {
		is_mcs = txrate[k]->flags & IEEE80211_TX_RC_MCS ? true : false;
		if (!is_mcs) {
			if ((txrate[k]->idx >= 0)
			    && (txrate[k]->idx <
				hw->wiphy->bands[tx_info->band]->n_bitrates)) {
				rspec[k] =
				    hw->wiphy->bands[tx_info->band]->
				    bitrates[txrate[k]->idx].hw_value;
				short_preamble[k] =
				    txrate[k]->
				    flags & IEEE80211_TX_RC_USE_SHORT_PREAMBLE ?
				    true : false;
			} else {
				rspec[k] = BRCM_RATE_1M;
			}
		} else {
			rspec[k] = mac80211_wlc_set_nrate(wlc, wlc->band,
					NRATE_MCS_INUSE | txrate[k]->idx);
		}

		use_rts |=
		    txrate[k]->
		    flags & IEEE80211_TX_RC_USE_RTS_CTS ? true : false;
		use_cts |=
		    txrate[k]->
		    flags & IEEE80211_TX_RC_USE_CTS_PROTECT ? true : false;


		if (!rspec_active(rspec[k])) {
			rspec[k] = BRCM_RATE_1M;
		} else {
			if (!is_multicast_ether_addr(h->addr1)) {
				
				brcms_c_antsel_antcfg_get(wlc->asi, false,
					false, 0, 0, &antcfg, &fbantcfg);
			}
		}
	}

	phyctl1_stf = wlc->stf->ss_opmode;

	if (wlc->pub->_n_enab & SUPPORT_11N) {
		for (k = 0; k < hw->max_rates; k++) {
			if (((is_mcs_rate(rspec[k]) &&
			      is_single_stream(rspec[k] & RSPEC_RATE_MASK)) ||
			     is_ofdm_rate(rspec[k]))
			    && ((rspec[k] & RSPEC_OVERRIDE_MCS_ONLY)
				|| !(rspec[k] & RSPEC_OVERRIDE))) {
				rspec[k] &= ~(RSPEC_STF_MASK | RSPEC_STC_MASK);

				
				if (is_mcs_rate(rspec[k])
				    && BRCMS_STF_SS_STBC_TX(wlc, scb)) {
					u8 stc;

					
					stc = 1;
					rspec[k] |= (PHY_TXC1_MODE_STBC <<
							RSPEC_STF_SHIFT) |
						    (stc << RSPEC_STC_SHIFT);
				} else
					rspec[k] |=
					    (phyctl1_stf << RSPEC_STF_SHIFT);
			}

			if (brcms_chspec_bw(wlc->chanspec) == BRCMS_40_MHZ) {
				
				mimo_ctlchbw = mimo_txbw =
				   CHSPEC_SB_UPPER(wlc_phy_chanspec_get(
								 wlc->band->pi))
				   ? PHY_TXC1_BW_20MHZ_UP : PHY_TXC1_BW_20MHZ;

				if (is_mcs_rate(rspec[k])) {
					
					if ((rspec[k] & RSPEC_RATE_MASK)
					    == 32) {
						mimo_txbw =
						    PHY_TXC1_BW_40MHZ_DUP;
						
					} else if (wlc->mimo_40txbw != AUTO)
						mimo_txbw = wlc->mimo_40txbw;
					
					else if (scb->flags & SCB_IS40)
						mimo_txbw = PHY_TXC1_BW_40MHZ;
				} else if (is_ofdm_rate(rspec[k])) {
					if (wlc->ofdm_40txbw != AUTO)
						mimo_txbw = wlc->ofdm_40txbw;
				} else if (wlc->cck_40txbw != AUTO) {
					mimo_txbw = wlc->cck_40txbw;
				}
			} else {
				if ((rspec[k] & RSPEC_RATE_MASK) == 32)
					
					rspec[k] = RSPEC_MIMORATE;

				mimo_txbw = PHY_TXC1_BW_20MHZ;
			}

			
			rspec[k] &= ~RSPEC_BW_MASK;
			if ((k == 0) || ((k > 0) && is_mcs_rate(rspec[k])))
				rspec[k] |= (mimo_txbw << RSPEC_BW_SHIFT);
			else
				rspec[k] |= (mimo_ctlchbw << RSPEC_BW_SHIFT);

			
			rspec[k] &= ~RSPEC_SHORT_GI;

			mimo_preamble_type = BRCMS_MM_PREAMBLE;
			if (txrate[k]->flags & IEEE80211_TX_RC_GREEN_FIELD)
				mimo_preamble_type = BRCMS_GF_PREAMBLE;

			if ((txrate[k]->flags & IEEE80211_TX_RC_MCS)
			    && (!is_mcs_rate(rspec[k]))) {
				wiphy_err(wlc->wiphy, "wl%d: %s: IEEE80211_TX_"
					  "RC_MCS != is_mcs_rate(rspec)\n",
					  wlc->pub->unit, __func__);
			}

			if (is_mcs_rate(rspec[k])) {
				preamble_type[k] = mimo_preamble_type;

				if ((rspec[k] & RSPEC_SHORT_GI)
				    && is_single_stream(rspec[k] &
							RSPEC_RATE_MASK))
					preamble_type[k] = BRCMS_MM_PREAMBLE;
			}

			
			if (!is_mcs_rate(rspec[0])
			    && (tx_info->control.rates[0].
				flags & IEEE80211_TX_RC_USE_SHORT_PREAMBLE))
				preamble_type[k] = BRCMS_SHORT_PREAMBLE;
		}
	} else {
		for (k = 0; k < hw->max_rates; k++) {
			
			rspec[k] &= ~RSPEC_BW_MASK;
			rspec[k] |= (PHY_TXC1_BW_20MHZ << RSPEC_BW_SHIFT);

			
			if (BRCMS_ISNPHY(wlc->band) && is_ofdm_rate(rspec[k])) {
				rspec[k] &= ~RSPEC_STF_MASK;
				rspec[k] |= phyctl1_stf << RSPEC_STF_SHIFT;
			}
		}
	}

	
	txrate[0]->count = 0;
	txrate[1]->count = 0;

	
	if ((ieee80211_is_data(h->frame_control) ||
	    ieee80211_is_mgmt(h->frame_control)) &&
	    (phylen > wlc->RTSThresh) && !is_multicast_ether_addr(h->addr1))
		use_rts = true;

	brcms_c_compute_plcp(wlc, rspec[0], phylen, plcp);
	brcms_c_compute_plcp(wlc, rspec[1], phylen, plcp_fallback);
	memcpy(&txh->FragPLCPFallback,
	       plcp_fallback, sizeof(txh->FragPLCPFallback));

	
	if (is_cck_rate(rspec[1])) {
		txh->FragPLCPFallback[4] = phylen & 0xff;
		txh->FragPLCPFallback[5] = (phylen & 0xff00) >> 8;
	}

	
	mainrates = is_ofdm_rate(rspec[0]) ?
			D11A_PHY_HDR_GRATE((struct ofdm_phy_hdr *) plcp) :
			plcp[0];

	
	if (!ieee80211_is_pspoll(h->frame_control) &&
	    !is_multicast_ether_addr(h->addr1) && !use_rifs) {
		durid =
		    brcms_c_compute_frame_dur(wlc, rspec[0], preamble_type[0],
					  next_frag_len);
		h->duration_id = cpu_to_le16(durid);
	} else if (use_rifs) {
		
		durid =
		    (u16) brcms_c_calc_frame_time(wlc, rspec[0],
						 preamble_type[0],
						 DOT11_MAX_FRAG_LEN);
		durid += RIFS_11N_TIME;
		h->duration_id = cpu_to_le16(durid);
	}

	
	if (ieee80211_is_pspoll(h->frame_control))
		txh->FragDurFallback = h->duration_id;
	else if (is_multicast_ether_addr(h->addr1) || use_rifs)
		txh->FragDurFallback = 0;
	else {
		durid = brcms_c_compute_frame_dur(wlc, rspec[1],
					      preamble_type[1], next_frag_len);
		txh->FragDurFallback = cpu_to_le16(durid);
	}

	
	if (frag == 0)
		mcl |= TXC_STARTMSDU;

	if (!is_multicast_ether_addr(h->addr1))
		mcl |= TXC_IMMEDACK;

	if (wlc->band->bandtype == BRCM_BAND_5G)
		mcl |= TXC_FREQBAND_5G;

	if (CHSPEC_IS40(wlc_phy_chanspec_get(wlc->band->pi)))
		mcl |= TXC_BW_40;

	
	if (hwtkmic)
		mcl |= TXC_AMIC;

	txh->MacTxControlLow = cpu_to_le16(mcl);

	
	mch = 0;

	
	if ((preamble_type[1] == BRCMS_SHORT_PREAMBLE) ||
	    (preamble_type[1] == BRCMS_GF_PREAMBLE)) {
		if (rspec2rate(rspec[1]) != BRCM_RATE_1M)
			mch |= TXC_PREAMBLE_DATA_FB_SHORT;
	}

	
	memcpy(&txh->MacFrameControl, &h->frame_control, sizeof(u16));
	txh->TxFesTimeNormal = cpu_to_le16(0);

	txh->TxFesTimeFallback = cpu_to_le16(0);

	
	memcpy(&txh->TxFrameRA, &h->addr1, ETH_ALEN);

	
	txh->TxFrameID = cpu_to_le16(frameid);

	txh->TxStatus = cpu_to_le16(status);

	txh->MaxNMpdus = cpu_to_le16(0);
	txh->MaxABytes_MRT = cpu_to_le16(0);
	txh->MaxABytes_FBR = cpu_to_le16(0);
	txh->MinMBytes = cpu_to_le16(0);

	
	if (use_rts || use_cts) {
		if (use_rts && use_cts)
			use_cts = false;

		for (k = 0; k < 2; k++) {
			rts_rspec[k] = brcms_c_rspec_to_rts_rspec(wlc, rspec[k],
							      false,
							      mimo_ctlchbw);
		}

		if (!is_ofdm_rate(rts_rspec[0]) &&
		    !((rspec2rate(rts_rspec[0]) == BRCM_RATE_1M) ||
		      (wlc->PLCPHdr_override == BRCMS_PLCP_LONG))) {
			rts_preamble_type[0] = BRCMS_SHORT_PREAMBLE;
			mch |= TXC_PREAMBLE_RTS_MAIN_SHORT;
		}

		if (!is_ofdm_rate(rts_rspec[1]) &&
		    !((rspec2rate(rts_rspec[1]) == BRCM_RATE_1M) ||
		      (wlc->PLCPHdr_override == BRCMS_PLCP_LONG))) {
			rts_preamble_type[1] = BRCMS_SHORT_PREAMBLE;
			mch |= TXC_PREAMBLE_RTS_FB_SHORT;
		}

		
		if (use_cts) {
			txh->MacTxControlLow |= cpu_to_le16(TXC_SENDCTS);
		} else {
			txh->MacTxControlLow |= cpu_to_le16(TXC_SENDRTS);
			txh->MacTxControlLow |= cpu_to_le16(TXC_LONGFRAME);
		}

		
		rts_plcp = txh->RTSPhyHeader;
		if (use_cts)
			rts_phylen = DOT11_CTS_LEN + FCS_LEN;
		else
			rts_phylen = DOT11_RTS_LEN + FCS_LEN;

		brcms_c_compute_plcp(wlc, rts_rspec[0], rts_phylen, rts_plcp);

		
		brcms_c_compute_plcp(wlc, rts_rspec[1], rts_phylen,
				 rts_plcp_fallback);
		memcpy(&txh->RTSPLCPFallback, rts_plcp_fallback,
		       sizeof(txh->RTSPLCPFallback));

		
		rts = (struct ieee80211_rts *)&txh->rts_frame;

		durid = brcms_c_compute_rtscts_dur(wlc, use_cts, rts_rspec[0],
					       rspec[0], rts_preamble_type[0],
					       preamble_type[0], phylen, false);
		rts->duration = cpu_to_le16(durid);
		
		durid = brcms_c_compute_rtscts_dur(wlc, use_cts,
					       rts_rspec[1], rspec[1],
					       rts_preamble_type[1],
					       preamble_type[1], phylen, false);
		txh->RTSDurFallback = cpu_to_le16(durid);

		if (use_cts) {
			rts->frame_control = cpu_to_le16(IEEE80211_FTYPE_CTL |
							 IEEE80211_STYPE_CTS);

			memcpy(&rts->ra, &h->addr2, ETH_ALEN);
		} else {
			rts->frame_control = cpu_to_le16(IEEE80211_FTYPE_CTL |
							 IEEE80211_STYPE_RTS);

			memcpy(&rts->ra, &h->addr1, 2 * ETH_ALEN);
		}

		mainrates |= (is_ofdm_rate(rts_rspec[0]) ?
				D11A_PHY_HDR_GRATE(
					(struct ofdm_phy_hdr *) rts_plcp) :
				rts_plcp[0]) << 8;
	} else {
		memset((char *)txh->RTSPhyHeader, 0, D11_PHY_HDR_LEN);
		memset((char *)&txh->rts_frame, 0,
			sizeof(struct ieee80211_rts));
		memset((char *)txh->RTSPLCPFallback, 0,
		      sizeof(txh->RTSPLCPFallback));
		txh->RTSDurFallback = 0;
	}

#ifdef SUPPORT_40MHZ
	
	if ((tx_info->flags & IEEE80211_TX_CTL_AMPDU) && is_mcs_rate(rspec))
		txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM] =
		   brcm_c_ampdu_null_delim_cnt(wlc->ampdu, scb, rspec, phylen);

#endif

	txh->MacTxControlHigh = cpu_to_le16(mch);

	txh->MainRates = cpu_to_le16(mainrates);

	
	xfts = frametype(rspec[1], wlc->mimoft);
	xfts |= (frametype(rts_rspec[0], wlc->mimoft) << XFTS_RTS_FT_SHIFT);
	xfts |= (frametype(rts_rspec[1], wlc->mimoft) << XFTS_FBRRTS_FT_SHIFT);
	xfts |= CHSPEC_CHANNEL(wlc_phy_chanspec_get(wlc->band->pi)) <<
							     XFTS_CHANNEL_SHIFT;
	txh->XtraFrameTypes = cpu_to_le16(xfts);

	
	phyctl = frametype(rspec[0], wlc->mimoft);
	if ((preamble_type[0] == BRCMS_SHORT_PREAMBLE) ||
	    (preamble_type[0] == BRCMS_GF_PREAMBLE)) {
		if (rspec2rate(rspec[0]) != BRCM_RATE_1M)
			phyctl |= PHY_TXC_SHORT_HDR;
	}

	
	phyctl |= brcms_c_stf_d11hdrs_phyctl_txant(wlc, rspec[0]);
	txh->PhyTxControlWord = cpu_to_le16(phyctl);

	
	if (BRCMS_PHY_11N_CAP(wlc->band)) {
		u16 phyctl1 = 0;

		phyctl1 = brcms_c_phytxctl1_calc(wlc, rspec[0]);
		txh->PhyTxControlWord_1 = cpu_to_le16(phyctl1);
		phyctl1 = brcms_c_phytxctl1_calc(wlc, rspec[1]);
		txh->PhyTxControlWord_1_Fbr = cpu_to_le16(phyctl1);

		if (use_rts || use_cts) {
			phyctl1 = brcms_c_phytxctl1_calc(wlc, rts_rspec[0]);
			txh->PhyTxControlWord_1_Rts = cpu_to_le16(phyctl1);
			phyctl1 = brcms_c_phytxctl1_calc(wlc, rts_rspec[1]);
			txh->PhyTxControlWord_1_FbrRts = cpu_to_le16(phyctl1);
		}

		if (is_mcs_rate(rspec[0]) &&
		    (preamble_type[0] == BRCMS_MM_PREAMBLE)) {
			u16 mmodelen =
			    brcms_c_calc_lsig_len(wlc, rspec[0], phylen);
			txh->MModeLen = cpu_to_le16(mmodelen);
		}

		if (is_mcs_rate(rspec[1]) &&
		    (preamble_type[1] == BRCMS_MM_PREAMBLE)) {
			u16 mmodefbrlen =
			    brcms_c_calc_lsig_len(wlc, rspec[1], phylen);
			txh->MModeFbrLen = cpu_to_le16(mmodefbrlen);
		}
	}

	ac = skb_get_queue_mapping(p);
	if ((scb->flags & SCB_WMECAP) && qos && wlc->edcf_txop[ac]) {
		uint frag_dur, dur, dur_fallback;

		
		if (!(tx_info->flags & IEEE80211_TX_CTL_AMPDU) && frag == 0) {
			frag_dur =
			    brcms_c_calc_frame_time(wlc, rspec[0],
					preamble_type[0], phylen);

			if (rts) {
				
				dur =
				    brcms_c_calc_cts_time(wlc, rts_rspec[0],
						      rts_preamble_type[0]);
				dur_fallback =
				    brcms_c_calc_cts_time(wlc, rts_rspec[1],
						      rts_preamble_type[1]);
				
				dur += le16_to_cpu(rts->duration);
				dur_fallback +=
					le16_to_cpu(txh->RTSDurFallback);
			} else if (use_rifs) {
				dur = frag_dur;
				dur_fallback = 0;
			} else {
				
				dur = frag_dur;
				dur +=
				    brcms_c_compute_frame_dur(wlc, rspec[0],
							  preamble_type[0], 0);

				dur_fallback =
				    brcms_c_calc_frame_time(wlc, rspec[1],
							preamble_type[1],
							phylen);
				dur_fallback +=
				    brcms_c_compute_frame_dur(wlc, rspec[1],
							  preamble_type[1], 0);
			}
			
			txh->TxFesTimeNormal = cpu_to_le16((u16) dur);
			txh->TxFesTimeFallback =
				cpu_to_le16((u16) dur_fallback);

			if (wlc->edcf_txop[ac] >= (dur - frag_dur)) {
				uint newfragthresh;

				newfragthresh =
				    brcms_c_calc_frame_len(wlc,
					rspec[0], preamble_type[0],
					(wlc->edcf_txop[ac] -
						(dur - frag_dur)));
				
				if (newfragthresh < DOT11_MIN_FRAG_LEN)
					newfragthresh =
					    DOT11_MIN_FRAG_LEN;
				else if (newfragthresh >
					 wlc->usr_fragthresh)
					newfragthresh =
					    wlc->usr_fragthresh;
				
				if (wlc->fragthresh[queue] !=
				    (u16) newfragthresh)
					wlc->fragthresh[queue] =
					    (u16) newfragthresh;
			} else {
				wiphy_err(wlc->wiphy, "wl%d: %s txop invalid "
					  "for rate %d\n",
					  wlc->pub->unit, fifo_names[queue],
					  rspec2rate(rspec[0]));
			}

			if (dur > wlc->edcf_txop[ac])
				wiphy_err(wlc->wiphy, "wl%d: %s: %s txop "
					  "exceeded phylen %d/%d dur %d/%d\n",
					  wlc->pub->unit, __func__,
					  fifo_names[queue],
					  phylen, wlc->fragthresh[queue],
					  dur, wlc->edcf_txop[ac]);
		}
	}

	return 0;
}

void brcms_c_sendpkt_mac80211(struct brcms_c_info *wlc, struct sk_buff *sdu,
			      struct ieee80211_hw *hw)
{
	u8 prio;
	uint fifo;
	struct scb *scb = &wlc->pri_scb;
	struct ieee80211_hdr *d11_header = (struct ieee80211_hdr *)(sdu->data);

	prio = ieee80211_is_data(d11_header->frame_control) ? sdu->priority :
		MAXPRIO;
	fifo = prio2fifo[prio];
	if (brcms_c_d11hdrs_mac80211(wlc, hw, sdu, scb, 0, 1, fifo, 0))
		return;
	brcms_c_txq_enq(wlc, scb, sdu, BRCMS_PRIO_TO_PREC(prio));
	brcms_c_send_q(wlc);
}

void brcms_c_send_q(struct brcms_c_info *wlc)
{
	struct sk_buff *pkt[DOT11_MAXNUMFRAGS];
	int prec;
	u16 prec_map;
	int err = 0, i, count;
	uint fifo;
	struct brcms_txq_info *qi = wlc->pkt_queue;
	struct pktq *q = &qi->q;
	struct ieee80211_tx_info *tx_info;

	prec_map = wlc->tx_prec_map;

	while (prec_map && (pkt[0] = brcmu_pktq_mdeq(q, prec_map, &prec))) {
		tx_info = IEEE80211_SKB_CB(pkt[0]);
		if (tx_info->flags & IEEE80211_TX_CTL_AMPDU) {
			err = brcms_c_sendampdu(wlc->ampdu, qi, pkt, prec);
		} else {
			count = 1;
			err = brcms_c_prep_pdu(wlc, pkt[0], &fifo);
			if (!err) {
				for (i = 0; i < count; i++)
					brcms_c_txfifo(wlc, fifo, pkt[i], true,
						       1);
			}
		}

		if (err == -EBUSY) {
			brcmu_pktq_penq_head(q, prec, pkt[0]);
			if (prec_map == wlc->tx_prec_map)
				break;
			prec_map = wlc->tx_prec_map;
		}
	}
}

void
brcms_c_txfifo(struct brcms_c_info *wlc, uint fifo, struct sk_buff *p,
	       bool commit, s8 txpktpend)
{
	u16 frameid = INVALIDFID;
	struct d11txh *txh;

	txh = (struct d11txh *) (p->data);

	if (fifo == TX_BCMC_FIFO)
		frameid = le16_to_cpu(txh->TxFrameID);

	if (commit) {
		wlc->core->txpktpend[fifo] += txpktpend;
		BCMMSG(wlc->wiphy, "pktpend inc %d to %d\n",
			 txpktpend, wlc->core->txpktpend[fifo]);
	}

	
	if (frameid != INVALIDFID) {
		brcms_b_write_shm(wlc->hw, M_BCMC_FID, frameid);
	}

	if (dma_txfast(wlc->hw->di[fifo], p, commit) < 0)
		wiphy_err(wlc->wiphy, "txfifo: fatal, toss frames !!!\n");
}

u32
brcms_c_rspec_to_rts_rspec(struct brcms_c_info *wlc, u32 rspec,
			   bool use_rspec, u16 mimo_ctlchbw)
{
	u32 rts_rspec = 0;

	if (use_rspec)
		
		rts_rspec = rspec;
	else if (wlc->band->gmode && wlc->protection->_g && !is_cck_rate(rspec))
		rts_rspec = brcms_basic_rate(wlc, BRCM_RATE_11M);
	else
		rts_rspec = brcms_basic_rate(wlc, rspec);

	if (BRCMS_PHY_11N_CAP(wlc->band)) {
		
		rts_rspec &= ~RSPEC_BW_MASK;

		if (rspec_is40mhz(rspec) && !is_cck_rate(rts_rspec))
			rts_rspec |= (PHY_TXC1_BW_40MHZ_DUP << RSPEC_BW_SHIFT);
		else
			rts_rspec |= (mimo_ctlchbw << RSPEC_BW_SHIFT);

		
		if (is_ofdm_rate(rts_rspec)) {
			rts_rspec &= ~RSPEC_STF_MASK;
			rts_rspec |= (wlc->stf->ss_opmode << RSPEC_STF_SHIFT);
		}
	}
	return rts_rspec;
}

void
brcms_c_txfifo_complete(struct brcms_c_info *wlc, uint fifo, s8 txpktpend)
{
	wlc->core->txpktpend[fifo] -= txpktpend;
	BCMMSG(wlc->wiphy, "pktpend dec %d to %d\n", txpktpend,
	       wlc->core->txpktpend[fifo]);

	
	wlc->tx_prec_map |= wlc->fifo2prec_map[fifo];

	
}

static void brcms_c_bcn_li_upd(struct brcms_c_info *wlc)
{
	
	if (wlc->bcn_li_dtim == 1)
		brcms_b_write_shm(wlc->hw, M_BCN_LI, 0);
	else
		brcms_b_write_shm(wlc->hw, M_BCN_LI,
			      (wlc->bcn_li_dtim << 8) | wlc->bcn_li_bcn);
}

static void
brcms_b_read_tsf(struct brcms_hardware *wlc_hw, u32 *tsf_l_ptr,
		  u32 *tsf_h_ptr)
{
	struct bcma_device *core = wlc_hw->d11core;

	
	*tsf_l_ptr = bcma_read32(core, D11REGOFFS(tsf_timerlow));
	*tsf_h_ptr = bcma_read32(core, D11REGOFFS(tsf_timerhigh));
}

static u64 brcms_c_recover_tsf64(struct brcms_c_info *wlc,
				 struct d11rxhdr *rxh)
{
	u32 tsf_h, tsf_l;
	u16 rx_tsf_0_15, rx_tsf_16_31;

	brcms_b_read_tsf(wlc->hw, &tsf_l, &tsf_h);

	rx_tsf_16_31 = (u16)(tsf_l >> 16);
	rx_tsf_0_15 = rxh->RxTSFTime;

	if ((u16)tsf_l < rx_tsf_0_15) {
		rx_tsf_16_31 -= 1;
		if (rx_tsf_16_31 == 0xffff)
			tsf_h -= 1;
	}

	return ((u64)tsf_h << 32) | (((u32)rx_tsf_16_31 << 16) + rx_tsf_0_15);
}

static void
prep_mac80211_status(struct brcms_c_info *wlc, struct d11rxhdr *rxh,
		     struct sk_buff *p,
		     struct ieee80211_rx_status *rx_status)
{
	int preamble;
	int channel;
	u32 rspec;
	unsigned char *plcp;

	
	rx_status->mactime = brcms_c_recover_tsf64(wlc, rxh);
	rx_status->flag |= RX_FLAG_MACTIME_MPDU;

	channel = BRCMS_CHAN_CHANNEL(rxh->RxChan);

	if (channel > 14) {
		rx_status->band = IEEE80211_BAND_5GHZ;
		rx_status->freq = ieee80211_ofdm_chan_to_freq(
					WF_CHAN_FACTOR_5_G/2, channel);

	} else {
		rx_status->band = IEEE80211_BAND_2GHZ;
		rx_status->freq = ieee80211_dsss_chan_to_freq(channel);
	}

	rx_status->signal = wlc_phy_rssi_compute(wlc->hw->band->pi, rxh);

	
	
	rx_status->antenna =
		(rxh->PhyRxStatus_0 & PRXS0_RXANT_UPSUBBAND) ? 1 : 0;

	plcp = p->data;

	rspec = brcms_c_compute_rspec(rxh, plcp);
	if (is_mcs_rate(rspec)) {
		rx_status->rate_idx = rspec & RSPEC_RATE_MASK;
		rx_status->flag |= RX_FLAG_HT;
		if (rspec_is40mhz(rspec))
			rx_status->flag |= RX_FLAG_40MHZ;
	} else {
		switch (rspec2rate(rspec)) {
		case BRCM_RATE_1M:
			rx_status->rate_idx = 0;
			break;
		case BRCM_RATE_2M:
			rx_status->rate_idx = 1;
			break;
		case BRCM_RATE_5M5:
			rx_status->rate_idx = 2;
			break;
		case BRCM_RATE_11M:
			rx_status->rate_idx = 3;
			break;
		case BRCM_RATE_6M:
			rx_status->rate_idx = 4;
			break;
		case BRCM_RATE_9M:
			rx_status->rate_idx = 5;
			break;
		case BRCM_RATE_12M:
			rx_status->rate_idx = 6;
			break;
		case BRCM_RATE_18M:
			rx_status->rate_idx = 7;
			break;
		case BRCM_RATE_24M:
			rx_status->rate_idx = 8;
			break;
		case BRCM_RATE_36M:
			rx_status->rate_idx = 9;
			break;
		case BRCM_RATE_48M:
			rx_status->rate_idx = 10;
			break;
		case BRCM_RATE_54M:
			rx_status->rate_idx = 11;
			break;
		default:
			wiphy_err(wlc->wiphy, "%s: Unknown rate\n", __func__);
		}

		if (rx_status->band == IEEE80211_BAND_5GHZ)
			rx_status->rate_idx -= BRCMS_LEGACY_5G_RATE_OFFSET;

		
		preamble = 0;
		if (is_cck_rate(rspec)) {
			if (rxh->PhyRxStatus_0 & PRXS0_SHORTH)
				rx_status->flag |= RX_FLAG_SHORTPRE;
		} else if (is_ofdm_rate(rspec)) {
			rx_status->flag |= RX_FLAG_SHORTPRE;
		} else {
			wiphy_err(wlc->wiphy, "%s: Unknown modulation\n",
				  __func__);
		}
	}

	if (plcp3_issgi(plcp[3]))
		rx_status->flag |= RX_FLAG_SHORT_GI;

	if (rxh->RxStatus1 & RXS_DECERR) {
		rx_status->flag |= RX_FLAG_FAILED_PLCP_CRC;
		wiphy_err(wlc->wiphy, "%s:  RX_FLAG_FAILED_PLCP_CRC\n",
			  __func__);
	}
	if (rxh->RxStatus1 & RXS_FCSERR) {
		rx_status->flag |= RX_FLAG_FAILED_FCS_CRC;
		wiphy_err(wlc->wiphy, "%s:  RX_FLAG_FAILED_FCS_CRC\n",
			  __func__);
	}
}

static void
brcms_c_recvctl(struct brcms_c_info *wlc, struct d11rxhdr *rxh,
		struct sk_buff *p)
{
	int len_mpdu;
	struct ieee80211_rx_status rx_status;
	struct ieee80211_hdr *hdr;

	memset(&rx_status, 0, sizeof(rx_status));
	prep_mac80211_status(wlc, rxh, p, &rx_status);

	
	len_mpdu = p->len - D11_PHY_HDR_LEN - FCS_LEN;
	skb_pull(p, D11_PHY_HDR_LEN);
	__skb_trim(p, len_mpdu);

	
	if (wlc->hw->suspended_fifos) {
		hdr = (struct ieee80211_hdr *)p->data;
		if (ieee80211_is_beacon(hdr->frame_control))
			brcms_b_mute(wlc->hw, false);
	}

	memcpy(IEEE80211_SKB_RXCB(p), &rx_status, sizeof(rx_status));
	ieee80211_rx_irqsafe(wlc->pub->ieee_hw, p);
}

u16
brcms_c_calc_lsig_len(struct brcms_c_info *wlc, u32 ratespec,
		      uint mac_len)
{
	uint nsyms, len = 0, kNdps;

	BCMMSG(wlc->wiphy, "wl%d: rate %d, len%d\n",
		 wlc->pub->unit, rspec2rate(ratespec), mac_len);

	if (is_mcs_rate(ratespec)) {
		uint mcs = ratespec & RSPEC_RATE_MASK;
		int tot_streams = (mcs_2_txstreams(mcs) + 1) +
				  rspec_stc(ratespec);

		
		kNdps = mcs_2_rate(mcs, rspec_is40mhz(ratespec),
				   rspec_issgi(ratespec)) * 4;

		if (rspec_stc(ratespec) == 0)
			nsyms =
			    CEIL((APHY_SERVICE_NBITS + 8 * mac_len +
				  APHY_TAIL_NBITS) * 1000, kNdps);
		else
			
			nsyms =
			    2 *
			    CEIL((APHY_SERVICE_NBITS + 8 * mac_len +
				  APHY_TAIL_NBITS) * 1000, 2 * kNdps);

		
		nsyms += (tot_streams + 3);
		len = (3 * nsyms) - 3;
	}

	return (u16) len;
}

static void
brcms_c_mod_prb_rsp_rate_table(struct brcms_c_info *wlc, uint frame_len)
{
	const struct brcms_c_rateset *rs_dflt;
	struct brcms_c_rateset rs;
	u8 rate;
	u16 entry_ptr;
	u8 plcp[D11_PHY_HDR_LEN];
	u16 dur, sifs;
	uint i;

	sifs = get_sifs(wlc->band);

	rs_dflt = brcms_c_rateset_get_hwrs(wlc);

	brcms_c_rateset_copy(rs_dflt, &rs);
	brcms_c_rateset_mcs_upd(&rs, wlc->stf->txstreams);

	for (i = 0; i < rs.count; i++) {
		rate = rs.rates[i] & BRCMS_RATE_MASK;

		entry_ptr = brcms_b_rate_shm_offset(wlc->hw, rate);

		
		brcms_c_compute_plcp(wlc, rate, frame_len, plcp);

		dur = (u16) brcms_c_calc_frame_time(wlc, rate,
						BRCMS_LONG_PREAMBLE, frame_len);
		dur += sifs;

		
		brcms_b_write_shm(wlc->hw, entry_ptr + M_RT_PRS_PLCP_POS,
			      (u16) (plcp[0] + (plcp[1] << 8)));
		brcms_b_write_shm(wlc->hw, entry_ptr + M_RT_PRS_PLCP_POS + 2,
			      (u16) (plcp[2] + (plcp[3] << 8)));
		brcms_b_write_shm(wlc->hw, entry_ptr + M_RT_PRS_DUR_POS, dur);
	}
}

static void
brcms_c_bcn_prb_template(struct brcms_c_info *wlc, u16 type,
			 u32 bcn_rspec,
			 struct brcms_bss_cfg *cfg, u16 *buf, int *len)
{
	static const u8 ether_bcast[ETH_ALEN] = {255, 255, 255, 255, 255, 255};
	struct cck_phy_hdr *plcp;
	struct ieee80211_mgmt *h;
	int hdr_len, body_len;

	hdr_len = D11_PHY_HDR_LEN + DOT11_MAC_HDR_LEN;

	
	body_len = *len - hdr_len;
	
	*len = hdr_len + body_len;

	
	memset((char *)buf, 0, hdr_len);

	plcp = (struct cck_phy_hdr *) buf;

	if (type == IEEE80211_STYPE_BEACON)
		
		brcms_c_compute_plcp(wlc, bcn_rspec,
				 (DOT11_MAC_HDR_LEN + body_len + FCS_LEN),
				 (u8 *) plcp);

	
	
	brcms_c_beacon_phytxctl_txant_upd(wlc, bcn_rspec);

	h = (struct ieee80211_mgmt *)&plcp[1];

	
	h->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | type);

	
	
	if (type == IEEE80211_STYPE_BEACON)
		memcpy(&h->da, &ether_bcast, ETH_ALEN);
	memcpy(&h->sa, &cfg->cur_etheraddr, ETH_ALEN);
	memcpy(&h->bssid, &cfg->BSSID, ETH_ALEN);

	
}

int brcms_c_get_header_len(void)
{
	return TXOFF;
}

void brcms_c_update_beacon(struct brcms_c_info *wlc)
{
	struct brcms_bss_cfg *bsscfg = wlc->bsscfg;

	if (bsscfg->up && !bsscfg->BSS)
		
		wlc->defmacintmask &= ~MI_BCNTPL;
}

static void
brcms_c_shm_ssid_upd(struct brcms_c_info *wlc, struct brcms_bss_cfg *cfg)
{
	u8 *ssidptr = cfg->SSID;
	u16 base = M_SSID;
	u8 ssidbuf[IEEE80211_MAX_SSID_LEN];

	
	memset(ssidbuf, 0, IEEE80211_MAX_SSID_LEN);
	memcpy(ssidbuf, ssidptr, cfg->SSID_len);

	brcms_c_copyto_shm(wlc, base, ssidbuf, IEEE80211_MAX_SSID_LEN);
	brcms_b_write_shm(wlc->hw, M_SSIDLEN, (u16) cfg->SSID_len);
}

static void
brcms_c_bss_update_probe_resp(struct brcms_c_info *wlc,
			      struct brcms_bss_cfg *cfg,
			      bool suspend)
{
	u16 prb_resp[BCN_TMPL_LEN / 2];
	int len = BCN_TMPL_LEN;


	
	brcms_c_bcn_prb_template(wlc, IEEE80211_STYPE_PROBE_RESP, 0,
				 cfg, prb_resp, &len);

	if (suspend)
		brcms_c_suspend_mac_and_wait(wlc);

	
	brcms_b_write_template_ram(wlc->hw, T_PRS_TPL_BASE,
				    (len + 3) & ~3, prb_resp);

	
	brcms_b_write_shm(wlc->hw, M_PRB_RESP_FRM_LEN, (u16) len);

	
	brcms_c_shm_ssid_upd(wlc, cfg);

	len += (-D11_PHY_HDR_LEN + FCS_LEN);
	brcms_c_mod_prb_rsp_rate_table(wlc, (u16) len);

	if (suspend)
		brcms_c_enable_mac(wlc);
}

void brcms_c_update_probe_resp(struct brcms_c_info *wlc, bool suspend)
{
	struct brcms_bss_cfg *bsscfg = wlc->bsscfg;

	
	if (bsscfg->up && !bsscfg->BSS)
		brcms_c_bss_update_probe_resp(wlc, bsscfg, suspend);
}

int brcms_c_prep_pdu(struct brcms_c_info *wlc, struct sk_buff *pdu, uint *fifop)
{
	uint fifo;
	struct d11txh *txh;
	struct ieee80211_hdr *h;
	struct scb *scb;

	txh = (struct d11txh *) (pdu->data);
	h = (struct ieee80211_hdr *)((u8 *) (txh + 1) + D11_PHY_HDR_LEN);

	fifo = le16_to_cpu(txh->TxFrameID) & TXFID_QUEUE_MASK;

	scb = NULL;

	*fifop = fifo;

	
	if (*wlc->core->txavail[fifo] < MAX_DMA_SEGS) {
		
		
		wlc->tx_prec_map &= ~(wlc->fifo2prec_map[fifo]);
		return -EBUSY;
	}
	return 0;
}

int brcms_b_xmtfifo_sz_get(struct brcms_hardware *wlc_hw, uint fifo,
			   uint *blocks)
{
	if (fifo >= NFIFO)
		return -EINVAL;

	*blocks = wlc_hw->xmtfifo_sz[fifo];

	return 0;
}

void
brcms_c_set_addrmatch(struct brcms_c_info *wlc, int match_reg_offset,
		  const u8 *addr)
{
	brcms_b_set_addrmatch(wlc->hw, match_reg_offset, addr);
	if (match_reg_offset == RCM_BSSID_OFFSET)
		memcpy(wlc->bsscfg->BSSID, addr, ETH_ALEN);
}

void brcms_c_scan_start(struct brcms_c_info *wlc)
{
	wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_SCAN, true);
}

void brcms_c_scan_stop(struct brcms_c_info *wlc)
{
	wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_SCAN, false);
}

void brcms_c_associate_upd(struct brcms_c_info *wlc, bool state)
{
	wlc->pub->associated = state;
	wlc->bsscfg->associated = state;
}

void brcms_c_inval_dma_pkts(struct brcms_hardware *hw,
			       struct ieee80211_sta *sta,
			       void (*dma_callback_fn))
{
	struct dma_pub *dmah;
	int i;
	for (i = 0; i < NFIFO; i++) {
		dmah = hw->di[i];
		if (dmah != NULL)
			dma_walk_packets(dmah, dma_callback_fn, sta);
	}
}

int brcms_c_get_curband(struct brcms_c_info *wlc)
{
	return wlc->band->bandunit;
}

void brcms_c_wait_for_tx_completion(struct brcms_c_info *wlc, bool drop)
{
	int timeout = 20;

	
	if (drop)
		brcmu_pktq_flush(&wlc->pkt_queue->q, false, NULL, NULL);

	
	while (!pktq_empty(&wlc->pkt_queue->q) || brcms_txpktpendtot(wlc) > 0) {
		brcms_msleep(wlc->wl, 1);

		if (--timeout == 0)
			break;
	}

	WARN_ON_ONCE(timeout == 0);
}

void brcms_c_set_beacon_listen_interval(struct brcms_c_info *wlc, u8 interval)
{
	wlc->bcn_li_bcn = interval;
	if (wlc->pub->up)
		brcms_c_bcn_li_upd(wlc);
}

int brcms_c_set_tx_power(struct brcms_c_info *wlc, int txpwr)
{
	uint qdbm;

	
	qdbm = min_t(uint, txpwr * BRCMS_TXPWR_DB_FACTOR, 0xff);
	return wlc_phy_txpower_set(wlc->band->pi, qdbm, false);
}

int brcms_c_get_tx_power(struct brcms_c_info *wlc)
{
	uint qdbm;
	bool override;

	wlc_phy_txpower_get(wlc->band->pi, &qdbm, &override);

	
	return (int)(qdbm / BRCMS_TXPWR_DB_FACTOR);
}

static void brcms_c_recv(struct brcms_c_info *wlc, struct sk_buff *p)
{
	struct d11rxhdr *rxh;
	struct ieee80211_hdr *h;
	uint len;
	bool is_amsdu;

	BCMMSG(wlc->wiphy, "wl%d\n", wlc->pub->unit);

	
	rxh = (struct d11rxhdr *) (p->data);

	
	skb_pull(p, BRCMS_HWRXOFF);

	
	if (rxh->RxStatus1 & RXS_PBPRES) {
		if (p->len < 2) {
			wiphy_err(wlc->wiphy, "wl%d: recv: rcvd runt of "
				  "len %d\n", wlc->pub->unit, p->len);
			goto toss;
		}
		skb_pull(p, 2);
	}

	h = (struct ieee80211_hdr *)(p->data + D11_PHY_HDR_LEN);
	len = p->len;

	if (rxh->RxStatus1 & RXS_FCSERR) {
		if (!(wlc->filter_flags & FIF_FCSFAIL))
			goto toss;
	}

	
	if (len < D11_PHY_HDR_LEN + sizeof(h->frame_control))
		goto toss;

	
	is_amsdu = rxh->RxStatus2 & RXS_AMSDU_MASK;
	if (is_amsdu)
		goto toss;

	brcms_c_recvctl(wlc, rxh, p);
	return;

 toss:
	brcmu_pkt_buf_free_skb(p);
}

static bool
brcms_b_recv(struct brcms_hardware *wlc_hw, uint fifo, bool bound)
{
	struct sk_buff *p;
	struct sk_buff *next = NULL;
	struct sk_buff_head recv_frames;

	uint n = 0;
	uint bound_limit = bound ? RXBND : -1;

	BCMMSG(wlc_hw->wlc->wiphy, "wl%d\n", wlc_hw->unit);
	skb_queue_head_init(&recv_frames);

	
	while (dma_rx(wlc_hw->di[fifo], &recv_frames)) {

		
		if (++n >= bound_limit)
			break;
	}

	
	dma_rxfill(wlc_hw->di[fifo]);

	
	skb_queue_walk_safe(&recv_frames, p, next) {
		struct d11rxhdr_le *rxh_le;
		struct d11rxhdr *rxh;

		skb_unlink(p, &recv_frames);
		rxh_le = (struct d11rxhdr_le *)p->data;
		rxh = (struct d11rxhdr *)p->data;

		
		rxh->RxFrameSize = le16_to_cpu(rxh_le->RxFrameSize);
		rxh->PhyRxStatus_0 = le16_to_cpu(rxh_le->PhyRxStatus_0);
		rxh->PhyRxStatus_1 = le16_to_cpu(rxh_le->PhyRxStatus_1);
		rxh->PhyRxStatus_2 = le16_to_cpu(rxh_le->PhyRxStatus_2);
		rxh->PhyRxStatus_3 = le16_to_cpu(rxh_le->PhyRxStatus_3);
		rxh->PhyRxStatus_4 = le16_to_cpu(rxh_le->PhyRxStatus_4);
		rxh->PhyRxStatus_5 = le16_to_cpu(rxh_le->PhyRxStatus_5);
		rxh->RxStatus1 = le16_to_cpu(rxh_le->RxStatus1);
		rxh->RxStatus2 = le16_to_cpu(rxh_le->RxStatus2);
		rxh->RxTSFTime = le16_to_cpu(rxh_le->RxTSFTime);
		rxh->RxChan = le16_to_cpu(rxh_le->RxChan);

		brcms_c_recv(wlc_hw->wlc, p);
	}

	return n >= bound_limit;
}

bool brcms_c_dpc(struct brcms_c_info *wlc, bool bounded)
{
	u32 macintstatus;
	struct brcms_hardware *wlc_hw = wlc->hw;
	struct bcma_device *core = wlc_hw->d11core;
	struct wiphy *wiphy = wlc->wiphy;

	if (brcms_deviceremoved(wlc)) {
		wiphy_err(wiphy, "wl%d: %s: dead chip\n", wlc_hw->unit,
			  __func__);
		brcms_down(wlc->wl);
		return false;
	}

	
	macintstatus = wlc->macintstatus;
	wlc->macintstatus = 0;

	BCMMSG(wlc->wiphy, "wl%d: macintstatus 0x%x\n",
	       wlc_hw->unit, macintstatus);

	WARN_ON(macintstatus & MI_PRQ); 

	
	if (macintstatus & MI_TFS) {
		bool fatal;
		if (brcms_b_txstatus(wlc->hw, bounded, &fatal))
			wlc->macintstatus |= MI_TFS;
		if (fatal) {
			wiphy_err(wiphy, "MI_TFS: fatal\n");
			goto fatal;
		}
	}

	if (macintstatus & (MI_TBTT | MI_DTIM_TBTT))
		brcms_c_tbtt(wlc);

	
	if (macintstatus & MI_ATIMWINEND) {
		BCMMSG(wlc->wiphy, "end of ATIM window\n");
		bcma_set32(core, D11REGOFFS(maccommand), wlc->qvalid);
		wlc->qvalid = 0;
	}

	if (macintstatus & MI_DMAINT)
		if (brcms_b_recv(wlc_hw, RX_FIFO, bounded))
			wlc->macintstatus |= MI_DMAINT;

	
	if (macintstatus & MI_BG_NOISE)
		wlc_phy_noise_sample_intr(wlc_hw->band->pi);

	if (macintstatus & MI_GP0) {
		wiphy_err(wiphy, "wl%d: PSM microcode watchdog fired at %d "
			  "(seconds). Resetting.\n", wlc_hw->unit, wlc_hw->now);

		printk_once("%s : PSM Watchdog, chipid 0x%x, chiprev 0x%x\n",
			    __func__, ai_get_chip_id(wlc_hw->sih),
			    ai_get_chiprev(wlc_hw->sih));
		brcms_fatal_error(wlc_hw->wlc->wl);
	}

	
	if (macintstatus & MI_TO)
		bcma_write32(core, D11REGOFFS(gptimer), 0);

	if (macintstatus & MI_RFDISABLE) {
		BCMMSG(wlc->wiphy, "wl%d: BMAC Detected a change on the"
		       " RF Disable Input\n", wlc_hw->unit);
		brcms_rfkill_set_hw_state(wlc->wl);
	}

	
	if (!pktq_empty(&wlc->pkt_queue->q))
		brcms_c_send_q(wlc);

	
	return wlc->macintstatus != 0;

 fatal:
	brcms_fatal_error(wlc_hw->wlc->wl);
	return wlc->macintstatus != 0;
}

void brcms_c_init(struct brcms_c_info *wlc, bool mute_tx)
{
	struct bcma_device *core = wlc->hw->d11core;
	u16 chanspec;

	BCMMSG(wlc->wiphy, "wl%d\n", wlc->pub->unit);

	if (wlc->pub->associated)
		chanspec = wlc->home_chanspec;
	else
		chanspec = brcms_c_init_chanspec(wlc);

	brcms_b_init(wlc->hw, chanspec);

	
	brcms_c_bcn_li_upd(wlc);

	
	brcms_c_set_mac(wlc->bsscfg);
	brcms_c_set_bssid(wlc->bsscfg);

	
	if (wlc->pub->associated && wlc->bsscfg->up) {
		u32 bi;

		
		bi = wlc->bsscfg->current_bss->beacon_period << 10;
		bcma_write32(core, D11REGOFFS(tsf_cfprep),
			     bi << CFPREP_CBI_SHIFT);

		
		brcms_c_set_ps_ctrl(wlc);
	}

	brcms_c_bandinit_ordered(wlc, chanspec);

	
	brcms_b_write_shm(wlc->hw, M_PRS_MAXTIME, wlc->prb_resp_timeout);

	
	brcms_b_write_shm(wlc->hw, M_MBURST_TXOP,
		      (wlc->
		       _rifs ? (EDCF_AC_VO_TXOP_AP << 5) : MAXFRAMEBURST_TXOP));

	
	brcms_c_duty_cycle_set(wlc, wlc->tx_duty_cycle_ofdm, true, true);
	brcms_c_duty_cycle_set(wlc, wlc->tx_duty_cycle_cck, false, true);

	brcms_c_ampdu_shm_upd(wlc->ampdu);

	
	brcms_c_bsinit(wlc);

	
	bcma_set16(core, D11REGOFFS(ifs_ctl), IFS_USEEDCF);
	brcms_c_edcf_setparams(wlc, false);

	
	brcms_c_tx_prec_map_init(wlc);

	
	if (wlc->ucode_rev == 0) {
		wlc->ucode_rev =
		    brcms_b_read_shm(wlc->hw, M_BOM_REV_MAJOR) << NBITS(u16);
		wlc->ucode_rev |= brcms_b_read_shm(wlc->hw, M_BOM_REV_MINOR);
	}

	
	brcms_c_enable_mac(wlc);

	
	if (mute_tx)
		brcms_b_mute(wlc->hw, true);

	
	brcms_c_txflowcontrol_reset(wlc);

	
	bcma_write32(core, D11REGOFFS(rfdisabledly), RFDISABLE_DEFAULT);

	if (GFIELD(wlc->wme_retries[0], EDCF_SHORT) == 0) {
		
		int ac;

		for (ac = 0; ac < IEEE80211_NUM_ACS; ac++)
			wlc->wme_retries[ac] =
			    brcms_b_read_shm(wlc->hw, M_AC_TXLMT_ADDR(ac));
	}
}

struct brcms_c_info *
brcms_c_attach(struct brcms_info *wl, struct bcma_device *core, uint unit,
	       bool piomode, uint *perr)
{
	struct brcms_c_info *wlc;
	uint err = 0;
	uint i, j;
	struct brcms_pub *pub;

	
	wlc = (struct brcms_c_info *) brcms_c_attach_malloc(unit, &err, 0);
	if (wlc == NULL)
		goto fail;
	wlc->wiphy = wl->wiphy;
	pub = wlc->pub;

#if defined(DEBUG)
	wlc_info_dbg = wlc;
#endif

	wlc->band = wlc->bandstate[0];
	wlc->core = wlc->corestate;
	wlc->wl = wl;
	pub->unit = unit;
	pub->_piomode = piomode;
	wlc->bandinit_pending = false;

	
	brcms_c_info_init(wlc, unit);

	
	brcms_c_ap_upd(wlc);

	err = brcms_b_attach(wlc, core, unit, piomode);
	if (err)
		goto fail;

	brcms_c_protection_upd(wlc, BRCMS_PROT_N_PAM_OVR, OFF);

	pub->phy_11ncapable = BRCMS_PHY_11N_CAP(wlc->band);

	
	wlc->tx_duty_cycle_ofdm = 0;
	wlc->tx_duty_cycle_cck = 0;

	brcms_c_stf_phy_chain_calc(wlc);

	
	if (BRCMS_ISNPHY(wlc->band) && (wlc->stf->txstreams == 1))
		wlc->stf->txant = wlc->stf->hw_txchain - 1;

	
	wlc_phy_stf_chain_init(wlc->band->pi, wlc->stf->hw_txchain,
			       wlc->stf->hw_rxchain);

	
	for (i = 0; i < NFIFO; i++)
		wlc->core->txavail[i] = wlc->hw->txavail[i];

	memcpy(&wlc->perm_etheraddr, &wlc->hw->etheraddr, ETH_ALEN);
	memcpy(&pub->cur_etheraddr, &wlc->hw->etheraddr, ETH_ALEN);

	for (j = 0; j < wlc->pub->_nbands; j++) {
		wlc->band = wlc->bandstate[j];

		if (!brcms_c_attach_stf_ant_init(wlc)) {
			err = 24;
			goto fail;
		}

		
		wlc->band->CWmin = APHY_CWMIN;
		wlc->band->CWmax = PHY_CWMAX;

		
		if (wlc->band->bandtype == BRCM_BAND_2G) {
			wlc->band->gmode = GMODE_AUTO;
			brcms_c_protection_upd(wlc, BRCMS_PROT_G_USER,
					   wlc->band->gmode);
		}

		
		if (BRCMS_PHY_11N_CAP(wlc->band)) {
			pub->_n_enab = SUPPORT_11N;
			brcms_c_protection_upd(wlc, BRCMS_PROT_N_USER,
						   ((pub->_n_enab ==
						     SUPPORT_11N) ? WL_11N_2x2 :
						    WL_11N_3x3));
		}

		
		brcms_default_rateset(wlc, &wlc->band->defrateset);

		
		brcms_c_rateset_filter(&wlc->band->defrateset,
				   &wlc->band->hw_rateset, false,
				   BRCMS_RATES_CCK_OFDM, BRCMS_RATE_MASK,
				   (bool) (wlc->pub->_n_enab & SUPPORT_11N));
	}

	brcms_c_stf_phy_txant_upd(wlc);

	
	err = brcms_c_attach_module(wlc);
	if (err != 0)
		goto fail;

	if (!brcms_c_timers_init(wlc, unit)) {
		wiphy_err(wl->wiphy, "wl%d: %s: init_timer failed\n", unit,
			  __func__);
		err = 32;
		goto fail;
	}

	
	wlc->cmi = brcms_c_channel_mgr_attach(wlc);
	if (!wlc->cmi) {
		wiphy_err(wl->wiphy, "wl%d: %s: channel_mgr_attach failed"
			  "\n", unit, __func__);
		err = 33;
		goto fail;
	}

	
	brcms_c_bss_default_init(wlc);


	
	wlc->pkt_queue = brcms_c_txq_alloc(wlc);
	if (wlc->pkt_queue == NULL) {
		wiphy_err(wl->wiphy, "wl%d: %s: failed to malloc tx queue\n",
			  unit, __func__);
		err = 100;
		goto fail;
	}

	wlc->bsscfg->wlc = wlc;

	wlc->mimoft = FT_HT;
	wlc->mimo_40txbw = AUTO;
	wlc->ofdm_40txbw = AUTO;
	wlc->cck_40txbw = AUTO;
	brcms_c_update_mimo_band_bwcap(wlc, BRCMS_N_BW_20IN2G_40IN5G);

	
	if (BRCMS_SGI_CAP_PHY(wlc)) {
		brcms_c_ht_update_sgi_rx(wlc, (BRCMS_N_SGI_20 |
					       BRCMS_N_SGI_40));
	} else if (BRCMS_ISSSLPNPHY(wlc->band)) {
		brcms_c_ht_update_sgi_rx(wlc, (BRCMS_N_SGI_20 |
					       BRCMS_N_SGI_40));
	} else {
		brcms_c_ht_update_sgi_rx(wlc, 0);
	}

	brcms_b_antsel_set(wlc->hw, wlc->asi->antsel_avail);

	if (perr)
		*perr = 0;

	return wlc;

 fail:
	wiphy_err(wl->wiphy, "wl%d: %s: failed with err %d\n",
		  unit, __func__, err);
	if (wlc)
		brcms_c_detach(wlc);

	if (perr)
		*perr = err;
	return NULL;
}
