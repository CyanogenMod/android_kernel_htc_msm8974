/*
 * bcm.c - Broadcast Manager to filter/send (cyclic) CAN content
 *
 * Copyright (c) 2002-2007 Volkswagen Group Electronic Research
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Volkswagen nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Alternatively, provided that this notice is retained in full, this
 * software may be distributed under the terms of the GNU General
 * Public License ("GPL") version 2, in which case the provisions of the
 * GPL apply INSTEAD OF those given above.
 *
 * The provided data structures and external interfaces from this code
 * are not restricted to be used by modules with a GPL compatible license.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uio.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/socket.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <linux/can.h>
#include <linux/can/core.h>
#include <linux/can/bcm.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/net_namespace.h>

#define MAX_NFRAMES 256

#define RX_RECV    0x40 
#define RX_THR     0x80 
#define BCM_CAN_DLC_MASK 0x0F 

#define REGMASK(id) ((id & CAN_EFF_FLAG) ? \
		     (CAN_EFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG) : \
		     (CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG))

#define CAN_BCM_VERSION CAN_VERSION
static __initdata const char banner[] = KERN_INFO
	"can: broadcast manager protocol (rev " CAN_BCM_VERSION " t)\n";

MODULE_DESCRIPTION("PF_CAN broadcast manager protocol");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Oliver Hartkopp <oliver.hartkopp@volkswagen.de>");
MODULE_ALIAS("can-proto-2");

static inline u64 GET_U64(const struct can_frame *cp)
{
	return *(u64 *)cp->data;
}

struct bcm_op {
	struct list_head list;
	int ifindex;
	canid_t can_id;
	u32 flags;
	unsigned long frames_abs, frames_filtered;
	struct timeval ival1, ival2;
	struct hrtimer timer, thrtimer;
	struct tasklet_struct tsklet, thrtsklet;
	ktime_t rx_stamp, kt_ival1, kt_ival2, kt_lastmsg;
	int rx_ifindex;
	u32 count;
	u32 nframes;
	u32 currframe;
	struct can_frame *frames;
	struct can_frame *last_frames;
	struct can_frame sframe;
	struct can_frame last_sframe;
	struct sock *sk;
	struct net_device *rx_reg_dev;
};

static struct proc_dir_entry *proc_dir;

struct bcm_sock {
	struct sock sk;
	int bound;
	int ifindex;
	struct notifier_block notifier;
	struct list_head rx_ops;
	struct list_head tx_ops;
	unsigned long dropped_usr_msgs;
	struct proc_dir_entry *bcm_proc_read;
	char procname [32]; 
};

static inline struct bcm_sock *bcm_sk(const struct sock *sk)
{
	return (struct bcm_sock *)sk;
}

#define CFSIZ sizeof(struct can_frame)
#define OPSIZ sizeof(struct bcm_op)
#define MHSIZ sizeof(struct bcm_msg_head)

static char *bcm_proc_getifname(char *result, int ifindex)
{
	struct net_device *dev;

	if (!ifindex)
		return "any";

	rcu_read_lock();
	dev = dev_get_by_index_rcu(&init_net, ifindex);
	if (dev)
		strcpy(result, dev->name);
	else
		strcpy(result, "???");
	rcu_read_unlock();

	return result;
}

static int bcm_proc_show(struct seq_file *m, void *v)
{
	char ifname[IFNAMSIZ];
	struct sock *sk = (struct sock *)m->private;
	struct bcm_sock *bo = bcm_sk(sk);
	struct bcm_op *op;

	seq_printf(m, ">>> socket %pK", sk->sk_socket);
	seq_printf(m, " / sk %pK", sk);
	seq_printf(m, " / bo %pK", bo);
	seq_printf(m, " / dropped %lu", bo->dropped_usr_msgs);
	seq_printf(m, " / bound %s", bcm_proc_getifname(ifname, bo->ifindex));
	seq_printf(m, " <<<\n");

	list_for_each_entry(op, &bo->rx_ops, list) {

		unsigned long reduction;

		
		if (!op->frames_abs)
			continue;

		seq_printf(m, "rx_op: %03X %-5s ",
				op->can_id, bcm_proc_getifname(ifname, op->ifindex));
		seq_printf(m, "[%u]%c ", op->nframes,
				(op->flags & RX_CHECK_DLC)?'d':' ');
		if (op->kt_ival1.tv64)
			seq_printf(m, "timeo=%lld ",
					(long long)
					ktime_to_us(op->kt_ival1));

		if (op->kt_ival2.tv64)
			seq_printf(m, "thr=%lld ",
					(long long)
					ktime_to_us(op->kt_ival2));

		seq_printf(m, "# recv %ld (%ld) => reduction: ",
				op->frames_filtered, op->frames_abs);

		reduction = 100 - (op->frames_filtered * 100) / op->frames_abs;

		seq_printf(m, "%s%ld%%\n",
				(reduction == 100)?"near ":"", reduction);
	}

	list_for_each_entry(op, &bo->tx_ops, list) {

		seq_printf(m, "tx_op: %03X %s [%u] ",
				op->can_id,
				bcm_proc_getifname(ifname, op->ifindex),
				op->nframes);

		if (op->kt_ival1.tv64)
			seq_printf(m, "t1=%lld ",
					(long long) ktime_to_us(op->kt_ival1));

		if (op->kt_ival2.tv64)
			seq_printf(m, "t2=%lld ",
					(long long) ktime_to_us(op->kt_ival2));

		seq_printf(m, "# sent %ld\n", op->frames_abs);
	}
	seq_putc(m, '\n');
	return 0;
}

static int bcm_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bcm_proc_show, PDE(inode)->data);
}

static const struct file_operations bcm_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= bcm_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void bcm_can_tx(struct bcm_op *op)
{
	struct sk_buff *skb;
	struct net_device *dev;
	struct can_frame *cf = &op->frames[op->currframe];

	
	if (!op->ifindex)
		return;

	dev = dev_get_by_index(&init_net, op->ifindex);
	if (!dev) {
		
		return;
	}

	skb = alloc_skb(CFSIZ, gfp_any());
	if (!skb)
		goto out;

	memcpy(skb_put(skb, CFSIZ), cf, CFSIZ);

	
	skb->dev = dev;
	skb->sk = op->sk;
	can_send(skb, 1);

	
	op->currframe++;
	op->frames_abs++;

	
	if (op->currframe >= op->nframes)
		op->currframe = 0;
 out:
	dev_put(dev);
}

static void bcm_send_to_user(struct bcm_op *op, struct bcm_msg_head *head,
			     struct can_frame *frames, int has_timestamp)
{
	struct sk_buff *skb;
	struct can_frame *firstframe;
	struct sockaddr_can *addr;
	struct sock *sk = op->sk;
	unsigned int datalen = head->nframes * CFSIZ;
	int err;

	skb = alloc_skb(sizeof(*head) + datalen, gfp_any());
	if (!skb)
		return;

	memcpy(skb_put(skb, sizeof(*head)), head, sizeof(*head));

	if (head->nframes) {
		
		firstframe = (struct can_frame *)skb_tail_pointer(skb);

		memcpy(skb_put(skb, datalen), frames, datalen);

		if (head->nframes == 1)
			firstframe->can_dlc &= BCM_CAN_DLC_MASK;
	}

	if (has_timestamp) {
		
		skb->tstamp = op->rx_stamp;
	}


	BUILD_BUG_ON(sizeof(skb->cb) < sizeof(struct sockaddr_can));
	addr = (struct sockaddr_can *)skb->cb;
	memset(addr, 0, sizeof(*addr));
	addr->can_family  = AF_CAN;
	addr->can_ifindex = op->rx_ifindex;

	err = sock_queue_rcv_skb(sk, skb);
	if (err < 0) {
		struct bcm_sock *bo = bcm_sk(sk);

		kfree_skb(skb);
		
		bo->dropped_usr_msgs++;
	}
}

static void bcm_tx_start_timer(struct bcm_op *op)
{
	if (op->kt_ival1.tv64 && op->count)
		hrtimer_start(&op->timer,
			      ktime_add(ktime_get(), op->kt_ival1),
			      HRTIMER_MODE_ABS);
	else if (op->kt_ival2.tv64)
		hrtimer_start(&op->timer,
			      ktime_add(ktime_get(), op->kt_ival2),
			      HRTIMER_MODE_ABS);
}

static void bcm_tx_timeout_tsklet(unsigned long data)
{
	struct bcm_op *op = (struct bcm_op *)data;
	struct bcm_msg_head msg_head;

	if (op->kt_ival1.tv64 && (op->count > 0)) {

		op->count--;
		if (!op->count && (op->flags & TX_COUNTEVT)) {

			
			msg_head.opcode  = TX_EXPIRED;
			msg_head.flags   = op->flags;
			msg_head.count   = op->count;
			msg_head.ival1   = op->ival1;
			msg_head.ival2   = op->ival2;
			msg_head.can_id  = op->can_id;
			msg_head.nframes = 0;

			bcm_send_to_user(op, &msg_head, NULL, 0);
		}
		bcm_can_tx(op);

	} else if (op->kt_ival2.tv64)
		bcm_can_tx(op);

	bcm_tx_start_timer(op);
}

static enum hrtimer_restart bcm_tx_timeout_handler(struct hrtimer *hrtimer)
{
	struct bcm_op *op = container_of(hrtimer, struct bcm_op, timer);

	tasklet_schedule(&op->tsklet);

	return HRTIMER_NORESTART;
}

static void bcm_rx_changed(struct bcm_op *op, struct can_frame *data)
{
	struct bcm_msg_head head;

	
	op->frames_filtered++;

	
	if (op->frames_filtered > ULONG_MAX/100)
		op->frames_filtered = op->frames_abs = 0;

	
	data->can_dlc &= (BCM_CAN_DLC_MASK|RX_RECV);

	head.opcode  = RX_CHANGED;
	head.flags   = op->flags;
	head.count   = op->count;
	head.ival1   = op->ival1;
	head.ival2   = op->ival2;
	head.can_id  = op->can_id;
	head.nframes = 1;

	bcm_send_to_user(op, &head, data, 1);
}

static void bcm_rx_update_and_send(struct bcm_op *op,
				   struct can_frame *lastdata,
				   const struct can_frame *rxdata)
{
	memcpy(lastdata, rxdata, CFSIZ);

	
	lastdata->can_dlc |= (RX_RECV|RX_THR);

	
	if (!op->kt_ival2.tv64) {
		
		bcm_rx_changed(op, lastdata);
		return;
	}

	
	if (hrtimer_active(&op->thrtimer))
		return;

	
	if (!op->kt_lastmsg.tv64)
		goto rx_changed_settime;

	
	if (ktime_us_delta(ktime_get(), op->kt_lastmsg) <
	    ktime_to_us(op->kt_ival2)) {
		
		hrtimer_start(&op->thrtimer,
			      ktime_add(op->kt_lastmsg, op->kt_ival2),
			      HRTIMER_MODE_ABS);
		return;
	}

	
rx_changed_settime:
	bcm_rx_changed(op, lastdata);
	op->kt_lastmsg = ktime_get();
}

static void bcm_rx_cmp_to_index(struct bcm_op *op, unsigned int index,
				const struct can_frame *rxdata)
{

	if (!(op->last_frames[index].can_dlc & RX_RECV)) {
		
		bcm_rx_update_and_send(op, &op->last_frames[index], rxdata);
		return;
	}

	

	if ((GET_U64(&op->frames[index]) & GET_U64(rxdata)) !=
	    (GET_U64(&op->frames[index]) & GET_U64(&op->last_frames[index]))) {
		bcm_rx_update_and_send(op, &op->last_frames[index], rxdata);
		return;
	}

	if (op->flags & RX_CHECK_DLC) {
		
		if (rxdata->can_dlc != (op->last_frames[index].can_dlc &
					BCM_CAN_DLC_MASK)) {
			bcm_rx_update_and_send(op, &op->last_frames[index],
					       rxdata);
			return;
		}
	}
}

static void bcm_rx_starttimer(struct bcm_op *op)
{
	if (op->flags & RX_NO_AUTOTIMER)
		return;

	if (op->kt_ival1.tv64)
		hrtimer_start(&op->timer, op->kt_ival1, HRTIMER_MODE_REL);
}

static void bcm_rx_timeout_tsklet(unsigned long data)
{
	struct bcm_op *op = (struct bcm_op *)data;
	struct bcm_msg_head msg_head;

	
	msg_head.opcode  = RX_TIMEOUT;
	msg_head.flags   = op->flags;
	msg_head.count   = op->count;
	msg_head.ival1   = op->ival1;
	msg_head.ival2   = op->ival2;
	msg_head.can_id  = op->can_id;
	msg_head.nframes = 0;

	bcm_send_to_user(op, &msg_head, NULL, 0);
}

static enum hrtimer_restart bcm_rx_timeout_handler(struct hrtimer *hrtimer)
{
	struct bcm_op *op = container_of(hrtimer, struct bcm_op, timer);

	
	tasklet_hi_schedule(&op->tsklet);

	

	
	if ((op->flags & RX_ANNOUNCE_RESUME) && op->last_frames) {
		
		memset(op->last_frames, 0, op->nframes * CFSIZ);
	}

	return HRTIMER_NORESTART;
}

static inline int bcm_rx_do_flush(struct bcm_op *op, int update,
				  unsigned int index)
{
	if ((op->last_frames) && (op->last_frames[index].can_dlc & RX_THR)) {
		if (update)
			bcm_rx_changed(op, &op->last_frames[index]);
		return 1;
	}
	return 0;
}

static int bcm_rx_thr_flush(struct bcm_op *op, int update)
{
	int updated = 0;

	if (op->nframes > 1) {
		unsigned int i;

		
		for (i = 1; i < op->nframes; i++)
			updated += bcm_rx_do_flush(op, update, i);

	} else {
		
		updated += bcm_rx_do_flush(op, update, 0);
	}

	return updated;
}

static void bcm_rx_thr_tsklet(unsigned long data)
{
	struct bcm_op *op = (struct bcm_op *)data;

	
	bcm_rx_thr_flush(op, 1);
}

static enum hrtimer_restart bcm_rx_thr_handler(struct hrtimer *hrtimer)
{
	struct bcm_op *op = container_of(hrtimer, struct bcm_op, thrtimer);

	tasklet_schedule(&op->thrtsklet);

	if (bcm_rx_thr_flush(op, 0)) {
		hrtimer_forward(hrtimer, ktime_get(), op->kt_ival2);
		return HRTIMER_RESTART;
	} else {
		
		op->kt_lastmsg = ktime_set(0, 0);
		return HRTIMER_NORESTART;
	}
}

static void bcm_rx_handler(struct sk_buff *skb, void *data)
{
	struct bcm_op *op = (struct bcm_op *)data;
	const struct can_frame *rxframe = (struct can_frame *)skb->data;
	unsigned int i;

	
	hrtimer_cancel(&op->timer);

	if (op->can_id != rxframe->can_id)
		return;

	
	op->rx_stamp = skb->tstamp;
	
	op->rx_ifindex = skb->dev->ifindex;
	
	op->frames_abs++;

	if (op->flags & RX_RTR_FRAME) {
		
		bcm_can_tx(op);
		return;
	}

	if (op->flags & RX_FILTER_ID) {
		
		bcm_rx_update_and_send(op, &op->last_frames[0], rxframe);
		goto rx_starttimer;
	}

	if (op->nframes == 1) {
		
		bcm_rx_cmp_to_index(op, 0, rxframe);
		goto rx_starttimer;
	}

	if (op->nframes > 1) {

		for (i = 1; i < op->nframes; i++) {
			if ((GET_U64(&op->frames[0]) & GET_U64(rxframe)) ==
			    (GET_U64(&op->frames[0]) &
			     GET_U64(&op->frames[i]))) {
				bcm_rx_cmp_to_index(op, i, rxframe);
				break;
			}
		}
	}

rx_starttimer:
	bcm_rx_starttimer(op);
}

static struct bcm_op *bcm_find_op(struct list_head *ops, canid_t can_id,
				  int ifindex)
{
	struct bcm_op *op;

	list_for_each_entry(op, ops, list) {
		if ((op->can_id == can_id) && (op->ifindex == ifindex))
			return op;
	}

	return NULL;
}

static void bcm_remove_op(struct bcm_op *op)
{
	hrtimer_cancel(&op->timer);
	hrtimer_cancel(&op->thrtimer);

	if (op->tsklet.func)
		tasklet_kill(&op->tsklet);

	if (op->thrtsklet.func)
		tasklet_kill(&op->thrtsklet);

	if ((op->frames) && (op->frames != &op->sframe))
		kfree(op->frames);

	if ((op->last_frames) && (op->last_frames != &op->last_sframe))
		kfree(op->last_frames);

	kfree(op);
}

static void bcm_rx_unreg(struct net_device *dev, struct bcm_op *op)
{
	if (op->rx_reg_dev == dev) {
		can_rx_unregister(dev, op->can_id, REGMASK(op->can_id),
				  bcm_rx_handler, op);

		
		op->rx_reg_dev = NULL;
	} else
		printk(KERN_ERR "can-bcm: bcm_rx_unreg: registered device "
		       "mismatch %p %p\n", op->rx_reg_dev, dev);
}

static int bcm_delete_rx_op(struct list_head *ops, canid_t can_id, int ifindex)
{
	struct bcm_op *op, *n;

	list_for_each_entry_safe(op, n, ops, list) {
		if ((op->can_id == can_id) && (op->ifindex == ifindex)) {

			if (op->ifindex) {
				if (op->rx_reg_dev) {
					struct net_device *dev;

					dev = dev_get_by_index(&init_net,
							       op->ifindex);
					if (dev) {
						bcm_rx_unreg(dev, op);
						dev_put(dev);
					}
				}
			} else
				can_rx_unregister(NULL, op->can_id,
						  REGMASK(op->can_id),
						  bcm_rx_handler, op);

			list_del(&op->list);
			bcm_remove_op(op);
			return 1; 
		}
	}

	return 0; 
}

static int bcm_delete_tx_op(struct list_head *ops, canid_t can_id, int ifindex)
{
	struct bcm_op *op, *n;

	list_for_each_entry_safe(op, n, ops, list) {
		if ((op->can_id == can_id) && (op->ifindex == ifindex)) {
			list_del(&op->list);
			bcm_remove_op(op);
			return 1; 
		}
	}

	return 0; 
}

static int bcm_read_op(struct list_head *ops, struct bcm_msg_head *msg_head,
		       int ifindex)
{
	struct bcm_op *op = bcm_find_op(ops, msg_head->can_id, ifindex);

	if (!op)
		return -EINVAL;

	
	msg_head->flags   = op->flags;
	msg_head->count   = op->count;
	msg_head->ival1   = op->ival1;
	msg_head->ival2   = op->ival2;
	msg_head->nframes = op->nframes;

	bcm_send_to_user(op, msg_head, op->frames, 0);

	return MHSIZ;
}

static int bcm_tx_setup(struct bcm_msg_head *msg_head, struct msghdr *msg,
			int ifindex, struct sock *sk)
{
	struct bcm_sock *bo = bcm_sk(sk);
	struct bcm_op *op;
	unsigned int i;
	int err;

	
	if (!ifindex)
		return -ENODEV;

	
	if (msg_head->nframes < 1 || msg_head->nframes > MAX_NFRAMES)
		return -EINVAL;

	
	op = bcm_find_op(&bo->tx_ops, msg_head->can_id, ifindex);

	if (op) {
		

		if (msg_head->nframes > op->nframes)
			return -E2BIG;

		
		for (i = 0; i < msg_head->nframes; i++) {
			err = memcpy_fromiovec((u8 *)&op->frames[i],
					       msg->msg_iov, CFSIZ);

			if (op->frames[i].can_dlc > 8)
				err = -EINVAL;

			if (err < 0)
				return err;

			if (msg_head->flags & TX_CP_CAN_ID) {
				
				op->frames[i].can_id = msg_head->can_id;
			}
		}

	} else {
		

		op = kzalloc(OPSIZ, GFP_KERNEL);
		if (!op)
			return -ENOMEM;

		op->can_id    = msg_head->can_id;

		
		if (msg_head->nframes > 1) {
			op->frames = kmalloc(msg_head->nframes * CFSIZ,
					     GFP_KERNEL);
			if (!op->frames) {
				kfree(op);
				return -ENOMEM;
			}
		} else
			op->frames = &op->sframe;

		for (i = 0; i < msg_head->nframes; i++) {
			err = memcpy_fromiovec((u8 *)&op->frames[i],
					       msg->msg_iov, CFSIZ);

			if (op->frames[i].can_dlc > 8)
				err = -EINVAL;

			if (err < 0) {
				if (op->frames != &op->sframe)
					kfree(op->frames);
				kfree(op);
				return err;
			}

			if (msg_head->flags & TX_CP_CAN_ID) {
				
				op->frames[i].can_id = msg_head->can_id;
			}
		}

		
		op->last_frames = NULL;

		
		op->sk = sk;
		op->ifindex = ifindex;

		
		hrtimer_init(&op->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		op->timer.function = bcm_tx_timeout_handler;

		
		tasklet_init(&op->tsklet, bcm_tx_timeout_tsklet,
			     (unsigned long) op);

		
		hrtimer_init(&op->thrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

		
		list_add(&op->list, &bo->tx_ops);

	} 

	if (op->nframes != msg_head->nframes) {
		op->nframes   = msg_head->nframes;
		
		op->currframe = 0;
	}

	

	op->flags = msg_head->flags;

	if (op->flags & TX_RESET_MULTI_IDX) {
		
		op->currframe = 0;
	}

	if (op->flags & SETTIMER) {
		
		op->count = msg_head->count;
		op->ival1 = msg_head->ival1;
		op->ival2 = msg_head->ival2;
		op->kt_ival1 = timeval_to_ktime(msg_head->ival1);
		op->kt_ival2 = timeval_to_ktime(msg_head->ival2);

		
		if (!op->kt_ival1.tv64 && !op->kt_ival2.tv64)
			hrtimer_cancel(&op->timer);
	}

	if (op->flags & STARTTIMER) {
		hrtimer_cancel(&op->timer);
		
		op->flags |= TX_ANNOUNCE;
	}

	if (op->flags & TX_ANNOUNCE) {
		bcm_can_tx(op);
		if (op->count)
			op->count--;
	}

	if (op->flags & STARTTIMER)
		bcm_tx_start_timer(op);

	return msg_head->nframes * CFSIZ + MHSIZ;
}

static int bcm_rx_setup(struct bcm_msg_head *msg_head, struct msghdr *msg,
			int ifindex, struct sock *sk)
{
	struct bcm_sock *bo = bcm_sk(sk);
	struct bcm_op *op;
	int do_rx_register;
	int err = 0;

	if ((msg_head->flags & RX_FILTER_ID) || (!(msg_head->nframes))) {
		
		msg_head->flags |= RX_FILTER_ID;
		
		msg_head->nframes = 0;
	}

	
	if (msg_head->nframes > MAX_NFRAMES + 1)
		return -EINVAL;

	if ((msg_head->flags & RX_RTR_FRAME) &&
	    ((msg_head->nframes != 1) ||
	     (!(msg_head->can_id & CAN_RTR_FLAG))))
		return -EINVAL;

	
	op = bcm_find_op(&bo->rx_ops, msg_head->can_id, ifindex);
	if (op) {
		

		if (msg_head->nframes > op->nframes)
			return -E2BIG;

		if (msg_head->nframes) {
			
			err = memcpy_fromiovec((u8 *)op->frames,
					       msg->msg_iov,
					       msg_head->nframes * CFSIZ);
			if (err < 0)
				return err;

			
			memset(op->last_frames, 0, msg_head->nframes * CFSIZ);
		}

		op->nframes = msg_head->nframes;

		
		do_rx_register = 0;

	} else {
		
		op = kzalloc(OPSIZ, GFP_KERNEL);
		if (!op)
			return -ENOMEM;

		op->can_id    = msg_head->can_id;
		op->nframes   = msg_head->nframes;

		if (msg_head->nframes > 1) {
			
			op->frames = kmalloc(msg_head->nframes * CFSIZ,
					     GFP_KERNEL);
			if (!op->frames) {
				kfree(op);
				return -ENOMEM;
			}

			
			op->last_frames = kzalloc(msg_head->nframes * CFSIZ,
						  GFP_KERNEL);
			if (!op->last_frames) {
				kfree(op->frames);
				kfree(op);
				return -ENOMEM;
			}

		} else {
			op->frames = &op->sframe;
			op->last_frames = &op->last_sframe;
		}

		if (msg_head->nframes) {
			err = memcpy_fromiovec((u8 *)op->frames, msg->msg_iov,
					       msg_head->nframes * CFSIZ);
			if (err < 0) {
				if (op->frames != &op->sframe)
					kfree(op->frames);
				if (op->last_frames != &op->last_sframe)
					kfree(op->last_frames);
				kfree(op);
				return err;
			}
		}

		
		op->sk = sk;
		op->ifindex = ifindex;

		
		hrtimer_init(&op->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		op->timer.function = bcm_rx_timeout_handler;

		
		tasklet_init(&op->tsklet, bcm_rx_timeout_tsklet,
			     (unsigned long) op);

		hrtimer_init(&op->thrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		op->thrtimer.function = bcm_rx_thr_handler;

		
		tasklet_init(&op->thrtsklet, bcm_rx_thr_tsklet,
			     (unsigned long) op);

		
		list_add(&op->list, &bo->rx_ops);

		
		do_rx_register = 1;

	} 

	
	op->flags = msg_head->flags;

	if (op->flags & RX_RTR_FRAME) {

		
		hrtimer_cancel(&op->thrtimer);
		hrtimer_cancel(&op->timer);

		if ((op->flags & TX_CP_CAN_ID) ||
		    (op->frames[0].can_id == op->can_id))
			op->frames[0].can_id = op->can_id & ~CAN_RTR_FLAG;

	} else {
		if (op->flags & SETTIMER) {

			
			op->ival1 = msg_head->ival1;
			op->ival2 = msg_head->ival2;
			op->kt_ival1 = timeval_to_ktime(msg_head->ival1);
			op->kt_ival2 = timeval_to_ktime(msg_head->ival2);

			
			if (!op->kt_ival1.tv64)
				hrtimer_cancel(&op->timer);

			op->kt_lastmsg = ktime_set(0, 0);
			hrtimer_cancel(&op->thrtimer);
			bcm_rx_thr_flush(op, 1);
		}

		if ((op->flags & STARTTIMER) && op->kt_ival1.tv64)
			hrtimer_start(&op->timer, op->kt_ival1,
				      HRTIMER_MODE_REL);
	}

	
	if (do_rx_register) {
		if (ifindex) {
			struct net_device *dev;

			dev = dev_get_by_index(&init_net, ifindex);
			if (dev) {
				err = can_rx_register(dev, op->can_id,
						      REGMASK(op->can_id),
						      bcm_rx_handler, op,
						      "bcm");

				op->rx_reg_dev = dev;
				dev_put(dev);
			}

		} else
			err = can_rx_register(NULL, op->can_id,
					      REGMASK(op->can_id),
					      bcm_rx_handler, op, "bcm");
		if (err) {
			
			list_del(&op->list);
			bcm_remove_op(op);
			return err;
		}
	}

	return msg_head->nframes * CFSIZ + MHSIZ;
}

static int bcm_tx_send(struct msghdr *msg, int ifindex, struct sock *sk)
{
	struct sk_buff *skb;
	struct net_device *dev;
	int err;

	
	if (!ifindex)
		return -ENODEV;

	skb = alloc_skb(CFSIZ, GFP_KERNEL);

	if (!skb)
		return -ENOMEM;

	err = memcpy_fromiovec(skb_put(skb, CFSIZ), msg->msg_iov, CFSIZ);
	if (err < 0) {
		kfree_skb(skb);
		return err;
	}

	dev = dev_get_by_index(&init_net, ifindex);
	if (!dev) {
		kfree_skb(skb);
		return -ENODEV;
	}

	skb->dev = dev;
	skb->sk  = sk;
	err = can_send(skb, 1); 
	dev_put(dev);

	if (err)
		return err;

	return CFSIZ + MHSIZ;
}

static int bcm_sendmsg(struct kiocb *iocb, struct socket *sock,
		       struct msghdr *msg, size_t size)
{
	struct sock *sk = sock->sk;
	struct bcm_sock *bo = bcm_sk(sk);
	int ifindex = bo->ifindex; 
	struct bcm_msg_head msg_head;
	int ret; 

	if (!bo->bound)
		return -ENOTCONN;

	
	if (size < MHSIZ || (size - MHSIZ) % CFSIZ)
		return -EINVAL;

	

	if (!ifindex && msg->msg_name) {
		
		struct sockaddr_can *addr =
			(struct sockaddr_can *)msg->msg_name;

		if (msg->msg_namelen < sizeof(*addr))
			return -EINVAL;

		if (addr->can_family != AF_CAN)
			return -EINVAL;

		
		ifindex = addr->can_ifindex;

		if (ifindex) {
			struct net_device *dev;

			dev = dev_get_by_index(&init_net, ifindex);
			if (!dev)
				return -ENODEV;

			if (dev->type != ARPHRD_CAN) {
				dev_put(dev);
				return -ENODEV;
			}

			dev_put(dev);
		}
	}

	

	ret = memcpy_fromiovec((u8 *)&msg_head, msg->msg_iov, MHSIZ);
	if (ret < 0)
		return ret;

	lock_sock(sk);

	switch (msg_head.opcode) {

	case TX_SETUP:
		ret = bcm_tx_setup(&msg_head, msg, ifindex, sk);
		break;

	case RX_SETUP:
		ret = bcm_rx_setup(&msg_head, msg, ifindex, sk);
		break;

	case TX_DELETE:
		if (bcm_delete_tx_op(&bo->tx_ops, msg_head.can_id, ifindex))
			ret = MHSIZ;
		else
			ret = -EINVAL;
		break;

	case RX_DELETE:
		if (bcm_delete_rx_op(&bo->rx_ops, msg_head.can_id, ifindex))
			ret = MHSIZ;
		else
			ret = -EINVAL;
		break;

	case TX_READ:
		
		msg_head.opcode  = TX_STATUS;
		ret = bcm_read_op(&bo->tx_ops, &msg_head, ifindex);
		break;

	case RX_READ:
		
		msg_head.opcode  = RX_STATUS;
		ret = bcm_read_op(&bo->rx_ops, &msg_head, ifindex);
		break;

	case TX_SEND:
		
		if ((msg_head.nframes != 1) || (size != CFSIZ + MHSIZ))
			ret = -EINVAL;
		else
			ret = bcm_tx_send(msg, ifindex, sk);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	release_sock(sk);

	return ret;
}

static int bcm_notifier(struct notifier_block *nb, unsigned long msg,
			void *data)
{
	struct net_device *dev = (struct net_device *)data;
	struct bcm_sock *bo = container_of(nb, struct bcm_sock, notifier);
	struct sock *sk = &bo->sk;
	struct bcm_op *op;
	int notify_enodev = 0;

	if (!net_eq(dev_net(dev), &init_net))
		return NOTIFY_DONE;

	if (dev->type != ARPHRD_CAN)
		return NOTIFY_DONE;

	switch (msg) {

	case NETDEV_UNREGISTER:
		lock_sock(sk);

		
		list_for_each_entry(op, &bo->rx_ops, list)
			if (op->rx_reg_dev == dev)
				bcm_rx_unreg(dev, op);

		
		if (bo->bound && bo->ifindex == dev->ifindex) {
			bo->bound   = 0;
			bo->ifindex = 0;
			notify_enodev = 1;
		}

		release_sock(sk);

		if (notify_enodev) {
			sk->sk_err = ENODEV;
			if (!sock_flag(sk, SOCK_DEAD))
				sk->sk_error_report(sk);
		}
		break;

	case NETDEV_DOWN:
		if (bo->bound && bo->ifindex == dev->ifindex) {
			sk->sk_err = ENETDOWN;
			if (!sock_flag(sk, SOCK_DEAD))
				sk->sk_error_report(sk);
		}
	}

	return NOTIFY_DONE;
}

static int bcm_init(struct sock *sk)
{
	struct bcm_sock *bo = bcm_sk(sk);

	bo->bound            = 0;
	bo->ifindex          = 0;
	bo->dropped_usr_msgs = 0;
	bo->bcm_proc_read    = NULL;

	INIT_LIST_HEAD(&bo->tx_ops);
	INIT_LIST_HEAD(&bo->rx_ops);

	
	bo->notifier.notifier_call = bcm_notifier;

	register_netdevice_notifier(&bo->notifier);

	return 0;
}

static int bcm_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct bcm_sock *bo;
	struct bcm_op *op, *next;

	if (sk == NULL)
		return 0;

	bo = bcm_sk(sk);

	

	unregister_netdevice_notifier(&bo->notifier);

	lock_sock(sk);

	list_for_each_entry_safe(op, next, &bo->tx_ops, list)
		bcm_remove_op(op);

	list_for_each_entry_safe(op, next, &bo->rx_ops, list) {
		if (op->ifindex) {
			if (op->rx_reg_dev) {
				struct net_device *dev;

				dev = dev_get_by_index(&init_net, op->ifindex);
				if (dev) {
					bcm_rx_unreg(dev, op);
					dev_put(dev);
				}
			}
		} else
			can_rx_unregister(NULL, op->can_id,
					  REGMASK(op->can_id),
					  bcm_rx_handler, op);

		bcm_remove_op(op);
	}

	
	if (proc_dir && bo->bcm_proc_read)
		remove_proc_entry(bo->procname, proc_dir);

	
	if (bo->bound) {
		bo->bound   = 0;
		bo->ifindex = 0;
	}

	sock_orphan(sk);
	sock->sk = NULL;

	release_sock(sk);
	sock_put(sk);

	return 0;
}

static int bcm_connect(struct socket *sock, struct sockaddr *uaddr, int len,
		       int flags)
{
	struct sockaddr_can *addr = (struct sockaddr_can *)uaddr;
	struct sock *sk = sock->sk;
	struct bcm_sock *bo = bcm_sk(sk);

	if (len < sizeof(*addr))
		return -EINVAL;

	if (bo->bound)
		return -EISCONN;

	
	if (addr->can_ifindex) {
		struct net_device *dev;

		dev = dev_get_by_index(&init_net, addr->can_ifindex);
		if (!dev)
			return -ENODEV;

		if (dev->type != ARPHRD_CAN) {
			dev_put(dev);
			return -ENODEV;
		}

		bo->ifindex = dev->ifindex;
		dev_put(dev);

	} else {
		
		bo->ifindex = 0;
	}

	bo->bound = 1;

	if (proc_dir) {
		
		sprintf(bo->procname, "%lu", sock_i_ino(sk));
		bo->bcm_proc_read = proc_create_data(bo->procname, 0644,
						     proc_dir,
						     &bcm_proc_fops, sk);
	}

	return 0;
}

static int bcm_recvmsg(struct kiocb *iocb, struct socket *sock,
		       struct msghdr *msg, size_t size, int flags)
{
	struct sock *sk = sock->sk;
	struct sk_buff *skb;
	int error = 0;
	int noblock;
	int err;

	noblock =  flags & MSG_DONTWAIT;
	flags   &= ~MSG_DONTWAIT;
	skb = skb_recv_datagram(sk, flags, noblock, &error);
	if (!skb)
		return error;

	if (skb->len < size)
		size = skb->len;

	err = memcpy_toiovec(msg->msg_iov, skb->data, size);
	if (err < 0) {
		skb_free_datagram(sk, skb);
		return err;
	}

	sock_recv_ts_and_drops(msg, sk, skb);

	if (msg->msg_name) {
		msg->msg_namelen = sizeof(struct sockaddr_can);
		memcpy(msg->msg_name, skb->cb, msg->msg_namelen);
	}

	skb_free_datagram(sk, skb);

	return size;
}

static const struct proto_ops bcm_ops = {
	.family        = PF_CAN,
	.release       = bcm_release,
	.bind          = sock_no_bind,
	.connect       = bcm_connect,
	.socketpair    = sock_no_socketpair,
	.accept        = sock_no_accept,
	.getname       = sock_no_getname,
	.poll          = datagram_poll,
	.ioctl         = can_ioctl,	
	.listen        = sock_no_listen,
	.shutdown      = sock_no_shutdown,
	.setsockopt    = sock_no_setsockopt,
	.getsockopt    = sock_no_getsockopt,
	.sendmsg       = bcm_sendmsg,
	.recvmsg       = bcm_recvmsg,
	.mmap          = sock_no_mmap,
	.sendpage      = sock_no_sendpage,
};

static struct proto bcm_proto __read_mostly = {
	.name       = "CAN_BCM",
	.owner      = THIS_MODULE,
	.obj_size   = sizeof(struct bcm_sock),
	.init       = bcm_init,
};

static const struct can_proto bcm_can_proto = {
	.type       = SOCK_DGRAM,
	.protocol   = CAN_BCM,
	.ops        = &bcm_ops,
	.prot       = &bcm_proto,
};

static int __init bcm_module_init(void)
{
	int err;

	printk(banner);

	err = can_proto_register(&bcm_can_proto);
	if (err < 0) {
		printk(KERN_ERR "can: registration of bcm protocol failed\n");
		return err;
	}

	
	proc_dir = proc_mkdir("can-bcm", init_net.proc_net);
	return 0;
}

static void __exit bcm_module_exit(void)
{
	can_proto_unregister(&bcm_can_proto);

	if (proc_dir)
		proc_net_remove(&init_net, "can-bcm");
}

module_init(bcm_module_init);
module_exit(bcm_module_exit);
