/* -----------------------------------------------------------------------------
 * Copyright (c) 2011 Ozmo Inc
 * Released under the GNU General Public License Version 2 (GPLv2).
 *
 * This file provides protocol independent part of the implementation of the USB
 * service for a PD.
 * The implementation of this service is split into two parts the first of which
 * is protocol independent and the second contains protocol specific details.
 * This split is to allow alternative protocols to be defined.
 * The implemenation of this service uses ozhcd.c to implement a USB HCD.
 * -----------------------------------------------------------------------------
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <asm/unaligned.h>
#include "ozconfig.h"
#include "ozprotocol.h"
#include "ozeltbuf.h"
#include "ozpd.h"
#include "ozproto.h"
#include "ozusbif.h"
#include "ozhcd.h"
#include "oztrace.h"
#include "ozusbsvc.h"
#include "ozevent.h"
int oz_usb_init(void)
{
	oz_event_log(OZ_EVT_SERVICE, 1, OZ_APPID_USB, 0, 0);
	return oz_hcd_init();
}
void oz_usb_term(void)
{
	oz_event_log(OZ_EVT_SERVICE, 2, OZ_APPID_USB, 0, 0);
	oz_hcd_term();
}
int oz_usb_start(struct oz_pd *pd, int resume)
{
	int rc = 0;
	struct oz_usb_ctx *usb_ctx;
	struct oz_usb_ctx *old_ctx = 0;
	oz_event_log(OZ_EVT_SERVICE, 3, OZ_APPID_USB, 0, resume);
	if (resume) {
		oz_trace("USB service resumed.\n");
		return 0;
	}
	oz_trace("USB service started.\n");
	usb_ctx = kzalloc(sizeof(struct oz_usb_ctx), GFP_ATOMIC);
	if (usb_ctx == 0)
		return -ENOMEM;
	atomic_set(&usb_ctx->ref_count, 1);
	usb_ctx->pd = pd;
	usb_ctx->stopped = 0;
	spin_lock_bh(&pd->app_lock[OZ_APPID_USB-1]);
	old_ctx = pd->app_ctx[OZ_APPID_USB-1];
	if (old_ctx == 0)
		pd->app_ctx[OZ_APPID_USB-1] = usb_ctx;
	oz_usb_get(pd->app_ctx[OZ_APPID_USB-1]);
	spin_unlock_bh(&pd->app_lock[OZ_APPID_USB-1]);
	if (old_ctx) {
		oz_trace("Already have USB context.\n");
		kfree(usb_ctx);
		usb_ctx = old_ctx;
	} else if (usb_ctx) {
		oz_pd_get(pd);
	}
	if (usb_ctx->hport) {
		oz_hcd_pd_reset(usb_ctx, usb_ctx->hport);
	} else {
		usb_ctx->hport = oz_hcd_pd_arrived(usb_ctx);
		if (usb_ctx->hport == 0) {
			oz_trace("USB hub returned null port.\n");
			spin_lock_bh(&pd->app_lock[OZ_APPID_USB-1]);
			pd->app_ctx[OZ_APPID_USB-1] = 0;
			spin_unlock_bh(&pd->app_lock[OZ_APPID_USB-1]);
			oz_usb_put(usb_ctx);
			rc = -1;
		}
	}
	oz_usb_put(usb_ctx);
	return rc;
}
void oz_usb_stop(struct oz_pd *pd, int pause)
{
	struct oz_usb_ctx *usb_ctx;
	oz_event_log(OZ_EVT_SERVICE, 4, OZ_APPID_USB, 0, pause);
	if (pause) {
		oz_trace("USB service paused.\n");
		return;
	}
	spin_lock_bh(&pd->app_lock[OZ_APPID_USB-1]);
	usb_ctx = (struct oz_usb_ctx *)pd->app_ctx[OZ_APPID_USB-1];
	pd->app_ctx[OZ_APPID_USB-1] = 0;
	spin_unlock_bh(&pd->app_lock[OZ_APPID_USB-1]);
	if (usb_ctx) {
		unsigned long tout = jiffies + HZ;
		oz_trace("USB service stopping...\n");
		usb_ctx->stopped = 1;
		while ((atomic_read(&usb_ctx->ref_count) > 2) &&
			time_before(jiffies, tout))
			;
		oz_trace("USB service stopped.\n");
		oz_hcd_pd_departed(usb_ctx->hport);
		oz_usb_put(usb_ctx);
	}
}
void oz_usb_get(void *hpd)
{
	struct oz_usb_ctx *usb_ctx = (struct oz_usb_ctx *)hpd;
	atomic_inc(&usb_ctx->ref_count);
}
void oz_usb_put(void *hpd)
{
	struct oz_usb_ctx *usb_ctx = (struct oz_usb_ctx *)hpd;
	if (atomic_dec_and_test(&usb_ctx->ref_count)) {
		oz_trace("Dealloc USB context.\n");
		oz_pd_put(usb_ctx->pd);
		kfree(usb_ctx);
	}
}
int oz_usb_heartbeat(struct oz_pd *pd)
{
	struct oz_usb_ctx *usb_ctx;
	int rc = 0;
	spin_lock_bh(&pd->app_lock[OZ_APPID_USB-1]);
	usb_ctx = (struct oz_usb_ctx *)pd->app_ctx[OZ_APPID_USB-1];
	if (usb_ctx)
		oz_usb_get(usb_ctx);
	spin_unlock_bh(&pd->app_lock[OZ_APPID_USB-1]);
	if (usb_ctx == 0)
		return rc;
	if (usb_ctx->stopped)
		goto done;
	if (usb_ctx->hport)
		if (oz_hcd_heartbeat(usb_ctx->hport))
			rc = 1;
done:
	oz_usb_put(usb_ctx);
	return rc;
}
int oz_usb_stream_create(void *hpd, u8 ep_num)
{
	struct oz_usb_ctx *usb_ctx = (struct oz_usb_ctx *)hpd;
	struct oz_pd *pd = usb_ctx->pd;
	oz_trace("oz_usb_stream_create(0x%x)\n", ep_num);
	if (pd->mode & OZ_F_ISOC_NO_ELTS) {
		oz_isoc_stream_create(pd, ep_num);
	} else {
		oz_pd_get(pd);
		if (oz_elt_stream_create(&pd->elt_buff, ep_num,
			4*pd->max_tx_size)) {
			oz_pd_put(pd);
			return -1;
		}
	}
	return 0;
}
int oz_usb_stream_delete(void *hpd, u8 ep_num)
{
	struct oz_usb_ctx *usb_ctx = (struct oz_usb_ctx *)hpd;
	if (usb_ctx) {
		struct oz_pd *pd = usb_ctx->pd;
		if (pd) {
			oz_trace("oz_usb_stream_delete(0x%x)\n", ep_num);
			if (pd->mode & OZ_F_ISOC_NO_ELTS) {
				oz_isoc_stream_delete(pd, ep_num);
			} else {
				if (oz_elt_stream_delete(&pd->elt_buff, ep_num))
					return -1;
				oz_pd_put(pd);
			}
		}
	}
	return 0;
}
void oz_usb_request_heartbeat(void *hpd)
{
	struct oz_usb_ctx *usb_ctx = (struct oz_usb_ctx *)hpd;
	if (usb_ctx && usb_ctx->pd)
		oz_pd_request_heartbeat(usb_ctx->pd);
}
