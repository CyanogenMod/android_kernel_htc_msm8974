/*
 * File...........: linux/drivers/s390/block/dasd.c
 * Author(s)......: Holger Smolinski <Holger.Smolinski@de.ibm.com>
 *		    Horst Hummel <Horst.Hummel@de.ibm.com>
 *		    Carsten Otte <Cotte@de.ibm.com>
 *		    Martin Schwidefsky <schwidefsky@de.ibm.com>
 * Bugreports.to..: <Linux390@de.ibm.com>
 * Copyright IBM Corp. 1999, 2009
 */

#define KMSG_COMPONENT "dasd"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/hdreg.h>
#include <linux/async.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>

#include <asm/ccwdev.h>
#include <asm/ebcdic.h>
#include <asm/idals.h>
#include <asm/itcw.h>
#include <asm/diag.h>

#define PRINTK_HEADER "dasd:"

#include "dasd_int.h"
#define DASD_CHANQ_MAX_SIZE 4

#define DASD_SLEEPON_START_TAG	(void *) 1
#define DASD_SLEEPON_END_TAG	(void *) 2

debug_info_t *dasd_debug_area;
static struct dentry *dasd_debugfs_root_entry;
struct dasd_discipline *dasd_diag_discipline_pointer;
void dasd_int_handler(struct ccw_device *, unsigned long, struct irb *);

MODULE_AUTHOR("Holger Smolinski <Holger.Smolinski@de.ibm.com>");
MODULE_DESCRIPTION("Linux on S/390 DASD device driver,"
		   " Copyright 2000 IBM Corporation");
MODULE_SUPPORTED_DEVICE("dasd");
MODULE_LICENSE("GPL");

static int  dasd_alloc_queue(struct dasd_block *);
static void dasd_setup_queue(struct dasd_block *);
static void dasd_free_queue(struct dasd_block *);
static void dasd_flush_request_queue(struct dasd_block *);
static int dasd_flush_block_queue(struct dasd_block *);
static void dasd_device_tasklet(struct dasd_device *);
static void dasd_block_tasklet(struct dasd_block *);
static void do_kick_device(struct work_struct *);
static void do_restore_device(struct work_struct *);
static void do_reload_device(struct work_struct *);
static void dasd_return_cqr_cb(struct dasd_ccw_req *, void *);
static void dasd_device_timeout(unsigned long);
static void dasd_block_timeout(unsigned long);
static void __dasd_process_erp(struct dasd_device *, struct dasd_ccw_req *);
static void dasd_profile_init(struct dasd_profile *, struct dentry *);
static void dasd_profile_exit(struct dasd_profile *);

static wait_queue_head_t dasd_init_waitq;
static wait_queue_head_t dasd_flush_wq;
static wait_queue_head_t generic_waitq;

struct dasd_device *dasd_alloc_device(void)
{
	struct dasd_device *device;

	device = kzalloc(sizeof(struct dasd_device), GFP_ATOMIC);
	if (!device)
		return ERR_PTR(-ENOMEM);

	
	device->ccw_mem = (void *) __get_free_pages(GFP_ATOMIC | GFP_DMA, 1);
	if (!device->ccw_mem) {
		kfree(device);
		return ERR_PTR(-ENOMEM);
	}
	
	device->erp_mem = (void *) get_zeroed_page(GFP_ATOMIC | GFP_DMA);
	if (!device->erp_mem) {
		free_pages((unsigned long) device->ccw_mem, 1);
		kfree(device);
		return ERR_PTR(-ENOMEM);
	}

	dasd_init_chunklist(&device->ccw_chunks, device->ccw_mem, PAGE_SIZE*2);
	dasd_init_chunklist(&device->erp_chunks, device->erp_mem, PAGE_SIZE);
	spin_lock_init(&device->mem_lock);
	atomic_set(&device->tasklet_scheduled, 0);
	tasklet_init(&device->tasklet,
		     (void (*)(unsigned long)) dasd_device_tasklet,
		     (unsigned long) device);
	INIT_LIST_HEAD(&device->ccw_queue);
	init_timer(&device->timer);
	device->timer.function = dasd_device_timeout;
	device->timer.data = (unsigned long) device;
	INIT_WORK(&device->kick_work, do_kick_device);
	INIT_WORK(&device->restore_device, do_restore_device);
	INIT_WORK(&device->reload_device, do_reload_device);
	device->state = DASD_STATE_NEW;
	device->target = DASD_STATE_NEW;
	mutex_init(&device->state_mutex);
	spin_lock_init(&device->profile.lock);
	return device;
}

void dasd_free_device(struct dasd_device *device)
{
	kfree(device->private);
	free_page((unsigned long) device->erp_mem);
	free_pages((unsigned long) device->ccw_mem, 1);
	kfree(device);
}

struct dasd_block *dasd_alloc_block(void)
{
	struct dasd_block *block;

	block = kzalloc(sizeof(*block), GFP_ATOMIC);
	if (!block)
		return ERR_PTR(-ENOMEM);
	
	atomic_set(&block->open_count, -1);

	spin_lock_init(&block->request_queue_lock);
	atomic_set(&block->tasklet_scheduled, 0);
	tasklet_init(&block->tasklet,
		     (void (*)(unsigned long)) dasd_block_tasklet,
		     (unsigned long) block);
	INIT_LIST_HEAD(&block->ccw_queue);
	spin_lock_init(&block->queue_lock);
	init_timer(&block->timer);
	block->timer.function = dasd_block_timeout;
	block->timer.data = (unsigned long) block;
	spin_lock_init(&block->profile.lock);

	return block;
}

void dasd_free_block(struct dasd_block *block)
{
	kfree(block);
}

static int dasd_state_new_to_known(struct dasd_device *device)
{
	int rc;

	dasd_get_device(device);

	if (device->block) {
		rc = dasd_alloc_queue(device->block);
		if (rc) {
			dasd_put_device(device);
			return rc;
		}
	}
	device->state = DASD_STATE_KNOWN;
	return 0;
}

static int dasd_state_known_to_new(struct dasd_device *device)
{
	
	dasd_eer_disable(device);
	
	if (device->discipline) {
		if (device->discipline->uncheck_device)
			device->discipline->uncheck_device(device);
		module_put(device->discipline->owner);
	}
	device->discipline = NULL;
	if (device->base_discipline)
		module_put(device->base_discipline->owner);
	device->base_discipline = NULL;
	device->state = DASD_STATE_NEW;

	if (device->block)
		dasd_free_queue(device->block);

	
	dasd_put_device(device);
	return 0;
}

static struct dentry *dasd_debugfs_setup(const char *name,
					 struct dentry *base_dentry)
{
	struct dentry *pde;

	if (!base_dentry)
		return NULL;
	pde = debugfs_create_dir(name, base_dentry);
	if (!pde || IS_ERR(pde))
		return NULL;
	return pde;
}

static int dasd_state_known_to_basic(struct dasd_device *device)
{
	struct dasd_block *block = device->block;
	int rc;

	
	if (block) {
		rc = dasd_gendisk_alloc(block);
		if (rc)
			return rc;
		block->debugfs_dentry =
			dasd_debugfs_setup(block->gdp->disk_name,
					   dasd_debugfs_root_entry);
		dasd_profile_init(&block->profile, block->debugfs_dentry);
		if (dasd_global_profile_level == DASD_PROFILE_ON)
			dasd_profile_on(&device->block->profile);
	}
	device->debugfs_dentry =
		dasd_debugfs_setup(dev_name(&device->cdev->dev),
				   dasd_debugfs_root_entry);
	dasd_profile_init(&device->profile, device->debugfs_dentry);

	
	device->debug_area = debug_register(dev_name(&device->cdev->dev), 4, 1,
					    8 * sizeof(long));
	debug_register_view(device->debug_area, &debug_sprintf_view);
	debug_set_level(device->debug_area, DBF_WARNING);
	DBF_DEV_EVENT(DBF_EMERG, device, "%s", "debug area created");

	device->state = DASD_STATE_BASIC;
	return 0;
}

static int dasd_state_basic_to_known(struct dasd_device *device)
{
	int rc;
	if (device->block) {
		dasd_profile_exit(&device->block->profile);
		if (device->block->debugfs_dentry)
			debugfs_remove(device->block->debugfs_dentry);
		dasd_gendisk_free(device->block);
		dasd_block_clear_timer(device->block);
	}
	rc = dasd_flush_device_queue(device);
	if (rc)
		return rc;
	dasd_device_clear_timer(device);
	dasd_profile_exit(&device->profile);
	if (device->debugfs_dentry)
		debugfs_remove(device->debugfs_dentry);

	DBF_DEV_EVENT(DBF_EMERG, device, "%p debug area deleted", device);
	if (device->debug_area != NULL) {
		debug_unregister(device->debug_area);
		device->debug_area = NULL;
	}
	device->state = DASD_STATE_KNOWN;
	return 0;
}

static int dasd_state_basic_to_ready(struct dasd_device *device)
{
	int rc;
	struct dasd_block *block;

	rc = 0;
	block = device->block;
	
	if (block) {
		if (block->base->discipline->do_analysis != NULL)
			rc = block->base->discipline->do_analysis(block);
		if (rc) {
			if (rc != -EAGAIN)
				device->state = DASD_STATE_UNFMT;
			return rc;
		}
		dasd_setup_queue(block);
		set_capacity(block->gdp,
			     block->blocks << block->s2b_shift);
		device->state = DASD_STATE_READY;
		rc = dasd_scan_partitions(block);
		if (rc)
			device->state = DASD_STATE_BASIC;
	} else {
		device->state = DASD_STATE_READY;
	}
	return rc;
}

static int dasd_state_ready_to_basic(struct dasd_device *device)
{
	int rc;

	device->state = DASD_STATE_BASIC;
	if (device->block) {
		struct dasd_block *block = device->block;
		rc = dasd_flush_block_queue(block);
		if (rc) {
			device->state = DASD_STATE_READY;
			return rc;
		}
		dasd_flush_request_queue(block);
		dasd_destroy_partitions(block);
		block->blocks = 0;
		block->bp_block = 0;
		block->s2b_shift = 0;
	}
	return 0;
}

static int dasd_state_unfmt_to_basic(struct dasd_device *device)
{
	device->state = DASD_STATE_BASIC;
	return 0;
}

static int
dasd_state_ready_to_online(struct dasd_device * device)
{
	int rc;
	struct gendisk *disk;
	struct disk_part_iter piter;
	struct hd_struct *part;

	if (device->discipline->ready_to_online) {
		rc = device->discipline->ready_to_online(device);
		if (rc)
			return rc;
	}
	device->state = DASD_STATE_ONLINE;
	if (device->block) {
		dasd_schedule_block_bh(device->block);
		if ((device->features & DASD_FEATURE_USERAW)) {
			disk = device->block->gdp;
			kobject_uevent(&disk_to_dev(disk)->kobj, KOBJ_CHANGE);
			return 0;
		}
		disk = device->block->bdev->bd_disk;
		disk_part_iter_init(&piter, disk, DISK_PITER_INCL_PART0);
		while ((part = disk_part_iter_next(&piter)))
			kobject_uevent(&part_to_dev(part)->kobj, KOBJ_CHANGE);
		disk_part_iter_exit(&piter);
	}
	return 0;
}

static int dasd_state_online_to_ready(struct dasd_device *device)
{
	int rc;
	struct gendisk *disk;
	struct disk_part_iter piter;
	struct hd_struct *part;

	if (device->discipline->online_to_ready) {
		rc = device->discipline->online_to_ready(device);
		if (rc)
			return rc;
	}
	device->state = DASD_STATE_READY;
	if (device->block && !(device->features & DASD_FEATURE_USERAW)) {
		disk = device->block->bdev->bd_disk;
		disk_part_iter_init(&piter, disk, DISK_PITER_INCL_PART0);
		while ((part = disk_part_iter_next(&piter)))
			kobject_uevent(&part_to_dev(part)->kobj, KOBJ_CHANGE);
		disk_part_iter_exit(&piter);
	}
	return 0;
}

static int dasd_increase_state(struct dasd_device *device)
{
	int rc;

	rc = 0;
	if (device->state == DASD_STATE_NEW &&
	    device->target >= DASD_STATE_KNOWN)
		rc = dasd_state_new_to_known(device);

	if (!rc &&
	    device->state == DASD_STATE_KNOWN &&
	    device->target >= DASD_STATE_BASIC)
		rc = dasd_state_known_to_basic(device);

	if (!rc &&
	    device->state == DASD_STATE_BASIC &&
	    device->target >= DASD_STATE_READY)
		rc = dasd_state_basic_to_ready(device);

	if (!rc &&
	    device->state == DASD_STATE_UNFMT &&
	    device->target > DASD_STATE_UNFMT)
		rc = -EPERM;

	if (!rc &&
	    device->state == DASD_STATE_READY &&
	    device->target >= DASD_STATE_ONLINE)
		rc = dasd_state_ready_to_online(device);

	return rc;
}

