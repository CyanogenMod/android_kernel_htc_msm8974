/*
 * Copyright (c) 2006 Oracle.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/gfp.h>
#include <net/sock.h>
#include <linux/in.h>
#include <linux/list.h>
#include <linux/ratelimit.h>
#include <linux/export.h>

#include "rds.h"

static int send_batch_count = 64;
module_param(send_batch_count, int, 0444);
MODULE_PARM_DESC(send_batch_count, " batch factor when working the send queue");

static void rds_send_remove_from_sock(struct list_head *messages, int status);

void rds_send_reset(struct rds_connection *conn)
{
	struct rds_message *rm, *tmp;
	unsigned long flags;

	if (conn->c_xmit_rm) {
		rm = conn->c_xmit_rm;
		conn->c_xmit_rm = NULL;
		rds_message_unmapped(rm);
		rds_message_put(rm);
	}

	conn->c_xmit_sg = 0;
	conn->c_xmit_hdr_off = 0;
	conn->c_xmit_data_off = 0;
	conn->c_xmit_atomic_sent = 0;
	conn->c_xmit_rdma_sent = 0;
	conn->c_xmit_data_sent = 0;

	conn->c_map_queued = 0;

	conn->c_unacked_packets = rds_sysctl_max_unacked_packets;
	conn->c_unacked_bytes = rds_sysctl_max_unacked_bytes;

	
	spin_lock_irqsave(&conn->c_lock, flags);
	list_for_each_entry_safe(rm, tmp, &conn->c_retrans, m_conn_item) {
		set_bit(RDS_MSG_ACK_REQUIRED, &rm->m_flags);
		set_bit(RDS_MSG_RETRANSMITTED, &rm->m_flags);
	}
	list_splice_init(&conn->c_retrans, &conn->c_send_queue);
	spin_unlock_irqrestore(&conn->c_lock, flags);
}

static int acquire_in_xmit(struct rds_connection *conn)
{
	return test_and_set_bit(RDS_IN_XMIT, &conn->c_flags) == 0;
}

static void release_in_xmit(struct rds_connection *conn)
{
	clear_bit(RDS_IN_XMIT, &conn->c_flags);
	smp_mb__after_clear_bit();
	if (waitqueue_active(&conn->c_waitq))
		wake_up_all(&conn->c_waitq);
}

int rds_send_xmit(struct rds_connection *conn)
{
	struct rds_message *rm;
	unsigned long flags;
	unsigned int tmp;
	struct scatterlist *sg;
	int ret = 0;
	LIST_HEAD(to_be_dropped);

restart:

	if (!acquire_in_xmit(conn)) {
		rds_stats_inc(s_send_lock_contention);
		ret = -ENOMEM;
		goto out;
	}

	if (!rds_conn_up(conn)) {
		release_in_xmit(conn);
		ret = 0;
		goto out;
	}

	if (conn->c_trans->xmit_prepare)
		conn->c_trans->xmit_prepare(conn);

	while (1) {

		rm = conn->c_xmit_rm;

		if (!rm && test_and_clear_bit(0, &conn->c_map_queued)) {
			rm = rds_cong_update_alloc(conn);
			if (IS_ERR(rm)) {
				ret = PTR_ERR(rm);
				break;
			}
			rm->data.op_active = 1;

			conn->c_xmit_rm = rm;
		}

		if (!rm) {
			unsigned int len;

			spin_lock_irqsave(&conn->c_lock, flags);

			if (!list_empty(&conn->c_send_queue)) {
				rm = list_entry(conn->c_send_queue.next,
						struct rds_message,
						m_conn_item);
				rds_message_addref(rm);

				list_move_tail(&rm->m_conn_item, &conn->c_retrans);
			}

			spin_unlock_irqrestore(&conn->c_lock, flags);

			if (!rm)
				break;

			if (rm->rdma.op_active &&
			    test_bit(RDS_MSG_RETRANSMITTED, &rm->m_flags)) {
				spin_lock_irqsave(&conn->c_lock, flags);
				if (test_and_clear_bit(RDS_MSG_ON_CONN, &rm->m_flags))
					list_move(&rm->m_conn_item, &to_be_dropped);
				spin_unlock_irqrestore(&conn->c_lock, flags);
				continue;
			}

			
			len = ntohl(rm->m_inc.i_hdr.h_len);
			if (conn->c_unacked_packets == 0 ||
			    conn->c_unacked_bytes < len) {
				__set_bit(RDS_MSG_ACK_REQUIRED, &rm->m_flags);

				conn->c_unacked_packets = rds_sysctl_max_unacked_packets;
				conn->c_unacked_bytes = rds_sysctl_max_unacked_bytes;
				rds_stats_inc(s_send_ack_required);
			} else {
				conn->c_unacked_bytes -= len;
				conn->c_unacked_packets--;
			}

			conn->c_xmit_rm = rm;
		}

		
		if (rm->rdma.op_active && !conn->c_xmit_rdma_sent) {
			rm->m_final_op = &rm->rdma;
			ret = conn->c_trans->xmit_rdma(conn, &rm->rdma);
			if (ret)
				break;
			conn->c_xmit_rdma_sent = 1;

			set_bit(RDS_MSG_MAPPED, &rm->m_flags);
		}

		if (rm->atomic.op_active && !conn->c_xmit_atomic_sent) {
			rm->m_final_op = &rm->atomic;
			ret = conn->c_trans->xmit_atomic(conn, &rm->atomic);
			if (ret)
				break;
			conn->c_xmit_atomic_sent = 1;

			set_bit(RDS_MSG_MAPPED, &rm->m_flags);
		}

		if (rm->data.op_nents == 0) {
			int ops_present;
			int all_ops_are_silent = 1;

			ops_present = (rm->atomic.op_active || rm->rdma.op_active);
			if (rm->atomic.op_active && !rm->atomic.op_silent)
				all_ops_are_silent = 0;
			if (rm->rdma.op_active && !rm->rdma.op_silent)
				all_ops_are_silent = 0;

			if (ops_present && all_ops_are_silent
			    && !rm->m_rdma_cookie)
				rm->data.op_active = 0;
		}

		if (rm->data.op_active && !conn->c_xmit_data_sent) {
			rm->m_final_op = &rm->data;
			ret = conn->c_trans->xmit(conn, rm,
						  conn->c_xmit_hdr_off,
						  conn->c_xmit_sg,
						  conn->c_xmit_data_off);
			if (ret <= 0)
				break;

			if (conn->c_xmit_hdr_off < sizeof(struct rds_header)) {
				tmp = min_t(int, ret,
					    sizeof(struct rds_header) -
					    conn->c_xmit_hdr_off);
				conn->c_xmit_hdr_off += tmp;
				ret -= tmp;
			}

			sg = &rm->data.op_sg[conn->c_xmit_sg];
			while (ret) {
				tmp = min_t(int, ret, sg->length -
						      conn->c_xmit_data_off);
				conn->c_xmit_data_off += tmp;
				ret -= tmp;
				if (conn->c_xmit_data_off == sg->length) {
					conn->c_xmit_data_off = 0;
					sg++;
					conn->c_xmit_sg++;
					BUG_ON(ret != 0 &&
					       conn->c_xmit_sg == rm->data.op_nents);
				}
			}

			if (conn->c_xmit_hdr_off == sizeof(struct rds_header) &&
			    (conn->c_xmit_sg == rm->data.op_nents))
				conn->c_xmit_data_sent = 1;
		}

		if (!rm->data.op_active || conn->c_xmit_data_sent) {
			conn->c_xmit_rm = NULL;
			conn->c_xmit_sg = 0;
			conn->c_xmit_hdr_off = 0;
			conn->c_xmit_data_off = 0;
			conn->c_xmit_rdma_sent = 0;
			conn->c_xmit_atomic_sent = 0;
			conn->c_xmit_data_sent = 0;

			rds_message_put(rm);
		}
	}

	if (conn->c_trans->xmit_complete)
		conn->c_trans->xmit_complete(conn);

	release_in_xmit(conn);

	
	if (!list_empty(&to_be_dropped)) {
		
		list_for_each_entry(rm, &to_be_dropped, m_conn_item)
			rds_message_put(rm);
		rds_send_remove_from_sock(&to_be_dropped, RDS_RDMA_DROPPED);
	}

	if (ret == 0) {
		smp_mb();
		if (!list_empty(&conn->c_send_queue)) {
			rds_stats_inc(s_send_lock_queue_raced);
			goto restart;
		}
	}
out:
	return ret;
}

static void rds_send_sndbuf_remove(struct rds_sock *rs, struct rds_message *rm)
{
	u32 len = be32_to_cpu(rm->m_inc.i_hdr.h_len);

	assert_spin_locked(&rs->rs_lock);

	BUG_ON(rs->rs_snd_bytes < len);
	rs->rs_snd_bytes -= len;

	if (rs->rs_snd_bytes == 0)
		rds_stats_inc(s_send_queue_empty);
}

static inline int rds_send_is_acked(struct rds_message *rm, u64 ack,
				    is_acked_func is_acked)
{
	if (is_acked)
		return is_acked(rm, ack);
	return be64_to_cpu(rm->m_inc.i_hdr.h_sequence) <= ack;
}

void rds_rdma_send_complete(struct rds_message *rm, int status)
{
	struct rds_sock *rs = NULL;
	struct rm_rdma_op *ro;
	struct rds_notifier *notifier;
	unsigned long flags;

	spin_lock_irqsave(&rm->m_rs_lock, flags);

	ro = &rm->rdma;
	if (test_bit(RDS_MSG_ON_SOCK, &rm->m_flags) &&
	    ro->op_active && ro->op_notify && ro->op_notifier) {
		notifier = ro->op_notifier;
		rs = rm->m_rs;
		sock_hold(rds_rs_to_sk(rs));

		notifier->n_status = status;
		spin_lock(&rs->rs_lock);
		list_add_tail(&notifier->n_list, &rs->rs_notify_queue);
		spin_unlock(&rs->rs_lock);

		ro->op_notifier = NULL;
	}

	spin_unlock_irqrestore(&rm->m_rs_lock, flags);

	if (rs) {
		rds_wake_sk_sleep(rs);
		sock_put(rds_rs_to_sk(rs));
	}
}
EXPORT_SYMBOL_GPL(rds_rdma_send_complete);

void rds_atomic_send_complete(struct rds_message *rm, int status)
{
	struct rds_sock *rs = NULL;
	struct rm_atomic_op *ao;
	struct rds_notifier *notifier;
	unsigned long flags;

	spin_lock_irqsave(&rm->m_rs_lock, flags);

	ao = &rm->atomic;
	if (test_bit(RDS_MSG_ON_SOCK, &rm->m_flags)
	    && ao->op_active && ao->op_notify && ao->op_notifier) {
		notifier = ao->op_notifier;
		rs = rm->m_rs;
		sock_hold(rds_rs_to_sk(rs));

		notifier->n_status = status;
		spin_lock(&rs->rs_lock);
		list_add_tail(&notifier->n_list, &rs->rs_notify_queue);
		spin_unlock(&rs->rs_lock);

		ao->op_notifier = NULL;
	}

	spin_unlock_irqrestore(&rm->m_rs_lock, flags);

	if (rs) {
		rds_wake_sk_sleep(rs);
		sock_put(rds_rs_to_sk(rs));
	}
}
EXPORT_SYMBOL_GPL(rds_atomic_send_complete);

static inline void
__rds_send_complete(struct rds_sock *rs, struct rds_message *rm, int status)
{
	struct rm_rdma_op *ro;
	struct rm_atomic_op *ao;

	ro = &rm->rdma;
	if (ro->op_active && ro->op_notify && ro->op_notifier) {
		ro->op_notifier->n_status = status;
		list_add_tail(&ro->op_notifier->n_list, &rs->rs_notify_queue);
		ro->op_notifier = NULL;
	}

	ao = &rm->atomic;
	if (ao->op_active && ao->op_notify && ao->op_notifier) {
		ao->op_notifier->n_status = status;
		list_add_tail(&ao->op_notifier->n_list, &rs->rs_notify_queue);
		ao->op_notifier = NULL;
	}

	
}

struct rds_message *rds_send_get_message(struct rds_connection *conn,
					 struct rm_rdma_op *op)
{
	struct rds_message *rm, *tmp, *found = NULL;
	unsigned long flags;

	spin_lock_irqsave(&conn->c_lock, flags);

	list_for_each_entry_safe(rm, tmp, &conn->c_retrans, m_conn_item) {
		if (&rm->rdma == op) {
			atomic_inc(&rm->m_refcount);
			found = rm;
			goto out;
		}
	}

	list_for_each_entry_safe(rm, tmp, &conn->c_send_queue, m_conn_item) {
		if (&rm->rdma == op) {
			atomic_inc(&rm->m_refcount);
			found = rm;
			break;
		}
	}

out:
	spin_unlock_irqrestore(&conn->c_lock, flags);

	return found;
}
EXPORT_SYMBOL_GPL(rds_send_get_message);

static void rds_send_remove_from_sock(struct list_head *messages, int status)
{
	unsigned long flags;
	struct rds_sock *rs = NULL;
	struct rds_message *rm;

	while (!list_empty(messages)) {
		int was_on_sock = 0;

		rm = list_entry(messages->next, struct rds_message,
				m_conn_item);
		list_del_init(&rm->m_conn_item);

		spin_lock_irqsave(&rm->m_rs_lock, flags);
		if (!test_bit(RDS_MSG_ON_SOCK, &rm->m_flags))
			goto unlock_and_drop;

		if (rs != rm->m_rs) {
			if (rs) {
				rds_wake_sk_sleep(rs);
				sock_put(rds_rs_to_sk(rs));
			}
			rs = rm->m_rs;
			sock_hold(rds_rs_to_sk(rs));
		}
		spin_lock(&rs->rs_lock);

		if (test_and_clear_bit(RDS_MSG_ON_SOCK, &rm->m_flags)) {
			struct rm_rdma_op *ro = &rm->rdma;
			struct rds_notifier *notifier;

			list_del_init(&rm->m_sock_item);
			rds_send_sndbuf_remove(rs, rm);

			if (ro->op_active && ro->op_notifier &&
			       (ro->op_notify || (ro->op_recverr && status))) {
				notifier = ro->op_notifier;
				list_add_tail(&notifier->n_list,
						&rs->rs_notify_queue);
				if (!notifier->n_status)
					notifier->n_status = status;
				rm->rdma.op_notifier = NULL;
			}
			was_on_sock = 1;
			rm->m_rs = NULL;
		}
		spin_unlock(&rs->rs_lock);

unlock_and_drop:
		spin_unlock_irqrestore(&rm->m_rs_lock, flags);
		rds_message_put(rm);
		if (was_on_sock)
			rds_message_put(rm);
	}

	if (rs) {
		rds_wake_sk_sleep(rs);
		sock_put(rds_rs_to_sk(rs));
	}
}

void rds_send_drop_acked(struct rds_connection *conn, u64 ack,
			 is_acked_func is_acked)
{
	struct rds_message *rm, *tmp;
	unsigned long flags;
	LIST_HEAD(list);

	spin_lock_irqsave(&conn->c_lock, flags);

	list_for_each_entry_safe(rm, tmp, &conn->c_retrans, m_conn_item) {
		if (!rds_send_is_acked(rm, ack, is_acked))
			break;

		list_move(&rm->m_conn_item, &list);
		clear_bit(RDS_MSG_ON_CONN, &rm->m_flags);
	}

	
	if (!list_empty(&list))
		smp_mb__after_clear_bit();

	spin_unlock_irqrestore(&conn->c_lock, flags);

	
	rds_send_remove_from_sock(&list, RDS_RDMA_SUCCESS);
}
EXPORT_SYMBOL_GPL(rds_send_drop_acked);

void rds_send_drop_to(struct rds_sock *rs, struct sockaddr_in *dest)
{
	struct rds_message *rm, *tmp;
	struct rds_connection *conn;
	unsigned long flags;
	LIST_HEAD(list);

	
	spin_lock_irqsave(&rs->rs_lock, flags);

	list_for_each_entry_safe(rm, tmp, &rs->rs_send_queue, m_sock_item) {
		if (dest && (dest->sin_addr.s_addr != rm->m_daddr ||
			     dest->sin_port != rm->m_inc.i_hdr.h_dport))
			continue;

		list_move(&rm->m_sock_item, &list);
		rds_send_sndbuf_remove(rs, rm);
		clear_bit(RDS_MSG_ON_SOCK, &rm->m_flags);
	}

	
	smp_mb__after_clear_bit();

	spin_unlock_irqrestore(&rs->rs_lock, flags);

	if (list_empty(&list))
		return;

	
	list_for_each_entry(rm, &list, m_sock_item) {

		conn = rm->m_inc.i_conn;

		spin_lock_irqsave(&conn->c_lock, flags);
		if (!test_and_clear_bit(RDS_MSG_ON_CONN, &rm->m_flags)) {
			spin_unlock_irqrestore(&conn->c_lock, flags);
			continue;
		}
		list_del_init(&rm->m_conn_item);
		spin_unlock_irqrestore(&conn->c_lock, flags);

		spin_lock_irqsave(&rm->m_rs_lock, flags);

		spin_lock(&rs->rs_lock);
		__rds_send_complete(rs, rm, RDS_RDMA_CANCELED);
		spin_unlock(&rs->rs_lock);

		rm->m_rs = NULL;
		spin_unlock_irqrestore(&rm->m_rs_lock, flags);

		rds_message_put(rm);
	}

	rds_wake_sk_sleep(rs);

	while (!list_empty(&list)) {
		rm = list_entry(list.next, struct rds_message, m_sock_item);
		list_del_init(&rm->m_sock_item);

		rds_message_wait(rm);
		rds_message_put(rm);
	}
}

static int rds_send_queue_rm(struct rds_sock *rs, struct rds_connection *conn,
			     struct rds_message *rm, __be16 sport,
			     __be16 dport, int *queued)
{
	unsigned long flags;
	u32 len;

	if (*queued)
		goto out;

	len = be32_to_cpu(rm->m_inc.i_hdr.h_len);

	spin_lock_irqsave(&rs->rs_lock, flags);

	if (rs->rs_snd_bytes < rds_sk_sndbuf(rs)) {
		rs->rs_snd_bytes += len;

		if (rs->rs_snd_bytes >= rds_sk_sndbuf(rs) / 2)
			__set_bit(RDS_MSG_ACK_REQUIRED, &rm->m_flags);

		list_add_tail(&rm->m_sock_item, &rs->rs_send_queue);
		set_bit(RDS_MSG_ON_SOCK, &rm->m_flags);
		rds_message_addref(rm);
		rm->m_rs = rs;

		rds_message_populate_header(&rm->m_inc.i_hdr, sport, dport, 0);
		rm->m_inc.i_conn = conn;
		rds_message_addref(rm);

		spin_lock(&conn->c_lock);
		rm->m_inc.i_hdr.h_sequence = cpu_to_be64(conn->c_next_tx_seq++);
		list_add_tail(&rm->m_conn_item, &conn->c_send_queue);
		set_bit(RDS_MSG_ON_CONN, &rm->m_flags);
		spin_unlock(&conn->c_lock);

		rdsdebug("queued msg %p len %d, rs %p bytes %d seq %llu\n",
			 rm, len, rs, rs->rs_snd_bytes,
			 (unsigned long long)be64_to_cpu(rm->m_inc.i_hdr.h_sequence));

		*queued = 1;
	}

	spin_unlock_irqrestore(&rs->rs_lock, flags);
out:
	return *queued;
}

static int rds_rm_size(struct msghdr *msg, int data_len)
{
	struct cmsghdr *cmsg;
	int size = 0;
	int cmsg_groups = 0;
	int retval;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (!CMSG_OK(msg, cmsg))
			return -EINVAL;

		if (cmsg->cmsg_level != SOL_RDS)
			continue;

		switch (cmsg->cmsg_type) {
		case RDS_CMSG_RDMA_ARGS:
			cmsg_groups |= 1;
			retval = rds_rdma_extra_size(CMSG_DATA(cmsg));
			if (retval < 0)
				return retval;
			size += retval;

			break;

		case RDS_CMSG_RDMA_DEST:
		case RDS_CMSG_RDMA_MAP:
			cmsg_groups |= 2;
			
			break;

		case RDS_CMSG_ATOMIC_CSWP:
		case RDS_CMSG_ATOMIC_FADD:
		case RDS_CMSG_MASKED_ATOMIC_CSWP:
		case RDS_CMSG_MASKED_ATOMIC_FADD:
			cmsg_groups |= 1;
			size += sizeof(struct scatterlist);
			break;

		default:
			return -EINVAL;
		}

	}

	size += ceil(data_len, PAGE_SIZE) * sizeof(struct scatterlist);

	
	if (cmsg_groups == 3)
		return -EINVAL;

	return size;
}

static int rds_cmsg_send(struct rds_sock *rs, struct rds_message *rm,
			 struct msghdr *msg, int *allocated_mr)
{
	struct cmsghdr *cmsg;
	int ret = 0;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (!CMSG_OK(msg, cmsg))
			return -EINVAL;

		if (cmsg->cmsg_level != SOL_RDS)
			continue;

		switch (cmsg->cmsg_type) {
		case RDS_CMSG_RDMA_ARGS:
			ret = rds_cmsg_rdma_args(rs, rm, cmsg);
			break;

		case RDS_CMSG_RDMA_DEST:
			ret = rds_cmsg_rdma_dest(rs, rm, cmsg);
			break;

		case RDS_CMSG_RDMA_MAP:
			ret = rds_cmsg_rdma_map(rs, rm, cmsg);
			if (!ret)
				*allocated_mr = 1;
			break;
		case RDS_CMSG_ATOMIC_CSWP:
		case RDS_CMSG_ATOMIC_FADD:
		case RDS_CMSG_MASKED_ATOMIC_CSWP:
		case RDS_CMSG_MASKED_ATOMIC_FADD:
			ret = rds_cmsg_atomic(rs, rm, cmsg);
			break;

		default:
			return -EINVAL;
		}

		if (ret)
			break;
	}

	return ret;
}

int rds_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg,
		size_t payload_len)
{
	struct sock *sk = sock->sk;
	struct rds_sock *rs = rds_sk_to_rs(sk);
	struct sockaddr_in *usin = (struct sockaddr_in *)msg->msg_name;
	__be32 daddr;
	__be16 dport;
	struct rds_message *rm = NULL;
	struct rds_connection *conn;
	int ret = 0;
	int queued = 0, allocated_mr = 0;
	int nonblock = msg->msg_flags & MSG_DONTWAIT;
	long timeo = sock_sndtimeo(sk, nonblock);

	
	
	if (msg->msg_flags & ~(MSG_DONTWAIT | MSG_CMSG_COMPAT)) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (msg->msg_namelen) {
		
		if (msg->msg_namelen < sizeof(*usin) || usin->sin_family != AF_INET) {
			ret = -EINVAL;
			goto out;
		}
		daddr = usin->sin_addr.s_addr;
		dport = usin->sin_port;
	} else {
		
		lock_sock(sk);
		daddr = rs->rs_conn_addr;
		dport = rs->rs_conn_port;
		release_sock(sk);
	}

	
	if (daddr == 0 || rs->rs_bound_addr == 0) {
		ret = -ENOTCONN; 
		goto out;
	}

	
	ret = rds_rm_size(msg, payload_len);
	if (ret < 0)
		goto out;

	rm = rds_message_alloc(ret, GFP_KERNEL);
	if (!rm) {
		ret = -ENOMEM;
		goto out;
	}

	
	if (payload_len) {
		rm->data.op_sg = rds_message_alloc_sgs(rm, ceil(payload_len, PAGE_SIZE));
		if (!rm->data.op_sg) {
			ret = -ENOMEM;
			goto out;
		}
		ret = rds_message_copy_from_user(rm, msg->msg_iov, payload_len);
		if (ret)
			goto out;
	}
	rm->data.op_active = 1;

	rm->m_daddr = daddr;

	if (rs->rs_conn && rs->rs_conn->c_faddr == daddr)
		conn = rs->rs_conn;
	else {
		conn = rds_conn_create_outgoing(rs->rs_bound_addr, daddr,
					rs->rs_transport,
					sock->sk->sk_allocation);
		if (IS_ERR(conn)) {
			ret = PTR_ERR(conn);
			goto out;
		}
		rs->rs_conn = conn;
	}

	
	ret = rds_cmsg_send(rs, rm, msg, &allocated_mr);
	if (ret)
		goto out;

	if (rm->rdma.op_active && !conn->c_trans->xmit_rdma) {
		printk_ratelimited(KERN_NOTICE "rdma_op %p conn xmit_rdma %p\n",
			       &rm->rdma, conn->c_trans->xmit_rdma);
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (rm->atomic.op_active && !conn->c_trans->xmit_atomic) {
		printk_ratelimited(KERN_NOTICE "atomic_op %p conn xmit_atomic %p\n",
			       &rm->atomic, conn->c_trans->xmit_atomic);
		ret = -EOPNOTSUPP;
		goto out;
	}

	rds_conn_connect_if_down(conn);

	ret = rds_cong_wait(conn->c_fcong, dport, nonblock, rs);
	if (ret) {
		rs->rs_seen_congestion = 1;
		goto out;
	}

	while (!rds_send_queue_rm(rs, conn, rm, rs->rs_bound_port,
				  dport, &queued)) {
		rds_stats_inc(s_send_queue_full);
		
		if (payload_len > rds_sk_sndbuf(rs)) {
			ret = -EMSGSIZE;
			goto out;
		}
		if (nonblock) {
			ret = -EAGAIN;
			goto out;
		}

		timeo = wait_event_interruptible_timeout(*sk_sleep(sk),
					rds_send_queue_rm(rs, conn, rm,
							  rs->rs_bound_port,
							  dport,
							  &queued),
					timeo);
		rdsdebug("sendmsg woke queued %d timeo %ld\n", queued, timeo);
		if (timeo > 0 || timeo == MAX_SCHEDULE_TIMEOUT)
			continue;

		ret = timeo;
		if (ret == 0)
			ret = -ETIMEDOUT;
		goto out;
	}

	rds_stats_inc(s_send_queued);

	if (!test_bit(RDS_LL_SEND_FULL, &conn->c_flags))
		rds_send_xmit(conn);

	rds_message_put(rm);
	return payload_len;

out:
	if (allocated_mr)
		rds_rdma_unuse(rs, rds_rdma_cookie_key(rm->m_rdma_cookie), 1);

	if (rm)
		rds_message_put(rm);
	return ret;
}

int
rds_send_pong(struct rds_connection *conn, __be16 dport)
{
	struct rds_message *rm;
	unsigned long flags;
	int ret = 0;

	rm = rds_message_alloc(0, GFP_ATOMIC);
	if (!rm) {
		ret = -ENOMEM;
		goto out;
	}

	rm->m_daddr = conn->c_faddr;
	rm->data.op_active = 1;

	rds_conn_connect_if_down(conn);

	ret = rds_cong_wait(conn->c_fcong, dport, 1, NULL);
	if (ret)
		goto out;

	spin_lock_irqsave(&conn->c_lock, flags);
	list_add_tail(&rm->m_conn_item, &conn->c_send_queue);
	set_bit(RDS_MSG_ON_CONN, &rm->m_flags);
	rds_message_addref(rm);
	rm->m_inc.i_conn = conn;

	rds_message_populate_header(&rm->m_inc.i_hdr, 0, dport,
				    conn->c_next_tx_seq);
	conn->c_next_tx_seq++;
	spin_unlock_irqrestore(&conn->c_lock, flags);

	rds_stats_inc(s_send_queued);
	rds_stats_inc(s_send_pong);

	if (!test_bit(RDS_LL_SEND_FULL, &conn->c_flags))
		rds_send_xmit(conn);

	rds_message_put(rm);
	return 0;

out:
	if (rm)
		rds_message_put(rm);
	return ret;
}
