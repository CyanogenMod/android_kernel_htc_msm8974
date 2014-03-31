#ifndef _ASM_IA64_MACHVEC_HPZX1_SWIOTLB_h
#define _ASM_IA64_MACHVEC_HPZX1_SWIOTLB_h

extern ia64_mv_setup_t				dig_setup;
extern ia64_mv_dma_get_ops			hwsw_dma_get_ops;

#define platform_name				"hpzx1_swiotlb"
#define platform_setup				dig_setup
#define platform_dma_init			machvec_noop
#define platform_dma_get_ops			hwsw_dma_get_ops

#endif 
