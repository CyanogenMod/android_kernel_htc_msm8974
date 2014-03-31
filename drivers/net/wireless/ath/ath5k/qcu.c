/*
 * Copyright (c) 2004-2008 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006-2008 Nick Kossifidis <mickflemm@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
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
 *
 */


#include "ath5k.h"
#include "reg.h"
#include "debug.h"
#include <linux/log2.h>




u32
ath5k_hw_num_tx_pending(struct ath5k_hw *ah, unsigned int queue)
{
	u32 pending;
	AR5K_ASSERT_ENTRY(queue, ah->ah_capabilities.cap_queues.q_tx_num);

	
	if (ah->ah_txq[queue].tqi_type == AR5K_TX_QUEUE_INACTIVE)
		return false;

	
	if (ah->ah_version == AR5K_AR5210)
		return false;

	pending = ath5k_hw_reg_read(ah, AR5K_QUEUE_STATUS(queue));
	pending &= AR5K_QCU_STS_FRMPENDCNT;

	if (!pending && AR5K_REG_READ_Q(ah, AR5K_QCU_TXE, queue))
		return true;

	return pending;
}

void
ath5k_hw_release_tx_queue(struct ath5k_hw *ah, unsigned int queue)
{
	if (WARN_ON(queue >= ah->ah_capabilities.cap_queues.q_tx_num))
		return;

	
	ah->ah_txq[queue].tqi_type = AR5K_TX_QUEUE_INACTIVE;
	
	AR5K_Q_DISABLE_BITS(ah->ah_txq_status, queue);
}

static u16
ath5k_cw_validate(u16 cw_req)
{
	cw_req = min(cw_req, (u16)1023);

	
	if (is_power_of_2(cw_req + 1))
		return cw_req;

	
	if (is_power_of_2(cw_req))
		return cw_req - 1;

	cw_req = (u16) roundup_pow_of_two(cw_req) - 1;

	return cw_req;
}

int
ath5k_hw_get_tx_queueprops(struct ath5k_hw *ah, int queue,
		struct ath5k_txq_info *queue_info)
{
	memcpy(queue_info, &ah->ah_txq[queue], sizeof(struct ath5k_txq_info));
	return 0;
}

int
ath5k_hw_set_tx_queueprops(struct ath5k_hw *ah, int queue,
				const struct ath5k_txq_info *qinfo)
{
	struct ath5k_txq_info *qi;

	AR5K_ASSERT_ENTRY(queue, ah->ah_capabilities.cap_queues.q_tx_num);

	qi = &ah->ah_txq[queue];

	if (qi->tqi_type == AR5K_TX_QUEUE_INACTIVE)
		return -EIO;

	
	qi->tqi_type = qinfo->tqi_type;
	qi->tqi_subtype = qinfo->tqi_subtype;
	qi->tqi_flags = qinfo->tqi_flags;
	qi->tqi_aifs = min(qinfo->tqi_aifs, (u8)0xFC);
	qi->tqi_cw_min = ath5k_cw_validate(qinfo->tqi_cw_min);
	qi->tqi_cw_max = ath5k_cw_validate(qinfo->tqi_cw_max);
	qi->tqi_cbr_period = qinfo->tqi_cbr_period;
	qi->tqi_cbr_overflow_limit = qinfo->tqi_cbr_overflow_limit;
	qi->tqi_burst_time = qinfo->tqi_burst_time;
	qi->tqi_ready_time = qinfo->tqi_ready_time;

	
	
	if ((qinfo->tqi_type == AR5K_TX_QUEUE_DATA &&
		((qinfo->tqi_subtype == AR5K_WME_AC_VI) ||
		 (qinfo->tqi_subtype == AR5K_WME_AC_VO))) ||
	     qinfo->tqi_type == AR5K_TX_QUEUE_UAPSD)
		qi->tqi_flags |= AR5K_TXQ_FLAG_POST_FR_BKOFF_DIS;

	return 0;
}