static int dasd_decrease_state(struct dasd_device *device)
{
	int rc;

	rc = 0;
	if (device->state == DASD_STATE_ONLINE &&
	    device->target <= DASD_STATE_READY)
		rc = dasd_state_online_to_ready(device);

	if (!rc &&
	    device->state == DASD_STATE_READY &&
	    device->target <= DASD_STATE_BASIC)
		rc = dasd_state_ready_to_basic(device);

	if (!rc &&
	    device->state == DASD_STATE_UNFMT &&
	    device->target <= DASD_STATE_BASIC)
		rc = dasd_state_unfmt_to_basic(device);

	if (!rc &&
	    device->state == DASD_STATE_BASIC &&
	    device->target <= DASD_STATE_KNOWN)
		rc = dasd_state_basic_to_known(device);

	if (!rc &&
	    device->state == DASD_STATE_KNOWN &&
	    device->target <= DASD_STATE_NEW)
		rc = dasd_state_known_to_new(device);

	return rc;
}

static void dasd_change_state(struct dasd_device *device)
{
	int rc;

	if (device->state == device->target)
		
		return;
	if (device->state < device->target)
		rc = dasd_increase_state(device);
	else
		rc = dasd_decrease_state(device);
	if (rc == -EAGAIN)
		return;
	if (rc)
		device->target = device->state;

	if (device->state == device->target)
		wake_up(&dasd_init_waitq);

	
	kobject_uevent(&device->cdev->dev.kobj, KOBJ_CHANGE);
}

static void do_kick_device(struct work_struct *work)
{
	struct dasd_device *device = container_of(work, struct dasd_device, kick_work);
	mutex_lock(&device->state_mutex);
	dasd_change_state(device);
	mutex_unlock(&device->state_mutex);
	dasd_schedule_device_bh(device);
	dasd_put_device(device);
}

void dasd_kick_device(struct dasd_device *device)
{
	dasd_get_device(device);
	
	schedule_work(&device->kick_work);
}

static void do_reload_device(struct work_struct *work)
{
	struct dasd_device *device = container_of(work, struct dasd_device,
						  reload_device);
	device->discipline->reload(device);
	dasd_put_device(device);
}

void dasd_reload_device(struct dasd_device *device)
{
	dasd_get_device(device);
	
	schedule_work(&device->reload_device);
}
EXPORT_SYMBOL(dasd_reload_device);

static void do_restore_device(struct work_struct *work)
{
	struct dasd_device *device = container_of(work, struct dasd_device,
						  restore_device);
	device->cdev->drv->restore(device->cdev);
	dasd_put_device(device);
}

void dasd_restore_device(struct dasd_device *device)
{
	dasd_get_device(device);
	
	schedule_work(&device->restore_device);
}

void dasd_set_target_state(struct dasd_device *device, int target)
{
	dasd_get_device(device);
	mutex_lock(&device->state_mutex);
	
	if (dasd_probeonly && target > DASD_STATE_READY)
		target = DASD_STATE_READY;
	if (device->target != target) {
		if (device->state == target)
			wake_up(&dasd_init_waitq);
		device->target = target;
	}
	if (device->state != device->target)
		dasd_change_state(device);
	mutex_unlock(&device->state_mutex);
	dasd_put_device(device);
}

static inline int _wait_for_device(struct dasd_device *device)
{
	return (device->state == device->target);
}

void dasd_enable_device(struct dasd_device *device)
{
	dasd_set_target_state(device, DASD_STATE_ONLINE);
	if (device->state <= DASD_STATE_KNOWN)
		
		dasd_set_target_state(device, DASD_STATE_NEW);
	
	wait_event(dasd_init_waitq, _wait_for_device(device));

	dasd_reload_device(device);
	if (device->discipline->kick_validate)
		device->discipline->kick_validate(device);
}


unsigned int dasd_global_profile_level = DASD_PROFILE_OFF;

#ifdef CONFIG_DASD_PROFILE
struct dasd_profile_info dasd_global_profile_data;
static struct dentry *dasd_global_profile_dentry;
static struct dentry *dasd_debugfs_global_entry;

static void dasd_profile_start(struct dasd_block *block,
			       struct dasd_ccw_req *cqr,
			       struct request *req)
{
	struct list_head *l;
	unsigned int counter;
	struct dasd_device *device;

	
	counter = 0;
	if (dasd_global_profile_level || block->profile.data)
		list_for_each(l, &block->ccw_queue)
			if (++counter >= 31)
				break;

	if (dasd_global_profile_level) {
		dasd_global_profile_data.dasd_io_nr_req[counter]++;
		if (rq_data_dir(req) == READ)
			dasd_global_profile_data.dasd_read_nr_req[counter]++;
	}

	spin_lock(&block->profile.lock);
	if (block->profile.data)
		block->profile.data->dasd_io_nr_req[counter]++;
		if (rq_data_dir(req) == READ)
			block->profile.data->dasd_read_nr_req[counter]++;
	spin_unlock(&block->profile.lock);

	device = cqr->startdev;
	if (device->profile.data) {
		counter = 1; 
		list_for_each(l, &device->ccw_queue)
			if (++counter >= 31)
				break;
	}
	spin_lock(&device->profile.lock);
	if (device->profile.data) {
		device->profile.data->dasd_io_nr_req[counter]++;
		if (rq_data_dir(req) == READ)
			device->profile.data->dasd_read_nr_req[counter]++;
	}
	spin_unlock(&device->profile.lock);
}


#define dasd_profile_counter(value, index)			   \
{								   \
	for (index = 0; index < 31 && value >> (2+index); index++) \
		;						   \
}

static void dasd_profile_end_add_data(struct dasd_profile_info *data,
				      int is_alias,
				      int is_tpm,
				      int is_read,
				      long sectors,
				      int sectors_ind,
				      int tottime_ind,
				      int tottimeps_ind,
				      int strtime_ind,
				      int irqtime_ind,
				      int irqtimeps_ind,
				      int endtime_ind)
{
	
	if (data->dasd_io_reqs == UINT_MAX) {
			memset(data, 0, sizeof(*data));
			getnstimeofday(&data->starttod);
	}
	data->dasd_io_reqs++;
	data->dasd_io_sects += sectors;
	if (is_alias)
		data->dasd_io_alias++;
	if (is_tpm)
		data->dasd_io_tpm++;

	data->dasd_io_secs[sectors_ind]++;
	data->dasd_io_times[tottime_ind]++;
	data->dasd_io_timps[tottimeps_ind]++;
	data->dasd_io_time1[strtime_ind]++;
	data->dasd_io_time2[irqtime_ind]++;
	data->dasd_io_time2ps[irqtimeps_ind]++;
	data->dasd_io_time3[endtime_ind]++;

	if (is_read) {
		data->dasd_read_reqs++;
		data->dasd_read_sects += sectors;
		if (is_alias)
			data->dasd_read_alias++;
		if (is_tpm)
			data->dasd_read_tpm++;
		data->dasd_read_secs[sectors_ind]++;
		data->dasd_read_times[tottime_ind]++;
		data->dasd_read_time1[strtime_ind]++;
		data->dasd_read_time2[irqtime_ind]++;
		data->dasd_read_time3[endtime_ind]++;
	}
}

static void dasd_profile_end(struct dasd_block *block,
			     struct dasd_ccw_req *cqr,
			     struct request *req)
{
	long strtime, irqtime, endtime, tottime;	
	long tottimeps, sectors;
	struct dasd_device *device;
	int sectors_ind, tottime_ind, tottimeps_ind, strtime_ind;
	int irqtime_ind, irqtimeps_ind, endtime_ind;

	device = cqr->startdev;
	if (!(dasd_global_profile_level ||
	      block->profile.data ||
	      device->profile.data))
		return;

	sectors = blk_rq_sectors(req);
	if (!cqr->buildclk || !cqr->startclk ||
	    !cqr->stopclk || !cqr->endclk ||
	    !sectors)
		return;

	strtime = ((cqr->startclk - cqr->buildclk) >> 12);
	irqtime = ((cqr->stopclk - cqr->startclk) >> 12);
	endtime = ((cqr->endclk - cqr->stopclk) >> 12);
	tottime = ((cqr->endclk - cqr->buildclk) >> 12);
	tottimeps = tottime / sectors;

	dasd_profile_counter(sectors, sectors_ind);
	dasd_profile_counter(tottime, tottime_ind);
	dasd_profile_counter(tottimeps, tottimeps_ind);
	dasd_profile_counter(strtime, strtime_ind);
	dasd_profile_counter(irqtime, irqtime_ind);
	dasd_profile_counter(irqtime / sectors, irqtimeps_ind);
	dasd_profile_counter(endtime, endtime_ind);

	if (dasd_global_profile_level) {
		dasd_profile_end_add_data(&dasd_global_profile_data,
					  cqr->startdev != block->base,
					  cqr->cpmode == 1,
					  rq_data_dir(req) == READ,
					  sectors, sectors_ind, tottime_ind,
					  tottimeps_ind, strtime_ind,
					  irqtime_ind, irqtimeps_ind,
					  endtime_ind);
	}

	spin_lock(&block->profile.lock);
	if (block->profile.data)
		dasd_profile_end_add_data(block->profile.data,
					  cqr->startdev != block->base,
					  cqr->cpmode == 1,
					  rq_data_dir(req) == READ,
					  sectors, sectors_ind, tottime_ind,
					  tottimeps_ind, strtime_ind,
					  irqtime_ind, irqtimeps_ind,
					  endtime_ind);
	spin_unlock(&block->profile.lock);

	spin_lock(&device->profile.lock);
	if (device->profile.data)
		dasd_profile_end_add_data(device->profile.data,
					  cqr->startdev != block->base,
					  cqr->cpmode == 1,
					  rq_data_dir(req) == READ,
					  sectors, sectors_ind, tottime_ind,
					  tottimeps_ind, strtime_ind,
					  irqtime_ind, irqtimeps_ind,
					  endtime_ind);
	spin_unlock(&device->profile.lock);
}

void dasd_profile_reset(struct dasd_profile *profile)
{
	struct dasd_profile_info *data;

	spin_lock_bh(&profile->lock);
	data = profile->data;
	if (!data) {
		spin_unlock_bh(&profile->lock);
		return;
	}
	memset(data, 0, sizeof(*data));
	getnstimeofday(&data->starttod);
	spin_unlock_bh(&profile->lock);
}

void dasd_global_profile_reset(void)
{
	memset(&dasd_global_profile_data, 0, sizeof(dasd_global_profile_data));
	getnstimeofday(&dasd_global_profile_data.starttod);
}

int dasd_profile_on(struct dasd_profile *profile)
{
	struct dasd_profile_info *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	spin_lock_bh(&profile->lock);
	if (profile->data) {
		spin_unlock_bh(&profile->lock);
		kfree(data);
		return 0;
	}
	getnstimeofday(&data->starttod);
	profile->data = data;
	spin_unlock_bh(&profile->lock);
	return 0;
}

void dasd_profile_off(struct dasd_profile *profile)
{
	spin_lock_bh(&profile->lock);
	kfree(profile->data);
	profile->data = NULL;
	spin_unlock_bh(&profile->lock);
}

char *dasd_get_user_string(const char __user *user_buf, size_t user_len)
{
	char *buffer;

	buffer = vmalloc(user_len + 1);
	if (buffer == NULL)
		return ERR_PTR(-ENOMEM);
	if (copy_from_user(buffer, user_buf, user_len) != 0) {
		vfree(buffer);
		return ERR_PTR(-EFAULT);
	}
	
	if (buffer[user_len - 1] == '\n')
		buffer[user_len - 1] = 0;
	else
		buffer[user_len] = 0;
	return buffer;
}

static ssize_t dasd_stats_write(struct file *file,
				const char __user *user_buf,
				size_t user_len, loff_t *pos)
{
	char *buffer, *str;
	int rc;
	struct seq_file *m = (struct seq_file *)file->private_data;
	struct dasd_profile *prof = m->private;

	if (user_len > 65536)
		user_len = 65536;
	buffer = dasd_get_user_string(user_buf, user_len);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	str = skip_spaces(buffer);
	rc = user_len;
	if (strncmp(str, "reset", 5) == 0) {
		dasd_profile_reset(prof);
	} else if (strncmp(str, "on", 2) == 0) {
		rc = dasd_profile_on(prof);
		if (!rc)
			rc = user_len;
	} else if (strncmp(str, "off", 3) == 0) {
		dasd_profile_off(prof);
	} else
		rc = -EINVAL;
	vfree(buffer);
	return rc;
}

