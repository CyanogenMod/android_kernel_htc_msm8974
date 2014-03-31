
/* Copyright 1999-2000 by Mitchell Blank Jr */
/* And on ppp_async.c; Copyright 1999 Paul Mackerras */

/*
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 * This driver provides the encapsulation and framing for sending
 * and receiving PPP frames in ATM AAL5 PDUs.
 */


#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/capability.h>
#include <linux/ppp_defs.h>
#include <linux/ppp-ioctl.h>
#include <linux/ppp_channel.h>
#include <linux/atmppp.h>

#include "common.h"

enum pppoatm_encaps {
	e_autodetect = PPPOATM_ENCAPS_AUTODETECT,
	e_vc = PPPOATM_ENCAPS_VC,
	e_llc = PPPOATM_ENCAPS_LLC,
};

struct pppoatm_vcc {
	struct atm_vcc	*atmvcc;	
	void (*old_push)(struct atm_vcc *, struct sk_buff *);
	void (*old_pop)(struct atm_vcc *, struct sk_buff *);
					
	enum pppoatm_encaps encaps;
	int flags;			
	struct ppp_channel chan;	
	struct tasklet_struct wakeup_tasklet;
};

static const unsigned char pppllc[6] = { 0xFE, 0xFE, 0x03, 0xCF, 0xC0, 0x21 };
#define LLC_LEN		(4)

static inline struct pppoatm_vcc *atmvcc_to_pvcc(const struct atm_vcc *atmvcc)
{
	return (struct pppoatm_vcc *) (atmvcc->user_back);
}

static inline struct pppoatm_vcc *chan_to_pvcc(const struct ppp_channel *chan)
{
	return (struct pppoatm_vcc *) (chan->private);
}

static void pppoatm_wakeup_sender(unsigned long arg)
{
	ppp_output_wakeup((struct ppp_channel *) arg);
}

static void pppoatm_pop(struct atm_vcc *atmvcc, struct sk_buff *skb)
{
	struct pppoatm_vcc *pvcc = atmvcc_to_pvcc(atmvcc);
	pvcc->old_pop(atmvcc, skb);
	tasklet_schedule(&pvcc->wakeup_tasklet);
}

static void pppoatm_unassign_vcc(struct atm_vcc *atmvcc)
{
	struct pppoatm_vcc *pvcc;
	pvcc = atmvcc_to_pvcc(atmvcc);
	atmvcc->push = pvcc->old_push;
	atmvcc->pop = pvcc->old_pop;
	tasklet_kill(&pvcc->wakeup_tasklet);
	ppp_unregister_channel(&pvcc->chan);
	atmvcc->user_back = NULL;
	kfree(pvcc);
	
	module_put(THIS_MODULE);
}

static void pppoatm_push(struct atm_vcc *atmvcc, struct sk_buff *skb)
{
	struct pppoatm_vcc *pvcc = atmvcc_to_pvcc(atmvcc);
	pr_debug("\n");
	if (skb == NULL) {			
		pr_debug("removing ATMPPP VCC %p\n", pvcc);
		pppoatm_unassign_vcc(atmvcc);
		atmvcc->push(atmvcc, NULL);	
		return;
	}
	atm_return(atmvcc, skb->truesize);
	switch (pvcc->encaps) {
	case e_llc:
		if (skb->len < LLC_LEN ||
		    memcmp(skb->data, pppllc, LLC_LEN))
			goto error;
		skb_pull(skb, LLC_LEN);
		break;
	case e_autodetect:
		if (pvcc->chan.ppp == NULL) {	
			kfree_skb(skb);
			return;
		}
		if (skb->len >= sizeof(pppllc) &&
		    !memcmp(skb->data, pppllc, sizeof(pppllc))) {
			pvcc->encaps = e_llc;
			skb_pull(skb, LLC_LEN);
			break;
		}
		if (skb->len >= (sizeof(pppllc) - LLC_LEN) &&
		    !memcmp(skb->data, &pppllc[LLC_LEN],
		    sizeof(pppllc) - LLC_LEN)) {
			pvcc->encaps = e_vc;
			pvcc->chan.mtu += LLC_LEN;
			break;
		}
		pr_debug("Couldn't autodetect yet (skb: %02X %02X %02X %02X %02X %02X)\n",
			 skb->data[0], skb->data[1], skb->data[2],
			 skb->data[3], skb->data[4], skb->data[5]);
		goto error;
	case e_vc:
		break;
	}
	ppp_input(&pvcc->chan, skb);
	return;

error:
	kfree_skb(skb);
	ppp_input_error(&pvcc->chan, 0);
}

