/*
 * sonic.c
 *
 * (C) 2005 Finn Thain
 *
 * Converted to DMA API, added zero-copy buffer handling, and
 * (from the mac68k project) introduced dhd's support for 16-bit cards.
 *
 * (C) 1996,1998 by Thomas Bogendoerfer (tsbogend@alpha.franken.de)
 *
 * This driver is based on work from Andreas Busse, but most of
 * the code is rewritten.
 *
 * (C) 1995 by Andreas Busse (andy@waldorf-gmbh.de)
 *
 *    Core code included by system sonic drivers
 *
 * And... partially rewritten again by David Huggins-Daines in order
 * to cope with screwed up Macintosh NICs that may or may not use
 * 16-bit DMA.
 *
 * (C) 1999 David Huggins-Daines <dhd@debian.org>
 *
 */




static int sonic_open(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);
	int i;

	if (sonic_debug > 2)
		printk("sonic_open: initializing sonic driver.\n");

	for (i = 0; i < SONIC_NUM_RRS; i++) {
		struct sk_buff *skb = netdev_alloc_skb(dev, SONIC_RBSIZE + 2);
		if (skb == NULL) {
			while(i > 0) { 
				i--;
				dev_kfree_skb(lp->rx_skb[i]);
				lp->rx_skb[i] = NULL;
			}
			printk(KERN_ERR "%s: couldn't allocate receive buffers\n",
			       dev->name);
			return -ENOMEM;
		}
		
		if (SONIC_BUS_SCALE(lp->dma_bitmode) == 2)
			skb_reserve(skb, 2);
		lp->rx_skb[i] = skb;
	}

	for (i = 0; i < SONIC_NUM_RRS; i++) {
		dma_addr_t laddr = dma_map_single(lp->device, skb_put(lp->rx_skb[i], SONIC_RBSIZE),
		                                  SONIC_RBSIZE, DMA_FROM_DEVICE);
		if (!laddr) {
			while(i > 0) { 
				i--;
				dma_unmap_single(lp->device, lp->rx_laddr[i], SONIC_RBSIZE, DMA_FROM_DEVICE);
				lp->rx_laddr[i] = (dma_addr_t)0;
			}
			for (i = 0; i < SONIC_NUM_RRS; i++) {
				dev_kfree_skb(lp->rx_skb[i]);
				lp->rx_skb[i] = NULL;
			}
			printk(KERN_ERR "%s: couldn't map rx DMA buffers\n",
			       dev->name);
			return -ENOMEM;
		}
		lp->rx_laddr[i] = laddr;
	}

	sonic_init(dev);

	netif_start_queue(dev);

	if (sonic_debug > 2)
		printk("sonic_open: Initialization done.\n");

	return 0;
}


static int sonic_close(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);
	int i;

	if (sonic_debug > 2)
		printk("sonic_close\n");

	netif_stop_queue(dev);

	SONIC_WRITE(SONIC_IMR, 0);
	SONIC_WRITE(SONIC_ISR, 0x7fff);
	SONIC_WRITE(SONIC_CMD, SONIC_CR_RST);

	
	for (i = 0; i < SONIC_NUM_TDS; i++) {
		if(lp->tx_laddr[i]) {
			dma_unmap_single(lp->device, lp->tx_laddr[i], lp->tx_len[i], DMA_TO_DEVICE);
			lp->tx_laddr[i] = (dma_addr_t)0;
		}
		if(lp->tx_skb[i]) {
			dev_kfree_skb(lp->tx_skb[i]);
			lp->tx_skb[i] = NULL;
		}
	}

	
	for (i = 0; i < SONIC_NUM_RRS; i++) {
		if(lp->rx_laddr[i]) {
			dma_unmap_single(lp->device, lp->rx_laddr[i], SONIC_RBSIZE, DMA_FROM_DEVICE);
			lp->rx_laddr[i] = (dma_addr_t)0;
		}
		if(lp->rx_skb[i]) {
			dev_kfree_skb(lp->rx_skb[i]);
			lp->rx_skb[i] = NULL;
		}
	}

	return 0;
}

