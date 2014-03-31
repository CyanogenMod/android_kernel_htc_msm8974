/*
 *   Copyright (C) International Business Machines Corp., 2000-2005
 *   Portions Copyright (C) Christoph Hellwig, 2001-2002
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/completion.h>
#include <linux/freezer.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kthread.h>
#include <linux/seq_file.h>
#include "jfs_incore.h"
#include "jfs_inode.h"
#include "jfs_filsys.h"
#include "jfs_metapage.h"
#include "jfs_dinode.h"
#include "jfs_imap.h"
#include "jfs_dmap.h"
#include "jfs_superblock.h"
#include "jfs_debug.h"

static struct {
	int freetid;		
	int freelock;		
	wait_queue_head_t freewait;	
	wait_queue_head_t freelockwait;	
	wait_queue_head_t lowlockwait;	
	int tlocksInUse;	
	spinlock_t LazyLock;	
	struct list_head unlock_queue;	
	struct list_head anon_list;	
	struct list_head anon_list2;	
} TxAnchor;

int jfs_tlocks_low;		

#ifdef CONFIG_JFS_STATISTICS
static struct {
	uint txBegin;
	uint txBegin_barrier;
	uint txBegin_lockslow;
	uint txBegin_freetid;
	uint txBeginAnon;
	uint txBeginAnon_barrier;
	uint txBeginAnon_lockslow;
	uint txLockAlloc;
	uint txLockAlloc_freelock;
} TxStat;
#endif

static int nTxBlock = -1;	
module_param(nTxBlock, int, 0);
MODULE_PARM_DESC(nTxBlock,
		 "Number of transaction blocks (max:65536)");

static int nTxLock = -1;	
module_param(nTxLock, int, 0);
MODULE_PARM_DESC(nTxLock,
		 "Number of transaction locks (max:65536)");

struct tblock *TxBlock;	
static int TxLockLWM;	
static int TxLockHWM;	
static int TxLockVHWM;	
struct tlock *TxLock;	

static DEFINE_SPINLOCK(jfsTxnLock);

#define TXN_LOCK()		spin_lock(&jfsTxnLock)
#define TXN_UNLOCK()		spin_unlock(&jfsTxnLock)

#define LAZY_LOCK_INIT()	spin_lock_init(&TxAnchor.LazyLock);
#define LAZY_LOCK(flags)	spin_lock_irqsave(&TxAnchor.LazyLock, flags)
#define LAZY_UNLOCK(flags) spin_unlock_irqrestore(&TxAnchor.LazyLock, flags)

static DECLARE_WAIT_QUEUE_HEAD(jfs_commit_thread_wait);
static int jfs_commit_thread_waking;

static inline void TXN_SLEEP_DROP_LOCK(wait_queue_head_t * event)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(event, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);
	TXN_UNLOCK();
	io_schedule();
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(event, &wait);
}

#define TXN_SLEEP(event)\
{\
	TXN_SLEEP_DROP_LOCK(event);\
	TXN_LOCK();\
}

#define TXN_WAKEUP(event) wake_up_all(event)

static struct {
	tid_t maxtid;		
	lid_t maxlid;		
	int ntid;		
	int nlid;		
	int waitlock;		
} stattx;

static int diLog(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
		struct tlock * tlck, struct commit * cd);
static int dataLog(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
		struct tlock * tlck);
static void dtLog(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
		struct tlock * tlck);
static void mapLog(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
		struct tlock * tlck);
static void txAllocPMap(struct inode *ip, struct maplock * maplock,
		struct tblock * tblk);
static void txForce(struct tblock * tblk);
static int txLog(struct jfs_log * log, struct tblock * tblk,
		struct commit * cd);
static void txUpdateMap(struct tblock * tblk);
static void txRelease(struct tblock * tblk);
static void xtLog(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
	   struct tlock * tlck);
static void LogSyncRelease(struct metapage * mp);


static lid_t txLockAlloc(void)
{
	lid_t lid;

	INCREMENT(TxStat.txLockAlloc);
	if (!TxAnchor.freelock) {
		INCREMENT(TxStat.txLockAlloc_freelock);
	}

	while (!(lid = TxAnchor.freelock))
		TXN_SLEEP(&TxAnchor.freelockwait);
	TxAnchor.freelock = TxLock[lid].next;
	HIGHWATERMARK(stattx.maxlid, lid);
	if ((++TxAnchor.tlocksInUse > TxLockHWM) && (jfs_tlocks_low == 0)) {
		jfs_info("txLockAlloc tlocks low");
		jfs_tlocks_low = 1;
		wake_up_process(jfsSyncThread);
	}

	return lid;
}

static void txLockFree(lid_t lid)
{
	TxLock[lid].tid = 0;
	TxLock[lid].next = TxAnchor.freelock;
	TxAnchor.freelock = lid;
	TxAnchor.tlocksInUse--;
	if (jfs_tlocks_low && (TxAnchor.tlocksInUse < TxLockLWM)) {
		jfs_info("txLockFree jfs_tlocks_low no more");
		jfs_tlocks_low = 0;
		TXN_WAKEUP(&TxAnchor.lowlockwait);
	}
	TXN_WAKEUP(&TxAnchor.freelockwait);
}

int txInit(void)
{
	int k, size;
	struct sysinfo si;

	

	if (nTxLock == -1) {
		if (nTxBlock == -1) {
			
			si_meminfo(&si);
			if (si.totalram > (256 * 1024)) 
				nTxLock = 64 * 1024;
			else
				nTxLock = si.totalram >> 2;
		} else if (nTxBlock > (8 * 1024))
			nTxLock = 64 * 1024;
		else
			nTxLock = nTxBlock << 3;
	}
	if (nTxBlock == -1)
		nTxBlock = nTxLock >> 3;

	
	if (nTxBlock < 16)
		nTxBlock = 16;	
	if (nTxBlock > 65536)
		nTxBlock = 65536;
	if (nTxLock < 256)
		nTxLock = 256;	
	if (nTxLock > 65536)
		nTxLock = 65536;

	printk(KERN_INFO "JFS: nTxBlock = %d, nTxLock = %d\n",
	       nTxBlock, nTxLock);
	TxLockLWM = (nTxLock * 4) / 10;
	TxLockHWM = (nTxLock * 7) / 10;
	TxLockVHWM = (nTxLock * 8) / 10;

	size = sizeof(struct tblock) * nTxBlock;
	TxBlock = vmalloc(size);
	if (TxBlock == NULL)
		return -ENOMEM;

	for (k = 1; k < nTxBlock - 1; k++) {
		TxBlock[k].next = k + 1;
		init_waitqueue_head(&TxBlock[k].gcwait);
		init_waitqueue_head(&TxBlock[k].waitor);
	}
	TxBlock[k].next = 0;
	init_waitqueue_head(&TxBlock[k].gcwait);
	init_waitqueue_head(&TxBlock[k].waitor);

	TxAnchor.freetid = 1;
	init_waitqueue_head(&TxAnchor.freewait);

	stattx.maxtid = 1;	

	size = sizeof(struct tlock) * nTxLock;
	TxLock = vmalloc(size);
	if (TxLock == NULL) {
		vfree(TxBlock);
		return -ENOMEM;
	}

	
	for (k = 1; k < nTxLock - 1; k++)
		TxLock[k].next = k + 1;
	TxLock[k].next = 0;
	init_waitqueue_head(&TxAnchor.freelockwait);
	init_waitqueue_head(&TxAnchor.lowlockwait);

	TxAnchor.freelock = 1;
	TxAnchor.tlocksInUse = 0;
	INIT_LIST_HEAD(&TxAnchor.anon_list);
	INIT_LIST_HEAD(&TxAnchor.anon_list2);

	LAZY_LOCK_INIT();
	INIT_LIST_HEAD(&TxAnchor.unlock_queue);

	stattx.maxlid = 1;	

	return 0;
}

void txExit(void)
{
	vfree(TxLock);
	TxLock = NULL;
	vfree(TxBlock);
	TxBlock = NULL;
}

tid_t txBegin(struct super_block *sb, int flag)
{
	tid_t t;
	struct tblock *tblk;
	struct jfs_log *log;

	jfs_info("txBegin: flag = 0x%x", flag);
	log = JFS_SBI(sb)->log;

	TXN_LOCK();

	INCREMENT(TxStat.txBegin);

      retry:
	if (!(flag & COMMIT_FORCE)) {
		if (test_bit(log_SYNCBARRIER, &log->flag) ||
		    test_bit(log_QUIESCE, &log->flag)) {
			INCREMENT(TxStat.txBegin_barrier);
			TXN_SLEEP(&log->syncwait);
			goto retry;
		}
	}
	if (flag == 0) {
		if (TxAnchor.tlocksInUse > TxLockVHWM) {
			INCREMENT(TxStat.txBegin_lockslow);
			TXN_SLEEP(&TxAnchor.lowlockwait);
			goto retry;
		}
	}

	if ((t = TxAnchor.freetid) == 0) {
		jfs_info("txBegin: waiting for free tid");
		INCREMENT(TxStat.txBegin_freetid);
		TXN_SLEEP(&TxAnchor.freewait);
		goto retry;
	}

	tblk = tid_to_tblock(t);

	if ((tblk->next == 0) && !(flag & COMMIT_FORCE)) {
		
		jfs_info("txBegin: waiting for free tid");
		INCREMENT(TxStat.txBegin_freetid);
		TXN_SLEEP(&TxAnchor.freewait);
		goto retry;
	}

	TxAnchor.freetid = tblk->next;


	tblk->next = tblk->last = tblk->xflag = tblk->flag = tblk->lsn = 0;

	tblk->sb = sb;
	++log->logtid;
	tblk->logtid = log->logtid;

	++log->active;

	HIGHWATERMARK(stattx.maxtid, t);	
	INCREMENT(stattx.ntid);	

	TXN_UNLOCK();

	jfs_info("txBegin: returning tid = %d", t);

	return t;
}

void txBeginAnon(struct super_block *sb)
{
	struct jfs_log *log;

	log = JFS_SBI(sb)->log;

	TXN_LOCK();
	INCREMENT(TxStat.txBeginAnon);

      retry:
	if (test_bit(log_SYNCBARRIER, &log->flag) ||
	    test_bit(log_QUIESCE, &log->flag)) {
		INCREMENT(TxStat.txBeginAnon_barrier);
		TXN_SLEEP(&log->syncwait);
		goto retry;
	}

	if (TxAnchor.tlocksInUse > TxLockVHWM) {
		INCREMENT(TxStat.txBeginAnon_lockslow);
		TXN_SLEEP(&TxAnchor.lowlockwait);
		goto retry;
	}
	TXN_UNLOCK();
}

void txEnd(tid_t tid)
{
	struct tblock *tblk = tid_to_tblock(tid);
	struct jfs_log *log;

	jfs_info("txEnd: tid = %d", tid);
	TXN_LOCK();

	TXN_WAKEUP(&tblk->waitor);

	log = JFS_SBI(tblk->sb)->log;

	if (tblk->flag & tblkGC_LAZY) {
		jfs_info("txEnd called w/lazy tid: %d, tblk = 0x%p", tid, tblk);
		TXN_UNLOCK();

		spin_lock_irq(&log->gclock);	
		tblk->flag |= tblkGC_UNLOCKED;
		spin_unlock_irq(&log->gclock);	
		return;
	}

	jfs_info("txEnd: tid: %d, tblk = 0x%p", tid, tblk);

	assert(tblk->next == 0);

	tblk->next = TxAnchor.freetid;
	TxAnchor.freetid = tid;

	if (--log->active == 0) {
		clear_bit(log_FLUSH, &log->flag);

		if (test_bit(log_SYNCBARRIER, &log->flag)) {
			TXN_UNLOCK();

			
			jfs_syncpt(log, 1);

			jfs_info("log barrier off: 0x%x", log->lsn);

			
			clear_bit(log_SYNCBARRIER, &log->flag);

			
			TXN_WAKEUP(&log->syncwait);

			goto wakeup;
		}
	}

	TXN_UNLOCK();
wakeup:
	TXN_WAKEUP(&TxAnchor.freewait);
}

struct tlock *txLock(tid_t tid, struct inode *ip, struct metapage * mp,
		     int type)
{
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	int dir_xtree = 0;
	lid_t lid;
	tid_t xtid;
	struct tlock *tlck;
	struct xtlock *xtlck;
	struct linelock *linelock;
	xtpage_t *p;
	struct tblock *tblk;

	TXN_LOCK();

	if (S_ISDIR(ip->i_mode) && (type & tlckXTREE) &&
	    !(mp->xflag & COMMIT_PAGE)) {
		dir_xtree = 1;
		lid = jfs_ip->xtlid;
	} else
		lid = mp->lid;

	
	if (lid == 0)
		goto allocateLock;

	jfs_info("txLock: tid:%d ip:0x%p mp:0x%p lid:%d", tid, ip, mp, lid);

	
	tlck = lid_to_tlock(lid);
	if ((xtid = tlck->tid) == tid) {
		TXN_UNLOCK();
		goto grantLock;
	}

	if (xtid == 0) {
		tlck->tid = tid;
		TXN_UNLOCK();
		tblk = tid_to_tblock(tid);
		if (jfs_ip->atlhead == lid) {
			if (jfs_ip->atltail == lid) {
				TXN_LOCK();
				list_del_init(&jfs_ip->anon_inode_list);
				TXN_UNLOCK();
			}
			jfs_ip->atlhead = tlck->next;
		} else {
			lid_t last;
			for (last = jfs_ip->atlhead;
			     lid_to_tlock(last)->next != lid;
			     last = lid_to_tlock(last)->next) {
				assert(last);
			}
			lid_to_tlock(last)->next = tlck->next;
			if (jfs_ip->atltail == lid)
				jfs_ip->atltail = last;
		}

		

		if (tblk->next)
			lid_to_tlock(tblk->last)->next = lid;
		else
			tblk->next = lid;
		tlck->next = 0;
		tblk->last = lid;

		goto grantLock;
	}

	goto waitLock;

      allocateLock:
	lid = txLockAlloc();
	tlck = lid_to_tlock(lid);

	tlck->tid = tid;

	TXN_UNLOCK();

	
	if (mp->xflag & COMMIT_PAGE) {

		tlck->flag = tlckPAGELOCK;

		
		metapage_nohomeok(mp);

		jfs_info("locking mp = 0x%p, nohomeok = %d tid = %d tlck = 0x%p",
			 mp, mp->nohomeok, tid, tlck);

		if ((tid == 0) && mp->lsn)
			set_cflag(COMMIT_Synclist, ip);
	}
	
	else
		tlck->flag = tlckINODELOCK;

	if (S_ISDIR(ip->i_mode))
		tlck->flag |= tlckDIRECTORY;

	tlck->type = 0;

	
	tlck->ip = ip;
	tlck->mp = mp;
	if (dir_xtree)
		jfs_ip->xtlid = lid;
	else
		mp->lid = lid;

	
	if (tid) {
		tblk = tid_to_tblock(tid);
		if (tblk->next)
			lid_to_tlock(tblk->last)->next = lid;
		else
			tblk->next = lid;
		tlck->next = 0;
		tblk->last = lid;
	}
	else {
		tlck->next = jfs_ip->atlhead;
		jfs_ip->atlhead = lid;
		if (tlck->next == 0) {
			
			jfs_ip->atltail = lid;
			TXN_LOCK();
			list_add_tail(&jfs_ip->anon_inode_list,
				      &TxAnchor.anon_list);
			TXN_UNLOCK();
		}
	}

	
	linelock = (struct linelock *) & tlck->lock;
	linelock->next = 0;
	linelock->flag = tlckLINELOCK;
	linelock->maxcnt = TLOCKSHORT;
	linelock->index = 0;

	switch (type & tlckTYPE) {
	case tlckDTREE:
		linelock->l2linesize = L2DTSLOTSIZE;
		break;

	case tlckXTREE:
		linelock->l2linesize = L2XTSLOTSIZE;

		xtlck = (struct xtlock *) linelock;
		xtlck->header.offset = 0;
		xtlck->header.length = 2;

		if (type & tlckNEW) {
			xtlck->lwm.offset = XTENTRYSTART;
		} else {
			if (mp->xflag & COMMIT_PAGE)
				p = (xtpage_t *) mp->data;
			else
				p = &jfs_ip->i_xtroot;
			xtlck->lwm.offset =
			    le16_to_cpu(p->header.nextindex);
		}
		xtlck->lwm.length = 0;	
		xtlck->twm.offset = 0;
		xtlck->hwm.offset = 0;

		xtlck->index = 2;
		break;

	case tlckINODE:
		linelock->l2linesize = L2INODESLOTSIZE;
		break;

	case tlckDATA:
		linelock->l2linesize = L2DATASLOTSIZE;
		break;

	default:
		jfs_err("UFO tlock:0x%p", tlck);
	}

      grantLock:
	tlck->type |= type;

	return tlck;

      waitLock:
	
	
	if (jfs_ip->fileset != AGGREGATE_I) {
		printk(KERN_ERR "txLock: trying to lock locked page!");
		print_hex_dump(KERN_ERR, "ip: ", DUMP_PREFIX_ADDRESS, 16, 4,
			       ip, sizeof(*ip), 0);
		print_hex_dump(KERN_ERR, "mp: ", DUMP_PREFIX_ADDRESS, 16, 4,
			       mp, sizeof(*mp), 0);
		print_hex_dump(KERN_ERR, "Locker's tblock: ",
			       DUMP_PREFIX_ADDRESS, 16, 4, tid_to_tblock(tid),
			       sizeof(struct tblock), 0);
		print_hex_dump(KERN_ERR, "Tlock: ", DUMP_PREFIX_ADDRESS, 16, 4,
			       tlck, sizeof(*tlck), 0);
		BUG();
	}
	INCREMENT(stattx.waitlock);	
	TXN_UNLOCK();
	release_metapage(mp);
	TXN_LOCK();
	xtid = tlck->tid;	

	jfs_info("txLock: in waitLock, tid = %d, xtid = %d, lid = %d",
		 tid, xtid, lid);

	
	if (xtid && (tlck->mp == mp) && (mp->lid == lid))
		TXN_SLEEP_DROP_LOCK(&tid_to_tblock(xtid)->waitor);
	else
		TXN_UNLOCK();
	jfs_info("txLock: awakened     tid = %d, lid = %d", tid, lid);

	return NULL;
}

/*
 * NAME:	txRelease()
 *
 * FUNCTION:	Release buffers associated with transaction locks, but don't
 *		mark homeok yet.  The allows other transactions to modify
 *		buffers, but won't let them go to disk until commit record
 *		actually gets written.
 *
 * PARAMETER:
 *		tblk	-
 *
 * RETURN:	Errors from subroutines.
 */
