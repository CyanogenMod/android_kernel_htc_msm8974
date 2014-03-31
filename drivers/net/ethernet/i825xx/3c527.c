/* 3c527.c: 3Com Etherlink/MC32 driver for Linux 2.4 and 2.6.
 *
 *	(c) Copyright 1998 Red Hat Software Inc
 *	Written by Alan Cox.
 *	Further debugging by Carl Drougge.
 *      Initial SMP support by Felipe W Damasio <felipewd@terra.com.br>
 *      Heavily modified by Richard Procter <rnp@paradise.net.nz>
 *
 *	Based on skeleton.c written 1993-94 by Donald Becker and ne2.c
 *	(for the MCA stuff) written by Wim Dumon.
 *
 *	Thanks to 3Com for making this possible by providing me with the
 *	documentation.
 *
 *	This software may be used and distributed according to the terms
 *	of the GNU General Public License, incorporated herein by reference.
 *
 */

#define DRV_NAME		"3c527"
#define DRV_VERSION		"0.7-SMP"
#define DRV_RELDATE		"2003/09/21"

static const char *version =
DRV_NAME ".c:v" DRV_VERSION " " DRV_RELDATE " Richard Procter <rnp@paradise.net.nz>\n";


#include <linux/module.h>

#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/mca-legacy.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/ethtool.h>
#include <linux/completion.h>
#include <linux/bitops.h>
#include <linux/semaphore.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>

#include "3c527.h"

MODULE_LICENSE("GPL");

static const char* cardname = DRV_NAME;

#ifndef NET_DEBUG
#define NET_DEBUG 2
#endif

static unsigned int mc32_debug = NET_DEBUG;

#define MC32_IO_EXTENT	8

#define TX_RING_LEN     32       
#define RX_RING_LEN     8        

#define RX_COPYBREAK    200      

static const int WORKAROUND_82586=1;

struct mc32_ring_desc
{
	volatile struct skb_header *p;
	struct sk_buff *skb;
};

struct mc32_local
{
	int slot;

	u32 base;
	volatile struct mc32_mailbox *rx_box;
	volatile struct mc32_mailbox *tx_box;
	volatile struct mc32_mailbox *exec_box;
        volatile struct mc32_stats *stats;    
        u16 tx_chain;           
	u16 rx_chain;           
        u16 tx_len;             
        u16 rx_len;             

	u16 xceiver_desired_state; 
	u16 cmd_nonblocking;    
	u16 mc_reload_wait;	
	u32 mc_list_valid;	

	struct mc32_ring_desc tx_ring[TX_RING_LEN];	
	struct mc32_ring_desc rx_ring[RX_RING_LEN];	

	atomic_t tx_count;	
	atomic_t tx_ring_head;  
	u16 tx_ring_tail;       

	u16 rx_ring_tail;       

	struct semaphore cmd_mutex;    
        struct completion execution_cmd; 
	struct completion xceiver_cmd;   
};

#define SA_ADDR0 0x02
#define SA_ADDR1 0x60
#define SA_ADDR2 0xAC

struct mca_adapters_t {
	unsigned int	id;
	char		*name;
};

static const struct mca_adapters_t mc32_adapters[] = {
	{ 0x0041, "3COM EtherLink MC/32" },
	{ 0x8EF5, "IBM High Performance Lan Adapter" },
	{ 0x0000, NULL }
};


static inline u16 next_rx(u16 rx) { return (rx+1)&(RX_RING_LEN-1); };
static inline u16 prev_rx(u16 rx) { return (rx-1)&(RX_RING_LEN-1); };

static inline u16 next_tx(u16 tx) { return (tx+1)&(TX_RING_LEN-1); };


static int	mc32_probe1(struct net_device *dev, int ioaddr);
static int      mc32_command(struct net_device *dev, u16 cmd, void *data, int len);
static int	mc32_open(struct net_device *dev);
static void	mc32_timeout(struct net_device *dev);
static netdev_tx_t mc32_send_packet(struct sk_buff *skb,
				    struct net_device *dev);
static irqreturn_t mc32_interrupt(int irq, void *dev_id);
static int	mc32_close(struct net_device *dev);
static struct	net_device_stats *mc32_get_stats(struct net_device *dev);
static void	mc32_set_multicast_list(struct net_device *dev);
static void	mc32_reset_multicast_list(struct net_device *dev);
static const struct ethtool_ops netdev_ethtool_ops;