static void sonic_tx_timeout(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);
	int i;
	SONIC_WRITE(SONIC_IMR, 0);
	SONIC_WRITE(SONIC_ISR, 0x7fff);
	SONIC_WRITE(SONIC_CMD, SONIC_CR_RST);
	
	for (i = 0; i < SONIC_NUM_TDS; i++) {
		if(lp->tx_laddr[i]) {
			dma_unmap_single(lp->device, lp->tx_laddr[i], lp->tx_len[i], DMA_TO_DEVICE);
			lp->tx_laddr[i] = (dma_addr_t)0;
		}
		if(lp->tx_skb[i]) {
			dev_kfree_skb(lp->tx_skb[i]);
			lp->tx_skb[i] = NULL;
		}
	}
	
	sonic_init(dev);
	lp->stats.tx_errors++;
	dev->trans_start = jiffies; 
	netif_wake_queue(dev);
}


static int sonic_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);
	dma_addr_t laddr;
	int length;
	int entry = lp->next_tx;

	if (sonic_debug > 2)
		printk("sonic_send_packet: skb=%p, dev=%p\n", skb, dev);

	length = skb->len;
	if (length < ETH_ZLEN) {
		if (skb_padto(skb, ETH_ZLEN))
			return NETDEV_TX_OK;
		length = ETH_ZLEN;
	}


	laddr = dma_map_single(lp->device, skb->data, length, DMA_TO_DEVICE);
	if (!laddr) {
		printk(KERN_ERR "%s: failed to map tx DMA buffer.\n", dev->name);
		dev_kfree_skb(skb);
		return NETDEV_TX_BUSY;
	}

	sonic_tda_put(dev, entry, SONIC_TD_STATUS, 0);       
	sonic_tda_put(dev, entry, SONIC_TD_FRAG_COUNT, 1);   
	sonic_tda_put(dev, entry, SONIC_TD_PKTSIZE, length); 
	sonic_tda_put(dev, entry, SONIC_TD_FRAG_PTR_L, laddr & 0xffff);
	sonic_tda_put(dev, entry, SONIC_TD_FRAG_PTR_H, laddr >> 16);
	sonic_tda_put(dev, entry, SONIC_TD_FRAG_SIZE, length);
	sonic_tda_put(dev, entry, SONIC_TD_LINK,
		sonic_tda_get(dev, entry, SONIC_TD_LINK) | SONIC_EOL);

	wmb();
	lp->tx_len[entry] = length;
	lp->tx_laddr[entry] = laddr;
	lp->tx_skb[entry] = skb;

	wmb();
	sonic_tda_put(dev, lp->eol_tx, SONIC_TD_LINK,
				  sonic_tda_get(dev, lp->eol_tx, SONIC_TD_LINK) & ~SONIC_EOL);
	lp->eol_tx = entry;

	lp->next_tx = (entry + 1) & SONIC_TDS_MASK;
	if (lp->tx_skb[lp->next_tx] != NULL) {
		
		if (sonic_debug > 3)
			printk("%s: stopping queue\n", dev->name);
		netif_stop_queue(dev);
		
	} else netif_start_queue(dev);

	if (sonic_debug > 2)
		printk("sonic_send_packet: issuing Tx command\n");

	SONIC_WRITE(SONIC_CMD, SONIC_CR_TXP);

	return NETDEV_TX_OK;
}

