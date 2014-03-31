#ifndef _ASM_IA64_MACHVEC_DIG_VTD_h
#define _ASM_IA64_MACHVEC_DIG_VTD_h

extern ia64_mv_setup_t			dig_setup;
extern ia64_mv_dma_init			pci_iommu_alloc;

#define platform_name				"dig_vtd"
#define platform_setup				dig_setup
#define platform_dma_init			pci_iommu_alloc

#endif 
