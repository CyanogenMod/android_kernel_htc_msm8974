/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2000-2007 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <linux/module.h>
#include <asm/sn/nodepda.h>
#include <asm/sn/addrs.h>
#include <asm/sn/arch.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/pda.h>
#include <asm/sn/shubio.h>
#include <asm/nodedata.h>
#include <asm/delay.h>

#include <linux/bootmem.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <asm/sn/bte.h>

#ifndef L1_CACHE_MASK
#define L1_CACHE_MASK (L1_CACHE_BYTES - 1)
#endif

#define MAX_INTERFACES_TO_TRY		4
#define MAX_NODES_TO_TRY		2

static struct bteinfo_s *bte_if_on_node(nasid_t nasid, int interface)
{
	nodepda_t *tmp_nodepda;

	if (nasid_to_cnodeid(nasid) == -1)
		return (struct bteinfo_s *)NULL;

	tmp_nodepda = NODEPDA(nasid_to_cnodeid(nasid));
	return &tmp_nodepda->bte_if[interface];

}

static inline void bte_start_transfer(struct bteinfo_s *bte, u64 len, u64 mode)
{
	if (is_shub2()) {
		BTE_CTRL_STORE(bte, (IBLS_BUSY | ((len) | (mode) << 24)));
	} else {
		BTE_LNSTAT_STORE(bte, len);
		BTE_CTRL_STORE(bte, mode);
	}
}