static void txRelease(struct tblock * tblk)
{
	struct metapage *mp;
	lid_t lid;
	struct tlock *tlck;

	TXN_LOCK();

	for (lid = tblk->next; lid; lid = tlck->next) {
		tlck = lid_to_tlock(lid);
		if ((mp = tlck->mp) != NULL &&
		    (tlck->type & tlckBTROOT) == 0) {
			assert(mp->xflag & COMMIT_PAGE);
			mp->lid = 0;
		}
	}

	TXN_WAKEUP(&tblk->waitor);

	TXN_UNLOCK();
}

static void txUnlock(struct tblock * tblk)
{
	struct tlock *tlck;
	struct linelock *linelock;
	lid_t lid, next, llid, k;
	struct metapage *mp;
	struct jfs_log *log;
	int difft, diffp;
	unsigned long flags;

	jfs_info("txUnlock: tblk = 0x%p", tblk);
	log = JFS_SBI(tblk->sb)->log;

	/*
	 * mark page under tlock homeok (its log has been written):
	 */
	for (lid = tblk->next; lid; lid = next) {
		tlck = lid_to_tlock(lid);
		next = tlck->next;

		jfs_info("unlocking lid = %d, tlck = 0x%p", lid, tlck);

		
		if ((mp = tlck->mp) != NULL &&
		    (tlck->type & tlckBTROOT) == 0) {
			assert(mp->xflag & COMMIT_PAGE);

			hold_metapage(mp);

			assert(mp->nohomeok > 0);
			_metapage_homeok(mp);

			
			LOGSYNC_LOCK(log, flags);
			if (mp->clsn) {
				logdiff(difft, tblk->clsn, log);
				logdiff(diffp, mp->clsn, log);
				if (difft > diffp)
					mp->clsn = tblk->clsn;
			} else
				mp->clsn = tblk->clsn;
			LOGSYNC_UNLOCK(log, flags);

			assert(!(tlck->flag & tlckFREEPAGE));

			put_metapage(mp);
		}

		TXN_LOCK();

		llid = ((struct linelock *) & tlck->lock)->next;
		while (llid) {
			linelock = (struct linelock *) lid_to_tlock(llid);
			k = linelock->next;
			txLockFree(llid);
			llid = k;
		}
		txLockFree(lid);

		TXN_UNLOCK();
	}
	tblk->next = tblk->last = 0;

	if (tblk->lsn) {
		LOGSYNC_LOCK(log, flags);
		log->count--;
		list_del(&tblk->synclist);
		LOGSYNC_UNLOCK(log, flags);
	}
}