static void cleanup_card(struct net_device *dev)
{
	struct mc32_local *lp = netdev_priv(dev);
	unsigned slot = lp->slot;
	mca_mark_as_unused(slot);
	mca_set_adapter_name(slot, NULL);
	free_irq(dev->irq, dev);
	release_region(dev->base_addr, MC32_IO_EXTENT);
}


struct net_device *__init mc32_probe(int unit)
{
	struct net_device *dev = alloc_etherdev(sizeof(struct mc32_local));
	static int current_mca_slot = -1;
	int i;
	int err;

	if (!dev)
		return ERR_PTR(-ENOMEM);

	if (unit >= 0)
		sprintf(dev->name, "eth%d", unit);



	for(i = 0; (mc32_adapters[i].name != NULL); i++) {
		current_mca_slot =
			mca_find_unused_adapter(mc32_adapters[i].id, 0);

		if(current_mca_slot != MCA_NOTFOUND) {
			if(!mc32_probe1(dev, current_mca_slot))
			{
				mca_set_adapter_name(current_mca_slot,
						mc32_adapters[i].name);
				mca_mark_as_used(current_mca_slot);
				err = register_netdev(dev);
				if (err) {
					cleanup_card(dev);
					free_netdev(dev);
					dev = ERR_PTR(err);
				}
				return dev;
			}

		}
	}
	free_netdev(dev);
	return ERR_PTR(-ENODEV);
}

static const struct net_device_ops netdev_ops = {
	.ndo_open		= mc32_open,
	.ndo_stop		= mc32_close,
	.ndo_start_xmit		= mc32_send_packet,
	.ndo_get_stats		= mc32_get_stats,
	.ndo_set_rx_mode	= mc32_set_multicast_list,
	.ndo_tx_timeout		= mc32_timeout,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};


