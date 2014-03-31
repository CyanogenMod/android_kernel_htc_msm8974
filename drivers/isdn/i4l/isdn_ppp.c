/* $Id: isdn_ppp.c,v 1.1.2.3 2004/02/10 01:07:13 keil Exp $
 *
 * Linux ISDN subsystem, functions for synchronous PPP (linklevel).
 *
 * Copyright 1995,96 by Michael Hipp (Michael.Hipp@student.uni-tuebingen.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/isdn.h>
#include <linux/poll.h>
#include <linux/ppp-comp.h>
#include <linux/slab.h>
#ifdef CONFIG_IPPP_FILTER
#include <linux/filter.h>
#endif

#include "isdn_common.h"
#include "isdn_ppp.h"
#include "isdn_net.h"

#ifndef PPP_IPX
#define PPP_IPX 0x002b
#endif

static int isdn_ppp_fill_rq(unsigned char *buf, int len, int proto, int slot);
static int isdn_ppp_closewait(int slot);
static void isdn_ppp_push_higher(isdn_net_dev *net_dev, isdn_net_local *lp,
				 struct sk_buff *skb, int proto);
static int isdn_ppp_if_get_unit(char *namebuf);
static int isdn_ppp_set_compressor(struct ippp_struct *is, struct isdn_ppp_comp_data *);
static struct sk_buff *isdn_ppp_decompress(struct sk_buff *,
					   struct ippp_struct *, struct ippp_struct *, int *proto);
static void isdn_ppp_receive_ccp(isdn_net_dev *net_dev, isdn_net_local *lp,
				 struct sk_buff *skb, int proto);
static struct sk_buff *isdn_ppp_compress(struct sk_buff *skb_in, int *proto,
					 struct ippp_struct *is, struct ippp_struct *master, int type);
static void isdn_ppp_send_ccp(isdn_net_dev *net_dev, isdn_net_local *lp,
			      struct sk_buff *skb);

static void isdn_ppp_ccp_kickup(struct ippp_struct *is);
static void isdn_ppp_ccp_xmit_reset(struct ippp_struct *is, int proto,
				    unsigned char code, unsigned char id,
				    unsigned char *data, int len);
static struct ippp_ccp_reset *isdn_ppp_ccp_reset_alloc(struct ippp_struct *is);
static void isdn_ppp_ccp_reset_free(struct ippp_struct *is);
static void isdn_ppp_ccp_reset_free_state(struct ippp_struct *is,
					  unsigned char id);
static void isdn_ppp_ccp_timer_callback(unsigned long closure);
static struct ippp_ccp_reset_state *isdn_ppp_ccp_reset_alloc_state(struct ippp_struct *is,
								   unsigned char id);
static void isdn_ppp_ccp_reset_trans(struct ippp_struct *is,
				     struct isdn_ppp_resetparams *rp);
static void isdn_ppp_ccp_reset_ack_rcvd(struct ippp_struct *is,
					unsigned char id);



#ifdef CONFIG_ISDN_MPP
static ippp_bundle *isdn_ppp_bundle_arr = NULL;

static int isdn_ppp_mp_bundle_array_init(void);
static int isdn_ppp_mp_init(isdn_net_local *lp, ippp_bundle *add_to);
static void isdn_ppp_mp_receive(isdn_net_dev *net_dev, isdn_net_local *lp,
				struct sk_buff *skb);
static void isdn_ppp_mp_cleanup(isdn_net_local *lp);

static int isdn_ppp_bundle(struct ippp_struct *, int unit);
#endif	

char *isdn_ppp_revision = "$Revision: 1.1.2.3 $";

static struct ippp_struct *ippp_table[ISDN_MAX_CHANNELS];

static struct isdn_ppp_compressor *ipc_head = NULL;

static void
isdn_ppp_frame_log(char *info, char *data, int len, int maxlen, int unit, int slot)
{
	int cnt,
		j,
		i;
	char buf[80];

	if (len < maxlen)
		maxlen = len;

	for (i = 0, cnt = 0; cnt < maxlen; i++) {
		for (j = 0; j < 16 && cnt < maxlen; j++, cnt++)
			sprintf(buf + j * 3, "%02x ", (unsigned char)data[cnt]);
		printk(KERN_DEBUG "[%d/%d].%s[%d]: %s\n", unit, slot, info, i, buf);
	}
}

int
isdn_ppp_free(isdn_net_local *lp)
{
	struct ippp_struct *is;

	if (lp->ppp_slot < 0 || lp->ppp_slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "%s: ppp_slot(%d) out of range\n",
		       __func__, lp->ppp_slot);
		return 0;
	}

#ifdef CONFIG_ISDN_MPP
	spin_lock(&lp->netdev->pb->lock);
#endif
	isdn_net_rm_from_bundle(lp);
#ifdef CONFIG_ISDN_MPP
	if (lp->netdev->pb->ref_ct == 1)	
		isdn_ppp_mp_cleanup(lp);

	lp->netdev->pb->ref_ct--;
	spin_unlock(&lp->netdev->pb->lock);
#endif 
	if (lp->ppp_slot < 0 || lp->ppp_slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "%s: ppp_slot(%d) now invalid\n",
		       __func__, lp->ppp_slot);
		return 0;
	}
	is = ippp_table[lp->ppp_slot];
	if ((is->state & IPPP_CONNECT))
		isdn_ppp_closewait(lp->ppp_slot);	
	else if (is->state & IPPP_ASSIGNED)
		is->state = IPPP_OPEN;	

	if (is->debug & 0x1)
		printk(KERN_DEBUG "isdn_ppp_free %d %lx %lx\n", lp->ppp_slot, (long) lp, (long) is->lp);

	is->lp = NULL;          
	lp->ppp_slot = -1;      

	return 0;
}

int
isdn_ppp_bind(isdn_net_local *lp)
{
	int i;
	int unit = 0;
	struct ippp_struct *is;
	int retval;

	if (lp->pppbind < 0) {  
		isdn_net_dev *net_dev = dev->netdev;
		char exclusive[ISDN_MAX_CHANNELS];	
		memset(exclusive, 0, ISDN_MAX_CHANNELS);
		while (net_dev) {	
			isdn_net_local *lp = net_dev->local;
			if (lp->pppbind >= 0)
				exclusive[lp->pppbind] = 1;
			net_dev = net_dev->next;
		}
		for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
			if (ippp_table[i]->state == IPPP_OPEN && !exclusive[ippp_table[i]->minor]) {	
				break;
			}
		}
	} else {
		for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
			if (ippp_table[i]->minor == lp->pppbind &&
			    (ippp_table[i]->state & IPPP_OPEN) == IPPP_OPEN)
				break;
		}
	}

	if (i >= ISDN_MAX_CHANNELS) {
		printk(KERN_WARNING "isdn_ppp_bind: Can't find a (free) connection to the ipppd daemon.\n");
		retval = -1;
		goto out;
	}
	
	unit = isdn_ppp_if_get_unit(lp->netdev->dev->name);
	if (unit < 0) {
		printk(KERN_ERR "isdn_ppp_bind: illegal interface name %s.\n",
		       lp->netdev->dev->name);
		retval = -1;
		goto out;
	}

	lp->ppp_slot = i;
	is = ippp_table[i];
	is->lp = lp;
	is->unit = unit;
	is->state = IPPP_OPEN | IPPP_ASSIGNED;	
#ifdef CONFIG_ISDN_MPP
	retval = isdn_ppp_mp_init(lp, NULL);
	if (retval < 0)
		goto out;
#endif 

	retval = lp->ppp_slot;

out:
	return retval;
}


void
isdn_ppp_wakeup_daemon(isdn_net_local *lp)
{
	if (lp->ppp_slot < 0 || lp->ppp_slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "%s: ppp_slot(%d) out of range\n",
		       __func__, lp->ppp_slot);
		return;
	}
	ippp_table[lp->ppp_slot]->state = IPPP_OPEN | IPPP_CONNECT | IPPP_NOBLOCK;
	wake_up_interruptible(&ippp_table[lp->ppp_slot]->wq);
}

static int
isdn_ppp_closewait(int slot)
{
	struct ippp_struct *is;

	if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "%s: slot(%d) out of range\n",
		       __func__, slot);
		return 0;
	}
	is = ippp_table[slot];
	if (is->state)
		wake_up_interruptible(&is->wq);
	is->state = IPPP_CLOSEWAIT;
	return 1;
}


static int
isdn_ppp_get_slot(void)
{
	int i;
	for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
		if (!ippp_table[i]->state)
			return i;
	}
	return -1;
}


int
isdn_ppp_open(int min, struct file *file)
{
	int slot;
	struct ippp_struct *is;

	if (min < 0 || min >= ISDN_MAX_CHANNELS)
		return -ENODEV;

	slot = isdn_ppp_get_slot();
	if (slot < 0) {
		return -EBUSY;
	}
	is = file->private_data = ippp_table[slot];

	printk(KERN_DEBUG "ippp, open, slot: %d, minor: %d, state: %04x\n",
	       slot, min, is->state);

	
	is->link_compressor   = is->compressor = NULL;
	is->link_decompressor = is->decompressor = NULL;
	is->link_comp_stat    = is->comp_stat = NULL;
	is->link_decomp_stat  = is->decomp_stat = NULL;
	is->compflags = 0;

	is->reset = isdn_ppp_ccp_reset_alloc(is);

	is->lp = NULL;
	is->mp_seqno = 0;       
	is->pppcfg = 0;         
	is->mpppcfg = 0;        
	is->last_link_seqno = -1;	
	is->unit = -1;          
	is->mru = 1524;         
	is->maxcid = 16;        
	is->tk = current;
	init_waitqueue_head(&is->wq);
	is->first = is->rq + NUM_RCV_BUFFS - 1;	
	is->last = is->rq;
	is->minor = min;
#ifdef CONFIG_ISDN_PPP_VJ
	is->slcomp = slhc_init(16, 16);	
#endif
#ifdef CONFIG_IPPP_FILTER
	is->pass_filter = NULL;
	is->active_filter = NULL;
#endif
	is->state = IPPP_OPEN;

	return 0;
}

void
isdn_ppp_release(int min, struct file *file)
{
	int i;
	struct ippp_struct *is;

	if (min < 0 || min >= ISDN_MAX_CHANNELS)
		return;
	is = file->private_data;

	if (!is) {
		printk(KERN_ERR "%s: no file->private_data\n", __func__);
		return;
	}
	if (is->debug & 0x1)
		printk(KERN_DEBUG "ippp: release, minor: %d %lx\n", min, (long) is->lp);

	if (is->lp) {           
		isdn_net_dev *p = is->lp->netdev;

		if (!p) {
			printk(KERN_ERR "%s: no lp->netdev\n", __func__);
			return;
		}
		is->state &= ~IPPP_CONNECT;	
		isdn_net_hangup(p->dev);
	}
	for (i = 0; i < NUM_RCV_BUFFS; i++) {
		kfree(is->rq[i].buf);
		is->rq[i].buf = NULL;
	}
	is->first = is->rq + NUM_RCV_BUFFS - 1;	
	is->last = is->rq;

#ifdef CONFIG_ISDN_PPP_VJ
	slhc_free(is->slcomp);
	is->slcomp = NULL;
#endif
#ifdef CONFIG_IPPP_FILTER
	kfree(is->pass_filter);
	is->pass_filter = NULL;
	kfree(is->active_filter);
	is->active_filter = NULL;
#endif

	if (is->comp_stat)
		is->compressor->free(is->comp_stat);
	if (is->link_comp_stat)
		is->link_compressor->free(is->link_comp_stat);
	if (is->link_decomp_stat)
		is->link_decompressor->free(is->link_decomp_stat);
	if (is->decomp_stat)
		is->decompressor->free(is->decomp_stat);
	is->compressor   = is->link_compressor   = NULL;
	is->decompressor = is->link_decompressor = NULL;
	is->comp_stat    = is->link_comp_stat    = NULL;
	is->decomp_stat  = is->link_decomp_stat  = NULL;

	
	if (is->reset)
		isdn_ppp_ccp_reset_free(is);

	
	is->state = 0;
}

static int
get_arg(void __user *b, void *val, int len)
{
	if (len <= 0)
		len = sizeof(void *);
	if (copy_from_user(val, b, len))
		return -EFAULT;
	return 0;
}

static int
set_arg(void __user *b, void *val, int len)
{
	if (len <= 0)
		len = sizeof(void *);
	if (copy_to_user(b, val, len))
		return -EFAULT;
	return 0;
}

#ifdef CONFIG_IPPP_FILTER
static int get_filter(void __user *arg, struct sock_filter **p)
{
	struct sock_fprog uprog;
	struct sock_filter *code = NULL;
	int len, err;

	if (copy_from_user(&uprog, arg, sizeof(uprog)))
		return -EFAULT;

	if (!uprog.len) {
		*p = NULL;
		return 0;
	}

	
	len = uprog.len * sizeof(struct sock_filter);
	code = memdup_user(uprog.filter, len);
	if (IS_ERR(code))
		return PTR_ERR(code);

	err = sk_chk_filter(code, uprog.len);
	if (err) {
		kfree(code);
		return err;
	}

	*p = code;
	return uprog.len;
}
#endif 

int
isdn_ppp_ioctl(int min, struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long val;
	int r, i, j;
	struct ippp_struct *is;
	isdn_net_local *lp;
	struct isdn_ppp_comp_data data;
	void __user *argp = (void __user *)arg;

	is = file->private_data;
	lp = is->lp;

	if (is->debug & 0x1)
		printk(KERN_DEBUG "isdn_ppp_ioctl: minor: %d cmd: %x state: %x\n", min, cmd, is->state);

	if (!(is->state & IPPP_OPEN))
		return -EINVAL;

	switch (cmd) {
	case PPPIOCBUNDLE:
#ifdef CONFIG_ISDN_MPP
		if (!(is->state & IPPP_CONNECT))
			return -EINVAL;
		if ((r = get_arg(argp, &val, sizeof(val))))
			return r;
		printk(KERN_DEBUG "iPPP-bundle: minor: %d, slave unit: %d, master unit: %d\n",
		       (int) min, (int) is->unit, (int) val);
		return isdn_ppp_bundle(is, val);
#else
		return -1;
#endif
		break;
	case PPPIOCGUNIT:	
		if ((r = set_arg(argp, &is->unit, sizeof(is->unit))))
			return r;
		break;
	case PPPIOCGIFNAME:
		if (!lp)
			return -EINVAL;
		if ((r = set_arg(argp, lp->netdev->dev->name,
				 strlen(lp->netdev->dev->name))))
			return r;
		break;
	case PPPIOCGMPFLAGS:	
		if ((r = set_arg(argp, &is->mpppcfg, sizeof(is->mpppcfg))))
			return r;
		break;
	case PPPIOCSMPFLAGS:	
		if ((r = get_arg(argp, &val, sizeof(val))))
			return r;
		is->mpppcfg = val;
		break;
	case PPPIOCGFLAGS:	
		if ((r = set_arg(argp, &is->pppcfg, sizeof(is->pppcfg))))
			return r;
		break;
	case PPPIOCSFLAGS:	
		if ((r = get_arg(argp, &val, sizeof(val)))) {
			return r;
		}
		if (val & SC_ENABLE_IP && !(is->pppcfg & SC_ENABLE_IP) && (is->state & IPPP_CONNECT)) {
			if (lp) {
				
				is->pppcfg = val; 
				netif_wake_queue(lp->netdev->dev);
				break;
			}
		}
		is->pppcfg = val;
		break;
	case PPPIOCGIDLE:	
		if (lp) {
			struct ppp_idle pidle;
			pidle.xmit_idle = pidle.recv_idle = lp->huptimer;
			if ((r = set_arg(argp, &pidle, sizeof(struct ppp_idle))))
				return r;
		}
		break;
	case PPPIOCSMRU:	
		if ((r = get_arg(argp, &val, sizeof(val))))
			return r;
		is->mru = val;
		break;
	case PPPIOCSMPMRU:
		break;
	case PPPIOCSMPMTU:
		break;
	case PPPIOCSMAXCID:	
		if ((r = get_arg(argp, &val, sizeof(val))))
			return r;
		val++;
		if (is->maxcid != val) {
#ifdef CONFIG_ISDN_PPP_VJ
			struct slcompress *sltmp;
#endif
			if (is->debug & 0x1)
				printk(KERN_DEBUG "ippp, ioctl: changed MAXCID to %ld\n", val);
			is->maxcid = val;
#ifdef CONFIG_ISDN_PPP_VJ
			sltmp = slhc_init(16, val);
			if (!sltmp) {
				printk(KERN_ERR "ippp, can't realloc slhc struct\n");
				return -ENOMEM;
			}
			if (is->slcomp)
				slhc_free(is->slcomp);
			is->slcomp = sltmp;
#endif
		}
		break;
	case PPPIOCGDEBUG:
		if ((r = set_arg(argp, &is->debug, sizeof(is->debug))))
			return r;
		break;
	case PPPIOCSDEBUG:
		if ((r = get_arg(argp, &val, sizeof(val))))
			return r;
		is->debug = val;
		break;
	case PPPIOCGCOMPRESSORS:
	{
		unsigned long protos[8] = {0,};
		struct isdn_ppp_compressor *ipc = ipc_head;
		while (ipc) {
			j = ipc->num / (sizeof(long) * 8);
			i = ipc->num % (sizeof(long) * 8);
			if (j < 8)
				protos[j] |= (0x1 << i);
			ipc = ipc->next;
		}
		if ((r = set_arg(argp, protos, 8 * sizeof(long))))
			return r;
	}
	break;
	case PPPIOCSCOMPRESSOR:
		if ((r = get_arg(argp, &data, sizeof(struct isdn_ppp_comp_data))))
			return r;
		return isdn_ppp_set_compressor(is, &data);
	case PPPIOCGCALLINFO:
	{
		struct pppcallinfo pci;
		memset((char *)&pci, 0, sizeof(struct pppcallinfo));
		if (lp)
		{
			strncpy(pci.local_num, lp->msn, 63);
			if (lp->dial) {
				strncpy(pci.remote_num, lp->dial->num, 63);
			}
			pci.charge_units = lp->charge;
			if (lp->outgoing)
				pci.calltype = CALLTYPE_OUTGOING;
			else
				pci.calltype = CALLTYPE_INCOMING;
			if (lp->flags & ISDN_NET_CALLBACK)
				pci.calltype |= CALLTYPE_CALLBACK;
		}
		return set_arg(argp, &pci, sizeof(struct pppcallinfo));
	}
#ifdef CONFIG_IPPP_FILTER
	case PPPIOCSPASS:
	{
		struct sock_filter *code;
		int len = get_filter(argp, &code);
		if (len < 0)
			return len;
		kfree(is->pass_filter);
		is->pass_filter = code;
		is->pass_len = len;
		break;
	}
	case PPPIOCSACTIVE:
	{
		struct sock_filter *code;
		int len = get_filter(argp, &code);
		if (len < 0)
			return len;
		kfree(is->active_filter);
		is->active_filter = code;
		is->active_len = len;
		break;
	}
#endif 
	default:
		break;
	}
	return 0;
}

unsigned int
isdn_ppp_poll(struct file *file, poll_table *wait)
{
	u_int mask;
	struct ippp_buf_queue *bf, *bl;
	u_long flags;
	struct ippp_struct *is;

	is = file->private_data;

	if (is->debug & 0x2)
		printk(KERN_DEBUG "isdn_ppp_poll: minor: %d\n",
		       iminor(file->f_path.dentry->d_inode));

	
	poll_wait(file, &is->wq, wait);

	if (!(is->state & IPPP_OPEN)) {
		if (is->state == IPPP_CLOSEWAIT)
			return POLLHUP;
		printk(KERN_DEBUG "isdn_ppp: device not open\n");
		return POLLERR;
	}
	
	mask = POLLOUT | POLLWRNORM;

	spin_lock_irqsave(&is->buflock, flags);
	bl = is->last;
	bf = is->first;
	if (bf->next != bl || (is->state & IPPP_NOBLOCK)) {
		is->state &= ~IPPP_NOBLOCK;
		mask |= POLLIN | POLLRDNORM;
	}
	spin_unlock_irqrestore(&is->buflock, flags);
	return mask;
}


static int
isdn_ppp_fill_rq(unsigned char *buf, int len, int proto, int slot)
{
	struct ippp_buf_queue *bf, *bl;
	u_long flags;
	u_char *nbuf;
	struct ippp_struct *is;

	if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_WARNING "ippp: illegal slot(%d).\n", slot);
		return 0;
	}
	is = ippp_table[slot];

	if (!(is->state & IPPP_CONNECT)) {
		printk(KERN_DEBUG "ippp: device not activated.\n");
		return 0;
	}
	nbuf = kmalloc(len + 4, GFP_ATOMIC);
	if (!nbuf) {
		printk(KERN_WARNING "ippp: Can't alloc buf\n");
		return 0;
	}
	nbuf[0] = PPP_ALLSTATIONS;
	nbuf[1] = PPP_UI;
	nbuf[2] = proto >> 8;
	nbuf[3] = proto & 0xff;
	memcpy(nbuf + 4, buf, len);

	spin_lock_irqsave(&is->buflock, flags);
	bf = is->first;
	bl = is->last;

	if (bf == bl) {
		printk(KERN_WARNING "ippp: Queue is full; discarding first buffer\n");
		bf = bf->next;
		kfree(bf->buf);
		is->first = bf;
	}
	bl->buf = (char *) nbuf;
	bl->len = len + 4;

	is->last = bl->next;
	spin_unlock_irqrestore(&is->buflock, flags);
	wake_up_interruptible(&is->wq);
	return len;
}


int
isdn_ppp_read(int min, struct file *file, char __user *buf, int count)
{
	struct ippp_struct *is;
	struct ippp_buf_queue *b;
	u_long flags;
	u_char *save_buf;

	is = file->private_data;

	if (!(is->state & IPPP_OPEN))
		return 0;

	if (!access_ok(VERIFY_WRITE, buf, count))
		return -EFAULT;

	spin_lock_irqsave(&is->buflock, flags);
	b = is->first->next;
	save_buf = b->buf;
	if (!save_buf) {
		spin_unlock_irqrestore(&is->buflock, flags);
		return -EAGAIN;
	}
	if (b->len < count)
		count = b->len;
	b->buf = NULL;
	is->first = b;

	spin_unlock_irqrestore(&is->buflock, flags);
	if (copy_to_user(buf, save_buf, count))
		count = -EFAULT;
	kfree(save_buf);

	return count;
}


int
isdn_ppp_write(int min, struct file *file, const char __user *buf, int count)
{
	isdn_net_local *lp;
	struct ippp_struct *is;
	int proto;
	unsigned char protobuf[4];

	is = file->private_data;

	if (!(is->state & IPPP_CONNECT))
		return 0;

	lp = is->lp;

	

	if (!lp)
		printk(KERN_DEBUG "isdn_ppp_write: lp == NULL\n");
	else {
		if (copy_from_user(protobuf, buf, 4))
			return -EFAULT;
		proto = PPP_PROTOCOL(protobuf);
		if (proto != PPP_LCP)
			lp->huptimer = 0;

		if (lp->isdn_device < 0 || lp->isdn_channel < 0)
			return 0;

		if ((dev->drv[lp->isdn_device]->flags & DRV_FLAG_RUNNING) &&
		    lp->dialstate == 0 &&
		    (lp->flags & ISDN_NET_CONNECTED)) {
			unsigned short hl;
			struct sk_buff *skb;
			hl = dev->drv[lp->isdn_device]->interface->hl_hdrlen;
			skb = alloc_skb(hl + count, GFP_ATOMIC);
			if (!skb) {
				printk(KERN_WARNING "isdn_ppp_write: out of memory!\n");
				return count;
			}
			skb_reserve(skb, hl);
			if (copy_from_user(skb_put(skb, count), buf, count))
			{
				kfree_skb(skb);
				return -EFAULT;
			}
			if (is->debug & 0x40) {
				printk(KERN_DEBUG "ppp xmit: len %d\n", (int) skb->len);
				isdn_ppp_frame_log("xmit", skb->data, skb->len, 32, is->unit, lp->ppp_slot);
			}

			isdn_ppp_send_ccp(lp->netdev, lp, skb); 

			isdn_net_write_super(lp, skb);
		}
	}
	return count;
}


int
isdn_ppp_init(void)
{
	int i,
		j;

#ifdef CONFIG_ISDN_MPP
	if (isdn_ppp_mp_bundle_array_init() < 0)
		return -ENOMEM;
#endif 

	for (i = 0; i < ISDN_MAX_CHANNELS; i++) {
		if (!(ippp_table[i] = kzalloc(sizeof(struct ippp_struct), GFP_KERNEL))) {
			printk(KERN_WARNING "isdn_ppp_init: Could not alloc ippp_table\n");
			for (j = 0; j < i; j++)
				kfree(ippp_table[j]);
			return -1;
		}
		spin_lock_init(&ippp_table[i]->buflock);
		ippp_table[i]->state = 0;
		ippp_table[i]->first = ippp_table[i]->rq + NUM_RCV_BUFFS - 1;
		ippp_table[i]->last = ippp_table[i]->rq;

		for (j = 0; j < NUM_RCV_BUFFS; j++) {
			ippp_table[i]->rq[j].buf = NULL;
			ippp_table[i]->rq[j].last = ippp_table[i]->rq +
				(NUM_RCV_BUFFS + j - 1) % NUM_RCV_BUFFS;
			ippp_table[i]->rq[j].next = ippp_table[i]->rq + (j + 1) % NUM_RCV_BUFFS;
		}
	}
	return 0;
}

void
isdn_ppp_cleanup(void)
{
	int i;

	for (i = 0; i < ISDN_MAX_CHANNELS; i++)
		kfree(ippp_table[i]);

#ifdef CONFIG_ISDN_MPP
	kfree(isdn_ppp_bundle_arr);
#endif 

}

static int isdn_ppp_skip_ac(struct ippp_struct *is, struct sk_buff *skb)
{
	if (skb->len < 1)
		return -1;

	if (skb->data[0] == 0xff) {
		if (skb->len < 2)
			return -1;

		if (skb->data[1] != 0x03)
			return -1;

		
		skb_pull(skb, 2);
	} else {
		if (is->pppcfg & SC_REJ_COMP_AC)
			
			return -1;
	}
	return 0;
}

static int isdn_ppp_strip_proto(struct sk_buff *skb)
{
	int proto;

	if (skb->len < 1)
		return -1;

	if (skb->data[0] & 0x1) {
		
		proto = skb->data[0];
		skb_pull(skb, 1);
	} else {
		if (skb->len < 2)
			return -1;
		proto = ((int) skb->data[0] << 8) + skb->data[1];
		skb_pull(skb, 2);
	}
	return proto;
}


void isdn_ppp_receive(isdn_net_dev *net_dev, isdn_net_local *lp, struct sk_buff *skb)
{
	struct ippp_struct *is;
	int slot;
	int proto;

	BUG_ON(net_dev->local->master); 

	slot = lp->ppp_slot;
	if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "isdn_ppp_receive: lp->ppp_slot(%d)\n",
		       lp->ppp_slot);
		kfree_skb(skb);
		return;
	}
	is = ippp_table[slot];

	if (is->debug & 0x4) {
		printk(KERN_DEBUG "ippp_receive: is:%08lx lp:%08lx slot:%d unit:%d len:%d\n",
		       (long)is, (long)lp, lp->ppp_slot, is->unit, (int)skb->len);
		isdn_ppp_frame_log("receive", skb->data, skb->len, 32, is->unit, lp->ppp_slot);
	}

	if (isdn_ppp_skip_ac(is, skb) < 0) {
		kfree_skb(skb);
		return;
	}
	proto = isdn_ppp_strip_proto(skb);
	if (proto < 0) {
		kfree_skb(skb);
		return;
	}

#ifdef CONFIG_ISDN_MPP
	if (is->compflags & SC_LINK_DECOMP_ON) {
		skb = isdn_ppp_decompress(skb, is, NULL, &proto);
		if (!skb) 
			return;
	}

	if (!(is->mpppcfg & SC_REJ_MP_PROT)) { 
		if (proto == PPP_MP) {
			isdn_ppp_mp_receive(net_dev, lp, skb);
			return;
		}
	}
#endif
	isdn_ppp_push_higher(net_dev, lp, skb, proto);
}

static void
isdn_ppp_push_higher(isdn_net_dev *net_dev, isdn_net_local *lp, struct sk_buff *skb, int proto)
{
	struct net_device *dev = net_dev->dev;
	struct ippp_struct *is, *mis;
	isdn_net_local *mlp = NULL;
	int slot;

	slot = lp->ppp_slot;
	if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "isdn_ppp_push_higher: lp->ppp_slot(%d)\n",
		       lp->ppp_slot);
		goto drop_packet;
	}
	is = ippp_table[slot];

	if (lp->master) { 
		mlp = ISDN_MASTER_PRIV(lp);
		slot = mlp->ppp_slot;
		if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
			printk(KERN_ERR "isdn_ppp_push_higher: master->ppp_slot(%d)\n",
			       lp->ppp_slot);
			goto drop_packet;
		}
	}
	mis = ippp_table[slot];

	if (is->debug & 0x10) {
		printk(KERN_DEBUG "push, skb %d %04x\n", (int) skb->len, proto);
		isdn_ppp_frame_log("rpush", skb->data, skb->len, 32, is->unit, lp->ppp_slot);
	}
	if (mis->compflags & SC_DECOMP_ON) {
		skb = isdn_ppp_decompress(skb, is, mis, &proto);
		if (!skb) 
			return;
	}
	switch (proto) {
	case PPP_IPX:  
		if (is->debug & 0x20)
			printk(KERN_DEBUG "isdn_ppp: IPX\n");
		skb->protocol = htons(ETH_P_IPX);
		break;
	case PPP_IP:
		if (is->debug & 0x20)
			printk(KERN_DEBUG "isdn_ppp: IP\n");
		skb->protocol = htons(ETH_P_IP);
		break;
	case PPP_COMP:
	case PPP_COMPFRAG:
		printk(KERN_INFO "isdn_ppp: unexpected compressed frame dropped\n");
		goto drop_packet;
#ifdef CONFIG_ISDN_PPP_VJ
	case PPP_VJC_UNCOMP:
		if (is->debug & 0x20)
			printk(KERN_DEBUG "isdn_ppp: VJC_UNCOMP\n");
		if (net_dev->local->ppp_slot < 0) {
			printk(KERN_ERR "%s: net_dev->local->ppp_slot(%d) out of range\n",
			       __func__, net_dev->local->ppp_slot);
			goto drop_packet;
		}
		if (slhc_remember(ippp_table[net_dev->local->ppp_slot]->slcomp, skb->data, skb->len) <= 0) {
			printk(KERN_WARNING "isdn_ppp: received illegal VJC_UNCOMP frame!\n");
			goto drop_packet;
		}
		skb->protocol = htons(ETH_P_IP);
		break;
	case PPP_VJC_COMP:
		if (is->debug & 0x20)
			printk(KERN_DEBUG "isdn_ppp: VJC_COMP\n");
		{
			struct sk_buff *skb_old = skb;
			int pkt_len;
			skb = dev_alloc_skb(skb_old->len + 128);

			if (!skb) {
				printk(KERN_WARNING "%s: Memory squeeze, dropping packet.\n", dev->name);
				skb = skb_old;
				goto drop_packet;
			}
			skb_put(skb, skb_old->len + 128);
			skb_copy_from_linear_data(skb_old, skb->data,
						  skb_old->len);
			if (net_dev->local->ppp_slot < 0) {
				printk(KERN_ERR "%s: net_dev->local->ppp_slot(%d) out of range\n",
				       __func__, net_dev->local->ppp_slot);
				goto drop_packet;
			}
			pkt_len = slhc_uncompress(ippp_table[net_dev->local->ppp_slot]->slcomp,
						  skb->data, skb_old->len);
			kfree_skb(skb_old);
			if (pkt_len < 0)
				goto drop_packet;

			skb_trim(skb, pkt_len);
			skb->protocol = htons(ETH_P_IP);
		}
		break;
#endif
	case PPP_CCP:
	case PPP_CCPFRAG:
		isdn_ppp_receive_ccp(net_dev, lp, skb, proto);
		if (skb->data[0] == CCP_RESETREQ ||
		    skb->data[0] == CCP_RESETACK)
			break;
		
	default:
		isdn_ppp_fill_rq(skb->data, skb->len, proto, lp->ppp_slot);	
		kfree_skb(skb);
		return;
	}

#ifdef CONFIG_IPPP_FILTER
	skb_push(skb, 4);

	{
		u_int16_t *p = (u_int16_t *) skb->data;

		*p = 0;	
	}

	if (is->pass_filter
	    && sk_run_filter(skb, is->pass_filter) == 0) {
		if (is->debug & 0x2)
			printk(KERN_DEBUG "IPPP: inbound frame filtered.\n");
		kfree_skb(skb);
		return;
	}
	if (!(is->active_filter
	      && sk_run_filter(skb, is->active_filter) == 0)) {
		if (is->debug & 0x2)
			printk(KERN_DEBUG "IPPP: link-active filter: resetting huptimer.\n");
		lp->huptimer = 0;
		if (mlp)
			mlp->huptimer = 0;
	}
	skb_pull(skb, 4);
#else 
	lp->huptimer = 0;
	if (mlp)
		mlp->huptimer = 0;
#endif 
	skb->dev = dev;
	skb_reset_mac_header(skb);
	netif_rx(skb);
	
	return;

drop_packet:
	net_dev->local->stats.rx_dropped++;
	kfree_skb(skb);
}

static unsigned char *isdn_ppp_skb_push(struct sk_buff **skb_p, int len)
{
	struct sk_buff *skb = *skb_p;

	if (skb_headroom(skb) < len) {
		struct sk_buff *nskb = skb_realloc_headroom(skb, len);

		if (!nskb) {
			printk(KERN_ERR "isdn_ppp_skb_push: can't realloc headroom!\n");
			dev_kfree_skb(skb);
			return NULL;
		}
		printk(KERN_DEBUG "isdn_ppp_skb_push:under %d %d\n", skb_headroom(skb), len);
		dev_kfree_skb(skb);
		*skb_p = nskb;
		return skb_push(nskb, len);
	}
	return skb_push(skb, len);
}


int
isdn_ppp_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	isdn_net_local *lp, *mlp;
	isdn_net_dev *nd;
	unsigned int proto = PPP_IP;     
	struct ippp_struct *ipt, *ipts;
	int slot, retval = NETDEV_TX_OK;

	mlp = netdev_priv(netdev);
	nd = mlp->netdev;       

	slot = mlp->ppp_slot;
	if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "isdn_ppp_xmit: lp->ppp_slot(%d)\n",
		       mlp->ppp_slot);
		kfree_skb(skb);
		goto out;
	}
	ipts = ippp_table[slot];

	if (!(ipts->pppcfg & SC_ENABLE_IP)) {	
		if (ipts->debug & 0x1)
			printk(KERN_INFO "%s: IP frame delayed.\n", netdev->name);
		retval = NETDEV_TX_BUSY;
		goto out;
	}

	switch (ntohs(skb->protocol)) {
	case ETH_P_IP:
		proto = PPP_IP;
		break;
	case ETH_P_IPX:
		proto = PPP_IPX;	
		break;
	default:
		printk(KERN_ERR "isdn_ppp: skipped unsupported protocol: %#x.\n",
		       skb->protocol);
		dev_kfree_skb(skb);
		goto out;
	}

	lp = isdn_net_get_locked_lp(nd);
	if (!lp) {
		printk(KERN_WARNING "%s: all channels busy - requeuing!\n", netdev->name);
		retval = NETDEV_TX_BUSY;
		goto out;
	}
	

	slot = lp->ppp_slot;
	if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "isdn_ppp_xmit: lp->ppp_slot(%d)\n",
		       lp->ppp_slot);
		kfree_skb(skb);
		goto unlock;
	}
	ipt = ippp_table[slot];


	skb_pull(skb, IPPP_MAX_HEADER);

#ifdef CONFIG_IPPP_FILTER
	*skb_push(skb, 4) = 1; 

	{
		__be16 *p = (__be16 *)skb->data;

		p++;
		*p = htons(proto);
	}

	if (ipt->pass_filter
	    && sk_run_filter(skb, ipt->pass_filter) == 0) {
		if (ipt->debug & 0x4)
			printk(KERN_DEBUG "IPPP: outbound frame filtered.\n");
		kfree_skb(skb);
		goto unlock;
	}
	if (!(ipt->active_filter
	      && sk_run_filter(skb, ipt->active_filter) == 0)) {
		if (ipt->debug & 0x4)
			printk(KERN_DEBUG "IPPP: link-active filter: resetting huptimer.\n");
		lp->huptimer = 0;
	}
	skb_pull(skb, 4);
#else 
	lp->huptimer = 0;
#endif 

	if (ipt->debug & 0x4)
		printk(KERN_DEBUG "xmit skb, len %d\n", (int) skb->len);
	if (ipts->debug & 0x40)
		isdn_ppp_frame_log("xmit0", skb->data, skb->len, 32, ipts->unit, lp->ppp_slot);

#ifdef CONFIG_ISDN_PPP_VJ
	if (proto == PPP_IP && ipts->pppcfg & SC_COMP_TCP) {	
		struct sk_buff *new_skb;
		unsigned short hl;
		hl = dev->drv[lp->isdn_device]->interface->hl_hdrlen + IPPP_MAX_HEADER;
		new_skb = alloc_skb(hl + skb->len, GFP_ATOMIC);
		if (new_skb) {
			u_char *buf;
			int pktlen;

			skb_reserve(new_skb, hl);
			new_skb->dev = skb->dev;
			skb_put(new_skb, skb->len);
			buf = skb->data;

			pktlen = slhc_compress(ipts->slcomp, skb->data, skb->len, new_skb->data,
					       &buf, !(ipts->pppcfg & SC_NO_TCP_CCID));

			if (buf != skb->data) {
				if (new_skb->data != buf)
					printk(KERN_ERR "isdn_ppp: FATAL error after slhc_compress!!\n");
				dev_kfree_skb(skb);
				skb = new_skb;
			} else {
				dev_kfree_skb(new_skb);
			}

			skb_trim(skb, pktlen);
			if (skb->data[0] & SL_TYPE_COMPRESSED_TCP) {	
				proto = PPP_VJC_COMP;
				skb->data[0] ^= SL_TYPE_COMPRESSED_TCP;
			} else {
				if (skb->data[0] >= SL_TYPE_UNCOMPRESSED_TCP)
					proto = PPP_VJC_UNCOMP;
				skb->data[0] = (skb->data[0] & 0x0f) | 0x40;
			}
		}
	}
#endif

	if (ipts->compflags & SC_COMP_ON) {
		if (ipts->compflags & SC_DECOMP_ON) {
			skb = isdn_ppp_compress(skb, &proto, ipt, ipts, 0);
		} else {
			printk(KERN_DEBUG "isdn_ppp: CCP not yet up - sending as-is\n");
		}
	}

	if (ipt->debug & 0x24)
		printk(KERN_DEBUG "xmit2 skb, len %d, proto %04x\n", (int) skb->len, proto);

#ifdef CONFIG_ISDN_MPP
	if (ipt->mpppcfg & SC_MP_PROT) {
		
		long mp_seqno = ipts->mp_seqno;
		ipts->mp_seqno++;
		if (ipt->mpppcfg & SC_OUT_SHORT_SEQ) {
			unsigned char *data = isdn_ppp_skb_push(&skb, 3);
			if (!data)
				goto unlock;
			mp_seqno &= 0xfff;
			data[0] = MP_BEGIN_FRAG | MP_END_FRAG | ((mp_seqno >> 8) & 0xf);	
			data[1] = mp_seqno & 0xff;
			data[2] = proto;	
		} else {
			unsigned char *data = isdn_ppp_skb_push(&skb, 5);
			if (!data)
				goto unlock;
			data[0] = MP_BEGIN_FRAG | MP_END_FRAG;	
			data[1] = (mp_seqno >> 16) & 0xff;	
			data[2] = (mp_seqno >> 8) & 0xff;
			data[3] = (mp_seqno >> 0) & 0xff;
			data[4] = proto;	
		}
		proto = PPP_MP; 
	}
#endif

	if (ipt->compflags & SC_LINK_COMP_ON)
		skb = isdn_ppp_compress(skb, &proto, ipt, ipts, 1);

	if ((ipt->pppcfg & SC_COMP_PROT) && (proto <= 0xff)) {
		unsigned char *data = isdn_ppp_skb_push(&skb, 1);
		if (!data)
			goto unlock;
		data[0] = proto & 0xff;
	}
	else {
		unsigned char *data = isdn_ppp_skb_push(&skb, 2);
		if (!data)
			goto unlock;
		data[0] = (proto >> 8) & 0xff;
		data[1] = proto & 0xff;
	}
	if (!(ipt->pppcfg & SC_COMP_AC)) {
		unsigned char *data = isdn_ppp_skb_push(&skb, 2);
		if (!data)
			goto unlock;
		data[0] = 0xff;    
		data[1] = 0x03;    
	}

	

	if (ipts->debug & 0x40) {
		printk(KERN_DEBUG "skb xmit: len: %d\n", (int) skb->len);
		isdn_ppp_frame_log("xmit", skb->data, skb->len, 32, ipt->unit, lp->ppp_slot);
	}

	isdn_net_writebuf_skb(lp, skb);

unlock:
	spin_unlock_bh(&lp->xmit_lock);
out:
	return retval;
}

#ifdef CONFIG_IPPP_FILTER

int isdn_ppp_autodial_filter(struct sk_buff *skb, isdn_net_local *lp)
{
	struct ippp_struct *is = ippp_table[lp->ppp_slot];
	u_int16_t proto;
	int drop = 0;

	switch (ntohs(skb->protocol)) {
	case ETH_P_IP:
		proto = PPP_IP;
		break;
	case ETH_P_IPX:
		proto = PPP_IPX;
		break;
	default:
		printk(KERN_ERR "isdn_ppp_autodial_filter: unsupported protocol 0x%x.\n",
		       skb->protocol);
		return 1;
	}

	*skb_pull(skb, IPPP_MAX_HEADER - 4) = 1; 

	{
		__be16 *p = (__be16 *)skb->data;

		p++;
		*p = htons(proto);
	}

	drop |= is->pass_filter
		&& sk_run_filter(skb, is->pass_filter) == 0;
	drop |= is->active_filter
		&& sk_run_filter(skb, is->active_filter) == 0;

	skb_push(skb, IPPP_MAX_HEADER - 4);
	return drop;
}
#endif
#ifdef CONFIG_ISDN_MPP

#define MP_HEADER_LEN	5

#define MP_LONGSEQ_MASK		0x00ffffff
#define MP_SHORTSEQ_MASK	0x00000fff
#define MP_LONGSEQ_MAX		MP_LONGSEQ_MASK
#define MP_SHORTSEQ_MAX		MP_SHORTSEQ_MASK
#define MP_LONGSEQ_MAXBIT	((MP_LONGSEQ_MASK + 1) >> 1)
#define MP_SHORTSEQ_MAXBIT	((MP_SHORTSEQ_MASK + 1) >> 1)

#define MP_LT(a, b)	((a - b) & MP_LONGSEQ_MAXBIT)
#define MP_LE(a, b)	!((b - a) & MP_LONGSEQ_MAXBIT)
#define MP_GT(a, b)	((b - a) & MP_LONGSEQ_MAXBIT)
#define MP_GE(a, b)	!((a - b) & MP_LONGSEQ_MAXBIT)

#define MP_SEQ(f)	((*(u32 *)(f->data + 1)))
#define MP_FLAGS(f)	(f->data[0])

static int isdn_ppp_mp_bundle_array_init(void)
{
	int i;
	int sz = ISDN_MAX_CHANNELS * sizeof(ippp_bundle);
	if ((isdn_ppp_bundle_arr = kzalloc(sz, GFP_KERNEL)) == NULL)
		return -ENOMEM;
	for (i = 0; i < ISDN_MAX_CHANNELS; i++)
		spin_lock_init(&isdn_ppp_bundle_arr[i].lock);
	return 0;
}

static ippp_bundle *isdn_ppp_mp_bundle_alloc(void)
{
	int i;
	for (i = 0; i < ISDN_MAX_CHANNELS; i++)
		if (isdn_ppp_bundle_arr[i].ref_ct <= 0)
			return (isdn_ppp_bundle_arr + i);
	return NULL;
}

static int isdn_ppp_mp_init(isdn_net_local *lp, ippp_bundle *add_to)
{
	struct ippp_struct *is;

	if (lp->ppp_slot < 0) {
		printk(KERN_ERR "%s: lp->ppp_slot(%d) out of range\n",
		       __func__, lp->ppp_slot);
		return (-EINVAL);
	}

	is = ippp_table[lp->ppp_slot];
	if (add_to) {
		if (lp->netdev->pb)
			lp->netdev->pb->ref_ct--;
		lp->netdev->pb = add_to;
	} else {		
		is->mp_seqno = 0;
		if ((lp->netdev->pb = isdn_ppp_mp_bundle_alloc()) == NULL)
			return -ENOMEM;
		lp->next = lp->last = lp;	
		lp->netdev->pb->frags = NULL;
		lp->netdev->pb->frames = 0;
		lp->netdev->pb->seq = UINT_MAX;
	}
	lp->netdev->pb->ref_ct++;

	is->last_link_seqno = 0;
	return 0;
}

static u32 isdn_ppp_mp_get_seq(int short_seq,
			       struct sk_buff *skb, u32 last_seq);
static struct sk_buff *isdn_ppp_mp_discard(ippp_bundle *mp,
					   struct sk_buff *from, struct sk_buff *to);
static void isdn_ppp_mp_reassembly(isdn_net_dev *net_dev, isdn_net_local *lp,
				   struct sk_buff *from, struct sk_buff *to);
static void isdn_ppp_mp_free_skb(ippp_bundle *mp, struct sk_buff *skb);
static void isdn_ppp_mp_print_recv_pkt(int slot, struct sk_buff *skb);

static void isdn_ppp_mp_receive(isdn_net_dev *net_dev, isdn_net_local *lp,
				struct sk_buff *skb)
{
	struct ippp_struct *is;
	isdn_net_local *lpq;
	ippp_bundle *mp;
	isdn_mppp_stats *stats;
	struct sk_buff *newfrag, *frag, *start, *nextf;
	u32 newseq, minseq, thisseq;
	unsigned long flags;
	int slot;

	spin_lock_irqsave(&net_dev->pb->lock, flags);
	mp = net_dev->pb;
	stats = &mp->stats;
	slot = lp->ppp_slot;
	if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "%s: lp->ppp_slot(%d)\n",
		       __func__, lp->ppp_slot);
		stats->frame_drops++;
		dev_kfree_skb(skb);
		spin_unlock_irqrestore(&mp->lock, flags);
		return;
	}
	is = ippp_table[slot];
	if (++mp->frames > stats->max_queue_len)
		stats->max_queue_len = mp->frames;

	if (is->debug & 0x8)
		isdn_ppp_mp_print_recv_pkt(lp->ppp_slot, skb);

	newseq = isdn_ppp_mp_get_seq(is->mpppcfg & SC_IN_SHORT_SEQ,
				     skb, is->last_link_seqno);


	if (mp->seq > MP_LONGSEQ_MAX && (newseq & MP_LONGSEQ_MAXBIT)) {
		mp->seq = newseq;	
	} else if (MP_LT(newseq, mp->seq)) {
		stats->frame_drops++;
		isdn_ppp_mp_free_skb(mp, skb);
		spin_unlock_irqrestore(&mp->lock, flags);
		return;
	}

	
	is->last_link_seqno = minseq = newseq;
	for (lpq = net_dev->queue;;) {
		slot = lpq->ppp_slot;
		if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
			printk(KERN_ERR "%s: lpq->ppp_slot(%d)\n",
			       __func__, lpq->ppp_slot);
		} else {
			u32 lls = ippp_table[slot]->last_link_seqno;
			if (MP_LT(lls, minseq))
				minseq = lls;
		}
		if ((lpq = lpq->next) == net_dev->queue)
			break;
	}
	if (MP_LT(minseq, mp->seq))
		minseq = mp->seq;	
	newfrag = skb;

	
	if ((frag = mp->frags) == NULL || MP_LT(newseq, MP_SEQ(frag))) {
		newfrag->next = frag;
		mp->frags = frag = newfrag;
		newfrag = NULL;
	}

	start = MP_FLAGS(frag) & MP_BEGIN_FRAG &&
		MP_SEQ(frag) == mp->seq ? frag : NULL;

	while (start != NULL || newfrag != NULL) {

		thisseq = MP_SEQ(frag);
		nextf = frag->next;

		
		if (newfrag != NULL && thisseq == newseq) {
			isdn_ppp_mp_free_skb(mp, newfrag);
			newfrag = NULL;
		}

		
		if (newfrag != NULL && (nextf == NULL ||
					MP_LT(newseq, MP_SEQ(nextf)))) {
			newfrag->next = nextf;
			frag->next = nextf = newfrag;
			newfrag = NULL;
		}

		if (start != NULL) {
			
			if (start != frag && (MP_FLAGS(frag) & MP_BEGIN_FRAG)) {
				printk(KERN_WARNING"isdn_mppp(seq %d): new "
				       "BEGIN flag with no prior END", thisseq);
				stats->seqerrs++;
				stats->frame_drops++;
				start = isdn_ppp_mp_discard(mp, start, frag);
				nextf = frag->next;
			}
		} else if (MP_LE(thisseq, minseq)) {
			if (MP_FLAGS(frag) & MP_BEGIN_FRAG)
				start = frag;
			else {
				if (MP_FLAGS(frag) & MP_END_FRAG)
					stats->frame_drops++;
				if (mp->frags == frag)
					mp->frags = nextf;
				isdn_ppp_mp_free_skb(mp, frag);
				frag = nextf;
				continue;
			}
		}

		if (start != NULL && (MP_FLAGS(frag) & MP_END_FRAG)) {
			minseq = mp->seq = (thisseq + 1) & MP_LONGSEQ_MASK;
			
			isdn_ppp_mp_reassembly(net_dev, lp, start, nextf);

			start = NULL;
			frag = NULL;

			mp->frags = nextf;
		}

		if (nextf != NULL &&
		    ((thisseq + 1) & MP_LONGSEQ_MASK) == MP_SEQ(nextf)) {

			if (frag == NULL) {
				if (MP_FLAGS(nextf) & MP_BEGIN_FRAG)
					start = nextf;
				else
				{
					printk(KERN_WARNING"isdn_mppp(seq %d):"
					       " END flag with no following "
					       "BEGIN", thisseq);
					stats->seqerrs++;
				}
			}

		} else {
			if (nextf != NULL && frag != NULL &&
			    MP_LT(thisseq, minseq)) {
				stats->frame_drops++;
				mp->frags = isdn_ppp_mp_discard(mp, start, nextf);
			}
			
			start = NULL;
		}

		frag = nextf;
	}	

	if (mp->frags == NULL)
		mp->frags = frag;

	if (mp->frames > MP_MAX_QUEUE_LEN) {
		stats->overflows++;
		while (mp->frames > MP_MAX_QUEUE_LEN) {
			frag = mp->frags->next;
			isdn_ppp_mp_free_skb(mp, mp->frags);
			mp->frags = frag;
		}
	}
	spin_unlock_irqrestore(&mp->lock, flags);
}

static void isdn_ppp_mp_cleanup(isdn_net_local *lp)
{
	struct sk_buff *frag = lp->netdev->pb->frags;
	struct sk_buff *nextfrag;
	while (frag) {
		nextfrag = frag->next;
		isdn_ppp_mp_free_skb(lp->netdev->pb, frag);
		frag = nextfrag;
	}
	lp->netdev->pb->frags = NULL;
}

static u32 isdn_ppp_mp_get_seq(int short_seq,
			       struct sk_buff *skb, u32 last_seq)
{
	u32 seq;
	int flags = skb->data[0] & (MP_BEGIN_FRAG | MP_END_FRAG);

	if (!short_seq)
	{
		seq = ntohl(*(__be32 *)skb->data) & MP_LONGSEQ_MASK;
		skb_push(skb, 1);
	}
	else
	{
		seq = ntohs(*(__be16 *)skb->data) & MP_SHORTSEQ_MASK;

		
		if (!(seq &  MP_SHORTSEQ_MAXBIT) &&
		    (last_seq &  MP_SHORTSEQ_MAXBIT) &&
		    (unsigned long)last_seq <= MP_LONGSEQ_MAX)
			seq |= (last_seq + MP_SHORTSEQ_MAX + 1) &
				(~MP_SHORTSEQ_MASK & MP_LONGSEQ_MASK);
		else
			seq |= last_seq & (~MP_SHORTSEQ_MASK & MP_LONGSEQ_MASK);

		skb_push(skb, 3);	
	}
	*(u32 *)(skb->data + 1) = seq;	
	skb->data[0] = flags;	        
	return seq;
}

struct sk_buff *isdn_ppp_mp_discard(ippp_bundle *mp,
				    struct sk_buff *from, struct sk_buff *to)
{
	if (from)
		while (from != to) {
			struct sk_buff *next = from->next;
			isdn_ppp_mp_free_skb(mp, from);
			from = next;
		}
	return from;
}

void isdn_ppp_mp_reassembly(isdn_net_dev *net_dev, isdn_net_local *lp,
			    struct sk_buff *from, struct sk_buff *to)
{
	ippp_bundle *mp = net_dev->pb;
	int proto;
	struct sk_buff *skb;
	unsigned int tot_len;

	if (lp->ppp_slot < 0 || lp->ppp_slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "%s: lp->ppp_slot(%d) out of range\n",
		       __func__, lp->ppp_slot);
		return;
	}
	if (MP_FLAGS(from) == (MP_BEGIN_FRAG | MP_END_FRAG)) {
		if (ippp_table[lp->ppp_slot]->debug & 0x40)
			printk(KERN_DEBUG "isdn_mppp: reassembly: frame %d, "
			       "len %d\n", MP_SEQ(from), from->len);
		skb = from;
		skb_pull(skb, MP_HEADER_LEN);
		mp->frames--;
	} else {
		struct sk_buff *frag;
		int n;

		for (tot_len = n = 0, frag = from; frag != to; frag = frag->next, n++)
			tot_len += frag->len - MP_HEADER_LEN;

		if (ippp_table[lp->ppp_slot]->debug & 0x40)
			printk(KERN_DEBUG"isdn_mppp: reassembling frames %d "
			       "to %d, len %d\n", MP_SEQ(from),
			       (MP_SEQ(from) + n - 1) & MP_LONGSEQ_MASK, tot_len);
		if ((skb = dev_alloc_skb(tot_len)) == NULL) {
			printk(KERN_ERR "isdn_mppp: cannot allocate sk buff "
			       "of size %d\n", tot_len);
			isdn_ppp_mp_discard(mp, from, to);
			return;
		}

		while (from != to) {
			unsigned int len = from->len - MP_HEADER_LEN;

			skb_copy_from_linear_data_offset(from, MP_HEADER_LEN,
							 skb_put(skb, len),
							 len);
			frag = from->next;
			isdn_ppp_mp_free_skb(mp, from);
			from = frag;
		}
	}
	proto = isdn_ppp_strip_proto(skb);
	isdn_ppp_push_higher(net_dev, lp, skb, proto);
}

static void isdn_ppp_mp_free_skb(ippp_bundle *mp, struct sk_buff *skb)
{
	dev_kfree_skb(skb);
	mp->frames--;
}

static void isdn_ppp_mp_print_recv_pkt(int slot, struct sk_buff *skb)
{
	printk(KERN_DEBUG "mp_recv: %d/%d -> %02x %02x %02x %02x %02x %02x\n",
	       slot, (int) skb->len,
	       (int) skb->data[0], (int) skb->data[1], (int) skb->data[2],
	       (int) skb->data[3], (int) skb->data[4], (int) skb->data[5]);
}

static int
isdn_ppp_bundle(struct ippp_struct *is, int unit)
{
	char ifn[IFNAMSIZ + 1];
	isdn_net_dev *p;
	isdn_net_local *lp, *nlp;
	int rc;
	unsigned long flags;

	sprintf(ifn, "ippp%d", unit);
	p = isdn_net_findif(ifn);
	if (!p) {
		printk(KERN_ERR "ippp_bundle: cannot find %s\n", ifn);
		return -EINVAL;
	}

	spin_lock_irqsave(&p->pb->lock, flags);

	nlp = is->lp;
	lp = p->queue;
	if (nlp->ppp_slot < 0 || nlp->ppp_slot >= ISDN_MAX_CHANNELS ||
	    lp->ppp_slot < 0 || lp->ppp_slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "ippp_bundle: binding to invalid slot %d\n",
		       nlp->ppp_slot < 0 || nlp->ppp_slot >= ISDN_MAX_CHANNELS ?
		       nlp->ppp_slot : lp->ppp_slot);
		rc = -EINVAL;
		goto out;
	}

	isdn_net_add_to_bundle(p, nlp);

	ippp_table[nlp->ppp_slot]->unit = ippp_table[lp->ppp_slot]->unit;

	
	ippp_table[nlp->ppp_slot]->pppcfg |= ippp_table[lp->ppp_slot]->pppcfg &
		(SC_ENABLE_IP | SC_NO_TCP_CCID | SC_REJ_COMP_TCP);
	ippp_table[nlp->ppp_slot]->mpppcfg |= ippp_table[lp->ppp_slot]->mpppcfg &
		(SC_MP_PROT | SC_REJ_MP_PROT | SC_OUT_SHORT_SEQ | SC_IN_SHORT_SEQ);
	rc = isdn_ppp_mp_init(nlp, p->pb);
out:
	spin_unlock_irqrestore(&p->pb->lock, flags);
	return rc;
}

#endif 


static int
isdn_ppp_dev_ioctl_stats(int slot, struct ifreq *ifr, struct net_device *dev)
{
	struct ppp_stats __user *res = ifr->ifr_data;
	struct ppp_stats t;
	isdn_net_local *lp = netdev_priv(dev);

	if (!access_ok(VERIFY_WRITE, res, sizeof(struct ppp_stats)))
		return -EFAULT;

	

	memset(&t, 0, sizeof(struct ppp_stats));
	if (dev->flags & IFF_UP) {
		t.p.ppp_ipackets = lp->stats.rx_packets;
		t.p.ppp_ibytes = lp->stats.rx_bytes;
		t.p.ppp_ierrors = lp->stats.rx_errors;
		t.p.ppp_opackets = lp->stats.tx_packets;
		t.p.ppp_obytes = lp->stats.tx_bytes;
		t.p.ppp_oerrors = lp->stats.tx_errors;
#ifdef CONFIG_ISDN_PPP_VJ
		if (slot >= 0 && ippp_table[slot]->slcomp) {
			struct slcompress *slcomp = ippp_table[slot]->slcomp;
			t.vj.vjs_packets = slcomp->sls_o_compressed + slcomp->sls_o_uncompressed;
			t.vj.vjs_compressed = slcomp->sls_o_compressed;
			t.vj.vjs_searches = slcomp->sls_o_searches;
			t.vj.vjs_misses = slcomp->sls_o_misses;
			t.vj.vjs_errorin = slcomp->sls_i_error;
			t.vj.vjs_tossed = slcomp->sls_i_tossed;
			t.vj.vjs_uncompressedin = slcomp->sls_i_uncompressed;
			t.vj.vjs_compressedin = slcomp->sls_i_compressed;
		}
#endif
	}
	if (copy_to_user(res, &t, sizeof(struct ppp_stats)))
		return -EFAULT;
	return 0;
}

int
isdn_ppp_dev_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int error = 0;
	int len;
	isdn_net_local *lp = netdev_priv(dev);


	if (lp->p_encap != ISDN_NET_ENCAP_SYNCPPP)
		return -EINVAL;

	switch (cmd) {
#define PPP_VERSION "2.3.7"
	case SIOCGPPPVER:
		len = strlen(PPP_VERSION) + 1;
		if (copy_to_user(ifr->ifr_data, PPP_VERSION, len))
			error = -EFAULT;
		break;

	case SIOCGPPPSTATS:
		error = isdn_ppp_dev_ioctl_stats(lp->ppp_slot, ifr, dev);
		break;
	default:
		error = -EINVAL;
		break;
	}
	return error;
}

static int
isdn_ppp_if_get_unit(char *name)
{
	int len,
		i,
		unit = 0,
		deci;

	len = strlen(name);

	if (strncmp("ippp", name, 4) || len > 8)
		return -1;

	for (i = 0, deci = 1; i < len; i++, deci *= 10) {
		char a = name[len - i - 1];
		if (a >= '0' && a <= '9')
			unit += (a - '0') * deci;
		else
			break;
	}
	if (!i || len - i != 4)
		unit = -1;

	return unit;
}


int
isdn_ppp_dial_slave(char *name)
{
#ifdef CONFIG_ISDN_MPP
	isdn_net_dev *ndev;
	isdn_net_local *lp;
	struct net_device *sdev;

	if (!(ndev = isdn_net_findif(name)))
		return 1;
	lp = ndev->local;
	if (!(lp->flags & ISDN_NET_CONNECTED))
		return 5;

	sdev = lp->slave;
	while (sdev) {
		isdn_net_local *mlp = netdev_priv(sdev);
		if (!(mlp->flags & ISDN_NET_CONNECTED))
			break;
		sdev = mlp->slave;
	}
	if (!sdev)
		return 2;

	isdn_net_dial_req(netdev_priv(sdev));
	return 0;
#else
	return -1;
#endif
}

int
isdn_ppp_hangup_slave(char *name)
{
#ifdef CONFIG_ISDN_MPP
	isdn_net_dev *ndev;
	isdn_net_local *lp;
	struct net_device *sdev;

	if (!(ndev = isdn_net_findif(name)))
		return 1;
	lp = ndev->local;
	if (!(lp->flags & ISDN_NET_CONNECTED))
		return 5;

	sdev = lp->slave;
	while (sdev) {
		isdn_net_local *mlp = netdev_priv(sdev);

		if (mlp->slave) { 
			isdn_net_local *nlp = ISDN_SLAVE_PRIV(mlp);

			if (!(nlp->flags & ISDN_NET_CONNECTED))
				break;
		} else if (mlp->flags & ISDN_NET_CONNECTED)
			break;

		sdev = mlp->slave;
	}
	if (!sdev)
		return 2;

	isdn_net_hangup(sdev);
	return 0;
#else
	return -1;
#endif
}




static void isdn_ppp_ccp_kickup(struct ippp_struct *is)
{
	isdn_ppp_fill_rq(NULL, 0, PPP_COMP, is->lp->ppp_slot);
}



static void isdn_ppp_ccp_xmit_reset(struct ippp_struct *is, int proto,
				    unsigned char code, unsigned char id,
				    unsigned char *data, int len)
{
	struct sk_buff *skb;
	unsigned char *p;
	int hl;
	int cnt = 0;
	isdn_net_local *lp = is->lp;

	
	hl = dev->drv[lp->isdn_device]->interface->hl_hdrlen;
	skb = alloc_skb(len + hl + 16, GFP_ATOMIC);
	if (!skb) {
		printk(KERN_WARNING
		       "ippp: CCP cannot send reset - out of memory\n");
		return;
	}
	skb_reserve(skb, hl);

	
	if (!(is->pppcfg & SC_COMP_AC)) {
		p = skb_put(skb, 2);
		*p++ = 0xff;
		*p++ = 0x03;
	}

	
	p = skb_put(skb, 6);
	*p++ = (proto >> 8);
	*p++ = (proto & 0xff);
	*p++ = code;
	*p++ = id;
	cnt = 4 + len;
	*p++ = (cnt >> 8);
	*p++ = (cnt & 0xff);

	
	if (len) {
		p = skb_put(skb, len);
		memcpy(p, data, len);
	}

	
	printk(KERN_DEBUG "Sending CCP Frame:\n");
	isdn_ppp_frame_log("ccp-xmit", skb->data, skb->len, 32, is->unit, lp->ppp_slot);

	isdn_net_write_super(lp, skb);
}

static struct ippp_ccp_reset *isdn_ppp_ccp_reset_alloc(struct ippp_struct *is)
{
	struct ippp_ccp_reset *r;
	r = kzalloc(sizeof(struct ippp_ccp_reset), GFP_KERNEL);
	if (!r) {
		printk(KERN_ERR "ippp_ccp: failed to allocate reset data"
		       " structure - no mem\n");
		return NULL;
	}
	printk(KERN_DEBUG "ippp_ccp: allocated reset data structure %p\n", r);
	is->reset = r;
	return r;
}

static void isdn_ppp_ccp_reset_free(struct ippp_struct *is)
{
	unsigned int id;

	printk(KERN_DEBUG "ippp_ccp: freeing reset data structure %p\n",
	       is->reset);
	for (id = 0; id < 256; id++) {
		if (is->reset->rs[id]) {
			isdn_ppp_ccp_reset_free_state(is, (unsigned char)id);
		}
	}
	kfree(is->reset);
	is->reset = NULL;
}

static void isdn_ppp_ccp_reset_free_state(struct ippp_struct *is,
					  unsigned char id)
{
	struct ippp_ccp_reset_state *rs;

	if (is->reset->rs[id]) {
		printk(KERN_DEBUG "ippp_ccp: freeing state for id %d\n", id);
		rs = is->reset->rs[id];
		
		if (rs->ta)
			del_timer(&rs->timer);
		is->reset->rs[id] = NULL;
		kfree(rs);
	} else {
		printk(KERN_WARNING "ippp_ccp: id %d is not allocated\n", id);
	}
}

static void isdn_ppp_ccp_timer_callback(unsigned long closure)
{
	struct ippp_ccp_reset_state *rs =
		(struct ippp_ccp_reset_state *)closure;

	if (!rs) {
		printk(KERN_ERR "ippp_ccp: timer cb with zero closure.\n");
		return;
	}
	if (rs->ta && rs->state == CCPResetSentReq) {
		
		if (!rs->expra) {
			rs->ta = 0;
			isdn_ppp_ccp_reset_free_state(rs->is, rs->id);
			return;
		}
		printk(KERN_DEBUG "ippp_ccp: CCP Reset timed out for id %d\n",
		       rs->id);
		
		isdn_ppp_ccp_xmit_reset(rs->is, PPP_CCP, CCP_RESETREQ, rs->id,
					rs->data, rs->dlen);
		
		rs->timer.expires = jiffies + HZ * 5;
		add_timer(&rs->timer);
	} else {
		printk(KERN_WARNING "ippp_ccp: timer cb in wrong state %d\n",
		       rs->state);
	}
}

static struct ippp_ccp_reset_state *isdn_ppp_ccp_reset_alloc_state(struct ippp_struct *is,
								   unsigned char id)
{
	struct ippp_ccp_reset_state *rs;
	if (is->reset->rs[id]) {
		printk(KERN_WARNING "ippp_ccp: old state exists for id %d\n",
		       id);
		return NULL;
	} else {
		rs = kzalloc(sizeof(struct ippp_ccp_reset_state), GFP_KERNEL);
		if (!rs)
			return NULL;
		rs->state = CCPResetIdle;
		rs->is = is;
		rs->id = id;
		init_timer(&rs->timer);
		rs->timer.data = (unsigned long)rs;
		rs->timer.function = isdn_ppp_ccp_timer_callback;
		is->reset->rs[id] = rs;
	}
	return rs;
}


static void isdn_ppp_ccp_reset_trans(struct ippp_struct *is,
				     struct isdn_ppp_resetparams *rp)
{
	struct ippp_ccp_reset_state *rs;

	if (rp->valid) {
		
		if (rp->rsend) {
			
			if (!(rp->idval)) {
				printk(KERN_ERR "ippp_ccp: decompressor must"
				       " specify reset id\n");
				return;
			}
			if (is->reset->rs[rp->id]) {
				rs = is->reset->rs[rp->id];
				if (rs->state == CCPResetSentReq && rs->ta) {
					printk(KERN_DEBUG "ippp_ccp: reset"
					       " trans still in progress"
					       " for id %d\n", rp->id);
				} else {
					printk(KERN_WARNING "ippp_ccp: reset"
					       " trans in wrong state %d for"
					       " id %d\n", rs->state, rp->id);
				}
			} else {
				
				printk(KERN_DEBUG "ippp_ccp: new trans for id"
				       " %d to be started\n", rp->id);
				rs = isdn_ppp_ccp_reset_alloc_state(is, rp->id);
				if (!rs) {
					printk(KERN_ERR "ippp_ccp: out of mem"
					       " allocing ccp trans\n");
					return;
				}
				rs->state = CCPResetSentReq;
				rs->expra = rp->expra;
				if (rp->dtval) {
					rs->dlen = rp->dlen;
					memcpy(rs->data, rp->data, rp->dlen);
				}
				
				isdn_ppp_ccp_xmit_reset(is, PPP_CCP,
							CCP_RESETREQ, rs->id,
							rs->data, rs->dlen);
				
				rs->timer.expires = jiffies + 5 * HZ;
				add_timer(&rs->timer);
				rs->ta = 1;
			}
		} else {
			printk(KERN_DEBUG "ippp_ccp: no reset sent\n");
		}
	} else {
		if (is->reset->rs[is->reset->lastid]) {
			rs = is->reset->rs[is->reset->lastid];
			if (rs->state == CCPResetSentReq && rs->ta) {
				printk(KERN_DEBUG "ippp_ccp: reset"
				       " trans still in progress"
				       " for id %d\n", rp->id);
			} else {
				printk(KERN_WARNING "ippp_ccp: reset"
				       " trans in wrong state %d for"
				       " id %d\n", rs->state, rp->id);
			}
		} else {
			printk(KERN_DEBUG "ippp_ccp: new trans for id"
			       " %d to be started\n", is->reset->lastid);
			rs = isdn_ppp_ccp_reset_alloc_state(is,
							    is->reset->lastid);
			if (!rs) {
				printk(KERN_ERR "ippp_ccp: out of mem"
				       " allocing ccp trans\n");
				return;
			}
			rs->state = CCPResetSentReq;
			rs->expra = 1;
			rs->dlen = 0;
			
			isdn_ppp_ccp_xmit_reset(is, PPP_CCP, CCP_RESETREQ,
						rs->id, NULL, 0);
			
			rs->timer.expires = jiffies + 5 * HZ;
			add_timer(&rs->timer);
			rs->ta = 1;
		}
	}
}

static void isdn_ppp_ccp_reset_ack_rcvd(struct ippp_struct *is,
					unsigned char id)
{
	struct ippp_ccp_reset_state *rs = is->reset->rs[id];

	if (rs) {
		if (rs->ta && rs->state == CCPResetSentReq) {
			
			if (!rs->expra)
				printk(KERN_DEBUG "ippp_ccp: ResetAck received"
				       " for id %d but not expected\n", id);
		} else {
			printk(KERN_INFO "ippp_ccp: ResetAck received out of"
			       "sync for id %d\n", id);
		}
		if (rs->ta) {
			rs->ta = 0;
			del_timer(&rs->timer);
		}
		isdn_ppp_ccp_reset_free_state(is, id);
	} else {
		printk(KERN_INFO "ippp_ccp: ResetAck received for unknown id"
		       " %d\n", id);
	}
	
	is->reset->lastid++;
}


static struct sk_buff *isdn_ppp_decompress(struct sk_buff *skb, struct ippp_struct *is, struct ippp_struct *master,
					   int *proto)
{
	void *stat = NULL;
	struct isdn_ppp_compressor *ipc = NULL;
	struct sk_buff *skb_out;
	int len;
	struct ippp_struct *ri;
	struct isdn_ppp_resetparams rsparm;
	unsigned char rsdata[IPPP_RESET_MAXDATABYTES];

	if (!master) {
		
		stat = is->link_decomp_stat;
		ipc = is->link_decompressor;
		ri = is;
	} else {
		stat = master->decomp_stat;
		ipc = master->decompressor;
		ri = master;
	}

	if (!ipc) {
		
		printk(KERN_DEBUG "ippp: no decompressor defined!\n");
		return skb;
	}
	BUG_ON(!stat); 

	if ((master && *proto == PPP_COMP) || (!master && *proto == PPP_COMPFRAG)) {
		

		
		memset(&rsparm, 0, sizeof(rsparm));
		rsparm.data = rsdata;
		rsparm.maxdlen = IPPP_RESET_MAXDATABYTES;

		skb_out = dev_alloc_skb(is->mru + PPP_HDRLEN);
		if (!skb_out) {
			kfree_skb(skb);
			printk(KERN_ERR "ippp: decomp memory allocation failure\n");
			return NULL;
		}
		len = ipc->decompress(stat, skb, skb_out, &rsparm);
		kfree_skb(skb);
		if (len <= 0) {
			switch (len) {
			case DECOMP_ERROR:
				printk(KERN_INFO "ippp: decomp wants reset %s params\n",
				       rsparm.valid ? "with" : "without");

				isdn_ppp_ccp_reset_trans(ri, &rsparm);
				break;
			case DECOMP_FATALERROR:
				ri->pppcfg |= SC_DC_FERROR;
				
				isdn_ppp_ccp_kickup(ri);
				break;
			}
			kfree_skb(skb_out);
			return NULL;
		}
		*proto = isdn_ppp_strip_proto(skb_out);
		if (*proto < 0) {
			kfree_skb(skb_out);
			return NULL;
		}
		return skb_out;
	} else {
		
		
		ipc->incomp(stat, skb, *proto);
		return skb;
	}
}

static struct sk_buff *isdn_ppp_compress(struct sk_buff *skb_in, int *proto,
					 struct ippp_struct *is, struct ippp_struct *master, int type)
{
	int ret;
	int new_proto;
	struct isdn_ppp_compressor *compressor;
	void *stat;
	struct sk_buff *skb_out;

	
	if (*proto < 0 || *proto > 0x3fff) {
		return skb_in;
	}

	if (type) { 
		return skb_in;
	}
	else {
		if (!master) {
			compressor = is->compressor;
			stat = is->comp_stat;
		}
		else {
			compressor = master->compressor;
			stat = master->comp_stat;
		}
		new_proto = PPP_COMP;
	}

	if (!compressor) {
		printk(KERN_ERR "isdn_ppp: No compressor set!\n");
		return skb_in;
	}
	if (!stat) {
		printk(KERN_ERR "isdn_ppp: Compressor not initialized?\n");
		return skb_in;
	}

	
	skb_out = alloc_skb(skb_in->len + skb_in->len / 2 + 32 +
			    skb_headroom(skb_in), GFP_ATOMIC);
	if (!skb_out)
		return skb_in;
	skb_reserve(skb_out, skb_headroom(skb_in));

	ret = (compressor->compress)(stat, skb_in, skb_out, *proto);
	if (!ret) {
		dev_kfree_skb(skb_out);
		return skb_in;
	}

	dev_kfree_skb(skb_in);
	*proto = new_proto;
	return skb_out;
}

static void isdn_ppp_receive_ccp(isdn_net_dev *net_dev, isdn_net_local *lp,
				 struct sk_buff *skb, int proto)
{
	struct ippp_struct *is;
	struct ippp_struct *mis;
	int len;
	struct isdn_ppp_resetparams rsparm;
	unsigned char rsdata[IPPP_RESET_MAXDATABYTES];

	printk(KERN_DEBUG "Received CCP frame from peer slot(%d)\n",
	       lp->ppp_slot);
	if (lp->ppp_slot < 0 || lp->ppp_slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "%s: lp->ppp_slot(%d) out of range\n",
		       __func__, lp->ppp_slot);
		return;
	}
	is = ippp_table[lp->ppp_slot];
	isdn_ppp_frame_log("ccp-rcv", skb->data, skb->len, 32, is->unit, lp->ppp_slot);

	if (lp->master) {
		int slot = ISDN_MASTER_PRIV(lp)->ppp_slot;
		if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
			printk(KERN_ERR "%s: slot(%d) out of range\n",
			       __func__, slot);
			return;
		}
		mis = ippp_table[slot];
	} else
		mis = is;

	switch (skb->data[0]) {
	case CCP_CONFREQ:
		if (is->debug & 0x10)
			printk(KERN_DEBUG "Disable compression here!\n");
		if (proto == PPP_CCP)
			mis->compflags &= ~SC_COMP_ON;
		else
			is->compflags &= ~SC_LINK_COMP_ON;
		break;
	case CCP_TERMREQ:
	case CCP_TERMACK:
		if (is->debug & 0x10)
			printk(KERN_DEBUG "Disable (de)compression here!\n");
		if (proto == PPP_CCP)
			mis->compflags &= ~(SC_DECOMP_ON | SC_COMP_ON);
		else
			is->compflags &= ~(SC_LINK_DECOMP_ON | SC_LINK_COMP_ON);
		break;
	case CCP_CONFACK:
		
		if (is->debug & 0x10)
			printk(KERN_DEBUG "Enable decompression here!\n");
		if (proto == PPP_CCP) {
			if (!mis->decompressor)
				break;
			mis->compflags |= SC_DECOMP_ON;
		} else {
			if (!is->decompressor)
				break;
			is->compflags |= SC_LINK_DECOMP_ON;
		}
		break;

	case CCP_RESETACK:
		printk(KERN_DEBUG "Received ResetAck from peer\n");
		len = (skb->data[2] << 8) | skb->data[3];
		len -= 4;

		if (proto == PPP_CCP) {
			isdn_ppp_ccp_reset_ack_rcvd(mis, skb->data[1]);
			if (mis->decompressor && mis->decomp_stat)
				mis->decompressor->
					reset(mis->decomp_stat,
					      skb->data[0],
					      skb->data[1],
					      len ? &skb->data[4] : NULL,
					      len, NULL);
			
			mis->compflags &= ~SC_DECOMP_DISCARD;
		}
		else {
			isdn_ppp_ccp_reset_ack_rcvd(is, skb->data[1]);
			if (is->link_decompressor && is->link_decomp_stat)
				is->link_decompressor->
					reset(is->link_decomp_stat,
					      skb->data[0],
					      skb->data[1],
					      len ? &skb->data[4] : NULL,
					      len, NULL);
			
			is->compflags &= ~SC_LINK_DECOMP_DISCARD;
		}
		break;

	case CCP_RESETREQ:
		printk(KERN_DEBUG "Received ResetReq from peer\n");
		
		
		memset(&rsparm, 0, sizeof(rsparm));
		rsparm.data = rsdata;
		rsparm.maxdlen = IPPP_RESET_MAXDATABYTES;
		
		len = (skb->data[2] << 8) | skb->data[3];
		len -= 4;
		if (proto == PPP_CCP) {
			if (mis->compressor && mis->comp_stat)
				mis->compressor->
					reset(mis->comp_stat,
					      skb->data[0],
					      skb->data[1],
					      len ? &skb->data[4] : NULL,
					      len, &rsparm);
		}
		else {
			if (is->link_compressor && is->link_comp_stat)
				is->link_compressor->
					reset(is->link_comp_stat,
					      skb->data[0],
					      skb->data[1],
					      len ? &skb->data[4] : NULL,
					      len, &rsparm);
		}
		
		if (rsparm.valid) {
			
			if (rsparm.rsend) {
				
				isdn_ppp_ccp_xmit_reset(is, proto, CCP_RESETACK,
							rsparm.idval ? rsparm.id
							: skb->data[1],
							rsparm.dtval ?
							rsparm.data : NULL,
							rsparm.dtval ?
							rsparm.dlen : 0);
			} else {
				printk(KERN_DEBUG "ResetAck suppressed\n");
			}
		} else {
			
			isdn_ppp_ccp_xmit_reset(is, proto, CCP_RESETACK,
						skb->data[1],
						len ? &skb->data[4] : NULL,
						len);
		}
		break;
	}
}






static void isdn_ppp_send_ccp(isdn_net_dev *net_dev, isdn_net_local *lp, struct sk_buff *skb)
{
	struct ippp_struct *mis, *is;
	int proto, slot = lp->ppp_slot;
	unsigned char *data;

	if (!skb || skb->len < 3)
		return;
	if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
		printk(KERN_ERR "%s: lp->ppp_slot(%d) out of range\n",
		       __func__, slot);
		return;
	}
	is = ippp_table[slot];
	
	data = skb->data;
	if (!(is->pppcfg & SC_COMP_AC) && data[0] == 0xff && data[1] == 0x03) {
		data += 2;
		if (skb->len < 5)
			return;
	}

	proto = ((int)data[0]<<8) + data[1];
	if (proto != PPP_CCP && proto != PPP_CCPFRAG)
		return;

	printk(KERN_DEBUG "Received CCP frame from daemon:\n");
	isdn_ppp_frame_log("ccp-xmit", skb->data, skb->len, 32, is->unit, lp->ppp_slot);

	if (lp->master) {
		slot = ISDN_MASTER_PRIV(lp)->ppp_slot;
		if (slot < 0 || slot >= ISDN_MAX_CHANNELS) {
			printk(KERN_ERR "%s: slot(%d) out of range\n",
			       __func__, slot);
			return;
		}
		mis = ippp_table[slot];
	} else
		mis = is;
	if (mis != is)
		printk(KERN_DEBUG "isdn_ppp: Ouch! Master CCP sends on slave slot!\n");

	switch (data[2]) {
	case CCP_CONFREQ:
		if (is->debug & 0x10)
			printk(KERN_DEBUG "Disable decompression here!\n");
		if (proto == PPP_CCP)
			is->compflags &= ~SC_DECOMP_ON;
		else
			is->compflags &= ~SC_LINK_DECOMP_ON;
		break;
	case CCP_TERMREQ:
	case CCP_TERMACK:
		if (is->debug & 0x10)
			printk(KERN_DEBUG "Disable (de)compression here!\n");
		if (proto == PPP_CCP)
			is->compflags &= ~(SC_DECOMP_ON | SC_COMP_ON);
		else
			is->compflags &= ~(SC_LINK_DECOMP_ON | SC_LINK_COMP_ON);
		break;
	case CCP_CONFACK:
		
		if (is->debug & 0x10)
			printk(KERN_DEBUG "Enable compression here!\n");
		if (proto == PPP_CCP) {
			if (!is->compressor)
				break;
			is->compflags |= SC_COMP_ON;
		} else {
			if (!is->compressor)
				break;
			is->compflags |= SC_LINK_COMP_ON;
		}
		break;
	case CCP_RESETACK:
		
		if (is->debug & 0x10)
			printk(KERN_DEBUG "Reset decompression state here!\n");
		printk(KERN_DEBUG "ResetAck from daemon passed by\n");
		if (proto == PPP_CCP) {
			
			if (is->compressor && is->comp_stat)
				is->compressor->reset(is->comp_stat, 0, 0,
						      NULL, 0, NULL);
			is->compflags &= ~SC_COMP_DISCARD;
		}
		else {
			if (is->link_compressor && is->link_comp_stat)
				is->link_compressor->reset(is->link_comp_stat,
							   0, 0, NULL, 0, NULL);
			is->compflags &= ~SC_LINK_COMP_DISCARD;
		}
		break;
	case CCP_RESETREQ:
		
		printk(KERN_DEBUG "ResetReq from daemon passed by\n");
		break;
	}
}

int isdn_ppp_register_compressor(struct isdn_ppp_compressor *ipc)
{
	ipc->next = ipc_head;
	ipc->prev = NULL;
	if (ipc_head) {
		ipc_head->prev = ipc;
	}
	ipc_head = ipc;
	return 0;
}

int isdn_ppp_unregister_compressor(struct isdn_ppp_compressor *ipc)
{
	if (ipc->prev)
		ipc->prev->next = ipc->next;
	else
		ipc_head = ipc->next;
	if (ipc->next)
		ipc->next->prev = ipc->prev;
	ipc->prev = ipc->next = NULL;
	return 0;
}

static int isdn_ppp_set_compressor(struct ippp_struct *is, struct isdn_ppp_comp_data *data)
{
	struct isdn_ppp_compressor *ipc = ipc_head;
	int ret;
	void *stat;
	int num = data->num;

	if (is->debug & 0x10)
		printk(KERN_DEBUG "[%d] Set %s type %d\n", is->unit,
		       (data->flags & IPPP_COMP_FLAG_XMIT) ? "compressor" : "decompressor", num);


	if (!(data->flags & IPPP_COMP_FLAG_XMIT) && !is->reset) {
		printk(KERN_ERR "ippp_ccp: no reset data structure - can't"
		       " allow decompression.\n");
		return -ENOMEM;
	}

	while (ipc) {
		if (ipc->num == num) {
			stat = ipc->alloc(data);
			if (stat) {
				ret = ipc->init(stat, data, is->unit, 0);
				if (!ret) {
					printk(KERN_ERR "Can't init (de)compression!\n");
					ipc->free(stat);
					stat = NULL;
					break;
				}
			}
			else {
				printk(KERN_ERR "Can't alloc (de)compression!\n");
				break;
			}

			if (data->flags & IPPP_COMP_FLAG_XMIT) {
				if (data->flags & IPPP_COMP_FLAG_LINK) {
					if (is->link_comp_stat)
						is->link_compressor->free(is->link_comp_stat);
					is->link_comp_stat = stat;
					is->link_compressor = ipc;
				}
				else {
					if (is->comp_stat)
						is->compressor->free(is->comp_stat);
					is->comp_stat = stat;
					is->compressor = ipc;
				}
			}
			else {
				if (data->flags & IPPP_COMP_FLAG_LINK) {
					if (is->link_decomp_stat)
						is->link_decompressor->free(is->link_decomp_stat);
					is->link_decomp_stat = stat;
					is->link_decompressor = ipc;
				}
				else {
					if (is->decomp_stat)
						is->decompressor->free(is->decomp_stat);
					is->decomp_stat = stat;
					is->decompressor = ipc;
				}
			}
			return 0;
		}
		ipc = ipc->next;
	}
	return -EINVAL;
}
