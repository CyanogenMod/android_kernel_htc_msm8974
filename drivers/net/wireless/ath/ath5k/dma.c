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



void
ath5k_hw_start_rx_dma(struct ath5k_hw *ah)
{
	ath5k_hw_reg_write(ah, AR5K_CR_RXE, AR5K_CR);
	ath5k_hw_reg_read(ah, AR5K_CR);
}

static int
ath5k_hw_stop_rx_dma(struct ath5k_hw *ah)
{
	unsigned int i;

	ath5k_hw_reg_write(ah, AR5K_CR_RXD, AR5K_CR);

	for (i = 1000; i > 0 &&
			(ath5k_hw_reg_read(ah, AR5K_CR) & AR5K_CR_RXE) != 0;
			i--)
		udelay(100);

	if (!i)
		ATH5K_DBG(ah, ATH5K_DEBUG_DMA,
				"failed to stop RX DMA !\n");

	return i ? 0 : -EBUSY;
}

u32
ath5k_hw_get_rxdp(struct ath5k_hw *ah)
{
	return ath5k_hw_reg_read(ah, AR5K_RXDP);
}

int
ath5k_hw_set_rxdp(struct ath5k_hw *ah, u32 phys_addr)
{
	if (ath5k_hw_reg_read(ah, AR5K_CR) & AR5K_CR_RXE) {
		ATH5K_DBG(ah, ATH5K_DEBUG_DMA,
				"tried to set RXDP while rx was active !\n");
		return -EIO;
	}

	ath5k_hw_reg_write(ah, phys_addr, AR5K_RXDP);
	return 0;
}



int
ath5k_hw_start_tx_dma(struct ath5k_hw *ah, unsigned int queue)
{
	u32 tx_queue;

	AR5K_ASSERT_ENTRY(queue, ah->ah_capabilities.cap_queues.q_tx_num);

	
	if (ah->ah_txq[queue].tqi_type == AR5K_TX_QUEUE_INACTIVE)
		return -EINVAL;

	if (ah->ah_version == AR5K_AR5210) {
		tx_queue = ath5k_hw_reg_read(ah, AR5K_CR);

		switch (ah->ah_txq[queue].tqi_type) {
		case AR5K_TX_QUEUE_DATA:
			tx_queue |= AR5K_CR_TXE0 & ~AR5K_CR_TXD0;
			break;
		case AR5K_TX_QUEUE_BEACON:
			tx_queue |= AR5K_CR_TXE1 & ~AR5K_CR_TXD1;
			ath5k_hw_reg_write(ah, AR5K_BCR_TQ1V | AR5K_BCR_BDMAE,
					AR5K_BSR);
			break;
		case AR5K_TX_QUEUE_CAB:
			tx_queue |= AR5K_CR_TXE1 & ~AR5K_CR_TXD1;
			ath5k_hw_reg_write(ah, AR5K_BCR_TQ1FV | AR5K_BCR_TQ1V |
				AR5K_BCR_BDMAE, AR5K_BSR);
			break;
		default:
			return -EINVAL;
		}
		
		ath5k_hw_reg_write(ah, tx_queue, AR5K_CR);
		ath5k_hw_reg_read(ah, AR5K_CR);
	} else {
		
		if (AR5K_REG_READ_Q(ah, AR5K_QCU_TXD, queue))
			return -EIO;

		
		AR5K_REG_WRITE_Q(ah, AR5K_QCU_TXE, queue);
	}

	return 0;
}

