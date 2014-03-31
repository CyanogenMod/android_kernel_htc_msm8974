/*
 * Arch specific extensions to struct device
 *
 * This file is released under the GPLv2
 */
#ifndef _ASM_POWERPC_DEVICE_H
#define _ASM_POWERPC_DEVICE_H

struct dma_map_ops;
struct device_node;

struct dev_archdata {
	
	struct dma_map_ops	*dma_ops;

	union {
		dma_addr_t	dma_offset;
		void		*iommu_table_base;
	} dma_data;

#ifdef CONFIG_SWIOTLB
	dma_addr_t		max_direct_dma_addr;
#endif
#ifdef CONFIG_EEH
	struct eeh_dev		*edev;
#endif
};

struct pdev_archdata {
	u64 dma_mask;
};

#define ARCH_HAS_DMA_GET_REQUIRED_MASK

#endif 
