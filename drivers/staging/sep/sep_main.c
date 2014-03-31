/*
 *
 *  sep_main.c - Security Processor Driver main group of functions
 *
 *  Copyright(c) 2009-2011 Intel Corporation. All rights reserved.
 *  Contributions(c) 2009-2011 Discretix. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  CONTACTS:
 *
 *  Mark Allyn		mark.a.allyn@intel.com
 *  Jayant Mangalampalli jayant.mangalampalli@intel.com
 *
 *  CHANGES:
 *
 *  2009.06.26	Initial publish
 *  2010.09.14  Upgrade to Medfield
 *  2011.01.21  Move to sep_main.c to allow for sep_crypto.c
 *  2011.02.22  Enable kernel crypto operation
 *
 *  Please note that this driver is based on information in the Discretix
 *  CryptoCell 5.2 Driver Implementation Guide; the Discretix CryptoCell 5.2
 *  Integration Intel Medfield appendix; the Discretix CryptoCell 5.2
 *  Linux Driver Integration Guide; and the Discretix CryptoCell 5.2 System
 *  Overview and Integration Guide.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <asm/current.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <asm/cacheflush.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/async.h>
#include <linux/crypto.h>
#include <crypto/internal/hash.h>
#include <crypto/scatterwalk.h>
#include <crypto/sha.h>
#include <crypto/md5.h>
#include <crypto/aes.h>
#include <crypto/des.h>
#include <crypto/hash.h>

#include "sep_driver_hw_defs.h"
#include "sep_driver_config.h"
#include "sep_driver_api.h"
#include "sep_dev.h"
#include "sep_crypto.h"

#define CREATE_TRACE_POINTS
#include "sep_trace_events.h"

#ifdef DEBUG
#define sep_dump_message(sep)	_sep_dump_message(sep)
#else
#define sep_dump_message(sep)
#endif


struct sep_device *sep_dev;

void sep_queue_status_remove(struct sep_device *sep,
				      struct sep_queue_info **queue_elem)
{
	unsigned long lck_flags;

	dev_dbg(&sep->pdev->dev, "[PID%d] sep_queue_status_remove\n",
		current->pid);

	if (!queue_elem || !(*queue_elem)) {
		dev_dbg(&sep->pdev->dev, "PID%d %s null\n",
					current->pid, __func__);
		return;
	}

	spin_lock_irqsave(&sep->sep_queue_lock, lck_flags);
	list_del(&(*queue_elem)->list);
	sep->sep_queue_num--;
	spin_unlock_irqrestore(&sep->sep_queue_lock, lck_flags);

	kfree(*queue_elem);
	*queue_elem = NULL;

	dev_dbg(&sep->pdev->dev, "[PID%d] sep_queue_status_remove return\n",
		current->pid);
	return;
}

struct sep_queue_info *sep_queue_status_add(
						struct sep_device *sep,
						u32 opcode,
						u32 size,
						u32 pid,
						u8 *name, size_t name_len)
{
	unsigned long lck_flags;
	struct sep_queue_info *my_elem = NULL;

	my_elem = kzalloc(sizeof(struct sep_queue_info), GFP_KERNEL);

	if (!my_elem)
		return NULL;

	dev_dbg(&sep->pdev->dev, "[PID%d] kzalloc ok\n", current->pid);

	my_elem->data.opcode = opcode;
	my_elem->data.size = size;
	my_elem->data.pid = pid;

	if (name_len > TASK_COMM_LEN)
		name_len = TASK_COMM_LEN;

	memcpy(&my_elem->data.name, name, name_len);

	spin_lock_irqsave(&sep->sep_queue_lock, lck_flags);

	list_add_tail(&my_elem->list, &sep->sep_queue_status);
	sep->sep_queue_num++;

	spin_unlock_irqrestore(&sep->sep_queue_lock, lck_flags);

	return my_elem;
}

static int sep_allocate_dmatables_region(struct sep_device *sep,
					 void **dmatables_region,
					 struct sep_dma_context *dma_ctx,
					 const u32 table_count)
{
	const size_t new_len =
		SYNCHRONIC_DMA_TABLES_AREA_SIZE_BYTES - 1;

	void *tmp_region = NULL;

	dev_dbg(&sep->pdev->dev, "[PID%d] dma_ctx = 0x%p\n",
				current->pid, dma_ctx);
	dev_dbg(&sep->pdev->dev, "[PID%d] dmatables_region = 0x%p\n",
				current->pid, dmatables_region);

	if (!dma_ctx || !dmatables_region) {
		dev_warn(&sep->pdev->dev,
			"[PID%d] dma context/region uninitialized\n",
			current->pid);
		return -EINVAL;
	}

	dev_dbg(&sep->pdev->dev, "[PID%d] newlen = 0x%08zX\n",
				current->pid, new_len);
	dev_dbg(&sep->pdev->dev, "[PID%d] oldlen = 0x%08X\n", current->pid,
				dma_ctx->dmatables_len);
	tmp_region = kzalloc(new_len + dma_ctx->dmatables_len, GFP_KERNEL);
	if (!tmp_region) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] no mem for dma tables region\n",
				current->pid);
		return -ENOMEM;
	}

	
	if (*dmatables_region) {
		memcpy(tmp_region, *dmatables_region, dma_ctx->dmatables_len);
		kfree(*dmatables_region);
		*dmatables_region = NULL;
	}

	*dmatables_region = tmp_region;

	dma_ctx->dmatables_len += new_len;

	return 0;
}

int sep_wait_transaction(struct sep_device *sep)
{
	int error = 0;
	DEFINE_WAIT(wait);

	if (0 == test_and_set_bit(SEP_TRANSACTION_STARTED_LOCK_BIT,
				&sep->in_use_flags)) {
		dev_dbg(&sep->pdev->dev,
			"[PID%d] no transactions, returning\n",
				current->pid);
		goto end_function_setpid;
	}

	for (;;) {
		prepare_to_wait_exclusive(&sep->event_transactions,
					  &wait,
					  TASK_INTERRUPTIBLE);
		if (0 == test_and_set_bit(SEP_TRANSACTION_STARTED_LOCK_BIT,
					  &sep->in_use_flags)) {
			dev_dbg(&sep->pdev->dev,
				"[PID%d] no transactions, breaking\n",
					current->pid);
			break;
		}
		dev_dbg(&sep->pdev->dev,
			"[PID%d] transactions ongoing, sleeping\n",
				current->pid);
		schedule();
		dev_dbg(&sep->pdev->dev, "[PID%d] woken up\n", current->pid);

		if (signal_pending(current)) {
			dev_dbg(&sep->pdev->dev, "[PID%d] received signal\n",
							current->pid);
			error = -EINTR;
			goto end_function;
		}
	}
end_function_setpid:
	
	sep->pid_doing_transaction = current->pid;

end_function:
	finish_wait(&sep->event_transactions, &wait);

	return error;
}

static inline int sep_check_transaction_owner(struct sep_device *sep)
{
	dev_dbg(&sep->pdev->dev, "[PID%d] transaction pid = %d\n",
		current->pid,
		sep->pid_doing_transaction);

	if ((sep->pid_doing_transaction == 0) ||
		(current->pid != sep->pid_doing_transaction)) {
		return -EACCES;
	}

	
	return 0;
}

#ifdef DEBUG

static void _sep_dump_message(struct sep_device *sep)
{
	int count;

	u32 *p = sep->shared_addr;

	for (count = 0; count < 10 * 4; count += 4)
		dev_dbg(&sep->pdev->dev,
			"[PID%d] Word %d of the message is %x\n",
				current->pid, count/4, *p++);
}

#endif

static int sep_map_and_alloc_shared_area(struct sep_device *sep)
{
	sep->shared_addr = dma_alloc_coherent(&sep->pdev->dev,
		sep->shared_size,
		&sep->shared_bus, GFP_KERNEL);

	if (!sep->shared_addr) {
		dev_dbg(&sep->pdev->dev,
			"[PID%d] shared memory dma_alloc_coherent failed\n",
				current->pid);
		return -ENOMEM;
	}
	dev_dbg(&sep->pdev->dev,
		"[PID%d] shared_addr %zx bytes @%p (bus %llx)\n",
				current->pid,
				sep->shared_size, sep->shared_addr,
				(unsigned long long)sep->shared_bus);
	return 0;
}

static void sep_unmap_and_free_shared_area(struct sep_device *sep)
{
	dma_free_coherent(&sep->pdev->dev, sep->shared_size,
				sep->shared_addr, sep->shared_bus);
}

#ifdef DEBUG

static void *sep_shared_bus_to_virt(struct sep_device *sep,
						dma_addr_t bus_address)
{
	return sep->shared_addr + (bus_address - sep->shared_bus);
}

#endif

static int sep_open(struct inode *inode, struct file *filp)
{
	struct sep_device *sep;
	struct sep_private_data *priv;

	dev_dbg(&sep_dev->pdev->dev, "[PID%d] open\n", current->pid);

	if (filp->f_flags & O_NONBLOCK)
		return -ENOTSUPP;


	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	sep = sep_dev;
	priv->device = sep;
	filp->private_data = priv;

	dev_dbg(&sep_dev->pdev->dev, "[PID%d] priv is 0x%p\n",
					current->pid, priv);

	
	return 0;
}

int sep_free_dma_table_data_handler(struct sep_device *sep,
					   struct sep_dma_context **dma_ctx)
{
	int count;
	int dcb_counter;
	
	struct sep_dma_resource *dma;

	dev_dbg(&sep->pdev->dev,
		"[PID%d] sep_free_dma_table_data_handler\n",
			current->pid);

	if (!dma_ctx || !(*dma_ctx)) {
		
		dev_dbg(&sep->pdev->dev,
			"[PID%d] no DMA context or context already freed\n",
				current->pid);

		return 0;
	}

	dev_dbg(&sep->pdev->dev, "[PID%d] (*dma_ctx)->nr_dcb_creat 0x%x\n",
					current->pid,
					(*dma_ctx)->nr_dcb_creat);

	for (dcb_counter = 0;
	     dcb_counter < (*dma_ctx)->nr_dcb_creat; dcb_counter++) {
		dma = &(*dma_ctx)->dma_res_arr[dcb_counter];

		
		if (dma->in_map_array) {
			for (count = 0; count < dma->in_num_pages; count++) {
				dma_unmap_page(&sep->pdev->dev,
					dma->in_map_array[count].dma_addr,
					dma->in_map_array[count].size,
					DMA_TO_DEVICE);
			}
			kfree(dma->in_map_array);
		}

		if (((*dma_ctx)->secure_dma == false) &&
			(dma->out_map_array)) {

			for (count = 0; count < dma->out_num_pages; count++) {
				dma_unmap_page(&sep->pdev->dev,
					dma->out_map_array[count].dma_addr,
					dma->out_map_array[count].size,
					DMA_FROM_DEVICE);
			}
			kfree(dma->out_map_array);
		}

		
		if (dma->in_page_array) {
			for (count = 0; count < dma->in_num_pages; count++) {
				flush_dcache_page(dma->in_page_array[count]);
				page_cache_release(dma->in_page_array[count]);
			}
			kfree(dma->in_page_array);
		}

		
		if (((*dma_ctx)->secure_dma == false) &&
			(dma->out_page_array)) {

			for (count = 0; count < dma->out_num_pages; count++) {
				if (!PageReserved(dma->out_page_array[count]))

					SetPageDirty(dma->
					out_page_array[count]);

				flush_dcache_page(dma->out_page_array[count]);
				page_cache_release(dma->out_page_array[count]);
			}
			kfree(dma->out_page_array);
		}

		if (dma->src_sg) {
			dma_unmap_sg(&sep->pdev->dev, dma->src_sg,
				dma->in_map_num_entries, DMA_TO_DEVICE);
			dma->src_sg = NULL;
		}

		if (dma->dst_sg) {
			dma_unmap_sg(&sep->pdev->dev, dma->dst_sg,
				dma->in_map_num_entries, DMA_FROM_DEVICE);
			dma->dst_sg = NULL;
		}

		
		dma->in_page_array = NULL;
		dma->out_page_array = NULL;
		dma->in_num_pages = 0;
		dma->out_num_pages = 0;
		dma->in_map_array = NULL;
		dma->out_map_array = NULL;
		dma->in_map_num_entries = 0;
		dma->out_map_num_entries = 0;
	}

	(*dma_ctx)->nr_dcb_creat = 0;
	(*dma_ctx)->num_lli_tables_created = 0;

	kfree(*dma_ctx);
	*dma_ctx = NULL;

	dev_dbg(&sep->pdev->dev,
		"[PID%d] sep_free_dma_table_data_handler end\n",
			current->pid);

	return 0;
}

static int sep_end_transaction_handler(struct sep_device *sep,
				       struct sep_dma_context **dma_ctx,
				       struct sep_call_status *call_status,
				       struct sep_queue_info **my_queue_elem)
{
	dev_dbg(&sep->pdev->dev, "[PID%d] ending transaction\n", current->pid);

	if (sep_check_transaction_owner(sep)) {
		dev_dbg(&sep->pdev->dev, "[PID%d] not transaction owner\n",
						current->pid);
		return 0;
	}

	
	sep_queue_status_remove(sep, my_queue_elem);

	
	if (dma_ctx)
		sep_free_dma_table_data_handler(sep, dma_ctx);

	
	if (call_status)
		call_status->status = 0;

	memset(sep->shared_addr, 0,
	       SEP_DRIVER_MESSAGE_SHARED_AREA_SIZE_IN_BYTES);

	
#ifdef SEP_ENABLE_RUNTIME_PM
	if (sep->in_use) {
		sep->in_use = 0;
		pm_runtime_mark_last_busy(&sep->pdev->dev);
		pm_runtime_put_autosuspend(&sep->pdev->dev);
	}
#endif

	clear_bit(SEP_WORKING_LOCK_BIT, &sep->in_use_flags);
	sep->pid_doing_transaction = 0;

	
	dev_dbg(&sep->pdev->dev, "[PID%d] waking up next transaction\n",
					current->pid);
	clear_bit(SEP_TRANSACTION_STARTED_LOCK_BIT, &sep->in_use_flags);
	wake_up(&sep->event_transactions);

	return 0;
}


static int sep_release(struct inode *inode, struct file *filp)
{
	struct sep_private_data * const private_data = filp->private_data;
	struct sep_call_status *call_status = &private_data->call_status;
	struct sep_device *sep = private_data->device;
	struct sep_dma_context **dma_ctx = &private_data->dma_ctx;
	struct sep_queue_info **my_queue_elem = &private_data->my_queue_elem;

	dev_dbg(&sep->pdev->dev, "[PID%d] release\n", current->pid);

	sep_end_transaction_handler(sep, dma_ctx, call_status,
		my_queue_elem);

	kfree(filp->private_data);

	return 0;
}

static int sep_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct sep_private_data * const private_data = filp->private_data;
	struct sep_call_status *call_status = &private_data->call_status;
	struct sep_device *sep = private_data->device;
	struct sep_queue_info **my_queue_elem = &private_data->my_queue_elem;
	dma_addr_t bus_addr;
	unsigned long error = 0;

	dev_dbg(&sep->pdev->dev, "[PID%d] sep_mmap\n", current->pid);

	
	error = sep_wait_transaction(sep);
	if (error)
		
		goto end_function;

	memset(sep->shared_addr, 0,
	       SEP_DRIVER_MESSAGE_SHARED_AREA_SIZE_IN_BYTES);

	if ((vma->vm_end - vma->vm_start) > SEP_DRIVER_MMMAP_AREA_SIZE) {
		error = -EINVAL;
		goto end_function_with_error;
	}

	dev_dbg(&sep->pdev->dev, "[PID%d] shared_addr is %p\n",
					current->pid, sep->shared_addr);

	
	bus_addr = sep->shared_bus;

	if (remap_pfn_range(vma, vma->vm_start, bus_addr >> PAGE_SHIFT,
		vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		dev_dbg(&sep->pdev->dev, "[PID%d] remap_page_range failed\n",
						current->pid);
		error = -EAGAIN;
		goto end_function_with_error;
	}

	
	set_bit(SEP_LEGACY_MMAP_DONE_OFFSET, &call_status->status);

	goto end_function;

end_function_with_error:
	
	sep_end_transaction_handler(sep, NULL, call_status,
		my_queue_elem);

end_function:
	return error;
}

static unsigned int sep_poll(struct file *filp, poll_table *wait)
{
	struct sep_private_data * const private_data = filp->private_data;
	struct sep_call_status *call_status = &private_data->call_status;
	struct sep_device *sep = private_data->device;
	u32 mask = 0;
	u32 retval = 0;
	u32 retval2 = 0;
	unsigned long lock_irq_flag;

	
	if (sep_check_transaction_owner(sep)) {
		dev_dbg(&sep->pdev->dev, "[PID%d] poll pid not owner\n",
						current->pid);
		mask = POLLERR;
		goto end_function;
	}

	
	if (0 == test_bit(SEP_LEGACY_SENDMSG_DONE_OFFSET,
			  &call_status->status)) {
		dev_warn(&sep->pdev->dev, "[PID%d] sendmsg not called\n",
						current->pid);
		mask = POLLERR;
		goto end_function;
	}


	
	dev_dbg(&sep->pdev->dev, "[PID%d] poll: calling wait sep_event\n",
					current->pid);

	poll_wait(filp, &sep->event_interrupt, wait);

	dev_dbg(&sep->pdev->dev,
		"[PID%d] poll: send_ct is %lx reply ct is %lx\n",
			current->pid, sep->send_ct, sep->reply_ct);

	
	retval2 = sep_read_reg(sep, HW_HOST_SEP_HOST_GPR3_REG_ADDR);
	if ((retval2 != 0x0) && (retval2 != 0x8)) {
		dev_dbg(&sep->pdev->dev, "[PID%d] poll; poll error %x\n",
						current->pid, retval2);
		mask |= POLLERR;
		goto end_function;
	}

	spin_lock_irqsave(&sep->snd_rply_lck, lock_irq_flag);

	if (sep->send_ct == sep->reply_ct) {
		spin_unlock_irqrestore(&sep->snd_rply_lck, lock_irq_flag);
		retval = sep_read_reg(sep, HW_HOST_SEP_HOST_GPR2_REG_ADDR);
		dev_dbg(&sep->pdev->dev,
			"[PID%d] poll: data ready check (GPR2)  %x\n",
				current->pid, retval);

		
		if ((retval >> 30) & 0x1) {
			dev_dbg(&sep->pdev->dev,
				"[PID%d] poll: SEP printf request\n",
					current->pid);
			goto end_function;
		}

		
		if (retval >> 31) {
			dev_dbg(&sep->pdev->dev,
				"[PID%d] poll: SEP request\n",
					current->pid);
		} else {
			dev_dbg(&sep->pdev->dev,
				"[PID%d] poll: normal return\n",
					current->pid);
			sep_dump_message(sep);
			dev_dbg(&sep->pdev->dev,
				"[PID%d] poll; SEP reply POLLIN|POLLRDNORM\n",
					current->pid);
			mask |= POLLIN | POLLRDNORM;
		}
		set_bit(SEP_LEGACY_POLL_DONE_OFFSET, &call_status->status);
	} else {
		spin_unlock_irqrestore(&sep->snd_rply_lck, lock_irq_flag);
		dev_dbg(&sep->pdev->dev,
			"[PID%d] poll; no reply; returning mask of 0\n",
				current->pid);
		mask = 0;
	}

end_function:
	return mask;
}

static u32 *sep_time_address(struct sep_device *sep)
{
	return sep->shared_addr +
		SEP_DRIVER_SYSTEM_TIME_MEMORY_OFFSET_IN_BYTES;
}

static unsigned long sep_set_time(struct sep_device *sep)
{
	struct timeval time;
	u32 *time_addr;	


	do_gettimeofday(&time);

	
	time_addr = sep_time_address(sep);

	time_addr[0] = SEP_TIME_VAL_TOKEN;
	time_addr[1] = time.tv_sec;

	dev_dbg(&sep->pdev->dev, "[PID%d] time.tv_sec is %lu\n",
					current->pid, time.tv_sec);
	dev_dbg(&sep->pdev->dev, "[PID%d] time_addr is %p\n",
					current->pid, time_addr);
	dev_dbg(&sep->pdev->dev, "[PID%d] sep->shared_addr is %p\n",
					current->pid, sep->shared_addr);

	return time.tv_sec;
}

int sep_send_command_handler(struct sep_device *sep)
{
	unsigned long lock_irq_flag;
	u32 *msg_pool;
	int error = 0;

	
	msg_pool = (u32 *)sep->shared_addr;
	msg_pool += 2;

	
	if (*msg_pool != SEP_START_MSG_TOKEN) {
		dev_warn(&sep->pdev->dev, "start message token not present\n");
		error = -EPROTO;
		goto end_function;
	}

	
	msg_pool += 1;
	if ((*msg_pool < 2) ||
		(*msg_pool > SEP_DRIVER_MAX_MESSAGE_SIZE_IN_BYTES)) {

		dev_warn(&sep->pdev->dev, "invalid message size\n");
		error = -EPROTO;
		goto end_function;
	}

	
	msg_pool += 1;
	if (*msg_pool < 2) {
		dev_warn(&sep->pdev->dev, "invalid message opcode\n");
		error = -EPROTO;
		goto end_function;
	}

#if defined(CONFIG_PM_RUNTIME) && defined(SEP_ENABLE_RUNTIME_PM)
	dev_dbg(&sep->pdev->dev, "[PID%d] before pm sync status 0x%X\n",
					current->pid,
					sep->pdev->dev.power.runtime_status);
	sep->in_use = 1; 
	pm_runtime_get_sync(&sep->pdev->dev);
#endif

	if (test_and_set_bit(SEP_WORKING_LOCK_BIT, &sep->in_use_flags)) {
		error = -EPROTO;
		goto end_function;
	}
	sep->in_use = 1; 
	sep_set_time(sep);

	sep_dump_message(sep);

	
	spin_lock_irqsave(&sep->snd_rply_lck, lock_irq_flag);
	sep->send_ct++;
	spin_unlock_irqrestore(&sep->snd_rply_lck, lock_irq_flag);

	dev_dbg(&sep->pdev->dev,
		"[PID%d] sep_send_command_handler send_ct %lx reply_ct %lx\n",
			current->pid, sep->send_ct, sep->reply_ct);

	
	sep_write_reg(sep, HW_HOST_HOST_SEP_GPR0_REG_ADDR, 0x2);

end_function:
	return error;
}

static int sep_crypto_dma(
	struct sep_device *sep,
	struct scatterlist *sg,
	struct sep_dma_map **dma_maps,
	enum dma_data_direction direction)
{
	struct scatterlist *temp_sg;

	u32 count_segment;
	u32 count_mapped;
	struct sep_dma_map *sep_dma;
	int ct1;

	if (sg->length == 0)
		return 0;

	
	temp_sg = sg;
	count_segment = 0;
	while (temp_sg) {
		count_segment += 1;
		temp_sg = scatterwalk_sg_next(temp_sg);
	}
	dev_dbg(&sep->pdev->dev,
		"There are (hex) %x segments in sg\n", count_segment);

	
	count_mapped = dma_map_sg(&sep->pdev->dev, sg,
		count_segment, direction);

	dev_dbg(&sep->pdev->dev,
		"There are (hex) %x maps in sg\n", count_mapped);

	if (count_mapped == 0) {
		dev_dbg(&sep->pdev->dev, "Cannot dma_map_sg\n");
		return -ENOMEM;
	}

	sep_dma = kmalloc(sizeof(struct sep_dma_map) *
		count_mapped, GFP_ATOMIC);

	if (sep_dma == NULL) {
		dev_dbg(&sep->pdev->dev, "Cannot allocate dma_maps\n");
		return -ENOMEM;
	}

	for_each_sg(sg, temp_sg, count_mapped, ct1) {
		sep_dma[ct1].dma_addr = sg_dma_address(temp_sg);
		sep_dma[ct1].size = sg_dma_len(temp_sg);
		dev_dbg(&sep->pdev->dev, "(all hex) map %x dma %lx len %lx\n",
			ct1, (unsigned long)sep_dma[ct1].dma_addr,
			(unsigned long)sep_dma[ct1].size);
		}

	*dma_maps = sep_dma;
	return count_mapped;

}

static int sep_crypto_lli(
	struct sep_device *sep,
	struct scatterlist *sg,
	struct sep_dma_map **maps,
	struct sep_lli_entry **llis,
	u32 data_size,
	enum dma_data_direction direction)
{

	int ct1;
	struct sep_lli_entry *sep_lli;
	struct sep_dma_map *sep_map;

	int nbr_ents;

	nbr_ents = sep_crypto_dma(sep, sg, maps, direction);
	if (nbr_ents <= 0) {
		dev_dbg(&sep->pdev->dev, "crypto_dma failed %x\n",
			nbr_ents);
		return nbr_ents;
	}

	sep_map = *maps;

	sep_lli = kmalloc(sizeof(struct sep_lli_entry) * nbr_ents, GFP_ATOMIC);

	if (sep_lli == NULL) {
		dev_dbg(&sep->pdev->dev, "Cannot allocate lli_maps\n");

		kfree(*maps);
		*maps = NULL;
		return -ENOMEM;
	}

	for (ct1 = 0; ct1 < nbr_ents; ct1 += 1) {
		sep_lli[ct1].bus_address = (u32)sep_map[ct1].dma_addr;

		
		if (sep_map[ct1].size > data_size)
			sep_map[ct1].size = data_size;

		sep_lli[ct1].block_size = (u32)sep_map[ct1].size;
	}

	*llis = sep_lli;
	return nbr_ents;
}

static int sep_lock_kernel_pages(struct sep_device *sep,
	unsigned long kernel_virt_addr,
	u32 data_size,
	struct sep_lli_entry **lli_array_ptr,
	int in_out_flag,
	struct sep_dma_context *dma_ctx)

{
	u32 num_pages;
	struct scatterlist *sg;

	
	struct sep_lli_entry *lli_array;
	
	struct sep_dma_map *map_array;

	enum dma_data_direction direction;

	lli_array = NULL;
	map_array = NULL;

	if (in_out_flag == SEP_DRIVER_IN_FLAG) {
		direction = DMA_TO_DEVICE;
		sg = dma_ctx->src_sg;
	} else {
		direction = DMA_FROM_DEVICE;
		sg = dma_ctx->dst_sg;
	}

	num_pages = sep_crypto_lli(sep, sg, &map_array, &lli_array,
		data_size, direction);

	if (num_pages <= 0) {
		dev_dbg(&sep->pdev->dev, "sep_crypto_lli returned error %x\n",
			num_pages);
		return -ENOMEM;
	}

	

	
	if (in_out_flag == SEP_DRIVER_IN_FLAG) {
		*lli_array_ptr = lli_array;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_num_pages =
								num_pages;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_page_array =
								NULL;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_map_array =
								map_array;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_map_num_entries =
								num_pages;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].src_sg =
			dma_ctx->src_sg;
	} else {
		*lli_array_ptr = lli_array;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_num_pages =
								num_pages;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_page_array =
								NULL;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_map_array =
								map_array;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].
					out_map_num_entries = num_pages;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].dst_sg =
			dma_ctx->dst_sg;
	}

	return 0;
}

static int sep_lock_user_pages(struct sep_device *sep,
	u32 app_virt_addr,
	u32 data_size,
	struct sep_lli_entry **lli_array_ptr,
	int in_out_flag,
	struct sep_dma_context *dma_ctx)

{
	int error = 0;
	u32 count;
	int result;
	
	u32 end_page;
	
	u32 start_page;
	
	u32 num_pages;
	
	struct page **page_array;
	
	struct sep_lli_entry *lli_array;
	
	struct sep_dma_map *map_array;

	
	end_page = (app_virt_addr + data_size - 1) >> PAGE_SHIFT;
	start_page = app_virt_addr >> PAGE_SHIFT;
	num_pages = end_page - start_page + 1;

	dev_dbg(&sep->pdev->dev,
		"[PID%d] lock user pages app_virt_addr is %x\n",
			current->pid, app_virt_addr);

	dev_dbg(&sep->pdev->dev, "[PID%d] data_size is (hex) %x\n",
					current->pid, data_size);
	dev_dbg(&sep->pdev->dev, "[PID%d] start_page is (hex) %x\n",
					current->pid, start_page);
	dev_dbg(&sep->pdev->dev, "[PID%d] end_page is (hex) %x\n",
					current->pid, end_page);
	dev_dbg(&sep->pdev->dev, "[PID%d] num_pages is (hex) %x\n",
					current->pid, num_pages);

	
	page_array = kmalloc(sizeof(struct page *) * num_pages, GFP_ATOMIC);
	if (!page_array) {
		error = -ENOMEM;
		goto end_function;
	}
	map_array = kmalloc(sizeof(struct sep_dma_map) * num_pages, GFP_ATOMIC);
	if (!map_array) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] kmalloc for map_array failed\n",
				current->pid);
		error = -ENOMEM;
		goto end_function_with_error1;
	}

	lli_array = kmalloc(sizeof(struct sep_lli_entry) * num_pages,
		GFP_ATOMIC);

	if (!lli_array) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] kmalloc for lli_array failed\n",
				current->pid);
		error = -ENOMEM;
		goto end_function_with_error2;
	}

	
	down_read(&current->mm->mmap_sem);
	result = get_user_pages(current, current->mm, app_virt_addr,
		num_pages,
		((in_out_flag == SEP_DRIVER_IN_FLAG) ? 0 : 1),
		0, page_array, NULL);

	up_read(&current->mm->mmap_sem);

	
	if (result != num_pages) {
		dev_warn(&sep->pdev->dev,
			"[PID%d] not all pages locked by get_user_pages, "
			"result 0x%X, num_pages 0x%X\n",
				current->pid, result, num_pages);
		error = -ENOMEM;
		goto end_function_with_error3;
	}

	dev_dbg(&sep->pdev->dev, "[PID%d] get_user_pages succeeded\n",
					current->pid);

	for (count = 0; count < num_pages; count++) {
		
		map_array[count].dma_addr =
			dma_map_page(&sep->pdev->dev, page_array[count],
			0, PAGE_SIZE, DMA_BIDIRECTIONAL);

		map_array[count].size = PAGE_SIZE;

		
		lli_array[count].bus_address = (u32)map_array[count].dma_addr;
		lli_array[count].block_size = PAGE_SIZE;

		dev_dbg(&sep->pdev->dev,
			"[PID%d] lli_array[%x].bus_address is %08lx, "
			"lli_array[%x].block_size is (hex) %x\n", current->pid,
			count, (unsigned long)lli_array[count].bus_address,
			count, lli_array[count].block_size);
	}

	
	lli_array[0].bus_address =
		lli_array[0].bus_address + (app_virt_addr & (~PAGE_MASK));

	
	if ((PAGE_SIZE - (app_virt_addr & (~PAGE_MASK))) >= data_size)
		lli_array[0].block_size = data_size;
	else
		lli_array[0].block_size =
			PAGE_SIZE - (app_virt_addr & (~PAGE_MASK));

		dev_dbg(&sep->pdev->dev,
			"[PID%d] After check if page 0 has all data\n",
			current->pid);
		dev_dbg(&sep->pdev->dev,
			"[PID%d] lli_array[0].bus_address is (hex) %08lx, "
			"lli_array[0].block_size is (hex) %x\n",
			current->pid,
			(unsigned long)lli_array[0].bus_address,
			lli_array[0].block_size);


	
	if (num_pages > 1) {
		lli_array[num_pages - 1].block_size =
			(app_virt_addr + data_size) & (~PAGE_MASK);
		if (lli_array[num_pages - 1].block_size == 0)
			lli_array[num_pages - 1].block_size = PAGE_SIZE;

		dev_dbg(&sep->pdev->dev,
			"[PID%d] After last page size adjustment\n",
			current->pid);
		dev_dbg(&sep->pdev->dev,
			"[PID%d] lli_array[%x].bus_address is (hex) %08lx, "
			"lli_array[%x].block_size is (hex) %x\n",
			current->pid,
			num_pages - 1,
			(unsigned long)lli_array[num_pages - 1].bus_address,
			num_pages - 1,
			lli_array[num_pages - 1].block_size);
	}

	
	if (in_out_flag == SEP_DRIVER_IN_FLAG) {
		*lli_array_ptr = lli_array;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_num_pages =
								num_pages;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_page_array =
								page_array;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_map_array =
								map_array;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_map_num_entries =
								num_pages;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].src_sg = NULL;
	} else {
		*lli_array_ptr = lli_array;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_num_pages =
								num_pages;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_page_array =
								page_array;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_map_array =
								map_array;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].
					out_map_num_entries = num_pages;
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].dst_sg = NULL;
	}
	goto end_function;

end_function_with_error3:
	
	kfree(lli_array);

end_function_with_error2:
	kfree(map_array);

end_function_with_error1:
	
	kfree(page_array);

end_function:
	return error;
}

static int sep_lli_table_secure_dma(struct sep_device *sep,
	u32 app_virt_addr,
	u32 data_size,
	struct sep_lli_entry **lli_array_ptr,
	int in_out_flag,
	struct sep_dma_context *dma_ctx)

{
	int error = 0;
	u32 count;
	
	u32 end_page;
	
	u32 start_page;
	
	u32 num_pages;
	
	struct sep_lli_entry *lli_array;

	
	end_page = (app_virt_addr + data_size - 1) >> PAGE_SHIFT;
	start_page = app_virt_addr >> PAGE_SHIFT;
	num_pages = end_page - start_page + 1;

	dev_dbg(&sep->pdev->dev, "[PID%d] lock user pages"
		" app_virt_addr is %x\n", current->pid, app_virt_addr);

	dev_dbg(&sep->pdev->dev, "[PID%d] data_size is (hex) %x\n",
		current->pid, data_size);
	dev_dbg(&sep->pdev->dev, "[PID%d] start_page is (hex) %x\n",
		current->pid, start_page);
	dev_dbg(&sep->pdev->dev, "[PID%d] end_page is (hex) %x\n",
		current->pid, end_page);
	dev_dbg(&sep->pdev->dev, "[PID%d] num_pages is (hex) %x\n",
		current->pid, num_pages);

	lli_array = kmalloc(sizeof(struct sep_lli_entry) * num_pages,
		GFP_ATOMIC);

	if (!lli_array) {
		dev_warn(&sep->pdev->dev,
			"[PID%d] kmalloc for lli_array failed\n",
			current->pid);
		return -ENOMEM;
	}

	start_page = start_page << PAGE_SHIFT;
	for (count = 0; count < num_pages; count++) {
		
		lli_array[count].bus_address = start_page;
		lli_array[count].block_size = PAGE_SIZE;

		start_page += PAGE_SIZE;

		dev_dbg(&sep->pdev->dev,
			"[PID%d] lli_array[%x].bus_address is %08lx, "
			"lli_array[%x].block_size is (hex) %x\n",
			current->pid,
			count, (unsigned long)lli_array[count].bus_address,
			count, lli_array[count].block_size);
	}

	
	lli_array[0].bus_address =
		lli_array[0].bus_address + (app_virt_addr & (~PAGE_MASK));

	
	if ((PAGE_SIZE - (app_virt_addr & (~PAGE_MASK))) >= data_size)
		lli_array[0].block_size = data_size;
	else
		lli_array[0].block_size =
			PAGE_SIZE - (app_virt_addr & (~PAGE_MASK));

	dev_dbg(&sep->pdev->dev,
		"[PID%d] After check if page 0 has all data\n"
		"lli_array[0].bus_address is (hex) %08lx, "
		"lli_array[0].block_size is (hex) %x\n",
		current->pid,
		(unsigned long)lli_array[0].bus_address,
		lli_array[0].block_size);

	
	if (num_pages > 1) {
		lli_array[num_pages - 1].block_size =
			(app_virt_addr + data_size) & (~PAGE_MASK);
		if (lli_array[num_pages - 1].block_size == 0)
			lli_array[num_pages - 1].block_size = PAGE_SIZE;

		dev_dbg(&sep->pdev->dev,
			"[PID%d] After last page size adjustment\n"
			"lli_array[%x].bus_address is (hex) %08lx, "
			"lli_array[%x].block_size is (hex) %x\n",
			current->pid, num_pages - 1,
			(unsigned long)lli_array[num_pages - 1].bus_address,
			num_pages - 1,
			lli_array[num_pages - 1].block_size);
	}
	*lli_array_ptr = lli_array;
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_num_pages = num_pages;
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_page_array = NULL;
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_map_array = NULL;
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_map_num_entries = 0;

	return error;
}

static u32 sep_calculate_lli_table_max_size(struct sep_device *sep,
	struct sep_lli_entry *lli_in_array_ptr,
	u32 num_array_entries,
	u32 *last_table_flag)
{
	u32 counter;
	
	u32 table_data_size = 0;
	
	u32 next_table_data_size;

	*last_table_flag = 0;

	for (counter = 0;
		(counter < (SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP - 1)) &&
			(counter < num_array_entries); counter++)
		table_data_size += lli_in_array_ptr[counter].block_size;

	if (counter == num_array_entries) {
		
		*last_table_flag = 1;
		goto end_function;
	}

	next_table_data_size = 0;
	for (; counter < num_array_entries; counter++) {
		next_table_data_size += lli_in_array_ptr[counter].block_size;
		if (next_table_data_size >= SEP_DRIVER_MIN_DATA_SIZE_PER_TABLE)
			break;
	}

	if (next_table_data_size &&
		next_table_data_size < SEP_DRIVER_MIN_DATA_SIZE_PER_TABLE)

		table_data_size -= (SEP_DRIVER_MIN_DATA_SIZE_PER_TABLE -
			next_table_data_size);

end_function:
	return table_data_size;
}

static void sep_build_lli_table(struct sep_device *sep,
	struct sep_lli_entry	*lli_array_ptr,
	struct sep_lli_entry	*lli_table_ptr,
	u32 *num_processed_entries_ptr,
	u32 *num_table_entries_ptr,
	u32 table_data_size)
{
	
	u32 curr_table_data_size;
	
	u32 array_counter;

	
	curr_table_data_size = 0;
	array_counter = 0;
	*num_table_entries_ptr = 1;

	dev_dbg(&sep->pdev->dev,
		"[PID%d] build lli table table_data_size: (hex) %x\n",
			current->pid, table_data_size);

	
	while (curr_table_data_size < table_data_size) {
		
		(*num_table_entries_ptr)++;

		lli_table_ptr->bus_address =
			cpu_to_le32(lli_array_ptr[array_counter].bus_address);

		lli_table_ptr->block_size =
			cpu_to_le32(lli_array_ptr[array_counter].block_size);

		curr_table_data_size += lli_array_ptr[array_counter].block_size;

		dev_dbg(&sep->pdev->dev,
			"[PID%d] lli_table_ptr is %p\n",
				current->pid, lli_table_ptr);
		dev_dbg(&sep->pdev->dev,
			"[PID%d] lli_table_ptr->bus_address: %08lx\n",
				current->pid,
				(unsigned long)lli_table_ptr->bus_address);

		dev_dbg(&sep->pdev->dev,
			"[PID%d] lli_table_ptr->block_size is (hex) %x\n",
				current->pid, lli_table_ptr->block_size);

		
		if (curr_table_data_size > table_data_size) {
			dev_dbg(&sep->pdev->dev,
				"[PID%d] curr_table_data_size too large\n",
					current->pid);

			
			lli_table_ptr->block_size =
				cpu_to_le32(lli_table_ptr->block_size) -
				(curr_table_data_size - table_data_size);

			
			lli_array_ptr[array_counter].bus_address +=
			cpu_to_le32(lli_table_ptr->block_size);

			
			lli_array_ptr[array_counter].block_size =
				(curr_table_data_size - table_data_size);
		} else
			
			array_counter++;

		dev_dbg(&sep->pdev->dev,
			"[PID%d] lli_table_ptr->bus_address is %08lx\n",
				current->pid,
				(unsigned long)lli_table_ptr->bus_address);
		dev_dbg(&sep->pdev->dev,
			"[PID%d] lli_table_ptr->block_size is (hex) %x\n",
				current->pid,
				lli_table_ptr->block_size);

		
		lli_table_ptr++;
	}

	
	lli_table_ptr->bus_address = 0xffffffff;
	lli_table_ptr->block_size = 0;

	
	*num_processed_entries_ptr += array_counter;

}

static dma_addr_t sep_shared_area_virt_to_bus(struct sep_device *sep,
	void *virt_address)
{
	dev_dbg(&sep->pdev->dev, "[PID%d] sh virt to phys v %p\n",
					current->pid, virt_address);
	dev_dbg(&sep->pdev->dev, "[PID%d] sh virt to phys p %08lx\n",
		current->pid,
		(unsigned long)
		sep->shared_bus + (virt_address - sep->shared_addr));

	return sep->shared_bus + (size_t)(virt_address - sep->shared_addr);
}

static void *sep_shared_area_bus_to_virt(struct sep_device *sep,
	dma_addr_t bus_address)
{
	dev_dbg(&sep->pdev->dev, "[PID%d] shared bus to virt b=%lx v=%lx\n",
		current->pid,
		(unsigned long)bus_address, (unsigned long)(sep->shared_addr +
			(size_t)(bus_address - sep->shared_bus)));

	return sep->shared_addr	+ (size_t)(bus_address - sep->shared_bus);
}

static void sep_debug_print_lli_tables(struct sep_device *sep,
	struct sep_lli_entry *lli_table_ptr,
	unsigned long num_table_entries,
	unsigned long table_data_size)
{
#ifdef DEBUG
	unsigned long table_count = 1;
	unsigned long entries_count = 0;

	dev_dbg(&sep->pdev->dev, "[PID%d] sep_debug_print_lli_tables start\n",
					current->pid);
	if (num_table_entries == 0) {
		dev_dbg(&sep->pdev->dev, "[PID%d] no table to print\n",
			current->pid);
		return;
	}

	while ((unsigned long) lli_table_ptr->bus_address != 0xffffffff) {
		dev_dbg(&sep->pdev->dev,
			"[PID%d] lli table %08lx, "
			"table_data_size is (hex) %lx\n",
				current->pid, table_count, table_data_size);
		dev_dbg(&sep->pdev->dev,
			"[PID%d] num_table_entries is (hex) %lx\n",
				current->pid, num_table_entries);

		
		for (entries_count = 0; entries_count < num_table_entries;
			entries_count++, lli_table_ptr++) {

			dev_dbg(&sep->pdev->dev,
				"[PID%d] lli_table_ptr address is %08lx\n",
				current->pid,
				(unsigned long) lli_table_ptr);

			dev_dbg(&sep->pdev->dev,
				"[PID%d] phys address is %08lx "
				"block size is (hex) %x\n", current->pid,
				(unsigned long)lli_table_ptr->bus_address,
				lli_table_ptr->block_size);
		}

		
		lli_table_ptr--;

		dev_dbg(&sep->pdev->dev,
			"[PID%d] phys lli_table_ptr->block_size "
			"is (hex) %x\n",
			current->pid,
			lli_table_ptr->block_size);

		dev_dbg(&sep->pdev->dev,
			"[PID%d] phys lli_table_ptr->physical_address "
			"is %08lx\n",
			current->pid,
			(unsigned long)lli_table_ptr->bus_address);


		table_data_size = lli_table_ptr->block_size & 0xffffff;
		num_table_entries = (lli_table_ptr->block_size >> 24) & 0xff;

		dev_dbg(&sep->pdev->dev,
			"[PID%d] phys table_data_size is "
			"(hex) %lx num_table_entries is"
			" %lx bus_address is%lx\n",
				current->pid,
				table_data_size,
				num_table_entries,
				(unsigned long)lli_table_ptr->bus_address);

		if ((unsigned long)lli_table_ptr->bus_address != 0xffffffff)
			lli_table_ptr = (struct sep_lli_entry *)
				sep_shared_bus_to_virt(sep,
				(unsigned long)lli_table_ptr->bus_address);

		table_count++;
	}
	dev_dbg(&sep->pdev->dev, "[PID%d] sep_debug_print_lli_tables end\n",
					current->pid);
#endif
}


static void sep_prepare_empty_lli_table(struct sep_device *sep,
		dma_addr_t *lli_table_addr_ptr,
		u32 *num_entries_ptr,
		u32 *table_data_size_ptr,
		void **dmatables_region,
		struct sep_dma_context *dma_ctx)
{
	struct sep_lli_entry *lli_table_ptr;

	
	lli_table_ptr =
		(struct sep_lli_entry *)(sep->shared_addr +
		SYNCHRONIC_DMA_TABLES_AREA_OFFSET_BYTES +
		dma_ctx->num_lli_tables_created * sizeof(struct sep_lli_entry) *
			SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP);

	if (dmatables_region && *dmatables_region)
		lli_table_ptr = *dmatables_region;

	lli_table_ptr->bus_address = 0;
	lli_table_ptr->block_size = 0;

	lli_table_ptr++;
	lli_table_ptr->bus_address = 0xFFFFFFFF;
	lli_table_ptr->block_size = 0;

	
	*lli_table_addr_ptr = sep->shared_bus +
		SYNCHRONIC_DMA_TABLES_AREA_OFFSET_BYTES +
		dma_ctx->num_lli_tables_created *
		sizeof(struct sep_lli_entry) *
		SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP;

	
	*num_entries_ptr = 2;
	*table_data_size_ptr = 0;

	
	dma_ctx->num_lli_tables_created++;
}

static int sep_prepare_input_dma_table(struct sep_device *sep,
	unsigned long app_virt_addr,
	u32 data_size,
	u32 block_size,
	dma_addr_t *lli_table_ptr,
	u32 *num_entries_ptr,
	u32 *table_data_size_ptr,
	bool is_kva,
	void **dmatables_region,
	struct sep_dma_context *dma_ctx
)
{
	int error = 0;
	
	struct sep_lli_entry *info_entry_ptr;
	
	struct sep_lli_entry *lli_array_ptr;
	
	u32 current_entry = 0;
	
	u32 sep_lli_entries = 0;
	
	struct sep_lli_entry *in_lli_table_ptr;
	
	u32 table_data_size = 0;
	
	u32 last_table_flag = 0;
	
	u32 num_entries_in_table = 0;
	
	void *lli_table_alloc_addr = NULL;
	void *dma_lli_table_alloc_addr = NULL;
	void *dma_in_lli_table_ptr = NULL;

	dev_dbg(&sep->pdev->dev, "[PID%d] prepare intput dma "
				 "tbl data size: (hex) %x\n",
					current->pid, data_size);

	dev_dbg(&sep->pdev->dev, "[PID%d] block_size is (hex) %x\n",
					current->pid, block_size);

	
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_page_array = NULL;
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_num_pages = 0;

	
	lli_table_alloc_addr = (void *)(sep->shared_addr +
		SYNCHRONIC_DMA_TABLES_AREA_OFFSET_BYTES +
		dma_ctx->num_lli_tables_created * sizeof(struct sep_lli_entry) *
		SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP);

	if (data_size == 0) {
		if (dmatables_region) {
			error = sep_allocate_dmatables_region(sep,
						dmatables_region,
						dma_ctx,
						1);
			if (error)
				return error;
		}
		
		sep_prepare_empty_lli_table(sep, lli_table_ptr,
				num_entries_ptr, table_data_size_ptr,
				dmatables_region, dma_ctx);
		goto update_dcb_counter;
	}

	
	if (is_kva == true)
		error = sep_lock_kernel_pages(sep, app_virt_addr,
			data_size, &lli_array_ptr, SEP_DRIVER_IN_FLAG,
			dma_ctx);
	else
		error = sep_lock_user_pages(sep, app_virt_addr,
			data_size, &lli_array_ptr, SEP_DRIVER_IN_FLAG,
			dma_ctx);

	if (error)
		goto end_function;

	dev_dbg(&sep->pdev->dev,
		"[PID%d] output sep_in_num_pages is (hex) %x\n",
		current->pid,
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_num_pages);

	current_entry = 0;
	info_entry_ptr = NULL;

	sep_lli_entries =
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_num_pages;

	dma_lli_table_alloc_addr = lli_table_alloc_addr;
	if (dmatables_region) {
		error = sep_allocate_dmatables_region(sep,
					dmatables_region,
					dma_ctx,
					sep_lli_entries);
		if (error)
			return error;
		lli_table_alloc_addr = *dmatables_region;
	}

	
	while (current_entry < sep_lli_entries) {

		
		in_lli_table_ptr =
			(struct sep_lli_entry *)lli_table_alloc_addr;
		dma_in_lli_table_ptr =
			(struct sep_lli_entry *)dma_lli_table_alloc_addr;

		lli_table_alloc_addr += sizeof(struct sep_lli_entry) *
			SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP;
		dma_lli_table_alloc_addr += sizeof(struct sep_lli_entry) *
			SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP;

		if (dma_lli_table_alloc_addr >
			((void *)sep->shared_addr +
			SYNCHRONIC_DMA_TABLES_AREA_OFFSET_BYTES +
			SYNCHRONIC_DMA_TABLES_AREA_SIZE_BYTES)) {

			error = -ENOMEM;
			goto end_function_error;

		}

		
		dma_ctx->num_lli_tables_created++;

		
		table_data_size = sep_calculate_lli_table_max_size(sep,
			&lli_array_ptr[current_entry],
			(sep_lli_entries - current_entry),
			&last_table_flag);

		if (!last_table_flag)
			table_data_size =
				(table_data_size / block_size) * block_size;

		dev_dbg(&sep->pdev->dev,
			"[PID%d] output table_data_size is (hex) %x\n",
				current->pid,
				table_data_size);

		
		sep_build_lli_table(sep, &lli_array_ptr[current_entry],
			in_lli_table_ptr,
			&current_entry, &num_entries_in_table, table_data_size);

		if (info_entry_ptr == NULL) {

			
			*lli_table_ptr = sep_shared_area_virt_to_bus(sep,
				dma_in_lli_table_ptr);
			*num_entries_ptr = num_entries_in_table;
			*table_data_size_ptr = table_data_size;

			dev_dbg(&sep->pdev->dev,
				"[PID%d] output lli_table_in_ptr is %08lx\n",
				current->pid,
				(unsigned long)*lli_table_ptr);

		} else {
			
			info_entry_ptr->bus_address =
				sep_shared_area_virt_to_bus(sep,
							dma_in_lli_table_ptr);
			info_entry_ptr->block_size =
				((num_entries_in_table) << 24) |
				(table_data_size);
		}
		
		info_entry_ptr = in_lli_table_ptr + num_entries_in_table - 1;
	}
	
	if (!dmatables_region) {
		sep_debug_print_lli_tables(sep, (struct sep_lli_entry *)
			sep_shared_area_bus_to_virt(sep, *lli_table_ptr),
			*num_entries_ptr, *table_data_size_ptr);
	}

	
	kfree(lli_array_ptr);

update_dcb_counter:
	
	dma_ctx->nr_dcb_creat++;
	goto end_function;

end_function_error:
	
	kfree(dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_map_array);
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_map_array = NULL;
	kfree(lli_array_ptr);
	kfree(dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_page_array);
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_page_array = NULL;

end_function:
	return error;

}

static int sep_construct_dma_tables_from_lli(
	struct sep_device *sep,
	struct sep_lli_entry *lli_in_array,
	u32	sep_in_lli_entries,
	struct sep_lli_entry *lli_out_array,
	u32	sep_out_lli_entries,
	u32	block_size,
	dma_addr_t *lli_table_in_ptr,
	dma_addr_t *lli_table_out_ptr,
	u32	*in_num_entries_ptr,
	u32	*out_num_entries_ptr,
	u32	*table_data_size_ptr,
	void	**dmatables_region,
	struct sep_dma_context *dma_ctx)
{
	
	void *lli_table_alloc_addr = NULL;
	void *dma_lli_table_alloc_addr = NULL;
	
	struct sep_lli_entry *in_lli_table_ptr = NULL;
	
	struct sep_lli_entry *dma_in_lli_table_ptr = NULL;
	
	struct sep_lli_entry *out_lli_table_ptr = NULL;
	
	struct sep_lli_entry *dma_out_lli_table_ptr = NULL;
	
	struct sep_lli_entry *info_in_entry_ptr = NULL;
	
	struct sep_lli_entry *info_out_entry_ptr = NULL;
	
	u32 current_in_entry = 0;
	
	u32 current_out_entry = 0;
	
	u32 in_table_data_size = 0;
	
	u32 out_table_data_size = 0;
	
	u32 last_table_flag = 0;
	
	u32 table_data_size = 0;
	
	u32 num_entries_in_table = 0;
	
	u32 num_entries_out_table = 0;

	if (!dma_ctx) {
		dev_warn(&sep->pdev->dev, "DMA context uninitialized\n");
		return -EINVAL;
	}

	
	lli_table_alloc_addr = (void *)(sep->shared_addr +
		SYNCHRONIC_DMA_TABLES_AREA_OFFSET_BYTES +
		(dma_ctx->num_lli_tables_created *
		(sizeof(struct sep_lli_entry) *
		SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP)));
	dma_lli_table_alloc_addr = lli_table_alloc_addr;

	if (dmatables_region) {
		
		if (sep_allocate_dmatables_region(sep,
					dmatables_region,
					dma_ctx,
					2*sep_in_lli_entries))
			return -ENOMEM;
		lli_table_alloc_addr = *dmatables_region;
	}

	
	while (current_in_entry < sep_in_lli_entries) {
		
		in_lli_table_ptr =
			(struct sep_lli_entry *)lli_table_alloc_addr;
		dma_in_lli_table_ptr =
			(struct sep_lli_entry *)dma_lli_table_alloc_addr;

		lli_table_alloc_addr += sizeof(struct sep_lli_entry) *
			SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP;
		dma_lli_table_alloc_addr += sizeof(struct sep_lli_entry) *
			SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP;

		
		out_lli_table_ptr =
			(struct sep_lli_entry *)lli_table_alloc_addr;
		dma_out_lli_table_ptr =
			(struct sep_lli_entry *)dma_lli_table_alloc_addr;

		
		if ((dma_lli_table_alloc_addr + sizeof(struct sep_lli_entry) *
			SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP) >
			((void *)sep->shared_addr +
			SYNCHRONIC_DMA_TABLES_AREA_OFFSET_BYTES +
			SYNCHRONIC_DMA_TABLES_AREA_SIZE_BYTES)) {

			dev_warn(&sep->pdev->dev, "dma table limit overrun\n");
			return -ENOMEM;
		}

		
		dma_ctx->num_lli_tables_created += 2;

		lli_table_alloc_addr += sizeof(struct sep_lli_entry) *
			SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP;
		dma_lli_table_alloc_addr += sizeof(struct sep_lli_entry) *
			SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP;

		
		in_table_data_size =
			sep_calculate_lli_table_max_size(sep,
			&lli_in_array[current_in_entry],
			(sep_in_lli_entries - current_in_entry),
			&last_table_flag);

		
		out_table_data_size =
			sep_calculate_lli_table_max_size(sep,
			&lli_out_array[current_out_entry],
			(sep_out_lli_entries - current_out_entry),
			&last_table_flag);

		if (!last_table_flag) {
			in_table_data_size = (in_table_data_size /
				block_size) * block_size;
			out_table_data_size = (out_table_data_size /
				block_size) * block_size;
		}

		table_data_size = in_table_data_size;
		if (table_data_size > out_table_data_size)
			table_data_size = out_table_data_size;

		dev_dbg(&sep->pdev->dev,
			"[PID%d] construct tables from lli"
			" in_table_data_size is (hex) %x\n", current->pid,
			in_table_data_size);

		dev_dbg(&sep->pdev->dev,
			"[PID%d] construct tables from lli"
			"out_table_data_size is (hex) %x\n", current->pid,
			out_table_data_size);

		
		sep_build_lli_table(sep, &lli_in_array[current_in_entry],
			in_lli_table_ptr,
			&current_in_entry,
			&num_entries_in_table,
			table_data_size);

		
		sep_build_lli_table(sep, &lli_out_array[current_out_entry],
			out_lli_table_ptr,
			&current_out_entry,
			&num_entries_out_table,
			table_data_size);

		
		if (info_in_entry_ptr == NULL) {
			
			*lli_table_in_ptr =
			sep_shared_area_virt_to_bus(sep, dma_in_lli_table_ptr);

			*in_num_entries_ptr = num_entries_in_table;

			*lli_table_out_ptr =
				sep_shared_area_virt_to_bus(sep,
				dma_out_lli_table_ptr);

			*out_num_entries_ptr = num_entries_out_table;
			*table_data_size_ptr = table_data_size;

			dev_dbg(&sep->pdev->dev,
				"[PID%d] output lli_table_in_ptr is %08lx\n",
				current->pid,
				(unsigned long)*lli_table_in_ptr);
			dev_dbg(&sep->pdev->dev,
				"[PID%d] output lli_table_out_ptr is %08lx\n",
				current->pid,
				(unsigned long)*lli_table_out_ptr);
		} else {
			
			info_in_entry_ptr->bus_address =
				sep_shared_area_virt_to_bus(sep,
				dma_in_lli_table_ptr);

			info_in_entry_ptr->block_size =
				((num_entries_in_table) << 24) |
				(table_data_size);

			
			info_out_entry_ptr->bus_address =
				sep_shared_area_virt_to_bus(sep,
				dma_out_lli_table_ptr);

			info_out_entry_ptr->block_size =
				((num_entries_out_table) << 24) |
				(table_data_size);

			dev_dbg(&sep->pdev->dev,
				"[PID%d] output lli_table_in_ptr:%08lx %08x\n",
				current->pid,
				(unsigned long)info_in_entry_ptr->bus_address,
				info_in_entry_ptr->block_size);

			dev_dbg(&sep->pdev->dev,
				"[PID%d] output lli_table_out_ptr:"
				"%08lx  %08x\n",
				current->pid,
				(unsigned long)info_out_entry_ptr->bus_address,
				info_out_entry_ptr->block_size);
		}

		
		info_in_entry_ptr = in_lli_table_ptr +
			num_entries_in_table - 1;
		info_out_entry_ptr = out_lli_table_ptr +
			num_entries_out_table - 1;

		dev_dbg(&sep->pdev->dev,
			"[PID%d] output num_entries_out_table is %x\n",
			current->pid,
			(u32)num_entries_out_table);
		dev_dbg(&sep->pdev->dev,
			"[PID%d] output info_in_entry_ptr is %lx\n",
			current->pid,
			(unsigned long)info_in_entry_ptr);
		dev_dbg(&sep->pdev->dev,
			"[PID%d] output info_out_entry_ptr is %lx\n",
			current->pid,
			(unsigned long)info_out_entry_ptr);
	}

	
	if (!dmatables_region) {
		sep_debug_print_lli_tables(
			sep,
			(struct sep_lli_entry *)
			sep_shared_area_bus_to_virt(sep, *lli_table_in_ptr),
			*in_num_entries_ptr,
			*table_data_size_ptr);
	}

	
	if (!dmatables_region) {
		sep_debug_print_lli_tables(
			sep,
			(struct sep_lli_entry *)
			sep_shared_area_bus_to_virt(sep, *lli_table_out_ptr),
			*out_num_entries_ptr,
			*table_data_size_ptr);
	}

	return 0;
}

static int sep_prepare_input_output_dma_table(struct sep_device *sep,
	unsigned long app_virt_in_addr,
	unsigned long app_virt_out_addr,
	u32 data_size,
	u32 block_size,
	dma_addr_t *lli_table_in_ptr,
	dma_addr_t *lli_table_out_ptr,
	u32 *in_num_entries_ptr,
	u32 *out_num_entries_ptr,
	u32 *table_data_size_ptr,
	bool is_kva,
	void **dmatables_region,
	struct sep_dma_context *dma_ctx)

{
	int error = 0;
	
	struct sep_lli_entry *lli_in_array;
	
	struct sep_lli_entry *lli_out_array;

	if (!dma_ctx) {
		error = -EINVAL;
		goto end_function;
	}

	if (data_size == 0) {
		
		if (dmatables_region) {
			error = sep_allocate_dmatables_region(
					sep,
					dmatables_region,
					dma_ctx,
					2);
		  if (error)
			goto end_function;
		}
		sep_prepare_empty_lli_table(sep, lli_table_in_ptr,
			in_num_entries_ptr, table_data_size_ptr,
			dmatables_region, dma_ctx);

		sep_prepare_empty_lli_table(sep, lli_table_out_ptr,
			out_num_entries_ptr, table_data_size_ptr,
			dmatables_region, dma_ctx);

		goto update_dcb_counter;
	}

	
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_page_array = NULL;
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_page_array = NULL;

	
	if (is_kva == true) {
		dev_dbg(&sep->pdev->dev, "[PID%d] Locking kernel input pages\n",
						current->pid);
		error = sep_lock_kernel_pages(sep, app_virt_in_addr,
				data_size, &lli_in_array, SEP_DRIVER_IN_FLAG,
				dma_ctx);
		if (error) {
			dev_warn(&sep->pdev->dev,
				"[PID%d] sep_lock_kernel_pages for input "
				"virtual buffer failed\n", current->pid);

			goto end_function;
		}

		dev_dbg(&sep->pdev->dev, "[PID%d] Locking kernel output pages\n",
						current->pid);
		error = sep_lock_kernel_pages(sep, app_virt_out_addr,
				data_size, &lli_out_array, SEP_DRIVER_OUT_FLAG,
				dma_ctx);

		if (error) {
			dev_warn(&sep->pdev->dev,
				"[PID%d] sep_lock_kernel_pages for output "
				"virtual buffer failed\n", current->pid);

			goto end_function_free_lli_in;
		}

	}

	else {
		dev_dbg(&sep->pdev->dev, "[PID%d] Locking user input pages\n",
						current->pid);
		error = sep_lock_user_pages(sep, app_virt_in_addr,
				data_size, &lli_in_array, SEP_DRIVER_IN_FLAG,
				dma_ctx);
		if (error) {
			dev_warn(&sep->pdev->dev,
				"[PID%d] sep_lock_user_pages for input "
				"virtual buffer failed\n", current->pid);

			goto end_function;
		}

		if (dma_ctx->secure_dma == true) {
			
			dev_dbg(&sep->pdev->dev, "[PID%d] in secure_dma\n",
				current->pid);
			error = sep_lli_table_secure_dma(sep,
				app_virt_out_addr, data_size, &lli_out_array,
				SEP_DRIVER_OUT_FLAG, dma_ctx);
			if (error) {
				dev_warn(&sep->pdev->dev,
					"[PID%d] secure dma table setup "
					" for output virtual buffer failed\n",
					current->pid);

				goto end_function_free_lli_in;
			}
		} else {
			
			dev_dbg(&sep->pdev->dev, "[PID%d] not in secure_dma\n",
				current->pid);

			dev_dbg(&sep->pdev->dev,
				"[PID%d] Locking user output pages\n",
				current->pid);

			error = sep_lock_user_pages(sep, app_virt_out_addr,
				data_size, &lli_out_array, SEP_DRIVER_OUT_FLAG,
				dma_ctx);

			if (error) {
				dev_warn(&sep->pdev->dev,
					"[PID%d] sep_lock_user_pages"
					" for output virtual buffer failed\n",
					current->pid);

				goto end_function_free_lli_in;
			}
		}
	}

	dev_dbg(&sep->pdev->dev, "[PID%d] After lock; prep input output dma "
		"table sep_in_num_pages is (hex) %x\n", current->pid,
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_num_pages);

	dev_dbg(&sep->pdev->dev, "[PID%d] sep_out_num_pages is (hex) %x\n",
		current->pid,
		dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_num_pages);

	dev_dbg(&sep->pdev->dev, "[PID%d] SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP"
		" is (hex) %x\n", current->pid,
		SEP_DRIVER_ENTRIES_PER_TABLE_IN_SEP);

	
	dev_dbg(&sep->pdev->dev, "[PID%d] calling create table from lli\n",
					current->pid);
	error = sep_construct_dma_tables_from_lli(
			sep, lli_in_array,
			dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].
								in_num_pages,
			lli_out_array,
			dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].
								out_num_pages,
			block_size, lli_table_in_ptr, lli_table_out_ptr,
			in_num_entries_ptr, out_num_entries_ptr,
			table_data_size_ptr, dmatables_region, dma_ctx);

	if (error) {
		dev_warn(&sep->pdev->dev,
			"[PID%d] sep_construct_dma_tables_from_lli failed\n",
			current->pid);
		goto end_function_with_error;
	}

	kfree(lli_out_array);
	kfree(lli_in_array);

update_dcb_counter:
	
	dma_ctx->nr_dcb_creat++;

	goto end_function;

end_function_with_error:
	kfree(dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_map_array);
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_map_array = NULL;
	kfree(dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_page_array);
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].out_page_array = NULL;
	kfree(lli_out_array);


end_function_free_lli_in:
	kfree(dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_map_array);
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_map_array = NULL;
	kfree(dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_page_array);
	dma_ctx->dma_res_arr[dma_ctx->nr_dcb_creat].in_page_array = NULL;
	kfree(lli_in_array);

end_function:

	return error;

}

int sep_prepare_input_output_dma_table_in_dcb(struct sep_device *sep,
	unsigned long  app_in_address,
	unsigned long  app_out_address,
	u32  data_in_size,
	u32  block_size,
	u32  tail_block_size,
	bool isapplet,
	bool	is_kva,
	bool	secure_dma,
	struct sep_dcblock *dcb_region,
	void **dmatables_region,
	struct sep_dma_context **dma_ctx,
	struct scatterlist *src_sg,
	struct scatterlist *dst_sg)
{
	int error = 0;
	
	u32 tail_size = 0;
	
	struct sep_dcblock *dcb_table_ptr = NULL;
	
	dma_addr_t in_first_mlli_address = 0;
	
	u32  in_first_num_entries = 0;
	
	dma_addr_t  out_first_mlli_address = 0;
	
	u32  out_first_num_entries = 0;
	
	u32  first_data_size = 0;

	dev_dbg(&sep->pdev->dev, "[PID%d] app_in_address %lx\n",
		current->pid, app_in_address);

	dev_dbg(&sep->pdev->dev, "[PID%d] app_out_address %lx\n",
		current->pid, app_out_address);

	dev_dbg(&sep->pdev->dev, "[PID%d] data_in_size %x\n",
		current->pid, data_in_size);

	dev_dbg(&sep->pdev->dev, "[PID%d] block_size %x\n",
		current->pid, block_size);

	dev_dbg(&sep->pdev->dev, "[PID%d] tail_block_size %x\n",
		current->pid, tail_block_size);

	dev_dbg(&sep->pdev->dev, "[PID%d] isapplet %x\n",
		current->pid, isapplet);

	dev_dbg(&sep->pdev->dev, "[PID%d] is_kva %x\n",
		current->pid, is_kva);

	dev_dbg(&sep->pdev->dev, "[PID%d] src_sg %p\n",
		current->pid, src_sg);

	dev_dbg(&sep->pdev->dev, "[PID%d] dst_sg %p\n",
		current->pid, dst_sg);

	if (!dma_ctx) {
		dev_warn(&sep->pdev->dev, "[PID%d] no DMA context pointer\n",
						current->pid);
		error = -EINVAL;
		goto end_function;
	}

	if (*dma_ctx) {
		
		dev_dbg(&sep->pdev->dev, "[PID%d] DMA context already set\n",
						current->pid);
	} else {
		*dma_ctx = kzalloc(sizeof(**dma_ctx), GFP_KERNEL);
		if (!(*dma_ctx)) {
			dev_dbg(&sep->pdev->dev,
				"[PID%d] Not enough memory for DMA context\n",
				current->pid);
		  error = -ENOMEM;
		  goto end_function;
		}
		dev_dbg(&sep->pdev->dev,
			"[PID%d] Created DMA context addr at 0x%p\n",
			current->pid, *dma_ctx);
	}

	(*dma_ctx)->secure_dma = secure_dma;

	
	(*dma_ctx)->src_sg = src_sg;
	(*dma_ctx)->dst_sg = dst_sg;

	if ((*dma_ctx)->nr_dcb_creat == SEP_MAX_NUM_SYNC_DMA_OPS) {
		
		dev_dbg(&sep->pdev->dev, "[PID%d] no more DCBs available\n",
						current->pid);
		error = -ENOSPC;
		goto end_function_error;
	}

	
	if (dcb_region) {
		dcb_table_ptr = dcb_region;
	} else {
		dcb_table_ptr = (struct sep_dcblock *)(sep->shared_addr +
			SEP_DRIVER_SYSTEM_DCB_MEMORY_OFFSET_IN_BYTES +
			((*dma_ctx)->nr_dcb_creat *
						sizeof(struct sep_dcblock)));
	}

	
	dcb_table_ptr->input_mlli_address = 0;
	dcb_table_ptr->input_mlli_num_entries = 0;
	dcb_table_ptr->input_mlli_data_size = 0;
	dcb_table_ptr->output_mlli_address = 0;
	dcb_table_ptr->output_mlli_num_entries = 0;
	dcb_table_ptr->output_mlli_data_size = 0;
	dcb_table_ptr->tail_data_size = 0;
	dcb_table_ptr->out_vr_tail_pt = 0;

	if (isapplet == true) {

		
		if (data_in_size < SEP_DRIVER_MIN_DATA_SIZE_PER_TABLE) {
			if (is_kva == true) {
				error = -ENODEV;
				goto end_function_error;
			} else {
				if (copy_from_user(dcb_table_ptr->tail_data,
					(void __user *)app_in_address,
					data_in_size)) {
					error = -EFAULT;
					goto end_function_error;
				}
			}

			dcb_table_ptr->tail_data_size = data_in_size;

			
			if (app_out_address)
				dcb_table_ptr->out_vr_tail_pt =
				(aligned_u64)app_out_address;

			tail_size = 0x0;
			data_in_size = 0x0;

		} else {
			if (!app_out_address) {
				tail_size = data_in_size % block_size;
				if (!tail_size) {
					if (tail_block_size == block_size)
						tail_size = block_size;
				}
			} else {
				tail_size = 0;
			}
		}
		if (tail_size) {
			if (tail_size > sizeof(dcb_table_ptr->tail_data))
				return -EINVAL;
			if (is_kva == true) {
				error = -ENODEV;
				goto end_function_error;
			} else {
				
				if (copy_from_user(dcb_table_ptr->tail_data,
					(void __user *)(app_in_address +
					data_in_size - tail_size), tail_size)) {
					error = -EFAULT;
					goto end_function_error;
				}
			}
			if (app_out_address)
				dcb_table_ptr->out_vr_tail_pt =
					(aligned_u64)app_out_address +
					data_in_size - tail_size;

			
			dcb_table_ptr->tail_data_size = tail_size;
			data_in_size = (data_in_size - tail_size);
		}
	}
	
	if (app_out_address) {
		
		error = sep_prepare_input_output_dma_table(sep,
				app_in_address,
				app_out_address,
				data_in_size,
				block_size,
				&in_first_mlli_address,
				&out_first_mlli_address,
				&in_first_num_entries,
				&out_first_num_entries,
				&first_data_size,
				is_kva,
				dmatables_region,
				*dma_ctx);
	} else {
		
		error = sep_prepare_input_dma_table(sep,
				app_in_address,
				data_in_size,
				block_size,
				&in_first_mlli_address,
				&in_first_num_entries,
				&first_data_size,
				is_kva,
				dmatables_region,
				*dma_ctx);
	}

	if (error) {
		dev_warn(&sep->pdev->dev,
			"prepare DMA table call failed "
			"from prepare DCB call\n");
		goto end_function_error;
	}

	
	dcb_table_ptr->input_mlli_address = in_first_mlli_address;
	dcb_table_ptr->input_mlli_num_entries = in_first_num_entries;
	dcb_table_ptr->input_mlli_data_size = first_data_size;
	dcb_table_ptr->output_mlli_address = out_first_mlli_address;
	dcb_table_ptr->output_mlli_num_entries = out_first_num_entries;
	dcb_table_ptr->output_mlli_data_size = first_data_size;

	goto end_function;

end_function_error:
	kfree(*dma_ctx);
	*dma_ctx = NULL;

end_function:
	return error;

}


static int sep_free_dma_tables_and_dcb(struct sep_device *sep, bool isapplet,
	bool is_kva, struct sep_dma_context **dma_ctx)
{
	struct sep_dcblock *dcb_table_ptr;
	unsigned long pt_hold;
	void *tail_pt;

	int i = 0;
	int error = 0;
	int error_temp = 0;

	dev_dbg(&sep->pdev->dev, "[PID%d] sep_free_dma_tables_and_dcb\n",
					current->pid);

	if (((*dma_ctx)->secure_dma == false) && (isapplet == true)) {
		dev_dbg(&sep->pdev->dev, "[PID%d] handling applet\n",
			current->pid);

		
		
		dcb_table_ptr = (struct sep_dcblock *)
			(sep->shared_addr +
			SEP_DRIVER_SYSTEM_DCB_MEMORY_OFFSET_IN_BYTES);

		for (i = 0; dma_ctx && *dma_ctx &&
			i < (*dma_ctx)->nr_dcb_creat; i++, dcb_table_ptr++) {
			if (dcb_table_ptr->out_vr_tail_pt) {
				pt_hold = (unsigned long)dcb_table_ptr->
					out_vr_tail_pt;
				tail_pt = (void *)pt_hold;
				if (is_kva == true) {
					error = -ENODEV;
					break;
				} else {
					error_temp = copy_to_user(
						(void __user *)tail_pt,
						dcb_table_ptr->tail_data,
						dcb_table_ptr->tail_data_size);
				}
				if (error_temp) {
					
					error = -EFAULT;
					break;
				}
			}
		}
	}

	
	sep_free_dma_table_data_handler(sep, dma_ctx);

	dev_dbg(&sep->pdev->dev, "[PID%d] sep_free_dma_tables_and_dcb end\n",
					current->pid);

	return error;
}

static int sep_prepare_dcb_handler(struct sep_device *sep, unsigned long arg,
				   bool secure_dma,
				   struct sep_dma_context **dma_ctx)
{
	int error;
	
	static struct build_dcb_struct command_args;

	
	if (copy_from_user(&command_args, (void __user *)arg,
					sizeof(struct build_dcb_struct))) {
		error = -EFAULT;
		goto end_function;
	}

	dev_dbg(&sep->pdev->dev,
		"[PID%d] prep dcb handler app_in_address is %08llx\n",
			current->pid, command_args.app_in_address);
	dev_dbg(&sep->pdev->dev,
		"[PID%d] app_out_address is %08llx\n",
			current->pid, command_args.app_out_address);
	dev_dbg(&sep->pdev->dev,
		"[PID%d] data_size is %x\n",
			current->pid, command_args.data_in_size);
	dev_dbg(&sep->pdev->dev,
		"[PID%d] block_size is %x\n",
			current->pid, command_args.block_size);
	dev_dbg(&sep->pdev->dev,
		"[PID%d] tail block_size is %x\n",
			current->pid, command_args.tail_block_size);
	dev_dbg(&sep->pdev->dev,
		"[PID%d] is_applet is %x\n",
			current->pid, command_args.is_applet);

	if (!command_args.app_in_address) {
		dev_warn(&sep->pdev->dev,
			"[PID%d] null app_in_address\n", current->pid);
		error = -EINVAL;
		goto end_function;
	}

	error = sep_prepare_input_output_dma_table_in_dcb(sep,
			(unsigned long)command_args.app_in_address,
			(unsigned long)command_args.app_out_address,
			command_args.data_in_size, command_args.block_size,
			command_args.tail_block_size,
			command_args.is_applet, false,
			secure_dma, NULL, NULL, dma_ctx, NULL, NULL);

end_function:
	return error;

}

static int sep_free_dcb_handler(struct sep_device *sep,
				struct sep_dma_context **dma_ctx)
{
	if (!dma_ctx || !(*dma_ctx)) {
		dev_dbg(&sep->pdev->dev,
			"[PID%d] no dma context defined, nothing to free\n",
			current->pid);
		return -EINVAL;
	}

	dev_dbg(&sep->pdev->dev, "[PID%d] free dcbs num of DCBs %x\n",
		current->pid,
		(*dma_ctx)->nr_dcb_creat);

	return sep_free_dma_tables_and_dcb(sep, false, false, dma_ctx);
}

static long sep_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct sep_private_data * const private_data = filp->private_data;
	struct sep_call_status *call_status = &private_data->call_status;
	struct sep_device *sep = private_data->device;
	struct sep_dma_context **dma_ctx = &private_data->dma_ctx;
	struct sep_queue_info **my_queue_elem = &private_data->my_queue_elem;
	int error = 0;

	dev_dbg(&sep->pdev->dev, "[PID%d] ioctl cmd 0x%x\n",
		current->pid, cmd);
	dev_dbg(&sep->pdev->dev, "[PID%d] dma context addr 0x%p\n",
		current->pid, *dma_ctx);

	
	error = sep_check_transaction_owner(sep);
	if (error) {
		dev_dbg(&sep->pdev->dev, "[PID%d] ioctl pid is not owner\n",
			current->pid);
		goto end_function;
	}

	
	if (0 == test_bit(SEP_LEGACY_MMAP_DONE_OFFSET,
				&call_status->status)) {
		dev_dbg(&sep->pdev->dev,
			"[PID%d] mmap not called\n", current->pid);
		error = -EPROTO;
		goto end_function;
	}

	
	if (_IOC_TYPE(cmd) != SEP_IOC_MAGIC_NUMBER) {
		error = -ENOTTY;
		goto end_function;
	}

	switch (cmd) {
	case SEP_IOCSENDSEPCOMMAND:
		dev_dbg(&sep->pdev->dev,
			"[PID%d] SEP_IOCSENDSEPCOMMAND start\n",
			current->pid);
		if (1 == test_bit(SEP_LEGACY_SENDMSG_DONE_OFFSET,
				  &call_status->status)) {
			dev_warn(&sep->pdev->dev,
				"[PID%d] send msg already done\n",
				current->pid);
			error = -EPROTO;
			goto end_function;
		}
		
		error = sep_send_command_handler(sep);
		if (!error)
			set_bit(SEP_LEGACY_SENDMSG_DONE_OFFSET,
				&call_status->status);
		dev_dbg(&sep->pdev->dev,
			"[PID%d] SEP_IOCSENDSEPCOMMAND end\n",
			current->pid);
		break;
	case SEP_IOCENDTRANSACTION:
		dev_dbg(&sep->pdev->dev,
			"[PID%d] SEP_IOCENDTRANSACTION start\n",
			current->pid);
		error = sep_end_transaction_handler(sep, dma_ctx, call_status,
						    my_queue_elem);
		dev_dbg(&sep->pdev->dev,
			"[PID%d] SEP_IOCENDTRANSACTION end\n",
			current->pid);
		break;
	case SEP_IOCPREPAREDCB:
		dev_dbg(&sep->pdev->dev,
			"[PID%d] SEP_IOCPREPAREDCB start\n",
			current->pid);
	case SEP_IOCPREPAREDCB_SECURE_DMA:
		dev_dbg(&sep->pdev->dev,
			"[PID%d] SEP_IOCPREPAREDCB_SECURE_DMA start\n",
			current->pid);
		if (1 == test_bit(SEP_LEGACY_SENDMSG_DONE_OFFSET,
				  &call_status->status)) {
			dev_dbg(&sep->pdev->dev,
				"[PID%d] dcb prep needed before send msg\n",
				current->pid);
			error = -EPROTO;
			goto end_function;
		}

		if (!arg) {
			dev_dbg(&sep->pdev->dev,
				"[PID%d] dcb null arg\n", current->pid);
			error = -EINVAL;
			goto end_function;
		}

		if (cmd == SEP_IOCPREPAREDCB) {
			
			dev_dbg(&sep->pdev->dev,
				"[PID%d] SEP_IOCPREPAREDCB (no secure_dma)\n",
				current->pid);

			error = sep_prepare_dcb_handler(sep, arg, false,
				dma_ctx);
		} else {
			
			dev_dbg(&sep->pdev->dev,
				"[PID%d] SEP_IOC_POC (with secure_dma)\n",
				current->pid);

			error = sep_prepare_dcb_handler(sep, arg, true,
				dma_ctx);
		}
		dev_dbg(&sep->pdev->dev, "[PID%d] dcb's end\n",
			current->pid);
		break;
	case SEP_IOCFREEDCB:
		dev_dbg(&sep->pdev->dev, "[PID%d] SEP_IOCFREEDCB start\n",
			current->pid);
	case SEP_IOCFREEDCB_SECURE_DMA:
		dev_dbg(&sep->pdev->dev,
			"[PID%d] SEP_IOCFREEDCB_SECURE_DMA start\n",
			current->pid);
		error = sep_free_dcb_handler(sep, dma_ctx);
		dev_dbg(&sep->pdev->dev, "[PID%d] SEP_IOCFREEDCB end\n",
			current->pid);
		break;
	default:
		error = -ENOTTY;
		dev_dbg(&sep->pdev->dev, "[PID%d] default end\n",
			current->pid);
		break;
	}

end_function:
	dev_dbg(&sep->pdev->dev, "[PID%d] ioctl end\n", current->pid);

	return error;
}

static irqreturn_t sep_inthandler(int irq, void *dev_id)
{
	unsigned long lock_irq_flag;
	u32 reg_val, reg_val2 = 0;
	struct sep_device *sep = dev_id;
	irqreturn_t int_error = IRQ_HANDLED;

	
#if defined(CONFIG_PM_RUNTIME) && defined(SEP_ENABLE_RUNTIME_PM)
	if (sep->pdev->dev.power.runtime_status != RPM_ACTIVE) {
		dev_dbg(&sep->pdev->dev, "interrupt during pwr save\n");
		return IRQ_NONE;
	}
#endif

	if (test_bit(SEP_WORKING_LOCK_BIT, &sep->in_use_flags) == 0) {
		dev_dbg(&sep->pdev->dev, "interrupt while nobody using sep\n");
		return IRQ_NONE;
	}

	
	reg_val = sep_read_reg(sep, HW_HOST_IRR_REG_ADDR);

	dev_dbg(&sep->pdev->dev, "sep int: IRR REG val: %x\n", reg_val);

	if (reg_val & (0x1 << 13)) {

		
		spin_lock_irqsave(&sep->snd_rply_lck, lock_irq_flag);
		sep->reply_ct++;
		spin_unlock_irqrestore(&sep->snd_rply_lck, lock_irq_flag);

		dev_dbg(&sep->pdev->dev, "sep int: send_ct %lx reply_ct %lx\n",
					sep->send_ct, sep->reply_ct);

		
		if (sep->in_kernel) {
			tasklet_schedule(&sep->finish_tasklet);
			goto finished_interrupt;
		}

		
		reg_val2 = sep_read_reg(sep, HW_HOST_SEP_HOST_GPR2_REG_ADDR);
		dev_dbg(&sep->pdev->dev,
			"SEP Interrupt - GPR2 is %08x\n", reg_val2);

		clear_bit(SEP_WORKING_LOCK_BIT, &sep->in_use_flags);

		if ((reg_val2 >> 30) & 0x1) {
			dev_dbg(&sep->pdev->dev, "int: printf request\n");
		} else if (reg_val2 >> 31) {
			dev_dbg(&sep->pdev->dev, "int: daemon request\n");
		} else {
			dev_dbg(&sep->pdev->dev, "int: SEP reply\n");
			wake_up(&sep->event_interrupt);
		}
	} else {
		dev_dbg(&sep->pdev->dev, "int: not SEP interrupt\n");
		int_error = IRQ_NONE;
	}

finished_interrupt:

	if (int_error == IRQ_HANDLED)
		sep_write_reg(sep, HW_HOST_ICR_REG_ADDR, reg_val);

	return int_error;
}

static int sep_reconfig_shared_area(struct sep_device *sep)
{
	int ret_val;

	
	unsigned long end_time;

	
	dev_dbg(&sep->pdev->dev, "reconfig shared; sending %08llx to sep\n",
				(unsigned long long)sep->shared_bus);

	sep_write_reg(sep, HW_HOST_HOST_SEP_GPR1_REG_ADDR, sep->shared_bus);

	
	ret_val = sep_read_reg(sep, HW_HOST_SEP_HOST_GPR1_REG_ADDR);

	end_time = jiffies + (WAIT_TIME * HZ);

	while ((time_before(jiffies, end_time)) && (ret_val != 0xffffffff) &&
		(ret_val != sep->shared_bus))
		ret_val = sep_read_reg(sep, HW_HOST_SEP_HOST_GPR1_REG_ADDR);

	
	if (ret_val != sep->shared_bus) {
		dev_warn(&sep->pdev->dev, "could not reconfig shared area\n");
		dev_warn(&sep->pdev->dev, "result was %x\n", ret_val);
		ret_val = -ENOMEM;
	} else
		ret_val = 0;

	dev_dbg(&sep->pdev->dev, "reconfig shared area end\n");

	return ret_val;
}

ssize_t sep_activate_dcb_dmatables_context(struct sep_device *sep,
					struct sep_dcblock **dcb_region,
					void **dmatables_region,
					struct sep_dma_context *dma_ctx)
{
	void *dmaregion_free_start = NULL;
	void *dmaregion_free_end = NULL;
	void *dcbregion_free_start = NULL;
	void *dcbregion_free_end = NULL;
	ssize_t error = 0;

	dev_dbg(&sep->pdev->dev, "[PID%d] activating dcb/dma region\n",
		current->pid);

	if (1 > dma_ctx->nr_dcb_creat) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] invalid number of dcbs to activate 0x%08X\n",
			 current->pid, dma_ctx->nr_dcb_creat);
		error = -EINVAL;
		goto end_function;
	}

	dmaregion_free_start = sep->shared_addr
				+ SYNCHRONIC_DMA_TABLES_AREA_OFFSET_BYTES;
	dmaregion_free_end = dmaregion_free_start
				+ SYNCHRONIC_DMA_TABLES_AREA_SIZE_BYTES - 1;

	if (dmaregion_free_start
	     + dma_ctx->dmatables_len > dmaregion_free_end) {
		error = -ENOMEM;
		goto end_function;
	}
	memcpy(dmaregion_free_start,
	       *dmatables_region,
	       dma_ctx->dmatables_len);
	
	kfree(*dmatables_region);
	*dmatables_region = NULL;

	
	dcbregion_free_start = sep->shared_addr +
				SEP_DRIVER_SYSTEM_DCB_MEMORY_OFFSET_IN_BYTES;
	dcbregion_free_end = dcbregion_free_start +
				(SEP_MAX_NUM_SYNC_DMA_OPS *
					sizeof(struct sep_dcblock)) - 1;

	if (dcbregion_free_start
	     + (dma_ctx->nr_dcb_creat * sizeof(struct sep_dcblock))
	     > dcbregion_free_end) {
		error = -ENOMEM;
		goto end_function;
	}

	memcpy(dcbregion_free_start,
	       *dcb_region,
	       dma_ctx->nr_dcb_creat * sizeof(struct sep_dcblock));

	
	dev_dbg(&sep->pdev->dev, "activate: input table\n");
	sep_debug_print_lli_tables(sep,
		(struct sep_lli_entry *)sep_shared_area_bus_to_virt(sep,
		(*dcb_region)->input_mlli_address),
		(*dcb_region)->input_mlli_num_entries,
		(*dcb_region)->input_mlli_data_size);

	dev_dbg(&sep->pdev->dev, "activate: output table\n");
	sep_debug_print_lli_tables(sep,
		(struct sep_lli_entry *)sep_shared_area_bus_to_virt(sep,
		(*dcb_region)->output_mlli_address),
		(*dcb_region)->output_mlli_num_entries,
		(*dcb_region)->output_mlli_data_size);

	dev_dbg(&sep->pdev->dev,
		 "[PID%d] printing activated tables\n", current->pid);

end_function:
	kfree(*dmatables_region);
	*dmatables_region = NULL;

	kfree(*dcb_region);
	*dcb_region = NULL;

	return error;
}

static ssize_t sep_create_dcb_dmatables_context(struct sep_device *sep,
			struct sep_dcblock **dcb_region,
			void **dmatables_region,
			struct sep_dma_context **dma_ctx,
			const struct build_dcb_struct __user *user_dcb_args,
			const u32 num_dcbs, bool secure_dma)
{
	int error = 0;
	int i = 0;
	struct build_dcb_struct *dcb_args = NULL;

	dev_dbg(&sep->pdev->dev, "[PID%d] creating dcb/dma region\n",
		current->pid);

	if (!dcb_region || !dma_ctx || !dmatables_region || !user_dcb_args) {
		error = -EINVAL;
		goto end_function;
	}

	if (SEP_MAX_NUM_SYNC_DMA_OPS < num_dcbs) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] invalid number of dcbs 0x%08X\n",
			 current->pid, num_dcbs);
		error = -EINVAL;
		goto end_function;
	}

	dcb_args = kzalloc(num_dcbs * sizeof(struct build_dcb_struct),
			   GFP_KERNEL);
	if (!dcb_args) {
		dev_warn(&sep->pdev->dev, "[PID%d] no memory for dcb args\n",
			 current->pid);
		error = -ENOMEM;
		goto end_function;
	}

	if (copy_from_user(dcb_args,
			user_dcb_args,
			num_dcbs * sizeof(struct build_dcb_struct))) {
		error = -EINVAL;
		goto end_function;
	}

	
	*dcb_region = kzalloc(num_dcbs * sizeof(struct sep_dcblock),
			      GFP_KERNEL);
	if (!(*dcb_region)) {
		error = -ENOMEM;
		goto end_function;
	}

	
	for (i = 0; i < num_dcbs; i++) {
		error = sep_prepare_input_output_dma_table_in_dcb(sep,
				(unsigned long)dcb_args[i].app_in_address,
				(unsigned long)dcb_args[i].app_out_address,
				dcb_args[i].data_in_size,
				dcb_args[i].block_size,
				dcb_args[i].tail_block_size,
				dcb_args[i].is_applet,
				false, secure_dma,
				*dcb_region, dmatables_region,
				dma_ctx,
				NULL,
				NULL);
		if (error) {
			dev_warn(&sep->pdev->dev,
				 "[PID%d] dma table creation failed\n",
				 current->pid);
			goto end_function;
		}

		if (dcb_args[i].app_in_address != 0)
			(*dma_ctx)->input_data_len += dcb_args[i].data_in_size;
	}

end_function:
	kfree(dcb_args);
	return error;

}

int sep_create_dcb_dmatables_context_kernel(struct sep_device *sep,
			struct sep_dcblock **dcb_region,
			void **dmatables_region,
			struct sep_dma_context **dma_ctx,
			const struct build_dcb_struct_kernel *dcb_data,
			const u32 num_dcbs)
{
	int error = 0;
	int i = 0;

	dev_dbg(&sep->pdev->dev, "[PID%d] creating dcb/dma region\n",
		current->pid);

	if (!dcb_region || !dma_ctx || !dmatables_region || !dcb_data) {
		error = -EINVAL;
		goto end_function;
	}

	if (SEP_MAX_NUM_SYNC_DMA_OPS < num_dcbs) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] invalid number of dcbs 0x%08X\n",
			 current->pid, num_dcbs);
		error = -EINVAL;
		goto end_function;
	}

	dev_dbg(&sep->pdev->dev, "[PID%d] num_dcbs is %d\n",
		current->pid, num_dcbs);

	
	*dcb_region = kzalloc(num_dcbs * sizeof(struct sep_dcblock),
			      GFP_KERNEL);
	if (!(*dcb_region)) {
		error = -ENOMEM;
		goto end_function;
	}

	
	for (i = 0; i < num_dcbs; i++) {
		error = sep_prepare_input_output_dma_table_in_dcb(sep,
				(unsigned long)dcb_data->app_in_address,
				(unsigned long)dcb_data->app_out_address,
				dcb_data->data_in_size,
				dcb_data->block_size,
				dcb_data->tail_block_size,
				dcb_data->is_applet,
				true,
				false,
				*dcb_region, dmatables_region,
				dma_ctx,
				dcb_data->src_sg,
				dcb_data->dst_sg);
		if (error) {
			dev_warn(&sep->pdev->dev,
				 "[PID%d] dma table creation failed\n",
				 current->pid);
			goto end_function;
		}
	}

end_function:
	return error;

}

static ssize_t sep_activate_msgarea_context(struct sep_device *sep,
					    void **msg_region,
					    const size_t msg_len)
{
	dev_dbg(&sep->pdev->dev, "[PID%d] activating msg region\n",
		current->pid);

	if (!msg_region || !(*msg_region) ||
	    SEP_DRIVER_MESSAGE_SHARED_AREA_SIZE_IN_BYTES < msg_len) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] invalid act msgarea len 0x%08zX\n",
			 current->pid, msg_len);
		return -EINVAL;
	}

	memcpy(sep->shared_addr, *msg_region, msg_len);

	return 0;
}

static ssize_t sep_create_msgarea_context(struct sep_device *sep,
					  void **msg_region,
					  const void __user *msg_user,
					  const size_t msg_len)
{
	int error = 0;

	dev_dbg(&sep->pdev->dev, "[PID%d] creating msg region\n",
		current->pid);

	if (!msg_region ||
	    !msg_user ||
	    SEP_DRIVER_MAX_MESSAGE_SIZE_IN_BYTES < msg_len ||
	    SEP_DRIVER_MIN_MESSAGE_SIZE_IN_BYTES > msg_len) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] invalid creat msgarea len 0x%08zX\n",
			 current->pid, msg_len);
		error = -EINVAL;
		goto end_function;
	}

	
	*msg_region = kzalloc(msg_len, GFP_KERNEL);
	if (!(*msg_region)) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] no mem for msgarea context\n",
			 current->pid);
		error = -ENOMEM;
		goto end_function;
	}

	
	if (copy_from_user(*msg_region, msg_user, msg_len)) {
		error = -EINVAL;
		goto end_function;
	}

end_function:
	if (error && msg_region) {
		kfree(*msg_region);
		*msg_region = NULL;
	}

	return error;
}


static ssize_t sep_read(struct file *filp,
			char __user *buf_user, size_t count_user,
			loff_t *offset)
{
	struct sep_private_data * const private_data = filp->private_data;
	struct sep_call_status *call_status = &private_data->call_status;
	struct sep_device *sep = private_data->device;
	struct sep_dma_context **dma_ctx = &private_data->dma_ctx;
	struct sep_queue_info **my_queue_elem = &private_data->my_queue_elem;
	ssize_t error = 0, error_tmp = 0;

	
	error = sep_check_transaction_owner(sep);
	if (error) {
		dev_dbg(&sep->pdev->dev, "[PID%d] read pid is not owner\n",
			current->pid);
		goto end_function;
	}

	
	if (0 == test_bit(SEP_FASTCALL_WRITE_DONE_OFFSET,
			&call_status->status)) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] fastcall write not called\n",
			 current->pid);
		error = -EPROTO;
		goto end_function_error;
	}

	if (!buf_user) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] null user buffer\n",
			 current->pid);
		error = -EINVAL;
		goto end_function_error;
	}


	
	wait_event(sep->event_interrupt,
		   test_bit(SEP_WORKING_LOCK_BIT,
			    &sep->in_use_flags) == 0);

	sep_dump_message(sep);

	dev_dbg(&sep->pdev->dev, "[PID%d] count_user = 0x%08zX\n",
		current->pid, count_user);

	
	if (count_user > SEP_DRIVER_MESSAGE_SHARED_AREA_SIZE_IN_BYTES)
		count_user = SEP_DRIVER_MESSAGE_SHARED_AREA_SIZE_IN_BYTES;

	if (copy_to_user(buf_user, sep->shared_addr, count_user)) {
		error = -EFAULT;
		goto end_function_error;
	}

	dev_dbg(&sep->pdev->dev, "[PID%d] read succeeded\n", current->pid);
	error = count_user;

end_function_error:
	
	error_tmp = sep_free_dcb_handler(sep, dma_ctx);
	if (error_tmp)
		dev_warn(&sep->pdev->dev, "[PID%d] dcb free failed\n",
			current->pid);

	
	error_tmp = sep_end_transaction_handler(sep, dma_ctx, call_status,
		my_queue_elem);
	if (error_tmp)
		dev_warn(&sep->pdev->dev,
			 "[PID%d] ending transaction failed\n",
			 current->pid);

end_function:
	return error;
}

static inline ssize_t sep_fastcall_args_get(struct sep_device *sep,
					    struct sep_fastcall_hdr *args,
					    const char __user *buf_user,
					    const size_t count_user)
{
	ssize_t error = 0;
	size_t actual_count = 0;

	if (!buf_user) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] null user buffer\n",
			 current->pid);
		error = -EINVAL;
		goto end_function;
	}

	if (count_user < sizeof(struct sep_fastcall_hdr)) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] too small message size 0x%08zX\n",
			 current->pid, count_user);
		error = -EINVAL;
		goto end_function;
	}


	if (copy_from_user(args, buf_user, sizeof(struct sep_fastcall_hdr))) {
		error = -EFAULT;
		goto end_function;
	}

	if (SEP_FC_MAGIC != args->magic) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] invalid fastcall magic 0x%08X\n",
			 current->pid, args->magic);
		error = -EINVAL;
		goto end_function;
	}

	dev_dbg(&sep->pdev->dev, "[PID%d] fastcall hdr num of DCBs 0x%08X\n",
		current->pid, args->num_dcbs);
	dev_dbg(&sep->pdev->dev, "[PID%d] fastcall hdr msg len 0x%08X\n",
		current->pid, args->msg_len);

	if (SEP_DRIVER_MAX_MESSAGE_SIZE_IN_BYTES < args->msg_len ||
	    SEP_DRIVER_MIN_MESSAGE_SIZE_IN_BYTES > args->msg_len) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] invalid message length\n",
			 current->pid);
		error = -EINVAL;
		goto end_function;
	}

	actual_count = sizeof(struct sep_fastcall_hdr)
			+ args->msg_len
			+ (args->num_dcbs * sizeof(struct build_dcb_struct));

	if (actual_count != count_user) {
		dev_warn(&sep->pdev->dev,
			 "[PID%d] inconsistent message "
			 "sizes 0x%08zX vs 0x%08zX\n",
			 current->pid, actual_count, count_user);
		error = -EMSGSIZE;
		goto end_function;
	}

end_function:
	return error;
}

static ssize_t sep_write(struct file *filp,
			 const char __user *buf_user, size_t count_user,
			 loff_t *offset)
{
	struct sep_private_data * const private_data = filp->private_data;
	struct sep_call_status *call_status = &private_data->call_status;
	struct sep_device *sep = private_data->device;
	struct sep_dma_context *dma_ctx = NULL;
	struct sep_fastcall_hdr call_hdr = {0};
	void *msg_region = NULL;
	void *dmatables_region = NULL;
	struct sep_dcblock *dcb_region = NULL;
	ssize_t error = 0;
	struct sep_queue_info *my_queue_elem = NULL;
	bool my_secure_dma; 

	dev_dbg(&sep->pdev->dev, "[PID%d] sep dev is 0x%p\n",
		current->pid, sep);
	dev_dbg(&sep->pdev->dev, "[PID%d] private_data is 0x%p\n",
		current->pid, private_data);

	error = sep_fastcall_args_get(sep, &call_hdr, buf_user, count_user);
	if (error)
		goto end_function;

	buf_user += sizeof(struct sep_fastcall_hdr);

	if (call_hdr.secure_dma == 0)
		my_secure_dma = false;
	else
		my_secure_dma = true;

	dev_dbg(&sep->pdev->dev, "[PID%d] waiting for double buffering "
				 "region access\n", current->pid);
	error = down_interruptible(&sep->sep_doublebuf);
	dev_dbg(&sep->pdev->dev, "[PID%d] double buffering region start\n",
					current->pid);
	if (error) {
		
		goto end_function_error;
	}


	if (0 < call_hdr.num_dcbs) {
		error = sep_create_dcb_dmatables_context(sep,
				&dcb_region,
				&dmatables_region,
				&dma_ctx,
				(const struct build_dcb_struct __user *)
					buf_user,
				call_hdr.num_dcbs, my_secure_dma);
		if (error)
			goto end_function_error_doublebuf;

		buf_user += call_hdr.num_dcbs * sizeof(struct build_dcb_struct);
	}

	error = sep_create_msgarea_context(sep,
					   &msg_region,
					   buf_user,
					   call_hdr.msg_len);
	if (error)
		goto end_function_error_doublebuf;

	dev_dbg(&sep->pdev->dev, "[PID%d] updating queue status\n",
							current->pid);
	my_queue_elem = sep_queue_status_add(sep,
				((struct sep_msgarea_hdr *)msg_region)->opcode,
				(dma_ctx) ? dma_ctx->input_data_len : 0,
				     current->pid,
				     current->comm, sizeof(current->comm));

	if (!my_queue_elem) {
		dev_dbg(&sep->pdev->dev, "[PID%d] updating queue"
					"status error\n", current->pid);
		error = -ENOMEM;
		goto end_function_error_doublebuf;
	}

	
	error = sep_wait_transaction(sep);

	if (error) {
		
		dev_dbg(&sep->pdev->dev, "[PID%d] interrupted by signal\n",
			current->pid);
		sep_queue_status_remove(sep, &my_queue_elem);
		goto end_function_error_doublebuf;
	}

	dev_dbg(&sep->pdev->dev, "[PID%d] saving queue element\n",
		current->pid);
	private_data->my_queue_elem = my_queue_elem;

	
	error = sep_activate_msgarea_context(sep, &msg_region,
					     call_hdr.msg_len);
	if (error)
		goto end_function_error_clear_transact;

	sep_dump_message(sep);

	if (0 < call_hdr.num_dcbs) {
		error = sep_activate_dcb_dmatables_context(sep,
				&dcb_region,
				&dmatables_region,
				dma_ctx);
		if (error)
			goto end_function_error_clear_transact;
	}

	
	error = sep_send_command_handler(sep);
	if (error)
		goto end_function_error_clear_transact;

	
	private_data->dma_ctx = dma_ctx;
	
	set_bit(SEP_FASTCALL_WRITE_DONE_OFFSET, &call_status->status);
	error = count_user;

	up(&sep->sep_doublebuf);
	dev_dbg(&sep->pdev->dev, "[PID%d] double buffering region end\n",
		current->pid);

	goto end_function;

end_function_error_clear_transact:
	sep_end_transaction_handler(sep, &dma_ctx, call_status,
						&private_data->my_queue_elem);

end_function_error_doublebuf:
	up(&sep->sep_doublebuf);
	dev_dbg(&sep->pdev->dev, "[PID%d] double buffering region end\n",
		current->pid);

end_function_error:
	if (dma_ctx)
		sep_free_dma_table_data_handler(sep, &dma_ctx);

end_function:
	kfree(dcb_region);
	kfree(dmatables_region);
	kfree(msg_region);

	return error;
}
static loff_t sep_seek(struct file *filp, loff_t offset, int origin)
{
	return -ENOSYS;
}



static const struct file_operations sep_file_operations = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = sep_ioctl,
	.poll = sep_poll,
	.open = sep_open,
	.release = sep_release,
	.mmap = sep_mmap,
	.read = sep_read,
	.write = sep_write,
	.llseek = sep_seek,
};

static ssize_t
sep_sysfs_read(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr,
		char *buf, loff_t pos, size_t count)
{
	unsigned long lck_flags;
	size_t nleft = count;
	struct sep_device *sep = sep_dev;
	struct sep_queue_info *queue_elem = NULL;
	u32 queue_num = 0;
	u32 i = 1;

	spin_lock_irqsave(&sep->sep_queue_lock, lck_flags);

	queue_num = sep->sep_queue_num;
	if (queue_num > SEP_DOUBLEBUF_USERS_LIMIT)
		queue_num = SEP_DOUBLEBUF_USERS_LIMIT;


	if (count < sizeof(queue_num)
			+ (queue_num * sizeof(struct sep_queue_data))) {
		spin_unlock_irqrestore(&sep->sep_queue_lock, lck_flags);
		return -EINVAL;
	}

	memcpy(buf, &queue_num, sizeof(queue_num));
	buf += sizeof(queue_num);
	nleft -= sizeof(queue_num);

	list_for_each_entry(queue_elem, &sep->sep_queue_status, list) {
		if (i++ > queue_num)
			break;

		memcpy(buf, &queue_elem->data, sizeof(queue_elem->data));
		nleft -= sizeof(queue_elem->data);
		buf += sizeof(queue_elem->data);
	}
	spin_unlock_irqrestore(&sep->sep_queue_lock, lck_flags);

	return count - nleft;
}

static const struct bin_attribute queue_status = {
	.attr = {.name = "queue_status", .mode = 0444},
	.read = sep_sysfs_read,
	.size = sizeof(u32)
		+ (SEP_DOUBLEBUF_USERS_LIMIT * sizeof(struct sep_queue_data)),
};

static int sep_register_driver_with_fs(struct sep_device *sep)
{
	int ret_val;

	sep->miscdev_sep.minor = MISC_DYNAMIC_MINOR;
	sep->miscdev_sep.name = SEP_DEV_NAME;
	sep->miscdev_sep.fops = &sep_file_operations;

	ret_val = misc_register(&sep->miscdev_sep);
	if (ret_val) {
		dev_warn(&sep->pdev->dev, "misc reg fails for SEP %x\n",
			ret_val);
		return ret_val;
	}

	ret_val = device_create_bin_file(sep->miscdev_sep.this_device,
								&queue_status);
	if (ret_val) {
		dev_warn(&sep->pdev->dev, "sysfs attribute1 fails for SEP %x\n",
			ret_val);
		return ret_val;
	}

	return ret_val;
}


static int __devinit sep_probe(struct pci_dev *pdev,
	const struct pci_device_id *ent)
{
	int error = 0;
	struct sep_device *sep = NULL;

	if (sep_dev != NULL) {
		dev_dbg(&pdev->dev, "only one SEP supported.\n");
		return -EBUSY;
	}

	
	error = pci_enable_device(pdev);
	if (error) {
		dev_warn(&pdev->dev, "error enabling pci device\n");
		goto end_function;
	}

	
	sep_dev = kzalloc(sizeof(struct sep_device), GFP_ATOMIC);
	if (sep_dev == NULL) {
		dev_warn(&pdev->dev,
			"can't kmalloc the sep_device structure\n");
		error = -ENOMEM;
		goto end_function_disable_device;
	}

	sep = sep_dev;

	sep->pdev = pci_dev_get(pdev);

	init_waitqueue_head(&sep->event_transactions);
	init_waitqueue_head(&sep->event_interrupt);
	spin_lock_init(&sep->snd_rply_lck);
	spin_lock_init(&sep->sep_queue_lock);
	sema_init(&sep->sep_doublebuf, SEP_DOUBLEBUF_USERS_LIMIT);

	INIT_LIST_HEAD(&sep->sep_queue_status);

	dev_dbg(&sep->pdev->dev, "sep probe: PCI obtained, "
		"device being prepared\n");

	
	sep->reg_physical_addr = pci_resource_start(sep->pdev, 0);
	if (!sep->reg_physical_addr) {
		dev_warn(&sep->pdev->dev, "Error getting register start\n");
		error = -ENODEV;
		goto end_function_free_sep_dev;
	}

	sep->reg_physical_end = pci_resource_end(sep->pdev, 0);
	if (!sep->reg_physical_end) {
		dev_warn(&sep->pdev->dev, "Error getting register end\n");
		error = -ENODEV;
		goto end_function_free_sep_dev;
	}

	sep->reg_addr = ioremap_nocache(sep->reg_physical_addr,
		(size_t)(sep->reg_physical_end - sep->reg_physical_addr + 1));
	if (!sep->reg_addr) {
		dev_warn(&sep->pdev->dev, "Error getting register virtual\n");
		error = -ENODEV;
		goto end_function_free_sep_dev;
	}

	dev_dbg(&sep->pdev->dev,
		"Register area start %llx end %llx virtual %p\n",
		(unsigned long long)sep->reg_physical_addr,
		(unsigned long long)sep->reg_physical_end,
		sep->reg_addr);

	
	sep->shared_size = SEP_DRIVER_MESSAGE_SHARED_AREA_SIZE_IN_BYTES +
		SYNCHRONIC_DMA_TABLES_AREA_SIZE_BYTES +
		SEP_DRIVER_DATA_POOL_SHARED_AREA_SIZE_IN_BYTES +
		SEP_DRIVER_STATIC_AREA_SIZE_IN_BYTES +
		SEP_DRIVER_SYSTEM_DATA_MEMORY_SIZE_IN_BYTES;

	if (sep_map_and_alloc_shared_area(sep)) {
		error = -ENOMEM;
		
		goto end_function_error;
	}

	
	sep_write_reg(sep, HW_HOST_ICR_REG_ADDR, 0xFFFFFFFF);

	
	sep_write_reg(sep, HW_HOST_IMR_REG_ADDR, (~(0x1 << 13)));

	
	sep->reply_ct = sep_read_reg(sep, HW_HOST_SEP_HOST_GPR2_REG_ADDR);
	sep->reply_ct &= 0x3FFFFFFF;
	sep->send_ct = sep->reply_ct;

	
	error = request_irq(pdev->irq, sep_inthandler, IRQF_SHARED,
		"sep_driver", sep);

	if (error)
		goto end_function_deallocate_sep_shared_area;

	
	error = sep_reconfig_shared_area(sep);
	if (error)
		goto end_function_free_irq;

	sep->in_use = 1;

	
	
	error = sep_register_driver_with_fs(sep);

	if (error) {
		dev_err(&sep->pdev->dev, "error registering dev file\n");
		goto end_function_free_irq;
	}

	sep->in_use = 0; 
#ifdef SEP_ENABLE_RUNTIME_PM
	pm_runtime_put_noidle(&sep->pdev->dev);
	pm_runtime_allow(&sep->pdev->dev);
	pm_runtime_set_autosuspend_delay(&sep->pdev->dev,
		SUSPEND_DELAY);
	pm_runtime_use_autosuspend(&sep->pdev->dev);
	pm_runtime_mark_last_busy(&sep->pdev->dev);
	sep->power_save_setup = 1;
#endif
	
#if defined(CONFIG_CRYPTO) || defined(CONFIG_CRYPTO_MODULE)
	error = sep_crypto_setup();
	if (error) {
		dev_err(&sep->pdev->dev, "crypto setup failed\n");
		goto end_function_free_irq;
	}
#endif
	goto end_function;

end_function_free_irq:
	free_irq(pdev->irq, sep);

end_function_deallocate_sep_shared_area:
	
	sep_unmap_and_free_shared_area(sep);

end_function_error:
	iounmap(sep->reg_addr);

end_function_free_sep_dev:
	pci_dev_put(sep_dev->pdev);
	kfree(sep_dev);
	sep_dev = NULL;

end_function_disable_device:
	pci_disable_device(pdev);

end_function:
	return error;
}

static void sep_remove(struct pci_dev *pdev)
{
	struct sep_device *sep = sep_dev;

	
	misc_deregister(&sep->miscdev_sep);

	
#if defined(CONFIG_CRYPTO) || defined(CONFIG_CRYPTO_MODULE)
	sep_crypto_takedown();
#endif
	
	free_irq(sep->pdev->irq, sep);

	
	sep_unmap_and_free_shared_area(sep_dev);
	iounmap(sep_dev->reg_addr);

#ifdef SEP_ENABLE_RUNTIME_PM
	if (sep->in_use) {
		sep->in_use = 0;
		pm_runtime_forbid(&sep->pdev->dev);
		pm_runtime_get_noresume(&sep->pdev->dev);
	}
#endif
	pci_dev_put(sep_dev->pdev);
	kfree(sep_dev);
	sep_dev = NULL;
}

static DEFINE_PCI_DEVICE_TABLE(sep_pci_id_tbl) = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x0826)},
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x08e9)},
 	{0}
};

MODULE_DEVICE_TABLE(pci, sep_pci_id_tbl);

#ifdef SEP_ENABLE_RUNTIME_PM

static int sep_pci_resume(struct device *dev)
{
	struct sep_device *sep = sep_dev;

	dev_dbg(&sep->pdev->dev, "pci resume called\n");

	if (sep->power_state == SEP_DRIVER_POWERON)
		return 0;

	
	sep_write_reg(sep, HW_HOST_ICR_REG_ADDR, 0xFFFFFFFF);

	
	sep_write_reg(sep, HW_HOST_IMR_REG_ADDR, (~(0x1 << 13)));

	
	sep->reply_ct = sep_read_reg(sep, HW_HOST_SEP_HOST_GPR2_REG_ADDR);
	sep->reply_ct &= 0x3FFFFFFF;
	sep->send_ct = sep->reply_ct;

	sep->power_state = SEP_DRIVER_POWERON;

	return 0;
}

static int sep_pci_suspend(struct device *dev)
{
	struct sep_device *sep = sep_dev;

	dev_dbg(&sep->pdev->dev, "pci suspend called\n");
	if (sep->in_use == 1)
		return -EAGAIN;

	sep->power_state = SEP_DRIVER_POWEROFF;

	
	sep_write_reg(sep, HW_HOST_ICR_REG_ADDR, 0xFFFFFFFF);

	
	sep_write_reg(sep, HW_HOST_IMR_REG_ADDR, 0xFFFFFFFF);

	return 0;
}

static int sep_pm_runtime_resume(struct device *dev)
{

	u32 retval2;
	u32 delay_count;
	struct sep_device *sep = sep_dev;

	dev_dbg(&sep->pdev->dev, "pm runtime resume called\n");

	retval2 = 0;
	delay_count = 0;
	while ((!retval2) && (delay_count < SCU_DELAY_MAX)) {
		retval2 = sep_read_reg(sep, HW_HOST_SEP_HOST_GPR3_REG_ADDR);
		retval2 &= 0x00000008;
		if (!retval2) {
			udelay(SCU_DELAY_ITERATION);
			delay_count += 1;
		}
	}

	if (!retval2) {
		dev_warn(&sep->pdev->dev, "scu boot bit not set at resume\n");
		return -EINVAL;
	}

	
	sep_write_reg(sep, HW_HOST_ICR_REG_ADDR, 0xFFFFFFFF);

	
	sep_write_reg(sep, HW_HOST_IMR_REG_ADDR, (~(0x1 << 13)));

	
	sep->reply_ct = sep_read_reg(sep, HW_HOST_SEP_HOST_GPR2_REG_ADDR);
	sep->reply_ct &= 0x3FFFFFFF;
	sep->send_ct = sep->reply_ct;

	return 0;
}

static int sep_pm_runtime_suspend(struct device *dev)
{
	struct sep_device *sep = sep_dev;

	dev_dbg(&sep->pdev->dev, "pm runtime suspend called\n");

	
	sep_write_reg(sep, HW_HOST_ICR_REG_ADDR, 0xFFFFFFFF);
	return 0;
}

static const struct dev_pm_ops sep_pm = {
	.runtime_resume = sep_pm_runtime_resume,
	.runtime_suspend = sep_pm_runtime_suspend,
	.resume = sep_pci_resume,
	.suspend = sep_pci_suspend,
};
#endif 

static struct pci_driver sep_pci_driver = {
#ifdef SEP_ENABLE_RUNTIME_PM
	.driver = {
		.pm = &sep_pm,
	},
#endif
	.name = "sep_sec_driver",
	.id_table = sep_pci_id_tbl,
	.probe = sep_probe,
	.remove = sep_remove
};


static int __init sep_init(void)
{
	return pci_register_driver(&sep_pci_driver);
}


static void __exit sep_exit(void)
{
	pci_unregister_driver(&sep_pci_driver);
}


module_init(sep_init);
module_exit(sep_exit);

MODULE_LICENSE("GPL");