static void dasd_stats_array(struct seq_file *m, unsigned int *array)
{
	int i;

	for (i = 0; i < 32; i++)
		seq_printf(m, "%u ", array[i]);
	seq_putc(m, '\n');
}

static void dasd_stats_seq_print(struct seq_file *m,
				 struct dasd_profile_info *data)
{
	seq_printf(m, "start_time %ld.%09ld\n",
		   data->starttod.tv_sec, data->starttod.tv_nsec);
	seq_printf(m, "total_requests %u\n", data->dasd_io_reqs);
	seq_printf(m, "total_sectors %u\n", data->dasd_io_sects);
	seq_printf(m, "total_pav %u\n", data->dasd_io_alias);
	seq_printf(m, "total_hpf %u\n", data->dasd_io_tpm);
	seq_printf(m, "histogram_sectors ");
	dasd_stats_array(m, data->dasd_io_secs);
	seq_printf(m, "histogram_io_times ");
	dasd_stats_array(m, data->dasd_io_times);
	seq_printf(m, "histogram_io_times_weighted ");
	dasd_stats_array(m, data->dasd_io_timps);
	seq_printf(m, "histogram_time_build_to_ssch ");
	dasd_stats_array(m, data->dasd_io_time1);
	seq_printf(m, "histogram_time_ssch_to_irq ");
	dasd_stats_array(m, data->dasd_io_time2);
	seq_printf(m, "histogram_time_ssch_to_irq_weighted ");
	dasd_stats_array(m, data->dasd_io_time2ps);
	seq_printf(m, "histogram_time_irq_to_end ");
	dasd_stats_array(m, data->dasd_io_time3);
	seq_printf(m, "histogram_ccw_queue_length ");
	dasd_stats_array(m, data->dasd_io_nr_req);
	seq_printf(m, "total_read_requests %u\n", data->dasd_read_reqs);
	seq_printf(m, "total_read_sectors %u\n", data->dasd_read_sects);
	seq_printf(m, "total_read_pav %u\n", data->dasd_read_alias);
	seq_printf(m, "total_read_hpf %u\n", data->dasd_read_tpm);
	seq_printf(m, "histogram_read_sectors ");
	dasd_stats_array(m, data->dasd_read_secs);
	seq_printf(m, "histogram_read_times ");
	dasd_stats_array(m, data->dasd_read_times);
	seq_printf(m, "histogram_read_time_build_to_ssch ");
	dasd_stats_array(m, data->dasd_read_time1);
	seq_printf(m, "histogram_read_time_ssch_to_irq ");
	dasd_stats_array(m, data->dasd_read_time2);
	seq_printf(m, "histogram_read_time_irq_to_end ");
	dasd_stats_array(m, data->dasd_read_time3);
	seq_printf(m, "histogram_read_ccw_queue_length ");
	dasd_stats_array(m, data->dasd_read_nr_req);
}

static int dasd_stats_show(struct seq_file *m, void *v)
{
	struct dasd_profile *profile;
	struct dasd_profile_info *data;

	profile = m->private;
	spin_lock_bh(&profile->lock);
	data = profile->data;
	if (!data) {
		spin_unlock_bh(&profile->lock);
		seq_printf(m, "disabled\n");
		return 0;
	}
	dasd_stats_seq_print(m, data);
	spin_unlock_bh(&profile->lock);
	return 0;
}

static int dasd_stats_open(struct inode *inode, struct file *file)
{
	struct dasd_profile *profile = inode->i_private;
	return single_open(file, dasd_stats_show, profile);
}

static const struct file_operations dasd_stats_raw_fops = {
	.owner		= THIS_MODULE,
	.open		= dasd_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= dasd_stats_write,
};

static ssize_t dasd_stats_global_write(struct file *file,
				       const char __user *user_buf,
				       size_t user_len, loff_t *pos)
{
	char *buffer, *str;
	ssize_t rc;

	if (user_len > 65536)
		user_len = 65536;
	buffer = dasd_get_user_string(user_buf, user_len);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);
	str = skip_spaces(buffer);
	rc = user_len;
	if (strncmp(str, "reset", 5) == 0) {
		dasd_global_profile_reset();
	} else if (strncmp(str, "on", 2) == 0) {
		dasd_global_profile_reset();
		dasd_global_profile_level = DASD_PROFILE_GLOBAL_ONLY;
	} else if (strncmp(str, "off", 3) == 0) {
		dasd_global_profile_level = DASD_PROFILE_OFF;
	} else
		rc = -EINVAL;
	vfree(buffer);
	return rc;
}

static int dasd_stats_global_show(struct seq_file *m, void *v)
{
	if (!dasd_global_profile_level) {
		seq_printf(m, "disabled\n");
		return 0;
	}
	dasd_stats_seq_print(m, &dasd_global_profile_data);
	return 0;
}

static int dasd_stats_global_open(struct inode *inode, struct file *file)
{
	return single_open(file, dasd_stats_global_show, NULL);
}

static const struct file_operations dasd_stats_global_fops = {
	.owner		= THIS_MODULE,
	.open		= dasd_stats_global_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= dasd_stats_global_write,
};

static void dasd_profile_init(struct dasd_profile *profile,
			      struct dentry *base_dentry)
{
	umode_t mode;
	struct dentry *pde;

	if (!base_dentry)
		return;
	profile->dentry = NULL;
	profile->data = NULL;
	mode = (S_IRUSR | S_IWUSR | S_IFREG);
	pde = debugfs_create_file("statistics", mode, base_dentry,
				  profile, &dasd_stats_raw_fops);
	if (pde && !IS_ERR(pde))
		profile->dentry = pde;
	return;
}

static void dasd_profile_exit(struct dasd_profile *profile)
{
	dasd_profile_off(profile);
	if (profile->dentry) {
		debugfs_remove(profile->dentry);
		profile->dentry = NULL;
	}
}

static void dasd_statistics_removeroot(void)
{
	dasd_global_profile_level = DASD_PROFILE_OFF;
	if (dasd_global_profile_dentry) {
		debugfs_remove(dasd_global_profile_dentry);
		dasd_global_profile_dentry = NULL;
	}
	if (dasd_debugfs_global_entry)
		debugfs_remove(dasd_debugfs_global_entry);
	if (dasd_debugfs_root_entry)
		debugfs_remove(dasd_debugfs_root_entry);
}

static void dasd_statistics_createroot(void)
{
	umode_t mode;
	struct dentry *pde;

	dasd_debugfs_root_entry = NULL;
	dasd_debugfs_global_entry = NULL;
	dasd_global_profile_dentry = NULL;
	pde = debugfs_create_dir("dasd", NULL);
	if (!pde || IS_ERR(pde))
		goto error;
	dasd_debugfs_root_entry = pde;
	pde = debugfs_create_dir("global", dasd_debugfs_root_entry);
	if (!pde || IS_ERR(pde))
		goto error;
	dasd_debugfs_global_entry = pde;

	mode = (S_IRUSR | S_IWUSR | S_IFREG);
	pde = debugfs_create_file("statistics", mode, dasd_debugfs_global_entry,
				  NULL, &dasd_stats_global_fops);
	if (!pde || IS_ERR(pde))
		goto error;
	dasd_global_profile_dentry = pde;
	return;

error:
	DBF_EVENT(DBF_ERR, "%s",
		  "Creation of the dasd debugfs interface failed");
	dasd_statistics_removeroot();
	return;
}

#else
#define dasd_profile_start(block, cqr, req) do {} while (0)
#define dasd_profile_end(block, cqr, req) do {} while (0)

static void dasd_statistics_createroot(void)
{
	return;
}

static void dasd_statistics_removeroot(void)
{
	return;
}

int dasd_stats_generic_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Statistics are not activated in this kernel\n");
	return 0;
}

static void dasd_profile_init(struct dasd_profile *profile,
			      struct dentry *base_dentry)
{
	return;
}

static void dasd_profile_exit(struct dasd_profile *profile)
{
	return;
}

int dasd_profile_on(struct dasd_profile *profile)
{
	return 0;
}

#endif				

struct dasd_ccw_req *dasd_kmalloc_request(int magic, int cplength,
					  int datasize,
					  struct dasd_device *device)
{
	struct dasd_ccw_req *cqr;

	
	BUG_ON(datasize > PAGE_SIZE ||
	     (cplength*sizeof(struct ccw1)) > PAGE_SIZE);

	cqr = kzalloc(sizeof(struct dasd_ccw_req), GFP_ATOMIC);
	if (cqr == NULL)
		return ERR_PTR(-ENOMEM);
	cqr->cpaddr = NULL;
	if (cplength > 0) {
		cqr->cpaddr = kcalloc(cplength, sizeof(struct ccw1),
				      GFP_ATOMIC | GFP_DMA);
		if (cqr->cpaddr == NULL) {
			kfree(cqr);
			return ERR_PTR(-ENOMEM);
		}
	}
	cqr->data = NULL;
	if (datasize > 0) {
		cqr->data = kzalloc(datasize, GFP_ATOMIC | GFP_DMA);
		if (cqr->data == NULL) {
			kfree(cqr->cpaddr);
			kfree(cqr);
			return ERR_PTR(-ENOMEM);
		}
	}
	cqr->magic =  magic;
	set_bit(DASD_CQR_FLAGS_USE_ERP, &cqr->flags);
	dasd_get_device(device);
	return cqr;
}

struct dasd_ccw_req *dasd_smalloc_request(int magic, int cplength,
					  int datasize,
					  struct dasd_device *device)
{
	unsigned long flags;
	struct dasd_ccw_req *cqr;
	char *data;
	int size;

	size = (sizeof(struct dasd_ccw_req) + 7L) & -8L;
	if (cplength > 0)
		size += cplength * sizeof(struct ccw1);
	if (datasize > 0)
		size += datasize;
	spin_lock_irqsave(&device->mem_lock, flags);
	cqr = (struct dasd_ccw_req *)
		dasd_alloc_chunk(&device->ccw_chunks, size);
	spin_unlock_irqrestore(&device->mem_lock, flags);
	if (cqr == NULL)
		return ERR_PTR(-ENOMEM);
	memset(cqr, 0, sizeof(struct dasd_ccw_req));
	data = (char *) cqr + ((sizeof(struct dasd_ccw_req) + 7L) & -8L);
	cqr->cpaddr = NULL;
	if (cplength > 0) {
		cqr->cpaddr = (struct ccw1 *) data;
		data += cplength*sizeof(struct ccw1);
		memset(cqr->cpaddr, 0, cplength*sizeof(struct ccw1));
	}
	cqr->data = NULL;
	if (datasize > 0) {
		cqr->data = data;
 		memset(cqr->data, 0, datasize);
	}
	cqr->magic = magic;
	set_bit(DASD_CQR_FLAGS_USE_ERP, &cqr->flags);
	dasd_get_device(device);
	return cqr;
}

void dasd_kfree_request(struct dasd_ccw_req *cqr, struct dasd_device *device)
{
#ifdef CONFIG_64BIT
	struct ccw1 *ccw;

	
	ccw = cqr->cpaddr;
	do {
		clear_normalized_cda(ccw);
	} while (ccw++->flags & (CCW_FLAG_CC | CCW_FLAG_DC));
#endif
	kfree(cqr->cpaddr);
	kfree(cqr->data);
	kfree(cqr);
	dasd_put_device(device);
}

void dasd_sfree_request(struct dasd_ccw_req *cqr, struct dasd_device *device)
{
	unsigned long flags;

	spin_lock_irqsave(&device->mem_lock, flags);
	dasd_free_chunk(&device->ccw_chunks, cqr);
	spin_unlock_irqrestore(&device->mem_lock, flags);
	dasd_put_device(device);
}

static inline int dasd_check_cqr(struct dasd_ccw_req *cqr)
{
	struct dasd_device *device;

	if (cqr == NULL)
		return -EINVAL;
	device = cqr->startdev;
	if (strncmp((char *) &cqr->magic, device->discipline->ebcname, 4)) {
		DBF_DEV_EVENT(DBF_WARNING, device,
			    " dasd_ccw_req 0x%08x magic doesn't match"
			    " discipline 0x%08x",
			    cqr->magic,
			    *(unsigned int *) device->discipline->name);
		return -EINVAL;
	}
	return 0;
}

