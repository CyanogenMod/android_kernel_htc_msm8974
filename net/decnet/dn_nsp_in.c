
/******************************************************************************
    (c) 1995-1998 E.M. Serrat		emserrat@geocities.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*******************************************************************************/

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/inet.h>
#include <linux/route.h>
#include <linux/slab.h>
#include <net/sock.h>
#include <net/tcp_states.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/termios.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/netfilter_decnet.h>
#include <net/neighbour.h>
#include <net/dst.h>
#include <net/dn.h>
#include <net/dn_nsp.h>
#include <net/dn_dev.h>
#include <net/dn_route.h>

extern int decnet_log_martians;

static void dn_log_martian(struct sk_buff *skb, const char *msg)
{
	if (decnet_log_martians && net_ratelimit()) {
		char *devname = skb->dev ? skb->dev->name : "???";
		struct dn_skb_cb *cb = DN_SKB_CB(skb);
		printk(KERN_INFO "DECnet: Martian packet (%s) dev=%s src=0x%04hx dst=0x%04hx srcport=0x%04hx dstport=0x%04hx\n",
		       msg, devname, le16_to_cpu(cb->src), le16_to_cpu(cb->dst),
		       le16_to_cpu(cb->src_port), le16_to_cpu(cb->dst_port));
	}
}

static void dn_ack(struct sock *sk, struct sk_buff *skb, unsigned short ack)
{
	struct dn_scp *scp = DN_SK(sk);
	unsigned short type = ((ack >> 12) & 0x0003);
	int wakeup = 0;

	switch (type) {
	case 0: 
		if (dn_after(ack, scp->ackrcv_dat)) {
			scp->ackrcv_dat = ack & 0x0fff;
			wakeup |= dn_nsp_check_xmit_queue(sk, skb,
							  &scp->data_xmit_queue,
							  ack);
		}
		break;
	case 1: 
		break;
	case 2: 
		if (dn_after(ack, scp->ackrcv_oth)) {
			scp->ackrcv_oth = ack & 0x0fff;
			wakeup |= dn_nsp_check_xmit_queue(sk, skb,
							  &scp->other_xmit_queue,
							  ack);
		}
		break;
	case 3: 
		break;
	}

	if (wakeup && !sock_flag(sk, SOCK_DEAD))
		sk->sk_state_change(sk);
}

static int dn_process_ack(struct sock *sk, struct sk_buff *skb, int oth)
{
	__le16 *ptr = (__le16 *)skb->data;
	int len = 0;
	unsigned short ack;

	if (skb->len < 2)
		return len;

	if ((ack = le16_to_cpu(*ptr)) & 0x8000) {
		skb_pull(skb, 2);
		ptr++;
		len += 2;
		if ((ack & 0x4000) == 0) {
			if (oth)
				ack ^= 0x2000;
			dn_ack(sk, skb, ack);
		}
	}

	if (skb->len < 2)
		return len;

	if ((ack = le16_to_cpu(*ptr)) & 0x8000) {
		skb_pull(skb, 2);
		len += 2;
		if ((ack & 0x4000) == 0) {
			if (oth)
				ack ^= 0x2000;
			dn_ack(sk, skb, ack);
		}
	}

	return len;
}


static inline int dn_check_idf(unsigned char **pptr, int *len, unsigned char max, unsigned char follow_on)
{
	unsigned char *ptr = *pptr;
	unsigned char flen = *ptr++;

	(*len)--;
	if (flen > max)
		return -1;
	if ((flen + follow_on) > *len)
		return -1;

	*len -= flen;
	*pptr = ptr + flen;
	return 0;
}

static struct {
	unsigned short reason;
	const char *text;
} ci_err_table[] = {
 { 0,             "CI: Truncated message" },
 { NSP_REASON_ID, "CI: Destination username error" },
 { NSP_REASON_ID, "CI: Destination username type" },
 { NSP_REASON_US, "CI: Source username error" },
 { 0,             "CI: Truncated at menuver" },
 { 0,             "CI: Truncated before access or user data" },
 { NSP_REASON_IO, "CI: Access data format error" },
 { NSP_REASON_IO, "CI: User data format error" }
};

static struct sock *dn_find_listener(struct sk_buff *skb, unsigned short *reason)
{
	struct dn_skb_cb *cb = DN_SKB_CB(skb);
	struct nsp_conn_init_msg *msg = (struct nsp_conn_init_msg *)skb->data;
	struct sockaddr_dn dstaddr;
	struct sockaddr_dn srcaddr;
	unsigned char type = 0;
	int dstlen;
	int srclen;
	unsigned char *ptr;
	int len;
	int err = 0;
	unsigned char menuver;

