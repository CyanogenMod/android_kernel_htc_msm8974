/*********************************************************************
 *
 * Filename:      irlap.c
 * Version:       1.0
 * Description:   IrLAP implementation for Linux
 * Status:        Stable
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Mon Aug  4 20:40:53 1997
 * Modified at:   Tue Dec 14 09:26:44 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 *
 *     Copyright (c) 1998-1999 Dag Brattli, All Rights Reserved.
 *     Copyright (c) 2000-2003 Jean Tourrilhes <jt@hpl.hp.com>
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *     MA 02111-1307 USA
 *
 ********************************************************************/

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/random.h>
#include <linux/module.h>
#include <linux/seq_file.h>

#include <net/irda/irda.h>
#include <net/irda/irda_device.h>
#include <net/irda/irqueue.h>
#include <net/irda/irlmp.h>
#include <net/irda/irlmp_frame.h>
#include <net/irda/irlap_frame.h>
#include <net/irda/irlap.h>
#include <net/irda/timer.h>
#include <net/irda/qos.h>

static hashbin_t *irlap = NULL;
int sysctl_slot_timeout = SLOT_TIMEOUT * 1000 / HZ;

int sysctl_warn_noreply_time = 3;

extern void irlap_queue_xmit(struct irlap_cb *self, struct sk_buff *skb);
static void __irlap_close(struct irlap_cb *self);
static void irlap_init_qos_capabilities(struct irlap_cb *self,
					struct qos_info *qos_user);

#ifdef CONFIG_IRDA_DEBUG
static const char *const lap_reasons[] = {
	"ERROR, NOT USED",
	"LAP_DISC_INDICATION",
	"LAP_NO_RESPONSE",
	"LAP_RESET_INDICATION",
	"LAP_FOUND_NONE",
	"LAP_MEDIA_BUSY",
	"LAP_PRIMARY_CONFLICT",
	"ERROR, NOT USED",
};
#endif	

int __init irlap_init(void)
{
	IRDA_ASSERT(sizeof(struct xid_frame) == 14, ;);
	IRDA_ASSERT(sizeof(struct test_frame) == 10, ;);
	IRDA_ASSERT(sizeof(struct ua_frame) == 10, ;);
	IRDA_ASSERT(sizeof(struct snrm_frame) == 11, ;);

	
	irlap = hashbin_new(HB_LOCK);
	if (irlap == NULL) {
		IRDA_ERROR("%s: can't allocate irlap hashbin!\n",
			   __func__);
		return -ENOMEM;
	}

	return 0;
}

void irlap_cleanup(void)
{
	IRDA_ASSERT(irlap != NULL, return;);

	hashbin_delete(irlap, (FREE_FUNC) __irlap_close);
}

struct irlap_cb *irlap_open(struct net_device *dev, struct qos_info *qos,
			    const char *hw_name)
{
	struct irlap_cb *self;

	IRDA_DEBUG(4, "%s()\n", __func__);

	
	self = kzalloc(sizeof(struct irlap_cb), GFP_KERNEL);
	if (self == NULL)
		return NULL;

	self->magic = LAP_MAGIC;

	
	self->netdev = dev;
	self->qos_dev = qos;
	
	if(hw_name != NULL) {
		strlcpy(self->hw_name, hw_name, sizeof(self->hw_name));
	} else {
		self->hw_name[0] = '\0';
	}

	
	dev->atalk_ptr = self;

	self->state = LAP_OFFLINE;

	
	skb_queue_head_init(&self->txq);
	skb_queue_head_init(&self->txq_ultra);
	skb_queue_head_init(&self->wx_list);

	
	do {
		get_random_bytes(&self->saddr, sizeof(self->saddr));
	} while ((self->saddr == 0x0) || (self->saddr == BROADCAST) ||
		 (hashbin_lock_find(irlap, self->saddr, NULL)) );
	
	memcpy(dev->dev_addr, &self->saddr, 4);

	init_timer(&self->slot_timer);
	init_timer(&self->query_timer);
	init_timer(&self->discovery_timer);
	init_timer(&self->final_timer);
	init_timer(&self->poll_timer);
	init_timer(&self->wd_timer);
	init_timer(&self->backoff_timer);
	init_timer(&self->media_busy_timer);

	irlap_apply_default_connection_parameters(self);

	self->N3 = 3; 

	self->state = LAP_NDM;

	hashbin_insert(irlap, (irda_queue_t *) self, self->saddr, NULL);

	irlmp_register_link(self, self->saddr, &self->notify);

	return self;
}
EXPORT_SYMBOL(irlap_open);

