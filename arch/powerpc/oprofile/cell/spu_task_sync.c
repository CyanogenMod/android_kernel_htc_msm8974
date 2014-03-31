/*
 * Cell Broadband Engine OProfile Support
 *
 * (C) Copyright IBM Corporation 2006
 *
 * Author: Maynard Johnson <maynardj@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/dcookies.h>
#include <linux/kref.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/numa.h>
#include <linux/oprofile.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "pr_util.h"

#define RELEASE_ALL 9999

static DEFINE_SPINLOCK(buffer_lock);
static DEFINE_SPINLOCK(cache_lock);
static int num_spu_nodes;
int spu_prof_num_nodes;

struct spu_buffer spu_buff[MAX_NUMNODES * SPUS_PER_NODE];
struct delayed_work spu_work;
static unsigned max_spu_buff;

static void spu_buff_add(unsigned long int value, int spu)
{
	int full = 1;

	if (spu_buff[spu].head >= spu_buff[spu].tail) {
		if ((spu_buff[spu].head - spu_buff[spu].tail)
		    <  (max_spu_buff - 1))
			full = 0;

	} else if (spu_buff[spu].tail > spu_buff[spu].head) {
		if ((spu_buff[spu].tail - spu_buff[spu].head)
		    > 1)
			full = 0;
	}

	if (!full) {
		spu_buff[spu].buff[spu_buff[spu].head] = value;
		spu_buff[spu].head++;

		if (spu_buff[spu].head >= max_spu_buff)
			spu_buff[spu].head = 0;
	} else {
		oprofile_cpu_buffer_inc_smpl_lost();
	}
}

void sync_spu_buff(void)
{
	int spu;
	unsigned long flags;
	int curr_head;

	for (spu = 0; spu < num_spu_nodes; spu++) {
		if (spu_buff[spu].buff == NULL)
			continue;

		spin_lock_irqsave(&buffer_lock, flags);
		curr_head = spu_buff[spu].head;
		spin_unlock_irqrestore(&buffer_lock, flags);

		oprofile_put_buff(spu_buff[spu].buff,
				  spu_buff[spu].tail,
				  curr_head, max_spu_buff);

		spin_lock_irqsave(&buffer_lock, flags);
		spu_buff[spu].tail = curr_head;
		spin_unlock_irqrestore(&buffer_lock, flags);
	}

}

static void wq_sync_spu_buff(struct work_struct *work)
{
	
	sync_spu_buff();

	
	if (spu_prof_running)
		schedule_delayed_work(&spu_work, DEFAULT_TIMER_EXPIRE);
}

struct cached_info {
	struct vma_to_fileoffset_map *map;
	struct spu *the_spu;	
	struct kref cache_ref;
};

static struct cached_info *spu_info[MAX_NUMNODES * 8];

static void destroy_cached_info(struct kref *kref)
{
	struct cached_info *info;

	info = container_of(kref, struct cached_info, cache_ref);
	vma_map_free(info->map);
	kfree(info);
	module_put(THIS_MODULE);
}

static struct cached_info *get_cached_info(struct spu *the_spu, int spu_num)
{
	struct kref *ref;
	struct cached_info *ret_info;

	if (spu_num >= num_spu_nodes) {
		printk(KERN_ERR "SPU_PROF: "
		       "%s, line %d: Invalid index %d into spu info cache\n",
		       __func__, __LINE__, spu_num);
		ret_info = NULL;
		goto out;
	}
	if (!spu_info[spu_num] && the_spu) {
		ref = spu_get_profile_private_kref(the_spu->ctx);
		if (ref) {
			spu_info[spu_num] = container_of(ref, struct cached_info, cache_ref);
			kref_get(&spu_info[spu_num]->cache_ref);
		}
	}

	ret_info = spu_info[spu_num];
 out:
	return ret_info;
}


static int
prepare_cached_spu_info(struct spu *spu, unsigned long objectId)
{
	unsigned long flags;
	struct vma_to_fileoffset_map *new_map;
	int retval = 0;
	struct cached_info *info;

	info = get_cached_info(spu, spu->number);

	if (info) {
		pr_debug("Found cached SPU info.\n");
		goto out;
	}

	info = kzalloc(sizeof(struct cached_info), GFP_KERNEL);
	if (!info) {
		printk(KERN_ERR "SPU_PROF: "
		       "%s, line %d: create vma_map failed\n",
		       __func__, __LINE__);
		retval = -ENOMEM;
		goto err_alloc;
	}
	new_map = create_vma_map(spu, objectId);
	if (!new_map) {
		printk(KERN_ERR "SPU_PROF: "
		       "%s, line %d: create vma_map failed\n",
		       __func__, __LINE__);
		retval = -ENOMEM;
		goto err_alloc;
	}

	pr_debug("Created vma_map\n");
	info->map = new_map;
	info->the_spu = spu;
	kref_init(&info->cache_ref);
	spin_lock_irqsave(&cache_lock, flags);
	spu_info[spu->number] = info;
	
	kref_get(&info->cache_ref);

	try_module_get(THIS_MODULE);
	spu_set_profile_private_kref(spu->ctx, &info->cache_ref,
				destroy_cached_info);
	spin_unlock_irqrestore(&cache_lock, flags);
	goto out;

err_alloc:
	kfree(info);
out:
	return retval;
}

static int release_cached_info(int spu_index)
{
	int index, end;

	if (spu_index == RELEASE_ALL) {
		end = num_spu_nodes;
		index = 0;
	} else {
		if (spu_index >= num_spu_nodes) {
			printk(KERN_ERR "SPU_PROF: "
				"%s, line %d: "
				"Invalid index %d into spu info cache\n",
				__func__, __LINE__, spu_index);
			goto out;
		}
		end = spu_index + 1;
		index = spu_index;
	}
	for (; index < end; index++) {
		if (spu_info[index]) {
			kref_put(&spu_info[index]->cache_ref,
				 destroy_cached_info);
			spu_info[index] = NULL;
		}
	}

out:
	return 0;
}


static inline unsigned long fast_get_dcookie(struct path *path)
{
	unsigned long cookie;

	if (path->dentry->d_flags & DCACHE_COOKIE)
		return (unsigned long)path->dentry;
	get_dcookie(path, &cookie);
	return cookie;
}

static unsigned long
get_exec_dcookie_and_offset(struct spu *spu, unsigned int *offsetp,
			    unsigned long *spu_bin_dcookie,
			    unsigned long spu_ref)
{
	unsigned long app_cookie = 0;
	unsigned int my_offset = 0;
	struct file *app = NULL;
	struct vm_area_struct *vma;
	struct mm_struct *mm = spu->mm;

	if (!mm)
		goto out;

	down_read(&mm->mmap_sem);

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (!vma->vm_file)
			continue;
		if (!(vma->vm_flags & VM_EXECUTABLE))
			continue;
		app_cookie = fast_get_dcookie(&vma->vm_file->f_path);
		pr_debug("got dcookie for %s\n",
			 vma->vm_file->f_dentry->d_name.name);
		app = vma->vm_file;
		break;
	}

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (vma->vm_start > spu_ref || vma->vm_end <= spu_ref)
			continue;
		my_offset = spu_ref - vma->vm_start;
		if (!vma->vm_file)
			goto fail_no_image_cookie;

		pr_debug("Found spu ELF at %X(object-id:%lx) for file %s\n",
			 my_offset, spu_ref,
			 vma->vm_file->f_dentry->d_name.name);
		*offsetp = my_offset;
		break;
	}

	*spu_bin_dcookie = fast_get_dcookie(&vma->vm_file->f_path);
	pr_debug("got dcookie for %s\n", vma->vm_file->f_dentry->d_name.name);

	up_read(&mm->mmap_sem);

out:
	return app_cookie;

fail_no_image_cookie:
	up_read(&mm->mmap_sem);

	printk(KERN_ERR "SPU_PROF: "
		"%s, line %d: Cannot find dcookie for SPU binary\n",
		__func__, __LINE__);
	goto out;
}



static int process_context_switch(struct spu *spu, unsigned long objectId)
{
	unsigned long flags;
	int retval;
	unsigned int offset = 0;
	unsigned long spu_cookie = 0, app_dcookie;

	retval = prepare_cached_spu_info(spu, objectId);
	if (retval)
		goto out;

	app_dcookie = get_exec_dcookie_and_offset(spu, &offset, &spu_cookie, objectId);
	if (!app_dcookie || !spu_cookie) {
		retval  = -ENOENT;
		goto out;
	}

	
	spin_lock_irqsave(&buffer_lock, flags);
	spu_buff_add(ESCAPE_CODE, spu->number);
	spu_buff_add(SPU_CTX_SWITCH_CODE, spu->number);
	spu_buff_add(spu->number, spu->number);
	spu_buff_add(spu->pid, spu->number);
	spu_buff_add(spu->tgid, spu->number);
	spu_buff_add(app_dcookie, spu->number);
	spu_buff_add(spu_cookie, spu->number);
	spu_buff_add(offset, spu->number);

	/* Set flag to indicate SPU PC data can now be written out.  If
	 * the SPU program counter data is seen before an SPU context
	 * record is seen, the postprocessing will fail.
	 */
	spu_buff[spu->number].ctx_sw_seen = 1;

	spin_unlock_irqrestore(&buffer_lock, flags);
	smp_wmb();	/* insure spu event buffer updates are written */
			