int dasd_term_IO(struct dasd_ccw_req *cqr)
{
	struct dasd_device *device;
	int retries, rc;
	char errorstring[ERRORLENGTH];

	
	rc = dasd_check_cqr(cqr);
	if (rc)
		return rc;
	retries = 0;
	device = (struct dasd_device *) cqr->startdev;
	while ((retries < 5) && (cqr->status == DASD_CQR_IN_IO)) {
		rc = ccw_device_clear(device->cdev, (long) cqr);
		switch (rc) {
		case 0:	
			cqr->status = DASD_CQR_CLEAR_PENDING;
			cqr->stopclk = get_clock();
			cqr->starttime = 0;
			DBF_DEV_EVENT(DBF_DEBUG, device,
				      "terminate cqr %p successful",
				      cqr);
			break;
		case -ENODEV:
			DBF_DEV_EVENT(DBF_ERR, device, "%s",
				      "device gone, retry");
			break;
		case -EIO:
			DBF_DEV_EVENT(DBF_ERR, device, "%s",
				      "I/O error, retry");
			break;
		case -EINVAL:
		case -EBUSY:
			DBF_DEV_EVENT(DBF_ERR, device, "%s",
				      "device busy, retry later");
			break;
		default:
			
			snprintf(errorstring, ERRORLENGTH, "10 %d", rc);
			dev_err(&device->cdev->dev, "An error occurred in the "
				"DASD device driver, reason=%s\n", errorstring);
			BUG();
			break;
		}
		retries++;
	}
	dasd_schedule_device_bh(device);
	return rc;
}

int dasd_start_IO(struct dasd_ccw_req *cqr)
{
	struct dasd_device *device;
	int rc;
	char errorstring[ERRORLENGTH];

	
	rc = dasd_check_cqr(cqr);
	if (rc) {
		cqr->intrc = rc;
		return rc;
	}
	device = (struct dasd_device *) cqr->startdev;
	if (((cqr->block &&
	      test_bit(DASD_FLAG_LOCK_STOLEN, &cqr->block->base->flags)) ||
	     test_bit(DASD_FLAG_LOCK_STOLEN, &device->flags)) &&
	    !test_bit(DASD_CQR_ALLOW_SLOCK, &cqr->flags)) {
		DBF_DEV_EVENT(DBF_DEBUG, device, "start_IO: return request %p "
			      "because of stolen lock", cqr);
		cqr->status = DASD_CQR_ERROR;
		cqr->intrc = -EPERM;
		return -EPERM;
	}
	if (cqr->retries < 0) {
		
		sprintf(errorstring, "14 %p", cqr);
		dev_err(&device->cdev->dev, "An error occurred in the DASD "
			"device driver, reason=%s\n", errorstring);
		cqr->status = DASD_CQR_ERROR;
		return -EIO;
	}
	cqr->startclk = get_clock();
	cqr->starttime = jiffies;
	cqr->retries--;
	if (!test_bit(DASD_CQR_VERIFY_PATH, &cqr->flags)) {
		cqr->lpm &= device->path_data.opm;
		if (!cqr->lpm)
			cqr->lpm = device->path_data.opm;
	}
	if (cqr->cpmode == 1) {
		rc = ccw_device_tm_start(device->cdev, cqr->cpaddr,
					 (long) cqr, cqr->lpm);
	} else {
		rc = ccw_device_start(device->cdev, cqr->cpaddr,
				      (long) cqr, cqr->lpm, 0);
	}
	switch (rc) {
	case 0:
		cqr->status = DASD_CQR_IN_IO;
		break;
	case -EBUSY:
		DBF_DEV_EVENT(DBF_WARNING, device, "%s",
			      "start_IO: device busy, retry later");
		break;
	case -ETIMEDOUT:
		DBF_DEV_EVENT(DBF_WARNING, device, "%s",
			      "start_IO: request timeout, retry later");
		break;
	case -EACCES:
		if (test_bit(DASD_CQR_VERIFY_PATH, &cqr->flags)) {
			DBF_DEV_EVENT(DBF_WARNING, device,
				      "start_IO: selected paths gone (%x)",
				      cqr->lpm);
		} else if (cqr->lpm != device->path_data.opm) {
			cqr->lpm = device->path_data.opm;
			DBF_DEV_EVENT(DBF_DEBUG, device, "%s",
				      "start_IO: selected paths gone,"
				      " retry on all paths");
		} else {
			DBF_DEV_EVENT(DBF_WARNING, device, "%s",
				      "start_IO: all paths in opm gone,"
				      " do path verification");
			dasd_generic_last_path_gone(device);
			device->path_data.opm = 0;
			device->path_data.ppm = 0;
			device->path_data.npm = 0;
			device->path_data.tbvpm =
				ccw_device_get_path_mask(device->cdev);
		}
		break;
	case -ENODEV:
		DBF_DEV_EVENT(DBF_WARNING, device, "%s",
			      "start_IO: -ENODEV device gone, retry");
		break;
	case -EIO:
		DBF_DEV_EVENT(DBF_WARNING, device, "%s",
			      "start_IO: -EIO device gone, retry");
		break;
	case -EINVAL:
		
		DBF_DEV_EVENT(DBF_WARNING, device, "%s",
			      "start_IO: -EINVAL device currently "
			      "not accessible");
		break;
	default:
		
		snprintf(errorstring, ERRORLENGTH, "11 %d", rc);
		dev_err(&device->cdev->dev,
			"An error occurred in the DASD device driver, "
			"reason=%s\n", errorstring);
		BUG();
		break;
	}
	cqr->intrc = rc;
	return rc;
}

static void dasd_device_timeout(unsigned long ptr)
{
	unsigned long flags;
	struct dasd_device *device;

	device = (struct dasd_device *) ptr;
	spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
	
	dasd_device_remove_stop_bits(device, DASD_STOPPED_PENDING);
	spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);
	dasd_schedule_device_bh(device);
}

void dasd_device_set_timer(struct dasd_device *device, int expires)
{
	if (expires == 0)
		del_timer(&device->timer);
	else
		mod_timer(&device->timer, jiffies + expires);
}

void dasd_device_clear_timer(struct dasd_device *device)
{
	del_timer(&device->timer);
}

static void dasd_handle_killed_request(struct ccw_device *cdev,
				       unsigned long intparm)
{
	struct dasd_ccw_req *cqr;
	struct dasd_device *device;

	if (!intparm)
		return;
	cqr = (struct dasd_ccw_req *) intparm;
	if (cqr->status != DASD_CQR_IN_IO) {
		DBF_EVENT_DEVID(DBF_DEBUG, cdev,
				"invalid status in handle_killed_request: "
				"%02x", cqr->status);
		return;
	}

	device = dasd_device_from_cdev_locked(cdev);
	if (IS_ERR(device)) {
		DBF_EVENT_DEVID(DBF_DEBUG, cdev, "%s",
				"unable to get device from cdev");
		return;
	}

	if (!cqr->startdev ||
	    device != cqr->startdev ||
	    strncmp(cqr->startdev->discipline->ebcname,
		    (char *) &cqr->magic, 4)) {
		DBF_EVENT_DEVID(DBF_DEBUG, cdev, "%s",
				"invalid device in request");
		dasd_put_device(device);
		return;
	}

	
	cqr->status = DASD_CQR_QUEUED;

	dasd_device_clear_timer(device);
	dasd_schedule_device_bh(device);
	dasd_put_device(device);
}

void dasd_generic_handle_state_change(struct dasd_device *device)
{
	
	dasd_eer_snss(device);

	dasd_device_remove_stop_bits(device, DASD_STOPPED_PENDING);
	dasd_schedule_device_bh(device);
	if (device->block)
		dasd_schedule_block_bh(device->block);
}

void dasd_int_handler(struct ccw_device *cdev, unsigned long intparm,
		      struct irb *irb)
{
	struct dasd_ccw_req *cqr, *next;
	struct dasd_device *device;
	unsigned long long now;
	int expires;

	if (IS_ERR(irb)) {
		switch (PTR_ERR(irb)) {
		case -EIO:
			break;
		case -ETIMEDOUT:
			DBF_EVENT_DEVID(DBF_WARNING, cdev, "%s: "
					"request timed out\n", __func__);
			break;
		default:
			DBF_EVENT_DEVID(DBF_WARNING, cdev, "%s: "
					"unknown error %ld\n", __func__,
					PTR_ERR(irb));
		}
		dasd_handle_killed_request(cdev, intparm);
		return;
	}

	now = get_clock();
	cqr = (struct dasd_ccw_req *) intparm;
	
	if (!cqr ||
	    !(scsw_dstat(&irb->scsw) == (DEV_STAT_CHN_END | DEV_STAT_DEV_END) &&
	      scsw_cstat(&irb->scsw) == 0)) {
		if (cqr)
			memcpy(&cqr->irb, irb, sizeof(*irb));
		device = dasd_device_from_cdev_locked(cdev);
		if (IS_ERR(device))
			return;
		
		if (device->discipline == dasd_diag_discipline_pointer) {
			dasd_put_device(device);
			return;
		}
		device->discipline->dump_sense_dbf(device, irb, "int");
		if (device->features & DASD_FEATURE_ERPLOG)
			device->discipline->dump_sense(device, cqr, irb);
		device->discipline->check_for_device_change(device, cqr, irb);
		dasd_put_device(device);
	}
	if (!cqr)
		return;

	device = (struct dasd_device *) cqr->startdev;
	if (!device ||
	    strncmp(device->discipline->ebcname, (char *) &cqr->magic, 4)) {
		DBF_EVENT_DEVID(DBF_DEBUG, cdev, "%s",
				"invalid device in request");
		return;
	}

	
	if (cqr->status == DASD_CQR_CLEAR_PENDING &&
	    scsw_fctl(&irb->scsw) & SCSW_FCTL_CLEAR_FUNC) {
		cqr->status = DASD_CQR_CLEARED;
		dasd_device_clear_timer(device);
		wake_up(&dasd_flush_wq);
		dasd_schedule_device_bh(device);
		return;
	}

	
	if (cqr->status != DASD_CQR_IN_IO) {
		DBF_DEV_EVENT(DBF_DEBUG, device, "invalid status: bus_id %s, "
			      "status %02x", dev_name(&cdev->dev), cqr->status);
		return;
	}

	next = NULL;
	expires = 0;
	if (scsw_dstat(&irb->scsw) == (DEV_STAT_CHN_END | DEV_STAT_DEV_END) &&
	    scsw_cstat(&irb->scsw) == 0) {
		
		cqr->status = DASD_CQR_SUCCESS;
		cqr->stopclk = now;
		
		if (cqr->devlist.next != &device->ccw_queue) {
			next = list_entry(cqr->devlist.next,
					  struct dasd_ccw_req, devlist);
		}
	} else {  
		if (!test_bit(DASD_CQR_FLAGS_USE_ERP, &cqr->flags) &&
		    cqr->retries > 0) {
			if (cqr->lpm == device->path_data.opm)
				DBF_DEV_EVENT(DBF_DEBUG, device,
					      "default ERP in fastpath "
					      "(%i retries left)",
					      cqr->retries);
			if (!test_bit(DASD_CQR_VERIFY_PATH, &cqr->flags))
				cqr->lpm = device->path_data.opm;
			cqr->status = DASD_CQR_QUEUED;
			next = cqr;
		} else
			cqr->status = DASD_CQR_ERROR;
	}
	if (next && (next->status == DASD_CQR_QUEUED) &&
	    (!device->stopped)) {
		if (device->discipline->start_IO(next) == 0)
			expires = next->expires;
	}
	if (expires != 0)
		dasd_device_set_timer(device, expires);
	else
		dasd_device_clear_timer(device);
	dasd_schedule_device_bh(device);
}

enum uc_todo dasd_generic_uc_handler(struct ccw_device *cdev, struct irb *irb)
{
	struct dasd_device *device;

	device = dasd_device_from_cdev_locked(cdev);

	if (IS_ERR(device))
		goto out;
	if (test_bit(DASD_FLAG_OFFLINE, &device->flags) ||
	   device->state != device->target ||
	   !device->discipline->check_for_device_change){
		dasd_put_device(device);
		goto out;
	}
	if (device->discipline->dump_sense_dbf)
		device->discipline->dump_sense_dbf(device, irb, "uc");
	device->discipline->check_for_device_change(device, NULL, irb);
	dasd_put_device(device);
out:
	return UC_TODO_RETRY;
}
EXPORT_SYMBOL_GPL(dasd_generic_uc_handler);

static void __dasd_device_recovery(struct dasd_device *device,
				   struct dasd_ccw_req *ref_cqr)
{
	struct list_head *l, *n;
	struct dasd_ccw_req *cqr;