struct tlock *txMaplock(tid_t tid, struct inode *ip, int type)
{
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	lid_t lid;
	struct tblock *tblk;
	struct tlock *tlck;
	struct maplock *maplock;

	TXN_LOCK();

	lid = txLockAlloc();
	tlck = lid_to_tlock(lid);

	tlck->tid = tid;

	
	tlck->flag = tlckINODELOCK;
	if (S_ISDIR(ip->i_mode))
		tlck->flag |= tlckDIRECTORY;
	tlck->ip = ip;
	tlck->mp = NULL;

	tlck->type = type;

	
	if (tid) {
		tblk = tid_to_tblock(tid);
		if (tblk->next)
			lid_to_tlock(tblk->last)->next = lid;
		else
			tblk->next = lid;
		tlck->next = 0;
		tblk->last = lid;
	}
	else {
		tlck->next = jfs_ip->atlhead;
		jfs_ip->atlhead = lid;
		if (tlck->next == 0) {
			
			jfs_ip->atltail = lid;
			list_add_tail(&jfs_ip->anon_inode_list,
				      &TxAnchor.anon_list);
		}
	}

	TXN_UNLOCK();

	
	maplock = (struct maplock *) & tlck->lock;
	maplock->next = 0;
	maplock->maxcnt = 0;
	maplock->index = 0;

	return tlck;
}

struct linelock *txLinelock(struct linelock * tlock)
{
	lid_t lid;
	struct tlock *tlck;
	struct linelock *linelock;

	TXN_LOCK();

	
	lid = txLockAlloc();
	tlck = lid_to_tlock(lid);

	TXN_UNLOCK();

	
	linelock = (struct linelock *) tlck;
	linelock->next = 0;
	linelock->flag = tlckLINELOCK;
	linelock->maxcnt = TLOCKLONG;
	linelock->index = 0;
	if (tlck->flag & tlckDIRECTORY)
		linelock->flag |= tlckDIRECTORY;

	
	linelock->next = tlock->next;
	tlock->next = lid;

	return linelock;
}


