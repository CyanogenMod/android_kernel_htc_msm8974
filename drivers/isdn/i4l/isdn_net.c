/* $Id: isdn_net.c,v 1.1.2.2 2004/01/12 22:37:19 keil Exp $
 *
 * Linux ISDN subsystem, network interfaces and related functions (linklevel).
 *
 * Copyright 1994-1998  by Fritz Elfert (fritz@isdn4linux.de)
 * Copyright 1995,96    by Thinking Objects Software GmbH Wuerzburg
 * Copyright 1995,96    by Michael Hipp (Michael.Hipp@student.uni-tuebingen.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * Data Over Voice (DOV) support added - Guy Ellis 23-Mar-02
 *                                       guy@traverse.com.au
 * Outgoing calls - looks for a 'V' in first char of dialed number
 * Incoming calls - checks first character of eaz as follows:
 *   Numeric - accept DATA only - original functionality
 *   'V'     - accept VOICE (DOV) only
 *   'B'     - accept BOTH DATA and DOV types
 *
 * Jan 2001: fix CISCO HDLC      Bjoern A. Zeeb <i4l@zabbadoz.net>
 *           for info on the protocol, see
 *           http://i4l.zabbadoz.net/i4l/cisco-hdlc.txt
 */

#include <linux/isdn.h>
#include <linux/slab.h>
#include <net/arp.h>
#include <net/dst.h>
#include <net/pkt_sched.h>
#include <linux/inetdevice.h>
#include "isdn_common.h"
#include "isdn_net.h"
#ifdef CONFIG_ISDN_PPP
#include "isdn_ppp.h"
#endif
#ifdef CONFIG_ISDN_X25
#include <linux/concap.h>
#include "isdn_concap.h"
#endif




static __inline__ int isdn_net_device_started(isdn_net_dev *n)
{
	isdn_net_local *lp = n->local;
	struct net_device *dev;

	if (lp->master)
		dev = lp->master;
	else
		dev = n->dev;
	return netif_running(dev);
}

static __inline__ void isdn_net_device_wake_queue(isdn_net_local *lp)
{
	if (lp->master)
		netif_wake_queue(lp->master);
	else
		netif_wake_queue(lp->netdev->dev);
}

static __inline__ void isdn_net_device_stop_queue(isdn_net_local *lp)
{
	if (lp->master)
		netif_stop_queue(lp->master);
	else
		netif_stop_queue(lp->netdev->dev);
}

static __inline__ int isdn_net_device_busy(isdn_net_local *lp)
{
	isdn_net_local *nlp;
	isdn_net_dev *nd;
	unsigned long flags;

	if (!isdn_net_lp_busy(lp))
		return 0;

	if (lp->master)
		nd = ISDN_MASTER_PRIV(lp)->netdev;
	else
		nd = lp->netdev;

	spin_lock_irqsave(&nd->queue_lock, flags);
	nlp = lp->next;
	while (nlp != lp) {
		if (!isdn_net_lp_busy(nlp)) {
			spin_unlock_irqrestore(&nd->queue_lock, flags);
			return 0;
		}
		nlp = nlp->next;
	}
	spin_unlock_irqrestore(&nd->queue_lock, flags);
	return 1;
}

static __inline__ void isdn_net_inc_frame_cnt(isdn_net_local *lp)
{
	atomic_inc(&lp->frame_cnt);
	if (isdn_net_device_busy(lp))
		isdn_net_device_stop_queue(lp);
}

static __inline__ void isdn_net_dec_frame_cnt(isdn_net_local *lp)
{
	atomic_dec(&lp->frame_cnt);

	if (!(isdn_net_device_busy(lp))) {
		if (!skb_queue_empty(&lp->super_tx_queue)) {
			schedule_work(&lp->tqueue);
		} else {
			isdn_net_device_wake_queue(lp);
		}
	}
}

static __inline__ void isdn_net_zero_frame_cnt(isdn_net_local *lp)
{
	atomic_set(&lp->frame_cnt, 0);
}


#define ISDN_NET_TX_TIMEOUT (20 * HZ)


static int isdn_net_force_dial_lp(isdn_net_local *);
static netdev_tx_t isdn_net_start_xmit(struct sk_buff *,
				       struct net_device *);

static void isdn_net_ciscohdlck_connected(isdn_net_local *lp);
static void isdn_net_ciscohdlck_disconnected(isdn_net_local *lp);

char *isdn_net_revision = "$Revision: 1.1.2.2 $";


static void
isdn_net_unreachable(struct net_device *dev, struct sk_buff *skb, char *reason)
{
	if (skb) {

		u_short proto = ntohs(skb->protocol);

		printk(KERN_DEBUG "isdn_net: %s: %s, signalling dst_link_failure %s\n",
		       dev->name,
		       (reason != NULL) ? reason : "unknown",
		       (proto != ETH_P_IP) ? "Protocol != ETH_P_IP" : "");

		dst_link_failure(skb);
	}
	else {  
		printk(KERN_DEBUG "isdn_net: %s: %s\n",
		       dev->name,
		       (reason != NULL) ? reason : "reason unknown");
	}
}

static void
isdn_net_reset(struct net_device *dev)
{
#ifdef CONFIG_ISDN_X25
	struct concap_device_ops *dops =
		((isdn_net_local *)netdev_priv(dev))->dops;
	struct concap_proto *cprot =
		((isdn_net_local *)netdev_priv(dev))->netdev->cprot;
#endif
#ifdef CONFIG_ISDN_X25
	if (cprot && cprot->pops && dops)
		cprot->pops->restart(cprot, dev, dops);
#endif
}

static int
isdn_net_open(struct net_device *dev)
{
	int i;
	struct net_device *p;
	struct in_device *in_dev;

	netif_start_queue(dev);

	isdn_net_reset(dev);
	
	for (i = 0; i < ETH_ALEN - sizeof(u32); i++)
		dev->dev_addr[i] = 0xfc;
	if ((in_dev = dev->ip_ptr) != NULL) {
		struct in_ifaddr *ifa = in_dev->ifa_list;
		if (ifa != NULL)
			memcpy(dev->dev_addr + 2, &ifa->ifa_local, 4);
	}

	
	p = MASTER_TO_SLAVE(dev);
	if (p) {
		while (p) {
			isdn_net_reset(p);
			p = MASTER_TO_SLAVE(p);
		}
	}
	isdn_lock_drivers();
	return 0;
}

static void
isdn_net_bind_channel(isdn_net_local *lp, int idx)
{
	lp->flags |= ISDN_NET_CONNECTED;
	lp->isdn_device = dev->drvmap[idx];
	lp->isdn_channel = dev->chanmap[idx];
	dev->rx_netdev[idx] = lp->netdev;
	dev->st_netdev[idx] = lp->netdev;
}

static void
isdn_net_unbind_channel(isdn_net_local *lp)
{
	skb_queue_purge(&lp->super_tx_queue);

	if (!lp->master) {	
		qdisc_reset_all_tx(lp->netdev->dev);
	}
	lp->dialstate = 0;
	dev->rx_netdev[isdn_dc2minor(lp->isdn_device, lp->isdn_channel)] = NULL;
	dev->st_netdev[isdn_dc2minor(lp->isdn_device, lp->isdn_channel)] = NULL;
	if (lp->isdn_device != -1 && lp->isdn_channel != -1)
		isdn_free_channel(lp->isdn_device, lp->isdn_channel,
				  ISDN_USAGE_NET);
	lp->flags &= ~ISDN_NET_CONNECTED;
	lp->isdn_device = -1;
	lp->isdn_channel = -1;
}

static unsigned long last_jiffies = -HZ;

void
isdn_net_autohup(void)
{
	isdn_net_dev *p = dev->netdev;
	int anymore;

	anymore = 0;
	while (p) {
		isdn_net_local *l = p->local;
		if (jiffies == last_jiffies)
			l->cps = l->transcount;
		else
			l->cps = (l->transcount * HZ) / (jiffies - last_jiffies);
		l->transcount = 0;
		if (dev->net_verbose > 3)
			printk(KERN_DEBUG "%s: %d bogocps\n", p->dev->name, l->cps);
		if ((l->flags & ISDN_NET_CONNECTED) && (!l->dialstate)) {
			anymore = 1;
			l->huptimer++;
			if ((l->onhtime) &&
			    (l->huptimer > l->onhtime))
			{
				if (l->hupflags & ISDN_MANCHARGE &&
				    l->hupflags & ISDN_CHARGEHUP) {
					while (time_after(jiffies, l->chargetime + l->chargeint))
						l->chargetime += l->chargeint;
					if (time_after(jiffies, l->chargetime + l->chargeint - 2 * HZ))
						if (l->outgoing || l->hupflags & ISDN_INHUP)
							isdn_net_hangup(p->dev);
				} else if (l->outgoing) {
					if (l->hupflags & ISDN_CHARGEHUP) {
						if (l->hupflags & ISDN_WAITCHARGE) {
							printk(KERN_DEBUG "isdn_net: Hupflags of %s are %X\n",
							       p->dev->name, l->hupflags);
							isdn_net_hangup(p->dev);
						} else if (time_after(jiffies, l->chargetime + l->chargeint)) {
							printk(KERN_DEBUG
							       "isdn_net: %s: chtime = %lu, chint = %d\n",
							       p->dev->name, l->chargetime, l->chargeint);
							isdn_net_hangup(p->dev);
						}
					} else
						isdn_net_hangup(p->dev);
				} else if (l->hupflags & ISDN_INHUP)
					isdn_net_hangup(p->dev);
			}

			if (dev->global_flags & ISDN_GLOBAL_STOPPED || (ISDN_NET_DIALMODE(*l) == ISDN_NET_DM_OFF)) {
				isdn_net_hangup(p->dev);
				break;
			}
		}
		p = (isdn_net_dev *) p->next;
	}
	last_jiffies = jiffies;
	isdn_timer_ctrl(ISDN_TIMER_NETHANGUP, anymore);
}

static void isdn_net_lp_disconnected(isdn_net_local *lp)
{
	isdn_net_rm_from_bundle(lp);
}

