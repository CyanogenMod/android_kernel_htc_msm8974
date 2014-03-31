/*
    A Davicom DM9102/DM9102A/DM9102A+DM9801/DM9102A+DM9802 NIC fast
    ethernet driver for Linux.
    Copyright (C) 1997  Sten Wang

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    DAVICOM Web-Site: www.davicom.com.tw

    Author: Sten Wang, 886-3-5798797-8517, E-mail: sten_wang@davicom.com.tw
    Maintainer: Tobias Ringstrom <tori@unhappy.mine.nu>

    (C)Copyright 1997-1998 DAVICOM Semiconductor,Inc. All Rights Reserved.

    Marcelo Tosatti <marcelo@conectiva.com.br> :
    Made it compile in 2.3 (device to net_device)

    Alan Cox <alan@lxorguk.ukuu.org.uk> :
    Cleaned up for kernel merge.
    Removed the back compatibility support
    Reformatted, fixing spelling etc as I went
    Removed IRQ 0-15 assumption

    Jeff Garzik <jgarzik@pobox.com> :
    Updated to use new PCI driver API.
    Resource usage cleanups.
    Report driver version to user.

    Tobias Ringstrom <tori@unhappy.mine.nu> :
    Cleaned up and added SMP safety.  Thanks go to Jeff Garzik,
    Andrew Morton and Frank Davis for the SMP safety fixes.

    Vojtech Pavlik <vojtech@suse.cz> :
    Cleaned up pointer arithmetics.
    Fixed a lot of 64bit issues.
    Cleaned up printk()s a bit.
    Fixed some obvious big endian problems.

    Tobias Ringstrom <tori@unhappy.mine.nu> :
    Use time_after for jiffies calculation.  Added ethtool
    support.  Updated PCI resource allocation.  Do not
    forget to unmap PCI mapped skbs.

    Alan Cox <alan@lxorguk.ukuu.org.uk>
    Added new PCI identifiers provided by Clear Zhang at ALi
    for their 1563 ethernet device.

    TODO

    Check on 64 bit boxes.
    Check and fix on big endian boxes.

    Test and make sure PCI latency is now correct for all cases.
*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define DRV_NAME	"dmfe"
#define DRV_VERSION	"1.36.4"
#define DRV_RELDATE	"2002-01-17"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>
#include <linux/bitops.h>

#include <asm/processor.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/uaccess.h>
#include <asm/irq.h>

#ifdef CONFIG_TULIP_DM910X
#include <linux/of.h>
#endif


#define PCI_DM9132_ID   0x91321282      
#define PCI_DM9102_ID   0x91021282      
#define PCI_DM9100_ID   0x91001282      
#define PCI_DM9009_ID   0x90091282      

#define DM9102_IO_SIZE  0x80
#define DM9102A_IO_SIZE 0x100
#define TX_MAX_SEND_CNT 0x1             
#define TX_DESC_CNT     0x10            
#define RX_DESC_CNT     0x20            
#define TX_FREE_DESC_CNT (TX_DESC_CNT - 2)	
#define TX_WAKE_DESC_CNT (TX_DESC_CNT - 3)	
#define DESC_ALL_CNT    (TX_DESC_CNT + RX_DESC_CNT)
#define TX_BUF_ALLOC    0x600
#define RX_ALLOC_SIZE   0x620
#define DM910X_RESET    1
#define CR0_DEFAULT     0x00E00000      
#define CR6_DEFAULT     0x00080000      
#define CR7_DEFAULT     0x180c1
#define CR15_DEFAULT    0x06            
#define TDES0_ERR_MASK  0x4302          
#define MAX_PACKET_SIZE 1514
#define DMFE_MAX_MULTICAST 14
#define RX_COPY_SIZE	100
#define MAX_CHECK_PACKET 0x8000
#define DM9801_NOISE_FLOOR 8
#define DM9802_NOISE_FLOOR 5

#define DMFE_WOL_LINKCHANGE	0x20000000
#define DMFE_WOL_SAMPLEPACKET	0x10000000
#define DMFE_WOL_MAGICPACKET	0x08000000


#define DMFE_10MHF      0
#define DMFE_100MHF     1
#define DMFE_10MFD      4
#define DMFE_100MFD     5
#define DMFE_AUTO       8
#define DMFE_1M_HPNA    0x10

#define DMFE_TXTH_72	0x400000	
#define DMFE_TXTH_96	0x404000	
#define DMFE_TXTH_128	0x0000		
#define DMFE_TXTH_256	0x4000		
#define DMFE_TXTH_512	0x8000		
#define DMFE_TXTH_1K	0xC000		

#define DMFE_TIMER_WUT  (jiffies + HZ * 1)
#define DMFE_TX_TIMEOUT ((3*HZ)/2)	
#define DMFE_TX_KICK 	(HZ/2)	

#define DMFE_DBUG(dbug_now, msg, value)			\
	do {						\
		if (dmfe_debug || (dbug_now))		\
			pr_err("%s %lx\n",		\
			       (msg), (long) (value));	\
	} while (0)

#define SHOW_MEDIA_TYPE(mode)				\
	pr_info("Change Speed to %sMhz %s duplex\n" ,	\
		(mode & 1) ? "100":"10",		\
		(mode & 4) ? "full":"half");


#define CR9_SROM_READ   0x4800
#define CR9_SRCS        0x1
#define CR9_SRCLK       0x2
#define CR9_CRDOUT      0x8
#define SROM_DATA_0     0x0
#define SROM_DATA_1     0x4
#define PHY_DATA_1      0x20000
#define PHY_DATA_0      0x00000
#define MDCLKH          0x10000

#define PHY_POWER_DOWN	0x800

#define SROM_V41_CODE   0x14

#define SROM_CLK_WRITE(data, ioaddr) \
	outl(data|CR9_SROM_READ|CR9_SRCS,ioaddr); \
	udelay(5); \
	outl(data|CR9_SROM_READ|CR9_SRCS|CR9_SRCLK,ioaddr); \
	udelay(5); \
	outl(data|CR9_SROM_READ|CR9_SRCS,ioaddr); \
	udelay(5);

#define __CHK_IO_SIZE(pci_id, dev_rev) \
 (( ((pci_id)==PCI_DM9132_ID) || ((dev_rev) >= 0x30) ) ? \
	DM9102A_IO_SIZE: DM9102_IO_SIZE)

#define CHK_IO_SIZE(pci_dev) \
	(__CHK_IO_SIZE(((pci_dev)->device << 16) | (pci_dev)->vendor, \
	(pci_dev)->revision))

#define DEVICE net_device

struct tx_desc {
        __le32 tdes0, tdes1, tdes2, tdes3; 
        char *tx_buf_ptr;               
        struct tx_desc *next_tx_desc;
} __attribute__(( aligned(32) ));

struct rx_desc {
	__le32 rdes0, rdes1, rdes2, rdes3; 
	struct sk_buff *rx_skb_ptr;	
	struct rx_desc *next_rx_desc;
} __attribute__(( aligned(32) ));

struct dmfe_board_info {
	u32 chip_id;			
	u8 chip_revision;		
	struct DEVICE *next_dev;	
	struct pci_dev *pdev;		
	spinlock_t lock;

	long ioaddr;			
	u32 cr0_data;
	u32 cr5_data;
	u32 cr6_data;
	u32 cr7_data;
	u32 cr15_data;

	
	dma_addr_t buf_pool_dma_ptr;	
	dma_addr_t buf_pool_dma_start;	
	dma_addr_t desc_pool_dma_ptr;	
	dma_addr_t first_tx_desc_dma;
	dma_addr_t first_rx_desc_dma;

	
	unsigned char *buf_pool_ptr;	
	unsigned char *buf_pool_start;	
	unsigned char *desc_pool_ptr;	
	struct tx_desc *first_tx_desc;
	struct tx_desc *tx_insert_ptr;
	struct tx_desc *tx_remove_ptr;
	struct rx_desc *first_rx_desc;
	struct rx_desc *rx_insert_ptr;
	struct rx_desc *rx_ready_ptr;	
	unsigned long tx_packet_cnt;	
	unsigned long tx_queue_cnt;	
	unsigned long rx_avail_cnt;	
	unsigned long interval_rx_cnt;	

	u16 HPNA_command;		
	u16 HPNA_timer;			
	u16 dbug_cnt;
	u16 NIC_capability;		
	u16 PHY_reg4;			

	u8 HPNA_present;		
	u8 chip_type;			
	u8 media_mode;			
	u8 op_mode;			
	u8 phy_addr;
	u8 wait_reset;			
	u8 dm910x_chk_mode;		
	u8 first_in_callback;		
	u8 wol_mode;			
	struct timer_list timer;

	
	unsigned long tx_fifo_underrun;
	unsigned long tx_loss_carrier;
	unsigned long tx_no_carrier;
	unsigned long tx_late_collision;
	unsigned long tx_excessive_collision;
	unsigned long tx_jabber_timeout;
	unsigned long reset_count;
	unsigned long reset_cr8;
	unsigned long reset_fatal;
	unsigned long reset_TXtimeout;

	
	unsigned char srom[128];
};

enum dmfe_offsets {
	DCR0 = 0x00, DCR1 = 0x08, DCR2 = 0x10, DCR3 = 0x18, DCR4 = 0x20,
	DCR5 = 0x28, DCR6 = 0x30, DCR7 = 0x38, DCR8 = 0x40, DCR9 = 0x48,
	DCR10 = 0x50, DCR11 = 0x58, DCR12 = 0x60, DCR13 = 0x68, DCR14 = 0x70,
	DCR15 = 0x78
};

enum dmfe_CR6_bits {
	CR6_RXSC = 0x2, CR6_PBF = 0x8, CR6_PM = 0x40, CR6_PAM = 0x80,
	CR6_FDM = 0x200, CR6_TXSC = 0x2000, CR6_STI = 0x100000,
	CR6_SFT = 0x200000, CR6_RXA = 0x40000000, CR6_NO_PURGE = 0x20000000
};

static int __devinitdata printed_version;
static const char version[] __devinitconst =
	"Davicom DM9xxx net driver, version " DRV_VERSION " (" DRV_RELDATE ")";

static int dmfe_debug;
static unsigned char dmfe_media_mode = DMFE_AUTO;
static u32 dmfe_cr6_user_set;

static int debug;
static u32 cr6set;
static unsigned char mode = 8;
static u8 chkmode = 1;
static u8 HPNA_mode;		
static u8 HPNA_rx_cmd;		
static u8 HPNA_tx_cmd;		
static u8 HPNA_NoiseFloor;	
static u8 SF_mode;		


static int dmfe_open(struct DEVICE *);
static netdev_tx_t dmfe_start_xmit(struct sk_buff *, struct DEVICE *);
static int dmfe_stop(struct DEVICE *);
static void dmfe_set_filter_mode(struct DEVICE *);
static const struct ethtool_ops netdev_ethtool_ops;
static u16 read_srom_word(long ,int);
static irqreturn_t dmfe_interrupt(int , void *);
#ifdef CONFIG_NET_POLL_CONTROLLER
static void poll_dmfe (struct net_device *dev);
#endif
static void dmfe_descriptor_init(struct net_device *, unsigned long);
static void allocate_rx_buffer(struct net_device *);
static void update_cr6(u32, unsigned long);
static void send_filter_frame(struct DEVICE *);
static void dm9132_id_table(struct DEVICE *);
static u16 phy_read(unsigned long, u8, u8, u32);
static void phy_write(unsigned long, u8, u8, u16, u32);
static void phy_write_1bit(unsigned long, u32);
static u16 phy_read_1bit(unsigned long);
static u8 dmfe_sense_speed(struct dmfe_board_info *);
static void dmfe_process_mode(struct dmfe_board_info *);
static void dmfe_timer(unsigned long);
static inline u32 cal_CRC(unsigned char *, unsigned int, u8);
static void dmfe_rx_packet(struct DEVICE *, struct dmfe_board_info *);
static void dmfe_free_tx_pkt(struct DEVICE *, struct dmfe_board_info *);
static void dmfe_reuse_skb(struct dmfe_board_info *, struct sk_buff *);
static void dmfe_dynamic_reset(struct DEVICE *);
static void dmfe_free_rxbuffer(struct dmfe_board_info *);
static void dmfe_init_dm910x(struct DEVICE *);
static void dmfe_parse_srom(struct dmfe_board_info *);
static void dmfe_program_DM9801(struct dmfe_board_info *, int);
static void dmfe_program_DM9802(struct dmfe_board_info *);
static void dmfe_HPNA_remote_cmd_chk(struct dmfe_board_info * );
static void dmfe_set_phyxcer(struct dmfe_board_info *);


static const struct net_device_ops netdev_ops = {
	.ndo_open 		= dmfe_open,
	.ndo_stop		= dmfe_stop,
	.ndo_start_xmit		= dmfe_start_xmit,
	.ndo_set_rx_mode	= dmfe_set_filter_mode,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= poll_dmfe,
#endif
};


static int __devinit dmfe_init_one (struct pci_dev *pdev,
				    const struct pci_device_id *ent)
{
	struct dmfe_board_info *db;	
	struct net_device *dev;
	u32 pci_pmr;
	int i, err;

	DMFE_DBUG(0, "dmfe_init_one()", 0);

	if (!printed_version++)
		pr_info("%s\n", version);

#ifdef CONFIG_TULIP_DM910X
	if ((ent->driver_data == PCI_DM9100_ID && pdev->revision >= 0x30) ||
	    ent->driver_data == PCI_DM9102_ID) {
		struct device_node *dp = pci_device_to_OF_node(pdev);

		if (dp && of_get_property(dp, "local-mac-address", NULL)) {
			pr_info("skipping on-board DM910x (use tulip)\n");
			return -ENODEV;
		}
	}
#endif

	
	dev = alloc_etherdev(sizeof(*db));
	if (dev == NULL)
		return -ENOMEM;
	SET_NETDEV_DEV(dev, &pdev->dev);

	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
		pr_warn("32-bit PCI DMA not available\n");
		err = -ENODEV;
		goto err_out_free;
	}

	
	err = pci_enable_device(pdev);
	if (err)
		goto err_out_free;

	if (!pci_resource_start(pdev, 0)) {
		pr_err("I/O base is zero\n");
		err = -ENODEV;
		goto err_out_disable;
	}

	if (pci_resource_len(pdev, 0) < (CHK_IO_SIZE(pdev)) ) {
		pr_err("Allocated I/O size too small\n");
		err = -ENODEV;
		goto err_out_disable;
	}

#if 0	

	

	pci_write_config_byte(pdev, PCI_LATENCY_TIMER, 0x80);
#endif

	if (pci_request_regions(pdev, DRV_NAME)) {
		pr_err("Failed to request PCI regions\n");
		err = -ENODEV;
		goto err_out_disable;
	}

	
	db = netdev_priv(dev);

	
	db->desc_pool_ptr = pci_alloc_consistent(pdev, sizeof(struct tx_desc) *
			DESC_ALL_CNT + 0x20, &db->desc_pool_dma_ptr);
	if (!db->desc_pool_ptr)
		goto err_out_res;

	db->buf_pool_ptr = pci_alloc_consistent(pdev, TX_BUF_ALLOC *
			TX_DESC_CNT + 4, &db->buf_pool_dma_ptr);
	if (!db->buf_pool_ptr)
		goto err_out_free_desc;

	db->first_tx_desc = (struct tx_desc *) db->desc_pool_ptr;
	db->first_tx_desc_dma = db->desc_pool_dma_ptr;
	db->buf_pool_start = db->buf_pool_ptr;
	db->buf_pool_dma_start = db->buf_pool_dma_ptr;

	db->chip_id = ent->driver_data;
	db->ioaddr = pci_resource_start(pdev, 0);
	db->chip_revision = pdev->revision;
	db->wol_mode = 0;

	db->pdev = pdev;

	dev->base_addr = db->ioaddr;
	dev->irq = pdev->irq;
	pci_set_drvdata(pdev, dev);
	dev->netdev_ops = &netdev_ops;
	dev->ethtool_ops = &netdev_ethtool_ops;
	netif_carrier_off(dev);
	spin_lock_init(&db->lock);

	pci_read_config_dword(pdev, 0x50, &pci_pmr);
	pci_pmr &= 0x70000;
	if ( (pci_pmr == 0x10000) && (db->chip_revision == 0x31) )
		db->chip_type = 1;	
	else
		db->chip_type = 0;

	
	for (i = 0; i < 64; i++)
		((__le16 *) db->srom)[i] =
			cpu_to_le16(read_srom_word(db->ioaddr, i));

	
	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = db->srom[20 + i];

	err = register_netdev (dev);
	if (err)
		goto err_out_free_buf;

	dev_info(&dev->dev, "Davicom DM%04lx at pci%s, %pM, irq %d\n",
		 ent->driver_data >> 16,
		 pci_name(pdev), dev->dev_addr, dev->irq);

	pci_set_master(pdev);

	return 0;

err_out_free_buf:
	pci_free_consistent(pdev, TX_BUF_ALLOC * TX_DESC_CNT + 4,
			    db->buf_pool_ptr, db->buf_pool_dma_ptr);
err_out_free_desc:
	pci_free_consistent(pdev, sizeof(struct tx_desc) * DESC_ALL_CNT + 0x20,
			    db->desc_pool_ptr, db->desc_pool_dma_ptr);
err_out_res:
	pci_release_regions(pdev);
err_out_disable:
	pci_disable_device(pdev);
err_out_free:
	pci_set_drvdata(pdev, NULL);
	free_netdev(dev);

	return err;
}


static void __devexit dmfe_remove_one (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct dmfe_board_info *db = netdev_priv(dev);

	DMFE_DBUG(0, "dmfe_remove_one()", 0);

 	if (dev) {

		unregister_netdev(dev);

		pci_free_consistent(db->pdev, sizeof(struct tx_desc) *
					DESC_ALL_CNT + 0x20, db->desc_pool_ptr,
 					db->desc_pool_dma_ptr);
		pci_free_consistent(db->pdev, TX_BUF_ALLOC * TX_DESC_CNT + 4,
					db->buf_pool_ptr, db->buf_pool_dma_ptr);
		pci_release_regions(pdev);
		free_netdev(dev);	

		pci_set_drvdata(pdev, NULL);
	}

	DMFE_DBUG(0, "dmfe_remove_one() exit", 0);
}



static int dmfe_open(struct DEVICE *dev)
{
	int ret;
	struct dmfe_board_info *db = netdev_priv(dev);

	DMFE_DBUG(0, "dmfe_open", 0);

	ret = request_irq(dev->irq, dmfe_interrupt,
			  IRQF_SHARED, dev->name, dev);
	if (ret)
		return ret;

	
	db->cr6_data = CR6_DEFAULT | dmfe_cr6_user_set;
	db->tx_packet_cnt = 0;
	db->tx_queue_cnt = 0;
	db->rx_avail_cnt = 0;
	db->wait_reset = 0;

	db->first_in_callback = 0;
	db->NIC_capability = 0xf;	
	db->PHY_reg4 = 0x1e0;

	
	if ( !chkmode || (db->chip_id == PCI_DM9132_ID) ||
		(db->chip_revision >= 0x30) ) {
    		db->cr6_data |= DMFE_TXTH_256;
		db->cr0_data = CR0_DEFAULT;
		db->dm910x_chk_mode=4;		
 	} else {
		db->cr6_data |= CR6_SFT;	
		db->cr0_data = 0;
		db->dm910x_chk_mode = 1;	
	}

	
	dmfe_init_dm910x(dev);

	
	netif_wake_queue(dev);

	
	init_timer(&db->timer);
	db->timer.expires = DMFE_TIMER_WUT + HZ * 2;
	db->timer.data = (unsigned long)dev;
	db->timer.function = dmfe_timer;
	add_timer(&db->timer);

	return 0;
}



static void dmfe_init_dm910x(struct DEVICE *dev)
{
	struct dmfe_board_info *db = netdev_priv(dev);
	unsigned long ioaddr = db->ioaddr;

	DMFE_DBUG(0, "dmfe_init_dm910x()", 0);

	
	outl(DM910X_RESET, ioaddr + DCR0);	
	udelay(100);
	outl(db->cr0_data, ioaddr + DCR0);
	udelay(5);

	
	db->phy_addr = 1;

	
	dmfe_parse_srom(db);
	db->media_mode = dmfe_media_mode;

	
	outl(0x180, ioaddr + DCR12);		
	if (db->chip_id == PCI_DM9009_ID) {
		outl(0x80, ioaddr + DCR12);	
		mdelay(300);			
	}
	outl(0x0, ioaddr + DCR12);	

	
	if ( !(db->media_mode & 0x10) )	
		dmfe_set_phyxcer(db);

	
	if ( !(db->media_mode & DMFE_AUTO) )
		db->op_mode = db->media_mode; 	

	
	dmfe_descriptor_init(dev, ioaddr);

	
	update_cr6(db->cr6_data, ioaddr);

	
	if (db->chip_id == PCI_DM9132_ID)
		dm9132_id_table(dev);	
	else
		send_filter_frame(dev);	

	
	db->cr7_data = CR7_DEFAULT;
	outl(db->cr7_data, ioaddr + DCR7);

	
	outl(db->cr15_data, ioaddr + DCR15);

	
	db->cr6_data |= CR6_RXSC | CR6_TXSC | 0x40000;
	update_cr6(db->cr6_data, ioaddr);
}



static netdev_tx_t dmfe_start_xmit(struct sk_buff *skb,
					 struct DEVICE *dev)
{
	struct dmfe_board_info *db = netdev_priv(dev);
	struct tx_desc *txptr;
	unsigned long flags;

	DMFE_DBUG(0, "dmfe_start_xmit", 0);

	
	if (skb->len > MAX_PACKET_SIZE) {
		pr_err("big packet = %d\n", (u16)skb->len);
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	
	netif_stop_queue(dev);

	spin_lock_irqsave(&db->lock, flags);

	
	if (db->tx_queue_cnt >= TX_FREE_DESC_CNT) {
		spin_unlock_irqrestore(&db->lock, flags);
		pr_err("No Tx resource %ld\n", db->tx_queue_cnt);
		return NETDEV_TX_BUSY;
	}

	
	outl(0, dev->base_addr + DCR7);

	
	txptr = db->tx_insert_ptr;
	skb_copy_from_linear_data(skb, txptr->tx_buf_ptr, skb->len);
	txptr->tdes1 = cpu_to_le32(0xe1000000 | skb->len);

	
	db->tx_insert_ptr = txptr->next_tx_desc;

	
	if ( (!db->tx_queue_cnt) && (db->tx_packet_cnt < TX_MAX_SEND_CNT) ) {
		txptr->tdes0 = cpu_to_le32(0x80000000);	
		db->tx_packet_cnt++;			
		outl(0x1, dev->base_addr + DCR1);	
		dev->trans_start = jiffies;		
	} else {
		db->tx_queue_cnt++;			
		outl(0x1, dev->base_addr + DCR1);	
	}

	
	if ( db->tx_queue_cnt < TX_FREE_DESC_CNT )
		netif_wake_queue(dev);

	
	spin_unlock_irqrestore(&db->lock, flags);
	outl(db->cr7_data, dev->base_addr + DCR7);

	
	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}



static int dmfe_stop(struct DEVICE *dev)
{
	struct dmfe_board_info *db = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;

	DMFE_DBUG(0, "dmfe_stop", 0);

	
	netif_stop_queue(dev);

	
	del_timer_sync(&db->timer);

	
	outl(DM910X_RESET, ioaddr + DCR0);
	udelay(5);
	phy_write(db->ioaddr, db->phy_addr, 0, 0x8000, db->chip_id);

	
	free_irq(dev->irq, dev);

	
	dmfe_free_rxbuffer(db);

#if 0
	
	printk("FU:%lx EC:%lx LC:%lx NC:%lx LOC:%lx TXJT:%lx RESET:%lx RCR8:%lx FAL:%lx TT:%lx\n",
	       db->tx_fifo_underrun, db->tx_excessive_collision,
	       db->tx_late_collision, db->tx_no_carrier, db->tx_loss_carrier,
	       db->tx_jabber_timeout, db->reset_count, db->reset_cr8,
	       db->reset_fatal, db->reset_TXtimeout);
#endif

	return 0;
}



static irqreturn_t dmfe_interrupt(int irq, void *dev_id)
{
	struct DEVICE *dev = dev_id;
	struct dmfe_board_info *db = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	unsigned long flags;

	DMFE_DBUG(0, "dmfe_interrupt()", 0);

	spin_lock_irqsave(&db->lock, flags);

	
	db->cr5_data = inl(ioaddr + DCR5);
	outl(db->cr5_data, ioaddr + DCR5);
	if ( !(db->cr5_data & 0xc1) ) {
		spin_unlock_irqrestore(&db->lock, flags);
		return IRQ_HANDLED;
	}

	
	outl(0, ioaddr + DCR7);

	
	if (db->cr5_data & 0x2000) {
		
		DMFE_DBUG(1, "System bus error happen. CR5=", db->cr5_data);
		db->reset_fatal++;
		db->wait_reset = 1;	
		spin_unlock_irqrestore(&db->lock, flags);
		return IRQ_HANDLED;
	}

	 
	if ( (db->cr5_data & 0x40) && db->rx_avail_cnt )
		dmfe_rx_packet(dev, db);

	
	if (db->rx_avail_cnt<RX_DESC_CNT)
		allocate_rx_buffer(dev);

	
	if ( db->cr5_data & 0x01)
		dmfe_free_tx_pkt(dev, db);

	
	if (db->dm910x_chk_mode & 0x2) {
		db->dm910x_chk_mode = 0x4;
		db->cr6_data |= 0x100;
		update_cr6(db->cr6_data, db->ioaddr);
	}

	
	outl(db->cr7_data, ioaddr + DCR7);

	spin_unlock_irqrestore(&db->lock, flags);
	return IRQ_HANDLED;
}


#ifdef CONFIG_NET_POLL_CONTROLLER

static void poll_dmfe (struct net_device *dev)
{
	disable_irq(dev->irq);
	dmfe_interrupt (dev->irq, dev);
	enable_irq(dev->irq);
}
#endif


static void dmfe_free_tx_pkt(struct DEVICE *dev, struct dmfe_board_info * db)
{
	struct tx_desc *txptr;
	unsigned long ioaddr = dev->base_addr;
	u32 tdes0;

	txptr = db->tx_remove_ptr;
	while(db->tx_packet_cnt) {
		tdes0 = le32_to_cpu(txptr->tdes0);
		if (tdes0 & 0x80000000)
			break;

		
		db->tx_packet_cnt--;
		dev->stats.tx_packets++;

		
		if ( tdes0 != 0x7fffffff ) {
			dev->stats.collisions += (tdes0 >> 3) & 0xf;
			dev->stats.tx_bytes += le32_to_cpu(txptr->tdes1) & 0x7ff;
			if (tdes0 & TDES0_ERR_MASK) {
				dev->stats.tx_errors++;

				if (tdes0 & 0x0002) {	
					db->tx_fifo_underrun++;
					if ( !(db->cr6_data & CR6_SFT) ) {
						db->cr6_data = db->cr6_data | CR6_SFT;
						update_cr6(db->cr6_data, db->ioaddr);
					}
				}
				if (tdes0 & 0x0100)
					db->tx_excessive_collision++;
				if (tdes0 & 0x0200)
					db->tx_late_collision++;
				if (tdes0 & 0x0400)
					db->tx_no_carrier++;
				if (tdes0 & 0x0800)
					db->tx_loss_carrier++;
				if (tdes0 & 0x4000)
					db->tx_jabber_timeout++;
			}
		}

    		txptr = txptr->next_tx_desc;
	}

	
	db->tx_remove_ptr = txptr;

	
	if ( (db->tx_packet_cnt < TX_MAX_SEND_CNT) && db->tx_queue_cnt ) {
		txptr->tdes0 = cpu_to_le32(0x80000000);	
		db->tx_packet_cnt++;			
		db->tx_queue_cnt--;
		outl(0x1, ioaddr + DCR1);		
		dev->trans_start = jiffies;		
	}

	
	if ( db->tx_queue_cnt < TX_WAKE_DESC_CNT )
		netif_wake_queue(dev);	
}



static inline u32 cal_CRC(unsigned char * Data, unsigned int Len, u8 flag)
{
	u32 crc = crc32(~0, Data, Len);
	if (flag) crc = ~crc;
	return crc;
}



static void dmfe_rx_packet(struct DEVICE *dev, struct dmfe_board_info * db)
{
	struct rx_desc *rxptr;
	struct sk_buff *skb, *newskb;
	int rxlen;
	u32 rdes0;

	rxptr = db->rx_ready_ptr;

	while(db->rx_avail_cnt) {
		rdes0 = le32_to_cpu(rxptr->rdes0);
		if (rdes0 & 0x80000000)	
			break;

		db->rx_avail_cnt--;
		db->interval_rx_cnt++;

		pci_unmap_single(db->pdev, le32_to_cpu(rxptr->rdes2),
				 RX_ALLOC_SIZE, PCI_DMA_FROMDEVICE);

		if ( (rdes0 & 0x300) != 0x300) {
			
			
			DMFE_DBUG(0, "Reuse SK buffer, rdes0", rdes0);
			dmfe_reuse_skb(db, rxptr->rx_skb_ptr);
		} else {
			
			rxlen = ( (rdes0 >> 16) & 0x3fff) - 4;

			
			if (rdes0 & 0x8000) {
				
				dev->stats.rx_errors++;
				if (rdes0 & 1)
					dev->stats.rx_fifo_errors++;
				if (rdes0 & 2)
					dev->stats.rx_crc_errors++;
				if (rdes0 & 0x80)
					dev->stats.rx_length_errors++;
			}

			if ( !(rdes0 & 0x8000) ||
				((db->cr6_data & CR6_PM) && (rxlen>6)) ) {
				skb = rxptr->rx_skb_ptr;

				
				if ( (db->dm910x_chk_mode & 1) &&
					(cal_CRC(skb->data, rxlen, 1) !=
					(*(u32 *) (skb->data+rxlen) ))) { 
					
					dmfe_reuse_skb(db, rxptr->rx_skb_ptr);
					db->dm910x_chk_mode = 3;
				} else {
					
					
					if ((rxlen < RX_COPY_SIZE) &&
						((newskb = netdev_alloc_skb(dev, rxlen + 2))
						!= NULL)) {

						skb = newskb;
						
						skb_reserve(skb, 2); 
						skb_copy_from_linear_data(rxptr->rx_skb_ptr,
							  skb_put(skb, rxlen),
									  rxlen);
						dmfe_reuse_skb(db, rxptr->rx_skb_ptr);
					} else
						skb_put(skb, rxlen);

					skb->protocol = eth_type_trans(skb, dev);
					netif_rx(skb);
					dev->stats.rx_packets++;
					dev->stats.rx_bytes += rxlen;
				}
			} else {
				
				DMFE_DBUG(0, "Reuse SK buffer, rdes0", rdes0);
				dmfe_reuse_skb(db, rxptr->rx_skb_ptr);
			}
		}

		rxptr = rxptr->next_rx_desc;
	}

	db->rx_ready_ptr = rxptr;
}


static void dmfe_set_filter_mode(struct DEVICE * dev)
{
	struct dmfe_board_info *db = netdev_priv(dev);
	unsigned long flags;
	int mc_count = netdev_mc_count(dev);

	DMFE_DBUG(0, "dmfe_set_filter_mode()", 0);
	spin_lock_irqsave(&db->lock, flags);

	if (dev->flags & IFF_PROMISC) {
		DMFE_DBUG(0, "Enable PROM Mode", 0);
		db->cr6_data |= CR6_PM | CR6_PBF;
		update_cr6(db->cr6_data, db->ioaddr);
		spin_unlock_irqrestore(&db->lock, flags);
		return;
	}

	if (dev->flags & IFF_ALLMULTI || mc_count > DMFE_MAX_MULTICAST) {
		DMFE_DBUG(0, "Pass all multicast address", mc_count);
		db->cr6_data &= ~(CR6_PM | CR6_PBF);
		db->cr6_data |= CR6_PAM;
		spin_unlock_irqrestore(&db->lock, flags);
		return;
	}

	DMFE_DBUG(0, "Set multicast address", mc_count);
	if (db->chip_id == PCI_DM9132_ID)
		dm9132_id_table(dev);	
	else
		send_filter_frame(dev);	
	spin_unlock_irqrestore(&db->lock, flags);
}


static void dmfe_ethtool_get_drvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	struct dmfe_board_info *np = netdev_priv(dev);

	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	if (np->pdev)
		strlcpy(info->bus_info, pci_name(np->pdev),
			sizeof(info->bus_info));
	else
		sprintf(info->bus_info, "EISA 0x%lx %d",
			dev->base_addr, dev->irq);
}

static int dmfe_ethtool_set_wol(struct net_device *dev,
				struct ethtool_wolinfo *wolinfo)
{
	struct dmfe_board_info *db = netdev_priv(dev);

	if (wolinfo->wolopts & (WAKE_UCAST | WAKE_MCAST | WAKE_BCAST |
		   		WAKE_ARP | WAKE_MAGICSECURE))
		   return -EOPNOTSUPP;

	db->wol_mode = wolinfo->wolopts;
	return 0;
}

static void dmfe_ethtool_get_wol(struct net_device *dev,
				 struct ethtool_wolinfo *wolinfo)
{
	struct dmfe_board_info *db = netdev_priv(dev);

	wolinfo->supported = WAKE_PHY | WAKE_MAGIC;
	wolinfo->wolopts = db->wol_mode;
}


static const struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= dmfe_ethtool_get_drvinfo,
	.get_link               = ethtool_op_get_link,
	.set_wol		= dmfe_ethtool_set_wol,
	.get_wol		= dmfe_ethtool_get_wol,
};


static void dmfe_timer(unsigned long data)
{
	u32 tmp_cr8;
	unsigned char tmp_cr12;
	struct DEVICE *dev = (struct DEVICE *) data;
	struct dmfe_board_info *db = netdev_priv(dev);
 	unsigned long flags;

	int link_ok, link_ok_phy;

	DMFE_DBUG(0, "dmfe_timer()", 0);
	spin_lock_irqsave(&db->lock, flags);

	
	if (db->first_in_callback == 0) {
		db->first_in_callback = 1;
		if (db->chip_type && (db->chip_id==PCI_DM9102_ID)) {
			db->cr6_data &= ~0x40000;
			update_cr6(db->cr6_data, db->ioaddr);
			phy_write(db->ioaddr,
				  db->phy_addr, 0, 0x1000, db->chip_id);
			db->cr6_data |= 0x40000;
			update_cr6(db->cr6_data, db->ioaddr);
			db->timer.expires = DMFE_TIMER_WUT + HZ * 2;
			add_timer(&db->timer);
			spin_unlock_irqrestore(&db->lock, flags);
			return;
		}
	}


	
	if ( (db->dm910x_chk_mode & 0x1) &&
		(dev->stats.rx_packets > MAX_CHECK_PACKET) )
		db->dm910x_chk_mode = 0x4;

	
	tmp_cr8 = inl(db->ioaddr + DCR8);
	if ( (db->interval_rx_cnt==0) && (tmp_cr8) ) {
		db->reset_cr8++;
		db->wait_reset = 1;
	}
	db->interval_rx_cnt = 0;

	
	if ( db->tx_packet_cnt &&
	     time_after(jiffies, dev_trans_start(dev) + DMFE_TX_KICK) ) {
		outl(0x1, dev->base_addr + DCR1);   

		
		if (time_after(jiffies, dev_trans_start(dev) + DMFE_TX_TIMEOUT) ) {
			db->reset_TXtimeout++;
			db->wait_reset = 1;
			dev_warn(&dev->dev, "Tx timeout - resetting\n");
		}
	}

	if (db->wait_reset) {
		DMFE_DBUG(0, "Dynamic Reset device", db->tx_packet_cnt);
		db->reset_count++;
		dmfe_dynamic_reset(dev);
		db->first_in_callback = 0;
		db->timer.expires = DMFE_TIMER_WUT;
		add_timer(&db->timer);
		spin_unlock_irqrestore(&db->lock, flags);
		return;
	}

	
	if (db->chip_id == PCI_DM9132_ID)
		tmp_cr12 = inb(db->ioaddr + DCR9 + 3);	
	else
		tmp_cr12 = inb(db->ioaddr + DCR12);	

	if ( ((db->chip_id == PCI_DM9102_ID) &&
		(db->chip_revision == 0x30)) ||
		((db->chip_id == PCI_DM9132_ID) &&
		(db->chip_revision == 0x10)) ) {
		
		if (tmp_cr12 & 2)
			link_ok = 0;
		else
			link_ok = 1;
	}
	else
		link_ok = (tmp_cr12 & 0x43) ? 1 : 0;



	
	phy_read (db->ioaddr, db->phy_addr, 1, db->chip_id);
	link_ok_phy = (phy_read (db->ioaddr,
		       db->phy_addr, 1, db->chip_id) & 0x4) ? 1 : 0;

	if (link_ok_phy != link_ok) {
		DMFE_DBUG (0, "PHY and chip report different link status", 0);
		link_ok = link_ok | link_ok_phy;
 	}

	if ( !link_ok && netif_carrier_ok(dev)) {
		
		DMFE_DBUG(0, "Link Failed", tmp_cr12);
		netif_carrier_off(dev);

		
		
		if ( !(db->media_mode & 0x38) )
			phy_write(db->ioaddr, db->phy_addr,
				  0, 0x1000, db->chip_id);

		
		if (db->media_mode & DMFE_AUTO) {
			
			db->cr6_data|=0x00040000;	
			db->cr6_data&=~0x00000200;	
			update_cr6(db->cr6_data, db->ioaddr);
		}
	} else if (!netif_carrier_ok(dev)) {

		DMFE_DBUG(0, "Link link OK", tmp_cr12);

		
		if ( !(db->media_mode & DMFE_AUTO) || !dmfe_sense_speed(db)) {
			netif_carrier_on(dev);
			SHOW_MEDIA_TYPE(db->op_mode);
		}

		dmfe_process_mode(db);
	}

	
	if (db->HPNA_command & 0xf00) {
		db->HPNA_timer--;
		if (!db->HPNA_timer)
			dmfe_HPNA_remote_cmd_chk(db);
	}

	
	db->timer.expires = DMFE_TIMER_WUT;
	add_timer(&db->timer);
	spin_unlock_irqrestore(&db->lock, flags);
}



static void dmfe_dynamic_reset(struct DEVICE *dev)
{
	struct dmfe_board_info *db = netdev_priv(dev);

	DMFE_DBUG(0, "dmfe_dynamic_reset()", 0);

	
	db->cr6_data &= ~(CR6_RXSC | CR6_TXSC);	
	update_cr6(db->cr6_data, dev->base_addr);
	outl(0, dev->base_addr + DCR7);		
	outl(inl(dev->base_addr + DCR5), dev->base_addr + DCR5);

	
	netif_stop_queue(dev);

	
	dmfe_free_rxbuffer(db);

	
	db->tx_packet_cnt = 0;
	db->tx_queue_cnt = 0;
	db->rx_avail_cnt = 0;
	netif_carrier_off(dev);
	db->wait_reset = 0;

	
	dmfe_init_dm910x(dev);

	
	netif_wake_queue(dev);
}



static void dmfe_free_rxbuffer(struct dmfe_board_info * db)
{
	DMFE_DBUG(0, "dmfe_free_rxbuffer()", 0);

	
	while (db->rx_avail_cnt) {
		dev_kfree_skb(db->rx_ready_ptr->rx_skb_ptr);
		db->rx_ready_ptr = db->rx_ready_ptr->next_rx_desc;
		db->rx_avail_cnt--;
	}
}



static void dmfe_reuse_skb(struct dmfe_board_info *db, struct sk_buff * skb)
{
	struct rx_desc *rxptr = db->rx_insert_ptr;

	if (!(rxptr->rdes0 & cpu_to_le32(0x80000000))) {
		rxptr->rx_skb_ptr = skb;
		rxptr->rdes2 = cpu_to_le32( pci_map_single(db->pdev,
			    skb->data, RX_ALLOC_SIZE, PCI_DMA_FROMDEVICE) );
		wmb();
		rxptr->rdes0 = cpu_to_le32(0x80000000);
		db->rx_avail_cnt++;
		db->rx_insert_ptr = rxptr->next_rx_desc;
	} else
		DMFE_DBUG(0, "SK Buffer reuse method error", db->rx_avail_cnt);
}



static void dmfe_descriptor_init(struct net_device *dev, unsigned long ioaddr)
{
	struct dmfe_board_info *db = netdev_priv(dev);
	struct tx_desc *tmp_tx;
	struct rx_desc *tmp_rx;
	unsigned char *tmp_buf;
	dma_addr_t tmp_tx_dma, tmp_rx_dma;
	dma_addr_t tmp_buf_dma;
	int i;

	DMFE_DBUG(0, "dmfe_descriptor_init()", 0);

	
	db->tx_insert_ptr = db->first_tx_desc;
	db->tx_remove_ptr = db->first_tx_desc;
	outl(db->first_tx_desc_dma, ioaddr + DCR4);     

	
	db->first_rx_desc = (void *)db->first_tx_desc +
			sizeof(struct tx_desc) * TX_DESC_CNT;

	db->first_rx_desc_dma =  db->first_tx_desc_dma +
			sizeof(struct tx_desc) * TX_DESC_CNT;
	db->rx_insert_ptr = db->first_rx_desc;
	db->rx_ready_ptr = db->first_rx_desc;
	outl(db->first_rx_desc_dma, ioaddr + DCR3);	

	
	tmp_buf = db->buf_pool_start;
	tmp_buf_dma = db->buf_pool_dma_start;
	tmp_tx_dma = db->first_tx_desc_dma;
	for (tmp_tx = db->first_tx_desc, i = 0; i < TX_DESC_CNT; i++, tmp_tx++) {
		tmp_tx->tx_buf_ptr = tmp_buf;
		tmp_tx->tdes0 = cpu_to_le32(0);
		tmp_tx->tdes1 = cpu_to_le32(0x81000000);	
		tmp_tx->tdes2 = cpu_to_le32(tmp_buf_dma);
		tmp_tx_dma += sizeof(struct tx_desc);
		tmp_tx->tdes3 = cpu_to_le32(tmp_tx_dma);
		tmp_tx->next_tx_desc = tmp_tx + 1;
		tmp_buf = tmp_buf + TX_BUF_ALLOC;
		tmp_buf_dma = tmp_buf_dma + TX_BUF_ALLOC;
	}
	(--tmp_tx)->tdes3 = cpu_to_le32(db->first_tx_desc_dma);
	tmp_tx->next_tx_desc = db->first_tx_desc;

	 
	tmp_rx_dma=db->first_rx_desc_dma;
	for (tmp_rx = db->first_rx_desc, i = 0; i < RX_DESC_CNT; i++, tmp_rx++) {
		tmp_rx->rdes0 = cpu_to_le32(0);
		tmp_rx->rdes1 = cpu_to_le32(0x01000600);
		tmp_rx_dma += sizeof(struct rx_desc);
		tmp_rx->rdes3 = cpu_to_le32(tmp_rx_dma);
		tmp_rx->next_rx_desc = tmp_rx + 1;
	}
	(--tmp_rx)->rdes3 = cpu_to_le32(db->first_rx_desc_dma);
	tmp_rx->next_rx_desc = db->first_rx_desc;

	
	allocate_rx_buffer(dev);
}


/*
 *	Update CR6 value
 *	Firstly stop DM910X , then written value and start
 */

