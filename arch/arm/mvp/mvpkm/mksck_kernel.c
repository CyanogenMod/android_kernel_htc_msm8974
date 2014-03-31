/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5


#define __KERNEL_SYSCALLS__
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/syscalls.h>
#include <linux/kmod.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/rcupdate.h>
#include <linux/smp.h>
#include <linux/spinlock.h>

#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/vmalloc.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <net/sock.h>

#include <asm/memory.h>
#include <asm/system.h>
#include <linux/uaccess.h>

#include "mvp.h"
#include "actions.h"
#include "mvpkm_kernel.h"
#include "mksck_kernel.h"
#include "mksck_sockaddr.h"
#include "mutex_kernel.h"

void NORETURN
FatalError(char const *file,
	   int line,
	   FECode feCode,
	   int bugno,
	   char const *fmt,
	   ...)
{
	static DEFINE_MUTEX(fatalErrorMutex);

	mutex_lock(&fatalErrorMutex);

	FATALERROR_COMMON(printk, vprintk, file, line, feCode, bugno, fmt);

	dump_stack();

	
	mutex_unlock(&fatalErrorMutex);

	do_exit(1);
}



static struct proto mksckProto = {
	.name     = "AF_MKSCK",
	.owner    = THIS_MODULE,
	.obj_size = sizeof(struct sock),
};

static int
MksckCreate(struct net *net,
	    struct socket *sock,
	    int protocol,
	    int kern);

static struct net_proto_family mksckFamilyOps = {
	.family = AF_MKSCK,
	.owner  = THIS_MODULE,
	.create = MksckCreate,
};

static int MksckFault(struct vm_area_struct *vma, struct vm_fault *vmf);


static struct vm_operations_struct mksckVMOps = {
	.fault = MksckFault
};

static spinlock_t mksckPageListLock;
static MksckPage *mksckPages[MKSCK_MAX_SHARES];

static int MksckRelease(struct socket *sock);
static int MksckBacklogRcv(struct sock *sk, struct sk_buff *skb);
static void MksckSkDestruct(struct sock *sk);
static int
MksckBind(struct socket *sock,
	  struct sockaddr *addr,
	  int addrLen);
static int MksckBindGeneric(struct sock *sk, Mksck_Address addr);
static int
MksckDgramRecvMsg(struct kiocb *kiocb,
		  struct socket *sock,
		  struct msghdr *msg,
		  size_t len,
		  int flags);
static int
MksckDgramSendMsg(struct kiocb *kiocb,
		  struct socket *sock,
		  struct msghdr *msg,
		  size_t len);
static int
MksckGetName(struct socket *sock,
	     struct sockaddr *addr,
	     int *addrLen,
	     int peer);
static unsigned int
MksckPoll(struct file *filp,
	  struct socket *sock,
	  poll_table *wait);
static int
MksckDgramConnect(struct socket *sock,
		  struct sockaddr *addr,
		  int addrLen,
		  int flags);
static int
MksckMMap(struct file *file,
	  struct socket *sock,
	  struct vm_area_struct *vma);

static void MksckPageRelease(struct MksckPage *mksckPage);

static const struct proto_ops mksckDgramOps = {
	.family     = AF_MKSCK,
	.owner      = THIS_MODULE,
	.release    = MksckRelease,
	.bind       = MksckBind,
	.connect    = MksckDgramConnect,
	.socketpair = sock_no_socketpair,
	.accept     = sock_no_accept,
	.getname    = MksckGetName,
	.poll       = MksckPoll,
	.ioctl      = sock_no_ioctl,
	.listen     = sock_no_listen,
	.shutdown   = sock_no_shutdown, 
	.setsockopt = sock_no_setsockopt,
	.getsockopt = sock_no_getsockopt,
	.sendmsg    = MksckDgramSendMsg,
	.recvmsg    = MksckDgramRecvMsg,
	.mmap       = MksckMMap,
	.sendpage   = sock_no_sendpage,
};


int
Mksck_Init(void)
{
	int err;

	spin_lock_init(&mksckPageListLock);

	err = proto_register(&mksckProto, 1);
	if (err != 0) {
		pr_err("Mksck_Init: Cannot register AF_MKSCK protocol" \
		       ", errno = %d.\n", err);
		return err;
	}

	err = sock_register(&mksckFamilyOps);
	if (err < 0) {
		pr_err("Mksck_Init: Could not register address family" \
		       " AF_MKSCK (errno = %d).\n", err);
		return err;
	}

	return 0;
}


void
Mksck_Exit(void)
{
	sock_unregister(mksckFamilyOps.family);
	proto_unregister(&mksckProto);
}


static int
MksckCreate(struct net *net,
	    struct socket *sock,
	    int protocol,
	    int kern)
{
	struct sock *sk;
	uid_t currentUid = current_euid();

	if (!(currentUid == 0 ||
	    currentUid == Mvpkm_vmwareUid)) {
		pr_warn("MksckCreate: rejected from process %s " \
			"tgid=%d, pid=%d euid:%d.\n",
			current->comm,
			task_tgid_vnr(current),
			task_pid_vnr(current),
			currentUid);
		return -EPERM;
	}

	if (!sock)
		return -EINVAL;

	if (protocol)
		return -EPROTONOSUPPORT;

	switch (sock->type) {
	case SOCK_DGRAM:
		sock->ops = &mksckDgramOps;
		break;
	default:
		return -ESOCKTNOSUPPORT;
	}

	sock->state = SS_UNCONNECTED;

	sk = sk_alloc(net, mksckFamilyOps.family, GFP_KERNEL, &mksckProto);
	if (!sk)
		return -ENOMEM;

	sock_init_data(sock, sk);

	sk->sk_type        = SOCK_DGRAM;
	sk->sk_destruct    = MksckSkDestruct;
	sk->sk_backlog_rcv = MksckBacklogRcv;


	sk->sk_protinfo = NULL;
	sock_reset_flag(sk, SOCK_DONE);

	return 0;
}


static int
MksckRelease(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (sk) {
		lock_sock(sk);
		sock_orphan(sk);
		release_sock(sk);
		sock_put(sk);
	}

	sock->sk = NULL;
	sock->state = SS_FREE;

	return 0;
}


static int
MksckBacklogRcv(struct sock *sk,
		struct sk_buff *skb)
{
	pr_err("MksckBacklogRcv: should never get here\n");
	return -EIO;
}


