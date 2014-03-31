/*
 *   fs/cifs/transport.c
 *
 *   Copyright (C) International Business Machines  Corp., 2002,2008
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *   Jeremy Allison (jra@samba.org) 2006.
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/fs.h>
#include <linux/list.h>
#include <linux/gfp.h>
#include <linux/wait.h>
#include <linux/net.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <asm/uaccess.h>
#include <asm/processor.h>
#include <linux/mempool.h>
#include "cifspdu.h"
#include "cifsglob.h"
#include "cifsproto.h"
#include "cifs_debug.h"

extern mempool_t *cifs_mid_poolp;

static void
wake_up_task(struct mid_q_entry *mid)
{
	wake_up_process(mid->callback_data);
}

struct mid_q_entry *
AllocMidQEntry(const struct smb_hdr *smb_buffer, struct TCP_Server_Info *server)
{
	struct mid_q_entry *temp;

	if (server == NULL) {
		cERROR(1, "Null TCP session in AllocMidQEntry");
		return NULL;
	}

	temp = mempool_alloc(cifs_mid_poolp, GFP_NOFS);
	if (temp == NULL)
		return temp;
	else {
		memset(temp, 0, sizeof(struct mid_q_entry));
		temp->mid = smb_buffer->Mid;	
		temp->pid = current->pid;
		temp->command = cpu_to_le16(smb_buffer->Command);
		cFYI(1, "For smb_command %d", smb_buffer->Command);
	 
		
		temp->when_alloc = jiffies;

		temp->callback = wake_up_task;
		temp->callback_data = current;
	}

	atomic_inc(&midCount);
	temp->mid_state = MID_REQUEST_ALLOCATED;
	return temp;
}

void
DeleteMidQEntry(struct mid_q_entry *midEntry)
{
#ifdef CONFIG_CIFS_STATS2
	unsigned long now;
#endif
	midEntry->mid_state = MID_FREE;
	atomic_dec(&midCount);
	if (midEntry->large_buf)
		cifs_buf_release(midEntry->resp_buf);
	else
		cifs_small_buf_release(midEntry->resp_buf);
#ifdef CONFIG_CIFS_STATS2
	now = jiffies;
	if ((now - midEntry->when_alloc) > HZ) {
		if ((cifsFYI & CIFS_TIMER) &&
		    (midEntry->command != cpu_to_le16(SMB_COM_LOCKING_ANDX))) {
			printk(KERN_DEBUG " CIFS slow rsp: cmd %d mid %llu",
			       midEntry->command, midEntry->mid);
			printk(" A: 0x%lx S: 0x%lx R: 0x%lx\n",
			       now - midEntry->when_alloc,
			       now - midEntry->when_sent,
			       now - midEntry->when_received);
		}
	}
#endif
	mempool_free(midEntry, cifs_mid_poolp);
}

static void
delete_mid(struct mid_q_entry *mid)
{
	spin_lock(&GlobalMid_Lock);
	list_del(&mid->qhead);
	spin_unlock(&GlobalMid_Lock);

	DeleteMidQEntry(mid);
}

static int
smb_sendv(struct TCP_Server_Info *server, struct kvec *iov, int n_vec)
{
	int rc = 0;
	int i = 0;
	struct msghdr smb_msg;
	__be32 *buf_len = (__be32 *)(iov[0].iov_base);
	unsigned int len = iov[0].iov_len;
	unsigned int total_len;
	int first_vec = 0;
	unsigned int smb_buf_length = get_rfc1002_length(iov[0].iov_base);
	struct socket *ssocket = server->ssocket;

	if (ssocket == NULL)
		return -ENOTSOCK; 

	smb_msg.msg_name = (struct sockaddr *) &server->dstaddr;
	smb_msg.msg_namelen = sizeof(struct sockaddr);
	smb_msg.msg_control = NULL;
	smb_msg.msg_controllen = 0;
	if (server->noblocksnd)
		smb_msg.msg_flags = MSG_DONTWAIT + MSG_NOSIGNAL;
	else
		smb_msg.msg_flags = MSG_NOSIGNAL;

	total_len = 0;
	for (i = 0; i < n_vec; i++)
		total_len += iov[i].iov_len;

	cFYI(1, "Sending smb:  total_len %d", total_len);
	dump_smb(iov[0].iov_base, len);

	i = 0;
	while (total_len) {
		rc = kernel_sendmsg(ssocket, &smb_msg, &iov[first_vec],
				    n_vec - first_vec, total_len);
		if ((rc == -ENOSPC) || (rc == -EAGAIN)) {
			i++;
			if ((i >= 14) || (!server->noblocksnd && (i > 2))) {
				cERROR(1, "sends on sock %p stuck for 15 seconds",
				    ssocket);
				rc = -EAGAIN;
				break;
			}
			msleep(1 << i);
			continue;
		}
		if (rc < 0)
			break;

		if (rc == total_len) {
			total_len = 0;
			break;
		} else if (rc > total_len) {
			cERROR(1, "sent %d requested %d", rc, total_len);
			break;
		}
		if (rc == 0) {
			cERROR(1, "tcp sent no data");
			msleep(500);
			continue;
		}
		total_len -= rc;
		
		for (i = first_vec; i < n_vec; i++) {
			if (iov[i].iov_len) {
				if (rc > iov[i].iov_len) {
					rc -= iov[i].iov_len;
					iov[i].iov_len = 0;
				} else {
					iov[i].iov_base += rc;
					iov[i].iov_len -= rc;
					first_vec = i;
					break;
				}
			}
		}
		i = 0; 
	}

	if ((total_len > 0) && (total_len != smb_buf_length + 4)) {
		cFYI(1, "partial send (%d remaining), terminating session",
			total_len);
		server->tcpStatus = CifsNeedReconnect;
	}

	if (rc < 0 && rc != -EINTR)
		cERROR(1, "Error %d sending data on socket to server", rc);
	else
		rc = 0;

	
	*buf_len = cpu_to_be32(smb_buf_length);

	return rc;
}

int
smb_send(struct TCP_Server_Info *server, struct smb_hdr *smb_buffer,
	 unsigned int smb_buf_length)
{
	struct kvec iov;

	iov.iov_base = smb_buffer;
	iov.iov_len = smb_buf_length + 4;

	return smb_sendv(server, &iov, 1);
}

static int
wait_for_free_credits(struct TCP_Server_Info *server, const int optype,
		      int *credits)
{
	int rc;

	spin_lock(&server->req_lock);
	if (optype == CIFS_ASYNC_OP) {
		
		server->in_flight++;
		*credits -= 1;
		spin_unlock(&server->req_lock);
		return 0;
	}

	while (1) {
		if (*credits <= 0) {
			spin_unlock(&server->req_lock);
			cifs_num_waiters_inc(server);
			rc = wait_event_killable(server->request_q,
						 has_credits(server, credits));
			cifs_num_waiters_dec(server);
			if (rc)
				return rc;
			spin_lock(&server->req_lock);
		} else {
			if (server->tcpStatus == CifsExiting) {
				spin_unlock(&server->req_lock);
				return -ENOENT;
			}


			
			if (optype != CIFS_BLOCKING_OP) {
				*credits -= 1;
				server->in_flight++;
			}
			spin_unlock(&server->req_lock);
			break;
		}
	}
	return 0;
}

static int
wait_for_free_request(struct TCP_Server_Info *server, const int optype)
{
	return wait_for_free_credits(server, optype, get_credits_field(server));
}

static int allocate_mid(struct cifs_ses *ses, struct smb_hdr *in_buf,
			struct mid_q_entry **ppmidQ)
{
	if (ses->server->tcpStatus == CifsExiting) {
		return -ENOENT;
	}

	if (ses->server->tcpStatus == CifsNeedReconnect) {
		cFYI(1, "tcp session dead - return to caller to retry");
		return -EAGAIN;
	}

	if (ses->status != CifsGood) {
		
		if ((in_buf->Command != SMB_COM_SESSION_SETUP_ANDX) &&
			(in_buf->Command != SMB_COM_NEGOTIATE))
			return -EAGAIN;
		
	}
	*ppmidQ = AllocMidQEntry(in_buf, ses->server);
	if (*ppmidQ == NULL)
		return -ENOMEM;
	spin_lock(&GlobalMid_Lock);
	list_add_tail(&(*ppmidQ)->qhead, &ses->server->pending_mid_q);
	spin_unlock(&GlobalMid_Lock);
	return 0;
}

static int
wait_for_response(struct TCP_Server_Info *server, struct mid_q_entry *midQ)
{
	int error;

	error = wait_event_freezekillable(server->response_q,
				    midQ->mid_state != MID_REQUEST_SUBMITTED);
	if (error < 0)
		return -ERESTARTSYS;

	return 0;
}

static int
cifs_setup_async_request(struct TCP_Server_Info *server, struct kvec *iov,
			 unsigned int nvec, struct mid_q_entry **ret_mid)
{
	int rc;
	struct smb_hdr *hdr = (struct smb_hdr *)iov[0].iov_base;
	struct mid_q_entry *mid;

	
	if (server->sec_mode & (SECMODE_SIGN_REQUIRED | SECMODE_SIGN_ENABLED))
		hdr->Flags2 |= SMBFLG2_SECURITY_SIGNATURE;

	mid = AllocMidQEntry(hdr, server);
	if (mid == NULL)
		return -ENOMEM;

	
	spin_lock(&GlobalMid_Lock);
	list_add_tail(&mid->qhead, &server->pending_mid_q);
	spin_unlock(&GlobalMid_Lock);

	rc = cifs_sign_smb2(iov, nvec, server, &mid->sequence_number);
	if (rc)
		delete_mid(mid);
	*ret_mid = mid;
	return rc;
}

int
cifs_call_async(struct TCP_Server_Info *server, struct kvec *iov,
		unsigned int nvec, mid_receive_t *receive,
		mid_callback_t *callback, void *cbdata, bool ignore_pend)
{
	int rc;
	struct mid_q_entry *mid;

	rc = wait_for_free_request(server, ignore_pend ? CIFS_ASYNC_OP : 0);
	if (rc)
		return rc;

	mutex_lock(&server->srv_mutex);
	rc = cifs_setup_async_request(server, iov, nvec, &mid);
	if (rc) {
		mutex_unlock(&server->srv_mutex);
		cifs_add_credits(server, 1);
		wake_up(&server->request_q);
		return rc;
	}

	mid->receive = receive;
	mid->callback = callback;
	mid->callback_data = cbdata;
	mid->mid_state = MID_REQUEST_SUBMITTED;

	cifs_in_send_inc(server);
	rc = smb_sendv(server, iov, nvec);
	cifs_in_send_dec(server);
	cifs_save_when_sent(mid);
	mutex_unlock(&server->srv_mutex);

	if (rc)
		goto out_err;

	return rc;
out_err:
	delete_mid(mid);
	cifs_add_credits(server, 1);
	wake_up(&server->request_q);
	return rc;
}

int
SendReceiveNoRsp(const unsigned int xid, struct cifs_ses *ses,
		 char *in_buf, int flags)
{
	int rc;
	struct kvec iov[1];
	int resp_buf_type;

	iov[0].iov_base = in_buf;
	iov[0].iov_len = get_rfc1002_length(in_buf) + 4;
	flags |= CIFS_NO_RESP;
	rc = SendReceive2(xid, ses, iov, 1, &resp_buf_type, flags);
	cFYI(DBG2, "SendRcvNoRsp flags %d rc %d", flags, rc);

	return rc;
}

static int
cifs_sync_mid_result(struct mid_q_entry *mid, struct TCP_Server_Info *server)
{
	int rc = 0;

	cFYI(1, "%s: cmd=%d mid=%llu state=%d", __func__,
	     le16_to_cpu(mid->command), mid->mid, mid->mid_state);

	spin_lock(&GlobalMid_Lock);
	switch (mid->mid_state) {
	case MID_RESPONSE_RECEIVED:
		spin_unlock(&GlobalMid_Lock);
		return rc;
	case MID_RETRY_NEEDED:
		rc = -EAGAIN;
		break;
	case MID_RESPONSE_MALFORMED:
		rc = -EIO;
		break;
	case MID_SHUTDOWN:
		rc = -EHOSTDOWN;
		break;
	default:
		list_del_init(&mid->qhead);
		cERROR(1, "%s: invalid mid state mid=%llu state=%d", __func__,
		       mid->mid, mid->mid_state);
		rc = -EIO;
	}
	spin_unlock(&GlobalMid_Lock);

	DeleteMidQEntry(mid);
	return rc;
}

static int
send_nt_cancel(struct TCP_Server_Info *server, struct smb_hdr *in_buf,
		struct mid_q_entry *mid)
{
	int rc = 0;

	
	in_buf->smb_buf_length = cpu_to_be32(sizeof(struct smb_hdr) - 4  + 2);
	in_buf->Command = SMB_COM_NT_CANCEL;
	in_buf->WordCount = 0;
	put_bcc(0, in_buf);

	mutex_lock(&server->srv_mutex);
	rc = cifs_sign_smb(in_buf, server, &mid->sequence_number);
	if (rc) {
		mutex_unlock(&server->srv_mutex);
		return rc;
	}
	rc = smb_send(server, in_buf, be32_to_cpu(in_buf->smb_buf_length));
	mutex_unlock(&server->srv_mutex);

	cFYI(1, "issued NT_CANCEL for mid %u, rc = %d",
		in_buf->Mid, rc);

	return rc;
}

int
cifs_check_receive(struct mid_q_entry *mid, struct TCP_Server_Info *server,
		   bool log_error)
{
	unsigned int len = get_rfc1002_length(mid->resp_buf) + 4;

	dump_smb(mid->resp_buf, min_t(u32, 92, len));

	
	if (server->sec_mode & (SECMODE_SIGN_REQUIRED | SECMODE_SIGN_ENABLED)) {
		struct kvec iov;

		iov.iov_base = mid->resp_buf;
		iov.iov_len = len;
		
		if (cifs_verify_signature(&iov, 1, server,
					  mid->sequence_number + 1) != 0)
			cERROR(1, "Unexpected SMB signature");
	}

	
	return map_smb_to_linux_error(mid->resp_buf, log_error);
}

static int
cifs_setup_request(struct cifs_ses *ses, struct kvec *iov,
		   unsigned int nvec, struct mid_q_entry **ret_mid)
{
	int rc;
	struct smb_hdr *hdr = (struct smb_hdr *)iov[0].iov_base;
	struct mid_q_entry *mid;

	rc = allocate_mid(ses, hdr, &mid);
	if (rc)
		return rc;
	rc = cifs_sign_smb2(iov, nvec, ses->server, &mid->sequence_number);
	if (rc)
		delete_mid(mid);
	*ret_mid = mid;
	return rc;
}

int
SendReceive2(const unsigned int xid, struct cifs_ses *ses,
	     struct kvec *iov, int n_vec, int *pRespBufType ,
	     const int flags)
{
	int rc = 0;
	int long_op;
	struct mid_q_entry *midQ;
	char *buf = iov[0].iov_base;

	long_op = flags & CIFS_TIMEOUT_MASK;

	*pRespBufType = CIFS_NO_BUFFER;  

	if ((ses == NULL) || (ses->server == NULL)) {
		cifs_small_buf_release(buf);
		cERROR(1, "Null session");
		return -EIO;
	}

	if (ses->server->tcpStatus == CifsExiting) {
		cifs_small_buf_release(buf);
		return -ENOENT;
	}


	rc = wait_for_free_request(ses->server, long_op);
	if (rc) {
		cifs_small_buf_release(buf);
		return rc;
	}


	mutex_lock(&ses->server->srv_mutex);

	rc = cifs_setup_request(ses, iov, n_vec, &midQ);
	if (rc) {
		mutex_unlock(&ses->server->srv_mutex);
		cifs_small_buf_release(buf);
		
		cifs_add_credits(ses->server, 1);
		return rc;
	}

	midQ->mid_state = MID_REQUEST_SUBMITTED;
	cifs_in_send_inc(ses->server);
	rc = smb_sendv(ses->server, iov, n_vec);
	cifs_in_send_dec(ses->server);
	cifs_save_when_sent(midQ);

	mutex_unlock(&ses->server->srv_mutex);

	if (rc < 0) {
		cifs_small_buf_release(buf);
		goto out;
	}

	if (long_op == CIFS_ASYNC_OP) {
		cifs_small_buf_release(buf);
		goto out;
	}

	rc = wait_for_response(ses->server, midQ);
	if (rc != 0) {
		send_nt_cancel(ses->server, (struct smb_hdr *)buf, midQ);
		spin_lock(&GlobalMid_Lock);
		if (midQ->mid_state == MID_REQUEST_SUBMITTED) {
			midQ->callback = DeleteMidQEntry;
			spin_unlock(&GlobalMid_Lock);
			cifs_small_buf_release(buf);
			cifs_add_credits(ses->server, 1);
			return rc;
		}
		spin_unlock(&GlobalMid_Lock);
	}

	cifs_small_buf_release(buf);

	rc = cifs_sync_mid_result(midQ, ses->server);
	if (rc != 0) {
		cifs_add_credits(ses->server, 1);
		return rc;
	}

	if (!midQ->resp_buf || midQ->mid_state != MID_RESPONSE_RECEIVED) {
		rc = -EIO;
		cFYI(1, "Bad MID state?");
		goto out;
	}

	buf = (char *)midQ->resp_buf;
	iov[0].iov_base = buf;
	iov[0].iov_len = get_rfc1002_length(buf) + 4;
	if (midQ->large_buf)
		*pRespBufType = CIFS_LARGE_BUFFER;
	else
		*pRespBufType = CIFS_SMALL_BUFFER;

	rc = cifs_check_receive(midQ, ses->server, flags & CIFS_LOG_ERROR);

	
	if ((flags & CIFS_NO_RESP) == 0)
		midQ->resp_buf = NULL;
out:
	delete_mid(midQ);
	cifs_add_credits(ses->server, 1);

	return rc;
}

int
SendReceive(const unsigned int xid, struct cifs_ses *ses,
	    struct smb_hdr *in_buf, struct smb_hdr *out_buf,
	    int *pbytes_returned, const int long_op)
{
	int rc = 0;
	struct mid_q_entry *midQ;

	if (ses == NULL) {
		cERROR(1, "Null smb session");
		return -EIO;
	}
	if (ses->server == NULL) {
		cERROR(1, "Null tcp session");
		return -EIO;
	}

	if (ses->server->tcpStatus == CifsExiting)
		return -ENOENT;


	if (be32_to_cpu(in_buf->smb_buf_length) > CIFSMaxBufSize +
			MAX_CIFS_HDR_SIZE - 4) {
		cERROR(1, "Illegal length, greater than maximum frame, %d",
			   be32_to_cpu(in_buf->smb_buf_length));
		return -EIO;
	}

	rc = wait_for_free_request(ses->server, long_op);
	if (rc)
		return rc;


	mutex_lock(&ses->server->srv_mutex);

	rc = allocate_mid(ses, in_buf, &midQ);
	if (rc) {
		mutex_unlock(&ses->server->srv_mutex);
		
		cifs_add_credits(ses->server, 1);
		return rc;
	}

	rc = cifs_sign_smb(in_buf, ses->server, &midQ->sequence_number);
	if (rc) {
		mutex_unlock(&ses->server->srv_mutex);
		goto out;
	}

	midQ->mid_state = MID_REQUEST_SUBMITTED;

	cifs_in_send_inc(ses->server);
	rc = smb_send(ses->server, in_buf, be32_to_cpu(in_buf->smb_buf_length));
	cifs_in_send_dec(ses->server);
	cifs_save_when_sent(midQ);
	mutex_unlock(&ses->server->srv_mutex);

	if (rc < 0)
		goto out;

	if (long_op == CIFS_ASYNC_OP)
		goto out;

	rc = wait_for_response(ses->server, midQ);
	if (rc != 0) {
		send_nt_cancel(ses->server, in_buf, midQ);
		spin_lock(&GlobalMid_Lock);
		if (midQ->mid_state == MID_REQUEST_SUBMITTED) {
			
			midQ->callback = DeleteMidQEntry;
			spin_unlock(&GlobalMid_Lock);
			cifs_add_credits(ses->server, 1);
			return rc;
		}
		spin_unlock(&GlobalMid_Lock);
	}

	rc = cifs_sync_mid_result(midQ, ses->server);
	if (rc != 0) {
		cifs_add_credits(ses->server, 1);
		return rc;
	}

	if (!midQ->resp_buf || !out_buf ||
	    midQ->mid_state != MID_RESPONSE_RECEIVED) {
		rc = -EIO;
		cERROR(1, "Bad MID state?");
		goto out;
	}

	*pbytes_returned = get_rfc1002_length(midQ->resp_buf);
	memcpy(out_buf, midQ->resp_buf, *pbytes_returned + 4);
	rc = cifs_check_receive(midQ, ses->server, 0);
out:
	delete_mid(midQ);
	cifs_add_credits(ses->server, 1);

	return rc;
}


static int
send_lock_cancel(const unsigned int xid, struct cifs_tcon *tcon,
			struct smb_hdr *in_buf,
			struct smb_hdr *out_buf)
{
	int bytes_returned;
	struct cifs_ses *ses = tcon->ses;
	LOCK_REQ *pSMB = (LOCK_REQ *)in_buf;


	pSMB->LockType = LOCKING_ANDX_CANCEL_LOCK|LOCKING_ANDX_LARGE_FILES;
	pSMB->Timeout = 0;
	pSMB->hdr.Mid = GetNextMid(ses->server);

	return SendReceive(xid, ses, in_buf, out_buf,
			&bytes_returned, 0);
}

int
SendReceiveBlockingLock(const unsigned int xid, struct cifs_tcon *tcon,
	    struct smb_hdr *in_buf, struct smb_hdr *out_buf,
	    int *pbytes_returned)
{
	int rc = 0;
	int rstart = 0;
	struct mid_q_entry *midQ;
	struct cifs_ses *ses;

	if (tcon == NULL || tcon->ses == NULL) {
		cERROR(1, "Null smb session");
		return -EIO;
	}
	ses = tcon->ses;

	if (ses->server == NULL) {
		cERROR(1, "Null tcp session");
		return -EIO;
	}

	if (ses->server->tcpStatus == CifsExiting)
		return -ENOENT;


	if (be32_to_cpu(in_buf->smb_buf_length) > CIFSMaxBufSize +
			MAX_CIFS_HDR_SIZE - 4) {
		cERROR(1, "Illegal length, greater than maximum frame, %d",
			   be32_to_cpu(in_buf->smb_buf_length));
		return -EIO;
	}

	rc = wait_for_free_request(ses->server, CIFS_BLOCKING_OP);
	if (rc)
		return rc;


	mutex_lock(&ses->server->srv_mutex);

	rc = allocate_mid(ses, in_buf, &midQ);
	if (rc) {
		mutex_unlock(&ses->server->srv_mutex);
		return rc;
	}

	rc = cifs_sign_smb(in_buf, ses->server, &midQ->sequence_number);
	if (rc) {
		delete_mid(midQ);
		mutex_unlock(&ses->server->srv_mutex);
		return rc;
	}

	midQ->mid_state = MID_REQUEST_SUBMITTED;
	cifs_in_send_inc(ses->server);
	rc = smb_send(ses->server, in_buf, be32_to_cpu(in_buf->smb_buf_length));
	cifs_in_send_dec(ses->server);
	cifs_save_when_sent(midQ);
	mutex_unlock(&ses->server->srv_mutex);

	if (rc < 0) {
		delete_mid(midQ);
		return rc;
	}

	
	rc = wait_event_interruptible(ses->server->response_q,
		(!(midQ->mid_state == MID_REQUEST_SUBMITTED)) ||
		((ses->server->tcpStatus != CifsGood) &&
		 (ses->server->tcpStatus != CifsNew)));

	
	if ((rc == -ERESTARTSYS) &&
		(midQ->mid_state == MID_REQUEST_SUBMITTED) &&
		((ses->server->tcpStatus == CifsGood) ||
		 (ses->server->tcpStatus == CifsNew))) {

		if (in_buf->Command == SMB_COM_TRANSACTION2) {
			rc = send_nt_cancel(ses->server, in_buf, midQ);
			if (rc) {
				delete_mid(midQ);
				return rc;
			}
		} else {

			rc = send_lock_cancel(xid, tcon, in_buf, out_buf);

			if (rc && rc != -ENOLCK) {
				delete_mid(midQ);
				return rc;
			}
		}

		rc = wait_for_response(ses->server, midQ);
		if (rc) {
			send_nt_cancel(ses->server, in_buf, midQ);
			spin_lock(&GlobalMid_Lock);
			if (midQ->mid_state == MID_REQUEST_SUBMITTED) {
				
				midQ->callback = DeleteMidQEntry;
				spin_unlock(&GlobalMid_Lock);
				return rc;
			}
			spin_unlock(&GlobalMid_Lock);
		}

		
		rstart = 1;
	}

	rc = cifs_sync_mid_result(midQ, ses->server);
	if (rc != 0)
		return rc;

	
	if (out_buf == NULL || midQ->mid_state != MID_RESPONSE_RECEIVED) {
		rc = -EIO;
		cERROR(1, "Bad MID state?");
		goto out;
	}

	*pbytes_returned = get_rfc1002_length(midQ->resp_buf);
	memcpy(out_buf, midQ->resp_buf, *pbytes_returned + 4);
	rc = cifs_check_receive(midQ, ses->server, 0);
out:
	delete_mid(midQ);
	if (rstart && rc == -EACCES)
		return -ERESTARTSYS;
	return rc;
}