static void update_cr6(u32 cr6_data, unsigned long ioaddr)
{
	u32 cr6_tmp;

	cr6_tmp = cr6_data & ~0x2002;           
	outl(cr6_tmp, ioaddr + DCR6);
	udelay(5);
	outl(cr6_data, ioaddr + DCR6);
	udelay(5);
}



static void dm9132_id_table(struct DEVICE *dev)
{
	struct netdev_hw_addr *ha;
	u16 * addrptr;
	unsigned long ioaddr = dev->base_addr+0xc0;		
	u32 hash_val;
	u16 i, hash_table[4];

	DMFE_DBUG(0, "dm9132_id_table()", 0);

	
	addrptr = (u16 *) dev->dev_addr;
	outw(addrptr[0], ioaddr);
	ioaddr += 4;
	outw(addrptr[1], ioaddr);
	ioaddr += 4;
	outw(addrptr[2], ioaddr);
	ioaddr += 4;

	
	memset(hash_table, 0, sizeof(hash_table));

	
	hash_table[3] = 0x8000;

	
	netdev_for_each_mc_addr(ha, dev) {
		hash_val = cal_CRC((char *) ha->addr, 6, 0) & 0x3f;
		hash_table[hash_val / 16] |= (u16) 1 << (hash_val % 16);
	}

	
	for (i = 0; i < 4; i++, ioaddr += 4)
		outw(hash_table[i], ioaddr);
}