int
isdn_net_stat_callback(int idx, isdn_ctrl *c)
{
	isdn_net_dev *p = dev->st_netdev[idx];
	int cmd = c->command;

	if (p) {
		isdn_net_local *lp = p->local;
#ifdef CONFIG_ISDN_X25
		struct concap_proto *cprot = lp->netdev->cprot;
		struct concap_proto_ops *pops = cprot ? cprot->pops : NULL;
#endif
		switch (cmd) {
		case ISDN_STAT_BSENT:
			
			if ((lp->flags & ISDN_NET_CONNECTED) &&
			    (!lp->dialstate)) {
				isdn_net_dec_frame_cnt(lp);
				lp->stats.tx_packets++;
				lp->stats.tx_bytes += c->parm.length;
			}
			return 1;
		case ISDN_STAT_DCONN:
			
			switch (lp->dialstate) {
			case 4:
			case 7:
			case 8:
				lp->dialstate++;
				return 1;
			case 12:
				lp->dialstate = 5;
				return 1;
			}
			break;
		case ISDN_STAT_DHUP:
			
#ifdef CONFIG_ISDN_X25

			if (!(lp->flags & ISDN_NET_CONNECTED)
			    && pops && pops->disconn_ind)
				pops->disconn_ind(cprot);
#endif 
			if ((!lp->dialstate) && (lp->flags & ISDN_NET_CONNECTED)) {
				if (lp->p_encap == ISDN_NET_ENCAP_CISCOHDLCK)
					isdn_net_ciscohdlck_disconnected(lp);
#ifdef CONFIG_ISDN_PPP
				if (lp->p_encap == ISDN_NET_ENCAP_SYNCPPP)
					isdn_ppp_free(lp);
#endif
				isdn_net_lp_disconnected(lp);
				isdn_all_eaz(lp->isdn_device, lp->isdn_channel);
				printk(KERN_INFO "%s: remote hangup\n", p->dev->name);
				printk(KERN_INFO "%s: Chargesum is %d\n", p->dev->name,
				       lp->charge);
				isdn_net_unbind_channel(lp);
				return 1;
			}
			break;
#ifdef CONFIG_ISDN_X25
		case ISDN_STAT_BHUP:
			
			if (pops && pops->disconn_ind) {
				pops->disconn_ind(cprot);
				return 1;
			}
			break;
#endif 
		case ISDN_STAT_BCONN:
			
			isdn_net_zero_frame_cnt(lp);
			switch (lp->dialstate) {
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 12:
				if (lp->dialstate <= 6) {
					dev->usage[idx] |= ISDN_USAGE_OUTGOING;
					isdn_info_update();
				} else
					dev->rx_netdev[idx] = p;
				lp->dialstate = 0;
				isdn_timer_ctrl(ISDN_TIMER_NETHANGUP, 1);
				if (lp->p_encap == ISDN_NET_ENCAP_CISCOHDLCK)
					isdn_net_ciscohdlck_connected(lp);
				if (lp->p_encap != ISDN_NET_ENCAP_SYNCPPP) {
					if (lp->master) { 
						isdn_net_dev *nd = ISDN_MASTER_PRIV(lp)->netdev;
						isdn_net_add_to_bundle(nd, lp);
					}
				}
				printk(KERN_INFO "isdn_net: %s connected\n", p->dev->name);
				lp->chargetime = jiffies;

				
				lp->dialstarted = 0;
				lp->dialwait_timer = 0;

#ifdef CONFIG_ISDN_PPP
				if (lp->p_encap == ISDN_NET_ENCAP_SYNCPPP)
					isdn_ppp_wakeup_daemon(lp);
#endif
#ifdef CONFIG_ISDN_X25
				
				if (pops)
					if (pops->connect_ind)
						pops->connect_ind(cprot);
#endif 
				
				if (lp->p_encap != ISDN_NET_ENCAP_SYNCPPP)
					isdn_net_device_wake_queue(lp);
				return 1;
			}
			break;
		case ISDN_STAT_NODCH:
			
			if (lp->dialstate == 4) {
				lp->dialstate--;
				return 1;
			}
			break;
		case ISDN_STAT_CINF:
			lp->charge++;
			if (lp->hupflags & ISDN_HAVECHARGE) {
				lp->hupflags &= ~ISDN_WAITCHARGE;
				lp->chargeint = jiffies - lp->chargetime - (2 * HZ);
			}
			if (lp->hupflags & ISDN_WAITCHARGE)
				lp->hupflags |= ISDN_HAVECHARGE;
			lp->chargetime = jiffies;
			printk(KERN_DEBUG "isdn_net: Got CINF chargetime of %s now %lu\n",
			       p->dev->name, lp->chargetime);
			return 1;
		}
	}
	return 0;
}

void
isdn_net_dial(void)
{
	isdn_net_dev *p = dev->netdev;
	int anymore = 0;
	int i;
	isdn_ctrl cmd;
	u_char *phone_number;

	while (p) {
		isdn_net_local *lp = p->local;

#ifdef ISDN_DEBUG_NET_DIAL
		if (lp->dialstate)
			printk(KERN_DEBUG "%s: dialstate=%d\n", p->dev->name, lp->dialstate);
#endif
		switch (lp->dialstate) {
		case 0:
			
			break;
		case 1:
			lp->dial = lp->phone[1];
			if (!lp->dial) {
				printk(KERN_WARNING "%s: phone number deleted?\n",
				       p->dev->name);
				isdn_net_hangup(p->dev);
				break;
			}
			anymore = 1;

			if (lp->dialtimeout > 0)
				if (lp->dialstarted == 0 || time_after(jiffies, lp->dialstarted + lp->dialtimeout + lp->dialwait)) {
					lp->dialstarted = jiffies;
					lp->dialwait_timer = 0;
				}

			lp->dialstate++;
			
		case 2:
			
			cmd.driver = lp->isdn_device;
			cmd.arg = lp->isdn_channel;
			cmd.command = ISDN_CMD_CLREAZ;
			isdn_command(&cmd);
			sprintf(cmd.parm.num, "%s", isdn_map_eaz2msn(lp->msn, cmd.driver));
			cmd.command = ISDN_CMD_SETEAZ;
			isdn_command(&cmd);
			lp->dialretry = 0;
			anymore = 1;
			lp->dialstate++;
			
		case 3:
			if (dev->global_flags & ISDN_GLOBAL_STOPPED || (ISDN_NET_DIALMODE(*lp) == ISDN_NET_DM_OFF)) {
				char *s;
				if (dev->global_flags & ISDN_GLOBAL_STOPPED)
					s = "dial suppressed: isdn system stopped";
				else
					s = "dial suppressed: dialmode `off'";
				isdn_net_unreachable(p->dev, NULL, s);
				isdn_net_hangup(p->dev);
				break;
			}
			cmd.driver = lp->isdn_device;
			cmd.command = ISDN_CMD_SETL2;
			cmd.arg = lp->isdn_channel + (lp->l2_proto << 8);
			isdn_command(&cmd);
			cmd.driver = lp->isdn_device;
			cmd.command = ISDN_CMD_SETL3;
			cmd.arg = lp->isdn_channel + (lp->l3_proto << 8);
			isdn_command(&cmd);
			cmd.driver = lp->isdn_device;
			cmd.arg = lp->isdn_channel;
			if (!lp->dial) {
				printk(KERN_WARNING "%s: phone number deleted?\n",
				       p->dev->name);
				isdn_net_hangup(p->dev);
				break;
			}
			if (!strncmp(lp->dial->num, "LEASED", strlen("LEASED"))) {
				lp->dialstate = 4;
				printk(KERN_INFO "%s: Open leased line ...\n", p->dev->name);
			} else {
				if (lp->dialtimeout > 0)
					if (time_after(jiffies, lp->dialstarted + lp->dialtimeout)) {
						lp->dialwait_timer = jiffies + lp->dialwait;
						lp->dialstarted = 0;
						isdn_net_unreachable(p->dev, NULL, "dial: timed out");
						isdn_net_hangup(p->dev);
						break;
					}

				cmd.driver = lp->isdn_device;
				cmd.command = ISDN_CMD_DIAL;
				cmd.parm.setup.si2 = 0;

				
				phone_number = lp->dial->num;
				if ((*phone_number == 'v') ||
				    (*phone_number == 'V')) { 
					cmd.parm.setup.si1 = 1;
				} else { 
					cmd.parm.setup.si1 = 7;
				}

				strcpy(cmd.parm.setup.phone, phone_number);
				if (!(lp->dial = (isdn_net_phone *) lp->dial->next)) {
					lp->dial = lp->phone[1];
					lp->dialretry++;

					if (lp->dialretry > lp->dialmax) {
						if (lp->dialtimeout == 0) {
							lp->dialwait_timer = jiffies + lp->dialwait;
							lp->dialstarted = 0;
							isdn_net_unreachable(p->dev, NULL, "dial: tried all numbers dialmax times");
						}
						isdn_net_hangup(p->dev);
						break;
					}
				}
				sprintf(cmd.parm.setup.eazmsn, "%s",
					isdn_map_eaz2msn(lp->msn, cmd.driver));
				i = isdn_dc2minor(lp->isdn_device, lp->isdn_channel);
				if (i >= 0) {
					strcpy(dev->num[i], cmd.parm.setup.phone);
					dev->usage[i] |= ISDN_USAGE_OUTGOING;
					isdn_info_update();
				}
				printk(KERN_INFO "%s: dialing %d %s... %s\n", p->dev->name,
				       lp->dialretry, cmd.parm.setup.phone,
				       (cmd.parm.setup.si1 == 1) ? "DOV" : "");
				lp->dtimer = 0;
#ifdef ISDN_DEBUG_NET_DIAL
				printk(KERN_DEBUG "dial: d=%d c=%d\n", lp->isdn_device,
				       lp->isdn_channel);
#endif
				isdn_command(&cmd);
			}
			lp->huptimer = 0;
			lp->outgoing = 1;
			if (lp->chargeint) {
				lp->hupflags |= ISDN_HAVECHARGE;
				lp->hupflags &= ~ISDN_WAITCHARGE;
			} else {
				lp->hupflags |= ISDN_WAITCHARGE;
				lp->hupflags &= ~ISDN_HAVECHARGE;
			}
			anymore = 1;
			lp->dialstate =
				(lp->cbdelay &&
				 (lp->flags & ISDN_NET_CBOUT)) ? 12 : 4;
			break;
		case 4:
			if (lp->dtimer++ > ISDN_TIMER_DTIMEOUT10)
				lp->dialstate = 3;
			anymore = 1;
			break;
		case 5:
			
			cmd.driver = lp->isdn_device;
			cmd.arg = lp->isdn_channel;
			cmd.command = ISDN_CMD_ACCEPTB;
			anymore = 1;
			lp->dtimer = 0;
			lp->dialstate++;
			isdn_command(&cmd);
			break;
		case 6:
#ifdef ISDN_DEBUG_NET_DIAL
			printk(KERN_DEBUG "dialtimer2: %d\n", lp->dtimer);
#endif
			if (lp->dtimer++ > ISDN_TIMER_DTIMEOUT10)
				lp->dialstate = 3;
			anymore = 1;
			break;
		case 7:
#ifdef ISDN_DEBUG_NET_DIAL
			printk(KERN_DEBUG "dialtimer4: %d\n", lp->dtimer);
#endif
			cmd.driver = lp->isdn_device;
			cmd.command = ISDN_CMD_SETL2;
			cmd.arg = lp->isdn_channel + (lp->l2_proto << 8);
			isdn_command(&cmd);
			cmd.driver = lp->isdn_device;
			cmd.command = ISDN_CMD_SETL3;
			cmd.arg = lp->isdn_channel + (lp->l3_proto << 8);
			isdn_command(&cmd);
			if (lp->dtimer++ > ISDN_TIMER_DTIMEOUT15)
				isdn_net_hangup(p->dev);
			else {
				anymore = 1;
				lp->dialstate++;
			}
			break;
		case 9:
			
			cmd.driver = lp->isdn_device;
			cmd.arg = lp->isdn_channel;
			cmd.command = ISDN_CMD_ACCEPTB;
			isdn_command(&cmd);
			anymore = 1;
			lp->dtimer = 0;
			lp->dialstate++;
			break;
		case 8:
		case 10:
			
#ifdef ISDN_DEBUG_NET_DIAL
			printk(KERN_DEBUG "dialtimer4: %d\n", lp->dtimer);
#endif
			if (lp->dtimer++ > ISDN_TIMER_DTIMEOUT10)
				isdn_net_hangup(p->dev);
			else
				anymore = 1;
			break;
		case 11:
			
			if (lp->dtimer++ > lp->cbdelay)
				lp->dialstate = 1;
			anymore = 1;
			break;
		case 12:
			if (lp->dtimer++ > lp->cbdelay)
			{
				printk(KERN_INFO "%s: hangup waiting for callback ...\n", p->dev->name);
				lp->dtimer = 0;
				lp->dialstate = 4;
				cmd.driver = lp->isdn_device;
				cmd.command = ISDN_CMD_HANGUP;
				cmd.arg = lp->isdn_channel;
				isdn_command(&cmd);
				isdn_all_eaz(lp->isdn_device, lp->isdn_channel);
			}
			anymore = 1;
			break;
		default:
			printk(KERN_WARNING "isdn_net: Illegal dialstate %d for device %s\n",
			       lp->dialstate, p->dev->name);
		}
		p = (isdn_net_dev *) p->next;
	}
	isdn_timer_ctrl(ISDN_TIMER_NETDIAL, anymore);
}

