/*
 * LocalPlus Bus FIFO driver for the Freescale MPC52xx.
 *
 * Copyright (C) 2009 Secret Lab Technologies Ltd.
 *
 * This file is released under the GPLv2
 *
 * Todo:
 * - Add support for multiple requests to be queued.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/mpc52xx.h>
#include <asm/time.h>

#include <sysdev/bestcomm/bestcomm.h>
#include <sysdev/bestcomm/bestcomm_priv.h>
#include <sysdev/bestcomm/gen_bd.h>

MODULE_AUTHOR("Grant Likely <grant.likely@secretlab.ca>");
MODULE_DESCRIPTION("MPC5200 LocalPlus FIFO device driver");
MODULE_LICENSE("GPL");

#define LPBFIFO_REG_PACKET_SIZE		(0x00)
#define LPBFIFO_REG_START_ADDRESS	(0x04)
#define LPBFIFO_REG_CONTROL		(0x08)
#define LPBFIFO_REG_ENABLE		(0x0C)
#define LPBFIFO_REG_BYTES_DONE_STATUS	(0x14)
#define LPBFIFO_REG_FIFO_DATA		(0x40)
#define LPBFIFO_REG_FIFO_STATUS		(0x44)
#define LPBFIFO_REG_FIFO_CONTROL	(0x48)
#define LPBFIFO_REG_FIFO_ALARM		(0x4C)

struct mpc52xx_lpbfifo {
	struct device *dev;
	phys_addr_t regs_phys;
	void __iomem *regs;
	int irq;
	spinlock_t lock;

	struct bcom_task *bcom_tx_task;
	struct bcom_task *bcom_rx_task;
	struct bcom_task *bcom_cur_task;

	
	struct mpc52xx_lpbfifo_request *req;
	int dma_irqs_enabled;
};

static struct mpc52xx_lpbfifo lpbfifo;

static void mpc52xx_lpbfifo_kick(struct mpc52xx_lpbfifo_request *req)
{
	size_t transfer_size = req->size - req->pos;
	struct bcom_bd *bd;
	void __iomem *reg;
	u32 *data;
	int i;
	int bit_fields;
	int dma = !(req->flags & MPC52XX_LPBFIFO_FLAG_NO_DMA);
	int write = req->flags & MPC52XX_LPBFIFO_FLAG_WRITE;
	int poll_dma = req->flags & MPC52XX_LPBFIFO_FLAG_POLL_DMA;

	
	out_be32(lpbfifo.regs + LPBFIFO_REG_ENABLE, 0x01010000);

	
	out_be32(lpbfifo.regs + LPBFIFO_REG_ENABLE, 0x00000001);
	if (!dma) {
		if (transfer_size > 512)
			transfer_size = 512;

		
		if (write) {
			reg = lpbfifo.regs + LPBFIFO_REG_FIFO_DATA;
			data = req->data + req->pos;
			for (i = 0; i < transfer_size; i += 4)
				out_be32(reg, *data++);
		}

		
		out_be32(lpbfifo.regs + LPBFIFO_REG_ENABLE, 0x00000301);
	} else {
		if (write) {
			out_be32(lpbfifo.regs + LPBFIFO_REG_FIFO_ALARM, 0x1e4);
			out_8(lpbfifo.regs + LPBFIFO_REG_FIFO_CONTROL, 7);
			lpbfifo.bcom_cur_task = lpbfifo.bcom_tx_task;
		} else {
			out_be32(lpbfifo.regs + LPBFIFO_REG_FIFO_ALARM, 0x1ff);
			out_8(lpbfifo.regs + LPBFIFO_REG_FIFO_CONTROL, 0);
			lpbfifo.bcom_cur_task = lpbfifo.bcom_rx_task;

			if (poll_dma) {
				if (lpbfifo.dma_irqs_enabled) {
					disable_irq(bcom_get_task_irq(lpbfifo.bcom_rx_task));
					lpbfifo.dma_irqs_enabled = 0;
				}
			} else {
				if (!lpbfifo.dma_irqs_enabled) {
					enable_irq(bcom_get_task_irq(lpbfifo.bcom_rx_task));
					lpbfifo.dma_irqs_enabled = 1;
				}
			}
		}

		bd = bcom_prepare_next_buffer(lpbfifo.bcom_cur_task);
		bd->status = transfer_size;
		if (!write) {
			transfer_size += 4; 
		}
		bd->data[0] = req->data_phys + req->pos;
		bcom_submit_next_buffer(lpbfifo.bcom_cur_task, NULL);

		
		bit_fields = 0x00000201;

		
		if (write && (!poll_dma))
			bit_fields |= 0x00000100; 
		out_be32(lpbfifo.regs + LPBFIFO_REG_ENABLE, bit_fields);
	}

	
	out_be32(lpbfifo.regs + LPBFIFO_REG_START_ADDRESS,
		 req->offset + req->pos);
	out_be32(lpbfifo.regs + LPBFIFO_REG_PACKET_SIZE, transfer_size);

	bit_fields = req->cs << 24 | 0x000008;
	if (!write)
		bit_fields |= 0x010000; 
	out_be32(lpbfifo.regs + LPBFIFO_REG_CONTROL, bit_fields);

	
	out_8(lpbfifo.regs + LPBFIFO_REG_PACKET_SIZE, 0x01);
	if (dma)
		bcom_enable(lpbfifo.bcom_cur_task);
}

static irqreturn_t mpc52xx_lpbfifo_irq(int irq, void *dev_id)
{
	struct mpc52xx_lpbfifo_request *req;
	u32 status = in_8(lpbfifo.regs + LPBFIFO_REG_BYTES_DONE_STATUS);
	void __iomem *reg;
	u32 *data;
	int count, i;
	int do_callback = 0;
	u32 ts;
	unsigned long flags;
	int dma, write, poll_dma;

	spin_lock_irqsave(&lpbfifo.lock, flags);
	ts = get_tbl();

	req = lpbfifo.req;
	if (!req) {
		spin_unlock_irqrestore(&lpbfifo.lock, flags);
		pr_err("bogus LPBFIFO IRQ\n");
		return IRQ_HANDLED;
	}

	dma = !(req->flags & MPC52XX_LPBFIFO_FLAG_NO_DMA);
	write = req->flags & MPC52XX_LPBFIFO_FLAG_WRITE;
	poll_dma = req->flags & MPC52XX_LPBFIFO_FLAG_POLL_DMA;

	if (dma && !write) {
		spin_unlock_irqrestore(&lpbfifo.lock, flags);
		pr_err("bogus LPBFIFO IRQ (dma and not writting)\n");
		return IRQ_HANDLED;
	}

	if ((status & 0x01) == 0) {
		goto out;
	}

	
	if (status & 0x10) {
		out_be32(lpbfifo.regs + LPBFIFO_REG_ENABLE, 0x01010000);
		do_callback = 1;
		goto out;
	}

	
	count = in_be32(lpbfifo.regs + LPBFIFO_REG_BYTES_DONE_STATUS);
	count &= 0x00ffffff;

	if (!dma && !write) {
		
		reg = lpbfifo.regs + LPBFIFO_REG_FIFO_DATA;
		data = req->data + req->pos;
		for (i = 0; i < count; i += 4)
			*data++ = in_be32(reg);
	}

	
	req->pos += count;

	
	if (req->size - req->pos)
		mpc52xx_lpbfifo_kick(req); 
	else
		do_callback = 1;

 out:
	
	out_8(lpbfifo.regs + LPBFIFO_REG_BYTES_DONE_STATUS, 0x01);

	if (dma && (status & 0x11)) {
		bcom_retrieve_buffer(lpbfifo.bcom_cur_task, &status, NULL);
	}
	req->last_byte = ((u8 *)req->data)[req->size - 1];

	if (do_callback)
		lpbfifo.req = NULL;

	if (irq != 0) 
		req->irq_count++;

	req->irq_ticks += get_tbl() - ts;
	spin_unlock_irqrestore(&lpbfifo.lock, flags);

	
	if (do_callback && req->callback)
		req->callback(req);

	return IRQ_HANDLED;
}

static irqreturn_t mpc52xx_lpbfifo_bcom_irq(int irq, void *dev_id)
{
	struct mpc52xx_lpbfifo_request *req;
	unsigned long flags;
	u32 status;
	u32 ts;

	spin_lock_irqsave(&lpbfifo.lock, flags);
	ts = get_tbl();

	req = lpbfifo.req;
	if (!req || (req->flags & MPC52XX_LPBFIFO_FLAG_NO_DMA)) {
		spin_unlock_irqrestore(&lpbfifo.lock, flags);
		return IRQ_HANDLED;
	}

	if (irq != 0) 
		req->irq_count++;

	if (!bcom_buffer_done(lpbfifo.bcom_cur_task)) {
		spin_unlock_irqrestore(&lpbfifo.lock, flags);

		req->buffer_not_done_cnt++;
		if ((req->buffer_not_done_cnt % 1000) == 0)
			pr_err("transfer stalled\n");

		return IRQ_HANDLED;
	}

	bcom_retrieve_buffer(lpbfifo.bcom_cur_task, &status, NULL);

	req->last_byte = ((u8 *)req->data)[req->size - 1];

	req->pos = status & 0x00ffffff;

	
	lpbfifo.req = NULL;

	
	req->irq_ticks += get_tbl() - ts;
	spin_unlock_irqrestore(&lpbfifo.lock, flags);

	if (req->callback)
		req->callback(req);

	return IRQ_HANDLED;
}

void mpc52xx_lpbfifo_poll(void)
{
	struct mpc52xx_lpbfifo_request *req = lpbfifo.req;
	int dma = !(req->flags & MPC52XX_LPBFIFO_FLAG_NO_DMA);
	int write = req->flags & MPC52XX_LPBFIFO_FLAG_WRITE;

	if (dma && write)
		mpc52xx_lpbfifo_irq(0, NULL);
	else 
		mpc52xx_lpbfifo_bcom_irq(0, NULL);
}
EXPORT_SYMBOL(mpc52xx_lpbfifo_poll);

int mpc52xx_lpbfifo_submit(struct mpc52xx_lpbfifo_request *req)
{
	unsigned long flags;

	if (!lpbfifo.regs)
		return -ENODEV;

	spin_lock_irqsave(&lpbfifo.lock, flags);

	
	if (lpbfifo.req) {
		spin_unlock_irqrestore(&lpbfifo.lock, flags);
		return -EBUSY;
	}

	
	lpbfifo.req = req;
	req->irq_count = 0;
	req->irq_ticks = 0;
	req->buffer_not_done_cnt = 0;
	req->pos = 0;

	mpc52xx_lpbfifo_kick(req);
	spin_unlock_irqrestore(&lpbfifo.lock, flags);
	return 0;
}
EXPORT_SYMBOL(mpc52xx_lpbfifo_submit);

void mpc52xx_lpbfifo_abort(struct mpc52xx_lpbfifo_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&lpbfifo.lock, flags);
	if (lpbfifo.req == req) {
		
		bcom_gen_bd_rx_reset(lpbfifo.bcom_rx_task);
		bcom_gen_bd_tx_reset(lpbfifo.bcom_tx_task);
		out_be32(lpbfifo.regs + LPBFIFO_REG_ENABLE, 0x01010000);
		lpbfifo.req = NULL;
	}
	spin_unlock_irqrestore(&lpbfifo.lock, flags);
}
EXPORT_SYMBOL(mpc52xx_lpbfifo_abort);

static int __devinit mpc52xx_lpbfifo_probe(struct platform_device *op)
{
	struct resource res;
	int rc = -ENOMEM;

	if (lpbfifo.dev != NULL)
		return -ENOSPC;

	lpbfifo.irq = irq_of_parse_and_map(op->dev.of_node, 0);
	if (!lpbfifo.irq)
		return -ENODEV;

	if (of_address_to_resource(op->dev.of_node, 0, &res))
		return -ENODEV;
	lpbfifo.regs_phys = res.start;
	lpbfifo.regs = of_iomap(op->dev.of_node, 0);
	if (!lpbfifo.regs)
		return -ENOMEM;

	spin_lock_init(&lpbfifo.lock);

	
	out_be32(lpbfifo.regs + LPBFIFO_REG_ENABLE, 0x01010000);

	
	rc = request_irq(lpbfifo.irq, mpc52xx_lpbfifo_irq, 0,
			 "mpc52xx-lpbfifo", &lpbfifo);
	if (rc)
		goto err_irq;

	
	lpbfifo.bcom_rx_task =
		bcom_gen_bd_rx_init(2, res.start + LPBFIFO_REG_FIFO_DATA,
				    BCOM_INITIATOR_SCLPC, BCOM_IPR_SCLPC,
				    16*1024*1024);
	if (!lpbfifo.bcom_rx_task)
		goto err_bcom_rx;

	rc = request_irq(bcom_get_task_irq(lpbfifo.bcom_rx_task),
			 mpc52xx_lpbfifo_bcom_irq, 0,
			 "mpc52xx-lpbfifo-rx", &lpbfifo);
	if (rc)
		goto err_bcom_rx_irq;

	lpbfifo.dma_irqs_enabled = 1;

	
	lpbfifo.bcom_tx_task =
		bcom_gen_bd_tx_init(2, res.start + LPBFIFO_REG_FIFO_DATA,
				    BCOM_INITIATOR_SCLPC, BCOM_IPR_SCLPC);
	if (!lpbfifo.bcom_tx_task)
		goto err_bcom_tx;

	lpbfifo.dev = &op->dev;
	return 0;

 err_bcom_tx:
	free_irq(bcom_get_task_irq(lpbfifo.bcom_rx_task), &lpbfifo);
 err_bcom_rx_irq:
	bcom_gen_bd_rx_release(lpbfifo.bcom_rx_task);
 err_bcom_rx:
 err_irq:
	iounmap(lpbfifo.regs);
	lpbfifo.regs = NULL;

	dev_err(&op->dev, "mpc52xx_lpbfifo_probe() failed\n");
	return -ENODEV;
}


static int __devexit mpc52xx_lpbfifo_remove(struct platform_device *op)
{
	if (lpbfifo.dev != &op->dev)
		return 0;

	
	out_be32(lpbfifo.regs + LPBFIFO_REG_ENABLE, 0x01010000);

	
	free_irq(bcom_get_task_irq(lpbfifo.bcom_tx_task), &lpbfifo);
	bcom_gen_bd_tx_release(lpbfifo.bcom_tx_task);
	
	
	free_irq(bcom_get_task_irq(lpbfifo.bcom_rx_task), &lpbfifo);
	bcom_gen_bd_rx_release(lpbfifo.bcom_rx_task);

	free_irq(lpbfifo.irq, &lpbfifo);
	iounmap(lpbfifo.regs);
	lpbfifo.regs = NULL;
	lpbfifo.dev = NULL;

	return 0;
}

static struct of_device_id mpc52xx_lpbfifo_match[] __devinitconst = {
	{ .compatible = "fsl,mpc5200-lpbfifo", },
	{},
};

static struct platform_driver mpc52xx_lpbfifo_driver = {
	.driver = {
		.name = "mpc52xx-lpbfifo",
		.owner = THIS_MODULE,
		.of_match_table = mpc52xx_lpbfifo_match,
	},
	.probe = mpc52xx_lpbfifo_probe,
	.remove = __devexit_p(mpc52xx_lpbfifo_remove),
};

static int __init mpc52xx_lpbfifo_init(void)
{
	return platform_driver_register(&mpc52xx_lpbfifo_driver);
}
module_init(mpc52xx_lpbfifo_init);

static void __exit mpc52xx_lpbfifo_exit(void)
{
	platform_driver_unregister(&mpc52xx_lpbfifo_driver);
}
module_exit(mpc52xx_lpbfifo_exit);