static void send_filter_frame(struct DEVICE *dev)
{
	struct dmfe_board_info *db = netdev_priv(dev);
	struct netdev_hw_addr *ha;
	struct tx_desc *txptr;
	u16 * addrptr;
	u32 * suptr;
	int i;

	DMFE_DBUG(0, "send_filter_frame()", 0);

	txptr = db->tx_insert_ptr;
	suptr = (u32 *) txptr->tx_buf_ptr;

	
	addrptr = (u16 *) dev->dev_addr;
	*suptr++ = addrptr[0];
	*suptr++ = addrptr[1];
	*suptr++ = addrptr[2];

	
	*suptr++ = 0xffff;
	*suptr++ = 0xffff;
	*suptr++ = 0xffff;

	
	netdev_for_each_mc_addr(ha, dev) {
		addrptr = (u16 *) ha->addr;
		*suptr++ = addrptr[0];
		*suptr++ = addrptr[1];
		*suptr++ = addrptr[2];
	}

	for (i = netdev_mc_count(dev); i < 14; i++) {
		*suptr++ = 0xffff;
		*suptr++ = 0xffff;
		*suptr++ = 0xffff;
	}

	
	db->tx_insert_ptr = txptr->next_tx_desc;
	txptr->tdes1 = cpu_to_le32(0x890000c0);

	
	if (!db->tx_packet_cnt) {
		
		db->tx_packet_cnt++;
		txptr->tdes0 = cpu_to_le32(0x80000000);
		update_cr6(db->cr6_data | 0x2000, dev->base_addr);
		outl(0x1, dev->base_addr + DCR1);	
		update_cr6(db->cr6_data, dev->base_addr);
		dev->trans_start = jiffies;
	} else
		db->tx_queue_cnt++;	
}



