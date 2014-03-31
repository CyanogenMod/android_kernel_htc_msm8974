/* $Id: isdn_common.c,v 1.1.2.3 2004/02/10 01:07:13 keil Exp $
 *
 * Linux ISDN subsystem, common used functions (linklevel).
 *
 * Copyright 1994-1999  by Fritz Elfert (fritz@isdn4linux.de)
 * Copyright 1995,96    Thinking Objects Software GmbH Wuerzburg
 * Copyright 1995,96    by Michael Hipp (Michael.Hipp@student.uni-tuebingen.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/isdn.h>
#include <linux/mutex.h>
#include "isdn_common.h"
#include "isdn_tty.h"
#include "isdn_net.h"
#include "isdn_ppp.h"
#ifdef CONFIG_ISDN_AUDIO
#include "isdn_audio.h"
#endif
#ifdef CONFIG_ISDN_DIVERSION_MODULE
#define CONFIG_ISDN_DIVERSION
#endif
#ifdef CONFIG_ISDN_DIVERSION
#include <linux/isdn_divertif.h>
#endif 
#include "isdn_v110.h"

#undef ISDN_DEBUG_STATCALLB

MODULE_DESCRIPTION("ISDN4Linux: link layer");
MODULE_AUTHOR("Fritz Elfert");
MODULE_LICENSE("GPL");

isdn_dev *dev;

static DEFINE_MUTEX(isdn_mutex);
static char *isdn_revision = "$Revision: 1.1.2.3 $";

extern char *isdn_net_revision;
extern char *isdn_tty_revision;
#ifdef CONFIG_ISDN_PPP
extern char *isdn_ppp_revision;
#else
static char *isdn_ppp_revision = ": none $";
#endif
#ifdef CONFIG_ISDN_AUDIO
extern char *isdn_audio_revision;
#else
static char *isdn_audio_revision = ": none $";
#endif
extern char *isdn_v110_revision;

#ifdef CONFIG_ISDN_DIVERSION
static isdn_divert_if *divert_if; 
#endif 


static int isdn_writebuf_stub(int, int, const u_char __user *, int);
static void set_global_features(void);
static int isdn_wildmat(char *s, char *p);
static int isdn_add_channels(isdn_driver_t *d, int drvidx, int n, int adding);

static inline void
isdn_lock_driver(isdn_driver_t *drv)
{
	try_module_get(drv->interface->owner);
	drv->locks++;
}

void
isdn_lock_drivers(void)
{
	int i;

	for (i = 0; i < ISDN_MAX_DRIVERS; i++) {
		if (!dev->drv[i])
			continue;
		isdn_lock_driver(dev->drv[i]);
	}
}

static inline void
isdn_unlock_driver(isdn_driver_t *drv)
{
	if (drv->locks > 0) {
		drv->locks--;
		module_put(drv->interface->owner);
	}
}

void
isdn_unlock_drivers(void)
{
	int i;

	for (i = 0; i < ISDN_MAX_DRIVERS; i++) {
		if (!dev->drv[i])
			continue;
		isdn_unlock_driver(dev->drv[i]);
	}
}

#if defined(ISDN_DEBUG_NET_DUMP) || defined(ISDN_DEBUG_MODEM_DUMP)
void
isdn_dumppkt(char *s, u_char *p, int len, int dumplen)
{
	int dumpc;

	printk(KERN_DEBUG "%s(%d) ", s, len);
	for (dumpc = 0; (dumpc < dumplen) && (len); len--, dumpc++)
		printk(" %02x", *p++);
	printk("\n");
}
#endif

/*
 * I picked the pattern-matching-functions from an old GNU-tar version (1.10)
 * It was originally written and put to PD by rs@mirror.TMC.COM (Rich Salz)
 */
static int
isdn_star(char *s, char *p)
{
	while (isdn_wildmat(s, p)) {
		if (*++s == '\0')
			return (2);
	}
	return (0);
}


static int
isdn_wildmat(char *s, char *p)
{
	register int last;
	register int matched;
	register int reverse;
	register int nostar = 1;

	if (!(*s) && !(*p))
		return (1);
	for (; *p; s++, p++)
		switch (*p) {
		case '\\':
			p++;
		default:
			if (*s != *p)
				return (*s == '\0') ? 2 : 1;
					continue;
		case '?':
			
			if (*s == '\0')
				return (2);
			continue;
		case '*':
			nostar = 0;
			
			return (*++p ? isdn_star(s, p) : 0);
		case '[':
			
			if ((reverse = (p[1] == '^')))
				p++;
			for (last = 0, matched = 0; *++p && (*p != ']'); last = *p)
				
				if (*p == '-' ? *s <= *++p && *s >= last : *s == *p)
					matched = 1;
			if (matched == reverse)
				return (1);
			continue;
		}
	return (*s == '\0') ? 0 : nostar;
}

int isdn_msncmp(const char *msn1, const char *msn2)
{
	char TmpMsn1[ISDN_MSNLEN];
	char TmpMsn2[ISDN_MSNLEN];
	char *p;

	for (p = TmpMsn1; *msn1 && *msn1 != ':';)  
		*p++ = *msn1++;
	*p = '\0';

	for (p = TmpMsn2; *msn2 && *msn2 != ':';)  
		*p++ = *msn2++;
	*p = '\0';

	return isdn_wildmat(TmpMsn1, TmpMsn2);
}

int
isdn_dc2minor(int di, int ch)
{
	int i;
	for (i = 0; i < ISDN_MAX_CHANNELS; i++)
		if (dev->chanmap[i] == ch && dev->drvmap[i] == di)
			return i;
	return -1;
}

static int isdn_timer_cnt1 = 0;
static int isdn_timer_cnt2 = 0;
static int isdn_timer_cnt3 = 0;

static void
isdn_timer_funct(ulong dummy)
{
	int tf = dev->tflags;
	if (tf & ISDN_TIMER_FAST) {
		if (tf & ISDN_TIMER_MODEMREAD)
			isdn_tty_readmodem();
		if (tf & ISDN_TIMER_MODEMPLUS)
			isdn_tty_modem_escape();
		if (tf & ISDN_TIMER_MODEMXMIT)
			isdn_tty_modem_xmit();
	}
	if (tf & ISDN_TIMER_SLOW) {
		if (++isdn_timer_cnt1 >= ISDN_TIMER_02SEC) {
			isdn_timer_cnt1 = 0;
			if (tf & ISDN_TIMER_NETDIAL)
				isdn_net_dial();
		}
		if (++isdn_timer_cnt2 >= ISDN_TIMER_1SEC) {
			isdn_timer_cnt2 = 0;
			if (tf & ISDN_TIMER_NETHANGUP)
				isdn_net_autohup();
			if (++isdn_timer_cnt3 >= ISDN_TIMER_RINGING) {
				isdn_timer_cnt3 = 0;
				if (tf & ISDN_TIMER_MODEMRING)
					isdn_tty_modem_ring();
			}
			if (tf & ISDN_TIMER_CARRIER)
				isdn_tty_carrier_timeout();
		}
	}
	if (tf)
		mod_timer(&dev->timer, jiffies + ISDN_TIMER_RES);
}

void
isdn_timer_ctrl(int tf, int onoff)
{
	unsigned long flags;
	int old_tflags;

	spin_lock_irqsave(&dev->timerlock, flags);
	if ((tf & ISDN_TIMER_SLOW) && (!(dev->tflags & ISDN_TIMER_SLOW))) {
		
		isdn_timer_cnt1 = 0;
		isdn_timer_cnt2 = 0;
	}
	old_tflags = dev->tflags;
	if (onoff)
		dev->tflags |= tf;
	else
		dev->tflags &= ~tf;
	if (dev->tflags && !old_tflags)
		mod_timer(&dev->timer, jiffies + ISDN_TIMER_RES);
	spin_unlock_irqrestore(&dev->timerlock, flags);
}

static void
isdn_receive_skb_callback(int di, int channel, struct sk_buff *skb)
{
	int i;

	if ((i = isdn_dc2minor(di, channel)) == -1) {
		dev_kfree_skb(skb);
		return;
	}
	
	dev->ibytes[i] += skb->len;

	
	if (isdn_net_rcv_skb(i, skb))
		return;

	if (dev->v110[i]) {
		atomic_inc(&dev->v110use[i]);
		skb = isdn_v110_decode(dev->v110[i], skb);
		atomic_dec(&dev->v110use[i]);
		if (!skb)
			return;
	}

	
	if (skb->len) {
		if (isdn_tty_rcv_skb(i, di, channel, skb))
			return;
		wake_up_interruptible(&dev->drv[di]->rcv_waitq[channel]);
	} else
		dev_kfree_skb(skb);
}