int txCommit(tid_t tid,		
	     int nip,		
	     struct inode **iplist,	
	     int flag)
{
	int rc = 0;
	struct commit cd;
	struct jfs_log *log;
	struct tblock *tblk;
	struct lrd *lrd;
	struct inode *ip;
	struct jfs_inode_info *jfs_ip;
	int k, n;
	ino_t top;
	struct super_block *sb;

	jfs_info("txCommit, tid = %d, flag = %d", tid, flag);
	
	if (isReadOnly(iplist[0])) {
		rc = -EROFS;
		goto TheEnd;
	}

	sb = cd.sb = iplist[0]->i_sb;
	cd.tid = tid;

	if (tid == 0)
		tid = txBegin(sb, 0);
	tblk = tid_to_tblock(tid);

	log = JFS_SBI(sb)->log;
	cd.log = log;

	
	lrd = &cd.lrd;
	lrd->logtid = cpu_to_le32(tblk->logtid);
	lrd->backchain = 0;

	tblk->xflag |= flag;

	if ((flag & (COMMIT_FORCE | COMMIT_SYNC)) == 0)
		tblk->xflag |= COMMIT_LAZY;
	cd.iplist = iplist;
	cd.nip = nip;

	for (k = 0; k < cd.nip; k++) {
		top = (cd.iplist[k])->i_ino;
		for (n = k + 1; n < cd.nip; n++) {
			ip = cd.iplist[n];
			if (ip->i_ino > top) {
				top = ip->i_ino;
				cd.iplist[n] = cd.iplist[k];
				cd.iplist[k] = ip;
			}
		}

		ip = cd.iplist[k];
		jfs_ip = JFS_IP(ip);

		/*
		 * BUGBUG - This code has temporarily been removed.  The
		 * intent is to ensure that any file data is written before
		 * the metadata is committed to the journal.  This prevents
		 * uninitialized data from appearing in a file after the
		 * journal has been replayed.  (The uninitialized data
		 * could be sensitive data removed by another user.)
		 *
		 * The problem now is that we are holding the IWRITELOCK
		 * on the inode, and calling filemap_fdatawrite on an
		 * unmapped page will cause a deadlock in jfs_get_block.
		 *
		 * The long term solution is to pare down the use of
		 * IWRITELOCK.  We are currently holding it too long.
		 * We could also be smarter about which data pages need
		 * to be written before the transaction is committed and
		 * when we don't need to worry about it at all.
		 *
		 * if ((!S_ISDIR(ip->i_mode))
		 *    && (tblk->flag & COMMIT_DELETE) == 0)
		 *	filemap_write_and_wait(ip->i_mapping);
		 */

		clear_cflag(COMMIT_Dirty, ip);

		
		if (jfs_ip->atlhead) {
			lid_to_tlock(jfs_ip->atltail)->next = tblk->next;
			tblk->next = jfs_ip->atlhead;
			if (!tblk->last)
				tblk->last = jfs_ip->atltail;
			jfs_ip->atlhead = jfs_ip->atltail = 0;
			TXN_LOCK();
			list_del_init(&jfs_ip->anon_inode_list);
			TXN_UNLOCK();
		}

		if (((rc = diWrite(tid, ip))))
			goto out;
	}

	if ((rc = txLog(log, tblk, &cd)))
		goto TheEnd;

	if (tblk->xflag & COMMIT_DELETE) {
		ihold(tblk->u.ip);
		if (tblk->u.ip->i_state & I_SYNC)
			tblk->xflag &= ~COMMIT_LAZY;
	}

	ASSERT((!(tblk->xflag & COMMIT_DELETE)) ||
	       ((tblk->u.ip->i_nlink == 0) &&
		!test_cflag(COMMIT_Nolink, tblk->u.ip)));

	lrd->type = cpu_to_le16(LOG_COMMIT);
	lrd->length = 0;
	lmLog(log, tblk, lrd, NULL);

	lmGroupCommit(log, tblk);


	if (flag & COMMIT_FORCE)
		txForce(tblk);

	if (tblk->xflag & COMMIT_FORCE)
		txUpdateMap(tblk);

	txRelease(tblk);

	if ((tblk->flag & tblkGC_LAZY) == 0)
		txUnlock(tblk);


	for (k = 0; k < cd.nip; k++) {
		ip = cd.iplist[k];
		jfs_ip = JFS_IP(ip);

		jfs_ip->bxflag = 0;
		jfs_ip->blid = 0;
	}

      out:
	if (rc != 0)
		txAbort(tid, 1);

      TheEnd:
	jfs_info("txCommit: tid = %d, returning %d", tid, rc);
	return rc;
}

static int txLog(struct jfs_log * log, struct tblock * tblk, struct commit * cd)
{
	int rc = 0;
	struct inode *ip;
	lid_t lid;
	struct tlock *tlck;
	struct lrd *lrd = &cd->lrd;

	for (lid = tblk->next; lid; lid = tlck->next) {
		tlck = lid_to_tlock(lid);

		tlck->flag |= tlckLOG;

		
		ip = tlck->ip;
		lrd->aggregate = cpu_to_le32(JFS_SBI(ip->i_sb)->aggregate);
		lrd->log.redopage.fileset = cpu_to_le32(JFS_IP(ip)->fileset);
		lrd->log.redopage.inode = cpu_to_le32(ip->i_ino);

		
		switch (tlck->type & tlckTYPE) {
		case tlckXTREE:
			xtLog(log, tblk, lrd, tlck);
			break;

		case tlckDTREE:
			dtLog(log, tblk, lrd, tlck);
			break;

		case tlckINODE:
			diLog(log, tblk, lrd, tlck, cd);
			break;

		case tlckMAP:
			mapLog(log, tblk, lrd, tlck);
			break;

		case tlckDATA:
			dataLog(log, tblk, lrd, tlck);
			break;

		default:
			jfs_err("UFO tlock:0x%p", tlck);
		}
	}

	return rc;
}

static int diLog(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
		 struct tlock * tlck, struct commit * cd)
{
	int rc = 0;
	struct metapage *mp;
	pxd_t *pxd;
	struct pxd_lock *pxdlock;

	mp = tlck->mp;

	
	lrd->log.redopage.type = cpu_to_le16(LOG_INODE);
	lrd->log.redopage.l2linesize = cpu_to_le16(L2INODESLOTSIZE);

	pxd = &lrd->log.redopage.pxd;

	if (tlck->type & tlckENTRY) {
		
		lrd->type = cpu_to_le16(LOG_REDOPAGE);
		PXDaddress(pxd, mp->index);
		PXDlength(pxd,
			  mp->logical_size >> tblk->sb->s_blocksize_bits);
		lrd->backchain = cpu_to_le32(lmLog(log, tblk, lrd, tlck));

		
		tlck->flag |= tlckWRITEPAGE;
	} else if (tlck->type & tlckFREE) {

		lrd->type = cpu_to_le16(LOG_NOREDOINOEXT);
		lrd->log.noredoinoext.iagnum =
		    cpu_to_le32((u32) (size_t) cd->iplist[1]);
		lrd->log.noredoinoext.inoext_idx =
		    cpu_to_le32((u32) (size_t) cd->iplist[2]);

		pxdlock = (struct pxd_lock *) & tlck->lock;
		*pxd = pxdlock->pxd;
		lrd->backchain = cpu_to_le32(lmLog(log, tblk, lrd, NULL));

		
		tlck->flag |= tlckUPDATEMAP;

		
		tlck->flag |= tlckWRITEPAGE;
	} else
		jfs_err("diLog: UFO type tlck:0x%p", tlck);
#ifdef  _JFS_WIP
	else {
		assert(tlck->type & tlckEA);

		lrd->type = cpu_to_le16(LOG_UPDATEMAP);
		pxdlock = (struct pxd_lock *) & tlck->lock;
		nlock = pxdlock->index;
		for (i = 0; i < nlock; i++, pxdlock++) {
			if (pxdlock->flag & mlckALLOCPXD)
				lrd->log.updatemap.type =
				    cpu_to_le16(LOG_ALLOCPXD);
			else
				lrd->log.updatemap.type =
				    cpu_to_le16(LOG_FREEPXD);
			lrd->log.updatemap.nxd = cpu_to_le16(1);
			lrd->log.updatemap.pxd = pxdlock->pxd;
			lrd->backchain =
			    cpu_to_le32(lmLog(log, tblk, lrd, NULL));
		}

		
		tlck->flag |= tlckUPDATEMAP;
	}
#endif				

	return rc;
}

static int dataLog(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
	    struct tlock * tlck)
{
	struct metapage *mp;
	pxd_t *pxd;

	mp = tlck->mp;

	
	lrd->log.redopage.type = cpu_to_le16(LOG_DATA);
	lrd->log.redopage.l2linesize = cpu_to_le16(L2DATASLOTSIZE);

	pxd = &lrd->log.redopage.pxd;

	
	lrd->type = cpu_to_le16(LOG_REDOPAGE);

	if (jfs_dirtable_inline(tlck->ip)) {
		mp->lid = 0;
		grab_metapage(mp);
		metapage_homeok(mp);
		discard_metapage(mp);
		tlck->mp = NULL;
		return 0;
	}

	PXDaddress(pxd, mp->index);
	PXDlength(pxd, mp->logical_size >> tblk->sb->s_blocksize_bits);

	lrd->backchain = cpu_to_le32(lmLog(log, tblk, lrd, tlck));

	
	tlck->flag |= tlckWRITEPAGE;

	return 0;
}

static void dtLog(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
	   struct tlock * tlck)
{
	struct metapage *mp;
	struct pxd_lock *pxdlock;
	pxd_t *pxd;

	mp = tlck->mp;

	
	lrd->log.redopage.type = cpu_to_le16(LOG_DTREE);
	lrd->log.redopage.l2linesize = cpu_to_le16(L2DTSLOTSIZE);

	pxd = &lrd->log.redopage.pxd;

	if (tlck->type & tlckBTROOT)
		lrd->log.redopage.type |= cpu_to_le16(LOG_BTROOT);

	if (tlck->type & (tlckNEW | tlckEXTEND)) {
		lrd->type = cpu_to_le16(LOG_REDOPAGE);
		if (tlck->type & tlckEXTEND)
			lrd->log.redopage.type |= cpu_to_le16(LOG_EXTEND);
		else
			lrd->log.redopage.type |= cpu_to_le16(LOG_NEW);
		PXDaddress(pxd, mp->index);
		PXDlength(pxd,
			  mp->logical_size >> tblk->sb->s_blocksize_bits);
		lrd->backchain = cpu_to_le32(lmLog(log, tblk, lrd, tlck));

		if (tlck->type & tlckBTROOT)
			return;
		tlck->flag |= tlckUPDATEMAP;
		pxdlock = (struct pxd_lock *) & tlck->lock;
		pxdlock->flag = mlckALLOCPXD;
		pxdlock->pxd = *pxd;

		pxdlock->index = 1;

		
		tlck->flag |= tlckWRITEPAGE;
		return;
	}