static void allocate_rx_buffer(struct net_device *dev)
{
	struct dmfe_board_info *db = netdev_priv(dev);
	struct rx_desc *rxptr;
	struct sk_buff *skb;

	rxptr = db->rx_insert_ptr;

	while(db->rx_avail_cnt < RX_DESC_CNT) {
		if ( ( skb = netdev_alloc_skb(dev, RX_ALLOC_SIZE) ) == NULL )
			break;
		rxptr->rx_skb_ptr = skb; 
		rxptr->rdes2 = cpu_to_le32( pci_map_single(db->pdev, skb->data,
				    RX_ALLOC_SIZE, PCI_DMA_FROMDEVICE) );
		wmb();
		rxptr->rdes0 = cpu_to_le32(0x80000000);
		rxptr = rxptr->next_rx_desc;
		db->rx_avail_cnt++;
	}

	db->rx_insert_ptr = rxptr;
}



static u16 read_srom_word(long ioaddr, int offset)
{
	int i;
	u16 srom_data = 0;
	long cr9_ioaddr = ioaddr + DCR9;

	outl(CR9_SROM_READ, cr9_ioaddr);
	outl(CR9_SROM_READ | CR9_SRCS, cr9_ioaddr);

	
	SROM_CLK_WRITE(SROM_DATA_1, cr9_ioaddr);
	SROM_CLK_WRITE(SROM_DATA_1, cr9_ioaddr);
	SROM_CLK_WRITE(SROM_DATA_0, cr9_ioaddr);

	
	for (i = 5; i >= 0; i--) {
		srom_data = (offset & (1 << i)) ? SROM_DATA_1 : SROM_DATA_0;
		SROM_CLK_WRITE(srom_data, cr9_ioaddr);
	}

	outl(CR9_SROM_READ | CR9_SRCS, cr9_ioaddr);

	for (i = 16; i > 0; i--) {
		outl(CR9_SROM_READ | CR9_SRCS | CR9_SRCLK, cr9_ioaddr);
		udelay(5);
		srom_data = (srom_data << 1) |
				((inl(cr9_ioaddr) & CR9_CRDOUT) ? 1 : 0);
		outl(CR9_SROM_READ | CR9_SRCS, cr9_ioaddr);
		udelay(5);
	}

	outl(CR9_SROM_READ, cr9_ioaddr);
	return srom_data;
}



