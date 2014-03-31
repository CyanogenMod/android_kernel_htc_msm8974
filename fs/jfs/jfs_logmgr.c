/*
 *   Copyright (C) International Business Machines Corp., 2000-2004
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
#include <linux/blkdev.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/buffer_head.h>		
#include <linux/bio.h>
#include <linux/freezer.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include "jfs_incore.h"
#include "jfs_filsys.h"
#include "jfs_metapage.h"
#include "jfs_superblock.h"
#include "jfs_txnmgr.h"
#include "jfs_debug.h"


static struct lbuf *log_redrive_list;
static DEFINE_SPINLOCK(log_redrive_lock);


#define LOG_LOCK_INIT(log)	mutex_init(&(log)->loglock)
#define LOG_LOCK(log)		mutex_lock(&((log)->loglock))
#define LOG_UNLOCK(log)		mutex_unlock(&((log)->loglock))



#define LOGGC_LOCK_INIT(log)	spin_lock_init(&(log)->gclock)
#define LOGGC_LOCK(log)		spin_lock_irq(&(log)->gclock)
#define LOGGC_UNLOCK(log)	spin_unlock_irq(&(log)->gclock)
#define LOGGC_WAKEUP(tblk)	wake_up_all(&(tblk)->gcwait)

#define	LOGSYNC_DELTA(logsize)		min((logsize)/8, 128*LOGPSIZE)
#define	LOGSYNC_BARRIER(logsize)	((logsize)/4)


static DEFINE_SPINLOCK(jfsLCacheLock);

#define	LCACHE_LOCK(flags)	spin_lock_irqsave(&jfsLCacheLock, flags)
#define	LCACHE_UNLOCK(flags)	spin_unlock_irqrestore(&jfsLCacheLock, flags)

#define LCACHE_SLEEP_COND(wq, cond, flags)	\
do {						\
	if (cond)				\
		break;				\
	__SLEEP_COND(wq, cond, LCACHE_LOCK(flags), LCACHE_UNLOCK(flags)); \
} while (0)

#define	LCACHE_WAKEUP(event)	wake_up(event)


#define	lbmREAD		0x0001
#define	lbmWRITE	0x0002	
#define	lbmRELEASE	0x0004	
#define	lbmSYNC		0x0008	
#define lbmFREE		0x0010	
#define	lbmDONE		0x0020
#define	lbmERROR	0x0040
#define lbmGC		0x0080	
#define lbmDIRECT	0x0100

static LIST_HEAD(jfs_external_logs);
static struct jfs_log *dummy_log = NULL;
static DEFINE_MUTEX(jfs_log_mutex);

static int lmWriteRecord(struct jfs_log * log, struct tblock * tblk,
			 struct lrd * lrd, struct tlock * tlck);

static int lmNextPage(struct jfs_log * log);
static int lmLogFileSystem(struct jfs_log * log, struct jfs_sb_info *sbi,
			   int activate);

static int open_inline_log(struct super_block *sb);
static int open_dummy_log(struct super_block *sb);
static int lbmLogInit(struct jfs_log * log);
static void lbmLogShutdown(struct jfs_log * log);
static struct lbuf *lbmAllocate(struct jfs_log * log, int);
static void lbmFree(struct lbuf * bp);
static void lbmfree(struct lbuf * bp);
static int lbmRead(struct jfs_log * log, int pn, struct lbuf ** bpp);
static void lbmWrite(struct jfs_log * log, struct lbuf * bp, int flag, int cant_block);
static void lbmDirectWrite(struct jfs_log * log, struct lbuf * bp, int flag);
static int lbmIOWait(struct lbuf * bp, int flag);
static bio_end_io_t lbmIODone;
static void lbmStartIO(struct lbuf * bp);
static void lmGCwrite(struct jfs_log * log, int cant_block);
static int lmLogSync(struct jfs_log * log, int hard_sync);



#ifdef CONFIG_JFS_STATISTICS
static struct lmStat {
	uint commit;		
	uint pagedone;		/* # of page written */
	uint submitted;		
	uint full_page;		
	uint partial_page;	
} lmStat;
#endif

static void write_special_inodes(struct jfs_log *log,
				 int (*writer)(struct address_space *))
{
	struct jfs_sb_info *sbi;

	list_for_each_entry(sbi, &log->sb_list, log_list) {
		writer(sbi->ipbmap->i_mapping);
		writer(sbi->ipimap->i_mapping);
		writer(sbi->direct_inode->i_mapping);
	}
}

int lmLog(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
	  struct tlock * tlck)
{
	int lsn;
	int diffp, difft;
	struct metapage *mp = NULL;
	unsigned long flags;

	jfs_info("lmLog: log:0x%p tblk:0x%p, lrd:0x%p tlck:0x%p",
		 log, tblk, lrd, tlck);

	LOG_LOCK(log);

	
	if (tblk == NULL)
		goto writeRecord;

	
	if (tlck == NULL ||
	    tlck->type & tlckBTROOT || (mp = tlck->mp) == NULL)
		goto writeRecord;

	lsn = log->lsn;

	LOGSYNC_LOCK(log, flags);

	if (mp->lsn == 0) {
		mp->log = log;
		mp->lsn = lsn;
		log->count++;

		
		list_add_tail(&mp->synclist, &log->synclist);
	}

	if (tblk->lsn == 0) {
		
		tblk->lsn = mp->lsn;
		log->count++;

		
		list_add(&tblk->synclist, &mp->synclist);
	}
	else {
		
		logdiff(diffp, mp->lsn, log);
		logdiff(difft, tblk->lsn, log);
		if (diffp < difft) {
			
			tblk->lsn = mp->lsn;

			
			list_move(&tblk->synclist, &mp->synclist);
		}
	}

	LOGSYNC_UNLOCK(log, flags);

      writeRecord:
	lsn = lmWriteRecord(log, tblk, lrd, tlck);

	logdiff(diffp, lsn, log);
	if (diffp >= log->nextsync)
		lsn = lmLogSync(log, 0);

	
	log->lsn = lsn;

	LOG_UNLOCK(log);

	
	return lsn;
}

static int
lmWriteRecord(struct jfs_log * log, struct tblock * tblk, struct lrd * lrd,
	      struct tlock * tlck)
{
	int lsn = 0;		
	struct lbuf *bp;	
	struct logpage *lp;	
	caddr_t dst;		
	int dstoffset;		
	int freespace;		
	caddr_t p;		
	caddr_t src;
	int srclen;
	int nbytes;		
	int i;
	int len;
	struct linelock *linelock;
	struct lv *lv;
	struct lvd *lvd;
	int l2linesize;

	len = 0;

	
	bp = (struct lbuf *) log->bp;
	lp = (struct logpage *) bp->l_ldata;
	dstoffset = log->eor;

	
	if (tlck == NULL)
		goto moveLrd;

	
	if (tlck->flag & tlckPAGELOCK) {
		p = (caddr_t) (tlck->mp->data);
		linelock = (struct linelock *) & tlck->lock;
	}
	