	if (tlck->type & (tlckENTRY | tlckRELINK)) {
		
		lrd->type = cpu_to_le16(LOG_REDOPAGE);
		PXDaddress(pxd, mp->index);
		PXDlength(pxd,
			  mp->logical_size >> tblk->sb->s_blocksize_bits);
		lrd->backchain = cpu_to_le32(lmLog(log, tblk, lrd, tlck));

		
		tlck->flag |= tlckWRITEPAGE;
		return;
	}

	if (tlck->type & (tlckFREE | tlckRELOCATE)) {
		lrd->type = cpu_to_le16(LOG_NOREDOPAGE);
		pxdlock = (struct pxd_lock *) & tlck->lock;
		*pxd = pxdlock->pxd;
		lrd->backchain = cpu_to_le32(lmLog(log, tblk, lrd, NULL));

		tlck->flag |= tlckUPDATEMAP;
	}
	return;
}

static void xtLog(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
	   struct tlock * tlck)
{
	struct inode *ip;
	struct metapage *mp;
	xtpage_t *p;
	struct xtlock *xtlck;
	struct maplock *maplock;
	struct xdlistlock *xadlock;
	struct pxd_lock *pxdlock;
	pxd_t *page_pxd;
	int next, lwm, hwm;

	ip = tlck->ip;
	mp = tlck->mp;

	
	lrd->log.redopage.type = cpu_to_le16(LOG_XTREE);
	lrd->log.redopage.l2linesize = cpu_to_le16(L2XTSLOTSIZE);

	page_pxd = &lrd->log.redopage.pxd;

	if (tlck->type & tlckBTROOT) {
		lrd->log.redopage.type |= cpu_to_le16(LOG_BTROOT);
		p = &JFS_IP(ip)->i_xtroot;
		if (S_ISDIR(ip->i_mode))
			lrd->log.redopage.type |=
			    cpu_to_le16(LOG_DIR_XTREE);
	} else
		p = (xtpage_t *) mp->data;
	next = le16_to_cpu(p->header.nextindex);

	xtlck = (struct xtlock *) & tlck->lock;

	maplock = (struct maplock *) & tlck->lock;
	xadlock = (struct xdlistlock *) maplock;

	if (tlck->type & (tlckNEW | tlckGROW | tlckRELINK)) {
		lrd->type = cpu_to_le16(LOG_REDOPAGE);
		PXDaddress(page_pxd, mp->index);
		PXDlength(page_pxd,
			  mp->logical_size >> tblk->sb->s_blocksize_bits);
		lrd->backchain = cpu_to_le32(lmLog(log, tblk, lrd, tlck));

		lwm = xtlck->lwm.offset;
		if (lwm == 0)
			lwm = XTPAGEMAXSLOT;

		if (lwm == next)
			goto out;
		if (lwm > next) {
			jfs_err("xtLog: lwm > next\n");
			goto out;
		}
		tlck->flag |= tlckUPDATEMAP;
		xadlock->flag = mlckALLOCXADLIST;
		xadlock->count = next - lwm;
		if ((xadlock->count <= 4) && (tblk->xflag & COMMIT_LAZY)) {
			int i;
			pxd_t *pxd;
			xadlock->flag = mlckALLOCPXDLIST;
			pxd = xadlock->xdlist = &xtlck->pxdlock;
			for (i = 0; i < xadlock->count; i++) {
				PXDaddress(pxd, addressXAD(&p->xad[lwm + i]));
				PXDlength(pxd, lengthXAD(&p->xad[lwm + i]));
				p->xad[lwm + i].flag &=
				    ~(XAD_NEW | XAD_EXTENDED);
				pxd++;
			}
		} else {
			xadlock->flag = mlckALLOCXADLIST;
			xadlock->xdlist = &p->xad[lwm];
			tblk->xflag &= ~COMMIT_LAZY;
		}
		jfs_info("xtLog: alloc ip:0x%p mp:0x%p tlck:0x%p lwm:%d "
			 "count:%d", tlck->ip, mp, tlck, lwm, xadlock->count);

		maplock->index = 1;

	      out:
		
		tlck->flag |= tlckWRITEPAGE;

		return;
	}

	/*
	 *	page deletion: file deletion/truncation (ref. xtTruncate())
	 *
	 * (page will be invalidated after log is written and bmap
	 * is updated from the page);
	 */
	if (tlck->type & tlckFREE) {
		if (tblk->xflag & COMMIT_TRUNCATE) {
			
			lrd->type = cpu_to_le16(LOG_NOREDOPAGE);
			PXDaddress(page_pxd, mp->index);
			PXDlength(page_pxd,
				  mp->logical_size >> tblk->sb->
				  s_blocksize_bits);
			lrd->backchain =
			    cpu_to_le32(lmLog(log, tblk, lrd, NULL));

			if (tlck->type & tlckBTROOT) {
				
				lrd->type = cpu_to_le16(LOG_REDOPAGE);
				lrd->backchain =
				    cpu_to_le32(lmLog(log, tblk, lrd, tlck));
			}
		}

		lrd->type = cpu_to_le16(LOG_UPDATEMAP);
		lrd->log.updatemap.type = cpu_to_le16(LOG_FREEXADLIST);
		xtlck = (struct xtlock *) & tlck->lock;
		hwm = xtlck->hwm.offset;
		lrd->log.updatemap.nxd =
		    cpu_to_le16(hwm - XTENTRYSTART + 1);
		
		xtlck->header.offset = XTENTRYSTART;
		xtlck->header.length = hwm - XTENTRYSTART + 1;
		xtlck->index = 1;
		lrd->backchain = cpu_to_le32(lmLog(log, tblk, lrd, tlck));

		tlck->flag |= tlckUPDATEMAP;
		xadlock->count = hwm - XTENTRYSTART + 1;
		if ((xadlock->count <= 4) && (tblk->xflag & COMMIT_LAZY)) {
			int i;
			pxd_t *pxd;
			xadlock->flag = mlckFREEPXDLIST;
			pxd = xadlock->xdlist = &xtlck->pxdlock;
			for (i = 0; i < xadlock->count; i++) {
				PXDaddress(pxd,
					addressXAD(&p->xad[XTENTRYSTART + i]));
				PXDlength(pxd,
					lengthXAD(&p->xad[XTENTRYSTART + i]));
				pxd++;
			}
		} else {
			xadlock->flag = mlckFREEXADLIST;
			xadlock->xdlist = &p->xad[XTENTRYSTART];
			tblk->xflag &= ~COMMIT_LAZY;
		}
		jfs_info("xtLog: free ip:0x%p mp:0x%p count:%d lwm:2",
			 tlck->ip, mp, xadlock->count);

		maplock->index = 1;

		
		if (((tblk->xflag & COMMIT_PWMAP) || S_ISDIR(ip->i_mode))
		    && !(tlck->type & tlckBTROOT))
			tlck->flag |= tlckFREEPAGE;
		return;
	}

