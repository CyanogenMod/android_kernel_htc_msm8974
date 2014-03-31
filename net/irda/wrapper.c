/*********************************************************************
 *
 * Filename:      wrapper.c
 * Version:       1.2
 * Description:   IrDA SIR async wrapper layer
 * Status:        Stable
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Mon Aug  4 20:40:53 1997
 * Modified at:   Fri Jan 28 13:21:09 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * Modified at:   Fri May 28  3:11 CST 1999
 * Modified by:   Horst von Brand <vonbrand@sleipnir.valparaiso.cl>
 *
 *     Copyright (c) 1998-2000 Dag Brattli <dagb@cs.uit.no>,
 *     All Rights Reserved.
 *     Copyright (c) 2000-2002 Jean Tourrilhes <jt@hpl.hp.com>
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is
 *     provided "AS-IS" and at no charge.
 *
 ********************************************************************/

#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/module.h>
#include <asm/byteorder.h>

#include <net/irda/irda.h>
#include <net/irda/wrapper.h>
#include <net/irda/crc.h>
#include <net/irda/irlap.h>
#include <net/irda/irlap_frame.h>
#include <net/irda/irda_device.h>


static inline int stuff_byte(__u8 byte, __u8 *buf)
{
	switch (byte) {
	case BOF: 
	case EOF: 
	case CE:
		
		buf[0] = CE;               
		buf[1] = byte^IRDA_TRANS;    
		return 2;
		
	default:
		 
		buf[0] = byte;
		return 1;
		
	}
}

int async_wrap_skb(struct sk_buff *skb, __u8 *tx_buff, int buffsize)
{
	struct irda_skb_cb *cb = (struct irda_skb_cb *) skb->cb;
	int xbofs;
	int i;
	int n;
	union {
		__u16 value;
		__u8 bytes[2];
	} fcs;

	
	fcs.value = INIT_FCS;
	n = 0;


	if (cb->magic != LAP_MAGIC) {
		IRDA_DEBUG(1, "%s(), wrong magic in skb!\n", __func__);
		xbofs = 10;
	} else
		xbofs = cb->xbofs + cb->xbofs_delay;

	IRDA_DEBUG(4, "%s(), xbofs=%d\n", __func__, xbofs);

	
	if (xbofs > 163) {
		IRDA_DEBUG(0, "%s(), too many xbofs (%d)\n", __func__,
			   xbofs);
		xbofs = 163;
	}

	memset(tx_buff + n, XBOF, xbofs);
	n += xbofs;

	
	tx_buff[n++] = BOF;

	
	for (i=0; i < skb->len; i++) {
		if(n >= (buffsize-5)) {
			IRDA_ERROR("%s(), tx buffer overflow (n=%d)\n",
				   __func__, n);
			return n;
		}

		n += stuff_byte(skb->data[i], tx_buff+n);
		fcs.value = irda_fcs(fcs.value, skb->data[i]);
	}

	
	fcs.value = ~fcs.value;
#ifdef __LITTLE_ENDIAN
	n += stuff_byte(fcs.bytes[0], tx_buff+n);
	n += stuff_byte(fcs.bytes[1], tx_buff+n);
#else 
	n += stuff_byte(fcs.bytes[1], tx_buff+n);
	n += stuff_byte(fcs.bytes[0], tx_buff+n);
#endif
	tx_buff[n++] = EOF;

	return n;
}
EXPORT_SYMBOL(async_wrap_skb);



static inline void
async_bump(struct net_device *dev,
	   struct net_device_stats *stats,
	   iobuff_t *rx_buff)
{
	struct sk_buff *newskb;
	struct sk_buff *dataskb;
	int		docopy;

	docopy = ((rx_buff->skb == NULL) ||
		  (rx_buff->len < IRDA_RX_COPY_THRESHOLD));

	
	newskb = dev_alloc_skb(docopy ? rx_buff->len + 1 : rx_buff->truesize);
	if (!newskb)  {
		stats->rx_dropped++;
		return;
	}

	skb_reserve(newskb, 1);

	if(docopy) {
		
		skb_copy_to_linear_data(newskb, rx_buff->data,
					rx_buff->len - 2);
		
		dataskb = newskb;
	} else {
		
		dataskb = rx_buff->skb;
		
		rx_buff->skb = newskb;
		rx_buff->head = newskb->data;	
		
	}

	
	skb_put(dataskb, rx_buff->len - 2);

	
	dataskb->dev = dev;
	skb_reset_mac_header(dataskb);
	dataskb->protocol = htons(ETH_P_IRDA);

	netif_rx(dataskb);

	stats->rx_packets++;
	stats->rx_bytes += rx_buff->len;

	
	rx_buff->data = rx_buff->head;
	rx_buff->len = 0;
}

