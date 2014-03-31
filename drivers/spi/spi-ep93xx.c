/*
 * Driver for Cirrus Logic EP93xx SPI controller.
 *
 * Copyright (C) 2010-2011 Mika Westerberg
 *
 * Explicit FIFO handling code was inspired by amba-pl022 driver.
 *
 * Chip select support using other than built-in GPIOs by H. Hartley Sweeten.
 *
 * For more information about the SPI controller see documentation on Cirrus
 * Logic web site:
 *     http://www.cirrus.com/en/pubs/manual/EP93xx_Users_Guide_UM1.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/scatterlist.h>
#include <linux/spi/spi.h>

#include <mach/dma.h>
#include <mach/ep93xx_spi.h>

#define SSPCR0			0x0000
#define SSPCR0_MODE_SHIFT	6
#define SSPCR0_SCR_SHIFT	8

#define SSPCR1			0x0004
#define SSPCR1_RIE		BIT(0)
#define SSPCR1_TIE		BIT(1)
#define SSPCR1_RORIE		BIT(2)
#define SSPCR1_LBM		BIT(3)
#define SSPCR1_SSE		BIT(4)
#define SSPCR1_MS		BIT(5)
#define SSPCR1_SOD		BIT(6)

#define SSPDR			0x0008

#define SSPSR			0x000c
#define SSPSR_TFE		BIT(0)
#define SSPSR_TNF		BIT(1)
#define SSPSR_RNE		BIT(2)
#define SSPSR_RFF		BIT(3)
#define SSPSR_BSY		BIT(4)
#define SSPCPSR			0x0010

#define SSPIIR			0x0014
#define SSPIIR_RIS		BIT(0)
#define SSPIIR_TIS		BIT(1)
#define SSPIIR_RORIS		BIT(2)
#define SSPICR			SSPIIR

#define SPI_TIMEOUT		5
#define SPI_FIFO_SIZE		8

/**
 * struct ep93xx_spi - EP93xx SPI controller structure
 * @lock: spinlock that protects concurrent accesses to fields @running,
 *        @current_msg and @msg_queue
 * @pdev: pointer to platform device
 * @clk: clock for the controller
 * @regs_base: pointer to ioremap()'d registers
 * @sspdr_phys: physical address of the SSPDR register
 * @irq: IRQ number used by the driver
 * @min_rate: minimum clock rate (in Hz) supported by the controller
 * @max_rate: maximum clock rate (in Hz) supported by the controller
 * @running: is the queue running
 * @wq: workqueue used by the driver
 * @msg_work: work that is queued for the driver
 * @wait: wait here until given transfer is completed
 * @msg_queue: queue for the messages
 * @current_msg: message that is currently processed (or %NULL if none)
 * @tx: current byte in transfer to transmit
 * @rx: current byte in transfer to receive
 * @fifo_level: how full is FIFO (%0..%SPI_FIFO_SIZE - %1). Receiving one
 *              frame decreases this level and sending one frame increases it.
 * @dma_rx: RX DMA channel
 * @dma_tx: TX DMA channel
 * @dma_rx_data: RX parameters passed to the DMA engine
 * @dma_tx_data: TX parameters passed to the DMA engine
 * @rx_sgt: sg table for RX transfers
 * @tx_sgt: sg table for TX transfers
 * @zeropage: dummy page used as RX buffer when only TX buffer is passed in by
 *            the client
 *
 * This structure holds EP93xx SPI controller specific information. When
 * @running is %true, driver accepts transfer requests from protocol drivers.
 * @current_msg is used to hold pointer to the message that is currently
 * processed. If @current_msg is %NULL, it means that no processing is going
 * on.
 *
 * Most of the fields are only written once and they can be accessed without
 * taking the @lock. Fields that are accessed concurrently are: @current_msg,
 * @running, and @msg_queue.
 */