int
isdn_command(isdn_ctrl *cmd)
{
	if (cmd->driver == -1) {
		printk(KERN_WARNING "isdn_command command(%x) driver -1\n", cmd->command);
		return (1);
	}
	if (!dev->drv[cmd->driver]) {
		printk(KERN_WARNING "isdn_command command(%x) dev->drv[%d] NULL\n",
		       cmd->command, cmd->driver);
		return (1);
	}
	if (!dev->drv[cmd->driver]->interface) {
		printk(KERN_WARNING "isdn_command command(%x) dev->drv[%d]->interface NULL\n",
		       cmd->command, cmd->driver);
		return (1);
	}
	if (cmd->command == ISDN_CMD_SETL2) {
		int idx = isdn_dc2minor(cmd->driver, cmd->arg & 255);
		unsigned long l2prot = (cmd->arg >> 8) & 255;
		unsigned long features = (dev->drv[cmd->driver]->interface->features
					  >> ISDN_FEATURE_L2_SHIFT) &
			ISDN_FEATURE_L2_MASK;
		unsigned long l2_feature = (1 << l2prot);

		switch (l2prot) {
		case ISDN_PROTO_L2_V11096:
		case ISDN_PROTO_L2_V11019:
		case ISDN_PROTO_L2_V11038:
			if (!(features & l2_feature)) {
				dev->v110emu[idx] = l2prot;
				cmd->arg = (cmd->arg & 255) |
					(ISDN_PROTO_L2_TRANS << 8);
			} else
				dev->v110emu[idx] = 0;
		}
	}
	return dev->drv[cmd->driver]->interface->command(cmd);
}

void
isdn_all_eaz(int di, int ch)
{
	isdn_ctrl cmd;

	if (di < 0)
		return;
	cmd.driver = di;
	cmd.arg = ch;
	cmd.command = ISDN_CMD_SETEAZ;
	cmd.parm.num[0] = '\0';
	isdn_command(&cmd);
}

#include <linux/isdn/capicmd.h>

static int
isdn_capi_rec_hl_msg(capi_msg *cm)
{
	switch (cm->Command) {
	case CAPI_FACILITY:
		
		return (isdn_tty_capi_facility(cm));
	default:
		return (-1);
	}
}

static int
isdn_status_callback(isdn_ctrl *c)
{
	int di;
	u_long flags;
	int i;
	int r;
	int retval = 0;
	isdn_ctrl cmd;
	isdn_net_dev *p;

	di = c->driver;
	i = isdn_dc2minor(di, c->arg);
	switch (c->command) {
	case ISDN_STAT_BSENT:
		if (i < 0)
			return -1;
		if (dev->global_flags & ISDN_GLOBAL_STOPPED)
			return 0;
		if (isdn_net_stat_callback(i, c))
			return 0;
		if (isdn_v110_stat_callback(i, c))
			return 0;
		if (isdn_tty_stat_callback(i, c))
			return 0;
		wake_up_interruptible(&dev->drv[di]->snd_waitq[c->arg]);
		break;
	case ISDN_STAT_STAVAIL:
		dev->drv[di]->stavail += c->arg;
		wake_up_interruptible(&dev->drv[di]->st_waitq);
		break;
	case ISDN_STAT_RUN:
		dev->drv[di]->flags |= DRV_FLAG_RUNNING;
		for (i = 0; i < ISDN_MAX_CHANNELS; i++)
			if (dev->drvmap[i] == di)
				isdn_all_eaz(di, dev->chanmap[i]);
		set_global_features();
		break;
	case ISDN_STAT_STOP:
		dev->drv[di]->flags &= ~DRV_FLAG_RUNNING;
		break;
	case ISDN_STAT_ICALL:
		if (i < 0)
			return -1;
#ifdef ISDN_DEBUG_STATCALLB
		printk(KERN_DEBUG "ICALL (net): %d %ld %s\n", di, c->arg, c->parm.num);
#endif
		if (dev->global_flags & ISDN_GLOBAL_STOPPED) {
			cmd.driver = di;
			cmd.arg = c->arg;
			cmd.command = ISDN_CMD_HANGUP;
			isdn_command(&cmd);
			return 0;
		}
		
		r = ((c->command == ISDN_STAT_ICALLW) ? 0 : isdn_net_find_icall(di, c->arg, i, &c->parm.setup));
		switch (r) {
		case 0:
			if (c->command == ISDN_STAT_ICALL)
				if ((retval = isdn_tty_find_icall(di, c->arg, &c->parm.setup))) return (retval);
#ifdef CONFIG_ISDN_DIVERSION
			if (divert_if)
				if ((retval = divert_if->stat_callback(c)))
					return (retval); 
#endif 
			if ((!retval) && (dev->drv[di]->flags & DRV_FLAG_REJBUS)) {
				
				cmd.driver = di;
				cmd.arg = c->arg;
				cmd.command = ISDN_CMD_HANGUP;
				isdn_command(&cmd);
				retval = 2;
			}
			break;
		case 1:
			
			isdn_net_dial();
			cmd.driver = di;
			cmd.arg = c->arg;
			cmd.command = ISDN_CMD_ACCEPTD;
			for (p = dev->netdev; p; p = p->next)
				if (p->local->isdn_channel == cmd.arg)
				{
					strcpy(cmd.parm.setup.eazmsn, p->local->msn);
					isdn_command(&cmd);
					retval = 1;
					break;
				}
			break;

		case 2:	
		case 3:	
			retval = 2;
			printk(KERN_INFO "isdn: Rejecting Call\n");
			cmd.driver = di;
			cmd.arg = c->arg;
			cmd.command = ISDN_CMD_HANGUP;
			isdn_command(&cmd);
			if (r == 3)
				break;
			
		case 4:
			
			isdn_net_dial();
			break;
		case 5:
			
			retval = 3;
			break;
		}
#ifdef ISDN_DEBUG_STATCALLB
		printk(KERN_DEBUG "ICALL: ret=%d\n", retval);
#endif
		return retval;
		break;
	case ISDN_STAT_CINF:
		if (i < 0)
			return -1;
#ifdef ISDN_DEBUG_STATCALLB
		printk(KERN_DEBUG "CINF: %ld %s\n", c->arg, c->parm.num);
#endif
		if (dev->global_flags & ISDN_GLOBAL_STOPPED)
			return 0;
		if (strcmp(c->parm.num, "0"))
			isdn_net_stat_callback(i, c);
		isdn_tty_stat_callback(i, c);
		break;
	case ISDN_STAT_CAUSE:
#ifdef ISDN_DEBUG_STATCALLB
		printk(KERN_DEBUG "CAUSE: %ld %s\n", c->arg, c->parm.num);
#endif
		printk(KERN_INFO "isdn: %s,ch%ld cause: %s\n",
		       dev->drvid[di], c->arg, c->parm.num);
		isdn_tty_stat_callback(i, c);
#ifdef CONFIG_ISDN_DIVERSION
		if (divert_if)
			divert_if->stat_callback(c);
#endif 
		break;
	case ISDN_STAT_DISPLAY:
#ifdef ISDN_DEBUG_STATCALLB
		printk(KERN_DEBUG "DISPLAY: %ld %s\n", c->arg, c->parm.display);
#endif
		isdn_tty_stat_callback(i, c);
#ifdef CONFIG_ISDN_DIVERSION
		if (divert_if)
			divert_if->stat_callback(c);
#endif 
		break;
	case ISDN_STAT_DCONN:
		if (i < 0)
			return -1;
#ifdef ISDN_DEBUG_STATCALLB
		printk(KERN_DEBUG "DCONN: %ld\n", c->arg);
#endif
		if (dev->global_flags & ISDN_GLOBAL_STOPPED)
			return 0;
		
		if (isdn_net_stat_callback(i, c))
			break;
		isdn_v110_stat_callback(i, c);
		
		if (isdn_tty_stat_callback(i, c)) {
			cmd.driver = di;
			cmd.arg = c->arg;
			cmd.command = ISDN_CMD_ACCEPTB;
			isdn_command(&cmd);
			break;
		}
		break;
	case ISDN_STAT_DHUP:
		if (i < 0)
			return -1;
#ifdef ISDN_DEBUG_STATCALLB
		printk(KERN_DEBUG "DHUP: %ld\n", c->arg);
#endif
		if (dev->global_flags & ISDN_GLOBAL_STOPPED)
			return 0;
		dev->drv[di]->online &= ~(1 << (c->arg));
		isdn_info_update();
		
		if (isdn_net_stat_callback(i, c))
			break;
		isdn_v110_stat_callback(i, c);
		if (isdn_tty_stat_callback(i, c))
			break;
#ifdef CONFIG_ISDN_DIVERSION
		if (divert_if)
			divert_if->stat_callback(c);
#endif 
		break;
		break;
	case ISDN_STAT_BCONN:
		if (i < 0)
			return -1;
#ifdef ISDN_DEBUG_STATCALLB
		printk(KERN_DEBUG "BCONN: %ld\n", c->arg);
#endif
		
		if (dev->global_flags & ISDN_GLOBAL_STOPPED)
			return 0;
		dev->drv[di]->online |= (1 << (c->arg));
		isdn_info_update();
		if (isdn_net_stat_callback(i, c))
			break;
		isdn_v110_stat_callback(i, c);
		if (isdn_tty_stat_callback(i, c))
			break;
		break;
	case ISDN_STAT_BHUP:
		if (i < 0)
			return -1;
#ifdef ISDN_DEBUG_STATCALLB
		printk(KERN_DEBUG "BHUP: %ld\n", c->arg);
#endif
		if (dev->global_flags & ISDN_GLOBAL_STOPPED)
			return 0;
		dev->drv[di]->online &= ~(1 << (c->arg));
		isdn_info_update();
#ifdef CONFIG_ISDN_X25
		
		if (isdn_net_stat_callback(i, c))
			break;
#endif
		isdn_v110_stat_callback(i, c);
		if (isdn_tty_stat_callback(i, c))
			break;
		break;
	case ISDN_STAT_NODCH:
		if (i < 0)
			return -1;
#ifdef ISDN_DEBUG_STATCALLB
		printk(KERN_DEBUG "NODCH: %ld\n", c->arg);
#endif
		if (dev->global_flags & ISDN_GLOBAL_STOPPED)
			return 0;
		if (isdn_net_stat_callback(i, c))
			break;
		if (isdn_tty_stat_callback(i, c))
			break;
		break;
	case ISDN_STAT_ADDCH:
		spin_lock_irqsave(&dev->lock, flags);
		if (isdn_add_channels(dev->drv[di], di, c->arg, 1)) {
			spin_unlock_irqrestore(&dev->lock, flags);
			return -1;
		}
		spin_unlock_irqrestore(&dev->lock, flags);
		isdn_info_update();
		break;
	case ISDN_STAT_DISCH:
		spin_lock_irqsave(&dev->lock, flags);
		for (i = 0; i < ISDN_MAX_CHANNELS; i++)
			if ((dev->drvmap[i] == di) &&
			    (dev->chanmap[i] == c->arg)) {
				if (c->parm.num[0])
					dev->usage[i] &= ~ISDN_USAGE_DISABLED;
				else
					if (USG_NONE(dev->usage[i])) {
						dev->usage[i] |= ISDN_USAGE_DISABLED;
					}
					else
						retval = -1;
				break;
			}
		spin_unlock_irqrestore(&dev->lock, flags);
		isdn_info_update();
		break;
	case ISDN_STAT_UNLOAD:
		while (dev->drv[di]->locks > 0) {
			isdn_unlock_driver(dev->drv[di]);
		}
		spin_lock_irqsave(&dev->lock, flags);
		isdn_tty_stat_callback(i, c);
		for (i = 0; i < ISDN_MAX_CHANNELS; i++)
			if (dev->drvmap[i] == di) {
				dev->drvmap[i] = -1;
				dev->chanmap[i] = -1;
				dev->usage[i] &= ~ISDN_USAGE_DISABLED;
			}
		dev->drivers--;
		dev->channels -= dev->drv[di]->channels;
		kfree(dev->drv[di]->rcverr);
		kfree(dev->drv[di]->rcvcount);
		for (i = 0; i < dev->drv[di]->channels; i++)
			skb_queue_purge(&dev->drv[di]->rpqueue[i]);
		kfree(dev->drv[di]->rpqueue);
		kfree(dev->drv[di]->rcv_waitq);
		kfree(dev->drv[di]);
		dev->drv[di] = NULL;
		dev->drvid[di][0] = '\0';
		isdn_info_update();
		set_global_features();
		spin_unlock_irqrestore(&dev->lock, flags);
		return 0;
	case ISDN_STAT_L1ERR:
		break;
	case CAPI_PUT_MESSAGE:
		return (isdn_capi_rec_hl_msg(&c->parm.cmsg));
#ifdef CONFIG_ISDN_TTY_FAX
	case ISDN_STAT_FAXIND:
		isdn_tty_stat_callback(i, c);
		break;
#endif
#ifdef CONFIG_ISDN_AUDIO
	case ISDN_STAT_AUDIO:
		isdn_tty_stat_callback(i, c);
		break;
#endif
#ifdef CONFIG_ISDN_DIVERSION
	case ISDN_STAT_PROT:
	case ISDN_STAT_REDIR:
		if (divert_if)
			return (divert_if->stat_callback(c));
#endif 
	default:
		return -1;
	}
	return 0;
}