	if (tlck->type & tlckTRUNCATE) {
		
		pxd_t pxd = pxd;	
		int twm;

		tblk->xflag &= ~COMMIT_LAZY;
		lwm = xtlck->lwm.offset;
		if (lwm == 0)
			lwm = XTPAGEMAXSLOT;
		hwm = xtlck->hwm.offset;
		twm = xtlck->twm.offset;

		lrd->type = cpu_to_le16(LOG_REDOPAGE);
		PXDaddress(page_pxd, mp->index);
		PXDlength(page_pxd,
			  mp->logical_size >> tblk->sb->s_blocksize_bits);
		lrd->backchain = cpu_to_le32(lmLog(log, tblk, lrd, tlck));

		if (twm == next - 1) {
			pxdlock = (struct pxd_lock *) & xtlck->pxdlock;
			
			lrd->type = cpu_to_le16(LOG_UPDATEMAP);
			lrd->log.updatemap.type = cpu_to_le16(LOG_FREEPXD);
			lrd->log.updatemap.nxd = cpu_to_le16(1);
			lrd->log.updatemap.pxd = pxdlock->pxd;
			pxd = pxdlock->pxd;	
			lrd->backchain =
			    cpu_to_le32(lmLog(log, tblk, lrd, NULL));
		}

		if (hwm >= next) {
			lrd->type = cpu_to_le16(LOG_UPDATEMAP);
			lrd->log.updatemap.type =
			    cpu_to_le16(LOG_FREEXADLIST);
			xtlck = (struct xtlock *) & tlck->lock;
			hwm = xtlck->hwm.offset;
			lrd->log.updatemap.nxd =
			    cpu_to_le16(hwm - next + 1);
			
			xtlck->header.offset = next;
			xtlck->header.length = hwm - next + 1;
			xtlck->index = 1;
			lrd->backchain =
			    cpu_to_le32(lmLog(log, tblk, lrd, tlck));
		}

		maplock->index = 0;

		if (lwm < next) {
			tlck->flag |= tlckUPDATEMAP;
			xadlock->flag = mlckALLOCXADLIST;
			xadlock->count = next - lwm;
			xadlock->xdlist = &p->xad[lwm];

			jfs_info("xtLog: alloc ip:0x%p mp:0x%p count:%d "
				 "lwm:%d next:%d",
				 tlck->ip, mp, xadlock->count, lwm, next);
			maplock->index++;
			xadlock++;
		}

		if (twm == next - 1) {
			tlck->flag |= tlckUPDATEMAP;
			pxdlock = (struct pxd_lock *) xadlock;
			pxdlock->flag = mlckFREEPXD;
			pxdlock->count = 1;
			pxdlock->pxd = pxd;

			jfs_info("xtLog: truncate ip:0x%p mp:0x%p count:%d "
				 "hwm:%d", ip, mp, pxdlock->count, hwm);
			maplock->index++;
			xadlock++;
		}

		if (hwm >= next) {
			tlck->flag |= tlckUPDATEMAP;
			xadlock->flag = mlckFREEXADLIST;
			xadlock->count = hwm - next + 1;
			xadlock->xdlist = &p->xad[next];

			jfs_info("xtLog: free ip:0x%p mp:0x%p count:%d "
				 "next:%d hwm:%d",
				 tlck->ip, mp, xadlock->count, next, hwm);
			maplock->index++;
		}

		
		tlck->flag |= tlckWRITEPAGE;
	}
	return;
}

static void mapLog(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
		   struct tlock * tlck)
{
	struct pxd_lock *pxdlock;
	int i, nlock;
	pxd_t *pxd;

	if (tlck->type & tlckRELOCATE) {
		lrd->type = cpu_to_le16(LOG_NOREDOPAGE);
		pxdlock = (struct pxd_lock *) & tlck->lock;
		pxd = &lrd->log.redopage.pxd;
		*pxd = pxdlock->pxd;
		lrd->backchain = cpu_to_le32(lmLog(log, tblk, lrd, NULL));

		lrd->type = cpu_to_le16(LOG_UPDATEMAP);
		lrd->log.updatemap.type = cpu_to_le16(LOG_FREEPXD);
		lrd->log.updatemap.nxd = cpu_to_le16(1);
		lrd->log.updatemap.pxd = pxdlock->pxd;
		lrd->backchain = cpu_to_le32(lmLog(log, tblk, lrd, NULL));

		tlck->flag |= tlckUPDATEMAP;
		return;
	}
	else {
		lrd->type = cpu_to_le16(LOG_UPDATEMAP);
		pxdlock = (struct pxd_lock *) & tlck->lock;
		nlock = pxdlock->index;
		for (i = 0; i < nlock; i++, pxdlock++) {
			if (pxdlock->flag & mlckALLOCPXD)
				lrd->log.updatemap.type =
				    cpu_to_le16(LOG_ALLOCPXD);
			else
				lrd->log.updatemap.type =
				    cpu_to_le16(LOG_FREEPXD);
			lrd->log.updatemap.nxd = cpu_to_le16(1);
			lrd->log.updatemap.pxd = pxdlock->pxd;
			lrd->backchain =
			    cpu_to_le32(lmLog(log, tblk, lrd, NULL));
			jfs_info("mapLog: xaddr:0x%lx xlen:0x%x",
				 (ulong) addressPXD(&pxdlock->pxd),
				 lengthPXD(&pxdlock->pxd));
		}

		
		tlck->flag |= tlckUPDATEMAP;
	}
}

void txEA(tid_t tid, struct inode *ip, dxd_t * oldea, dxd_t * newea)
{
	struct tlock *tlck = NULL;
	struct pxd_lock *maplock = NULL, *pxdlock = NULL;

	if (newea) {
		if (newea->flag & DXD_EXTENT) {
			tlck = txMaplock(tid, ip, tlckMAP);
			maplock = (struct pxd_lock *) & tlck->lock;
			pxdlock = (struct pxd_lock *) maplock;
			pxdlock->flag = mlckALLOCPXD;
			PXDaddress(&pxdlock->pxd, addressDXD(newea));
			PXDlength(&pxdlock->pxd, lengthDXD(newea));
			pxdlock++;
			maplock->index = 1;
		} else if (newea->flag & DXD_INLINE) {
			tlck = NULL;

			set_cflag(COMMIT_Inlineea, ip);
		}
	}

	if (!test_cflag(COMMIT_Nolink, ip) && oldea->flag & DXD_EXTENT) {
		if (tlck == NULL) {
			tlck = txMaplock(tid, ip, tlckMAP);
			maplock = (struct pxd_lock *) & tlck->lock;
			pxdlock = (struct pxd_lock *) maplock;
			maplock->index = 0;
		}
		pxdlock->flag = mlckFREEPXD;
		PXDaddress(&pxdlock->pxd, addressDXD(oldea));
		PXDlength(&pxdlock->pxd, lengthDXD(oldea));
		maplock->index++;
	}
}

static void txForce(struct tblock * tblk)
{
	struct tlock *tlck;
	lid_t lid, next;
	struct metapage *mp;

	tlck = lid_to_tlock(tblk->next);
	lid = tlck->next;
	tlck->next = 0;
	while (lid) {
		tlck = lid_to_tlock(lid);
		next = tlck->next;
		tlck->next = tblk->next;
		tblk->next = lid;
		lid = next;
	}

	for (lid = tblk->next; lid; lid = next) {
		tlck = lid_to_tlock(lid);
		next = tlck->next;

		if ((mp = tlck->mp) != NULL &&
		    (tlck->type & tlckBTROOT) == 0) {
			assert(mp->xflag & COMMIT_PAGE);

			if (tlck->flag & tlckWRITEPAGE) {
				tlck->flag &= ~tlckWRITEPAGE;

				
				force_metapage(mp);
#if 0
				assert(mp->nohomeok);
				set_bit(META_dirty, &mp->flag);
				set_bit(META_sync, &mp->flag);
#endif
			}
		}
	}
}

static void txUpdateMap(struct tblock * tblk)
{
	struct inode *ip;
	struct inode *ipimap;
	lid_t lid;
	struct tlock *tlck;
	struct maplock *maplock;
	struct pxd_lock pxdlock;
	int maptype;
	int k, nlock;
	struct metapage *mp = NULL;

	ipimap = JFS_SBI(tblk->sb)->ipimap;

	maptype = (tblk->xflag & COMMIT_PMAP) ? COMMIT_PMAP : COMMIT_PWMAP;


	for (lid = tblk->next; lid; lid = tlck->next) {
		tlck = lid_to_tlock(lid);

		if ((tlck->flag & tlckUPDATEMAP) == 0)
			continue;

		if (tlck->flag & tlckFREEPAGE) {
			mp = tlck->mp;
			ASSERT(mp->xflag & COMMIT_PAGE);
			grab_metapage(mp);
		}

		maplock = (struct maplock *) & tlck->lock;
		nlock = maplock->index;

		for (k = 0; k < nlock; k++, maplock++) {
			if (maplock->flag & mlckALLOC) {
				txAllocPMap(ipimap, maplock, tblk);
			}
			else {	

				if (tlck->flag & tlckDIRECTORY)
					txFreeMap(ipimap, maplock,
						  tblk, COMMIT_PWMAP);
				else
					txFreeMap(ipimap, maplock,
						  tblk, maptype);
			}
		}
		if (tlck->flag & tlckFREEPAGE) {
			if (!(tblk->flag & tblkGC_LAZY)) {
				
				ASSERT(mp->lid == lid);
				tlck->mp->lid = 0;
			}
			assert(mp->nohomeok == 1);
			metapage_homeok(mp);
			discard_metapage(mp);
			tlck->mp = NULL;
		}
	}
	if (tblk->xflag & COMMIT_CREATE) {
		diUpdatePMap(ipimap, tblk->ino, false, tblk);
		pxdlock.flag = mlckALLOCPXD;
		pxdlock.pxd = tblk->u.ixpxd;
		pxdlock.index = 1;
		txAllocPMap(ipimap, (struct maplock *) & pxdlock, tblk);
	} else if (tblk->xflag & COMMIT_DELETE) {
		ip = tblk->u.ip;
		diUpdatePMap(ipimap, ip->i_ino, true, tblk);
		iput(ip);
	}
}