static void __irlap_close(struct irlap_cb *self)
{
	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	
	del_timer(&self->slot_timer);
	del_timer(&self->query_timer);
	del_timer(&self->discovery_timer);
	del_timer(&self->final_timer);
	del_timer(&self->poll_timer);
	del_timer(&self->wd_timer);
	del_timer(&self->backoff_timer);
	del_timer(&self->media_busy_timer);

	irlap_flush_all_queues(self);

	self->magic = 0;

	kfree(self);
}

void irlap_close(struct irlap_cb *self)
{
	struct irlap_cb *lap;

	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);


	
	irlmp_unregister_link(self->saddr);
	self->notify.instance = NULL;

	
	lap = hashbin_remove(irlap, self->saddr, NULL);
	if (!lap) {
		IRDA_DEBUG(1, "%s(), Didn't find myself!\n", __func__);
		return;
	}
	__irlap_close(lap);
}
EXPORT_SYMBOL(irlap_close);

void irlap_connect_indication(struct irlap_cb *self, struct sk_buff *skb)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	irlap_init_qos_capabilities(self, NULL); 

	irlmp_link_connect_indication(self->notify.instance, self->saddr,
				      self->daddr, &self->qos_tx, skb);
}

void irlap_connect_response(struct irlap_cb *self, struct sk_buff *userdata)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	irlap_do_event(self, CONNECT_RESPONSE, userdata, NULL);
}

void irlap_connect_request(struct irlap_cb *self, __u32 daddr,
			   struct qos_info *qos_user, int sniff)
{
	IRDA_DEBUG(3, "%s(), daddr=0x%08x\n", __func__, daddr);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	self->daddr = daddr;

	irlap_init_qos_capabilities(self, qos_user);

	if ((self->state == LAP_NDM) && !self->media_busy)
		irlap_do_event(self, CONNECT_REQUEST, NULL, NULL);
	else
		self->connect_pending = TRUE;
}

void irlap_connect_confirm(struct irlap_cb *self, struct sk_buff *skb)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	irlmp_link_connect_confirm(self->notify.instance, &self->qos_tx, skb);
}

void irlap_data_indication(struct irlap_cb *self, struct sk_buff *skb,
			   int unreliable)
{
	
	skb_pull(skb, LAP_ADDR_HEADER+LAP_CTRL_HEADER);

	irlmp_link_data_indication(self->notify.instance, skb, unreliable);
}


void irlap_data_request(struct irlap_cb *self, struct sk_buff *skb,
			int unreliable)
{
	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	IRDA_DEBUG(3, "%s()\n", __func__);

	IRDA_ASSERT(skb_headroom(skb) >= (LAP_ADDR_HEADER+LAP_CTRL_HEADER),
		    return;);
	skb_push(skb, LAP_ADDR_HEADER+LAP_CTRL_HEADER);

	if (unreliable)
		skb->data[1] = UI_FRAME;
	else
		skb->data[1] = I_FRAME;

	
	skb_get(skb);

	
	skb_queue_tail(&self->txq, skb);

	if ((self->state == LAP_XMIT_P) || (self->state == LAP_XMIT_S)) {
		if((skb_queue_len(&self->txq) <= 1) && (!self->local_busy))
			irlap_do_event(self, DATA_REQUEST, skb, NULL);
	}
}

#ifdef CONFIG_IRDA_ULTRA
void irlap_unitdata_request(struct irlap_cb *self, struct sk_buff *skb)
{
	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	IRDA_DEBUG(3, "%s()\n", __func__);

	IRDA_ASSERT(skb_headroom(skb) >= (LAP_ADDR_HEADER+LAP_CTRL_HEADER),
	       return;);
	skb_push(skb, LAP_ADDR_HEADER+LAP_CTRL_HEADER);

	skb->data[0] = CBROADCAST;
	skb->data[1] = UI_FRAME;

	

	skb_queue_tail(&self->txq_ultra, skb);

	irlap_do_event(self, SEND_UI_FRAME, NULL, NULL);
}
#endif 