static void
MksckSkDestruct(struct sock *sk)
{
	Mksck *mksck;

	lock_sock(sk);
	mksck = sk->sk_protinfo;

	if (mksck != NULL) {
		sk->sk_protinfo = NULL;
		Mksck_CloseCommon(mksck);
	}

	if (sk->sk_user_data != NULL) {
		sock_kfree_s(sk, sk->sk_user_data, sizeof(int));
		sk->sk_user_data = NULL;
	}

	release_sock(sk);
}


static int
MksckBindGeneric(struct sock *sk,
		 Mksck_Address addr)
{
	int err;
	Mksck *mksck;
	struct MksckPage *mksckPage;

	if (sk->sk_protinfo != NULL)
		return -EISCONN;

	if (addr.vmId == MKSCK_VMID_UNDEF) {
		mksckPage = MksckPage_GetFromTgidIncRefc();
	} else {
		pr_err("MksckBind: host bind called on vmid 0x%X\n", addr.vmId);
		mksckPage = MksckPage_GetFromVmIdIncRefc(addr.vmId);
	}

	if (mksckPage == NULL) {
		pr_err("MksckBind: no mksckPage for vm 0x%X\n", addr.vmId);
		return -ENETUNREACH;
	}
	addr.vmId = mksckPage->vmId;

	err = Mutex_Lock(&mksckPage->mutex, MutexModeEX);
	if (err < 0)
		goto outDec;

	addr.port = MksckPage_GetFreePort(mksckPage, addr.port);
	if (addr.port == MKSCK_PORT_UNDEF) {
		err = -EINVAL;
		goto outUnlockDec;
	}

	mksck = MksckPage_AllocSocket(mksckPage, addr);
	if (mksck == NULL) {
		err = -EMFILE;
		goto outUnlockDec;
	}

	Mutex_Unlock(&mksckPage->mutex, MutexModeEX);

	sk->sk_protinfo = mksck;

	PRINTK("MksckBind: socket bound to %08X\n",
	       mksck->addr.addr);

	return 0;

outUnlockDec:
	Mutex_Unlock(&mksckPage->mutex, MutexModeEX);
outDec:
	MksckPage_DecRefc(mksckPage);
	return err;
}


static inline int
MksckTryBind(struct sock *sk)
{
	int err = 0;

	if (!sk->sk_protinfo) {
		static const Mksck_Address addr = { .addr = MKSCK_ADDR_UNDEF };

		err = MksckBindGeneric(sk, addr);
	}
	return err;
}



static int
MksckBind(struct socket *sock,
	  struct sockaddr *addr,
	  int addrLen)
{
	int err;
	struct sock *sk            = sock->sk;
	struct sockaddr_mk *addrMk = (struct sockaddr_mk *)addr;

	if (addrLen != sizeof(*addrMk))
		return -EINVAL;

	if (addrMk->mk_family != AF_MKSCK)
		return -EAFNOSUPPORT;

	lock_sock(sk);
	err = MksckBindGeneric(sk, addrMk->mk_addr);
	release_sock(sk);

	return err;
}

static int
LockPeer(Mksck_Address addr, Mksck **peerMksckR)
{
	int err = 0;
	struct MksckPage *peerMksckPage =
		MksckPage_GetFromVmIdIncRefc(addr.vmId);
	Mksck *peerMksck;

	if (peerMksckPage == NULL) {
		pr_info("LockPeer: vmId %x is not in use!\n", addr.vmId);
		return -ENETUNREACH;
	}
	if (!peerMksckPage->isGuest) {
		MksckPage_DecRefc(peerMksckPage);
		pr_err("LockPeer: vmId %x does not belong to a guest!\n",
		       addr.vmId);
		return -ENETUNREACH;
	}

	err = Mutex_Lock(&peerMksckPage->mutex, MutexModeSH);
	if (err < 0) {
		MksckPage_DecRefc(peerMksckPage);
		return err;
	}

	peerMksck = MksckPage_GetFromAddr(peerMksckPage, addr);

	if (peerMksck) {
		ATOMIC_ADDV(peerMksck->refCount, 1);
		*peerMksckR = peerMksck;
	} else {
		pr_err("LockPeer: addr %x is not a defined socket!\n",
		       addr.addr);
		err = -ENETUNREACH;
	}

	Mutex_Unlock(&peerMksckPage->mutex, MutexModeSH);
	MksckPage_DecRefc(peerMksckPage);
	return err;
}

static int
MksckDgramConnect(struct socket *sock,
		  struct sockaddr *addr,
		  int addrLen,
		  int flags)
{
	struct sock *sk = sock->sk;
	Mksck *mksck;
	struct sockaddr_mk *peerAddrMk = (struct sockaddr_mk *)addr;
	int err = 0;

	if (addrLen != sizeof(*peerAddrMk)) {
		pr_info("MksckConnect: wrong address length!\n");
		return -EINVAL;
	}
	if (peerAddrMk->mk_family != AF_MKSCK) {
		pr_info("MksckConnect: wrong address family!\n");
		return -EAFNOSUPPORT;
	}

	lock_sock(sk);

	err = MksckTryBind(sk);
	if (err)
		goto releaseSock;

	mksck = sk->sk_protinfo;

	Mksck_DisconnectPeer(mksck);
	sock->state = SS_UNCONNECTED;

	if (peerAddrMk->mk_addr.addr != MKSCK_ADDR_UNDEF) {
		sock->state = SS_CONNECTED;
		mksck->peerAddr = peerAddrMk->mk_addr;
		err = LockPeer(mksck->peerAddr, &mksck->peer);
		PRINTK("MksckConnect: socket %x is connected" \
		       " to %x!\n", mksck->addr.addr,  mksck->peerAddr.addr);
	}

releaseSock:
	release_sock(sk);

	return err;
}


static int
MksckGetName(struct socket *sock,
	     struct sockaddr *addr,
	     int *addrLen,
	     int peer)
{
	int err;
	Mksck *mksck;
	struct sock *sk = sock->sk;


	lock_sock(sk);
	mksck = sk->sk_protinfo;

	if (mksck == NULL) {
		if (peer) {
			err = -ENOTCONN;
		} else {
			((struct sockaddr_mk *)addr)->mk_family = AF_MKSCK;
			((struct sockaddr_mk *)addr)->mk_addr.addr =
				MKSCK_ADDR_UNDEF;
			*addrLen = sizeof(struct sockaddr_mk);
			err = 0;
		}
	} else if (!peer) {
		((struct sockaddr_mk *)addr)->mk_family = AF_MKSCK;
		((struct sockaddr_mk *)addr)->mk_addr   = mksck->addr;
		*addrLen = sizeof(struct sockaddr_mk);
		err = 0;
	} else if (mksck->peerAddr.addr == MKSCK_ADDR_UNDEF) {
		err = -ENOTCONN;
	} else {
		((struct sockaddr_mk *)addr)->mk_family = AF_MKSCK;
		((struct sockaddr_mk *)addr)->mk_addr   = mksck->peerAddr;
		*addrLen = sizeof(struct sockaddr_mk);
		err = 0;
	}

	release_sock(sk);

	return err;
}