	else if (tlck->flag & tlckINODELOCK) {
		if (tlck->type & tlckDTREE)
			p = (caddr_t) &JFS_IP(tlck->ip)->i_dtroot;
		else
			p = (caddr_t) &JFS_IP(tlck->ip)->i_xtroot;
		linelock = (struct linelock *) & tlck->lock;
	}
#ifdef	_JFS_WIP
	else if (tlck->flag & tlckINLINELOCK) {

		inlinelock = (struct inlinelock *) & tlck;
		p = (caddr_t) & inlinelock->pxd;
		linelock = (struct linelock *) & tlck;
	}
#endif				
	else {
		jfs_err("lmWriteRecord: UFO tlck:0x%p", tlck);
		return 0;	
	}
	l2linesize = linelock->l2linesize;

      moveData:
	ASSERT(linelock->index <= linelock->maxcnt);

	lv = linelock->lv;
	for (i = 0; i < linelock->index; i++, lv++) {
		if (lv->length == 0)
			continue;

		
		if (dstoffset >= LOGPSIZE - LOGPTLRSIZE) {
			
			lmNextPage(log);

			bp = log->bp;
			lp = (struct logpage *) bp->l_ldata;
			dstoffset = LOGPHDRSIZE;
		}

		src = (u8 *) p + (lv->offset << l2linesize);
		srclen = lv->length << l2linesize;
		len += srclen;
		while (srclen > 0) {
			freespace = (LOGPSIZE - LOGPTLRSIZE) - dstoffset;
			nbytes = min(freespace, srclen);
			dst = (caddr_t) lp + dstoffset;
			memcpy(dst, src, nbytes);
			dstoffset += nbytes;

			
			if (dstoffset < LOGPSIZE - LOGPTLRSIZE)
				break;

			
			lmNextPage(log);

			bp = (struct lbuf *) log->bp;
			lp = (struct logpage *) bp->l_ldata;
			dstoffset = LOGPHDRSIZE;

			srclen -= nbytes;
			src += nbytes;
		}

		len += 4;
		lvd = (struct lvd *) ((caddr_t) lp + dstoffset);
		lvd->offset = cpu_to_le16(lv->offset);
		lvd->length = cpu_to_le16(lv->length);
		dstoffset += 4;
		jfs_info("lmWriteRecord: lv offset:%d length:%d",
			 lv->offset, lv->length);
	}

	if ((i = linelock->next)) {
		linelock = (struct linelock *) lid_to_tlock(i);
		goto moveData;
	}

      moveLrd:
	lrd->length = cpu_to_le16(len);

	src = (caddr_t) lrd;
	srclen = LOGRDSIZE;

	while (srclen > 0) {
		freespace = (LOGPSIZE - LOGPTLRSIZE) - dstoffset;
		nbytes = min(freespace, srclen);
		dst = (caddr_t) lp + dstoffset;
		memcpy(dst, src, nbytes);

		dstoffset += nbytes;
		srclen -= nbytes;

		
		if (srclen)
			goto pageFull;


		
		log->eor = dstoffset;
		bp->l_eor = dstoffset;
		lsn = (log->page << L2LOGPSIZE) + dstoffset;

		if (lrd->type & cpu_to_le16(LOG_COMMIT)) {
			tblk->clsn = lsn;
			jfs_info("wr: tclsn:0x%x, beor:0x%x", tblk->clsn,
				 bp->l_eor);

			INCREMENT(lmStat.commit);	

			LOGGC_LOCK(log);

			
			tblk->flag = tblkGC_QUEUE;
			tblk->bp = log->bp;
			tblk->pn = log->page;
			tblk->eor = log->eor;

			
			list_add_tail(&tblk->cqueue, &log->cqueue);

			LOGGC_UNLOCK(log);
		}

		jfs_info("lmWriteRecord: lrd:0x%04x bp:0x%p pn:%d eor:0x%x",
			le16_to_cpu(lrd->type), log->bp, log->page, dstoffset);

		
		if (dstoffset < LOGPSIZE - LOGPTLRSIZE)
			return lsn;

	      pageFull:
		
		lmNextPage(log);

		bp = (struct lbuf *) log->bp;
		lp = (struct logpage *) bp->l_ldata;
		dstoffset = LOGPHDRSIZE;
		src += nbytes;
	}

	return lsn;
}


static int lmNextPage(struct jfs_log * log)
{
	struct logpage *lp;
	int lspn;		
	int pn;			
	struct lbuf *bp;
	struct lbuf *nextbp;
	struct tblock *tblk;

	
	pn = log->page;
	bp = log->bp;
	lp = (struct logpage *) bp->l_ldata;
	lspn = le32_to_cpu(lp->h.page);

	LOGGC_LOCK(log);

	
	if (list_empty(&log->cqueue))
		tblk = NULL;
	else
		tblk = list_entry(log->cqueue.prev, struct tblock, cqueue);


	
	if (tblk && tblk->pn == pn) {
		
		tblk->flag |= tblkGC_EOP;

		if (log->cflag & logGC_PAGEOUT) {
			if (bp->l_wqnext == NULL)
				lbmWrite(log, bp, 0, 0);
		} else {
			log->cflag |= logGC_PAGEOUT;
			lmGCwrite(log, 0);
		}
	}
	else {
		
		bp->l_ceor = bp->l_eor;
		lp->h.eor = lp->t.eor = cpu_to_le16(bp->l_ceor);
		lbmWrite(log, bp, lbmWRITE | lbmRELEASE | lbmFREE, 0);
	}
	LOGGC_UNLOCK(log);

	log->page = (pn == log->size - 1) ? 2 : pn + 1;
	log->eor = LOGPHDRSIZE;	

	
	nextbp = lbmAllocate(log, log->page);
	nextbp->l_eor = log->eor;
	log->bp = nextbp;

	
	lp = (struct logpage *) nextbp->l_ldata;
	lp->h.page = lp->t.page = cpu_to_le32(lspn + 1);
	lp->h.eor = lp->t.eor = cpu_to_le16(LOGPHDRSIZE);

	return 0;
}


/*
 * NAME:	lmGroupCommit()
 *
 * FUNCTION:	group commit
 *	initiate pageout of the pages with COMMIT in the order of
 *	page number - redrive pageout of the page at the head of
 *	pageout queue until full page has been written.
 *
 * RETURN:
 *
 * NOTE:
 *	LOGGC_LOCK serializes log group commit queue, and
 *	transaction blocks on the commit queue.
 *	N.B. LOG_LOCK is NOT held during lmGroupCommit().
 */