static u8 dmfe_sense_speed(struct dmfe_board_info * db)
{
	u8 ErrFlag = 0;
	u16 phy_mode;

	
	update_cr6( (db->cr6_data & ~0x40000), db->ioaddr);

	phy_mode = phy_read(db->ioaddr, db->phy_addr, 1, db->chip_id);
	phy_mode = phy_read(db->ioaddr, db->phy_addr, 1, db->chip_id);

	if ( (phy_mode & 0x24) == 0x24 ) {
		if (db->chip_id == PCI_DM9132_ID)	
			phy_mode = phy_read(db->ioaddr,
				    db->phy_addr, 7, db->chip_id) & 0xf000;
		else 				
			phy_mode = phy_read(db->ioaddr,
				    db->phy_addr, 17, db->chip_id) & 0xf000;
		switch (phy_mode) {
		case 0x1000: db->op_mode = DMFE_10MHF; break;
		case 0x2000: db->op_mode = DMFE_10MFD; break;
		case 0x4000: db->op_mode = DMFE_100MHF; break;
		case 0x8000: db->op_mode = DMFE_100MFD; break;
		default: db->op_mode = DMFE_10MHF;
			ErrFlag = 1;
			break;
		}
	} else {
		db->op_mode = DMFE_10MHF;
		DMFE_DBUG(0, "Link Failed :", phy_mode);
		ErrFlag = 1;
	}

	return ErrFlag;
}