static unsigned int MksckPoll(struct file *filp,
			      struct socket *sock,
			      poll_table *wait)
{
	struct sock *sk = sock->sk;
	unsigned int mask = 0;
	Mksck *mksck = NULL;
	uint32 read;
	int err;

	lock_sock(sk);
	err = MksckTryBind(sk);
	if (err) {
		release_sock(sk);
		return err;
	}

	mksck = sk->sk_protinfo;

	ATOMIC_ADDV(mksck->refCount, 1);
	release_sock(sk);

	err = Mutex_Lock(&mksck->mutex, MutexModeEX);
	if (err < 0) {
		PRINTK("MksckPoll: try to abort\n");
		return mask;
	}

	read = mksck->read;
	if (read != mksck->write) {
		mask |= POLLIN | POLLRDNORM; 

		Mutex_Unlock(&mksck->mutex, MutexModeEX);
	} else {
		Mutex_UnlPoll(&mksck->mutex, MutexModeEX,
			      MKSCK_CVAR_FILL, filp, wait);
	}

	Mksck_DecRefc(mksck);
	return mask;
}

static size_t
MksckPageDescManage(Mksck_PageDesc *pd,
		    uint32 pages,
		    int incr)
{
	size_t payloadLen = 0;
	uint32 i;

	for (i = 0; i < pages && pd[i].mpn != INVALID_MPN; ++i) {
		uint32 j;

		for (j = 0; j < 1 << pd[i].order; ++j) {
			struct page *page;
			MPN currMPN = pd[i].mpn + j;

			if (!pfn_valid(currMPN)) {
				pr_warn("MksckPageDescManage: Invalid MPN %x\n",
					currMPN);
			} else {
				page = pfn_to_page(currMPN);

				if (incr == 1)
					get_page(page);
				if (incr == -1)
					put_page(page);
			}

			payloadLen += PAGE_SIZE;
		}
	}

	return payloadLen;
}

#define MANAGE_INCREMENT  1
#define MANAGE_DECREMENT -1
#define MANAGE_COUNT      0


static size_t
MksckPageDescMap(Mksck_PageDesc *pd,
		 uint32 pages,
		 struct iovec *iov,
		 int iovCount,
		 struct vm_area_struct *vma)
{
	size_t payloadLen = 0;
	uint32 i;

	for (i = 0; i < pages && pd[i].mpn != INVALID_MPN; ++i) {
		uint32 j;

		for (j = 0; j < 1 << pd[i].order; ++j) {
			HUVA huva = 0;
			struct page *page;
			MPN currMPN = pd[i].mpn + j;

			while (iovCount > 0 && iov->iov_len == 0) {
				iovCount--;
				iov++;
			}

			if (iovCount == 0) {
				pr_warn("MksckPageDescMap: Invalid " \
					"iov length\n");
				goto map_done;
			}

			huva = (HUVA)iov->iov_base;

			if (huva & (PAGE_SIZE - 1) ||
			    iov->iov_len < PAGE_SIZE) {
				pr_warn("MksckPageDescMap: Invalid huva %x " \
					"or iov_len %d\n", huva, iov->iov_len);
				goto map_done;
			}

			if (vma == NULL || huva < vma->vm_start ||
			    huva >= vma->vm_end) {
				vma = find_vma(current->mm, huva);

				if (vma == NULL ||
				    huva < vma->vm_start ||
				    vma->vm_ops != &mksckVMOps) {
					pr_warn("MksckPageDescMap: " \
						"Invalid vma\n");
					goto map_done;
				}
			}

			if (!pfn_valid(currMPN)) {
				pr_warn("MksckPageDescMap: Invalid MPN %x\n",
					currMPN);
			} else {
				int rc;

				page = pfn_to_page(currMPN);

				rc = vm_insert_page(vma, huva, page);
				if (rc) {
					pr_warn("MksckPageDescMap: Failed to " \
						"insert %x at %x, error %d\n",
						currMPN, huva, rc);
					goto map_done;
				}

				ASSERT(iov->iov_len >= PAGE_SIZE);
				iov->iov_base += PAGE_SIZE;
				iov->iov_len -= PAGE_SIZE;
			}

			payloadLen += PAGE_SIZE;
		}
	}

map_done:
	return payloadLen;
}


static int
MsgHdrHasAvailableRoom(struct msghdr *msg)
{
	struct iovec *vec = msg->msg_iov;
	uint32 count = msg->msg_iovlen;

	while (count > 0 && vec->iov_len == 0) {
		count--;
		vec++;
	}

	return (count != 0);
}


struct MksckPageDescInfo {
	struct MksckPageDescInfo *next;
	uint32 flags;
	uint32 pages;
	uint32 mapCounts;
	Mksck_PageDesc descs[0];
};

static void MksckPageDescSkDestruct(struct sock *sk);
static int
MksckPageDescMMap(struct file *file,
		  struct socket *sock,
		  struct vm_area_struct *vma);
static int
MksckPageDescIoctl(struct socket *sock,
		   unsigned int cmd,
		   unsigned long arg);

static int
MksckPageDescRelease(struct socket *sock)
{
	
	struct sock *sk = sock->sk;

	if (sk) {
		lock_sock(sk);
		sock_orphan(sk);
		release_sock(sk);
		sock_put(sk);
	}

	sock->sk = NULL;
	sock->state = SS_FREE;

	return 0;
}


static const struct proto_ops mksckPageDescOps = {
	.family     = AF_MKSCK,
	.owner      = THIS_MODULE,
	.release    = MksckPageDescRelease,
	.bind       = sock_no_bind,
	.connect    = sock_no_connect,
	.socketpair = sock_no_socketpair,
	.accept     = sock_no_accept,
	.getname    = sock_no_getname,
	.poll       = sock_no_poll,
	.ioctl      = MksckPageDescIoctl,
	.listen     = sock_no_listen,
	.shutdown   = sock_no_shutdown,
	.setsockopt = sock_no_setsockopt,
	.getsockopt = sock_no_getsockopt,
	.sendmsg    = sock_no_sendmsg,
	.recvmsg    = sock_no_recvmsg,
	.mmap       = MksckPageDescMMap,
	.sendpage   = sock_no_sendpage,
};