	memset(&dstaddr, 0, sizeof(struct sockaddr_dn));
	memset(&srcaddr, 0, sizeof(struct sockaddr_dn));

	cb->src_port = msg->srcaddr;
	cb->dst_port = msg->dstaddr;
	cb->services = msg->services;
	cb->info     = msg->info;
	cb->segsize  = le16_to_cpu(msg->segsize);

	if (!pskb_may_pull(skb, sizeof(*msg)))
		goto err_out;

	skb_pull(skb, sizeof(*msg));

	len = skb->len;
	ptr = skb->data;

	dstlen = dn_username2sockaddr(ptr, len, &dstaddr, &type);
	err++;
	if (dstlen < 0)
		goto err_out;

	err++;
	if (type > 1)
		goto err_out;

	len -= dstlen;
	ptr += dstlen;

	srclen = dn_username2sockaddr(ptr, len, &srcaddr, &type);
	err++;
	if (srclen < 0)
		goto err_out;

	len -= srclen;
	ptr += srclen;
	err++;
	if (len < 1)
		goto err_out;

	menuver = *ptr;
	ptr++;
	len--;

	err++;
	if ((menuver & (DN_MENUVER_ACC | DN_MENUVER_USR)) && (len < 1))
		goto err_out;

	err++;
	if (menuver & DN_MENUVER_ACC) {
		if (dn_check_idf(&ptr, &len, 39, 1))
			goto err_out;
		if (dn_check_idf(&ptr, &len, 39, 1))
			goto err_out;
		if (dn_check_idf(&ptr, &len, 39, (menuver & DN_MENUVER_USR) ? 1 : 0))
			goto err_out;
	}

	err++;
	if (menuver & DN_MENUVER_USR) {
		if (dn_check_idf(&ptr, &len, 16, 0))
			goto err_out;
	}

	return dn_sklist_find_listener(&dstaddr);
err_out:
	dn_log_martian(skb, ci_err_table[err].text);
	*reason = ci_err_table[err].reason;
	return NULL;
}


static void dn_nsp_conn_init(struct sock *sk, struct sk_buff *skb)
{
	if (sk_acceptq_is_full(sk)) {
		kfree_skb(skb);
		return;
	}

	sk->sk_ack_backlog++;
	skb_queue_tail(&sk->sk_receive_queue, skb);
	sk->sk_state_change(sk);
}

static void dn_nsp_conn_conf(struct sock *sk, struct sk_buff *skb)
{
	struct dn_skb_cb *cb = DN_SKB_CB(skb);
	struct dn_scp *scp = DN_SK(sk);
	unsigned char *ptr;

	if (skb->len < 4)
		goto out;

	ptr = skb->data;
	cb->services = *ptr++;
	cb->info = *ptr++;
	cb->segsize = le16_to_cpu(*(__le16 *)ptr);

	if ((scp->state == DN_CI) || (scp->state == DN_CD)) {
		scp->persist = 0;
		scp->addrrem = cb->src_port;
		sk->sk_state = TCP_ESTABLISHED;
		scp->state = DN_RUN;
		scp->services_rem = cb->services;
		scp->info_rem = cb->info;
		scp->segsize_rem = cb->segsize;

		if ((scp->services_rem & NSP_FC_MASK) == NSP_FC_NONE)
			scp->max_window = decnet_no_fc_max_cwnd;

		if (skb->len > 0) {
			u16 dlen = *skb->data;
			if ((dlen <= 16) && (dlen <= skb->len)) {
				scp->conndata_in.opt_optl = cpu_to_le16(dlen);
				skb_copy_from_linear_data_offset(skb, 1,
					      scp->conndata_in.opt_data, dlen);
			}
		}
		dn_nsp_send_link(sk, DN_NOCHANGE, 0);
		if (!sock_flag(sk, SOCK_DEAD))
			sk->sk_state_change(sk);
	}

out:
	kfree_skb(skb);
}

static void dn_nsp_conn_ack(struct sock *sk, struct sk_buff *skb)
{
	struct dn_scp *scp = DN_SK(sk);

	if (scp->state == DN_CI) {
		scp->state = DN_CD;
		scp->persist = 0;
	}

	kfree_skb(skb);
}