static void dmfe_set_phyxcer(struct dmfe_board_info *db)
{
	u16 phy_reg;

	
	db->cr6_data &= ~0x40000;
	update_cr6(db->cr6_data, db->ioaddr);

	
	if (db->chip_id == PCI_DM9009_ID) {
		phy_reg = phy_read(db->ioaddr,
				   db->phy_addr, 18, db->chip_id) & ~0x1000;

		phy_write(db->ioaddr,
			  db->phy_addr, 18, phy_reg, db->chip_id);
	}

	
	phy_reg = phy_read(db->ioaddr, db->phy_addr, 4, db->chip_id) & ~0x01e0;

	if (db->media_mode & DMFE_AUTO) {
		
		phy_reg |= db->PHY_reg4;
	} else {
		
		switch(db->media_mode) {
		case DMFE_10MHF: phy_reg |= 0x20; break;
		case DMFE_10MFD: phy_reg |= 0x40; break;
		case DMFE_100MHF: phy_reg |= 0x80; break;
		case DMFE_100MFD: phy_reg |= 0x100; break;
		}
		if (db->chip_id == PCI_DM9009_ID) phy_reg &= 0x61;
	}

  	
	if ( !(phy_reg & 0x01e0)) {
		phy_reg|=db->PHY_reg4;
		db->media_mode|=DMFE_AUTO;
	}
	phy_write(db->ioaddr, db->phy_addr, 4, phy_reg, db->chip_id);

 	
	if ( db->chip_type && (db->chip_id == PCI_DM9102_ID) )
		phy_write(db->ioaddr, db->phy_addr, 0, 0x1800, db->chip_id);
	if ( !db->chip_type )
		phy_write(db->ioaddr, db->phy_addr, 0, 0x1200, db->chip_id);
}



