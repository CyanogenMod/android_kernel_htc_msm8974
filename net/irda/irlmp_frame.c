/*********************************************************************
 *
 * Filename:      irlmp_frame.c
 * Version:       0.9
 * Description:   IrLMP frame implementation
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Aug 19 02:09:59 1997
 * Modified at:   Mon Dec 13 13:41:12 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 *
 *     Copyright (c) 1998-1999 Dag Brattli <dagb@cs.uit.no>
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
#include <linux/kernel.h>

#include <net/irda/irda.h>
#include <net/irda/irlap.h>
#include <net/irda/timer.h>
#include <net/irda/irlmp.h>
#include <net/irda/irlmp_frame.h>
#include <net/irda/discovery.h>

static struct lsap_cb *irlmp_find_lsap(struct lap_cb *self, __u8 dlsap,
				       __u8 slsap, int status, hashbin_t *);

inline void irlmp_send_data_pdu(struct lap_cb *self, __u8 dlsap, __u8 slsap,
				int expedited, struct sk_buff *skb)
{
	skb->data[0] = dlsap;
	skb->data[1] = slsap;

	if (expedited) {
		IRDA_DEBUG(4, "%s(), sending expedited data\n", __func__);
		irlap_data_request(self->irlap, skb, TRUE);
	} else
		irlap_data_request(self->irlap, skb, FALSE);
}

void irlmp_send_lcf_pdu(struct lap_cb *self, __u8 dlsap, __u8 slsap,
			__u8 opcode, struct sk_buff *skb)
{
	__u8 *frame;

	IRDA_DEBUG(2, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LAP_MAGIC, return;);
	IRDA_ASSERT(skb != NULL, return;);

	frame = skb->data;

	frame[0] = dlsap | CONTROL_BIT;
	frame[1] = slsap;

	frame[2] = opcode;

	if (opcode == DISCONNECT)
		frame[3] = 0x01; 
	else
		frame[3] = 0x00; 

	irlap_data_request(self->irlap, skb, FALSE);
}

void irlmp_link_data_indication(struct lap_cb *self, struct sk_buff *skb,
				int unreliable)
{
	struct lsap_cb *lsap;
	__u8   slsap_sel;   
	__u8   dlsap_sel;   
	__u8   *fp;

	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LAP_MAGIC, return;);
	IRDA_ASSERT(skb->len > 2, return;);

	fp = skb->data;

	slsap_sel = fp[0] & LSAP_MASK;
	dlsap_sel = fp[1];

	if ((fp[0] & CONTROL_BIT) && (fp[2] == CONNECT_CMD)) {
		IRDA_DEBUG(3, "%s(), incoming connection, "
			   "source LSAP=%d, dest LSAP=%d\n",
			   __func__, slsap_sel, dlsap_sel);

		
		lsap = irlmp_find_lsap(self, dlsap_sel, slsap_sel, CONNECT_CMD,
				       irlmp->unconnected_lsaps);

		
		if (!lsap) {
			IRDA_DEBUG(1, "%s(), incoming connection for LSAP already connected\n", __func__);
			lsap = irlmp_find_lsap(self, dlsap_sel, slsap_sel, 0,
					       self->lsaps);
		}
	} else
		lsap = irlmp_find_lsap(self, dlsap_sel, slsap_sel, 0,
				       self->lsaps);

	if (lsap == NULL) {
		IRDA_DEBUG(2, "IrLMP, Sorry, no LSAP for received frame!\n");
		IRDA_DEBUG(2, "%s(), slsap_sel = %02x, dlsap_sel = %02x\n",
			   __func__, slsap_sel, dlsap_sel);
		if (fp[0] & CONTROL_BIT) {
			IRDA_DEBUG(2, "%s(), received control frame %02x\n",
				   __func__, fp[2]);
		} else {
			IRDA_DEBUG(2, "%s(), received data frame\n", __func__);
		}
		return;
	}

	if (fp[0] & CONTROL_BIT) {
		switch (fp[2]) {
		case CONNECT_CMD:
			lsap->lap = self;
			irlmp_do_lsap_event(lsap, LM_CONNECT_INDICATION, skb);
			break;
		case CONNECT_CNF:
			irlmp_do_lsap_event(lsap, LM_CONNECT_CONFIRM, skb);
			break;
		case DISCONNECT:
			IRDA_DEBUG(4, "%s(), Disconnect indication!\n",
				   __func__);
			irlmp_do_lsap_event(lsap, LM_DISCONNECT_INDICATION,
					    skb);
			break;
		case ACCESSMODE_CMD:
			IRDA_DEBUG(0, "Access mode cmd not implemented!\n");
			break;
		case ACCESSMODE_CNF:
			IRDA_DEBUG(0, "Access mode cnf not implemented!\n");
			break;
		default:
			IRDA_DEBUG(0, "%s(), Unknown control frame %02x\n",
				   __func__, fp[2]);
			break;
		}
	} else if (unreliable) {
		
		if (lsap->lsap_state == LSAP_DATA_TRANSFER_READY)
			irlmp_udata_indication(lsap, skb);
		else
			irlmp_do_lsap_event(lsap, LM_UDATA_INDICATION, skb);
	} else {
		
		if (lsap->lsap_state == LSAP_DATA_TRANSFER_READY)
			irlmp_data_indication(lsap, skb);
		else
			irlmp_do_lsap_event(lsap, LM_DATA_INDICATION, skb);
	}
}

#ifdef CONFIG_IRDA_ULTRA
void irlmp_link_unitdata_indication(struct lap_cb *self, struct sk_buff *skb)
{
	struct lsap_cb *lsap;
	__u8   slsap_sel;   
	__u8   dlsap_sel;   
	__u8   pid;         
	__u8   *fp;
	unsigned long flags;

	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LAP_MAGIC, return;);
	IRDA_ASSERT(skb->len > 2, return;);

	fp = skb->data;

	slsap_sel = fp[0] & LSAP_MASK;
	dlsap_sel = fp[1];
	pid       = fp[2];

	if (pid & 0x80) {
		IRDA_DEBUG(0, "%s(), extension in PID not supp!\n",
			   __func__);
		return;
	}

	
	if ((slsap_sel != LSAP_CONNLESS) || (dlsap_sel != LSAP_CONNLESS)) {
		IRDA_DEBUG(0, "%s(), dropping frame!\n", __func__);
		return;
	}

	
	spin_lock_irqsave(&irlmp->unconnected_lsaps->hb_spinlock, flags);
	lsap = (struct lsap_cb *) hashbin_get_first(irlmp->unconnected_lsaps);
	while (lsap != NULL) {
		if ((lsap->slsap_sel == slsap_sel) &&
		    (lsap->dlsap_sel == dlsap_sel) &&
		    (lsap->pid == pid))
		{
			break;
		}
		lsap = (struct lsap_cb *) hashbin_get_next(irlmp->unconnected_lsaps);
	}
	spin_unlock_irqrestore(&irlmp->unconnected_lsaps->hb_spinlock, flags);

	if (lsap)
		irlmp_connless_data_indication(lsap, skb);
	else {
		IRDA_DEBUG(0, "%s(), found no matching LSAP!\n", __func__);
	}
}
#endif 

void irlmp_link_disconnect_indication(struct lap_cb *lap,
				      struct irlap_cb *irlap,
				      LAP_REASON reason,
				      struct sk_buff *skb)
{
	IRDA_DEBUG(2, "%s()\n", __func__);

	IRDA_ASSERT(lap != NULL, return;);
	IRDA_ASSERT(lap->magic == LMP_LAP_MAGIC, return;);

	lap->reason = reason;
	lap->daddr = DEV_ADDR_ANY;

	

	irlmp_do_lap_event(lap, LM_LAP_DISCONNECT_INDICATION, NULL);
}

void irlmp_link_connect_indication(struct lap_cb *self, __u32 saddr,
				   __u32 daddr, struct qos_info *qos,
				   struct sk_buff *skb)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	
	self->qos = qos;

	
	self->daddr = daddr;
	IRDA_ASSERT(self->saddr == saddr, return;);

	irlmp_do_lap_event(self, LM_LAP_CONNECT_INDICATION, skb);
}

void irlmp_link_connect_confirm(struct lap_cb *self, struct qos_info *qos,
				struct sk_buff *skb)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LAP_MAGIC, return;);
	IRDA_ASSERT(qos != NULL, return;);

	

	
	self->qos = qos;

	irlmp_do_lap_event(self, LM_LAP_CONNECT_CONFIRM, NULL);
}

void irlmp_link_discovery_indication(struct lap_cb *self,
				     discovery_t *discovery)
{
	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LAP_MAGIC, return;);

	
	irlmp_add_discovery(irlmp->cachelog, discovery);

	irlmp_discovery_confirm(irlmp->cachelog, DISCOVERY_PASSIVE);
}

void irlmp_link_discovery_confirm(struct lap_cb *self, hashbin_t *log)
{
	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LMP_LAP_MAGIC, return;);

	
	irlmp_add_discovery_log(irlmp->cachelog, log);

	irlmp_discovery_confirm(irlmp->cachelog, DISCOVERY_ACTIVE);
}

#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
static inline void irlmp_update_cache(struct lap_cb *lap,
				      struct lsap_cb *lsap)
{
	
	lap->cache.valid = FALSE;
	
	lap->cache.dlsap_sel = lsap->dlsap_sel;
	lap->cache.slsap_sel = lsap->slsap_sel;
	lap->cache.lsap = lsap;
	lap->cache.valid = TRUE;
}
#endif

static struct lsap_cb *irlmp_find_lsap(struct lap_cb *self, __u8 dlsap_sel,
				       __u8 slsap_sel, int status,
				       hashbin_t *queue)
{
	struct lsap_cb *lsap;
	unsigned long flags;

#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
	if ((self->cache.valid) &&
	    (self->cache.slsap_sel == slsap_sel) &&
	    (self->cache.dlsap_sel == dlsap_sel))
	{
		return self->cache.lsap;
	}
#endif

	spin_lock_irqsave(&queue->hb_spinlock, flags);

	lsap = (struct lsap_cb *) hashbin_get_first(queue);
	while (lsap != NULL) {
		if ((status == CONNECT_CMD) &&
		    (lsap->slsap_sel == slsap_sel) &&
		    (lsap->dlsap_sel == LSAP_ANY)) {
			lsap->dlsap_sel = dlsap_sel;
			break;
		}
		if ((lsap->slsap_sel == slsap_sel) &&
		    (lsap->dlsap_sel == dlsap_sel))
			break;

		lsap = (struct lsap_cb *) hashbin_get_next(queue);
	}
#ifdef CONFIG_IRDA_CACHE_LAST_LSAP
	if(lsap)
		irlmp_update_cache(self, lsap);
#endif
	spin_unlock_irqrestore(&queue->hb_spinlock, flags);

	
	return lsap;
}
