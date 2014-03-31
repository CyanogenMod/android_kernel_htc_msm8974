/*

 * l1oip.c  low level driver for tunneling layer 1 over IP
 *
 * NOTE: It is not compatible with TDMoIP nor "ISDN over IP".
 *
 * Author	Andreas Eversberg (jolly@eversberg.eu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#define L1OIP_VERSION	0	

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mISDNif.h>
#include <linux/mISDNhw.h>
#include <linux/mISDNdsp.h>
#include <linux/init.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <net/sock.h>
#include "core.h"
#include "l1oip.h"

static const char *l1oip_revision = "2.00";

static int l1oip_cnt;
static spinlock_t l1oip_lock;
static struct list_head l1oip_ilist;

#define MAX_CARDS	16
static u_int type[MAX_CARDS];
static u_int codec[MAX_CARDS];
static u_int ip[MAX_CARDS * 4];
static u_int port[MAX_CARDS];
static u_int remoteport[MAX_CARDS];
static u_int ondemand[MAX_CARDS];
static u_int limit[MAX_CARDS];
static u_int id[MAX_CARDS];
static int debug;
static int ulaw;

MODULE_AUTHOR("Andreas Eversberg");
MODULE_LICENSE("GPL");
module_param_array(type, uint, NULL, S_IRUGO | S_IWUSR);
module_param_array(codec, uint, NULL, S_IRUGO | S_IWUSR);
module_param_array(ip, uint, NULL, S_IRUGO | S_IWUSR);
module_param_array(port, uint, NULL, S_IRUGO | S_IWUSR);
module_param_array(remoteport, uint, NULL, S_IRUGO | S_IWUSR);
module_param_array(ondemand, uint, NULL, S_IRUGO | S_IWUSR);
module_param_array(limit, uint, NULL, S_IRUGO | S_IWUSR);
module_param_array(id, uint, NULL, S_IRUGO | S_IWUSR);
module_param(ulaw, uint, S_IRUGO | S_IWUSR);
module_param(debug, uint, S_IRUGO | S_IWUSR);

static int
l1oip_socket_send(struct l1oip *hc, u8 localcodec, u8 channel, u32 chanmask,
		  u16 timebase, u8 *buf, int len)
{
	u8 *p;
	int multi = 0;
	u8 frame[len + 32];
	struct socket *socket = NULL;

	if (debug & DEBUG_L1OIP_MSG)
		printk(KERN_DEBUG "%s: sending data to socket (len = %d)\n",
		       __func__, len);

	p = frame;

	
	if ((int)(hc->keep_tl.expires-jiffies) < 5 * HZ) {
		del_timer(&hc->keep_tl);
		hc->keep_tl.expires = jiffies + L1OIP_KEEPALIVE * HZ;
		add_timer(&hc->keep_tl);
	} else
		hc->keep_tl.expires = jiffies + L1OIP_KEEPALIVE * HZ;

	if (debug & DEBUG_L1OIP_MSG)
		printk(KERN_DEBUG "%s: resetting timer\n", __func__);

	
	if (!hc->sin_remote.sin_addr.s_addr || !hc->sin_remote.sin_port) {
		if (debug & DEBUG_L1OIP_MSG)
			printk(KERN_DEBUG "%s: dropping frame, because remote "
			       "IP is not set.\n", __func__);
		return len;
	}

	
	*p++ = (L1OIP_VERSION << 6) 
		| (hc->pri ? 0x20 : 0x00) 
		| (hc->id ? 0x10 : 0x00) 
		| localcodec;
	if (hc->id) {
		*p++ = hc->id >> 24; 
		*p++ = hc->id >> 16;
		*p++ = hc->id >> 8;
		*p++ = hc->id;
	}
	*p++ = (multi == 1) ? 0x80 : 0x00 + channel; 
	if (multi == 1)
		*p++ = len; 
	*p++ = timebase >> 8; 
	*p++ = timebase;

	if (buf && len) { 
		if (localcodec == 1 && ulaw)
			l1oip_ulaw_to_alaw(buf, len, p);
		else if (localcodec == 2 && !ulaw)
			l1oip_alaw_to_ulaw(buf, len, p);
		else if (localcodec == 3)
			len = l1oip_law_to_4bit(buf, len, p,
						&hc->chan[channel].codecstate);
		else
			memcpy(p, buf, len);
	}
	len += p - frame;

	
	spin_lock(&hc->socket_lock);
	if (!hc->socket) {
		spin_unlock(&hc->socket_lock);
		return 0;
	}
	
	socket = hc->socket;
	hc->socket = NULL;
	spin_unlock(&hc->socket_lock);
	
	if (debug & DEBUG_L1OIP_MSG)
		printk(KERN_DEBUG "%s: sending packet to socket (len "
		       "= %d)\n", __func__, len);
	hc->sendiov.iov_base = frame;
	hc->sendiov.iov_len  = len;
	len = kernel_sendmsg(socket, &hc->sendmsg, &hc->sendiov, 1, len);
	
	hc->socket = socket; 

	return len;
}


static void
l1oip_socket_recv(struct l1oip *hc, u8 remotecodec, u8 channel, u16 timebase,
		  u8 *buf, int len)
{
	struct sk_buff *nskb;
	struct bchannel *bch;
	struct dchannel *dch;
	u8 *p;
	u32 rx_counter;

	if (len == 0) {
		if (debug & DEBUG_L1OIP_MSG)
			printk(KERN_DEBUG "%s: received empty keepalive data, "
			       "ignoring\n", __func__);
		return;
	}

	if (debug & DEBUG_L1OIP_MSG)
		printk(KERN_DEBUG "%s: received data, sending to mISDN (%d)\n",
		       __func__, len);

	if (channel < 1 || channel > 127) {
		printk(KERN_WARNING "%s: packet error - channel %d out of "
		       "range\n", __func__, channel);
		return;
	}
	dch = hc->chan[channel].dch;
	bch = hc->chan[channel].bch;
	if (!dch && !bch) {
		printk(KERN_WARNING "%s: packet error - channel %d not in "
		       "stack\n", __func__, channel);
		return;
	}

	
	nskb = mI_alloc_skb((remotecodec == 3) ? (len << 1) : len, GFP_ATOMIC);
	if (!nskb) {
		printk(KERN_ERR "%s: No mem for skb.\n", __func__);
		return;
	}
	p = skb_put(nskb, (remotecodec == 3) ? (len << 1) : len);

	if (remotecodec == 1 && ulaw)
		l1oip_alaw_to_ulaw(buf, len, p);
	else if (remotecodec == 2 && !ulaw)
		l1oip_ulaw_to_alaw(buf, len, p);
	else if (remotecodec == 3)
		len = l1oip_4bit_to_law(buf, len, p);
	else
		memcpy(p, buf, len);

	
	if (dch && len >= 2) {
		dch->rx_skb = nskb;
		recv_Dchannel(dch);
	}
	if (bch) {
		
		rx_counter = hc->chan[channel].rx_counter;
		if (((s16)(timebase - rx_counter)) >= 0) {
			
			if (timebase >= (rx_counter & 0xffff))
				rx_counter =
					(rx_counter & 0xffff0000) | timebase;
			else
				rx_counter = ((rx_counter & 0xffff0000) + 0x10000)
					| timebase;
		} else {
			
			if (timebase < (rx_counter & 0xffff))
				rx_counter =
					(rx_counter & 0xffff0000) | timebase;
			else
				rx_counter = ((rx_counter & 0xffff0000) - 0x10000)
					| timebase;
		}
		hc->chan[channel].rx_counter = rx_counter;

#ifdef REORDER_DEBUG
		if (hc->chan[channel].disorder_flag) {
			struct sk_buff *skb;
			int cnt;
			skb = hc->chan[channel].disorder_skb;
			hc->chan[channel].disorder_skb = nskb;
			nskb = skb;
			cnt = hc->chan[channel].disorder_cnt;
			hc->chan[channel].disorder_cnt = rx_counter;
			rx_counter = cnt;
		}
		hc->chan[channel].disorder_flag ^= 1;
		if (nskb)
#endif
			queue_ch_frame(&bch->ch, PH_DATA_IND, rx_counter, nskb);
	}
}


static void
l1oip_socket_parse(struct l1oip *hc, struct sockaddr_in *sin, u8 *buf, int len)
{
	u32			packet_id;
	u8			channel;
	u8			remotecodec;
	u16			timebase;
	int			m, mlen;
	int			len_start = len; 
	struct dchannel		*dch = hc->chan[hc->d_idx].dch;

	if (debug & DEBUG_L1OIP_MSG)
		printk(KERN_DEBUG "%s: received frame, parsing... (%d)\n",
		       __func__, len);

	
	if (len < 1 + 1 + 2) {
		printk(KERN_WARNING "%s: packet error - length %d below "
		       "4 bytes\n", __func__, len);
		return;
	}

	
	if (((*buf) >> 6) != L1OIP_VERSION) {
		printk(KERN_WARNING "%s: packet error - unknown version %d\n",
		       __func__, buf[0]>>6);
		return;
	}

	
	if (((*buf) & 0x20) && !hc->pri) {
		printk(KERN_WARNING "%s: packet error - received E1 packet "
		       "on S0 interface\n", __func__);
		return;
	}
	if (!((*buf) & 0x20) && hc->pri) {
		printk(KERN_WARNING "%s: packet error - received S0 packet "
		       "on E1 interface\n", __func__);
		return;
	}

	
	packet_id = (*buf >> 4) & 1;

	
	remotecodec = (*buf) & 0x0f;
	if (remotecodec > 3) {
		printk(KERN_WARNING "%s: packet error - remotecodec %d "
		       "unsupported\n", __func__, remotecodec);
		return;
	}
	buf++;
	len--;

	
	if (packet_id) {
		if (!hc->id) {
			printk(KERN_WARNING "%s: packet error - packet has id "
			       "0x%x, but we have not\n", __func__, packet_id);
			return;
		}
		if (len < 4) {
			printk(KERN_WARNING "%s: packet error - packet too "
			       "short for ID value\n", __func__);
			return;
		}
		packet_id = (*buf++) << 24;
		packet_id += (*buf++) << 16;
		packet_id += (*buf++) << 8;
		packet_id += (*buf++);
		len -= 4;

		if (packet_id != hc->id) {
			printk(KERN_WARNING "%s: packet error - ID mismatch, "
			       "got 0x%x, we 0x%x\n",
			       __func__, packet_id, hc->id);
			return;
		}
	} else {
		if (hc->id) {
			printk(KERN_WARNING "%s: packet error - packet has no "
			       "ID, but we have\n", __func__);
			return;
		}
	}

multiframe:
	if (len < 1) {
		printk(KERN_WARNING "%s: packet error - packet too short, "
		       "channel expected at position %d.\n",
		       __func__, len-len_start + 1);
		return;
	}

	
	channel = *buf & 0x7f;
	m = *buf >> 7;
	buf++;
	len--;

	
	if (m) {
		if (len < 1) {
			printk(KERN_WARNING "%s: packet error - packet too "
			       "short, length expected at position %d.\n",
			       __func__, len_start - len - 1);
			return;
		}

		mlen = *buf++;
		len--;
		if (mlen == 0)
			mlen = 256;
		if (len < mlen + 3) {
			printk(KERN_WARNING "%s: packet error - length %d at "
			       "position %d exceeds total length %d.\n",
			       __func__, mlen, len_start-len - 1, len_start);
			return;
		}
		if (len == mlen + 3) {
			printk(KERN_WARNING "%s: packet error - length %d at "
			       "position %d will not allow additional "
			       "packet.\n",
			       __func__, mlen, len_start-len + 1);
			return;
		}
	} else
		mlen = len - 2; 

	if (len < 2) {
		printk(KERN_WARNING "%s: packet error - packet too short, time "
		       "base expected at position %d.\n",
		       __func__, len-len_start + 1);
		return;
	}

	
	timebase = (*buf++) << 8;
	timebase |= (*buf++);
	len -= 2;

	
	if (!test_bit(FLG_ACTIVE, &dch->Flags)) {
		if (debug & (DEBUG_L1OIP_MSG | DEBUG_L1OIP_SOCKET))
			printk(KERN_DEBUG "%s: interface become active due to "
			       "received packet\n", __func__);
		test_and_set_bit(FLG_ACTIVE, &dch->Flags);
		_queue_data(&dch->dev.D, PH_ACTIVATE_IND, MISDN_ID_ANY, 0,
			    NULL, GFP_ATOMIC);
	}

	
	l1oip_socket_recv(hc, remotecodec, channel, timebase, buf, mlen);
	buf += mlen;
	len -= mlen;

	
	if (m)
		goto multiframe;

	
	if ((int)(hc->timeout_tl.expires-jiffies) < 5 * HZ || !hc->timeout_on) {
		hc->timeout_on = 1;
		del_timer(&hc->timeout_tl);
		hc->timeout_tl.expires = jiffies + L1OIP_TIMEOUT * HZ;
		add_timer(&hc->timeout_tl);
	} else 
		hc->timeout_tl.expires = jiffies + L1OIP_TIMEOUT * HZ;

	
	if ((hc->sin_remote.sin_addr.s_addr != sin->sin_addr.s_addr)
	    || (hc->sin_remote.sin_port != sin->sin_port)) {
		if (debug & DEBUG_L1OIP_SOCKET)
			printk(KERN_DEBUG "%s: remote address changes from "
			       "0x%08x to 0x%08x (port %d to %d)\n", __func__,
			       ntohl(hc->sin_remote.sin_addr.s_addr),
			       ntohl(sin->sin_addr.s_addr),
			       ntohs(hc->sin_remote.sin_port),
			       ntohs(sin->sin_port));
		hc->sin_remote.sin_addr.s_addr = sin->sin_addr.s_addr;
		hc->sin_remote.sin_port = sin->sin_port;
	}
}


static int
l1oip_socket_thread(void *data)
{
	struct l1oip *hc = (struct l1oip *)data;
	int ret = 0;
	struct msghdr msg;
	struct sockaddr_in sin_rx;
	unsigned char *recvbuf;
	size_t recvbuf_size = 1500;
	int recvlen;
	struct socket *socket = NULL;
	DECLARE_COMPLETION_ONSTACK(wait);

	
	recvbuf = kmalloc(recvbuf_size, GFP_KERNEL);
	if (!recvbuf) {
		printk(KERN_ERR "%s: Failed to alloc recvbuf.\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	
	allow_signal(SIGTERM);

	
	if (sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &socket)) {
		printk(KERN_ERR "%s: Failed to create socket.\n", __func__);
		ret = -EIO;
		goto fail;
	}

	
	hc->sin_local.sin_family = AF_INET;
	hc->sin_local.sin_addr.s_addr = INADDR_ANY;
	hc->sin_local.sin_port = htons((unsigned short)hc->localport);

	
	hc->sin_remote.sin_family = AF_INET;
	hc->sin_remote.sin_addr.s_addr = htonl(hc->remoteip);
	hc->sin_remote.sin_port = htons((unsigned short)hc->remoteport);

	
	if (socket->ops->bind(socket, (struct sockaddr *)&hc->sin_local,
			      sizeof(hc->sin_local))) {
		printk(KERN_ERR "%s: Failed to bind socket to port %d.\n",
		       __func__, hc->localport);
		ret = -EINVAL;
		goto fail;
	}

	
	if (socket->sk == NULL) {
		printk(KERN_ERR "%s: socket->sk == NULL\n", __func__);
		ret = -EIO;
		goto fail;
	}

	
	msg.msg_name = &sin_rx;
	msg.msg_namelen = sizeof(sin_rx);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	
	hc->sendmsg.msg_name = &hc->sin_remote;
	hc->sendmsg.msg_namelen = sizeof(hc->sin_remote);
	hc->sendmsg.msg_control = NULL;
	hc->sendmsg.msg_controllen = 0;

	
	spin_lock(&hc->socket_lock);
	hc->socket = socket;
	spin_unlock(&hc->socket_lock);

	
	if (debug & DEBUG_L1OIP_SOCKET)
		printk(KERN_DEBUG "%s: socket created and open\n",
		       __func__);
	while (!signal_pending(current)) {
		struct kvec iov = {
			.iov_base = recvbuf,
			.iov_len = recvbuf_size,
		};
		recvlen = kernel_recvmsg(socket, &msg, &iov, 1,
					 recvbuf_size, 0);
		if (recvlen > 0) {
			l1oip_socket_parse(hc, &sin_rx, recvbuf, recvlen);
		} else {
			if (debug & DEBUG_L1OIP_SOCKET)
				printk(KERN_WARNING
				       "%s: broken pipe on socket\n", __func__);
		}
	}

	
	spin_lock(&hc->socket_lock);
	
	while (!hc->socket) {
		spin_unlock(&hc->socket_lock);
		schedule_timeout(HZ / 10);
		spin_lock(&hc->socket_lock);
	}
	hc->socket = NULL;
	spin_unlock(&hc->socket_lock);

	if (debug & DEBUG_L1OIP_SOCKET)
		printk(KERN_DEBUG "%s: socket thread terminating\n",
		       __func__);

fail:
	
	kfree(recvbuf);

	
	if (socket)
		sock_release(socket);

	
	complete(&hc->socket_complete);
	hc->socket_thread = NULL; 

	if (debug & DEBUG_L1OIP_SOCKET)
		printk(KERN_DEBUG "%s: socket thread terminated\n",
		       __func__);
	return ret;
}

static void
l1oip_socket_close(struct l1oip *hc)
{
	struct dchannel *dch = hc->chan[hc->d_idx].dch;

	
	if (hc->socket_thread) {
		if (debug & DEBUG_L1OIP_SOCKET)
			printk(KERN_DEBUG "%s: socket thread exists, "
			       "killing...\n", __func__);
		send_sig(SIGTERM, hc->socket_thread, 0);
		wait_for_completion(&hc->socket_complete);
	}

	
	if (test_bit(FLG_ACTIVE, &dch->Flags)) {
		if (debug & (DEBUG_L1OIP_MSG | DEBUG_L1OIP_SOCKET))
			printk(KERN_DEBUG "%s: interface become deactivated "
			       "due to timeout\n", __func__);
		test_and_clear_bit(FLG_ACTIVE, &dch->Flags);
		_queue_data(&dch->dev.D, PH_DEACTIVATE_IND, MISDN_ID_ANY, 0,
			    NULL, GFP_ATOMIC);
	}
}

static int
l1oip_socket_open(struct l1oip *hc)
{
	
	l1oip_socket_close(hc);

	init_completion(&hc->socket_complete);

	
	hc->socket_thread = kthread_run(l1oip_socket_thread, hc, "l1oip_%s",
					hc->name);
	if (IS_ERR(hc->socket_thread)) {
		int err = PTR_ERR(hc->socket_thread);
		printk(KERN_ERR "%s: Failed (%d) to create socket process.\n",
		       __func__, err);
		hc->socket_thread = NULL;
		sock_release(hc->socket);
		return err;
	}
	if (debug & DEBUG_L1OIP_SOCKET)
		printk(KERN_DEBUG "%s: socket thread created\n", __func__);

	return 0;
}


static void
l1oip_send_bh(struct work_struct *work)
{
	struct l1oip *hc = container_of(work, struct l1oip, workq);

	if (debug & (DEBUG_L1OIP_MSG | DEBUG_L1OIP_SOCKET))
		printk(KERN_DEBUG "%s: keepalive timer expired, sending empty "
		       "frame on dchannel\n", __func__);

	
	l1oip_socket_send(hc, 0, hc->d_idx, 0, 0, NULL, 0);
}


static void
l1oip_keepalive(void *data)
{
	struct l1oip *hc = (struct l1oip *)data;

	schedule_work(&hc->workq);
}

static void
l1oip_timeout(void *data)
{
	struct l1oip			*hc = (struct l1oip *)data;
	struct dchannel		*dch = hc->chan[hc->d_idx].dch;

	if (debug & DEBUG_L1OIP_MSG)
		printk(KERN_DEBUG "%s: timeout timer expired, turn layer one "
		       "down.\n", __func__);

	hc->timeout_on = 0; 

	
	if (test_bit(FLG_ACTIVE, &dch->Flags)) {
		if (debug & (DEBUG_L1OIP_MSG | DEBUG_L1OIP_SOCKET))
			printk(KERN_DEBUG "%s: interface become deactivated "
			       "due to timeout\n", __func__);
		test_and_clear_bit(FLG_ACTIVE, &dch->Flags);
		_queue_data(&dch->dev.D, PH_DEACTIVATE_IND, MISDN_ID_ANY, 0,
			    NULL, GFP_ATOMIC);
	}

	
	if (hc->ondemand) {
		if (debug & DEBUG_L1OIP_MSG)
			printk(KERN_DEBUG "%s: on demand causes ip address to "
			       "be removed\n", __func__);
		hc->sin_remote.sin_addr.s_addr = 0;
	}
}


static int
handle_dmsg(struct mISDNchannel *ch, struct sk_buff *skb)
{
	struct mISDNdevice	*dev = container_of(ch, struct mISDNdevice, D);
	struct dchannel		*dch = container_of(dev, struct dchannel, dev);
	struct l1oip			*hc = dch->hw;
	struct mISDNhead	*hh = mISDN_HEAD_P(skb);
	int			ret = -EINVAL;
	int			l, ll;
	unsigned char		*p;

	switch (hh->prim) {
	case PH_DATA_REQ:
		if (skb->len < 1) {
			printk(KERN_WARNING "%s: skb too small\n",
			       __func__);
			break;
		}
		if (skb->len > MAX_DFRAME_LEN_L1 || skb->len > L1OIP_MAX_LEN) {
			printk(KERN_WARNING "%s: skb too large\n",
			       __func__);
			break;
		}
		
		p = skb->data;
		l = skb->len;
		while (l) {
			ll = (l < L1OIP_MAX_PERFRAME) ? l : L1OIP_MAX_PERFRAME;
			l1oip_socket_send(hc, 0, dch->slot, 0,
					  hc->chan[dch->slot].tx_counter++, p, ll);
			p += ll;
			l -= ll;
		}
		skb_trim(skb, 0);
		queue_ch_frame(ch, PH_DATA_CNF, hh->id, skb);
		return 0;
	case PH_ACTIVATE_REQ:
		if (debug & (DEBUG_L1OIP_MSG | DEBUG_L1OIP_SOCKET))
			printk(KERN_DEBUG "%s: PH_ACTIVATE channel %d (1..%d)\n"
			       , __func__, dch->slot, hc->b_num + 1);
		skb_trim(skb, 0);
		if (test_bit(FLG_ACTIVE, &dch->Flags))
			queue_ch_frame(ch, PH_ACTIVATE_IND, hh->id, skb);
		else
			queue_ch_frame(ch, PH_DEACTIVATE_IND, hh->id, skb);
		return 0;
	case PH_DEACTIVATE_REQ:
		if (debug & (DEBUG_L1OIP_MSG | DEBUG_L1OIP_SOCKET))
			printk(KERN_DEBUG "%s: PH_DEACTIVATE channel %d "
			       "(1..%d)\n", __func__, dch->slot,
			       hc->b_num + 1);
		skb_trim(skb, 0);
		if (test_bit(FLG_ACTIVE, &dch->Flags))
			queue_ch_frame(ch, PH_ACTIVATE_IND, hh->id, skb);
		else
			queue_ch_frame(ch, PH_DEACTIVATE_IND, hh->id, skb);
		return 0;
	}
	if (!ret)
		dev_kfree_skb(skb);
	return ret;
}

static int
channel_dctrl(struct dchannel *dch, struct mISDN_ctrl_req *cq)
{
	int	ret = 0;
	struct l1oip	*hc = dch->hw;

	switch (cq->op) {
	case MISDN_CTRL_GETOP:
		cq->op = MISDN_CTRL_SETPEER | MISDN_CTRL_UNSETPEER
			| MISDN_CTRL_GETPEER;
		break;
	case MISDN_CTRL_SETPEER:
		hc->remoteip = (u32)cq->p1;
		hc->remoteport = cq->p2 & 0xffff;
		hc->localport = cq->p2 >> 16;
		if (!hc->remoteport)
			hc->remoteport = hc->localport;
		if (debug & DEBUG_L1OIP_SOCKET)
			printk(KERN_DEBUG "%s: got new ip address from user "
			       "space.\n", __func__);
		l1oip_socket_open(hc);
		break;
	case MISDN_CTRL_UNSETPEER:
		if (debug & DEBUG_L1OIP_SOCKET)
			printk(KERN_DEBUG "%s: removing ip address.\n",
			       __func__);
		hc->remoteip = 0;
		l1oip_socket_open(hc);
		break;
	case MISDN_CTRL_GETPEER:
		if (debug & DEBUG_L1OIP_SOCKET)
			printk(KERN_DEBUG "%s: getting ip address.\n",
			       __func__);
		cq->p1 = hc->remoteip;
		cq->p2 = hc->remoteport | (hc->localport << 16);
		break;
	default:
		printk(KERN_WARNING "%s: unknown Op %x\n",
		       __func__, cq->op);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int
open_dchannel(struct l1oip *hc, struct dchannel *dch, struct channel_req *rq)
{
	if (debug & DEBUG_HW_OPEN)
		printk(KERN_DEBUG "%s: dev(%d) open from %p\n", __func__,
		       dch->dev.id, __builtin_return_address(0));
	if (rq->protocol == ISDN_P_NONE)
		return -EINVAL;
	if ((dch->dev.D.protocol != ISDN_P_NONE) &&
	    (dch->dev.D.protocol != rq->protocol)) {
		if (debug & DEBUG_HW_OPEN)
			printk(KERN_WARNING "%s: change protocol %x to %x\n",
			       __func__, dch->dev.D.protocol, rq->protocol);
	}
	if (dch->dev.D.protocol != rq->protocol)
		dch->dev.D.protocol = rq->protocol;

	if (test_bit(FLG_ACTIVE, &dch->Flags)) {
		_queue_data(&dch->dev.D, PH_ACTIVATE_IND, MISDN_ID_ANY,
			    0, NULL, GFP_KERNEL);
	}
	rq->ch = &dch->dev.D;
	if (!try_module_get(THIS_MODULE))
		printk(KERN_WARNING "%s:cannot get module\n", __func__);
	return 0;
}

static int
open_bchannel(struct l1oip *hc, struct dchannel *dch, struct channel_req *rq)
{
	struct bchannel	*bch;
	int		ch;

	if (!test_channelmap(rq->adr.channel, dch->dev.channelmap))
		return -EINVAL;
	if (rq->protocol == ISDN_P_NONE)
		return -EINVAL;
	ch = rq->adr.channel; 
	bch = hc->chan[ch].bch;
	if (!bch) {
		printk(KERN_ERR "%s:internal error ch %d has no bch\n",
		       __func__, ch);
		return -EINVAL;
	}
	if (test_and_set_bit(FLG_OPEN, &bch->Flags))
		return -EBUSY; 
	bch->ch.protocol = rq->protocol;
	rq->ch = &bch->ch;
	if (!try_module_get(THIS_MODULE))
		printk(KERN_WARNING "%s:cannot get module\n", __func__);
	return 0;
}

static int
l1oip_dctrl(struct mISDNchannel *ch, u_int cmd, void *arg)
{
	struct mISDNdevice	*dev = container_of(ch, struct mISDNdevice, D);
	struct dchannel		*dch = container_of(dev, struct dchannel, dev);
	struct l1oip			*hc = dch->hw;
	struct channel_req	*rq;
	int			err = 0;

	if (dch->debug & DEBUG_HW)
		printk(KERN_DEBUG "%s: cmd:%x %p\n",
		       __func__, cmd, arg);
	switch (cmd) {
	case OPEN_CHANNEL:
		rq = arg;
		switch (rq->protocol) {
		case ISDN_P_TE_S0:
		case ISDN_P_NT_S0:
			if (hc->pri) {
				err = -EINVAL;
				break;
			}
			err = open_dchannel(hc, dch, rq);
			break;
		case ISDN_P_TE_E1:
		case ISDN_P_NT_E1:
			if (!hc->pri) {
				err = -EINVAL;
				break;
			}
			err = open_dchannel(hc, dch, rq);
			break;
		default:
			err = open_bchannel(hc, dch, rq);
		}
		break;
	case CLOSE_CHANNEL:
		if (debug & DEBUG_HW_OPEN)
			printk(KERN_DEBUG "%s: dev(%d) close from %p\n",
			       __func__, dch->dev.id,
			       __builtin_return_address(0));
		module_put(THIS_MODULE);
		break;
	case CONTROL_CHANNEL:
		err = channel_dctrl(dch, arg);
		break;
	default:
		if (dch->debug & DEBUG_HW)
			printk(KERN_DEBUG "%s: unknown command %x\n",
			       __func__, cmd);
		err = -EINVAL;
	}
	return err;
}

static int
handle_bmsg(struct mISDNchannel *ch, struct sk_buff *skb)
{
	struct bchannel		*bch = container_of(ch, struct bchannel, ch);
	struct l1oip			*hc = bch->hw;
	int			ret = -EINVAL;
	struct mISDNhead	*hh = mISDN_HEAD_P(skb);
	int			l, ll;
	unsigned char		*p;

	switch (hh->prim) {
	case PH_DATA_REQ:
		if (skb->len <= 0) {
			printk(KERN_WARNING "%s: skb too small\n",
			       __func__);
			break;
		}
		if (skb->len > MAX_DFRAME_LEN_L1 || skb->len > L1OIP_MAX_LEN) {
			printk(KERN_WARNING "%s: skb too large\n",
			       __func__);
			break;
		}
		
		l = skb->len;
		if (!memchr_inv(skb->data, 0xff, l)) {
			if (debug & DEBUG_L1OIP_MSG)
				printk(KERN_DEBUG "%s: got AIS, not sending, "
				       "but counting\n", __func__);
			hc->chan[bch->slot].tx_counter += l;
			skb_trim(skb, 0);
			queue_ch_frame(ch, PH_DATA_CNF, hh->id, skb);
			return 0;
		}
		
		l = skb->len;
		if (!memchr_inv(skb->data, 0x2a, l)) {
			if (debug & DEBUG_L1OIP_MSG)
				printk(KERN_DEBUG "%s: got silence, not sending"
				       ", but counting\n", __func__);
			hc->chan[bch->slot].tx_counter += l;
			skb_trim(skb, 0);
			queue_ch_frame(ch, PH_DATA_CNF, hh->id, skb);
			return 0;
		}

		
		p = skb->data;
		l = skb->len;
		while (l) {
			ll = (l < L1OIP_MAX_PERFRAME) ? l : L1OIP_MAX_PERFRAME;
			l1oip_socket_send(hc, hc->codec, bch->slot, 0,
					  hc->chan[bch->slot].tx_counter, p, ll);
			hc->chan[bch->slot].tx_counter += ll;
			p += ll;
			l -= ll;
		}
		skb_trim(skb, 0);
		queue_ch_frame(ch, PH_DATA_CNF, hh->id, skb);
		return 0;
	case PH_ACTIVATE_REQ:
		if (debug & (DEBUG_L1OIP_MSG | DEBUG_L1OIP_SOCKET))
			printk(KERN_DEBUG "%s: PH_ACTIVATE channel %d (1..%d)\n"
			       , __func__, bch->slot, hc->b_num + 1);
		hc->chan[bch->slot].codecstate = 0;
		test_and_set_bit(FLG_ACTIVE, &bch->Flags);
		skb_trim(skb, 0);
		queue_ch_frame(ch, PH_ACTIVATE_IND, hh->id, skb);
		return 0;
	case PH_DEACTIVATE_REQ:
		if (debug & (DEBUG_L1OIP_MSG | DEBUG_L1OIP_SOCKET))
			printk(KERN_DEBUG "%s: PH_DEACTIVATE channel %d "
			       "(1..%d)\n", __func__, bch->slot,
			       hc->b_num + 1);
		test_and_clear_bit(FLG_ACTIVE, &bch->Flags);
		skb_trim(skb, 0);
		queue_ch_frame(ch, PH_DEACTIVATE_IND, hh->id, skb);
		return 0;
	}
	if (!ret)
		dev_kfree_skb(skb);
	return ret;
}

static int
channel_bctrl(struct bchannel *bch, struct mISDN_ctrl_req *cq)
{
	int			ret = 0;
	struct dsp_features	*features =
		(struct dsp_features *)(*((u_long *)&cq->p1));

	switch (cq->op) {
	case MISDN_CTRL_GETOP:
		cq->op = MISDN_CTRL_HW_FEATURES_OP;
		break;
	case MISDN_CTRL_HW_FEATURES: 
		if (debug & DEBUG_L1OIP_MSG)
			printk(KERN_DEBUG "%s: HW_FEATURE request\n",
			       __func__);
		
		features->unclocked = 1;
		features->unordered = 1;
		break;
	default:
		printk(KERN_WARNING "%s: unknown Op %x\n",
		       __func__, cq->op);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int
l1oip_bctrl(struct mISDNchannel *ch, u_int cmd, void *arg)
{
	struct bchannel	*bch = container_of(ch, struct bchannel, ch);
	int		err = -EINVAL;

	if (bch->debug & DEBUG_HW)
		printk(KERN_DEBUG "%s: cmd:%x %p\n",
		       __func__, cmd, arg);
	switch (cmd) {
	case CLOSE_CHANNEL:
		test_and_clear_bit(FLG_OPEN, &bch->Flags);
		test_and_clear_bit(FLG_ACTIVE, &bch->Flags);
		ch->protocol = ISDN_P_NONE;
		ch->peer = NULL;
		module_put(THIS_MODULE);
		err = 0;
		break;
	case CONTROL_CHANNEL:
		err = channel_bctrl(bch, arg);
		break;
	default:
		printk(KERN_WARNING "%s: unknown prim(%x)\n",
		       __func__, cmd);
	}
	return err;
}


static void
release_card(struct l1oip *hc)
{
	int	ch;

	if (timer_pending(&hc->keep_tl))
		del_timer(&hc->keep_tl);

	if (timer_pending(&hc->timeout_tl))
		del_timer(&hc->timeout_tl);

	cancel_work_sync(&hc->workq);

	if (hc->socket_thread)
		l1oip_socket_close(hc);

	if (hc->registered && hc->chan[hc->d_idx].dch)
		mISDN_unregister_device(&hc->chan[hc->d_idx].dch->dev);
	for (ch = 0; ch < 128; ch++) {
		if (hc->chan[ch].dch) {
			mISDN_freedchannel(hc->chan[ch].dch);
			kfree(hc->chan[ch].dch);
		}
		if (hc->chan[ch].bch) {
			mISDN_freebchannel(hc->chan[ch].bch);
			kfree(hc->chan[ch].bch);
#ifdef REORDER_DEBUG
			if (hc->chan[ch].disorder_skb)
				dev_kfree_skb(hc->chan[ch].disorder_skb);
#endif
		}
	}

	spin_lock(&l1oip_lock);
	list_del(&hc->list);
	spin_unlock(&l1oip_lock);

	kfree(hc);
}

static void
l1oip_cleanup(void)
{
	struct l1oip *hc, *next;

	list_for_each_entry_safe(hc, next, &l1oip_ilist, list)
		release_card(hc);

	l1oip_4bit_free();
}


static int
init_card(struct l1oip *hc, int pri, int bundle)
{
	struct dchannel	*dch;
	struct bchannel	*bch;
	int		ret;
	int		i, ch;

	spin_lock_init(&hc->socket_lock);
	hc->idx = l1oip_cnt;
	hc->pri = pri;
	hc->d_idx = pri ? 16 : 3;
	hc->b_num = pri ? 30 : 2;
	hc->bundle = bundle;
	if (hc->pri)
		sprintf(hc->name, "l1oip-e1.%d", l1oip_cnt + 1);
	else
		sprintf(hc->name, "l1oip-s0.%d", l1oip_cnt + 1);

	switch (codec[l1oip_cnt]) {
	case 0: 
	case 1: 
	case 2: 
	case 3: 
		break;
	default:
		printk(KERN_ERR "Codec(%d) not supported.\n",
		       codec[l1oip_cnt]);
		return -EINVAL;
	}
	hc->codec = codec[l1oip_cnt];
	if (debug & DEBUG_L1OIP_INIT)
		printk(KERN_DEBUG "%s: using codec %d\n",
		       __func__, hc->codec);

	if (id[l1oip_cnt] == 0) {
		printk(KERN_WARNING "Warning: No 'id' value given or "
		       "0, this is highly unsecure. Please use 32 "
		       "bit randmom number 0x...\n");
	}
	hc->id = id[l1oip_cnt];
	if (debug & DEBUG_L1OIP_INIT)
		printk(KERN_DEBUG "%s: using id 0x%x\n", __func__, hc->id);

	hc->ondemand = ondemand[l1oip_cnt];
	if (hc->ondemand && !hc->id) {
		printk(KERN_ERR "%s: ondemand option only allowed in "
		       "conjunction with non 0 ID\n", __func__);
		return -EINVAL;
	}

	if (limit[l1oip_cnt])
		hc->b_num = limit[l1oip_cnt];
	if (!pri && hc->b_num > 2) {
		printk(KERN_ERR "Maximum limit for BRI interface is 2 "
		       "channels.\n");
		return -EINVAL;
	}
	if (pri && hc->b_num > 126) {
		printk(KERN_ERR "Maximum limit for PRI interface is 126 "
		       "channels.\n");
		return -EINVAL;
	}
	if (pri && hc->b_num > 30) {
		printk(KERN_WARNING "Maximum limit for BRI interface is 30 "
		       "channels.\n");
		printk(KERN_WARNING "Your selection of %d channels must be "
		       "supported by application.\n", hc->limit);
	}

	hc->remoteip = ip[l1oip_cnt << 2] << 24
		| ip[(l1oip_cnt << 2) + 1] << 16
		| ip[(l1oip_cnt << 2) + 2] << 8
		| ip[(l1oip_cnt << 2) + 3];
	hc->localport = port[l1oip_cnt]?:(L1OIP_DEFAULTPORT + l1oip_cnt);
	if (remoteport[l1oip_cnt])
		hc->remoteport = remoteport[l1oip_cnt];
	else
		hc->remoteport = hc->localport;
	if (debug & DEBUG_L1OIP_INIT)
		printk(KERN_DEBUG "%s: using local port %d remote ip "
		       "%d.%d.%d.%d port %d ondemand %d\n", __func__,
		       hc->localport, hc->remoteip >> 24,
		       (hc->remoteip >> 16) & 0xff,
		       (hc->remoteip >> 8) & 0xff, hc->remoteip & 0xff,
		       hc->remoteport, hc->ondemand);

	dch = kzalloc(sizeof(struct dchannel), GFP_KERNEL);
	if (!dch)
		return -ENOMEM;
	dch->debug = debug;
	mISDN_initdchannel(dch, MAX_DFRAME_LEN_L1, NULL);
	dch->hw = hc;
	if (pri)
		dch->dev.Dprotocols = (1 << ISDN_P_TE_E1) | (1 << ISDN_P_NT_E1);
	else
		dch->dev.Dprotocols = (1 << ISDN_P_TE_S0) | (1 << ISDN_P_NT_S0);
	dch->dev.Bprotocols = (1 << (ISDN_P_B_RAW & ISDN_P_B_MASK)) |
		(1 << (ISDN_P_B_HDLC & ISDN_P_B_MASK));
	dch->dev.D.send = handle_dmsg;
	dch->dev.D.ctrl = l1oip_dctrl;
	dch->dev.nrbchan = hc->b_num;
	dch->slot = hc->d_idx;
	hc->chan[hc->d_idx].dch = dch;
	i = 1;
	for (ch = 0; ch < dch->dev.nrbchan; ch++) {
		if (ch == 15)
			i++;
		bch = kzalloc(sizeof(struct bchannel), GFP_KERNEL);
		if (!bch) {
			printk(KERN_ERR "%s: no memory for bchannel\n",
			       __func__);
			return -ENOMEM;
		}
		bch->nr = i + ch;
		bch->slot = i + ch;
		bch->debug = debug;
		mISDN_initbchannel(bch, MAX_DATA_MEM);
		bch->hw = hc;
		bch->ch.send = handle_bmsg;
		bch->ch.ctrl = l1oip_bctrl;
		bch->ch.nr = i + ch;
		list_add(&bch->ch.list, &dch->dev.bchannels);
		hc->chan[i + ch].bch = bch;
		set_channelmap(bch->nr, dch->dev.channelmap);
	}
	
	ret = mISDN_register_device(&dch->dev, NULL, hc->name);
	if (ret)
		return ret;
	hc->registered = 1;

	if (debug & DEBUG_L1OIP_INIT)
		printk(KERN_DEBUG "%s: Setting up network card(%d)\n",
		       __func__, l1oip_cnt + 1);
	ret = l1oip_socket_open(hc);
	if (ret)
		return ret;

	hc->keep_tl.function = (void *)l1oip_keepalive;
	hc->keep_tl.data = (ulong)hc;
	init_timer(&hc->keep_tl);
	hc->keep_tl.expires = jiffies + 2 * HZ; 
	add_timer(&hc->keep_tl);

	hc->timeout_tl.function = (void *)l1oip_timeout;
	hc->timeout_tl.data = (ulong)hc;
	init_timer(&hc->timeout_tl);
	hc->timeout_on = 0; 

	return 0;
}

static int __init
l1oip_init(void)
{
	int		pri, bundle;
	struct l1oip		*hc;
	int		ret;

	printk(KERN_INFO "mISDN: Layer-1-over-IP driver Rev. %s\n",
	       l1oip_revision);

	INIT_LIST_HEAD(&l1oip_ilist);
	spin_lock_init(&l1oip_lock);

	if (l1oip_4bit_alloc(ulaw))
		return -ENOMEM;

	l1oip_cnt = 0;
	while (l1oip_cnt < MAX_CARDS && type[l1oip_cnt]) {
		switch (type[l1oip_cnt] & 0xff) {
		case 1:
			pri = 0;
			bundle = 0;
			break;
		case 2:
			pri = 1;
			bundle = 0;
			break;
		case 3:
			pri = 0;
			bundle = 1;
			break;
		case 4:
			pri = 1;
			bundle = 1;
			break;
		default:
			printk(KERN_ERR "Card type(%d) not supported.\n",
			       type[l1oip_cnt] & 0xff);
			l1oip_cleanup();
			return -EINVAL;
		}

		if (debug & DEBUG_L1OIP_INIT)
			printk(KERN_DEBUG "%s: interface %d is %s with %s.\n",
			       __func__, l1oip_cnt, pri ? "PRI" : "BRI",
			       bundle ? "bundled IP packet for all B-channels" :
			       "separate IP packets for every B-channel");

		hc = kzalloc(sizeof(struct l1oip), GFP_ATOMIC);
		if (!hc) {
			printk(KERN_ERR "No kmem for L1-over-IP driver.\n");
			l1oip_cleanup();
			return -ENOMEM;
		}
		INIT_WORK(&hc->workq, (void *)l1oip_send_bh);

		spin_lock(&l1oip_lock);
		list_add_tail(&hc->list, &l1oip_ilist);
		spin_unlock(&l1oip_lock);

		ret = init_card(hc, pri, bundle);
		if (ret) {
			l1oip_cleanup();
			return ret;
		}

		l1oip_cnt++;
	}
	printk(KERN_INFO "%d virtual devices registered\n", l1oip_cnt);
	return 0;
}

module_init(l1oip_init);
module_exit(l1oip_cleanup);