bte_result_t bte_copy(u64 src, u64 dest, u64 len, u64 mode, void *notification)
{
	u64 transfer_size;
	u64 transfer_stat;
	u64 notif_phys_addr;
	struct bteinfo_s *bte;
	bte_result_t bte_status;
	unsigned long irq_flags;
	unsigned long itc_end = 0;
	int nasid_to_try[MAX_NODES_TO_TRY];
	int my_nasid = cpuid_to_nasid(raw_smp_processor_id());
	int bte_if_index, nasid_index;
	int bte_first, btes_per_node = BTES_PER_NODE;

	BTE_PRINTK(("bte_copy(0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%p)\n",
		    src, dest, len, mode, notification));

	if (len == 0) {
		return BTE_SUCCESS;
	}

	BUG_ON(len & L1_CACHE_MASK);
	BUG_ON(src & L1_CACHE_MASK);
	BUG_ON(dest & L1_CACHE_MASK);
	BUG_ON(len > BTE_MAX_XFER);

	bte_first = raw_smp_processor_id() % btes_per_node;

	if (mode & BTE_USE_DEST) {
		
		nasid_to_try[0] = NASID_GET(dest);
		if (mode & BTE_USE_ANY) {
			nasid_to_try[1] = my_nasid;
		} else {
			nasid_to_try[1] = (int)NULL;
		}
	} else {
		
		nasid_to_try[0] = my_nasid;
		if (mode & BTE_USE_ANY) {
			nasid_to_try[1] = NASID_GET(dest);
		} else {
			nasid_to_try[1] = (int)NULL;
		}
	}

retry_bteop:
	do {
		local_irq_save(irq_flags);

		bte_if_index = bte_first;
		nasid_index = 0;

		
		while (nasid_index < MAX_NODES_TO_TRY) {
			bte = bte_if_on_node(nasid_to_try[nasid_index],bte_if_index);

			if (bte == NULL) {
				nasid_index++;
				continue;
			}

			if (spin_trylock(&bte->spinlock)) {
				if (!(*bte->most_rcnt_na & BTE_WORD_AVAILABLE) ||
				    (BTE_LNSTAT_LOAD(bte) & BTE_ACTIVE)) {
					
					spin_unlock(&bte->spinlock);
				} else {
					
					break;
				}
			}

			bte_if_index = (bte_if_index + 1) % btes_per_node; 
			if (bte_if_index == bte_first) {
				nasid_index++;
			}

			bte = NULL;
		}

		if (bte != NULL) {
			break;
		}

		local_irq_restore(irq_flags);

		if (!(mode & BTE_WACQUIRE)) {
			return BTEFAIL_NOTAVAIL;
		}
	} while (1);

	if (notification == NULL) {
		
		bte->most_rcnt_na = &bte->notify;
	} else {
		bte->most_rcnt_na = notification;
	}

	
	transfer_size = ((len >> L1_CACHE_SHIFT) & BTE_LEN_MASK);

	
	*bte->most_rcnt_na = BTE_WORD_BUSY;
	notif_phys_addr = (u64)bte->most_rcnt_na;

	
	BTE_PRINTKV(("IBSA = 0x%lx)\n", src));
	BTE_SRC_STORE(bte, src);
	BTE_PRINTKV(("IBDA = 0x%lx)\n", dest));
	BTE_DEST_STORE(bte, dest);

	
	BTE_PRINTKV(("IBNA = 0x%lx)\n", notif_phys_addr));
	BTE_NOTIF_STORE(bte, notif_phys_addr);

	
	BTE_PRINTK(("IBCT = 0x%lx)\n", BTE_VALID_MODE(mode)));
	bte_start_transfer(bte, transfer_size, BTE_VALID_MODE(mode));

	itc_end = ia64_get_itc() + (40000000 * local_cpu_data->cyc_per_usec);

	spin_unlock_irqrestore(&bte->spinlock, irq_flags);

	if (notification != NULL) {
		return BTE_SUCCESS;
	}

	while ((transfer_stat = *bte->most_rcnt_na) == BTE_WORD_BUSY) {
		cpu_relax();
		if (ia64_get_itc() > itc_end) {
			BTE_PRINTK(("BTE timeout nasid 0x%x bte%d IBLS = 0x%lx na 0x%lx\n",
				NASID_GET(bte->bte_base_addr), bte->bte_num,
				BTE_LNSTAT_LOAD(bte), *bte->most_rcnt_na) );
			bte->bte_error_count++;
			bte->bh_error = IBLS_ERROR;
			bte_error_handler((unsigned long)NODEPDA(bte->bte_cnode));
			*bte->most_rcnt_na = BTE_WORD_AVAILABLE;
			goto retry_bteop;
		}
	}

	BTE_PRINTKV((" Delay Done.  IBLS = 0x%lx, most_rcnt_na = 0x%lx\n",
		     BTE_LNSTAT_LOAD(bte), *bte->most_rcnt_na));

	if (transfer_stat & IBLS_ERROR) {
		bte_status = BTE_GET_ERROR_STATUS(transfer_stat);
	} else {
		bte_status = BTE_SUCCESS;
	}
	*bte->most_rcnt_na = BTE_WORD_AVAILABLE;

	BTE_PRINTK(("Returning status is 0x%lx and most_rcnt_na is 0x%lx\n",
		    BTE_LNSTAT_LOAD(bte), *bte->most_rcnt_na));

	return bte_status;
}

EXPORT_SYMBOL(bte_copy);