int
isdn_getnum(char **p)
{
	int v = -1;

	while (*p[0] >= '0' && *p[0] <= '9')
		v = ((v < 0) ? 0 : (v * 10)) + (int) ((*p[0]++) - '0');
	return v;
}

#define DLE 0x10

int
isdn_readbchan(int di, int channel, u_char *buf, u_char *fp, int len, wait_queue_head_t *sleep)
{
	int count;
	int count_pull;
	int count_put;
	int dflag;
	struct sk_buff *skb;
	u_char *cp;

	if (!dev->drv[di])
		return 0;
	if (skb_queue_empty(&dev->drv[di]->rpqueue[channel])) {
		if (sleep)
			interruptible_sleep_on(sleep);
		else
			return 0;
	}
	if (len > dev->drv[di]->rcvcount[channel])
		len = dev->drv[di]->rcvcount[channel];
	cp = buf;
	count = 0;
	while (len) {
		if (!(skb = skb_peek(&dev->drv[di]->rpqueue[channel])))
			break;
#ifdef CONFIG_ISDN_AUDIO
		if (ISDN_AUDIO_SKB_LOCK(skb))
			break;
		ISDN_AUDIO_SKB_LOCK(skb) = 1;
		if ((ISDN_AUDIO_SKB_DLECOUNT(skb)) || (dev->drv[di]->DLEflag & (1 << channel))) {
			char *p = skb->data;
			unsigned long DLEmask = (1 << channel);

			dflag = 0;
			count_pull = count_put = 0;
			while ((count_pull < skb->len) && (len > 0)) {
				len--;
				if (dev->drv[di]->DLEflag & DLEmask) {
					*cp++ = DLE;
					dev->drv[di]->DLEflag &= ~DLEmask;
				} else {
					*cp++ = *p;
					if (*p == DLE) {
						dev->drv[di]->DLEflag |= DLEmask;
						(ISDN_AUDIO_SKB_DLECOUNT(skb))--;
					}
					p++;
					count_pull++;
				}
				count_put++;
			}
			if (count_pull >= skb->len)
				dflag = 1;
		} else {
#endif
			
			dflag = 1;
			if ((count_pull = skb->len) > len) {
				count_pull = len;
				dflag = 0;
			}
			count_put = count_pull;
			skb_copy_from_linear_data(skb, cp, count_put);
			cp += count_put;
			len -= count_put;
#ifdef CONFIG_ISDN_AUDIO
		}
#endif
		count += count_put;
		if (fp) {
			memset(fp, 0, count_put);
			fp += count_put;
		}
		if (dflag) {
			if (fp)
				*(fp - 1) = 0xff;
#ifdef CONFIG_ISDN_AUDIO
			ISDN_AUDIO_SKB_LOCK(skb) = 0;
#endif
			skb = skb_dequeue(&dev->drv[di]->rpqueue[channel]);
			dev_kfree_skb(skb);
		} else {
			skb_pull(skb, count_pull);
#ifdef CONFIG_ISDN_AUDIO
			ISDN_AUDIO_SKB_LOCK(skb) = 0;
#endif
		}
		dev->drv[di]->rcvcount[channel] -= count_put;
	}
	return count;
}

int
isdn_readbchan_tty(int di, int channel, struct tty_struct *tty, int cisco_hack)
{
	int count;
	int count_pull;
	int count_put;
	int dflag;
	struct sk_buff *skb;
	char last = 0;
	int len;

	if (!dev->drv[di])
		return 0;
	if (skb_queue_empty(&dev->drv[di]->rpqueue[channel]))
		return 0;

	len = tty_buffer_request_room(tty, dev->drv[di]->rcvcount[channel]);
	if (len == 0)
		return len;

	count = 0;
	while (len) {
		if (!(skb = skb_peek(&dev->drv[di]->rpqueue[channel])))
			break;
#ifdef CONFIG_ISDN_AUDIO
		if (ISDN_AUDIO_SKB_LOCK(skb))
			break;
		ISDN_AUDIO_SKB_LOCK(skb) = 1;
		if ((ISDN_AUDIO_SKB_DLECOUNT(skb)) || (dev->drv[di]->DLEflag & (1 << channel))) {
			char *p = skb->data;
			unsigned long DLEmask = (1 << channel);

			dflag = 0;
			count_pull = count_put = 0;
			while ((count_pull < skb->len) && (len > 0)) {
				
				if (count_put)
					tty_insert_flip_char(tty, last, TTY_NORMAL);
				len--;
				if (dev->drv[di]->DLEflag & DLEmask) {
					last = DLE;
					dev->drv[di]->DLEflag &= ~DLEmask;
				} else {
					last = *p;
					if (last == DLE) {
						dev->drv[di]->DLEflag |= DLEmask;
						(ISDN_AUDIO_SKB_DLECOUNT(skb))--;
					}
					p++;
					count_pull++;
				}
				count_put++;
			}
			if (count_pull >= skb->len)
				dflag = 1;
		} else {
#endif
			
			dflag = 1;
			if ((count_pull = skb->len) > len) {
				count_pull = len;
				dflag = 0;
			}
			count_put = count_pull;
			if (count_put > 1)
				tty_insert_flip_string(tty, skb->data, count_put - 1);
			last = skb->data[count_put - 1];
			len -= count_put;
#ifdef CONFIG_ISDN_AUDIO
		}
#endif
		count += count_put;
		if (dflag) {
			if (cisco_hack)
				tty_insert_flip_char(tty, last, 0xFF);
			else
				tty_insert_flip_char(tty, last, TTY_NORMAL);
#ifdef CONFIG_ISDN_AUDIO
			ISDN_AUDIO_SKB_LOCK(skb) = 0;
#endif
			skb = skb_dequeue(&dev->drv[di]->rpqueue[channel]);
			dev_kfree_skb(skb);
		} else {
			tty_insert_flip_char(tty, last, TTY_NORMAL);
			skb_pull(skb, count_pull);
#ifdef CONFIG_ISDN_AUDIO
			ISDN_AUDIO_SKB_LOCK(skb) = 0;
#endif
		}
		dev->drv[di]->rcvcount[channel] -= count_put;
	}
	return count;
}