int lmGroupCommit(struct jfs_log * log, struct tblock * tblk)
{
	int rc = 0;

	LOGGC_LOCK(log);

	
	if (tblk->flag & tblkGC_COMMITTED) {
		if (tblk->flag & tblkGC_ERROR)
			rc = -EIO;

		LOGGC_UNLOCK(log);
		return rc;
	}
	jfs_info("lmGroup Commit: tblk = 0x%p, gcrtc = %d", tblk, log->gcrtc);

	if (tblk->xflag & COMMIT_LAZY)
		tblk->flag |= tblkGC_LAZY;

	if ((!(log->cflag & logGC_PAGEOUT)) && (!list_empty(&log->cqueue)) &&
	    (!(tblk->xflag & COMMIT_LAZY) || test_bit(log_FLUSH, &log->flag)
	     || jfs_tlocks_low)) {
		log->cflag |= logGC_PAGEOUT;

		lmGCwrite(log, 0);
	}

	if (tblk->xflag & COMMIT_LAZY) {
		LOGGC_UNLOCK(log);
		return 0;
	}

	

	if (tblk->flag & tblkGC_COMMITTED) {
		if (tblk->flag & tblkGC_ERROR)
			rc = -EIO;

		LOGGC_UNLOCK(log);
		return rc;
	}

	log->gcrtc++;
	tblk->flag |= tblkGC_READY;

	__SLEEP_COND(tblk->gcwait, (tblk->flag & tblkGC_COMMITTED),
		     LOGGC_LOCK(log), LOGGC_UNLOCK(log));

	
	if (tblk->flag & tblkGC_ERROR)
		rc = -EIO;

	LOGGC_UNLOCK(log);
	return rc;
}

static void lmGCwrite(struct jfs_log * log, int cant_write)
{
	struct lbuf *bp;
	struct logpage *lp;
	int gcpn;		
	struct tblock *tblk;
	struct tblock *xtblk = NULL;

	
	gcpn = list_entry(log->cqueue.next, struct tblock, cqueue)->pn;

	list_for_each_entry(tblk, &log->cqueue, cqueue) {
		if (tblk->pn != gcpn)
			break;

		xtblk = tblk;

		
		tblk->flag |= tblkGC_COMMIT;
	}
	tblk = xtblk;		

	bp = (struct lbuf *) tblk->bp;
	lp = (struct logpage *) bp->l_ldata;
	
	if (tblk->flag & tblkGC_EOP) {
		
		tblk->flag &= ~tblkGC_EOP;
		tblk->flag |= tblkGC_FREE;
		bp->l_ceor = bp->l_eor;
		lp->h.eor = lp->t.eor = cpu_to_le16(bp->l_ceor);
		lbmWrite(log, bp, lbmWRITE | lbmRELEASE | lbmGC,
			 cant_write);
		INCREMENT(lmStat.full_page);
	}
	
	else {
		bp->l_ceor = tblk->eor;	
		lp->h.eor = lp->t.eor = cpu_to_le16(bp->l_ceor);
		lbmWrite(log, bp, lbmWRITE | lbmGC, cant_write);
		INCREMENT(lmStat.partial_page);
	}
}

/*
 * NAME:	lmPostGC()
 *
 * FUNCTION:	group commit post-processing
 *	Processes transactions after their commit records have been written
 *	to disk, redriving log I/O if necessary.
 *
 * RETURN:	None
 *
 * NOTE:
 *	This routine is called a interrupt time by lbmIODone
 */
static void lmPostGC(struct lbuf * bp)
{
	unsigned long flags;
	struct jfs_log *log = bp->l_log;
	struct logpage *lp;
	struct tblock *tblk, *temp;

	
	spin_lock_irqsave(&log->gclock, flags);
	list_for_each_entry_safe(tblk, temp, &log->cqueue, cqueue) {
		if (!(tblk->flag & tblkGC_COMMIT))
			break;

		if (bp->l_flag & lbmERROR)
			tblk->flag |= tblkGC_ERROR;

		
		list_del(&tblk->cqueue);
		tblk->flag &= ~tblkGC_QUEUE;

		if (tblk == log->flush_tblk) {
			
			clear_bit(log_FLUSH, &log->flag);
			log->flush_tblk = NULL;
		}

		jfs_info("lmPostGC: tblk = 0x%p, flag = 0x%x", tblk,
			 tblk->flag);

		if (!(tblk->xflag & COMMIT_FORCE))
			txLazyUnlock(tblk);
		else {
			
			tblk->flag |= tblkGC_COMMITTED;

			if (tblk->flag & tblkGC_READY)
				log->gcrtc--;

			LOGGC_WAKEUP(tblk);
		}

		if (tblk->flag & tblkGC_FREE)
			lbmFree(bp);
		else if (tblk->flag & tblkGC_EOP) {
			
			lp = (struct logpage *) bp->l_ldata;
			bp->l_ceor = bp->l_eor;
			lp->h.eor = lp->t.eor = cpu_to_le16(bp->l_eor);
			jfs_info("lmPostGC: calling lbmWrite");
			lbmWrite(log, bp, lbmWRITE | lbmRELEASE | lbmFREE,
				 1);
		}

	}

	/* are there any transactions who have entered lnGroupCommit()
	 * (whose COMMITs are after that of the last log page written.
	 * They are waiting for new group commit (above at (SLEEP 1))
	 * or lazy transactions are on a full (queued) log page,
	 * select the latest ready transaction as new group leader and
	 * wake her up to lead her group.
	 */
	if ((!list_empty(&log->cqueue)) &&
	    ((log->gcrtc > 0) || (tblk->bp->l_wqnext != NULL) ||
	     test_bit(log_FLUSH, &log->flag) || jfs_tlocks_low))
		lmGCwrite(log, 1);

	else
		log->cflag &= ~logGC_PAGEOUT;

	
	spin_unlock_irqrestore(&log->gclock, flags);
	return;
}

/*
 * NAME:	lmLogSync()
 *
 * FUNCTION:	write log SYNCPT record for specified log
 *	if new sync address is available
 *	(normally the case if sync() is executed by back-ground
 *	process).
 *	calculate new value of i_nextsync which determines when
 *	this code is called again.
 *
 * PARAMETERS:	log	- log structure
 *		hard_sync - 1 to force all metadata to be written
 *
 * RETURN:	0
 *
 * serialization: LOG_LOCK() held on entry/exit
 */
