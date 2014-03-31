/* $Id: hysdn_sched.c,v 1.5.6.4 2001/11/06 21:58:19 kai Exp $
 *
 * Linux driver for HYSDN cards
 * scheduler routines for handling exchange card <-> pc.
 *
 * Author    Werner Cornelius (werner@titro.de) for Hypercope GmbH
 * Copyright 1999 by Werner Cornelius (werner@titro.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/signal.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "hysdn_defs.h"

int
hysdn_sched_rx(hysdn_card *card, unsigned char *buf, unsigned short len,
	       unsigned short chan)
{

	switch (chan) {
	case CHAN_NDIS_DATA:
		if (hynet_enable & (1 << card->myid)) {
			
			hysdn_rx_netpkt(card, buf, len);
		}
		break;

	case CHAN_ERRLOG:
		hysdn_card_errlog(card, (tErrLogEntry *) buf, len);
		if (card->err_log_state == ERRLOG_STATE_ON)
			card->err_log_state = ERRLOG_STATE_START;	
		break;
#ifdef CONFIG_HYSDN_CAPI
	case CHAN_CAPI:
		if (hycapi_enable & (1 << card->myid)) {
			hycapi_rx_capipkt(card, buf, len);
		}
		break;
#endif 
	default:
		printk(KERN_INFO "irq message channel %d len %d unhandled \n", chan, len);
		break;

	}			

	return (1);		
}				

int
hysdn_sched_tx(hysdn_card *card, unsigned char *buf,
	       unsigned short volatile *len, unsigned short volatile *chan,
	       unsigned short maxlen)
{
	struct sk_buff *skb;

	if (card->net_tx_busy) {
		card->net_tx_busy = 0;	
		hysdn_tx_netack(card);	
	}			
	
	if (card->async_busy) {
		if (card->async_len <= maxlen) {
			memcpy(buf, card->async_data, card->async_len);
			*len = card->async_len;
			*chan = card->async_channel;
			card->async_busy = 0;	
			return (1);
		}
		card->async_busy = 0;	
	}			
	if ((card->err_log_state == ERRLOG_STATE_START) &&
	    (maxlen >= ERRLOG_CMD_REQ_SIZE)) {
		strcpy(buf, ERRLOG_CMD_REQ);	
		*len = ERRLOG_CMD_REQ_SIZE;	
		*chan = CHAN_ERRLOG;	
		card->err_log_state = ERRLOG_STATE_ON;	
		return (1);	
	}			
	if ((card->err_log_state == ERRLOG_STATE_STOP) &&
	    (maxlen >= ERRLOG_CMD_STOP_SIZE)) {
		strcpy(buf, ERRLOG_CMD_STOP);	
		*len = ERRLOG_CMD_STOP_SIZE;	
		*chan = CHAN_ERRLOG;	
		card->err_log_state = ERRLOG_STATE_OFF;		
		return (1);	
	}			
	
	if ((hynet_enable & (1 << card->myid)) &&
	    (skb = hysdn_tx_netget(card)) != NULL)
	{
		if (skb->len <= maxlen) {
			
			skb_copy_from_linear_data(skb, buf, skb->len);
			*len = skb->len;
			*chan = CHAN_NDIS_DATA;
			card->net_tx_busy = 1;	
			return (1);	
		} else
			hysdn_tx_netack(card);	
	}			
#ifdef CONFIG_HYSDN_CAPI
	if (((hycapi_enable & (1 << card->myid))) &&
	    ((skb = hycapi_tx_capiget(card)) != NULL))
	{
		if (skb->len <= maxlen) {
			skb_copy_from_linear_data(skb, buf, skb->len);
			*len = skb->len;
			*chan = CHAN_CAPI;
			hycapi_tx_capiack(card);
			return (1);	
		}
	}
#endif 
	return (0);		
}				


int
hysdn_tx_cfgline(hysdn_card *card, unsigned char *line, unsigned short chan)
{
	int cnt = 50;		
	unsigned long flags;

	if (card->debug_flags & LOG_SCHED_ASYN)
		hysdn_addlog(card, "async tx-cfg chan=%d len=%d", chan, strlen(line) + 1);

	while (card->async_busy) {

		if (card->debug_flags & LOG_SCHED_ASYN)
			hysdn_addlog(card, "async tx-cfg delayed");

		msleep_interruptible(20);		
		if (!--cnt)
			return (-ERR_ASYNC_TIME);	
	}			

	spin_lock_irqsave(&card->hysdn_lock, flags);
	strcpy(card->async_data, line);
	card->async_len = strlen(line) + 1;
	card->async_channel = chan;
	card->async_busy = 1;	

	
	schedule_work(&card->irq_queue);
	spin_unlock_irqrestore(&card->hysdn_lock, flags);

	if (card->debug_flags & LOG_SCHED_ASYN)
		hysdn_addlog(card, "async tx-cfg data queued");

	cnt++;			

	while (card->async_busy) {

		if (card->debug_flags & LOG_SCHED_ASYN)
			hysdn_addlog(card, "async tx-cfg waiting for tx-ready");

		msleep_interruptible(20);		
		if (!--cnt)
			return (-ERR_ASYNC_TIME);	
	}			

	if (card->debug_flags & LOG_SCHED_ASYN)
		hysdn_addlog(card, "async tx-cfg data send");

	return (0);		
}				