static void dn_nsp_disc_init(struct sock *sk, struct sk_buff *skb)
{
	struct dn_scp *scp = DN_SK(sk);
	struct dn_skb_cb *cb = DN_SKB_CB(skb);
	unsigned short reason;

	if (skb->len < 2)
		goto out;

	reason = le16_to_cpu(*(__le16 *)skb->data);
	skb_pull(skb, 2);

	scp->discdata_in.opt_status = cpu_to_le16(reason);
	scp->discdata_in.opt_optl   = 0;
	memset(scp->discdata_in.opt_data, 0, 16);

	if (skb->len > 0) {
		u16 dlen = *skb->data;
		if ((dlen <= 16) && (dlen <= skb->len)) {
			scp->discdata_in.opt_optl = cpu_to_le16(dlen);
			skb_copy_from_linear_data_offset(skb, 1, scp->discdata_in.opt_data, dlen);
		}
	}

	scp->addrrem = cb->src_port;
	sk->sk_state = TCP_CLOSE;

	switch (scp->state) {
	case DN_CI:
	case DN_CD:
		scp->state = DN_RJ;
		sk->sk_err = ECONNREFUSED;
		break;
	case DN_RUN:
		sk->sk_shutdown |= SHUTDOWN_MASK;
		scp->state = DN_DN;
		break;
	case DN_DI:
		scp->state = DN_DIC;
		break;
	}

	if (!sock_flag(sk, SOCK_DEAD)) {
		if (sk->sk_socket->state != SS_UNCONNECTED)
			sk->sk_socket->state = SS_DISCONNECTING;
		sk->sk_state_change(sk);
	}

	if (scp->addrrem) {
		dn_nsp_send_disc(sk, NSP_DISCCONF, NSP_REASON_DC, GFP_ATOMIC);
	}
	scp->persist_fxn = dn_destroy_timer;
	scp->persist = dn_nsp_persist(sk);

out:
	kfree_skb(skb);
}

static void dn_nsp_disc_conf(struct sock *sk, struct sk_buff *skb)
{
	struct dn_scp *scp = DN_SK(sk);
	unsigned short reason;

	if (skb->len != 2)
		goto out;

	reason = le16_to_cpu(*(__le16 *)skb->data);

	sk->sk_state = TCP_CLOSE;

	switch (scp->state) {
	case DN_CI:
		scp->state = DN_NR;
		break;
	case DN_DR:
		if (reason == NSP_REASON_DC)
			scp->state = DN_DRC;
		if (reason == NSP_REASON_NL)
			scp->state = DN_CN;
		break;
	case DN_DI:
		scp->state = DN_DIC;
		break;
	case DN_RUN:
		sk->sk_shutdown |= SHUTDOWN_MASK;
	case DN_CC:
		scp->state = DN_CN;
	}

	if (!sock_flag(sk, SOCK_DEAD)) {
		if (sk->sk_socket->state != SS_UNCONNECTED)
			sk->sk_socket->state = SS_DISCONNECTING;
		sk->sk_state_change(sk);
	}

	scp->persist_fxn = dn_destroy_timer;
	scp->persist = dn_nsp_persist(sk);

out:
	kfree_skb(skb);
}

static void dn_nsp_linkservice(struct sock *sk, struct sk_buff *skb)
{
	struct dn_scp *scp = DN_SK(sk);
	unsigned short segnum;
	unsigned char lsflags;
	signed char fcval;
	int wake_up = 0;
	char *ptr = skb->data;
	unsigned char fctype = scp->services_rem & NSP_FC_MASK;

	if (skb->len != 4)
		goto out;

	segnum = le16_to_cpu(*(__le16 *)ptr);
	ptr += 2;
	lsflags = *(unsigned char *)ptr++;
	fcval = *ptr;

	if (lsflags & 0xf8)
		goto out;

	if (seq_next(scp->numoth_rcv, segnum)) {
		seq_add(&scp->numoth_rcv, 1);
		switch(lsflags & 0x04) { 
		case 0x00: 
			switch(lsflags & 0x03) { 
			case 0x00: 
				if (fcval < 0) {
					unsigned char p_fcval = -fcval;
					if ((scp->flowrem_dat > p_fcval) &&
					    (fctype == NSP_FC_SCMC)) {
						scp->flowrem_dat -= p_fcval;
					}
				} else if (fcval > 0) {
					scp->flowrem_dat += fcval;
					wake_up = 1;
				}
				break;
			case 0x01: 
				scp->flowrem_sw = DN_DONTSEND;
				break;
			case 0x02: 
				scp->flowrem_sw = DN_SEND;
				dn_nsp_output(sk);
				wake_up = 1;
			}
			break;
		case 0x04: 
			if (fcval > 0) {
				scp->flowrem_oth += fcval;
				wake_up = 1;
			}
			break;
		}
		if (wake_up && !sock_flag(sk, SOCK_DEAD))
			sk->sk_state_change(sk);
	}

	dn_nsp_send_oth_ack(sk);

out:
	kfree_skb(skb);
}