static int
ath5k_hw_stop_tx_dma(struct ath5k_hw *ah, unsigned int queue)
{
	unsigned int i = 40;
	u32 tx_queue, pending;

	AR5K_ASSERT_ENTRY(queue, ah->ah_capabilities.cap_queues.q_tx_num);

	
	if (ah->ah_txq[queue].tqi_type == AR5K_TX_QUEUE_INACTIVE)
		return -EINVAL;

	if (ah->ah_version == AR5K_AR5210) {
		tx_queue = ath5k_hw_reg_read(ah, AR5K_CR);

		switch (ah->ah_txq[queue].tqi_type) {
		case AR5K_TX_QUEUE_DATA:
			tx_queue |= AR5K_CR_TXD0 & ~AR5K_CR_TXE0;
			break;
		case AR5K_TX_QUEUE_BEACON:
		case AR5K_TX_QUEUE_CAB:
			
			tx_queue |= AR5K_CR_TXD1 & ~AR5K_CR_TXD1;
			ath5k_hw_reg_write(ah, 0, AR5K_BSR);
			break;
		default:
			return -EINVAL;
		}

		
		ath5k_hw_reg_write(ah, tx_queue, AR5K_CR);
		ath5k_hw_reg_read(ah, AR5K_CR);
	} else {

		AR5K_REG_ENABLE_BITS(ah, AR5K_QUEUE_MISC(queue),
					AR5K_QCU_MISC_DCU_EARLY);

		AR5K_REG_WRITE_Q(ah, AR5K_QCU_TXD, queue);

		
		for (i = 1000; i > 0 &&
		(AR5K_REG_READ_Q(ah, AR5K_QCU_TXE, queue) != 0);
		i--)
			udelay(100);

		if (AR5K_REG_READ_Q(ah, AR5K_QCU_TXE, queue))
			ATH5K_DBG(ah, ATH5K_DEBUG_DMA,
				"queue %i didn't stop !\n", queue);

		
		i = 1000;
		do {
			pending = ath5k_hw_reg_read(ah,
				AR5K_QUEUE_STATUS(queue)) &
				AR5K_QCU_STS_FRMPENDCNT;
			udelay(100);
		} while (--i && pending);

		if (ah->ah_mac_version >= (AR5K_SREV_AR2414 >> 4) &&
		    pending) {
			
			ath5k_hw_reg_write(ah,
				AR5K_REG_SM(100, AR5K_QUIET_CTL2_QT_PER)|
				AR5K_REG_SM(10, AR5K_QUIET_CTL2_QT_DUR),
				AR5K_QUIET_CTL2);

			
			ath5k_hw_reg_write(ah,
				AR5K_QUIET_CTL1_QT_EN |
				AR5K_REG_SM(ath5k_hw_reg_read(ah,
						AR5K_TSF_L32_5211) >> 10,
						AR5K_QUIET_CTL1_NEXT_QT_TSF),
				AR5K_QUIET_CTL1);

			
			AR5K_REG_ENABLE_BITS(ah, AR5K_DIAG_SW_5211,
					AR5K_DIAG_SW_CHANNEL_IDLE_HIGH);

			
			udelay(400);
			AR5K_REG_DISABLE_BITS(ah, AR5K_QUIET_CTL1,
						AR5K_QUIET_CTL1_QT_EN);

			
			i = 100;
			do {
				pending = ath5k_hw_reg_read(ah,
					AR5K_QUEUE_STATUS(queue)) &
					AR5K_QCU_STS_FRMPENDCNT;
				udelay(100);
			} while (--i && pending);

			AR5K_REG_DISABLE_BITS(ah, AR5K_DIAG_SW_5211,
					AR5K_DIAG_SW_CHANNEL_IDLE_HIGH);

			if (pending)
				ATH5K_DBG(ah, ATH5K_DEBUG_DMA,
					"quiet mechanism didn't work q:%i !\n",
					queue);
		}

		AR5K_REG_DISABLE_BITS(ah, AR5K_QUEUE_MISC(queue),
					AR5K_QCU_MISC_DCU_EARLY);

		
		ath5k_hw_reg_write(ah, 0, AR5K_QCU_TXD);
		if (pending) {
			ATH5K_DBG(ah, ATH5K_DEBUG_DMA,
					"tx dma didn't stop (q:%i, frm:%i) !\n",
					queue, pending);
			return -EBUSY;
		}
	}

	
	return 0;
}

int
ath5k_hw_stop_beacon_queue(struct ath5k_hw *ah, unsigned int queue)
{
	int ret;
	ret = ath5k_hw_stop_tx_dma(ah, queue);
	if (ret) {
		ATH5K_DBG(ah, ATH5K_DEBUG_DMA,
				"beacon queue didn't stop !\n");
		return -EIO;
	}
	return 0;
}

