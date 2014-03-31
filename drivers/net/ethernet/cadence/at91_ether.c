/*
 * Ethernet driver for the Atmel AT91RM9200 (Thunder)
 *
 *  Copyright (C) 2003 SAN People (Pty) Ltd
 *
 * Based on an earlier Atmel EMAC macrocell driver by Atmel and Lineo Inc.
 * Initial version by Rick Bronson 01/11/2003
 *
 * Intel LXT971A PHY support by Christopher Bahns & David Knickerbocker
 *   (Polaroid Corporation)
 *
 * Realtek RTL8201(B)L PHY support by Roman Avramenko <roman@imsystems.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mii.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/dma-mapping.h>
#include <linux/ethtool.h>
#include <linux/platform_data/macb.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/gfp.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>

#include <mach/at91rm9200_emac.h>
#include <asm/gpio.h>
#include <mach/board.h>

#include "at91_ether.h"

#define DRV_NAME	"at91_ether"
#define DRV_VERSION	"1.0"

#define LINK_POLL_INTERVAL	(HZ)


static inline unsigned long at91_emac_read(unsigned int reg)
{
	void __iomem *emac_base = (void __iomem *)AT91_VA_BASE_EMAC;

	return __raw_readl(emac_base + reg);
}

static inline void at91_emac_write(unsigned int reg, unsigned long value)
{
	void __iomem *emac_base = (void __iomem *)AT91_VA_BASE_EMAC;

	__raw_writel(value, emac_base + reg);
}


static void enable_mdi(void)
{
	unsigned long ctl;

	ctl = at91_emac_read(AT91_EMAC_CTL);
	at91_emac_write(AT91_EMAC_CTL, ctl | AT91_EMAC_MPE);	
}

static void disable_mdi(void)
{
	unsigned long ctl;

	ctl = at91_emac_read(AT91_EMAC_CTL);
	at91_emac_write(AT91_EMAC_CTL, ctl & ~AT91_EMAC_MPE);	
}

static inline void at91_phy_wait(void) {
	unsigned long timeout = jiffies + 2;

	while (!(at91_emac_read(AT91_EMAC_SR) & AT91_EMAC_SR_IDLE)) {
		if (time_after(jiffies, timeout)) {
			printk("at91_ether: MIO timeout\n");
			break;
		}
		cpu_relax();
	}
}

static void write_phy(unsigned char phy_addr, unsigned char address, unsigned int value)
{
	at91_emac_write(AT91_EMAC_MAN, AT91_EMAC_MAN_802_3 | AT91_EMAC_RW_W
		| ((phy_addr & 0x1f) << 23) | (address << 18) | (value & AT91_EMAC_DATA));

	
	at91_phy_wait();
}

static void read_phy(unsigned char phy_addr, unsigned char address, unsigned int *value)
{
	at91_emac_write(AT91_EMAC_MAN, AT91_EMAC_MAN_802_3 | AT91_EMAC_RW_R
		| ((phy_addr & 0x1f) << 23) | (address << 18));

	
	at91_phy_wait();

	*value = at91_emac_read(AT91_EMAC_MAN) & AT91_EMAC_DATA;
}


static void update_linkspeed(struct net_device *dev, int silent)
{
	struct at91_private *lp = netdev_priv(dev);
	unsigned int bmsr, bmcr, lpa, mac_cfg;
	unsigned int speed, duplex;

	if (!mii_link_ok(&lp->mii)) {		
		netif_carrier_off(dev);
		if (!silent)
			printk(KERN_INFO "%s: Link down.\n", dev->name);
		return;
	}

	
	read_phy(lp->phy_address, MII_BMSR, &bmsr);
	read_phy(lp->phy_address, MII_BMCR, &bmcr);
	if (bmcr & BMCR_ANENABLE) {				
		if (!(bmsr & BMSR_ANEGCOMPLETE))
			return;			

		read_phy(lp->phy_address, MII_LPA, &lpa);
		if ((lpa & LPA_100FULL) || (lpa & LPA_100HALF)) speed = SPEED_100;
		else speed = SPEED_10;
		if ((lpa & LPA_100FULL) || (lpa & LPA_10FULL)) duplex = DUPLEX_FULL;
		else duplex = DUPLEX_HALF;
	} else {
		speed = (bmcr & BMCR_SPEED100) ? SPEED_100 : SPEED_10;
		duplex = (bmcr & BMCR_FULLDPLX) ? DUPLEX_FULL : DUPLEX_HALF;
	}

	
	mac_cfg = at91_emac_read(AT91_EMAC_CFG) & ~(AT91_EMAC_SPD | AT91_EMAC_FD);
	if (speed == SPEED_100) {
		if (duplex == DUPLEX_FULL)		
			mac_cfg |= AT91_EMAC_SPD | AT91_EMAC_FD;
		else					
			mac_cfg |= AT91_EMAC_SPD;
	} else {
		if (duplex == DUPLEX_FULL)		
			mac_cfg |= AT91_EMAC_FD;
		else {}					
	}
	at91_emac_write(AT91_EMAC_CFG, mac_cfg);

	if (!silent)
		printk(KERN_INFO "%s: Link now %i-%s\n", dev->name, speed, (duplex == DUPLEX_FULL) ? "FullDuplex" : "HalfDuplex");
	netif_carrier_on(dev);
}

static irqreturn_t at91ether_phy_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct at91_private *lp = netdev_priv(dev);
	unsigned int phy;

	enable_mdi();
	if ((lp->phy_type == MII_DM9161_ID) || (lp->phy_type == MII_DM9161A_ID)) {
		read_phy(lp->phy_address, MII_DSINTR_REG, &phy);	
		if (!(phy & (1 << 0)))
			goto done;
	}
	else if (lp->phy_type == MII_LXT971A_ID) {
		read_phy(lp->phy_address, MII_ISINTS_REG, &phy);	
		if (!(phy & (1 << 2)))
			goto done;
	}
	else if (lp->phy_type == MII_BCM5221_ID) {
		read_phy(lp->phy_address, MII_BCMINTR_REG, &phy);	
		if (!(phy & (1 << 0)))
			goto done;
	}
	else if (lp->phy_type == MII_KS8721_ID) {
		read_phy(lp->phy_address, MII_TPISTATUS, &phy);		
		if (!(phy & ((1 << 2) | 1)))
			goto done;
	}
	else if (lp->phy_type == MII_T78Q21x3_ID) {			
		read_phy(lp->phy_address, MII_T78Q21INT_REG, &phy);
		if (!(phy & ((1 << 2) | 1)))
			goto done;
	}
	else if (lp->phy_type == MII_DP83848_ID) {
		read_phy(lp->phy_address, MII_DPPHYSTS_REG, &phy);	
		if (!(phy & (1 << 7)))
			goto done;
	}

	update_linkspeed(dev, 0);

done:
	disable_mdi();

	return IRQ_HANDLED;
}

static void enable_phyirq(struct net_device *dev)
{
	struct at91_private *lp = netdev_priv(dev);
	unsigned int dsintr, irq_number;
	int status;

	if (!gpio_is_valid(lp->board_data.phy_irq_pin)) {
		mod_timer(&lp->check_timer, jiffies + LINK_POLL_INTERVAL);
		return;
	}

	irq_number = lp->board_data.phy_irq_pin;
	status = request_irq(irq_number, at91ether_phy_interrupt, 0, dev->name, dev);
	if (status) {
		printk(KERN_ERR "at91_ether: PHY IRQ %d request failed - status %d!\n", irq_number, status);
		return;
	}

	spin_lock_irq(&lp->lock);
	enable_mdi();

	if ((lp->phy_type == MII_DM9161_ID) || (lp->phy_type == MII_DM9161A_ID)) {	
		read_phy(lp->phy_address, MII_DSINTR_REG, &dsintr);
		dsintr = dsintr & ~0xf00;		
		write_phy(lp->phy_address, MII_DSINTR_REG, dsintr);
	}
	else if (lp->phy_type == MII_LXT971A_ID) {	
		read_phy(lp->phy_address, MII_ISINTE_REG, &dsintr);
		dsintr = dsintr | 0xf2;			
		write_phy(lp->phy_address, MII_ISINTE_REG, dsintr);
	}
	else if (lp->phy_type == MII_BCM5221_ID) {	
		dsintr = (1 << 15) | ( 1 << 14);
		write_phy(lp->phy_address, MII_BCMINTR_REG, dsintr);
	}
	else if (lp->phy_type == MII_KS8721_ID) {	
		dsintr = (1 << 10) | ( 1 << 8);
		write_phy(lp->phy_address, MII_TPISTATUS, dsintr);
	}
	else if (lp->phy_type == MII_T78Q21x3_ID) {	
		read_phy(lp->phy_address, MII_T78Q21INT_REG, &dsintr);
		dsintr = dsintr | 0x500;		
		write_phy(lp->phy_address, MII_T78Q21INT_REG, dsintr);
	}
	else if (lp->phy_type == MII_DP83848_ID) {	
		read_phy(lp->phy_address, MII_DPMISR_REG, &dsintr);
		dsintr = dsintr | 0x3c;			
		write_phy(lp->phy_address, MII_DPMISR_REG, dsintr);
		read_phy(lp->phy_address, MII_DPMICR_REG, &dsintr);
		dsintr = dsintr | 0x3;			
		write_phy(lp->phy_address, MII_DPMICR_REG, dsintr);
	}

	disable_mdi();
	spin_unlock_irq(&lp->lock);
}

static void disable_phyirq(struct net_device *dev)
{
	struct at91_private *lp = netdev_priv(dev);
	unsigned int dsintr;
	unsigned int irq_number;

	if (!gpio_is_valid(lp->board_data.phy_irq_pin)) {
		del_timer_sync(&lp->check_timer);
		return;
	}

	spin_lock_irq(&lp->lock);
	enable_mdi();

	if ((lp->phy_type == MII_DM9161_ID) || (lp->phy_type == MII_DM9161A_ID)) {	
		read_phy(lp->phy_address, MII_DSINTR_REG, &dsintr);
		dsintr = dsintr | 0xf00;			
		write_phy(lp->phy_address, MII_DSINTR_REG, dsintr);
	}
	else if (lp->phy_type == MII_LXT971A_ID) {	
		read_phy(lp->phy_address, MII_ISINTE_REG, &dsintr);
		dsintr = dsintr & ~0xf2;			
		write_phy(lp->phy_address, MII_ISINTE_REG, dsintr);
	}
	else if (lp->phy_type == MII_BCM5221_ID) {	
		read_phy(lp->phy_address, MII_BCMINTR_REG, &dsintr);
		dsintr = ~(1 << 14);
		write_phy(lp->phy_address, MII_BCMINTR_REG, dsintr);
	}
	else if (lp->phy_type == MII_KS8721_ID) {	
		read_phy(lp->phy_address, MII_TPISTATUS, &dsintr);
		dsintr = ~((1 << 10) | (1 << 8));
		write_phy(lp->phy_address, MII_TPISTATUS, dsintr);
	}
	else if (lp->phy_type == MII_T78Q21x3_ID) {	
		read_phy(lp->phy_address, MII_T78Q21INT_REG, &dsintr);
		dsintr = dsintr & ~0x500;			
		write_phy(lp->phy_address, MII_T78Q21INT_REG, dsintr);
	}
	else if (lp->phy_type == MII_DP83848_ID) {	
		read_phy(lp->phy_address, MII_DPMICR_REG, &dsintr);
		dsintr = dsintr & ~0x3;				
		write_phy(lp->phy_address, MII_DPMICR_REG, dsintr);
		read_phy(lp->phy_address, MII_DPMISR_REG, &dsintr);
		dsintr = dsintr & ~0x3c;			
		write_phy(lp->phy_address, MII_DPMISR_REG, dsintr);
	}

	disable_mdi();
	spin_unlock_irq(&lp->lock);

	irq_number = lp->board_data.phy_irq_pin;
	free_irq(irq_number, dev);			
}

#if 0
static void reset_phy(struct net_device *dev)
{
	struct at91_private *lp = netdev_priv(dev);
	unsigned int bmcr;

	spin_lock_irq(&lp->lock);
	enable_mdi();

	
	write_phy(lp->phy_address, MII_BMCR, BMCR_RESET);

	
	do {
		read_phy(lp->phy_address, MII_BMCR, &bmcr);
	} while (!(bmcr & BMCR_RESET));

	disable_mdi();
	spin_unlock_irq(&lp->lock);
}
#endif

static void at91ether_check_link(unsigned long dev_id)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct at91_private *lp = netdev_priv(dev);

	enable_mdi();
	update_linkspeed(dev, 1);
	disable_mdi();

	mod_timer(&lp->check_timer, jiffies + LINK_POLL_INTERVAL);
}



static short __init unpack_mac_address(struct net_device *dev, unsigned int hi, unsigned int lo)
{
	char addr[6];

	if (machine_is_csb337()) {
		addr[5] = (lo & 0xff);			
		addr[4] = (lo & 0xff00) >> 8;
		addr[3] = (lo & 0xff0000) >> 16;
		addr[2] = (lo & 0xff000000) >> 24;
		addr[1] = (hi & 0xff);
		addr[0] = (hi & 0xff00) >> 8;
	}
	else {
		addr[0] = (lo & 0xff);
		addr[1] = (lo & 0xff00) >> 8;
		addr[2] = (lo & 0xff0000) >> 16;
		addr[3] = (lo & 0xff000000) >> 24;
		addr[4] = (hi & 0xff);
		addr[5] = (hi & 0xff00) >> 8;
	}

	if (is_valid_ether_addr(addr)) {
		memcpy(dev->dev_addr, &addr, 6);
		return 1;
	}
	return 0;
}

static void __init get_mac_address(struct net_device *dev)
{
	
	if (unpack_mac_address(dev, at91_emac_read(AT91_EMAC_SA1H), at91_emac_read(AT91_EMAC_SA1L)))
		return;
	
	if (unpack_mac_address(dev, at91_emac_read(AT91_EMAC_SA2H), at91_emac_read(AT91_EMAC_SA2L)))
		return;
	
	if (unpack_mac_address(dev, at91_emac_read(AT91_EMAC_SA3H), at91_emac_read(AT91_EMAC_SA3L)))
		return;
	
	if (unpack_mac_address(dev, at91_emac_read(AT91_EMAC_SA4H), at91_emac_read(AT91_EMAC_SA4L)))
		return;

	printk(KERN_ERR "at91_ether: Your bootloader did not configure a MAC address.\n");
}

static void update_mac_address(struct net_device *dev)
{
	at91_emac_write(AT91_EMAC_SA1L, (dev->dev_addr[3] << 24) | (dev->dev_addr[2] << 16) | (dev->dev_addr[1] << 8) | (dev->dev_addr[0]));
	at91_emac_write(AT91_EMAC_SA1H, (dev->dev_addr[5] << 8) | (dev->dev_addr[4]));

	at91_emac_write(AT91_EMAC_SA2L, 0);
	at91_emac_write(AT91_EMAC_SA2H, 0);
}

static int set_mac_address(struct net_device *dev, void* addr)
{
	struct sockaddr *address = addr;

	if (!is_valid_ether_addr(address->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, address->sa_data, dev->addr_len);
	update_mac_address(dev);

	printk("%s: Setting MAC address to %pM\n", dev->name,
	       dev->dev_addr);

	return 0;
}

static int inline hash_bit_value(int bitnr, __u8 *addr)
{
	if (addr[bitnr / 8] & (1 << (bitnr % 8)))
		return 1;
	return 0;
}


static int hash_get_index(__u8 *addr)
{
	int i, j, bitval;
	int hash_index = 0;

	for (j = 0; j < 6; j++) {
		for (i = 0, bitval = 0; i < 8; i++)
			bitval ^= hash_bit_value(i*6 + j, addr);

		hash_index |= (bitval << j);
	}

	return hash_index;
}

static void at91ether_sethashtable(struct net_device *dev)
{
	struct netdev_hw_addr *ha;
	unsigned long mc_filter[2];
	unsigned int bitnr;

	mc_filter[0] = mc_filter[1] = 0;

	netdev_for_each_mc_addr(ha, dev) {
		bitnr = hash_get_index(ha->addr);
		mc_filter[bitnr >> 5] |= 1 << (bitnr & 31);
	}

	at91_emac_write(AT91_EMAC_HSL, mc_filter[0]);
	at91_emac_write(AT91_EMAC_HSH, mc_filter[1]);
}

static void at91ether_set_multicast_list(struct net_device *dev)
{
	unsigned long cfg;

	cfg = at91_emac_read(AT91_EMAC_CFG);

	if (dev->flags & IFF_PROMISC)			
		cfg |= AT91_EMAC_CAF;
	else if (dev->flags & (~IFF_PROMISC))		
		cfg &= ~AT91_EMAC_CAF;

	if (dev->flags & IFF_ALLMULTI) {		
		at91_emac_write(AT91_EMAC_HSH, -1);
		at91_emac_write(AT91_EMAC_HSL, -1);
		cfg |= AT91_EMAC_MTI;
	} else if (!netdev_mc_empty(dev)) { 
		at91ether_sethashtable(dev);
		cfg |= AT91_EMAC_MTI;
	} else if (dev->flags & (~IFF_ALLMULTI)) {	
		at91_emac_write(AT91_EMAC_HSH, 0);
		at91_emac_write(AT91_EMAC_HSL, 0);
		cfg &= ~AT91_EMAC_MTI;
	}

	at91_emac_write(AT91_EMAC_CFG, cfg);
}


static int mdio_read(struct net_device *dev, int phy_id, int location)
{
	unsigned int value;

	read_phy(phy_id, location, &value);
	return value;
}

static void mdio_write(struct net_device *dev, int phy_id, int location, int value)
{
	write_phy(phy_id, location, value);
}

static int at91ether_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct at91_private *lp = netdev_priv(dev);
	int ret;

	spin_lock_irq(&lp->lock);
	enable_mdi();

	ret = mii_ethtool_gset(&lp->mii, cmd);

	disable_mdi();
	spin_unlock_irq(&lp->lock);

	if (lp->phy_media == PORT_FIBRE) {		
		cmd->supported = SUPPORTED_FIBRE;
		cmd->port = PORT_FIBRE;
	}

	return ret;
}

static int at91ether_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct at91_private *lp = netdev_priv(dev);
	int ret;

	spin_lock_irq(&lp->lock);
	enable_mdi();

	ret = mii_ethtool_sset(&lp->mii, cmd);

	disable_mdi();
	spin_unlock_irq(&lp->lock);

	return ret;
}

static int at91ether_nwayreset(struct net_device *dev)
{
	struct at91_private *lp = netdev_priv(dev);
	int ret;

	spin_lock_irq(&lp->lock);
	enable_mdi();

	ret = mii_nway_restart(&lp->mii);

	disable_mdi();
	spin_unlock_irq(&lp->lock);

	return ret;
}

static void at91ether_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	strlcpy(info->bus_info, dev_name(dev->dev.parent), sizeof(info->bus_info));
}

static const struct ethtool_ops at91ether_ethtool_ops = {
	.get_settings	= at91ether_get_settings,
	.set_settings	= at91ether_set_settings,
	.get_drvinfo	= at91ether_get_drvinfo,
	.nway_reset	= at91ether_nwayreset,
	.get_link	= ethtool_op_get_link,
};

static int at91ether_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct at91_private *lp = netdev_priv(dev);
	int res;

	if (!netif_running(dev))
		return -EINVAL;

	spin_lock_irq(&lp->lock);
	enable_mdi();
	res = generic_mii_ioctl(&lp->mii, if_mii(rq), cmd, NULL);
	disable_mdi();
	spin_unlock_irq(&lp->lock);

	return res;
}


static void at91ether_start(struct net_device *dev)
{
	struct at91_private *lp = netdev_priv(dev);
	struct recv_desc_bufs *dlist, *dlist_phys;
	int i;
	unsigned long ctl;

	dlist = lp->dlist;
	dlist_phys = lp->dlist_phys;

	for (i = 0; i < MAX_RX_DESCR; i++) {
		dlist->descriptors[i].addr = (unsigned int) &dlist_phys->recv_buf[i][0];
		dlist->descriptors[i].size = 0;
	}

	
	dlist->descriptors[i-1].addr |= EMAC_DESC_WRAP;

	
	lp->rxBuffIndex = 0;

	
	at91_emac_write(AT91_EMAC_RBQP, (unsigned long) dlist_phys);

	
	ctl = at91_emac_read(AT91_EMAC_CTL);
	at91_emac_write(AT91_EMAC_CTL, ctl | AT91_EMAC_RE | AT91_EMAC_TE);
}

static int at91ether_open(struct net_device *dev)
{
	struct at91_private *lp = netdev_priv(dev);
	unsigned long ctl;

	if (!is_valid_ether_addr(dev->dev_addr))
		return -EADDRNOTAVAIL;

	clk_enable(lp->ether_clk);		

	
	ctl = at91_emac_read(AT91_EMAC_CTL);
	at91_emac_write(AT91_EMAC_CTL, ctl | AT91_EMAC_CSR);

	
	update_mac_address(dev);

	
	enable_phyirq(dev);

	
	at91_emac_write(AT91_EMAC_IER, AT91_EMAC_RCOM | AT91_EMAC_RBNA
				| AT91_EMAC_TUND | AT91_EMAC_RTRY | AT91_EMAC_TCOM
				| AT91_EMAC_ROVR | AT91_EMAC_ABT);

	
	spin_lock_irq(&lp->lock);
	enable_mdi();
	update_linkspeed(dev, 0);
	disable_mdi();
	spin_unlock_irq(&lp->lock);

	at91ether_start(dev);
	netif_start_queue(dev);
	return 0;
}

static int at91ether_close(struct net_device *dev)
{
	struct at91_private *lp = netdev_priv(dev);
	unsigned long ctl;

	
	ctl = at91_emac_read(AT91_EMAC_CTL);
	at91_emac_write(AT91_EMAC_CTL, ctl & ~(AT91_EMAC_TE | AT91_EMAC_RE));

	
	disable_phyirq(dev);

	
	at91_emac_write(AT91_EMAC_IDR, AT91_EMAC_RCOM | AT91_EMAC_RBNA
				| AT91_EMAC_TUND | AT91_EMAC_RTRY | AT91_EMAC_TCOM
				| AT91_EMAC_ROVR | AT91_EMAC_ABT);

	netif_stop_queue(dev);

	clk_disable(lp->ether_clk);		

	return 0;
}

static int at91ether_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct at91_private *lp = netdev_priv(dev);

	if (at91_emac_read(AT91_EMAC_TSR) & AT91_EMAC_TSR_BNQ) {
		netif_stop_queue(dev);

		
		lp->skb = skb;
		lp->skb_length = skb->len;
		lp->skb_physaddr = dma_map_single(NULL, skb->data, skb->len, DMA_TO_DEVICE);
		dev->stats.tx_bytes += skb->len;

		
		at91_emac_write(AT91_EMAC_TAR, lp->skb_physaddr);
		
		at91_emac_write(AT91_EMAC_TCR, skb->len);

	} else {
		printk(KERN_ERR "at91_ether.c: at91ether_start_xmit() called, but device is busy!\n");
		return NETDEV_TX_BUSY;	
	}

	return NETDEV_TX_OK;
}

static struct net_device_stats *at91ether_stats(struct net_device *dev)
{
	int ale, lenerr, seqe, lcol, ecol;

	if (netif_running(dev)) {
		dev->stats.rx_packets += at91_emac_read(AT91_EMAC_OK);		
		ale = at91_emac_read(AT91_EMAC_ALE);
		dev->stats.rx_frame_errors += ale;				
		lenerr = at91_emac_read(AT91_EMAC_ELR) + at91_emac_read(AT91_EMAC_USF);
		dev->stats.rx_length_errors += lenerr;				
		seqe = at91_emac_read(AT91_EMAC_SEQE);
		dev->stats.rx_crc_errors += seqe;				
		dev->stats.rx_fifo_errors += at91_emac_read(AT91_EMAC_DRFC);	
		dev->stats.rx_errors += (ale + lenerr + seqe
			+ at91_emac_read(AT91_EMAC_CDE) + at91_emac_read(AT91_EMAC_RJB));

		dev->stats.tx_packets += at91_emac_read(AT91_EMAC_FRA);		
		dev->stats.tx_fifo_errors += at91_emac_read(AT91_EMAC_TUE);	
		dev->stats.tx_carrier_errors += at91_emac_read(AT91_EMAC_CSE);	
		dev->stats.tx_heartbeat_errors += at91_emac_read(AT91_EMAC_SQEE);

		lcol = at91_emac_read(AT91_EMAC_LCOL);
		ecol = at91_emac_read(AT91_EMAC_ECOL);
		dev->stats.tx_window_errors += lcol;			
		dev->stats.tx_aborted_errors += ecol;			

		dev->stats.collisions += (at91_emac_read(AT91_EMAC_SCOL) + at91_emac_read(AT91_EMAC_MCOL) + lcol + ecol);
	}
	return &dev->stats;
}

static void at91ether_rx(struct net_device *dev)
{
	struct at91_private *lp = netdev_priv(dev);
	struct recv_desc_bufs *dlist;
	unsigned char *p_recv;
	struct sk_buff *skb;
	unsigned int pktlen;

	dlist = lp->dlist;
	while (dlist->descriptors[lp->rxBuffIndex].addr & EMAC_DESC_DONE) {
		p_recv = dlist->recv_buf[lp->rxBuffIndex];
		pktlen = dlist->descriptors[lp->rxBuffIndex].size & 0x7ff;	
		skb = netdev_alloc_skb(dev, pktlen + 2);
		if (skb != NULL) {
			skb_reserve(skb, 2);
			memcpy(skb_put(skb, pktlen), p_recv, pktlen);

			skb->protocol = eth_type_trans(skb, dev);
			dev->stats.rx_bytes += pktlen;
			netif_rx(skb);
		}
		else {
			dev->stats.rx_dropped += 1;
			printk(KERN_NOTICE "%s: Memory squeeze, dropping packet.\n", dev->name);
		}

		if (dlist->descriptors[lp->rxBuffIndex].size & EMAC_MULTICAST)
			dev->stats.multicast++;

		dlist->descriptors[lp->rxBuffIndex].addr &= ~EMAC_DESC_DONE;	
		if (lp->rxBuffIndex == MAX_RX_DESCR-1)				
			lp->rxBuffIndex = 0;
		else
			lp->rxBuffIndex++;
	}
}

static irqreturn_t at91ether_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct at91_private *lp = netdev_priv(dev);
	unsigned long intstatus, ctl;

	intstatus = at91_emac_read(AT91_EMAC_ISR);

	if (intstatus & AT91_EMAC_RCOM)		
		at91ether_rx(dev);

	if (intstatus & AT91_EMAC_TCOM) {	
		
		if (intstatus & (AT91_EMAC_TUND | AT91_EMAC_RTRY))
			dev->stats.tx_errors += 1;

		if (lp->skb) {
			dev_kfree_skb_irq(lp->skb);
			lp->skb = NULL;
			dma_unmap_single(NULL, lp->skb_physaddr, lp->skb_length, DMA_TO_DEVICE);
		}
		netif_wake_queue(dev);
	}

	
	if (intstatus & AT91_EMAC_RBNA) {
		ctl = at91_emac_read(AT91_EMAC_CTL);
		at91_emac_write(AT91_EMAC_CTL, ctl & ~AT91_EMAC_RE);
		at91_emac_write(AT91_EMAC_CTL, ctl | AT91_EMAC_RE);
	}

	if (intstatus & AT91_EMAC_ROVR)
		printk("%s: ROVR error\n", dev->name);

	return IRQ_HANDLED;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void at91ether_poll_controller(struct net_device *dev)
{
	unsigned long flags;

	local_irq_save(flags);
	at91ether_interrupt(dev->irq, dev);
	local_irq_restore(flags);
}
#endif

static const struct net_device_ops at91ether_netdev_ops = {
	.ndo_open		= at91ether_open,
	.ndo_stop		= at91ether_close,
	.ndo_start_xmit		= at91ether_start_xmit,
	.ndo_get_stats		= at91ether_stats,
	.ndo_set_rx_mode	= at91ether_set_multicast_list,
	.ndo_set_mac_address	= set_mac_address,
	.ndo_do_ioctl		= at91ether_ioctl,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= eth_change_mtu,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= at91ether_poll_controller,
#endif
};

static int __init at91ether_setup(unsigned long phy_type, unsigned short phy_address,
			struct platform_device *pdev, struct clk *ether_clk)
{
	struct macb_platform_data *board_data = pdev->dev.platform_data;
	struct net_device *dev;
	struct at91_private *lp;
	unsigned int val;
	int res;

	dev = alloc_etherdev(sizeof(struct at91_private));
	if (!dev)
		return -ENOMEM;

	dev->base_addr = AT91_VA_BASE_EMAC;
	dev->irq = AT91RM9200_ID_EMAC;

	
	if (request_irq(dev->irq, at91ether_interrupt, 0, dev->name, dev)) {
		free_netdev(dev);
		return -EBUSY;
	}

	
	lp = netdev_priv(dev);
	lp->dlist = (struct recv_desc_bufs *) dma_alloc_coherent(NULL, sizeof(struct recv_desc_bufs), (dma_addr_t *) &lp->dlist_phys, GFP_KERNEL);
	if (lp->dlist == NULL) {
		free_irq(dev->irq, dev);
		free_netdev(dev);
		return -ENOMEM;
	}
	lp->board_data = *board_data;
	lp->ether_clk = ether_clk;
	platform_set_drvdata(pdev, dev);

	spin_lock_init(&lp->lock);

	ether_setup(dev);
	dev->netdev_ops = &at91ether_netdev_ops;
	dev->ethtool_ops = &at91ether_ethtool_ops;

	SET_NETDEV_DEV(dev, &pdev->dev);

	get_mac_address(dev);		
	update_mac_address(dev);	

	at91_emac_write(AT91_EMAC_CTL, 0);

	if (lp->board_data.is_rmii)
		at91_emac_write(AT91_EMAC_CFG, AT91_EMAC_CLK_DIV32 | AT91_EMAC_BIG | AT91_EMAC_RMII);
	else
		at91_emac_write(AT91_EMAC_CFG, AT91_EMAC_CLK_DIV32 | AT91_EMAC_BIG);

	
	spin_lock_irq(&lp->lock);
	enable_mdi();
	if ((phy_type == MII_DM9161_ID) || (lp->phy_type == MII_DM9161A_ID)) {
		read_phy(phy_address, MII_DSCR_REG, &val);
		if ((val & (1 << 10)) == 0)			
			lp->phy_media = PORT_FIBRE;
	} else if (machine_is_csb337()) {
		
		write_phy(phy_address, MII_LEDCTRL_REG, 0x0d22);
	} else if (machine_is_ecbat91())
		write_phy(phy_address, MII_LEDCTRL_REG, 0x156A);

	disable_mdi();
	spin_unlock_irq(&lp->lock);

	lp->mii.dev = dev;		
	lp->mii.mdio_read = mdio_read;
	lp->mii.mdio_write = mdio_write;
	lp->mii.phy_id = phy_address;
	lp->mii.phy_id_mask = 0x1f;
	lp->mii.reg_num_mask = 0x1f;

	lp->phy_type = phy_type;	
	lp->phy_address = phy_address;	

	
	res = register_netdev(dev);
	if (res) {
		free_irq(dev->irq, dev);
		free_netdev(dev);
		dma_free_coherent(NULL, sizeof(struct recv_desc_bufs), lp->dlist, (dma_addr_t)lp->dlist_phys);
		return res;
	}

	
	spin_lock_irq(&lp->lock);
	enable_mdi();
	update_linkspeed(dev, 0);
	disable_mdi();
	spin_unlock_irq(&lp->lock);
	netif_carrier_off(dev);		

	
	if (!gpio_is_valid(lp->board_data.phy_irq_pin)) {
		init_timer(&lp->check_timer);
		lp->check_timer.data = (unsigned long)dev;
		lp->check_timer.function = at91ether_check_link;
	} else if (lp->board_data.phy_irq_pin >= 32)
		gpio_request(lp->board_data.phy_irq_pin, "ethernet_phy");

	
	printk(KERN_INFO "%s: AT91 ethernet at 0x%08x int=%d %s%s (%pM)\n",
	       dev->name, (uint) dev->base_addr, dev->irq,
	       at91_emac_read(AT91_EMAC_CFG) & AT91_EMAC_SPD ? "100-" : "10-",
	       at91_emac_read(AT91_EMAC_CFG) & AT91_EMAC_FD ? "FullDuplex" : "HalfDuplex",
	       dev->dev_addr);
	if ((phy_type == MII_DM9161_ID) || (lp->phy_type == MII_DM9161A_ID))
		printk(KERN_INFO "%s: Davicom 9161 PHY %s\n", dev->name, (lp->phy_media == PORT_FIBRE) ? "(Fiber)" : "(Copper)");
	else if (phy_type == MII_LXT971A_ID)
		printk(KERN_INFO "%s: Intel LXT971A PHY\n", dev->name);
	else if (phy_type == MII_RTL8201_ID)
		printk(KERN_INFO "%s: Realtek RTL8201(B)L PHY\n", dev->name);
	else if (phy_type == MII_BCM5221_ID)
		printk(KERN_INFO "%s: Broadcom BCM5221 PHY\n", dev->name);
	else if (phy_type == MII_DP83847_ID)
		printk(KERN_INFO "%s: National Semiconductor DP83847 PHY\n", dev->name);
	else if (phy_type == MII_DP83848_ID)
		printk(KERN_INFO "%s: National Semiconductor DP83848 PHY\n", dev->name);
	else if (phy_type == MII_AC101L_ID)
		printk(KERN_INFO "%s: Altima AC101L PHY\n", dev->name);
	else if (phy_type == MII_KS8721_ID)
		printk(KERN_INFO "%s: Micrel KS8721 PHY\n", dev->name);
	else if (phy_type == MII_T78Q21x3_ID)
		printk(KERN_INFO "%s: Teridian 78Q21x3 PHY\n", dev->name);
	else if (phy_type == MII_LAN83C185_ID)
		printk(KERN_INFO "%s: SMSC LAN83C185 PHY\n", dev->name);

	return 0;
}

static int __init at91ether_probe(struct platform_device *pdev)
{
	unsigned int phyid1, phyid2;
	int detected = -1;
	unsigned long phy_id;
	unsigned short phy_address = 0;
	struct clk *ether_clk;

	ether_clk = clk_get(&pdev->dev, "ether_clk");
	if (IS_ERR(ether_clk)) {
		printk(KERN_ERR "at91_ether: no clock defined\n");
		return -ENODEV;
	}
	clk_enable(ether_clk);					

	while ((detected != 0) && (phy_address < 32)) {
		
		enable_mdi();
		read_phy(phy_address, MII_PHYSID1, &phyid1);
		read_phy(phy_address, MII_PHYSID2, &phyid2);
		disable_mdi();

		phy_id = (phyid1 << 16) | (phyid2 & 0xfff0);
		switch (phy_id) {
			case MII_DM9161_ID:		
			case MII_DM9161A_ID:		
			case MII_LXT971A_ID:		
			case MII_RTL8201_ID:		
			case MII_BCM5221_ID:		
			case MII_DP83847_ID:		
			case MII_DP83848_ID:		
			case MII_AC101L_ID:		
			case MII_KS8721_ID:		
			case MII_T78Q21x3_ID:		
			case MII_LAN83C185_ID:		
				detected = at91ether_setup(phy_id, phy_address, pdev, ether_clk);
				break;
		}

		phy_address++;
	}

	clk_disable(ether_clk);					

	return detected;
}

static int __devexit at91ether_remove(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct at91_private *lp = netdev_priv(dev);

	if (gpio_is_valid(lp->board_data.phy_irq_pin) &&
	    lp->board_data.phy_irq_pin >= 32)
		gpio_free(lp->board_data.phy_irq_pin);

	unregister_netdev(dev);
	free_irq(dev->irq, dev);
	dma_free_coherent(NULL, sizeof(struct recv_desc_bufs), lp->dlist, (dma_addr_t)lp->dlist_phys);
	clk_put(lp->ether_clk);

	platform_set_drvdata(pdev, NULL);
	free_netdev(dev);
	return 0;
}

#ifdef CONFIG_PM

static int at91ether_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct net_device *net_dev = platform_get_drvdata(pdev);
	struct at91_private *lp = netdev_priv(net_dev);

	if (netif_running(net_dev)) {
		if (gpio_is_valid(lp->board_data.phy_irq_pin)) {
			int phy_irq = lp->board_data.phy_irq_pin;
			disable_irq(phy_irq);
		}

		netif_stop_queue(net_dev);
		netif_device_detach(net_dev);

		clk_disable(lp->ether_clk);
	}
	return 0;
}

static int at91ether_resume(struct platform_device *pdev)
{
	struct net_device *net_dev = platform_get_drvdata(pdev);
	struct at91_private *lp = netdev_priv(net_dev);

	if (netif_running(net_dev)) {
		clk_enable(lp->ether_clk);

		netif_device_attach(net_dev);
		netif_start_queue(net_dev);

		if (gpio_is_valid(lp->board_data.phy_irq_pin)) {
			int phy_irq = lp->board_data.phy_irq_pin;
			enable_irq(phy_irq);
		}
	}
	return 0;
}

#else
#define at91ether_suspend	NULL
#define at91ether_resume	NULL
#endif

static struct platform_driver at91ether_driver = {
	.remove		= __devexit_p(at91ether_remove),
	.suspend	= at91ether_suspend,
	.resume		= at91ether_resume,
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init at91ether_init(void)
{
	return platform_driver_probe(&at91ether_driver, at91ether_probe);
}

static void __exit at91ether_exit(void)
{
	platform_driver_unregister(&at91ether_driver);
}

module_init(at91ether_init)
module_exit(at91ether_exit)

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AT91RM9200 EMAC Ethernet driver");
MODULE_AUTHOR("Andrew Victor");
MODULE_ALIAS("platform:" DRV_NAME);
