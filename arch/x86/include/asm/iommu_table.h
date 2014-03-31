#ifndef _ASM_X86_IOMMU_TABLE_H
#define _ASM_X86_IOMMU_TABLE_H

#include <asm/swiotlb.h>


struct iommu_table_entry {
	initcall_t	detect;
	initcall_t	depend;
	void		(*early_init)(void); 
	void		(*late_init)(void); 
#define IOMMU_FINISH_IF_DETECTED (1<<0)
#define IOMMU_DETECTED		 (1<<1)
	int		flags;
};


#define __IOMMU_INIT(_detect, _depend, _early_init, _late_init, _finish)\
	static const struct iommu_table_entry const			\
		__iommu_entry_##_detect __used				\
	__attribute__ ((unused, __section__(".iommu_table"),		\
			aligned((sizeof(void *)))))	\
	= {_detect, _depend, _early_init, _late_init,			\
	   _finish ? IOMMU_FINISH_IF_DETECTED : 0}
#define IOMMU_INIT_POST(_detect)					\
	__IOMMU_INIT(_detect, pci_swiotlb_detect_4gb,  0, 0, 0)

#define IOMMU_INIT_POST_FINISH(detect)					\
	__IOMMU_INIT(_detect, pci_swiotlb_detect_4gb,  0, 0, 1)

#define IOMMU_INIT_FINISH(_detect, _depend, _init, _late_init)		\
	__IOMMU_INIT(_detect, _depend, _init, _late_init, 1)

#define IOMMU_INIT(_detect, _depend, _init, _late_init)			\
	__IOMMU_INIT(_detect, _depend, _init, _late_init, 0)

void sort_iommu_table(struct iommu_table_entry *start,
		      struct iommu_table_entry *finish);

void check_iommu_entries(struct iommu_table_entry *start,
			 struct iommu_table_entry *finish);

#endif 