static inline int
isdn_minor2drv(int minor)
{
	return (dev->drvmap[minor]);
}

static inline int
isdn_minor2chan(int minor)
{
	return (dev->chanmap[minor]);
}

static char *
isdn_statstr(void)
{
	static char istatbuf[2048];
	char *p;
	int i;

	sprintf(istatbuf, "idmap:\t");
	p = istatbuf + strlen(istatbuf);
	for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
		sprintf(p, "%s ", (dev->drvmap[i] < 0) ? "-" : dev->drvid[dev->drvmap[i]]);
		p = istatbuf + strlen(istatbuf);
	}
	sprintf(p, "\nchmap:\t");
	p = istatbuf + strlen(istatbuf);
	for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
		sprintf(p, "%d ", dev->chanmap[i]);
		p = istatbuf + strlen(istatbuf);
	}
	sprintf(p, "\ndrmap:\t");
	p = istatbuf + strlen(istatbuf);
	for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
		sprintf(p, "%d ", dev->drvmap[i]);
		p = istatbuf + strlen(istatbuf);
	}
	sprintf(p, "\nusage:\t");
	p = istatbuf + strlen(istatbuf);
	for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
		sprintf(p, "%d ", dev->usage[i]);
		p = istatbuf + strlen(istatbuf);
	}
	sprintf(p, "\nflags:\t");
	p = istatbuf + strlen(istatbuf);
	for (i = 0; i < ISDN_MAX_DRIVERS; i++) {
		if (dev->drv[i]) {
			sprintf(p, "%ld ", dev->drv[i]->online);
			p = istatbuf + strlen(istatbuf);
		} else {
			sprintf(p, "? ");
			p = istatbuf + strlen(istatbuf);
		}
	}
	sprintf(p, "\nphone:\t");
	p = istatbuf + strlen(istatbuf);
	for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
		sprintf(p, "%s ", dev->num[i]);
		p = istatbuf + strlen(istatbuf);
	}
	sprintf(p, "\n");
	return istatbuf;
}


void
isdn_info_update(void)
{
	infostruct *p = dev->infochain;

	while (p) {
		*(p->private) = 1;
		p = (infostruct *) p->next;
	}
	wake_up_interruptible(&(dev->info_waitq));
}

static ssize_t
isdn_read(struct file *file, char __user *buf, size_t count, loff_t *off)
{
	uint minor = iminor(file->f_path.dentry->d_inode);
	int len = 0;
	int drvidx;
	int chidx;
	int retval;
	char *p;

	mutex_lock(&isdn_mutex);
	if (minor == ISDN_MINOR_STATUS) {
		if (!file->private_data) {
			if (file->f_flags & O_NONBLOCK) {
				retval = -EAGAIN;
				goto out;
			}
			interruptible_sleep_on(&(dev->info_waitq));
		}
		p = isdn_statstr();
		file->private_data = NULL;
		if ((len = strlen(p)) <= count) {
			if (copy_to_user(buf, p, len)) {
				retval = -EFAULT;
				goto out;
			}
			*off += len;
			retval = len;
			goto out;
		}
		retval = 0;
		goto out;
	}
	if (!dev->drivers) {
		retval = -ENODEV;
		goto out;
	}
	if (minor <= ISDN_MINOR_BMAX) {
		printk(KERN_WARNING "isdn_read minor %d obsolete!\n", minor);
		drvidx = isdn_minor2drv(minor);
		if (drvidx < 0) {
			retval = -ENODEV;
			goto out;
		}
		if (!(dev->drv[drvidx]->flags & DRV_FLAG_RUNNING)) {
			retval = -ENODEV;
			goto out;
		}
		chidx = isdn_minor2chan(minor);
		if (!(p = kmalloc(count, GFP_KERNEL))) {
			retval = -ENOMEM;
			goto out;
		}
		len = isdn_readbchan(drvidx, chidx, p, NULL, count,
				     &dev->drv[drvidx]->rcv_waitq[chidx]);
		*off += len;
		if (copy_to_user(buf, p, len))
			len = -EFAULT;
		kfree(p);
		retval = len;
		goto out;
	}
	if (minor <= ISDN_MINOR_CTRLMAX) {
		drvidx = isdn_minor2drv(minor - ISDN_MINOR_CTRL);
		if (drvidx < 0) {
			retval = -ENODEV;
			goto out;
		}
		if (!dev->drv[drvidx]->stavail) {
			if (file->f_flags & O_NONBLOCK) {
				retval = -EAGAIN;
				goto out;
			}
			interruptible_sleep_on(&(dev->drv[drvidx]->st_waitq));
		}
		if (dev->drv[drvidx]->interface->readstat) {
			if (count > dev->drv[drvidx]->stavail)
				count = dev->drv[drvidx]->stavail;
			len = dev->drv[drvidx]->interface->readstat(buf, count,
								    drvidx, isdn_minor2chan(minor - ISDN_MINOR_CTRL));
			if (len < 0) {
				retval = len;
				goto out;
			}
		} else {
			len = 0;
		}
		if (len)
			dev->drv[drvidx]->stavail -= len;
		else
			dev->drv[drvidx]->stavail = 0;
		*off += len;
		retval = len;
		goto out;
	}
#ifdef CONFIG_ISDN_PPP
	if (minor <= ISDN_MINOR_PPPMAX) {
		retval = isdn_ppp_read(minor - ISDN_MINOR_PPP, file, buf, count);
		goto out;
	}
#endif
	retval = -ENODEV;
out:
	mutex_unlock(&isdn_mutex);
	return retval;
}

static ssize_t
isdn_write(struct file *file, const char __user *buf, size_t count, loff_t *off)
{
	uint minor = iminor(file->f_path.dentry->d_inode);
	int drvidx;
	int chidx;
	int retval;

	if (minor == ISDN_MINOR_STATUS)
		return -EPERM;
	if (!dev->drivers)
		return -ENODEV;

	mutex_lock(&isdn_mutex);
	if (minor <= ISDN_MINOR_BMAX) {
		printk(KERN_WARNING "isdn_write minor %d obsolete!\n", minor);
		drvidx = isdn_minor2drv(minor);
		if (drvidx < 0) {
			retval = -ENODEV;
			goto out;
		}
		if (!(dev->drv[drvidx]->flags & DRV_FLAG_RUNNING)) {
			retval = -ENODEV;
			goto out;
		}
		chidx = isdn_minor2chan(minor);
		while ((retval = isdn_writebuf_stub(drvidx, chidx, buf, count)) == 0)
			interruptible_sleep_on(&dev->drv[drvidx]->snd_waitq[chidx]);
		goto out;
	}
	if (minor <= ISDN_MINOR_CTRLMAX) {
		drvidx = isdn_minor2drv(minor - ISDN_MINOR_CTRL);
		if (drvidx < 0) {
			retval = -ENODEV;
			goto out;
		}
		if (dev->drv[drvidx]->interface->writecmd)
			retval = dev->drv[drvidx]->interface->
				writecmd(buf, count, drvidx,
					 isdn_minor2chan(minor - ISDN_MINOR_CTRL));
		else
			retval = count;
		goto out;
	}
#ifdef CONFIG_ISDN_PPP
	if (minor <= ISDN_MINOR_PPPMAX) {
		retval = isdn_ppp_write(minor - ISDN_MINOR_PPP, file, buf, count);
		goto out;
	}
#endif
	retval = -ENODEV;
out:
	mutex_unlock(&isdn_mutex);
	return retval;
}