static int
MksckPageDescToFd(struct socket *sock,
		  struct msghdr *msg,
		  Mksck_PageDesc *pd,
		  uint32 pages)
{
	int retval;
	int newfd;
	struct socket *newsock;
	struct sock *newsk;
	struct sock *sk = sock->sk;
	struct MksckPageDescInfo **pmpdi, *mpdi;

	lock_sock(sk);

	if (sk->sk_user_data) {
		struct MksckPageDescInfo *mpdi2;

		
		newfd = *((int *)sk->sk_user_data);

		
		newsock = sockfd_lookup(newfd, &retval);
		if (!newsock) {
			retval = -EINVAL;
			goto endProcessingReleaseSock;
		}

		newsk = newsock->sk;
		lock_sock(newsk);
		sockfd_put(newsock);

		if (((struct sock *)newsk->sk_user_data) != sk) {

			retval = -EINVAL;
			release_sock(newsk);
			goto endProcessingReleaseSock;
		}

		mpdi = kmalloc(sizeof(struct MksckPageDescInfo) +
			       pages*sizeof(Mksck_PageDesc), GFP_KERNEL);
		if (!mpdi) {
			retval = -ENOMEM;
			release_sock(newsk);
			goto endProcessingReleaseSock;
		}

		retval = put_cmsg(msg, SOL_DECNET, 0, sizeof(int), &newfd);
		if (retval < 0)
			goto endProcessingKFreeReleaseSock;

		release_sock(sk);

		mpdi2 = (struct MksckPageDescInfo *)newsk->sk_protinfo;
		while (mpdi2->next)
			mpdi2 = mpdi2->next;

		pmpdi = &(mpdi2->next);
	} else {
		retval = sock_create(sk->sk_family, sock->type, 0, &newsock);
		if (retval < 0)
			goto endProcessingReleaseSock;

		newsk = newsock->sk;
		lock_sock(newsk);
		newsk->sk_destruct = &MksckPageDescSkDestruct;
		newsk->sk_user_data = sk;
		sock_hold(sk); 
		newsock->ops = &mksckPageDescOps;

		mpdi = kmalloc(sizeof(struct MksckPageDescInfo) +
			       pages*sizeof(Mksck_PageDesc), GFP_KERNEL);
		if (!mpdi) {
			retval = -ENOMEM;
			goto endProcessingFreeNewSock;
		}

		sk->sk_user_data = sock_kmalloc(sk, sizeof(int), GFP_KERNEL);
		if (sk->sk_user_data == NULL) {
			retval = -ENOMEM;
			goto endProcessingKFreeAndNewSock;
		}

		newfd = sock_map_fd(newsock, O_CLOEXEC);
		if (newfd < 0) {
			retval = newfd;
			sock_kfree_s(sk, sk->sk_user_data, sizeof(int));
			sk->sk_user_data = NULL;
			goto endProcessingKFreeAndNewSock;
		}

		retval = put_cmsg(msg, SOL_DECNET, 0, sizeof(int), &newfd);
		if (retval < 0) {
			sock_kfree_s(sk, sk->sk_user_data, sizeof(int));
			sk->sk_user_data = NULL;
			kfree(mpdi);
			release_sock(newsk);
			sockfd_put(newsock);
			sock_release(newsock);
			put_unused_fd(newfd);
			goto endProcessingReleaseSock;
		}

		*(int *)sk->sk_user_data = newfd;
		release_sock(sk);
		pmpdi = (struct MksckPageDescInfo **)(&(newsk->sk_protinfo));
	}

	mpdi->next  = NULL;
	mpdi->flags = 0;
	mpdi->mapCounts = 0;
	mpdi->pages = pages;
	memcpy(mpdi->descs, pd, pages*sizeof(Mksck_PageDesc));

	*pmpdi = mpdi; 
	release_sock(newsk);

	MksckPageDescManage(pd, pages, MANAGE_INCREMENT);
	return 0;

endProcessingKFreeAndNewSock:
	kfree(mpdi);
endProcessingFreeNewSock:
	release_sock(newsk);
	sock_release(newsock);
	release_sock(sk);
	return retval;

endProcessingKFreeReleaseSock:
	kfree(mpdi);
	release_sock(newsk);
endProcessingReleaseSock:
	release_sock(sk);
	return retval;
}

static void
MksckPageDescSkDestruct(struct sock *sk)
{
	struct sock *mkSk = NULL;
	struct MksckPageDescInfo *mpdi;
	lock_sock(sk);
	mpdi = sk->sk_protinfo;
	while (mpdi) {
		struct MksckPageDescInfo *next = mpdi->next;
		MksckPageDescManage(mpdi->descs, mpdi->pages,
				    MANAGE_DECREMENT);
		kfree(mpdi);
		mpdi = next;
	}
	if (sk->sk_user_data) {
		mkSk = (struct sock *)sk->sk_user_data;
		sk->sk_user_data = NULL;
	}
	sk->sk_protinfo  = NULL;
	release_sock(sk);

	if (mkSk) {
		lock_sock(mkSk);
		sock_kfree_s(mkSk, mkSk->sk_user_data, sizeof(int));
		mkSk->sk_user_data = NULL;
		release_sock(mkSk);
		sock_put(mkSk); 
	}
}

static int
MksckPageDescMMap(struct file *file,
		  struct socket *sock,
		  struct vm_area_struct *vma)
{
	struct sock *sk = sock->sk;
	struct MksckPageDescInfo *mpdi;
	struct iovec iov;
	unsigned long vm_flags;
	int freed = 0;

	iov.iov_base = (void *)vma->vm_start;
	iov.iov_len  = vma->vm_end - vma->vm_start;

	lock_sock(sk);
	mpdi = sk->sk_protinfo;

	if (!mpdi || sk->sk_user_data || vma->vm_pgoff) {
		release_sock(sk);
		pr_info("MMAP failed for virt %lx size %lx\n",
			vma->vm_start, vma->vm_end - vma->vm_start);
		return -EINVAL;
	}

	vm_flags = mpdi->flags;
	if ((vma->vm_flags & ~vm_flags) & (VM_READ|VM_WRITE)) {
		release_sock(sk);
		return -EACCES;
	}

	while (mpdi) {
		struct MksckPageDescInfo *next = mpdi->next;
		MksckPageDescMap(mpdi->descs, mpdi->pages, &iov, 1, vma);

		if (mpdi->mapCounts && !--mpdi->mapCounts) {
			MksckPageDescManage(mpdi->descs, mpdi->pages,
					    MANAGE_DECREMENT);
			kfree(mpdi);
			freed = 1;
		}
		mpdi = next;
	}