void
isdn_net_hangup(struct net_device *d)
{
	isdn_net_local *lp = netdev_priv(d);
	isdn_ctrl cmd;
#ifdef CONFIG_ISDN_X25
	struct concap_proto *cprot = lp->netdev->cprot;
	struct concap_proto_ops *pops = cprot ? cprot->pops : NULL;
#endif

	if (lp->flags & ISDN_NET_CONNECTED) {
		if (lp->slave != NULL) {
			isdn_net_local *slp = ISDN_SLAVE_PRIV(lp);
			if (slp->flags & ISDN_NET_CONNECTED) {
				printk(KERN_INFO
				       "isdn_net: hang up slave %s before %s\n",
				       lp->slave->name, d->name);
				isdn_net_hangup(lp->slave);
			}
		}
		printk(KERN_INFO "isdn_net: local hangup %s\n", d->name);
#ifdef CONFIG_ISDN_PPP
		if (lp->p_encap == ISDN_NET_ENCAP_SYNCPPP)
			isdn_ppp_free(lp);
#endif
		isdn_net_lp_disconnected(lp);
#ifdef CONFIG_ISDN_X25
		if (pops && pops->disconn_ind)
			pops->disconn_ind(cprot);
#endif 

		cmd.driver = lp->isdn_device;
		cmd.command = ISDN_CMD_HANGUP;
		cmd.arg = lp->isdn_channel;
		isdn_command(&cmd);
		printk(KERN_INFO "%s: Chargesum is %d\n", d->name, lp->charge);
		isdn_all_eaz(lp->isdn_device, lp->isdn_channel);
	}
	isdn_net_unbind_channel(lp);
}

typedef struct {
	__be16 source;
	__be16 dest;
} ip_ports;

static void
isdn_net_log_skb(struct sk_buff *skb, isdn_net_local *lp)
{
	
	const u_char *p = skb_network_header(skb);
	unsigned short proto = ntohs(skb->protocol);
	int data_ofs;
	ip_ports *ipp;
	char addinfo[100];

	addinfo[0] = '\0';
	
	if (p < skb->data || skb->network_header >= skb->tail) {
		
		char *buf = skb->data;

		printk(KERN_DEBUG "isdn_net: protocol %04x is buggy, dev %s\n", skb->protocol, lp->netdev->dev->name);
		p = buf;
		proto = ETH_P_IP;
		switch (lp->p_encap) {
		case ISDN_NET_ENCAP_IPTYP:
			proto = ntohs(*(__be16 *)&buf[0]);
			p = &buf[2];
			break;
		case ISDN_NET_ENCAP_ETHER:
			proto = ntohs(*(__be16 *)&buf[12]);
			p = &buf[14];
			break;
		case ISDN_NET_ENCAP_CISCOHDLC:
			proto = ntohs(*(__be16 *)&buf[2]);
			p = &buf[4];
			break;
#ifdef CONFIG_ISDN_PPP
		case ISDN_NET_ENCAP_SYNCPPP:
			proto = ntohs(skb->protocol);
			p = &buf[IPPP_MAX_HEADER];
			break;
#endif
		}
	}
	data_ofs = ((p[0] & 15) * 4);
	switch (proto) {
	case ETH_P_IP:
		switch (p[9]) {
		case 1:
			strcpy(addinfo, " ICMP");
			break;
		case 2:
			strcpy(addinfo, " IGMP");
			break;
		case 4:
			strcpy(addinfo, " IPIP");
			break;
		case 6:
			ipp = (ip_ports *) (&p[data_ofs]);
			sprintf(addinfo, " TCP, port: %d -> %d", ntohs(ipp->source),
				ntohs(ipp->dest));
			break;
		case 8:
			strcpy(addinfo, " EGP");
			break;
		case 12:
			strcpy(addinfo, " PUP");
			break;
		case 17:
			ipp = (ip_ports *) (&p[data_ofs]);
			sprintf(addinfo, " UDP, port: %d -> %d", ntohs(ipp->source),
				ntohs(ipp->dest));
			break;
		case 22:
			strcpy(addinfo, " IDP");
			break;
		}
		printk(KERN_INFO "OPEN: %pI4 -> %pI4%s\n",
		       p + 12, p + 16, addinfo);
		break;
	case ETH_P_ARP:
		printk(KERN_INFO "OPEN: ARP %pI4 -> *.*.*.* ?%pI4\n",
		       p + 14, p + 24);
		break;
	}
}

void isdn_net_write_super(isdn_net_local *lp, struct sk_buff *skb)
{
	if (in_irq()) {
		
		
		skb_queue_tail(&lp->super_tx_queue, skb);
		schedule_work(&lp->tqueue);
		return;
	}

	spin_lock_bh(&lp->xmit_lock);
	if (!isdn_net_lp_busy(lp)) {
		isdn_net_writebuf_skb(lp, skb);
	} else {
		skb_queue_tail(&lp->super_tx_queue, skb);
	}
	spin_unlock_bh(&lp->xmit_lock);
}

static void isdn_net_softint(struct work_struct *work)
{
	isdn_net_local *lp = container_of(work, isdn_net_local, tqueue);
	struct sk_buff *skb;

	spin_lock_bh(&lp->xmit_lock);
	while (!isdn_net_lp_busy(lp)) {
		skb = skb_dequeue(&lp->super_tx_queue);
		if (!skb)
			break;
		isdn_net_writebuf_skb(lp, skb);
	}
	spin_unlock_bh(&lp->xmit_lock);
}

void isdn_net_writebuf_skb(isdn_net_local *lp, struct sk_buff *skb)
{
	int ret;
	int len = skb->len;     

	if (isdn_net_lp_busy(lp)) {
		printk("isdn BUG at %s:%d!\n", __FILE__, __LINE__);
		goto error;
	}

	if (!(lp->flags & ISDN_NET_CONNECTED)) {
		printk("isdn BUG at %s:%d!\n", __FILE__, __LINE__);
		goto error;
	}
	ret = isdn_writebuf_skb_stub(lp->isdn_device, lp->isdn_channel, 1, skb);
	if (ret != len) {
		
		printk(KERN_WARNING "%s: HL driver queue full\n", lp->netdev->dev->name);
		goto error;
	}

	lp->transcount += len;
	isdn_net_inc_frame_cnt(lp);
	return;

error:
	dev_kfree_skb(skb);
	lp->stats.tx_errors++;

}



static int
isdn_net_xmit(struct net_device *ndev, struct sk_buff *skb)
{
	isdn_net_dev *nd;
	isdn_net_local *slp;
	isdn_net_local *lp = netdev_priv(ndev);
	int retv = NETDEV_TX_OK;

	if (((isdn_net_local *) netdev_priv(ndev))->master) {
		printk("isdn BUG at %s:%d!\n", __FILE__, __LINE__);
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	
#ifdef CONFIG_ISDN_PPP
	if (lp->p_encap == ISDN_NET_ENCAP_SYNCPPP) {
		return isdn_ppp_xmit(skb, ndev);
	}
#endif
	nd = ((isdn_net_local *) netdev_priv(ndev))->netdev;
	lp = isdn_net_get_locked_lp(nd);
	if (!lp) {
		printk(KERN_WARNING "%s: all channels busy - requeuing!\n", ndev->name);
		return NETDEV_TX_BUSY;
	}
	

	
	lp->huptimer = 0; 
	isdn_net_writebuf_skb(lp, skb);
	spin_unlock_bh(&lp->xmit_lock);

	if (lp->cps > lp->triggercps) {
		if (lp->slave) {
			if (!lp->sqfull) {
				
				lp->sqfull = 1;
				lp->sqfull_stamp = jiffies;
			} else {
				
				if (time_after(jiffies, lp->sqfull_stamp + lp->slavedelay)) {
					slp = ISDN_SLAVE_PRIV(lp);
					if (!(slp->flags & ISDN_NET_CONNECTED)) {
						isdn_net_force_dial_lp(ISDN_SLAVE_PRIV(lp));
					}
				}
			}
		}
	} else {
		if (lp->sqfull && time_after(jiffies, lp->sqfull_stamp + lp->slavedelay + (10 * HZ))) {
			lp->sqfull = 0;
		}
		
		nd->queue = nd->local;
	}

	return retv;

}

static void
isdn_net_adjust_hdr(struct sk_buff *skb, struct net_device *dev)
{
	isdn_net_local *lp = netdev_priv(dev);
	if (!skb)
		return;
	if (lp->p_encap == ISDN_NET_ENCAP_ETHER) {
		const int pullsize = skb_network_offset(skb) - ETH_HLEN;
		if (pullsize > 0) {
			printk(KERN_DEBUG "isdn_net: Pull junk %d\n", pullsize);
			skb_pull(skb, pullsize);
		}
	}
}


static void isdn_net_tx_timeout(struct net_device *ndev)
{
	isdn_net_local *lp = netdev_priv(ndev);

	printk(KERN_WARNING "isdn_tx_timeout dev %s dialstate %d\n", ndev->name, lp->dialstate);
	if (!lp->dialstate) {
		lp->stats.tx_errors++;
	}
	ndev->trans_start = jiffies;
	netif_wake_queue(ndev);
}

static netdev_tx_t
isdn_net_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	isdn_net_local *lp = netdev_priv(ndev);
#ifdef CONFIG_ISDN_X25
	struct concap_proto *cprot = lp->netdev->cprot;
	if (cprot && cprot->pops) {
		int ret = cprot->pops->encap_and_xmit(cprot, skb);

		if (ret)
			netif_stop_queue(ndev);
		return ret;
	} else
#endif
		
	{
#ifdef ISDN_DEBUG_NET_DUMP
		u_char *buf;
#endif
		isdn_net_adjust_hdr(skb, ndev);
#ifdef ISDN_DEBUG_NET_DUMP
		buf = skb->data;
		isdn_dumppkt("S:", buf, skb->len, 40);
#endif

		if (!(lp->flags & ISDN_NET_CONNECTED)) {
			int chi;
			
			if (!(ISDN_NET_DIALMODE(*lp) == ISDN_NET_DM_AUTO)) {
				isdn_net_unreachable(ndev, skb, "dial rejected: interface not in dialmode `auto'");
				dev_kfree_skb(skb);
				return NETDEV_TX_OK;
			}
			if (lp->phone[1]) {
				ulong flags;

				if (lp->dialwait_timer <= 0)
					if (lp->dialstarted > 0 && lp->dialtimeout > 0 && time_before(jiffies, lp->dialstarted + lp->dialtimeout + lp->dialwait))
						lp->dialwait_timer = lp->dialstarted + lp->dialtimeout + lp->dialwait;

				if (lp->dialwait_timer > 0) {
					if (time_before(jiffies, lp->dialwait_timer)) {
						isdn_net_unreachable(ndev, skb, "dial rejected: retry-time not reached");
						dev_kfree_skb(skb);
						return NETDEV_TX_OK;
					} else
						lp->dialwait_timer = 0;
				}
				
				spin_lock_irqsave(&dev->lock, flags);
				if (((chi =
				      isdn_get_free_channel(
					      ISDN_USAGE_NET,
					      lp->l2_proto,
					      lp->l3_proto,
					      lp->pre_device,
					      lp->pre_channel,
					      lp->msn)
					     ) < 0) &&
				    ((chi =
				      isdn_get_free_channel(
					      ISDN_USAGE_NET,
					      lp->l2_proto,
					      lp->l3_proto,
					      lp->pre_device,
					      lp->pre_channel^1,
					      lp->msn)
					    ) < 0)) {
					spin_unlock_irqrestore(&dev->lock, flags);
					isdn_net_unreachable(ndev, skb,
							     "No channel");
					dev_kfree_skb(skb);
					return NETDEV_TX_OK;
				}
				
				if (dev->net_verbose)
					isdn_net_log_skb(skb, lp);
				lp->dialstate = 1;
				
				isdn_net_bind_channel(lp, chi);
#ifdef CONFIG_ISDN_PPP
				if (lp->p_encap == ISDN_NET_ENCAP_SYNCPPP) {
					
					if (isdn_ppp_bind(lp) < 0) {
						dev_kfree_skb(skb);
						isdn_net_unbind_channel(lp);
						spin_unlock_irqrestore(&dev->lock, flags);
						return NETDEV_TX_OK;	
					}
#ifdef CONFIG_IPPP_FILTER
					if (isdn_ppp_autodial_filter(skb, lp)) {
						isdn_ppp_free(lp);
						isdn_net_unbind_channel(lp);
						spin_unlock_irqrestore(&dev->lock, flags);
						isdn_net_unreachable(ndev, skb, "dial rejected: packet filtered");
						dev_kfree_skb(skb);
						return NETDEV_TX_OK;
					}
#endif
					spin_unlock_irqrestore(&dev->lock, flags);
					isdn_net_dial();	
					netif_stop_queue(ndev);
					return NETDEV_TX_BUSY;	
				}
#endif
				
				spin_unlock_irqrestore(&dev->lock, flags);
				isdn_net_dial();
				isdn_net_device_stop_queue(lp);
				return NETDEV_TX_BUSY;
			} else {
				isdn_net_unreachable(ndev, skb,
						     "No phone number");
				dev_kfree_skb(skb);
				return NETDEV_TX_OK;
			}
		} else {
			
			ndev->trans_start = jiffies;
			if (!lp->dialstate) {
				
				int ret;
				ret = (isdn_net_xmit(ndev, skb));
				if (ret) netif_stop_queue(ndev);
				return ret;
			} else
				netif_stop_queue(ndev);
		}
	}
	return NETDEV_TX_BUSY;
}