static unsigned int
isdn_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	unsigned int minor = iminor(file->f_path.dentry->d_inode);
	int drvidx = isdn_minor2drv(minor - ISDN_MINOR_CTRL);

	mutex_lock(&isdn_mutex);
	if (minor == ISDN_MINOR_STATUS) {
		poll_wait(file, &(dev->info_waitq), wait);
		
		if (file->private_data) {
			mask |= POLLIN | POLLRDNORM;
		}
		goto out;
	}
	if (minor >= ISDN_MINOR_CTRL && minor <= ISDN_MINOR_CTRLMAX) {
		if (drvidx < 0) {
			
			mask = POLLHUP;
			goto out;
		}
		poll_wait(file, &(dev->drv[drvidx]->st_waitq), wait);
		mask = POLLOUT | POLLWRNORM;
		if (dev->drv[drvidx]->stavail) {
			mask |= POLLIN | POLLRDNORM;
		}
		goto out;
	}
#ifdef CONFIG_ISDN_PPP
	if (minor <= ISDN_MINOR_PPPMAX) {
		mask = isdn_ppp_poll(file, wait);
		goto out;
	}
#endif
	mask = POLLERR;
out:
	mutex_unlock(&isdn_mutex);
	return mask;
}


static int
isdn_ioctl(struct file *file, uint cmd, ulong arg)
{
	uint minor = iminor(file->f_path.dentry->d_inode);
	isdn_ctrl c;
	int drvidx;
	int ret;
	int i;
	char __user *p;
	char *s;
	union iocpar {
		char name[10];
		char bname[22];
		isdn_ioctl_struct iocts;
		isdn_net_ioctl_phone phone;
		isdn_net_ioctl_cfg cfg;
	} iocpar;
	void __user *argp = (void __user *)arg;

#define name  iocpar.name
#define bname iocpar.bname
#define iocts iocpar.iocts
#define phone iocpar.phone
#define cfg   iocpar.cfg

	if (minor == ISDN_MINOR_STATUS) {
		switch (cmd) {
		case IIOCGETDVR:
			return (TTY_DV +
				(NET_DV << 8) +
				(INF_DV << 16));
		case IIOCGETCPS:
			if (arg) {
				ulong __user *p = argp;
				int i;
				if (!access_ok(VERIFY_WRITE, p,
					       sizeof(ulong) * ISDN_MAX_CHANNELS * 2))
					return -EFAULT;
				for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
					put_user(dev->ibytes[i], p++);
					put_user(dev->obytes[i], p++);
				}
				return 0;
			} else
				return -EINVAL;
			break;
#ifdef CONFIG_NETDEVICES
		case IIOCNETGPN:
			if (arg) {
				if (copy_from_user(&phone, argp, sizeof(phone)))
					return -EFAULT;
				return isdn_net_getpeer(&phone, argp);
			} else
				return -EINVAL;
#endif
		default:
			return -EINVAL;
		}
	}
	if (!dev->drivers)
		return -ENODEV;
	if (minor <= ISDN_MINOR_BMAX) {
		drvidx = isdn_minor2drv(minor);
		if (drvidx < 0)
			return -ENODEV;
		if (!(dev->drv[drvidx]->flags & DRV_FLAG_RUNNING))
			return -ENODEV;
		return 0;
	}
	if (minor <= ISDN_MINOR_CTRLMAX) {
		switch (cmd) {
		case IIOCNETDWRSET:
			printk(KERN_INFO "INFO: ISDN_DW_ABC_EXTENSION not enabled\n");
			return (-EINVAL);
		case IIOCNETLCR:
			printk(KERN_INFO "INFO: ISDN_ABC_LCR_SUPPORT not enabled\n");
			return -ENODEV;
#ifdef CONFIG_NETDEVICES
		case IIOCNETAIF:
			
			if (arg) {
				if (copy_from_user(name, argp, sizeof(name)))
					return -EFAULT;
				s = name;
			} else {
				s = NULL;
			}
			ret = mutex_lock_interruptible(&dev->mtx);
			if (ret) return ret;
			if ((s = isdn_net_new(s, NULL))) {
				if (copy_to_user(argp, s, strlen(s) + 1)) {
					ret = -EFAULT;
				} else {
					ret = 0;
				}
			} else
				ret = -ENODEV;
			mutex_unlock(&dev->mtx);
			return ret;
		case IIOCNETASL:
			
			if (arg) {
				if (copy_from_user(bname, argp, sizeof(bname) - 1))
					return -EFAULT;
			} else
				return -EINVAL;
			ret = mutex_lock_interruptible(&dev->mtx);
			if (ret) return ret;
			if ((s = isdn_net_newslave(bname))) {
				if (copy_to_user(argp, s, strlen(s) + 1)) {
					ret = -EFAULT;
				} else {
					ret = 0;
				}
			} else
				ret = -ENODEV;
			mutex_unlock(&dev->mtx);
			return ret;
		case IIOCNETDIF:
			
			if (arg) {
				if (copy_from_user(name, argp, sizeof(name)))
					return -EFAULT;
				ret = mutex_lock_interruptible(&dev->mtx);
				if (ret) return ret;
				ret = isdn_net_rm(name);
				mutex_unlock(&dev->mtx);
				return ret;
			} else
				return -EINVAL;
		case IIOCNETSCF:
			
			if (arg) {
				if (copy_from_user(&cfg, argp, sizeof(cfg)))
					return -EFAULT;
				return isdn_net_setcfg(&cfg);
			} else
				return -EINVAL;
		case IIOCNETGCF:
			
			if (arg) {
				if (copy_from_user(&cfg, argp, sizeof(cfg)))
					return -EFAULT;
				if (!(ret = isdn_net_getcfg(&cfg))) {
					if (copy_to_user(argp, &cfg, sizeof(cfg)))
						return -EFAULT;
				}
				return ret;
			} else
				return -EINVAL;
		case IIOCNETANM:
			
			if (arg) {
				if (copy_from_user(&phone, argp, sizeof(phone)))
					return -EFAULT;
				ret = mutex_lock_interruptible(&dev->mtx);
				if (ret) return ret;
				ret = isdn_net_addphone(&phone);
				mutex_unlock(&dev->mtx);
				return ret;
			} else
				return -EINVAL;
		case IIOCNETGNM:
			
			if (arg) {
				if (copy_from_user(&phone, argp, sizeof(phone)))
					return -EFAULT;
				ret = mutex_lock_interruptible(&dev->mtx);
				if (ret) return ret;
				ret = isdn_net_getphones(&phone, argp);
				mutex_unlock(&dev->mtx);
				return ret;
			} else
				return -EINVAL;
		case IIOCNETDNM:
			
			if (arg) {
				if (copy_from_user(&phone, argp, sizeof(phone)))
					return -EFAULT;
				ret = mutex_lock_interruptible(&dev->mtx);
				if (ret) return ret;
				ret = isdn_net_delphone(&phone);
				mutex_unlock(&dev->mtx);
				return ret;
			} else
				return -EINVAL;
		case IIOCNETDIL:
			
			if (arg) {
				if (copy_from_user(name, argp, sizeof(name)))
					return -EFAULT;
				return isdn_net_force_dial(name);
			} else
				return -EINVAL;
#ifdef CONFIG_ISDN_PPP
		case IIOCNETALN:
			if (!arg)
				return -EINVAL;
			if (copy_from_user(name, argp, sizeof(name)))
				return -EFAULT;
			return isdn_ppp_dial_slave(name);
		case IIOCNETDLN:
			if (!arg)
				return -EINVAL;
			if (copy_from_user(name, argp, sizeof(name)))
				return -EFAULT;
			return isdn_ppp_hangup_slave(name);
#endif
		case IIOCNETHUP:
			
			if (!arg)
				return -EINVAL;
			if (copy_from_user(name, argp, sizeof(name)))
				return -EFAULT;
			return isdn_net_force_hangup(name);
			break;
#endif                          
		case IIOCSETVER:
			dev->net_verbose = arg;
			printk(KERN_INFO "isdn: Verbose-Level is %d\n", dev->net_verbose);
			return 0;
		case IIOCSETGST:
			if (arg)
				dev->global_flags |= ISDN_GLOBAL_STOPPED;
			else
				dev->global_flags &= ~ISDN_GLOBAL_STOPPED;
			printk(KERN_INFO "isdn: Global Mode %s\n",
			       (dev->global_flags & ISDN_GLOBAL_STOPPED) ? "stopped" : "running");
			return 0;
		case IIOCSETBRJ:
			drvidx = -1;
			if (arg) {
				int i;
				char *p;
				if (copy_from_user(&iocts, argp,
						   sizeof(isdn_ioctl_struct)))
					return -EFAULT;
				iocts.drvid[sizeof(iocts.drvid) - 1] = 0;
				if (strlen(iocts.drvid)) {
					if ((p = strchr(iocts.drvid, ',')))
						*p = 0;
					drvidx = -1;
					for (i = 0; i < ISDN_MAX_DRIVERS; i++)
						if (!(strcmp(dev->drvid[i], iocts.drvid))) {
							drvidx = i;
							break;
						}
				}
			}
			if (drvidx == -1)
				return -ENODEV;
			if (iocts.arg)
				dev->drv[drvidx]->flags |= DRV_FLAG_REJBUS;
			else
				dev->drv[drvidx]->flags &= ~DRV_FLAG_REJBUS;
			return 0;
		case IIOCSIGPRF:
			dev->profd = current;
			return 0;
			break;
		case IIOCGETPRF:
			
			if (arg) {
				char __user *p = argp;
				int i;

				if (!access_ok(VERIFY_WRITE, argp,
					       (ISDN_MODEM_NUMREG + ISDN_MSNLEN + ISDN_LMSNLEN)
					       * ISDN_MAX_CHANNELS))
					return -EFAULT;

				for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
					if (copy_to_user(p, dev->mdm.info[i].emu.profile,
							 ISDN_MODEM_NUMREG))
						return -EFAULT;
					p += ISDN_MODEM_NUMREG;
					if (copy_to_user(p, dev->mdm.info[i].emu.pmsn, ISDN_MSNLEN))
						return -EFAULT;
					p += ISDN_MSNLEN;
					if (copy_to_user(p, dev->mdm.info[i].emu.plmsn, ISDN_LMSNLEN))
						return -EFAULT;
					p += ISDN_LMSNLEN;
				}
				return (ISDN_MODEM_NUMREG + ISDN_MSNLEN + ISDN_LMSNLEN) * ISDN_MAX_CHANNELS;
			} else
				return -EINVAL;
			break;
		case IIOCSETPRF:
			
			if (arg) {
				char __user *p = argp;
				int i;

				if (!access_ok(VERIFY_READ, argp,
					       (ISDN_MODEM_NUMREG + ISDN_MSNLEN + ISDN_LMSNLEN)
					       * ISDN_MAX_CHANNELS))
					return -EFAULT;

				for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
					if (copy_from_user(dev->mdm.info[i].emu.profile, p,
							   ISDN_MODEM_NUMREG))
						return -EFAULT;
					p += ISDN_MODEM_NUMREG;
					if (copy_from_user(dev->mdm.info[i].emu.plmsn, p, ISDN_LMSNLEN))
						return -EFAULT;
					p += ISDN_LMSNLEN;
					if (copy_from_user(dev->mdm.info[i].emu.pmsn, p, ISDN_MSNLEN))
						return -EFAULT;
					p += ISDN_MSNLEN;
				}
				return 0;
			} else
				return -EINVAL;
			break;
		case IIOCSETMAP:
		case IIOCGETMAP:
			
			if (arg) {

				if (copy_from_user(&iocts, argp,
						   sizeof(isdn_ioctl_struct)))
					return -EFAULT;
				iocts.drvid[sizeof(iocts.drvid) - 1] = 0;
				if (strlen(iocts.drvid)) {
					drvidx = -1;
					for (i = 0; i < ISDN_MAX_DRIVERS; i++)
						if (!(strcmp(dev->drvid[i], iocts.drvid))) {
							drvidx = i;
							break;
						}
				} else
					drvidx = 0;
				if (drvidx == -1)
					return -ENODEV;
				if (cmd == IIOCSETMAP) {
					int loop = 1;

					p = (char __user *) iocts.arg;
					i = 0;
					while (loop) {
						int j = 0;

						while (1) {
							if (!access_ok(VERIFY_READ, p, 1))
								return -EFAULT;
							get_user(bname[j], p++);
							switch (bname[j]) {
							case '\0':
								loop = 0;
								
							case ',':
								bname[j] = '\0';
								strcpy(dev->drv[drvidx]->msn2eaz[i], bname);
								j = ISDN_MSNLEN;
								break;
							default:
								j++;
							}
							if (j >= ISDN_MSNLEN)
								break;
						}
						if (++i > 9)
							break;
					}
				} else {
					p = (char __user *) iocts.arg;
					for (i = 0; i < 10; i++) {
						snprintf(bname, sizeof(bname), "%s%s",
							 strlen(dev->drv[drvidx]->msn2eaz[i]) ?
							 dev->drv[drvidx]->msn2eaz[i] : "_",
							 (i < 9) ? "," : "\0");
						if (copy_to_user(p, bname, strlen(bname) + 1))
							return -EFAULT;
						p += strlen(bname);
					}
				}
				return 0;
			} else
				return -EINVAL;
		case IIOCDBGVAR:
			if (arg) {
				if (copy_to_user(argp, &dev, sizeof(ulong)))
					return -EFAULT;
				return 0;
			} else
				return -EINVAL;
			break;
		default:
			if ((cmd & IIOCDRVCTL) == IIOCDRVCTL)
				cmd = ((cmd >> _IOC_NRSHIFT) & _IOC_NRMASK) & ISDN_DRVIOCTL_MASK;
			else
				return -EINVAL;
			if (arg) {
				int i;
				char *p;
				if (copy_from_user(&iocts, argp, sizeof(isdn_ioctl_struct)))
					return -EFAULT;
				iocts.drvid[sizeof(iocts.drvid) - 1] = 0;
				if (strlen(iocts.drvid)) {
					if ((p = strchr(iocts.drvid, ',')))
						*p = 0;
					drvidx = -1;
					for (i = 0; i < ISDN_MAX_DRIVERS; i++)
						if (!(strcmp(dev->drvid[i], iocts.drvid))) {
							drvidx = i;
							break;
						}
				} else
					drvidx = 0;
				if (drvidx == -1)
					return -ENODEV;
				if (!access_ok(VERIFY_WRITE, argp,
					       sizeof(isdn_ioctl_struct)))
					return -EFAULT;
				c.driver = drvidx;
				c.command = ISDN_CMD_IOCTL;
				c.arg = cmd;
				memcpy(c.parm.num, &iocts.arg, sizeof(ulong));
				ret = isdn_command(&c);
				memcpy(&iocts.arg, c.parm.num, sizeof(ulong));
				if (copy_to_user(argp, &iocts, sizeof(isdn_ioctl_struct)))
					return -EFAULT;
				return ret;
			} else
				return -EINVAL;
		}
	}