struct ep93xx_spi {
	spinlock_t			lock;
	const struct platform_device	*pdev;
	struct clk			*clk;
	void __iomem			*regs_base;
	unsigned long			sspdr_phys;
	int				irq;
	unsigned long			min_rate;
	unsigned long			max_rate;
	bool				running;
	struct workqueue_struct		*wq;
	struct work_struct		msg_work;
	struct completion		wait;
	struct list_head		msg_queue;
	struct spi_message		*current_msg;
	size_t				tx;
	size_t				rx;
	size_t				fifo_level;
	struct dma_chan			*dma_rx;
	struct dma_chan			*dma_tx;
	struct ep93xx_dma_data		dma_rx_data;
	struct ep93xx_dma_data		dma_tx_data;
	struct sg_table			rx_sgt;
	struct sg_table			tx_sgt;
	void				*zeropage;
};

/**
 * struct ep93xx_spi_chip - SPI device hardware settings
 * @spi: back pointer to the SPI device
 * @rate: max rate in hz this chip supports
 * @div_cpsr: cpsr (pre-scaler) divider
 * @div_scr: scr divider
 * @dss: bits per word (4 - 16 bits)
 * @ops: private chip operations
 *
 * This structure is used to store hardware register specific settings for each
 * SPI device. Settings are written to hardware by function
 * ep93xx_spi_chip_setup().
 */
struct ep93xx_spi_chip {
	const struct spi_device		*spi;
	unsigned long			rate;
	u8				div_cpsr;
	u8				div_scr;
	u8				dss;
	struct ep93xx_spi_chip_ops	*ops;
};

#define bits_per_word_to_dss(bpw)	((bpw) - 1)

static inline void
ep93xx_spi_write_u8(const struct ep93xx_spi *espi, u16 reg, u8 value)
{
	__raw_writeb(value, espi->regs_base + reg);
}

static inline u8
ep93xx_spi_read_u8(const struct ep93xx_spi *spi, u16 reg)
{
	return __raw_readb(spi->regs_base + reg);
}

static inline void
ep93xx_spi_write_u16(const struct ep93xx_spi *espi, u16 reg, u16 value)
{
	__raw_writew(value, espi->regs_base + reg);
}

static inline u16
ep93xx_spi_read_u16(const struct ep93xx_spi *spi, u16 reg)
{
	return __raw_readw(spi->regs_base + reg);
}

static int ep93xx_spi_enable(const struct ep93xx_spi *espi)
{
	u8 regval;
	int err;

	err = clk_enable(espi->clk);
	if (err)
		return err;

	regval = ep93xx_spi_read_u8(espi, SSPCR1);
	regval |= SSPCR1_SSE;
	ep93xx_spi_write_u8(espi, SSPCR1, regval);

	return 0;
}

static void ep93xx_spi_disable(const struct ep93xx_spi *espi)
{
	u8 regval;

	regval = ep93xx_spi_read_u8(espi, SSPCR1);
	regval &= ~SSPCR1_SSE;
	ep93xx_spi_write_u8(espi, SSPCR1, regval);

	clk_disable(espi->clk);
}

static void ep93xx_spi_enable_interrupts(const struct ep93xx_spi *espi)
{
	u8 regval;

	regval = ep93xx_spi_read_u8(espi, SSPCR1);
	regval |= (SSPCR1_RORIE | SSPCR1_TIE | SSPCR1_RIE);
	ep93xx_spi_write_u8(espi, SSPCR1, regval);
}

static void ep93xx_spi_disable_interrupts(const struct ep93xx_spi *espi)
{
	u8 regval;

	regval = ep93xx_spi_read_u8(espi, SSPCR1);
	regval &= ~(SSPCR1_RORIE | SSPCR1_TIE | SSPCR1_RIE);
	ep93xx_spi_write_u8(espi, SSPCR1, regval);
}

