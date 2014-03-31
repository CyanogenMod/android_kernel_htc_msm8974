/*
 * Copyright (C) ST-Ericsson AB 2010
 * Contact: Sjur Brendeland / sjur.brandeland@stericsson.com
 * Author:  Daniel Martensson / Daniel.Martensson@stericsson.com
 * License terms: GNU General Public License (GPL) version 2.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <net/caif/caif_spi.h>

#ifndef CONFIG_CAIF_SPI_SYNC
#define SPI_DATA_POS 0
static inline int forward_to_spi_cmd(struct cfspi *cfspi)
{
	return cfspi->rx_cpck_len;
}
#else
#define SPI_DATA_POS SPI_CMD_SZ
static inline int forward_to_spi_cmd(struct cfspi *cfspi)
{
	return 0;
}
#endif

int spi_frm_align = 2;

int spi_up_head_align   = 1 << 1;
int spi_up_tail_align   = 1 << 0;
int spi_down_head_align = 1 << 2;
int spi_down_tail_align = 1 << 1;

#ifdef CONFIG_DEBUG_FS
static inline void debugfs_store_prev(struct cfspi *cfspi)
{
	
	cfspi->pcmd = cfspi->cmd;
	
	cfspi->tx_ppck_len = cfspi->tx_cpck_len;
	cfspi->rx_ppck_len = cfspi->rx_cpck_len;
}
#else
static inline void debugfs_store_prev(struct cfspi *cfspi)
{
}
#endif

void cfspi_xfer(struct work_struct *work)
{
	struct cfspi *cfspi;
	u8 *ptr = NULL;
	unsigned long flags;
	int ret;
	cfspi = container_of(work, struct cfspi, work);

	
	cfspi->cmd = SPI_CMD_EOT;

	for (;;) {

		cfspi_dbg_state(cfspi, CFSPI_STATE_WAITING);

		
		wait_event_interruptible(cfspi->wait,
				 test_bit(SPI_XFER, &cfspi->state) ||
				 test_bit(SPI_TERMINATE, &cfspi->state));

		if (test_bit(SPI_TERMINATE, &cfspi->state))
			return;

#if CFSPI_DBG_PREFILL
		
		memset(cfspi->xfer.va_tx, 0xFF, SPI_DMA_BUF_LEN);
		memset(cfspi->xfer.va_rx, 0xFF, SPI_DMA_BUF_LEN);
#endif	

		cfspi_dbg_state(cfspi, CFSPI_STATE_AWAKE);

	
		if (cfspi->tx_cpck_len) {
			int len;

			cfspi_dbg_state(cfspi, CFSPI_STATE_FETCH_PKT);

			
			ptr = (u8 *) cfspi->xfer.va_tx;
			ptr += SPI_IND_SZ;
			len = cfspi_xmitfrm(cfspi, ptr, cfspi->tx_cpck_len);
			WARN_ON(len != cfspi->tx_cpck_len);
	}

		cfspi_dbg_state(cfspi, CFSPI_STATE_GET_NEXT);

		
		cfspi->tx_npck_len = cfspi_xmitlen(cfspi);

		WARN_ON(cfspi->tx_npck_len > SPI_DMA_BUF_LEN);

		ptr = (u8 *) cfspi->xfer.va_tx;
		*ptr++ = SPI_CMD_IND;
		*ptr++ = (SPI_CMD_IND  & 0xFF00) >> 8;
		*ptr++ = cfspi->tx_npck_len & 0x00FF;
		*ptr++ = (cfspi->tx_npck_len & 0xFF00) >> 8;

		
		cfspi->xfer.tx_dma_len = cfspi->tx_cpck_len + SPI_IND_SZ;
		cfspi->xfer.rx_dma_len = cfspi->rx_cpck_len + SPI_CMD_SZ;

		
		if (cfspi->tx_cpck_len &&
			(cfspi->xfer.tx_dma_len % spi_frm_align)) {

			cfspi->xfer.tx_dma_len += spi_frm_align -
			    (cfspi->xfer.tx_dma_len % spi_frm_align);
		}

		
		if (cfspi->rx_cpck_len &&
			(cfspi->xfer.rx_dma_len % spi_frm_align)) {

			cfspi->xfer.rx_dma_len += spi_frm_align -
			    (cfspi->xfer.rx_dma_len % spi_frm_align);
		}

		cfspi_dbg_state(cfspi, CFSPI_STATE_INIT_XFER);

		
		ret = cfspi->dev->init_xfer(&cfspi->xfer, cfspi->dev);
		WARN_ON(ret);

		cfspi_dbg_state(cfspi, CFSPI_STATE_WAIT_ACTIVE);

		udelay(MIN_TRANSITION_TIME_USEC);

		cfspi_dbg_state(cfspi, CFSPI_STATE_SIG_ACTIVE);

		
		cfspi->dev->sig_xfer(true, cfspi->dev);

		cfspi_dbg_state(cfspi, CFSPI_STATE_WAIT_XFER_DONE);

		
		wait_for_completion(&cfspi->comp);

		cfspi_dbg_state(cfspi, CFSPI_STATE_XFER_DONE);

		if (cfspi->cmd == SPI_CMD_EOT) {
			clear_bit(SPI_SS_ON, &cfspi->state);
		}

		cfspi_dbg_state(cfspi, CFSPI_STATE_WAIT_INACTIVE);

		
		if (SPI_XFER_TIME_USEC(cfspi->xfer.tx_dma_len,
					cfspi->dev->clk_mhz) <
			MIN_TRANSITION_TIME_USEC) {

			udelay(MIN_TRANSITION_TIME_USEC -
				SPI_XFER_TIME_USEC
				(cfspi->xfer.tx_dma_len, cfspi->dev->clk_mhz));
		}

		cfspi_dbg_state(cfspi, CFSPI_STATE_SIG_INACTIVE);

		
		cfspi->dev->sig_xfer(false, cfspi->dev);

		
		if (cfspi->rx_cpck_len) {
			int len;

			cfspi_dbg_state(cfspi, CFSPI_STATE_DELIVER_PKT);

			
			ptr = ((u8 *)(cfspi->xfer.va_rx + SPI_DATA_POS));

			len = cfspi_rxfrm(cfspi, ptr, cfspi->rx_cpck_len);
			WARN_ON(len != cfspi->rx_cpck_len);
		}

		
		ptr = (u8 *) cfspi->xfer.va_rx;

		ptr += forward_to_spi_cmd(cfspi);

		cfspi->cmd = *ptr++;
		cfspi->cmd |= ((*ptr++) << 8) & 0xFF00;
		cfspi->rx_npck_len = *ptr++;
		cfspi->rx_npck_len |= ((*ptr++) << 8) & 0xFF00;

		WARN_ON(cfspi->rx_npck_len > SPI_DMA_BUF_LEN);
		WARN_ON(cfspi->cmd > SPI_CMD_EOT);

		debugfs_store_prev(cfspi);

		
		if (cfspi->cmd == SPI_CMD_EOT) {
			
			cfspi->tx_cpck_len = 0;
			cfspi->rx_cpck_len = 0;
		} else {
			
			cfspi->tx_cpck_len = cfspi->tx_npck_len;
			cfspi->rx_cpck_len = cfspi->rx_npck_len;
		}

		spin_lock_irqsave(&cfspi->lock, flags);
		if (cfspi->cmd == SPI_CMD_EOT && !cfspi_xmitlen(cfspi)
			&& !test_bit(SPI_SS_ON, &cfspi->state))
			clear_bit(SPI_XFER, &cfspi->state);

		spin_unlock_irqrestore(&cfspi->lock, flags);
	}
}

struct platform_driver cfspi_spi_driver = {
	.probe = cfspi_spi_probe,
	.remove = cfspi_spi_remove,
	.driver = {
		   .name = "cfspi_sspi",
		   .owner = THIS_MODULE,
		   },
};
