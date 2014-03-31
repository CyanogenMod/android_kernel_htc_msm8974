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
#ifndef	_H_JFS_LOGMGR
#define _H_JFS_LOGMGR

#include "jfs_filsys.h"
#include "jfs_lock.h"


#define	LOGPSIZE	4096
#define	L2LOGPSIZE	12

#define LOGPAGES	16	

#define	LOGSUPER_B	1
#define	LOGSTART_B	2

#define	LOGMAGIC	0x87654321
#define	LOGVERSION	1

#define MAX_ACTIVE	128	

struct logsuper {
	__le32 magic;		
	__le32 version;		
	__le32 serial;		
	__le32 size;		
	__le32 bsize;		
	__le32 l2bsize;		

	__le32 flag;		
	__le32 state;		

	__le32 end;		
	char uuid[16];		
	char label[16];		
	struct {
		char uuid[16];
	} active[MAX_ACTIVE];	
};

#define NULL_UUID "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"


#define	LOGMOUNT	0	
#define LOGREDONE	1	
#define LOGWRAP		2	
#define LOGREADERR	3	


/*
 *	log logical page
 *
 * (this comment should be rewritten !)
 * the header and trailer structures (h,t) will normally have
 * the same page and eor value.
 * An exception to this occurs when a complete page write is not
 * accomplished on a power failure. Since the hardware may "split write"
 * sectors in the page, any out of order sequence may occur during powerfail
 * and needs to be recognized during log replay.  The xor value is
 * an "exclusive or" of all log words in the page up to eor.  This
 * 32 bit eor is stored with the top 16 bits in the header and the
 * bottom 16 bits in the trailer.  logredo can easily recognize pages
 * that were not completed by reconstructing this eor and checking
 * the log page.
 *
 * Previous versions of the operating system did not allow split
 * writes and detected partially written records in logredo by
 * ordering the updates to the header, trailer, and the move of data
 * into the logdata area.  The order: (1) data is moved (2) header
 * is updated (3) trailer is updated.  In logredo, when the header
 * differed from the trailer, the header and trailer were reconciled
 * as follows: if h.page != t.page they were set to the smaller of
 * the two and h.eor and t.eor set to 8 (i.e. empty page). if (only)
 * h.eor != t.eor they were set to the smaller of their two values.
 */
struct logpage {
	struct {		
		__le32 page;	
		__le16 rsrvd;	
		__le16 eor;	
	} h;

	__le32 data[LOGPSIZE / 4 - 4];	

	struct {		
		__le32 page;	
		__le16 rsrvd;	
		__le16 eor;	
	} t;
};

#define LOGPHDRSIZE	8	
#define LOGPTLRSIZE	8	


/*
 *	log record
 *
 * (this comment should be rewritten !)
 * jfs uses only "after" log records (only a single writer is allowed
 * in a page, pages are written to temporary paging space if
 * if they must be written to disk before commit, and i/o is
 * scheduled for modified pages to their home location after
 * the log records containing the after values and the commit
 * record is written to the log on disk, undo discards the copy
 * in main-memory.)
 *
 * a log record consists of a data area of variable length followed by
 * a descriptor of fixed size LOGRDSIZE bytes.
 * the data area is rounded up to an integral number of 4-bytes and
 * must be no longer than LOGPSIZE.
 * the descriptor is of size of multiple of 4-bytes and aligned on a
 * 4-byte boundary.
 * records are packed one after the other in the data area of log pages.
 * (sometimes a DUMMY record is inserted so that at least one record ends
 * on every page or the longest record is placed on at most two pages).
 * the field eor in page header/trailer points to the byte following
 * the last record on a page.
 */

#define LOG_COMMIT		0x8000
#define LOG_SYNCPT		0x4000
#define LOG_MOUNT		0x2000
#define LOG_REDOPAGE		0x0800
#define LOG_NOREDOPAGE		0x0080
#define LOG_NOREDOINOEXT	0x0040
#define LOG_UPDATEMAP		0x0008
#define LOG_NOREDOFILE		0x0001

#define	LOG_INODE		0x0001
#define	LOG_XTREE		0x0002
#define	LOG_DTREE		0x0004
#define	LOG_BTROOT		0x0010
#define	LOG_EA			0x0020
#define	LOG_ACL			0x0040
#define	LOG_DATA		0x0080
#define	LOG_NEW			0x0100
#define	LOG_EXTEND		0x0200
#define LOG_RELOCATE		0x0400
#define LOG_DIR_XTREE		0x0800	

#define	LOG_ALLOCXADLIST	0x0080
#define	LOG_ALLOCPXDLIST	0x0040
#define	LOG_ALLOCXAD		0x0020
#define	LOG_ALLOCPXD		0x0010
#define	LOG_FREEXADLIST		0x0008
#define	LOG_FREEPXDLIST		0x0004
#define	LOG_FREEXAD		0x0002
#define	LOG_FREEPXD		0x0001