static irqreturn_t sonic_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct sonic_local *lp = netdev_priv(dev);
	int status;

	if (!(status = SONIC_READ(SONIC_ISR) & SONIC_IMR_DEFAULT))
		return IRQ_NONE;

	do {
		if (status & SONIC_INT_PKTRX) {
			if (sonic_debug > 2)
				printk("%s: packet rx\n", dev->name);
			sonic_rx(dev);	
			SONIC_WRITE(SONIC_ISR, SONIC_INT_PKTRX); 
		}

		if (status & SONIC_INT_TXDN) {
			int entry = lp->cur_tx;
			int td_status;
			int freed_some = 0;


			if (sonic_debug > 2)
				printk("%s: tx done\n", dev->name);

			while (lp->tx_skb[entry] != NULL) {
				if ((td_status = sonic_tda_get(dev, entry, SONIC_TD_STATUS)) == 0)
					break;

				if (td_status & 0x0001) {
					lp->stats.tx_packets++;
					lp->stats.tx_bytes += sonic_tda_get(dev, entry, SONIC_TD_PKTSIZE);
				} else {
					lp->stats.tx_errors++;
					if (td_status & 0x0642)
						lp->stats.tx_aborted_errors++;
					if (td_status & 0x0180)
						lp->stats.tx_carrier_errors++;
					if (td_status & 0x0020)
						lp->stats.tx_window_errors++;
					if (td_status & 0x0004)
						lp->stats.tx_fifo_errors++;
				}

				
				dev_kfree_skb_irq(lp->tx_skb[entry]);
				lp->tx_skb[entry] = NULL;
				
				dma_unmap_single(lp->device, lp->tx_laddr[entry], lp->tx_len[entry], DMA_TO_DEVICE);
				lp->tx_laddr[entry] = (dma_addr_t)0;
				freed_some = 1;

				if (sonic_tda_get(dev, entry, SONIC_TD_LINK) & SONIC_EOL) {
					entry = (entry + 1) & SONIC_TDS_MASK;
					break;
				}
				entry = (entry + 1) & SONIC_TDS_MASK;
			}

			if (freed_some || lp->tx_skb[entry] == NULL)
				netif_wake_queue(dev);  
			lp->cur_tx = entry;
			SONIC_WRITE(SONIC_ISR, SONIC_INT_TXDN); 
		}

		if (status & SONIC_INT_RFO) {
			if (sonic_debug > 1)
				printk("%s: rx fifo overrun\n", dev->name);
			lp->stats.rx_fifo_errors++;
			SONIC_WRITE(SONIC_ISR, SONIC_INT_RFO); 
		}
		if (status & SONIC_INT_RDE) {
			if (sonic_debug > 1)
				printk("%s: rx descriptors exhausted\n", dev->name);
			lp->stats.rx_dropped++;
			SONIC_WRITE(SONIC_ISR, SONIC_INT_RDE); 
		}
		if (status & SONIC_INT_RBAE) {
			if (sonic_debug > 1)
				printk("%s: rx buffer area exceeded\n", dev->name);
			lp->stats.rx_dropped++;
			SONIC_WRITE(SONIC_ISR, SONIC_INT_RBAE); 
		}

		
		if (status & SONIC_INT_FAE) {
			lp->stats.rx_frame_errors += 65536;
			SONIC_WRITE(SONIC_ISR, SONIC_INT_FAE); 
		}
		if (status & SONIC_INT_CRC) {
			lp->stats.rx_crc_errors += 65536;
			SONIC_WRITE(SONIC_ISR, SONIC_INT_CRC); 
		}
		if (status & SONIC_INT_MP) {
			lp->stats.rx_missed_errors += 65536;
			SONIC_WRITE(SONIC_ISR, SONIC_INT_MP); 
		}

		
		if (status & SONIC_INT_TXER) {
			if ((SONIC_READ(SONIC_TCR) & SONIC_TCR_FU) && (sonic_debug > 2))
				printk(KERN_ERR "%s: tx fifo underrun\n", dev->name);
			SONIC_WRITE(SONIC_ISR, SONIC_INT_TXER); 
		}

		
		if (status & SONIC_INT_BR) {
			printk(KERN_ERR "%s: Bus retry occurred! Device interrupt disabled.\n",
				dev->name);
			
			
			SONIC_WRITE(SONIC_IMR, 0);
			SONIC_WRITE(SONIC_ISR, SONIC_INT_BR); 
		}

		
		if (status & SONIC_INT_LCD)
			SONIC_WRITE(SONIC_ISR, SONIC_INT_LCD); 
	} while((status = SONIC_READ(SONIC_ISR) & SONIC_IMR_DEFAULT));
	return IRQ_HANDLED;
}