static int lmLogSync(struct jfs_log * log, int hard_sync)
{
	int logsize;
	int written;		/* written since last syncpt */
	int free;		
	int delta;		
	int more;		
	struct lrd lrd;
	int lsn;
	struct logsyncblk *lp;
	unsigned long flags;

	
	if (hard_sync)
		write_special_inodes(log, filemap_fdatawrite);
	else
		write_special_inodes(log, filemap_flush);


	if (log->sync == log->syncpt) {
		LOGSYNC_LOCK(log, flags);
		if (list_empty(&log->synclist))
			log->sync = log->lsn;
		else {
			lp = list_entry(log->synclist.next,
					struct logsyncblk, synclist);
			log->sync = lp->lsn;
		}
		LOGSYNC_UNLOCK(log, flags);

	}

	if (log->sync != log->syncpt) {
		lrd.logtid = 0;
		lrd.backchain = 0;
		lrd.type = cpu_to_le16(LOG_SYNCPT);
		lrd.length = 0;
		lrd.log.syncpt.sync = cpu_to_le32(log->sync);
		lsn = lmWriteRecord(log, NULL, &lrd, NULL);

		log->syncpt = log->sync;
	} else
		lsn = log->lsn;

	logsize = log->logsize;

	logdiff(written, lsn, log);
	free = logsize - written;
	delta = LOGSYNC_DELTA(logsize);
	more = min(free / 2, delta);
	if (more < 2 * LOGPSIZE) {
		jfs_warn("\n ... Log Wrap ... Log Wrap ... Log Wrap ...\n");
		
		

		
		log->syncpt = log->sync = lsn;
		log->nextsync = delta;
	} else
		/* next syncpt trigger = written + more */
		log->nextsync = written + more;

	/* if number of bytes written from last sync point is more
	 * than 1/4 of the log size, stop new transactions from
	 * starting until all current transactions are completed
	 * by setting syncbarrier flag.
	 */
	if (!test_bit(log_SYNCBARRIER, &log->flag) &&
	    (written > LOGSYNC_BARRIER(logsize)) && log->active) {
		set_bit(log_SYNCBARRIER, &log->flag);
		jfs_info("log barrier on: lsn=0x%x syncpt=0x%x", lsn,
			 log->syncpt);
		jfs_flush_journal(log, 0);
	}

	return lsn;
}

/*
 * NAME:	jfs_syncpt
 *
 * FUNCTION:	write log SYNCPT record for specified log
 *
 * PARAMETERS:	log	  - log structure
 *		hard_sync - set to 1 to force metadata to be written
 */
void jfs_syncpt(struct jfs_log *log, int hard_sync)
{	LOG_LOCK(log);
	lmLogSync(log, hard_sync);
	LOG_UNLOCK(log);
}

int lmLogOpen(struct super_block *sb)
{
	int rc;
	struct block_device *bdev;
	struct jfs_log *log;
	struct jfs_sb_info *sbi = JFS_SBI(sb);

	if (sbi->flag & JFS_NOINTEGRITY)
		return open_dummy_log(sb);

	if (sbi->mntflag & JFS_INLINELOG)
		return open_inline_log(sb);

	mutex_lock(&jfs_log_mutex);
	list_for_each_entry(log, &jfs_external_logs, journal_list) {
		if (log->bdev->bd_dev == sbi->logdev) {
			if (memcmp(log->uuid, sbi->loguuid,
				   sizeof(log->uuid))) {
				jfs_warn("wrong uuid on JFS journal\n");
				mutex_unlock(&jfs_log_mutex);
				return -EINVAL;
			}
			if ((rc = lmLogFileSystem(log, sbi, 1))) {
				mutex_unlock(&jfs_log_mutex);
				return rc;
			}
			goto journal_found;
		}
	}

	if (!(log = kzalloc(sizeof(struct jfs_log), GFP_KERNEL))) {
		mutex_unlock(&jfs_log_mutex);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&log->sb_list);
	init_waitqueue_head(&log->syncwait);


	bdev = blkdev_get_by_dev(sbi->logdev, FMODE_READ|FMODE_WRITE|FMODE_EXCL,
				 log);
	if (IS_ERR(bdev)) {
		rc = PTR_ERR(bdev);
		goto free;
	}

	log->bdev = bdev;
	memcpy(log->uuid, sbi->loguuid, sizeof(log->uuid));

	if ((rc = lmLogInit(log)))
		goto close;

	list_add(&log->journal_list, &jfs_external_logs);

	if ((rc = lmLogFileSystem(log, sbi, 1)))
		goto shutdown;

journal_found:
	LOG_LOCK(log);
	list_add(&sbi->log_list, &log->sb_list);
	sbi->log = log;
	LOG_UNLOCK(log);

	mutex_unlock(&jfs_log_mutex);
	return 0;

      shutdown:		
	list_del(&log->journal_list);
	lbmLogShutdown(log);

      close:		
	blkdev_put(bdev, FMODE_READ|FMODE_WRITE|FMODE_EXCL);

      free:		
	mutex_unlock(&jfs_log_mutex);
	kfree(log);

	jfs_warn("lmLogOpen: exit(%d)", rc);
	return rc;
}

static int open_inline_log(struct super_block *sb)
{
	struct jfs_log *log;
	int rc;

	if (!(log = kzalloc(sizeof(struct jfs_log), GFP_KERNEL)))
		return -ENOMEM;
	INIT_LIST_HEAD(&log->sb_list);
	init_waitqueue_head(&log->syncwait);

	set_bit(log_INLINELOG, &log->flag);
	log->bdev = sb->s_bdev;
	log->base = addressPXD(&JFS_SBI(sb)->logpxd);
	log->size = lengthPXD(&JFS_SBI(sb)->logpxd) >>
	    (L2LOGPSIZE - sb->s_blocksize_bits);
	log->l2bsize = sb->s_blocksize_bits;
	ASSERT(L2LOGPSIZE >= sb->s_blocksize_bits);

	if ((rc = lmLogInit(log))) {
		kfree(log);
		jfs_warn("lmLogOpen: exit(%d)", rc);
		return rc;
	}

	list_add(&JFS_SBI(sb)->log_list, &log->sb_list);
	JFS_SBI(sb)->log = log;

	return rc;
}

static int open_dummy_log(struct super_block *sb)
{
	int rc;

	mutex_lock(&jfs_log_mutex);
	if (!dummy_log) {
		dummy_log = kzalloc(sizeof(struct jfs_log), GFP_KERNEL);
		if (!dummy_log) {
			mutex_unlock(&jfs_log_mutex);
			return -ENOMEM;
		}
		INIT_LIST_HEAD(&dummy_log->sb_list);
		init_waitqueue_head(&dummy_log->syncwait);
		dummy_log->no_integrity = 1;
		
		dummy_log->base = 0;
		dummy_log->size = 1024;
		rc = lmLogInit(dummy_log);
		if (rc) {
			kfree(dummy_log);
			dummy_log = NULL;
			mutex_unlock(&jfs_log_mutex);
			return rc;
		}
	}

	LOG_LOCK(dummy_log);
	list_add(&JFS_SBI(sb)->log_list, &dummy_log->sb_list);
	JFS_SBI(sb)->log = dummy_log;
	LOG_UNLOCK(dummy_log);
	mutex_unlock(&jfs_log_mutex);

	return 0;
}