#ifdef CONFIG_IRDA_ULTRA
void irlap_unitdata_indication(struct irlap_cb *self, struct sk_buff *skb)
{
	IRDA_DEBUG(1, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);
	IRDA_ASSERT(skb != NULL, return;);

	
	skb_pull(skb, LAP_ADDR_HEADER+LAP_CTRL_HEADER);

	irlmp_link_unitdata_indication(self->notify.instance, skb);
}
#endif 

void irlap_disconnect_request(struct irlap_cb *self)
{
	IRDA_DEBUG(3, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	
	if (!skb_queue_empty(&self->txq)) {
		self->disconnect_pending = TRUE;
		return;
	}

	
	switch (self->state) {
	case LAP_XMIT_P:        
	case LAP_XMIT_S:        
	case LAP_CONN:          
	case LAP_RESET_WAIT:    
	case LAP_RESET_CHECK:
		irlap_do_event(self, DISCONNECT_REQUEST, NULL, NULL);
		break;
	default:
		IRDA_DEBUG(2, "%s(), disconnect pending!\n", __func__);
		self->disconnect_pending = TRUE;
		break;
	}
}

void irlap_disconnect_indication(struct irlap_cb *self, LAP_REASON reason)
{
	IRDA_DEBUG(1, "%s(), reason=%s\n", __func__, lap_reasons[reason]);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	
	irlap_flush_all_queues(self);

	switch (reason) {
	case LAP_RESET_INDICATION:
		IRDA_DEBUG(1, "%s(), Sending reset request!\n", __func__);
		irlap_do_event(self, RESET_REQUEST, NULL, NULL);
		break;
	case LAP_NO_RESPONSE:	   
	case LAP_DISC_INDICATION:  
	case LAP_FOUND_NONE:       
	case LAP_MEDIA_BUSY:
		irlmp_link_disconnect_indication(self->notify.instance, self,
						 reason, NULL);
		break;
	default:
		IRDA_ERROR("%s: Unknown reason %d\n", __func__, reason);
	}
}

void irlap_discovery_request(struct irlap_cb *self, discovery_t *discovery)
{
	struct irlap_info info;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);
	IRDA_ASSERT(discovery != NULL, return;);

	IRDA_DEBUG(4, "%s(), nslots = %d\n", __func__, discovery->nslots);

	IRDA_ASSERT((discovery->nslots == 1) || (discovery->nslots == 6) ||
		    (discovery->nslots == 8) || (discovery->nslots == 16),
		    return;);

	
	if (self->state != LAP_NDM) {
		IRDA_DEBUG(4, "%s(), discovery only possible in NDM mode\n",
			   __func__);
		irlap_discovery_confirm(self, NULL);
		return;
	}

	if (self->discovery_log != NULL) {
		hashbin_delete(self->discovery_log, (FREE_FUNC) kfree);
		self->discovery_log = NULL;
	}

	
	self->discovery_log = hashbin_new(HB_NOLOCK);

	if (self->discovery_log == NULL) {
		IRDA_WARNING("%s(), Unable to allocate discovery log!\n",
			     __func__);
		return;
	}

	info.S = discovery->nslots; 
	info.s = 0; 

	self->discovery_cmd = discovery;
	info.discovery = discovery;

	
	self->slot_timeout = sysctl_slot_timeout * HZ / 1000;

	irlap_do_event(self, DISCOVERY_REQUEST, NULL, &info);
}

void irlap_discovery_confirm(struct irlap_cb *self, hashbin_t *discovery_log)
{
	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	IRDA_ASSERT(self->notify.instance != NULL, return;);

	if (discovery_log)
		irda_device_set_media_busy(self->netdev, FALSE);

	
	irlmp_link_discovery_confirm(self->notify.instance, discovery_log);
}

void irlap_discovery_indication(struct irlap_cb *self, discovery_t *discovery)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);
	IRDA_ASSERT(discovery != NULL, return;);

	IRDA_ASSERT(self->notify.instance != NULL, return;);

	irda_device_set_media_busy(self->netdev, SMALL);

	irlmp_link_discovery_indication(self->notify.instance, discovery);
}

void irlap_status_indication(struct irlap_cb *self, int quality_of_link)
{
	switch (quality_of_link) {
	case STATUS_NO_ACTIVITY:
		IRDA_MESSAGE("IrLAP, no activity on link!\n");
		break;
	case STATUS_NOISY:
		IRDA_MESSAGE("IrLAP, noisy link!\n");
		break;
	default:
		break;
	}
	irlmp_status_indication(self->notify.instance,
				quality_of_link, LOCK_NO_CHANGE);
}