static void txAllocPMap(struct inode *ip, struct maplock * maplock,
			struct tblock * tblk)
{
	struct inode *ipbmap = JFS_SBI(ip->i_sb)->ipbmap;
	struct xdlistlock *xadlistlock;
	xad_t *xad;
	s64 xaddr;
	int xlen;
	struct pxd_lock *pxdlock;
	struct xdlistlock *pxdlistlock;
	pxd_t *pxd;
	int n;

	if (maplock->flag & mlckALLOCXADLIST) {
		xadlistlock = (struct xdlistlock *) maplock;
		xad = xadlistlock->xdlist;
		for (n = 0; n < xadlistlock->count; n++, xad++) {
			if (xad->flag & (XAD_NEW | XAD_EXTENDED)) {
				xaddr = addressXAD(xad);
				xlen = lengthXAD(xad);
				dbUpdatePMap(ipbmap, false, xaddr,
					     (s64) xlen, tblk);
				xad->flag &= ~(XAD_NEW | XAD_EXTENDED);
				jfs_info("allocPMap: xaddr:0x%lx xlen:%d",
					 (ulong) xaddr, xlen);
			}
		}
	} else if (maplock->flag & mlckALLOCPXD) {
		pxdlock = (struct pxd_lock *) maplock;
		xaddr = addressPXD(&pxdlock->pxd);
		xlen = lengthPXD(&pxdlock->pxd);
		dbUpdatePMap(ipbmap, false, xaddr, (s64) xlen, tblk);
		jfs_info("allocPMap: xaddr:0x%lx xlen:%d", (ulong) xaddr, xlen);
	} else {		

		pxdlistlock = (struct xdlistlock *) maplock;
		pxd = pxdlistlock->xdlist;
		for (n = 0; n < pxdlistlock->count; n++, pxd++) {
			xaddr = addressPXD(pxd);
			xlen = lengthPXD(pxd);
			dbUpdatePMap(ipbmap, false, xaddr, (s64) xlen,
				     tblk);
			jfs_info("allocPMap: xaddr:0x%lx xlen:%d",
				 (ulong) xaddr, xlen);
		}
	}
}

void txFreeMap(struct inode *ip,
	       struct maplock * maplock, struct tblock * tblk, int maptype)
{
	struct inode *ipbmap = JFS_SBI(ip->i_sb)->ipbmap;
	struct xdlistlock *xadlistlock;
	xad_t *xad;
	s64 xaddr;
	int xlen;
	struct pxd_lock *pxdlock;
	struct xdlistlock *pxdlistlock;
	pxd_t *pxd;
	int n;

	jfs_info("txFreeMap: tblk:0x%p maplock:0x%p maptype:0x%x",
		 tblk, maplock, maptype);

	if (maptype == COMMIT_PMAP || maptype == COMMIT_PWMAP) {
		if (maplock->flag & mlckFREEXADLIST) {
			xadlistlock = (struct xdlistlock *) maplock;
			xad = xadlistlock->xdlist;
			for (n = 0; n < xadlistlock->count; n++, xad++) {
				if (!(xad->flag & XAD_NEW)) {
					xaddr = addressXAD(xad);
					xlen = lengthXAD(xad);
					dbUpdatePMap(ipbmap, true, xaddr,
						     (s64) xlen, tblk);
					jfs_info("freePMap: xaddr:0x%lx "
						 "xlen:%d",
						 (ulong) xaddr, xlen);
				}
			}
		} else if (maplock->flag & mlckFREEPXD) {
			pxdlock = (struct pxd_lock *) maplock;
			xaddr = addressPXD(&pxdlock->pxd);
			xlen = lengthPXD(&pxdlock->pxd);
			dbUpdatePMap(ipbmap, true, xaddr, (s64) xlen,
				     tblk);
			jfs_info("freePMap: xaddr:0x%lx xlen:%d",
				 (ulong) xaddr, xlen);
		} else {	

			pxdlistlock = (struct xdlistlock *) maplock;
			pxd = pxdlistlock->xdlist;
			for (n = 0; n < pxdlistlock->count; n++, pxd++) {
				xaddr = addressPXD(pxd);
				xlen = lengthPXD(pxd);
				dbUpdatePMap(ipbmap, true, xaddr,
					     (s64) xlen, tblk);
				jfs_info("freePMap: xaddr:0x%lx xlen:%d",
					 (ulong) xaddr, xlen);
			}
		}
	}

	if (maptype == COMMIT_PWMAP || maptype == COMMIT_WMAP) {
		if (maplock->flag & mlckFREEXADLIST) {
			xadlistlock = (struct xdlistlock *) maplock;
			xad = xadlistlock->xdlist;
			for (n = 0; n < xadlistlock->count; n++, xad++) {
				xaddr = addressXAD(xad);
				xlen = lengthXAD(xad);
				dbFree(ip, xaddr, (s64) xlen);
				xad->flag = 0;
				jfs_info("freeWMap: xaddr:0x%lx xlen:%d",
					 (ulong) xaddr, xlen);
			}
		} else if (maplock->flag & mlckFREEPXD) {
			pxdlock = (struct pxd_lock *) maplock;
			xaddr = addressPXD(&pxdlock->pxd);
			xlen = lengthPXD(&pxdlock->pxd);
			dbFree(ip, xaddr, (s64) xlen);
			jfs_info("freeWMap: xaddr:0x%lx xlen:%d",
				 (ulong) xaddr, xlen);
		} else {	

			pxdlistlock = (struct xdlistlock *) maplock;
			pxd = pxdlistlock->xdlist;
			for (n = 0; n < pxdlistlock->count; n++, pxd++) {
				xaddr = addressPXD(pxd);
				xlen = lengthPXD(pxd);
				dbFree(ip, xaddr, (s64) xlen);
				jfs_info("freeWMap: xaddr:0x%lx xlen:%d",
					 (ulong) xaddr, xlen);
			}
		}
	}
}

void txFreelock(struct inode *ip)
{
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	struct tlock *xtlck, *tlck;
	lid_t xlid = 0, lid;

	if (!jfs_ip->atlhead)
		return;

	TXN_LOCK();
	xtlck = (struct tlock *) &jfs_ip->atlhead;

	while ((lid = xtlck->next) != 0) {
		tlck = lid_to_tlock(lid);
		if (tlck->flag & tlckFREELOCK) {
			xtlck->next = tlck->next;
			txLockFree(lid);
		} else {
			xtlck = tlck;
			xlid = lid;
		}
	}

	if (jfs_ip->atlhead)
		jfs_ip->atltail = xlid;
	else {
		jfs_ip->atltail = 0;
		list_del_init(&jfs_ip->anon_inode_list);
	}
	TXN_UNLOCK();
}

void txAbort(tid_t tid, int dirty)
{
	lid_t lid, next;
	struct metapage *mp;
	struct tblock *tblk = tid_to_tblock(tid);
	struct tlock *tlck;

	for (lid = tblk->next; lid; lid = next) {
		tlck = lid_to_tlock(lid);
		next = tlck->next;
		mp = tlck->mp;
		JFS_IP(tlck->ip)->xtlid = 0;

		if (mp) {
			mp->lid = 0;

			if (mp->xflag & COMMIT_PAGE && mp->lsn)
				LogSyncRelease(mp);
		}
		
		TXN_LOCK();
		txLockFree(lid);
		TXN_UNLOCK();
	}

	

	tblk->next = tblk->last = 0;

	if (dirty)
		jfs_error(tblk->sb, "txAbort");

	return;
}

static void txLazyCommit(struct tblock * tblk)
{
	struct jfs_log *log;

	while (((tblk->flag & tblkGC_READY) == 0) &&
	       ((tblk->flag & tblkGC_UNLOCKED) == 0)) {
		jfs_info("jfs_lazycommit: tblk 0x%p not unlocked", tblk);
		yield();
	}

	jfs_info("txLazyCommit: processing tblk 0x%p", tblk);

	txUpdateMap(tblk);

	log = (struct jfs_log *) JFS_SBI(tblk->sb)->log;

	spin_lock_irq(&log->gclock);	

	tblk->flag |= tblkGC_COMMITTED;

	if (tblk->flag & tblkGC_READY)
		log->gcrtc--;

	wake_up_all(&tblk->gcwait);	

	if (tblk->flag & tblkGC_LAZY) {
		spin_unlock_irq(&log->gclock);	
		txUnlock(tblk);
		tblk->flag &= ~tblkGC_LAZY;
		txEnd(tblk - TxBlock);	
	} else
		spin_unlock_irq(&log->gclock);	

	jfs_info("txLazyCommit: done: tblk = 0x%p", tblk);
}