static int
isdn_net_close(struct net_device *dev)
{
	struct net_device *p;
#ifdef CONFIG_ISDN_X25
	struct concap_proto *cprot =
		((isdn_net_local *)netdev_priv(dev))->netdev->cprot;
	
#endif

#ifdef CONFIG_ISDN_X25
	if (cprot && cprot->pops) cprot->pops->close(cprot);
#endif
	netif_stop_queue(dev);
	p = MASTER_TO_SLAVE(dev);
	if (p) {
		
		while (p) {
#ifdef CONFIG_ISDN_X25
			cprot = ((isdn_net_local *)netdev_priv(p))
				->netdev->cprot;
			if (cprot && cprot->pops)
				cprot->pops->close(cprot);
#endif
			isdn_net_hangup(p);
			p = MASTER_TO_SLAVE(p);
		}
	}
	isdn_net_hangup(dev);
	isdn_unlock_drivers();
	return 0;
}

static struct net_device_stats *
isdn_net_get_stats(struct net_device *dev)
{
	isdn_net_local *lp = netdev_priv(dev);
	return &lp->stats;
}


static __be16
isdn_net_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	struct ethhdr *eth;
	unsigned char *rawp;

	skb_reset_mac_header(skb);
	skb_pull(skb, ETH_HLEN);
	eth = eth_hdr(skb);

	if (*eth->h_dest & 1) {
		if (memcmp(eth->h_dest, dev->broadcast, ETH_ALEN) == 0)
			skb->pkt_type = PACKET_BROADCAST;
		else
			skb->pkt_type = PACKET_MULTICAST;
	}

	else if (dev->flags & (IFF_PROMISC )) {
		if (memcmp(eth->h_dest, dev->dev_addr, ETH_ALEN))
			skb->pkt_type = PACKET_OTHERHOST;
	}
	if (ntohs(eth->h_proto) >= 1536)
		return eth->h_proto;

	rawp = skb->data;

	if (*(unsigned short *) rawp == 0xFFFF)
		return htons(ETH_P_802_3);
	return htons(ETH_P_802_2);
}


static struct sk_buff*
isdn_net_ciscohdlck_alloc_skb(isdn_net_local *lp, int len)
{
	unsigned short hl = dev->drv[lp->isdn_device]->interface->hl_hdrlen;
	struct sk_buff *skb;

	skb = alloc_skb(hl + len, GFP_ATOMIC);
	if (skb)
		skb_reserve(skb, hl);
	else
		printk("isdn out of mem at %s:%d!\n", __FILE__, __LINE__);
	return skb;
}

static int
isdn_ciscohdlck_dev_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	isdn_net_local *lp = netdev_priv(dev);
	unsigned long len = 0;
	unsigned long expires = 0;
	int tmp = 0;
	int period = lp->cisco_keepalive_period;
	s8 debserint = lp->cisco_debserint;
	int rc = 0;

	if (lp->p_encap != ISDN_NET_ENCAP_CISCOHDLCK)
		return -EINVAL;

	switch (cmd) {
		
	case SIOCGKEEPPERIOD:
		len = (unsigned long)sizeof(lp->cisco_keepalive_period);
		if (copy_to_user(ifr->ifr_data,
				 &lp->cisco_keepalive_period, len))
			rc = -EFAULT;
		break;
	case SIOCSKEEPPERIOD:
		tmp = lp->cisco_keepalive_period;
		len = (unsigned long)sizeof(lp->cisco_keepalive_period);
		if (copy_from_user(&period, ifr->ifr_data, len))
			rc = -EFAULT;
		if ((period > 0) && (period <= 32767))
			lp->cisco_keepalive_period = period;
		else
			rc = -EINVAL;
		if (!rc && (tmp != lp->cisco_keepalive_period)) {
			expires = (unsigned long)(jiffies +
						  lp->cisco_keepalive_period * HZ);
			mod_timer(&lp->cisco_timer, expires);
			printk(KERN_INFO "%s: Keepalive period set "
			       "to %d seconds.\n",
			       dev->name, lp->cisco_keepalive_period);
		}
		break;

		
	case SIOCGDEBSERINT:
		len = (unsigned long)sizeof(lp->cisco_debserint);
		if (copy_to_user(ifr->ifr_data,
				 &lp->cisco_debserint, len))
			rc = -EFAULT;
		break;
	case SIOCSDEBSERINT:
		len = (unsigned long)sizeof(lp->cisco_debserint);
		if (copy_from_user(&debserint,
				   ifr->ifr_data, len))
			rc = -EFAULT;
		if ((debserint >= 0) && (debserint <= 64))
			lp->cisco_debserint = debserint;
		else
			rc = -EINVAL;
		break;

	default:
		rc = -EINVAL;
		break;
	}
	return (rc);
}


static int isdn_net_ioctl(struct net_device *dev,
			  struct ifreq *ifr, int cmd)
{
	isdn_net_local *lp = netdev_priv(dev);

	switch (lp->p_encap) {
#ifdef CONFIG_ISDN_PPP
	case ISDN_NET_ENCAP_SYNCPPP:
		return isdn_ppp_dev_ioctl(dev, ifr, cmd);
#endif
	case ISDN_NET_ENCAP_CISCOHDLCK:
		return isdn_ciscohdlck_dev_ioctl(dev, ifr, cmd);
	default:
		return -EINVAL;
	}
}

static void
isdn_net_ciscohdlck_slarp_send_keepalive(unsigned long data)
{
	isdn_net_local *lp = (isdn_net_local *) data;
	struct sk_buff *skb;
	unsigned char *p;
	unsigned long last_cisco_myseq = lp->cisco_myseq;
	int myseq_diff = 0;

	if (!(lp->flags & ISDN_NET_CONNECTED) || lp->dialstate) {
		printk("isdn BUG at %s:%d!\n", __FILE__, __LINE__);
		return;
	}
	lp->cisco_myseq++;

	myseq_diff = (lp->cisco_myseq - lp->cisco_mineseen);
	if ((lp->cisco_line_state) && ((myseq_diff >= 3) || (myseq_diff <= -3))) {
		
		lp->cisco_line_state = 0;
		printk(KERN_WARNING
		       "UPDOWN: Line protocol on Interface %s,"
		       " changed state to down\n", lp->netdev->dev->name);
		
	} else if ((!lp->cisco_line_state) &&
		   (myseq_diff >= 0) && (myseq_diff <= 2)) {
		
		lp->cisco_line_state = 1;
		printk(KERN_WARNING
		       "UPDOWN: Line protocol on Interface %s,"
		       " changed state to up\n", lp->netdev->dev->name);
		
	}

	if (lp->cisco_debserint)
		printk(KERN_DEBUG "%s: HDLC "
		       "myseq %lu, mineseen %lu%c, yourseen %lu, %s\n",
		       lp->netdev->dev->name, last_cisco_myseq, lp->cisco_mineseen,
		       ((last_cisco_myseq == lp->cisco_mineseen) ? '*' : 040),
		       lp->cisco_yourseq,
		       ((lp->cisco_line_state) ? "line up" : "line down"));

	skb = isdn_net_ciscohdlck_alloc_skb(lp, 4 + 14);
	if (!skb)
		return;

	p = skb_put(skb, 4 + 14);

	
	*(u8 *)(p + 0) = CISCO_ADDR_UNICAST;
	*(u8 *)(p + 1) = CISCO_CTRL;
	*(__be16 *)(p + 2) = cpu_to_be16(CISCO_TYPE_SLARP);

	
	*(__be32 *)(p +  4) = cpu_to_be32(CISCO_SLARP_KEEPALIVE);
	*(__be32 *)(p +  8) = cpu_to_be32(lp->cisco_myseq);
	*(__be32 *)(p + 12) = cpu_to_be32(lp->cisco_yourseq);
	*(__be16 *)(p + 16) = cpu_to_be16(0xffff); 
	p += 18;

	isdn_net_write_super(lp, skb);

	lp->cisco_timer.expires = jiffies + lp->cisco_keepalive_period * HZ;

	add_timer(&lp->cisco_timer);
}

static void
isdn_net_ciscohdlck_slarp_send_request(isdn_net_local *lp)
{
	struct sk_buff *skb;
	unsigned char *p;

	skb = isdn_net_ciscohdlck_alloc_skb(lp, 4 + 14);
	if (!skb)
		return;

	p = skb_put(skb, 4 + 14);

	
	*(u8 *)(p + 0) = CISCO_ADDR_UNICAST;
	*(u8 *)(p + 1) = CISCO_CTRL;
	*(__be16 *)(p + 2) = cpu_to_be16(CISCO_TYPE_SLARP);

	
	*(__be32 *)(p +  4) = cpu_to_be32(CISCO_SLARP_REQUEST);
	*(__be32 *)(p +  8) = cpu_to_be32(0); 
	*(__be32 *)(p + 12) = cpu_to_be32(0); 
	*(__be16 *)(p + 16) = cpu_to_be16(0); 
	p += 18;

	isdn_net_write_super(lp, skb);
}

static void
isdn_net_ciscohdlck_connected(isdn_net_local *lp)
{
	lp->cisco_myseq = 0;
	lp->cisco_mineseen = 0;
	lp->cisco_yourseq = 0;
	lp->cisco_keepalive_period = ISDN_TIMER_KEEPINT;
	lp->cisco_last_slarp_in = 0;
	lp->cisco_line_state = 0;
	lp->cisco_debserint = 0;

	
	isdn_net_ciscohdlck_slarp_send_request(lp);

	init_timer(&lp->cisco_timer);
	lp->cisco_timer.data = (unsigned long) lp;
	lp->cisco_timer.function = isdn_net_ciscohdlck_slarp_send_keepalive;
	lp->cisco_timer.expires = jiffies + lp->cisco_keepalive_period * HZ;
	add_timer(&lp->cisco_timer);
}

static void
isdn_net_ciscohdlck_disconnected(isdn_net_local *lp)
{
	del_timer(&lp->cisco_timer);
}

static void
isdn_net_ciscohdlck_slarp_send_reply(isdn_net_local *lp)
{
	struct sk_buff *skb;
	unsigned char *p;
	struct in_device *in_dev = NULL;
	__be32 addr = 0;		
	__be32 mask = 0;		

	if ((in_dev = lp->netdev->dev->ip_ptr) != NULL) {
		
		struct in_ifaddr *ifa = in_dev->ifa_list;
		if (ifa != NULL) {
			addr = ifa->ifa_local;
			mask = ifa->ifa_mask;
		}
	}

	skb = isdn_net_ciscohdlck_alloc_skb(lp, 4 + 14);
	if (!skb)
		return;

	p = skb_put(skb, 4 + 14);

	
	*(u8 *)(p + 0) = CISCO_ADDR_UNICAST;
	*(u8 *)(p + 1) = CISCO_CTRL;
	*(__be16 *)(p + 2) = cpu_to_be16(CISCO_TYPE_SLARP);

	*(__be32 *)(p +  4) = cpu_to_be32(CISCO_SLARP_REPLY);
	*(__be32 *)(p +  8) = addr; 
	*(__be32 *)(p + 12) = mask; 
	*(__be16 *)(p + 16) = cpu_to_be16(0); 
	p += 18;

	isdn_net_write_super(lp, skb);
}

static void
isdn_net_ciscohdlck_slarp_in(isdn_net_local *lp, struct sk_buff *skb)
{
	unsigned char *p;
	int period;
	u32 code;
	u32 my_seq;
	u32 your_seq;
	__be32 local;
	__be32 *addr, *mask;

	if (skb->len < 14)
		return;

	p = skb->data;
	code = be32_to_cpup((__be32 *)p);
	p += 4;

	switch (code) {
	case CISCO_SLARP_REQUEST:
		lp->cisco_yourseq = 0;
		isdn_net_ciscohdlck_slarp_send_reply(lp);
		break;
	case CISCO_SLARP_REPLY:
		addr = (__be32 *)p;
		mask = (__be32 *)(p + 4);
		if (*mask != cpu_to_be32(0xfffffffc))
			goto slarp_reply_out;
		if ((*addr & cpu_to_be32(3)) == cpu_to_be32(0) ||
		    (*addr & cpu_to_be32(3)) == cpu_to_be32(3))
			goto slarp_reply_out;
		local = *addr ^ cpu_to_be32(3);
		printk(KERN_INFO "%s: got slarp reply: remote ip: %pI4, local ip: %pI4 mask: %pI4\n",
		       lp->netdev->dev->name, addr, &local, mask);
		break;
	slarp_reply_out:
		printk(KERN_INFO "%s: got invalid slarp reply (%pI4/%pI4) - ignored\n",
		       lp->netdev->dev->name, addr, mask);
		break;
	case CISCO_SLARP_KEEPALIVE:
		period = (int)((jiffies - lp->cisco_last_slarp_in
				+ HZ / 2 - 1) / HZ);
		if (lp->cisco_debserint &&
		    (period != lp->cisco_keepalive_period) &&
		    lp->cisco_last_slarp_in) {
			printk(KERN_DEBUG "%s: Keepalive period mismatch - "
			       "is %d but should be %d.\n",
			       lp->netdev->dev->name, period,
			       lp->cisco_keepalive_period);
		}
		lp->cisco_last_slarp_in = jiffies;
		my_seq = be32_to_cpup((__be32 *)(p + 0));
		your_seq = be32_to_cpup((__be32 *)(p + 4));
		p += 10;
		lp->cisco_yourseq = my_seq;
		lp->cisco_mineseen = your_seq;
		break;
	}
}

static void
isdn_net_ciscohdlck_receive(isdn_net_local *lp, struct sk_buff *skb)
{
	unsigned char *p;
	u8 addr;
	u8 ctrl;
	u16 type;

	if (skb->len < 4)
		goto out_free;

	p = skb->data;
	addr = *(u8 *)(p + 0);
	ctrl = *(u8 *)(p + 1);
	type = be16_to_cpup((__be16 *)(p + 2));
	p += 4;
	skb_pull(skb, 4);

	if (addr != CISCO_ADDR_UNICAST && addr != CISCO_ADDR_BROADCAST) {
		printk(KERN_WARNING "%s: Unknown Cisco addr 0x%02x\n",
		       lp->netdev->dev->name, addr);
		goto out_free;
	}
	if (ctrl != CISCO_CTRL) {
		printk(KERN_WARNING "%s: Unknown Cisco ctrl 0x%02x\n",
		       lp->netdev->dev->name, ctrl);
		goto out_free;
	}

	switch (type) {
	case CISCO_TYPE_SLARP:
		isdn_net_ciscohdlck_slarp_in(lp, skb);
		goto out_free;
	case CISCO_TYPE_CDP:
		if (lp->cisco_debserint)
			printk(KERN_DEBUG "%s: Received CDP packet. use "
			       "\"no cdp enable\" on cisco.\n",
			       lp->netdev->dev->name);
		goto out_free;
	default:
		
		skb->protocol = htons(type);
		netif_rx(skb);
		return;
	}

out_free:
	kfree_skb(skb);
}

static void
isdn_net_receive(struct net_device *ndev, struct sk_buff *skb)
{
	isdn_net_local *lp = netdev_priv(ndev);
	isdn_net_local *olp = lp;	
#ifdef CONFIG_ISDN_X25
	struct concap_proto *cprot = lp->netdev->cprot;
#endif
	lp->transcount += skb->len;

	lp->stats.rx_packets++;
	lp->stats.rx_bytes += skb->len;
	if (lp->master) {
		ndev = lp->master;
		lp = netdev_priv(ndev);
		lp->stats.rx_packets++;
		lp->stats.rx_bytes += skb->len;
	}
	skb->dev = ndev;
	skb->pkt_type = PACKET_HOST;
	skb_reset_mac_header(skb);
#ifdef ISDN_DEBUG_NET_DUMP
	isdn_dumppkt("R:", skb->data, skb->len, 40);
#endif
	switch (lp->p_encap) {
	case ISDN_NET_ENCAP_ETHER:
		
		olp->huptimer = 0;
		lp->huptimer = 0;
		skb->protocol = isdn_net_type_trans(skb, ndev);
		break;
	case ISDN_NET_ENCAP_UIHDLC:
		
		olp->huptimer = 0;
		lp->huptimer = 0;
		skb_pull(skb, 2);
		
	case ISDN_NET_ENCAP_RAWIP:
		
		olp->huptimer = 0;
		lp->huptimer = 0;
		skb->protocol = htons(ETH_P_IP);
		break;
	case ISDN_NET_ENCAP_CISCOHDLCK:
		isdn_net_ciscohdlck_receive(lp, skb);
		return;
	case ISDN_NET_ENCAP_CISCOHDLC:
		
		skb_pull(skb, 2);
		
	case ISDN_NET_ENCAP_IPTYP:
		
		olp->huptimer = 0;
		lp->huptimer = 0;
		skb->protocol = *(__be16 *)&(skb->data[0]);
		skb_pull(skb, 2);
		if (*(unsigned short *) skb->data == 0xFFFF)
			skb->protocol = htons(ETH_P_802_3);
		break;
#ifdef CONFIG_ISDN_PPP
	case ISDN_NET_ENCAP_SYNCPPP:
		
		isdn_ppp_receive(lp->netdev, olp, skb);
		return;
#endif

	default:
#ifdef CONFIG_ISDN_X25
		
		if (cprot) if (cprot->pops)
				   if (cprot->pops->data_ind) {
					   cprot->pops->data_ind(cprot, skb);
					   return;
				   };
#endif 
		printk(KERN_WARNING "%s: unknown encapsulation, dropping\n",
		       lp->netdev->dev->name);
		kfree_skb(skb);
		return;
	}

	netif_rx(skb);
	return;
}

int
isdn_net_rcv_skb(int idx, struct sk_buff *skb)
{
	isdn_net_dev *p = dev->rx_netdev[idx];

	if (p) {
		isdn_net_local *lp = p->local;
		if ((lp->flags & ISDN_NET_CONNECTED) &&
		    (!lp->dialstate)) {
			isdn_net_receive(p->dev, skb);
			return 1;
		}
	}
	return 0;
}


static int isdn_net_header(struct sk_buff *skb, struct net_device *dev,
			   unsigned short type,
			   const void *daddr, const void *saddr, unsigned plen)
{
	isdn_net_local *lp = netdev_priv(dev);
	unsigned char *p;
	int len = 0;

	switch (lp->p_encap) {
	case ISDN_NET_ENCAP_ETHER:
		len = eth_header(skb, dev, type, daddr, saddr, plen);
		break;
#ifdef CONFIG_ISDN_PPP
	case ISDN_NET_ENCAP_SYNCPPP:
		
		len = IPPP_MAX_HEADER;
		skb_push(skb, len);
		break;
#endif
	case ISDN_NET_ENCAP_RAWIP:
		printk(KERN_WARNING "isdn_net_header called with RAW_IP!\n");
		len = 0;
		break;
	case ISDN_NET_ENCAP_IPTYP:
		
		*((__be16 *)skb_push(skb, 2)) = htons(type);
		len = 2;
		break;
	case ISDN_NET_ENCAP_UIHDLC:
		
		*((__be16 *)skb_push(skb, 2)) = htons(0x0103);
		len = 2;
		break;
	case ISDN_NET_ENCAP_CISCOHDLC:
	case ISDN_NET_ENCAP_CISCOHDLCK:
		p = skb_push(skb, 4);
		*(u8 *)(p + 0) = CISCO_ADDR_UNICAST;
		*(u8 *)(p + 1) = CISCO_CTRL;
		*(__be16 *)(p + 2) = cpu_to_be16(type);
		p += 4;
		len = 4;
		break;
#ifdef CONFIG_ISDN_X25
	default:
		
		if (lp->netdev->cprot) {
			printk(KERN_WARNING "isdn_net_header called with concap_proto!\n");
			len = 0;
			break;
		}
		break;
#endif 
	}
	return len;
}

static int
isdn_net_rebuild_header(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	isdn_net_local *lp = netdev_priv(dev);
	int ret = 0;

	if (lp->p_encap == ISDN_NET_ENCAP_ETHER) {
		struct ethhdr *eth = (struct ethhdr *) skb->data;


		if (eth->h_proto != htons(ETH_P_IP)) {
			printk(KERN_WARNING
			       "isdn_net: %s don't know how to resolve type %d addresses?\n",
			       dev->name, (int) eth->h_proto);
			memcpy(eth->h_source, dev->dev_addr, dev->addr_len);
			return 0;
		}
#ifdef CONFIG_INET
		ret = arp_find(eth->h_dest, skb);
#endif
	}
	return ret;
}

static int isdn_header_cache(const struct neighbour *neigh, struct hh_cache *hh,
			     __be16 type)
{
	const struct net_device *dev = neigh->dev;
	isdn_net_local *lp = netdev_priv(dev);

	if (lp->p_encap == ISDN_NET_ENCAP_ETHER)
		return eth_header_cache(neigh, hh, type);
	return -1;
}

static void isdn_header_cache_update(struct hh_cache *hh,
				     const struct net_device *dev,
				     const unsigned char *haddr)
{
	isdn_net_local *lp = netdev_priv(dev);
	if (lp->p_encap == ISDN_NET_ENCAP_ETHER)
		eth_header_cache_update(hh, dev, haddr);
}

static const struct header_ops isdn_header_ops = {
	.create = isdn_net_header,
	.rebuild = isdn_net_rebuild_header,
	.cache = isdn_header_cache,
	.cache_update = isdn_header_cache_update,
};

static int
isdn_net_init(struct net_device *ndev)
{
	ushort max_hlhdr_len = 0;
	int drvidx;


	for (drvidx = 0; drvidx < ISDN_MAX_DRIVERS; drvidx++)
		if (dev->drv[drvidx])
			if (max_hlhdr_len < dev->drv[drvidx]->interface->hl_hdrlen)
				max_hlhdr_len = dev->drv[drvidx]->interface->hl_hdrlen;

	ndev->hard_header_len = ETH_HLEN + max_hlhdr_len;
	return 0;
}