static int __init mc32_probe1(struct net_device *dev, int slot)
{
	static unsigned version_printed;
	int i, err;
	u8 POS;
	u32 base;
	struct mc32_local *lp = netdev_priv(dev);
	static const u16 mca_io_bases[] = {
		0x7280,0x7290,
		0x7680,0x7690,
		0x7A80,0x7A90,
		0x7E80,0x7E90
	};
	static const u32 mca_mem_bases[] = {
		0x00C0000,
		0x00C4000,
		0x00C8000,
		0x00CC000,
		0x00D0000,
		0x00D4000,
		0x00D8000,
		0x00DC000
	};
	static const char * const failures[] = {
		"Processor instruction",
		"Processor data bus",
		"Processor data bus",
		"Processor data bus",
		"Adapter bus",
		"ROM checksum",
		"Base RAM",
		"Extended RAM",
		"82586 internal loopback",
		"82586 initialisation failure",
		"Adapter list configuration error"
	};

	

	if (mc32_debug  &&  version_printed++ == 0)
		pr_debug("%s", version);

	pr_info("%s: %s found in slot %d: ", dev->name, cardname, slot);

	POS = mca_read_stored_pos(slot, 2);

	if(!(POS&1))
	{
		pr_cont("disabled.\n");
		return -ENODEV;
	}

	
	dev->base_addr = mca_io_bases[(POS>>1)&7];
	dev->mem_start = mca_mem_bases[(POS>>4)&7];

	POS = mca_read_stored_pos(slot, 4);
	if(!(POS&1))
	{
		pr_cont("memory window disabled.\n");
		return -ENODEV;
	}

	POS = mca_read_stored_pos(slot, 5);

	i=(POS>>4)&3;
	if(i==3)
	{
		pr_cont("invalid memory window.\n");
		return -ENODEV;
	}

	i*=16384;
	i+=16384;

	dev->mem_end=dev->mem_start + i;

	dev->irq = ((POS>>2)&3)+9;

	if(!request_region(dev->base_addr, MC32_IO_EXTENT, cardname))
	{
		pr_cont("io 0x%3lX, which is busy.\n", dev->base_addr);
		return -EBUSY;
	}

	pr_cont("io 0x%3lX irq %d mem 0x%lX (%dK)\n",
		dev->base_addr, dev->irq, dev->mem_start, i/1024);


	



	
	for (i = 0; i < 6; i++)
	{
		mca_write_pos(slot, 6, i+12);
		mca_write_pos(slot, 7, 0);

		dev->dev_addr[i] = mca_read_pos(slot,3);
	}

	pr_info("%s: Address %pM ", dev->name, dev->dev_addr);

	mca_write_pos(slot, 6, 0);
	mca_write_pos(slot, 7, 0);

	POS = mca_read_stored_pos(slot, 4);

	if(POS&2)
		pr_cont(": BNC port selected.\n");
	else
		pr_cont(": AUI port selected.\n");

	POS=inb(dev->base_addr+HOST_CTRL);
	POS|=HOST_CTRL_ATTN|HOST_CTRL_RESET;
	POS&=~HOST_CTRL_INTE;
	outb(POS, dev->base_addr+HOST_CTRL);
	
	udelay(100);
	
	POS&=~(HOST_CTRL_ATTN|HOST_CTRL_RESET);
	outb(POS, dev->base_addr+HOST_CTRL);

	udelay(300);


	err = request_irq(dev->irq, mc32_interrupt, IRQF_SHARED, DRV_NAME, dev);
	if (err) {
		release_region(dev->base_addr, MC32_IO_EXTENT);
		pr_err("%s: unable to get IRQ %d.\n", DRV_NAME, dev->irq);
		goto err_exit_ports;
	}

	memset(lp, 0, sizeof(struct mc32_local));
	lp->slot = slot;

	i=0;

	base = inb(dev->base_addr);

	while(base == 0xFF)
	{
		i++;
		if(i == 1000)
		{
			pr_err("%s: failed to boot adapter.\n", dev->name);
			err = -ENODEV;
			goto err_exit_irq;
		}
		udelay(1000);
		if(inb(dev->base_addr+2)&(1<<5))
			base = inb(dev->base_addr);
	}

	if(base>0)
	{
		if(base < 0x0C)
			pr_err("%s: %s%s.\n", dev->name, failures[base-1],
				base<0x0A?" test failure":"");
		else
			pr_err("%s: unknown failure %d.\n", dev->name, base);
		err = -ENODEV;
		goto err_exit_irq;
	}

	base=0;
	for(i=0;i<4;i++)
	{
		int n=0;

		while(!(inb(dev->base_addr+2)&(1<<5)))
		{
			n++;
			udelay(50);
			if(n>100)
			{
				pr_err("%s: mailbox read fail (%d).\n", dev->name, i);
				err = -ENODEV;
				goto err_exit_irq;
			}
		}

		base|=(inb(dev->base_addr)<<(8*i));
	}

	lp->exec_box=isa_bus_to_virt(dev->mem_start+base);

	base=lp->exec_box->data[1]<<16|lp->exec_box->data[0];

	lp->base = dev->mem_start+base;

	lp->rx_box=isa_bus_to_virt(lp->base + lp->exec_box->data[2]);
	lp->tx_box=isa_bus_to_virt(lp->base + lp->exec_box->data[3]);

	lp->stats = isa_bus_to_virt(lp->base + lp->exec_box->data[5]);


	lp->tx_chain 		= lp->exec_box->data[8];   
	lp->rx_chain 		= lp->exec_box->data[10];  
	lp->tx_len 		= lp->exec_box->data[9];   
	lp->rx_len 		= lp->exec_box->data[11];  

	sema_init(&lp->cmd_mutex, 0);
	init_completion(&lp->execution_cmd);
	init_completion(&lp->xceiver_cmd);

	pr_info("%s: Firmware Rev %d. %d RX buffers, %d TX buffers. Base of 0x%08X.\n",
		dev->name, lp->exec_box->data[12], lp->rx_len, lp->tx_len, lp->base);

	dev->netdev_ops		= &netdev_ops;
	dev->watchdog_timeo	= HZ*5;	
	dev->ethtool_ops	= &netdev_ethtool_ops;

	return 0;

err_exit_irq:
	free_irq(dev->irq, dev);
err_exit_ports:
	release_region(dev->base_addr, MC32_IO_EXTENT);
	return err;
}



static inline void mc32_ready_poll(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	while(!(inb(ioaddr+HOST_STATUS)&HOST_STATUS_CRR));
}



static int mc32_command_nowait(struct net_device *dev, u16 cmd, void *data, int len)
{
	struct mc32_local *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;
	int ret = -1;

	if (down_trylock(&lp->cmd_mutex) == 0)
	{
		lp->cmd_nonblocking=1;
		lp->exec_box->mbox=0;
		lp->exec_box->mbox=cmd;
		memcpy((void *)lp->exec_box->data, data, len);
		barrier();	

		
		mc32_ready_poll(dev);
		outb(1<<6, ioaddr+HOST_CMD);

		ret = 0;

		
	}

	return ret;
}