void irlap_reset_indication(struct irlap_cb *self)
{
	IRDA_DEBUG(1, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	if (self->state == LAP_RESET_WAIT)
		irlap_do_event(self, RESET_REQUEST, NULL, NULL);
	else
		irlap_do_event(self, RESET_RESPONSE, NULL, NULL);
}

void irlap_reset_confirm(void)
{
	IRDA_DEBUG(1, "%s()\n", __func__);
}

int irlap_generate_rand_time_slot(int S, int s)
{
	static int rand;
	int slot;

	IRDA_ASSERT((S - s) > 0, return 0;);

	rand += jiffies;
	rand ^= (rand << 12);
	rand ^= (rand >> 20);

	slot = s + rand % (S-s);

	IRDA_ASSERT((slot >= s) || (slot < S), return 0;);

	return slot;
}

void irlap_update_nr_received(struct irlap_cb *self, int nr)
{
	struct sk_buff *skb = NULL;
	int count = 0;


	if (nr == self->vs) {
		while ((skb = skb_dequeue(&self->wx_list)) != NULL) {
			dev_kfree_skb(skb);
		}
		
		self->va = nr - 1;
	} else {
		
		while ((skb_peek(&self->wx_list) != NULL) &&
		       (((self->va+1) % 8) != nr))
		{
			skb = skb_dequeue(&self->wx_list);
			dev_kfree_skb(skb);

			self->va = (self->va + 1) % 8;
			count++;
		}
	}

	
	self->window = self->window_size - skb_queue_len(&self->wx_list);
}

int irlap_validate_ns_received(struct irlap_cb *self, int ns)
{
	
	if (ns == self->vr)
		return NS_EXPECTED;
	return NS_UNEXPECTED;

	
}
int irlap_validate_nr_received(struct irlap_cb *self, int nr)
{
	
	if (nr == self->vs) {
		IRDA_DEBUG(4, "%s(), expected!\n", __func__);
		return NR_EXPECTED;
	}

	if (self->va < self->vs) {
		if ((nr >= self->va) && (nr <= self->vs))
			return NR_UNEXPECTED;
	} else {
		if ((nr >= self->va) || (nr <= self->vs))
			return NR_UNEXPECTED;
	}

	
	return NR_INVALID;
}

void irlap_initiate_connection_state(struct irlap_cb *self)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	
	self->vs = self->vr = 0;

	
	self->va = 7;

	self->window = 1;

	self->remote_busy = FALSE;
	self->retry_count = 0;
}

void irlap_wait_min_turn_around(struct irlap_cb *self, struct qos_info *qos)
{
	__u32 min_turn_time;
	__u32 speed;

	
	speed = qos->baud_rate.value;
	min_turn_time = qos->min_turn_time.value;

	
	if (speed > 115200) {
		self->mtt_required = min_turn_time;
		return;
	}

	self->xbofs_delay = irlap_min_turn_time_in_bytes(speed, min_turn_time);
}

void irlap_flush_all_queues(struct irlap_cb *self)
{
	struct sk_buff* skb;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	
	while ((skb = skb_dequeue(&self->txq)) != NULL)
		dev_kfree_skb(skb);

	while ((skb = skb_dequeue(&self->txq_ultra)) != NULL)
		dev_kfree_skb(skb);

	
	while ((skb = skb_dequeue(&self->wx_list)) != NULL)
		dev_kfree_skb(skb);
}

static void irlap_change_speed(struct irlap_cb *self, __u32 speed, int now)
{
	struct sk_buff *skb;

	IRDA_DEBUG(0, "%s(), setting speed to %d\n", __func__, speed);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	self->speed = speed;

	
	if (now) {
		
		skb = alloc_skb(0, GFP_ATOMIC);
		if (skb)
			irlap_queue_xmit(self, skb);
	}
}

