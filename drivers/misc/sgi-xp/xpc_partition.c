/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2004-2008 Silicon Graphics, Inc.  All Rights Reserved.
 */


#include <linux/device.h>
#include <linux/hardirq.h>
#include <linux/slab.h>
#include "xpc.h"
#include <asm/uv/uv_hub.h>

int xpc_exiting;

struct xpc_rsvd_page *xpc_rsvd_page;
static unsigned long *xpc_part_nasids;
unsigned long *xpc_mach_nasids;

static int xpc_nasid_mask_nbytes;	
int xpc_nasid_mask_nlongs;	

struct xpc_partition *xpc_partitions;

void *
xpc_kmalloc_cacheline_aligned(size_t size, gfp_t flags, void **base)
{
	
	*base = kmalloc(size, flags);
	if (*base == NULL)
		return NULL;

	if ((u64)*base == L1_CACHE_ALIGN((u64)*base))
		return *base;

	kfree(*base);

	
	*base = kmalloc(size + L1_CACHE_BYTES, flags);
	if (*base == NULL)
		return NULL;

	return (void *)L1_CACHE_ALIGN((u64)*base);
}

static unsigned long
xpc_get_rsvd_page_pa(int nasid)
{
	enum xp_retval ret;
	u64 cookie = 0;
	unsigned long rp_pa = nasid;	
	size_t len = 0;
	size_t buf_len = 0;
	void *buf = buf;
	void *buf_base = NULL;
	enum xp_retval (*get_partition_rsvd_page_pa)
		(void *, u64 *, unsigned long *, size_t *) =
		xpc_arch_ops.get_partition_rsvd_page_pa;

	while (1) {

		ret = get_partition_rsvd_page_pa(buf, &cookie, &rp_pa, &len);

		dev_dbg(xpc_part, "SAL returned with ret=%d, cookie=0x%016lx, "
			"address=0x%016lx, len=0x%016lx\n", ret,
			(unsigned long)cookie, rp_pa, len);

		if (ret != xpNeedMoreInfo)
			break;

		
		if (is_shub())
			len = L1_CACHE_ALIGN(len);

		if (len > buf_len) {
			if (buf_base != NULL)
				kfree(buf_base);
			buf_len = L1_CACHE_ALIGN(len);
			buf = xpc_kmalloc_cacheline_aligned(buf_len, GFP_KERNEL,
							    &buf_base);
			if (buf_base == NULL) {
				dev_err(xpc_part, "unable to kmalloc "
					"len=0x%016lx\n", buf_len);
				ret = xpNoMemory;
				break;
			}
		}

		ret = xp_remote_memcpy(xp_pa(buf), rp_pa, len);
		if (ret != xpSuccess) {
			dev_dbg(xpc_part, "xp_remote_memcpy failed %d\n", ret);
			break;
		}
	}

	kfree(buf_base);

	if (ret != xpSuccess)
		rp_pa = 0;

	dev_dbg(xpc_part, "reserved page at phys address 0x%016lx\n", rp_pa);
	return rp_pa;
}

int
xpc_setup_rsvd_page(void)
{
	int ret;
	struct xpc_rsvd_page *rp;
	unsigned long rp_pa;
	unsigned long new_ts_jiffies;

	

	preempt_disable();
	rp_pa = xpc_get_rsvd_page_pa(xp_cpu_to_nasid(smp_processor_id()));
	preempt_enable();
	if (rp_pa == 0) {
		dev_err(xpc_part, "SAL failed to locate the reserved page\n");
		return -ESRCH;
	}
	rp = (struct xpc_rsvd_page *)__va(xp_socket_pa(rp_pa));

	if (rp->SAL_version < 3) {
		
		rp->SAL_partid &= 0xff;
	}
	BUG_ON(rp->SAL_partid != xp_partition_id);

	if (rp->SAL_partid < 0 || rp->SAL_partid >= xp_max_npartitions) {
		dev_err(xpc_part, "the reserved page's partid of %d is outside "
			"supported range (< 0 || >= %d)\n", rp->SAL_partid,
			xp_max_npartitions);
		return -EINVAL;
	}

	rp->version = XPC_RP_VERSION;
	rp->max_npartitions = xp_max_npartitions;

	
	if (rp->SAL_version == 1) {
		
		rp->SAL_nasids_size = 128;
	}
	xpc_nasid_mask_nbytes = rp->SAL_nasids_size;
	xpc_nasid_mask_nlongs = BITS_TO_LONGS(rp->SAL_nasids_size *
					      BITS_PER_BYTE);

	
	xpc_part_nasids = XPC_RP_PART_NASIDS(rp);
	xpc_mach_nasids = XPC_RP_MACH_NASIDS(rp);

	ret = xpc_arch_ops.setup_rsvd_page(rp);
	if (ret != 0)
		return ret;

	new_ts_jiffies = jiffies;
	if (new_ts_jiffies == 0 || new_ts_jiffies == rp->ts_jiffies)
		new_ts_jiffies++;
	rp->ts_jiffies = new_ts_jiffies;

	xpc_rsvd_page = rp;
	return 0;
}