static void dmfe_process_mode(struct dmfe_board_info *db)
{
	u16 phy_reg;

	
	if (db->op_mode & 0x4)
		db->cr6_data |= CR6_FDM;	
	else
		db->cr6_data &= ~CR6_FDM;	

	
	if (db->op_mode & 0x10)		
		db->cr6_data |= 0x40000;
	else
		db->cr6_data &= ~0x40000;

	update_cr6(db->cr6_data, db->ioaddr);

	
	if ( !(db->media_mode & 0x18)) {
		
		phy_reg = phy_read(db->ioaddr, db->phy_addr, 6, db->chip_id);
		if ( !(phy_reg & 0x1) ) {
			
			phy_reg = 0x0;
			switch(db->op_mode) {
			case DMFE_10MHF: phy_reg = 0x0; break;
			case DMFE_10MFD: phy_reg = 0x100; break;
			case DMFE_100MHF: phy_reg = 0x2000; break;
			case DMFE_100MFD: phy_reg = 0x2100; break;
			}
			phy_write(db->ioaddr,
				  db->phy_addr, 0, phy_reg, db->chip_id);
       			if ( db->chip_type && (db->chip_id == PCI_DM9102_ID) )
				mdelay(20);
			phy_write(db->ioaddr,
				  db->phy_addr, 0, phy_reg, db->chip_id);
		}
	}
}



static void phy_write(unsigned long iobase, u8 phy_addr, u8 offset,
		      u16 phy_data, u32 chip_id)
{
	u16 i;
	unsigned long ioaddr;

	if (chip_id == PCI_DM9132_ID) {
		ioaddr = iobase + 0x80 + offset * 4;
		outw(phy_data, ioaddr);
	} else {
		
		ioaddr = iobase + DCR9;

		
		for (i = 0; i < 35; i++)
			phy_write_1bit(ioaddr, PHY_DATA_1);

		
		phy_write_1bit(ioaddr, PHY_DATA_0);
		phy_write_1bit(ioaddr, PHY_DATA_1);

		
		phy_write_1bit(ioaddr, PHY_DATA_0);
		phy_write_1bit(ioaddr, PHY_DATA_1);

		
		for (i = 0x10; i > 0; i = i >> 1)
			phy_write_1bit(ioaddr,
				       phy_addr & i ? PHY_DATA_1 : PHY_DATA_0);

		
		for (i = 0x10; i > 0; i = i >> 1)
			phy_write_1bit(ioaddr,
				       offset & i ? PHY_DATA_1 : PHY_DATA_0);

		/* written trasnition */
		phy_write_1bit(ioaddr, PHY_DATA_1);
		phy_write_1bit(ioaddr, PHY_DATA_0);

		
		for ( i = 0x8000; i > 0; i >>= 1)
			phy_write_1bit(ioaddr,
				       phy_data & i ? PHY_DATA_1 : PHY_DATA_0);
	}
}



static u16 phy_read(unsigned long iobase, u8 phy_addr, u8 offset, u32 chip_id)
{
	int i;
	u16 phy_data;
	unsigned long ioaddr;

	if (chip_id == PCI_DM9132_ID) {
		
		ioaddr = iobase + 0x80 + offset * 4;
		phy_data = inw(ioaddr);
	} else {
		
		ioaddr = iobase + DCR9;

		
		for (i = 0; i < 35; i++)
			phy_write_1bit(ioaddr, PHY_DATA_1);

		
		phy_write_1bit(ioaddr, PHY_DATA_0);
		phy_write_1bit(ioaddr, PHY_DATA_1);

		
		phy_write_1bit(ioaddr, PHY_DATA_1);
		phy_write_1bit(ioaddr, PHY_DATA_0);

		
		for (i = 0x10; i > 0; i = i >> 1)
			phy_write_1bit(ioaddr,
				       phy_addr & i ? PHY_DATA_1 : PHY_DATA_0);

		
		for (i = 0x10; i > 0; i = i >> 1)
			phy_write_1bit(ioaddr,
				       offset & i ? PHY_DATA_1 : PHY_DATA_0);

		
		phy_read_1bit(ioaddr);

		
		for (phy_data = 0, i = 0; i < 16; i++) {
			phy_data <<= 1;
			phy_data |= phy_read_1bit(ioaddr);
		}
	}

	return phy_data;
}



static void phy_write_1bit(unsigned long ioaddr, u32 phy_data)
{
	outl(phy_data, ioaddr);			
	udelay(1);
	outl(phy_data | MDCLKH, ioaddr);	
	udelay(1);
	outl(phy_data, ioaddr);			
	udelay(1);
}



static u16 phy_read_1bit(unsigned long ioaddr)
{
	u16 phy_data;

	outl(0x50000, ioaddr);
	udelay(1);
	phy_data = ( inl(ioaddr) >> 19 ) & 0x1;
	outl(0x40000, ioaddr);
	udelay(1);

	return phy_data;
}



static void dmfe_parse_srom(struct dmfe_board_info * db)
{
	char * srom = db->srom;
	int dmfe_mode, tmp_reg;

	DMFE_DBUG(0, "dmfe_parse_srom() ", 0);

	
	db->cr15_data = CR15_DEFAULT;

	
	if ( ( (int) srom[18] & 0xff) == SROM_V41_CODE) {
		
		
		db->NIC_capability = le16_to_cpup((__le16 *) (srom + 34));
		db->PHY_reg4 = 0;
		for (tmp_reg = 1; tmp_reg < 0x10; tmp_reg <<= 1) {
			switch( db->NIC_capability & tmp_reg ) {
			case 0x1: db->PHY_reg4 |= 0x0020; break;
			case 0x2: db->PHY_reg4 |= 0x0040; break;
			case 0x4: db->PHY_reg4 |= 0x0080; break;
			case 0x8: db->PHY_reg4 |= 0x0100; break;
			}
		}

		
		dmfe_mode = (le32_to_cpup((__le32 *) (srom + 34)) &
			     le32_to_cpup((__le32 *) (srom + 36)));
		switch(dmfe_mode) {
		case 0x4: dmfe_media_mode = DMFE_100MHF; break;	
		case 0x2: dmfe_media_mode = DMFE_10MFD; break;	
		case 0x8: dmfe_media_mode = DMFE_100MFD; break;	
		case 0x100:
		case 0x200: dmfe_media_mode = DMFE_1M_HPNA; break;
		}

		
		
		if ( (SF_mode & 0x1) || (srom[43] & 0x80) )
			db->cr15_data |= 0x40;

		
		if ( (SF_mode & 0x2) || (srom[40] & 0x1) )
			db->cr15_data |= 0x400;

		
		if ( (SF_mode & 0x4) || (srom[40] & 0xe) )
			db->cr15_data |= 0x9800;
	}

	
	db->HPNA_command = 1;

	
	if (HPNA_rx_cmd == 0)
		db->HPNA_command |= 0x8000;

	 
	if (HPNA_tx_cmd == 1)
		switch(HPNA_mode) {	
		case 0: db->HPNA_command |= 0x0904; break;
		case 1: db->HPNA_command |= 0x0a00; break;
		case 2: db->HPNA_command |= 0x0506; break;
		case 3: db->HPNA_command |= 0x0602; break;
		}
	else
		switch(HPNA_mode) {	
		case 0: db->HPNA_command |= 0x0004; break;
		case 1: db->HPNA_command |= 0x0000; break;
		case 2: db->HPNA_command |= 0x0006; break;
		case 3: db->HPNA_command |= 0x0002; break;
		}

	
	db->HPNA_present = 0;
	update_cr6(db->cr6_data|0x40000, db->ioaddr);
	tmp_reg = phy_read(db->ioaddr, db->phy_addr, 3, db->chip_id);
	if ( ( tmp_reg & 0xfff0 ) == 0xb900 ) {
		
		db->HPNA_timer = 8;
		if ( phy_read(db->ioaddr, db->phy_addr, 31, db->chip_id) == 0x4404) {
			
			db->HPNA_present = 1;
			dmfe_program_DM9801(db, tmp_reg);
		} else {
			
			db->HPNA_present = 2;
			dmfe_program_DM9802(db);
		}
	}

}