u32
ath5k_hw_get_txdp(struct ath5k_hw *ah, unsigned int queue)
{
	u16 tx_reg;

	AR5K_ASSERT_ENTRY(queue, ah->ah_capabilities.cap_queues.q_tx_num);

	
	if (ah->ah_version == AR5K_AR5210) {
		switch (ah->ah_txq[queue].tqi_type) {
		case AR5K_TX_QUEUE_DATA:
			tx_reg = AR5K_NOQCU_TXDP0;
			break;
		case AR5K_TX_QUEUE_BEACON:
		case AR5K_TX_QUEUE_CAB:
			tx_reg = AR5K_NOQCU_TXDP1;
			break;
		default:
			return 0xffffffff;
		}
	} else {
		tx_reg = AR5K_QUEUE_TXDP(queue);
	}

	return ath5k_hw_reg_read(ah, tx_reg);
}

int
ath5k_hw_set_txdp(struct ath5k_hw *ah, unsigned int queue, u32 phys_addr)
{
	u16 tx_reg;

	AR5K_ASSERT_ENTRY(queue, ah->ah_capabilities.cap_queues.q_tx_num);

	if (ah->ah_version == AR5K_AR5210) {
		switch (ah->ah_txq[queue].tqi_type) {
		case AR5K_TX_QUEUE_DATA:
			tx_reg = AR5K_NOQCU_TXDP0;
			break;
		case AR5K_TX_QUEUE_BEACON:
		case AR5K_TX_QUEUE_CAB:
			tx_reg = AR5K_NOQCU_TXDP1;
			break;
		default:
			return -EINVAL;
		}
	} else {
		if (AR5K_REG_READ_Q(ah, AR5K_QCU_TXE, queue))
			return -EIO;

		tx_reg = AR5K_QUEUE_TXDP(queue);
	}

	
	ath5k_hw_reg_write(ah, phys_addr, tx_reg);

	return 0;
}

int
ath5k_hw_update_tx_triglevel(struct ath5k_hw *ah, bool increase)
{
	u32 trigger_level, imr;
	int ret = -EIO;

	imr = ath5k_hw_set_imr(ah, ah->ah_imr & ~AR5K_INT_GLOBAL);

	trigger_level = AR5K_REG_MS(ath5k_hw_reg_read(ah, AR5K_TXCFG),
			AR5K_TXCFG_TXFULL);

	if (!increase) {
		if (--trigger_level < AR5K_TUNE_MIN_TX_FIFO_THRES)
			goto done;
	} else
		trigger_level +=
			((AR5K_TUNE_MAX_TX_FIFO_THRES - trigger_level) / 2);

	if (ah->ah_version == AR5K_AR5210)
		ath5k_hw_reg_write(ah, trigger_level, AR5K_TRIG_LVL);
	else
		AR5K_REG_WRITE_BITS(ah, AR5K_TXCFG,
				AR5K_TXCFG_TXFULL, trigger_level);

	ret = 0;

done:
	ath5k_hw_set_imr(ah, imr);

	return ret;
}



bool
ath5k_hw_is_intr_pending(struct ath5k_hw *ah)
{
	return ath5k_hw_reg_read(ah, AR5K_INTPEND) == 1 ? 1 : 0;
}