bte_result_t bte_unaligned_copy(u64 src, u64 dest, u64 len, u64 mode)
{
	int destFirstCacheOffset;
	u64 headBteSource;
	u64 headBteLen;
	u64 headBcopySrcOffset;
	u64 headBcopyDest;
	u64 headBcopyLen;
	u64 footBteSource;
	u64 footBteLen;
	u64 footBcopyDest;
	u64 footBcopyLen;
	bte_result_t rv;
	char *bteBlock, *bteBlock_unaligned;

	if (len == 0) {
		return BTE_SUCCESS;
	}

	
	bteBlock_unaligned = kmalloc(len + 3 * L1_CACHE_BYTES, GFP_KERNEL);
	if (bteBlock_unaligned == NULL) {
		return BTEFAIL_NOTAVAIL;
	}
	bteBlock = (char *)L1_CACHE_ALIGN((u64) bteBlock_unaligned);

	headBcopySrcOffset = src & L1_CACHE_MASK;
	destFirstCacheOffset = dest & L1_CACHE_MASK;

	if (headBcopySrcOffset == destFirstCacheOffset) {

		headBteSource = src & ~L1_CACHE_MASK;
		headBcopyDest = dest;
		if (headBcopySrcOffset) {
			headBcopyLen =
			    (len >
			     (L1_CACHE_BYTES -
			      headBcopySrcOffset) ? L1_CACHE_BYTES
			     - headBcopySrcOffset : len);
			headBteLen = L1_CACHE_BYTES;
		} else {
			headBcopyLen = 0;
			headBteLen = 0;
		}

		if (len > headBcopyLen) {
			footBcopyLen = (len - headBcopyLen) & L1_CACHE_MASK;
			footBteLen = L1_CACHE_BYTES;

			footBteSource = src + len - footBcopyLen;
			footBcopyDest = dest + len - footBcopyLen;

			if (footBcopyDest == (headBcopyDest + headBcopyLen)) {
				headBcopyLen += footBcopyLen;
				headBteLen += footBteLen;
			} else if (footBcopyLen > 0) {
				rv = bte_copy(footBteSource,
					      ia64_tpa((unsigned long)bteBlock),
					      footBteLen, mode, NULL);
				if (rv != BTE_SUCCESS) {
					kfree(bteBlock_unaligned);
					return rv;
				}

				memcpy(__va(footBcopyDest),
				       (char *)bteBlock, footBcopyLen);
			}
		} else {
			footBcopyLen = 0;
			footBteLen = 0;
		}

		if (len > (headBcopyLen + footBcopyLen)) {
			
			rv = bte_copy((src + headBcopyLen),
				      (dest +
				       headBcopyLen),
				      (len - headBcopyLen -
				       footBcopyLen), mode, NULL);
			if (rv != BTE_SUCCESS) {
				kfree(bteBlock_unaligned);
				return rv;
			}

		}
	} else {


		headBcopySrcOffset = src & L1_CACHE_MASK;
		headBcopyDest = dest;
		headBcopyLen = len;

		headBteSource = src - headBcopySrcOffset;
		
		headBteLen = L1_CACHE_ALIGN(len + headBcopySrcOffset);
	}

	if (headBcopyLen > 0) {
		rv = bte_copy(headBteSource,
			      ia64_tpa((unsigned long)bteBlock), headBteLen,
			      mode, NULL);
		if (rv != BTE_SUCCESS) {
			kfree(bteBlock_unaligned);
			return rv;
		}

		memcpy(__va(headBcopyDest), ((char *)bteBlock +
					     headBcopySrcOffset), headBcopyLen);
	}
	kfree(bteBlock_unaligned);
	return BTE_SUCCESS;
}

EXPORT_SYMBOL(bte_unaligned_copy);


void bte_init_node(nodepda_t * mynodepda, cnodeid_t cnode)
{
	int i;


	spin_lock_init(&mynodepda->bte_recovery_lock);
	init_timer(&mynodepda->bte_recovery_timer);
	mynodepda->bte_recovery_timer.function = bte_error_handler;
	mynodepda->bte_recovery_timer.data = (unsigned long)mynodepda;

	for (i = 0; i < BTES_PER_NODE; i++) {
		u64 *base_addr;

		
		base_addr = (u64 *)
		    REMOTE_HUB_ADDR(cnodeid_to_nasid(cnode), BTE_BASE_ADDR(i));
		mynodepda->bte_if[i].bte_base_addr = base_addr;
		mynodepda->bte_if[i].bte_source_addr = BTE_SOURCE_ADDR(base_addr);
		mynodepda->bte_if[i].bte_destination_addr = BTE_DEST_ADDR(base_addr);
		mynodepda->bte_if[i].bte_control_addr = BTE_CTRL_ADDR(base_addr);
		mynodepda->bte_if[i].bte_notify_addr = BTE_NOTIF_ADDR(base_addr);

		mynodepda->bte_if[i].most_rcnt_na =
		    &(mynodepda->bte_if[i].notify);
		mynodepda->bte_if[i].notify = BTE_WORD_AVAILABLE;
		spin_lock_init(&mynodepda->bte_if[i].spinlock);

		mynodepda->bte_if[i].bte_cnode = cnode;
		mynodepda->bte_if[i].bte_error_count = 0;
		mynodepda->bte_if[i].bte_num = i;
		mynodepda->bte_if[i].cleanup_active = 0;
		mynodepda->bte_if[i].bh_error = 0;
	}

}