static void dmfe_program_DM9801(struct dmfe_board_info * db, int HPNA_rev)
{
	uint reg17, reg25;

	if ( !HPNA_NoiseFloor ) HPNA_NoiseFloor = DM9801_NOISE_FLOOR;
	switch(HPNA_rev) {
	case 0xb900: 
		db->HPNA_command |= 0x1000;
		reg25 = phy_read(db->ioaddr, db->phy_addr, 24, db->chip_id);
		reg25 = ( (reg25 + HPNA_NoiseFloor) & 0xff) | 0xf000;
		reg17 = phy_read(db->ioaddr, db->phy_addr, 17, db->chip_id);
		break;
	case 0xb901: 
		reg25 = phy_read(db->ioaddr, db->phy_addr, 25, db->chip_id);
		reg25 = (reg25 & 0xff00) + HPNA_NoiseFloor;
		reg17 = phy_read(db->ioaddr, db->phy_addr, 17, db->chip_id);
		reg17 = (reg17 & 0xfff0) + HPNA_NoiseFloor + 3;
		break;
	case 0xb902: 
	case 0xb903: 
	default:
		db->HPNA_command |= 0x1000;
		reg25 = phy_read(db->ioaddr, db->phy_addr, 25, db->chip_id);
		reg25 = (reg25 & 0xff00) + HPNA_NoiseFloor - 5;
		reg17 = phy_read(db->ioaddr, db->phy_addr, 17, db->chip_id);
		reg17 = (reg17 & 0xfff0) + HPNA_NoiseFloor;
		break;
	}
	phy_write(db->ioaddr, db->phy_addr, 16, db->HPNA_command, db->chip_id);
	phy_write(db->ioaddr, db->phy_addr, 17, reg17, db->chip_id);
	phy_write(db->ioaddr, db->phy_addr, 25, reg25, db->chip_id);
}



static void dmfe_program_DM9802(struct dmfe_board_info * db)
{
	uint phy_reg;

	if ( !HPNA_NoiseFloor ) HPNA_NoiseFloor = DM9802_NOISE_FLOOR;
	phy_write(db->ioaddr, db->phy_addr, 16, db->HPNA_command, db->chip_id);
	phy_reg = phy_read(db->ioaddr, db->phy_addr, 25, db->chip_id);
	phy_reg = ( phy_reg & 0xff00) + HPNA_NoiseFloor;
	phy_write(db->ioaddr, db->phy_addr, 25, phy_reg, db->chip_id);
}



static void dmfe_HPNA_remote_cmd_chk(struct dmfe_board_info * db)
{
	uint phy_reg;

	
	phy_reg = phy_read(db->ioaddr, db->phy_addr, 17, db->chip_id) & 0x60;
	switch(phy_reg) {
	case 0x00: phy_reg = 0x0a00;break; 
	case 0x20: phy_reg = 0x0900;break; 
	case 0x40: phy_reg = 0x0600;break; 
	case 0x60: phy_reg = 0x0500;break; 
	}

	
	if ( phy_reg != (db->HPNA_command & 0x0f00) ) {
		phy_write(db->ioaddr, db->phy_addr, 16, db->HPNA_command,
			  db->chip_id);
		db->HPNA_timer=8;
	} else
		db->HPNA_timer=600;	
}



static DEFINE_PCI_DEVICE_TABLE(dmfe_pci_tbl) = {
	{ 0x1282, 0x9132, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_DM9132_ID },
	{ 0x1282, 0x9102, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_DM9102_ID },
	{ 0x1282, 0x9100, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_DM9100_ID },
	{ 0x1282, 0x9009, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PCI_DM9009_ID },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, dmfe_pci_tbl);


#ifdef CONFIG_PM
static int dmfe_suspend(struct pci_dev *pci_dev, pm_message_t state)
{
	struct net_device *dev = pci_get_drvdata(pci_dev);
	struct dmfe_board_info *db = netdev_priv(dev);
	u32 tmp;

	
	netif_device_detach(dev);

	
	db->cr6_data &= ~(CR6_RXSC | CR6_TXSC);
	update_cr6(db->cr6_data, dev->base_addr);

	
	outl(0, dev->base_addr + DCR7);
	outl(inl (dev->base_addr + DCR5), dev->base_addr + DCR5);

	
	dmfe_free_rxbuffer(db);

	
	pci_read_config_dword(pci_dev, 0x40, &tmp);
	tmp &= ~(DMFE_WOL_LINKCHANGE|DMFE_WOL_MAGICPACKET);

	if (db->wol_mode & WAKE_PHY)
		tmp |= DMFE_WOL_LINKCHANGE;
	if (db->wol_mode & WAKE_MAGIC)
		tmp |= DMFE_WOL_MAGICPACKET;

	pci_write_config_dword(pci_dev, 0x40, tmp);

	pci_enable_wake(pci_dev, PCI_D3hot, 1);
	pci_enable_wake(pci_dev, PCI_D3cold, 1);

	
	pci_save_state(pci_dev);
	pci_set_power_state(pci_dev, pci_choose_state (pci_dev, state));

	return 0;
}

static int dmfe_resume(struct pci_dev *pci_dev)
{
	struct net_device *dev = pci_get_drvdata(pci_dev);
	u32 tmp;

	pci_set_power_state(pci_dev, PCI_D0);
	pci_restore_state(pci_dev);

	
	dmfe_init_dm910x(dev);

	
	pci_read_config_dword(pci_dev, 0x40, &tmp);

	tmp &= ~(DMFE_WOL_LINKCHANGE | DMFE_WOL_MAGICPACKET);
	pci_write_config_dword(pci_dev, 0x40, tmp);

	pci_enable_wake(pci_dev, PCI_D3hot, 0);
	pci_enable_wake(pci_dev, PCI_D3cold, 0);

	
	netif_device_attach(dev);

	return 0;
}
#else
#define dmfe_suspend NULL
#define dmfe_resume NULL
#endif

static struct pci_driver dmfe_driver = {
	.name		= "dmfe",
	.id_table	= dmfe_pci_tbl,
	.probe		= dmfe_init_one,
	.remove		= __devexit_p(dmfe_remove_one),
	.suspend        = dmfe_suspend,
	.resume         = dmfe_resume
};

MODULE_AUTHOR("Sten Wang, sten_wang@davicom.com.tw");
MODULE_DESCRIPTION("Davicom DM910X fast ethernet driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

module_param(debug, int, 0);
module_param(mode, byte, 0);
module_param(cr6set, int, 0);
module_param(chkmode, byte, 0);
module_param(HPNA_mode, byte, 0);
module_param(HPNA_rx_cmd, byte, 0);
module_param(HPNA_tx_cmd, byte, 0);
module_param(HPNA_NoiseFloor, byte, 0);
module_param(SF_mode, byte, 0);
MODULE_PARM_DESC(debug, "Davicom DM9xxx enable debugging (0-1)");
MODULE_PARM_DESC(mode, "Davicom DM9xxx: "
		"Bit 0: 10/100Mbps, bit 2: duplex, bit 8: HomePNA");

MODULE_PARM_DESC(SF_mode, "Davicom DM9xxx special function "
		"(bit 0: VLAN, bit 1 Flow Control, bit 2: TX pause packet)");


static int __init dmfe_init_module(void)
{
	int rc;

	pr_info("%s\n", version);
	printed_version = 1;

	DMFE_DBUG(0, "init_module() ", debug);

	if (debug)
		dmfe_debug = debug;	
	if (cr6set)
		dmfe_cr6_user_set = cr6set;

 	switch(mode) {
   	case DMFE_10MHF:
	case DMFE_100MHF:
	case DMFE_10MFD:
	case DMFE_100MFD:
	case DMFE_1M_HPNA:
		dmfe_media_mode = mode;
		break;
	default:dmfe_media_mode = DMFE_AUTO;
		break;
	}

	if (HPNA_mode > 4)
		HPNA_mode = 0;		
	if (HPNA_rx_cmd > 1)
		HPNA_rx_cmd = 0;	
	if (HPNA_tx_cmd > 1)
		HPNA_tx_cmd = 0;	
	if (HPNA_NoiseFloor > 15)
		HPNA_NoiseFloor = 0;

	rc = pci_register_driver(&dmfe_driver);
	if (rc < 0)
		return rc;

	return 0;
}



static void __exit dmfe_cleanup_module(void)
{
	DMFE_DBUG(0, "dmfe_clean_module() ", debug);
	pci_unregister_driver(&dmfe_driver);
}

module_init(dmfe_init_module);
module_exit(dmfe_cleanup_module);