int lmLogInit(struct jfs_log * log)
{
	int rc = 0;
	struct lrd lrd;
	struct logsuper *logsuper;
	struct lbuf *bpsuper;
	struct lbuf *bp;
	struct logpage *lp;
	int lsn = 0;

	jfs_info("lmLogInit: log:0x%p", log);

	
	LOGGC_LOCK_INIT(log);

	
	LOG_LOCK_INIT(log);

	LOGSYNC_LOCK_INIT(log);

	INIT_LIST_HEAD(&log->synclist);

	INIT_LIST_HEAD(&log->cqueue);
	log->flush_tblk = NULL;

	log->count = 0;

	if ((rc = lbmLogInit(log)))
		return rc;

	if (!test_bit(log_INLINELOG, &log->flag))
		log->l2bsize = L2LOGPSIZE;

	
	if (log->no_integrity) {
		bp = lbmAllocate(log , 0);
		log->bp = bp;
		bp->l_pn = bp->l_eor = 0;
	} else {
		if ((rc = lbmRead(log, 1, &bpsuper)))
			goto errout10;

		logsuper = (struct logsuper *) bpsuper->l_ldata;

		if (logsuper->magic != cpu_to_le32(LOGMAGIC)) {
			jfs_warn("*** Log Format Error ! ***");
			rc = -EINVAL;
			goto errout20;
		}

		
		if (logsuper->state != cpu_to_le32(LOGREDONE)) {
			jfs_warn("*** Log Is Dirty ! ***");
			rc = -EINVAL;
			goto errout20;
		}

		
		if (test_bit(log_INLINELOG,&log->flag)) {
			if (log->size != le32_to_cpu(logsuper->size)) {
				rc = -EINVAL;
				goto errout20;
			}
			jfs_info("lmLogInit: inline log:0x%p base:0x%Lx "
				 "size:0x%x", log,
				 (unsigned long long) log->base, log->size);
		} else {
			if (memcmp(logsuper->uuid, log->uuid, 16)) {
				jfs_warn("wrong uuid on JFS log device");
				goto errout20;
			}
			log->size = le32_to_cpu(logsuper->size);
			log->l2bsize = le32_to_cpu(logsuper->l2bsize);
			jfs_info("lmLogInit: external log:0x%p base:0x%Lx "
				 "size:0x%x", log,
				 (unsigned long long) log->base, log->size);
		}

		log->page = le32_to_cpu(logsuper->end) / LOGPSIZE;
		log->eor = le32_to_cpu(logsuper->end) - (LOGPSIZE * log->page);

		
		if ((rc = lbmRead(log, log->page, &bp)))
			goto errout20;

		lp = (struct logpage *) bp->l_ldata;

		jfs_info("lmLogInit: lsn:0x%x page:%d eor:%d:%d",
			 le32_to_cpu(logsuper->end), log->page, log->eor,
			 le16_to_cpu(lp->h.eor));

		log->bp = bp;
		bp->l_pn = log->page;
		bp->l_eor = log->eor;

		
		if (log->eor >= LOGPSIZE - LOGPTLRSIZE)
			lmNextPage(log);

		lrd.logtid = 0;
		lrd.backchain = 0;
		lrd.type = cpu_to_le16(LOG_SYNCPT);
		lrd.length = 0;
		lrd.log.syncpt.sync = 0;
		lsn = lmWriteRecord(log, NULL, &lrd, NULL);
		bp = log->bp;
		bp->l_ceor = bp->l_eor;
		lp = (struct logpage *) bp->l_ldata;
		lp->h.eor = lp->t.eor = cpu_to_le16(bp->l_eor);
		lbmWrite(log, bp, lbmWRITE | lbmSYNC, 0);
		if ((rc = lbmIOWait(bp, 0)))
			goto errout30;

		logsuper->state = cpu_to_le32(LOGMOUNT);
		log->serial = le32_to_cpu(logsuper->serial) + 1;
		logsuper->serial = cpu_to_le32(log->serial);
		lbmDirectWrite(log, bpsuper, lbmWRITE | lbmRELEASE | lbmSYNC);
		if ((rc = lbmIOWait(bpsuper, lbmFREE)))
			goto errout30;
	}

	
	log->logsize = (log->size - 2) << L2LOGPSIZE;
	log->lsn = lsn;
	log->syncpt = lsn;
	log->sync = log->syncpt;
	log->nextsync = LOGSYNC_DELTA(log->logsize);

	jfs_info("lmLogInit: lsn:0x%x syncpt:0x%x sync:0x%x",
		 log->lsn, log->syncpt, log->sync);

	log->clsn = lsn;

	return 0;

      errout30:		
	log->wqueue = NULL;
	bp->l_wqnext = NULL;
	lbmFree(bp);

      errout20:		
	lbmFree(bpsuper);

      errout10:		
	lbmLogShutdown(log);

	jfs_warn("lmLogInit: exit(%d)", rc);
	return rc;
}


int lmLogClose(struct super_block *sb)
{
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	struct jfs_log *log = sbi->log;
	struct block_device *bdev;
	int rc = 0;

	jfs_info("lmLogClose: log:0x%p", log);

	mutex_lock(&jfs_log_mutex);
	LOG_LOCK(log);
	list_del(&sbi->log_list);
	LOG_UNLOCK(log);
	sbi->log = NULL;

	/*
	 * We need to make sure all of the "written" metapages
	 * actually make it to disk
	 */
	sync_blockdev(sb->s_bdev);

	if (test_bit(log_INLINELOG, &log->flag)) {
		rc = lmLogShutdown(log);
		kfree(log);
		goto out;
	}

	if (!log->no_integrity)
		lmLogFileSystem(log, sbi, 0);

	if (!list_empty(&log->sb_list))
		goto out;

	if (log->no_integrity)
		goto out;

	list_del(&log->journal_list);
	bdev = log->bdev;
	rc = lmLogShutdown(log);

	blkdev_put(bdev, FMODE_READ|FMODE_WRITE|FMODE_EXCL);

	kfree(log);

      out:
	mutex_unlock(&jfs_log_mutex);
	jfs_info("lmLogClose: exit(%d)", rc);
	return rc;
}


/*
 * NAME:	jfs_flush_journal()
 *
 * FUNCTION:	initiate write of any outstanding transactions to the journal
 *		and optionally wait until they are all written to disk
 *
 *		wait == 0  flush until latest txn is committed, don't wait
 *		wait == 1  flush until latest txn is committed, wait
 *		wait > 1   flush until all txn's are complete, wait
 */