	if (!ref_cqr->block)
		return;

	list_for_each_safe(l, n, &device->ccw_queue) {
		cqr = list_entry(l, struct dasd_ccw_req, devlist);
		if (cqr->status == DASD_CQR_QUEUED &&
		    ref_cqr->block == cqr->block) {
			cqr->status = DASD_CQR_CLEARED;
		}
	}
};

static void __dasd_device_process_ccw_queue(struct dasd_device *device,
					    struct list_head *final_queue)
{
	struct list_head *l, *n;
	struct dasd_ccw_req *cqr;

	
	list_for_each_safe(l, n, &device->ccw_queue) {
		cqr = list_entry(l, struct dasd_ccw_req, devlist);

		
		if (cqr->status == DASD_CQR_QUEUED ||
		    cqr->status == DASD_CQR_IN_IO ||
		    cqr->status == DASD_CQR_CLEAR_PENDING)
			break;
		if (cqr->status == DASD_CQR_ERROR) {
			__dasd_device_recovery(device, cqr);
		}
		
		list_move_tail(&cqr->devlist, final_queue);
	}
}

static void __dasd_device_process_final_queue(struct dasd_device *device,
					      struct list_head *final_queue)
{
	struct list_head *l, *n;
	struct dasd_ccw_req *cqr;
	struct dasd_block *block;
	void (*callback)(struct dasd_ccw_req *, void *data);
	void *callback_data;
	char errorstring[ERRORLENGTH];

	list_for_each_safe(l, n, final_queue) {
		cqr = list_entry(l, struct dasd_ccw_req, devlist);
		list_del_init(&cqr->devlist);
		block = cqr->block;
		callback = cqr->callback;
		callback_data = cqr->callback_data;
		if (block)
			spin_lock_bh(&block->queue_lock);
		switch (cqr->status) {
		case DASD_CQR_SUCCESS:
			cqr->status = DASD_CQR_DONE;
			break;
		case DASD_CQR_ERROR:
			cqr->status = DASD_CQR_NEED_ERP;
			break;
		case DASD_CQR_CLEARED:
			cqr->status = DASD_CQR_TERMINATED;
			break;
		default:
			
			snprintf(errorstring, ERRORLENGTH, "12 %p %x02", cqr, cqr->status);
			dev_err(&device->cdev->dev,
				"An error occurred in the DASD device driver, "
				"reason=%s\n", errorstring);
			BUG();
		}
		if (cqr->callback != NULL)
			(callback)(cqr, callback_data);
		if (block)
			spin_unlock_bh(&block->queue_lock);
	}
}

static void __dasd_device_check_expire(struct dasd_device *device)
{
	struct dasd_ccw_req *cqr;

	if (list_empty(&device->ccw_queue))
		return;
	cqr = list_entry(device->ccw_queue.next, struct dasd_ccw_req, devlist);
	if ((cqr->status == DASD_CQR_IN_IO && cqr->expires != 0) &&
	    (time_after_eq(jiffies, cqr->expires + cqr->starttime))) {
		if (device->discipline->term_IO(cqr) != 0) {
			
			dev_err(&device->cdev->dev,
				"cqr %p timed out (%lus) but cannot be "
				"ended, retrying in 5 s\n",
				cqr, (cqr->expires/HZ));
			cqr->expires += 5*HZ;
			dasd_device_set_timer(device, 5*HZ);
		} else {
			dev_err(&device->cdev->dev,
				"cqr %p timed out (%lus), %i retries "
				"remaining\n", cqr, (cqr->expires/HZ),
				cqr->retries);
		}
	}
}

static void __dasd_device_start_head(struct dasd_device *device)
{
	struct dasd_ccw_req *cqr;
	int rc;

	if (list_empty(&device->ccw_queue))
		return;
	cqr = list_entry(device->ccw_queue.next, struct dasd_ccw_req, devlist);
	if (cqr->status != DASD_CQR_QUEUED)
		return;
	if (device->stopped &&
	    !(!(device->stopped & ~(DASD_STOPPED_DC_WAIT | DASD_UNRESUMED_PM))
	      && test_bit(DASD_CQR_VERIFY_PATH, &cqr->flags))) {
		cqr->intrc = -EAGAIN;
		cqr->status = DASD_CQR_CLEARED;
		dasd_schedule_device_bh(device);
		return;
	}

	rc = device->discipline->start_IO(cqr);
	if (rc == 0)
		dasd_device_set_timer(device, cqr->expires);
	else if (rc == -EACCES) {
		dasd_schedule_device_bh(device);
	} else
		
		dasd_device_set_timer(device, 50);
}

static void __dasd_device_check_path_events(struct dasd_device *device)
{
	int rc;

	if (device->path_data.tbvpm) {
		if (device->stopped & ~(DASD_STOPPED_DC_WAIT |
					DASD_UNRESUMED_PM))
			return;
		rc = device->discipline->verify_path(
			device, device->path_data.tbvpm);
		if (rc)
			dasd_device_set_timer(device, 50);
		else
			device->path_data.tbvpm = 0;
	}
};

int dasd_flush_device_queue(struct dasd_device *device)
{
	struct dasd_ccw_req *cqr, *n;
	int rc;
	struct list_head flush_queue;

	INIT_LIST_HEAD(&flush_queue);
	spin_lock_irq(get_ccwdev_lock(device->cdev));
	rc = 0;
	list_for_each_entry_safe(cqr, n, &device->ccw_queue, devlist) {
		
		switch (cqr->status) {
		case DASD_CQR_IN_IO:
			rc = device->discipline->term_IO(cqr);
			if (rc) {
				
				dev_err(&device->cdev->dev,
					"Flushing the DASD request queue "
					"failed for request %p\n", cqr);
				
				goto finished;
			}
			break;
		case DASD_CQR_QUEUED:
			cqr->stopclk = get_clock();
			cqr->status = DASD_CQR_CLEARED;
			break;
		default: 
			break;
		}
		list_move_tail(&cqr->devlist, &flush_queue);
	}
finished:
	spin_unlock_irq(get_ccwdev_lock(device->cdev));
	list_for_each_entry_safe(cqr, n, &flush_queue, devlist)
		wait_event(dasd_flush_wq,
			   (cqr->status != DASD_CQR_CLEAR_PENDING));
	__dasd_device_process_final_queue(device, &flush_queue);
	return rc;
}

static void dasd_device_tasklet(struct dasd_device *device)
{
	struct list_head final_queue;

	atomic_set (&device->tasklet_scheduled, 0);
	INIT_LIST_HEAD(&final_queue);
	spin_lock_irq(get_ccwdev_lock(device->cdev));
	
	__dasd_device_check_expire(device);
	
	__dasd_device_process_ccw_queue(device, &final_queue);
	__dasd_device_check_path_events(device);
	spin_unlock_irq(get_ccwdev_lock(device->cdev));
	
	__dasd_device_process_final_queue(device, &final_queue);
	spin_lock_irq(get_ccwdev_lock(device->cdev));
	
	__dasd_device_start_head(device);
	spin_unlock_irq(get_ccwdev_lock(device->cdev));
	dasd_put_device(device);
}

void dasd_schedule_device_bh(struct dasd_device *device)
{
	
	if (atomic_cmpxchg (&device->tasklet_scheduled, 0, 1) != 0)
		return;
	dasd_get_device(device);
	tasklet_hi_schedule(&device->tasklet);
}

void dasd_device_set_stop_bits(struct dasd_device *device, int bits)
{
	device->stopped |= bits;
}
EXPORT_SYMBOL_GPL(dasd_device_set_stop_bits);

void dasd_device_remove_stop_bits(struct dasd_device *device, int bits)
{
	device->stopped &= ~bits;
	if (!device->stopped)
		wake_up(&generic_waitq);
}
EXPORT_SYMBOL_GPL(dasd_device_remove_stop_bits);

void dasd_add_request_head(struct dasd_ccw_req *cqr)
{
	struct dasd_device *device;
	unsigned long flags;

	device = cqr->startdev;
	spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
	cqr->status = DASD_CQR_QUEUED;
	list_add(&cqr->devlist, &device->ccw_queue);
	
	dasd_schedule_device_bh(device);
	spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);
}

void dasd_add_request_tail(struct dasd_ccw_req *cqr)
{
	struct dasd_device *device;
	unsigned long flags;

	device = cqr->startdev;
	spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
	cqr->status = DASD_CQR_QUEUED;
	list_add_tail(&cqr->devlist, &device->ccw_queue);
	
	dasd_schedule_device_bh(device);
	spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);
}

void dasd_wakeup_cb(struct dasd_ccw_req *cqr, void *data)
{
	spin_lock_irq(get_ccwdev_lock(cqr->startdev->cdev));
	cqr->callback_data = DASD_SLEEPON_END_TAG;
	spin_unlock_irq(get_ccwdev_lock(cqr->startdev->cdev));
	wake_up(&generic_waitq);
}
EXPORT_SYMBOL_GPL(dasd_wakeup_cb);

static inline int _wait_for_wakeup(struct dasd_ccw_req *cqr)
{
	struct dasd_device *device;
	int rc;

	device = cqr->startdev;
	spin_lock_irq(get_ccwdev_lock(device->cdev));
	rc = (cqr->callback_data == DASD_SLEEPON_END_TAG);
	spin_unlock_irq(get_ccwdev_lock(device->cdev));
	return rc;
}

static int __dasd_sleep_on_erp(struct dasd_ccw_req *cqr)
{
	struct dasd_device *device;
	dasd_erp_fn_t erp_fn;

	if (cqr->status == DASD_CQR_FILLED)
		return 0;
	device = cqr->startdev;
	if (test_bit(DASD_CQR_FLAGS_USE_ERP, &cqr->flags)) {
		if (cqr->status == DASD_CQR_TERMINATED) {
			device->discipline->handle_terminated_request(cqr);
			return 1;
		}
		if (cqr->status == DASD_CQR_NEED_ERP) {
			erp_fn = device->discipline->erp_action(cqr);
			erp_fn(cqr);
			return 1;
		}
		if (cqr->status == DASD_CQR_FAILED)
			dasd_log_sense(cqr, &cqr->irb);
		if (cqr->refers) {
			__dasd_process_erp(device, cqr);
			return 1;
		}
	}
	return 0;
}

static int __dasd_sleep_on_loop_condition(struct dasd_ccw_req *cqr)
{
	if (test_bit(DASD_CQR_FLAGS_USE_ERP, &cqr->flags)) {
		if (cqr->refers) 
			return 1;
		return ((cqr->status != DASD_CQR_DONE) &&
			(cqr->status != DASD_CQR_FAILED));
	} else
		return (cqr->status == DASD_CQR_FILLED);
}

static int _dasd_sleep_on(struct dasd_ccw_req *maincqr, int interruptible)
{
	struct dasd_device *device;
	int rc;
	struct list_head ccw_queue;
	struct dasd_ccw_req *cqr;

	INIT_LIST_HEAD(&ccw_queue);
	maincqr->status = DASD_CQR_FILLED;
	device = maincqr->startdev;
	list_add(&maincqr->blocklist, &ccw_queue);
	for (cqr = maincqr;  __dasd_sleep_on_loop_condition(cqr);
	     cqr = list_first_entry(&ccw_queue,
				    struct dasd_ccw_req, blocklist)) {

		if (__dasd_sleep_on_erp(cqr))
			continue;
		if (cqr->status != DASD_CQR_FILLED) 
			continue;
		if (test_bit(DASD_FLAG_LOCK_STOLEN, &device->flags) &&
		    !test_bit(DASD_CQR_ALLOW_SLOCK, &cqr->flags)) {
			cqr->status = DASD_CQR_FAILED;
			cqr->intrc = -EPERM;
			continue;
		}
		
		if (device->stopped & ~DASD_STOPPED_PENDING &&
		    test_bit(DASD_CQR_FLAGS_FAILFAST, &cqr->flags) &&
		    (!dasd_eer_enabled(device))) {
			cqr->status = DASD_CQR_FAILED;
			continue;
		}
		
		if (interruptible) {
			rc = wait_event_interruptible(
				generic_waitq, !(device->stopped));
			if (rc == -ERESTARTSYS) {
				cqr->status = DASD_CQR_FAILED;
				maincqr->intrc = rc;
				continue;
			}
		} else
			wait_event(generic_waitq, !(device->stopped));

		if (!cqr->callback)
			cqr->callback = dasd_wakeup_cb;

		cqr->callback_data = DASD_SLEEPON_START_TAG;
		dasd_add_request_tail(cqr);
		if (interruptible) {
			rc = wait_event_interruptible(
				generic_waitq, _wait_for_wakeup(cqr));
			if (rc == -ERESTARTSYS) {
				dasd_cancel_req(cqr);
				
				wait_event(generic_waitq,
					   _wait_for_wakeup(cqr));
				cqr->status = DASD_CQR_FAILED;
				maincqr->intrc = rc;
				continue;
			}
		} else
			wait_event(generic_waitq, _wait_for_wakeup(cqr));
	}

	maincqr->endclk = get_clock();
	if ((maincqr->status != DASD_CQR_DONE) &&
	    (maincqr->intrc != -ERESTARTSYS))
		dasd_log_sense(maincqr, &maincqr->irb);
	if (maincqr->status == DASD_CQR_DONE)
		rc = 0;
	else if (maincqr->intrc)
		rc = maincqr->intrc;
	else
		rc = -EIO;
	return rc;
}