static __inline__ int dn_queue_skb(struct sock *sk, struct sk_buff *skb, int sig, struct sk_buff_head *queue)
{
	int err;
	int skb_len;

	if (atomic_read(&sk->sk_rmem_alloc) + skb->truesize >=
	    (unsigned)sk->sk_rcvbuf) {
		err = -ENOMEM;
		goto out;
	}

	err = sk_filter(sk, skb);
	if (err)
		goto out;

	skb_len = skb->len;
	skb_set_owner_r(skb, sk);
	skb_queue_tail(queue, skb);

	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_data_ready(sk, skb_len);
out:
	return err;
}

static void dn_nsp_otherdata(struct sock *sk, struct sk_buff *skb)
{
	struct dn_scp *scp = DN_SK(sk);
	unsigned short segnum;
	struct dn_skb_cb *cb = DN_SKB_CB(skb);
	int queued = 0;

	if (skb->len < 2)
		goto out;

	cb->segnum = segnum = le16_to_cpu(*(__le16 *)skb->data);
	skb_pull(skb, 2);

	if (seq_next(scp->numoth_rcv, segnum)) {

		if (dn_queue_skb(sk, skb, SIGURG, &scp->other_receive_queue) == 0) {
			seq_add(&scp->numoth_rcv, 1);
			scp->other_report = 0;
			queued = 1;
		}
	}

	dn_nsp_send_oth_ack(sk);
out:
	if (!queued)
		kfree_skb(skb);
}

static void dn_nsp_data(struct sock *sk, struct sk_buff *skb)
{
	int queued = 0;
	unsigned short segnum;
	struct dn_skb_cb *cb = DN_SKB_CB(skb);
	struct dn_scp *scp = DN_SK(sk);

	if (skb->len < 2)
		goto out;

	cb->segnum = segnum = le16_to_cpu(*(__le16 *)skb->data);
	skb_pull(skb, 2);

	if (seq_next(scp->numdat_rcv, segnum)) {
		if (dn_queue_skb(sk, skb, SIGIO, &sk->sk_receive_queue) == 0) {
			seq_add(&scp->numdat_rcv, 1);
			queued = 1;
		}

		if ((scp->flowloc_sw == DN_SEND) && dn_congested(sk)) {
			scp->flowloc_sw = DN_DONTSEND;
			dn_nsp_send_link(sk, DN_DONTSEND, 0);
		}
	}

	dn_nsp_send_data_ack(sk);
out:
	if (!queued)
		kfree_skb(skb);
}

static void dn_returned_conn_init(struct sock *sk, struct sk_buff *skb)
{
	struct dn_scp *scp = DN_SK(sk);

	if (scp->state == DN_CI) {
		scp->state = DN_NC;
		sk->sk_state = TCP_CLOSE;
		if (!sock_flag(sk, SOCK_DEAD))
			sk->sk_state_change(sk);
	}

	kfree_skb(skb);
}

static int dn_nsp_no_socket(struct sk_buff *skb, unsigned short reason)
{
	struct dn_skb_cb *cb = DN_SKB_CB(skb);
	int ret = NET_RX_DROP;

	
	if (cb->rt_flags & DN_RT_F_RTS)
		goto out;

	if ((reason != NSP_REASON_OK) && ((cb->nsp_flags & 0x0c) == 0x08)) {
		switch (cb->nsp_flags & 0x70) {
		case 0x10:
		case 0x60: 
			dn_nsp_return_disc(skb, NSP_DISCINIT, reason);
			ret = NET_RX_SUCCESS;
			break;
		case 0x20: 
			dn_nsp_return_disc(skb, NSP_DISCCONF, reason);
			ret = NET_RX_SUCCESS;
			break;
		}
	}

out:
	kfree_skb(skb);
	return ret;
}