	if (freed)
		sk->sk_protinfo  = NULL;

	vma->vm_ops = &mksckVMOps;
	release_sock(sk);
	return 0;
}

static int
MksckPageDescIoctl(struct socket *sock,
		   unsigned int cmd,
		   unsigned long arg)
{
	struct sock *mksck = NULL;
	struct sock *sk = sock->sk;
	struct MksckPageDescInfo *mpdi;
	unsigned long ul[2];
	int retval = 0;

	switch (cmd) {
	case MKSCK_DETACH:
		lock_sock(sk);
		mpdi = sk->sk_protinfo;

		if (copy_from_user(ul, (void *)arg, sizeof(ul))) {
			retval = -EFAULT;

		} else if (!mpdi || !sk->sk_user_data) {
			retval = -EINVAL;
		} else {
			uint32 flags = calc_vm_prot_bits(ul[0]);

			ul[0] = 0;
			while (mpdi) {
				struct MksckPageDescInfo *next = mpdi->next;

				ul[0] += MksckPageDescManage(mpdi->descs,
							     mpdi->pages,
							     MANAGE_COUNT);
				mpdi->mapCounts = ul[1];
				mpdi = next;
			}
			if (copy_to_user((void *)arg, ul, sizeof(ul[0]))) {
				retval = -EFAULT;
			} else {
				mpdi = sk->sk_protinfo;
				mpdi->flags = flags;
				mksck = (struct sock *)sk->sk_user_data;
				sk->sk_user_data = NULL;
			}
		}

		release_sock(sk);

		sk = mksck;
		if (sk) {
			lock_sock(sk);
			sock_kfree_s(sk, sk->sk_user_data, sizeof(int));
			sk->sk_user_data = NULL;
			release_sock(sk);
			sock_put(sk);
		}
		break;
	default:
		retval = -EINVAL;
		break;
	}
	return retval;
}


static int
MksckDgramRecvMsg(struct kiocb *kiocb,
		  struct socket *sock,
		  struct msghdr *msg,
		  size_t len,
		  int flags)
{
	int err = 0;
	struct sock *sk = sock->sk;
	Mksck *mksck;
	Mksck_Datagram *dg;
	struct sockaddr_mk *fromAddr;
	uint32 read;
	struct iovec *iov;
	size_t payloadLen, untypedLen;
	uint32 iovCount;

	if (flags & MSG_OOB || flags & MSG_ERRQUEUE)
		return -EOPNOTSUPP;

	if ((msg->msg_name != NULL) && (msg->msg_namelen < sizeof(*fromAddr)))
		return -EINVAL;

	lock_sock(sk);
	err = MksckTryBind(sk);
	if (err) {
		release_sock(sk);
		return err;
	}
	mksck = sk->sk_protinfo;

	ATOMIC_ADDV(mksck->refCount, 1);
	release_sock(sk);

	while (1) {
		err = Mutex_Lock(&mksck->mutex, MutexModeEX);
		if (err < 0)
			goto decRefc;

		read = mksck->read;
		if (read != mksck->write)
			break;

		if (flags & MSG_DONTWAIT) {
			Mutex_Unlock(&mksck->mutex, MutexModeEX);
			err = -EAGAIN;
			goto decRefc;
		}

		mksck->foundEmpty++;
		err = Mutex_UnlSleep(&mksck->mutex, MutexModeEX,
				     MKSCK_CVAR_FILL);
		if (err < 0) {
			PRINTK("MksckDgramRecvMsg: aborted\n");
			goto decRefc;
		}
	}

	dg = (void *)&mksck->buff[read];

	if (msg->msg_name != NULL) {
		fromAddr            = (void *)msg->msg_name;
		fromAddr->mk_addr   = dg->fromAddr;
		fromAddr->mk_family = AF_MKSCK;
		msg->msg_namelen    = sizeof(*fromAddr);
	} else {
		msg->msg_namelen = 0;
	}

	iov = msg->msg_iov;
	iovCount = msg->msg_iovlen;
	untypedLen = dg->len - dg->pages * sizeof(Mksck_PageDesc) - dg->pad;
	payloadLen = untypedLen;

	if (untypedLen <= len) {
		err = memcpy_toiovec(iov, dg->data, untypedLen);
		if (err < 0) {
			pr_warn("MksckDgramRecvMsg: Failed to " \
				"memcpy_to_iovec untyped message component " \
				"(buf len %d datagram len %d (untyped %d))\n",
				len, dg->len, untypedLen);
		}
	} else {
		err = -EINVAL;
	}

	if (err >= 0 && dg->pages > 0) {
		Mksck_PageDesc *pd =
			(Mksck_PageDesc *)(dg->data + untypedLen + dg->pad);


		if (msg->msg_controllen > 0)
			err = MksckPageDescToFd(sock, msg, pd, dg->pages);

		if ((msg->msg_controllen <= 0) ||
		    (err != 0) ||
		    (MsgHdrHasAvailableRoom(msg) != 0)) {
			down_write(&current->mm->mmap_sem);
			payloadLen += MksckPageDescMap(pd, dg->pages,
						       iov, iovCount, NULL);
			up_write(&current->mm->mmap_sem);
		}
	}

	if ((err >= 0) && Mksck_IncReadIndex(mksck, read, dg))
		Mutex_UnlWake(&mksck->mutex, MutexModeEX,
			      MKSCK_CVAR_ROOM, true);
	else
		Mutex_Unlock(&mksck->mutex, MutexModeEX);

	if (err >= 0)
		err = payloadLen;

decRefc:
	Mksck_DecRefc(mksck);
	return err;
}