static void sonic_rx(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);
	int status;
	int entry = lp->cur_rx;

	while (sonic_rda_get(dev, entry, SONIC_RD_IN_USE) == 0) {
		struct sk_buff *used_skb;
		struct sk_buff *new_skb;
		dma_addr_t new_laddr;
		u16 bufadr_l;
		u16 bufadr_h;
		int pkt_len;

		status = sonic_rda_get(dev, entry, SONIC_RD_STATUS);
		if (status & SONIC_RCR_PRX) {
			
			new_skb = netdev_alloc_skb(dev, SONIC_RBSIZE + 2);
			if (new_skb == NULL) {
				printk(KERN_ERR "%s: Memory squeeze, dropping packet.\n", dev->name);
				lp->stats.rx_dropped++;
				break;
			}
			
			if(SONIC_BUS_SCALE(lp->dma_bitmode) == 2)
				skb_reserve(new_skb, 2);

			new_laddr = dma_map_single(lp->device, skb_put(new_skb, SONIC_RBSIZE),
		                               SONIC_RBSIZE, DMA_FROM_DEVICE);
			if (!new_laddr) {
				dev_kfree_skb(new_skb);
				printk(KERN_ERR "%s: Failed to map rx buffer, dropping packet.\n", dev->name);
				lp->stats.rx_dropped++;
				break;
			}

			
			dma_unmap_single(lp->device, lp->rx_laddr[entry], SONIC_RBSIZE, DMA_FROM_DEVICE);
			used_skb = lp->rx_skb[entry];
			pkt_len = sonic_rda_get(dev, entry, SONIC_RD_PKTLEN);
			skb_trim(used_skb, pkt_len);
			used_skb->protocol = eth_type_trans(used_skb, dev);
			netif_rx(used_skb);
			lp->stats.rx_packets++;
			lp->stats.rx_bytes += pkt_len;

			
			lp->rx_laddr[entry] = new_laddr;
			lp->rx_skb[entry] = new_skb;

			bufadr_l = (unsigned long)new_laddr & 0xffff;
			bufadr_h = (unsigned long)new_laddr >> 16;
			sonic_rra_put(dev, entry, SONIC_RR_BUFADR_L, bufadr_l);
			sonic_rra_put(dev, entry, SONIC_RR_BUFADR_H, bufadr_h);
		} else {
			
			lp->stats.rx_errors++;
			if (status & SONIC_RCR_FAER)
				lp->stats.rx_frame_errors++;
			if (status & SONIC_RCR_CRCR)
				lp->stats.rx_crc_errors++;
		}
		if (status & SONIC_RCR_LPKT) {
			lp->cur_rwp += SIZEOF_SONIC_RR * SONIC_BUS_SCALE(lp->dma_bitmode);
			if (lp->cur_rwp >= lp->rra_end) lp->cur_rwp = lp->rra_laddr & 0xffff;
			SONIC_WRITE(SONIC_RWP, lp->cur_rwp);
			if (SONIC_READ(SONIC_ISR) & SONIC_INT_RBE) {
				if (sonic_debug > 2)
					printk("%s: rx buffer exhausted\n", dev->name);
				SONIC_WRITE(SONIC_ISR, SONIC_INT_RBE); 
			}
		} else
			printk(KERN_ERR "%s: rx desc without RCR_LPKT. Shouldn't happen !?\n",
			     dev->name);
		sonic_rda_put(dev, entry, SONIC_RD_LINK,
			sonic_rda_get(dev, entry, SONIC_RD_LINK) | SONIC_EOL);
		sonic_rda_put(dev, entry, SONIC_RD_IN_USE, 1);
		sonic_rda_put(dev, lp->eol_rx, SONIC_RD_LINK,
			sonic_rda_get(dev, lp->eol_rx, SONIC_RD_LINK) & ~SONIC_EOL);
		lp->eol_rx = entry;
		lp->cur_rx = entry = (entry + 1) & SONIC_RDS_MASK;
	}
}


static struct net_device_stats *sonic_get_stats(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);

	
	lp->stats.rx_crc_errors += SONIC_READ(SONIC_CRCT);
	SONIC_WRITE(SONIC_CRCT, 0xffff);
	lp->stats.rx_frame_errors += SONIC_READ(SONIC_FAET);
	SONIC_WRITE(SONIC_FAET, 0xffff);
	lp->stats.rx_missed_errors += SONIC_READ(SONIC_MPT);
	SONIC_WRITE(SONIC_MPT, 0xffff);

	return &lp->stats;
}