void jfs_flush_journal(struct jfs_log *log, int wait)
{
	int i;
	struct tblock *target = NULL;

	
	if (!log)
		return;

	jfs_info("jfs_flush_journal: log:0x%p wait=%d", log, wait);

	LOGGC_LOCK(log);

	if (!list_empty(&log->cqueue)) {
		/*
		 * This ensures that we will keep writing to the journal as long
		 * as there are unwritten commit records
		 */
		target = list_entry(log->cqueue.prev, struct tblock, cqueue);

		if (test_bit(log_FLUSH, &log->flag)) {
			if (log->flush_tblk)
				log->flush_tblk = target;
		} else {
			
			log->flush_tblk = target;
			set_bit(log_FLUSH, &log->flag);

			if (!(log->cflag & logGC_PAGEOUT)) {
				log->cflag |= logGC_PAGEOUT;
				lmGCwrite(log, 0);
			}
		}
	}
	if ((wait > 1) || test_bit(log_SYNCBARRIER, &log->flag)) {
		
		set_bit(log_FLUSH, &log->flag);
		log->flush_tblk = NULL;
	}

	if (wait && target && !(target->flag & tblkGC_COMMITTED)) {
		DECLARE_WAITQUEUE(__wait, current);

		add_wait_queue(&target->gcwait, &__wait);
		set_current_state(TASK_UNINTERRUPTIBLE);
		LOGGC_UNLOCK(log);
		schedule();
		__set_current_state(TASK_RUNNING);
		LOGGC_LOCK(log);
		remove_wait_queue(&target->gcwait, &__wait);
	}
	LOGGC_UNLOCK(log);

	if (wait < 2)
		return;

	write_special_inodes(log, filemap_fdatawrite);

	if ((!list_empty(&log->cqueue)) || !list_empty(&log->synclist)) {
		for (i = 0; i < 200; i++) {	
			msleep(250);
			write_special_inodes(log, filemap_fdatawrite);
			if (list_empty(&log->cqueue) &&
			    list_empty(&log->synclist))
				break;
		}
	}
	assert(list_empty(&log->cqueue));

#ifdef CONFIG_JFS_DEBUG
	if (!list_empty(&log->synclist)) {
		struct logsyncblk *lp;

		printk(KERN_ERR "jfs_flush_journal: synclist not empty\n");
		list_for_each_entry(lp, &log->synclist, synclist) {
			if (lp->xflag & COMMIT_PAGE) {
				struct metapage *mp = (struct metapage *)lp;
				print_hex_dump(KERN_ERR, "metapage: ",
					       DUMP_PREFIX_ADDRESS, 16, 4,
					       mp, sizeof(struct metapage), 0);
				print_hex_dump(KERN_ERR, "page: ",
					       DUMP_PREFIX_ADDRESS, 16,
					       sizeof(long), mp->page,
					       sizeof(struct page), 0);
			} else
				print_hex_dump(KERN_ERR, "tblock:",
					       DUMP_PREFIX_ADDRESS, 16, 4,
					       lp, sizeof(struct tblock), 0);
		}
	}
#else
	WARN_ON(!list_empty(&log->synclist));
#endif
	clear_bit(log_FLUSH, &log->flag);
}

int lmLogShutdown(struct jfs_log * log)
{
	int rc;
	struct lrd lrd;
	int lsn;
	struct logsuper *logsuper;
	struct lbuf *bpsuper;
	struct lbuf *bp;
	struct logpage *lp;

	jfs_info("lmLogShutdown: log:0x%p", log);

	jfs_flush_journal(log, 2);

	lrd.logtid = 0;
	lrd.backchain = 0;
	lrd.type = cpu_to_le16(LOG_SYNCPT);
	lrd.length = 0;
	lrd.log.syncpt.sync = 0;

	lsn = lmWriteRecord(log, NULL, &lrd, NULL);
	bp = log->bp;
	lp = (struct logpage *) bp->l_ldata;
	lp->h.eor = lp->t.eor = cpu_to_le16(bp->l_eor);
	lbmWrite(log, log->bp, lbmWRITE | lbmRELEASE | lbmSYNC, 0);
	lbmIOWait(log->bp, lbmFREE);
	log->bp = NULL;

	if ((rc = lbmRead(log, 1, &bpsuper)))
		goto out;

	logsuper = (struct logsuper *) bpsuper->l_ldata;
	logsuper->state = cpu_to_le32(LOGREDONE);
	logsuper->end = cpu_to_le32(lsn);
	lbmDirectWrite(log, bpsuper, lbmWRITE | lbmRELEASE | lbmSYNC);
	rc = lbmIOWait(bpsuper, lbmFREE);

	jfs_info("lmLogShutdown: lsn:0x%x page:%d eor:%d",
		 lsn, log->page, log->eor);

      out:
	lbmLogShutdown(log);

	if (rc) {
		jfs_warn("lmLogShutdown: exit(%d)", rc);
	}
	return rc;
}


static int lmLogFileSystem(struct jfs_log * log, struct jfs_sb_info *sbi,
			   int activate)
{
	int rc = 0;
	int i;
	struct logsuper *logsuper;
	struct lbuf *bpsuper;
	char *uuid = sbi->uuid;

	if ((rc = lbmRead(log, 1, &bpsuper)))
		return rc;

	logsuper = (struct logsuper *) bpsuper->l_ldata;
	if (activate) {
		for (i = 0; i < MAX_ACTIVE; i++)
			if (!memcmp(logsuper->active[i].uuid, NULL_UUID, 16)) {
				memcpy(logsuper->active[i].uuid, uuid, 16);
				sbi->aggregate = i;
				break;
			}
		if (i == MAX_ACTIVE) {
			jfs_warn("Too many file systems sharing journal!");
			lbmFree(bpsuper);
			return -EMFILE;	
		}
	} else {
		for (i = 0; i < MAX_ACTIVE; i++)
			if (!memcmp(logsuper->active[i].uuid, uuid, 16)) {
				memcpy(logsuper->active[i].uuid, NULL_UUID, 16);
				break;
			}
		if (i == MAX_ACTIVE) {
			jfs_warn("Somebody stomped on the journal!");
			lbmFree(bpsuper);
			return -EIO;
		}

	}

	lbmDirectWrite(log, bpsuper, lbmWRITE | lbmRELEASE | lbmSYNC);
	rc = lbmIOWait(bpsuper, lbmFREE);

	return rc;
}


static int lbmLogInit(struct jfs_log * log)
{				
	int i;
	struct lbuf *lbuf;

	jfs_info("lbmLogInit: log:0x%p", log);

	
	log->bp = NULL;

	
	log->wqueue = NULL;

	init_waitqueue_head(&log->free_wait);

	log->lbuf_free = NULL;

	for (i = 0; i < LOGPAGES;) {
		char *buffer;
		uint offset;
		struct page *page;

		buffer = (char *) get_zeroed_page(GFP_KERNEL);
		if (buffer == NULL)
			goto error;
		page = virt_to_page(buffer);
		for (offset = 0; offset < PAGE_SIZE; offset += LOGPSIZE) {
			lbuf = kmalloc(sizeof(struct lbuf), GFP_KERNEL);
			if (lbuf == NULL) {
				if (offset == 0)
					free_page((unsigned long) buffer);
				goto error;
			}
			if (offset) 
				get_page(page);
			lbuf->l_offset = offset;
			lbuf->l_ldata = buffer + offset;
			lbuf->l_page = page;
			lbuf->l_log = log;
			init_waitqueue_head(&lbuf->l_ioevent);

			lbuf->l_freelist = log->lbuf_free;
			log->lbuf_free = lbuf;
			i++;
		}
	}

	return (0);

      error:
	lbmLogShutdown(log);
	return -ENOMEM;
}


static void lbmLogShutdown(struct jfs_log * log)
{
	struct lbuf *lbuf;

	jfs_info("lbmLogShutdown: log:0x%p", log);

	lbuf = log->lbuf_free;
	while (lbuf) {
		struct lbuf *next = lbuf->l_freelist;
		__free_page(lbuf->l_page);
		kfree(lbuf);
		lbuf = next;
	}
}


static struct lbuf *lbmAllocate(struct jfs_log * log, int pn)
{
	struct lbuf *bp;
	unsigned long flags;