static int
MksckDgramSendMsg(struct kiocb *kiocb,
		  struct socket *sock,
		  struct msghdr *msg,
		  size_t len)
{
	int             err = 0;
	struct sock    *sk = sock->sk;
	Mksck          *peerMksck;
	Mksck_Datagram *dg;
	uint32          needed;
	uint32          write;
	Mksck_Address   fromAddr;

	if (msg->msg_flags & MSG_OOB)
		return -EOPNOTSUPP;

	if (len > MKSCK_XFER_MAX)
		return -EMSGSIZE;

	lock_sock(sk);
	do {
		Mksck *mksck;
		Mksck_Address peerAddr = {
		    .addr =
			(msg->msg_name ?
			 ((struct sockaddr_mk *)msg->msg_name)->mk_addr.addr :
			 MKSCK_ADDR_UNDEF)
		};

		err = MksckTryBind(sk);
		if (err)
			break;

		mksck = sk->sk_protinfo;
		fromAddr = mksck->addr;

		peerMksck = mksck->peer;
		if (peerMksck) {
			if (peerAddr.addr != MKSCK_ADDR_UNDEF &&
			    peerAddr.addr != mksck->peerAddr.addr) {
				err = -EISCONN;
				break;
			}

			ATOMIC_ADDV(peerMksck->refCount, 1);
		} else if (peerAddr.addr == MKSCK_ADDR_UNDEF) {
			err = -ENOTCONN;
		} else {
			err = LockPeer(peerAddr, &peerMksck);
		}
	} while (0);
	release_sock(sk);

	if (err)
		return err;

	needed = MKSCK_DGSIZE(len);
	while (1) {
		err = Mutex_Lock(&peerMksck->mutex, MutexModeEX);
		if (err < 0)
			goto decRefc;

		if (peerMksck->shutDown & MKSCK_SHUT_RD) {
			err = -ENOTCONN;
			goto unlockDecRefc;
		}

		write = Mksck_FindSendRoom(peerMksck, needed);
		if (write != MKSCK_FINDSENDROOM_FULL)
			break;

		if (msg->msg_flags & MSG_DONTWAIT) {
			err = -EAGAIN;
			goto unlockDecRefc;
		}

		peerMksck->foundFull++;
		err = Mutex_UnlSleep(&peerMksck->mutex, MutexModeEX,
				     MKSCK_CVAR_ROOM);
		if (err < 0) {
			PRINTK("MksckDgramSendMsg: aborted\n");
			goto decRefc;
		}
	}

	dg = (void *)&peerMksck->buff[write];

	dg->fromAddr = fromAddr;
	dg->len      = len;

	err = memcpy_fromiovec(dg->data, msg->msg_iov, len);
	if (err != 0)
		goto unlockDecRefc;

	Mksck_IncWriteIndex(peerMksck, write, needed);

	Mutex_UnlWake(&peerMksck->mutex, MutexModeEX, MKSCK_CVAR_FILL, false);

	if (peerMksck->rcvCBEntryMVA != 0) {
		MksckPage *peerMksckPage = Mksck_ToSharedPage(peerMksck);

		err = Mutex_Lock(&peerMksckPage->mutex, MutexModeSH);
		if (err == 0) {
			uint32 sockIdx = peerMksck->index;
			struct MvpkmVM *vm =
				(struct MvpkmVM *)peerMksckPage->vmHKVA;

			if (vm) {
				WorldSwitchPage *wsp = vm->wsp;

				ASSERT(sockIdx <
				       8 * sizeof(peerMksckPage->wakeVMMRecv));
				ATOMIC_ORV(peerMksckPage->wakeVMMRecv,
					   1U << sockIdx);

				if (wsp)
					Mvpkm_WakeGuest(vm, ACTION_MKSCK);
			}
			Mutex_Unlock(&peerMksckPage->mutex, MutexModeSH);
		}
	}

	if (!err)
		err = len;

decRefc:
	Mksck_DecRefc(peerMksck);
	return err;

unlockDecRefc:
	Mutex_Unlock(&peerMksck->mutex, MutexModeEX);
	goto decRefc;
}


static int
MksckFault(struct vm_area_struct *vma,
	   struct vm_fault *vmf)
{
	return VM_FAULT_SIGBUS;
}

static int
MksckMMap(struct file *file,
	  struct socket *sock,
	  struct vm_area_struct *vma)
{
	vma->vm_ops = &mksckVMOps;

	return 0;
}

void
Mksck_WakeBlockedSockets(MksckPage *mksckPage)
{
	Mksck *mksck;
	uint32 i, wakeHostRecv;

	wakeHostRecv = mksckPage->wakeHostRecv;
	if (wakeHostRecv != 0) {
		mksckPage->wakeHostRecv = 0;
		for (i = 0; wakeHostRecv != 0; i++) {
			if (wakeHostRecv & 1) {
				mksck = &mksckPage->sockets[i];
				Mutex_CondSig(&mksck->mutex,
					      MKSCK_CVAR_FILL, true);
			}
			wakeHostRecv >>= 1;
		}
	}
}

MksckPage *
MksckPageAlloc(void)
{
	uint32 jj;

	MksckPage *mksckPage = vmalloc(MKSCKPAGE_SIZE);

	if (mksckPage) {
		memset(mksckPage, 0, MKSCKPAGE_SIZE);
		ATOMIC_SETV(mksckPage->refCount, 1);
		mksckPage->portStore = MKSCK_PORT_HIGH;

		Mutex_Init(&mksckPage->mutex);
		for (jj = 0; jj < MKSCK_SOCKETS_PER_PAGE; jj++)
			Mutex_Init(&mksckPage->sockets[jj].mutex);
	}

	return mksckPage;
}

static void
MksckPageRelease(MksckPage *mksckPage)
{
	int ii;

	for (ii = 0; ii < MKSCK_SOCKETS_PER_PAGE; ii++)
		Mutex_Destroy(&mksckPage->sockets[ii].mutex);

	Mutex_Destroy(&mksckPage->mutex);

	vfree(mksckPage);
}

static inline Mksck_VmId
GetHostVmId(void)
{
	uint32 jj;
	Mksck_VmId vmId, vmIdFirstVacant = MKSCK_VMID_UNDEF;
	MksckPage *mksckPage;
	uint32 tgid = task_tgid_vnr(current);

	for (jj = 0, vmId = MKSCK_TGID2VMID(tgid);
	     jj < MKSCK_MAX_SHARES;
	     jj++, vmId++) {
		if (vmId > MKSCK_VMID_HIGH)
			vmId = 0;

		mksckPage = mksckPages[MKSCK_VMID2IDX(vmId)];
		if (mksckPage) {
			if (mksckPage->tgid == tgid &&
			    !mksckPage->isGuest)
				return mksckPage->vmId;
		} else if (vmIdFirstVacant == MKSCK_VMID_UNDEF) {
			vmIdFirstVacant = vmId;
		}
	}

	return vmIdFirstVacant;
}


static inline Mksck_VmId
GetNewGuestVmId(void)
{
	Mksck_VmId vmId;

	for (vmId = 0; vmId < MKSCK_MAX_SHARES; vmId++) {
		if (!mksckPages[MKSCK_VMID2IDX(vmId)])
			return vmId;
	}

	return MKSCK_VMID_UNDEF;
}


MksckPage *
MksckPage_GetFromIdx(uint32 idx)
{
	MksckPage *mksckPage = mksckPages[idx];
	ASSERT(mksckPage);
	ASSERT(idx < MKSCK_MAX_SHARES);
	ASSERT(ATOMIC_GETO(mksckPage->refCount));
	return mksckPage;
}