static int ep93xx_spi_calc_divisors(const struct ep93xx_spi *espi,
				    struct ep93xx_spi_chip *chip,
				    unsigned long rate)
{
	unsigned long spi_clk_rate = clk_get_rate(espi->clk);
	int cpsr, scr;

	rate = clamp(rate, espi->min_rate, espi->max_rate);

	for (cpsr = 2; cpsr <= 254; cpsr += 2) {
		for (scr = 0; scr <= 255; scr++) {
			if ((spi_clk_rate / (cpsr * (scr + 1))) <= rate) {
				chip->div_scr = (u8)scr;
				chip->div_cpsr = (u8)cpsr;
				return 0;
			}
		}
	}

	return -EINVAL;
}

static void ep93xx_spi_cs_control(struct spi_device *spi, bool control)
{
	struct ep93xx_spi_chip *chip = spi_get_ctldata(spi);
	int value = (spi->mode & SPI_CS_HIGH) ? control : !control;

	if (chip->ops && chip->ops->cs_control)
		chip->ops->cs_control(spi, value);
}

static int ep93xx_spi_setup(struct spi_device *spi)
{
	struct ep93xx_spi *espi = spi_master_get_devdata(spi->master);
	struct ep93xx_spi_chip *chip;

	if (spi->bits_per_word < 4 || spi->bits_per_word > 16) {
		dev_err(&espi->pdev->dev, "invalid bits per word %d\n",
			spi->bits_per_word);
		return -EINVAL;
	}

	chip = spi_get_ctldata(spi);
	if (!chip) {
		dev_dbg(&espi->pdev->dev, "initial setup for %s\n",
			spi->modalias);

		chip = kzalloc(sizeof(*chip), GFP_KERNEL);
		if (!chip)
			return -ENOMEM;

		chip->spi = spi;
		chip->ops = spi->controller_data;

		if (chip->ops && chip->ops->setup) {
			int ret = chip->ops->setup(spi);
			if (ret) {
				kfree(chip);
				return ret;
			}
		}

		spi_set_ctldata(spi, chip);
	}

	if (spi->max_speed_hz != chip->rate) {
		int err;

		err = ep93xx_spi_calc_divisors(espi, chip, spi->max_speed_hz);
		if (err != 0) {
			spi_set_ctldata(spi, NULL);
			kfree(chip);
			return err;
		}
		chip->rate = spi->max_speed_hz;
	}

	chip->dss = bits_per_word_to_dss(spi->bits_per_word);

	ep93xx_spi_cs_control(spi, false);
	return 0;
}

static int ep93xx_spi_transfer(struct spi_device *spi, struct spi_message *msg)
{
	struct ep93xx_spi *espi = spi_master_get_devdata(spi->master);
	struct spi_transfer *t;
	unsigned long flags;

	if (!msg || !msg->complete)
		return -EINVAL;

	
	list_for_each_entry(t, &msg->transfers, transfer_list) {
		if (t->bits_per_word) {
			if (t->bits_per_word < 4 || t->bits_per_word > 16)
				return -EINVAL;
		}
		if (t->speed_hz && t->speed_hz < espi->min_rate)
				return -EINVAL;
	}

	msg->state = NULL;
	msg->status = 0;
	msg->actual_length = 0;

	spin_lock_irqsave(&espi->lock, flags);
	if (!espi->running) {
		spin_unlock_irqrestore(&espi->lock, flags);
		return -ESHUTDOWN;
	}
	list_add_tail(&msg->queue, &espi->msg_queue);
	queue_work(espi->wq, &espi->msg_work);
	spin_unlock_irqrestore(&espi->lock, flags);

	return 0;
}

static void ep93xx_spi_cleanup(struct spi_device *spi)
{
	struct ep93xx_spi_chip *chip;

	chip = spi_get_ctldata(spi);
	if (chip) {
		if (chip->ops && chip->ops->cleanup)
			chip->ops->cleanup(spi);
		spi_set_ctldata(spi, NULL);
		kfree(chip);
	}
}

