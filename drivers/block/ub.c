/*
 * The low performance USB storage driver (ub).
 *
 * Copyright (c) 1999, 2000 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 * Copyright (C) 2004 Pete Zaitcev (zaitcev@yahoo.com)
 *
 * This work is a part of Linux kernel, is derived from it,
 * and is not licensed separately. See file COPYING for details.
 *
 * TODO (sorted by decreasing priority)
 *  -- Return sense now that rq allows it (we always auto-sense anyway).
 *  -- set readonly flag for CDs, set removable flag for CF readers
 *  -- do inquiry and verify we got a disk and not a tape (for LUN mismatch)
 *  -- verify the 13 conditions and do bulk resets
 *  -- highmem
 *  -- move top_sense and work_bcs into separate allocations (if they survive)
 *     for cache purists and esoteric architectures.
 *  -- Allocate structure for LUN 0 before the first ub_sync_tur, avoid NULL. ?
 *  -- prune comments, they are too volumnous
 *  -- Resove XXX's
 *  -- CLEAR, CLR2STS, CLRRS seem to be ripe for refactoring.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb_usual.h>
#include <linux/blkdev.h>
#include <linux/timer.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <scsi/scsi.h>

#define DRV_NAME "ub"

#define UB_MAJOR 180


#define UB_MAX_LUNS   9


#define UB_PARTS_PER_LUN      8

#define UB_MAX_CDB_SIZE      16		

#define UB_SENSE_SIZE  18

struct ub_dev;

#define UB_MAX_REQ_SG	9	
#define UB_MAX_SECTORS 64

#define UB_URB_TIMEOUT	(HZ*2)
#define UB_DATA_TIMEOUT	(HZ*5)	
#define UB_STAT_TIMEOUT	(HZ*5)	
#define UB_CTRL_TIMEOUT	(HZ/2)	

#define UB_DIR_NONE	0
#define UB_DIR_READ	1
#define UB_DIR_ILLEGAL2	2
#define UB_DIR_WRITE	3

#define UB_DIR_CHAR(c)  (((c)==UB_DIR_WRITE)? 'w': \
			 (((c)==UB_DIR_READ)? 'r': 'n'))

enum ub_scsi_cmd_state {
	UB_CMDST_INIT,			
	UB_CMDST_CMD,			
	UB_CMDST_DATA,			
	UB_CMDST_CLR2STS,		
	UB_CMDST_STAT,			
	UB_CMDST_CLEAR,			
	UB_CMDST_CLRRS,			
	UB_CMDST_SENSE,			
	UB_CMDST_DONE			
};

struct ub_scsi_cmd {
	unsigned char cdb[UB_MAX_CDB_SIZE];
	unsigned char cdb_len;

	unsigned char dir;		
	enum ub_scsi_cmd_state state;
	unsigned int tag;
	struct ub_scsi_cmd *next;

	int error;			
	unsigned int act_len;		
	unsigned char key, asc, ascq;	

	int stat_count;			
	unsigned int timeo;		

	unsigned int len;		
	unsigned int current_sg;
	unsigned int nsg;		
	struct scatterlist sgv[UB_MAX_REQ_SG];

	struct ub_lun *lun;
	void (*done)(struct ub_dev *, struct ub_scsi_cmd *);
	void *back;
};

struct ub_request {
	struct request *rq;
	unsigned int current_try;
	unsigned int nsg;		
	struct scatterlist sgv[UB_MAX_REQ_SG];
};

struct ub_capacity {
	unsigned long nsec;		
	unsigned int bsize;		
	unsigned int bshift;		
};

struct ub_completion {
	unsigned int done;
	spinlock_t lock;
};

static DEFINE_MUTEX(ub_mutex);
static inline void ub_init_completion(struct ub_completion *x)
{
	x->done = 0;
	spin_lock_init(&x->lock);
}

#define UB_INIT_COMPLETION(x)	((x).done = 0)

static void ub_complete(struct ub_completion *x)
{
	unsigned long flags;

	spin_lock_irqsave(&x->lock, flags);
	x->done++;
	spin_unlock_irqrestore(&x->lock, flags);
}

static int ub_is_completed(struct ub_completion *x)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&x->lock, flags);
	ret = x->done;
	spin_unlock_irqrestore(&x->lock, flags);
	return ret;
}

struct ub_scsi_cmd_queue {
	int qlen, qmax;
	struct ub_scsi_cmd *head, *tail;
};

struct ub_lun {
	struct ub_dev *udev;
	struct list_head link;
	struct gendisk *disk;
	int id;				
	int num;			
	char name[16];

	int changed;			
	int removable;
	int readonly;

	struct ub_request urq;

	
	int cmda[1];
	struct ub_scsi_cmd cmdv[1];

	struct ub_capacity capacity; 
};

struct ub_dev {
	spinlock_t *lock;
	atomic_t poison;		
	int openc;			
					
	int reset;			
	int bad_resid;
	unsigned int tagcnt;
	char name[12];
	struct usb_device *dev;
	struct usb_interface *intf;

	struct list_head luns;

	unsigned int send_bulk_pipe;	
	unsigned int recv_bulk_pipe;
	unsigned int send_ctrl_pipe;
	unsigned int recv_ctrl_pipe;

	struct tasklet_struct tasklet;

	struct ub_scsi_cmd_queue cmd_queue;
	struct ub_scsi_cmd top_rqs_cmd;	
	unsigned char top_sense[UB_SENSE_SIZE];

	struct ub_completion work_done;
	struct urb work_urb;
	struct timer_list work_timer;
	int last_pipe;			
	__le32 signature;		
	struct bulk_cb_wrap work_bcb;
	struct bulk_cs_wrap work_bcs;
	struct usb_ctrlrequest work_cr;

	struct work_struct reset_work;
	wait_queue_head_t reset_wait;
};

static void ub_cleanup(struct ub_dev *sc);
static int ub_request_fn_1(struct ub_lun *lun, struct request *rq);
static void ub_cmd_build_block(struct ub_dev *sc, struct ub_lun *lun,
    struct ub_scsi_cmd *cmd, struct ub_request *urq);
static void ub_cmd_build_packet(struct ub_dev *sc, struct ub_lun *lun,
    struct ub_scsi_cmd *cmd, struct ub_request *urq);
static void ub_rw_cmd_done(struct ub_dev *sc, struct ub_scsi_cmd *cmd);
static void ub_end_rq(struct request *rq, unsigned int status);
static int ub_rw_cmd_retry(struct ub_dev *sc, struct ub_lun *lun,
    struct ub_request *urq, struct ub_scsi_cmd *cmd);
static int ub_submit_scsi(struct ub_dev *sc, struct ub_scsi_cmd *cmd);
static void ub_urb_complete(struct urb *urb);
static void ub_scsi_action(unsigned long _dev);
static void ub_scsi_dispatch(struct ub_dev *sc);
static void ub_scsi_urb_compl(struct ub_dev *sc, struct ub_scsi_cmd *cmd);
static void ub_data_start(struct ub_dev *sc, struct ub_scsi_cmd *cmd);
static void ub_state_done(struct ub_dev *sc, struct ub_scsi_cmd *cmd, int rc);
static int __ub_state_stat(struct ub_dev *sc, struct ub_scsi_cmd *cmd);
static void ub_state_stat(struct ub_dev *sc, struct ub_scsi_cmd *cmd);
static void ub_state_stat_counted(struct ub_dev *sc, struct ub_scsi_cmd *cmd);
static void ub_state_sense(struct ub_dev *sc, struct ub_scsi_cmd *cmd);
static int ub_submit_clear_stall(struct ub_dev *sc, struct ub_scsi_cmd *cmd,
    int stalled_pipe);
static void ub_top_sense_done(struct ub_dev *sc, struct ub_scsi_cmd *scmd);
static void ub_reset_enter(struct ub_dev *sc, int try);
static void ub_reset_task(struct work_struct *work);
static int ub_sync_tur(struct ub_dev *sc, struct ub_lun *lun);
static int ub_sync_read_cap(struct ub_dev *sc, struct ub_lun *lun,
    struct ub_capacity *ret);
static int ub_sync_reset(struct ub_dev *sc);
static int ub_probe_clear_stall(struct ub_dev *sc, int stalled_pipe);
static int ub_probe_lun(struct ub_dev *sc, int lnum);

#ifdef CONFIG_USB_LIBUSUAL

#define ub_usb_ids  usb_storage_usb_ids
#else

static const struct usb_device_id ub_usb_ids[] = {
	{ USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, USB_SC_SCSI, USB_PR_BULK) },
	{ }
};

MODULE_DEVICE_TABLE(usb, ub_usb_ids);
#endif 

#define UB_MAX_HOSTS  26
static char ub_hostv[UB_MAX_HOSTS];

#define UB_QLOCK_NUM 5
static spinlock_t ub_qlockv[UB_QLOCK_NUM];
static int ub_qlock_next = 0;

static DEFINE_SPINLOCK(ub_lock);	

static int ub_id_get(void)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&ub_lock, flags);
	for (i = 0; i < UB_MAX_HOSTS; i++) {
		if (ub_hostv[i] == 0) {
			ub_hostv[i] = 1;
			spin_unlock_irqrestore(&ub_lock, flags);
			return i;
		}
	}
	spin_unlock_irqrestore(&ub_lock, flags);
	return -1;
}

static void ub_id_put(int id)
{
	unsigned long flags;

	if (id < 0 || id >= UB_MAX_HOSTS) {
		printk(KERN_ERR DRV_NAME ": bad host ID %d\n", id);
		return;
	}

	spin_lock_irqsave(&ub_lock, flags);
	if (ub_hostv[id] == 0) {
		spin_unlock_irqrestore(&ub_lock, flags);
		printk(KERN_ERR DRV_NAME ": freeing free host ID %d\n", id);
		return;
	}
	ub_hostv[id] = 0;
	spin_unlock_irqrestore(&ub_lock, flags);
}

static spinlock_t *ub_next_lock(void)
{
	unsigned long flags;
	spinlock_t *ret;

	spin_lock_irqsave(&ub_lock, flags);
	ret = &ub_qlockv[ub_qlock_next];
	ub_qlock_next = (ub_qlock_next + 1) % UB_QLOCK_NUM;
	spin_unlock_irqrestore(&ub_lock, flags);
	return ret;
}

static void ub_put(struct ub_dev *sc)
{
	unsigned long flags;

	spin_lock_irqsave(&ub_lock, flags);
	--sc->openc;
	if (sc->openc == 0 && atomic_read(&sc->poison)) {
		spin_unlock_irqrestore(&ub_lock, flags);
		ub_cleanup(sc);
	} else {
		spin_unlock_irqrestore(&ub_lock, flags);
	}
}

static void ub_cleanup(struct ub_dev *sc)
{
	struct list_head *p;
	struct ub_lun *lun;
	struct request_queue *q;

	while (!list_empty(&sc->luns)) {
		p = sc->luns.next;
		lun = list_entry(p, struct ub_lun, link);
		list_del(p);

		
		if ((q = lun->disk->queue) != NULL)
			blk_cleanup_queue(q);
		
		put_disk(lun->disk);
		lun->disk = NULL;

		ub_id_put(lun->id);
		kfree(lun);
	}

	usb_set_intfdata(sc->intf, NULL);
	usb_put_intf(sc->intf);
	usb_put_dev(sc->dev);
	kfree(sc);
}

static struct ub_scsi_cmd *ub_get_cmd(struct ub_lun *lun)
{
	struct ub_scsi_cmd *ret;

	if (lun->cmda[0])
		return NULL;
	ret = &lun->cmdv[0];
	lun->cmda[0] = 1;
	return ret;
}

static void ub_put_cmd(struct ub_lun *lun, struct ub_scsi_cmd *cmd)
{
	if (cmd != &lun->cmdv[0]) {
		printk(KERN_WARNING "%s: releasing a foreign cmd %p\n",
		    lun->name, cmd);
		return;
	}
	if (!lun->cmda[0]) {
		printk(KERN_WARNING "%s: releasing a free cmd\n", lun->name);
		return;
	}
	lun->cmda[0] = 0;
}

static void ub_cmdq_add(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{
	struct ub_scsi_cmd_queue *t = &sc->cmd_queue;

	if (t->qlen++ == 0) {
		t->head = cmd;
		t->tail = cmd;
	} else {
		t->tail->next = cmd;
		t->tail = cmd;
	}

	if (t->qlen > t->qmax)
		t->qmax = t->qlen;
}

static void ub_cmdq_insert(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{
	struct ub_scsi_cmd_queue *t = &sc->cmd_queue;

	if (t->qlen++ == 0) {
		t->head = cmd;
		t->tail = cmd;
	} else {
		cmd->next = t->head;
		t->head = cmd;
	}

	if (t->qlen > t->qmax)
		t->qmax = t->qlen;
}

static struct ub_scsi_cmd *ub_cmdq_pop(struct ub_dev *sc)
{
	struct ub_scsi_cmd_queue *t = &sc->cmd_queue;
	struct ub_scsi_cmd *cmd;

	if (t->qlen == 0)
		return NULL;
	if (--t->qlen == 0)
		t->tail = NULL;
	cmd = t->head;
	t->head = cmd->next;
	cmd->next = NULL;
	return cmd;
}

#define ub_cmdq_peek(sc)  ((sc)->cmd_queue.head)


static void ub_request_fn(struct request_queue *q)
{
	struct ub_lun *lun = q->queuedata;
	struct request *rq;

	while ((rq = blk_peek_request(q)) != NULL) {
		if (ub_request_fn_1(lun, rq) != 0) {
			blk_stop_queue(q);
			break;
		}
	}
}

static int ub_request_fn_1(struct ub_lun *lun, struct request *rq)
{
	struct ub_dev *sc = lun->udev;
	struct ub_scsi_cmd *cmd;
	struct ub_request *urq;
	int n_elem;

	if (atomic_read(&sc->poison)) {
		blk_start_request(rq);
		ub_end_rq(rq, DID_NO_CONNECT << 16);
		return 0;
	}

	if (lun->changed && rq->cmd_type != REQ_TYPE_BLOCK_PC) {
		blk_start_request(rq);
		ub_end_rq(rq, SAM_STAT_CHECK_CONDITION);
		return 0;
	}

	if (lun->urq.rq != NULL)
		return -1;
	if ((cmd = ub_get_cmd(lun)) == NULL)
		return -1;
	memset(cmd, 0, sizeof(struct ub_scsi_cmd));

	blk_start_request(rq);

	urq = &lun->urq;
	memset(urq, 0, sizeof(struct ub_request));
	urq->rq = rq;

	sg_init_table(&urq->sgv[0], UB_MAX_REQ_SG);
	n_elem = blk_rq_map_sg(lun->disk->queue, rq, &urq->sgv[0]);
	if (n_elem < 0) {
		
		printk(KERN_INFO "%s: failed request map (%d)\n",
		    lun->name, n_elem);
		goto drop;
	}
	if (n_elem > UB_MAX_REQ_SG) {	
		printk(KERN_WARNING "%s: request with %d segments\n",
		    lun->name, n_elem);
		goto drop;
	}
	urq->nsg = n_elem;

	if (rq->cmd_type == REQ_TYPE_BLOCK_PC) {
		ub_cmd_build_packet(sc, lun, cmd, urq);
	} else {
		ub_cmd_build_block(sc, lun, cmd, urq);
	}
	cmd->state = UB_CMDST_INIT;
	cmd->lun = lun;
	cmd->done = ub_rw_cmd_done;
	cmd->back = urq;

	cmd->tag = sc->tagcnt++;
	if (ub_submit_scsi(sc, cmd) != 0)
		goto drop;

	return 0;

drop:
	ub_put_cmd(lun, cmd);
	ub_end_rq(rq, DID_ERROR << 16);
	return 0;
}

static void ub_cmd_build_block(struct ub_dev *sc, struct ub_lun *lun,
    struct ub_scsi_cmd *cmd, struct ub_request *urq)
{
	struct request *rq = urq->rq;
	unsigned int block, nblks;

	if (rq_data_dir(rq) == WRITE)
		cmd->dir = UB_DIR_WRITE;
	else
		cmd->dir = UB_DIR_READ;

	cmd->nsg = urq->nsg;
	memcpy(cmd->sgv, urq->sgv, sizeof(struct scatterlist) * cmd->nsg);

	block = blk_rq_pos(rq) >> lun->capacity.bshift;
	nblks = blk_rq_sectors(rq) >> lun->capacity.bshift;

	cmd->cdb[0] = (cmd->dir == UB_DIR_READ)? READ_10: WRITE_10;
	
	cmd->cdb[2] = block >> 24;
	cmd->cdb[3] = block >> 16;
	cmd->cdb[4] = block >> 8;
	cmd->cdb[5] = block;
	cmd->cdb[7] = nblks >> 8;
	cmd->cdb[8] = nblks;
	cmd->cdb_len = 10;

	cmd->len = blk_rq_bytes(rq);
}

static void ub_cmd_build_packet(struct ub_dev *sc, struct ub_lun *lun,
    struct ub_scsi_cmd *cmd, struct ub_request *urq)
{
	struct request *rq = urq->rq;

	if (blk_rq_bytes(rq) == 0) {
		cmd->dir = UB_DIR_NONE;
	} else {
		if (rq_data_dir(rq) == WRITE)
			cmd->dir = UB_DIR_WRITE;
		else
			cmd->dir = UB_DIR_READ;
	}

	cmd->nsg = urq->nsg;
	memcpy(cmd->sgv, urq->sgv, sizeof(struct scatterlist) * cmd->nsg);

	memcpy(&cmd->cdb, rq->cmd, rq->cmd_len);
	cmd->cdb_len = rq->cmd_len;

	cmd->len = blk_rq_bytes(rq);

	cmd->timeo = rq->timeout;
}

static void ub_rw_cmd_done(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{
	struct ub_lun *lun = cmd->lun;
	struct ub_request *urq = cmd->back;
	struct request *rq;
	unsigned int scsi_status;

	rq = urq->rq;

	if (cmd->error == 0) {
		if (rq->cmd_type == REQ_TYPE_BLOCK_PC) {
			if (cmd->act_len >= rq->resid_len)
				rq->resid_len = 0;
			else
				rq->resid_len -= cmd->act_len;
			scsi_status = 0;
		} else {
			if (cmd->act_len != cmd->len) {
				scsi_status = SAM_STAT_CHECK_CONDITION;
			} else {
				scsi_status = 0;
			}
		}
	} else {
		if (rq->cmd_type == REQ_TYPE_BLOCK_PC) {
			
			memcpy(rq->sense, sc->top_sense, UB_SENSE_SIZE);
			rq->sense_len = UB_SENSE_SIZE;
			if (sc->top_sense[0] != 0)
				scsi_status = SAM_STAT_CHECK_CONDITION;
			else
				scsi_status = DID_ERROR << 16;
		} else {
			if (cmd->error == -EIO &&
			    (cmd->key == 0 ||
			     cmd->key == MEDIUM_ERROR ||
			     cmd->key == UNIT_ATTENTION)) {
				if (ub_rw_cmd_retry(sc, lun, urq, cmd) == 0)
					return;
			}
			scsi_status = SAM_STAT_CHECK_CONDITION;
		}
	}

	urq->rq = NULL;

	ub_put_cmd(lun, cmd);
	ub_end_rq(rq, scsi_status);
	blk_start_queue(lun->disk->queue);
}

static void ub_end_rq(struct request *rq, unsigned int scsi_status)
{
	int error;

	if (scsi_status == 0) {
		error = 0;
	} else {
		error = -EIO;
		rq->errors = scsi_status;
	}
	__blk_end_request_all(rq, error);
}

static int ub_rw_cmd_retry(struct ub_dev *sc, struct ub_lun *lun,
    struct ub_request *urq, struct ub_scsi_cmd *cmd)
{

	if (atomic_read(&sc->poison))
		return -ENXIO;

	ub_reset_enter(sc, urq->current_try);

	if (urq->current_try >= 3)
		return -EIO;
	urq->current_try++;

	
	printk(KERN_DEBUG "%s: dir %c len/act %d/%d "
	    "[sense %x %02x %02x] retry %d\n",
	    sc->name, UB_DIR_CHAR(cmd->dir), cmd->len, cmd->act_len,
	    cmd->key, cmd->asc, cmd->ascq, urq->current_try);

	memset(cmd, 0, sizeof(struct ub_scsi_cmd));
	ub_cmd_build_block(sc, lun, cmd, urq);

	cmd->state = UB_CMDST_INIT;
	cmd->lun = lun;
	cmd->done = ub_rw_cmd_done;
	cmd->back = urq;

	cmd->tag = sc->tagcnt++;

#if 0 
	return ub_submit_scsi(sc, cmd);
#else
	ub_cmdq_add(sc, cmd);
	return 0;
#endif
}

static int ub_submit_scsi(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{

	if (cmd->state != UB_CMDST_INIT ||
	    (cmd->dir != UB_DIR_NONE && cmd->len == 0)) {
		return -EINVAL;
	}

	ub_cmdq_add(sc, cmd);
	tasklet_schedule(&sc->tasklet);
	return 0;
}

static int ub_scsi_cmd_start(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{
	struct bulk_cb_wrap *bcb;
	int rc;

	bcb = &sc->work_bcb;

	memset(&sc->top_sense, 0, UB_SENSE_SIZE);

	
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = cmd->tag;		
	bcb->DataTransferLength = cpu_to_le32(cmd->len);
	bcb->Flags = (cmd->dir == UB_DIR_READ) ? 0x80 : 0;
	bcb->Lun = (cmd->lun != NULL) ? cmd->lun->num : 0;
	bcb->Length = cmd->cdb_len;

	
	memcpy(bcb->CDB, cmd->cdb, UB_MAX_CDB_SIZE);

	UB_INIT_COMPLETION(sc->work_done);

	sc->last_pipe = sc->send_bulk_pipe;
	usb_fill_bulk_urb(&sc->work_urb, sc->dev, sc->send_bulk_pipe,
	    bcb, US_BULK_CB_WRAP_LEN, ub_urb_complete, sc);

	if ((rc = usb_submit_urb(&sc->work_urb, GFP_ATOMIC)) != 0) {
		
		ub_complete(&sc->work_done);
		return rc;
	}

	sc->work_timer.expires = jiffies + UB_URB_TIMEOUT;
	add_timer(&sc->work_timer);

	cmd->state = UB_CMDST_CMD;
	return 0;
}

static void ub_urb_timeout(unsigned long arg)
{
	struct ub_dev *sc = (struct ub_dev *) arg;
	unsigned long flags;

	spin_lock_irqsave(sc->lock, flags);
	if (!ub_is_completed(&sc->work_done))
		usb_unlink_urb(&sc->work_urb);
	spin_unlock_irqrestore(sc->lock, flags);
}

static void ub_urb_complete(struct urb *urb)
{
	struct ub_dev *sc = urb->context;

	ub_complete(&sc->work_done);
	tasklet_schedule(&sc->tasklet);
}

static void ub_scsi_action(unsigned long _dev)
{
	struct ub_dev *sc = (struct ub_dev *) _dev;
	unsigned long flags;

	spin_lock_irqsave(sc->lock, flags);
	ub_scsi_dispatch(sc);
	spin_unlock_irqrestore(sc->lock, flags);
}

static void ub_scsi_dispatch(struct ub_dev *sc)
{
	struct ub_scsi_cmd *cmd;
	int rc;

	while (!sc->reset && (cmd = ub_cmdq_peek(sc)) != NULL) {
		if (cmd->state == UB_CMDST_DONE) {
			ub_cmdq_pop(sc);
			(*cmd->done)(sc, cmd);
		} else if (cmd->state == UB_CMDST_INIT) {
			if ((rc = ub_scsi_cmd_start(sc, cmd)) == 0)
				break;
			cmd->error = rc;
			cmd->state = UB_CMDST_DONE;
		} else {
			if (!ub_is_completed(&sc->work_done))
				break;
			del_timer(&sc->work_timer);
			ub_scsi_urb_compl(sc, cmd);
		}
	}
}

static void ub_scsi_urb_compl(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{
	struct urb *urb = &sc->work_urb;
	struct bulk_cs_wrap *bcs;
	int endp;
	int len;
	int rc;

	if (atomic_read(&sc->poison)) {
		ub_state_done(sc, cmd, -ENODEV);
		return;
	}

	endp = usb_pipeendpoint(sc->last_pipe);
	if (usb_pipein(sc->last_pipe))
		endp |= USB_DIR_IN;

	if (cmd->state == UB_CMDST_CLEAR) {
		if (urb->status == -EPIPE) {
			printk(KERN_NOTICE "%s: stall on control pipe\n",
			    sc->name);
			goto Bad_End;
		}


		usb_reset_endpoint(sc->dev, endp);

		ub_state_sense(sc, cmd);

	} else if (cmd->state == UB_CMDST_CLR2STS) {
		if (urb->status == -EPIPE) {
			printk(KERN_NOTICE "%s: stall on control pipe\n",
			    sc->name);
			goto Bad_End;
		}


		usb_reset_endpoint(sc->dev, endp);

		ub_state_stat(sc, cmd);

	} else if (cmd->state == UB_CMDST_CLRRS) {
		if (urb->status == -EPIPE) {
			printk(KERN_NOTICE "%s: stall on control pipe\n",
			    sc->name);
			goto Bad_End;
		}


		usb_reset_endpoint(sc->dev, endp);

		ub_state_stat_counted(sc, cmd);

	} else if (cmd->state == UB_CMDST_CMD) {
		switch (urb->status) {
		case 0:
			break;
		case -EOVERFLOW:
			goto Bad_End;
		case -EPIPE:
			rc = ub_submit_clear_stall(sc, cmd, sc->last_pipe);
			if (rc != 0) {
				printk(KERN_NOTICE "%s: "
				    "unable to submit clear (%d)\n",
				    sc->name, rc);
				ub_state_done(sc, cmd, rc);
				return;
			}
			cmd->state = UB_CMDST_CLEAR;
			return;
		case -ESHUTDOWN:	
		case -EILSEQ:		
			ub_state_done(sc, cmd, -ENODEV);
			return;
		default:
			goto Bad_End;
		}
		if (urb->actual_length != US_BULK_CB_WRAP_LEN) {
			goto Bad_End;
		}

		if (cmd->dir == UB_DIR_NONE || cmd->nsg < 1) {
			ub_state_stat(sc, cmd);
			return;
		}

		
		ub_data_start(sc, cmd);

	} else if (cmd->state == UB_CMDST_DATA) {
		if (urb->status == -EPIPE) {
			rc = ub_submit_clear_stall(sc, cmd, sc->last_pipe);
			if (rc != 0) {
				printk(KERN_NOTICE "%s: "
				    "unable to submit clear (%d)\n",
				    sc->name, rc);
				ub_state_done(sc, cmd, rc);
				return;
			}
			cmd->state = UB_CMDST_CLR2STS;
			return;
		}
		if (urb->status == -EOVERFLOW) {
			cmd->error = -EOVERFLOW;	
			ub_state_stat(sc, cmd);
			return;
		}

		if (cmd->dir == UB_DIR_WRITE) {
			len = urb->actual_length;
			if (urb->status != 0 ||
			    len != cmd->sgv[cmd->current_sg].length) {
				cmd->act_len += len;

				cmd->error = -EIO;
				ub_state_stat(sc, cmd);
				return;
			}

		} else {
			if (urb->status != 0)
				cmd->error = -EIO;

			len = urb->actual_length;
			if (urb->status != 0 ||
			    len != cmd->sgv[cmd->current_sg].length) {
				if ((len & 0x1FF) == US_BULK_CS_WRAP_LEN)
					goto Bad_End;
			}
		}

		cmd->act_len += urb->actual_length;

		if (++cmd->current_sg < cmd->nsg) {
			ub_data_start(sc, cmd);
			return;
		}
		ub_state_stat(sc, cmd);

	} else if (cmd->state == UB_CMDST_STAT) {
		if (urb->status == -EPIPE) {
			rc = ub_submit_clear_stall(sc, cmd, sc->last_pipe);
			if (rc != 0) {
				printk(KERN_NOTICE "%s: "
				    "unable to submit clear (%d)\n",
				    sc->name, rc);
				ub_state_done(sc, cmd, rc);
				return;
			}

			cmd->error = -EIO;		

			cmd->state = UB_CMDST_CLRRS;
			return;
		}

		
		if (urb->status != 0)
			goto Bad_End;

		if (urb->actual_length == 0) {
			ub_state_stat_counted(sc, cmd);
			return;
		}


		bcs = &sc->work_bcs;

		if (sc->signature == cpu_to_le32(0)) {
			sc->signature = bcs->Signature;
			if (sc->signature == cpu_to_le32(0)) {
				ub_state_stat_counted(sc, cmd);
				return;
			}
		} else {
			if (bcs->Signature != sc->signature) {
				ub_state_stat_counted(sc, cmd);
				return;
			}
		}

		if (bcs->Tag != cmd->tag) {
			ub_state_stat_counted(sc, cmd);
			return;
		}

		if (!sc->bad_resid) {
			len = le32_to_cpu(bcs->Residue);
			if (len != cmd->len - cmd->act_len) {
				if (cmd->len == cmd->act_len) {
					printk(KERN_NOTICE "%s: "
					    "bad residual %d of %d, ignoring\n",
					    sc->name, len, cmd->len);
					sc->bad_resid = 1;
				}
			}
		}

		switch (bcs->Status) {
		case US_BULK_STAT_OK:
			break;
		case US_BULK_STAT_FAIL:
			ub_state_sense(sc, cmd);
			return;
		case US_BULK_STAT_PHASE:
			goto Bad_End;
		default:
			printk(KERN_INFO "%s: unknown CSW status 0x%x\n",
			    sc->name, bcs->Status);
			ub_state_done(sc, cmd, -EINVAL);
			return;
		}

		
		if (cmd->error != 0) {
			ub_state_sense(sc, cmd);
			return;
		}
		cmd->state = UB_CMDST_DONE;
		ub_cmdq_pop(sc);
		(*cmd->done)(sc, cmd);

	} else if (cmd->state == UB_CMDST_SENSE) {
		ub_state_done(sc, cmd, -EIO);

	} else {
		printk(KERN_WARNING "%s: wrong command state %d\n",
		    sc->name, cmd->state);
		ub_state_done(sc, cmd, -EINVAL);
		return;
	}
	return;

Bad_End: 
	ub_state_done(sc, cmd, -EIO);
}

static void ub_data_start(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{
	struct scatterlist *sg = &cmd->sgv[cmd->current_sg];
	int pipe;
	int rc;

	UB_INIT_COMPLETION(sc->work_done);

	if (cmd->dir == UB_DIR_READ)
		pipe = sc->recv_bulk_pipe;
	else
		pipe = sc->send_bulk_pipe;
	sc->last_pipe = pipe;
	usb_fill_bulk_urb(&sc->work_urb, sc->dev, pipe, sg_virt(sg),
	    sg->length, ub_urb_complete, sc);

	if ((rc = usb_submit_urb(&sc->work_urb, GFP_ATOMIC)) != 0) {
		
		ub_complete(&sc->work_done);
		ub_state_done(sc, cmd, rc);
		return;
	}

	if (cmd->timeo)
		sc->work_timer.expires = jiffies + cmd->timeo;
	else
		sc->work_timer.expires = jiffies + UB_DATA_TIMEOUT;
	add_timer(&sc->work_timer);

	cmd->state = UB_CMDST_DATA;
}

static void ub_state_done(struct ub_dev *sc, struct ub_scsi_cmd *cmd, int rc)
{

	cmd->error = rc;
	cmd->state = UB_CMDST_DONE;
	ub_cmdq_pop(sc);
	(*cmd->done)(sc, cmd);
}

static int __ub_state_stat(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{
	int rc;

	UB_INIT_COMPLETION(sc->work_done);

	sc->last_pipe = sc->recv_bulk_pipe;
	usb_fill_bulk_urb(&sc->work_urb, sc->dev, sc->recv_bulk_pipe,
	    &sc->work_bcs, US_BULK_CS_WRAP_LEN, ub_urb_complete, sc);

	if ((rc = usb_submit_urb(&sc->work_urb, GFP_ATOMIC)) != 0) {
		
		ub_complete(&sc->work_done);
		ub_state_done(sc, cmd, rc);
		return -1;
	}

	if (cmd->timeo)
		sc->work_timer.expires = jiffies + cmd->timeo;
	else
		sc->work_timer.expires = jiffies + UB_STAT_TIMEOUT;
	add_timer(&sc->work_timer);
	return 0;
}

static void ub_state_stat(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{

	if (__ub_state_stat(sc, cmd) != 0)
		return;

	cmd->stat_count = 0;
	cmd->state = UB_CMDST_STAT;
}

static void ub_state_stat_counted(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{

	if (++cmd->stat_count >= 4) {
		ub_state_sense(sc, cmd);
		return;
	}

	if (__ub_state_stat(sc, cmd) != 0)
		return;

	cmd->state = UB_CMDST_STAT;
}

static void ub_state_sense(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{
	struct ub_scsi_cmd *scmd;
	struct scatterlist *sg;
	int rc;

	if (cmd->cdb[0] == REQUEST_SENSE) {
		rc = -EPIPE;
		goto error;
	}

	scmd = &sc->top_rqs_cmd;
	memset(scmd, 0, sizeof(struct ub_scsi_cmd));
	scmd->cdb[0] = REQUEST_SENSE;
	scmd->cdb[4] = UB_SENSE_SIZE;
	scmd->cdb_len = 6;
	scmd->dir = UB_DIR_READ;
	scmd->state = UB_CMDST_INIT;
	scmd->nsg = 1;
	sg = &scmd->sgv[0];
	sg_init_table(sg, UB_MAX_REQ_SG);
	sg_set_page(sg, virt_to_page(sc->top_sense), UB_SENSE_SIZE,
			(unsigned long)sc->top_sense & (PAGE_SIZE-1));
	scmd->len = UB_SENSE_SIZE;
	scmd->lun = cmd->lun;
	scmd->done = ub_top_sense_done;
	scmd->back = cmd;

	scmd->tag = sc->tagcnt++;

	cmd->state = UB_CMDST_SENSE;

	ub_cmdq_insert(sc, scmd);
	return;

error:
	ub_state_done(sc, cmd, rc);
}

static int ub_submit_clear_stall(struct ub_dev *sc, struct ub_scsi_cmd *cmd,
    int stalled_pipe)
{
	int endp;
	struct usb_ctrlrequest *cr;
	int rc;

	endp = usb_pipeendpoint(stalled_pipe);
	if (usb_pipein (stalled_pipe))
		endp |= USB_DIR_IN;

	cr = &sc->work_cr;
	cr->bRequestType = USB_RECIP_ENDPOINT;
	cr->bRequest = USB_REQ_CLEAR_FEATURE;
	cr->wValue = cpu_to_le16(USB_ENDPOINT_HALT);
	cr->wIndex = cpu_to_le16(endp);
	cr->wLength = cpu_to_le16(0);

	UB_INIT_COMPLETION(sc->work_done);

	usb_fill_control_urb(&sc->work_urb, sc->dev, sc->send_ctrl_pipe,
	    (unsigned char*) cr, NULL, 0, ub_urb_complete, sc);

	if ((rc = usb_submit_urb(&sc->work_urb, GFP_ATOMIC)) != 0) {
		ub_complete(&sc->work_done);
		return rc;
	}

	sc->work_timer.expires = jiffies + UB_CTRL_TIMEOUT;
	add_timer(&sc->work_timer);
	return 0;
}

static void ub_top_sense_done(struct ub_dev *sc, struct ub_scsi_cmd *scmd)
{
	unsigned char *sense = sc->top_sense;
	struct ub_scsi_cmd *cmd;

	if ((cmd = ub_cmdq_peek(sc)) == NULL) {
		printk(KERN_WARNING "%s: sense done while idle\n", sc->name);
		return;
	}
	if (cmd != scmd->back) {
		printk(KERN_WARNING "%s: "
		    "sense done for wrong command 0x%x\n",
		    sc->name, cmd->tag);
		return;
	}
	if (cmd->state != UB_CMDST_SENSE) {
		printk(KERN_WARNING "%s: sense done with bad cmd state %d\n",
		    sc->name, cmd->state);
		return;
	}

	cmd->key = sense[2] & 0x0F;
	cmd->asc = sense[12];
	cmd->ascq = sense[13];

	ub_scsi_urb_compl(sc, cmd);
}


static void ub_reset_enter(struct ub_dev *sc, int try)
{

	if (sc->reset) {
		
		return;
	}
	sc->reset = try + 1;

#if 0 
	unsigned long flags;
	spin_lock_irqsave(&ub_lock, flags);
	sc->openc++;
	spin_unlock_irqrestore(&ub_lock, flags);
#endif

#if 0 
	struct ub_lun *lun;
	list_for_each_entry(lun, &sc->luns, link) {
		blk_stop_queue(lun->disk->queue);
	}
#endif

	schedule_work(&sc->reset_work);
}

static void ub_reset_task(struct work_struct *work)
{
	struct ub_dev *sc = container_of(work, struct ub_dev, reset_work);
	unsigned long flags;
	struct ub_lun *lun;
	int rc;

	if (!sc->reset) {
		printk(KERN_WARNING "%s: Running reset unrequested\n",
		    sc->name);
		return;
	}

	if (atomic_read(&sc->poison)) {
		;
	} else if ((sc->reset & 1) == 0) {
		ub_sync_reset(sc);
		msleep(700);	
		ub_probe_clear_stall(sc, sc->recv_bulk_pipe);
		ub_probe_clear_stall(sc, sc->send_bulk_pipe);
	} else if (sc->dev->actconfig->desc.bNumInterfaces != 1) {
		;
	} else {
		rc = usb_lock_device_for_reset(sc->dev, sc->intf);
		if (rc < 0) {
			printk(KERN_NOTICE
			    "%s: usb_lock_device_for_reset failed (%d)\n",
			    sc->name, rc);
		} else {
			rc = usb_reset_device(sc->dev);
			if (rc < 0) {
				printk(KERN_NOTICE "%s: "
				    "usb_lock_device_for_reset failed (%d)\n",
				    sc->name, rc);
			}
			usb_unlock_device(sc->dev);
		}
	}

	spin_lock_irqsave(sc->lock, flags);
	sc->reset = 0;
	tasklet_schedule(&sc->tasklet);
	list_for_each_entry(lun, &sc->luns, link) {
		blk_start_queue(lun->disk->queue);
	}
	wake_up(&sc->reset_wait);
	spin_unlock_irqrestore(sc->lock, flags);
}

static int ub_pre_reset(struct usb_interface *iface) {
	return 0;
}

static int ub_post_reset(struct usb_interface *iface) {
	return 0;
}

static void ub_revalidate(struct ub_dev *sc, struct ub_lun *lun)
{

	lun->readonly = 0;	

	lun->capacity.nsec = 0;
	lun->capacity.bsize = 512;
	lun->capacity.bshift = 0;

	if (ub_sync_tur(sc, lun) != 0)
		return;			
	lun->changed = 0;

	if (ub_sync_read_cap(sc, lun, &lun->capacity) != 0) {
		if (ub_sync_read_cap(sc, lun, &lun->capacity) != 0) {
			lun->capacity.nsec = 0;
			lun->capacity.bsize = 512;
			lun->capacity.bshift = 0;
		}
	}
}

static int ub_bd_open(struct block_device *bdev, fmode_t mode)
{
	struct ub_lun *lun = bdev->bd_disk->private_data;
	struct ub_dev *sc = lun->udev;
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&ub_lock, flags);
	if (atomic_read(&sc->poison)) {
		spin_unlock_irqrestore(&ub_lock, flags);
		return -ENXIO;
	}
	sc->openc++;
	spin_unlock_irqrestore(&ub_lock, flags);

	if (lun->removable || lun->readonly)
		check_disk_change(bdev);

	if (lun->removable && lun->changed && !(mode & FMODE_NDELAY)) {
		rc = -ENOMEDIUM;
		goto err_open;
	}

	if (lun->readonly && (mode & FMODE_WRITE)) {
		rc = -EROFS;
		goto err_open;
	}

	return 0;

err_open:
	ub_put(sc);
	return rc;
}

static int ub_bd_unlocked_open(struct block_device *bdev, fmode_t mode)
{
	int ret;

	mutex_lock(&ub_mutex);
	ret = ub_bd_open(bdev, mode);
	mutex_unlock(&ub_mutex);

	return ret;
}


static int ub_bd_release(struct gendisk *disk, fmode_t mode)
{
	struct ub_lun *lun = disk->private_data;
	struct ub_dev *sc = lun->udev;

	mutex_lock(&ub_mutex);
	ub_put(sc);
	mutex_unlock(&ub_mutex);

	return 0;
}

static int ub_bd_ioctl(struct block_device *bdev, fmode_t mode,
    unsigned int cmd, unsigned long arg)
{
	void __user *usermem = (void __user *) arg;
	int ret;

	mutex_lock(&ub_mutex);
	ret = scsi_cmd_blk_ioctl(bdev, mode, cmd, usermem);
	mutex_unlock(&ub_mutex);

	return ret;
}

static int ub_bd_revalidate(struct gendisk *disk)
{
	struct ub_lun *lun = disk->private_data;

	ub_revalidate(lun->udev, lun);

	
	blk_queue_logical_block_size(disk->queue, lun->capacity.bsize);
	set_capacity(disk, lun->capacity.nsec);
	

	return 0;
}

static unsigned int ub_bd_check_events(struct gendisk *disk,
				       unsigned int clearing)
{
	struct ub_lun *lun = disk->private_data;

	if (!lun->removable)
		return 0;

	if (ub_sync_tur(lun->udev, lun) != 0) {
		lun->changed = 1;
		return DISK_EVENT_MEDIA_CHANGE;
	}

	return lun->changed ? DISK_EVENT_MEDIA_CHANGE : 0;
}

static const struct block_device_operations ub_bd_fops = {
	.owner		= THIS_MODULE,
	.open		= ub_bd_unlocked_open,
	.release	= ub_bd_release,
	.ioctl		= ub_bd_ioctl,
	.check_events	= ub_bd_check_events,
	.revalidate_disk = ub_bd_revalidate,
};

static void ub_probe_done(struct ub_dev *sc, struct ub_scsi_cmd *cmd)
{
	struct completion *cop = cmd->back;
	complete(cop);
}

static int ub_sync_tur(struct ub_dev *sc, struct ub_lun *lun)
{
	struct ub_scsi_cmd *cmd;
	enum { ALLOC_SIZE = sizeof(struct ub_scsi_cmd) };
	unsigned long flags;
	struct completion compl;
	int rc;

	init_completion(&compl);

	rc = -ENOMEM;
	if ((cmd = kzalloc(ALLOC_SIZE, GFP_KERNEL)) == NULL)
		goto err_alloc;

	cmd->cdb[0] = TEST_UNIT_READY;
	cmd->cdb_len = 6;
	cmd->dir = UB_DIR_NONE;
	cmd->state = UB_CMDST_INIT;
	cmd->lun = lun;			
	cmd->done = ub_probe_done;
	cmd->back = &compl;

	spin_lock_irqsave(sc->lock, flags);
	cmd->tag = sc->tagcnt++;

	rc = ub_submit_scsi(sc, cmd);
	spin_unlock_irqrestore(sc->lock, flags);

	if (rc != 0)
		goto err_submit;

	wait_for_completion(&compl);

	rc = cmd->error;

	if (rc == -EIO && cmd->key != 0)	
		rc = cmd->key;

err_submit:
	kfree(cmd);
err_alloc:
	return rc;
}

static int ub_sync_read_cap(struct ub_dev *sc, struct ub_lun *lun,
    struct ub_capacity *ret)
{
	struct ub_scsi_cmd *cmd;
	struct scatterlist *sg;
	char *p;
	enum { ALLOC_SIZE = sizeof(struct ub_scsi_cmd) + 8 };
	unsigned long flags;
	unsigned int bsize, shift;
	unsigned long nsec;
	struct completion compl;
	int rc;

	init_completion(&compl);

	rc = -ENOMEM;
	if ((cmd = kzalloc(ALLOC_SIZE, GFP_KERNEL)) == NULL)
		goto err_alloc;
	p = (char *)cmd + sizeof(struct ub_scsi_cmd);

	cmd->cdb[0] = 0x25;
	cmd->cdb_len = 10;
	cmd->dir = UB_DIR_READ;
	cmd->state = UB_CMDST_INIT;
	cmd->nsg = 1;
	sg = &cmd->sgv[0];
	sg_init_table(sg, UB_MAX_REQ_SG);
	sg_set_page(sg, virt_to_page(p), 8, (unsigned long)p & (PAGE_SIZE-1));
	cmd->len = 8;
	cmd->lun = lun;
	cmd->done = ub_probe_done;
	cmd->back = &compl;

	spin_lock_irqsave(sc->lock, flags);
	cmd->tag = sc->tagcnt++;

	rc = ub_submit_scsi(sc, cmd);
	spin_unlock_irqrestore(sc->lock, flags);

	if (rc != 0)
		goto err_submit;

	wait_for_completion(&compl);

	if (cmd->error != 0) {
		rc = -EIO;
		goto err_read;
	}
	if (cmd->act_len != 8) {
		rc = -EIO;
		goto err_read;
	}

	
	nsec = be32_to_cpu(*(__be32 *)p) + 1;
	bsize = be32_to_cpu(*(__be32 *)(p + 4));
	switch (bsize) {
	case 512:	shift = 0;	break;
	case 1024:	shift = 1;	break;
	case 2048:	shift = 2;	break;
	case 4096:	shift = 3;	break;
	default:
		rc = -EDOM;
		goto err_inv_bsize;
	}

	ret->bsize = bsize;
	ret->bshift = shift;
	ret->nsec = nsec << shift;
	rc = 0;

err_inv_bsize:
err_read:
err_submit:
	kfree(cmd);
err_alloc:
	return rc;
}

static void ub_probe_urb_complete(struct urb *urb)
{
	struct completion *cop = urb->context;
	complete(cop);
}

static void ub_probe_timeout(unsigned long arg)
{
	struct completion *cop = (struct completion *) arg;
	complete(cop);
}

static int ub_sync_reset(struct ub_dev *sc)
{
	int ifnum = sc->intf->cur_altsetting->desc.bInterfaceNumber;
	struct usb_ctrlrequest *cr;
	struct completion compl;
	struct timer_list timer;
	int rc;

	init_completion(&compl);

	cr = &sc->work_cr;
	cr->bRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE;
	cr->bRequest = US_BULK_RESET_REQUEST;
	cr->wValue = cpu_to_le16(0);
	cr->wIndex = cpu_to_le16(ifnum);
	cr->wLength = cpu_to_le16(0);

	usb_fill_control_urb(&sc->work_urb, sc->dev, sc->send_ctrl_pipe,
	    (unsigned char*) cr, NULL, 0, ub_probe_urb_complete, &compl);

	if ((rc = usb_submit_urb(&sc->work_urb, GFP_KERNEL)) != 0) {
		printk(KERN_WARNING
		     "%s: Unable to submit a bulk reset (%d)\n", sc->name, rc);
		return rc;
	}

	init_timer(&timer);
	timer.function = ub_probe_timeout;
	timer.data = (unsigned long) &compl;
	timer.expires = jiffies + UB_CTRL_TIMEOUT;
	add_timer(&timer);

	wait_for_completion(&compl);

	del_timer_sync(&timer);
	usb_kill_urb(&sc->work_urb);

	return sc->work_urb.status;
}

static int ub_sync_getmaxlun(struct ub_dev *sc)
{
	int ifnum = sc->intf->cur_altsetting->desc.bInterfaceNumber;
	unsigned char *p;
	enum { ALLOC_SIZE = 1 };
	struct usb_ctrlrequest *cr;
	struct completion compl;
	struct timer_list timer;
	int nluns;
	int rc;

	init_completion(&compl);

	rc = -ENOMEM;
	if ((p = kmalloc(ALLOC_SIZE, GFP_KERNEL)) == NULL)
		goto err_alloc;
	*p = 55;

	cr = &sc->work_cr;
	cr->bRequestType = USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
	cr->bRequest = US_BULK_GET_MAX_LUN;
	cr->wValue = cpu_to_le16(0);
	cr->wIndex = cpu_to_le16(ifnum);
	cr->wLength = cpu_to_le16(1);

	usb_fill_control_urb(&sc->work_urb, sc->dev, sc->recv_ctrl_pipe,
	    (unsigned char*) cr, p, 1, ub_probe_urb_complete, &compl);

	if ((rc = usb_submit_urb(&sc->work_urb, GFP_KERNEL)) != 0)
		goto err_submit;

	init_timer(&timer);
	timer.function = ub_probe_timeout;
	timer.data = (unsigned long) &compl;
	timer.expires = jiffies + UB_CTRL_TIMEOUT;
	add_timer(&timer);

	wait_for_completion(&compl);

	del_timer_sync(&timer);
	usb_kill_urb(&sc->work_urb);

	if ((rc = sc->work_urb.status) < 0)
		goto err_io;

	if (sc->work_urb.actual_length != 1) {
		nluns = 0;
	} else {
		if ((nluns = *p) == 55) {
			nluns = 0;
		} else {
  			
			nluns += 1;
			if (nluns > UB_MAX_LUNS)
				nluns = UB_MAX_LUNS;
		}
	}

	kfree(p);
	return nluns;

err_io:
err_submit:
	kfree(p);
err_alloc:
	return rc;
}

static int ub_probe_clear_stall(struct ub_dev *sc, int stalled_pipe)
{
	int endp;
	struct usb_ctrlrequest *cr;
	struct completion compl;
	struct timer_list timer;
	int rc;

	init_completion(&compl);

	endp = usb_pipeendpoint(stalled_pipe);
	if (usb_pipein (stalled_pipe))
		endp |= USB_DIR_IN;

	cr = &sc->work_cr;
	cr->bRequestType = USB_RECIP_ENDPOINT;
	cr->bRequest = USB_REQ_CLEAR_FEATURE;
	cr->wValue = cpu_to_le16(USB_ENDPOINT_HALT);
	cr->wIndex = cpu_to_le16(endp);
	cr->wLength = cpu_to_le16(0);

	usb_fill_control_urb(&sc->work_urb, sc->dev, sc->send_ctrl_pipe,
	    (unsigned char*) cr, NULL, 0, ub_probe_urb_complete, &compl);

	if ((rc = usb_submit_urb(&sc->work_urb, GFP_KERNEL)) != 0) {
		printk(KERN_WARNING
		     "%s: Unable to submit a probe clear (%d)\n", sc->name, rc);
		return rc;
	}

	init_timer(&timer);
	timer.function = ub_probe_timeout;
	timer.data = (unsigned long) &compl;
	timer.expires = jiffies + UB_CTRL_TIMEOUT;
	add_timer(&timer);

	wait_for_completion(&compl);

	del_timer_sync(&timer);
	usb_kill_urb(&sc->work_urb);

	usb_reset_endpoint(sc->dev, endp);

	return 0;
}

static int ub_get_pipes(struct ub_dev *sc, struct usb_device *dev,
    struct usb_interface *intf)
{
	struct usb_host_interface *altsetting = intf->cur_altsetting;
	struct usb_endpoint_descriptor *ep_in = NULL;
	struct usb_endpoint_descriptor *ep_out = NULL;
	struct usb_endpoint_descriptor *ep;
	int i;

	for (i = 0; i < altsetting->desc.bNumEndpoints; i++) {
		ep = &altsetting->endpoint[i].desc;

		
		if (usb_endpoint_xfer_bulk(ep)) {
			
			if (usb_endpoint_dir_in(ep)) {
				if (ep_in == NULL)
					ep_in = ep;
			} else {
				if (ep_out == NULL)
					ep_out = ep;
			}
		}
	}

	if (ep_in == NULL || ep_out == NULL) {
		printk(KERN_NOTICE "%s: failed endpoint check\n", sc->name);
		return -ENODEV;
	}

	
	sc->send_ctrl_pipe = usb_sndctrlpipe(dev, 0);
	sc->recv_ctrl_pipe = usb_rcvctrlpipe(dev, 0);
	sc->send_bulk_pipe = usb_sndbulkpipe(dev,
		usb_endpoint_num(ep_out));
	sc->recv_bulk_pipe = usb_rcvbulkpipe(dev, 
		usb_endpoint_num(ep_in));

	return 0;
}

static int ub_probe(struct usb_interface *intf,
    const struct usb_device_id *dev_id)
{
	struct ub_dev *sc;
	int nluns;
	int rc;
	int i;

	if (usb_usual_check_type(dev_id, USB_US_TYPE_UB))
		return -ENXIO;

	rc = -ENOMEM;
	if ((sc = kzalloc(sizeof(struct ub_dev), GFP_KERNEL)) == NULL)
		goto err_core;
	sc->lock = ub_next_lock();
	INIT_LIST_HEAD(&sc->luns);
	usb_init_urb(&sc->work_urb);
	tasklet_init(&sc->tasklet, ub_scsi_action, (unsigned long)sc);
	atomic_set(&sc->poison, 0);
	INIT_WORK(&sc->reset_work, ub_reset_task);
	init_waitqueue_head(&sc->reset_wait);

	init_timer(&sc->work_timer);
	sc->work_timer.data = (unsigned long) sc;
	sc->work_timer.function = ub_urb_timeout;

	ub_init_completion(&sc->work_done);
	sc->work_done.done = 1;		

	sc->dev = interface_to_usbdev(intf);
	sc->intf = intf;
	
	usb_set_intfdata(intf, sc);
	usb_get_dev(sc->dev);
	usb_get_intf(sc->intf);

	snprintf(sc->name, 12, DRV_NAME "(%d.%d)",
	    sc->dev->bus->busnum, sc->dev->devnum);

	

	if (ub_get_pipes(sc, sc->dev, intf) != 0)
		goto err_dev_desc;


#if 0 
	ub_probe_clear_stall(sc, sc->recv_bulk_pipe);
	ub_probe_clear_stall(sc, sc->send_bulk_pipe);
#endif

	for (i = 0; i < 3; i++) {  
		if ((rc = ub_sync_tur(sc, NULL)) <= 0) break;
		if (rc != 0x6) break;
		msleep(10);
	}

	nluns = 1;
	for (i = 0; i < 3; i++) {
		if ((rc = ub_sync_getmaxlun(sc)) < 0)
			break;
		if (rc != 0) {
			nluns = rc;
			break;
		}
		msleep(100);
	}

	for (i = 0; i < nluns; i++) {
		ub_probe_lun(sc, i);
	}
	return 0;

err_dev_desc:
	usb_set_intfdata(intf, NULL);
	usb_put_intf(sc->intf);
	usb_put_dev(sc->dev);
	kfree(sc);
err_core:
	return rc;
}

static int ub_probe_lun(struct ub_dev *sc, int lnum)
{
	struct ub_lun *lun;
	struct request_queue *q;
	struct gendisk *disk;
	int rc;

	rc = -ENOMEM;
	if ((lun = kzalloc(sizeof(struct ub_lun), GFP_KERNEL)) == NULL)
		goto err_alloc;
	lun->num = lnum;

	rc = -ENOSR;
	if ((lun->id = ub_id_get()) == -1)
		goto err_id;

	lun->udev = sc;

	snprintf(lun->name, 16, DRV_NAME "%c(%d.%d.%d)",
	    lun->id + 'a', sc->dev->bus->busnum, sc->dev->devnum, lun->num);

	lun->removable = 1;		
	lun->changed = 1;		
	ub_revalidate(sc, lun);

	rc = -ENOMEM;
	if ((disk = alloc_disk(UB_PARTS_PER_LUN)) == NULL)
		goto err_diskalloc;

	sprintf(disk->disk_name, DRV_NAME "%c", lun->id + 'a');
	disk->major = UB_MAJOR;
	disk->first_minor = lun->id * UB_PARTS_PER_LUN;
	disk->fops = &ub_bd_fops;
	disk->private_data = lun;
	disk->driverfs_dev = &sc->intf->dev;

	rc = -ENOMEM;
	if ((q = blk_init_queue(ub_request_fn, sc->lock)) == NULL)
		goto err_blkqinit;

	disk->queue = q;

	blk_queue_bounce_limit(q, BLK_BOUNCE_HIGH);
	blk_queue_max_segments(q, UB_MAX_REQ_SG);
	blk_queue_segment_boundary(q, 0xffffffff);	
	blk_queue_max_hw_sectors(q, UB_MAX_SECTORS);
	blk_queue_logical_block_size(q, lun->capacity.bsize);

	lun->disk = disk;
	q->queuedata = lun;
	list_add(&lun->link, &sc->luns);

	set_capacity(disk, lun->capacity.nsec);
	if (lun->removable)
		disk->flags |= GENHD_FL_REMOVABLE;

	add_disk(disk);

	return 0;

err_blkqinit:
	put_disk(disk);
err_diskalloc:
	ub_id_put(lun->id);
err_id:
	kfree(lun);
err_alloc:
	return rc;
}

static void ub_disconnect(struct usb_interface *intf)
{
	struct ub_dev *sc = usb_get_intfdata(intf);
	struct ub_lun *lun;
	unsigned long flags;

	spin_lock_irqsave(&ub_lock, flags);
	sc->openc++;
	spin_unlock_irqrestore(&ub_lock, flags);

	atomic_set(&sc->poison, 1);

	wait_event(sc->reset_wait, !sc->reset);

	spin_lock_irqsave(sc->lock, flags);
	{
		struct ub_scsi_cmd *cmd;
		int cnt = 0;
		while ((cmd = ub_cmdq_peek(sc)) != NULL) {
			cmd->error = -ENOTCONN;
			cmd->state = UB_CMDST_DONE;
			ub_cmdq_pop(sc);
			(*cmd->done)(sc, cmd);
			cnt++;
		}
		if (cnt != 0) {
			printk(KERN_WARNING "%s: "
			    "%d was queued after shutdown\n", sc->name, cnt);
		}
	}
	spin_unlock_irqrestore(sc->lock, flags);

	list_for_each_entry(lun, &sc->luns, link) {
		del_gendisk(lun->disk);
	}

	spin_lock_irqsave(sc->lock, flags);
	if (sc->work_urb.status == -EINPROGRESS) {	
		printk(KERN_WARNING "%s: "
		    "URB is active after disconnect\n", sc->name);
	}
	spin_unlock_irqrestore(sc->lock, flags);

	del_timer_sync(&sc->work_timer);


	ub_put(sc);
}

static struct usb_driver ub_driver = {
	.name =		"ub",
	.probe =	ub_probe,
	.disconnect =	ub_disconnect,
	.id_table =	ub_usb_ids,
	.pre_reset =	ub_pre_reset,
	.post_reset =	ub_post_reset,
};

static int __init ub_init(void)
{
	int rc;
	int i;

	pr_info("'Low Performance USB Block' driver is deprecated. "
			"Please switch to usb-storage\n");
	for (i = 0; i < UB_QLOCK_NUM; i++)
		spin_lock_init(&ub_qlockv[i]);

	if ((rc = register_blkdev(UB_MAJOR, DRV_NAME)) != 0)
		goto err_regblkdev;

	if ((rc = usb_register(&ub_driver)) != 0)
		goto err_register;

	usb_usual_set_present(USB_US_TYPE_UB);
	return 0;

err_register:
	unregister_blkdev(UB_MAJOR, DRV_NAME);
err_regblkdev:
	return rc;
}

static void __exit ub_exit(void)
{
	usb_deregister(&ub_driver);

	unregister_blkdev(UB_MAJOR, DRV_NAME);
	usb_usual_clear_present(USB_US_TYPE_UB);
}

module_init(ub_init);
module_exit(ub_exit);

MODULE_LICENSE("GPL");