MksckPage *
MksckPage_GetFromVmId(Mksck_VmId vmId)
{
	MksckPage *mksckPage = mksckPages[MKSCK_VMID2IDX(vmId)];
	ASSERT(mksckPage);
	ASSERT(mksckPage->vmId == vmId);
	ASSERT(ATOMIC_GETO(mksckPage->refCount));
	return mksckPage;
}


MksckPage *
MksckPage_GetFromVmIdIncRefc(Mksck_VmId vmId)
{
	MksckPage *mksckPage;

	spin_lock(&mksckPageListLock);
	mksckPage = mksckPages[MKSCK_VMID2IDX(vmId)];

	if (!mksckPage || (mksckPage->vmId != vmId)) {
		pr_info("MksckPage_GetFromVmIdIncRefc: vmId %04X not found\n",
			vmId);
		mksckPage = NULL;
	} else {
		ATOMIC_ADDV(mksckPage->refCount, 1);
	}
	spin_unlock(&mksckPageListLock);
	return mksckPage;
}


MksckPage *
MksckPage_GetFromTgidIncRefc(void)
{
	MksckPage *mksckPage;
	Mksck_VmId vmId;

	while (1) {
		spin_lock(&mksckPageListLock);
		vmId = GetHostVmId();

		if (vmId == MKSCK_VMID_UNDEF) {
			spin_unlock(&mksckPageListLock);
			return NULL;
		}

		mksckPage = mksckPages[MKSCK_VMID2IDX(vmId)];
		if (mksckPage != NULL) {
			ATOMIC_ADDV(mksckPage->refCount, 1);
			spin_unlock(&mksckPageListLock);
			return mksckPage;
		}

		spin_unlock(&mksckPageListLock);
		mksckPage = MksckPageAlloc();
		if (mksckPage == NULL)
			return NULL;

		spin_lock(&mksckPageListLock);
		vmId = GetHostVmId();
		if ((vmId != MKSCK_VMID_UNDEF) &&
		    (mksckPages[MKSCK_VMID2IDX(vmId)] == NULL))
			break;

		spin_unlock(&mksckPageListLock);
		MksckPageRelease(mksckPage);
	}

	mksckPages[MKSCK_VMID2IDX(vmId)] = mksckPage;
	mksckPage->vmId    = vmId;
	mksckPage->isGuest = false;
	mksckPage->vmHKVA  = 0;
	mksckPage->tgid    = task_tgid_vnr(current);
	pr_warn("New host mksck page is allocated: idx %x, vmId %x, tgid %d\n",
		MKSCK_VMID2IDX(vmId), vmId, mksckPage->tgid);

	spin_unlock(&mksckPageListLock);
	return mksckPage;
}

int
Mksck_WspInitialize(struct MvpkmVM *vm)
{
	WorldSwitchPage *wsp = vm->wsp;
	int err;
	Mksck_VmId vmId;
	MksckPage *mksckPage;

	if (wsp->guestId)
		return -EBUSY;

	mksckPage = MksckPageAlloc();
	if (!mksckPage)
		return -ENOMEM;

	spin_lock(&mksckPageListLock);

	vmId = GetNewGuestVmId();
	if (vmId == MKSCK_VMID_UNDEF) {
		err = -EMFILE;
		MksckPageRelease(mksckPage);
		pr_err("Mksck_WspInitialize: Cannot allocate vmId\n");
	} else {
		mksckPages[MKSCK_VMID2IDX(vmId)] = mksckPage;
		mksckPage->vmId    = vmId;
		mksckPage->isGuest = true;
		mksckPage->vmHKVA  = (HKVA)vm;
		

		wsp->guestId = vmId;

		pr_warn("New guest mksck page is allocated: idx %x, vmId %x\n",
			MKSCK_VMID2IDX(vmId), vmId);

		err = 0;

		/*
		 * All stable, ie, mksckPages[] written, ok to unlock now.
		 */
		spin_unlock(&mksckPageListLock);
	}

	return err;
}

void
Mksck_WspRelease(WorldSwitchPage *wsp)
{
	int ii;
	int err;
	MksckPage *mksckPage = MksckPage_GetFromVmId(wsp->guestId);

	uint32 isOpened = wsp->isOpened;
	Mksck *mksck = mksckPage->sockets;

	while (isOpened) {
		if (isOpened & 1) {
			ASSERT(ATOMIC_GETO(mksck->refCount) != 0);

			if (mksck->peer) {
				MksckPage *mksckPagePeer =
				    MksckPage_GetFromVmId(mksck->peerAddr.vmId);

				ASSERT(mksckPagePeer);
				mksck->peer =
				    MksckPage_GetFromAddr(mksckPagePeer,
							  mksck->peerAddr);
				ASSERT(mksck->peer);
				
			}

			Mksck_CloseCommon(mksck);
		}
		isOpened >>= 1;
		mksck++;
	}

	err = Mutex_Lock(&mksckPage->mutex, MutexModeEX);
	ASSERT(!err);
	mksckPage->vmHKVA = 0;
	Mutex_Unlock(&mksckPage->mutex, MutexModeEX);

	MksckPage_DecRefc(mksckPage);

	if (wsp->guestPageMapped) {
		wsp->guestPageMapped = false;
		MksckPage_DecRefc(mksckPage);
	}

	for (ii = 0; ii < MKSCK_MAX_SHARES; ii++) {
		if (wsp->isPageMapped[ii]) {
			MksckPage *mksckPageOther = MksckPage_GetFromIdx(ii);

			wsp->isPageMapped[ii] = false;
			MksckPage_DecRefc(mksckPageOther);
		}
	}
}

void
Mksck_DisconnectPeer(Mksck *mksck)
{
	Mksck *peerMksck = mksck->peer;

	if (peerMksck != NULL) {
		mksck->peer = NULL;
		mksck->peerAddr.addr = MKSCK_ADDR_UNDEF;
		Mksck_DecRefc(peerMksck);
	}
}


void
MksckPage_DecRefc(struct MksckPage *mksckPage)
{
	uint32 oldRefc;

	DMB();
	do {
		while ((oldRefc = ATOMIC_GETO(mksckPage->refCount)) == 1) {

			spin_lock(&mksckPageListLock);
			if (ATOMIC_SETIF(mksckPage->refCount, 0, 1)) {
				uint32 ii = MKSCK_VMID2IDX(mksckPage->vmId);

				ASSERT(ii < MKSCK_MAX_SHARES);
				ASSERT(mksckPages[ii] == mksckPage);
				mksckPages[ii] = NULL;
				spin_unlock(&mksckPageListLock);
				pr_warn("%s mksck page is released: idx %x, " \
					"vmId %x, tgid %d\n",
					mksckPage->isGuest ? "Guest" : "Host",
					ii, mksckPage->vmId, mksckPage->tgid);
				MksckPageRelease(mksckPage);
				return;
			}
			spin_unlock(&mksckPageListLock);
		}
		ASSERT(oldRefc != 0);
	} while (!ATOMIC_SETIF(mksckPage->refCount, oldRefc - 1, oldRefc));
}