static int mc32_command(struct net_device *dev, u16 cmd, void *data, int len)
{
	struct mc32_local *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;
	int ret = 0;

	down(&lp->cmd_mutex);


	lp->cmd_nonblocking=0;
	lp->exec_box->mbox=0;
	lp->exec_box->mbox=cmd;
	memcpy((void *)lp->exec_box->data, data, len);
	barrier();	

	mc32_ready_poll(dev);
	outb(1<<6, ioaddr+HOST_CMD);

	wait_for_completion(&lp->execution_cmd);

	if(lp->exec_box->mbox&(1<<13))
		ret = -1;

	up(&lp->cmd_mutex);


	if(lp->mc_reload_wait)
	{
		mc32_reset_multicast_list(dev);
	}

	return ret;
}



static void mc32_start_transceiver(struct net_device *dev) {

	struct mc32_local *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;

	
	if (lp->xceiver_desired_state==HALTED)
		return;

	
	mc32_ready_poll(dev);
	lp->rx_box->mbox=0;
	lp->rx_box->data[0]=lp->rx_ring[prev_rx(lp->rx_ring_tail)].p->next;
	outb(HOST_CMD_START_RX, ioaddr+HOST_CMD);

	mc32_ready_poll(dev);
	lp->tx_box->mbox=0;
	outb(HOST_CMD_RESTRT_TX, ioaddr+HOST_CMD);   

	
}



static void mc32_halt_transceiver(struct net_device *dev)
{
	struct mc32_local *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;

	mc32_ready_poll(dev);
	lp->rx_box->mbox=0;
	outb(HOST_CMD_SUSPND_RX, ioaddr+HOST_CMD);
	wait_for_completion(&lp->xceiver_cmd);

	mc32_ready_poll(dev);
	lp->tx_box->mbox=0;
	outb(HOST_CMD_SUSPND_TX, ioaddr+HOST_CMD);
	wait_for_completion(&lp->xceiver_cmd);
}



static int mc32_load_rx_ring(struct net_device *dev)
{
	struct mc32_local *lp = netdev_priv(dev);
	int i;
	u16 rx_base;
	volatile struct skb_header *p;

	rx_base=lp->rx_chain;

	for(i=0; i<RX_RING_LEN; i++) {
		lp->rx_ring[i].skb=alloc_skb(1532, GFP_KERNEL);
		if (lp->rx_ring[i].skb==NULL) {
			for (;i>=0;i--)
				kfree_skb(lp->rx_ring[i].skb);
			return -ENOBUFS;
		}
		skb_reserve(lp->rx_ring[i].skb, 18);

		p=isa_bus_to_virt(lp->base+rx_base);

		p->control=0;
		p->data=isa_virt_to_bus(lp->rx_ring[i].skb->data);
		p->status=0;
		p->length=1532;

		lp->rx_ring[i].p=p;
		rx_base=p->next;
	}

	lp->rx_ring[i-1].p->control |= CONTROL_EOL;

	lp->rx_ring_tail=0;

	return 0;
}



static void mc32_flush_rx_ring(struct net_device *dev)
{
	struct mc32_local *lp = netdev_priv(dev);
	int i;

	for(i=0; i < RX_RING_LEN; i++)
	{
		if (lp->rx_ring[i].skb) {
			dev_kfree_skb(lp->rx_ring[i].skb);
			lp->rx_ring[i].skb = NULL;
		}
		lp->rx_ring[i].p=NULL;
	}
}



static void mc32_load_tx_ring(struct net_device *dev)
{
	struct mc32_local *lp = netdev_priv(dev);
	volatile struct skb_header *p;
	int i;
	u16 tx_base;

	tx_base=lp->tx_box->data[0];

	for(i=0 ; i<TX_RING_LEN ; i++)
	{
		p=isa_bus_to_virt(lp->base+tx_base);
		lp->tx_ring[i].p=p;
		lp->tx_ring[i].skb=NULL;

		tx_base=p->next;
	}

	
	

	atomic_set(&lp->tx_count, TX_RING_LEN-1);
	atomic_set(&lp->tx_ring_head, 0);
	lp->tx_ring_tail=0;
}



