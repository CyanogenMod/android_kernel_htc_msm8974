/*********************************************************************
 *
 * Filename:      timer.c
 * Version:
 * Description:
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sat Aug 16 00:59:29 1997
 * Modified at:   Wed Dec  8 12:50:34 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 *
 *     Copyright (c) 1997, 1999 Dag Brattli <dagb@cs.uit.no>,
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

#include <linux/delay.h>

#include <net/irda/timer.h>
#include <net/irda/irda.h>
#include <net/irda/irda_device.h>
#include <net/irda/irlap.h>
#include <net/irda/irlmp.h>

extern int  sysctl_slot_timeout;

static void irlap_slot_timer_expired(void* data);
static void irlap_query_timer_expired(void* data);
static void irlap_final_timer_expired(void* data);
static void irlap_wd_timer_expired(void* data);
static void irlap_backoff_timer_expired(void* data);
static void irlap_media_busy_expired(void* data);

void irlap_start_slot_timer(struct irlap_cb *self, int timeout)
{
	irda_start_timer(&self->slot_timer, timeout, (void *) self,
			 irlap_slot_timer_expired);
}

void irlap_start_query_timer(struct irlap_cb *self, int S, int s)
{
	int timeout;

	timeout = ((sysctl_slot_timeout * HZ / 1000) * (S - s)
		   + XIDEXTRA_TIMEOUT + SMALLBUSY_TIMEOUT);

	irda_start_timer( &self->query_timer, timeout, (void *) self,
			  irlap_query_timer_expired);
}

void irlap_start_final_timer(struct irlap_cb *self, int timeout)
{
	irda_start_timer(&self->final_timer, timeout, (void *) self,
			 irlap_final_timer_expired);
}

void irlap_start_wd_timer(struct irlap_cb *self, int timeout)
{
	irda_start_timer(&self->wd_timer, timeout, (void *) self,
			 irlap_wd_timer_expired);
}

void irlap_start_backoff_timer(struct irlap_cb *self, int timeout)
{
	irda_start_timer(&self->backoff_timer, timeout, (void *) self,
			 irlap_backoff_timer_expired);
}

void irlap_start_mbusy_timer(struct irlap_cb *self, int timeout)
{
	irda_start_timer(&self->media_busy_timer, timeout,
			 (void *) self, irlap_media_busy_expired);
}

void irlap_stop_mbusy_timer(struct irlap_cb *self)
{
	
	del_timer(&self->media_busy_timer);

	if (self->state == LAP_NDM)
		irlap_do_event(self, MEDIA_BUSY_TIMER_EXPIRED, NULL, NULL);
}

void irlmp_start_watchdog_timer(struct lsap_cb *self, int timeout)
{
	irda_start_timer(&self->watchdog_timer, timeout, (void *) self,
			 irlmp_watchdog_timer_expired);
}

void irlmp_start_discovery_timer(struct irlmp_cb *self, int timeout)
{
	irda_start_timer(&self->discovery_timer, timeout, (void *) self,
			 irlmp_discovery_timer_expired);
}

void irlmp_start_idle_timer(struct lap_cb *self, int timeout)
{
	irda_start_timer(&self->idle_timer, timeout, (void *) self,
			 irlmp_idle_timer_expired);
}

void irlmp_stop_idle_timer(struct lap_cb *self)
{
	
	del_timer(&self->idle_timer);
}

static void irlap_slot_timer_expired(void *data)
{
	struct irlap_cb *self = (struct irlap_cb *) data;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	irlap_do_event(self, SLOT_TIMER_EXPIRED, NULL, NULL);
}

static void irlap_query_timer_expired(void *data)
{
	struct irlap_cb *self = (struct irlap_cb *) data;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	irlap_do_event(self, QUERY_TIMER_EXPIRED, NULL, NULL);
}

static void irlap_final_timer_expired(void *data)
{
	struct irlap_cb *self = (struct irlap_cb *) data;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	irlap_do_event(self, FINAL_TIMER_EXPIRED, NULL, NULL);
}

static void irlap_wd_timer_expired(void *data)
{
	struct irlap_cb *self = (struct irlap_cb *) data;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	irlap_do_event(self, WD_TIMER_EXPIRED, NULL, NULL);
}

static void irlap_backoff_timer_expired(void *data)
{
	struct irlap_cb *self = (struct irlap_cb *) data;

	IRDA_ASSERT(self != NULL, return;);
	IRDA_ASSERT(self->magic == LAP_MAGIC, return;);

	irlap_do_event(self, BACKOFF_TIMER_EXPIRED, NULL, NULL);
}


static void irlap_media_busy_expired(void *data)
{
	struct irlap_cb *self = (struct irlap_cb *) data;

	IRDA_ASSERT(self != NULL, return;);

	irda_device_set_media_busy(self->netdev, FALSE);
}
