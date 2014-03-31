/*********************************************************************
 *
 * Filename:      irlap_event.c
 * Version:       0.9
 * Description:   IrLAP state machine implementation
 * Status:        Experimental.
 * Author:        Dag Brattli <dag@brattli.net>
 * Created at:    Sat Aug 16 00:59:29 1997
 * Modified at:   Sat Dec 25 21:07:57 1999
 * Modified by:   Dag Brattli <dag@brattli.net>
 *
 *     Copyright (c) 1998-2000 Dag Brattli <dag@brattli.net>,
 *     Copyright (c) 1998      Thomas Davis <ratbert@radiks.net>
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

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/slab.h>

#include <net/irda/irda.h>
#include <net/irda/irlap_event.h>

#include <net/irda/timer.h>
#include <net/irda/irlap.h>
#include <net/irda/irlap_frame.h>
#include <net/irda/qos.h>
#include <net/irda/parameters.h>
#include <net/irda/irlmp.h>		

#include <net/irda/irda_device.h>

#ifdef CONFIG_IRDA_FAST_RR
int sysctl_fast_poll_increase = 50;
#endif

static int irlap_state_ndm    (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_query  (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_reply  (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_conn   (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_setup  (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_offline(struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_xmit_p (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_pclose (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_nrm_p  (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_reset_wait(struct irlap_cb *self, IRLAP_EVENT event,
				  struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_reset  (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_nrm_s  (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_xmit_s (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_sclose (struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info);
static int irlap_state_reset_check(struct irlap_cb *, IRLAP_EVENT event,
				   struct sk_buff *, struct irlap_info *);

#ifdef CONFIG_IRDA_DEBUG
static const char *const irlap_event[] = {
	"DISCOVERY_REQUEST",
	"CONNECT_REQUEST",
	"CONNECT_RESPONSE",
	"DISCONNECT_REQUEST",
	"DATA_REQUEST",
	"RESET_REQUEST",
	"RESET_RESPONSE",
	"SEND_I_CMD",
	"SEND_UI_FRAME",
	"RECV_DISCOVERY_XID_CMD",
	"RECV_DISCOVERY_XID_RSP",
	"RECV_SNRM_CMD",
	"RECV_TEST_CMD",
	"RECV_TEST_RSP",
	"RECV_UA_RSP",
	"RECV_DM_RSP",
	"RECV_RD_RSP",
	"RECV_I_CMD",
	"RECV_I_RSP",
	"RECV_UI_FRAME",
	"RECV_FRMR_RSP",
	"RECV_RR_CMD",
	"RECV_RR_RSP",
	"RECV_RNR_CMD",
	"RECV_RNR_RSP",
	"RECV_REJ_CMD",
	"RECV_REJ_RSP",
	"RECV_SREJ_CMD",
	"RECV_SREJ_RSP",
	"RECV_DISC_CMD",
	"SLOT_TIMER_EXPIRED",
	"QUERY_TIMER_EXPIRED",
	"FINAL_TIMER_EXPIRED",
	"POLL_TIMER_EXPIRED",
	"DISCOVERY_TIMER_EXPIRED",
	"WD_TIMER_EXPIRED",
	"BACKOFF_TIMER_EXPIRED",
	"MEDIA_BUSY_TIMER_EXPIRED",
};
#endif	

const char *const irlap_state[] = {
	"LAP_NDM",
	"LAP_QUERY",
	"LAP_REPLY",
	"LAP_CONN",
	"LAP_SETUP",
	"LAP_OFFLINE",
	"LAP_XMIT_P",
	"LAP_PCLOSE",
	"LAP_NRM_P",
	"LAP_RESET_WAIT",
	"LAP_RESET",
	"LAP_NRM_S",
	"LAP_XMIT_S",
	"LAP_SCLOSE",
	"LAP_RESET_CHECK",
};

static int (*state[])(struct irlap_cb *self, IRLAP_EVENT event,
		      struct sk_buff *skb, struct irlap_info *info) =
{
	irlap_state_ndm,
	irlap_state_query,
	irlap_state_reply,
	irlap_state_conn,
	irlap_state_setup,
	irlap_state_offline,
	irlap_state_xmit_p,
	irlap_state_pclose,
	irlap_state_nrm_p,
	irlap_state_reset_wait,
	irlap_state_reset,
	irlap_state_nrm_s,
	irlap_state_xmit_s,
	irlap_state_sclose,
	irlap_state_reset_check,
};

static void irlap_poll_timer_expired(void *data)
{
	struct irlap_cb *self = (struct irlap_cb *) data;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	irlap_do_event(self, POLL_TIMER_EXPIRED, NULL, NULL);
}

static void irlap_start_poll_timer(struct irlap_cb *self, int timeout)
{
	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

#ifdef CONFIG_IRDA_FAST_RR
	if (skb_queue_empty(&self->txq) || self->remote_busy) {
		if (self->fast_RR == TRUE) {
			if (self->fast_RR_timeout < timeout) {
				self->fast_RR_timeout +=
					(sysctl_fast_poll_increase * HZ/1000);

				
				timeout = self->fast_RR_timeout;
			}
		} else {
			self->fast_RR = TRUE;

			
			self->fast_RR_timeout = 0;
			timeout = 0;
		}
	} else
		self->fast_RR = FALSE;

	IRDA_DEBUG(3, "%s(), timeout=%d (%ld)\n", __func__, timeout, jiffies);
#endif 

	if (timeout == 0)
		irlap_do_event(self, POLL_TIMER_EXPIRED, NULL, NULL);
	else
		irda_start_timer(&self->poll_timer, timeout, self,
				 irlap_poll_timer_expired);
}

void irlap_do_event(struct irlap_cb *self, IRLAP_EVENT event,
		    struct sk_buff *skb, struct irlap_info *info)
{
	int ret;

	if (!self || self->magic != LAP_MAGIC)
		return;

	IRDA_DEBUG(3, "%s(), event = %s, state = %s\n", __func__,
		   irlap_event[event], irlap_state[self->state]);

	ret = (*state[self->state])(self, event, skb, info);

	switch (self->state) {
	case LAP_XMIT_P: 
	case LAP_XMIT_S:
		IRDA_DEBUG(2, "%s() : queue len = %d\n", __func__,
			   skb_queue_len(&self->txq));

		if (!skb_queue_empty(&self->txq)) {
			
			self->local_busy = TRUE;


			
			while ((skb = skb_dequeue(&self->txq)) != NULL) {
				
				ret = (*state[self->state])(self, SEND_I_CMD,
							    skb, NULL);
				kfree_skb(skb);

				
				irlmp_flow_indication(self->notify.instance,
						      FLOW_START);

				if (ret == -EPROTO)
					break; 
			}
			
			self->local_busy = FALSE;
		} else if (self->disconnect_pending) {
			self->disconnect_pending = FALSE;

			ret = (*state[self->state])(self, DISCONNECT_REQUEST,
						    NULL, NULL);
		}
		break;
	default:
		break;
	}
}

static int irlap_state_ndm(struct irlap_cb *self, IRLAP_EVENT event,
			   struct sk_buff *skb, struct irlap_info *info)
{
	discovery_t *discovery_rsp;
	int ret = 0;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	switch (event) {
	case CONNECT_REQUEST:
		IRDA_ASSERT(self->netdev != NULL, return -1;);

		if (self->media_busy) {
			IRDA_DEBUG(0, "%s(), CONNECT_REQUEST: media busy!\n",
				   __func__);

			
			irlap_next_state(self, LAP_NDM);

			irlap_disconnect_indication(self, LAP_MEDIA_BUSY);
		} else {
			irlap_send_snrm_frame(self, &self->qos_rx);

			
			irlap_start_final_timer(self, self->final_timeout);

			self->retry_count = 0;
			irlap_next_state(self, LAP_SETUP);
		}
		break;
	case RECV_SNRM_CMD:
		
		if (info) {
			self->daddr = info->daddr;
			self->caddr = info->caddr;

			irlap_next_state(self, LAP_CONN);

			irlap_connect_indication(self, skb);
		} else {
			IRDA_DEBUG(0, "%s(), SNRM frame does not "
				   "contain an I field!\n", __func__);
		}
		break;
	case DISCOVERY_REQUEST:
		IRDA_ASSERT(info != NULL, return -1;);

		if (self->media_busy) {
			IRDA_DEBUG(1, "%s(), DISCOVERY_REQUEST: media busy!\n",
				   __func__);
			

			
			irlap_discovery_confirm(self, NULL);
			return 0;
		}

		self->S = info->S;
		self->s = info->s;
		irlap_send_discovery_xid_frame(self, info->S, info->s, TRUE,
					       info->discovery);
		self->frame_sent = FALSE;
		self->s++;

		irlap_start_slot_timer(self, self->slot_timeout);
		irlap_next_state(self, LAP_QUERY);
		break;
	case RECV_DISCOVERY_XID_CMD:
		IRDA_ASSERT(info != NULL, return -1;);

		
		if (info->s <= info->S) {
			self->slot = irlap_generate_rand_time_slot(info->S,
								   info->s);
			if (self->slot == info->s) {
				discovery_rsp = irlmp_get_discovery_response();
				discovery_rsp->data.daddr = info->daddr;

				irlap_send_discovery_xid_frame(self, info->S,
							       self->slot,
							       FALSE,
							       discovery_rsp);
				self->frame_sent = TRUE;
			} else
				self->frame_sent = FALSE;

			irlap_start_query_timer(self, info->S, info->s);
			irlap_next_state(self, LAP_REPLY);
		} else {
			IRDA_DEBUG(1, "%s(), Receiving final discovery request, missed the discovery slots :-(\n", __func__);

			
			irlap_discovery_indication(self, info->discovery);
		}
		break;
	case MEDIA_BUSY_TIMER_EXPIRED:
#ifdef CONFIG_IRDA_ULTRA
		
		if (!skb_queue_empty(&self->txq_ultra)) {
			ret = (*state[self->state])(self, SEND_UI_FRAME,
						    NULL, NULL);
		}
#endif 
		if (self->connect_pending) {
			self->connect_pending = FALSE;

			if (self->disconnect_pending)
				irlap_disconnect_indication(self, LAP_DISC_INDICATION);
			else
				ret = (*state[self->state])(self,
							    CONNECT_REQUEST,
							    NULL, NULL);
			self->disconnect_pending = FALSE;
		}
		break;
#ifdef CONFIG_IRDA_ULTRA
	case SEND_UI_FRAME:
	{
		int i;
		
		for (i=0; ((i<2) && (self->media_busy == FALSE)); i++) {
			skb = skb_dequeue(&self->txq_ultra);
			if (skb)
				irlap_send_ui_frame(self, skb, CBROADCAST,
						    CMD_FRAME);
			else
				break;
		}
		if (i == 2) {
			
			irda_device_set_media_busy(self->netdev, TRUE);
		}
		break;
	}
	case RECV_UI_FRAME:
		
		if (info->caddr != CBROADCAST) {
			IRDA_DEBUG(0, "%s(), not a broadcast frame!\n",
				   __func__);
		} else
			irlap_unitdata_indication(self, skb);
		break;
#endif 
	case RECV_TEST_CMD:
		
		skb_pull(skb, sizeof(struct test_frame));

		irlap_send_test_frame(self, CBROADCAST, info->daddr, skb);
		break;
	case RECV_TEST_RSP:
		IRDA_DEBUG(0, "%s() not implemented!\n", __func__);
		break;
	default:
		IRDA_DEBUG(2, "%s(), Unknown event %s\n", __func__,
			   irlap_event[event]);

		ret = -1;
		break;
	}
	return ret;
}

static int irlap_state_query(struct irlap_cb *self, IRLAP_EVENT event,
			     struct sk_buff *skb, struct irlap_info *info)
{
	int ret = 0;

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	switch (event) {
	case RECV_DISCOVERY_XID_RSP:
		IRDA_ASSERT(info != NULL, return -1;);
		IRDA_ASSERT(info->discovery != NULL, return -1;);

		IRDA_DEBUG(4, "%s(), daddr=%08x\n", __func__,
			   info->discovery->data.daddr);

		if (!self->discovery_log) {
			IRDA_WARNING("%s: discovery log is gone! "
				     "maybe the discovery timeout has been set"
				     " too short?\n", __func__);
			break;
		}
		hashbin_insert(self->discovery_log,
			       (irda_queue_t *) info->discovery,
			       info->discovery->data.daddr, NULL);

		
		

		break;
	case RECV_DISCOVERY_XID_CMD:

		IRDA_ASSERT(info != NULL, return -1;);

		IRDA_DEBUG(1, "%s(), Receiving discovery request (s = %d) while performing discovery :-(\n", __func__, info->s);

		
		if (info->s == 0xff)
			irlap_discovery_indication(self, info->discovery);
		break;
	case SLOT_TIMER_EXPIRED:
		if (irda_device_is_receiving(self->netdev) && !self->add_wait) {
			IRDA_DEBUG(2, "%s(), device is slow to answer, "
				   "waiting some more!\n", __func__);
			irlap_start_slot_timer(self, msecs_to_jiffies(10));
			self->add_wait = TRUE;
			return ret;
		}
		self->add_wait = FALSE;

		if (self->s < self->S) {
			irlap_send_discovery_xid_frame(self, self->S,
						       self->s, TRUE,
						       self->discovery_cmd);
			self->s++;
			irlap_start_slot_timer(self, self->slot_timeout);

			
			irlap_next_state(self, LAP_QUERY);
		} else {
			
			irlap_send_discovery_xid_frame(self, self->S, 0xff,
						       TRUE,
						       self->discovery_cmd);

			
			irlap_next_state(self, LAP_NDM);

			irlap_discovery_confirm(self, self->discovery_log);

			
			self->discovery_log = NULL;
		}
		break;
	default:
		IRDA_DEBUG(2, "%s(), Unknown event %s\n", __func__,
			   irlap_event[event]);

		ret = -1;
		break;
	}
	return ret;
}

static int irlap_state_reply(struct irlap_cb *self, IRLAP_EVENT event,
			     struct sk_buff *skb, struct irlap_info *info)
{
	discovery_t *discovery_rsp;
	int ret=0;

	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	switch (event) {
	case QUERY_TIMER_EXPIRED:
		IRDA_DEBUG(0, "%s(), QUERY_TIMER_EXPIRED <%ld>\n",
			   __func__, jiffies);
		irlap_next_state(self, LAP_NDM);
		break;
	case RECV_DISCOVERY_XID_CMD:
		IRDA_ASSERT(info != NULL, return -1;);
		
		if (info->s == 0xff) {
			del_timer(&self->query_timer);

			

			
			irlap_next_state(self, LAP_NDM);

			irlap_discovery_indication(self, info->discovery);
		} else {
			
			if ((info->s >= self->slot) && (!self->frame_sent)) {
				discovery_rsp = irlmp_get_discovery_response();
				discovery_rsp->data.daddr = info->daddr;

				irlap_send_discovery_xid_frame(self, info->S,
							       self->slot,
							       FALSE,
							       discovery_rsp);

				self->frame_sent = TRUE;
			}
			irlap_start_query_timer(self, info->S, info->s);

			
			
		}
		break;
	default:
		IRDA_DEBUG(1, "%s(), Unknown event %d, %s\n", __func__,
			   event, irlap_event[event]);

		ret = -1;
		break;
	}
	return ret;
}

static int irlap_state_conn(struct irlap_cb *self, IRLAP_EVENT event,
			    struct sk_buff *skb, struct irlap_info *info)
{
	int ret = 0;

	IRDA_DEBUG(4, "%s(), event=%s\n", __func__, irlap_event[ event]);

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	switch (event) {
	case CONNECT_RESPONSE:
		skb_pull(skb, sizeof(struct snrm_frame));

		IRDA_ASSERT(self->netdev != NULL, return -1;);

		irlap_qos_negotiate(self, skb);

		irlap_initiate_connection_state(self);

		irlap_apply_connection_parameters(self, FALSE);

		irlap_send_ua_response_frame(self, &self->qos_rx);

#if 0
		irlap_send_ua_response_frame(self, &self->qos_rx);
#endif

		irlap_start_wd_timer(self, self->wd_timeout);
		irlap_next_state(self, LAP_NRM_S);

		break;
	case RECV_DISCOVERY_XID_CMD:
		IRDA_DEBUG(3, "%s(), event RECV_DISCOVER_XID_CMD!\n",
			   __func__);
		irlap_next_state(self, LAP_NDM);

		break;
	case DISCONNECT_REQUEST:
		IRDA_DEBUG(0, "%s(), Disconnect request!\n", __func__);
		irlap_send_dm_frame(self);
		irlap_next_state( self, LAP_NDM);
		irlap_disconnect_indication(self, LAP_DISC_INDICATION);
		break;
	default:
		IRDA_DEBUG(1, "%s(), Unknown event %d, %s\n", __func__,
			   event, irlap_event[event]);

		ret = -1;
		break;
	}

	return ret;
}

static int irlap_state_setup(struct irlap_cb *self, IRLAP_EVENT event,
			     struct sk_buff *skb, struct irlap_info *info)
{
	int ret = 0;

	IRDA_DEBUG(4, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	switch (event) {
	case FINAL_TIMER_EXPIRED:
		if (self->retry_count < self->N3) {
			irlap_start_backoff_timer(self, msecs_to_jiffies(20 +
							(jiffies % 30)));
		} else {
			
			irlap_next_state(self, LAP_NDM);

			irlap_disconnect_indication(self, LAP_FOUND_NONE);
		}
		break;
	case BACKOFF_TIMER_EXPIRED:
		irlap_send_snrm_frame(self, &self->qos_rx);
		irlap_start_final_timer(self, self->final_timeout);
		self->retry_count++;
		break;
	case RECV_SNRM_CMD:
		IRDA_DEBUG(4, "%s(), SNRM battle!\n", __func__);

		IRDA_ASSERT(skb != NULL, return 0;);
		IRDA_ASSERT(info != NULL, return 0;);

		if (info &&(info->daddr > self->saddr)) {
			del_timer(&self->final_timer);
			irlap_initiate_connection_state(self);

			IRDA_ASSERT(self->netdev != NULL, return -1;);

			skb_pull(skb, sizeof(struct snrm_frame));

			irlap_qos_negotiate(self, skb);

			
			irlap_apply_connection_parameters(self, FALSE);
			irlap_send_ua_response_frame(self, &self->qos_rx);

			irlap_next_state(self, LAP_NRM_S);
			irlap_connect_confirm(self, skb);

			irlap_start_wd_timer(self, self->wd_timeout);
		} else {
			
			irlap_next_state(self, LAP_SETUP);
		}
		break;
	case RECV_UA_RSP:
		
		del_timer(&self->final_timer);

		
		irlap_initiate_connection_state(self);

		
		IRDA_ASSERT(skb->len > 10, return -1;);

		skb_pull(skb, sizeof(struct ua_frame));

		IRDA_ASSERT(self->netdev != NULL, return -1;);

		irlap_qos_negotiate(self, skb);

		
		irlap_apply_connection_parameters(self, TRUE);
		self->retry_count = 0;

		irlap_wait_min_turn_around(self, &self->qos_tx);

		
		irlap_send_rr_frame(self, CMD_FRAME);

		irlap_start_final_timer(self, self->final_timeout/2);
		irlap_next_state(self, LAP_NRM_P);

		irlap_connect_confirm(self, skb);
		break;
	case RECV_DM_RSP:     
	case RECV_DISC_CMD:
		del_timer(&self->final_timer);
		irlap_next_state(self, LAP_NDM);

		irlap_disconnect_indication(self, LAP_DISC_INDICATION);
		break;
	default:
		IRDA_DEBUG(1, "%s(), Unknown event %d, %s\n", __func__,
			   event, irlap_event[event]);

		ret = -1;
		break;
	}
	return ret;
}

static int irlap_state_offline(struct irlap_cb *self, IRLAP_EVENT event,
			       struct sk_buff *skb, struct irlap_info *info)
{
	IRDA_DEBUG( 0, "%s(), Unknown event\n", __func__);

	return -1;
}

static int irlap_state_xmit_p(struct irlap_cb *self, IRLAP_EVENT event,
			      struct sk_buff *skb, struct irlap_info *info)
{
	int ret = 0;

	switch (event) {
	case SEND_I_CMD:
		if ((self->window > 0) && (!self->remote_busy)) {
			int nextfit;
#ifdef CONFIG_IRDA_DYNAMIC_WINDOW
			struct sk_buff *skb_next;


			skb_next = skb_peek(&self->txq);

			nextfit = ((skb_next != NULL) &&
				   ((skb_next->len + skb->len) <=
				    self->bytes_left));

			if((!nextfit) && (skb->len > self->bytes_left)) {
				IRDA_DEBUG(0, "%s(), Not allowed to transmit"
					   " more bytes!\n", __func__);
				
				skb_queue_head(&self->txq, skb_get(skb));
				return -EPROTO;
			}

			
			self->bytes_left -= skb->len;
#else	
			nextfit = !skb_queue_empty(&self->txq);
#endif	
			if ((self->window > 1) && (nextfit)) {
				
				irlap_send_data_primary(self, skb);
				irlap_next_state(self, LAP_XMIT_P);
			} else {
				
				irlap_send_data_primary_poll(self, skb);

				ret = -EPROTO;
			}
#ifdef CONFIG_IRDA_FAST_RR
			
			self->fast_RR = FALSE;
#endif 
		} else {
			IRDA_DEBUG(4, "%s(), Unable to send! remote busy?\n",
				   __func__);
			skb_queue_head(&self->txq, skb_get(skb));

			ret = -EPROTO;
		}
		break;
	case POLL_TIMER_EXPIRED:
		IRDA_DEBUG(3, "%s(), POLL_TIMER_EXPIRED <%ld>\n",
			    __func__, jiffies);
		irlap_send_rr_frame(self, CMD_FRAME);
		
		self->window = self->window_size;
#ifdef CONFIG_IRDA_DYNAMIC_WINDOW
		
		self->bytes_left = self->line_capacity;
#endif 
		irlap_start_final_timer(self, self->final_timeout);
		irlap_next_state(self, LAP_NRM_P);
		break;
	case DISCONNECT_REQUEST:
		del_timer(&self->poll_timer);
		irlap_wait_min_turn_around(self, &self->qos_tx);
		irlap_send_disc_frame(self);
		irlap_flush_all_queues(self);
		irlap_start_final_timer(self, self->final_timeout);
		self->retry_count = 0;
		irlap_next_state(self, LAP_PCLOSE);
		break;
	case DATA_REQUEST:
		break;
	default:
		IRDA_DEBUG(0, "%s(), Unknown event %s\n",
			   __func__, irlap_event[event]);

		ret = -EINVAL;
		break;
	}
	return ret;
}

static int irlap_state_pclose(struct irlap_cb *self, IRLAP_EVENT event,
			      struct sk_buff *skb, struct irlap_info *info)
{
	int ret = 0;

	IRDA_DEBUG(1, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	switch (event) {
	case RECV_UA_RSP: 
	case RECV_DM_RSP:
		del_timer(&self->final_timer);

		
		irlap_apply_default_connection_parameters(self);

		
		irlap_next_state(self, LAP_NDM);

		irlap_disconnect_indication(self, LAP_DISC_INDICATION);
		break;
	case FINAL_TIMER_EXPIRED:
		if (self->retry_count < self->N3) {
			irlap_wait_min_turn_around(self, &self->qos_tx);
			irlap_send_disc_frame(self);
			irlap_start_final_timer(self, self->final_timeout);
			self->retry_count++;
			
		} else {
			irlap_apply_default_connection_parameters(self);

			
			irlap_next_state(self, LAP_NDM);

			irlap_disconnect_indication(self, LAP_NO_RESPONSE);
		}
		break;
	default:
		IRDA_DEBUG(1, "%s(), Unknown event %d\n", __func__, event);

		ret = -1;
		break;
	}
	return ret;
}

static int irlap_state_nrm_p(struct irlap_cb *self, IRLAP_EVENT event,
			     struct sk_buff *skb, struct irlap_info *info)
{
	int ret = 0;
	int ns_status;
	int nr_status;

	switch (event) {
	case RECV_I_RSP: 
		if (unlikely(skb->len <= LAP_ADDR_HEADER + LAP_CTRL_HEADER)) {
			irlap_wait_min_turn_around(self, &self->qos_tx);
			irlap_send_rr_frame(self, CMD_FRAME);
			
			break;
		}
		
#ifdef CONFIG_IRDA_FAST_RR
		self->fast_RR = FALSE;
#endif 
		IRDA_ASSERT( info != NULL, return -1;);

		ns_status = irlap_validate_ns_received(self, info->ns);
		nr_status = irlap_validate_nr_received(self, info->nr);

		if ((ns_status == NS_EXPECTED) && (nr_status == NR_EXPECTED)) {

			
			self->vr = (self->vr + 1) % 8;

			
			irlap_update_nr_received(self, info->nr);

			self->retry_count = 0;
			self->ack_required = TRUE;

			
			if (!info->pf) {
				
				irlap_next_state(self, LAP_NRM_P);

				irlap_data_indication(self, skb, FALSE);
			} else {
				
				del_timer(&self->final_timer);

				irlap_wait_min_turn_around(self, &self->qos_tx);

				irlap_data_indication(self, skb, FALSE);

				irlap_next_state(self, LAP_XMIT_P);

				irlap_start_poll_timer(self, self->poll_timeout);
			}
			break;

		}
		
		if ((ns_status == NS_UNEXPECTED) && (nr_status == NR_EXPECTED))
		{
			if (!info->pf) {
				irlap_update_nr_received(self, info->nr);


				
				irlap_next_state(self, LAP_NRM_P);
			} else {
				IRDA_DEBUG(4,
				       "%s(), missing or duplicate frame!\n",
					   __func__);

				
				irlap_update_nr_received(self, info->nr);

				irlap_wait_min_turn_around(self, &self->qos_tx);
				irlap_send_rr_frame(self, CMD_FRAME);

				self->ack_required = FALSE;

				irlap_start_final_timer(self, self->final_timeout);
				irlap_next_state(self, LAP_NRM_P);
			}
			break;
		}
		if ((ns_status == NS_EXPECTED) && (nr_status == NR_UNEXPECTED))
		{
			if (info->pf) {
				self->vr = (self->vr + 1) % 8;

				
				irlap_update_nr_received(self, info->nr);

				
				irlap_resend_rejected_frames(self, CMD_FRAME);

				self->ack_required = FALSE;

				irlap_start_final_timer(self, 2 * self->final_timeout);

				
				irlap_next_state(self, LAP_NRM_P);

				irlap_data_indication(self, skb, FALSE);
			} else {
				self->vr = (self->vr + 1) % 8;

				
				irlap_update_nr_received(self, info->nr);

				self->ack_required = FALSE;

				
				irlap_next_state(self, LAP_NRM_P);

				irlap_data_indication(self, skb, FALSE);
			}
			break;
		}
		if ((ns_status == NS_UNEXPECTED) &&
		    (nr_status == NR_UNEXPECTED))
		{
			IRDA_DEBUG(4, "%s(), unexpected nr and ns!\n",
				   __func__);
			if (info->pf) {
				
				irlap_resend_rejected_frames(self, CMD_FRAME);

				irlap_start_final_timer(self, 2 * self->final_timeout);

				
				irlap_next_state(self, LAP_NRM_P);
			} else {
				
				

				self->ack_required = FALSE;
			}
			break;
		}

		if ((nr_status == NR_INVALID) || (ns_status == NS_INVALID)) {
			if (info->pf) {
				del_timer(&self->final_timer);

				irlap_next_state(self, LAP_RESET_WAIT);

				irlap_disconnect_indication(self, LAP_RESET_INDICATION);
				self->xmitflag = TRUE;
			} else {
				del_timer(&self->final_timer);

				irlap_disconnect_indication(self, LAP_RESET_INDICATION);

				self->xmitflag = FALSE;
			}
			break;
		}
		IRDA_DEBUG(1, "%s(), Not implemented!\n", __func__);
		IRDA_DEBUG(1, "%s(), event=%s, ns_status=%d, nr_status=%d\n",
		       __func__, irlap_event[event], ns_status, nr_status);
		break;
	case RECV_UI_FRAME:
		
		if (!info->pf) {
			irlap_data_indication(self, skb, TRUE);
			irlap_next_state(self, LAP_NRM_P);
		} else {
			del_timer(&self->final_timer);
			irlap_data_indication(self, skb, TRUE);
			irlap_next_state(self, LAP_XMIT_P);
			IRDA_DEBUG(1, "%s: RECV_UI_FRAME: next state %s\n", __func__, irlap_state[self->state]);
			irlap_start_poll_timer(self, self->poll_timeout);
		}
		break;
	case RECV_RR_RSP:
		self->remote_busy = FALSE;

		
		del_timer(&self->final_timer);

		ret = irlap_validate_nr_received(self, info->nr);
		if (ret == NR_EXPECTED) {
			
			irlap_update_nr_received(self, info->nr);

			self->retry_count = 0;
			irlap_wait_min_turn_around(self, &self->qos_tx);

			irlap_next_state(self, LAP_XMIT_P);

			
			irlap_start_poll_timer(self, self->poll_timeout);
		} else if (ret == NR_UNEXPECTED) {
			IRDA_ASSERT(info != NULL, return -1;);

			
			irlap_update_nr_received(self, info->nr);

			IRDA_DEBUG(4, "RECV_RR_FRAME: Retrans:%d, nr=%d, va=%d, "
			      "vs=%d, vr=%d\n",
			      self->retry_count, info->nr, self->va,
			      self->vs, self->vr);

			
			irlap_resend_rejected_frames(self, CMD_FRAME);
			irlap_start_final_timer(self, self->final_timeout * 2);

			irlap_next_state(self, LAP_NRM_P);
		} else if (ret == NR_INVALID) {
			IRDA_DEBUG(1, "%s(), Received RR with "
				   "invalid nr !\n", __func__);

			irlap_next_state(self, LAP_RESET_WAIT);

			irlap_disconnect_indication(self, LAP_RESET_INDICATION);
			self->xmitflag = TRUE;
		}
		break;
	case RECV_RNR_RSP:
		IRDA_ASSERT(info != NULL, return -1;);

		
		del_timer(&self->final_timer);
		self->remote_busy = TRUE;

		
		irlap_update_nr_received(self, info->nr);
		irlap_next_state(self, LAP_XMIT_P);

		
		irlap_start_poll_timer(self, self->poll_timeout);
		break;
	case RECV_FRMR_RSP:
		del_timer(&self->final_timer);
		self->xmitflag = TRUE;
		irlap_next_state(self, LAP_RESET_WAIT);
		irlap_reset_indication(self);
		break;
	case FINAL_TIMER_EXPIRED:
		if (irda_device_is_receiving(self->netdev) && !self->add_wait) {
			IRDA_DEBUG(1, "FINAL_TIMER_EXPIRED when receiving a "
			      "frame! Waiting a little bit more!\n");
			irlap_start_final_timer(self, msecs_to_jiffies(300));

			self->add_wait = TRUE;
			break;
		}
		self->add_wait = FALSE;

		
		if (self->retry_count < self->N2) {
			if (skb_peek(&self->wx_list) == NULL) {
				
				IRDA_DEBUG(4, "nrm_p: resending rr");
				irlap_wait_min_turn_around(self, &self->qos_tx);
				irlap_send_rr_frame(self, CMD_FRAME);
			} else {
				IRDA_DEBUG(4, "nrm_p: resend frames");
				irlap_resend_rejected_frames(self, CMD_FRAME);
			}

			irlap_start_final_timer(self, self->final_timeout);
			self->retry_count++;
			IRDA_DEBUG(4, "irlap_state_nrm_p: FINAL_TIMER_EXPIRED:"
				   " retry_count=%d\n", self->retry_count);

			if((self->retry_count % self->N1) == 0)
				irlap_status_indication(self,
							STATUS_NO_ACTIVITY);

			
		} else {
			irlap_apply_default_connection_parameters(self);

			
			irlap_next_state(self, LAP_NDM);
			irlap_disconnect_indication(self, LAP_NO_RESPONSE);
		}
		break;
	case RECV_REJ_RSP:
		irlap_update_nr_received(self, info->nr);
		if (self->remote_busy) {
			irlap_wait_min_turn_around(self, &self->qos_tx);
			irlap_send_rr_frame(self, CMD_FRAME);
		} else
			irlap_resend_rejected_frames(self, CMD_FRAME);
		irlap_start_final_timer(self, 2 * self->final_timeout);
		break;
	case RECV_SREJ_RSP:
		irlap_update_nr_received(self, info->nr);
		if (self->remote_busy) {
			irlap_wait_min_turn_around(self, &self->qos_tx);
			irlap_send_rr_frame(self, CMD_FRAME);
		} else
			irlap_resend_rejected_frame(self, CMD_FRAME);
		irlap_start_final_timer(self, 2 * self->final_timeout);
		break;
	case RECV_RD_RSP:
		IRDA_DEBUG(1, "%s(), RECV_RD_RSP\n", __func__);

		irlap_flush_all_queues(self);
		irlap_next_state(self, LAP_XMIT_P);
		
		irlap_disconnect_request(self);
		break;
	default:
		IRDA_DEBUG(1, "%s(), Unknown event %s\n",
			    __func__, irlap_event[event]);

		ret = -1;
		break;
	}
	return ret;
}

static int irlap_state_reset_wait(struct irlap_cb *self, IRLAP_EVENT event,
				  struct sk_buff *skb, struct irlap_info *info)
{
	int ret = 0;

	IRDA_DEBUG(3, "%s(), event = %s\n", __func__, irlap_event[event]);

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	switch (event) {
	case RESET_REQUEST:
		if (self->xmitflag) {
			irlap_wait_min_turn_around(self, &self->qos_tx);
			irlap_send_snrm_frame(self, NULL);
			irlap_start_final_timer(self, self->final_timeout);
			irlap_next_state(self, LAP_RESET);
		} else {
			irlap_start_final_timer(self, self->final_timeout);
			irlap_next_state(self, LAP_RESET);
		}
		break;
	case DISCONNECT_REQUEST:
		irlap_wait_min_turn_around( self, &self->qos_tx);
		irlap_send_disc_frame( self);
		irlap_flush_all_queues( self);
		irlap_start_final_timer( self, self->final_timeout);
		self->retry_count = 0;
		irlap_next_state( self, LAP_PCLOSE);
		break;
	default:
		IRDA_DEBUG(2, "%s(), Unknown event %s\n", __func__,
			   irlap_event[event]);

		ret = -1;
		break;
	}
	return ret;
}

static int irlap_state_reset(struct irlap_cb *self, IRLAP_EVENT event,
			     struct sk_buff *skb, struct irlap_info *info)
{
	int ret = 0;

	IRDA_DEBUG(3, "%s(), event = %s\n", __func__, irlap_event[event]);

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	switch (event) {
	case RECV_DISC_CMD:
		del_timer(&self->final_timer);

		irlap_apply_default_connection_parameters(self);

		
		irlap_next_state(self, LAP_NDM);

		irlap_disconnect_indication(self, LAP_NO_RESPONSE);

		break;
	case RECV_UA_RSP:
		del_timer(&self->final_timer);

		
		irlap_initiate_connection_state(self);

		irlap_reset_confirm();

		self->remote_busy = FALSE;

		irlap_next_state(self, LAP_XMIT_P);

		irlap_start_poll_timer(self, self->poll_timeout);

		break;
	case FINAL_TIMER_EXPIRED:
		if (self->retry_count < 3) {
			irlap_wait_min_turn_around(self, &self->qos_tx);

			IRDA_ASSERT(self->netdev != NULL, return -1;);
			irlap_send_snrm_frame(self, self->qos_dev);

			self->retry_count++; 

			irlap_start_final_timer(self, self->final_timeout);
			irlap_next_state(self, LAP_RESET);
		} else if (self->retry_count >= self->N3) {
			irlap_apply_default_connection_parameters(self);

			
			irlap_next_state(self, LAP_NDM);

			irlap_disconnect_indication(self, LAP_NO_RESPONSE);
		}
		break;
	case RECV_SNRM_CMD:
		if (!info) {
			IRDA_DEBUG(3, "%s(), RECV_SNRM_CMD\n", __func__);
			irlap_initiate_connection_state(self);
			irlap_wait_min_turn_around(self, &self->qos_tx);
			irlap_send_ua_response_frame(self, &self->qos_rx);
			irlap_reset_confirm();
			irlap_start_wd_timer(self, self->wd_timeout);
			irlap_next_state(self, LAP_NDM);
		} else {
			IRDA_DEBUG(0,
				   "%s(), SNRM frame contained an I field!\n",
				   __func__);
		}
		break;
	default:
		IRDA_DEBUG(1, "%s(), Unknown event %s\n",
			   __func__, irlap_event[event]);

		ret = -1;
		break;
	}
	return ret;
}

static int irlap_state_xmit_s(struct irlap_cb *self, IRLAP_EVENT event,
			      struct sk_buff *skb, struct irlap_info *info)
{
	int ret = 0;

	IRDA_DEBUG(4, "%s(), event=%s\n", __func__, irlap_event[event]);

	IRDA_ASSERT(self != NULL, return -ENODEV;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -EBADR;);

	switch (event) {
	case SEND_I_CMD:
		if ((self->window > 0) && (!self->remote_busy)) {
			int nextfit;
#ifdef CONFIG_IRDA_DYNAMIC_WINDOW
			struct sk_buff *skb_next;


			skb_next = skb_peek(&self->txq);
			nextfit = ((skb_next != NULL) &&
				   ((skb_next->len + skb->len) <=
				    self->bytes_left));

			if((!nextfit) && (skb->len > self->bytes_left)) {
				IRDA_DEBUG(0, "%s(), Not allowed to transmit"
					   " more bytes!\n", __func__);
				
				skb_queue_head(&self->txq, skb_get(skb));

				self->window = self->window_size;
				self->bytes_left = self->line_capacity;
				irlap_start_wd_timer(self, self->wd_timeout);

				irlap_next_state(self, LAP_NRM_S);

				return -EPROTO; 
			}
			
			self->bytes_left -= skb->len;
#else	
			nextfit = !skb_queue_empty(&self->txq);
#endif 
			if ((self->window > 1) && (nextfit)) {
				irlap_send_data_secondary(self, skb);
				irlap_next_state(self, LAP_XMIT_S);
			} else {
				irlap_send_data_secondary_final(self, skb);
				irlap_next_state(self, LAP_NRM_S);

				ret = -EPROTO;
			}
		} else {
			IRDA_DEBUG(2, "%s(), Unable to send!\n", __func__);
			skb_queue_head(&self->txq, skb_get(skb));
			ret = -EPROTO;
		}
		break;
	case DISCONNECT_REQUEST:
		irlap_send_rd_frame(self);
		irlap_flush_all_queues(self);
		irlap_start_wd_timer(self, self->wd_timeout);
		irlap_next_state(self, LAP_SCLOSE);
		break;
	case DATA_REQUEST:
		break;
	default:
		IRDA_DEBUG(2, "%s(), Unknown event %s\n", __func__,
			   irlap_event[event]);

		ret = -EINVAL;
		break;
	}
	return ret;
}

static int irlap_state_nrm_s(struct irlap_cb *self, IRLAP_EVENT event,
			     struct sk_buff *skb, struct irlap_info *info)
{
	int ns_status;
	int nr_status;
	int ret = 0;

	IRDA_DEBUG(4, "%s(), event=%s\n", __func__, irlap_event[ event]);

	IRDA_ASSERT(self != NULL, return -1;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -1;);

	switch (event) {
	case RECV_I_CMD: 
		
		IRDA_DEBUG(4, "%s(), event=%s nr=%d, vs=%d, ns=%d, "
			   "vr=%d, pf=%d\n", __func__,
			   irlap_event[event], info->nr,
			   self->vs, info->ns, self->vr, info->pf);

		self->retry_count = 0;

		ns_status = irlap_validate_ns_received(self, info->ns);
		nr_status = irlap_validate_nr_received(self, info->nr);
		if ((ns_status == NS_EXPECTED) && (nr_status == NR_EXPECTED)) {

			
			self->vr = (self->vr + 1) % 8;

			
			irlap_update_nr_received(self, info->nr);

			if (!info->pf) {

				self->ack_required = TRUE;

#if 0
				irda_start_timer(WD_TIMER, self->wd_timeout);
#endif
				
				irlap_next_state(self, LAP_NRM_S);

				irlap_data_indication(self, skb, FALSE);
				break;
			} else {
				irlap_wait_min_turn_around(self, &self->qos_tx);

				irlap_data_indication(self, skb, FALSE);

				
				if (!skb_queue_empty(&self->txq) &&
				    (self->window > 0))
				{
					self->ack_required = TRUE;

					del_timer(&self->wd_timer);

					irlap_next_state(self, LAP_XMIT_S);
				} else {
					irlap_send_rr_frame(self, RSP_FRAME);
					irlap_start_wd_timer(self,
							     self->wd_timeout);

					
					irlap_next_state(self, LAP_NRM_S);
				}
				break;
			}
		}
		if ((ns_status == NS_UNEXPECTED) && (nr_status == NR_EXPECTED))
		{
			
			if (!info->pf) {
				irlap_update_nr_received(self, info->nr);

				irlap_start_wd_timer(self, self->wd_timeout);
			} else {
				
				irlap_update_nr_received(self, info->nr);

				irlap_wait_min_turn_around(self, &self->qos_tx);
				irlap_send_rr_frame(self, RSP_FRAME);

				irlap_start_wd_timer(self, self->wd_timeout);
			}
			break;
		}

		if ((ns_status == NS_EXPECTED) && (nr_status == NR_UNEXPECTED))
		{
			if (info->pf) {
				IRDA_DEBUG(4, "RECV_I_RSP: frame(s) lost\n");

				self->vr = (self->vr + 1) % 8;

				
				irlap_update_nr_received(self, info->nr);

				
				irlap_resend_rejected_frames(self, RSP_FRAME);

				
				irlap_next_state(self, LAP_NRM_S);

				irlap_data_indication(self, skb, FALSE);
				irlap_start_wd_timer(self, self->wd_timeout);
				break;
			}
			if (!info->pf) {
				self->vr = (self->vr + 1) % 8;

				
				irlap_update_nr_received(self, info->nr);

				
				irlap_next_state(self, LAP_NRM_S);

				irlap_data_indication(self, skb, FALSE);
				irlap_start_wd_timer(self, self->wd_timeout);
			}
			break;
		}

		if (ret == NR_INVALID) {
			IRDA_DEBUG(0, "NRM_S, NR_INVALID not implemented!\n");
		}
		if (ret == NS_INVALID) {
			IRDA_DEBUG(0, "NRM_S, NS_INVALID not implemented!\n");
		}
		break;
	case RECV_UI_FRAME:
		if (!info->pf) {
			irlap_data_indication(self, skb, TRUE);
			irlap_next_state(self, LAP_NRM_S); 
		} else {
			if (!skb_queue_empty(&self->txq) &&
			    (self->window > 0) && !self->remote_busy)
			{
				irlap_data_indication(self, skb, TRUE);

				del_timer(&self->wd_timer);

				irlap_next_state(self, LAP_XMIT_S);
			} else {
				irlap_data_indication(self, skb, TRUE);

				irlap_wait_min_turn_around(self, &self->qos_tx);

				irlap_send_rr_frame(self, RSP_FRAME);
				self->ack_required = FALSE;

				irlap_start_wd_timer(self, self->wd_timeout);

				
				irlap_next_state(self, LAP_NRM_S);
			}
		}
		break;
	case RECV_RR_CMD:
		self->retry_count = 0;

		nr_status = irlap_validate_nr_received(self, info->nr);
		if (nr_status == NR_EXPECTED) {
			if (!skb_queue_empty(&self->txq) &&
			    (self->window > 0)) {
				self->remote_busy = FALSE;

				
				irlap_update_nr_received(self, info->nr);
				del_timer(&self->wd_timer);

				irlap_wait_min_turn_around(self, &self->qos_tx);
				irlap_next_state(self, LAP_XMIT_S);
			} else {
				self->remote_busy = FALSE;
				
				irlap_update_nr_received(self, info->nr);
				irlap_wait_min_turn_around(self, &self->qos_tx);
				irlap_start_wd_timer(self, self->wd_timeout);

				if (self->disconnect_pending) {
					
					irlap_send_rd_frame(self);
					irlap_flush_all_queues(self);

					irlap_next_state(self, LAP_SCLOSE);
				} else {
					
					irlap_send_rr_frame(self, RSP_FRAME);

					irlap_next_state(self, LAP_NRM_S);
				}
			}
		} else if (nr_status == NR_UNEXPECTED) {
			self->remote_busy = FALSE;
			irlap_update_nr_received(self, info->nr);
			irlap_resend_rejected_frames(self, RSP_FRAME);

			irlap_start_wd_timer(self, self->wd_timeout);

			
			irlap_next_state(self, LAP_NRM_S);
		} else {
			IRDA_DEBUG(1, "%s(), invalid nr not implemented!\n",
				   __func__);
		}
		break;
	case RECV_SNRM_CMD:
		
		if (!info) {
			del_timer(&self->wd_timer);
			IRDA_DEBUG(1, "%s(), received SNRM cmd\n", __func__);
			irlap_next_state(self, LAP_RESET_CHECK);

			irlap_reset_indication(self);
		} else {
			IRDA_DEBUG(0,
				   "%s(), SNRM frame contained an I-field!\n",
				   __func__);

		}
		break;
	case RECV_REJ_CMD:
		irlap_update_nr_received(self, info->nr);
		if (self->remote_busy) {
			irlap_wait_min_turn_around(self, &self->qos_tx);
			irlap_send_rr_frame(self, RSP_FRAME);
		} else
			irlap_resend_rejected_frames(self, RSP_FRAME);
		irlap_start_wd_timer(self, self->wd_timeout);
		break;
	case RECV_SREJ_CMD:
		irlap_update_nr_received(self, info->nr);
		if (self->remote_busy) {
			irlap_wait_min_turn_around(self, &self->qos_tx);
			irlap_send_rr_frame(self, RSP_FRAME);
		} else
			irlap_resend_rejected_frame(self, RSP_FRAME);
		irlap_start_wd_timer(self, self->wd_timeout);
		break;
	case WD_TIMER_EXPIRED:
		IRDA_DEBUG(1, "%s(), retry_count = %d\n", __func__,
			   self->retry_count);

		if (self->retry_count < (self->N2 / 2)) {
			
			irlap_start_wd_timer(self, self->wd_timeout);
			self->retry_count++;

			if((self->retry_count % (self->N1 / 2)) == 0)
				irlap_status_indication(self,
							STATUS_NO_ACTIVITY);
		} else {
			irlap_apply_default_connection_parameters(self);

			
			irlap_next_state(self, LAP_NDM);
			irlap_disconnect_indication(self, LAP_NO_RESPONSE);
		}
		break;
	case RECV_DISC_CMD:
		
		irlap_next_state(self, LAP_NDM);

		
		irlap_wait_min_turn_around(self, &self->qos_tx);
		irlap_send_ua_response_frame(self, NULL);

		del_timer(&self->wd_timer);
		irlap_flush_all_queues(self);
		
		irlap_apply_default_connection_parameters(self);

		irlap_disconnect_indication(self, LAP_DISC_INDICATION);
		break;
	case RECV_DISCOVERY_XID_CMD:
		irlap_wait_min_turn_around(self, &self->qos_tx);
		irlap_send_rr_frame(self, RSP_FRAME);
		self->ack_required = TRUE;
		irlap_start_wd_timer(self, self->wd_timeout);
		irlap_next_state(self, LAP_NRM_S);

		break;
	case RECV_TEST_CMD:
		
		skb_pull(skb, LAP_ADDR_HEADER + LAP_CTRL_HEADER);

		irlap_wait_min_turn_around(self, &self->qos_tx);
		irlap_start_wd_timer(self, self->wd_timeout);

		
		irlap_send_test_frame(self, self->caddr, info->daddr, skb);
		break;
	default:
		IRDA_DEBUG(1, "%s(), Unknown event %d, (%s)\n", __func__,
			   event, irlap_event[event]);

		ret = -EINVAL;
		break;
	}
	return ret;
}

static int irlap_state_sclose(struct irlap_cb *self, IRLAP_EVENT event,
			      struct sk_buff *skb, struct irlap_info *info)
{
	IRDA_DEBUG(1, "%s()\n", __func__);

	IRDA_ASSERT(self != NULL, return -ENODEV;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -EBADR;);

	switch (event) {
	case RECV_DISC_CMD:
		
		irlap_next_state(self, LAP_NDM);

		
		irlap_wait_min_turn_around(self, &self->qos_tx);
		irlap_send_ua_response_frame(self, NULL);

		del_timer(&self->wd_timer);
		
		irlap_apply_default_connection_parameters(self);

		irlap_disconnect_indication(self, LAP_DISC_INDICATION);
		break;
	case RECV_DM_RSP:
	case RECV_RR_RSP:
	case RECV_RNR_RSP:
	case RECV_REJ_RSP:
	case RECV_SREJ_RSP:
	case RECV_I_RSP:
		
		irlap_next_state(self, LAP_NDM);

		del_timer(&self->wd_timer);
		irlap_apply_default_connection_parameters(self);

		irlap_disconnect_indication(self, LAP_DISC_INDICATION);
		break;
	case WD_TIMER_EXPIRED:
		
		irlap_next_state(self, LAP_NDM);

		irlap_apply_default_connection_parameters(self);

		irlap_disconnect_indication(self, LAP_DISC_INDICATION);
		break;
	default:
		if (info != NULL  &&  info->pf) {
			del_timer(&self->wd_timer);
			irlap_wait_min_turn_around(self, &self->qos_tx);
			irlap_send_rd_frame(self);
			irlap_start_wd_timer(self, self->wd_timeout);
			break;		
		}

		IRDA_DEBUG(1, "%s(), Unknown event %d, (%s)\n", __func__,
			   event, irlap_event[event]);

		break;
	}

	return -1;
}

static int irlap_state_reset_check( struct irlap_cb *self, IRLAP_EVENT event,
				   struct sk_buff *skb,
				   struct irlap_info *info)
{
	int ret = 0;

	IRDA_DEBUG(1, "%s(), event=%s\n", __func__, irlap_event[event]);

	IRDA_ASSERT(self != NULL, return -ENODEV;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return -EBADR;);

	switch (event) {
	case RESET_RESPONSE:
		irlap_send_ua_response_frame(self, &self->qos_rx);
		irlap_initiate_connection_state(self);
		irlap_start_wd_timer(self, WD_TIMEOUT);
		irlap_flush_all_queues(self);

		irlap_next_state(self, LAP_NRM_S);
		break;
	case DISCONNECT_REQUEST:
		irlap_wait_min_turn_around(self, &self->qos_tx);
		irlap_send_rd_frame(self);
		irlap_start_wd_timer(self, WD_TIMEOUT);
		irlap_next_state(self, LAP_SCLOSE);
		break;
	default:
		IRDA_DEBUG(1, "%s(), Unknown event %d, (%s)\n", __func__,
			   event, irlap_event[event]);

		ret = -EINVAL;
		break;
	}
	return ret;
}