static void ep93xx_spi_chip_setup(const struct ep93xx_spi *espi,
				  const struct ep93xx_spi_chip *chip)
{
	u16 cr0;

	cr0 = chip->div_scr << SSPCR0_SCR_SHIFT;
	cr0 |= (chip->spi->mode & (SPI_CPHA|SPI_CPOL)) << SSPCR0_MODE_SHIFT;
	cr0 |= chip->dss;

	dev_dbg(&espi->pdev->dev, "setup: mode %d, cpsr %d, scr %d, dss %d\n",
		chip->spi->mode, chip->div_cpsr, chip->div_scr, chip->dss);
	dev_dbg(&espi->pdev->dev, "setup: cr0 %#x", cr0);

	ep93xx_spi_write_u8(espi, SSPCPSR, chip->div_cpsr);
	ep93xx_spi_write_u16(espi, SSPCR0, cr0);
}

static inline int bits_per_word(const struct ep93xx_spi *espi)
{
	struct spi_message *msg = espi->current_msg;
	struct spi_transfer *t = msg->state;

	return t->bits_per_word ? t->bits_per_word : msg->spi->bits_per_word;
}

static void ep93xx_do_write(struct ep93xx_spi *espi, struct spi_transfer *t)
{
	if (bits_per_word(espi) > 8) {
		u16 tx_val = 0;

		if (t->tx_buf)
			tx_val = ((u16 *)t->tx_buf)[espi->tx];
		ep93xx_spi_write_u16(espi, SSPDR, tx_val);
		espi->tx += sizeof(tx_val);
	} else {
		u8 tx_val = 0;

		if (t->tx_buf)
			tx_val = ((u8 *)t->tx_buf)[espi->tx];
		ep93xx_spi_write_u8(espi, SSPDR, tx_val);
		espi->tx += sizeof(tx_val);
	}
}

static void ep93xx_do_read(struct ep93xx_spi *espi, struct spi_transfer *t)
{
	if (bits_per_word(espi) > 8) {
		u16 rx_val;

		rx_val = ep93xx_spi_read_u16(espi, SSPDR);
		if (t->rx_buf)
			((u16 *)t->rx_buf)[espi->rx] = rx_val;
		espi->rx += sizeof(rx_val);
	} else {
		u8 rx_val;

		rx_val = ep93xx_spi_read_u8(espi, SSPDR);
		if (t->rx_buf)
			((u8 *)t->rx_buf)[espi->rx] = rx_val;
		espi->rx += sizeof(rx_val);
	}
}

static int ep93xx_spi_read_write(struct ep93xx_spi *espi)
{
	struct spi_message *msg = espi->current_msg;
	struct spi_transfer *t = msg->state;

	
	while ((ep93xx_spi_read_u8(espi, SSPSR) & SSPSR_RNE)) {
		ep93xx_do_read(espi, t);
		espi->fifo_level--;
	}

	
	while (espi->fifo_level < SPI_FIFO_SIZE && espi->tx < t->len) {
		ep93xx_do_write(espi, t);
		espi->fifo_level++;
	}

	if (espi->rx == t->len)
		return 0;

	return -EINPROGRESS;
}

static void ep93xx_spi_pio_transfer(struct ep93xx_spi *espi)
{
	if (ep93xx_spi_read_write(espi)) {
		ep93xx_spi_enable_interrupts(espi);
		wait_for_completion(&espi->wait);
	}
}