	LCACHE_LOCK(flags);
	LCACHE_SLEEP_COND(log->free_wait, (bp = log->lbuf_free), flags);
	log->lbuf_free = bp->l_freelist;
	LCACHE_UNLOCK(flags);

	bp->l_flag = 0;

	bp->l_wqnext = NULL;
	bp->l_freelist = NULL;

	bp->l_pn = pn;
	bp->l_blkno = log->base + (pn << (L2LOGPSIZE - log->l2bsize));
	bp->l_ceor = 0;

	return bp;
}


static void lbmFree(struct lbuf * bp)
{
	unsigned long flags;

	LCACHE_LOCK(flags);

	lbmfree(bp);

	LCACHE_UNLOCK(flags);
}

static void lbmfree(struct lbuf * bp)
{
	struct jfs_log *log = bp->l_log;

	assert(bp->l_wqnext == NULL);

	bp->l_freelist = log->lbuf_free;
	log->lbuf_free = bp;

	wake_up(&log->free_wait);
	return;
}


static inline void lbmRedrive(struct lbuf *bp)
{
	unsigned long flags;

	spin_lock_irqsave(&log_redrive_lock, flags);
	bp->l_redrive_next = log_redrive_list;
	log_redrive_list = bp;
	spin_unlock_irqrestore(&log_redrive_lock, flags);

	wake_up_process(jfsIOthread);
}


static int lbmRead(struct jfs_log * log, int pn, struct lbuf ** bpp)
{
	struct bio *bio;
	struct lbuf *bp;

	*bpp = bp = lbmAllocate(log, pn);
	jfs_info("lbmRead: bp:0x%p pn:0x%x", bp, pn);

	bp->l_flag |= lbmREAD;

	bio = bio_alloc(GFP_NOFS, 1);

	bio->bi_sector = bp->l_blkno << (log->l2bsize - 9);
	bio->bi_bdev = log->bdev;
	bio->bi_io_vec[0].bv_page = bp->l_page;
	bio->bi_io_vec[0].bv_len = LOGPSIZE;
	bio->bi_io_vec[0].bv_offset = bp->l_offset;

	bio->bi_vcnt = 1;
	bio->bi_idx = 0;
	bio->bi_size = LOGPSIZE;

	bio->bi_end_io = lbmIODone;
	bio->bi_private = bp;
	submit_bio(READ_SYNC, bio);

	wait_event(bp->l_ioevent, (bp->l_flag != lbmREAD));

	return 0;
}


static void lbmWrite(struct jfs_log * log, struct lbuf * bp, int flag,
		     int cant_block)
{
	struct lbuf *tail;
	unsigned long flags;

	jfs_info("lbmWrite: bp:0x%p flag:0x%x pn:0x%x", bp, flag, bp->l_pn);

	
	bp->l_blkno =
	    log->base + (bp->l_pn << (L2LOGPSIZE - log->l2bsize));

	LCACHE_LOCK(flags);		

	bp->l_flag = flag;

	tail = log->wqueue;

	
	if (bp->l_wqnext == NULL) {
		
		if (tail == NULL) {
			log->wqueue = bp;
			bp->l_wqnext = bp;
		} else {
			log->wqueue = bp;
			bp->l_wqnext = tail->l_wqnext;
			tail->l_wqnext = bp;
		}

		tail = bp;
	}

	
	if ((bp != tail->l_wqnext) || !(flag & lbmWRITE)) {
		LCACHE_UNLOCK(flags);	
		return;
	}

	LCACHE_UNLOCK(flags);	

	if (cant_block)
		lbmRedrive(bp);
	else if (flag & lbmSYNC)
		lbmStartIO(bp);
	else {
		LOGGC_UNLOCK(log);
		lbmStartIO(bp);
		LOGGC_LOCK(log);
	}
}


static void lbmDirectWrite(struct jfs_log * log, struct lbuf * bp, int flag)
{
	jfs_info("lbmDirectWrite: bp:0x%p flag:0x%x pn:0x%x",
		 bp, flag, bp->l_pn);

	bp->l_flag = flag | lbmDIRECT;

	
	bp->l_blkno =
	    log->base + (bp->l_pn << (L2LOGPSIZE - log->l2bsize));

	lbmStartIO(bp);
}


static void lbmStartIO(struct lbuf * bp)
{
	struct bio *bio;
	struct jfs_log *log = bp->l_log;

	jfs_info("lbmStartIO\n");

	bio = bio_alloc(GFP_NOFS, 1);
	bio->bi_sector = bp->l_blkno << (log->l2bsize - 9);
	bio->bi_bdev = log->bdev;
	bio->bi_io_vec[0].bv_page = bp->l_page;
	bio->bi_io_vec[0].bv_len = LOGPSIZE;
	bio->bi_io_vec[0].bv_offset = bp->l_offset;

	bio->bi_vcnt = 1;
	bio->bi_idx = 0;
	bio->bi_size = LOGPSIZE;

	bio->bi_end_io = lbmIODone;
	bio->bi_private = bp;

	
	if (log->no_integrity) {
		bio->bi_size = 0;
		lbmIODone(bio, 0);
	} else {
		submit_bio(WRITE_SYNC, bio);
		INCREMENT(lmStat.submitted);
	}
}


static int lbmIOWait(struct lbuf * bp, int flag)
{
	unsigned long flags;
	int rc = 0;

	jfs_info("lbmIOWait1: bp:0x%p flag:0x%x:0x%x", bp, bp->l_flag, flag);

	LCACHE_LOCK(flags);		

	LCACHE_SLEEP_COND(bp->l_ioevent, (bp->l_flag & lbmDONE), flags);

	rc = (bp->l_flag & lbmERROR) ? -EIO : 0;

	if (flag & lbmFREE)
		lbmfree(bp);

	LCACHE_UNLOCK(flags);	

	jfs_info("lbmIOWait2: bp:0x%p flag:0x%x:0x%x", bp, bp->l_flag, flag);
	return rc;
}