static void
isdn_net_swapbind(int drvidx)
{
	isdn_net_dev *p;

#ifdef ISDN_DEBUG_NET_ICALL
	printk(KERN_DEBUG "n_fi: swapping ch of %d\n", drvidx);
#endif
	p = dev->netdev;
	while (p) {
		if (p->local->pre_device == drvidx)
			switch (p->local->pre_channel) {
			case 0:
				p->local->pre_channel = 1;
				break;
			case 1:
				p->local->pre_channel = 0;
				break;
			}
		p = (isdn_net_dev *) p->next;
	}
}

static void
isdn_net_swap_usage(int i1, int i2)
{
	int u1 = dev->usage[i1] & ISDN_USAGE_EXCLUSIVE;
	int u2 = dev->usage[i2] & ISDN_USAGE_EXCLUSIVE;

#ifdef ISDN_DEBUG_NET_ICALL
	printk(KERN_DEBUG "n_fi: usage of %d and %d\n", i1, i2);
#endif
	dev->usage[i1] &= ~ISDN_USAGE_EXCLUSIVE;
	dev->usage[i1] |= u2;
	dev->usage[i2] &= ~ISDN_USAGE_EXCLUSIVE;
	dev->usage[i2] |= u1;
	isdn_info_update();
}


int
isdn_net_find_icall(int di, int ch, int idx, setup_parm *setup)
{
	char *eaz;
	int si1;
	int si2;
	int ematch;
	int wret;
	int swapped;
	int sidx = 0;
	u_long flags;
	isdn_net_dev *p;
	isdn_net_phone *n;
	char nr[ISDN_MSNLEN];
	char *my_eaz;

	
	if (!setup->phone[0]) {
		nr[0] = '0';
		nr[1] = '\0';
		printk(KERN_INFO "isdn_net: Incoming call without OAD, assuming '0'\n");
	} else
		strlcpy(nr, setup->phone, ISDN_MSNLEN);
	si1 = (int) setup->si1;
	si2 = (int) setup->si2;
	if (!setup->eazmsn[0]) {
		printk(KERN_WARNING "isdn_net: Incoming call without CPN, assuming '0'\n");
		eaz = "0";
	} else
		eaz = setup->eazmsn;
	if (dev->net_verbose > 1)
		printk(KERN_INFO "isdn_net: call from %s,%d,%d -> %s\n", nr, si1, si2, eaz);
	if ((si1 != 7) && (si1 != 1)) {
		if (dev->net_verbose > 1)
			printk(KERN_INFO "isdn_net: Service-Indicator not 1 or 7, ignored\n");
		return 0;
	}
	n = (isdn_net_phone *) 0;
	p = dev->netdev;
	ematch = wret = swapped = 0;
#ifdef ISDN_DEBUG_NET_ICALL
	printk(KERN_DEBUG "n_fi: di=%d ch=%d idx=%d usg=%d\n", di, ch, idx,
	       dev->usage[idx]);
#endif
	while (p) {
		int matchret;
		isdn_net_local *lp = p->local;

		
		switch (swapped) {
		case 2:
			isdn_net_swap_usage(idx, sidx);
			
		case 1:
			isdn_net_swapbind(di);
			break;
		}
		swapped = 0;
		
		my_eaz = isdn_map_eaz2msn(lp->msn, di);
		if (si1 == 1) { 
			if (*my_eaz == 'v' || *my_eaz == 'V' ||
			    *my_eaz == 'b' || *my_eaz == 'B')
				my_eaz++; 
			else
				my_eaz = NULL; 
		} else { 
			if (*my_eaz == 'b' || *my_eaz == 'B')
				my_eaz++; 
		}
		if (my_eaz)
			matchret = isdn_msncmp(eaz, my_eaz);
		else
			matchret = 1;
		if (!matchret)
			ematch = 1;

		
		if (matchret > wret)
			wret = matchret;
#ifdef ISDN_DEBUG_NET_ICALL
		printk(KERN_DEBUG "n_fi: if='%s', l.msn=%s, l.flags=%d, l.dstate=%d\n",
		       p->dev->name, lp->msn, lp->flags, lp->dialstate);
#endif
		if ((!matchret) &&                                        
		    (((!(lp->flags & ISDN_NET_CONNECTED)) &&              
		      (USG_NONE(dev->usage[idx]))) ||                     
		     ((((lp->dialstate == 4) || (lp->dialstate == 12)) && 
		       (!(lp->flags & ISDN_NET_CALLBACK)))                
			     )))
		{
#ifdef ISDN_DEBUG_NET_ICALL
			printk(KERN_DEBUG "n_fi: match1, pdev=%d pch=%d\n",
			       lp->pre_device, lp->pre_channel);
#endif
			if (dev->usage[idx] & ISDN_USAGE_EXCLUSIVE) {
				if ((lp->pre_channel != ch) ||
				    (lp->pre_device != di)) {
					if (ch == 0) {
						sidx = isdn_dc2minor(di, 1);
#ifdef ISDN_DEBUG_NET_ICALL
						printk(KERN_DEBUG "n_fi: ch is 0\n");
#endif
						if (USG_NONE(dev->usage[sidx])) {
							if (dev->usage[sidx] & ISDN_USAGE_EXCLUSIVE) {
#ifdef ISDN_DEBUG_NET_ICALL
								printk(KERN_DEBUG "n_fi: 2nd channel is down and bound\n");
#endif
								if ((lp->pre_device == di) &&
								    (lp->pre_channel == 1)) {
									isdn_net_swapbind(di);
									swapped = 1;
								} else {
									
									p = (isdn_net_dev *) p->next;
									continue;
								}
							} else {
#ifdef ISDN_DEBUG_NET_ICALL
								printk(KERN_DEBUG "n_fi: 2nd channel is down and unbound\n");
#endif
								
								isdn_net_swap_usage(idx, sidx);
								isdn_net_swapbind(di);
								swapped = 2;
							}
							
#ifdef ISDN_DEBUG_NET_ICALL
							printk(KERN_DEBUG "n_fi: final check\n");
#endif
							if ((dev->usage[idx] & ISDN_USAGE_EXCLUSIVE) &&
							    ((lp->pre_channel != ch) ||
							     (lp->pre_device != di))) {
#ifdef ISDN_DEBUG_NET_ICALL
								printk(KERN_DEBUG "n_fi: final check failed\n");
#endif
								p = (isdn_net_dev *) p->next;
								continue;
							}
						}
					} else {
						
#ifdef ISDN_DEBUG_NET_ICALL
						printk(KERN_DEBUG "n_fi: already on 2nd channel\n");
#endif
					}
				}
			}
#ifdef ISDN_DEBUG_NET_ICALL
			printk(KERN_DEBUG "n_fi: match2\n");
#endif
			n = lp->phone[0];
			if (lp->flags & ISDN_NET_SECURE) {
				while (n) {
					if (!isdn_msncmp(nr, n->num))
						break;
					n = (isdn_net_phone *) n->next;
				}
			}
			if (n || (!(lp->flags & ISDN_NET_SECURE))) {
#ifdef ISDN_DEBUG_NET_ICALL
				printk(KERN_DEBUG "n_fi: match3\n");
#endif
				

				if (ISDN_NET_DIALMODE(*lp) == ISDN_NET_DM_OFF) {
					printk(KERN_INFO "incoming call, interface %s `stopped' -> rejected\n",
					       p->dev->name);
					return 3;
				}
				if (!isdn_net_device_started(p)) {
					printk(KERN_INFO "%s: incoming call, interface down -> rejected\n",
					       p->dev->name);
					return 3;
				}
				if (lp->master) {
					isdn_net_local *mlp = ISDN_MASTER_PRIV(lp);
					printk(KERN_DEBUG "ICALLslv: %s\n", p->dev->name);
					printk(KERN_DEBUG "master=%s\n", lp->master->name);
					if (mlp->flags & ISDN_NET_CONNECTED) {
						printk(KERN_DEBUG "master online\n");
						
						while (mlp->slave) {
							if (ISDN_SLAVE_PRIV(mlp) == lp)
								break;
							mlp = ISDN_SLAVE_PRIV(mlp);
						}
					} else
						printk(KERN_DEBUG "master offline\n");
					
					printk(KERN_DEBUG "mlpf: %d\n", mlp->flags & ISDN_NET_CONNECTED);
					if (!(mlp->flags & ISDN_NET_CONNECTED)) {
						p = (isdn_net_dev *) p->next;
						continue;
					}
				}
				if (lp->flags & ISDN_NET_CALLBACK) {
					int chi;
					if (ISDN_NET_DIALMODE(*lp) == ISDN_NET_DM_OFF) {
						printk(KERN_INFO "incoming call for callback, interface %s `off' -> rejected\n",
						       p->dev->name);
						return 3;
					}
					printk(KERN_DEBUG "%s: call from %s -> %s, start callback\n",
					       p->dev->name, nr, eaz);
					if (lp->phone[1]) {
						
						spin_lock_irqsave(&dev->lock, flags);
						if ((chi =
						     isdn_get_free_channel(
							     ISDN_USAGE_NET,
							     lp->l2_proto,
							     lp->l3_proto,
							     lp->pre_device,
							     lp->pre_channel,
							     lp->msn)
							    ) < 0) {

							printk(KERN_WARNING "isdn_net_find_icall: No channel for %s\n",
							       p->dev->name);
							spin_unlock_irqrestore(&dev->lock, flags);
							return 0;
						}
						
						lp->dtimer = 0;
						lp->dialstate = 11;
						
						isdn_net_bind_channel(lp, chi);
#ifdef CONFIG_ISDN_PPP
						if (lp->p_encap == ISDN_NET_ENCAP_SYNCPPP)
							if (isdn_ppp_bind(lp) < 0) {
								spin_unlock_irqrestore(&dev->lock, flags);
								isdn_net_unbind_channel(lp);
								return 0;
							}
#endif
						spin_unlock_irqrestore(&dev->lock, flags);
						
						return (lp->flags & ISDN_NET_CBHUP) ? 2 : 4;
					} else
						printk(KERN_WARNING "isdn_net: %s: No phone number\n",
						       p->dev->name);
					return 0;
				} else {
					printk(KERN_DEBUG "%s: call from %s -> %s accepted\n",
					       p->dev->name, nr, eaz);
					if ((lp->dialstate == 4) || (lp->dialstate == 12)) {
#ifdef CONFIG_ISDN_PPP
						if (lp->p_encap == ISDN_NET_ENCAP_SYNCPPP)
							isdn_ppp_free(lp);
#endif
						isdn_net_lp_disconnected(lp);
						isdn_free_channel(lp->isdn_device, lp->isdn_channel,
								  ISDN_USAGE_NET);
					}
					spin_lock_irqsave(&dev->lock, flags);
					dev->usage[idx] &= ISDN_USAGE_EXCLUSIVE;
					dev->usage[idx] |= ISDN_USAGE_NET;
					strcpy(dev->num[idx], nr);
					isdn_info_update();
					dev->st_netdev[idx] = lp->netdev;
					lp->isdn_device = di;
					lp->isdn_channel = ch;
					lp->ppp_slot = -1;
					lp->flags |= ISDN_NET_CONNECTED;
					lp->dialstate = 7;
					lp->dtimer = 0;
					lp->outgoing = 0;
					lp->huptimer = 0;
					lp->hupflags |= ISDN_WAITCHARGE;
					lp->hupflags &= ~ISDN_HAVECHARGE;
#ifdef CONFIG_ISDN_PPP
					if (lp->p_encap == ISDN_NET_ENCAP_SYNCPPP) {
						if (isdn_ppp_bind(lp) < 0) {
							isdn_net_unbind_channel(lp);
							spin_unlock_irqrestore(&dev->lock, flags);
							return 0;
						}
					}
#endif
					spin_unlock_irqrestore(&dev->lock, flags);
					return 1;
				}
			}
		}
		p = (isdn_net_dev *) p->next;
	}
	
	if (!ematch || dev->net_verbose)
		printk(KERN_INFO "isdn_net: call from %s -> %d %s ignored\n", nr, di, eaz);
	return (wret == 2) ? 5 : 0;
}

