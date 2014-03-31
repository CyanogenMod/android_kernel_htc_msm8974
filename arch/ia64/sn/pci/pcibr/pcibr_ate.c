/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001-2006 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/pcibr_provider.h>
#include <asm/sn/pcibus_provider_defs.h>
#include <asm/sn/pcidev.h>

int pcibr_invalidate_ate;	

static void mark_ate(struct ate_resource *ate_resource, int start, int number,
		     u64 value)
{
	u64 *ate = ate_resource->ate;
	int index;
	int length = 0;

	for (index = start; length < number; index++, length++)
		ate[index] = value;
}

static int find_free_ate(struct ate_resource *ate_resource, int start,
			 int count)
{
	u64 *ate = ate_resource->ate;
	int index;
	int start_free;

	for (index = start; index < ate_resource->num_ate;) {
		if (!ate[index]) {
			int i;
			int free;
			free = 0;
			start_free = index;	
			for (i = start_free; i < ate_resource->num_ate; i++) {
				if (!ate[i]) {	
					if (++free == count)
						return start_free;
				} else {
					index = i + 1;
					break;
				}
			}
			if (i >= ate_resource->num_ate)
				return -1;
		} else
			index++;	
	}

	return -1;
}

static inline void free_ate_resource(struct ate_resource *ate_resource,
				     int start)
{
	mark_ate(ate_resource, start, ate_resource->ate[start], 0);
	if ((ate_resource->lowest_free_index > start) ||
	    (ate_resource->lowest_free_index < 0))
		ate_resource->lowest_free_index = start;
}

static inline int alloc_ate_resource(struct ate_resource *ate_resource,
				     int ate_needed)
{
	int start_index;

	if (ate_resource->lowest_free_index < 0)
		return -1;

	start_index =
	    find_free_ate(ate_resource, ate_resource->lowest_free_index,
			  ate_needed);
	if (start_index >= 0)
		mark_ate(ate_resource, start_index, ate_needed, ate_needed);

	ate_resource->lowest_free_index =
	    find_free_ate(ate_resource, ate_resource->lowest_free_index, 1);

	return start_index;
}

int pcibr_ate_alloc(struct pcibus_info *pcibus_info, int count)
{
	int status;
	unsigned long flags;

	spin_lock_irqsave(&pcibus_info->pbi_lock, flags);
	status = alloc_ate_resource(&pcibus_info->pbi_int_ate_resource, count);
	spin_unlock_irqrestore(&pcibus_info->pbi_lock, flags);

	return status;
}

static inline u64 __iomem *pcibr_ate_addr(struct pcibus_info *pcibus_info,
				       int ate_index)
{
	if (ate_index < pcibus_info->pbi_int_ate_size) {
		return pcireg_int_ate_addr(pcibus_info, ate_index);
	}
	panic("pcibr_ate_addr: invalid ate_index 0x%x", ate_index);
}

void inline
ate_write(struct pcibus_info *pcibus_info, int ate_index, int count,
	  volatile u64 ate)
{
	while (count-- > 0) {
		if (ate_index < pcibus_info->pbi_int_ate_size) {
			pcireg_int_ate_set(pcibus_info, ate_index, ate);
		} else {
			panic("ate_write: invalid ate_index 0x%x", ate_index);
		}
		ate_index++;
		ate += IOPGSIZE;
	}

	pcireg_tflush_get(pcibus_info);	
}

void pcibr_ate_free(struct pcibus_info *pcibus_info, int index)
{

	volatile u64 ate;
	int count;
	unsigned long flags;

	if (pcibr_invalidate_ate) {
		
		ate = *pcibr_ate_addr(pcibus_info, index);
		count = pcibus_info->pbi_int_ate_resource.ate[index];
		ate_write(pcibus_info, index, count, (ate & ~PCI32_ATE_V));
	}

	spin_lock_irqsave(&pcibus_info->pbi_lock, flags);
	free_ate_resource(&pcibus_info->pbi_int_ate_resource, index);
	spin_unlock_irqrestore(&pcibus_info->pbi_lock, flags);
}