static inline void
async_unwrap_bof(struct net_device *dev,
		 struct net_device_stats *stats,
		 iobuff_t *rx_buff, __u8 byte)
{
	switch(rx_buff->state) {
	case LINK_ESCAPE:
	case INSIDE_FRAME:
		IRDA_DEBUG(1, "%s(), Discarding incomplete frame\n",
			   __func__);
		stats->rx_errors++;
		stats->rx_missed_errors++;
		irda_device_set_media_busy(dev, TRUE);
		break;

	case OUTSIDE_FRAME:
	case BEGIN_FRAME:
	default:
		
		break;
	}

	
	rx_buff->state = BEGIN_FRAME;
	rx_buff->in_frame = TRUE;

	
	rx_buff->data = rx_buff->head;
	rx_buff->len = 0;
	rx_buff->fcs = INIT_FCS;
}

static inline void
async_unwrap_eof(struct net_device *dev,
		 struct net_device_stats *stats,
		 iobuff_t *rx_buff, __u8 byte)
{
#ifdef POSTPONE_RX_CRC
	int	i;
#endif

	switch(rx_buff->state) {
	case OUTSIDE_FRAME:
		
		stats->rx_errors++;
		stats->rx_missed_errors++;
		irda_device_set_media_busy(dev, TRUE);
		break;

	case BEGIN_FRAME:
	case LINK_ESCAPE:
	case INSIDE_FRAME:
	default:
		rx_buff->state = OUTSIDE_FRAME;
		rx_buff->in_frame = FALSE;

#ifdef POSTPONE_RX_CRC
		for(i = 0; i < rx_buff->len; i++)
			rx_buff->fcs = irda_fcs(rx_buff->fcs,
						rx_buff->data[i]);
#endif

		
		if (rx_buff->fcs == GOOD_FCS) {
			
			async_bump(dev, stats, rx_buff);
			break;
		} else {
			
			irda_device_set_media_busy(dev, TRUE);

			IRDA_DEBUG(1, "%s(), crc error\n", __func__);
			stats->rx_errors++;
			stats->rx_crc_errors++;
		}
		break;
	}
}

static inline void
async_unwrap_ce(struct net_device *dev,
		 struct net_device_stats *stats,
		 iobuff_t *rx_buff, __u8 byte)
{
	switch(rx_buff->state) {
	case OUTSIDE_FRAME:
		
		irda_device_set_media_busy(dev, TRUE);
		break;

	case LINK_ESCAPE:
		IRDA_WARNING("%s: state not defined\n", __func__);
		break;

	case BEGIN_FRAME:
	case INSIDE_FRAME:
	default:
		
		rx_buff->state = LINK_ESCAPE;
		break;
	}
}

static inline void
async_unwrap_other(struct net_device *dev,
		   struct net_device_stats *stats,
		   iobuff_t *rx_buff, __u8 byte)
{
	switch(rx_buff->state) {
	case INSIDE_FRAME:
		
		if (rx_buff->len < rx_buff->truesize)  {
			rx_buff->data[rx_buff->len++] = byte;
#ifndef POSTPONE_RX_CRC
			rx_buff->fcs = irda_fcs(rx_buff->fcs, byte);
#endif
		} else {
			IRDA_DEBUG(1, "%s(), Rx buffer overflow, aborting\n",
				   __func__);
			rx_buff->state = OUTSIDE_FRAME;
		}
		break;

	case LINK_ESCAPE:
		byte ^= IRDA_TRANS;
		if (rx_buff->len < rx_buff->truesize)  {
			rx_buff->data[rx_buff->len++] = byte;
#ifndef POSTPONE_RX_CRC
			rx_buff->fcs = irda_fcs(rx_buff->fcs, byte);
#endif
			rx_buff->state = INSIDE_FRAME;
		} else {
			IRDA_DEBUG(1, "%s(), Rx buffer overflow, aborting\n",
				   __func__);
			rx_buff->state = OUTSIDE_FRAME;
		}
		break;

	case OUTSIDE_FRAME:
		
		if(byte != XBOF)
			irda_device_set_media_busy(dev, TRUE);
		break;

	case BEGIN_FRAME:
	default:
		rx_buff->data[rx_buff->len++] = byte;
#ifndef POSTPONE_RX_CRC
		rx_buff->fcs = irda_fcs(rx_buff->fcs, byte);
#endif
		rx_buff->state = INSIDE_FRAME;
		break;
	}
}

void async_unwrap_char(struct net_device *dev,
		       struct net_device_stats *stats,
		       iobuff_t *rx_buff, __u8 byte)
{
	switch(byte) {
	case CE:
		async_unwrap_ce(dev, stats, rx_buff, byte);
		break;
	case BOF:
		async_unwrap_bof(dev, stats, rx_buff, byte);
		break;
	case EOF:
		async_unwrap_eof(dev, stats, rx_buff, byte);
		break;
	default:
		async_unwrap_other(dev, stats, rx_buff, byte);
		break;
	}
}
EXPORT_SYMBOL(async_unwrap_char);