isdn_net_dev *
isdn_net_findif(char *name)
{
	isdn_net_dev *p = dev->netdev;

	while (p) {
		if (!strcmp(p->dev->name, name))
			return p;
		p = (isdn_net_dev *) p->next;
	}
	return (isdn_net_dev *) NULL;
}

static int
isdn_net_force_dial_lp(isdn_net_local *lp)
{
	if ((!(lp->flags & ISDN_NET_CONNECTED)) && !lp->dialstate) {
		int chi;
		if (lp->phone[1]) {
			ulong flags;

			
			spin_lock_irqsave(&dev->lock, flags);
			if ((chi = isdn_get_free_channel(
				     ISDN_USAGE_NET,
				     lp->l2_proto,
				     lp->l3_proto,
				     lp->pre_device,
				     lp->pre_channel,
				     lp->msn)) < 0) {
				printk(KERN_WARNING "isdn_net_force_dial: No channel for %s\n",
				       lp->netdev->dev->name);
				spin_unlock_irqrestore(&dev->lock, flags);
				return -EAGAIN;
			}
			lp->dialstate = 1;
			
			isdn_net_bind_channel(lp, chi);
#ifdef CONFIG_ISDN_PPP
			if (lp->p_encap == ISDN_NET_ENCAP_SYNCPPP)
				if (isdn_ppp_bind(lp) < 0) {
					isdn_net_unbind_channel(lp);
					spin_unlock_irqrestore(&dev->lock, flags);
					return -EAGAIN;
				}
#endif
			
			spin_unlock_irqrestore(&dev->lock, flags);
			isdn_net_dial();
			return 0;
		} else
			return -EINVAL;
	} else
		return -EBUSY;
}

int
isdn_net_dial_req(isdn_net_local *lp)
{
	
	if (!(ISDN_NET_DIALMODE(*lp) == ISDN_NET_DM_AUTO)) return -EBUSY;

	return isdn_net_force_dial_lp(lp);
}

int
isdn_net_force_dial(char *name)
{
	isdn_net_dev *p = isdn_net_findif(name);

	if (!p)
		return -ENODEV;
	return (isdn_net_force_dial_lp(p->local));
}

static const struct net_device_ops isdn_netdev_ops = {
	.ndo_init	      = isdn_net_init,
	.ndo_open	      = isdn_net_open,
	.ndo_stop	      = isdn_net_close,
	.ndo_do_ioctl	      = isdn_net_ioctl,

	.ndo_start_xmit	      = isdn_net_start_xmit,
	.ndo_get_stats	      = isdn_net_get_stats,
	.ndo_tx_timeout	      = isdn_net_tx_timeout,
};

static void _isdn_setup(struct net_device *dev)
{
	isdn_net_local *lp = netdev_priv(dev);

	ether_setup(dev);

	
	dev->flags = IFF_NOARP | IFF_POINTOPOINT;

	
	dev->priv_flags &= ~IFF_TX_SKB_SHARING;
	dev->header_ops = NULL;
	dev->netdev_ops = &isdn_netdev_ops;

	
	dev->tx_queue_len = 30;

	lp->p_encap = ISDN_NET_ENCAP_RAWIP;
	lp->magic = ISDN_NET_MAGIC;
	lp->last = lp;
	lp->next = lp;
	lp->isdn_device = -1;
	lp->isdn_channel = -1;
	lp->pre_device = -1;
	lp->pre_channel = -1;
	lp->exclusive = -1;
	lp->ppp_slot = -1;
	lp->pppbind = -1;
	skb_queue_head_init(&lp->super_tx_queue);
	lp->l2_proto = ISDN_PROTO_L2_X75I;
	lp->l3_proto = ISDN_PROTO_L3_TRANS;
	lp->triggercps = 6000;
	lp->slavedelay = 10 * HZ;
	lp->hupflags = ISDN_INHUP;	
	lp->onhtime = 10;	
	lp->dialmax = 1;
	
	lp->flags = ISDN_NET_CBHUP | ISDN_NET_DM_MANUAL;
	lp->cbdelay = 25;	
	lp->dialtimeout = -1;  
	lp->dialwait = 5 * HZ; 
	lp->dialstarted = 0;   
	lp->dialwait_timer = 0;  
}

char *
isdn_net_new(char *name, struct net_device *master)
{
	isdn_net_dev *netdev;

	
	if (isdn_net_findif(name)) {
		printk(KERN_WARNING "isdn_net: interface %s already exists\n", name);
		return NULL;
	}
	if (name == NULL)
		return NULL;
	if (!(netdev = kzalloc(sizeof(isdn_net_dev), GFP_KERNEL))) {
		printk(KERN_WARNING "isdn_net: Could not allocate net-device\n");
		return NULL;
	}
	netdev->dev = alloc_netdev(sizeof(isdn_net_local), name, _isdn_setup);
	if (!netdev->dev) {
		printk(KERN_WARNING "isdn_net: Could not allocate network device\n");
		kfree(netdev);
		return NULL;
	}
	netdev->local = netdev_priv(netdev->dev);

	if (master) {
		
		struct net_device *p = MASTER_TO_SLAVE(master);
		struct net_device *q = master;

		netdev->local->master = master;
		
		while (p) {
			q = p;
			p = MASTER_TO_SLAVE(p);
		}
		MASTER_TO_SLAVE(q) = netdev->dev;
	} else {
		
		netdev->dev->watchdog_timeo = ISDN_NET_TX_TIMEOUT;
		if (register_netdev(netdev->dev) != 0) {
			printk(KERN_WARNING "isdn_net: Could not register net-device\n");
			free_netdev(netdev->dev);
			kfree(netdev);
			return NULL;
		}
	}
	netdev->queue = netdev->local;
	spin_lock_init(&netdev->queue_lock);

	netdev->local->netdev = netdev;

	INIT_WORK(&netdev->local->tqueue, isdn_net_softint);
	spin_lock_init(&netdev->local->xmit_lock);

	
	netdev->next = (void *) dev->netdev;
	dev->netdev = netdev;
	return netdev->dev->name;
}

char *
isdn_net_newslave(char *parm)
{
	char *p = strchr(parm, ',');
	isdn_net_dev *n;
	char newname[10];

	if (p) {
		
		if (!strlen(p + 1))
			return NULL;
		strcpy(newname, p + 1);
		*p = 0;
		
		if (!(n = isdn_net_findif(parm)))
			return NULL;
		
		if (n->local->master)
			return NULL;
		
		if (isdn_net_device_started(n))
			return NULL;
		return (isdn_net_new(newname, n->dev));
	}
	return NULL;
}

int
isdn_net_setcfg(isdn_net_ioctl_cfg *cfg)
{
	isdn_net_dev *p = isdn_net_findif(cfg->name);
	ulong features;
	int i;
	int drvidx;
	int chidx;
	char drvid[25];

	if (p) {
		isdn_net_local *lp = p->local;

		
		features = ((1 << cfg->l2_proto) << ISDN_FEATURE_L2_SHIFT) |
			((1 << cfg->l3_proto) << ISDN_FEATURE_L3_SHIFT);
		for (i = 0; i < ISDN_MAX_DRIVERS; i++)
			if (dev->drv[i])
				if ((dev->drv[i]->interface->features & features) == features)
					break;
		if (i == ISDN_MAX_DRIVERS) {
			printk(KERN_WARNING "isdn_net: No driver with selected features\n");
			return -ENODEV;
		}
		if (lp->p_encap != cfg->p_encap) {
#ifdef CONFIG_ISDN_X25
			struct concap_proto *cprot = p->cprot;
#endif
			if (isdn_net_device_started(p)) {
				printk(KERN_WARNING "%s: cannot change encap when if is up\n",
				       p->dev->name);
				return -EBUSY;
			}
#ifdef CONFIG_ISDN_X25
			if (cprot && cprot->pops)
				cprot->pops->proto_del(cprot);
			p->cprot = NULL;
			lp->dops = NULL;
			
			switch (cfg->p_encap) {
			case ISDN_NET_ENCAP_X25IFACE:
				lp->dops = &isdn_concap_reliable_dl_dops;
			}
			
			p->cprot = isdn_concap_new(cfg->p_encap);
#endif
		}
		switch (cfg->p_encap) {
		case ISDN_NET_ENCAP_SYNCPPP:
#ifndef CONFIG_ISDN_PPP
			printk(KERN_WARNING "%s: SyncPPP support not configured\n",
			       p->dev->name);
			return -EINVAL;
#else
			p->dev->type = ARPHRD_PPP;	
			p->dev->addr_len = 0;
#endif
			break;
		case ISDN_NET_ENCAP_X25IFACE:
#ifndef CONFIG_ISDN_X25
			printk(KERN_WARNING "%s: isdn-x25 support not configured\n",
			       p->dev->name);
			return -EINVAL;
#else
			p->dev->type = ARPHRD_X25;	
			p->dev->addr_len = 0;
#endif
			break;
		case ISDN_NET_ENCAP_CISCOHDLCK:
			break;
		default:
			if (cfg->p_encap >= 0 &&
			    cfg->p_encap <= ISDN_NET_ENCAP_MAX_ENCAP)
				break;
			printk(KERN_WARNING
			       "%s: encapsulation protocol %d not supported\n",
			       p->dev->name, cfg->p_encap);
			return -EINVAL;
		}
		if (strlen(cfg->drvid)) {
			
			char *c,
				*e;

			if (strnlen(cfg->drvid, sizeof(cfg->drvid)) ==
			    sizeof(cfg->drvid))
				return -EINVAL;
			drvidx = -1;
			chidx = -1;
			strcpy(drvid, cfg->drvid);
			if ((c = strchr(drvid, ','))) {
				
				chidx = (int) simple_strtoul(c + 1, &e, 10);
				if (e == c)
					chidx = -1;
				*c = '\0';
			}
			for (i = 0; i < ISDN_MAX_DRIVERS; i++)
				
				if (!(strcmp(dev->drvid[i], drvid))) {
					drvidx = i;
					break;
				}
			if ((drvidx == -1) || (chidx == -1))
				
				return -ENODEV;
		} else {
			
			drvidx = lp->pre_device;
			chidx = lp->pre_channel;
		}
		if (cfg->exclusive > 0) {
			unsigned long flags;

			
			spin_lock_irqsave(&dev->lock, flags);
			if ((i = isdn_get_free_channel(ISDN_USAGE_NET,
						       lp->l2_proto, lp->l3_proto, drvidx,
						       chidx, lp->msn)) < 0) {
				
				lp->exclusive = -1;
				spin_unlock_irqrestore(&dev->lock, flags);
				return -EBUSY;
			}
			
			dev->usage[i] = ISDN_USAGE_EXCLUSIVE;
			isdn_info_update();
			spin_unlock_irqrestore(&dev->lock, flags);
			lp->exclusive = i;
		} else {
			
			lp->exclusive = -1;
			if ((lp->pre_device != -1) && (cfg->exclusive == -1)) {
				isdn_unexclusive_channel(lp->pre_device, lp->pre_channel);
				isdn_free_channel(lp->pre_device, lp->pre_channel, ISDN_USAGE_NET);
				drvidx = -1;
				chidx = -1;
			}
		}
		strlcpy(lp->msn, cfg->eaz, sizeof(lp->msn));
		lp->pre_device = drvidx;
		lp->pre_channel = chidx;
		lp->onhtime = cfg->onhtime;
		lp->charge = cfg->charge;
		lp->l2_proto = cfg->l2_proto;
		lp->l3_proto = cfg->l3_proto;
		lp->cbdelay = cfg->cbdelay;
		lp->dialmax = cfg->dialmax;
		lp->triggercps = cfg->triggercps;
		lp->slavedelay = cfg->slavedelay * HZ;
		lp->pppbind = cfg->pppbind;
		lp->dialtimeout = cfg->dialtimeout >= 0 ? cfg->dialtimeout * HZ : -1;
		lp->dialwait = cfg->dialwait * HZ;
		if (cfg->secure)
			lp->flags |= ISDN_NET_SECURE;
		else
			lp->flags &= ~ISDN_NET_SECURE;
		if (cfg->cbhup)
			lp->flags |= ISDN_NET_CBHUP;
		else
			lp->flags &= ~ISDN_NET_CBHUP;
		switch (cfg->callback) {
		case 0:
			lp->flags &= ~(ISDN_NET_CALLBACK | ISDN_NET_CBOUT);
			break;
		case 1:
			lp->flags |= ISDN_NET_CALLBACK;
			lp->flags &= ~ISDN_NET_CBOUT;
			break;
		case 2:
			lp->flags |= ISDN_NET_CBOUT;
			lp->flags &= ~ISDN_NET_CALLBACK;
			break;
		}
		lp->flags &= ~ISDN_NET_DIALMODE_MASK;	
		if (cfg->dialmode && !(cfg->dialmode & ISDN_NET_DIALMODE_MASK)) {
			
			printk(KERN_WARNING
			       "Old isdnctrl version detected! Please update.\n");
			lp->flags |= ISDN_NET_DM_OFF; 
		}
		else {
			lp->flags |= cfg->dialmode;  
		}
		if (cfg->chargehup)
			lp->hupflags |= ISDN_CHARGEHUP;
		else
			lp->hupflags &= ~ISDN_CHARGEHUP;
		if (cfg->ihup)
			lp->hupflags |= ISDN_INHUP;
		else
			lp->hupflags &= ~ISDN_INHUP;
		if (cfg->chargeint > 10) {
			lp->hupflags |= ISDN_CHARGEHUP | ISDN_HAVECHARGE | ISDN_MANCHARGE;
			lp->chargeint = cfg->chargeint * HZ;
		}
		if (cfg->p_encap != lp->p_encap) {
			if (cfg->p_encap == ISDN_NET_ENCAP_RAWIP) {
				p->dev->header_ops = NULL;
				p->dev->flags = IFF_NOARP | IFF_POINTOPOINT;
			} else {
				p->dev->header_ops = &isdn_header_ops;
				if (cfg->p_encap == ISDN_NET_ENCAP_ETHER)
					p->dev->flags = IFF_BROADCAST | IFF_MULTICAST;
				else
					p->dev->flags = IFF_NOARP | IFF_POINTOPOINT;
			}
		}
		lp->p_encap = cfg->p_encap;
		return 0;
	}
	return -ENODEV;
}

