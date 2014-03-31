/*
 * net/tipc/socket.c: TIPC socket API
 *
 * Copyright (c) 2001-2007, Ericsson AB
 * Copyright (c) 2004-2008, 2010-2011, Wind River Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/export.h>
#include <net/sock.h>

#include "core.h"
#include "port.h"

#define SS_LISTENING	-1	
#define SS_READY	-2	

#define OVERLOAD_LIMIT_BASE	5000
#define CONN_TIMEOUT_DEFAULT	8000	

struct tipc_sock {
	struct sock sk;
	struct tipc_port *p;
	struct tipc_portid peer_name;
	unsigned int conn_timeout;
};

#define tipc_sk(sk) ((struct tipc_sock *)(sk))
#define tipc_sk_port(sk) ((struct tipc_port *)(tipc_sk(sk)->p))

#define tipc_rx_ready(sock) (!skb_queue_empty(&sock->sk->sk_receive_queue) || \
			(sock->state == SS_DISCONNECTING))

static int backlog_rcv(struct sock *sk, struct sk_buff *skb);
static u32 dispatch(struct tipc_port *tport, struct sk_buff *buf);
static void wakeupdispatch(struct tipc_port *tport);

static const struct proto_ops packet_ops;
static const struct proto_ops stream_ops;
static const struct proto_ops msg_ops;

static struct proto tipc_proto;

static int sockets_enabled;

static atomic_t tipc_queue_size = ATOMIC_INIT(0);



static void advance_rx_queue(struct sock *sk)
{
	kfree_skb(__skb_dequeue(&sk->sk_receive_queue));
	atomic_dec(&tipc_queue_size);
}


static void discard_rx_queue(struct sock *sk)
{
	struct sk_buff *buf;

	while ((buf = __skb_dequeue(&sk->sk_receive_queue))) {
		atomic_dec(&tipc_queue_size);
		kfree_skb(buf);
	}
}


static void reject_rx_queue(struct sock *sk)
{
	struct sk_buff *buf;

	while ((buf = __skb_dequeue(&sk->sk_receive_queue))) {
		tipc_reject_msg(buf, TIPC_ERR_NO_PORT);
		atomic_dec(&tipc_queue_size);
	}
}


static int tipc_create(struct net *net, struct socket *sock, int protocol,
		       int kern)
{
	const struct proto_ops *ops;
	socket_state state;
	struct sock *sk;
	struct tipc_port *tp_ptr;

	

	if (unlikely(protocol != 0))
		return -EPROTONOSUPPORT;

	switch (sock->type) {
	case SOCK_STREAM:
		ops = &stream_ops;
		state = SS_UNCONNECTED;
		break;
	case SOCK_SEQPACKET:
		ops = &packet_ops;
		state = SS_UNCONNECTED;
		break;
	case SOCK_DGRAM:
	case SOCK_RDM:
		ops = &msg_ops;
		state = SS_READY;
		break;
	default:
		return -EPROTOTYPE;
	}

	

	sk = sk_alloc(net, AF_TIPC, GFP_KERNEL, &tipc_proto);
	if (sk == NULL)
		return -ENOMEM;

	

	tp_ptr = tipc_createport_raw(sk, &dispatch, &wakeupdispatch,
				     TIPC_LOW_IMPORTANCE);
	if (unlikely(!tp_ptr)) {
		sk_free(sk);
		return -ENOMEM;
	}

	

	sock->ops = ops;
	sock->state = state;

	sock_init_data(sock, sk);
	sk->sk_backlog_rcv = backlog_rcv;
	tipc_sk(sk)->p = tp_ptr;
	tipc_sk(sk)->conn_timeout = CONN_TIMEOUT_DEFAULT;

	spin_unlock_bh(tp_ptr->lock);

	if (sock->state == SS_READY) {
		tipc_set_portunreturnable(tp_ptr->ref, 1);
		if (sock->type == SOCK_DGRAM)
			tipc_set_portunreliable(tp_ptr->ref, 1);
	}

	return 0;
}


static int release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct tipc_port *tport;
	struct sk_buff *buf;
	int res;


	if (sk == NULL)
		return 0;

	tport = tipc_sk_port(sk);
	lock_sock(sk);


	while (sock->state != SS_DISCONNECTING) {
		buf = __skb_dequeue(&sk->sk_receive_queue);
		if (buf == NULL)
			break;
		atomic_dec(&tipc_queue_size);
		if (TIPC_SKB_CB(buf)->handle != 0)
			kfree_skb(buf);
		else {
			if ((sock->state == SS_CONNECTING) ||
			    (sock->state == SS_CONNECTED)) {
				sock->state = SS_DISCONNECTING;
				tipc_disconnect(tport->ref);
			}
			tipc_reject_msg(buf, TIPC_ERR_NO_PORT);
		}
	}


	res = tipc_deleteport(tport->ref);

	

	discard_rx_queue(sk);

	

	sock->state = SS_DISCONNECTING;
	release_sock(sk);

	sock_put(sk);
	sock->sk = NULL;

	return res;
}


static int bind(struct socket *sock, struct sockaddr *uaddr, int uaddr_len)
{
	struct sockaddr_tipc *addr = (struct sockaddr_tipc *)uaddr;
	u32 portref = tipc_sk_port(sock->sk)->ref;

	if (unlikely(!uaddr_len))
		return tipc_withdraw(portref, 0, NULL);

	if (uaddr_len < sizeof(struct sockaddr_tipc))
		return -EINVAL;
	if (addr->family != AF_TIPC)
		return -EAFNOSUPPORT;

	if (addr->addrtype == TIPC_ADDR_NAME)
		addr->addr.nameseq.upper = addr->addr.nameseq.lower;
	else if (addr->addrtype != TIPC_ADDR_NAMESEQ)
		return -EAFNOSUPPORT;

	if (addr->addr.nameseq.type < TIPC_RESERVED_TYPES)
		return -EACCES;

	return (addr->scope > 0) ?
		tipc_publish(portref, addr->scope, &addr->addr.nameseq) :
		tipc_withdraw(portref, -addr->scope, &addr->addr.nameseq);
}


static int get_name(struct socket *sock, struct sockaddr *uaddr,
		    int *uaddr_len, int peer)
{
	struct sockaddr_tipc *addr = (struct sockaddr_tipc *)uaddr;
	struct tipc_sock *tsock = tipc_sk(sock->sk);

	memset(addr, 0, sizeof(*addr));
	if (peer) {
		if ((sock->state != SS_CONNECTED) &&
			((peer != 2) || (sock->state != SS_DISCONNECTING)))
			return -ENOTCONN;
		addr->addr.id.ref = tsock->peer_name.ref;
		addr->addr.id.node = tsock->peer_name.node;
	} else {
		addr->addr.id.ref = tsock->p->ref;
		addr->addr.id.node = tipc_own_addr;
	}

	*uaddr_len = sizeof(*addr);
	addr->addrtype = TIPC_ADDR_ID;
	addr->family = AF_TIPC;
	addr->scope = 0;
	addr->addr.name.domain = 0;

	return 0;
}


static unsigned int poll(struct file *file, struct socket *sock,
			 poll_table *wait)
{
	struct sock *sk = sock->sk;
	u32 mask = 0;

	poll_wait(file, sk_sleep(sk), wait);

	switch ((int)sock->state) {
	case SS_READY:
	case SS_CONNECTED:
		if (!tipc_sk_port(sk)->congested)
			mask |= POLLOUT;
		
	case SS_CONNECTING:
	case SS_LISTENING:
		if (!skb_queue_empty(&sk->sk_receive_queue))
			mask |= (POLLIN | POLLRDNORM);
		break;
	case SS_DISCONNECTING:
		mask = (POLLIN | POLLRDNORM | POLLHUP);
		break;
	}

	return mask;
}


static int dest_name_check(struct sockaddr_tipc *dest, struct msghdr *m)
{
	struct tipc_cfg_msg_hdr hdr;

	if (likely(dest->addr.name.name.type >= TIPC_RESERVED_TYPES))
		return 0;
	if (likely(dest->addr.name.name.type == TIPC_TOP_SRV))
		return 0;
	if (likely(dest->addr.name.name.type != TIPC_CFG_SRV))
		return -EACCES;

	if (!m->msg_iovlen || (m->msg_iov[0].iov_len < sizeof(hdr)))
		return -EMSGSIZE;
	if (copy_from_user(&hdr, m->msg_iov[0].iov_base, sizeof(hdr)))
		return -EFAULT;
	if ((ntohs(hdr.tcm_type) & 0xC000) && (!capable(CAP_NET_ADMIN)))
		return -EACCES;

	return 0;
}


static int send_msg(struct kiocb *iocb, struct socket *sock,
		    struct msghdr *m, size_t total_len)
{
	struct sock *sk = sock->sk;
	struct tipc_port *tport = tipc_sk_port(sk);
	struct sockaddr_tipc *dest = (struct sockaddr_tipc *)m->msg_name;
	int needs_conn;
	long timeout_val;
	int res = -EINVAL;

	if (unlikely(!dest))
		return -EDESTADDRREQ;
	if (unlikely((m->msg_namelen < sizeof(*dest)) ||
		     (dest->family != AF_TIPC)))
		return -EINVAL;
	if ((total_len > TIPC_MAX_USER_MSG_SIZE) ||
	    (m->msg_iovlen > (unsigned)INT_MAX))
		return -EMSGSIZE;

	if (iocb)
		lock_sock(sk);

	needs_conn = (sock->state != SS_READY);
	if (unlikely(needs_conn)) {
		if (sock->state == SS_LISTENING) {
			res = -EPIPE;
			goto exit;
		}
		if (sock->state != SS_UNCONNECTED) {
			res = -EISCONN;
			goto exit;
		}
		if ((tport->published) ||
		    ((sock->type == SOCK_STREAM) && (total_len != 0))) {
			res = -EOPNOTSUPP;
			goto exit;
		}
		if (dest->addrtype == TIPC_ADDR_NAME) {
			tport->conn_type = dest->addr.name.name.type;
			tport->conn_instance = dest->addr.name.name.instance;
		}

		

		reject_rx_queue(sk);
	}

	timeout_val = sock_sndtimeo(sk, m->msg_flags & MSG_DONTWAIT);

	do {
		if (dest->addrtype == TIPC_ADDR_NAME) {
			res = dest_name_check(dest, m);
			if (res)
				break;
			res = tipc_send2name(tport->ref,
					     &dest->addr.name.name,
					     dest->addr.name.domain,
					     m->msg_iovlen,
					     m->msg_iov,
					     total_len);
		} else if (dest->addrtype == TIPC_ADDR_ID) {
			res = tipc_send2port(tport->ref,
					     &dest->addr.id,
					     m->msg_iovlen,
					     m->msg_iov,
					     total_len);
		} else if (dest->addrtype == TIPC_ADDR_MCAST) {
			if (needs_conn) {
				res = -EOPNOTSUPP;
				break;
			}
			res = dest_name_check(dest, m);
			if (res)
				break;
			res = tipc_multicast(tport->ref,
					     &dest->addr.nameseq,
					     m->msg_iovlen,
					     m->msg_iov,
					     total_len);
		}
		if (likely(res != -ELINKCONG)) {
			if (needs_conn && (res >= 0))
				sock->state = SS_CONNECTING;
			break;
		}
		if (timeout_val <= 0L) {
			res = timeout_val ? timeout_val : -EWOULDBLOCK;
			break;
		}
		release_sock(sk);
		timeout_val = wait_event_interruptible_timeout(*sk_sleep(sk),
					       !tport->congested, timeout_val);
		lock_sock(sk);
	} while (1);

exit:
	if (iocb)
		release_sock(sk);
	return res;
}


static int send_packet(struct kiocb *iocb, struct socket *sock,
		       struct msghdr *m, size_t total_len)
{
	struct sock *sk = sock->sk;
	struct tipc_port *tport = tipc_sk_port(sk);
	struct sockaddr_tipc *dest = (struct sockaddr_tipc *)m->msg_name;
	long timeout_val;
	int res;

	

	if (unlikely(dest))
		return send_msg(iocb, sock, m, total_len);

	if ((total_len > TIPC_MAX_USER_MSG_SIZE) ||
	    (m->msg_iovlen > (unsigned)INT_MAX))
		return -EMSGSIZE;

	if (iocb)
		lock_sock(sk);

	timeout_val = sock_sndtimeo(sk, m->msg_flags & MSG_DONTWAIT);

	do {
		if (unlikely(sock->state != SS_CONNECTED)) {
			if (sock->state == SS_DISCONNECTING)
				res = -EPIPE;
			else
				res = -ENOTCONN;
			break;
		}

		res = tipc_send(tport->ref, m->msg_iovlen, m->msg_iov,
				total_len);
		if (likely(res != -ELINKCONG))
			break;
		if (timeout_val <= 0L) {
			res = timeout_val ? timeout_val : -EWOULDBLOCK;
			break;
		}
		release_sock(sk);
		timeout_val = wait_event_interruptible_timeout(*sk_sleep(sk),
			(!tport->congested || !tport->connected), timeout_val);
		lock_sock(sk);
	} while (1);

	if (iocb)
		release_sock(sk);
	return res;
}


static int send_stream(struct kiocb *iocb, struct socket *sock,
		       struct msghdr *m, size_t total_len)
{
	struct sock *sk = sock->sk;
	struct tipc_port *tport = tipc_sk_port(sk);
	struct msghdr my_msg;
	struct iovec my_iov;
	struct iovec *curr_iov;
	int curr_iovlen;
	char __user *curr_start;
	u32 hdr_size;
	int curr_left;
	int bytes_to_send;
	int bytes_sent;
	int res;

	lock_sock(sk);

	

	if (unlikely(sock->state != SS_CONNECTED)) {
		if (sock->state == SS_UNCONNECTED) {
			res = send_packet(NULL, sock, m, total_len);
			goto exit;
		} else if (sock->state == SS_DISCONNECTING) {
			res = -EPIPE;
			goto exit;
		} else {
			res = -ENOTCONN;
			goto exit;
		}
	}

	if (unlikely(m->msg_name)) {
		res = -EISCONN;
		goto exit;
	}

	if ((total_len > (unsigned)INT_MAX) ||
	    (m->msg_iovlen > (unsigned)INT_MAX)) {
		res = -EMSGSIZE;
		goto exit;
	}


	curr_iov = m->msg_iov;
	curr_iovlen = m->msg_iovlen;
	my_msg.msg_iov = &my_iov;
	my_msg.msg_iovlen = 1;
	my_msg.msg_flags = m->msg_flags;
	my_msg.msg_name = NULL;
	bytes_sent = 0;

	hdr_size = msg_hdr_sz(&tport->phdr);

	while (curr_iovlen--) {
		curr_start = curr_iov->iov_base;
		curr_left = curr_iov->iov_len;

		while (curr_left) {
			bytes_to_send = tport->max_pkt - hdr_size;
			if (bytes_to_send > TIPC_MAX_USER_MSG_SIZE)
				bytes_to_send = TIPC_MAX_USER_MSG_SIZE;
			if (curr_left < bytes_to_send)
				bytes_to_send = curr_left;
			my_iov.iov_base = curr_start;
			my_iov.iov_len = bytes_to_send;
			res = send_packet(NULL, sock, &my_msg, bytes_to_send);
			if (res < 0) {
				if (bytes_sent)
					res = bytes_sent;
				goto exit;
			}
			curr_left -= bytes_to_send;
			curr_start += bytes_to_send;
			bytes_sent += bytes_to_send;
		}

		curr_iov++;
	}
	res = bytes_sent;
exit:
	release_sock(sk);
	return res;
}


static int auto_connect(struct socket *sock, struct tipc_msg *msg)
{
	struct tipc_sock *tsock = tipc_sk(sock->sk);

	if (msg_errcode(msg)) {
		sock->state = SS_DISCONNECTING;
		return -ECONNREFUSED;
	}

	tsock->peer_name.ref = msg_origport(msg);
	tsock->peer_name.node = msg_orignode(msg);
	tipc_connect2port(tsock->p->ref, &tsock->peer_name);
	tipc_set_portimportance(tsock->p->ref, msg_importance(msg));
	sock->state = SS_CONNECTED;
	return 0;
}


static void set_orig_addr(struct msghdr *m, struct tipc_msg *msg)
{
	struct sockaddr_tipc *addr = (struct sockaddr_tipc *)m->msg_name;

	if (addr) {
		addr->family = AF_TIPC;
		addr->addrtype = TIPC_ADDR_ID;
		addr->addr.id.ref = msg_origport(msg);
		addr->addr.id.node = msg_orignode(msg);
		addr->addr.name.domain = 0;	
		addr->scope = 0;		
		m->msg_namelen = sizeof(struct sockaddr_tipc);
	}
}


static int anc_data_recv(struct msghdr *m, struct tipc_msg *msg,
				struct tipc_port *tport)
{
	u32 anc_data[3];
	u32 err;
	u32 dest_type;
	int has_name;
	int res;

	if (likely(m->msg_controllen == 0))
		return 0;

	

	err = msg ? msg_errcode(msg) : 0;
	if (unlikely(err)) {
		anc_data[0] = err;
		anc_data[1] = msg_data_sz(msg);
		res = put_cmsg(m, SOL_TIPC, TIPC_ERRINFO, 8, anc_data);
		if (res)
			return res;
		if (anc_data[1]) {
			res = put_cmsg(m, SOL_TIPC, TIPC_RETDATA, anc_data[1],
				       msg_data(msg));
			if (res)
				return res;
		}
	}

	

	dest_type = msg ? msg_type(msg) : TIPC_DIRECT_MSG;
	switch (dest_type) {
	case TIPC_NAMED_MSG:
		has_name = 1;
		anc_data[0] = msg_nametype(msg);
		anc_data[1] = msg_namelower(msg);
		anc_data[2] = msg_namelower(msg);
		break;
	case TIPC_MCAST_MSG:
		has_name = 1;
		anc_data[0] = msg_nametype(msg);
		anc_data[1] = msg_namelower(msg);
		anc_data[2] = msg_nameupper(msg);
		break;
	case TIPC_CONN_MSG:
		has_name = (tport->conn_type != 0);
		anc_data[0] = tport->conn_type;
		anc_data[1] = tport->conn_instance;
		anc_data[2] = tport->conn_instance;
		break;
	default:
		has_name = 0;
	}
	if (has_name) {
		res = put_cmsg(m, SOL_TIPC, TIPC_DESTNAME, 12, anc_data);
		if (res)
			return res;
	}

	return 0;
}


static int recv_msg(struct kiocb *iocb, struct socket *sock,
		    struct msghdr *m, size_t buf_len, int flags)
{
	struct sock *sk = sock->sk;
	struct tipc_port *tport = tipc_sk_port(sk);
	struct sk_buff *buf;
	struct tipc_msg *msg;
	long timeout;
	unsigned int sz;
	u32 err;
	int res;

	

	if (unlikely(!buf_len))
		return -EINVAL;

	lock_sock(sk);

	if (unlikely(sock->state == SS_UNCONNECTED)) {
		res = -ENOTCONN;
		goto exit;
	}

	timeout = sock_rcvtimeo(sk, flags & MSG_DONTWAIT);
restart:

	

	while (skb_queue_empty(&sk->sk_receive_queue)) {
		if (sock->state == SS_DISCONNECTING) {
			res = -ENOTCONN;
			goto exit;
		}
		if (timeout <= 0L) {
			res = timeout ? timeout : -EWOULDBLOCK;
			goto exit;
		}
		release_sock(sk);
		timeout = wait_event_interruptible_timeout(*sk_sleep(sk),
							   tipc_rx_ready(sock),
							   timeout);
		lock_sock(sk);
	}

	

	buf = skb_peek(&sk->sk_receive_queue);
	msg = buf_msg(buf);
	sz = msg_data_sz(msg);
	err = msg_errcode(msg);

	

	if (unlikely(sock->state == SS_CONNECTING)) {
		res = auto_connect(sock, msg);
		if (res)
			goto exit;
	}

	

	if ((!sz) && (!err)) {
		advance_rx_queue(sk);
		goto restart;
	}

	

	set_orig_addr(m, msg);

	

	res = anc_data_recv(m, msg, tport);
	if (res)
		goto exit;

	

	if (!err) {
		if (unlikely(buf_len < sz)) {
			sz = buf_len;
			m->msg_flags |= MSG_TRUNC;
		}
		res = skb_copy_datagram_iovec(buf, msg_hdr_sz(msg),
					      m->msg_iov, sz);
		if (res)
			goto exit;
		res = sz;
	} else {
		if ((sock->state == SS_READY) ||
		    ((err == TIPC_CONN_SHUTDOWN) || m->msg_control))
			res = 0;
		else
			res = -ECONNRESET;
	}

	

	if (likely(!(flags & MSG_PEEK))) {
		if ((sock->state != SS_READY) &&
		    (++tport->conn_unacked >= TIPC_FLOW_CONTROL_WIN))
			tipc_acknowledge(tport->ref, tport->conn_unacked);
		advance_rx_queue(sk);
	}
exit:
	release_sock(sk);
	return res;
}


static int recv_stream(struct kiocb *iocb, struct socket *sock,
		       struct msghdr *m, size_t buf_len, int flags)
{
	struct sock *sk = sock->sk;
	struct tipc_port *tport = tipc_sk_port(sk);
	struct sk_buff *buf;
	struct tipc_msg *msg;
	long timeout;
	unsigned int sz;
	int sz_to_copy, target, needed;
	int sz_copied = 0;
	u32 err;
	int res = 0;

	

	if (unlikely(!buf_len))
		return -EINVAL;

	lock_sock(sk);

	if (unlikely((sock->state == SS_UNCONNECTED) ||
		     (sock->state == SS_CONNECTING))) {
		res = -ENOTCONN;
		goto exit;
	}

	target = sock_rcvlowat(sk, flags & MSG_WAITALL, buf_len);
	timeout = sock_rcvtimeo(sk, flags & MSG_DONTWAIT);
restart:

	

	while (skb_queue_empty(&sk->sk_receive_queue)) {
		if (sock->state == SS_DISCONNECTING) {
			res = -ENOTCONN;
			goto exit;
		}
		if (timeout <= 0L) {
			res = timeout ? timeout : -EWOULDBLOCK;
			goto exit;
		}
		release_sock(sk);
		timeout = wait_event_interruptible_timeout(*sk_sleep(sk),
							   tipc_rx_ready(sock),
							   timeout);
		lock_sock(sk);
	}

	

	buf = skb_peek(&sk->sk_receive_queue);
	msg = buf_msg(buf);
	sz = msg_data_sz(msg);
	err = msg_errcode(msg);

	

	if ((!sz) && (!err)) {
		advance_rx_queue(sk);
		goto restart;
	}

	

	if (sz_copied == 0) {
		set_orig_addr(m, msg);
		res = anc_data_recv(m, msg, tport);
		if (res)
			goto exit;
	}

	

	if (!err) {
		u32 offset = (u32)(unsigned long)(TIPC_SKB_CB(buf)->handle);

		sz -= offset;
		needed = (buf_len - sz_copied);
		sz_to_copy = (sz <= needed) ? sz : needed;

		res = skb_copy_datagram_iovec(buf, msg_hdr_sz(msg) + offset,
					      m->msg_iov, sz_to_copy);
		if (res)
			goto exit;

		sz_copied += sz_to_copy;

		if (sz_to_copy < sz) {
			if (!(flags & MSG_PEEK))
				TIPC_SKB_CB(buf)->handle =
				(void *)(unsigned long)(offset + sz_to_copy);
			goto exit;
		}
	} else {
		if (sz_copied != 0)
			goto exit; 

		if ((err == TIPC_CONN_SHUTDOWN) || m->msg_control)
			res = 0;
		else
			res = -ECONNRESET;
	}

	

	if (likely(!(flags & MSG_PEEK))) {
		if (unlikely(++tport->conn_unacked >= TIPC_FLOW_CONTROL_WIN))
			tipc_acknowledge(tport->ref, tport->conn_unacked);
		advance_rx_queue(sk);
	}

	

	if ((sz_copied < buf_len) &&	
	    (!skb_queue_empty(&sk->sk_receive_queue) ||
	    (sz_copied < target)) &&	
	    (!(flags & MSG_PEEK)) &&	
	    (!err))			
		goto restart;

exit:
	release_sock(sk);
	return sz_copied ? sz_copied : res;
}


static int rx_queue_full(struct tipc_msg *msg, u32 queue_size, u32 base)
{
	u32 threshold;
	u32 imp = msg_importance(msg);

	if (imp == TIPC_LOW_IMPORTANCE)
		threshold = base;
	else if (imp == TIPC_MEDIUM_IMPORTANCE)
		threshold = base * 2;
	else if (imp == TIPC_HIGH_IMPORTANCE)
		threshold = base * 100;
	else
		return 0;

	if (msg_connected(msg))
		threshold *= 4;

	return queue_size >= threshold;
}


static u32 filter_rcv(struct sock *sk, struct sk_buff *buf)
{
	struct socket *sock = sk->sk_socket;
	struct tipc_msg *msg = buf_msg(buf);
	u32 recv_q_len;

	


	if (sock->state == SS_READY) {
		if (msg_connected(msg))
			return TIPC_ERR_NO_PORT;
	} else {
		if (msg_mcast(msg))
			return TIPC_ERR_NO_PORT;
		if (sock->state == SS_CONNECTED) {
			if (!msg_connected(msg))
				return TIPC_ERR_NO_PORT;
		} else if (sock->state == SS_CONNECTING) {
			if (!msg_connected(msg) && (msg_errcode(msg) == 0))
				return TIPC_ERR_NO_PORT;
		} else if (sock->state == SS_LISTENING) {
			if (msg_connected(msg) || msg_errcode(msg))
				return TIPC_ERR_NO_PORT;
		} else if (sock->state == SS_DISCONNECTING) {
			return TIPC_ERR_NO_PORT;
		} else  {
			if (msg_connected(msg) || msg_errcode(msg))
				return TIPC_ERR_NO_PORT;
		}
	}

	

	recv_q_len = (u32)atomic_read(&tipc_queue_size);
	if (unlikely(recv_q_len >= OVERLOAD_LIMIT_BASE)) {
		if (rx_queue_full(msg, recv_q_len, OVERLOAD_LIMIT_BASE))
			return TIPC_ERR_OVERLOAD;
	}
	recv_q_len = skb_queue_len(&sk->sk_receive_queue);
	if (unlikely(recv_q_len >= (OVERLOAD_LIMIT_BASE / 2))) {
		if (rx_queue_full(msg, recv_q_len, OVERLOAD_LIMIT_BASE / 2))
			return TIPC_ERR_OVERLOAD;
	}

	

	TIPC_SKB_CB(buf)->handle = 0;
	atomic_inc(&tipc_queue_size);
	__skb_queue_tail(&sk->sk_receive_queue, buf);

	

	if (unlikely(msg_errcode(msg) && (sock->state == SS_CONNECTED))) {
		sock->state = SS_DISCONNECTING;
		tipc_disconnect_port(tipc_sk_port(sk));
	}

	if (waitqueue_active(sk_sleep(sk)))
		wake_up_interruptible(sk_sleep(sk));
	return TIPC_OK;
}


static int backlog_rcv(struct sock *sk, struct sk_buff *buf)
{
	u32 res;

	res = filter_rcv(sk, buf);
	if (res)
		tipc_reject_msg(buf, res);
	return 0;
}


static u32 dispatch(struct tipc_port *tport, struct sk_buff *buf)
{
	struct sock *sk = (struct sock *)tport->usr_handle;
	u32 res;


	bh_lock_sock(sk);
	if (!sock_owned_by_user(sk)) {
		res = filter_rcv(sk, buf);
	} else {
		if (sk_add_backlog(sk, buf))
			res = TIPC_ERR_OVERLOAD;
		else
			res = TIPC_OK;
	}
	bh_unlock_sock(sk);

	return res;
}


static void wakeupdispatch(struct tipc_port *tport)
{
	struct sock *sk = (struct sock *)tport->usr_handle;

	if (waitqueue_active(sk_sleep(sk)))
		wake_up_interruptible(sk_sleep(sk));
}


static int connect(struct socket *sock, struct sockaddr *dest, int destlen,
		   int flags)
{
	struct sock *sk = sock->sk;
	struct sockaddr_tipc *dst = (struct sockaddr_tipc *)dest;
	struct msghdr m = {NULL,};
	struct sk_buff *buf;
	struct tipc_msg *msg;
	unsigned int timeout;
	int res;

	lock_sock(sk);

	

	if (sock->state == SS_READY) {
		res = -EOPNOTSUPP;
		goto exit;
	}

	

	if (flags & O_NONBLOCK) {
		res = -EOPNOTSUPP;
		goto exit;
	}

	

	if (sock->state == SS_LISTENING) {
		res = -EOPNOTSUPP;
		goto exit;
	}
	if (sock->state == SS_CONNECTING) {
		res = -EALREADY;
		goto exit;
	}
	if (sock->state != SS_UNCONNECTED) {
		res = -EISCONN;
		goto exit;
	}


	if (dst->addrtype == TIPC_ADDR_MCAST) {
		res = -EINVAL;
		goto exit;
	}

	

	reject_rx_queue(sk);

	

	m.msg_name = dest;
	m.msg_namelen = destlen;
	res = send_msg(NULL, sock, &m, 0);
	if (res < 0)
		goto exit;

	

	timeout = tipc_sk(sk)->conn_timeout;
	release_sock(sk);
	res = wait_event_interruptible_timeout(*sk_sleep(sk),
			(!skb_queue_empty(&sk->sk_receive_queue) ||
			(sock->state != SS_CONNECTING)),
			timeout ? (long)msecs_to_jiffies(timeout)
				: MAX_SCHEDULE_TIMEOUT);
	lock_sock(sk);

	if (res > 0) {
		buf = skb_peek(&sk->sk_receive_queue);
		if (buf != NULL) {
			msg = buf_msg(buf);
			res = auto_connect(sock, msg);
			if (!res) {
				if (!msg_data_sz(msg))
					advance_rx_queue(sk);
			}
		} else {
			if (sock->state == SS_CONNECTED)
				res = -EISCONN;
			else
				res = -ECONNREFUSED;
		}
	} else {
		if (res == 0)
			res = -ETIMEDOUT;
		else
			; 
		sock->state = SS_DISCONNECTING;
	}

exit:
	release_sock(sk);
	return res;
}


static int listen(struct socket *sock, int len)
{
	struct sock *sk = sock->sk;
	int res;

	lock_sock(sk);

	if (sock->state != SS_UNCONNECTED)
		res = -EINVAL;
	else {
		sock->state = SS_LISTENING;
		res = 0;
	}

	release_sock(sk);
	return res;
}


static int accept(struct socket *sock, struct socket *new_sock, int flags)
{
	struct sock *sk = sock->sk;
	struct sk_buff *buf;
	int res;

	lock_sock(sk);

	if (sock->state != SS_LISTENING) {
		res = -EINVAL;
		goto exit;
	}

	while (skb_queue_empty(&sk->sk_receive_queue)) {
		if (flags & O_NONBLOCK) {
			res = -EWOULDBLOCK;
			goto exit;
		}
		release_sock(sk);
		res = wait_event_interruptible(*sk_sleep(sk),
				(!skb_queue_empty(&sk->sk_receive_queue)));
		lock_sock(sk);
		if (res)
			goto exit;
	}

	buf = skb_peek(&sk->sk_receive_queue);

	res = tipc_create(sock_net(sock->sk), new_sock, 0, 0);
	if (!res) {
		struct sock *new_sk = new_sock->sk;
		struct tipc_sock *new_tsock = tipc_sk(new_sk);
		struct tipc_port *new_tport = new_tsock->p;
		u32 new_ref = new_tport->ref;
		struct tipc_msg *msg = buf_msg(buf);

		lock_sock(new_sk);


		reject_rx_queue(new_sk);

		

		new_tsock->peer_name.ref = msg_origport(msg);
		new_tsock->peer_name.node = msg_orignode(msg);
		tipc_connect2port(new_ref, &new_tsock->peer_name);
		new_sock->state = SS_CONNECTED;

		tipc_set_portimportance(new_ref, msg_importance(msg));
		if (msg_named(msg)) {
			new_tport->conn_type = msg_nametype(msg);
			new_tport->conn_instance = msg_nameinst(msg);
		}


		if (!msg_data_sz(msg)) {
			struct msghdr m = {NULL,};

			advance_rx_queue(sk);
			send_packet(NULL, new_sock, &m, 0);
		} else {
			__skb_dequeue(&sk->sk_receive_queue);
			__skb_queue_head(&new_sk->sk_receive_queue, buf);
		}
		release_sock(new_sk);
	}
exit:
	release_sock(sk);
	return res;
}


static int shutdown(struct socket *sock, int how)
{
	struct sock *sk = sock->sk;
	struct tipc_port *tport = tipc_sk_port(sk);
	struct sk_buff *buf;
	int res;

	if (how != SHUT_RDWR)
		return -EINVAL;

	lock_sock(sk);

	switch (sock->state) {
	case SS_CONNECTING:
	case SS_CONNECTED:

		
restart:
		buf = __skb_dequeue(&sk->sk_receive_queue);
		if (buf) {
			atomic_dec(&tipc_queue_size);
			if (TIPC_SKB_CB(buf)->handle != 0) {
				kfree_skb(buf);
				goto restart;
			}
			tipc_disconnect(tport->ref);
			tipc_reject_msg(buf, TIPC_CONN_SHUTDOWN);
		} else {
			tipc_shutdown(tport->ref);
		}

		sock->state = SS_DISCONNECTING;

		

	case SS_DISCONNECTING:

		

		discard_rx_queue(sk);
		if (waitqueue_active(sk_sleep(sk)))
			wake_up_interruptible(sk_sleep(sk));
		res = 0;
		break;

	default:
		res = -ENOTCONN;
	}

	release_sock(sk);
	return res;
}


static int setsockopt(struct socket *sock,
		      int lvl, int opt, char __user *ov, unsigned int ol)
{
	struct sock *sk = sock->sk;
	struct tipc_port *tport = tipc_sk_port(sk);
	u32 value;
	int res;

	if ((lvl == IPPROTO_TCP) && (sock->type == SOCK_STREAM))
		return 0;
	if (lvl != SOL_TIPC)
		return -ENOPROTOOPT;
	if (ol < sizeof(value))
		return -EINVAL;
	res = get_user(value, (u32 __user *)ov);
	if (res)
		return res;

	lock_sock(sk);

	switch (opt) {
	case TIPC_IMPORTANCE:
		res = tipc_set_portimportance(tport->ref, value);
		break;
	case TIPC_SRC_DROPPABLE:
		if (sock->type != SOCK_STREAM)
			res = tipc_set_portunreliable(tport->ref, value);
		else
			res = -ENOPROTOOPT;
		break;
	case TIPC_DEST_DROPPABLE:
		res = tipc_set_portunreturnable(tport->ref, value);
		break;
	case TIPC_CONN_TIMEOUT:
		tipc_sk(sk)->conn_timeout = value;
		
		break;
	default:
		res = -EINVAL;
	}

	release_sock(sk);

	return res;
}


static int getsockopt(struct socket *sock,
		      int lvl, int opt, char __user *ov, int __user *ol)
{
	struct sock *sk = sock->sk;
	struct tipc_port *tport = tipc_sk_port(sk);
	int len;
	u32 value;
	int res;

	if ((lvl == IPPROTO_TCP) && (sock->type == SOCK_STREAM))
		return put_user(0, ol);
	if (lvl != SOL_TIPC)
		return -ENOPROTOOPT;
	res = get_user(len, ol);
	if (res)
		return res;

	lock_sock(sk);

	switch (opt) {
	case TIPC_IMPORTANCE:
		res = tipc_portimportance(tport->ref, &value);
		break;
	case TIPC_SRC_DROPPABLE:
		res = tipc_portunreliable(tport->ref, &value);
		break;
	case TIPC_DEST_DROPPABLE:
		res = tipc_portunreturnable(tport->ref, &value);
		break;
	case TIPC_CONN_TIMEOUT:
		value = tipc_sk(sk)->conn_timeout;
		
		break;
	case TIPC_NODE_RECVQ_DEPTH:
		value = (u32)atomic_read(&tipc_queue_size);
		break;
	case TIPC_SOCK_RECVQ_DEPTH:
		value = skb_queue_len(&sk->sk_receive_queue);
		break;
	default:
		res = -EINVAL;
	}

	release_sock(sk);

	if (res)
		return res;	

	if (len < sizeof(value))
		return -EINVAL;

	if (copy_to_user(ov, &value, sizeof(value)))
		return -EFAULT;

	return put_user(sizeof(value), ol);
}


static const struct proto_ops msg_ops = {
	.owner		= THIS_MODULE,
	.family		= AF_TIPC,
	.release	= release,
	.bind		= bind,
	.connect	= connect,
	.socketpair	= sock_no_socketpair,
	.accept		= sock_no_accept,
	.getname	= get_name,
	.poll		= poll,
	.ioctl		= sock_no_ioctl,
	.listen		= sock_no_listen,
	.shutdown	= shutdown,
	.setsockopt	= setsockopt,
	.getsockopt	= getsockopt,
	.sendmsg	= send_msg,
	.recvmsg	= recv_msg,
	.mmap		= sock_no_mmap,
	.sendpage	= sock_no_sendpage
};

static const struct proto_ops packet_ops = {
	.owner		= THIS_MODULE,
	.family		= AF_TIPC,
	.release	= release,
	.bind		= bind,
	.connect	= connect,
	.socketpair	= sock_no_socketpair,
	.accept		= accept,
	.getname	= get_name,
	.poll		= poll,
	.ioctl		= sock_no_ioctl,
	.listen		= listen,
	.shutdown	= shutdown,
	.setsockopt	= setsockopt,
	.getsockopt	= getsockopt,
	.sendmsg	= send_packet,
	.recvmsg	= recv_msg,
	.mmap		= sock_no_mmap,
	.sendpage	= sock_no_sendpage
};

static const struct proto_ops stream_ops = {
	.owner		= THIS_MODULE,
	.family		= AF_TIPC,
	.release	= release,
	.bind		= bind,
	.connect	= connect,
	.socketpair	= sock_no_socketpair,
	.accept		= accept,
	.getname	= get_name,
	.poll		= poll,
	.ioctl		= sock_no_ioctl,
	.listen		= listen,
	.shutdown	= shutdown,
	.setsockopt	= setsockopt,
	.getsockopt	= getsockopt,
	.sendmsg	= send_stream,
	.recvmsg	= recv_stream,
	.mmap		= sock_no_mmap,
	.sendpage	= sock_no_sendpage
};

static const struct net_proto_family tipc_family_ops = {
	.owner		= THIS_MODULE,
	.family		= AF_TIPC,
	.create		= tipc_create
};

static struct proto tipc_proto = {
	.name		= "TIPC",
	.owner		= THIS_MODULE,
	.obj_size	= sizeof(struct tipc_sock)
};

int tipc_socket_init(void)
{
	int res;

	res = proto_register(&tipc_proto, 1);
	if (res) {
		err("Failed to register TIPC protocol type\n");
		goto out;
	}

	res = sock_register(&tipc_family_ops);
	if (res) {
		err("Failed to register TIPC socket type\n");
		proto_unregister(&tipc_proto);
		goto out;
	}

	sockets_enabled = 1;
 out:
	return res;
}


void tipc_socket_stop(void)
{
	if (!sockets_enabled)
		return;

	sockets_enabled = 0;
	sock_unregister(tipc_family_ops.family);
	proto_unregister(&tipc_proto);
}