int
ath5k_hw_get_isr(struct ath5k_hw *ah, enum ath5k_int *interrupt_mask)
{
	u32 data = 0;

	if (ah->ah_version == AR5K_AR5210) {
		u32 isr = 0;
		isr = ath5k_hw_reg_read(ah, AR5K_ISR);
		if (unlikely(isr == AR5K_INT_NOCARD)) {
			*interrupt_mask = isr;
			return -ENODEV;
		}

		*interrupt_mask = (isr & AR5K_INT_COMMON) & ah->ah_imr;

		
		if (unlikely(isr & (AR5K_ISR_SSERR | AR5K_ISR_MCABT
						| AR5K_ISR_DPERR)))
			*interrupt_mask |= AR5K_INT_FATAL;


		data = isr;
	} else {
		u32 pisr = 0;
		u32 pisr_clear = 0;
		u32 sisr0 = 0;
		u32 sisr1 = 0;
		u32 sisr2 = 0;
		u32 sisr3 = 0;
		u32 sisr4 = 0;

		
		pisr = ath5k_hw_reg_read(ah, AR5K_PISR);
		if (unlikely(pisr == AR5K_INT_NOCARD)) {
			*interrupt_mask = pisr;
			return -ENODEV;
		}

		sisr0 = ath5k_hw_reg_read(ah, AR5K_SISR0);
		sisr1 = ath5k_hw_reg_read(ah, AR5K_SISR1);
		sisr2 = ath5k_hw_reg_read(ah, AR5K_SISR2);
		sisr3 = ath5k_hw_reg_read(ah, AR5K_SISR3);
		sisr4 = ath5k_hw_reg_read(ah, AR5K_SISR4);


		pisr_clear = pisr & ~AR5K_ISR_BITS_FROM_SISRS;

		ath5k_hw_reg_write(ah, sisr0, AR5K_SISR0);
		ath5k_hw_reg_write(ah, sisr1, AR5K_SISR1);
		ath5k_hw_reg_write(ah, sisr2, AR5K_SISR2);
		ath5k_hw_reg_write(ah, sisr3, AR5K_SISR3);
		ath5k_hw_reg_write(ah, sisr4, AR5K_SISR4);
		ath5k_hw_reg_write(ah, pisr_clear, AR5K_PISR);
		
		ath5k_hw_reg_read(ah, AR5K_PISR);

		*interrupt_mask = (pisr & AR5K_INT_COMMON) & ah->ah_imr;


		if (pisr & AR5K_ISR_TXOK)
			ah->ah_txq_isr_txok_all |= AR5K_REG_MS(sisr0,
						AR5K_SISR0_QCU_TXOK);

		if (pisr & AR5K_ISR_TXDESC)
			ah->ah_txq_isr_txok_all |= AR5K_REG_MS(sisr0,
						AR5K_SISR0_QCU_TXDESC);

		if (pisr & AR5K_ISR_TXERR)
			ah->ah_txq_isr_txok_all |= AR5K_REG_MS(sisr1,
						AR5K_SISR1_QCU_TXERR);

		if (pisr & AR5K_ISR_TXEOL)
			ah->ah_txq_isr_txok_all |= AR5K_REG_MS(sisr1,
						AR5K_SISR1_QCU_TXEOL);

		if (pisr & AR5K_ISR_TXURN)
			ah->ah_txq_isr_txurn |= AR5K_REG_MS(sisr2,
						AR5K_SISR2_QCU_TXURN);

		

		
		if (pisr & AR5K_ISR_TIM)
			*interrupt_mask |= AR5K_INT_TIM;

		
		if (pisr & AR5K_ISR_BCNMISC) {
			if (sisr2 & AR5K_SISR2_TIM)
				*interrupt_mask |= AR5K_INT_TIM;
			if (sisr2 & AR5K_SISR2_DTIM)
				*interrupt_mask |= AR5K_INT_DTIM;
			if (sisr2 & AR5K_SISR2_DTIM_SYNC)
				*interrupt_mask |= AR5K_INT_DTIM_SYNC;
			if (sisr2 & AR5K_SISR2_BCN_TIMEOUT)
				*interrupt_mask |= AR5K_INT_BCN_TIMEOUT;
			if (sisr2 & AR5K_SISR2_CAB_TIMEOUT)
				*interrupt_mask |= AR5K_INT_CAB_TIMEOUT;
		}

		

		if (unlikely(pisr & (AR5K_ISR_HIUERR)))
			*interrupt_mask |= AR5K_INT_FATAL;

		
		if (unlikely(pisr & (AR5K_ISR_BNR)))
			*interrupt_mask |= AR5K_INT_BNR;

		
		if (unlikely(pisr & (AR5K_ISR_QCBRORN))) {
			*interrupt_mask |= AR5K_INT_QCBRORN;
			ah->ah_txq_isr_qcborn |= AR5K_REG_MS(sisr3,
						AR5K_SISR3_QCBRORN);
		}

		
		if (unlikely(pisr & (AR5K_ISR_QCBRURN))) {
			*interrupt_mask |= AR5K_INT_QCBRURN;
			ah->ah_txq_isr_qcburn |= AR5K_REG_MS(sisr3,
						AR5K_SISR3_QCBRURN);
		}

		
		if (unlikely(pisr & (AR5K_ISR_QTRIG))) {
			*interrupt_mask |= AR5K_INT_QTRIG;
			ah->ah_txq_isr_qtrig |= AR5K_REG_MS(sisr4,
						AR5K_SISR4_QTRIG);
		}

		data = pisr;
	}

	if (unlikely(*interrupt_mask == 0 && net_ratelimit()))
		ATH5K_PRINTF("ISR: 0x%08x IMR: 0x%08x\n", data, ah->ah_imr);

	return 0;
}