int
isdn_net_getcfg(isdn_net_ioctl_cfg *cfg)
{
	isdn_net_dev *p = isdn_net_findif(cfg->name);

	if (p) {
		isdn_net_local *lp = p->local;

		strcpy(cfg->eaz, lp->msn);
		cfg->exclusive = lp->exclusive;
		if (lp->pre_device >= 0) {
			sprintf(cfg->drvid, "%s,%d", dev->drvid[lp->pre_device],
				lp->pre_channel);
		} else
			cfg->drvid[0] = '\0';
		cfg->onhtime = lp->onhtime;
		cfg->charge = lp->charge;
		cfg->l2_proto = lp->l2_proto;
		cfg->l3_proto = lp->l3_proto;
		cfg->p_encap = lp->p_encap;
		cfg->secure = (lp->flags & ISDN_NET_SECURE) ? 1 : 0;
		cfg->callback = 0;
		if (lp->flags & ISDN_NET_CALLBACK)
			cfg->callback = 1;
		if (lp->flags & ISDN_NET_CBOUT)
			cfg->callback = 2;
		cfg->cbhup = (lp->flags & ISDN_NET_CBHUP) ? 1 : 0;
		cfg->dialmode = lp->flags & ISDN_NET_DIALMODE_MASK;
		cfg->chargehup = (lp->hupflags & 4) ? 1 : 0;
		cfg->ihup = (lp->hupflags & 8) ? 1 : 0;
		cfg->cbdelay = lp->cbdelay;
		cfg->dialmax = lp->dialmax;
		cfg->triggercps = lp->triggercps;
		cfg->slavedelay = lp->slavedelay / HZ;
		cfg->chargeint = (lp->hupflags & ISDN_CHARGEHUP) ?
			(lp->chargeint / HZ) : 0;
		cfg->pppbind = lp->pppbind;
		cfg->dialtimeout = lp->dialtimeout >= 0 ? lp->dialtimeout / HZ : -1;
		cfg->dialwait = lp->dialwait / HZ;
		if (lp->slave) {
			if (strlen(lp->slave->name) >= 10)
				strcpy(cfg->slave, "too-long");
			else
				strcpy(cfg->slave, lp->slave->name);
		} else
			cfg->slave[0] = '\0';
		if (lp->master) {
			if (strlen(lp->master->name) >= 10)
				strcpy(cfg->master, "too-long");
			else
				strcpy(cfg->master, lp->master->name);
		} else
			cfg->master[0] = '\0';
		return 0;
	}
	return -ENODEV;
}

int
isdn_net_addphone(isdn_net_ioctl_phone *phone)
{
	isdn_net_dev *p = isdn_net_findif(phone->name);
	isdn_net_phone *n;

	if (p) {
		if (!(n = kmalloc(sizeof(isdn_net_phone), GFP_KERNEL)))
			return -ENOMEM;
		strlcpy(n->num, phone->phone, sizeof(n->num));
		n->next = p->local->phone[phone->outgoing & 1];
		p->local->phone[phone->outgoing & 1] = n;
		return 0;
	}
	return -ENODEV;
}

int
isdn_net_getphones(isdn_net_ioctl_phone *phone, char __user *phones)
{
	isdn_net_dev *p = isdn_net_findif(phone->name);
	int inout = phone->outgoing & 1;
	int more = 0;
	int count = 0;
	isdn_net_phone *n;

	if (!p)
		return -ENODEV;
	inout &= 1;
	for (n = p->local->phone[inout]; n; n = n->next) {
		if (more) {
			put_user(' ', phones++);
			count++;
		}
		if (copy_to_user(phones, n->num, strlen(n->num) + 1)) {
			return -EFAULT;
		}
		phones += strlen(n->num);
		count += strlen(n->num);
		more = 1;
	}
	put_user(0, phones);
	count++;
	return count;
}

int
isdn_net_getpeer(isdn_net_ioctl_phone *phone, isdn_net_ioctl_phone __user *peer)
{
	isdn_net_dev *p = isdn_net_findif(phone->name);
	int ch, dv, idx;

	if (!p)
		return -ENODEV;
	ch = p->local->isdn_channel;
	dv = p->local->isdn_device;
	if (ch < 0 && dv < 0)
		return -ENOTCONN;
	idx = isdn_dc2minor(dv, ch);
	if (idx < 0)
		return -ENODEV;
	
	if (strncmp(dev->num[idx], "???", 3) == 0)
		return -ENOTCONN;
	strncpy(phone->phone, dev->num[idx], ISDN_MSNLEN);
	phone->outgoing = USG_OUTGOING(dev->usage[idx]);
	if (copy_to_user(peer, phone, sizeof(*peer)))
		return -EFAULT;
	return 0;
}
int
isdn_net_delphone(isdn_net_ioctl_phone *phone)
{
	isdn_net_dev *p = isdn_net_findif(phone->name);
	int inout = phone->outgoing & 1;
	isdn_net_phone *n;
	isdn_net_phone *m;

	if (p) {
		n = p->local->phone[inout];
		m = NULL;
		while (n) {
			if (!strcmp(n->num, phone->phone)) {
				if (p->local->dial == n)
					p->local->dial = n->next;
				if (m)
					m->next = n->next;
				else
					p->local->phone[inout] = n->next;
				kfree(n);
				return 0;
			}
			m = n;
			n = (isdn_net_phone *) n->next;
		}
		return -EINVAL;
	}
	return -ENODEV;
}

static int
isdn_net_rmallphone(isdn_net_dev *p)
{
	isdn_net_phone *n;
	isdn_net_phone *m;
	int i;

	for (i = 0; i < 2; i++) {
		n = p->local->phone[i];
		while (n) {
			m = n->next;
			kfree(n);
			n = m;
		}
		p->local->phone[i] = NULL;
	}
	p->local->dial = NULL;
	return 0;
}

int
isdn_net_force_hangup(char *name)
{
	isdn_net_dev *p = isdn_net_findif(name);
	struct net_device *q;

	if (p) {
		if (p->local->isdn_device < 0)
			return 1;
		q = p->local->slave;
		
		while (q) {
			isdn_net_hangup(q);
			q = MASTER_TO_SLAVE(q);
		}
		isdn_net_hangup(p->dev);
		return 0;
	}
	return -ENODEV;
}

static int
isdn_net_realrm(isdn_net_dev *p, isdn_net_dev *q)
{
	u_long flags;

	if (isdn_net_device_started(p)) {
		return -EBUSY;
	}
#ifdef CONFIG_ISDN_X25
	if (p->cprot && p->cprot->pops)
		p->cprot->pops->proto_del(p->cprot);
#endif
	
	isdn_net_rmallphone(p);
	
	if (p->local->exclusive != -1)
		isdn_unexclusive_channel(p->local->pre_device, p->local->pre_channel);
	if (p->local->master) {
		
		if (((isdn_net_local *) ISDN_MASTER_PRIV(p->local))->slave ==
		    p->dev)
			((isdn_net_local *)ISDN_MASTER_PRIV(p->local))->slave =
				p->local->slave;
	} else {
		
		unregister_netdev(p->dev);
	}
	
	spin_lock_irqsave(&dev->lock, flags);
	if (q)
		q->next = p->next;
	else
		dev->netdev = p->next;
	if (p->local->slave) {
		
		char *slavename = p->local->slave->name;
		isdn_net_dev *n = dev->netdev;
		q = NULL;
		while (n) {
			if (!strcmp(n->dev->name, slavename)) {
				spin_unlock_irqrestore(&dev->lock, flags);
				isdn_net_realrm(n, q);
				spin_lock_irqsave(&dev->lock, flags);
				break;
			}
			q = n;
			n = (isdn_net_dev *)n->next;
		}
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	
	if (dev->netdev == NULL)
		isdn_timer_ctrl(ISDN_TIMER_NETHANGUP, 0);
	free_netdev(p->dev);
	kfree(p);

	return 0;
}

int
isdn_net_rm(char *name)
{
	u_long flags;
	isdn_net_dev *p;
	isdn_net_dev *q;

	
	spin_lock_irqsave(&dev->lock, flags);
	p = dev->netdev;
	q = NULL;
	while (p) {
		if (!strcmp(p->dev->name, name)) {
			spin_unlock_irqrestore(&dev->lock, flags);
			return (isdn_net_realrm(p, q));
		}
		q = p;
		p = (isdn_net_dev *) p->next;
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	
	if (dev->netdev == NULL)
		isdn_timer_ctrl(ISDN_TIMER_NETHANGUP, 0);
	return -ENODEV;
}

int
isdn_net_rmall(void)
{
	u_long flags;
	int ret;

	
	spin_lock_irqsave(&dev->lock, flags);
	while (dev->netdev) {
		if (!dev->netdev->local->master) {
			
			spin_unlock_irqrestore(&dev->lock, flags);
			if ((ret = isdn_net_realrm(dev->netdev, NULL))) {
				return ret;
			}
			spin_lock_irqsave(&dev->lock, flags);
		}
	}
	dev->netdev = NULL;
	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}