static void irlap_init_qos_capabilities(struct irlap_cb *self,
					struct qos_info *qos_user)
{
	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);
	IRDA_ASSERT(self->netdev != NULL, return;);

	
	irda_init_max_qos_capabilies(&self->qos_rx);

	
	irda_qos_compute_intersection(&self->qos_rx, self->qos_dev);

	if (qos_user) {
		IRDA_DEBUG(1, "%s(), Found user specified QoS!\n", __func__);

		if (qos_user->baud_rate.bits)
			self->qos_rx.baud_rate.bits &= qos_user->baud_rate.bits;

		if (qos_user->max_turn_time.bits)
			self->qos_rx.max_turn_time.bits &= qos_user->max_turn_time.bits;
		if (qos_user->data_size.bits)
			self->qos_rx.data_size.bits &= qos_user->data_size.bits;

		if (qos_user->link_disc_time.bits)
			self->qos_rx.link_disc_time.bits &= qos_user->link_disc_time.bits;
	}

	
	self->qos_rx.max_turn_time.bits &= 0x01;

	
	

	irda_qos_bits_to_value(&self->qos_rx);
}

void irlap_apply_default_connection_parameters(struct irlap_cb *self)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	
	self->next_bofs   = 12;
	self->bofs_count  = 12;

	
	irlap_change_speed(self, 9600, TRUE);

	
	irda_device_set_media_busy(self->netdev, TRUE);

	while ((self->caddr == 0x00) || (self->caddr == 0xfe)) {
		get_random_bytes(&self->caddr, sizeof(self->caddr));
		self->caddr &= 0xfe;
	}

	
	self->slot_timeout = sysctl_slot_timeout;
	self->final_timeout = FINAL_TIMEOUT;
	self->poll_timeout = POLL_TIMEOUT;
	self->wd_timeout = WD_TIMEOUT;

	
	self->qos_tx.baud_rate.value = 9600;
	self->qos_rx.baud_rate.value = 9600;
	self->qos_tx.max_turn_time.value = 0;
	self->qos_rx.max_turn_time.value = 0;
	self->qos_tx.min_turn_time.value = 0;
	self->qos_rx.min_turn_time.value = 0;
	self->qos_tx.data_size.value = 64;
	self->qos_rx.data_size.value = 64;
	self->qos_tx.window_size.value = 1;
	self->qos_rx.window_size.value = 1;
	self->qos_tx.additional_bofs.value = 12;
	self->qos_rx.additional_bofs.value = 12;
	self->qos_tx.link_disc_time.value = 0;
	self->qos_rx.link_disc_time.value = 0;

	irlap_flush_all_queues(self);

	self->disconnect_pending = FALSE;
	self->connect_pending = FALSE;
}

void irlap_apply_connection_parameters(struct irlap_cb *self, int now)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	
	self->next_bofs   = self->qos_tx.additional_bofs.value;
	if (now)
		self->bofs_count = self->next_bofs;

	
	irlap_change_speed(self, self->qos_tx.baud_rate.value, now);

	self->window_size = self->qos_tx.window_size.value;
	self->window      = self->qos_tx.window_size.value;

#ifdef CONFIG_IRDA_DYNAMIC_WINDOW
	self->line_capacity =
		irlap_max_line_capacity(self->qos_tx.baud_rate.value,
					self->qos_tx.max_turn_time.value);
	self->bytes_left = self->line_capacity;
#endif 


	IRDA_ASSERT(self->qos_tx.max_turn_time.value != 0, return;);
	IRDA_ASSERT(self->qos_rx.max_turn_time.value != 0, return;);
	self->poll_timeout = self->qos_tx.max_turn_time.value * HZ / 1000;
	self->final_timeout = self->qos_rx.max_turn_time.value * HZ / 1000;
	self->wd_timeout = self->final_timeout * 2;


	if (self->qos_tx.link_disc_time.value == sysctl_warn_noreply_time)
		self->N1 = -2; 
	else
		self->N1 = sysctl_warn_noreply_time * 1000 /
		  self->qos_rx.max_turn_time.value;

	IRDA_DEBUG(4, "Setting N1 = %d\n", self->N1);

	
	self->N2 = self->qos_tx.link_disc_time.value * 1000 /
		self->qos_rx.max_turn_time.value;
	IRDA_DEBUG(4, "Setting N2 = %d\n", self->N2);
}

#ifdef CONFIG_PROC_FS
struct irlap_iter_state {
	int id;
};

static void *irlap_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct irlap_iter_state *iter = seq->private;
	struct irlap_cb *self;

	
	spin_lock_irq(&irlap->hb_spinlock);
	iter->id = 0;

	for (self = (struct irlap_cb *) hashbin_get_first(irlap);
	     self; self = (struct irlap_cb *) hashbin_get_next(irlap)) {
		if (iter->id == *pos)
			break;
		++iter->id;
	}

	return self;
}