enum ath5k_int
ath5k_hw_set_imr(struct ath5k_hw *ah, enum ath5k_int new_mask)
{
	enum ath5k_int old_mask, int_mask;

	old_mask = ah->ah_imr;

	if (old_mask & AR5K_INT_GLOBAL) {
		ath5k_hw_reg_write(ah, AR5K_IER_DISABLE, AR5K_IER);
		ath5k_hw_reg_read(ah, AR5K_IER);
	}

	int_mask = new_mask & AR5K_INT_COMMON;

	if (ah->ah_version != AR5K_AR5210) {
		
		u32 simr2 = ath5k_hw_reg_read(ah, AR5K_SIMR2)
				& AR5K_SIMR2_QCU_TXURN;

		
		if (new_mask & AR5K_INT_FATAL) {
			int_mask |= AR5K_IMR_HIUERR;
			simr2 |= (AR5K_SIMR2_MCABT | AR5K_SIMR2_SSERR
				| AR5K_SIMR2_DPERR);
		}

		
		if (new_mask & AR5K_INT_TIM)
			int_mask |= AR5K_IMR_TIM;

		if (new_mask & AR5K_INT_TIM)
			simr2 |= AR5K_SISR2_TIM;
		if (new_mask & AR5K_INT_DTIM)
			simr2 |= AR5K_SISR2_DTIM;
		if (new_mask & AR5K_INT_DTIM_SYNC)
			simr2 |= AR5K_SISR2_DTIM_SYNC;
		if (new_mask & AR5K_INT_BCN_TIMEOUT)
			simr2 |= AR5K_SISR2_BCN_TIMEOUT;
		if (new_mask & AR5K_INT_CAB_TIMEOUT)
			simr2 |= AR5K_SISR2_CAB_TIMEOUT;

		
		if (new_mask & AR5K_INT_BNR)
			int_mask |= AR5K_INT_BNR;

		ath5k_hw_reg_write(ah, int_mask, AR5K_PIMR);
		ath5k_hw_reg_write(ah, simr2, AR5K_SIMR2);

	} else {
		
		if (new_mask & AR5K_INT_FATAL)
			int_mask |= (AR5K_IMR_SSERR | AR5K_IMR_MCABT
				| AR5K_IMR_HIUERR | AR5K_IMR_DPERR);

		
		ath5k_hw_reg_write(ah, int_mask, AR5K_IMR);
	}

	if (!(new_mask & AR5K_INT_RXNOFRM))
		ath5k_hw_reg_write(ah, 0, AR5K_RXNOFRM);

	
	ah->ah_imr = new_mask;

	
	if (new_mask & AR5K_INT_GLOBAL) {
		ath5k_hw_reg_write(ah, AR5K_IER_ENABLE, AR5K_IER);
		ath5k_hw_reg_read(ah, AR5K_IER);
	}

	return old_mask;
}



void
ath5k_hw_dma_init(struct ath5k_hw *ah)
{
	if (ah->ah_version != AR5K_AR5210) {
		AR5K_REG_WRITE_BITS(ah, AR5K_TXCFG,
			AR5K_TXCFG_SDMAMR, AR5K_DMASIZE_128B);
		AR5K_REG_WRITE_BITS(ah, AR5K_RXCFG,
			AR5K_RXCFG_SDMAMW, AR5K_DMASIZE_128B);
	}

	
	if (ah->ah_version != AR5K_AR5210)
		ath5k_hw_set_imr(ah, ah->ah_imr);

}

int
ath5k_hw_dma_stop(struct ath5k_hw *ah)
{
	int i, qmax, err;
	err = 0;

	
	ath5k_hw_set_imr(ah, 0);

	
	err = ath5k_hw_stop_rx_dma(ah);
	if (err)
		return err;

	if (ah->ah_version != AR5K_AR5210) {
		ath5k_hw_reg_write(ah, 0xffffffff, AR5K_PISR);
		qmax = AR5K_NUM_TX_QUEUES;
	} else {
		
		ath5k_hw_reg_read(ah, AR5K_ISR);
		qmax = AR5K_NUM_TX_QUEUES_NOQCU;
	}

	for (i = 0; i < qmax; i++) {
		err = ath5k_hw_stop_tx_dma(ah, i);
		
		if (err && err != -EINVAL)
			return err;
	}

	return 0;
}