int jfs_lazycommit(void *arg)
{
	int WorkDone;
	struct tblock *tblk;
	unsigned long flags;
	struct jfs_sb_info *sbi;

	do {
		LAZY_LOCK(flags);
		jfs_commit_thread_waking = 0;	
		while (!list_empty(&TxAnchor.unlock_queue)) {
			WorkDone = 0;
			list_for_each_entry(tblk, &TxAnchor.unlock_queue,
					    cqueue) {

				sbi = JFS_SBI(tblk->sb);
				if (sbi->commit_state & IN_LAZYCOMMIT)
					continue;

				sbi->commit_state |= IN_LAZYCOMMIT;
				WorkDone = 1;

				list_del(&tblk->cqueue);

				LAZY_UNLOCK(flags);
				txLazyCommit(tblk);
				LAZY_LOCK(flags);

				sbi->commit_state &= ~IN_LAZYCOMMIT;
				break;
			}

			
			if (!WorkDone)
				break;
		}
		
		jfs_commit_thread_waking = 0;

		if (freezing(current)) {
			LAZY_UNLOCK(flags);
			try_to_freeze();
		} else {
			DECLARE_WAITQUEUE(wq, current);

			add_wait_queue(&jfs_commit_thread_wait, &wq);
			set_current_state(TASK_INTERRUPTIBLE);
			LAZY_UNLOCK(flags);
			schedule();
			__set_current_state(TASK_RUNNING);
			remove_wait_queue(&jfs_commit_thread_wait, &wq);
		}
	} while (!kthread_should_stop());

	if (!list_empty(&TxAnchor.unlock_queue))
		jfs_err("jfs_lazycommit being killed w/pending transactions!");
	else
		jfs_info("jfs_lazycommit being killed\n");
	return 0;
}

void txLazyUnlock(struct tblock * tblk)
{
	unsigned long flags;

	LAZY_LOCK(flags);

	list_add_tail(&tblk->cqueue, &TxAnchor.unlock_queue);
	if (!(JFS_SBI(tblk->sb)->commit_state & IN_LAZYCOMMIT) &&
	    !jfs_commit_thread_waking) {
		jfs_commit_thread_waking = 1;
		wake_up(&jfs_commit_thread_wait);
	}
	LAZY_UNLOCK(flags);
}

static void LogSyncRelease(struct metapage * mp)
{
	struct jfs_log *log = mp->log;

	assert(mp->nohomeok);
	assert(log);
	metapage_homeok(mp);
}

void txQuiesce(struct super_block *sb)
{
	struct inode *ip;
	struct jfs_inode_info *jfs_ip;
	struct jfs_log *log = JFS_SBI(sb)->log;
	tid_t tid;

	set_bit(log_QUIESCE, &log->flag);

	TXN_LOCK();
restart:
	while (!list_empty(&TxAnchor.anon_list)) {
		jfs_ip = list_entry(TxAnchor.anon_list.next,
				    struct jfs_inode_info,
				    anon_inode_list);
		ip = &jfs_ip->vfs_inode;

		TXN_UNLOCK();
		tid = txBegin(ip->i_sb, COMMIT_INODE | COMMIT_FORCE);
		mutex_lock(&jfs_ip->commit_mutex);
		txCommit(tid, 1, &ip, 0);
		txEnd(tid);
		mutex_unlock(&jfs_ip->commit_mutex);
		cond_resched();
		TXN_LOCK();
	}

	if (!list_empty(&TxAnchor.anon_list2)) {
		list_splice(&TxAnchor.anon_list2, &TxAnchor.anon_list);
		INIT_LIST_HEAD(&TxAnchor.anon_list2);
		goto restart;
	}
	TXN_UNLOCK();

	jfs_flush_journal(log, 0);
}

void txResume(struct super_block *sb)
{
	struct jfs_log *log = JFS_SBI(sb)->log;

	clear_bit(log_QUIESCE, &log->flag);
	TXN_WAKEUP(&log->syncwait);
}

int jfs_sync(void *arg)
{
	struct inode *ip;
	struct jfs_inode_info *jfs_ip;
	tid_t tid;

	do {
		TXN_LOCK();
		while (jfs_tlocks_low && !list_empty(&TxAnchor.anon_list)) {
			jfs_ip = list_entry(TxAnchor.anon_list.next,
					    struct jfs_inode_info,
					    anon_inode_list);
			ip = &jfs_ip->vfs_inode;

			if (! igrab(ip)) {
				list_del_init(&jfs_ip->anon_inode_list);
			} else if (mutex_trylock(&jfs_ip->commit_mutex)) {
				TXN_UNLOCK();
				tid = txBegin(ip->i_sb, COMMIT_INODE);
				txCommit(tid, 1, &ip, 0);
				txEnd(tid);
				mutex_unlock(&jfs_ip->commit_mutex);

				iput(ip);
				cond_resched();
				TXN_LOCK();
			} else {

				
				list_del(&jfs_ip->anon_inode_list);

				
				list_add(&jfs_ip->anon_inode_list,
					 &TxAnchor.anon_list2);

				TXN_UNLOCK();
				iput(ip);
				TXN_LOCK();
			}
		}
		
		list_splice_init(&TxAnchor.anon_list2, &TxAnchor.anon_list);

		if (freezing(current)) {
			TXN_UNLOCK();
			try_to_freeze();
		} else {
			set_current_state(TASK_INTERRUPTIBLE);
			TXN_UNLOCK();
			schedule();
			__set_current_state(TASK_RUNNING);
		}
	} while (!kthread_should_stop());

	jfs_info("jfs_sync being killed");
	return 0;
}

#if defined(CONFIG_PROC_FS) && defined(CONFIG_JFS_DEBUG)
static int jfs_txanchor_proc_show(struct seq_file *m, void *v)
{
	char *freewait;
	char *freelockwait;
	char *lowlockwait;

	freewait =
	    waitqueue_active(&TxAnchor.freewait) ? "active" : "empty";
	freelockwait =
	    waitqueue_active(&TxAnchor.freelockwait) ? "active" : "empty";
	lowlockwait =
	    waitqueue_active(&TxAnchor.lowlockwait) ? "active" : "empty";

	seq_printf(m,
		       "JFS TxAnchor\n"
		       "============\n"
		       "freetid = %d\n"
		       "freewait = %s\n"
		       "freelock = %d\n"
		       "freelockwait = %s\n"
		       "lowlockwait = %s\n"
		       "tlocksInUse = %d\n"
		       "jfs_tlocks_low = %d\n"
		       "unlock_queue is %sempty\n",
		       TxAnchor.freetid,
		       freewait,
		       TxAnchor.freelock,
		       freelockwait,
		       lowlockwait,
		       TxAnchor.tlocksInUse,
		       jfs_tlocks_low,
		       list_empty(&TxAnchor.unlock_queue) ? "" : "not ");
	return 0;
}

static int jfs_txanchor_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, jfs_txanchor_proc_show, NULL);
}

const struct file_operations jfs_txanchor_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= jfs_txanchor_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

#if defined(CONFIG_PROC_FS) && defined(CONFIG_JFS_STATISTICS)
static int jfs_txstats_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m,
		       "JFS TxStats\n"
		       "===========\n"
		       "calls to txBegin = %d\n"
		       "txBegin blocked by sync barrier = %d\n"
		       "txBegin blocked by tlocks low = %d\n"
		       "txBegin blocked by no free tid = %d\n"
		       "calls to txBeginAnon = %d\n"
		       "txBeginAnon blocked by sync barrier = %d\n"
		       "txBeginAnon blocked by tlocks low = %d\n"
		       "calls to txLockAlloc = %d\n"
		       "tLockAlloc blocked by no free lock = %d\n",
		       TxStat.txBegin,
		       TxStat.txBegin_barrier,
		       TxStat.txBegin_lockslow,
		       TxStat.txBegin_freetid,
		       TxStat.txBeginAnon,
		       TxStat.txBeginAnon_barrier,
		       TxStat.txBeginAnon_lockslow,
		       TxStat.txLockAlloc,
		       TxStat.txLockAlloc_freelock);
	return 0;
}

static int jfs_txstats_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, jfs_txstats_proc_show, NULL);
}

const struct file_operations jfs_txstats_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= jfs_txstats_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif
