/*********************************************************************
 *
 * Filename:      irttp.c
 * Version:       1.2
 * Description:   Tiny Transport Protocol (TTP) implementation
 * Status:        Stable
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sun Aug 31 20:14:31 1997
 * Modified at:   Wed Jan  5 11:31:27 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 *
 *     Copyright (c) 1998-2000 Dag Brattli <dagb@cs.uit.no>,
 *     All Rights Reserved.
 *     Copyright (c) 2000-2003 Jean Tourrilhes <jt@hpl.hp.com>
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
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/export.h>

#include <asm/byteorder.h>
#include <asm/unaligned.h>

#include <net/irda/irda.h>
#include <net/irda/irlap.h>
#include <net/irda/irlmp.h>
#include <net/irda/parameters.h>
#include <net/irda/irttp.h>

static struct irttp_cb *irttp;

static void __irttp_close_tsap(struct tsap_cb *self);

static int irttp_data_indication(void *instance, void *sap,
				 struct sk_buff *skb);
static int irttp_udata_indication(void *instance, void *sap,
				  struct sk_buff *skb);
static void irttp_disconnect_indication(void *instance, void *sap,
					LM_REASON reason, struct sk_buff *);
static void irttp_connect_indication(void *instance, void *sap,
				     struct qos_info *qos, __u32 max_sdu_size,
				     __u8 header_size, struct sk_buff *skb);
static void irttp_connect_confirm(void *instance, void *sap,
				  struct qos_info *qos, __u32 max_sdu_size,
				  __u8 header_size, struct sk_buff *skb);
static void irttp_run_tx_queue(struct tsap_cb *self);
static void irttp_run_rx_queue(struct tsap_cb *self);

static void irttp_flush_queues(struct tsap_cb *self);
static void irttp_fragment_skb(struct tsap_cb *self, struct sk_buff *skb);
static struct sk_buff *irttp_reassemble_skb(struct tsap_cb *self);
static void irttp_todo_expired(unsigned long data);
static int irttp_param_max_sdu_size(void *instance, irda_param_t *param,
				    int get);

static void irttp_flow_indication(void *instance, void *sap, LOCAL_FLOW flow);
static void irttp_status_indication(void *instance,
				    LINK_STATUS link, LOCK_STATUS lock);

static pi_minor_info_t pi_minor_call_table[] = {
	{ NULL, 0 },                                             
	{ irttp_param_max_sdu_size, PV_INTEGER | PV_BIG_ENDIAN } 
};
static pi_major_info_t pi_major_call_table[] = {{ pi_minor_call_table, 2 }};
static pi_param_info_t param_info = { pi_major_call_table, 1, 0x0f, 4 };


int __init irttp_init(void)
{
	irttp = kzalloc(sizeof(struct irttp_cb), GFP_KERNEL);
	if (irttp == NULL)
		return -ENOMEM;

	irttp->magic = TTP_MAGIC;

	irttp->tsaps = hashbin_new(HB_LOCK);
	if (!irttp->tsaps) {
		IRDA_ERROR("%s: can't allocate IrTTP hashbin!\n",
			   __func__);
		kfree(irttp);
		return -ENOMEM;
	}

	return 0;
}

void irttp_cleanup(void)
{
	
	IRDA_ASSERT(irttp->magic == TTP_MAGIC, return;);

	hashbin_delete(irttp->tsaps, (FREE_FUNC) __irttp_close_tsap);

	irttp->magic = 0;

	
	kfree(irttp);

	irttp = NULL;
}


static inline void irttp_start_todo_timer(struct tsap_cb *self, int timeout)
{
	
	mod_timer(&self->todo_timer, jiffies + timeout);
}

static void irttp_todo_expired(unsigned long data)
{
	struct tsap_cb *self = (struct tsap_cb *) data;

	
	if (!self || self->magic != TTP_TSAP_MAGIC)
		return;

	IRDA_DEBUG(4, "%s(instance=%p)\n", __func__, self);

	
	irttp_run_rx_queue(self);
	irttp_run_tx_queue(self);

	
	if (test_bit(0, &self->disconnect_pend)) {
		
		if (skb_queue_empty(&self->tx_queue)) {
			
			clear_bit(0, &self->disconnect_pend);	

			
			irttp_disconnect_request(self, self->disconnect_skb,
						 P_NORMAL);
			self->disconnect_skb = NULL;
		} else {
			
			irttp_start_todo_timer(self, HZ/10);

			
			return;
		}
	}

	
	if (self->close_pend)
		
		irttp_close_tsap(self);
}

static void irttp_flush_queues(struct tsap_cb *self)
{
	struct sk_buff* skb;

	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return;);

	
	while ((skb = skb_dequeue(&self->tx_queue)) != NULL)
		dev_kfree_skb(skb);

	
	while ((skb = skb_dequeue(&self->rx_queue)) != NULL)
		dev_kfree_skb(skb);

	
	while ((skb = skb_dequeue(&self->rx_fragments)) != NULL)
		dev_kfree_skb(skb);
}

static struct sk_buff *irttp_reassemble_skb(struct tsap_cb *self)
{
	struct sk_buff *skb, *frag;
	int n = 0;  

	IRDA_ASSERT(self != NULL, return NULL;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return NULL;);

	IRDA_DEBUG(2, "%s(), self->rx_sdu_size=%d\n", __func__,
		   self->rx_sdu_size);

	skb = dev_alloc_skb(TTP_HEADER + self->rx_sdu_size);
	if (!skb)
		return NULL;

	skb_reserve(skb, TTP_HEADER);
	skb_put(skb, self->rx_sdu_size);

	while ((frag = skb_dequeue(&self->rx_fragments)) != NULL) {
		skb_copy_to_linear_data_offset(skb, n, frag->data, frag->len);
		n += frag->len;

		dev_kfree_skb(frag);
	}

	IRDA_DEBUG(2,
		   "%s(), frame len=%d, rx_sdu_size=%d, rx_max_sdu_size=%d\n",
		   __func__, n, self->rx_sdu_size, self->rx_max_sdu_size);
	IRDA_ASSERT(n <= self->rx_sdu_size, n = self->rx_sdu_size;);

	
	skb_trim(skb, n);

	self->rx_sdu_size = 0;

	return skb;
}

static inline void irttp_fragment_skb(struct tsap_cb *self,
				      struct sk_buff *skb)
{
	struct sk_buff *frag;
	__u8 *frame;

	IRDA_DEBUG(2, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return;);
	IRDA_ASSERT(skb != NULL, return;);

	while (skb->len > self->max_seg_size) {
		IRDA_DEBUG(2, "%s(), fragmenting ...\n", __func__);

		
		frag = alloc_skb(self->max_seg_size+self->max_header_size,
				 GFP_ATOMIC);
		if (!frag)
			return;

		skb_reserve(frag, self->max_header_size);

		
		skb_copy_from_linear_data(skb, skb_put(frag, self->max_seg_size),
			      self->max_seg_size);

		
		frame = skb_push(frag, TTP_HEADER);
		frame[0] = TTP_MORE;

		
		skb_pull(skb, self->max_seg_size);

		
		skb_queue_tail(&self->tx_queue, frag);
	}
	
	IRDA_DEBUG(2, "%s(), queuing last segment\n", __func__);

	frame = skb_push(skb, TTP_HEADER);
	frame[0] = 0x00; 

	
	skb_queue_tail(&self->tx_queue, skb);
}

static int irttp_param_max_sdu_size(void *instance, irda_param_t *param,
				    int get)
{
	struct tsap_cb *self;

	self = instance;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return -1;);

	if (get)
		param->pv.i = self->tx_max_sdu_size;
	else
		self->tx_max_sdu_size = param->pv.i;

	IRDA_DEBUG(1, "%s(), MaxSduSize=%d\n", __func__, param->pv.i);

	return 0;
}


static void irttp_init_tsap(struct tsap_cb *tsap)
{
	spin_lock_init(&tsap->lock);
	init_timer(&tsap->todo_timer);

	skb_queue_head_init(&tsap->rx_queue);
	skb_queue_head_init(&tsap->tx_queue);
	skb_queue_head_init(&tsap->rx_fragments);
}

struct tsap_cb *irttp_open_tsap(__u8 stsap_sel, int credit, notify_t *notify)
{
	struct tsap_cb *self;
	struct lsap_cb *lsap;
	notify_t ttp_notify;

	IRDA_ASSERT(irttp->magic == TTP_MAGIC, return NULL;);

	if((stsap_sel != LSAP_ANY) &&
	   ((stsap_sel < 0x01) || (stsap_sel >= 0x70))) {
		IRDA_DEBUG(0, "%s(), invalid tsap!\n", __func__);
		return NULL;
	}

	self = kzalloc(sizeof(struct tsap_cb), GFP_ATOMIC);
	if (self == NULL) {
		IRDA_DEBUG(0, "%s(), unable to kmalloc!\n", __func__);
		return NULL;
	}

	
	irttp_init_tsap(self);

	
	self->todo_timer.data     = (unsigned long) self;
	self->todo_timer.function = &irttp_todo_expired;

	
	irda_notify_init(&ttp_notify);
	ttp_notify.connect_confirm = irttp_connect_confirm;
	ttp_notify.connect_indication = irttp_connect_indication;
	ttp_notify.disconnect_indication = irttp_disconnect_indication;
	ttp_notify.data_indication = irttp_data_indication;
	ttp_notify.udata_indication = irttp_udata_indication;
	ttp_notify.flow_indication = irttp_flow_indication;
	if(notify->status_indication != NULL)
		ttp_notify.status_indication = irttp_status_indication;
	ttp_notify.instance = self;
	strncpy(ttp_notify.name, notify->name, NOTIFY_MAX_NAME);

	self->magic = TTP_TSAP_MAGIC;
	self->connected = FALSE;

	lsap = irlmp_open_lsap(stsap_sel, &ttp_notify, 0);
	if (lsap == NULL) {
		IRDA_WARNING("%s: unable to allocate LSAP!!\n", __func__);
		return NULL;
	}

	self->stsap_sel = lsap->slsap_sel;
	IRDA_DEBUG(4, "%s(), stsap_sel=%02x\n", __func__, self->stsap_sel);

	self->notify = *notify;
	self->lsap = lsap;

	hashbin_insert(irttp->tsaps, (irda_queue_t *) self, (long) self, NULL);

	if (credit > TTP_RX_MAX_CREDIT)
		self->initial_credit = TTP_RX_MAX_CREDIT;
	else
		self->initial_credit = credit;

	return self;
}
EXPORT_SYMBOL(irttp_open_tsap);

static void __irttp_close_tsap(struct tsap_cb *self)
{
	
	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return;);

	irttp_flush_queues(self);

	del_timer(&self->todo_timer);

	if (self->disconnect_skb)
		dev_kfree_skb(self->disconnect_skb);

	self->connected = FALSE;
	self->magic = ~TTP_TSAP_MAGIC;

	kfree(self);
}

int irttp_close_tsap(struct tsap_cb *self)
{
	struct tsap_cb *tsap;

	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return -1;);

	
	if (self->connected) {
		
		if (!test_bit(0, &self->disconnect_pend)) {
			IRDA_WARNING("%s: TSAP still connected!\n",
				     __func__);
			irttp_disconnect_request(self, NULL, P_NORMAL);
		}
		self->close_pend = TRUE;
		irttp_start_todo_timer(self, HZ/10);

		return 0; 
	}

	tsap = hashbin_remove(irttp->tsaps, (long) self, NULL);

	IRDA_ASSERT(tsap == self, return -1;);

	
	if (self->lsap) {
		irlmp_close_lsap(self->lsap);
		self->lsap = NULL;
	}

	__irttp_close_tsap(self);

	return 0;
}
EXPORT_SYMBOL(irttp_close_tsap);

int irttp_udata_request(struct tsap_cb *self, struct sk_buff *skb)
{
	int ret;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return -1;);
	IRDA_ASSERT(skb != NULL, return -1;);

	IRDA_DEBUG(4, "%s()\n", __func__);

	
	if (skb->len == 0) {
		ret = 0;
		goto err;
	}

	
	if (!self->connected) {
		IRDA_WARNING("%s(), Not connected\n", __func__);
		ret = -ENOTCONN;
		goto err;
	}

	if (skb->len > self->max_seg_size) {
		IRDA_ERROR("%s(), UData is too large for IrLAP!\n", __func__);
		ret = -EMSGSIZE;
		goto err;
	}

	irlmp_udata_request(self->lsap, skb);
	self->stats.tx_packets++;

	return 0;

err:
	dev_kfree_skb(skb);
	return ret;
}
EXPORT_SYMBOL(irttp_udata_request);


int irttp_data_request(struct tsap_cb *self, struct sk_buff *skb)
{
	__u8 *frame;
	int ret;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return -1;);
	IRDA_ASSERT(skb != NULL, return -1;);

	IRDA_DEBUG(2, "%s() : queue len = %d\n", __func__,
		   skb_queue_len(&self->tx_queue));

	
	if (skb->len == 0) {
		ret = 0;
		goto err;
	}

	
	if (!self->connected) {
		IRDA_WARNING("%s: Not connected\n", __func__);
		ret = -ENOTCONN;
		goto err;
	}

	if ((self->tx_max_sdu_size == 0) && (skb->len > self->max_seg_size)) {
		IRDA_ERROR("%s: SAR disabled, and data is too large for IrLAP!\n",
			   __func__);
		ret = -EMSGSIZE;
		goto err;
	}

	if ((self->tx_max_sdu_size != 0) &&
	    (self->tx_max_sdu_size != TTP_SAR_UNBOUND) &&
	    (skb->len > self->tx_max_sdu_size))
	{
		IRDA_ERROR("%s: SAR enabled, but data is larger than TxMaxSduSize!\n",
			   __func__);
		ret = -EMSGSIZE;
		goto err;
	}
	if (skb_queue_len(&self->tx_queue) >= TTP_TX_MAX_QUEUE) {
		irttp_run_tx_queue(self);

		ret = -ENOBUFS;
		goto err;
	}

	
	if ((self->tx_max_sdu_size == 0) || (skb->len < self->max_seg_size)) {
		
		IRDA_ASSERT(skb_headroom(skb) >= TTP_HEADER, return -1;);
		frame = skb_push(skb, TTP_HEADER);
		frame[0] = 0x00; 

		skb_queue_tail(&self->tx_queue, skb);
	} else {
		irttp_fragment_skb(self, skb);
	}

	
	if ((!self->tx_sdu_busy) &&
	    (skb_queue_len(&self->tx_queue) > TTP_TX_HIGH_THRESHOLD)) {
		
		if (self->notify.flow_indication) {
			self->notify.flow_indication(self->notify.instance,
						     self, FLOW_STOP);
		}
		self->tx_sdu_busy = TRUE;
	}

	
	irttp_run_tx_queue(self);

	return 0;

err:
	dev_kfree_skb(skb);
	return ret;
}
EXPORT_SYMBOL(irttp_data_request);

static void irttp_run_tx_queue(struct tsap_cb *self)
{
	struct sk_buff *skb;
	unsigned long flags;
	int n;

	IRDA_DEBUG(2, "%s() : send_credit = %d, queue_len = %d\n",
		   __func__,
		   self->send_credit, skb_queue_len(&self->tx_queue));

	
	if (irda_lock(&self->tx_queue_lock) == FALSE)
		return;

	while ((self->send_credit > 0) &&
	       (!irlmp_lap_tx_queue_full(self->lsap)) &&
	       (skb = skb_dequeue(&self->tx_queue)))
	{
		spin_lock_irqsave(&self->lock, flags);

		n = self->avail_credit;
		self->avail_credit = 0;

		
		if (n > 127) {
			self->avail_credit = n-127;
			n = 127;
		}
		self->remote_credit += n;
		self->send_credit--;

		spin_unlock_irqrestore(&self->lock, flags);

		skb->data[0] |= (n & 0x7f);

		if (skb->sk != NULL) {
			
			skb_orphan(skb);
		}
			

		
		irlmp_data_request(self->lsap, skb);
		self->stats.tx_packets++;
	}

	if ((self->tx_sdu_busy) &&
	    (skb_queue_len(&self->tx_queue) < TTP_TX_LOW_THRESHOLD) &&
	    (!self->close_pend))
	{
		if (self->notify.flow_indication)
			self->notify.flow_indication(self->notify.instance,
						     self, FLOW_START);

		self->tx_sdu_busy = FALSE;
	}

	
	self->tx_queue_lock = 0;
}

static inline void irttp_give_credit(struct tsap_cb *self)
{
	struct sk_buff *tx_skb = NULL;
	unsigned long flags;
	int n;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return;);

	IRDA_DEBUG(4, "%s() send=%d,avail=%d,remote=%d\n",
		   __func__,
		   self->send_credit, self->avail_credit, self->remote_credit);

	
	tx_skb = alloc_skb(TTP_MAX_HEADER, GFP_ATOMIC);
	if (!tx_skb)
		return;

	
	skb_reserve(tx_skb, LMP_MAX_HEADER);

	spin_lock_irqsave(&self->lock, flags);

	n = self->avail_credit;
	self->avail_credit = 0;

	
	if (n > 127) {
		self->avail_credit = n - 127;
		n = 127;
	}
	self->remote_credit += n;

	spin_unlock_irqrestore(&self->lock, flags);

	skb_put(tx_skb, 1);
	tx_skb->data[0] = (__u8) (n & 0x7f);

	irlmp_data_request(self->lsap, tx_skb);
	self->stats.tx_packets++;
}

static int irttp_udata_indication(void *instance, void *sap,
				  struct sk_buff *skb)
{
	struct tsap_cb *self;
	int err;

	IRDA_DEBUG(4, "%s()\n", __func__);

	self = instance;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return -1;);
	IRDA_ASSERT(skb != NULL, return -1;);

	self->stats.rx_packets++;

	
	if (self->notify.udata_indication) {
		err = self->notify.udata_indication(self->notify.instance,
						    self,skb);
		
		if (!err)
			return 0;
	}
	
	dev_kfree_skb(skb);

	return 0;
}

static int irttp_data_indication(void *instance, void *sap,
				 struct sk_buff *skb)
{
	struct tsap_cb *self;
	unsigned long flags;
	int n;

	self = instance;

	n = skb->data[0] & 0x7f;     

	self->stats.rx_packets++;

	spin_lock_irqsave(&self->lock, flags);
	self->send_credit += n;
	if (skb->len > 1)
		self->remote_credit--;
	spin_unlock_irqrestore(&self->lock, flags);

	if (skb->len > 1) {
		skb_queue_tail(&self->rx_queue, skb);
	} else {
		
		dev_kfree_skb(skb);
	}


	irttp_run_rx_queue(self);


	if (self->send_credit == n) {
		
		irttp_run_tx_queue(self);
	}

	return 0;
}

static void irttp_status_indication(void *instance,
				    LINK_STATUS link, LOCK_STATUS lock)
{
	struct tsap_cb *self;

	IRDA_DEBUG(4, "%s()\n", __func__);

	self = instance;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return;);

	
	if (self->close_pend)
		return;

	if (self->notify.status_indication != NULL)
		self->notify.status_indication(self->notify.instance,
					       link, lock);
	else
		IRDA_DEBUG(2, "%s(), no handler\n", __func__);
}

static void irttp_flow_indication(void *instance, void *sap, LOCAL_FLOW flow)
{
	struct tsap_cb *self;

	self = instance;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return;);

	IRDA_DEBUG(4, "%s(instance=%p)\n", __func__, self);


	irttp_run_tx_queue(self);


	
	if(self->disconnect_pend)
		irttp_start_todo_timer(self, 0);
}

void irttp_flow_request(struct tsap_cb *self, LOCAL_FLOW flow)
{
	IRDA_DEBUG(1, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return;);

	switch (flow) {
	case FLOW_STOP:
		IRDA_DEBUG(1, "%s(), flow stop\n", __func__);
		self->rx_sdu_busy = TRUE;
		break;
	case FLOW_START:
		IRDA_DEBUG(1, "%s(), flow start\n", __func__);
		self->rx_sdu_busy = FALSE;

		irttp_run_rx_queue(self);

		break;
	default:
		IRDA_DEBUG(1, "%s(), Unknown flow command!\n", __func__);
	}
}
EXPORT_SYMBOL(irttp_flow_request);

int irttp_connect_request(struct tsap_cb *self, __u8 dtsap_sel,
			  __u32 saddr, __u32 daddr,
			  struct qos_info *qos, __u32 max_sdu_size,
			  struct sk_buff *userdata)
{
	struct sk_buff *tx_skb;
	__u8 *frame;
	__u8 n;

	IRDA_DEBUG(4, "%s(), max_sdu_size=%d\n", __func__, max_sdu_size);

	IRDA_ASSERT(self != NULL, return -EBADR;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return -EBADR;);

	if (self->connected) {
		if(userdata)
			dev_kfree_skb(userdata);
		return -EISCONN;
	}

	
	if (userdata == NULL) {
		tx_skb = alloc_skb(TTP_MAX_HEADER + TTP_SAR_HEADER,
				   GFP_ATOMIC);
		if (!tx_skb)
			return -ENOMEM;

		
		skb_reserve(tx_skb, TTP_MAX_HEADER + TTP_SAR_HEADER);
	} else {
		tx_skb = userdata;
		IRDA_ASSERT(skb_headroom(userdata) >= TTP_MAX_HEADER,
			{ dev_kfree_skb(userdata); return -1; } );
	}

	
	self->connected = FALSE;
	self->avail_credit = 0;
	self->rx_max_sdu_size = max_sdu_size;
	self->rx_sdu_size = 0;
	self->rx_sdu_busy = FALSE;
	self->dtsap_sel = dtsap_sel;

	n = self->initial_credit;

	self->remote_credit = 0;
	self->send_credit = 0;

	if (n > 127) {
		self->avail_credit=n-127;
		n = 127;
	}

	self->remote_credit = n;

	
	if (max_sdu_size > 0) {
		IRDA_ASSERT(skb_headroom(tx_skb) >= (TTP_MAX_HEADER + TTP_SAR_HEADER),
			{ dev_kfree_skb(tx_skb); return -1; } );

		
		frame = skb_push(tx_skb, TTP_HEADER+TTP_SAR_HEADER);

		frame[0] = TTP_PARAMETERS | n;
		frame[1] = 0x04; 
		frame[2] = 0x01; 
		frame[3] = 0x02; 

		put_unaligned(cpu_to_be16((__u16) max_sdu_size),
			      (__be16 *)(frame+4));
	} else {
		
		frame = skb_push(tx_skb, TTP_HEADER);

		
		frame[0] = n & 0x7f;
	}

	
	return irlmp_connect_request(self->lsap, dtsap_sel, saddr, daddr, qos,
				     tx_skb);
}
EXPORT_SYMBOL(irttp_connect_request);

static void irttp_connect_confirm(void *instance, void *sap,
				  struct qos_info *qos, __u32 max_seg_size,
				  __u8 max_header_size, struct sk_buff *skb)
{
	struct tsap_cb *self;
	int parameters;
	int ret;
	__u8 plen;
	__u8 n;

	IRDA_DEBUG(4, "%s()\n", __func__);

	self = instance;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return;);
	IRDA_ASSERT(skb != NULL, return;);

	self->max_seg_size = max_seg_size - TTP_HEADER;
	self->max_header_size = max_header_size + TTP_HEADER;

	if (qos) {
		IRDA_DEBUG(4, "IrTTP, Negotiated BAUD_RATE: %02x\n",
		       qos->baud_rate.bits);
		IRDA_DEBUG(4, "IrTTP, Negotiated BAUD_RATE: %d bps.\n",
		       qos->baud_rate.value);
	}

	n = skb->data[0] & 0x7f;

	IRDA_DEBUG(4, "%s(), Initial send_credit=%d\n", __func__, n);

	self->send_credit = n;
	self->tx_max_sdu_size = 0;
	self->connected = TRUE;

	parameters = skb->data[0] & 0x80;

	IRDA_ASSERT(skb->len >= TTP_HEADER, return;);
	skb_pull(skb, TTP_HEADER);

	if (parameters) {
		plen = skb->data[0];

		ret = irda_param_extract_all(self, skb->data+1,
					     IRDA_MIN(skb->len-1, plen),
					     &param_info);

		
		if (ret < 0) {
			IRDA_WARNING("%s: error extracting parameters\n",
				     __func__);
			dev_kfree_skb(skb);

			
			return;
		}
		
		skb_pull(skb, IRDA_MIN(skb->len, plen+1));
	}

	IRDA_DEBUG(4, "%s() send=%d,avail=%d,remote=%d\n", __func__,
	      self->send_credit, self->avail_credit, self->remote_credit);

	IRDA_DEBUG(2, "%s(), MaxSduSize=%d\n", __func__,
		   self->tx_max_sdu_size);

	if (self->notify.connect_confirm) {
		self->notify.connect_confirm(self->notify.instance, self, qos,
					     self->tx_max_sdu_size,
					     self->max_header_size, skb);
	} else
		dev_kfree_skb(skb);
}

static void irttp_connect_indication(void *instance, void *sap,
		struct qos_info *qos, __u32 max_seg_size, __u8 max_header_size,
		struct sk_buff *skb)
{
	struct tsap_cb *self;
	struct lsap_cb *lsap;
	int parameters;
	int ret;
	__u8 plen;
	__u8 n;

	self = instance;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return;);
	IRDA_ASSERT(skb != NULL, return;);

	lsap = sap;

	self->max_seg_size = max_seg_size - TTP_HEADER;
	self->max_header_size = max_header_size+TTP_HEADER;

	IRDA_DEBUG(4, "%s(), TSAP sel=%02x\n", __func__, self->stsap_sel);

	
	self->dtsap_sel = lsap->dlsap_sel;

	n = skb->data[0] & 0x7f;

	self->send_credit = n;
	self->tx_max_sdu_size = 0;

	parameters = skb->data[0] & 0x80;

	IRDA_ASSERT(skb->len >= TTP_HEADER, return;);
	skb_pull(skb, TTP_HEADER);

	if (parameters) {
		plen = skb->data[0];

		ret = irda_param_extract_all(self, skb->data+1,
					     IRDA_MIN(skb->len-1, plen),
					     &param_info);

		
		if (ret < 0) {
			IRDA_WARNING("%s: error extracting parameters\n",
				     __func__);
			dev_kfree_skb(skb);

			
			return;
		}

		
		skb_pull(skb, IRDA_MIN(skb->len, plen+1));
	}

	if (self->notify.connect_indication) {
		self->notify.connect_indication(self->notify.instance, self,
						qos, self->tx_max_sdu_size,
						self->max_header_size, skb);
	} else
		dev_kfree_skb(skb);
}

int irttp_connect_response(struct tsap_cb *self, __u32 max_sdu_size,
			   struct sk_buff *userdata)
{
	struct sk_buff *tx_skb;
	__u8 *frame;
	int ret;
	__u8 n;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return -1;);

	IRDA_DEBUG(4, "%s(), Source TSAP selector=%02x\n", __func__,
		   self->stsap_sel);

	
	if (userdata == NULL) {
		tx_skb = alloc_skb(TTP_MAX_HEADER + TTP_SAR_HEADER,
				   GFP_ATOMIC);
		if (!tx_skb)
			return -ENOMEM;

		
		skb_reserve(tx_skb, TTP_MAX_HEADER + TTP_SAR_HEADER);
	} else {
		tx_skb = userdata;
		IRDA_ASSERT(skb_headroom(userdata) >= TTP_MAX_HEADER,
			{ dev_kfree_skb(userdata); return -1; } );
	}

	self->avail_credit = 0;
	self->remote_credit = 0;
	self->rx_max_sdu_size = max_sdu_size;
	self->rx_sdu_size = 0;
	self->rx_sdu_busy = FALSE;

	n = self->initial_credit;

	
	if (n > 127) {
		self->avail_credit = n - 127;
		n = 127;
	}

	self->remote_credit = n;
	self->connected = TRUE;

	
	if (max_sdu_size > 0) {
		IRDA_ASSERT(skb_headroom(tx_skb) >= (TTP_MAX_HEADER + TTP_SAR_HEADER),
			{ dev_kfree_skb(tx_skb); return -1; } );

		
		frame = skb_push(tx_skb, TTP_HEADER+TTP_SAR_HEADER);

		frame[0] = TTP_PARAMETERS | n;
		frame[1] = 0x04; 

		

		frame[2] = 0x01; 
		frame[3] = 0x02; 

		put_unaligned(cpu_to_be16((__u16) max_sdu_size),
			      (__be16 *)(frame+4));
	} else {
		
		frame = skb_push(tx_skb, TTP_HEADER);

		frame[0] = n & 0x7f;
	}

	ret = irlmp_connect_response(self->lsap, tx_skb);

	return ret;
}
EXPORT_SYMBOL(irttp_connect_response);

struct tsap_cb *irttp_dup(struct tsap_cb *orig, void *instance)
{
	struct tsap_cb *new;
	unsigned long flags;

	IRDA_DEBUG(1, "%s()\n", __func__);

	
	spin_lock_irqsave(&irttp->tsaps->hb_spinlock, flags);

	
	if (!hashbin_find(irttp->tsaps, (long) orig, NULL)) {
		IRDA_DEBUG(0, "%s(), unable to find TSAP\n", __func__);
		spin_unlock_irqrestore(&irttp->tsaps->hb_spinlock, flags);
		return NULL;
	}

	
	new = kmemdup(orig, sizeof(struct tsap_cb), GFP_ATOMIC);
	if (!new) {
		IRDA_DEBUG(0, "%s(), unable to kmalloc\n", __func__);
		spin_unlock_irqrestore(&irttp->tsaps->hb_spinlock, flags);
		return NULL;
	}
	spin_lock_init(&new->lock);

	
	spin_unlock_irqrestore(&irttp->tsaps->hb_spinlock, flags);

	
	new->lsap = irlmp_dup(orig->lsap, new);
	if (!new->lsap) {
		IRDA_DEBUG(0, "%s(), dup failed!\n", __func__);
		kfree(new);
		return NULL;
	}

	
	new->notify.instance = instance;

	
	irttp_init_tsap(new);

	
	hashbin_insert(irttp->tsaps, (irda_queue_t *) new, (long) new, NULL);

	return new;
}
EXPORT_SYMBOL(irttp_dup);

int irttp_disconnect_request(struct tsap_cb *self, struct sk_buff *userdata,
			     int priority)
{
	int ret;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return -1;);

	
	if (!self->connected) {
		IRDA_DEBUG(4, "%s(), already disconnected!\n", __func__);
		if (userdata)
			dev_kfree_skb(userdata);
		return -1;
	}

	if(test_and_set_bit(0, &self->disconnect_pend)) {
		IRDA_DEBUG(0, "%s(), disconnect already pending\n",
			   __func__);
		if (userdata)
			dev_kfree_skb(userdata);

		
		irttp_run_tx_queue(self);
		return -1;
	}

	if (!skb_queue_empty(&self->tx_queue)) {
		if (priority == P_HIGH) {
			IRDA_DEBUG(1, "%s(): High priority!!()\n", __func__);
			irttp_flush_queues(self);
		} else if (priority == P_NORMAL) {
			
			self->disconnect_skb = userdata;  

			irttp_run_tx_queue(self);

			irttp_start_todo_timer(self, HZ/10);
			return -1;
		}
	}

	IRDA_DEBUG(1, "%s(), Disconnecting ...\n", __func__);
	self->connected = FALSE;

	if (!userdata) {
		struct sk_buff *tx_skb;
		tx_skb = alloc_skb(LMP_MAX_HEADER, GFP_ATOMIC);
		if (!tx_skb)
			return -ENOMEM;

		skb_reserve(tx_skb, LMP_MAX_HEADER);

		userdata = tx_skb;
	}
	ret = irlmp_disconnect_request(self->lsap, userdata);

	
	clear_bit(0, &self->disconnect_pend);	

	return ret;
}
EXPORT_SYMBOL(irttp_disconnect_request);

static void irttp_disconnect_indication(void *instance, void *sap,
		LM_REASON reason, struct sk_buff *skb)
{
	struct tsap_cb *self;

	IRDA_DEBUG(4, "%s()\n", __func__);

	self = instance;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == TTP_TSAP_MAGIC, return;);

	
	self->connected = FALSE;

	
	if (self->close_pend) {
		if (skb)
			dev_kfree_skb(skb);
		irttp_close_tsap(self);
		return;
	}


	
	if(self->notify.disconnect_indication)
		self->notify.disconnect_indication(self->notify.instance, self,
						   reason, skb);
	else
		if (skb)
			dev_kfree_skb(skb);
}

static void irttp_do_data_indication(struct tsap_cb *self, struct sk_buff *skb)
{
	int err;

	
	if (self->close_pend) {
		dev_kfree_skb(skb);
		return;
	}

	err = self->notify.data_indication(self->notify.instance, self, skb);

	if (err) {
		IRDA_DEBUG(0, "%s() requeueing skb!\n", __func__);

		
		self->rx_sdu_busy = TRUE;

		
		skb_push(skb, TTP_HEADER);
		skb->data[0] = 0x00; 

		
		skb_queue_head(&self->rx_queue, skb);
	}
}

static void irttp_run_rx_queue(struct tsap_cb *self)
{
	struct sk_buff *skb;
	int more = 0;

	IRDA_DEBUG(2, "%s() send=%d,avail=%d,remote=%d\n", __func__,
		   self->send_credit, self->avail_credit, self->remote_credit);

	
	if (irda_lock(&self->rx_queue_lock) == FALSE)
		return;

	while (!self->rx_sdu_busy && (skb = skb_dequeue(&self->rx_queue))) {
		
		more = skb->data[0] & 0x80;

		
		skb_pull(skb, TTP_HEADER);

		
		self->rx_sdu_size += skb->len;

		if (self->rx_max_sdu_size == TTP_SAR_DISABLE) {
			irttp_do_data_indication(self, skb);
			self->rx_sdu_size = 0;

			continue;
		}

		
		if (more) {
			if (self->rx_sdu_size <= self->rx_max_sdu_size) {
				IRDA_DEBUG(4, "%s(), queueing frag\n",
					   __func__);
				skb_queue_tail(&self->rx_fragments, skb);
			} else {
				
				dev_kfree_skb(skb);
			}
			continue;
		}
		if ((self->rx_sdu_size <= self->rx_max_sdu_size) ||
		    (self->rx_max_sdu_size == TTP_SAR_UNBOUND))
		{
			if (!skb_queue_empty(&self->rx_fragments)) {
				skb_queue_tail(&self->rx_fragments,
					       skb);

				skb = irttp_reassemble_skb(self);
			}

			
			irttp_do_data_indication(self, skb);
		} else {
			IRDA_DEBUG(1, "%s(), Truncated frame\n", __func__);

			
			dev_kfree_skb(skb);

			
			skb = irttp_reassemble_skb(self);

			irttp_do_data_indication(self, skb);
		}
		self->rx_sdu_size = 0;
	}

	self->avail_credit = (self->initial_credit -
			      (self->remote_credit +
			       skb_queue_len(&self->rx_queue) +
			       skb_queue_len(&self->rx_fragments)));

	
	if ((self->remote_credit <= TTP_RX_MIN_CREDIT) &&
	    (self->avail_credit > 0)) {
		
		irttp_give_credit(self);
	}

	
	self->rx_queue_lock = 0;
}

#ifdef CONFIG_PROC_FS
struct irttp_iter_state {
	int id;
};

static void *irttp_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct irttp_iter_state *iter = seq->private;
	struct tsap_cb *self;

	
	spin_lock_irq(&irttp->tsaps->hb_spinlock);
	iter->id = 0;

	for (self = (struct tsap_cb *) hashbin_get_first(irttp->tsaps);
	     self != NULL;
	     self = (struct tsap_cb *) hashbin_get_next(irttp->tsaps)) {
		if (iter->id == *pos)
			break;
		++iter->id;
	}

	return self;
}

static void *irttp_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct irttp_iter_state *iter = seq->private;

	++*pos;
	++iter->id;
	return (void *) hashbin_get_next(irttp->tsaps);
}

static void irttp_seq_stop(struct seq_file *seq, void *v)
{
	spin_unlock_irq(&irttp->tsaps->hb_spinlock);
}

static int irttp_seq_show(struct seq_file *seq, void *v)
{
	const struct irttp_iter_state *iter = seq->private;
	const struct tsap_cb *self = v;

	seq_printf(seq, "TSAP %d, ", iter->id);
	seq_printf(seq, "stsap_sel: %02x, ",
		   self->stsap_sel);
	seq_printf(seq, "dtsap_sel: %02x\n",
		   self->dtsap_sel);
	seq_printf(seq, "  connected: %s, ",
		   self->connected? "TRUE":"FALSE");
	seq_printf(seq, "avail credit: %d, ",
		   self->avail_credit);
	seq_printf(seq, "remote credit: %d, ",
		   self->remote_credit);
	seq_printf(seq, "send credit: %d\n",
		   self->send_credit);
	seq_printf(seq, "  tx packets: %lu, ",
		   self->stats.tx_packets);
	seq_printf(seq, "rx packets: %lu, ",
		   self->stats.rx_packets);
	seq_printf(seq, "tx_queue len: %u ",
		   skb_queue_len(&self->tx_queue));
	seq_printf(seq, "rx_queue len: %u\n",
		   skb_queue_len(&self->rx_queue));
	seq_printf(seq, "  tx_sdu_busy: %s, ",
		   self->tx_sdu_busy? "TRUE":"FALSE");
	seq_printf(seq, "rx_sdu_busy: %s\n",
		   self->rx_sdu_busy? "TRUE":"FALSE");
	seq_printf(seq, "  max_seg_size: %u, ",
		   self->max_seg_size);
	seq_printf(seq, "tx_max_sdu_size: %u, ",
		   self->tx_max_sdu_size);
	seq_printf(seq, "rx_max_sdu_size: %u\n",
		   self->rx_max_sdu_size);

	seq_printf(seq, "  Used by (%s)\n\n",
		   self->notify.name);
	return 0;
}

static const struct seq_operations irttp_seq_ops = {
	.start  = irttp_seq_start,
	.next   = irttp_seq_next,
	.stop   = irttp_seq_stop,
	.show   = irttp_seq_show,
};

static int irttp_seq_open(struct inode *inode, struct file *file)
{
	return seq_open_private(file, &irttp_seq_ops,
			sizeof(struct irttp_iter_state));
}

const struct file_operations irttp_seq_fops = {
	.owner		= THIS_MODULE,
	.open           = irttp_seq_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release	= seq_release_private,
};

#endif 