static void sonic_multicast_list(struct net_device *dev)
{
	struct sonic_local *lp = netdev_priv(dev);
	unsigned int rcr;
	struct netdev_hw_addr *ha;
	unsigned char *addr;
	int i;

	rcr = SONIC_READ(SONIC_RCR) & ~(SONIC_RCR_PRO | SONIC_RCR_AMC);
	rcr |= SONIC_RCR_BRD;	

	if (dev->flags & IFF_PROMISC) {	
		rcr |= SONIC_RCR_PRO;
	} else {
		if ((dev->flags & IFF_ALLMULTI) ||
		    (netdev_mc_count(dev) > 15)) {
			rcr |= SONIC_RCR_AMC;
		} else {
			if (sonic_debug > 2)
				printk("sonic_multicast_list: mc_count %d\n",
				       netdev_mc_count(dev));
			sonic_set_cam_enable(dev, 1);  
			i = 1;
			netdev_for_each_mc_addr(ha, dev) {
				addr = ha->addr;
				sonic_cda_put(dev, i, SONIC_CD_CAP0, addr[1] << 8 | addr[0]);
				sonic_cda_put(dev, i, SONIC_CD_CAP1, addr[3] << 8 | addr[2]);
				sonic_cda_put(dev, i, SONIC_CD_CAP2, addr[5] << 8 | addr[4]);
				sonic_set_cam_enable(dev, sonic_get_cam_enable(dev) | (1 << i));
				i++;
			}
			SONIC_WRITE(SONIC_CDC, 16);
			
			SONIC_WRITE(SONIC_CDP, lp->cda_laddr & 0xffff);
			SONIC_WRITE(SONIC_CMD, SONIC_CR_LCAM);
		}
	}

	if (sonic_debug > 2)
		printk("sonic_multicast_list: setting RCR=%x\n", rcr);

	SONIC_WRITE(SONIC_RCR, rcr);
}