#ifdef CONFIG_ISDN_PPP
	if (minor <= ISDN_MINOR_PPPMAX)
		return (isdn_ppp_ioctl(minor - ISDN_MINOR_PPP, file, cmd, arg));
#endif
	return -ENODEV;

#undef name
#undef bname
#undef iocts
#undef phone
#undef cfg
}

static long
isdn_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	mutex_lock(&isdn_mutex);
	ret = isdn_ioctl(file, cmd, arg);
	mutex_unlock(&isdn_mutex);

	return ret;
}

static int
isdn_open(struct inode *ino, struct file *filep)
{
	uint minor = iminor(ino);
	int drvidx;
	int chidx;
	int retval = -ENODEV;

	mutex_lock(&isdn_mutex);
	if (minor == ISDN_MINOR_STATUS) {
		infostruct *p;

		if ((p = kmalloc(sizeof(infostruct), GFP_KERNEL))) {
			p->next = (char *) dev->infochain;
			p->private = (char *) &(filep->private_data);
			dev->infochain = p;
			
			filep->private_data = (char *) 1;
			retval = 0;
			goto out;
		} else {
			retval = -ENOMEM;
			goto out;
		}
	}
	if (!dev->channels)
		goto out;
	if (minor <= ISDN_MINOR_BMAX) {
		printk(KERN_WARNING "isdn_open minor %d obsolete!\n", minor);
		drvidx = isdn_minor2drv(minor);
		if (drvidx < 0)
			goto out;
		chidx = isdn_minor2chan(minor);
		if (!(dev->drv[drvidx]->flags & DRV_FLAG_RUNNING))
			goto out;
		if (!(dev->drv[drvidx]->online & (1 << chidx)))
			goto out;
		isdn_lock_drivers();
		retval = 0;
		goto out;
	}
	if (minor <= ISDN_MINOR_CTRLMAX) {
		drvidx = isdn_minor2drv(minor - ISDN_MINOR_CTRL);
		if (drvidx < 0)
			goto out;
		isdn_lock_drivers();
		retval = 0;
		goto out;
	}
#ifdef CONFIG_ISDN_PPP
	if (minor <= ISDN_MINOR_PPPMAX) {
		retval = isdn_ppp_open(minor - ISDN_MINOR_PPP, filep);
		if (retval == 0)
			isdn_lock_drivers();
		goto out;
	}
#endif
out:
	nonseekable_open(ino, filep);
	mutex_unlock(&isdn_mutex);
	return retval;
}