static void lbmIODone(struct bio *bio, int error)
{
	struct lbuf *bp = bio->bi_private;
	struct lbuf *nextbp, *tail;
	struct jfs_log *log;
	unsigned long flags;

	jfs_info("lbmIODone: bp:0x%p flag:0x%x", bp, bp->l_flag);

	LCACHE_LOCK(flags);		

	bp->l_flag |= lbmDONE;

	if (!test_bit(BIO_UPTODATE, &bio->bi_flags)) {
		bp->l_flag |= lbmERROR;

		jfs_err("lbmIODone: I/O error in JFS log");
	}

	bio_put(bio);

	if (bp->l_flag & lbmREAD) {
		bp->l_flag &= ~lbmREAD;

		LCACHE_UNLOCK(flags);	

		
		LCACHE_WAKEUP(&bp->l_ioevent);

		return;
	}

	bp->l_flag &= ~lbmWRITE;
	INCREMENT(lmStat.pagedone);

	
	log = bp->l_log;
	log->clsn = (bp->l_pn << L2LOGPSIZE) + bp->l_ceor;

	if (bp->l_flag & lbmDIRECT) {
		LCACHE_WAKEUP(&bp->l_ioevent);
		LCACHE_UNLOCK(flags);
		return;
	}

	tail = log->wqueue;

	
	if (bp == tail) {
		if (bp->l_flag & lbmRELEASE) {
			log->wqueue = NULL;
			bp->l_wqnext = NULL;
		}
	}
	
	else {
		if (bp->l_flag & lbmRELEASE) {
			nextbp = tail->l_wqnext = bp->l_wqnext;
			bp->l_wqnext = NULL;

			if (nextbp->l_flag & lbmWRITE) {
				lbmRedrive(nextbp);
			}
		}
	}

	if (bp->l_flag & lbmSYNC) {
		LCACHE_UNLOCK(flags);	

		
		LCACHE_WAKEUP(&bp->l_ioevent);
	}

	else if (bp->l_flag & lbmGC) {
		LCACHE_UNLOCK(flags);
		lmPostGC(bp);
	}

	else {
		assert(bp->l_flag & lbmRELEASE);
		assert(bp->l_flag & lbmFREE);
		lbmfree(bp);

		LCACHE_UNLOCK(flags);	
	}
}

int jfsIOWait(void *arg)
{
	struct lbuf *bp;

	do {
		spin_lock_irq(&log_redrive_lock);
		while ((bp = log_redrive_list)) {
			log_redrive_list = bp->l_redrive_next;
			bp->l_redrive_next = NULL;
			spin_unlock_irq(&log_redrive_lock);
			lbmStartIO(bp);
			spin_lock_irq(&log_redrive_lock);
		}

		if (freezing(current)) {
			spin_unlock_irq(&log_redrive_lock);
			try_to_freeze();
		} else {
			set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock_irq(&log_redrive_lock);
			schedule();
			__set_current_state(TASK_RUNNING);
		}
	} while (!kthread_should_stop());

	jfs_info("jfsIOWait being killed!");
	return 0;
}

int lmLogFormat(struct jfs_log *log, s64 logAddress, int logSize)
{
	int rc = -EIO;
	struct jfs_sb_info *sbi;
	struct logsuper *logsuper;
	struct logpage *lp;
	int lspn;		
	struct lrd *lrd_ptr;
	int npages = 0;
	struct lbuf *bp;

	jfs_info("lmLogFormat: logAddress:%Ld logSize:%d",
		 (long long)logAddress, logSize);

	sbi = list_entry(log->sb_list.next, struct jfs_sb_info, log_list);

	
	bp = lbmAllocate(log, 1);

	npages = logSize >> sbi->l2nbperpage;

	/*
	 *	log space:
	 *
	 * page 0 - reserved;
	 * page 1 - log superblock;
	 * page 2 - log data page: A SYNC log record is written
	 *	    into this page at logform time;
	 * pages 3-N - log data page: set to empty log data pages;
	 */
	logsuper = (struct logsuper *) bp->l_ldata;

	logsuper->magic = cpu_to_le32(LOGMAGIC);
	logsuper->version = cpu_to_le32(LOGVERSION);
	logsuper->state = cpu_to_le32(LOGREDONE);
	logsuper->flag = cpu_to_le32(sbi->mntflag);	
	logsuper->size = cpu_to_le32(npages);
	logsuper->bsize = cpu_to_le32(sbi->bsize);
	logsuper->l2bsize = cpu_to_le32(sbi->l2bsize);
	logsuper->end = cpu_to_le32(2 * LOGPSIZE + LOGPHDRSIZE + LOGRDSIZE);

	bp->l_flag = lbmWRITE | lbmSYNC | lbmDIRECT;
	bp->l_blkno = logAddress + sbi->nbperpage;
	lbmStartIO(bp);
	if ((rc = lbmIOWait(bp, 0)))
		goto exit;

	/*
	 *	init pages 2 to npages-1 as log data pages:
	 *
	 * log page sequence number (lpsn) initialization:
	 *
	 * pn:   0     1     2     3                 n-1
	 *       +-----+-----+=====+=====+===.....===+=====+
	 * lspn:             N-1   0     1           N-2
	 *                   <--- N page circular file ---->
	 *
	 * the N (= npages-2) data pages of the log is maintained as
	 * a circular file for the log records;
	 * lpsn grows by 1 monotonically as each log page is written
	 * to the circular file of the log;
	 * and setLogpage() will not reset the page number even if
	 * the eor is equal to LOGPHDRSIZE. In order for binary search
	 * still work in find log end process, we have to simulate the
	 * log wrap situation at the log format time.
	 * The 1st log page written will have the highest lpsn. Then
	 * the succeeding log pages will have ascending order of
	 * the lspn starting from 0, ... (N-2)
	 */
	lp = (struct logpage *) bp->l_ldata;
	/*
	 * initialize 1st log page to be written: lpsn = N - 1,
	 * write a SYNCPT log record is written to this page
	 */
	lp->h.page = lp->t.page = cpu_to_le32(npages - 3);
	lp->h.eor = lp->t.eor = cpu_to_le16(LOGPHDRSIZE + LOGRDSIZE);

	lrd_ptr = (struct lrd *) &lp->data;
	lrd_ptr->logtid = 0;
	lrd_ptr->backchain = 0;
	lrd_ptr->type = cpu_to_le16(LOG_SYNCPT);
	lrd_ptr->length = 0;
	lrd_ptr->log.syncpt.sync = 0;

	bp->l_blkno += sbi->nbperpage;
	bp->l_flag = lbmWRITE | lbmSYNC | lbmDIRECT;
	lbmStartIO(bp);
	if ((rc = lbmIOWait(bp, 0)))
		goto exit;

	for (lspn = 0; lspn < npages - 3; lspn++) {
		lp->h.page = lp->t.page = cpu_to_le32(lspn);
		lp->h.eor = lp->t.eor = cpu_to_le16(LOGPHDRSIZE);

		bp->l_blkno += sbi->nbperpage;
		bp->l_flag = lbmWRITE | lbmSYNC | lbmDIRECT;
		lbmStartIO(bp);
		if ((rc = lbmIOWait(bp, 0)))
			goto exit;
	}

	rc = 0;
exit:
	
	lbmFree(bp);

	return rc;
}

#ifdef CONFIG_JFS_STATISTICS
static int jfs_lmstats_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m,
		       "JFS Logmgr stats\n"
		       "================\n"
		       "commits = %d\n"
		       "writes submitted = %d\n"
		       "writes completed = %d\n"
		       "full pages submitted = %d\n"
		       "partial pages submitted = %d\n",
		       lmStat.commit,
		       lmStat.submitted,
		       lmStat.pagedone,
		       lmStat.full_page,
		       lmStat.partial_page);
	return 0;
}

static int jfs_lmstats_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, jfs_lmstats_proc_show, NULL);
}

const struct file_operations jfs_lmstats_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= jfs_lmstats_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif 