static int sonic_init(struct net_device *dev)
{
	unsigned int cmd;
	struct sonic_local *lp = netdev_priv(dev);
	int i;

	SONIC_WRITE(SONIC_IMR, 0);
	SONIC_WRITE(SONIC_ISR, 0x7fff);
	SONIC_WRITE(SONIC_CMD, SONIC_CR_RST);

	SONIC_WRITE(SONIC_CMD, 0);
	SONIC_WRITE(SONIC_CMD, SONIC_CR_RXDIS);

	if (sonic_debug > 2)
		printk("sonic_init: initialize receive resource area\n");

	for (i = 0; i < SONIC_NUM_RRS; i++) {
		u16 bufadr_l = (unsigned long)lp->rx_laddr[i] & 0xffff;
		u16 bufadr_h = (unsigned long)lp->rx_laddr[i] >> 16;
		sonic_rra_put(dev, i, SONIC_RR_BUFADR_L, bufadr_l);
		sonic_rra_put(dev, i, SONIC_RR_BUFADR_H, bufadr_h);
		sonic_rra_put(dev, i, SONIC_RR_BUFSIZE_L, SONIC_RBSIZE >> 1);
		sonic_rra_put(dev, i, SONIC_RR_BUFSIZE_H, 0);
	}

	
	lp->rra_end = (lp->rra_laddr + SONIC_NUM_RRS * SIZEOF_SONIC_RR *
					SONIC_BUS_SCALE(lp->dma_bitmode)) & 0xffff;
	lp->cur_rwp = (lp->rra_laddr + (SONIC_NUM_RRS - 1) * SIZEOF_SONIC_RR *
					SONIC_BUS_SCALE(lp->dma_bitmode)) & 0xffff;

	SONIC_WRITE(SONIC_RSA, lp->rra_laddr & 0xffff);
	SONIC_WRITE(SONIC_REA, lp->rra_end);
	SONIC_WRITE(SONIC_RRP, lp->rra_laddr & 0xffff);
	SONIC_WRITE(SONIC_RWP, lp->cur_rwp);
	SONIC_WRITE(SONIC_URRA, lp->rra_laddr >> 16);
	SONIC_WRITE(SONIC_EOBC, (SONIC_RBSIZE >> 1) - (lp->dma_bitmode ? 2 : 1));

	
	if (sonic_debug > 3)
		printk("sonic_init: issuing RRRA command\n");

	SONIC_WRITE(SONIC_CMD, SONIC_CR_RRRA);
	i = 0;
	while (i++ < 100) {
		if (SONIC_READ(SONIC_CMD) & SONIC_CR_RRRA)
			break;
	}

	if (sonic_debug > 2)
		printk("sonic_init: status=%x i=%d\n", SONIC_READ(SONIC_CMD), i);

	if (sonic_debug > 2)
		printk("sonic_init: initialize receive descriptors\n");
	for (i=0; i<SONIC_NUM_RDS; i++) {
		sonic_rda_put(dev, i, SONIC_RD_STATUS, 0);
		sonic_rda_put(dev, i, SONIC_RD_PKTLEN, 0);
		sonic_rda_put(dev, i, SONIC_RD_PKTPTR_L, 0);
		sonic_rda_put(dev, i, SONIC_RD_PKTPTR_H, 0);
		sonic_rda_put(dev, i, SONIC_RD_SEQNO, 0);
		sonic_rda_put(dev, i, SONIC_RD_IN_USE, 1);
		sonic_rda_put(dev, i, SONIC_RD_LINK,
			lp->rda_laddr +
			((i+1) * SIZEOF_SONIC_RD * SONIC_BUS_SCALE(lp->dma_bitmode)));
	}
	
	sonic_rda_put(dev, SONIC_NUM_RDS - 1, SONIC_RD_LINK,
		(lp->rda_laddr & 0xffff) | SONIC_EOL);
	lp->eol_rx = SONIC_NUM_RDS - 1;
	lp->cur_rx = 0;
	SONIC_WRITE(SONIC_URDA, lp->rda_laddr >> 16);
	SONIC_WRITE(SONIC_CRDA, lp->rda_laddr & 0xffff);

	if (sonic_debug > 2)
		printk("sonic_init: initialize transmit descriptors\n");
	for (i = 0; i < SONIC_NUM_TDS; i++) {
		sonic_tda_put(dev, i, SONIC_TD_STATUS, 0);
		sonic_tda_put(dev, i, SONIC_TD_CONFIG, 0);
		sonic_tda_put(dev, i, SONIC_TD_PKTSIZE, 0);
		sonic_tda_put(dev, i, SONIC_TD_FRAG_COUNT, 0);
		sonic_tda_put(dev, i, SONIC_TD_LINK,
			(lp->tda_laddr & 0xffff) +
			(i + 1) * SIZEOF_SONIC_TD * SONIC_BUS_SCALE(lp->dma_bitmode));
		lp->tx_skb[i] = NULL;
	}
	
	sonic_tda_put(dev, SONIC_NUM_TDS - 1, SONIC_TD_LINK,
		(lp->tda_laddr & 0xffff));

	SONIC_WRITE(SONIC_UTDA, lp->tda_laddr >> 16);
	SONIC_WRITE(SONIC_CTDA, lp->tda_laddr & 0xffff);
	lp->cur_tx = lp->next_tx = 0;
	lp->eol_tx = SONIC_NUM_TDS - 1;

	sonic_cda_put(dev, 0, SONIC_CD_CAP0, dev->dev_addr[1] << 8 | dev->dev_addr[0]);
	sonic_cda_put(dev, 0, SONIC_CD_CAP1, dev->dev_addr[3] << 8 | dev->dev_addr[2]);
	sonic_cda_put(dev, 0, SONIC_CD_CAP2, dev->dev_addr[5] << 8 | dev->dev_addr[4]);
	sonic_set_cam_enable(dev, 1);

	for (i = 0; i < 16; i++)
		sonic_cda_put(dev, i, SONIC_CD_ENTRY_POINTER, i);

	SONIC_WRITE(SONIC_CDP, lp->cda_laddr & 0xffff);
	SONIC_WRITE(SONIC_CDC, 16);

	SONIC_WRITE(SONIC_CMD, SONIC_CR_LCAM);

	i = 0;
	while (i++ < 100) {
		if (SONIC_READ(SONIC_ISR) & SONIC_INT_LCD)
			break;
	}
	if (sonic_debug > 2) {
		printk("sonic_init: CMD=%x, ISR=%x\n, i=%d",
		       SONIC_READ(SONIC_CMD), SONIC_READ(SONIC_ISR), i);
	}

	SONIC_WRITE(SONIC_CMD, SONIC_CR_RXEN | SONIC_CR_STP);
	SONIC_WRITE(SONIC_RCR, SONIC_RCR_DEFAULT);
	SONIC_WRITE(SONIC_TCR, SONIC_TCR_DEFAULT);
	SONIC_WRITE(SONIC_ISR, 0x7fff);
	SONIC_WRITE(SONIC_IMR, SONIC_IMR_DEFAULT);

	cmd = SONIC_READ(SONIC_CMD);
	if ((cmd & SONIC_CR_RXEN) == 0 || (cmd & SONIC_CR_STP) == 0)
		printk(KERN_ERR "sonic_init: failed, status=%x\n", cmd);

	if (sonic_debug > 2)
		printk("sonic_init: new status=%x\n",
		       SONIC_READ(SONIC_CMD));

	return 0;
}

MODULE_LICENSE("GPL");