static void *irlap_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct irlap_iter_state *iter = seq->private;

	++*pos;
	++iter->id;
	return (void *) hashbin_get_next(irlap);
}

static void irlap_seq_stop(struct seq_file *seq, void *v)
{
	spin_unlock_irq(&irlap->hb_spinlock);
}

static int irlap_seq_show(struct seq_file *seq, void *v)
{
	const struct irlap_iter_state *iter = seq->private;
	const struct irlap_cb *self = v;

	IRDA_ASSERT(self->magic == LAP_MAGIC, return -EINVAL;);

	seq_printf(seq, "irlap%d ", iter->id);
	seq_printf(seq, "state: %s\n",
		   irlap_state[self->state]);

	seq_printf(seq, "  device name: %s, ",
		   (self->netdev) ? self->netdev->name : "bug");
	seq_printf(seq, "hardware name: %s\n", self->hw_name);

	seq_printf(seq, "  caddr: %#02x, ", self->caddr);
	seq_printf(seq, "saddr: %#08x, ", self->saddr);
	seq_printf(seq, "daddr: %#08x\n", self->daddr);

	seq_printf(seq, "  win size: %d, ",
		   self->window_size);
	seq_printf(seq, "win: %d, ", self->window);
#ifdef CONFIG_IRDA_DYNAMIC_WINDOW
	seq_printf(seq, "line capacity: %d, ",
		   self->line_capacity);
	seq_printf(seq, "bytes left: %d\n", self->bytes_left);
#endif 
	seq_printf(seq, "  tx queue len: %d ",
		   skb_queue_len(&self->txq));
	seq_printf(seq, "win queue len: %d ",
		   skb_queue_len(&self->wx_list));
	seq_printf(seq, "rbusy: %s", self->remote_busy ?
		   "TRUE" : "FALSE");
	seq_printf(seq, " mbusy: %s\n", self->media_busy ?
		   "TRUE" : "FALSE");

	seq_printf(seq, "  retrans: %d ", self->retry_count);
	seq_printf(seq, "vs: %d ", self->vs);
	seq_printf(seq, "vr: %d ", self->vr);
	seq_printf(seq, "va: %d\n", self->va);

	seq_printf(seq, "  qos\tbps\tmaxtt\tdsize\twinsize\taddbofs\tmintt\tldisc\tcomp\n");

	seq_printf(seq, "  tx\t%d\t",
		   self->qos_tx.baud_rate.value);
	seq_printf(seq, "%d\t",
		   self->qos_tx.max_turn_time.value);
	seq_printf(seq, "%d\t",
		   self->qos_tx.data_size.value);
	seq_printf(seq, "%d\t",
		   self->qos_tx.window_size.value);
	seq_printf(seq, "%d\t",
		   self->qos_tx.additional_bofs.value);
	seq_printf(seq, "%d\t",
		   self->qos_tx.min_turn_time.value);
	seq_printf(seq, "%d\t",
		   self->qos_tx.link_disc_time.value);
	seq_printf(seq, "\n");

	seq_printf(seq, "  rx\t%d\t",
		   self->qos_rx.baud_rate.value);
	seq_printf(seq, "%d\t",
		   self->qos_rx.max_turn_time.value);
	seq_printf(seq, "%d\t",
		   self->qos_rx.data_size.value);
	seq_printf(seq, "%d\t",
		   self->qos_rx.window_size.value);
	seq_printf(seq, "%d\t",
		   self->qos_rx.additional_bofs.value);
	seq_printf(seq, "%d\t",
		   self->qos_rx.min_turn_time.value);
	seq_printf(seq, "%d\n",
		   self->qos_rx.link_disc_time.value);

	return 0;
}

static const struct seq_operations irlap_seq_ops = {
	.start  = irlap_seq_start,
	.next   = irlap_seq_next,
	.stop   = irlap_seq_stop,
	.show   = irlap_seq_show,
};

static int irlap_seq_open(struct inode *inode, struct file *file)
{
	if (irlap == NULL)
		return -EINVAL;

	return seq_open_private(file, &irlap_seq_ops,
			sizeof(struct irlap_iter_state));
}

const struct file_operations irlap_seq_fops = {
	.owner		= THIS_MODULE,
	.open           = irlap_seq_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release	= seq_release_private,
};

#endif 