static void mc32_flush_tx_ring(struct net_device *dev)
{
	struct mc32_local *lp = netdev_priv(dev);
	int i;

	for (i=0; i < TX_RING_LEN; i++)
	{
		if (lp->tx_ring[i].skb)
		{
			dev_kfree_skb(lp->tx_ring[i].skb);
			lp->tx_ring[i].skb = NULL;
		}
	}

	atomic_set(&lp->tx_count, 0);
	atomic_set(&lp->tx_ring_head, 0);
	lp->tx_ring_tail=0;
}



static int mc32_open(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	struct mc32_local *lp = netdev_priv(dev);
	u8 one=1;
	u8 regs;
	u16 descnumbuffs[2] = {TX_RING_LEN, RX_RING_LEN};


	regs=inb(ioaddr+HOST_CTRL);
	regs|=HOST_CTRL_INTE;
	outb(regs, ioaddr+HOST_CTRL);


	up(&lp->cmd_mutex);



	mc32_command(dev, 4, &one, 2);


	mc32_halt_transceiver(dev);
	mc32_flush_tx_ring(dev);


	if(mc32_command(dev, 8, descnumbuffs, 4)) {
		pr_info("%s: %s rejected our buffer configuration!\n",
	 	       dev->name, cardname);
		mc32_close(dev);
		return -ENOBUFS;
	}

	
	mc32_command(dev, 6, NULL, 0);

	lp->tx_chain 		= lp->exec_box->data[8];   
	lp->rx_chain 		= lp->exec_box->data[10];  
	lp->tx_len 		= lp->exec_box->data[9];   
	lp->rx_len 		= lp->exec_box->data[11];  

	
	mc32_command(dev, 1, dev->dev_addr, 6);

	
	mc32_set_multicast_list(dev);

	if (WORKAROUND_82586) {
		u16 zero_word=0;
		mc32_command(dev, 0x0D, &zero_word, 2);   
	}

	mc32_load_tx_ring(dev);

	if(mc32_load_rx_ring(dev))
	{
		mc32_close(dev);
		return -ENOBUFS;
	}

	lp->xceiver_desired_state = RUNNING;

	
	mc32_start_transceiver(dev);

	netif_start_queue(dev);

	return 0;
}



static void mc32_timeout(struct net_device *dev)
{
	pr_warning("%s: transmit timed out?\n", dev->name);
	
	netif_wake_queue(dev);
}



static netdev_tx_t mc32_send_packet(struct sk_buff *skb,
				    struct net_device *dev)
{
	struct mc32_local *lp = netdev_priv(dev);
	u32 head = atomic_read(&lp->tx_ring_head);

	volatile struct skb_header *p, *np;

	netif_stop_queue(dev);

	if(atomic_read(&lp->tx_count)==0) {
		return NETDEV_TX_BUSY;
	}

	if (skb_padto(skb, ETH_ZLEN)) {
		netif_wake_queue(dev);
		return NETDEV_TX_OK;
	}

	atomic_dec(&lp->tx_count);

	
	p=lp->tx_ring[head].p;

	head = next_tx(head);

	
	np=lp->tx_ring[head].p;

	
	lp->tx_ring[head].skb=skb;

	np->length      = unlikely(skb->len < ETH_ZLEN) ? ETH_ZLEN : skb->len;
	np->data	= isa_virt_to_bus(skb->data);
	np->status	= 0;
	np->control     = CONTROL_EOP | CONTROL_EOL;
	wmb();


	atomic_set(&lp->tx_ring_head, head);
	p->control     &= ~CONTROL_EOL;

	netif_wake_queue(dev);
	return NETDEV_TX_OK;
}



static void mc32_update_stats(struct net_device *dev)
{
	struct mc32_local *lp = netdev_priv(dev);
	volatile struct mc32_stats *st = lp->stats;

	u32 rx_errors=0;

	rx_errors+=dev->stats.rx_crc_errors   +=st->rx_crc_errors;
	                                           st->rx_crc_errors=0;
	rx_errors+=dev->stats.rx_fifo_errors  +=st->rx_overrun_errors;
	                                           st->rx_overrun_errors=0;
	rx_errors+=dev->stats.rx_frame_errors +=st->rx_alignment_errors;
 	                                           st->rx_alignment_errors=0;
	rx_errors+=dev->stats.rx_length_errors+=st->rx_tooshort_errors;
	                                           st->rx_tooshort_errors=0;
	rx_errors+=dev->stats.rx_missed_errors+=st->rx_outofresource_errors;
	                                           st->rx_outofresource_errors=0;
        dev->stats.rx_errors=rx_errors;

	
	dev->stats.collisions+=st->dataC[10];
	st->dataC[10]=0;

	
	dev->stats.collisions+=st->dataC[11];
	st->dataC[11]=0;
}