int dasd_sleep_on(struct dasd_ccw_req *cqr)
{
	return _dasd_sleep_on(cqr, 0);
}

int dasd_sleep_on_interruptible(struct dasd_ccw_req *cqr)
{
	return _dasd_sleep_on(cqr, 1);
}

static inline int _dasd_term_running_cqr(struct dasd_device *device)
{
	struct dasd_ccw_req *cqr;
	int rc;

	if (list_empty(&device->ccw_queue))
		return 0;
	cqr = list_entry(device->ccw_queue.next, struct dasd_ccw_req, devlist);
	rc = device->discipline->term_IO(cqr);
	if (!rc)
		cqr->retries++;
	return rc;
}

int dasd_sleep_on_immediatly(struct dasd_ccw_req *cqr)
{
	struct dasd_device *device;
	int rc;

	device = cqr->startdev;
	if (test_bit(DASD_FLAG_LOCK_STOLEN, &device->flags) &&
	    !test_bit(DASD_CQR_ALLOW_SLOCK, &cqr->flags)) {
		cqr->status = DASD_CQR_FAILED;
		cqr->intrc = -EPERM;
		return -EIO;
	}
	spin_lock_irq(get_ccwdev_lock(device->cdev));
	rc = _dasd_term_running_cqr(device);
	if (rc) {
		spin_unlock_irq(get_ccwdev_lock(device->cdev));
		return rc;
	}
	cqr->callback = dasd_wakeup_cb;
	cqr->callback_data = DASD_SLEEPON_START_TAG;
	cqr->status = DASD_CQR_QUEUED;
	list_add(&cqr->devlist, device->ccw_queue.next);

	
	dasd_schedule_device_bh(device);

	spin_unlock_irq(get_ccwdev_lock(device->cdev));

	wait_event(generic_waitq, _wait_for_wakeup(cqr));

	if (cqr->status == DASD_CQR_DONE)
		rc = 0;
	else if (cqr->intrc)
		rc = cqr->intrc;
	else
		rc = -EIO;
	return rc;
}

int dasd_cancel_req(struct dasd_ccw_req *cqr)
{
	struct dasd_device *device = cqr->startdev;
	unsigned long flags;
	int rc;

	rc = 0;
	spin_lock_irqsave(get_ccwdev_lock(device->cdev), flags);
	switch (cqr->status) {
	case DASD_CQR_QUEUED:
		
		cqr->status = DASD_CQR_CLEARED;
		break;
	case DASD_CQR_IN_IO:
		
		rc = device->discipline->term_IO(cqr);
		if (rc) {
			dev_err(&device->cdev->dev,
				"Cancelling request %p failed with rc=%d\n",
				cqr, rc);
		} else {
			cqr->stopclk = get_clock();
		}
		break;
	default: 
		break;
	}
	spin_unlock_irqrestore(get_ccwdev_lock(device->cdev), flags);
	dasd_schedule_device_bh(device);
	return rc;
}



static void dasd_block_timeout(unsigned long ptr)
{
	unsigned long flags;
	struct dasd_block *block;

	block = (struct dasd_block *) ptr;
	spin_lock_irqsave(get_ccwdev_lock(block->base->cdev), flags);
	
	dasd_device_remove_stop_bits(block->base, DASD_STOPPED_PENDING);
	spin_unlock_irqrestore(get_ccwdev_lock(block->base->cdev), flags);
	dasd_schedule_block_bh(block);
}

void dasd_block_set_timer(struct dasd_block *block, int expires)
{
	if (expires == 0)
		del_timer(&block->timer);
	else
		mod_timer(&block->timer, jiffies + expires);
}

void dasd_block_clear_timer(struct dasd_block *block)
{
	del_timer(&block->timer);
}

static void __dasd_process_erp(struct dasd_device *device,
			       struct dasd_ccw_req *cqr)
{
	dasd_erp_fn_t erp_fn;

	if (cqr->status == DASD_CQR_DONE)
		DBF_DEV_EVENT(DBF_NOTICE, device, "%s", "ERP successful");
	else
		dev_err(&device->cdev->dev, "ERP failed for the DASD\n");
	erp_fn = device->discipline->erp_postaction(cqr);
	erp_fn(cqr);
}

static void __dasd_process_request_queue(struct dasd_block *block)
{
	struct request_queue *queue;
	struct request *req;
	struct dasd_ccw_req *cqr;
	struct dasd_device *basedev;
	unsigned long flags;
	queue = block->request_queue;
	basedev = block->base;
	
	if (queue == NULL)
		return;

	if (basedev->state < DASD_STATE_READY) {
		while ((req = blk_fetch_request(block->request_queue)))
			__blk_end_request_all(req, -EIO);
		return;
	}
	
	while ((req = blk_peek_request(queue))) {
		if (basedev->features & DASD_FEATURE_READONLY &&
		    rq_data_dir(req) == WRITE) {
			DBF_DEV_EVENT(DBF_ERR, basedev,
				      "Rejecting write request %p",
				      req);
			blk_start_request(req);
			__blk_end_request_all(req, -EIO);
			continue;
		}
		cqr = basedev->discipline->build_cp(basedev, block, req);
		if (IS_ERR(cqr)) {
			if (PTR_ERR(cqr) == -EBUSY)
				break;	
			if (PTR_ERR(cqr) == -ENOMEM)
				break;	
			if (PTR_ERR(cqr) == -EAGAIN) {
				if (!list_empty(&block->ccw_queue))
					break;
				spin_lock_irqsave(
					get_ccwdev_lock(basedev->cdev), flags);
				dasd_device_set_stop_bits(basedev,
							  DASD_STOPPED_PENDING);
				spin_unlock_irqrestore(
					get_ccwdev_lock(basedev->cdev), flags);
				dasd_block_set_timer(block, HZ/2);
				break;
			}
			DBF_DEV_EVENT(DBF_ERR, basedev,
				      "CCW creation failed (rc=%ld) "
				      "on request %p",
				      PTR_ERR(cqr), req);
			blk_start_request(req);
			__blk_end_request_all(req, -EIO);
			continue;
		}
		cqr->callback_data = (void *) req;
		cqr->status = DASD_CQR_FILLED;
		blk_start_request(req);
		list_add_tail(&cqr->blocklist, &block->ccw_queue);
		dasd_profile_start(block, cqr, req);
	}
}

static void __dasd_cleanup_cqr(struct dasd_ccw_req *cqr)
{
	struct request *req;
	int status;
	int error = 0;

	req = (struct request *) cqr->callback_data;
	dasd_profile_end(cqr->block, cqr, req);
	status = cqr->block->base->discipline->free_cp(cqr, req);
	if (status <= 0)
		error = status ? status : -EIO;
	__blk_end_request_all(req, error);
}

static void __dasd_process_block_ccw_queue(struct dasd_block *block,
					   struct list_head *final_queue)
{
	struct list_head *l, *n;
	struct dasd_ccw_req *cqr;
	dasd_erp_fn_t erp_fn;
	unsigned long flags;
	struct dasd_device *base = block->base;

restart:
	
	list_for_each_safe(l, n, &block->ccw_queue) {
		cqr = list_entry(l, struct dasd_ccw_req, blocklist);
		if (cqr->status != DASD_CQR_DONE &&
		    cqr->status != DASD_CQR_FAILED &&
		    cqr->status != DASD_CQR_NEED_ERP &&
		    cqr->status != DASD_CQR_TERMINATED)
			continue;

		if (cqr->status == DASD_CQR_TERMINATED) {
			base->discipline->handle_terminated_request(cqr);
			goto restart;
		}

		
		if (cqr->status == DASD_CQR_NEED_ERP) {
			erp_fn = base->discipline->erp_action(cqr);
			if (IS_ERR(erp_fn(cqr)))
				continue;
			goto restart;
		}

		
		if (cqr->status == DASD_CQR_FAILED) {
			dasd_log_sense(cqr, &cqr->irb);
		}

		
		if (dasd_eer_enabled(base) &&
		    cqr->status == DASD_CQR_FAILED) {
			dasd_eer_write(base, cqr, DASD_EER_FATALERROR);

			
			cqr->status = DASD_CQR_FILLED;
			cqr->retries = 255;
			spin_lock_irqsave(get_ccwdev_lock(base->cdev), flags);
			dasd_device_set_stop_bits(base, DASD_STOPPED_QUIESCE);
			spin_unlock_irqrestore(get_ccwdev_lock(base->cdev),
					       flags);
			goto restart;
		}

		
		if (cqr->refers) {
			__dasd_process_erp(base, cqr);
			goto restart;
		}

		
		cqr->endclk = get_clock();
		list_move_tail(&cqr->blocklist, final_queue);
	}
}

static void dasd_return_cqr_cb(struct dasd_ccw_req *cqr, void *data)
{
	dasd_schedule_block_bh(cqr->block);
}

static void __dasd_block_start_head(struct dasd_block *block)
{
	struct dasd_ccw_req *cqr;

	if (list_empty(&block->ccw_queue))
		return;
	list_for_each_entry(cqr, &block->ccw_queue, blocklist) {
		if (cqr->status != DASD_CQR_FILLED)
			continue;
		if (test_bit(DASD_FLAG_LOCK_STOLEN, &block->base->flags) &&
		    !test_bit(DASD_CQR_ALLOW_SLOCK, &cqr->flags)) {
			cqr->status = DASD_CQR_FAILED;
			cqr->intrc = -EPERM;
			dasd_schedule_block_bh(block);
			continue;
		}
		
		if (block->base->stopped & ~DASD_STOPPED_PENDING &&
		    test_bit(DASD_CQR_FLAGS_FAILFAST, &cqr->flags) &&
		    (!dasd_eer_enabled(block->base))) {
			cqr->status = DASD_CQR_FAILED;
			dasd_schedule_block_bh(block);
			continue;
		}
		
		if (block->base->stopped)
			return;

		
		if (!cqr->startdev)
			cqr->startdev = block->base;

		
		cqr->callback = dasd_return_cqr_cb;

		dasd_add_request_tail(cqr);
	}
}

static void dasd_block_tasklet(struct dasd_block *block)
{
	struct list_head final_queue;
	struct list_head *l, *n;
	struct dasd_ccw_req *cqr;

	atomic_set(&block->tasklet_scheduled, 0);
	INIT_LIST_HEAD(&final_queue);
	spin_lock(&block->queue_lock);
	
	__dasd_process_block_ccw_queue(block, &final_queue);
	spin_unlock(&block->queue_lock);
	
	spin_lock_irq(&block->request_queue_lock);
	list_for_each_safe(l, n, &final_queue) {
		cqr = list_entry(l, struct dasd_ccw_req, blocklist);
		list_del_init(&cqr->blocklist);
		__dasd_cleanup_cqr(cqr);
	}
	spin_lock(&block->queue_lock);
	
	__dasd_process_request_queue(block);
	
	__dasd_block_start_head(block);
	spin_unlock(&block->queue_lock);
	spin_unlock_irq(&block->request_queue_lock);
	dasd_put_device(block->base);
}

static void _dasd_wake_block_flush_cb(struct dasd_ccw_req *cqr, void *data)
{
	wake_up(&dasd_flush_wq);
}

