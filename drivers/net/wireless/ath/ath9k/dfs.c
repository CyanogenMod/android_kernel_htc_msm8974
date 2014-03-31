/*
 * Copyright (c) 2008-2011 Atheros Communications Inc.
 * Copyright (c) 2011 Neratec Solutions AG
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "hw.h"
#include "hw-ops.h"
#include "ath9k.h"
#include "dfs.h"
#include "dfs_debug.h"

struct dfs_radar_pulse {
	u16 freq;
	u64 ts;
	u32 width;
	u8 rssi;
};

struct ath_radar_data {
	u8 pulse_bw_info;
	u8 rssi;
	u8 ext_rssi;
	u8 pulse_length_ext;
	u8 pulse_length_pri;
};

static u32 dur_to_usecs(struct ath_hw *ah, u32 dur)
{
	const u32 AR93X_NSECS_PER_DUR = 800;
	const u32 AR93X_NSECS_PER_DUR_FAST = (8000 / 11);
	u32 nsecs;

	if (IS_CHAN_A_FAST_CLOCK(ah, ah->curchan))
		nsecs = dur * AR93X_NSECS_PER_DUR_FAST;
	else
		nsecs = dur * AR93X_NSECS_PER_DUR;

	return (nsecs + 500) / 1000;
}

#define PRI_CH_RADAR_FOUND 0x01
#define EXT_CH_RADAR_FOUND 0x02
static bool
ath9k_postprocess_radar_event(struct ath_softc *sc,
			      struct ath_radar_data *are,
			      struct dfs_radar_pulse *drp)
{
	u8 rssi;
	u16 dur;

	ath_dbg(ath9k_hw_common(sc->sc_ah), DFS,
		"pulse_bw_info=0x%x, pri,ext len/rssi=(%u/%u, %u/%u)\n",
		are->pulse_bw_info,
		are->pulse_length_pri, are->rssi,
		are->pulse_length_ext, are->ext_rssi);

	are->pulse_bw_info &= 0x03;

	switch (are->pulse_bw_info) {
	case PRI_CH_RADAR_FOUND:
		
		dur = are->pulse_length_pri;
		DFS_STAT_INC(sc, pri_phy_errors);
		rssi = (are->ext_rssi >= (are->rssi + 3)) ? 0 : are->rssi;
		break;
	case EXT_CH_RADAR_FOUND:
		
		dur = are->pulse_length_ext;
		DFS_STAT_INC(sc, ext_phy_errors);
		rssi = (are->rssi >= (are->ext_rssi + 12)) ? 0 : are->ext_rssi;
		break;
	case (PRI_CH_RADAR_FOUND | EXT_CH_RADAR_FOUND):
		if (are->pulse_length_ext >= are->pulse_length_pri)
			dur = are->pulse_length_ext;
		else
			dur = are->pulse_length_pri;
		DFS_STAT_INC(sc, dc_phy_errors);

		
		rssi = (are->rssi < are->ext_rssi) ? are->ext_rssi : are->rssi;
		break;
	default:
		DFS_STAT_INC(sc, bwinfo_discards);
		return false;
	}

	if (rssi == 0) {
		DFS_STAT_INC(sc, rssi_discards);
		return false;
	}


	
	drp->width = dur_to_usecs(sc->sc_ah, dur);
	drp->rssi = rssi;

	DFS_STAT_INC(sc, pulses_detected);
	return true;
}
#undef PRI_CH_RADAR_FOUND
#undef EXT_CH_RADAR_FOUND

void ath9k_dfs_process_phyerr(struct ath_softc *sc, void *data,
			      struct ath_rx_status *rs, u64 mactime)
{
	struct ath_radar_data ard;
	u16 datalen;
	char *vdata_end;
	struct dfs_radar_pulse drp;
	struct ath_hw *ah = sc->sc_ah;
	struct ath_common *common = ath9k_hw_common(ah);

	if ((!(rs->rs_phyerr != ATH9K_PHYERR_RADAR)) &&
	    (!(rs->rs_phyerr != ATH9K_PHYERR_FALSE_RADAR_EXT))) {
		ath_dbg(common, DFS,
			"Error: rs_phyer=0x%x not a radar error\n",
			rs->rs_phyerr);
		return;
	}

	datalen = rs->rs_datalen;
	if (datalen == 0) {
		DFS_STAT_INC(sc, datalen_discards);
		return;
	}

	ard.rssi = rs->rs_rssi_ctl0;
	ard.ext_rssi = rs->rs_rssi_ext0;

	if (ard.rssi & 0x80)
		ard.rssi = 0;
	if (ard.ext_rssi & 0x80)
		ard.ext_rssi = 0;

	vdata_end = (char *)data + datalen;
	ard.pulse_bw_info = vdata_end[-1];
	ard.pulse_length_ext = vdata_end[-2];
	ard.pulse_length_pri = vdata_end[-3];

	ath_dbg(common, DFS,
		"bw_info=%d, length_pri=%d, length_ext=%d, "
		"rssi_pri=%d, rssi_ext=%d\n",
		ard.pulse_bw_info, ard.pulse_length_pri, ard.pulse_length_ext,
		ard.rssi, ard.ext_rssi);

	drp.freq = ah->curchan->channel;
	drp.ts = mactime;
	if (ath9k_postprocess_radar_event(sc, &ard, &drp)) {
		static u64 last_ts;
		ath_dbg(common, DFS,
			"ath9k_dfs_process_phyerr: channel=%d, ts=%llu, "
			"width=%d, rssi=%d, delta_ts=%llu\n",
			drp.freq, drp.ts, drp.width, drp.rssi, drp.ts-last_ts);
		last_ts = drp.ts;
	}
}