static void mc32_rx_ring(struct net_device *dev)
{
	struct mc32_local *lp = netdev_priv(dev);
	volatile struct skb_header *p;
	u16 rx_ring_tail;
	u16 rx_old_tail;
	int x=0;

	rx_old_tail = rx_ring_tail = lp->rx_ring_tail;

	do
	{
		p=lp->rx_ring[rx_ring_tail].p;

		if(!(p->status & (1<<7))) { 
			break;
		}
		if(p->status & (1<<6)) 
		{

			u16 length=p->length;
			struct sk_buff *skb;
			struct sk_buff *newskb;

			

			if ((length > RX_COPYBREAK) &&
			    ((newskb = netdev_alloc_skb(dev, 1532)) != NULL))
			{
				skb=lp->rx_ring[rx_ring_tail].skb;
				skb_put(skb, length);

				skb_reserve(newskb,18);
				lp->rx_ring[rx_ring_tail].skb=newskb;
				p->data=isa_virt_to_bus(newskb->data);
			}
			else
			{
				skb = netdev_alloc_skb(dev, length + 2);

				if(skb==NULL) {
					dev->stats.rx_dropped++;
					goto dropped;
				}

				skb_reserve(skb,2);
				memcpy(skb_put(skb, length),
				       lp->rx_ring[rx_ring_tail].skb->data, length);
			}

			skb->protocol=eth_type_trans(skb,dev);
 			dev->stats.rx_packets++;
 			dev->stats.rx_bytes += length;
			netif_rx(skb);
		}

	dropped:
		p->length = 1532;
		p->status = 0;

		rx_ring_tail=next_rx(rx_ring_tail);
	}
        while(x++<48);

	
	

	if (rx_ring_tail != rx_old_tail)
	{
		lp->rx_ring[prev_rx(rx_ring_tail)].p->control |=  CONTROL_EOL;
		lp->rx_ring[prev_rx(rx_old_tail)].p->control  &= ~CONTROL_EOL;

		lp->rx_ring_tail=rx_ring_tail;
	}
}



static void mc32_tx_ring(struct net_device *dev)
{
	struct mc32_local *lp = netdev_priv(dev);
	volatile struct skb_header *np;


	while (lp->tx_ring_tail != atomic_read(&lp->tx_ring_head))
	{
		u16 t;

		t=next_tx(lp->tx_ring_tail);
		np=lp->tx_ring[t].p;

		if(!(np->status & (1<<7)))
		{
			
			break;
		}
		dev->stats.tx_packets++;
		if(!(np->status & (1<<6))) 
		{
			dev->stats.tx_errors++;

			switch(np->status&0x0F)
			{
				case 1:
					dev->stats.tx_aborted_errors++;
					break; 
				case 2:
					dev->stats.tx_fifo_errors++;
					break;
				case 3:
					dev->stats.tx_carrier_errors++;
					break;
				case 4:
					dev->stats.tx_window_errors++;
					break;  
				case 5:
					dev->stats.tx_aborted_errors++;
					break; 
			}
		}
		dev->stats.tx_bytes+=lp->tx_ring[t].skb->len;
		dev_kfree_skb_irq(lp->tx_ring[t].skb);
		lp->tx_ring[t].skb=NULL;
		atomic_inc(&lp->tx_count);
		netif_wake_queue(dev);

		lp->tx_ring_tail=t;
	}

}