int
ath5k_hw_setup_tx_queue(struct ath5k_hw *ah, enum ath5k_tx_queue queue_type,
		struct ath5k_txq_info *queue_info)
{
	unsigned int queue;
	int ret;

	
	if (ah->ah_capabilities.cap_queues.q_tx_num == 2) {
		switch (queue_type) {
		case AR5K_TX_QUEUE_DATA:
			queue = AR5K_TX_QUEUE_ID_NOQCU_DATA;
			break;
		case AR5K_TX_QUEUE_BEACON:
		case AR5K_TX_QUEUE_CAB:
			queue = AR5K_TX_QUEUE_ID_NOQCU_BEACON;
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (queue_type) {
		case AR5K_TX_QUEUE_DATA:
			for (queue = AR5K_TX_QUEUE_ID_DATA_MIN;
				ah->ah_txq[queue].tqi_type !=
				AR5K_TX_QUEUE_INACTIVE; queue++) {

				if (queue > AR5K_TX_QUEUE_ID_DATA_MAX)
					return -EINVAL;
			}
			break;
		case AR5K_TX_QUEUE_UAPSD:
			queue = AR5K_TX_QUEUE_ID_UAPSD;
			break;
		case AR5K_TX_QUEUE_BEACON:
			queue = AR5K_TX_QUEUE_ID_BEACON;
			break;
		case AR5K_TX_QUEUE_CAB:
			queue = AR5K_TX_QUEUE_ID_CAB;
			break;
		default:
			return -EINVAL;
		}
	}

	memset(&ah->ah_txq[queue], 0, sizeof(struct ath5k_txq_info));
	ah->ah_txq[queue].tqi_type = queue_type;

	if (queue_info != NULL) {
		queue_info->tqi_type = queue_type;
		ret = ath5k_hw_set_tx_queueprops(ah, queue, queue_info);
		if (ret)
			return ret;
	}

	AR5K_Q_ENABLE_BITS(ah->ah_txq_status, queue);

	return queue;
}



void
ath5k_hw_set_tx_retry_limits(struct ath5k_hw *ah,
				  unsigned int queue)
{
	
	if (ah->ah_version == AR5K_AR5210) {
		struct ath5k_txq_info *tq = &ah->ah_txq[queue];

		if (queue > 0)
			return;

		ath5k_hw_reg_write(ah,
			(tq->tqi_cw_min << AR5K_NODCU_RETRY_LMT_CW_MIN_S)
			| AR5K_REG_SM(ah->ah_retry_long,
				      AR5K_NODCU_RETRY_LMT_SLG_RETRY)
			| AR5K_REG_SM(ah->ah_retry_short,
				      AR5K_NODCU_RETRY_LMT_SSH_RETRY)
			| AR5K_REG_SM(ah->ah_retry_long,
				      AR5K_NODCU_RETRY_LMT_LG_RETRY)
			| AR5K_REG_SM(ah->ah_retry_short,
				      AR5K_NODCU_RETRY_LMT_SH_RETRY),
			AR5K_NODCU_RETRY_LMT);
	
	} else {
		ath5k_hw_reg_write(ah,
			AR5K_REG_SM(ah->ah_retry_long,
				    AR5K_DCU_RETRY_LMT_RTS)
			| AR5K_REG_SM(ah->ah_retry_long,
				      AR5K_DCU_RETRY_LMT_STA_RTS)
			| AR5K_REG_SM(max(ah->ah_retry_long, ah->ah_retry_short),
				      AR5K_DCU_RETRY_LMT_STA_DATA),
			AR5K_QUEUE_DFS_RETRY_LIMIT(queue));
	}
}

int
ath5k_hw_reset_tx_queue(struct ath5k_hw *ah, unsigned int queue)
{
	struct ath5k_txq_info *tq = &ah->ah_txq[queue];

	AR5K_ASSERT_ENTRY(queue, ah->ah_capabilities.cap_queues.q_tx_num);

	tq = &ah->ah_txq[queue];

	if ((ah->ah_version == AR5K_AR5210) ||
	(tq->tqi_type == AR5K_TX_QUEUE_INACTIVE))
		return 0;

	ath5k_hw_reg_write(ah,
		AR5K_REG_SM(tq->tqi_cw_min, AR5K_DCU_LCL_IFS_CW_MIN) |
		AR5K_REG_SM(tq->tqi_cw_max, AR5K_DCU_LCL_IFS_CW_MAX) |
		AR5K_REG_SM(tq->tqi_aifs, AR5K_DCU_LCL_IFS_AIFS),
		AR5K_QUEUE_DFS_LOCAL_IFS(queue));

	ath5k_hw_set_tx_retry_limits(ah, queue);



	
	AR5K_REG_ENABLE_BITS(ah, AR5K_QUEUE_DFS_MISC(queue),
				AR5K_DCU_MISC_FRAG_WAIT);

	
	if (ah->ah_mac_version < AR5K_SREV_AR5211)
		AR5K_REG_ENABLE_BITS(ah, AR5K_QUEUE_DFS_MISC(queue),
					AR5K_DCU_MISC_SEQNUM_CTL);

	
	if (tq->tqi_cbr_period) {
		ath5k_hw_reg_write(ah, AR5K_REG_SM(tq->tqi_cbr_period,
					AR5K_QCU_CBRCFG_INTVAL) |
					AR5K_REG_SM(tq->tqi_cbr_overflow_limit,
					AR5K_QCU_CBRCFG_ORN_THRES),
					AR5K_QUEUE_CBRCFG(queue));

		AR5K_REG_ENABLE_BITS(ah, AR5K_QUEUE_MISC(queue),
					AR5K_QCU_MISC_FRSHED_CBR);

		if (tq->tqi_cbr_overflow_limit)
			AR5K_REG_ENABLE_BITS(ah, AR5K_QUEUE_MISC(queue),
					AR5K_QCU_MISC_CBR_THRES_ENABLE);
	}

	
	if (tq->tqi_ready_time && (tq->tqi_type != AR5K_TX_QUEUE_CAB))
		ath5k_hw_reg_write(ah, AR5K_REG_SM(tq->tqi_ready_time,
					AR5K_QCU_RDYTIMECFG_INTVAL) |
					AR5K_QCU_RDYTIMECFG_ENABLE,
					AR5K_QUEUE_RDYTIMECFG(queue));

	if (tq->tqi_burst_time) {
		ath5k_hw_reg_write(ah, AR5K_REG_SM(tq->tqi_burst_time,
					AR5K_DCU_CHAN_TIME_DUR) |
					AR5K_DCU_CHAN_TIME_ENABLE,
					AR5K_QUEUE_DFS_CHANNEL_TIME(queue));

		if (tq->tqi_flags & AR5K_TXQ_FLAG_RDYTIME_EXP_POLICY_ENABLE)
			AR5K_REG_ENABLE_BITS(ah, AR5K_QUEUE_MISC(queue),
					AR5K_QCU_MISC_RDY_VEOL_POLICY);
	}

	
	if (tq->tqi_flags & AR5K_TXQ_FLAG_BACKOFF_DISABLE)
		ath5k_hw_reg_write(ah, AR5K_DCU_MISC_POST_FR_BKOFF_DIS,
					AR5K_QUEUE_DFS_MISC(queue));

	
	if (tq->tqi_flags & AR5K_TXQ_FLAG_FRAG_BURST_BACKOFF_ENABLE)
		ath5k_hw_reg_write(ah, AR5K_DCU_MISC_BACKOFF_FRAG,
					AR5K_QUEUE_DFS_MISC(queue));

	switch (tq->tqi_type) {
	case AR5K_TX_QUEUE_BEACON:
		AR5K_REG_ENABLE_BITS(ah, AR5K_QUEUE_MISC(queue),
				AR5K_QCU_MISC_FRSHED_DBA_GT |
				AR5K_QCU_MISC_CBREXP_BCN_DIS |
				AR5K_QCU_MISC_BCN_ENABLE);

		AR5K_REG_ENABLE_BITS(ah, AR5K_QUEUE_DFS_MISC(queue),
				(AR5K_DCU_MISC_ARBLOCK_CTL_GLOBAL <<
				AR5K_DCU_MISC_ARBLOCK_CTL_S) |
				AR5K_DCU_MISC_ARBLOCK_IGNORE |
				AR5K_DCU_MISC_POST_FR_BKOFF_DIS |
				AR5K_DCU_MISC_BCN_ENABLE);
		break;

	case AR5K_TX_QUEUE_CAB:
		
		AR5K_REG_ENABLE_BITS(ah, AR5K_QUEUE_MISC(queue),
					AR5K_QCU_MISC_FRSHED_DBA_GT |
					AR5K_QCU_MISC_CBREXP_DIS |
					AR5K_QCU_MISC_CBREXP_BCN_DIS);

		ath5k_hw_reg_write(ah, ((tq->tqi_ready_time -
					(AR5K_TUNE_SW_BEACON_RESP -
					AR5K_TUNE_DMA_BEACON_RESP) -
				AR5K_TUNE_ADDITIONAL_SWBA_BACKOFF) * 1024) |
					AR5K_QCU_RDYTIMECFG_ENABLE,
					AR5K_QUEUE_RDYTIMECFG(queue));

		AR5K_REG_ENABLE_BITS(ah, AR5K_QUEUE_DFS_MISC(queue),
					(AR5K_DCU_MISC_ARBLOCK_CTL_GLOBAL <<
					AR5K_DCU_MISC_ARBLOCK_CTL_S));
		break;

	case AR5K_TX_QUEUE_UAPSD:
		AR5K_REG_ENABLE_BITS(ah, AR5K_QUEUE_MISC(queue),
					AR5K_QCU_MISC_CBREXP_DIS);
		break;

	case AR5K_TX_QUEUE_DATA:
	default:
			break;
	}

	

	if (tq->tqi_flags & AR5K_TXQ_FLAG_TXOKINT_ENABLE)
		AR5K_Q_ENABLE_BITS(ah->ah_txq_imr_txok, queue);

	if (tq->tqi_flags & AR5K_TXQ_FLAG_TXERRINT_ENABLE)
		AR5K_Q_ENABLE_BITS(ah->ah_txq_imr_txerr, queue);

	if (tq->tqi_flags & AR5K_TXQ_FLAG_TXURNINT_ENABLE)
		AR5K_Q_ENABLE_BITS(ah->ah_txq_imr_txurn, queue);

	if (tq->tqi_flags & AR5K_TXQ_FLAG_TXDESCINT_ENABLE)
		AR5K_Q_ENABLE_BITS(ah->ah_txq_imr_txdesc, queue);

	if (tq->tqi_flags & AR5K_TXQ_FLAG_TXEOLINT_ENABLE)
		AR5K_Q_ENABLE_BITS(ah->ah_txq_imr_txeol, queue);

	if (tq->tqi_flags & AR5K_TXQ_FLAG_CBRORNINT_ENABLE)
		AR5K_Q_ENABLE_BITS(ah->ah_txq_imr_cbrorn, queue);

	if (tq->tqi_flags & AR5K_TXQ_FLAG_CBRURNINT_ENABLE)
		AR5K_Q_ENABLE_BITS(ah->ah_txq_imr_cbrurn, queue);

	if (tq->tqi_flags & AR5K_TXQ_FLAG_QTRIGINT_ENABLE)
		AR5K_Q_ENABLE_BITS(ah->ah_txq_imr_qtrig, queue);

	if (tq->tqi_flags & AR5K_TXQ_FLAG_TXNOFRMINT_ENABLE)
		AR5K_Q_ENABLE_BITS(ah->ah_txq_imr_nofrm, queue);

	

	
	ah->ah_txq_imr_txok &= ah->ah_txq_status;
	ah->ah_txq_imr_txerr &= ah->ah_txq_status;
	ah->ah_txq_imr_txurn &= ah->ah_txq_status;
	ah->ah_txq_imr_txdesc &= ah->ah_txq_status;
	ah->ah_txq_imr_txeol &= ah->ah_txq_status;
	ah->ah_txq_imr_cbrorn &= ah->ah_txq_status;
	ah->ah_txq_imr_cbrurn &= ah->ah_txq_status;
	ah->ah_txq_imr_qtrig &= ah->ah_txq_status;
	ah->ah_txq_imr_nofrm &= ah->ah_txq_status;

	ath5k_hw_reg_write(ah, AR5K_REG_SM(ah->ah_txq_imr_txok,
					AR5K_SIMR0_QCU_TXOK) |
					AR5K_REG_SM(ah->ah_txq_imr_txdesc,
					AR5K_SIMR0_QCU_TXDESC),
					AR5K_SIMR0);

	ath5k_hw_reg_write(ah, AR5K_REG_SM(ah->ah_txq_imr_txerr,
					AR5K_SIMR1_QCU_TXERR) |
					AR5K_REG_SM(ah->ah_txq_imr_txeol,
					AR5K_SIMR1_QCU_TXEOL),
					AR5K_SIMR1);

	
	AR5K_REG_DISABLE_BITS(ah, AR5K_SIMR2, AR5K_SIMR2_QCU_TXURN);
	AR5K_REG_ENABLE_BITS(ah, AR5K_SIMR2,
				AR5K_REG_SM(ah->ah_txq_imr_txurn,
				AR5K_SIMR2_QCU_TXURN));

	ath5k_hw_reg_write(ah, AR5K_REG_SM(ah->ah_txq_imr_cbrorn,
				AR5K_SIMR3_QCBRORN) |
				AR5K_REG_SM(ah->ah_txq_imr_cbrurn,
				AR5K_SIMR3_QCBRURN),
				AR5K_SIMR3);

	ath5k_hw_reg_write(ah, AR5K_REG_SM(ah->ah_txq_imr_qtrig,
				AR5K_SIMR4_QTRIG), AR5K_SIMR4);

	
	ath5k_hw_reg_write(ah, AR5K_REG_SM(ah->ah_txq_imr_nofrm,
				AR5K_TXNOFRM_QCU), AR5K_TXNOFRM);

	if (ah->ah_txq_imr_nofrm == 0)
		ath5k_hw_reg_write(ah, 0, AR5K_TXNOFRM);

	
	AR5K_REG_WRITE_Q(ah, AR5K_QUEUE_QCUMASK(queue), queue);

	return 0;
}



int ath5k_hw_set_ifs_intervals(struct ath5k_hw *ah, unsigned int slot_time)
{
	struct ieee80211_channel *channel = ah->ah_current_channel;
	struct ieee80211_rate *rate;
	u32 ack_tx_time, eifs, eifs_clock, sifs, sifs_clock;
	u32 slot_time_clock = ath5k_hw_htoclock(ah, slot_time);

	if (slot_time < 6 || slot_time_clock > AR5K_SLOT_TIME_MAX)
		return -EINVAL;

	sifs = ath5k_hw_get_default_sifs(ah);
	sifs_clock = ath5k_hw_htoclock(ah, sifs - 2);

	if (channel->band == IEEE80211_BAND_5GHZ)
		rate = &ah->sbands[IEEE80211_BAND_5GHZ].bitrates[0];
	else
		rate = &ah->sbands[IEEE80211_BAND_2GHZ].bitrates[0];

	ack_tx_time = ath5k_hw_get_frame_duration(ah, 10, rate, false);

	
	eifs = ack_tx_time + sifs + 2 * slot_time;
	eifs_clock = ath5k_hw_htoclock(ah, eifs);

	
	if (ah->ah_version == AR5K_AR5210) {
		u32 pifs, pifs_clock, difs, difs_clock;

		
		ath5k_hw_reg_write(ah, slot_time_clock, AR5K_SLOT_TIME);

		
		eifs_clock = AR5K_REG_SM(eifs_clock, AR5K_IFS1_EIFS);

		
		pifs = slot_time + sifs;
		pifs_clock = ath5k_hw_htoclock(ah, pifs);
		pifs_clock = AR5K_REG_SM(pifs_clock, AR5K_IFS1_PIFS);

		
		difs = sifs + 2 * slot_time;
		difs_clock = ath5k_hw_htoclock(ah, difs);

		
		ath5k_hw_reg_write(ah, (difs_clock <<
				AR5K_IFS0_DIFS_S) | sifs_clock,
				AR5K_IFS0);

		
		ath5k_hw_reg_write(ah, pifs_clock | eifs_clock |
				(AR5K_INIT_CARR_SENSE_EN << AR5K_IFS1_CS_EN_S),
				AR5K_IFS1);

		return 0;
	}

	
	ath5k_hw_reg_write(ah, slot_time_clock, AR5K_DCU_GBL_IFS_SLOT);

	
	ath5k_hw_reg_write(ah, eifs_clock, AR5K_DCU_GBL_IFS_EIFS);

	
	AR5K_REG_WRITE_BITS(ah, AR5K_DCU_GBL_IFS_MISC,
				AR5K_DCU_GBL_IFS_MISC_SIFS_DUR_USEC,
				sifs);

	
	ath5k_hw_reg_write(ah, sifs_clock, AR5K_DCU_GBL_IFS_SIFS);

	return 0;
}


int
ath5k_hw_init_queues(struct ath5k_hw *ah)
{
	int i, ret;

	
	

	if (ah->ah_version != AR5K_AR5210)
		for (i = 0; i < ah->ah_capabilities.cap_queues.q_tx_num; i++) {
			ret = ath5k_hw_reset_tx_queue(ah, i);
			if (ret) {
				ATH5K_ERR(ah,
					"failed to reset TX queue #%d\n", i);
				return ret;
			}
		}
	else
		ath5k_hw_set_tx_retry_limits(ah, 0);

	
	if (ah->ah_bwmode == AR5K_BWMODE_40MHZ)
		AR5K_REG_ENABLE_BITS(ah, AR5K_DCU_GBL_IFS_MISC,
				AR5K_DCU_GBL_IFS_MISC_TURBO_MODE);

	if (!ah->ah_coverage_class) {
		unsigned int slot_time = ath5k_hw_get_default_slottime(ah);
		ath5k_hw_set_ifs_intervals(ah, slot_time);
	}

	return 0;
}