void
xpc_teardown_rsvd_page(void)
{
	
	xpc_rsvd_page->ts_jiffies = 0;
}

enum xp_retval
xpc_get_remote_rp(int nasid, unsigned long *discovered_nasids,
		  struct xpc_rsvd_page *remote_rp, unsigned long *remote_rp_pa)
{
	int l;
	enum xp_retval ret;

	

	*remote_rp_pa = xpc_get_rsvd_page_pa(nasid);
	if (*remote_rp_pa == 0)
		return xpNoRsvdPageAddr;

	
	ret = xp_remote_memcpy(xp_pa(remote_rp), *remote_rp_pa,
			       XPC_RP_HEADER_SIZE + xpc_nasid_mask_nbytes);
	if (ret != xpSuccess)
		return ret;

	if (discovered_nasids != NULL) {
		unsigned long *remote_part_nasids =
		    XPC_RP_PART_NASIDS(remote_rp);

		for (l = 0; l < xpc_nasid_mask_nlongs; l++)
			discovered_nasids[l] |= remote_part_nasids[l];
	}

	
	if (remote_rp->ts_jiffies == 0)
		return xpRsvdPageNotSet;

	if (XPC_VERSION_MAJOR(remote_rp->version) !=
	    XPC_VERSION_MAJOR(XPC_RP_VERSION)) {
		return xpBadVersion;
	}

	
	if (remote_rp->SAL_partid < 0 ||
	    remote_rp->SAL_partid >= xp_max_npartitions ||
	    remote_rp->max_npartitions <= xp_partition_id) {
		return xpInvalidPartid;
	}

	if (remote_rp->SAL_partid == xp_partition_id)
		return xpLocalPartid;

	return xpSuccess;
}

int
xpc_partition_disengaged(struct xpc_partition *part)
{
	short partid = XPC_PARTID(part);
	int disengaged;

	disengaged = !xpc_arch_ops.partition_engaged(partid);
	if (part->disengage_timeout) {
		if (!disengaged) {
			if (time_is_after_jiffies(part->disengage_timeout)) {
				
				return 0;
			}


			dev_info(xpc_part, "deactivate request to remote "
				 "partition %d timed out\n", partid);
			xpc_disengage_timedout = 1;
			xpc_arch_ops.assume_partition_disengaged(partid);
			disengaged = 1;
		}
		part->disengage_timeout = 0;

		
		if (!in_interrupt())
			del_singleshot_timer_sync(&part->disengage_timer);

		DBUG_ON(part->act_state != XPC_P_AS_DEACTIVATING &&
			part->act_state != XPC_P_AS_INACTIVE);
		if (part->act_state != XPC_P_AS_INACTIVE)
			xpc_wakeup_channel_mgr(part);

		xpc_arch_ops.cancel_partition_deactivation_request(part);
	}
	return disengaged;
}

enum xp_retval
xpc_mark_partition_active(struct xpc_partition *part)
{
	unsigned long irq_flags;
	enum xp_retval ret;

	dev_dbg(xpc_part, "setting partition %d to ACTIVE\n", XPC_PARTID(part));

	spin_lock_irqsave(&part->act_lock, irq_flags);
	if (part->act_state == XPC_P_AS_ACTIVATING) {
		part->act_state = XPC_P_AS_ACTIVE;
		ret = xpSuccess;
	} else {
		DBUG_ON(part->reason == xpSuccess);
		ret = part->reason;
	}
	spin_unlock_irqrestore(&part->act_lock, irq_flags);

	return ret;
}

void
xpc_deactivate_partition(const int line, struct xpc_partition *part,
			 enum xp_retval reason)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&part->act_lock, irq_flags);

	if (part->act_state == XPC_P_AS_INACTIVE) {
		XPC_SET_REASON(part, reason, line);
		spin_unlock_irqrestore(&part->act_lock, irq_flags);
		if (reason == xpReactivating) {
			
			xpc_arch_ops.request_partition_reactivation(part);
		}
		return;
	}
	if (part->act_state == XPC_P_AS_DEACTIVATING) {
		if ((part->reason == xpUnloading && reason != xpUnloading) ||
		    reason == xpReactivating) {
			XPC_SET_REASON(part, reason, line);
		}
		spin_unlock_irqrestore(&part->act_lock, irq_flags);
		return;
	}

	part->act_state = XPC_P_AS_DEACTIVATING;
	XPC_SET_REASON(part, reason, line);

	spin_unlock_irqrestore(&part->act_lock, irq_flags);

	
	xpc_arch_ops.request_partition_deactivation(part);

	
	part->disengage_timeout = jiffies + (xpc_disengage_timelimit * HZ);
	part->disengage_timer.expires = part->disengage_timeout;
	add_timer(&part->disengage_timer);

	dev_dbg(xpc_part, "bringing partition %d down, reason = %d\n",
		XPC_PARTID(part), reason);

	xpc_partition_going_down(part, reason);
}