out:
	return retval;
}

static int spu_active_notify(struct notifier_block *self, unsigned long val,
				void *data)
{
	int retval;
	unsigned long flags;
	struct spu *the_spu = data;

	pr_debug("SPU event notification arrived\n");
	if (!val) {
		spin_lock_irqsave(&cache_lock, flags);
		retval = release_cached_info(the_spu->number);
		spin_unlock_irqrestore(&cache_lock, flags);
	} else {
		retval = process_context_switch(the_spu, val);
	}
	return retval;
}

static struct notifier_block spu_active = {
	.notifier_call = spu_active_notify,
};

static int number_of_online_nodes(void)
{
        u32 cpu; u32 tmp;
        int nodes = 0;
        for_each_online_cpu(cpu) {
                tmp = cbe_cpu_to_node(cpu) + 1;
                if (tmp > nodes)
                        nodes++;
        }
        return nodes;
}

static int oprofile_spu_buff_create(void)
{
	int spu;

	max_spu_buff = oprofile_get_cpu_buffer_size();

	for (spu = 0; spu < num_spu_nodes; spu++) {
		spu_buff[spu].head = 0;
		spu_buff[spu].tail = 0;


		spu_buff[spu].buff = kzalloc((max_spu_buff
					      * sizeof(unsigned long)),
					     GFP_KERNEL);

		if (!spu_buff[spu].buff) {
			printk(KERN_ERR "SPU_PROF: "
			       "%s, line %d:  oprofile_spu_buff_create "
		       "failed to allocate spu buffer %d.\n",
			       __func__, __LINE__, spu);

			
			while (spu >= 0) {
				kfree(spu_buff[spu].buff);
				spu_buff[spu].buff = 0;
				spu--;
			}
			return -ENOMEM;
		}
	}
	return 0;
}