int
MksckPage_LookupAndInsertPage(struct vm_area_struct *vma,
			      unsigned long address,
			      MPN mpn)
{
	int ii, jj;
	struct MksckPage **mksckPagePtr = mksckPages;

	spin_lock(&mksckPageListLock);
	for (jj = MKSCK_MAX_SHARES; jj--; mksckPagePtr++) {
		if (*mksckPagePtr) {
			for (ii = 0; ii < MKSCKPAGE_TOTAL; ii++) {
				HKVA tmp = ((HKVA)*mksckPagePtr) +
						ii * PAGE_SIZE;

				if (vmalloc_to_pfn((void *)tmp) == mpn &&
				    vm_insert_page(vma, address,
						   pfn_to_page(mpn)) == 0) {
					spin_unlock(&mksckPageListLock);
					return 0;
				}
			}
		}
	}
	spin_unlock(&mksckPageListLock);
	return -1;
}


static int
MksckPageInfoShow(struct seq_file *m,
		  void *private)
{
	int ii, jj;
	uint32 isPageMapped = 0;
	int err;
	struct MvpkmVM *vm;

	spin_lock(&mksckPageListLock);
	for (ii = 0; ii < MKSCK_MAX_SHARES; ii++) {
		struct MksckPage *mksckPage  = mksckPages[ii];

		if (mksckPage != NULL && mksckPage->isGuest) {
			ATOMIC_ADDV(mksckPage->refCount, 1);
			spin_unlock(&mksckPageListLock);

			err = Mutex_Lock(&mksckPage->mutex, MutexModeEX);
			vm = (struct MvpkmVM *)mksckPage->vmHKVA;

			if (err == 0 && vm && vm->wsp) {
				for (jj = 0; jj < MKSCK_MAX_SHARES; jj++) {
					if (vm->wsp->isPageMapped[jj])
						isPageMapped |= 1<<jj;
				}
			}
			Mutex_Unlock(&mksckPage->mutex, MutexModeEX);

			MksckPage_DecRefc(mksckPage);
			spin_lock(&mksckPageListLock);
			break;
		}
	}

	for (ii = 0; ii < MKSCK_MAX_SHARES; ii++) {
		struct MksckPage *mksckPage  = mksckPages[ii];

		if (mksckPage != NULL) {
			uint32 lState = ATOMIC_GETO(mksckPage->mutex.state);
			uint32 isOpened = 0; 

			seq_printf(m, "MksckPage[%02d]: { vmId = %4x(%c), " \
				   "refC = %2d%s", ii, mksckPage->vmId,
				   mksckPage->isGuest ? 'G' : 'H',
				   ATOMIC_GETO(mksckPage->refCount),
				   (isPageMapped&(1<<ii) ? "*" : ""));

			if (lState)
				seq_printf(m, ", lock=%x locked by line %d, " \
					   "unlocked by %d",
					   lState, mksckPage->mutex.line,
					   mksckPage->mutex.lineUnl);

			if (!mksckPage->isGuest) {
				struct task_struct *target;

				seq_printf(m, ", tgid = %d", mksckPage->tgid);

				rcu_read_lock();

				target = pid_task(find_vpid(mksckPage->tgid),
						  PIDTYPE_PID);
				seq_printf(m, "(%s)",
					   (target ? target->comm :
						     "no such process"));

				rcu_read_unlock();
			} else {
				ATOMIC_ADDV(mksckPage->refCount, 1);
				spin_unlock(&mksckPageListLock);

				err = Mutex_Lock(&mksckPage->mutex,
						 MutexModeEX);
				vm = (struct MvpkmVM *)mksckPage->vmHKVA;

				if (err == 0 && vm && vm->wsp)
					isOpened = vm->wsp->isOpened;

				Mutex_Unlock(&mksckPage->mutex, MutexModeEX);
				MksckPage_DecRefc(mksckPage);
				spin_lock(&mksckPageListLock);

				if (mksckPage != mksckPages[ii]) {
					seq_puts(m, " released }\n");
					continue;
				}
			}
			seq_puts(m, ", sockets[] = {");

			for (jj = 0;
			     jj < mksckPage->numAllocSocks;
			     jj++, isOpened >>= 1) {
				Mksck *mksck = mksckPage->sockets + jj;

				if (ATOMIC_GETO(mksck->refCount)) {
					uint32 blocked;
					char *shutdRO =
					    (mksck->shutDown & MKSCK_SHUT_RD ?
						" SHUTD_RD" : "");
					char *shutdRW =
					    (mksck->shutDown & MKSCK_SHUT_WR ?
						" SHUTD_WR" : "");

					lState =
						ATOMIC_GETO(mksck->mutex.state);

					seq_printf(m, "\n             " \
						   "{ addr = %8x, " \
						   "refC = %2d%s%s%s",
						   mksck->addr.addr,
						   ATOMIC_GETO(mksck->refCount),
						   (isOpened & 1 ? "*" : ""),
						   shutdRO,
						   shutdRW);

					if (mksck->peer)
						seq_printf(m,
							   ", peerAddr = %8x",
							  mksck->peerAddr.addr);

					if (lState)
						seq_printf(m,
							   ", lock=%x locked " \
							   "by line %d, " \
							   "unlocked by %d",
							   lState,
							   mksck->mutex.line,
							  mksck->mutex.lineUnl);

					blocked =
					    ATOMIC_GETO(mksck->mutex.blocked);
					if (blocked)
						seq_printf(m, ", blocked=%d",
							   blocked);

					seq_puts(m, " }");
				}
			}
			seq_puts(m, " } }\n");
		}
	}
	spin_unlock(&mksckPageListLock);

	return 0;
}


static int
MksckPageInfoOpen(struct inode *inode,
		  struct file *file)
{
	return single_open(file, MksckPageInfoShow, inode->i_private);
}

static const struct file_operations mksckPageInfoFops = {
	.open = MksckPageInfoOpen,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void
MksckPageInfo_Init(struct dentry *parent)
{
	debugfs_create_file("mksckPage", S_IROTH, parent,
			    NULL, &mksckPageInfoFops);
}