void
xpc_mark_partition_inactive(struct xpc_partition *part)
{
	unsigned long irq_flags;

	dev_dbg(xpc_part, "setting partition %d to INACTIVE\n",
		XPC_PARTID(part));

	spin_lock_irqsave(&part->act_lock, irq_flags);
	part->act_state = XPC_P_AS_INACTIVE;
	spin_unlock_irqrestore(&part->act_lock, irq_flags);
	part->remote_rp_pa = 0;
}

void
xpc_discovery(void)
{
	void *remote_rp_base;
	struct xpc_rsvd_page *remote_rp;
	unsigned long remote_rp_pa;
	int region;
	int region_size;
	int max_regions;
	int nasid;
	struct xpc_rsvd_page *rp;
	unsigned long *discovered_nasids;
	enum xp_retval ret;

	remote_rp = xpc_kmalloc_cacheline_aligned(XPC_RP_HEADER_SIZE +
						  xpc_nasid_mask_nbytes,
						  GFP_KERNEL, &remote_rp_base);
	if (remote_rp == NULL)
		return;

	discovered_nasids = kzalloc(sizeof(long) * xpc_nasid_mask_nlongs,
				    GFP_KERNEL);
	if (discovered_nasids == NULL) {
		kfree(remote_rp_base);
		return;
	}

	rp = (struct xpc_rsvd_page *)xpc_rsvd_page;

	region_size = xp_region_size;

	if (is_uv())
		max_regions = 256;
	else {
		max_regions = 64;

		switch (region_size) {
		case 128:
			max_regions *= 2;
		case 64:
			max_regions *= 2;
		case 32:
			max_regions *= 2;
			region_size = 16;
			DBUG_ON(!is_shub2());
		}
	}

	for (region = 0; region < max_regions; region++) {

		if (xpc_exiting)
			break;

		dev_dbg(xpc_part, "searching region %d\n", region);

		for (nasid = (region * region_size * 2);
		     nasid < ((region + 1) * region_size * 2); nasid += 2) {

			if (xpc_exiting)
				break;

			dev_dbg(xpc_part, "checking nasid %d\n", nasid);

			if (test_bit(nasid / 2, xpc_part_nasids)) {
				dev_dbg(xpc_part, "PROM indicates Nasid %d is "
					"part of the local partition; skipping "
					"region\n", nasid);
				break;
			}

			if (!(test_bit(nasid / 2, xpc_mach_nasids))) {
				dev_dbg(xpc_part, "PROM indicates Nasid %d was "
					"not on Numa-Link network at reset\n",
					nasid);
				continue;
			}

			if (test_bit(nasid / 2, discovered_nasids)) {
				dev_dbg(xpc_part, "Nasid %d is part of a "
					"partition which was previously "
					"discovered\n", nasid);
				continue;
			}

			

			ret = xpc_get_remote_rp(nasid, discovered_nasids,
						remote_rp, &remote_rp_pa);
			if (ret != xpSuccess) {
				dev_dbg(xpc_part, "unable to get reserved page "
					"from nasid %d, reason=%d\n", nasid,
					ret);

				if (ret == xpLocalPartid)
					break;

				continue;
			}

			xpc_arch_ops.request_partition_activation(remote_rp,
							 remote_rp_pa, nasid);
		}
	}

	kfree(discovered_nasids);
	kfree(remote_rp_base);
}

enum xp_retval
xpc_initiate_partid_to_nasids(short partid, void *nasid_mask)
{
	struct xpc_partition *part;
	unsigned long part_nasid_pa;

	part = &xpc_partitions[partid];
	if (part->remote_rp_pa == 0)
		return xpPartitionDown;

	memset(nasid_mask, 0, xpc_nasid_mask_nbytes);

	part_nasid_pa = (unsigned long)XPC_RP_PART_NASIDS(part->remote_rp_pa);

	return xp_remote_memcpy(xp_pa(nasid_mask), part_nasid_pa,
				xpc_nasid_mask_nbytes);
}