static int
isdn_close(struct inode *ino, struct file *filep)
{
	uint minor = iminor(ino);

	mutex_lock(&isdn_mutex);
	if (minor == ISDN_MINOR_STATUS) {
		infostruct *p = dev->infochain;
		infostruct *q = NULL;

		while (p) {
			if (p->private == (char *) &(filep->private_data)) {
				if (q)
					q->next = p->next;
				else
					dev->infochain = (infostruct *) (p->next);
				kfree(p);
				goto out;
			}
			q = p;
			p = (infostruct *) (p->next);
		}
		printk(KERN_WARNING "isdn: No private data while closing isdnctrl\n");
		goto out;
	}
	isdn_unlock_drivers();
	if (minor <= ISDN_MINOR_BMAX)
		goto out;
	if (minor <= ISDN_MINOR_CTRLMAX) {
		if (dev->profd == current)
			dev->profd = NULL;
		goto out;
	}
#ifdef CONFIG_ISDN_PPP
	if (minor <= ISDN_MINOR_PPPMAX)
		isdn_ppp_release(minor - ISDN_MINOR_PPP, filep);
#endif

out:
	mutex_unlock(&isdn_mutex);
	return 0;
}

static const struct file_operations isdn_fops =
{
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= isdn_read,
	.write		= isdn_write,
	.poll		= isdn_poll,
	.unlocked_ioctl	= isdn_unlocked_ioctl,
	.open		= isdn_open,
	.release	= isdn_close,
};

char *
isdn_map_eaz2msn(char *msn, int di)
{
	isdn_driver_t *this = dev->drv[di];
	int i;

	if (strlen(msn) == 1) {
		i = msn[0] - '0';
		if ((i >= 0) && (i <= 9))
			if (strlen(this->msn2eaz[i]))
				return (this->msn2eaz[i]);
	}
	return (msn);
}

#define L2V (~(ISDN_FEATURE_L2_V11096 | ISDN_FEATURE_L2_V11019 | ISDN_FEATURE_L2_V11038))

int
isdn_get_free_channel(int usage, int l2_proto, int l3_proto, int pre_dev
		      , int pre_chan, char *msn)
{
	int i;
	ulong features;
	ulong vfeatures;

	features = ((1 << l2_proto) | (0x10000 << l3_proto));
	vfeatures = (((1 << l2_proto) | (0x10000 << l3_proto)) &
		     ~(ISDN_FEATURE_L2_V11096 | ISDN_FEATURE_L2_V11019 | ISDN_FEATURE_L2_V11038));
	for (i = 0; i < ISDN_MAX_CHANNELS; i++)
		if (USG_NONE(dev->usage[i]) &&
		    (dev->drvmap[i] != -1)) {
			int d = dev->drvmap[i];
			if ((dev->usage[i] & ISDN_USAGE_EXCLUSIVE) &&
			    ((pre_dev != d) || (pre_chan != dev->chanmap[i])))
				continue;
			if (!strcmp(isdn_map_eaz2msn(msn, d), "-"))
				continue;
			if (dev->usage[i] & ISDN_USAGE_DISABLED)
				continue; 
			if (dev->drv[d]->flags & DRV_FLAG_RUNNING) {
				if (((dev->drv[d]->interface->features & features) == features) ||
				    (((dev->drv[d]->interface->features & vfeatures) == vfeatures) &&
				     (dev->drv[d]->interface->features & ISDN_FEATURE_L2_TRANS))) {
					if ((pre_dev < 0) || (pre_chan < 0)) {
						dev->usage[i] &= ISDN_USAGE_EXCLUSIVE;
						dev->usage[i] |= usage;
						isdn_info_update();
						return i;
					} else {
						if ((pre_dev == d) && (pre_chan == dev->chanmap[i])) {
							dev->usage[i] &= ISDN_USAGE_EXCLUSIVE;
							dev->usage[i] |= usage;
							isdn_info_update();
							return i;
						}
					}
				}
			}
		}
	return -1;
}

void
isdn_free_channel(int di, int ch, int usage)
{
	int i;

	if ((di < 0) || (ch < 0)) {
		printk(KERN_WARNING "%s: called with invalid drv(%d) or channel(%d)\n",
		       __func__, di, ch);
		return;
	}
	for (i = 0; i < ISDN_MAX_CHANNELS; i++)
		if (((!usage) || ((dev->usage[i] & ISDN_USAGE_MASK) == usage)) &&
		    (dev->drvmap[i] == di) &&
		    (dev->chanmap[i] == ch)) {
			dev->usage[i] &= (ISDN_USAGE_NONE | ISDN_USAGE_EXCLUSIVE);
			strcpy(dev->num[i], "???");
			dev->ibytes[i] = 0;
			dev->obytes[i] = 0;
			dev->v110emu[i] = 0;
			atomic_set(&(dev->v110use[i]), 0);
			isdn_v110_close(dev->v110[i]);
			dev->v110[i] = NULL;
			isdn_info_update();
			if (dev->drv[di])
				skb_queue_purge(&dev->drv[di]->rpqueue[ch]);
		}
}

void
isdn_unexclusive_channel(int di, int ch)
{
	int i;

	for (i = 0; i < ISDN_MAX_CHANNELS; i++)
		if ((dev->drvmap[i] == di) &&
		    (dev->chanmap[i] == ch)) {
			dev->usage[i] &= ~ISDN_USAGE_EXCLUSIVE;
			isdn_info_update();
			return;
		}
}

static int
isdn_writebuf_stub(int drvidx, int chan, const u_char __user *buf, int len)
{
	int ret;
	int hl = dev->drv[drvidx]->interface->hl_hdrlen;
	struct sk_buff *skb = alloc_skb(hl + len, GFP_ATOMIC);

	if (!skb)
		return -ENOMEM;
	skb_reserve(skb, hl);
	if (copy_from_user(skb_put(skb, len), buf, len)) {
		dev_kfree_skb(skb);
		return -EFAULT;
	}
	ret = dev->drv[drvidx]->interface->writebuf_skb(drvidx, chan, 1, skb);
	if (ret <= 0)
		dev_kfree_skb(skb);
	if (ret > 0)
		dev->obytes[isdn_dc2minor(drvidx, chan)] += ret;
	return ret;
}

int
isdn_writebuf_skb_stub(int drvidx, int chan, int ack, struct sk_buff *skb)
{
	int ret;
	struct sk_buff *nskb = NULL;
	int v110_ret = skb->len;
	int idx = isdn_dc2minor(drvidx, chan);

	if (dev->v110[idx]) {
		atomic_inc(&dev->v110use[idx]);
		nskb = isdn_v110_encode(dev->v110[idx], skb);
		atomic_dec(&dev->v110use[idx]);
		if (!nskb)
			return 0;
		v110_ret = *((int *)nskb->data);
		skb_pull(nskb, sizeof(int));
		if (!nskb->len) {
			dev_kfree_skb(nskb);
			return v110_ret;
		}
		
		ack = 1;
		ret = dev->drv[drvidx]->interface->writebuf_skb(drvidx, chan, ack, nskb);
	} else {
		int hl = dev->drv[drvidx]->interface->hl_hdrlen;

		if (skb_headroom(skb) < hl) {
			struct sk_buff *skb_tmp;

			skb_tmp = skb_realloc_headroom(skb, hl);
			printk(KERN_DEBUG "isdn_writebuf_skb_stub: reallocating headroom%s\n", skb_tmp ? "" : " failed");
			if (!skb_tmp) return -ENOMEM; 
			ret = dev->drv[drvidx]->interface->writebuf_skb(drvidx, chan, ack, skb_tmp);
			if (ret > 0) {
				dev_kfree_skb(skb);
			} else {
				dev_kfree_skb(skb_tmp);
			}
		} else {
			ret = dev->drv[drvidx]->interface->writebuf_skb(drvidx, chan, ack, skb);
		}
	}
	if (ret > 0) {
		dev->obytes[idx] += ret;
		if (dev->v110[idx]) {
			atomic_inc(&dev->v110use[idx]);
			dev->v110[idx]->skbuser++;
			atomic_dec(&dev->v110use[idx]);
			
			ret = v110_ret;
			if (ret == skb->len)
				dev_kfree_skb(skb);
		}
	} else
		if (dev->v110[idx])
			dev_kfree_skb(nskb);
	return ret;
}