static irqreturn_t mc32_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct mc32_local *lp;
	int ioaddr, status, boguscount = 0;
	int rx_event = 0;
	int tx_event = 0;

	ioaddr = dev->base_addr;
	lp = netdev_priv(dev);

	

	while((inb(ioaddr+HOST_STATUS)&HOST_STATUS_CWR) && boguscount++<2000)
	{
		status=inb(ioaddr+HOST_CMD);

		pr_debug("Status TX%d RX%d EX%d OV%d BC%d\n",
			(status&7), (status>>3)&7, (status>>6)&1,
			(status>>7)&1, boguscount);

		switch(status&7)
		{
			case 0:
				break;
			case 6: 
			case 2:	
				tx_event = 1;
				break;
			case 3: 
			case 4: 
				complete(&lp->xceiver_cmd);
				break;
			default:
				pr_notice("%s: strange tx ack %d\n", dev->name, status&7);
		}
		status>>=3;
		switch(status&7)
		{
			case 0:
				break;
			case 2:	
				rx_event=1;
				break;
			case 3: 
			case 4: 
				complete(&lp->xceiver_cmd);
				break;
			case 6:
				
				
				dev->stats.rx_dropped++;
				mc32_rx_ring(dev);
				mc32_start_transceiver(dev);
				break;
			default:
				pr_notice("%s: strange rx ack %d\n",
					dev->name, status&7);
		}
		status>>=3;
		if(status&1)
		{

			if (lp->cmd_nonblocking) {
				up(&lp->cmd_mutex);
				if (lp->mc_reload_wait)
					mc32_reset_multicast_list(dev);
			}
			else complete(&lp->execution_cmd);
		}
		if(status&2)
		{

			mc32_update_stats(dev);
		}
	}



	if(tx_event)
		mc32_tx_ring(dev);

	if(rx_event)
		mc32_rx_ring(dev);

	return IRQ_HANDLED;
}



static int mc32_close(struct net_device *dev)
{
	struct mc32_local *lp = netdev_priv(dev);
	int ioaddr = dev->base_addr;

	u8 regs;
	u16 one=1;

	lp->xceiver_desired_state = HALTED;
	netif_stop_queue(dev);


	mc32_command(dev, 4, &one, 2);

	

	mc32_halt_transceiver(dev);

	

	down(&lp->cmd_mutex);

	

	regs=inb(ioaddr+HOST_CTRL);
	regs&=~HOST_CTRL_INTE;
	outb(regs, ioaddr+HOST_CTRL);

	mc32_flush_rx_ring(dev);
	mc32_flush_tx_ring(dev);

	mc32_update_stats(dev);

	return 0;
}



static struct net_device_stats *mc32_get_stats(struct net_device *dev)
{
	mc32_update_stats(dev);
	return &dev->stats;
}



static void do_mc32_set_multicast_list(struct net_device *dev, int retry)
{
	struct mc32_local *lp = netdev_priv(dev);
	u16 filt = (1<<2); 

	if ((dev->flags&IFF_PROMISC) ||
	    (dev->flags&IFF_ALLMULTI) ||
	    netdev_mc_count(dev) > 10)
		
		filt |= 1;
	else if (!netdev_mc_empty(dev))
	{
		unsigned char block[62];
		unsigned char *bp;
		struct netdev_hw_addr *ha;

		if(retry==0)
			lp->mc_list_valid = 0;
		if(!lp->mc_list_valid)
		{
			block[1]=0;
			block[0]=netdev_mc_count(dev);
			bp=block+2;

			netdev_for_each_mc_addr(ha, dev) {
				memcpy(bp, ha->addr, 6);
				bp+=6;
			}
			if(mc32_command_nowait(dev, 2, block,
					       2+6*netdev_mc_count(dev))==-1)
			{
				lp->mc_reload_wait = 1;
				return;
			}
			lp->mc_list_valid=1;
		}
	}

	if(mc32_command_nowait(dev, 0, &filt, 2)==-1)
	{
		lp->mc_reload_wait = 1;
	}
	else {
		lp->mc_reload_wait = 0;
	}
}



static void mc32_set_multicast_list(struct net_device *dev)
{
	do_mc32_set_multicast_list(dev,0);
}



static void mc32_reset_multicast_list(struct net_device *dev)
{
	do_mc32_set_multicast_list(dev,1);
}

static void netdev_get_drvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	sprintf(info->bus_info, "MCA 0x%lx", dev->base_addr);
}

static u32 netdev_get_msglevel(struct net_device *dev)
{
	return mc32_debug;
}

static void netdev_set_msglevel(struct net_device *dev, u32 level)
{
	mc32_debug = level;
}

static const struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= netdev_get_drvinfo,
	.get_msglevel		= netdev_get_msglevel,
	.set_msglevel		= netdev_set_msglevel,
};

#ifdef MODULE

static struct net_device *this_device;


int __init init_module(void)
{
	this_device = mc32_probe(-1);
	if (IS_ERR(this_device))
		return PTR_ERR(this_device);
	return 0;
}


void __exit cleanup_module(void)
{
	unregister_netdev(this_device);
	cleanup_card(this_device);
	free_netdev(this_device);
}

#endif 