static struct dma_async_tx_descriptor *
ep93xx_spi_dma_prepare(struct ep93xx_spi *espi, enum dma_transfer_direction dir)
{
	struct spi_transfer *t = espi->current_msg->state;
	struct dma_async_tx_descriptor *txd;
	enum dma_slave_buswidth buswidth;
	struct dma_slave_config conf;
	struct scatterlist *sg;
	struct sg_table *sgt;
	struct dma_chan *chan;
	const void *buf, *pbuf;
	size_t len = t->len;
	int i, ret, nents;

	if (bits_per_word(espi) > 8)
		buswidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
	else
		buswidth = DMA_SLAVE_BUSWIDTH_1_BYTE;

	memset(&conf, 0, sizeof(conf));
	conf.direction = dir;

	if (dir == DMA_DEV_TO_MEM) {
		chan = espi->dma_rx;
		buf = t->rx_buf;
		sgt = &espi->rx_sgt;

		conf.src_addr = espi->sspdr_phys;
		conf.src_addr_width = buswidth;
	} else {
		chan = espi->dma_tx;
		buf = t->tx_buf;
		sgt = &espi->tx_sgt;

		conf.dst_addr = espi->sspdr_phys;
		conf.dst_addr_width = buswidth;
	}

	ret = dmaengine_slave_config(chan, &conf);
	if (ret)
		return ERR_PTR(ret);


	nents = DIV_ROUND_UP(len, PAGE_SIZE);
	if (nents != sgt->nents) {
		sg_free_table(sgt);

		ret = sg_alloc_table(sgt, nents, GFP_KERNEL);
		if (ret)
			return ERR_PTR(ret);
	}

	pbuf = buf;
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		size_t bytes = min_t(size_t, len, PAGE_SIZE);

		if (buf) {
			sg_set_page(sg, virt_to_page(pbuf), bytes,
				    offset_in_page(pbuf));
		} else {
			sg_set_page(sg, virt_to_page(espi->zeropage),
				    bytes, 0);
		}

		pbuf += bytes;
		len -= bytes;
	}

	if (WARN_ON(len)) {
		dev_warn(&espi->pdev->dev, "len = %d expected 0!", len);
		return ERR_PTR(-EINVAL);
	}

	nents = dma_map_sg(chan->device->dev, sgt->sgl, sgt->nents, dir);
	if (!nents)
		return ERR_PTR(-ENOMEM);

	txd = dmaengine_prep_slave_sg(chan, sgt->sgl, nents, dir, DMA_CTRL_ACK);
	if (!txd) {
		dma_unmap_sg(chan->device->dev, sgt->sgl, sgt->nents, dir);
		return ERR_PTR(-ENOMEM);
	}
	return txd;
}

static void ep93xx_spi_dma_finish(struct ep93xx_spi *espi,
				  enum dma_transfer_direction dir)
{
	struct dma_chan *chan;
	struct sg_table *sgt;

	if (dir == DMA_DEV_TO_MEM) {
		chan = espi->dma_rx;
		sgt = &espi->rx_sgt;
	} else {
		chan = espi->dma_tx;
		sgt = &espi->tx_sgt;
	}

	dma_unmap_sg(chan->device->dev, sgt->sgl, sgt->nents, dir);
}

static void ep93xx_spi_dma_callback(void *callback_param)
{
	complete(callback_param);
}

static void ep93xx_spi_dma_transfer(struct ep93xx_spi *espi)
{
	struct spi_message *msg = espi->current_msg;
	struct dma_async_tx_descriptor *rxd, *txd;

	rxd = ep93xx_spi_dma_prepare(espi, DMA_DEV_TO_MEM);
	if (IS_ERR(rxd)) {
		dev_err(&espi->pdev->dev, "DMA RX failed: %ld\n", PTR_ERR(rxd));
		msg->status = PTR_ERR(rxd);
		return;
	}

	txd = ep93xx_spi_dma_prepare(espi, DMA_MEM_TO_DEV);
	if (IS_ERR(txd)) {
		ep93xx_spi_dma_finish(espi, DMA_DEV_TO_MEM);
		dev_err(&espi->pdev->dev, "DMA TX failed: %ld\n", PTR_ERR(rxd));
		msg->status = PTR_ERR(txd);
		return;
	}

	
	rxd->callback = ep93xx_spi_dma_callback;
	rxd->callback_param = &espi->wait;

	
	dmaengine_submit(rxd);
	dmaengine_submit(txd);

	dma_async_issue_pending(espi->dma_rx);
	dma_async_issue_pending(espi->dma_tx);

	wait_for_completion(&espi->wait);

	ep93xx_spi_dma_finish(espi, DMA_MEM_TO_DEV);
	ep93xx_spi_dma_finish(espi, DMA_DEV_TO_MEM);
}

