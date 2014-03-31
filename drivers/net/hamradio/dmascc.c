/*
 * Driver for high-speed SCC boards (those with DMA support)
 * Copyright (C) 1997-2000 Klaus Kudielka
 *
 * S5SCC/DMA support by Janko Koleznik S52HI
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/if_arp.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/rtnetlink.h>
#include <linux/sockios.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <net/ax25.h>
#include "z8530.h"



#define NUM_TX_BUF      2	
#define NUM_RX_BUF      6	
#define BUF_SIZE        1576	



#define HW_PI           { "Ottawa PI", 0x300, 0x20, 0x10, 8, \
                            0, 8, 1843200, 3686400 }
#define HW_PI2          { "Ottawa PI2", 0x300, 0x20, 0x10, 8, \
			    0, 8, 3686400, 7372800 }
#define HW_TWIN         { "Gracilis PackeTwin", 0x200, 0x10, 0x10, 32, \
			    0, 4, 6144000, 6144000 }
#define HW_S5           { "S5SCC/DMA", 0x200, 0x10, 0x10, 32, \
                          0, 8, 4915200, 9830400 }

#define HARDWARE        { HW_PI, HW_PI2, HW_TWIN, HW_S5 }

#define TMR_0_HZ        25600	

#define TYPE_PI         0
#define TYPE_PI2        1
#define TYPE_TWIN       2
#define TYPE_S5         3
#define NUM_TYPES       4

#define MAX_NUM_DEVS    32



#define Z8530           0
#define Z85C30          1
#define Z85230          2

#define CHIPNAMES       { "Z8530", "Z85C30", "Z85230" }



#define SCCB_CMD        0x00
#define SCCB_DATA       0x01
#define SCCA_CMD        0x02
#define SCCA_DATA       0x03

#define TMR_CNT0        0x00
#define TMR_CNT1        0x01
#define TMR_CNT2        0x02
#define TMR_CTRL        0x03

#define PI_DREQ_MASK    0x04

#define TWIN_INT_REG    0x08
#define TWIN_CLR_TMR1   0x09
#define TWIN_CLR_TMR2   0x0a
#define TWIN_SPARE_1    0x0b
#define TWIN_DMA_CFG    0x08
#define TWIN_SERIAL_CFG 0x09
#define TWIN_DMA_CLR_FF 0x0a
#define TWIN_SPARE_2    0x0b



#define TWIN_SCC_MSK       0x01
#define TWIN_TMR1_MSK      0x02
#define TWIN_TMR2_MSK      0x04
#define TWIN_INT_MSK       0x07

#define TWIN_DTRA_ON       0x01
#define TWIN_DTRB_ON       0x02
#define TWIN_EXTCLKA       0x04
#define TWIN_EXTCLKB       0x08
#define TWIN_LOOPA_ON      0x10
#define TWIN_LOOPB_ON      0x20
#define TWIN_EI            0x80

#define TWIN_DMA_HDX_T1    0x08
#define TWIN_DMA_HDX_R1    0x0a
#define TWIN_DMA_HDX_T3    0x14
#define TWIN_DMA_HDX_R3    0x16
#define TWIN_DMA_FDX_T3R1  0x1b
#define TWIN_DMA_FDX_T1R3  0x1d



#define IDLE      0
#define TX_HEAD   1
#define TX_DATA   2
#define TX_PAUSE  3
#define TX_TAIL   4
#define RTS_OFF   5
#define WAIT      6
#define DCD_ON    7
#define RX_ON     8
#define DCD_OFF   9



#define SIOCGSCCPARAM SIOCDEVPRIVATE
#define SIOCSSCCPARAM (SIOCDEVPRIVATE+1)



struct scc_param {
	int pclk_hz;		
	int brg_tc;		
	int nrzi;		
	int clocks;		
	int txdelay;		
	int txtimeout;		
	int txtail;		
	int waittime;		
	int slottime;		
	int persist;		
	int dma;		
	int txpause;		
	int rtsoff;		
	int dcdon;		
	int dcdoff;		
};

struct scc_hardware {
	char *name;
	int io_region;
	int io_delta;
	int io_size;
	int num_devs;
	int scc_offset;
	int tmr_offset;
	int tmr_hz;
	int pclk_hz;
};

struct scc_priv {
	int type;
	int chip;
	struct net_device *dev;
	struct scc_info *info;

	int channel;
	int card_base, scc_cmd, scc_data;
	int tmr_cnt, tmr_ctrl, tmr_mode;
	struct scc_param param;
	char rx_buf[NUM_RX_BUF][BUF_SIZE];
	int rx_len[NUM_RX_BUF];
	int rx_ptr;
	struct work_struct rx_work;
	int rx_head, rx_tail, rx_count;
	int rx_over;
	char tx_buf[NUM_TX_BUF][BUF_SIZE];
	int tx_len[NUM_TX_BUF];
	int tx_ptr;
	int tx_head, tx_tail, tx_count;
	int state;
	unsigned long tx_start;
	int rr0;
	spinlock_t *register_lock;	
	spinlock_t ring_lock;
};

struct scc_info {
	int irq_used;
	int twin_serial_cfg;
	struct net_device *dev[2];
	struct scc_priv priv[2];
	struct scc_info *next;
	spinlock_t register_lock;	
};


static int setup_adapter(int card_base, int type, int n) __init;

static void write_scc(struct scc_priv *priv, int reg, int val);
static void write_scc_data(struct scc_priv *priv, int val, int fast);
static int read_scc(struct scc_priv *priv, int reg);
static int read_scc_data(struct scc_priv *priv);

static int scc_open(struct net_device *dev);
static int scc_close(struct net_device *dev);
static int scc_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
static int scc_send_packet(struct sk_buff *skb, struct net_device *dev);
static int scc_set_mac_address(struct net_device *dev, void *sa);

static inline void tx_on(struct scc_priv *priv);
static inline void rx_on(struct scc_priv *priv);
static inline void rx_off(struct scc_priv *priv);
static void start_timer(struct scc_priv *priv, int t, int r15);
static inline unsigned char random(void);

static inline void z8530_isr(struct scc_info *info);
static irqreturn_t scc_isr(int irq, void *dev_id);
static void rx_isr(struct scc_priv *priv);
static void special_condition(struct scc_priv *priv, int rc);
static void rx_bh(struct work_struct *);
static void tx_isr(struct scc_priv *priv);
static void es_isr(struct scc_priv *priv);
static void tm_isr(struct scc_priv *priv);



static int io[MAX_NUM_DEVS] __initdata = { 0, };

static struct scc_hardware hw[NUM_TYPES] = HARDWARE;



static struct scc_info *first;
static unsigned long rand;


MODULE_AUTHOR("Klaus Kudielka");
MODULE_DESCRIPTION("Driver for high-speed SCC boards");
module_param_array(io, int, NULL, 0);
MODULE_LICENSE("GPL");

static void __exit dmascc_exit(void)
{
	int i;
	struct scc_info *info;

	while (first) {
		info = first;

		
		for (i = 0; i < 2; i++)
			unregister_netdev(info->dev[i]);

		
		if (info->priv[0].type == TYPE_TWIN)
			outb(0, info->dev[0]->base_addr + TWIN_SERIAL_CFG);
		write_scc(&info->priv[0], R9, FHWRES);
		release_region(info->dev[0]->base_addr,
			       hw[info->priv[0].type].io_size);

		for (i = 0; i < 2; i++)
			free_netdev(info->dev[i]);

		
		first = info->next;
		kfree(info);
	}
}

static int __init dmascc_init(void)
{
	int h, i, j, n;
	int base[MAX_NUM_DEVS], tcmd[MAX_NUM_DEVS], t0[MAX_NUM_DEVS],
	    t1[MAX_NUM_DEVS];
	unsigned t_val;
	unsigned long time, start[MAX_NUM_DEVS], delay[MAX_NUM_DEVS],
	    counting[MAX_NUM_DEVS];

	
	rand = jiffies;
	
	n = 0;
	
	if (!io[0])
		printk(KERN_INFO "dmascc: autoprobing (dangerous)\n");

	
	for (h = 0; h < NUM_TYPES; h++) {

		if (io[0]) {
			
			for (i = 0; i < hw[h].num_devs; i++)
				base[i] = 0;
			for (i = 0; i < MAX_NUM_DEVS && io[i]; i++) {
				j = (io[i] -
				     hw[h].io_region) / hw[h].io_delta;
				if (j >= 0 && j < hw[h].num_devs &&
				    hw[h].io_region +
				    j * hw[h].io_delta == io[i]) {
					base[j] = io[i];
				}
			}
		} else {
			
			for (i = 0; i < hw[h].num_devs; i++) {
				base[i] =
				    hw[h].io_region + i * hw[h].io_delta;
			}
		}

		
		for (i = 0; i < hw[h].num_devs; i++)
			if (base[i]) {
				if (!request_region
				    (base[i], hw[h].io_size, "dmascc"))
					base[i] = 0;
				else {
					tcmd[i] =
					    base[i] + hw[h].tmr_offset +
					    TMR_CTRL;
					t0[i] =
					    base[i] + hw[h].tmr_offset +
					    TMR_CNT0;
					t1[i] =
					    base[i] + hw[h].tmr_offset +
					    TMR_CNT1;
				}
			}

		
		for (i = 0; i < hw[h].num_devs; i++)
			if (base[i]) {
				
				outb(0x36, tcmd[i]);
				outb((hw[h].tmr_hz / TMR_0_HZ) & 0xFF,
				     t0[i]);
				outb((hw[h].tmr_hz / TMR_0_HZ) >> 8,
				     t0[i]);
				
				outb(0x70, tcmd[i]);
				outb((TMR_0_HZ / HZ * 10) & 0xFF, t1[i]);
				outb((TMR_0_HZ / HZ * 10) >> 8, t1[i]);
				start[i] = jiffies;
				delay[i] = 0;
				counting[i] = 1;
				
				outb(0xb0, tcmd[i]);
			}
		time = jiffies;
		
		udelay(2000000 / TMR_0_HZ);

		
		while (jiffies - time < 13) {
			for (i = 0; i < hw[h].num_devs; i++)
				if (base[i] && counting[i]) {
					
					outb(0x40, tcmd[i]);
					t_val =
					    inb(t1[i]) + (inb(t1[i]) << 8);
					
					if (t_val == 0 ||
					    t_val > TMR_0_HZ / HZ * 10)
						counting[i] = 0;
					delay[i] = jiffies - start[i];
				}
		}

		
		for (i = 0; i < hw[h].num_devs; i++)
			if (base[i]) {
				if ((delay[i] >= 9 && delay[i] <= 11) &&
				    
				    (setup_adapter(base[i], h, n) == 0))
					n++;
				else
					release_region(base[i],
						       hw[h].io_size);
			}

	}			

	
	if (n)
		return 0;

	
	printk(KERN_INFO "dmascc: no adapters found\n");
	return -EIO;
}

module_init(dmascc_init);
module_exit(dmascc_exit);

static void __init dev_setup(struct net_device *dev)
{
	dev->type = ARPHRD_AX25;
	dev->hard_header_len = AX25_MAX_HEADER_LEN;
	dev->mtu = 1500;
	dev->addr_len = AX25_ADDR_LEN;
	dev->tx_queue_len = 64;
	memcpy(dev->broadcast, &ax25_bcast, AX25_ADDR_LEN);
	memcpy(dev->dev_addr, &ax25_defaddr, AX25_ADDR_LEN);
}

static const struct net_device_ops scc_netdev_ops = {
	.ndo_open = scc_open,
	.ndo_stop = scc_close,
	.ndo_start_xmit = scc_send_packet,
	.ndo_do_ioctl = scc_ioctl,
	.ndo_set_mac_address = scc_set_mac_address,
};

static int __init setup_adapter(int card_base, int type, int n)
{
	int i, irq, chip;
	struct scc_info *info;
	struct net_device *dev;
	struct scc_priv *priv;
	unsigned long time;
	unsigned int irqs;
	int tmr_base = card_base + hw[type].tmr_offset;
	int scc_base = card_base + hw[type].scc_offset;
	char *chipnames[] = CHIPNAMES;

	
	info = kzalloc(sizeof(struct scc_info), GFP_KERNEL | GFP_DMA);
	if (!info) {
		printk(KERN_ERR "dmascc: "
		       "could not allocate memory for %s at %#3x\n",
		       hw[type].name, card_base);
		goto out;
	}


	info->dev[0] = alloc_netdev(0, "", dev_setup);
	if (!info->dev[0]) {
		printk(KERN_ERR "dmascc: "
		       "could not allocate memory for %s at %#3x\n",
		       hw[type].name, card_base);
		goto out1;
	}

	info->dev[1] = alloc_netdev(0, "", dev_setup);
	if (!info->dev[1]) {
		printk(KERN_ERR "dmascc: "
		       "could not allocate memory for %s at %#3x\n",
		       hw[type].name, card_base);
		goto out2;
	}
	spin_lock_init(&info->register_lock);

	priv = &info->priv[0];
	priv->type = type;
	priv->card_base = card_base;
	priv->scc_cmd = scc_base + SCCA_CMD;
	priv->scc_data = scc_base + SCCA_DATA;
	priv->register_lock = &info->register_lock;

	
	write_scc(priv, R9, FHWRES | MIE | NV);

	
	write_scc(priv, R15, SHDLCE);
	if (!read_scc(priv, R15)) {
		
		chip = Z8530;
	} else {
		
		write_scc_data(priv, 0, 0);
		if (read_scc(priv, R0) & Tx_BUF_EMP) {
			
			chip = Z85230;
		} else {
			
			chip = Z85C30;
		}
	}
	write_scc(priv, R15, 0);

	
	irqs = probe_irq_on();

	
	if (type == TYPE_TWIN) {
		outb(0, card_base + TWIN_DMA_CFG);
		inb(card_base + TWIN_CLR_TMR1);
		inb(card_base + TWIN_CLR_TMR2);
		info->twin_serial_cfg = TWIN_EI;
		outb(info->twin_serial_cfg, card_base + TWIN_SERIAL_CFG);
	} else {
		write_scc(priv, R15, CTSIE);
		write_scc(priv, R0, RES_EXT_INT);
		write_scc(priv, R1, EXT_INT_ENAB);
	}

	
	outb(1, tmr_base + TMR_CNT1);
	outb(0, tmr_base + TMR_CNT1);

	
	time = jiffies;
	while (jiffies - time < 2 + HZ / TMR_0_HZ);
	irq = probe_irq_off(irqs);

	
	if (type == TYPE_TWIN) {
		inb(card_base + TWIN_CLR_TMR1);
	} else {
		write_scc(priv, R1, 0);
		write_scc(priv, R15, 0);
		write_scc(priv, R0, RES_EXT_INT);
	}

	if (irq <= 0) {
		printk(KERN_ERR
		       "dmascc: could not find irq of %s at %#3x (irq=%d)\n",
		       hw[type].name, card_base, irq);
		goto out3;
	}

	
	for (i = 0; i < 2; i++) {
		dev = info->dev[i];
		priv = &info->priv[i];
		priv->type = type;
		priv->chip = chip;
		priv->dev = dev;
		priv->info = info;
		priv->channel = i;
		spin_lock_init(&priv->ring_lock);
		priv->register_lock = &info->register_lock;
		priv->card_base = card_base;
		priv->scc_cmd = scc_base + (i ? SCCB_CMD : SCCA_CMD);
		priv->scc_data = scc_base + (i ? SCCB_DATA : SCCA_DATA);
		priv->tmr_cnt = tmr_base + (i ? TMR_CNT2 : TMR_CNT1);
		priv->tmr_ctrl = tmr_base + TMR_CTRL;
		priv->tmr_mode = i ? 0xb0 : 0x70;
		priv->param.pclk_hz = hw[type].pclk_hz;
		priv->param.brg_tc = -1;
		priv->param.clocks = TCTRxCP | RCRTxCP;
		priv->param.persist = 256;
		priv->param.dma = -1;
		INIT_WORK(&priv->rx_work, rx_bh);
		dev->ml_priv = priv;
		sprintf(dev->name, "dmascc%i", 2 * n + i);
		dev->base_addr = card_base;
		dev->irq = irq;
		dev->netdev_ops = &scc_netdev_ops;
		dev->header_ops = &ax25_header_ops;
	}
	if (register_netdev(info->dev[0])) {
		printk(KERN_ERR "dmascc: could not register %s\n",
		       info->dev[0]->name);
		goto out3;
	}
	if (register_netdev(info->dev[1])) {
		printk(KERN_ERR "dmascc: could not register %s\n",
		       info->dev[1]->name);
		goto out4;
	}


	info->next = first;
	first = info;
	printk(KERN_INFO "dmascc: found %s (%s) at %#3x, irq %d\n",
	       hw[type].name, chipnames[chip], card_base, irq);
	return 0;

      out4:
	unregister_netdev(info->dev[0]);
      out3:
	if (info->priv[0].type == TYPE_TWIN)
		outb(0, info->dev[0]->base_addr + TWIN_SERIAL_CFG);
	write_scc(&info->priv[0], R9, FHWRES);
	free_netdev(info->dev[1]);
      out2:
	free_netdev(info->dev[0]);
      out1:
	kfree(info);
      out:
	return -1;
}



static void write_scc(struct scc_priv *priv, int reg, int val)
{
	unsigned long flags;
	switch (priv->type) {
	case TYPE_S5:
		if (reg)
			outb(reg, priv->scc_cmd);
		outb(val, priv->scc_cmd);
		return;
	case TYPE_TWIN:
		if (reg)
			outb_p(reg, priv->scc_cmd);
		outb_p(val, priv->scc_cmd);
		return;
	default:
		spin_lock_irqsave(priv->register_lock, flags);
		outb_p(0, priv->card_base + PI_DREQ_MASK);
		if (reg)
			outb_p(reg, priv->scc_cmd);
		outb_p(val, priv->scc_cmd);
		outb(1, priv->card_base + PI_DREQ_MASK);
		spin_unlock_irqrestore(priv->register_lock, flags);
		return;
	}
}


static void write_scc_data(struct scc_priv *priv, int val, int fast)
{
	unsigned long flags;
	switch (priv->type) {
	case TYPE_S5:
		outb(val, priv->scc_data);
		return;
	case TYPE_TWIN:
		outb_p(val, priv->scc_data);
		return;
	default:
		if (fast)
			outb_p(val, priv->scc_data);
		else {
			spin_lock_irqsave(priv->register_lock, flags);
			outb_p(0, priv->card_base + PI_DREQ_MASK);
			outb_p(val, priv->scc_data);
			outb(1, priv->card_base + PI_DREQ_MASK);
			spin_unlock_irqrestore(priv->register_lock, flags);
		}
		return;
	}
}


static int read_scc(struct scc_priv *priv, int reg)
{
	int rc;
	unsigned long flags;
	switch (priv->type) {
	case TYPE_S5:
		if (reg)
			outb(reg, priv->scc_cmd);
		return inb(priv->scc_cmd);
	case TYPE_TWIN:
		if (reg)
			outb_p(reg, priv->scc_cmd);
		return inb_p(priv->scc_cmd);
	default:
		spin_lock_irqsave(priv->register_lock, flags);
		outb_p(0, priv->card_base + PI_DREQ_MASK);
		if (reg)
			outb_p(reg, priv->scc_cmd);
		rc = inb_p(priv->scc_cmd);
		outb(1, priv->card_base + PI_DREQ_MASK);
		spin_unlock_irqrestore(priv->register_lock, flags);
		return rc;
	}
}


static int read_scc_data(struct scc_priv *priv)
{
	int rc;
	unsigned long flags;
	switch (priv->type) {
	case TYPE_S5:
		return inb(priv->scc_data);
	case TYPE_TWIN:
		return inb_p(priv->scc_data);
	default:
		spin_lock_irqsave(priv->register_lock, flags);
		outb_p(0, priv->card_base + PI_DREQ_MASK);
		rc = inb_p(priv->scc_data);
		outb(1, priv->card_base + PI_DREQ_MASK);
		spin_unlock_irqrestore(priv->register_lock, flags);
		return rc;
	}
}


static int scc_open(struct net_device *dev)
{
	struct scc_priv *priv = dev->ml_priv;
	struct scc_info *info = priv->info;
	int card_base = priv->card_base;

	
	if (!info->irq_used) {
		if (request_irq(dev->irq, scc_isr, 0, "dmascc", info)) {
			return -EAGAIN;
		}
	}
	info->irq_used++;

	
	if (priv->param.dma >= 0) {
		if (request_dma(priv->param.dma, "dmascc")) {
			if (--info->irq_used == 0)
				free_irq(dev->irq, info);
			return -EAGAIN;
		} else {
			unsigned long flags = claim_dma_lock();
			clear_dma_ff(priv->param.dma);
			release_dma_lock(flags);
		}
	}

	
	priv->rx_ptr = 0;
	priv->rx_over = 0;
	priv->rx_head = priv->rx_tail = priv->rx_count = 0;
	priv->state = IDLE;
	priv->tx_head = priv->tx_tail = priv->tx_count = 0;
	priv->tx_ptr = 0;

	
	write_scc(priv, R9, (priv->channel ? CHRB : CHRA) | MIE | NV);
	
	write_scc(priv, R4, SDLC | X1CLK);
	
	write_scc(priv, R1, EXT_INT_ENAB | WT_FN_RDYFN);
	
	write_scc(priv, R3, Rx8);
	
	write_scc(priv, R5, Tx8);
	
	write_scc(priv, R6, 0);
	
	write_scc(priv, R7, FLAG);
	switch (priv->chip) {
	case Z85C30:
		
		write_scc(priv, R15, SHDLCE);
		
		write_scc(priv, R7, AUTOEOM);
		write_scc(priv, R15, 0);
		break;
	case Z85230:
		
		write_scc(priv, R15, SHDLCE);
		if (priv->param.dma >= 0) {
			if (priv->type == TYPE_TWIN)
				write_scc(priv, R7, AUTOEOM | TXFIFOE);
			else
				write_scc(priv, R7, AUTOEOM);
		} else {
			write_scc(priv, R7, AUTOEOM | RXFIFOH);
		}
		write_scc(priv, R15, 0);
		break;
	}
	
	write_scc(priv, R10, CRCPS | (priv->param.nrzi ? NRZI : NRZ));

	
	if (priv->param.brg_tc >= 0) {
		
		write_scc(priv, R12, priv->param.brg_tc & 0xFF);
		write_scc(priv, R13, (priv->param.brg_tc >> 8) & 0xFF);
		write_scc(priv, R14, SSBR | DTRREQ | BRSRC | BRENABL);
		
		write_scc(priv, R14, SEARCH | DTRREQ | BRSRC | BRENABL);
	} else {
		
		write_scc(priv, R14, DTRREQ | BRSRC);
	}

	
	if (priv->type == TYPE_TWIN) {
		
		outb((info->twin_serial_cfg &=
		      ~(priv->channel ? TWIN_EXTCLKB : TWIN_EXTCLKA)),
		     card_base + TWIN_SERIAL_CFG);
	}
	write_scc(priv, R11, priv->param.clocks);
	if ((priv->type == TYPE_TWIN) && !(priv->param.clocks & TRxCOI)) {
		
		outb((info->twin_serial_cfg |=
		      (priv->channel ? TWIN_EXTCLKB : TWIN_EXTCLKA)),
		     card_base + TWIN_SERIAL_CFG);
	}

	
	if (priv->type == TYPE_TWIN) {
		
		outb((info->twin_serial_cfg |= TWIN_EI |
		      (priv->channel ? TWIN_DTRB_ON : TWIN_DTRA_ON)),
		     card_base + TWIN_SERIAL_CFG);
	}

	
	priv->rr0 = read_scc(priv, R0);
	
	write_scc(priv, R15, DCDIE);

	netif_start_queue(dev);

	return 0;
}


static int scc_close(struct net_device *dev)
{
	struct scc_priv *priv = dev->ml_priv;
	struct scc_info *info = priv->info;
	int card_base = priv->card_base;

	netif_stop_queue(dev);

	if (priv->type == TYPE_TWIN) {
		
		outb((info->twin_serial_cfg &=
		      (priv->channel ? ~TWIN_DTRB_ON : ~TWIN_DTRA_ON)),
		     card_base + TWIN_SERIAL_CFG);
	}

	
	write_scc(priv, R9, (priv->channel ? CHRB : CHRA) | MIE | NV);
	if (priv->param.dma >= 0) {
		if (priv->type == TYPE_TWIN)
			outb(0, card_base + TWIN_DMA_CFG);
		free_dma(priv->param.dma);
	}
	if (--info->irq_used == 0)
		free_irq(dev->irq, info);

	return 0;
}


static int scc_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct scc_priv *priv = dev->ml_priv;

	switch (cmd) {
	case SIOCGSCCPARAM:
		if (copy_to_user
		    (ifr->ifr_data, &priv->param,
		     sizeof(struct scc_param)))
			return -EFAULT;
		return 0;
	case SIOCSSCCPARAM:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		if (netif_running(dev))
			return -EAGAIN;
		if (copy_from_user
		    (&priv->param, ifr->ifr_data,
		     sizeof(struct scc_param)))
			return -EFAULT;
		return 0;
	default:
		return -EINVAL;
	}
}


static int scc_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct scc_priv *priv = dev->ml_priv;
	unsigned long flags;
	int i;

	
	netif_stop_queue(dev);

	
	i = priv->tx_head;
	skb_copy_from_linear_data_offset(skb, 1, priv->tx_buf[i], skb->len - 1);
	priv->tx_len[i] = skb->len - 1;

	

	spin_lock_irqsave(&priv->ring_lock, flags);
	
	priv->tx_head = (i + 1) % NUM_TX_BUF;
	priv->tx_count++;

	if (priv->tx_count < NUM_TX_BUF)
		netif_wake_queue(dev);

	
	if (priv->state == IDLE) {
		
		priv->state = TX_HEAD;
		priv->tx_start = jiffies;
		write_scc(priv, R5, TxCRC_ENAB | RTS | TxENAB | Tx8);
		write_scc(priv, R15, 0);
		start_timer(priv, priv->param.txdelay, 0);
	}

	
	spin_unlock_irqrestore(&priv->ring_lock, flags);
	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}


static int scc_set_mac_address(struct net_device *dev, void *sa)
{
	memcpy(dev->dev_addr, ((struct sockaddr *) sa)->sa_data,
	       dev->addr_len);
	return 0;
}


static inline void tx_on(struct scc_priv *priv)
{
	int i, n;
	unsigned long flags;

	if (priv->param.dma >= 0) {
		n = (priv->chip == Z85230) ? 3 : 1;
		
		flags = claim_dma_lock();
		set_dma_mode(priv->param.dma, DMA_MODE_WRITE);
		set_dma_addr(priv->param.dma,
			     (int) priv->tx_buf[priv->tx_tail] + n);
		set_dma_count(priv->param.dma,
			      priv->tx_len[priv->tx_tail] - n);
		release_dma_lock(flags);
		
		write_scc(priv, R15, TxUIE);
		
		if (priv->type == TYPE_TWIN)
			outb((priv->param.dma ==
			      1) ? TWIN_DMA_HDX_T1 : TWIN_DMA_HDX_T3,
			     priv->card_base + TWIN_DMA_CFG);
		else
			write_scc(priv, R1,
				  EXT_INT_ENAB | WT_FN_RDYFN |
				  WT_RDY_ENAB);
		
		spin_lock_irqsave(priv->register_lock, flags);
		for (i = 0; i < n; i++)
			write_scc_data(priv,
				       priv->tx_buf[priv->tx_tail][i], 1);
		enable_dma(priv->param.dma);
		spin_unlock_irqrestore(priv->register_lock, flags);
	} else {
		write_scc(priv, R15, TxUIE);
		write_scc(priv, R1,
			  EXT_INT_ENAB | WT_FN_RDYFN | TxINT_ENAB);
		tx_isr(priv);
	}
	
	if (priv->chip == Z8530)
		write_scc(priv, R0, RES_EOM_L);
}


static inline void rx_on(struct scc_priv *priv)
{
	unsigned long flags;

	
	while (read_scc(priv, R0) & Rx_CH_AV)
		read_scc_data(priv);
	priv->rx_over = 0;
	if (priv->param.dma >= 0) {
		
		flags = claim_dma_lock();
		set_dma_mode(priv->param.dma, DMA_MODE_READ);
		set_dma_addr(priv->param.dma,
			     (int) priv->rx_buf[priv->rx_head]);
		set_dma_count(priv->param.dma, BUF_SIZE);
		release_dma_lock(flags);
		enable_dma(priv->param.dma);
		
		if (priv->type == TYPE_TWIN) {
			outb((priv->param.dma ==
			      1) ? TWIN_DMA_HDX_R1 : TWIN_DMA_HDX_R3,
			     priv->card_base + TWIN_DMA_CFG);
		}
		
		write_scc(priv, R1, EXT_INT_ENAB | INT_ERR_Rx |
			  WT_RDY_RT | WT_FN_RDYFN | WT_RDY_ENAB);
	} else {
		
		priv->rx_ptr = 0;
		
		write_scc(priv, R1, EXT_INT_ENAB | INT_ALL_Rx | WT_RDY_RT |
			  WT_FN_RDYFN);
	}
	write_scc(priv, R0, ERR_RES);
	write_scc(priv, R3, RxENABLE | Rx8 | RxCRC_ENAB);
}


static inline void rx_off(struct scc_priv *priv)
{
	
	write_scc(priv, R3, Rx8);
	
	if (priv->param.dma >= 0 && priv->type == TYPE_TWIN)
		outb(0, priv->card_base + TWIN_DMA_CFG);
	else
		write_scc(priv, R1, EXT_INT_ENAB | WT_FN_RDYFN);
	
	if (priv->param.dma >= 0)
		disable_dma(priv->param.dma);
}


static void start_timer(struct scc_priv *priv, int t, int r15)
{
	outb(priv->tmr_mode, priv->tmr_ctrl);
	if (t == 0) {
		tm_isr(priv);
	} else if (t > 0) {
		outb(t & 0xFF, priv->tmr_cnt);
		outb((t >> 8) & 0xFF, priv->tmr_cnt);
		if (priv->type != TYPE_TWIN) {
			write_scc(priv, R15, r15 | CTSIE);
			priv->rr0 |= CTS;
		}
	}
}


static inline unsigned char random(void)
{
	
	rand = rand * 1664525L + 1013904223L;
	return (unsigned char) (rand >> 24);
}

static inline void z8530_isr(struct scc_info *info)
{
	int is, i = 100;

	while ((is = read_scc(&info->priv[0], R3)) && i--) {
		if (is & CHARxIP) {
			rx_isr(&info->priv[0]);
		} else if (is & CHATxIP) {
			tx_isr(&info->priv[0]);
		} else if (is & CHAEXT) {
			es_isr(&info->priv[0]);
		} else if (is & CHBRxIP) {
			rx_isr(&info->priv[1]);
		} else if (is & CHBTxIP) {
			tx_isr(&info->priv[1]);
		} else {
			es_isr(&info->priv[1]);
		}
		write_scc(&info->priv[0], R0, RES_H_IUS);
		i++;
	}
	if (i < 0) {
		printk(KERN_ERR "dmascc: stuck in ISR with RR3=0x%02x.\n",
		       is);
	}
}


static irqreturn_t scc_isr(int irq, void *dev_id)
{
	struct scc_info *info = dev_id;

	spin_lock(info->priv[0].register_lock);

	if (info->priv[0].type == TYPE_TWIN) {
		int is, card_base = info->priv[0].card_base;
		while ((is = ~inb(card_base + TWIN_INT_REG)) &
		       TWIN_INT_MSK) {
			if (is & TWIN_SCC_MSK) {
				z8530_isr(info);
			} else if (is & TWIN_TMR1_MSK) {
				inb(card_base + TWIN_CLR_TMR1);
				tm_isr(&info->priv[0]);
			} else {
				inb(card_base + TWIN_CLR_TMR2);
				tm_isr(&info->priv[1]);
			}
		}
	} else
		z8530_isr(info);
	spin_unlock(info->priv[0].register_lock);
	return IRQ_HANDLED;
}


static void rx_isr(struct scc_priv *priv)
{
	if (priv->param.dma >= 0) {
		
		special_condition(priv, read_scc(priv, R1));
		write_scc(priv, R0, ERR_RES);
	} else {
		int rc;
		while (read_scc(priv, R0) & Rx_CH_AV) {
			rc = read_scc(priv, R1);
			if (priv->rx_ptr < BUF_SIZE)
				priv->rx_buf[priv->rx_head][priv->
							    rx_ptr++] =
				    read_scc_data(priv);
			else {
				priv->rx_over = 2;
				read_scc_data(priv);
			}
			special_condition(priv, rc);
		}
	}
}


static void special_condition(struct scc_priv *priv, int rc)
{
	int cb;
	unsigned long flags;

	

	if (rc & Rx_OVR) {
		
		priv->rx_over = 1;
		if (priv->param.dma < 0)
			write_scc(priv, R0, ERR_RES);
	} else if (rc & END_FR) {
		
		if (priv->param.dma >= 0) {
			flags = claim_dma_lock();
			cb = BUF_SIZE - get_dma_residue(priv->param.dma) -
			    2;
			release_dma_lock(flags);
		} else {
			cb = priv->rx_ptr - 2;
		}
		if (priv->rx_over) {
			
			priv->dev->stats.rx_errors++;
			if (priv->rx_over == 2)
				priv->dev->stats.rx_length_errors++;
			else
				priv->dev->stats.rx_fifo_errors++;
			priv->rx_over = 0;
		} else if (rc & CRC_ERR) {
			
			if (cb >= 15) {
				priv->dev->stats.rx_errors++;
				priv->dev->stats.rx_crc_errors++;
			}
		} else {
			if (cb >= 15) {
				if (priv->rx_count < NUM_RX_BUF - 1) {
					
					priv->rx_len[priv->rx_head] = cb;
					priv->rx_head =
					    (priv->rx_head +
					     1) % NUM_RX_BUF;
					priv->rx_count++;
					schedule_work(&priv->rx_work);
				} else {
					priv->dev->stats.rx_errors++;
					priv->dev->stats.rx_over_errors++;
				}
			}
		}
		
		if (priv->param.dma >= 0) {
			flags = claim_dma_lock();
			set_dma_addr(priv->param.dma,
				     (int) priv->rx_buf[priv->rx_head]);
			set_dma_count(priv->param.dma, BUF_SIZE);
			release_dma_lock(flags);
		} else {
			priv->rx_ptr = 0;
		}
	}
}


static void rx_bh(struct work_struct *ugli_api)
{
	struct scc_priv *priv = container_of(ugli_api, struct scc_priv, rx_work);
	int i = priv->rx_tail;
	int cb;
	unsigned long flags;
	struct sk_buff *skb;
	unsigned char *data;

	spin_lock_irqsave(&priv->ring_lock, flags);
	while (priv->rx_count) {
		spin_unlock_irqrestore(&priv->ring_lock, flags);
		cb = priv->rx_len[i];
		
		skb = dev_alloc_skb(cb + 1);
		if (skb == NULL) {
			
			priv->dev->stats.rx_dropped++;
		} else {
			
			data = skb_put(skb, cb + 1);
			data[0] = 0;
			memcpy(&data[1], priv->rx_buf[i], cb);
			skb->protocol = ax25_type_trans(skb, priv->dev);
			netif_rx(skb);
			priv->dev->stats.rx_packets++;
			priv->dev->stats.rx_bytes += cb;
		}
		spin_lock_irqsave(&priv->ring_lock, flags);
		
		priv->rx_tail = i = (i + 1) % NUM_RX_BUF;
		priv->rx_count--;
	}
	spin_unlock_irqrestore(&priv->ring_lock, flags);
}


static void tx_isr(struct scc_priv *priv)
{
	int i = priv->tx_tail, p = priv->tx_ptr;

	if (p == priv->tx_len[i]) {
		write_scc(priv, R0, RES_Tx_P);
		return;
	}

	
	while ((read_scc(priv, R0) & Tx_BUF_EMP) && p < priv->tx_len[i]) {
		write_scc_data(priv, priv->tx_buf[i][p++], 0);
	}

	
	if (!priv->tx_ptr && p && priv->chip == Z8530)
		write_scc(priv, R0, RES_EOM_L);

	priv->tx_ptr = p;
}


static void es_isr(struct scc_priv *priv)
{
	int i, rr0, drr0, res;
	unsigned long flags;

	
	rr0 = read_scc(priv, R0);
	write_scc(priv, R0, RES_EXT_INT);
	drr0 = priv->rr0 ^ rr0;
	priv->rr0 = rr0;

	if (priv->state == TX_DATA) {
		
		i = priv->tx_tail;
		if (priv->param.dma >= 0) {
			disable_dma(priv->param.dma);
			flags = claim_dma_lock();
			res = get_dma_residue(priv->param.dma);
			release_dma_lock(flags);
		} else {
			res = priv->tx_len[i] - priv->tx_ptr;
			priv->tx_ptr = 0;
		}
		
		if (priv->param.dma >= 0 && priv->type == TYPE_TWIN)
			outb(0, priv->card_base + TWIN_DMA_CFG);
		else
			write_scc(priv, R1, EXT_INT_ENAB | WT_FN_RDYFN);
		if (res) {
			
			priv->dev->stats.tx_errors++;
			priv->dev->stats.tx_fifo_errors++;
			
			write_scc(priv, R0, RES_EXT_INT);
			write_scc(priv, R0, RES_EXT_INT);
		} else {
			
			priv->dev->stats.tx_packets++;
			priv->dev->stats.tx_bytes += priv->tx_len[i];
			
			priv->tx_tail = (i + 1) % NUM_TX_BUF;
			priv->tx_count--;
			
			netif_wake_queue(priv->dev);
		}
		
		write_scc(priv, R15, 0);
		if (priv->tx_count &&
		    (jiffies - priv->tx_start) < priv->param.txtimeout) {
			priv->state = TX_PAUSE;
			start_timer(priv, priv->param.txpause, 0);
		} else {
			priv->state = TX_TAIL;
			start_timer(priv, priv->param.txtail, 0);
		}
	}

	
	if (drr0 & DCD) {
		if (rr0 & DCD) {
			switch (priv->state) {
			case IDLE:
			case WAIT:
				priv->state = DCD_ON;
				write_scc(priv, R15, 0);
				start_timer(priv, priv->param.dcdon, 0);
			}
		} else {
			switch (priv->state) {
			case RX_ON:
				rx_off(priv);
				priv->state = DCD_OFF;
				write_scc(priv, R15, 0);
				start_timer(priv, priv->param.dcdoff, 0);
			}
		}
	}

	
	if ((drr0 & CTS) && (~rr0 & CTS) && priv->type != TYPE_TWIN)
		tm_isr(priv);

}


static void tm_isr(struct scc_priv *priv)
{
	switch (priv->state) {
	case TX_HEAD:
	case TX_PAUSE:
		tx_on(priv);
		priv->state = TX_DATA;
		break;
	case TX_TAIL:
		write_scc(priv, R5, TxCRC_ENAB | Tx8);
		priv->state = RTS_OFF;
		if (priv->type != TYPE_TWIN)
			write_scc(priv, R15, 0);
		start_timer(priv, priv->param.rtsoff, 0);
		break;
	case RTS_OFF:
		write_scc(priv, R15, DCDIE);
		priv->rr0 = read_scc(priv, R0);
		if (priv->rr0 & DCD) {
			priv->dev->stats.collisions++;
			rx_on(priv);
			priv->state = RX_ON;
		} else {
			priv->state = WAIT;
			start_timer(priv, priv->param.waittime, DCDIE);
		}
		break;
	case WAIT:
		if (priv->tx_count) {
			priv->state = TX_HEAD;
			priv->tx_start = jiffies;
			write_scc(priv, R5,
				  TxCRC_ENAB | RTS | TxENAB | Tx8);
			write_scc(priv, R15, 0);
			start_timer(priv, priv->param.txdelay, 0);
		} else {
			priv->state = IDLE;
			if (priv->type != TYPE_TWIN)
				write_scc(priv, R15, DCDIE);
		}
		break;
	case DCD_ON:
	case DCD_OFF:
		write_scc(priv, R15, DCDIE);
		priv->rr0 = read_scc(priv, R0);
		if (priv->rr0 & DCD) {
			rx_on(priv);
			priv->state = RX_ON;
		} else {
			priv->state = WAIT;
			start_timer(priv,
				    random() / priv->param.persist *
				    priv->param.slottime, DCDIE);
		}
		break;
	}
}
