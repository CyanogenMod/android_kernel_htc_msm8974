/*
 * SuperH IrDA Driver
 *
 * Copyright (C) 2010 Renesas Solutions Corp.
 * Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 *
 * Based on sh_sir.c
 * Copyright (C) 2009 Renesas Solutions Corp.
 * Copyright 2006-2009 Analog Devices Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <net/irda/wrapper.h>
#include <net/irda/irda_device.h>

#define DRIVER_NAME "sh_irda"

#if defined(CONFIG_ARCH_SH7367) || defined(CONFIG_ARCH_SH7377)
#define __IRDARAM_LEN	0x13FF
#else
#define __IRDARAM_LEN	0x1039
#endif

#define IRTMR		0x1F00 
#define IRCFR		0x1F02 
#define IRCTR		0x1F04 
#define IRTFLR		0x1F20 
#define IRTCTR		0x1F22 
#define IRRFLR		0x1F40 
#define IRRCTR		0x1F42 
#define SIRISR		0x1F60 
#define SIRIMR		0x1F62 
#define SIRICR		0x1F64 
#define SIRBCR		0x1F68 
#define MFIRISR		0x1F70 
#define MFIRIMR		0x1F72 
#define MFIRICR		0x1F74 
#define CRCCTR		0x1F80 
#define CRCIR		0x1F86 
#define CRCCR		0x1F8A 
#define CRCOR		0x1F8E 
#define FIFOCP		0x1FC0 
#define FIFOFP		0x1FC2 
#define FIFORSMSK	0x1FC4 
#define FIFORSOR	0x1FC6 
#define FIFOSEL		0x1FC8 
#define FIFORS		0x1FCA 
#define FIFORFL		0x1FCC 
#define FIFORAMCP	0x1FCE 
#define FIFORAMFP	0x1FD0 
#define BIFCTL		0x1FD2 
#define IRDARAM		0x0000 
#define IRDARAM_LEN	__IRDARAM_LEN 

#define TMD_MASK	(0x3 << 14) 
#define TMD_SIR		(0x0 << 14)
#define TMD_MIR		(0x3 << 14)
#define TMD_FIR		(0x2 << 14)

#define FIFORIM		(1 << 8) 
#define MIM		(1 << 4) 
#define SIM		(1 << 0) 
#define xIM_MASK	(FIFORIM | MIM | SIM)

#define RTO_SHIFT	8 
#define RTO		(0x3 << RTO_SHIFT)

#define ARMOD		(1 << 15) 
#define TE		(1 <<  0) 

#define RFL_MASK	(0x1FFF) 

#define RE		(1 <<  0) 

#define FRE		(1 << 15) 
#define TROV		(1 << 11) 
#define xIR_9		(1 << 9)
#define TOT		xIR_9     
#define ABTD		xIR_9     
#define xIR_8		(1 << 8)
#define FER		xIR_8     
#define CRCER		xIR_8     
#define FTE		(1 << 7)  
#define xIR_MASK	(FRE | TROV | xIR_9 | xIR_8 | FTE)

#define BRC_MASK	(0x3F) 

#define CRC_RST		(1 << 15) 
#define CRC_CT_MASK	0x0FFF    

#define CRC_IN_MASK	0x0FFF    

enum sh_irda_mode {
	SH_IRDA_NONE = 0,
	SH_IRDA_SIR,
	SH_IRDA_MIR,
	SH_IRDA_FIR,
};

struct sh_irda_self;
struct sh_irda_xir_func {
	int (*xir_fre)	(struct sh_irda_self *self);
	int (*xir_trov)	(struct sh_irda_self *self);
	int (*xir_9)	(struct sh_irda_self *self);
	int (*xir_8)	(struct sh_irda_self *self);
	int (*xir_fte)	(struct sh_irda_self *self);
};

struct sh_irda_self {
	void __iomem		*membase;
	unsigned int		irq;
	struct platform_device	*pdev;

	struct net_device	*ndev;

	struct irlap_cb		*irlap;
	struct qos_info		qos;

	iobuff_t		tx_buff;
	iobuff_t		rx_buff;

	enum sh_irda_mode	mode;
	spinlock_t		lock;

	struct sh_irda_xir_func	*xir_func;
};

static void sh_irda_write(struct sh_irda_self *self, u32 offset, u16 data)
{
	unsigned long flags;

	spin_lock_irqsave(&self->lock, flags);
	iowrite16(data, self->membase + offset);
	spin_unlock_irqrestore(&self->lock, flags);
}

static u16 sh_irda_read(struct sh_irda_self *self, u32 offset)
{
	unsigned long flags;
	u16 ret;

	spin_lock_irqsave(&self->lock, flags);
	ret = ioread16(self->membase + offset);
	spin_unlock_irqrestore(&self->lock, flags);

	return ret;
}

static void sh_irda_update_bits(struct sh_irda_self *self, u32 offset,
			       u16 mask, u16 data)
{
	unsigned long flags;
	u16 old, new;

	spin_lock_irqsave(&self->lock, flags);
	old = ioread16(self->membase + offset);
	new = (old & ~mask) | data;
	if (old != new)
		iowrite16(data, self->membase + offset);
	spin_unlock_irqrestore(&self->lock, flags);
}

static void sh_irda_rcv_ctrl(struct sh_irda_self *self, int enable)
{
	struct device *dev = &self->ndev->dev;

	sh_irda_update_bits(self, IRRCTR, RE, enable ? RE : 0);
	dev_dbg(dev, "recv %s\n", enable ? "enable" : "disable");
}

static int sh_irda_set_timeout(struct sh_irda_self *self, int interval)
{
	struct device *dev = &self->ndev->dev;

	if (SH_IRDA_SIR != self->mode)
		interval = 0;

	if (interval < 0 || interval > 2) {
		dev_err(dev, "unsupported timeout interval\n");
		return -EINVAL;
	}

	sh_irda_update_bits(self, IRCFR, RTO, interval << RTO_SHIFT);
	return 0;
}

static int sh_irda_set_baudrate(struct sh_irda_self *self, int baudrate)
{
	struct device *dev = &self->ndev->dev;
	u16 val;

	if (baudrate < 0)
		return 0;

	if (SH_IRDA_SIR != self->mode) {
		dev_err(dev, "it is not SIR mode\n");
		return -EINVAL;
	}

	val = (48000000 / 26 / 16 / baudrate) - 1;
	dev_dbg(dev, "baudrate = %d,  val = 0x%02x\n", baudrate, val);

	sh_irda_update_bits(self, SIRBCR, BRC_MASK, val);

	return 0;
}

static int sh_irda_get_rcv_length(struct sh_irda_self *self)
{
	return RFL_MASK & sh_irda_read(self, IRRFLR);
}

static int sh_irda_xir_fre(struct sh_irda_self *self)
{
	struct device *dev = &self->ndev->dev;
	dev_err(dev, "none mode: frame recv\n");
	return 0;
}

static int sh_irda_xir_trov(struct sh_irda_self *self)
{
	struct device *dev = &self->ndev->dev;
	dev_err(dev, "none mode: buffer ram over\n");
	return 0;
}

static int sh_irda_xir_9(struct sh_irda_self *self)
{
	struct device *dev = &self->ndev->dev;
	dev_err(dev, "none mode: time over\n");
	return 0;
}

static int sh_irda_xir_8(struct sh_irda_self *self)
{
	struct device *dev = &self->ndev->dev;
	dev_err(dev, "none mode: framing error\n");
	return 0;
}

static int sh_irda_xir_fte(struct sh_irda_self *self)
{
	struct device *dev = &self->ndev->dev;
	dev_err(dev, "none mode: frame transmit end\n");
	return 0;
}

static struct sh_irda_xir_func sh_irda_xir_func = {
	.xir_fre	= sh_irda_xir_fre,
	.xir_trov	= sh_irda_xir_trov,
	.xir_9		= sh_irda_xir_9,
	.xir_8		= sh_irda_xir_8,
	.xir_fte	= sh_irda_xir_fte,
};

static struct sh_irda_xir_func sh_irda_mfir_func = {
	.xir_fre	= sh_irda_xir_fre,
	.xir_trov	= sh_irda_xir_trov,
	.xir_9		= sh_irda_xir_9,
	.xir_8		= sh_irda_xir_8,
	.xir_fte	= sh_irda_xir_fte,
};

static int sh_irda_sir_fre(struct sh_irda_self *self)
{
	struct device *dev = &self->ndev->dev;
	u16 data16;
	u8  *data = (u8 *)&data16;
	int len = sh_irda_get_rcv_length(self);
	int i, j;

	if (len > IRDARAM_LEN)
		len = IRDARAM_LEN;

	dev_dbg(dev, "frame recv length = %d\n", len);

	for (i = 0; i < len; i++) {
		j = i % 2;
		if (!j)
			data16 = sh_irda_read(self, IRDARAM + i);

		async_unwrap_char(self->ndev, &self->ndev->stats,
				  &self->rx_buff, data[j]);
	}
	self->ndev->last_rx = jiffies;

	sh_irda_rcv_ctrl(self, 1);

	return 0;
}

static int sh_irda_sir_trov(struct sh_irda_self *self)
{
	struct device *dev = &self->ndev->dev;

	dev_err(dev, "buffer ram over\n");
	sh_irda_rcv_ctrl(self, 1);
	return 0;
}

static int sh_irda_sir_tot(struct sh_irda_self *self)
{
	struct device *dev = &self->ndev->dev;

	dev_err(dev, "time over\n");
	sh_irda_set_baudrate(self, 9600);
	sh_irda_rcv_ctrl(self, 1);
	return 0;
}

static int sh_irda_sir_fer(struct sh_irda_self *self)
{
	struct device *dev = &self->ndev->dev;

	dev_err(dev, "framing error\n");
	sh_irda_rcv_ctrl(self, 1);
	return 0;
}

static int sh_irda_sir_fte(struct sh_irda_self *self)
{
	struct device *dev = &self->ndev->dev;

	dev_dbg(dev, "frame transmit end\n");
	netif_wake_queue(self->ndev);

	return 0;
}

static struct sh_irda_xir_func sh_irda_sir_func = {
	.xir_fre	= sh_irda_sir_fre,
	.xir_trov	= sh_irda_sir_trov,
	.xir_9		= sh_irda_sir_tot,
	.xir_8		= sh_irda_sir_fer,
	.xir_fte	= sh_irda_sir_fte,
};

static void sh_irda_set_mode(struct sh_irda_self *self, enum sh_irda_mode mode)
{
	struct device *dev = &self->ndev->dev;
	struct sh_irda_xir_func	*func;
	const char *name;
	u16 data;

	switch (mode) {
	case SH_IRDA_SIR:
		name	= "SIR";
		data	= TMD_SIR;
		func	= &sh_irda_sir_func;
		break;
	case SH_IRDA_MIR:
		name	= "MIR";
		data	= TMD_MIR;
		func	= &sh_irda_mfir_func;
		break;
	case SH_IRDA_FIR:
		name	= "FIR";
		data	= TMD_FIR;
		func	= &sh_irda_mfir_func;
		break;
	default:
		name	= "NONE";
		data	= 0;
		func	= &sh_irda_xir_func;
		break;
	}

	self->mode = mode;
	self->xir_func = func;
	sh_irda_update_bits(self, IRTMR, TMD_MASK, data);

	dev_dbg(dev, "switch to %s mode", name);
}

static void sh_irda_set_irq_mask(struct sh_irda_self *self)
{
	u16 tmr_hole;
	u16 xir_reg;

	
	sh_irda_update_bits(self, IRTMR,   xIM_MASK, xIM_MASK);
	sh_irda_update_bits(self, SIRIMR,  xIR_MASK, xIR_MASK);
	sh_irda_update_bits(self, MFIRIMR, xIR_MASK, xIR_MASK);

	
	sh_irda_update_bits(self, SIRICR,  xIR_MASK, xIR_MASK);
	sh_irda_update_bits(self, MFIRICR, xIR_MASK, xIR_MASK);

	switch (self->mode) {
	case SH_IRDA_SIR:
		tmr_hole	= SIM;
		xir_reg		= SIRIMR;
		break;
	case SH_IRDA_MIR:
	case SH_IRDA_FIR:
		tmr_hole	= MIM;
		xir_reg		= MFIRIMR;
		break;
	default:
		tmr_hole	= 0;
		xir_reg		= 0;
		break;
	}

	
	if (xir_reg) {
		sh_irda_update_bits(self, IRTMR, tmr_hole, 0);
		sh_irda_update_bits(self, xir_reg, xIR_MASK, 0);
	}
}

static irqreturn_t sh_irda_irq(int irq, void *dev_id)
{
	struct sh_irda_self *self = dev_id;
	struct sh_irda_xir_func	*func = self->xir_func;
	u16 isr = sh_irda_read(self, SIRISR);

	
	sh_irda_write(self, SIRICR, isr);

	if (isr & FRE)
		func->xir_fre(self);
	if (isr & TROV)
		func->xir_trov(self);
	if (isr & xIR_9)
		func->xir_9(self);
	if (isr & xIR_8)
		func->xir_8(self);
	if (isr & FTE)
		func->xir_fte(self);

	return IRQ_HANDLED;
}

static void sh_irda_crc_reset(struct sh_irda_self *self)
{
	sh_irda_write(self, CRCCTR, CRC_RST);
}

static void sh_irda_crc_add(struct sh_irda_self *self, u16 data)
{
	sh_irda_write(self, CRCIR, data & CRC_IN_MASK);
}

static u16 sh_irda_crc_cnt(struct sh_irda_self *self)
{
	return CRC_CT_MASK & sh_irda_read(self, CRCCTR);
}

static u16 sh_irda_crc_out(struct sh_irda_self *self)
{
	return sh_irda_read(self, CRCOR);
}

static int sh_irda_crc_init(struct sh_irda_self *self)
{
	struct device *dev = &self->ndev->dev;
	int ret = -EIO;
	u16 val;

	sh_irda_crc_reset(self);

	sh_irda_crc_add(self, 0xCC);
	sh_irda_crc_add(self, 0xF5);
	sh_irda_crc_add(self, 0xF1);
	sh_irda_crc_add(self, 0xA7);

	val = sh_irda_crc_cnt(self);
	if (4 != val) {
		dev_err(dev, "CRC count error %x\n", val);
		goto crc_init_out;
	}

	val = sh_irda_crc_out(self);
	if (0x51DF != val) {
		dev_err(dev, "CRC result error%x\n", val);
		goto crc_init_out;
	}

	ret = 0;

crc_init_out:

	sh_irda_crc_reset(self);
	return ret;
}

static void sh_irda_remove_iobuf(struct sh_irda_self *self)
{
	kfree(self->rx_buff.head);

	self->tx_buff.head = NULL;
	self->tx_buff.data = NULL;
	self->rx_buff.head = NULL;
	self->rx_buff.data = NULL;
}

static int sh_irda_init_iobuf(struct sh_irda_self *self, int rxsize, int txsize)
{
	if (self->rx_buff.head ||
	    self->tx_buff.head) {
		dev_err(&self->ndev->dev, "iobuff has already existed.");
		return -EINVAL;
	}

	
	self->rx_buff.head = kmalloc(rxsize, GFP_KERNEL);
	if (!self->rx_buff.head)
		return -ENOMEM;

	self->rx_buff.truesize	= rxsize;
	self->rx_buff.in_frame	= FALSE;
	self->rx_buff.state	= OUTSIDE_FRAME;
	self->rx_buff.data	= self->rx_buff.head;

	
	self->tx_buff.head	= self->membase + IRDARAM;
	self->tx_buff.truesize	= IRDARAM_LEN;

	return 0;
}

static int sh_irda_hard_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct sh_irda_self *self = netdev_priv(ndev);
	struct device *dev = &self->ndev->dev;
	int speed = irda_get_next_speed(skb);
	int ret;

	dev_dbg(dev, "hard xmit\n");

	netif_stop_queue(ndev);
	sh_irda_rcv_ctrl(self, 0);

	ret = sh_irda_set_baudrate(self, speed);
	if (ret < 0)
		goto sh_irda_hard_xmit_end;

	self->tx_buff.len = 0;
	if (skb->len) {
		unsigned long flags;

		spin_lock_irqsave(&self->lock, flags);
		self->tx_buff.len = async_wrap_skb(skb,
						   self->tx_buff.head,
						   self->tx_buff.truesize);
		spin_unlock_irqrestore(&self->lock, flags);

		if (self->tx_buff.len > self->tx_buff.truesize)
			self->tx_buff.len = self->tx_buff.truesize;

		sh_irda_write(self, IRTFLR, self->tx_buff.len);
		sh_irda_write(self, IRTCTR, ARMOD | TE);
	} else
		goto sh_irda_hard_xmit_end;

	dev_kfree_skb(skb);

	return 0;

sh_irda_hard_xmit_end:
	sh_irda_set_baudrate(self, 9600);
	netif_wake_queue(self->ndev);
	sh_irda_rcv_ctrl(self, 1);
	dev_kfree_skb(skb);

	return ret;

}

static int sh_irda_ioctl(struct net_device *ndev, struct ifreq *ifreq, int cmd)
{
	return 0;
}

static struct net_device_stats *sh_irda_stats(struct net_device *ndev)
{
	struct sh_irda_self *self = netdev_priv(ndev);

	return &self->ndev->stats;
}

static int sh_irda_open(struct net_device *ndev)
{
	struct sh_irda_self *self = netdev_priv(ndev);
	int err;

	pm_runtime_get_sync(&self->pdev->dev);
	err = sh_irda_crc_init(self);
	if (err)
		goto open_err;

	sh_irda_set_mode(self, SH_IRDA_SIR);
	sh_irda_set_timeout(self, 2);
	sh_irda_set_baudrate(self, 9600);

	self->irlap = irlap_open(ndev, &self->qos, DRIVER_NAME);
	if (!self->irlap) {
		err = -ENODEV;
		goto open_err;
	}

	netif_start_queue(ndev);
	sh_irda_rcv_ctrl(self, 1);
	sh_irda_set_irq_mask(self);

	dev_info(&ndev->dev, "opened\n");

	return 0;

open_err:
	pm_runtime_put_sync(&self->pdev->dev);

	return err;
}

static int sh_irda_stop(struct net_device *ndev)
{
	struct sh_irda_self *self = netdev_priv(ndev);

	
	if (self->irlap) {
		irlap_close(self->irlap);
		self->irlap = NULL;
	}

	netif_stop_queue(ndev);
	pm_runtime_put_sync(&self->pdev->dev);

	dev_info(&ndev->dev, "stoped\n");

	return 0;
}

static const struct net_device_ops sh_irda_ndo = {
	.ndo_open		= sh_irda_open,
	.ndo_stop		= sh_irda_stop,
	.ndo_start_xmit		= sh_irda_hard_xmit,
	.ndo_do_ioctl		= sh_irda_ioctl,
	.ndo_get_stats		= sh_irda_stats,
};

static int __devinit sh_irda_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct sh_irda_self *self;
	struct resource *res;
	int irq;
	int err = -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!res || irq < 0) {
		dev_err(&pdev->dev, "Not enough platform resources.\n");
		goto exit;
	}

	ndev = alloc_irdadev(sizeof(*self));
	if (!ndev)
		goto exit;

	self = netdev_priv(ndev);
	self->membase = ioremap_nocache(res->start, resource_size(res));
	if (!self->membase) {
		err = -ENXIO;
		dev_err(&pdev->dev, "Unable to ioremap.\n");
		goto err_mem_1;
	}

	err = sh_irda_init_iobuf(self, IRDA_SKB_MAX_MTU, IRDA_SIR_MAX_FRAME);
	if (err)
		goto err_mem_2;

	self->pdev = pdev;
	pm_runtime_enable(&pdev->dev);

	irda_init_max_qos_capabilies(&self->qos);

	ndev->netdev_ops	= &sh_irda_ndo;
	ndev->irq		= irq;

	self->ndev			= ndev;
	self->qos.baud_rate.bits	&= IR_9600; 
	self->qos.min_turn_time.bits	= 1; 
	spin_lock_init(&self->lock);

	irda_qos_bits_to_value(&self->qos);

	err = register_netdev(ndev);
	if (err)
		goto err_mem_4;

	platform_set_drvdata(pdev, ndev);

	if (request_irq(irq, sh_irda_irq, IRQF_DISABLED, "sh_irda", self)) {
		dev_warn(&pdev->dev, "Unable to attach sh_irda interrupt\n");
		goto err_mem_4;
	}

	dev_info(&pdev->dev, "SuperH IrDA probed\n");

	goto exit;

err_mem_4:
	pm_runtime_disable(&pdev->dev);
	sh_irda_remove_iobuf(self);
err_mem_2:
	iounmap(self->membase);
err_mem_1:
	free_netdev(ndev);
exit:
	return err;
}

static int __devexit sh_irda_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct sh_irda_self *self = netdev_priv(ndev);

	if (!self)
		return 0;

	unregister_netdev(ndev);
	pm_runtime_disable(&pdev->dev);
	sh_irda_remove_iobuf(self);
	iounmap(self->membase);
	free_netdev(ndev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int sh_irda_runtime_nop(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops sh_irda_pm_ops = {
	.runtime_suspend	= sh_irda_runtime_nop,
	.runtime_resume		= sh_irda_runtime_nop,
};

static struct platform_driver sh_irda_driver = {
	.probe	= sh_irda_probe,
	.remove	= __devexit_p(sh_irda_remove),
	.driver	= {
		.name	= DRIVER_NAME,
		.pm	= &sh_irda_pm_ops,
	},
};

module_platform_driver(sh_irda_driver);

MODULE_AUTHOR("Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>");
MODULE_DESCRIPTION("SuperH IrDA driver");
MODULE_LICENSE("GPL");