static void ep93xx_spi_process_transfer(struct ep93xx_spi *espi,
					struct spi_message *msg,
					struct spi_transfer *t)
{
	struct ep93xx_spi_chip *chip = spi_get_ctldata(msg->spi);

	msg->state = t;

	if (t->speed_hz || t->bits_per_word) {
		struct ep93xx_spi_chip tmp_chip = *chip;

		if (t->speed_hz) {
			int err;

			err = ep93xx_spi_calc_divisors(espi, &tmp_chip,
						       t->speed_hz);
			if (err) {
				dev_err(&espi->pdev->dev,
					"failed to adjust speed\n");
				msg->status = err;
				return;
			}
		}

		if (t->bits_per_word)
			tmp_chip.dss = bits_per_word_to_dss(t->bits_per_word);

		ep93xx_spi_chip_setup(espi, &tmp_chip);
	}

	espi->rx = 0;
	espi->tx = 0;

	if (espi->dma_rx && t->len > SPI_FIFO_SIZE)
		ep93xx_spi_dma_transfer(espi);
	else
		ep93xx_spi_pio_transfer(espi);

	if (msg->status)
		return;

	msg->actual_length += t->len;

	if (t->delay_usecs) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(usecs_to_jiffies(t->delay_usecs));
	}
	if (t->cs_change) {
		if (!list_is_last(&t->transfer_list, &msg->transfers)) {
			ep93xx_spi_cs_control(msg->spi, false);
			cond_resched();
			ep93xx_spi_cs_control(msg->spi, true);
		}
	}

	if (t->speed_hz || t->bits_per_word)
		ep93xx_spi_chip_setup(espi, chip);
}

static void ep93xx_spi_process_message(struct ep93xx_spi *espi,
				       struct spi_message *msg)
{
	unsigned long timeout;
	struct spi_transfer *t;
	int err;

	err = ep93xx_spi_enable(espi);
	if (err) {
		dev_err(&espi->pdev->dev, "failed to enable SPI controller\n");
		msg->status = err;
		return;
	}

	timeout = jiffies + msecs_to_jiffies(SPI_TIMEOUT);
	while (ep93xx_spi_read_u16(espi, SSPSR) & SSPSR_RNE) {
		if (time_after(jiffies, timeout)) {
			dev_warn(&espi->pdev->dev,
				 "timeout while flushing RX FIFO\n");
			msg->status = -ETIMEDOUT;
			return;
		}
		ep93xx_spi_read_u16(espi, SSPDR);
	}

	espi->fifo_level = 0;

	ep93xx_spi_chip_setup(espi, spi_get_ctldata(msg->spi));
	ep93xx_spi_cs_control(msg->spi, true);

	list_for_each_entry(t, &msg->transfers, transfer_list) {
		ep93xx_spi_process_transfer(espi, msg, t);
		if (msg->status)
			break;
	}

	ep93xx_spi_cs_control(msg->spi, false);
	ep93xx_spi_disable(espi);
}

#define work_to_espi(work) (container_of((work), struct ep93xx_spi, msg_work))

static void ep93xx_spi_work(struct work_struct *work)
{
	struct ep93xx_spi *espi = work_to_espi(work);
	struct spi_message *msg;

	spin_lock_irq(&espi->lock);
	if (!espi->running || espi->current_msg ||
		list_empty(&espi->msg_queue)) {
		spin_unlock_irq(&espi->lock);
		return;
	}
	msg = list_first_entry(&espi->msg_queue, struct spi_message, queue);
	list_del_init(&msg->queue);
	espi->current_msg = msg;
	spin_unlock_irq(&espi->lock);

	ep93xx_spi_process_message(espi, msg);

	spin_lock_irq(&espi->lock);
	espi->current_msg = NULL;
	if (espi->running && !list_empty(&espi->msg_queue))
		queue_work(espi->wq, &espi->msg_work);
	spin_unlock_irq(&espi->lock);

	
	msg->complete(msg->context);
}