struct lrd {
	__le32 logtid;		
	__le32 backchain;	
	__le16 type;		
	__le16 length;		
	__le32 aggregate;	
	

	union {


		struct {
			__le32 fileset;	
			__le32 inode;	
			__le16 type;	
			__le16 l2linesize;	
			pxd_t pxd;	
		} redopage;	

		struct {
			__le32 fileset;	
			__le32 inode;	
			__le16 type;	
			__le16 rsrvd;	
			pxd_t pxd;	
		} noredopage;	

		struct {
			__le32 fileset;	
			__le32 inode;	
			__le16 type;	
			__le16 nxd;	
			pxd_t pxd;	
		} updatemap;	

		struct {
			__le32 fileset;	
			__le32 iagnum;	
			__le32 inoext_idx;	
			pxd_t pxd;	
		} noredoinoext;	

		struct {
			__le32 sync;	
		} syncpt;


		struct {
			__le32 type;	
			__le32 nextent;	

			
		} freextent;

		/*
		 *	? NOREDOFILE: this file is freed
		 *
		 * do not apply records which precede this record in the log
		 * with the same inode number.
		 *
		 * NOREDOFILE must be the first to be written at commit
		 * (last to be read in logredo()) - it prevents
		 * replay of preceding updates of all preceding generations
		 * of the inumber esp. the on-disk inode itself.
		 */
		struct {
			__le32 fileset;	
			__le32 inode;	
		} noredofile;

		struct {
			__le32 fileset;	
			__le32 inode;	
			__le32 type;	
			pxd_t pxd;	
		} newpage;

	} log;
};					

#define	LOGRDSIZE	(sizeof(struct lrd))

struct lvd {
	__le16 offset;
	__le16 length;
};


struct jfs_log {

	struct list_head sb_list;
	struct list_head journal_list; 
	struct block_device *bdev; 
	int serial;		

	s64 base;		
	int size;		
	int l2bsize;		

	unsigned long flag;	

	struct lbuf *lbuf_free;	
	wait_queue_head_t free_wait;	

	
	int logtid;		
	int page;		
	int eor;		
	struct lbuf *bp;	

	struct mutex loglock;	

	
	int nextsync;		
	int active;		
	wait_queue_head_t syncwait;	

	
	uint cflag;		
	struct list_head cqueue; 
	struct tblock *flush_tblk; 
	int gcrtc;		
	struct tblock *gclrt;	
	spinlock_t gclock;	
	int logsize;		
	int lsn;		
	int clsn;		
	int syncpt;		
	int sync;		
	struct list_head synclist;	
	spinlock_t synclock;	
	struct lbuf *wqueue;	
	int count;		
	char uuid[16];		

	int no_integrity;	
};

#define log_INLINELOG	1
#define log_SYNCBARRIER	2
#define log_QUIESCE	3
#define log_FLUSH	4

#define logGC_PAGEOUT	0x00000001

#define tblkGC_QUEUE		0x0001
#define tblkGC_READY		0x0002
#define tblkGC_COMMIT		0x0004
#define tblkGC_COMMITTED	0x0008
#define tblkGC_EOP		0x0010
#define tblkGC_FREE		0x0020
#define tblkGC_LEADER		0x0040
#define tblkGC_ERROR		0x0080
#define tblkGC_LAZY		0x0100	
#define tblkGC_UNLOCKED		0x0200	

struct lbuf {
	struct jfs_log *l_log;	

	uint l_flag;		

	struct lbuf *l_wqnext;	
	struct lbuf *l_freelist;	

	int l_pn;		
	int l_eor;		
	int l_ceor;		

	s64 l_blkno;		
	caddr_t l_ldata;	
	struct page *l_page;	
	uint l_offset;		

	wait_queue_head_t l_ioevent;	
};

#define l_redrive_next l_freelist

struct logsyncblk {
	u16 xflag;		
	u16 flag;		
	lid_t lid;		
	s32 lsn;		
	struct list_head synclist;	
};


#define LOGSYNC_LOCK_INIT(log) spin_lock_init(&(log)->synclock)
#define LOGSYNC_LOCK(log, flags) spin_lock_irqsave(&(log)->synclock, flags)
#define LOGSYNC_UNLOCK(log, flags) \
	spin_unlock_irqrestore(&(log)->synclock, flags)

#define logdiff(diff, lsn, log)\
{\
	diff = (lsn) - (log)->syncpt;\
	if (diff < 0)\
		diff += (log)->logsize;\
}

extern int lmLogOpen(struct super_block *sb);
extern int lmLogClose(struct super_block *sb);
extern int lmLogShutdown(struct jfs_log * log);
extern int lmLogInit(struct jfs_log * log);
extern int lmLogFormat(struct jfs_log *log, s64 logAddress, int logSize);
extern int lmGroupCommit(struct jfs_log *, struct tblock *);
extern int jfsIOWait(void *);
extern void jfs_flush_journal(struct jfs_log * log, int wait);
extern void jfs_syncpt(struct jfs_log *log, int hard_sync);

#endif				