static int
isdn_add_channels(isdn_driver_t *d, int drvidx, int n, int adding)
{
	int j, k, m;

	init_waitqueue_head(&d->st_waitq);
	if (d->flags & DRV_FLAG_RUNNING)
		return -1;
	if (n < 1) return 0;

	m = (adding) ? d->channels + n : n;

	if (dev->channels + n > ISDN_MAX_CHANNELS) {
		printk(KERN_WARNING "register_isdn: Max. %d channels supported\n",
		       ISDN_MAX_CHANNELS);
		return -1;
	}

	if ((adding) && (d->rcverr))
		kfree(d->rcverr);
	if (!(d->rcverr = kzalloc(sizeof(int) * m, GFP_ATOMIC))) {
		printk(KERN_WARNING "register_isdn: Could not alloc rcverr\n");
		return -1;
	}

	if ((adding) && (d->rcvcount))
		kfree(d->rcvcount);
	if (!(d->rcvcount = kzalloc(sizeof(int) * m, GFP_ATOMIC))) {
		printk(KERN_WARNING "register_isdn: Could not alloc rcvcount\n");
		if (!adding)
			kfree(d->rcverr);
		return -1;
	}

	if ((adding) && (d->rpqueue)) {
		for (j = 0; j < d->channels; j++)
			skb_queue_purge(&d->rpqueue[j]);
		kfree(d->rpqueue);
	}
	if (!(d->rpqueue = kmalloc(sizeof(struct sk_buff_head) * m, GFP_ATOMIC))) {
		printk(KERN_WARNING "register_isdn: Could not alloc rpqueue\n");
		if (!adding) {
			kfree(d->rcvcount);
			kfree(d->rcverr);
		}
		return -1;
	}
	for (j = 0; j < m; j++) {
		skb_queue_head_init(&d->rpqueue[j]);
	}

	if ((adding) && (d->rcv_waitq))
		kfree(d->rcv_waitq);
	d->rcv_waitq = kmalloc(sizeof(wait_queue_head_t) * 2 * m, GFP_ATOMIC);
	if (!d->rcv_waitq) {
		printk(KERN_WARNING "register_isdn: Could not alloc rcv_waitq\n");
		if (!adding) {
			kfree(d->rpqueue);
			kfree(d->rcvcount);
			kfree(d->rcverr);
		}
		return -1;
	}
	d->snd_waitq = d->rcv_waitq + m;
	for (j = 0; j < m; j++) {
		init_waitqueue_head(&d->rcv_waitq[j]);
		init_waitqueue_head(&d->snd_waitq[j]);
	}

	dev->channels += n;
	for (j = d->channels; j < m; j++)
		for (k = 0; k < ISDN_MAX_CHANNELS; k++)
			if (dev->chanmap[k] < 0) {
				dev->chanmap[k] = j;
				dev->drvmap[k] = drvidx;
				break;
			}
	d->channels = m;
	return 0;
}


static void
set_global_features(void)
{
	int drvidx;

	dev->global_features = 0;
	for (drvidx = 0; drvidx < ISDN_MAX_DRIVERS; drvidx++) {
		if (!dev->drv[drvidx])
			continue;
		if (dev->drv[drvidx]->interface)
			dev->global_features |= dev->drv[drvidx]->interface->features;
	}
}

#ifdef CONFIG_ISDN_DIVERSION

static char *map_drvname(int di)
{
	if ((di < 0) || (di >= ISDN_MAX_DRIVERS))
		return (NULL);
	return (dev->drvid[di]); 
} 

static int map_namedrv(char *id)
{  int i;

	for (i = 0; i < ISDN_MAX_DRIVERS; i++)
	{ if (!strcmp(dev->drvid[i], id))
			return (i);
	}
	return (-1);
} 

int DIVERT_REG_NAME(isdn_divert_if *i_div)
{
	if (i_div->if_magic != DIVERT_IF_MAGIC)
		return (DIVERT_VER_ERR);
	switch (i_div->cmd)
	{
	case DIVERT_CMD_REL:
		if (divert_if != i_div)
			return (DIVERT_REL_ERR);
		divert_if = NULL; 
		return (DIVERT_NO_ERR);

	case DIVERT_CMD_REG:
		if (divert_if)
			return (DIVERT_REG_ERR);
		i_div->ll_cmd = isdn_command; 
		i_div->drv_to_name = map_drvname;
		i_div->name_to_drv = map_namedrv;
		divert_if = i_div; 
		return (DIVERT_NO_ERR);

	default:
		return (DIVERT_CMD_ERR);
	}
} 

EXPORT_SYMBOL(DIVERT_REG_NAME);

#endif 


EXPORT_SYMBOL(register_isdn);
#ifdef CONFIG_ISDN_PPP
EXPORT_SYMBOL(isdn_ppp_register_compressor);
EXPORT_SYMBOL(isdn_ppp_unregister_compressor);
#endif

int
register_isdn(isdn_if *i)
{
	isdn_driver_t *d;
	int j;
	ulong flags;
	int drvidx;

	if (dev->drivers >= ISDN_MAX_DRIVERS) {
		printk(KERN_WARNING "register_isdn: Max. %d drivers supported\n",
		       ISDN_MAX_DRIVERS);
		return 0;
	}
	if (!i->writebuf_skb) {
		printk(KERN_WARNING "register_isdn: No write routine given.\n");
		return 0;
	}
	if (!(d = kzalloc(sizeof(isdn_driver_t), GFP_KERNEL))) {
		printk(KERN_WARNING "register_isdn: Could not alloc driver-struct\n");
		return 0;
	}

	d->maxbufsize = i->maxbufsize;
	d->pktcount = 0;
	d->stavail = 0;
	d->flags = DRV_FLAG_LOADED;
	d->online = 0;
	d->interface = i;
	d->channels = 0;
	spin_lock_irqsave(&dev->lock, flags);
	for (drvidx = 0; drvidx < ISDN_MAX_DRIVERS; drvidx++)
		if (!dev->drv[drvidx])
			break;
	if (isdn_add_channels(d, drvidx, i->channels, 0)) {
		spin_unlock_irqrestore(&dev->lock, flags);
		kfree(d);
		return 0;
	}
	i->channels = drvidx;
	i->rcvcallb_skb = isdn_receive_skb_callback;
	i->statcallb = isdn_status_callback;
	if (!strlen(i->id))
		sprintf(i->id, "line%d", drvidx);
	for (j = 0; j < drvidx; j++)
		if (!strcmp(i->id, dev->drvid[j]))
			sprintf(i->id, "line%d", drvidx);
	dev->drv[drvidx] = d;
	strcpy(dev->drvid[drvidx], i->id);
	isdn_info_update();
	dev->drivers++;
	set_global_features();
	spin_unlock_irqrestore(&dev->lock, flags);
	return 1;
}


static char *
isdn_getrev(const char *revision)
{
	char *rev;
	char *p;

	if ((p = strchr(revision, ':'))) {
		rev = p + 2;
		p = strchr(rev, '$');
		*--p = 0;
	} else
		rev = "???";
	return rev;
}

static int __init isdn_init(void)
{
	int i;
	char tmprev[50];

	dev = vzalloc(sizeof(isdn_dev));
	if (!dev) {
		printk(KERN_WARNING "isdn: Could not allocate device-struct.\n");
		return -EIO;
	}
	init_timer(&dev->timer);
	dev->timer.function = isdn_timer_funct;
	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->timerlock);
#ifdef MODULE
	dev->owner = THIS_MODULE;
#endif
	mutex_init(&dev->mtx);
	init_waitqueue_head(&dev->info_waitq);
	for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
		dev->drvmap[i] = -1;
		dev->chanmap[i] = -1;
		dev->m_idx[i] = -1;
		strcpy(dev->num[i], "???");
		init_waitqueue_head(&dev->mdm.info[i].open_wait);
		init_waitqueue_head(&dev->mdm.info[i].close_wait);
	}
	if (register_chrdev(ISDN_MAJOR, "isdn", &isdn_fops)) {
		printk(KERN_WARNING "isdn: Could not register control devices\n");
		vfree(dev);
		return -EIO;
	}
	if ((isdn_tty_modem_init()) < 0) {
		printk(KERN_WARNING "isdn: Could not register tty devices\n");
		vfree(dev);
		unregister_chrdev(ISDN_MAJOR, "isdn");
		return -EIO;
	}
#ifdef CONFIG_ISDN_PPP
	if (isdn_ppp_init() < 0) {
		printk(KERN_WARNING "isdn: Could not create PPP-device-structs\n");
		isdn_tty_exit();
		unregister_chrdev(ISDN_MAJOR, "isdn");
		vfree(dev);
		return -EIO;
	}
#endif                          

	strcpy(tmprev, isdn_revision);
	printk(KERN_NOTICE "ISDN subsystem Rev: %s/", isdn_getrev(tmprev));
	strcpy(tmprev, isdn_tty_revision);
	printk("%s/", isdn_getrev(tmprev));
	strcpy(tmprev, isdn_net_revision);
	printk("%s/", isdn_getrev(tmprev));
	strcpy(tmprev, isdn_ppp_revision);
	printk("%s/", isdn_getrev(tmprev));
	strcpy(tmprev, isdn_audio_revision);
	printk("%s/", isdn_getrev(tmprev));
	strcpy(tmprev, isdn_v110_revision);
	printk("%s", isdn_getrev(tmprev));

#ifdef MODULE
	printk(" loaded\n");
#else
	printk("\n");
#endif
	isdn_info_update();
	return 0;
}

static void __exit isdn_exit(void)
{
#ifdef CONFIG_ISDN_PPP
	isdn_ppp_cleanup();
#endif
	if (isdn_net_rmall() < 0) {
		printk(KERN_WARNING "isdn: net-device busy, remove cancelled\n");
		return;
	}
	isdn_tty_exit();
	unregister_chrdev(ISDN_MAJOR, "isdn");
	del_timer(&dev->timer);
	
	vfree(dev);
	printk(KERN_NOTICE "ISDN-subsystem unloaded\n");
}

module_init(isdn_init);
module_exit(isdn_exit);