static irqreturn_t ep93xx_spi_interrupt(int irq, void *dev_id)
{
	struct ep93xx_spi *espi = dev_id;
	u8 irq_status = ep93xx_spi_read_u8(espi, SSPIIR);

	if (unlikely(irq_status & SSPIIR_RORIS)) {
		
		ep93xx_spi_write_u8(espi, SSPICR, 0);
		dev_warn(&espi->pdev->dev,
			 "receive overrun, aborting the message\n");
		espi->current_msg->status = -EIO;
	} else {
		if (ep93xx_spi_read_write(espi)) {
			return IRQ_HANDLED;
		}
	}

	ep93xx_spi_disable_interrupts(espi);
	complete(&espi->wait);
	return IRQ_HANDLED;
}

static bool ep93xx_spi_dma_filter(struct dma_chan *chan, void *filter_param)
{
	if (ep93xx_dma_chan_is_m2p(chan))
		return false;

	chan->private = filter_param;
	return true;
}

static int ep93xx_spi_setup_dma(struct ep93xx_spi *espi)
{
	dma_cap_mask_t mask;
	int ret;

	espi->zeropage = (void *)get_zeroed_page(GFP_KERNEL);
	if (!espi->zeropage)
		return -ENOMEM;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	espi->dma_rx_data.port = EP93XX_DMA_SSP;
	espi->dma_rx_data.direction = DMA_DEV_TO_MEM;
	espi->dma_rx_data.name = "ep93xx-spi-rx";

	espi->dma_rx = dma_request_channel(mask, ep93xx_spi_dma_filter,
					   &espi->dma_rx_data);
	if (!espi->dma_rx) {
		ret = -ENODEV;
		goto fail_free_page;
	}

	espi->dma_tx_data.port = EP93XX_DMA_SSP;
	espi->dma_tx_data.direction = DMA_MEM_TO_DEV;
	espi->dma_tx_data.name = "ep93xx-spi-tx";

	espi->dma_tx = dma_request_channel(mask, ep93xx_spi_dma_filter,
					   &espi->dma_tx_data);
	if (!espi->dma_tx) {
		ret = -ENODEV;
		goto fail_release_rx;
	}

	return 0;

fail_release_rx:
	dma_release_channel(espi->dma_rx);
	espi->dma_rx = NULL;
fail_free_page:
	free_page((unsigned long)espi->zeropage);

	return ret;
}

static void ep93xx_spi_release_dma(struct ep93xx_spi *espi)
{
	if (espi->dma_rx) {
		dma_release_channel(espi->dma_rx);
		sg_free_table(&espi->rx_sgt);
	}
	if (espi->dma_tx) {
		dma_release_channel(espi->dma_tx);
		sg_free_table(&espi->tx_sgt);
	}

	if (espi->zeropage)
		free_page((unsigned long)espi->zeropage);
}

