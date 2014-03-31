#ifndef _ASM_IA64_MACHVEC_HPZX1_h
#define _ASM_IA64_MACHVEC_HPZX1_h

extern ia64_mv_setup_t			dig_setup;
extern ia64_mv_dma_init			sba_dma_init;

#define platform_name				"hpzx1"
#define platform_setup				dig_setup
#define platform_dma_init			sba_dma_init

#endif 