int spu_sync_start(void)
{
	int spu;
	int ret = SKIP_GENERIC_SYNC;
	int register_ret;
	unsigned long flags = 0;

	spu_prof_num_nodes = number_of_online_nodes();
	num_spu_nodes = spu_prof_num_nodes * 8;
	INIT_DELAYED_WORK(&spu_work, wq_sync_spu_buff);

	ret = oprofile_spu_buff_create();
	if (ret)
		goto out;

	spin_lock_irqsave(&buffer_lock, flags);
	for (spu = 0; spu < num_spu_nodes; spu++) {
		spu_buff_add(ESCAPE_CODE, spu);
		spu_buff_add(SPU_PROFILING_CODE, spu);
		spu_buff_add(num_spu_nodes, spu);
	}
	spin_unlock_irqrestore(&buffer_lock, flags);

	for (spu = 0; spu < num_spu_nodes; spu++) {
		spu_buff[spu].ctx_sw_seen = 0;
		spu_buff[spu].last_guard_val = 0;
	}

	
	register_ret = spu_switch_event_register(&spu_active);
	if (register_ret) {
		ret = SYNC_START_ERROR;
		goto out;
	}

	pr_debug("spu_sync_start -- running.\n");
out:
	return ret;
}

void spu_sync_buffer(int spu_num, unsigned int *samples,
		     int num_samples)
{
	unsigned long long file_offset;
	unsigned long flags;
	int i;
	struct vma_to_fileoffset_map *map;
	struct spu *the_spu;
	unsigned long long spu_num_ll = spu_num;
	unsigned long long spu_num_shifted = spu_num_ll << 32;
	struct cached_info *c_info;

	spin_lock_irqsave(&cache_lock, flags);
	c_info = get_cached_info(NULL, spu_num);
	if (!c_info) {
		pr_debug("SPU_PROF: No cached SPU contex "
			  "for SPU #%d. Dropping samples.\n", spu_num);
		goto out;
	}

	map = c_info->map;
	the_spu = c_info->the_spu;
	spin_lock(&buffer_lock);
	for (i = 0; i < num_samples; i++) {
		unsigned int sample = *(samples+i);
		int grd_val = 0;
		file_offset = 0;
		if (sample == 0)
			continue;
		file_offset = vma_map_lookup( map, sample, the_spu, &grd_val);

		if (grd_val && grd_val != spu_buff[spu_num].last_guard_val) {
			spu_buff[spu_num].last_guard_val = grd_val;
			
			break;
		}

		/* We must ensure that the SPU context switch has been written
		 * out before samples for the SPU.  Otherwise, the SPU context
		 * information is not available and the postprocessing of the
		 * SPU PC will fail with no available anonymous map information.
		 */
		if (spu_buff[spu_num].ctx_sw_seen)
			spu_buff_add((file_offset | spu_num_shifted),
					 spu_num);
	}
	spin_unlock(&buffer_lock);
out:
	spin_unlock_irqrestore(&cache_lock, flags);
}


int spu_sync_stop(void)
{
	unsigned long flags = 0;
	int ret;
	int k;

	ret = spu_switch_event_unregister(&spu_active);

	if (ret)
		printk(KERN_ERR "SPU_PROF: "
		       "%s, line %d: spu_switch_event_unregister "	\
		       "returned %d\n",
		       __func__, __LINE__, ret);

	
	sync_spu_buff();

	spin_lock_irqsave(&cache_lock, flags);
	ret = release_cached_info(RELEASE_ALL);
	spin_unlock_irqrestore(&cache_lock, flags);

	cancel_delayed_work(&spu_work);

	for (k = 0; k < num_spu_nodes; k++) {
		spu_buff[k].ctx_sw_seen = 0;

		kfree(spu_buff[k].buff);
		spu_buff[k].buff = 0;
	}
	pr_debug("spu_sync_stop -- done.\n");
	return ret;
}