static int __devinit ep93xx_spi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct ep93xx_spi_info *info;
	struct ep93xx_spi *espi;
	struct resource *res;
	int error;

	info = pdev->dev.platform_data;

	master = spi_alloc_master(&pdev->dev, sizeof(*espi));
	if (!master) {
		dev_err(&pdev->dev, "failed to allocate spi master\n");
		return -ENOMEM;
	}

	master->setup = ep93xx_spi_setup;
	master->transfer = ep93xx_spi_transfer;
	master->cleanup = ep93xx_spi_cleanup;
	master->bus_num = pdev->id;
	master->num_chipselect = info->num_chipselect;
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;

	platform_set_drvdata(pdev, master);

	espi = spi_master_get_devdata(master);

	espi->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(espi->clk)) {
		dev_err(&pdev->dev, "unable to get spi clock\n");
		error = PTR_ERR(espi->clk);
		goto fail_release_master;
	}

	spin_lock_init(&espi->lock);
	init_completion(&espi->wait);

	espi->max_rate = clk_get_rate(espi->clk) / 2;
	espi->min_rate = clk_get_rate(espi->clk) / (254 * 256);
	espi->pdev = pdev;

	espi->irq = platform_get_irq(pdev, 0);
	if (espi->irq < 0) {
		error = -EBUSY;
		dev_err(&pdev->dev, "failed to get irq resources\n");
		goto fail_put_clock;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "unable to get iomem resource\n");
		error = -ENODEV;
		goto fail_put_clock;
	}

	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!res) {
		dev_err(&pdev->dev, "unable to request iomem resources\n");
		error = -EBUSY;
		goto fail_put_clock;
	}

	espi->sspdr_phys = res->start + SSPDR;
	espi->regs_base = ioremap(res->start, resource_size(res));
	if (!espi->regs_base) {
		dev_err(&pdev->dev, "failed to map resources\n");
		error = -ENODEV;
		goto fail_free_mem;
	}

	error = request_irq(espi->irq, ep93xx_spi_interrupt, 0,
			    "ep93xx-spi", espi);
	if (error) {
		dev_err(&pdev->dev, "failed to request irq\n");
		goto fail_unmap_regs;
	}

	if (info->use_dma && ep93xx_spi_setup_dma(espi))
		dev_warn(&pdev->dev, "DMA setup failed. Falling back to PIO\n");

	espi->wq = create_singlethread_workqueue("ep93xx_spid");
	if (!espi->wq) {
		dev_err(&pdev->dev, "unable to create workqueue\n");
		goto fail_free_dma;
	}
	INIT_WORK(&espi->msg_work, ep93xx_spi_work);
	INIT_LIST_HEAD(&espi->msg_queue);
	espi->running = true;

	
	ep93xx_spi_write_u8(espi, SSPCR1, 0);

	error = spi_register_master(master);
	if (error) {
		dev_err(&pdev->dev, "failed to register SPI master\n");
		goto fail_free_queue;
	}

	dev_info(&pdev->dev, "EP93xx SPI Controller at 0x%08lx irq %d\n",
		 (unsigned long)res->start, espi->irq);

	return 0;

fail_free_queue:
	destroy_workqueue(espi->wq);
fail_free_dma:
	ep93xx_spi_release_dma(espi);
	free_irq(espi->irq, espi);
fail_unmap_regs:
	iounmap(espi->regs_base);
fail_free_mem:
	release_mem_region(res->start, resource_size(res));
fail_put_clock:
	clk_put(espi->clk);
fail_release_master:
	spi_master_put(master);
	platform_set_drvdata(pdev, NULL);

	return error;
}

static int __devexit ep93xx_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct ep93xx_spi *espi = spi_master_get_devdata(master);
	struct resource *res;

	spin_lock_irq(&espi->lock);
	espi->running = false;
	spin_unlock_irq(&espi->lock);

	destroy_workqueue(espi->wq);

	spin_lock_irq(&espi->lock);
	while (!list_empty(&espi->msg_queue)) {
		struct spi_message *msg;

		msg = list_first_entry(&espi->msg_queue,
				       struct spi_message, queue);
		list_del_init(&msg->queue);
		msg->status = -ESHUTDOWN;
		spin_unlock_irq(&espi->lock);
		msg->complete(msg->context);
		spin_lock_irq(&espi->lock);
	}
	spin_unlock_irq(&espi->lock);

	ep93xx_spi_release_dma(espi);
	free_irq(espi->irq, espi);
	iounmap(espi->regs_base);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, resource_size(res));
	clk_put(espi->clk);
	platform_set_drvdata(pdev, NULL);

	spi_unregister_master(master);
	return 0;
}

static struct platform_driver ep93xx_spi_driver = {
	.driver		= {
		.name	= "ep93xx-spi",
		.owner	= THIS_MODULE,
	},
	.probe		= ep93xx_spi_probe,
	.remove		= __devexit_p(ep93xx_spi_remove),
};
module_platform_driver(ep93xx_spi_driver);

MODULE_DESCRIPTION("EP93xx SPI Controller driver");
MODULE_AUTHOR("Mika Westerberg <mika.westerberg@iki.fi>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ep93xx-spi");
