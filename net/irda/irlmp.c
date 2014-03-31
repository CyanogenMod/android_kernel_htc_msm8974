/*********************************************************************
 *
 * Filename:      irlmp.c
 * Version:       1.0
 * Description:   IrDA Link Management Protocol (LMP) layer
 * Status:        Stable.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sun Aug 17 20:54:32 1997
 * Modified at:   Wed Jan  5 11:26:03 2000
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/random.h>
#include <linux/seq_file.h>

#include <net/irda/irda.h>
#include <net/irda/timer.h>
#include <net/irda/qos.h>
#include <net/irda/irlap.h>
#include <net/irda/iriap.h>
#include <net/irda/irlmp.h>
#include <net/irda/irlmp_frame.h>

#include <asm/unaligned.h>

static __u8 irlmp_find_free_slsap(void);
static int irlmp_slsap_inuse(__u8 slsap_sel);

struct irlmp_cb *irlmp = NULL;

int  sysctl_discovery         = 0;
int  sysctl_discovery_timeout = 3; 
int  sysctl_discovery_slots   = 6; 
int  sysctl_lap_keepalive_time = LM_IDLE_TIMEOUT * 1000 / HZ;
char sysctl_devname[65];

const char *irlmp_reasons[] = {
	"ERROR, NOT USED",
	"LM_USER_REQUEST",
	"LM_LAP_DISCONNECT",
	"LM_CONNECT_FAILURE",
	"LM_LAP_RESET",
	"LM_INIT_DISCONNECT",
	"ERROR, NOT USED",
};

int __init irlmp_init(void)
{
	IRDA_DEBUG(1, "%s()\n", __func__);
	
	irlmp = kzalloc( sizeof(struct irlmp_cb), GFP_KERNEL);
	if (irlmp == NULL)
		return -ENOMEM;

	irlmp->magic = LMP_MAGIC;

	irlmp->clients = hashbin_new(HB_LOCK);
	irlmp->services = hashbin_new(HB_LOCK);
	irlmp->links = hashbin_new(HB_LOCK);
	irlmp->unconnected_lsaps = hashbin_new(HB_LOCK);
	irlmp->cachelog = hashbin_new(HB_NOLOCK);

	if ((irlmp->clients == NULL) ||
	    (irlmp->services == NULL) ||
	    (irlmp->links == NULL) ||
	    (irlmp->unconnected_lsaps == NULL) ||
	    (irlmp->cachelog == NULL)) {
		return -ENOMEM;
	}

	spin_lock_init(&irlmp->cachelog->hb_spinlock);

	irlmp->last_lsap_sel = 0x0f; 
	strcpy(sysctl_devname, "Linux");

	init_timer(&irlmp->discovery_timer);

	
	if (sysctl_discovery)
		irlmp_start_discovery_timer(irlmp,
					    sysctl_discovery_timeout*HZ);

	return 0;
}

void irlmp_cleanup(void)
{
	
	IRDA_ASSERT(irlmp != NULL, return;);
	IRDA_ASSERT(irlmp->magic == LMP_MAGIC, return;);

	del_timer(&irlmp->discovery_timer);

	hashbin_delete(irlmp->links, (FREE_FUNC) kfree);
	hashbin_delete(irlmp->unconnected_lsaps, (FREE_FUNC) kfree);
	hashbin_delete(irlmp->clients, (FREE_FUNC) kfree);
	hashbin_delete(irlmp->services, (FREE_FUNC) kfree);
	hashbin_delete(irlmp->cachelog, (FREE_FUNC) kfree);

	
	kfree(irlmp);
	irlmp = NULL;
}

struct lsap_cb *irlmp_open_lsap(__u8 slsap_sel, notify_t *notify, __u8 pid)
{
	struct lsap_cb *self;

	IRDA_ASSERT(notify != NULL, return NULL;);
	IRDA_ASSERT(irlmp != NULL, return NULL;);
	IRDA_ASSERT(irlmp->magic == LMP_MAGIC, return NULL;);
	IRDA_ASSERT(notify->instance != NULL, return NULL;);

	
	if (slsap_sel == LSAP_ANY) {
		slsap_sel = irlmp_find_free_slsap();
		if (!slsap_sel)
			return NULL;
	} else if (irlmp_slsap_inuse(slsap_sel))
		return NULL;

	
	self = kzalloc(sizeof(struct lsap_cb), GFP_ATOMIC);
	if (self == NULL) {
		IRDA_ERROR("%s: can't allocate memory\n", __func__);
		return NULL;
	}

	self->magic = LMP_LSAP_MAGIC;
	self->slsap_sel = slsap_sel;

	
	if (slsap_sel == LSAP_CONNLESS) {
#ifdef CONFIG_IRDA_ULTRA
		self->dlsap_sel = LSAP_CONNLESS;
		self->pid = pid;
#endif 
	} else
		self->dlsap_sel = LSAP_ANY;
	

	init_timer(&self->watchdog_timer);

	self->notify = *notify;

	self->lsap_state = LSAP_DISCONNECTED;

	
	hashbin_insert(irlmp->unconnected_lsaps, (irda_queue_t *) self,
		       (long) self, NULL);

	return self;
}
EXPORT_SYMBOL(irlmp_open_lsap);

static void __irlmp_close_lsap(struct lsap_cb *self)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return;);

	self->magic = 0;
	del_timer(&self->watchdog_timer); 

	if (self->conn_skb)
		dev_kfree_skb(self->conn_skb);

	kfree(self);
}

void irlmp_close_lsap(struct lsap_cb *self)
{
	struct lap_cb *lap;
	struct lsap_cb *lsap = NULL;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return;);

	lap = self->lap;
	if (lap) {
		IRDA_ASSERT(lap->magic == LMP_LAP_MAGIC, return;);
		if(self->lsap_state != LSAP_DISCONNECTED) {
			self->lsap_state = LSAP_DISCONNECTED;
			irlmp_do_lap_event(self->lap,
					   LM_LAP_DISCONNECT_REQUEST, NULL);
		}
		
		lsap = hashbin_remove(lap->lsaps, (long) self, NULL);
#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
		lap->cache.valid = FALSE;
#endif
	}
	self->lap = NULL;
	
	if (!lsap) {
		lsap = hashbin_remove(irlmp->unconnected_lsaps, (long) self,
				      NULL);
	}
	if (!lsap) {
		IRDA_DEBUG(0,
		     "%s(), Looks like somebody has removed me already!\n",
			   __func__);
		return;
	}
	__irlmp_close_lsap(self);
}
EXPORT_SYMBOL(irlmp_close_lsap);

void irlmp_register_link(struct irlap_cb *irlap, __u32 saddr, notify_t *notify)
{
	struct lap_cb *lap;

	IRDA_ASSERT(irlmp != NULL, return;);
	IRDA_ASSERT(irlmp->magic == LMP_MAGIC, return;);
	IRDA_ASSERT(notify != NULL, return;);

	lap = kzalloc(sizeof(struct lap_cb), GFP_KERNEL);
	if (lap == NULL) {
		IRDA_ERROR("%s: unable to kmalloc\n", __func__);
		return;
	}

	lap->irlap = irlap;
	lap->magic = LMP_LAP_MAGIC;
	lap->saddr = saddr;
	lap->daddr = DEV_ADDR_ANY;
#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
	lap->cache.valid = FALSE;
#endif
	lap->lsaps = hashbin_new(HB_LOCK);
	if (lap->lsaps == NULL) {
		IRDA_WARNING("%s(), unable to kmalloc lsaps\n", __func__);
		kfree(lap);
		return;
	}

	lap->lap_state = LAP_STANDBY;

	init_timer(&lap->idle_timer);

	hashbin_insert(irlmp->links, (irda_queue_t *) lap, lap->saddr, NULL);

	irda_notify_init(notify);
	notify->instance = lap;
}

void irlmp_unregister_link(__u32 saddr)
{
	struct lap_cb *link;

	IRDA_DEBUG(4, "%s()\n", __func__);

	link = hashbin_remove(irlmp->links, saddr, NULL);
	if (link) {
		IRDA_ASSERT(link->magic == LMP_LAP_MAGIC, return;);

		
		link->reason = LAP_DISC_INDICATION;
		link->daddr = DEV_ADDR_ANY;
		irlmp_do_lap_event(link, LM_LAP_DISCONNECT_INDICATION, NULL);

		
		irlmp_expire_discoveries(irlmp->cachelog, link->saddr, TRUE);

		
		del_timer(&link->idle_timer);
		link->magic = 0;
		hashbin_delete(link->lsaps, (FREE_FUNC) __irlmp_close_lsap);
		kfree(link);
	}
}

int irlmp_connect_request(struct lsap_cb *self, __u8 dlsap_sel,
			  __u32 saddr, __u32 daddr,
			  struct qos_info *qos, struct sk_buff *userdata)
{
	struct sk_buff *tx_skb = userdata;
	struct lap_cb *lap;
	struct lsap_cb *lsap;
	int ret;

	IRDA_ASSERT(self != NULL, return -EBADR;);
	IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return -EBADR;);

	IRDA_DEBUG(2,
	      "%s(), slsap_sel=%02x, dlsap_sel=%02x, saddr=%08x, daddr=%08x\n",
	      __func__, self->slsap_sel, dlsap_sel, saddr, daddr);

	if (test_bit(0, &self->connected)) {
		ret = -EISCONN;
		goto err;
	}

	
	if (!daddr) {
		ret = -EINVAL;
		goto err;
	}

	
	if (tx_skb == NULL) {
		tx_skb = alloc_skb(LMP_MAX_HEADER, GFP_ATOMIC);
		if (!tx_skb)
			return -ENOMEM;

		skb_reserve(tx_skb, LMP_MAX_HEADER);
	}

	
	IRDA_ASSERT(skb_headroom(tx_skb) >= LMP_CONTROL_HEADER, return -1;);
	skb_push(tx_skb, LMP_CONTROL_HEADER);

	self->dlsap_sel = dlsap_sel;

	if ((!saddr) || (saddr == DEV_ADDR_ANY)) {
		discovery_t *discovery;
		unsigned long flags;

		spin_lock_irqsave(&irlmp->cachelog->hb_spinlock, flags);
		if (daddr != DEV_ADDR_ANY)
			discovery = hashbin_find(irlmp->cachelog, daddr, NULL);
		else {
			IRDA_DEBUG(2, "%s(), no daddr\n", __func__);
			discovery = (discovery_t *)
				hashbin_get_first(irlmp->cachelog);
		}

		if (discovery) {
			saddr = discovery->data.saddr;
			daddr = discovery->data.daddr;
		}
		spin_unlock_irqrestore(&irlmp->cachelog->hb_spinlock, flags);
	}
	lap = hashbin_lock_find(irlmp->links, saddr, NULL);
	if (lap == NULL) {
		IRDA_DEBUG(1, "%s(), Unable to find a usable link!\n", __func__);
		ret = -EHOSTUNREACH;
		goto err;
	}

	
	if (lap->daddr == DEV_ADDR_ANY)
		lap->daddr = daddr;
	else if (lap->daddr != daddr) {
		
		if (HASHBIN_GET_SIZE(lap->lsaps) == 0) {
			IRDA_DEBUG(0, "%s(), sorry, but I'm waiting for LAP to timeout!\n", __func__);
			ret = -EAGAIN;
			goto err;
		}

		IRDA_DEBUG(0, "%s(), sorry, but link is busy!\n", __func__);
		ret = -EBUSY;
		goto err;
	}

	self->lap = lap;

	lsap = hashbin_remove(irlmp->unconnected_lsaps, (long) self, NULL);

	IRDA_ASSERT(lsap != NULL, return -1;);
	IRDA_ASSERT(lsap->magic == LMP_LSAP_MAGIC, return -1;);
	IRDA_ASSERT(lsap->lap != NULL, return -1;);
	IRDA_ASSERT(lsap->lap->magic == LMP_LAP_MAGIC, return -1;);

	hashbin_insert(self->lap->lsaps, (irda_queue_t *) self, (long) self,
		       NULL);

	set_bit(0, &self->connected);	

	if (qos)
		self->qos = *qos;

	irlmp_do_lsap_event(self, LM_CONNECT_REQUEST, tx_skb);

	
	dev_kfree_skb(tx_skb);

	return 0;

err:
	
	if(tx_skb)
		dev_kfree_skb(tx_skb);
	return ret;
}
EXPORT_SYMBOL(irlmp_connect_request);

void irlmp_connect_indication(struct lsap_cb *self, struct sk_buff *skb)
{
	int max_seg_size;
	int lap_header_size;
	int max_header_size;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return;);
	IRDA_ASSERT(skb != NULL, return;);
	IRDA_ASSERT(self->lap != NULL, return;);

	IRDA_DEBUG(2, "%s(), slsap_sel=%02x, dlsap_sel=%02x\n",
		   __func__, self->slsap_sel, self->dlsap_sel);


	self->qos = *self->lap->qos;

	max_seg_size = self->lap->qos->data_size.value-LMP_HEADER;
	lap_header_size = IRLAP_GET_HEADER_SIZE(self->lap->irlap);
	max_header_size = LMP_HEADER + lap_header_size;

	
	skb_pull(skb, LMP_CONTROL_HEADER);

	if (self->notify.connect_indication) {
		
		skb_get(skb);
		self->notify.connect_indication(self->notify.instance, self,
						&self->qos, max_seg_size,
						max_header_size, skb);
	}
}

int irlmp_connect_response(struct lsap_cb *self, struct sk_buff *userdata)
{
	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return -1;);
	IRDA_ASSERT(userdata != NULL, return -1;);


	IRDA_DEBUG(2, "%s(), slsap_sel=%02x, dlsap_sel=%02x\n",
		   __func__, self->slsap_sel, self->dlsap_sel);

	
	IRDA_ASSERT(skb_headroom(userdata) >= LMP_CONTROL_HEADER, return -1;);
	skb_push(userdata, LMP_CONTROL_HEADER);

	irlmp_do_lsap_event(self, LM_CONNECT_RESPONSE, userdata);

	
	dev_kfree_skb(userdata);

	return 0;
}
EXPORT_SYMBOL(irlmp_connect_response);

void irlmp_connect_confirm(struct lsap_cb *self, struct sk_buff *skb)
{
	int max_header_size;
	int lap_header_size;
	int max_seg_size;

	IRDA_DEBUG(3, "%s()\n", __func__);

	IRDA_ASSERT(skb != NULL, return;);
	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return;);
	IRDA_ASSERT(self->lap != NULL, return;);

	self->qos = *self->lap->qos;

	max_seg_size    = self->lap->qos->data_size.value-LMP_HEADER;
	lap_header_size = IRLAP_GET_HEADER_SIZE(self->lap->irlap);
	max_header_size = LMP_HEADER + lap_header_size;

	IRDA_DEBUG(2, "%s(), max_header_size=%d\n",
		   __func__, max_header_size);

	
	skb_pull(skb, LMP_CONTROL_HEADER);

	if (self->notify.connect_confirm) {
		
		skb_get(skb);
		self->notify.connect_confirm(self->notify.instance, self,
					     &self->qos, max_seg_size,
					     max_header_size, skb);
	}
}

struct lsap_cb *irlmp_dup(struct lsap_cb *orig, void *instance)
{
	struct lsap_cb *new;
	unsigned long flags;

	IRDA_DEBUG(1, "%s()\n", __func__);

	spin_lock_irqsave(&irlmp->unconnected_lsaps->hb_spinlock, flags);

	if ((!hashbin_find(irlmp->unconnected_lsaps, (long) orig, NULL)) ||
	    (orig->lap == NULL)) {
		IRDA_DEBUG(0, "%s(), invalid LSAP (wrong state)\n",
			   __func__);
		spin_unlock_irqrestore(&irlmp->unconnected_lsaps->hb_spinlock,
				       flags);
		return NULL;
	}

	
	new = kmemdup(orig, sizeof(*new), GFP_ATOMIC);
	if (!new)  {
		IRDA_DEBUG(0, "%s(), unable to kmalloc\n", __func__);
		spin_unlock_irqrestore(&irlmp->unconnected_lsaps->hb_spinlock,
				       flags);
		return NULL;
	}
	
	
	new->conn_skb = NULL;

	spin_unlock_irqrestore(&irlmp->unconnected_lsaps->hb_spinlock, flags);

	
	new->notify.instance = instance;

	init_timer(&new->watchdog_timer);

	hashbin_insert(irlmp->unconnected_lsaps, (irda_queue_t *) new,
		       (long) new, NULL);

#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
	
	new->lap->cache.valid = FALSE;
#endif 

	return new;
}

int irlmp_disconnect_request(struct lsap_cb *self, struct sk_buff *userdata)
{
	struct lsap_cb *lsap;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return -1;);
	IRDA_ASSERT(userdata != NULL, return -1;);

	if (! test_and_clear_bit(0, &self->connected)) {
		IRDA_DEBUG(0, "%s(), already disconnected!\n", __func__);
		dev_kfree_skb(userdata);
		return -1;
	}

	skb_push(userdata, LMP_CONTROL_HEADER);

	irlmp_do_lsap_event(self, LM_DISCONNECT_REQUEST, userdata);

	
	dev_kfree_skb(userdata);

	IRDA_ASSERT(self->lap != NULL, return -1;);
	IRDA_ASSERT(self->lap->magic == LMP_LAP_MAGIC, return -1;);
	IRDA_ASSERT(self->lap->lsaps != NULL, return -1;);

	lsap = hashbin_remove(self->lap->lsaps, (long) self, NULL);
#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
	self->lap->cache.valid = FALSE;
#endif

	IRDA_ASSERT(lsap != NULL, return -1;);
	IRDA_ASSERT(lsap->magic == LMP_LSAP_MAGIC, return -1;);
	IRDA_ASSERT(lsap == self, return -1;);

	hashbin_insert(irlmp->unconnected_lsaps, (irda_queue_t *) self,
		       (long) self, NULL);

	
	self->dlsap_sel = LSAP_ANY;
	self->lap = NULL;

	return 0;
}
EXPORT_SYMBOL(irlmp_disconnect_request);

void irlmp_disconnect_indication(struct lsap_cb *self, LM_REASON reason,
				 struct sk_buff *skb)
{
	struct lsap_cb *lsap;

	IRDA_DEBUG(1, "%s(), reason=%s\n", __func__, irlmp_reasons[reason]);
	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return;);

	IRDA_DEBUG(3, "%s(), slsap_sel=%02x, dlsap_sel=%02x\n",
		   __func__, self->slsap_sel, self->dlsap_sel);

	if (! test_and_clear_bit(0, &self->connected)) {
		IRDA_DEBUG(0, "%s(), already disconnected!\n", __func__);
		return;
	}

	IRDA_ASSERT(self->lap != NULL, return;);
	IRDA_ASSERT(self->lap->lsaps != NULL, return;);

	lsap = hashbin_remove(self->lap->lsaps, (long) self, NULL);
#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
	self->lap->cache.valid = FALSE;
#endif

	IRDA_ASSERT(lsap != NULL, return;);
	IRDA_ASSERT(lsap == self, return;);
	hashbin_insert(irlmp->unconnected_lsaps, (irda_queue_t *) lsap,
		       (long) lsap, NULL);

	self->dlsap_sel = LSAP_ANY;
	self->lap = NULL;

	if (self->notify.disconnect_indication) {
		
		if(skb)
			skb_get(skb);
		self->notify.disconnect_indication(self->notify.instance,
						   self, reason, skb);
	} else {
		IRDA_DEBUG(0, "%s(), no handler\n", __func__);
	}
}

void irlmp_do_expiry(void)
{
	struct lap_cb *lap;

	lap = (struct lap_cb *) hashbin_get_first(irlmp->links);
	while (lap != NULL) {
		IRDA_ASSERT(lap->magic == LMP_LAP_MAGIC, return;);

		if (lap->lap_state == LAP_STANDBY) {
			
			irlmp_expire_discoveries(irlmp->cachelog, lap->saddr,
						 FALSE);
		}
		lap = (struct lap_cb *) hashbin_get_next(irlmp->links);
	}
}

void irlmp_do_discovery(int nslots)
{
	struct lap_cb *lap;
	__u16 *data_hintsp;

	
	if ((nslots != 1) && (nslots != 6) && (nslots != 8) && (nslots != 16)){
		IRDA_WARNING("%s: invalid value for number of slots!\n",
			     __func__);
		nslots = sysctl_discovery_slots = 8;
	}

	
	data_hintsp = (__u16 *) irlmp->discovery_cmd.data.hints;
	put_unaligned(irlmp->hints.word, data_hintsp);

	irlmp->discovery_cmd.data.charset = CS_ASCII;
	strncpy(irlmp->discovery_cmd.data.info, sysctl_devname,
		NICKNAME_MAX_LEN);
	irlmp->discovery_cmd.name_len = strlen(irlmp->discovery_cmd.data.info);
	irlmp->discovery_cmd.nslots = nslots;

	lap = (struct lap_cb *) hashbin_get_first(irlmp->links);
	while (lap != NULL) {
		IRDA_ASSERT(lap->magic == LMP_LAP_MAGIC, return;);

		if (lap->lap_state == LAP_STANDBY) {
			
			irlmp_do_lap_event(lap, LM_LAP_DISCOVERY_REQUEST,
					   NULL);
		}
		lap = (struct lap_cb *) hashbin_get_next(irlmp->links);
	}
}

void irlmp_discovery_request(int nslots)
{
	
	irlmp_discovery_confirm(irlmp->cachelog, DISCOVERY_LOG);

	if (!sysctl_discovery) {
		
		if (nslots == DISCOVERY_DEFAULT_SLOTS)
			nslots = sysctl_discovery_slots;

		irlmp_do_discovery(nslots);
	}
}
EXPORT_SYMBOL(irlmp_discovery_request);

struct irda_device_info *irlmp_get_discoveries(int *pn, __u16 mask, int nslots)
{
	if (!sysctl_discovery) {
		
		if (nslots == DISCOVERY_DEFAULT_SLOTS)
			nslots = sysctl_discovery_slots;

		
		irlmp_do_discovery(nslots);
	}

	
	return irlmp_copy_discoveries(irlmp->cachelog, pn, mask, TRUE);
}
EXPORT_SYMBOL(irlmp_get_discoveries);

static inline void
irlmp_notify_client(irlmp_client_t *client,
		    hashbin_t *log, DISCOVERY_MODE mode)
{
	discinfo_t *discoveries;	
	int	number;			
	int	i;

	IRDA_DEBUG(3, "%s()\n", __func__);

	
	if (!client->disco_callback)
		return;


	discoveries = irlmp_copy_discoveries(log, &number,
					     client->hint_mask.word,
					     (mode == DISCOVERY_LOG));
	
	if (discoveries == NULL)
		return;	

	
	for(i = 0; i < number; i++)
		client->disco_callback(&(discoveries[i]), mode, client->priv);

	
	kfree(discoveries);
}

void irlmp_discovery_confirm(hashbin_t *log, DISCOVERY_MODE mode)
{
	irlmp_client_t *client;
	irlmp_client_t *client_next;

	IRDA_DEBUG(3, "%s()\n", __func__);

	IRDA_ASSERT(log != NULL, return;);

	if (!(HASHBIN_GET_SIZE(log)))
		return;

	
	client = (irlmp_client_t *) hashbin_get_first(irlmp->clients);
	while (NULL != hashbin_find_next(irlmp->clients, (long) client, NULL,
					 (void *) &client_next) ) {
		
		irlmp_notify_client(client, log, mode);

		client = client_next;
	}
}

void irlmp_discovery_expiry(discinfo_t *expiries, int number)
{
	irlmp_client_t *client;
	irlmp_client_t *client_next;
	int		i;

	IRDA_DEBUG(3, "%s()\n", __func__);

	IRDA_ASSERT(expiries != NULL, return;);

	
	client = (irlmp_client_t *) hashbin_get_first(irlmp->clients);
	while (NULL != hashbin_find_next(irlmp->clients, (long) client, NULL,
					 (void *) &client_next) ) {

		
		for(i = 0; i < number; i++) {
			
			if ((client->expir_callback) &&
			    (client->hint_mask.word &
			     get_unaligned((__u16 *)expiries[i].hints)
			     & 0x7f7f) )
				client->expir_callback(&(expiries[i]),
						       EXPIRY_TIMEOUT,
						       client->priv);
		}

		
		client = client_next;
	}
}

discovery_t *irlmp_get_discovery_response(void)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(irlmp != NULL, return NULL;);

	put_unaligned(irlmp->hints.word, (__u16 *)irlmp->discovery_rsp.data.hints);

	irlmp->discovery_rsp.data.charset = CS_ASCII;

	strncpy(irlmp->discovery_rsp.data.info, sysctl_devname,
		NICKNAME_MAX_LEN);
	irlmp->discovery_rsp.name_len = strlen(irlmp->discovery_rsp.data.info);

	return &irlmp->discovery_rsp;
}

int irlmp_data_request(struct lsap_cb *self, struct sk_buff *userdata)
{
	int	ret;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return -1;);

	
	IRDA_ASSERT(skb_headroom(userdata) >= LMP_HEADER, return -1;);
	skb_push(userdata, LMP_HEADER);

	ret = irlmp_do_lsap_event(self, LM_DATA_REQUEST, userdata);

	
	dev_kfree_skb(userdata);

	return ret;
}
EXPORT_SYMBOL(irlmp_data_request);

void irlmp_data_indication(struct lsap_cb *self, struct sk_buff *skb)
{
	
	skb_pull(skb, LMP_HEADER);

	if (self->notify.data_indication) {
		
		skb_get(skb);
		self->notify.data_indication(self->notify.instance, self, skb);
	}
}

int irlmp_udata_request(struct lsap_cb *self, struct sk_buff *userdata)
{
	int	ret;

	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(userdata != NULL, return -1;);

	
	IRDA_ASSERT(skb_headroom(userdata) >= LMP_HEADER, return -1;);
	skb_push(userdata, LMP_HEADER);

	ret = irlmp_do_lsap_event(self, LM_UDATA_REQUEST, userdata);

	
	dev_kfree_skb(userdata);

	return ret;
}

void irlmp_udata_indication(struct lsap_cb *self, struct sk_buff *skb)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return;);
	IRDA_ASSERT(skb != NULL, return;);

	
	skb_pull(skb, LMP_HEADER);

	if (self->notify.udata_indication) {
		
		skb_get(skb);
		self->notify.udata_indication(self->notify.instance, self,
					      skb);
	}
}

#ifdef CONFIG_IRDA_ULTRA
int irlmp_connless_data_request(struct lsap_cb *self, struct sk_buff *userdata,
				__u8 pid)
{
	struct sk_buff *clone_skb;
	struct lap_cb *lap;

	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(userdata != NULL, return -1;);

	
	IRDA_ASSERT(skb_headroom(userdata) >= LMP_HEADER+LMP_PID_HEADER,
		    return -1;);

	
	skb_push(userdata, LMP_PID_HEADER);
	if(self != NULL)
	  userdata->data[0] = self->pid;
	else
	  userdata->data[0] = pid;

	
	skb_push(userdata, LMP_HEADER);
	userdata->data[0] = userdata->data[1] = LSAP_CONNLESS;

	
	lap = (struct lap_cb *) hashbin_get_first(irlmp->links);
	while (lap != NULL) {
		IRDA_ASSERT(lap->magic == LMP_LAP_MAGIC, return -1;);

		clone_skb = skb_clone(userdata, GFP_ATOMIC);
		if (!clone_skb) {
			dev_kfree_skb(userdata);
			return -ENOMEM;
		}

		irlap_unitdata_request(lap->irlap, clone_skb);

		lap = (struct lap_cb *) hashbin_get_next(irlmp->links);
	}
	dev_kfree_skb(userdata);

	return 0;
}
#endif 

#ifdef CONFIG_IRDA_ULTRA
void irlmp_connless_data_indication(struct lsap_cb *self, struct sk_buff *skb)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return;);
	IRDA_ASSERT(skb != NULL, return;);

	
	skb_pull(skb, LMP_HEADER+LMP_PID_HEADER);

	if (self->notify.udata_indication) {
		
		skb_get(skb);
		self->notify.udata_indication(self->notify.instance, self,
					      skb);
	}
}
#endif 

void irlmp_status_indication(struct lap_cb *self,
			     LINK_STATUS link, LOCK_STATUS lock)
{
	struct lsap_cb *next;
	struct lsap_cb *curr;

	
	curr = (struct lsap_cb *) hashbin_get_first( self->lsaps);
	while (NULL != hashbin_find_next(self->lsaps, (long) curr, NULL,
					 (void *) &next) ) {
		IRDA_ASSERT(curr->magic == LMP_LSAP_MAGIC, return;);
		if (curr->notify.status_indication != NULL)
			curr->notify.status_indication(curr->notify.instance,
						       link, lock);
		else
			IRDA_DEBUG(2, "%s(), no handler\n", __func__);

		curr = next;
	}
}

void irlmp_flow_indication(struct lap_cb *self, LOCAL_FLOW flow)
{
	struct lsap_cb *next;
	struct lsap_cb *curr;
	int	lsap_todo;

	IRDA_ASSERT(self->magic == LMP_LAP_MAGIC, return;);
	IRDA_ASSERT(flow == FLOW_START, return;);

	lsap_todo = HASHBIN_GET_SIZE(self->lsaps);
	IRDA_DEBUG(4, "%s() : %d lsaps to scan\n", __func__, lsap_todo);

	while((lsap_todo--) &&
	      (IRLAP_GET_TX_QUEUE_LEN(self->irlap) < LAP_HIGH_THRESHOLD)) {
		
		next = self->flow_next;
		
		if(next == NULL)
			next = (struct lsap_cb *) hashbin_get_first(self->lsaps);
		
		curr = hashbin_find_next(self->lsaps, (long) next, NULL,
					 (void *) &self->flow_next);
		
		if(curr == NULL)
			break;
		IRDA_DEBUG(4, "%s() : curr is %p, next was %p and is now %p, still %d to go - queue len = %d\n", __func__, curr, next, self->flow_next, lsap_todo, IRLAP_GET_TX_QUEUE_LEN(self->irlap));

		
		if (curr->notify.flow_indication != NULL)
			curr->notify.flow_indication(curr->notify.instance,
						     curr, flow);
		else
			IRDA_DEBUG(1, "%s(), no handler\n", __func__);
	}
}

#if 0
__u8 *irlmp_hint_to_service(__u8 *hint)
{
	__u8 *service;
	int i = 0;

	service = kmalloc(16, GFP_ATOMIC);
	if (!service) {
		IRDA_DEBUG(1, "%s(), Unable to kmalloc!\n", __func__);
		return NULL;
	}

	if (!hint[0]) {
		IRDA_DEBUG(1, "<None>\n");
		kfree(service);
		return NULL;
	}
	if (hint[0] & HINT_PNP)
		IRDA_DEBUG(1, "PnP Compatible ");
	if (hint[0] & HINT_PDA)
		IRDA_DEBUG(1, "PDA/Palmtop ");
	if (hint[0] & HINT_COMPUTER)
		IRDA_DEBUG(1, "Computer ");
	if (hint[0] & HINT_PRINTER) {
		IRDA_DEBUG(1, "Printer ");
		service[i++] = S_PRINTER;
	}
	if (hint[0] & HINT_MODEM)
		IRDA_DEBUG(1, "Modem ");
	if (hint[0] & HINT_FAX)
		IRDA_DEBUG(1, "Fax ");
	if (hint[0] & HINT_LAN) {
		IRDA_DEBUG(1, "LAN Access ");
		service[i++] = S_LAN;
	}
	if (hint[0] & HINT_EXTENSION) {
		if (hint[1] & HINT_TELEPHONY) {
			IRDA_DEBUG(1, "Telephony ");
			service[i++] = S_TELEPHONY;
		} if (hint[1] & HINT_FILE_SERVER)
			IRDA_DEBUG(1, "File Server ");

		if (hint[1] & HINT_COMM) {
			IRDA_DEBUG(1, "IrCOMM ");
			service[i++] = S_COMM;
		}
		if (hint[1] & HINT_OBEX) {
			IRDA_DEBUG(1, "IrOBEX ");
			service[i++] = S_OBEX;
		}
	}
	IRDA_DEBUG(1, "\n");

	
	service[i++] = S_ANY;

	service[i] = S_END;

	return service;
}
#endif

static const __u16 service_hint_mapping[S_END][2] = {
	{ HINT_PNP,		0 },			
	{ HINT_PDA,		0 },			
	{ HINT_COMPUTER,	0 },			
	{ HINT_PRINTER,		0 },			
	{ HINT_MODEM,		0 },			
	{ HINT_FAX,		0 },			
	{ HINT_LAN,		0 },			
	{ HINT_EXTENSION,	HINT_TELEPHONY },	
	{ HINT_EXTENSION,	HINT_COMM },		
	{ HINT_EXTENSION,	HINT_OBEX },		
	{ 0xFF,			0xFF },			
};

__u16 irlmp_service_to_hint(int service)
{
	__u16_host_order hint;

	hint.byte[0] = service_hint_mapping[service][0];
	hint.byte[1] = service_hint_mapping[service][1];

	return hint.word;
}
EXPORT_SYMBOL(irlmp_service_to_hint);

void *irlmp_register_service(__u16 hints)
{
	irlmp_service_t *service;

	IRDA_DEBUG(4, "%s(), hints = %04x\n", __func__, hints);

	
	service = kmalloc(sizeof(irlmp_service_t), GFP_ATOMIC);
	if (!service) {
		IRDA_DEBUG(1, "%s(), Unable to kmalloc!\n", __func__);
		return NULL;
	}
	service->hints.word = hints;
	hashbin_insert(irlmp->services, (irda_queue_t *) service,
		       (long) service, NULL);

	irlmp->hints.word |= hints;

	return (void *)service;
}
EXPORT_SYMBOL(irlmp_register_service);

int irlmp_unregister_service(void *handle)
{
	irlmp_service_t *service;
	unsigned long flags;

	IRDA_DEBUG(4, "%s()\n", __func__);

	if (!handle)
		return -1;

	
	service = hashbin_lock_find(irlmp->services, (long) handle, NULL);
	if (!service) {
		IRDA_DEBUG(1, "%s(), Unknown service!\n", __func__);
		return -1;
	}

	hashbin_remove_this(irlmp->services, (irda_queue_t *) service);
	kfree(service);

	
	irlmp->hints.word = 0;

	
	spin_lock_irqsave(&irlmp->services->hb_spinlock, flags);
	service = (irlmp_service_t *) hashbin_get_first(irlmp->services);
	while (service) {
		irlmp->hints.word |= service->hints.word;

		service = (irlmp_service_t *)hashbin_get_next(irlmp->services);
	}
	spin_unlock_irqrestore(&irlmp->services->hb_spinlock, flags);
	return 0;
}
EXPORT_SYMBOL(irlmp_unregister_service);

void *irlmp_register_client(__u16 hint_mask, DISCOVERY_CALLBACK1 disco_clb,
			    DISCOVERY_CALLBACK2 expir_clb, void *priv)
{
	irlmp_client_t *client;

	IRDA_DEBUG(1, "%s()\n", __func__);
	IRDA_ASSERT(irlmp != NULL, return NULL;);

	
	client = kmalloc(sizeof(irlmp_client_t), GFP_ATOMIC);
	if (!client) {
		IRDA_DEBUG( 1, "%s(), Unable to kmalloc!\n", __func__);
		return NULL;
	}

	
	client->hint_mask.word = hint_mask;
	client->disco_callback = disco_clb;
	client->expir_callback = expir_clb;
	client->priv = priv;

	hashbin_insert(irlmp->clients, (irda_queue_t *) client,
		       (long) client, NULL);

	return (void *) client;
}
EXPORT_SYMBOL(irlmp_register_client);

int irlmp_update_client(void *handle, __u16 hint_mask,
			DISCOVERY_CALLBACK1 disco_clb,
			DISCOVERY_CALLBACK2 expir_clb, void *priv)
{
	irlmp_client_t *client;

	if (!handle)
		return -1;

	client = hashbin_lock_find(irlmp->clients, (long) handle, NULL);
	if (!client) {
		IRDA_DEBUG(1, "%s(), Unknown client!\n", __func__);
		return -1;
	}

	client->hint_mask.word = hint_mask;
	client->disco_callback = disco_clb;
	client->expir_callback = expir_clb;
	client->priv = priv;

	return 0;
}
EXPORT_SYMBOL(irlmp_update_client);

int irlmp_unregister_client(void *handle)
{
	struct irlmp_client *client;

	IRDA_DEBUG(4, "%s()\n", __func__);

	if (!handle)
		return -1;

	
	client = hashbin_lock_find(irlmp->clients, (long) handle, NULL);
	if (!client) {
		IRDA_DEBUG(1, "%s(), Unknown client!\n", __func__);
		return -1;
	}

	IRDA_DEBUG(4, "%s(), removing client!\n", __func__);
	hashbin_remove_this(irlmp->clients, (irda_queue_t *) client);
	kfree(client);

	return 0;
}
EXPORT_SYMBOL(irlmp_unregister_client);

static int irlmp_slsap_inuse(__u8 slsap_sel)
{
	struct lsap_cb *self;
	struct lap_cb *lap;
	unsigned long flags;

	IRDA_ASSERT(irlmp != NULL, return TRUE;);
	IRDA_ASSERT(irlmp->magic == LMP_MAGIC, return TRUE;);
	IRDA_ASSERT(slsap_sel != LSAP_ANY, return TRUE;);

	IRDA_DEBUG(4, "%s()\n", __func__);

#ifdef CONFIG_IRDA_ULTRA
	
	if (slsap_sel == LSAP_CONNLESS)
		return FALSE;
#endif 

	
	if (slsap_sel > LSAP_MAX)
		return TRUE;

	spin_lock_irqsave_nested(&irlmp->links->hb_spinlock, flags,
			SINGLE_DEPTH_NESTING);
	lap = (struct lap_cb *) hashbin_get_first(irlmp->links);
	while (lap != NULL) {
		IRDA_ASSERT(lap->magic == LMP_LAP_MAGIC, goto errlap;);

		spin_lock(&lap->lsaps->hb_spinlock);

		
		self = (struct lsap_cb *) hashbin_get_first(lap->lsaps);
		while (self != NULL) {
			IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC,
				    goto errlsap;);

			if ((self->slsap_sel == slsap_sel)) {
				IRDA_DEBUG(4, "Source LSAP selector=%02x in use\n",
					   self->slsap_sel);
				goto errlsap;
			}
			self = (struct lsap_cb*) hashbin_get_next(lap->lsaps);
		}
		spin_unlock(&lap->lsaps->hb_spinlock);

		
		lap = (struct lap_cb *) hashbin_get_next(irlmp->links);
	}
	spin_unlock_irqrestore(&irlmp->links->hb_spinlock, flags);

	spin_lock_irqsave(&irlmp->unconnected_lsaps->hb_spinlock, flags);

	self = (struct lsap_cb *) hashbin_get_first(irlmp->unconnected_lsaps);
	while (self != NULL) {
		IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, goto erruncon;);
		if ((self->slsap_sel == slsap_sel)) {
			IRDA_DEBUG(4, "Source LSAP selector=%02x in use (unconnected)\n",
				   self->slsap_sel);
			goto erruncon;
		}
		self = (struct lsap_cb*) hashbin_get_next(irlmp->unconnected_lsaps);
	}
	spin_unlock_irqrestore(&irlmp->unconnected_lsaps->hb_spinlock, flags);

	return FALSE;

errlsap:
	spin_unlock(&lap->lsaps->hb_spinlock);
IRDA_ASSERT_LABEL(errlap:)
	spin_unlock_irqrestore(&irlmp->links->hb_spinlock, flags);
	return TRUE;

erruncon:
	spin_unlock_irqrestore(&irlmp->unconnected_lsaps->hb_spinlock, flags);
	return TRUE;
}

static __u8 irlmp_find_free_slsap(void)
{
	__u8 lsap_sel;
	int wrapped = 0;

	IRDA_ASSERT(irlmp != NULL, return -1;);
	IRDA_ASSERT(irlmp->magic == LMP_MAGIC, return -1;);


	do {
		irlmp->last_lsap_sel++;

		
		if (irlmp->last_lsap_sel > LSAP_MAX) {
			
			irlmp->last_lsap_sel = 0x10;

			
			if (wrapped++) {
				IRDA_ERROR("%s: no more free LSAPs !\n",
					   __func__);
				return 0;
			}
		}

	} while (irlmp_slsap_inuse(irlmp->last_lsap_sel));

	
	lsap_sel = irlmp->last_lsap_sel;
	IRDA_DEBUG(4, "%s(), found free lsap_sel=%02x\n",
		   __func__, lsap_sel);

	return lsap_sel;
}

LM_REASON irlmp_convert_lap_reason( LAP_REASON lap_reason)
{
	int reason = LM_LAP_DISCONNECT;

	switch (lap_reason) {
	case LAP_DISC_INDICATION: 
		IRDA_DEBUG( 1, "%s(), LAP_DISC_INDICATION\n", __func__);
		reason = LM_USER_REQUEST;
		break;
	case LAP_NO_RESPONSE:    
		IRDA_DEBUG( 1, "%s(), LAP_NO_RESPONSE\n", __func__);
		reason = LM_LAP_DISCONNECT;
		break;
	case LAP_RESET_INDICATION:
		IRDA_DEBUG( 1, "%s(), LAP_RESET_INDICATION\n", __func__);
		reason = LM_LAP_RESET;
		break;
	case LAP_FOUND_NONE:
	case LAP_MEDIA_BUSY:
	case LAP_PRIMARY_CONFLICT:
		IRDA_DEBUG(1, "%s(), LAP_FOUND_NONE, LAP_MEDIA_BUSY or LAP_PRIMARY_CONFLICT\n", __func__);
		reason = LM_CONNECT_FAILURE;
		break;
	default:
		IRDA_DEBUG(1, "%s(), Unknown IrLAP disconnect reason %d!\n",
			   __func__, lap_reason);
		reason = LM_LAP_DISCONNECT;
		break;
	}

	return reason;
}

#ifdef CONFIG_PROC_FS

struct irlmp_iter_state {
	hashbin_t *hashbin;
};

#define LSAP_START_TOKEN	((void *)1)
#define LINK_START_TOKEN	((void *)2)

static void *irlmp_seq_hb_idx(struct irlmp_iter_state *iter, loff_t *off)
{
	void *element;

	spin_lock_irq(&iter->hashbin->hb_spinlock);
	for (element = hashbin_get_first(iter->hashbin);
	     element != NULL;
	     element = hashbin_get_next(iter->hashbin)) {
		if (!off || *off-- == 0) {
			
			return element;
		}
	}
	spin_unlock_irq(&iter->hashbin->hb_spinlock);
	iter->hashbin = NULL;
	return NULL;
}


static void *irlmp_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct irlmp_iter_state *iter = seq->private;
	void *v;
	loff_t off = *pos;

	iter->hashbin = NULL;
	if (off-- == 0)
		return LSAP_START_TOKEN;

	iter->hashbin = irlmp->unconnected_lsaps;
	v = irlmp_seq_hb_idx(iter, &off);
	if (v)
		return v;

	if (off-- == 0)
		return LINK_START_TOKEN;

	iter->hashbin = irlmp->links;
	return irlmp_seq_hb_idx(iter, &off);
}

static void *irlmp_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct irlmp_iter_state *iter = seq->private;

	++*pos;

	if (v == LSAP_START_TOKEN) {		
		iter->hashbin = irlmp->unconnected_lsaps;
		v = irlmp_seq_hb_idx(iter, NULL);
		return v ? v : LINK_START_TOKEN;
	}

	if (v == LINK_START_TOKEN) {		
		iter->hashbin = irlmp->links;
		return irlmp_seq_hb_idx(iter, NULL);
	}

	v = hashbin_get_next(iter->hashbin);

	if (v == NULL) {			
		spin_unlock_irq(&iter->hashbin->hb_spinlock);

		if (iter->hashbin == irlmp->unconnected_lsaps)
			v =  LINK_START_TOKEN;

		iter->hashbin = NULL;
	}
	return v;
}

static void irlmp_seq_stop(struct seq_file *seq, void *v)
{
	struct irlmp_iter_state *iter = seq->private;

	if (iter->hashbin)
		spin_unlock_irq(&iter->hashbin->hb_spinlock);
}

static int irlmp_seq_show(struct seq_file *seq, void *v)
{
	const struct irlmp_iter_state *iter = seq->private;
	struct lsap_cb *self = v;

	if (v == LSAP_START_TOKEN)
		seq_puts(seq, "Unconnected LSAPs:\n");
	else if (v == LINK_START_TOKEN)
		seq_puts(seq, "\nRegistered Link Layers:\n");
	else if (iter->hashbin == irlmp->unconnected_lsaps) {
		self = v;
		IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC, return -EINVAL; );
		seq_printf(seq, "lsap state: %s, ",
			   irlsap_state[ self->lsap_state]);
		seq_printf(seq,
			   "slsap_sel: %#02x, dlsap_sel: %#02x, ",
			   self->slsap_sel, self->dlsap_sel);
		seq_printf(seq, "(%s)", self->notify.name);
		seq_printf(seq, "\n");
	} else if (iter->hashbin == irlmp->links) {
		struct lap_cb *lap = v;

		seq_printf(seq, "lap state: %s, ",
			   irlmp_state[lap->lap_state]);

		seq_printf(seq, "saddr: %#08x, daddr: %#08x, ",
			   lap->saddr, lap->daddr);
		seq_printf(seq, "num lsaps: %d",
			   HASHBIN_GET_SIZE(lap->lsaps));
		seq_printf(seq, "\n");

		spin_lock(&lap->lsaps->hb_spinlock);

		seq_printf(seq, "\n  Connected LSAPs:\n");
		for (self = (struct lsap_cb *) hashbin_get_first(lap->lsaps);
		     self != NULL;
		     self = (struct lsap_cb *)hashbin_get_next(lap->lsaps)) {
			IRDA_ASSERT(self->magic == LMP_LSAP_MAGIC,
				    goto outloop;);
			seq_printf(seq, "  lsap state: %s, ",
				   irlsap_state[ self->lsap_state]);
			seq_printf(seq,
				   "slsap_sel: %#02x, dlsap_sel: %#02x, ",
				   self->slsap_sel, self->dlsap_sel);
			seq_printf(seq, "(%s)", self->notify.name);
			seq_putc(seq, '\n');

		}
	IRDA_ASSERT_LABEL(outloop:)
		spin_unlock(&lap->lsaps->hb_spinlock);
		seq_putc(seq, '\n');
	} else
		return -EINVAL;

	return 0;
}

static const struct seq_operations irlmp_seq_ops = {
	.start  = irlmp_seq_start,
	.next   = irlmp_seq_next,
	.stop   = irlmp_seq_stop,
	.show   = irlmp_seq_show,
};

static int irlmp_seq_open(struct inode *inode, struct file *file)
{
	IRDA_ASSERT(irlmp != NULL, return -EINVAL;);

	return seq_open_private(file, &irlmp_seq_ops,
			sizeof(struct irlmp_iter_state));
}

const struct file_operations irlmp_seq_fops = {
	.owner		= THIS_MODULE,
	.open           = irlmp_seq_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release	= seq_release_private,
};

#endif 