#define DROP_PACKET 1
static int pppoatm_send(struct ppp_channel *chan, struct sk_buff *skb)
{
	struct pppoatm_vcc *pvcc = chan_to_pvcc(chan);
	ATM_SKB(skb)->vcc = pvcc->atmvcc;
	pr_debug("(skb=0x%p, vcc=0x%p)\n", skb, pvcc->atmvcc);
	if (skb->data[0] == '\0' && (pvcc->flags & SC_COMP_PROT))
		(void) skb_pull(skb, 1);
	switch (pvcc->encaps) {		
	case e_llc:
		if (skb_headroom(skb) < LLC_LEN) {
			struct sk_buff *n;
			n = skb_realloc_headroom(skb, LLC_LEN);
			if (n != NULL &&
			    !atm_may_send(pvcc->atmvcc, n->truesize)) {
				kfree_skb(n);
				goto nospace;
			}
			kfree_skb(skb);
			skb = n;
			if (skb == NULL)
				return DROP_PACKET;
		} else if (!atm_may_send(pvcc->atmvcc, skb->truesize))
			goto nospace;
		memcpy(skb_push(skb, LLC_LEN), pppllc, LLC_LEN);
		break;
	case e_vc:
		if (!atm_may_send(pvcc->atmvcc, skb->truesize))
			goto nospace;
		break;
	case e_autodetect:
		pr_debug("Trying to send without setting encaps!\n");
		kfree_skb(skb);
		return 1;
	}

	atomic_add(skb->truesize, &sk_atm(ATM_SKB(skb)->vcc)->sk_wmem_alloc);
	ATM_SKB(skb)->atm_options = ATM_SKB(skb)->vcc->atm_options;
	pr_debug("atm_skb(%p)->vcc(%p)->dev(%p)\n",
		 skb, ATM_SKB(skb)->vcc, ATM_SKB(skb)->vcc->dev);
	return ATM_SKB(skb)->vcc->send(ATM_SKB(skb)->vcc, skb)
	    ? DROP_PACKET : 1;
nospace:
	if ((pvcc->flags & SC_COMP_PROT) && skb_headroom(skb) > 0 &&
	    skb->data[-1] == '\0')
		(void) skb_push(skb, 1);
	return 0;
}

static int pppoatm_devppp_ioctl(struct ppp_channel *chan, unsigned int cmd,
	unsigned long arg)
{
	switch (cmd) {
	case PPPIOCGFLAGS:
		return put_user(chan_to_pvcc(chan)->flags, (int __user *) arg)
		    ? -EFAULT : 0;
	case PPPIOCSFLAGS:
		return get_user(chan_to_pvcc(chan)->flags, (int __user *) arg)
		    ? -EFAULT : 0;
	}
	return -ENOTTY;
}

static const struct ppp_channel_ops pppoatm_ops = {
	.start_xmit = pppoatm_send,
	.ioctl = pppoatm_devppp_ioctl,
};

static int pppoatm_assign_vcc(struct atm_vcc *atmvcc, void __user *arg)
{
	struct atm_backend_ppp be;
	struct pppoatm_vcc *pvcc;
	int err;
	static const DECLARE_TASKLET(tasklet_proto, pppoatm_wakeup_sender, 0);
	if (copy_from_user(&be, arg, sizeof be))
		return -EFAULT;
	if (be.encaps != PPPOATM_ENCAPS_AUTODETECT &&
	    be.encaps != PPPOATM_ENCAPS_VC && be.encaps != PPPOATM_ENCAPS_LLC)
		return -EINVAL;
	pvcc = kzalloc(sizeof(*pvcc), GFP_KERNEL);
	if (pvcc == NULL)
		return -ENOMEM;
	pvcc->atmvcc = atmvcc;
	pvcc->old_push = atmvcc->push;
	pvcc->old_pop = atmvcc->pop;
	pvcc->encaps = (enum pppoatm_encaps) be.encaps;
	pvcc->chan.private = pvcc;
	pvcc->chan.ops = &pppoatm_ops;
	pvcc->chan.mtu = atmvcc->qos.txtp.max_sdu - PPP_HDRLEN -
	    (be.encaps == e_vc ? 0 : LLC_LEN);
	pvcc->wakeup_tasklet = tasklet_proto;
	pvcc->wakeup_tasklet.data = (unsigned long) &pvcc->chan;
	err = ppp_register_channel(&pvcc->chan);
	if (err != 0) {
		kfree(pvcc);
		return err;
	}
	atmvcc->user_back = pvcc;
	atmvcc->push = pppoatm_push;
	atmvcc->pop = pppoatm_pop;
	__module_get(THIS_MODULE);

	vcc_process_recv_queue(atmvcc);
	return 0;
}

static int pppoatm_ioctl(struct socket *sock, unsigned int cmd,
	unsigned long arg)
{
	struct atm_vcc *atmvcc = ATM_SD(sock);
	void __user *argp = (void __user *)arg;

	if (cmd != ATM_SETBACKEND && atmvcc->push != pppoatm_push)
		return -ENOIOCTLCMD;
	switch (cmd) {
	case ATM_SETBACKEND: {
		atm_backend_t b;
		if (get_user(b, (atm_backend_t __user *) argp))
			return -EFAULT;
		if (b != ATM_BACKEND_PPP)
			return -ENOIOCTLCMD;
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		return pppoatm_assign_vcc(atmvcc, argp);
		}
	case PPPIOCGCHAN:
		return put_user(ppp_channel_index(&atmvcc_to_pvcc(atmvcc)->
		    chan), (int __user *) argp) ? -EFAULT : 0;
	case PPPIOCGUNIT:
		return put_user(ppp_unit_number(&atmvcc_to_pvcc(atmvcc)->
		    chan), (int __user *) argp) ? -EFAULT : 0;
	}
	return -ENOIOCTLCMD;
}

static struct atm_ioctl pppoatm_ioctl_ops = {
	.owner	= THIS_MODULE,
	.ioctl	= pppoatm_ioctl,
};

static int __init pppoatm_init(void)
{
	register_atm_ioctl(&pppoatm_ioctl_ops);
	return 0;
}

static void __exit pppoatm_exit(void)
{
	deregister_atm_ioctl(&pppoatm_ioctl_ops);
}

module_init(pppoatm_init);
module_exit(pppoatm_exit);

MODULE_AUTHOR("Mitchell Blank Jr <mitch@sfgoth.com>");
MODULE_DESCRIPTION("RFC2364 PPP over ATM/AAL5");
MODULE_LICENSE("GPL");