static int dasd_flush_block_queue(struct dasd_block *block)
{
	struct dasd_ccw_req *cqr, *n;
	int rc, i;
	struct list_head flush_queue;

	INIT_LIST_HEAD(&flush_queue);
	spin_lock_bh(&block->queue_lock);
	rc = 0;
restart:
	list_for_each_entry_safe(cqr, n, &block->ccw_queue, blocklist) {
		
		if (cqr->status >= DASD_CQR_QUEUED)
			rc = dasd_cancel_req(cqr);
		if (rc < 0)
			break;
		cqr->callback = _dasd_wake_block_flush_cb;
		for (i = 0; cqr != NULL; cqr = cqr->refers, i++)
			list_move_tail(&cqr->blocklist, &flush_queue);
		if (i > 1)
			
			goto restart;
	}
	spin_unlock_bh(&block->queue_lock);
	
restart_cb:
	list_for_each_entry_safe(cqr, n, &flush_queue, blocklist) {
		wait_event(dasd_flush_wq, (cqr->status < DASD_CQR_QUEUED));
		
		if (cqr->refers) {
			spin_lock_bh(&block->queue_lock);
			__dasd_process_erp(block->base, cqr);
			spin_unlock_bh(&block->queue_lock);
			goto restart_cb;
		}
		
		spin_lock_irq(&block->request_queue_lock);
		cqr->endclk = get_clock();
		list_del_init(&cqr->blocklist);
		__dasd_cleanup_cqr(cqr);
		spin_unlock_irq(&block->request_queue_lock);
	}
	return rc;
}

void dasd_schedule_block_bh(struct dasd_block *block)
{
	
	if (atomic_cmpxchg(&block->tasklet_scheduled, 0, 1) != 0)
		return;
	
	dasd_get_device(block->base);
	tasklet_hi_schedule(&block->tasklet);
}



static void do_dasd_request(struct request_queue *queue)
{
	struct dasd_block *block;

	block = queue->queuedata;
	spin_lock(&block->queue_lock);
	
	__dasd_process_request_queue(block);
	
	__dasd_block_start_head(block);
	spin_unlock(&block->queue_lock);
}

static int dasd_alloc_queue(struct dasd_block *block)
{
	int rc;

	block->request_queue = blk_init_queue(do_dasd_request,
					       &block->request_queue_lock);
	if (block->request_queue == NULL)
		return -ENOMEM;

	block->request_queue->queuedata = block;

	elevator_exit(block->request_queue->elevator);
	block->request_queue->elevator = NULL;
	rc = elevator_init(block->request_queue, "deadline");
	if (rc) {
		blk_cleanup_queue(block->request_queue);
		return rc;
	}
	return 0;
}

static void dasd_setup_queue(struct dasd_block *block)
{
	int max;

	if (block->base->features & DASD_FEATURE_USERAW) {
		max = 2048;
	} else {
		max = block->base->discipline->max_blocks << block->s2b_shift;
	}
	blk_queue_logical_block_size(block->request_queue,
				     block->bp_block);
	blk_queue_max_hw_sectors(block->request_queue, max);
	blk_queue_max_segments(block->request_queue, -1L);
	blk_queue_max_segment_size(block->request_queue, PAGE_SIZE);
	blk_queue_segment_boundary(block->request_queue, PAGE_SIZE - 1);
}

static void dasd_free_queue(struct dasd_block *block)
{
	if (block->request_queue) {
		blk_cleanup_queue(block->request_queue);
		block->request_queue = NULL;
	}
}

static void dasd_flush_request_queue(struct dasd_block *block)
{
	struct request *req;

	if (!block->request_queue)
		return;

	spin_lock_irq(&block->request_queue_lock);
	while ((req = blk_fetch_request(block->request_queue)))
		__blk_end_request_all(req, -EIO);
	spin_unlock_irq(&block->request_queue_lock);
}

static int dasd_open(struct block_device *bdev, fmode_t mode)
{
	struct dasd_device *base;
	int rc;

	base = dasd_device_from_gendisk(bdev->bd_disk);
	if (!base)
		return -ENODEV;

	atomic_inc(&base->block->open_count);
	if (test_bit(DASD_FLAG_OFFLINE, &base->flags)) {
		rc = -ENODEV;
		goto unlock;
	}

	if (!try_module_get(base->discipline->owner)) {
		rc = -EINVAL;
		goto unlock;
	}

	if (dasd_probeonly) {
		dev_info(&base->cdev->dev,
			 "Accessing the DASD failed because it is in "
			 "probeonly mode\n");
		rc = -EPERM;
		goto out;
	}

	if (base->state <= DASD_STATE_BASIC) {
		DBF_DEV_EVENT(DBF_ERR, base, " %s",
			      " Cannot open unrecognized device");
		rc = -ENODEV;
		goto out;
	}

	if ((mode & FMODE_WRITE) &&
	    (test_bit(DASD_FLAG_DEVICE_RO, &base->flags) ||
	     (base->features & DASD_FEATURE_READONLY))) {
		rc = -EROFS;
		goto out;
	}

	dasd_put_device(base);
	return 0;

out:
	module_put(base->discipline->owner);
unlock:
	atomic_dec(&base->block->open_count);
	dasd_put_device(base);
	return rc;
}

static int dasd_release(struct gendisk *disk, fmode_t mode)
{
	struct dasd_device *base;

	base = dasd_device_from_gendisk(disk);
	if (!base)
		return -ENODEV;

	atomic_dec(&base->block->open_count);
	module_put(base->discipline->owner);
	dasd_put_device(base);
	return 0;
}

static int dasd_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct dasd_device *base;

	base = dasd_device_from_gendisk(bdev->bd_disk);
	if (!base)
		return -ENODEV;

	if (!base->discipline ||
	    !base->discipline->fill_geometry) {
		dasd_put_device(base);
		return -EINVAL;
	}
	base->discipline->fill_geometry(base->block, geo);
	geo->start = get_start_sect(bdev) >> base->block->s2b_shift;
	dasd_put_device(base);
	return 0;
}

const struct block_device_operations
dasd_device_operations = {
	.owner		= THIS_MODULE,
	.open		= dasd_open,
	.release	= dasd_release,
	.ioctl		= dasd_ioctl,
	.compat_ioctl	= dasd_ioctl,
	.getgeo		= dasd_getgeo,
};


static void
dasd_exit(void)
{
#ifdef CONFIG_PROC_FS
	dasd_proc_exit();
#endif
	dasd_eer_exit();
        if (dasd_page_cache != NULL) {
		kmem_cache_destroy(dasd_page_cache);
		dasd_page_cache = NULL;
	}
	dasd_gendisk_exit();
	dasd_devmap_exit();
	if (dasd_debug_area != NULL) {
		debug_unregister(dasd_debug_area);
		dasd_debug_area = NULL;
	}
	dasd_statistics_removeroot();
}


int dasd_device_is_ro(struct dasd_device *device)
{
	struct ccw_dev_id dev_id;
	struct diag210 diag_data;
	int rc;

	if (!MACHINE_IS_VM)
		return 0;
	ccw_device_get_id(device->cdev, &dev_id);
	memset(&diag_data, 0, sizeof(diag_data));
	diag_data.vrdcdvno = dev_id.devno;
	diag_data.vrdclen = sizeof(diag_data);
	rc = diag210(&diag_data);
	if (rc == 0 || rc == 2) {
		return diag_data.vrdcvfla & 0x80;
	} else {
		DBF_EVENT(DBF_WARNING, "diag210 failed for dev=%04x with rc=%d",
			  dev_id.devno, rc);
		return 0;
	}
}
EXPORT_SYMBOL_GPL(dasd_device_is_ro);

static void dasd_generic_auto_online(void *data, async_cookie_t cookie)
{
	struct ccw_device *cdev = data;
	int ret;

	ret = ccw_device_set_online(cdev);
	if (ret)
		pr_warning("%s: Setting the DASD online failed with rc=%d\n",
			   dev_name(&cdev->dev), ret);
}

int dasd_generic_probe(struct ccw_device *cdev,
		       struct dasd_discipline *discipline)
{
	int ret;

	ret = dasd_add_sysfs_files(cdev);
	if (ret) {
		DBF_EVENT_DEVID(DBF_WARNING, cdev, "%s",
				"dasd_generic_probe: could not add "
				"sysfs entries");
		return ret;
	}
	cdev->handler = &dasd_int_handler;

	if ((dasd_get_feature(cdev, DASD_FEATURE_INITIAL_ONLINE) > 0 ) ||
	    (dasd_autodetect && dasd_busid_known(dev_name(&cdev->dev)) != 0))
		async_schedule(dasd_generic_auto_online, cdev);
	return 0;
}

void dasd_generic_remove(struct ccw_device *cdev)
{
	struct dasd_device *device;
	struct dasd_block *block;

	cdev->handler = NULL;

	dasd_remove_sysfs_files(cdev);
	device = dasd_device_from_cdev(cdev);
	if (IS_ERR(device))
		return;
	if (test_and_set_bit(DASD_FLAG_OFFLINE, &device->flags)) {
		
		dasd_put_device(device);
		return;
	}
	dasd_set_target_state(device, DASD_STATE_NEW);
	
	block = device->block;
	dasd_delete_device(device);
	if (block)
		dasd_free_block(block);
}

int dasd_generic_set_online(struct ccw_device *cdev,
			    struct dasd_discipline *base_discipline)
{
	struct dasd_discipline *discipline;
	struct dasd_device *device;
	int rc;

	
	dasd_set_feature(cdev, DASD_FEATURE_INITIAL_ONLINE, 0);
	device = dasd_create_device(cdev);
	if (IS_ERR(device))
		return PTR_ERR(device);

	discipline = base_discipline;
	if (device->features & DASD_FEATURE_USEDIAG) {
	  	if (!dasd_diag_discipline_pointer) {
			pr_warning("%s Setting the DASD online failed because "
				   "of missing DIAG discipline\n",
				   dev_name(&cdev->dev));
			dasd_delete_device(device);
			return -ENODEV;
		}
		discipline = dasd_diag_discipline_pointer;
	}
	if (!try_module_get(base_discipline->owner)) {
		dasd_delete_device(device);
		return -EINVAL;
	}
	if (!try_module_get(discipline->owner)) {
		module_put(base_discipline->owner);
		dasd_delete_device(device);
		return -EINVAL;
	}
	device->base_discipline = base_discipline;
	device->discipline = discipline;

	
	rc = discipline->check_device(device);
	if (rc) {
		pr_warning("%s Setting the DASD online with discipline %s "
			   "failed with rc=%i\n",
			   dev_name(&cdev->dev), discipline->name, rc);
		module_put(discipline->owner);
		module_put(base_discipline->owner);
		dasd_delete_device(device);
		return rc;
	}

	dasd_set_target_state(device, DASD_STATE_ONLINE);
	if (device->state <= DASD_STATE_KNOWN) {
		pr_warning("%s Setting the DASD online failed because of a "
			   "missing discipline\n", dev_name(&cdev->dev));
		rc = -ENODEV;
		dasd_set_target_state(device, DASD_STATE_NEW);
		if (device->block)
			dasd_free_block(device->block);
		dasd_delete_device(device);
	} else
		pr_debug("dasd_generic device %s found\n",
				dev_name(&cdev->dev));

	wait_event(dasd_init_waitq, _wait_for_device(device));

	dasd_put_device(device);
	return rc;
}

int dasd_generic_set_offline(struct ccw_device *cdev)
{
	struct dasd_device *device;
	struct dasd_block *block;
	int max_count, open_count;

	device = dasd_device_from_cdev(cdev);
	if (IS_ERR(device))
		return PTR_ERR(device);
	if (test_and_set_bit(DASD_FLAG_OFFLINE, &device->flags)) {
		
		dasd_put_device(device);
		return 0;
	}
	if (device->block) {
		max_count = device->block->bdev ? 0 : -1;
		open_count = atomic_read(&device->block->open_count);
		if (open_count > max_count) {
			if (open_count > 0)
				pr_warning("%s: The DASD cannot be set offline "
					   "with open count %i\n",
					   dev_name(&cdev->dev), open_count);
			else
				pr_warning("%s: The DASD cannot be set offline "
					   "while it is in use\n",
					   dev_name(&cdev->dev));
			clear_bit(DASD_FLAG_OFFLINE, &device->flags);
			dasd_put_device(device);
			return -EBUSY;
		}
	}
	dasd_set_target_state(device, DASD_STATE_NEW);
	
	block = device->block;
	dasd_delete_device(device);
	if (block)
		dasd_free_block(block);
	return 0;
}

int dasd_generic_last_path_gone(struct dasd_device *device)
{
	struct dasd_ccw_req *cqr;

	dev_warn(&device->cdev->dev, "No operational channel path is left "
		 "for the device\n");
	DBF_DEV_EVENT(DBF_WARNING, device, "%s", "last path gone");
	
	dasd_eer_write(device, NULL, DASD_EER_NOPATH);

	if (device->state < DASD_STATE_BASIC)
		return 0;
	
	list_for_each_entry(cqr, &device->ccw_queue, devlist)
		if ((cqr->status == DASD_CQR_IN_IO) ||
		    (cqr->status == DASD_CQR_CLEAR_PENDING)) {
			cqr->status = DASD_CQR_QUEUED;
			cqr->retries++;
		}
	dasd_device_set_stop_bits(device, DASD_STOPPED_DC_WAIT);
	dasd_device_clear_timer(device);
	dasd_schedule_device_bh(device);
	return 1;
}
EXPORT_SYMBOL_GPL(dasd_generic_last_path_gone);

