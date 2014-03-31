
/*
 * edac_device.c
 * (C) 2007 www.douglaskthompson.com
 *
 * This file may be distributed under the terms of the
 * GNU General Public License.
 *
 * Written by Doug Thompson <norsk5@xmission.com>
 *
 * edac_device API implementation
 * 19 Jan 2007
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <linux/highmem.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/workqueue.h>
#include <asm/uaccess.h>
#include <asm/page.h>

#include "edac_core.h"
#include "edac_module.h"

static DEFINE_MUTEX(device_ctls_mutex);
static LIST_HEAD(edac_device_list);

#ifdef CONFIG_EDAC_DEBUG
static void edac_device_dump_device(struct edac_device_ctl_info *edac_dev)
{
	debugf3("\tedac_dev = %p dev_idx=%d \n", edac_dev, edac_dev->dev_idx);
	debugf4("\tedac_dev->edac_check = %p\n", edac_dev->edac_check);
	debugf3("\tdev = %p\n", edac_dev->dev);
	debugf3("\tmod_name:ctl_name = %s:%s\n",
		edac_dev->mod_name, edac_dev->ctl_name);
	debugf3("\tpvt_info = %p\n\n", edac_dev->pvt_info);
}
#endif				


struct edac_device_ctl_info *edac_device_alloc_ctl_info(
	unsigned sz_private,
	char *edac_device_name, unsigned nr_instances,
	char *edac_block_name, unsigned nr_blocks,
	unsigned offset_value,		
	struct edac_dev_sysfs_block_attribute *attrib_spec, unsigned nr_attrib,
	int device_index)
{
	struct edac_device_ctl_info *dev_ctl;
	struct edac_device_instance *dev_inst, *inst;
	struct edac_device_block *dev_blk, *blk_p, *blk;
	struct edac_dev_sysfs_block_attribute *dev_attrib, *attrib_p, *attrib;
	unsigned total_size;
	unsigned count;
	unsigned instance, block, attr;
	void *pvt;
	int err;

	debugf4("%s() instances=%d blocks=%d\n",
		__func__, nr_instances, nr_blocks);

	dev_ctl = (struct edac_device_ctl_info *)NULL;

	dev_inst = edac_align_ptr(&dev_ctl[1], sizeof(*dev_inst));

	dev_blk = edac_align_ptr(&dev_inst[nr_instances], sizeof(*dev_blk));

	count = nr_instances * nr_blocks;
	dev_attrib = edac_align_ptr(&dev_blk[count], sizeof(*dev_attrib));

	
	if (nr_attrib > 0) {
		
		count *= nr_attrib;

		
		pvt = edac_align_ptr(&dev_attrib[count], sz_private);
	} else {
		
		pvt = edac_align_ptr(dev_attrib, sz_private);
	}

	total_size = ((unsigned long)pvt) + sz_private;

	
	dev_ctl = kzalloc(total_size, GFP_KERNEL);
	if (dev_ctl == NULL)
		return NULL;

	dev_inst = (struct edac_device_instance *)
		(((char *)dev_ctl) + ((unsigned long)dev_inst));
	dev_blk = (struct edac_device_block *)
		(((char *)dev_ctl) + ((unsigned long)dev_blk));
	dev_attrib = (struct edac_dev_sysfs_block_attribute *)
		(((char *)dev_ctl) + ((unsigned long)dev_attrib));
	pvt = sz_private ? (((char *)dev_ctl) + ((unsigned long)pvt)) : NULL;

	
	dev_ctl->dev_idx = device_index;
	dev_ctl->nr_instances = nr_instances;
	dev_ctl->instances = dev_inst;
	dev_ctl->pvt_info = pvt;

	
	dev_ctl->log_ce = 1;
	dev_ctl->log_ue = 1;

	
	snprintf(dev_ctl->name,sizeof(dev_ctl->name),"%s",edac_device_name);

	debugf4("%s() edac_dev=%p next after end=%p\n",
		__func__, dev_ctl, pvt + sz_private );

	
	for (instance = 0; instance < nr_instances; instance++) {
		inst = &dev_inst[instance];
		inst->ctl = dev_ctl;
		inst->nr_blocks = nr_blocks;
		blk_p = &dev_blk[instance * nr_blocks];
		inst->blocks = blk_p;

		
		snprintf(inst->name, sizeof(inst->name),
			 "%s%u", edac_device_name, instance);

		
		for (block = 0; block < nr_blocks; block++) {
			blk = &blk_p[block];
			blk->instance = inst;
			snprintf(blk->name, sizeof(blk->name),
				 "%s%d", edac_block_name, block+offset_value);

			debugf4("%s() instance=%d inst_p=%p block=#%d "
				"block_p=%p name='%s'\n",
				__func__, instance, inst, block,
				blk, blk->name);

			if ((nr_attrib == 0) || (attrib_spec == NULL))
				continue;

			
			blk->nr_attribs = nr_attrib;
			attrib_p = &dev_attrib[block*nr_instances*nr_attrib];
			blk->block_attributes = attrib_p;

			debugf4("%s() THIS BLOCK_ATTRIB=%p\n",
				__func__, blk->block_attributes);

			for (attr = 0; attr < nr_attrib; attr++) {
				attrib = &attrib_p[attr];

				attrib->attr = attrib_spec[attr].attr;
				attrib->show = attrib_spec[attr].show;
				attrib->store = attrib_spec[attr].store;

				attrib->block = blk;	

				debugf4("%s() alloc-attrib=%p attrib_name='%s' "
					"attrib-spec=%p spec-name=%s\n",
					__func__, attrib, attrib->attr.name,
					&attrib_spec[attr],
					attrib_spec[attr].attr.name
					);
			}
		}
	}

	
	dev_ctl->op_state = OP_ALLOC;

	err = edac_device_register_sysfs_main_kobj(dev_ctl);
	if (err) {
		kfree(dev_ctl);
		return NULL;
	}


	return dev_ctl;
}
EXPORT_SYMBOL_GPL(edac_device_alloc_ctl_info);

void edac_device_free_ctl_info(struct edac_device_ctl_info *ctl_info)
{
	edac_device_unregister_sysfs_main_kobj(ctl_info);
}
EXPORT_SYMBOL_GPL(edac_device_free_ctl_info);

static struct edac_device_ctl_info *find_edac_device_by_dev(struct device *dev)
{
	struct edac_device_ctl_info *edac_dev;
	struct list_head *item;

	debugf0("%s()\n", __func__);

	list_for_each(item, &edac_device_list) {
		edac_dev = list_entry(item, struct edac_device_ctl_info, link);

		if (edac_dev->dev == dev)
			return edac_dev;
	}

	return NULL;
}

static int add_edac_dev_to_global_list(struct edac_device_ctl_info *edac_dev)
{
	struct list_head *item, *insert_before;
	struct edac_device_ctl_info *rover;

	insert_before = &edac_device_list;

	
	rover = find_edac_device_by_dev(edac_dev->dev);
	if (unlikely(rover != NULL))
		goto fail0;

	
	list_for_each(item, &edac_device_list) {
		rover = list_entry(item, struct edac_device_ctl_info, link);

		if (rover->dev_idx >= edac_dev->dev_idx) {
			if (unlikely(rover->dev_idx == edac_dev->dev_idx))
				goto fail1;

			insert_before = item;
			break;
		}
	}

	list_add_tail_rcu(&edac_dev->link, insert_before);
	return 0;

fail0:
	edac_printk(KERN_WARNING, EDAC_MC,
			"%s (%s) %s %s already assigned %d\n",
			dev_name(rover->dev), edac_dev_name(rover),
			rover->mod_name, rover->ctl_name, rover->dev_idx);
	return 1;

fail1:
	edac_printk(KERN_WARNING, EDAC_MC,
			"bug in low-level driver: attempt to assign\n"
			"    duplicate dev_idx %d in %s()\n", rover->dev_idx,
			__func__);
	return 1;
}

static void del_edac_device_from_global_list(struct edac_device_ctl_info
						*edac_device)
{
	list_del_rcu(&edac_device->link);

	synchronize_rcu();
	INIT_LIST_HEAD(&edac_device->link);
}

static void edac_device_workq_function(struct work_struct *work_req)
{
	struct delayed_work *d_work = to_delayed_work(work_req);
	struct edac_device_ctl_info *edac_dev = to_edac_device_ctl_work(d_work);

	mutex_lock(&device_ctls_mutex);

	
	if (edac_dev->op_state == OP_OFFLINE) {
		mutex_unlock(&device_ctls_mutex);
		return;
	}

	
	if ((edac_dev->op_state == OP_RUNNING_POLL) &&
		(edac_dev->edac_check != NULL)) {
			edac_dev->edac_check(edac_dev);
	}

	mutex_unlock(&device_ctls_mutex);

	if (edac_dev->poll_msec == 1000)
		queue_delayed_work(edac_workqueue, &edac_dev->work,
				round_jiffies_relative(edac_dev->delay));
	else
		queue_delayed_work(edac_workqueue, &edac_dev->work,
				edac_dev->delay);
}

void edac_device_workq_setup(struct edac_device_ctl_info *edac_dev,
				unsigned msec)
{
	debugf0("%s()\n", __func__);

	edac_dev->poll_msec = msec;
	edac_dev->delay = msecs_to_jiffies(msec);

	INIT_DELAYED_WORK(&edac_dev->work, edac_device_workq_function);

	if (edac_dev->poll_msec == 1000)
		queue_delayed_work(edac_workqueue, &edac_dev->work,
				round_jiffies_relative(edac_dev->delay));
	else
		queue_delayed_work(edac_workqueue, &edac_dev->work,
				edac_dev->delay);
}

void edac_device_workq_teardown(struct edac_device_ctl_info *edac_dev)
{
	int status;

	status = cancel_delayed_work(&edac_dev->work);
	if (status == 0) {
		
		flush_workqueue(edac_workqueue);
	}
}

void edac_device_reset_delay_period(struct edac_device_ctl_info *edac_dev,
					unsigned long value)
{
	
	edac_device_workq_teardown(edac_dev);

	
	mutex_lock(&device_ctls_mutex);

	
	edac_device_workq_setup(edac_dev, value);

	mutex_unlock(&device_ctls_mutex);
}

int edac_device_alloc_index(void)
{
	static atomic_t device_indexes = ATOMIC_INIT(0);

	return atomic_inc_return(&device_indexes) - 1;
}
EXPORT_SYMBOL_GPL(edac_device_alloc_index);

int edac_device_add_device(struct edac_device_ctl_info *edac_dev)
{
	debugf0("%s()\n", __func__);

#ifdef CONFIG_EDAC_DEBUG
	if (edac_debug_level >= 3)
		edac_device_dump_device(edac_dev);
#endif
	mutex_lock(&device_ctls_mutex);

	if (add_edac_dev_to_global_list(edac_dev))
		goto fail0;

	
	edac_dev->start_time = jiffies;

	
	if (edac_device_create_sysfs(edac_dev)) {
		edac_device_printk(edac_dev, KERN_WARNING,
					"failed to create sysfs device\n");
		goto fail1;
	}

	
	if (edac_dev->edac_check != NULL) {
		
		edac_dev->op_state = OP_RUNNING_POLL;

		edac_device_workq_setup(edac_dev, 1000);
	} else {
		edac_dev->op_state = OP_RUNNING_INTERRUPT;
	}

	
	edac_device_printk(edac_dev, KERN_INFO,
				"Giving out device to module '%s' controller "
				"'%s': DEV '%s' (%s)\n",
				edac_dev->mod_name,
				edac_dev->ctl_name,
				edac_dev_name(edac_dev),
				edac_op_state_to_string(edac_dev->op_state));

	mutex_unlock(&device_ctls_mutex);
	return 0;

fail1:
	
	del_edac_device_from_global_list(edac_dev);

fail0:
	mutex_unlock(&device_ctls_mutex);
	return 1;
}
EXPORT_SYMBOL_GPL(edac_device_add_device);

struct edac_device_ctl_info *edac_device_del_device(struct device *dev)
{
	struct edac_device_ctl_info *edac_dev;

	debugf0("%s()\n", __func__);

	mutex_lock(&device_ctls_mutex);

	
	edac_dev = find_edac_device_by_dev(dev);
	if (edac_dev == NULL) {
		mutex_unlock(&device_ctls_mutex);
		return NULL;
	}

	
	edac_dev->op_state = OP_OFFLINE;

	
	del_edac_device_from_global_list(edac_dev);

	mutex_unlock(&device_ctls_mutex);

	
	edac_device_workq_teardown(edac_dev);

	
	edac_device_remove_sysfs(edac_dev);

	edac_printk(KERN_INFO, EDAC_MC,
		"Removed device %d for %s %s: DEV %s\n",
		edac_dev->dev_idx,
		edac_dev->mod_name, edac_dev->ctl_name, edac_dev_name(edac_dev));

	return edac_dev;
}
EXPORT_SYMBOL_GPL(edac_device_del_device);

static inline int edac_device_get_log_ce(struct edac_device_ctl_info *edac_dev)
{
	return edac_dev->log_ce;
}

static inline int edac_device_get_log_ue(struct edac_device_ctl_info *edac_dev)
{
	return edac_dev->log_ue;
}

static inline int edac_device_get_panic_on_ue(struct edac_device_ctl_info
					*edac_dev)
{
	return edac_dev->panic_on_ue;
}

void edac_device_handle_ce(struct edac_device_ctl_info *edac_dev,
			int inst_nr, int block_nr, const char *msg)
{
	struct edac_device_instance *instance;
	struct edac_device_block *block = NULL;

	if ((inst_nr >= edac_dev->nr_instances) || (inst_nr < 0)) {
		edac_device_printk(edac_dev, KERN_ERR,
				"INTERNAL ERROR: 'instance' out of range "
				"(%d >= %d)\n", inst_nr,
				edac_dev->nr_instances);
		return;
	}

	instance = edac_dev->instances + inst_nr;

	if ((block_nr >= instance->nr_blocks) || (block_nr < 0)) {
		edac_device_printk(edac_dev, KERN_ERR,
				"INTERNAL ERROR: instance %d 'block' "
				"out of range (%d >= %d)\n",
				inst_nr, block_nr,
				instance->nr_blocks);
		return;
	}

	if (instance->nr_blocks > 0) {
		block = instance->blocks + block_nr;
		block->counters.ce_count++;
	}

	
	instance->counters.ce_count++;
	edac_dev->counters.ce_count++;

	if (edac_device_get_log_ce(edac_dev))
		edac_device_printk(edac_dev, KERN_WARNING,
				"CE: %s instance: %s block: %s '%s'\n",
				edac_dev->ctl_name, instance->name,
				block ? block->name : "N/A", msg);
}
EXPORT_SYMBOL_GPL(edac_device_handle_ce);

void edac_device_handle_ue(struct edac_device_ctl_info *edac_dev,
			int inst_nr, int block_nr, const char *msg)
{
	struct edac_device_instance *instance;
	struct edac_device_block *block = NULL;

	if ((inst_nr >= edac_dev->nr_instances) || (inst_nr < 0)) {
		edac_device_printk(edac_dev, KERN_ERR,
				"INTERNAL ERROR: 'instance' out of range "
				"(%d >= %d)\n", inst_nr,
				edac_dev->nr_instances);
		return;
	}

	instance = edac_dev->instances + inst_nr;

	if ((block_nr >= instance->nr_blocks) || (block_nr < 0)) {
		edac_device_printk(edac_dev, KERN_ERR,
				"INTERNAL ERROR: instance %d 'block' "
				"out of range (%d >= %d)\n",
				inst_nr, block_nr,
				instance->nr_blocks);
		return;
	}

	if (instance->nr_blocks > 0) {
		block = instance->blocks + block_nr;
		block->counters.ue_count++;
	}

	
	instance->counters.ue_count++;
	edac_dev->counters.ue_count++;

	if (edac_device_get_log_ue(edac_dev))
		edac_device_printk(edac_dev, KERN_EMERG,
				"UE: %s instance: %s block: %s '%s'\n",
				edac_dev->ctl_name, instance->name,
				block ? block->name : "N/A", msg);

	if (edac_device_get_panic_on_ue(edac_dev))
		panic("EDAC %s: UE instance: %s block %s '%s'\n",
			edac_dev->ctl_name, instance->name,
			block ? block->name : "N/A", msg);
}
EXPORT_SYMBOL_GPL(edac_device_handle_ue);