static int dn_nsp_rx_packet(struct sk_buff *skb)
{
	struct dn_skb_cb *cb = DN_SKB_CB(skb);
	struct sock *sk = NULL;
	unsigned char *ptr = (unsigned char *)skb->data;
	unsigned short reason = NSP_REASON_NL;

	if (!pskb_may_pull(skb, 2))
		goto free_out;

	skb_reset_transport_header(skb);
	cb->nsp_flags = *ptr++;

	if (decnet_debug_level & 2)
		printk(KERN_DEBUG "dn_nsp_rx: Message type 0x%02x\n", (int)cb->nsp_flags);

	if (cb->nsp_flags & 0x83)
		goto free_out;

	if ((cb->nsp_flags & 0x0c) == 0x08) {
		switch (cb->nsp_flags & 0x70) {
		case 0x00: 
		case 0x70: 
		case 0x50: 
			goto free_out;
		case 0x10:
		case 0x60:
			if (unlikely(cb->rt_flags & DN_RT_F_RTS))
				goto free_out;
			sk = dn_find_listener(skb, &reason);
			goto got_it;
		}
	}

	if (!pskb_may_pull(skb, 3))
		goto free_out;

	cb->dst_port = *(__le16 *)ptr;
	cb->src_port = 0;
	ptr += 2;

	if (pskb_may_pull(skb, 5)) {
		cb->src_port = *(__le16 *)ptr;
		ptr += 2;
		skb_pull(skb, 5);
	}

	if (unlikely(cb->rt_flags & DN_RT_F_RTS)) {
		__le16 tmp = cb->dst_port;
		cb->dst_port = cb->src_port;
		cb->src_port = tmp;
		tmp = cb->dst;
		cb->dst = cb->src;
		cb->src = tmp;
	}

	sk = dn_find_by_skb(skb);
got_it:
	if (sk != NULL) {
		struct dn_scp *scp = DN_SK(sk);

		
		scp->nsp_rxtshift = 0;

		if (cb->nsp_flags & ~0x60) {
			if (unlikely(skb_linearize(skb)))
				goto free_out;
		}

		return sk_receive_skb(sk, skb, 0);
	}

	return dn_nsp_no_socket(skb, reason);

free_out:
	kfree_skb(skb);
	return NET_RX_DROP;
}

int dn_nsp_rx(struct sk_buff *skb)
{
	return NF_HOOK(NFPROTO_DECNET, NF_DN_LOCAL_IN, skb, skb->dev, NULL,
		       dn_nsp_rx_packet);
}

int dn_nsp_backlog_rcv(struct sock *sk, struct sk_buff *skb)
{
	struct dn_scp *scp = DN_SK(sk);
	struct dn_skb_cb *cb = DN_SKB_CB(skb);

	if (cb->rt_flags & DN_RT_F_RTS) {
		if (cb->nsp_flags == 0x18 || cb->nsp_flags == 0x68)
			dn_returned_conn_init(sk, skb);
		else
			kfree_skb(skb);
		return NET_RX_SUCCESS;
	}

	if ((cb->nsp_flags & 0x0c) == 0x08) {
		switch (cb->nsp_flags & 0x70) {
		case 0x10:
		case 0x60:
			dn_nsp_conn_init(sk, skb);
			break;
		case 0x20:
			dn_nsp_conn_conf(sk, skb);
			break;
		case 0x30:
			dn_nsp_disc_init(sk, skb);
			break;
		case 0x40:
			dn_nsp_disc_conf(sk, skb);
			break;
		}

	} else if (cb->nsp_flags == 0x24) {
		dn_nsp_conn_ack(sk, skb);
	} else {
		int other = 1;

		
		if ((scp->state == DN_CC) && !sock_flag(sk, SOCK_DEAD)) {
			scp->state = DN_RUN;
			sk->sk_state = TCP_ESTABLISHED;
			sk->sk_state_change(sk);
		}

		if ((cb->nsp_flags & 0x1c) == 0)
			other = 0;
		if (cb->nsp_flags == 0x04)
			other = 0;

		dn_process_ack(sk, skb, other);

		if ((cb->nsp_flags & 0x0c) == 0) {

			if (scp->state != DN_RUN)
				goto free_out;

			switch (cb->nsp_flags) {
			case 0x10: 
				dn_nsp_linkservice(sk, skb);
				break;
			case 0x30: 
				dn_nsp_otherdata(sk, skb);
				break;
			default:
				dn_nsp_data(sk, skb);
			}

		} else { 
free_out:
			kfree_skb(skb);
		}
	}

	return NET_RX_SUCCESS;
}