int dasd_generic_path_operational(struct dasd_device *device)
{
	dev_info(&device->cdev->dev, "A channel path to the device has become "
		 "operational\n");
	DBF_DEV_EVENT(DBF_WARNING, device, "%s", "path operational");
	dasd_device_remove_stop_bits(device, DASD_STOPPED_DC_WAIT);
	if (device->stopped & DASD_UNRESUMED_PM) {
		dasd_device_remove_stop_bits(device, DASD_UNRESUMED_PM);
		dasd_restore_device(device);
		return 1;
	}
	dasd_schedule_device_bh(device);
	if (device->block)
		dasd_schedule_block_bh(device->block);
	return 1;
}
EXPORT_SYMBOL_GPL(dasd_generic_path_operational);

int dasd_generic_notify(struct ccw_device *cdev, int event)
{
	struct dasd_device *device;
	int ret;

	device = dasd_device_from_cdev_locked(cdev);
	if (IS_ERR(device))
		return 0;
	ret = 0;
	switch (event) {
	case CIO_GONE:
	case CIO_BOXED:
	case CIO_NO_PATH:
		device->path_data.opm = 0;
		device->path_data.ppm = 0;
		device->path_data.npm = 0;
		ret = dasd_generic_last_path_gone(device);
		break;
	case CIO_OPER:
		ret = 1;
		if (device->path_data.opm)
			ret = dasd_generic_path_operational(device);
		break;
	}
	dasd_put_device(device);
	return ret;
}

void dasd_generic_path_event(struct ccw_device *cdev, int *path_event)
{
	int chp;
	__u8 oldopm, eventlpm;
	struct dasd_device *device;

	device = dasd_device_from_cdev_locked(cdev);
	if (IS_ERR(device))
		return;
	for (chp = 0; chp < 8; chp++) {
		eventlpm = 0x80 >> chp;
		if (path_event[chp] & PE_PATH_GONE) {
			oldopm = device->path_data.opm;
			device->path_data.opm &= ~eventlpm;
			device->path_data.ppm &= ~eventlpm;
			device->path_data.npm &= ~eventlpm;
			if (oldopm && !device->path_data.opm)
				dasd_generic_last_path_gone(device);
		}
		if (path_event[chp] & PE_PATH_AVAILABLE) {
			device->path_data.opm &= ~eventlpm;
			device->path_data.ppm &= ~eventlpm;
			device->path_data.npm &= ~eventlpm;
			device->path_data.tbvpm |= eventlpm;
			dasd_schedule_device_bh(device);
		}
		if (path_event[chp] & PE_PATHGROUP_ESTABLISHED) {
			DBF_DEV_EVENT(DBF_WARNING, device, "%s",
				      "Pathgroup re-established\n");
			if (device->discipline->kick_validate)
				device->discipline->kick_validate(device);
		}
	}
	dasd_put_device(device);
}
EXPORT_SYMBOL_GPL(dasd_generic_path_event);

int dasd_generic_verify_path(struct dasd_device *device, __u8 lpm)
{
	if (!device->path_data.opm && lpm) {
		device->path_data.opm = lpm;
		dasd_generic_path_operational(device);
	} else
		device->path_data.opm |= lpm;
	return 0;
}
EXPORT_SYMBOL_GPL(dasd_generic_verify_path);


int dasd_generic_pm_freeze(struct ccw_device *cdev)
{
	struct dasd_ccw_req *cqr, *n;
	int rc;
	struct list_head freeze_queue;
	struct dasd_device *device = dasd_device_from_cdev(cdev);

	if (IS_ERR(device))
		return PTR_ERR(device);

	
	set_bit(DASD_FLAG_SUSPENDED, &device->flags);

	if (device->discipline->freeze)
		rc = device->discipline->freeze(device);

	
	dasd_device_set_stop_bits(device, DASD_STOPPED_PM);
	
	INIT_LIST_HEAD(&freeze_queue);
	spin_lock_irq(get_ccwdev_lock(cdev));
	rc = 0;
	list_for_each_entry_safe(cqr, n, &device->ccw_queue, devlist) {
		
		if (cqr->status == DASD_CQR_IN_IO) {
			rc = device->discipline->term_IO(cqr);
			if (rc) {
				
				dev_err(&device->cdev->dev,
					"Unable to terminate request %p "
					"on suspend\n", cqr);
				spin_unlock_irq(get_ccwdev_lock(cdev));
				dasd_put_device(device);
				return rc;
			}
		}
		list_move_tail(&cqr->devlist, &freeze_queue);
	}

	spin_unlock_irq(get_ccwdev_lock(cdev));

	list_for_each_entry_safe(cqr, n, &freeze_queue, devlist) {
		wait_event(dasd_flush_wq,
			   (cqr->status != DASD_CQR_CLEAR_PENDING));
		if (cqr->status == DASD_CQR_CLEARED)
			cqr->status = DASD_CQR_QUEUED;
	}
	
	spin_lock_irq(get_ccwdev_lock(cdev));
	list_splice_tail(&freeze_queue, &device->ccw_queue);
	spin_unlock_irq(get_ccwdev_lock(cdev));

	dasd_put_device(device);
	return rc;
}
EXPORT_SYMBOL_GPL(dasd_generic_pm_freeze);

int dasd_generic_restore_device(struct ccw_device *cdev)
{
	struct dasd_device *device = dasd_device_from_cdev(cdev);
	int rc = 0;

	if (IS_ERR(device))
		return PTR_ERR(device);

	
	dasd_device_remove_stop_bits(device,
				     (DASD_STOPPED_PM | DASD_UNRESUMED_PM));

	dasd_schedule_device_bh(device);

	if (device->discipline->restore && !(device->stopped))
		rc = device->discipline->restore(device);
	if (rc || device->stopped)
		device->stopped |= DASD_UNRESUMED_PM;

	if (device->block)
		dasd_schedule_block_bh(device->block);

	clear_bit(DASD_FLAG_SUSPENDED, &device->flags);
	dasd_put_device(device);
	return 0;
}
EXPORT_SYMBOL_GPL(dasd_generic_restore_device);

static struct dasd_ccw_req *dasd_generic_build_rdc(struct dasd_device *device,
						   void *rdc_buffer,
						   int rdc_buffer_size,
						   int magic)
{
	struct dasd_ccw_req *cqr;
	struct ccw1 *ccw;
	unsigned long *idaw;

	cqr = dasd_smalloc_request(magic, 1 , rdc_buffer_size, device);

	if (IS_ERR(cqr)) {
		
		dev_err(&device->cdev->dev,
			 "An error occurred in the DASD device driver, "
			 "reason=%s\n", "13");
		return cqr;
	}

	ccw = cqr->cpaddr;
	ccw->cmd_code = CCW_CMD_RDC;
	if (idal_is_needed(rdc_buffer, rdc_buffer_size)) {
		idaw = (unsigned long *) (cqr->data);
		ccw->cda = (__u32)(addr_t) idaw;
		ccw->flags = CCW_FLAG_IDA;
		idaw = idal_create_words(idaw, rdc_buffer, rdc_buffer_size);
	} else {
		ccw->cda = (__u32)(addr_t) rdc_buffer;
		ccw->flags = 0;
	}

	ccw->count = rdc_buffer_size;
	cqr->startdev = device;
	cqr->memdev = device;
	cqr->expires = 10*HZ;
	cqr->retries = 256;
	cqr->buildclk = get_clock();
	cqr->status = DASD_CQR_FILLED;
	return cqr;
}


int dasd_generic_read_dev_chars(struct dasd_device *device, int magic,
				void *rdc_buffer, int rdc_buffer_size)
{
	int ret;
	struct dasd_ccw_req *cqr;

	cqr = dasd_generic_build_rdc(device, rdc_buffer, rdc_buffer_size,
				     magic);
	if (IS_ERR(cqr))
		return PTR_ERR(cqr);

	ret = dasd_sleep_on(cqr);
	dasd_sfree_request(cqr, cqr->memdev);
	return ret;
}
EXPORT_SYMBOL_GPL(dasd_generic_read_dev_chars);

char *dasd_get_sense(struct irb *irb)
{
	struct tsb *tsb = NULL;
	char *sense = NULL;

	if (scsw_is_tm(&irb->scsw) && (irb->scsw.tm.fcxs == 0x01)) {
		if (irb->scsw.tm.tcw)
			tsb = tcw_get_tsb((struct tcw *)(unsigned long)
					  irb->scsw.tm.tcw);
		if (tsb && tsb->length == 64 && tsb->flags)
			switch (tsb->flags & 0x07) {
			case 1:	
				sense = tsb->tsa.iostat.sense;
				break;
			case 2: 
				sense = tsb->tsa.ddpc.sense;
				break;
			default:
				
				break;
			}
	} else if (irb->esw.esw0.erw.cons) {
		sense = irb->ecw;
	}
	return sense;
}
EXPORT_SYMBOL_GPL(dasd_get_sense);

static int __init dasd_init(void)
{
	int rc;

	init_waitqueue_head(&dasd_init_waitq);
	init_waitqueue_head(&dasd_flush_wq);
	init_waitqueue_head(&generic_waitq);

	
	dasd_debug_area = debug_register("dasd", 1, 1, 8 * sizeof(long));
	if (dasd_debug_area == NULL) {
		rc = -ENOMEM;
		goto failed;
	}
	debug_register_view(dasd_debug_area, &debug_sprintf_view);
	debug_set_level(dasd_debug_area, DBF_WARNING);

	DBF_EVENT(DBF_EMERG, "%s", "debug area created");

	dasd_diag_discipline_pointer = NULL;

	dasd_statistics_createroot();

	rc = dasd_devmap_init();
	if (rc)
		goto failed;
	rc = dasd_gendisk_init();
	if (rc)
		goto failed;
	rc = dasd_parse();
	if (rc)
		goto failed;
	rc = dasd_eer_init();
	if (rc)
		goto failed;
#ifdef CONFIG_PROC_FS
	rc = dasd_proc_init();
	if (rc)
		goto failed;
#endif

	return 0;
failed:
	pr_info("The DASD device driver could not be initialized\n");
	dasd_exit();
	return rc;
}

module_init(dasd_init);
module_exit(dasd_exit);

EXPORT_SYMBOL(dasd_debug_area);
EXPORT_SYMBOL(dasd_diag_discipline_pointer);

EXPORT_SYMBOL(dasd_add_request_head);
EXPORT_SYMBOL(dasd_add_request_tail);
EXPORT_SYMBOL(dasd_cancel_req);
EXPORT_SYMBOL(dasd_device_clear_timer);
EXPORT_SYMBOL(dasd_block_clear_timer);
EXPORT_SYMBOL(dasd_enable_device);
EXPORT_SYMBOL(dasd_int_handler);
EXPORT_SYMBOL(dasd_kfree_request);
EXPORT_SYMBOL(dasd_kick_device);
EXPORT_SYMBOL(dasd_kmalloc_request);
EXPORT_SYMBOL(dasd_schedule_device_bh);
EXPORT_SYMBOL(dasd_schedule_block_bh);
EXPORT_SYMBOL(dasd_set_target_state);
EXPORT_SYMBOL(dasd_device_set_timer);
EXPORT_SYMBOL(dasd_block_set_timer);
EXPORT_SYMBOL(dasd_sfree_request);
EXPORT_SYMBOL(dasd_sleep_on);
EXPORT_SYMBOL(dasd_sleep_on_immediatly);
EXPORT_SYMBOL(dasd_sleep_on_interruptible);
EXPORT_SYMBOL(dasd_smalloc_request);
EXPORT_SYMBOL(dasd_start_IO);
EXPORT_SYMBOL(dasd_term_IO);

EXPORT_SYMBOL_GPL(dasd_generic_probe);
EXPORT_SYMBOL_GPL(dasd_generic_remove);
EXPORT_SYMBOL_GPL(dasd_generic_notify);
EXPORT_SYMBOL_GPL(dasd_generic_set_online);
EXPORT_SYMBOL_GPL(dasd_generic_set_offline);
EXPORT_SYMBOL_GPL(dasd_generic_handle_state_change);
EXPORT_SYMBOL_GPL(dasd_flush_device_queue);
EXPORT_SYMBOL_GPL(dasd_alloc_block);
EXPORT_SYMBOL_GPL(dasd_free_block);
