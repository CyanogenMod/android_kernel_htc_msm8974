/*
 * edac_mc kernel module
 * (C) 2005, 2006 Linux Networx (http://lnxi.com)
 * This file may be distributed under the terms of the
 * GNU General Public License.
 *
 * Written by Thayne Harbaugh
 * Based on work by Dan Hollis <goemon at anime dot net> and others.
 *	http://www.anime.net/~goemon/linux-ecc/
 *
 * Modified by Dave Peterson and Doug Thompson
 *
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
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
#include <linux/edac.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/edac.h>
#include "edac_core.h"
#include "edac_module.h"

static DEFINE_MUTEX(mem_ctls_mutex);
static LIST_HEAD(mc_devices);

#ifdef CONFIG_EDAC_DEBUG

static void edac_mc_dump_channel(struct rank_info *chan)
{
	debugf4("\tchannel = %p\n", chan);
	debugf4("\tchannel->chan_idx = %d\n", chan->chan_idx);
	debugf4("\tchannel->ce_count = %d\n", chan->ce_count);
	debugf4("\tchannel->label = '%s'\n", chan->label);
	debugf4("\tchannel->csrow = %p\n\n", chan->csrow);
}

static void edac_mc_dump_csrow(struct csrow_info *csrow)
{
	debugf4("\tcsrow = %p\n", csrow);
	debugf4("\tcsrow->csrow_idx = %d\n", csrow->csrow_idx);
	debugf4("\tcsrow->first_page = 0x%lx\n", csrow->first_page);
	debugf4("\tcsrow->last_page = 0x%lx\n", csrow->last_page);
	debugf4("\tcsrow->page_mask = 0x%lx\n", csrow->page_mask);
	debugf4("\tcsrow->nr_pages = 0x%x\n", csrow->nr_pages);
	debugf4("\tcsrow->nr_channels = %d\n", csrow->nr_channels);
	debugf4("\tcsrow->channels = %p\n", csrow->channels);
	debugf4("\tcsrow->mci = %p\n\n", csrow->mci);
}

static void edac_mc_dump_mci(struct mem_ctl_info *mci)
{
	debugf3("\tmci = %p\n", mci);
	debugf3("\tmci->mtype_cap = %lx\n", mci->mtype_cap);
	debugf3("\tmci->edac_ctl_cap = %lx\n", mci->edac_ctl_cap);
	debugf3("\tmci->edac_cap = %lx\n", mci->edac_cap);
	debugf4("\tmci->edac_check = %p\n", mci->edac_check);
	debugf3("\tmci->nr_csrows = %d, csrows = %p\n",
		mci->nr_csrows, mci->csrows);
	debugf3("\tdev = %p\n", mci->dev);
	debugf3("\tmod_name:ctl_name = %s:%s\n", mci->mod_name, mci->ctl_name);
	debugf3("\tpvt_info = %p\n\n", mci->pvt_info);
}

#endif				

const char *edac_mem_types[] = {
	"Empty csrow",
	"Reserved csrow type",
	"Unknown csrow type",
	"Fast page mode RAM",
	"Extended data out RAM",
	"Burst Extended data out RAM",
	"Single data rate SDRAM",
	"Registered single data rate SDRAM",
	"Double data rate SDRAM",
	"Registered Double data rate SDRAM",
	"Rambus DRAM",
	"Unbuffered DDR2 RAM",
	"Fully buffered DDR2",
	"Registered DDR2 RAM",
	"Rambus XDR",
	"Unbuffered DDR3 RAM",
	"Registered DDR3 RAM",
};
EXPORT_SYMBOL_GPL(edac_mem_types);

void *edac_align_ptr(void *ptr, unsigned size)
{
	unsigned align, r;

	if (size > sizeof(long))
		align = sizeof(long long);
	else if (size > sizeof(int))
		align = sizeof(long);
	else if (size > sizeof(short))
		align = sizeof(int);
	else if (size > sizeof(char))
		align = sizeof(short);
	else
		return (char *)ptr;

	r = size % align;

	if (r == 0)
		return (char *)ptr;

	return (void *)(((unsigned long)ptr) + align - r);
}

struct mem_ctl_info *edac_mc_alloc(unsigned sz_pvt, unsigned nr_csrows,
				unsigned nr_chans, int edac_index)
{
	struct mem_ctl_info *mci;
	struct csrow_info *csi, *csrow;
	struct rank_info *chi, *chp, *chan;
	void *pvt;
	unsigned size;
	int row, chn;
	int err;

	mci = (struct mem_ctl_info *)0;
	csi = edac_align_ptr(&mci[1], sizeof(*csi));
	chi = edac_align_ptr(&csi[nr_csrows], sizeof(*chi));
	pvt = edac_align_ptr(&chi[nr_chans * nr_csrows], sz_pvt);
	size = ((unsigned long)pvt) + sz_pvt;

	mci = kzalloc(size, GFP_KERNEL);
	if (mci == NULL)
		return NULL;

	csi = (struct csrow_info *)(((char *)mci) + ((unsigned long)csi));
	chi = (struct rank_info *)(((char *)mci) + ((unsigned long)chi));
	pvt = sz_pvt ? (((char *)mci) + ((unsigned long)pvt)) : NULL;

	
	mci->mc_idx = edac_index;
	mci->csrows = csi;
	mci->pvt_info = pvt;
	mci->nr_csrows = nr_csrows;

	for (row = 0; row < nr_csrows; row++) {
		csrow = &csi[row];
		csrow->csrow_idx = row;
		csrow->mci = mci;
		csrow->nr_channels = nr_chans;
		chp = &chi[row * nr_chans];
		csrow->channels = chp;

		for (chn = 0; chn < nr_chans; chn++) {
			chan = &chp[chn];
			chan->chan_idx = chn;
			chan->csrow = csrow;
		}
	}

	mci->op_state = OP_ALLOC;
	INIT_LIST_HEAD(&mci->grp_kobj_list);

	err = edac_mc_register_sysfs_main_kobj(mci);
	if (err) {
		kfree(mci);
		return NULL;
	}

	return mci;
}
EXPORT_SYMBOL_GPL(edac_mc_alloc);

void edac_mc_free(struct mem_ctl_info *mci)
{
	debugf1("%s()\n", __func__);

	edac_mc_unregister_sysfs_main_kobj(mci);

	
	kfree(mci);
}
EXPORT_SYMBOL_GPL(edac_mc_free);


struct mem_ctl_info *find_mci_by_dev(struct device *dev)
{
	struct mem_ctl_info *mci;
	struct list_head *item;

	debugf3("%s()\n", __func__);

	list_for_each(item, &mc_devices) {
		mci = list_entry(item, struct mem_ctl_info, link);

		if (mci->dev == dev)
			return mci;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(find_mci_by_dev);

static int edac_mc_assert_error_check_and_clear(void)
{
	int old_state;

	if (edac_op_state == EDAC_OPSTATE_POLL)
		return 1;

	old_state = edac_err_assert;
	edac_err_assert = 0;

	return old_state;
}

static void edac_mc_workq_function(struct work_struct *work_req)
{
	struct delayed_work *d_work = to_delayed_work(work_req);
	struct mem_ctl_info *mci = to_edac_mem_ctl_work(d_work);

	mutex_lock(&mem_ctls_mutex);

	
	if (mci->op_state == OP_OFFLINE) {
		mutex_unlock(&mem_ctls_mutex);
		return;
	}

	
	if (edac_mc_assert_error_check_and_clear() && (mci->edac_check != NULL))
		mci->edac_check(mci);

	mutex_unlock(&mem_ctls_mutex);

	
	queue_delayed_work(edac_workqueue, &mci->work,
			msecs_to_jiffies(edac_mc_get_poll_msec()));
}

static void edac_mc_workq_setup(struct mem_ctl_info *mci, unsigned msec)
{
	debugf0("%s()\n", __func__);

	
	if (mci->op_state != OP_RUNNING_POLL)
		return;

	INIT_DELAYED_WORK(&mci->work, edac_mc_workq_function);
	queue_delayed_work(edac_workqueue, &mci->work, msecs_to_jiffies(msec));
}

static void edac_mc_workq_teardown(struct mem_ctl_info *mci)
{
	int status;

	if (mci->op_state != OP_RUNNING_POLL)
		return;

	status = cancel_delayed_work(&mci->work);
	if (status == 0) {
		debugf0("%s() not canceled, flush the queue\n",
			__func__);

		
		flush_workqueue(edac_workqueue);
	}
}

void edac_mc_reset_delay_period(int value)
{
	struct mem_ctl_info *mci;
	struct list_head *item;

	mutex_lock(&mem_ctls_mutex);

	list_for_each(item, &mc_devices) {
		mci = list_entry(item, struct mem_ctl_info, link);

		if (mci->op_state == OP_RUNNING_POLL)
			cancel_delayed_work(&mci->work);
	}

	mutex_unlock(&mem_ctls_mutex);


	
	mutex_lock(&mem_ctls_mutex);

	list_for_each(item, &mc_devices) {
		mci = list_entry(item, struct mem_ctl_info, link);

		edac_mc_workq_setup(mci, (unsigned long) value);
	}

	mutex_unlock(&mem_ctls_mutex);
}



static int add_mc_to_global_list(struct mem_ctl_info *mci)
{
	struct list_head *item, *insert_before;
	struct mem_ctl_info *p;

	insert_before = &mc_devices;

	p = find_mci_by_dev(mci->dev);
	if (unlikely(p != NULL))
		goto fail0;

	list_for_each(item, &mc_devices) {
		p = list_entry(item, struct mem_ctl_info, link);

		if (p->mc_idx >= mci->mc_idx) {
			if (unlikely(p->mc_idx == mci->mc_idx))
				goto fail1;

			insert_before = item;
			break;
		}
	}

	list_add_tail_rcu(&mci->link, insert_before);
	atomic_inc(&edac_handlers);
	return 0;

fail0:
	edac_printk(KERN_WARNING, EDAC_MC,
		"%s (%s) %s %s already assigned %d\n", dev_name(p->dev),
		edac_dev_name(mci), p->mod_name, p->ctl_name, p->mc_idx);
	return 1;

fail1:
	edac_printk(KERN_WARNING, EDAC_MC,
		"bug in low-level driver: attempt to assign\n"
		"    duplicate mc_idx %d in %s()\n", p->mc_idx, __func__);
	return 1;
}

static void del_mc_from_global_list(struct mem_ctl_info *mci)
{
	atomic_dec(&edac_handlers);
	list_del_rcu(&mci->link);

	synchronize_rcu();
	INIT_LIST_HEAD(&mci->link);
}

struct mem_ctl_info *edac_mc_find(int idx)
{
	struct list_head *item;
	struct mem_ctl_info *mci;

	list_for_each(item, &mc_devices) {
		mci = list_entry(item, struct mem_ctl_info, link);

		if (mci->mc_idx >= idx) {
			if (mci->mc_idx == idx)
				return mci;

			break;
		}
	}

	return NULL;
}
EXPORT_SYMBOL(edac_mc_find);


int edac_mc_add_mc(struct mem_ctl_info *mci)
{
	debugf0("%s()\n", __func__);

#ifdef CONFIG_EDAC_DEBUG
	if (edac_debug_level >= 3)
		edac_mc_dump_mci(mci);

	if (edac_debug_level >= 4) {
		int i;

		for (i = 0; i < mci->nr_csrows; i++) {
			int j;

			edac_mc_dump_csrow(&mci->csrows[i]);
			for (j = 0; j < mci->csrows[i].nr_channels; j++)
				edac_mc_dump_channel(&mci->csrows[i].
						channels[j]);
		}
	}
#endif
	mutex_lock(&mem_ctls_mutex);

	if (add_mc_to_global_list(mci))
		goto fail0;

	
	mci->start_time = jiffies;

	if (edac_create_sysfs_mci_device(mci)) {
		edac_mc_printk(mci, KERN_WARNING,
			"failed to create sysfs device\n");
		goto fail1;
	}

	
	if (mci->edac_check != NULL) {
		
		mci->op_state = OP_RUNNING_POLL;

		edac_mc_workq_setup(mci, edac_mc_get_poll_msec());
	} else {
		mci->op_state = OP_RUNNING_INTERRUPT;
	}

	
	edac_mc_printk(mci, KERN_INFO, "Giving out device to '%s' '%s':"
		" DEV %s\n", mci->mod_name, mci->ctl_name, edac_dev_name(mci));

	mutex_unlock(&mem_ctls_mutex);
	return 0;

fail1:
	del_mc_from_global_list(mci);

fail0:
	mutex_unlock(&mem_ctls_mutex);
	return 1;
}
EXPORT_SYMBOL_GPL(edac_mc_add_mc);

struct mem_ctl_info *edac_mc_del_mc(struct device *dev)
{
	struct mem_ctl_info *mci;

	debugf0("%s()\n", __func__);

	mutex_lock(&mem_ctls_mutex);

	
	mci = find_mci_by_dev(dev);
	if (mci == NULL) {
		mutex_unlock(&mem_ctls_mutex);
		return NULL;
	}

	del_mc_from_global_list(mci);
	mutex_unlock(&mem_ctls_mutex);

	
	edac_mc_workq_teardown(mci);

	
	mci->op_state = OP_OFFLINE;

	
	edac_remove_sysfs_mci_device(mci);

	edac_printk(KERN_INFO, EDAC_MC,
		"Removed device %d for %s %s: DEV %s\n", mci->mc_idx,
		mci->mod_name, mci->ctl_name, edac_dev_name(mci));

	return mci;
}
EXPORT_SYMBOL_GPL(edac_mc_del_mc);

static void edac_mc_scrub_block(unsigned long page, unsigned long offset,
				u32 size)
{
	struct page *pg;
	void *virt_addr;
	unsigned long flags = 0;

	debugf3("%s()\n", __func__);

	
	if (!pfn_valid(page))
		return;

	
	pg = pfn_to_page(page);

	if (PageHighMem(pg))
		local_irq_save(flags);

	virt_addr = kmap_atomic(pg);

	
	atomic_scrub(virt_addr + offset, size);

	
	kunmap_atomic(virt_addr);

	if (PageHighMem(pg))
		local_irq_restore(flags);
}

int edac_mc_find_csrow_by_page(struct mem_ctl_info *mci, unsigned long page)
{
	struct csrow_info *csrows = mci->csrows;
	int row, i;

	debugf1("MC%d: %s(): 0x%lx\n", mci->mc_idx, __func__, page);
	row = -1;

	for (i = 0; i < mci->nr_csrows; i++) {
		struct csrow_info *csrow = &csrows[i];

		if (csrow->nr_pages == 0)
			continue;

		debugf3("MC%d: %s(): first(0x%lx) page(0x%lx) last(0x%lx) "
			"mask(0x%lx)\n", mci->mc_idx, __func__,
			csrow->first_page, page, csrow->last_page,
			csrow->page_mask);

		if ((page >= csrow->first_page) &&
		    (page <= csrow->last_page) &&
		    ((page & csrow->page_mask) ==
		     (csrow->first_page & csrow->page_mask))) {
			row = i;
			break;
		}
	}

	if (row == -1)
		edac_mc_printk(mci, KERN_ERR,
			"could not look up page error address %lx\n",
			(unsigned long)page);

	return row;
}
EXPORT_SYMBOL_GPL(edac_mc_find_csrow_by_page);

void edac_mc_handle_ce(struct mem_ctl_info *mci,
		unsigned long page_frame_number,
		unsigned long offset_in_page, unsigned long syndrome,
		int row, int channel, const char *msg)
{
	unsigned long remapped_page;

	debugf3("MC%d: %s()\n", mci->mc_idx, __func__);

	
	if (row >= mci->nr_csrows || row < 0) {
		
		edac_mc_printk(mci, KERN_ERR,
			"INTERNAL ERROR: row out of range "
			"(%d >= %d)\n", row, mci->nr_csrows);
		edac_mc_handle_ce_no_info(mci, "INTERNAL ERROR");
		return;
	}

	if (channel >= mci->csrows[row].nr_channels || channel < 0) {
		
		edac_mc_printk(mci, KERN_ERR,
			"INTERNAL ERROR: channel out of range "
			"(%d >= %d)\n", channel,
			mci->csrows[row].nr_channels);
		edac_mc_handle_ce_no_info(mci, "INTERNAL ERROR");
		return;
	}

	if (edac_mc_get_log_ce())
		
		edac_mc_printk(mci, KERN_WARNING,
			"CE page 0x%lx, offset 0x%lx, grain %d, syndrome "
			"0x%lx, row %d, channel %d, label \"%s\": %s\n",
			page_frame_number, offset_in_page,
			mci->csrows[row].grain, syndrome, row, channel,
			mci->csrows[row].channels[channel].label, msg);

	mci->ce_count++;
	mci->csrows[row].ce_count++;
	mci->csrows[row].channels[channel].ce_count++;

	if (mci->scrub_mode & SCRUB_SW_SRC) {
		remapped_page = mci->ctl_page_to_phys ?
			mci->ctl_page_to_phys(mci, page_frame_number) :
			page_frame_number;

		edac_mc_scrub_block(remapped_page, offset_in_page,
				mci->csrows[row].grain);
	}
}
EXPORT_SYMBOL_GPL(edac_mc_handle_ce);

void edac_mc_handle_ce_no_info(struct mem_ctl_info *mci, const char *msg)
{
	if (edac_mc_get_log_ce())
		edac_mc_printk(mci, KERN_WARNING,
			"CE - no information available: %s\n", msg);

	mci->ce_noinfo_count++;
	mci->ce_count++;
}
EXPORT_SYMBOL_GPL(edac_mc_handle_ce_no_info);

void edac_mc_handle_ue(struct mem_ctl_info *mci,
		unsigned long page_frame_number,
		unsigned long offset_in_page, int row, const char *msg)
{
	int len = EDAC_MC_LABEL_LEN * 4;
	char labels[len + 1];
	char *pos = labels;
	int chan;
	int chars;

	debugf3("MC%d: %s()\n", mci->mc_idx, __func__);

	
	if (row >= mci->nr_csrows || row < 0) {
		
		edac_mc_printk(mci, KERN_ERR,
			"INTERNAL ERROR: row out of range "
			"(%d >= %d)\n", row, mci->nr_csrows);
		edac_mc_handle_ue_no_info(mci, "INTERNAL ERROR");
		return;
	}

	chars = snprintf(pos, len + 1, "%s",
			 mci->csrows[row].channels[0].label);
	len -= chars;
	pos += chars;

	for (chan = 1; (chan < mci->csrows[row].nr_channels) && (len > 0);
		chan++) {
		chars = snprintf(pos, len + 1, ":%s",
				 mci->csrows[row].channels[chan].label);
		len -= chars;
		pos += chars;
	}

	if (edac_mc_get_log_ue())
		edac_mc_printk(mci, KERN_EMERG,
			"UE page 0x%lx, offset 0x%lx, grain %d, row %d, "
			"labels \"%s\": %s\n", page_frame_number,
			offset_in_page, mci->csrows[row].grain, row,
			labels, msg);

	if (edac_mc_get_panic_on_ue())
		panic("EDAC MC%d: UE page 0x%lx, offset 0x%lx, grain %d, "
			"row %d, labels \"%s\": %s\n", mci->mc_idx,
			page_frame_number, offset_in_page,
			mci->csrows[row].grain, row, labels, msg);

	mci->ue_count++;
	mci->csrows[row].ue_count++;
}
EXPORT_SYMBOL_GPL(edac_mc_handle_ue);

void edac_mc_handle_ue_no_info(struct mem_ctl_info *mci, const char *msg)
{
	if (edac_mc_get_panic_on_ue())
		panic("EDAC MC%d: Uncorrected Error", mci->mc_idx);

	if (edac_mc_get_log_ue())
		edac_mc_printk(mci, KERN_WARNING,
			"UE - no information available: %s\n", msg);
	mci->ue_noinfo_count++;
	mci->ue_count++;
}
EXPORT_SYMBOL_GPL(edac_mc_handle_ue_no_info);

void edac_mc_handle_fbd_ue(struct mem_ctl_info *mci,
			unsigned int csrow,
			unsigned int channela,
			unsigned int channelb, char *msg)
{
	int len = EDAC_MC_LABEL_LEN * 4;
	char labels[len + 1];
	char *pos = labels;
	int chars;

	if (csrow >= mci->nr_csrows) {
		
		edac_mc_printk(mci, KERN_ERR,
			"INTERNAL ERROR: row out of range (%d >= %d)\n",
			csrow, mci->nr_csrows);
		edac_mc_handle_ue_no_info(mci, "INTERNAL ERROR");
		return;
	}

	if (channela >= mci->csrows[csrow].nr_channels) {
		
		edac_mc_printk(mci, KERN_ERR,
			"INTERNAL ERROR: channel-a out of range "
			"(%d >= %d)\n",
			channela, mci->csrows[csrow].nr_channels);
		edac_mc_handle_ue_no_info(mci, "INTERNAL ERROR");
		return;
	}

	if (channelb >= mci->csrows[csrow].nr_channels) {
		
		edac_mc_printk(mci, KERN_ERR,
			"INTERNAL ERROR: channel-b out of range "
			"(%d >= %d)\n",
			channelb, mci->csrows[csrow].nr_channels);
		edac_mc_handle_ue_no_info(mci, "INTERNAL ERROR");
		return;
	}

	mci->ue_count++;
	mci->csrows[csrow].ue_count++;

	
	chars = snprintf(pos, len + 1, "%s",
			 mci->csrows[csrow].channels[channela].label);
	len -= chars;
	pos += chars;
	chars = snprintf(pos, len + 1, "-%s",
			 mci->csrows[csrow].channels[channelb].label);

	if (edac_mc_get_log_ue())
		edac_mc_printk(mci, KERN_EMERG,
			"UE row %d, channel-a= %d channel-b= %d "
			"labels \"%s\": %s\n", csrow, channela, channelb,
			labels, msg);

	if (edac_mc_get_panic_on_ue())
		panic("UE row %d, channel-a= %d channel-b= %d "
			"labels \"%s\": %s\n", csrow, channela,
			channelb, labels, msg);
}
EXPORT_SYMBOL(edac_mc_handle_fbd_ue);

void edac_mc_handle_fbd_ce(struct mem_ctl_info *mci,
			unsigned int csrow, unsigned int channel, char *msg)
{

	
	if (csrow >= mci->nr_csrows) {
		
		edac_mc_printk(mci, KERN_ERR,
			"INTERNAL ERROR: row out of range (%d >= %d)\n",
			csrow, mci->nr_csrows);
		edac_mc_handle_ce_no_info(mci, "INTERNAL ERROR");
		return;
	}
	if (channel >= mci->csrows[csrow].nr_channels) {
		
		edac_mc_printk(mci, KERN_ERR,
			"INTERNAL ERROR: channel out of range (%d >= %d)\n",
			channel, mci->csrows[csrow].nr_channels);
		edac_mc_handle_ce_no_info(mci, "INTERNAL ERROR");
		return;
	}

	if (edac_mc_get_log_ce())
		
		edac_mc_printk(mci, KERN_WARNING,
			"CE row %d, channel %d, label \"%s\": %s\n",
			csrow, channel,
			mci->csrows[csrow].channels[channel].label, msg);

	mci->ce_count++;
	mci->csrows[csrow].ce_count++;
	mci->csrows[csrow].channels[channel].ce_count++;
}
EXPORT_SYMBOL(edac_mc_handle_fbd_ce);
