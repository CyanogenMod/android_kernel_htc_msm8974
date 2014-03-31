#include <linux/mmzone.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/types.h>
#include <asm/mmu.h>
#include <asm/io.h>
#include <asm/mmu_context.h>
#include <arch/svinto.h>

extern void tlb_init(void);


void __init 
paging_init(void)
{
	int i;
	unsigned long zones_size[MAX_NR_ZONES];

	printk("Setting up paging and the MMU.\n");
	
	

	for(i = 0; i < PTRS_PER_PGD; i++)
		swapper_pg_dir[i] = __pgd(0);
	

	per_cpu(current_pgd, smp_processor_id()) = init_mm.pgd;

	

	tlb_init();

	

#ifdef CONFIG_CRIS_LOW_MAP

#define CACHED_BOOTROM (KSEG_F | 0x08000000UL)

	*R_MMU_KSEG = ( IO_STATE(R_MMU_KSEG, seg_f, seg  ) |  
			IO_STATE(R_MMU_KSEG, seg_e, page ) |
			IO_STATE(R_MMU_KSEG, seg_d, page ) | 
			IO_STATE(R_MMU_KSEG, seg_c, page ) |   
			IO_STATE(R_MMU_KSEG, seg_b, seg  ) |  
#ifdef CONFIG_JULIETTE
			IO_STATE(R_MMU_KSEG, seg_a, seg  ) |  
#else
			IO_STATE(R_MMU_KSEG, seg_a, page ) |
#endif
			IO_STATE(R_MMU_KSEG, seg_9, seg  ) |  
			IO_STATE(R_MMU_KSEG, seg_8, seg  ) |  
			IO_STATE(R_MMU_KSEG, seg_7, page ) |  
			IO_STATE(R_MMU_KSEG, seg_6, seg  ) |  
			IO_STATE(R_MMU_KSEG, seg_5, seg  ) |  
			IO_STATE(R_MMU_KSEG, seg_4, page ) |  
			IO_STATE(R_MMU_KSEG, seg_3, page ) |  
			IO_STATE(R_MMU_KSEG, seg_2, page ) |  
			IO_STATE(R_MMU_KSEG, seg_1, page ) |  
			IO_STATE(R_MMU_KSEG, seg_0, page ) ); 

	*R_MMU_KBASE_HI = ( IO_FIELD(R_MMU_KBASE_HI, base_f, 0x3 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_e, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_d, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_c, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_b, 0xb ) |
#ifdef CONFIG_JULIETTE
			    IO_FIELD(R_MMU_KBASE_HI, base_a, 0xa ) |
#else
			    IO_FIELD(R_MMU_KBASE_HI, base_a, 0x0 ) |
#endif
			    IO_FIELD(R_MMU_KBASE_HI, base_9, 0x9 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_8, 0x8 ) );
	
	*R_MMU_KBASE_LO = ( IO_FIELD(R_MMU_KBASE_LO, base_7, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_6, 0x4 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_5, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_4, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_3, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_2, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_1, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_0, 0x0 ) );
#else
	

#define CACHED_BOOTROM (KSEG_A | 0x08000000UL)

	*R_MMU_KSEG = ( IO_STATE(R_MMU_KSEG, seg_f, seg  ) | 
			IO_STATE(R_MMU_KSEG, seg_e, seg  ) | 
			IO_STATE(R_MMU_KSEG, seg_d, page ) | 
			IO_STATE(R_MMU_KSEG, seg_c, seg  ) | 
			IO_STATE(R_MMU_KSEG, seg_b, seg  ) | 
			IO_STATE(R_MMU_KSEG, seg_a, seg  ) | 
			IO_STATE(R_MMU_KSEG, seg_9, page ) | 
			IO_STATE(R_MMU_KSEG, seg_8, page ) |
			IO_STATE(R_MMU_KSEG, seg_7, page ) |
			IO_STATE(R_MMU_KSEG, seg_6, page ) |
			IO_STATE(R_MMU_KSEG, seg_5, page ) |
			IO_STATE(R_MMU_KSEG, seg_4, page ) |
			IO_STATE(R_MMU_KSEG, seg_3, page ) |
			IO_STATE(R_MMU_KSEG, seg_2, page ) |
			IO_STATE(R_MMU_KSEG, seg_1, page ) |
			IO_STATE(R_MMU_KSEG, seg_0, page ) );

	*R_MMU_KBASE_HI = ( IO_FIELD(R_MMU_KBASE_HI, base_f, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_e, 0x8 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_d, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_c, 0x4 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_b, 0xb ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_a, 0x3 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_9, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_HI, base_8, 0x0 ) );
	
	*R_MMU_KBASE_LO = ( IO_FIELD(R_MMU_KBASE_LO, base_7, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_6, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_5, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_4, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_3, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_2, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_1, 0x0 ) |
			    IO_FIELD(R_MMU_KBASE_LO, base_0, 0x0 ) );
#endif

	*R_MMU_CONTEXT = ( IO_FIELD(R_MMU_CONTEXT, page_id, 0 ) );
	

	*R_MMU_CTRL = ( IO_STATE(R_MMU_CTRL, inv_excp, enable ) |
			IO_STATE(R_MMU_CTRL, acc_excp, enable ) |
			IO_STATE(R_MMU_CTRL, we_excp,  enable ) );
	
	*R_MMU_ENABLE = IO_STATE(R_MMU_ENABLE, mmu_enable, enable);


	empty_zero_page = (unsigned long)alloc_bootmem_pages(PAGE_SIZE);
	memset((void *)empty_zero_page, 0, PAGE_SIZE);

	

	zones_size[0] = ((unsigned long)high_memory - PAGE_OFFSET) >> PAGE_SHIFT;

	for (i = 1; i < MAX_NR_ZONES; i++)
		zones_size[i] = 0;


	free_area_init_node(0, zones_size, PAGE_OFFSET >> PAGE_SHIFT, 0);
}


static int
__init init_ioremap(void)
{
  
	

#ifdef CONFIG_CRIS_LOW_MAP
	
	port_cse1_addr = (volatile unsigned long *)(MEM_CSE1_START | 
	                                            MEM_NON_CACHEABLE);
	port_csp0_addr = (volatile unsigned long *)(MEM_CSP0_START |
	                                            MEM_NON_CACHEABLE);
	port_csp4_addr = (volatile unsigned long *)(MEM_CSP4_START |
	                                            MEM_NON_CACHEABLE);
#else
        
	port_cse1_addr = (volatile unsigned long *)
	  ioremap((unsigned long)(MEM_CSE1_START | MEM_NON_CACHEABLE), 16);
	port_csp0_addr = (volatile unsigned long *)
	  ioremap((unsigned long)(MEM_CSP0_START | MEM_NON_CACHEABLE), 16);
	port_csp4_addr = (volatile unsigned long *)
	  ioremap((unsigned long)(MEM_CSP4_START | MEM_NON_CACHEABLE), 16);
#endif	
	return 0;
}

__initcall(init_ioremap);


static inline void
flush_etrax_cacherange(void *startadr, int length)
{

	volatile short *flushadr = (volatile short *)(((unsigned long)startadr & ~PAGE_MASK) |
						      CACHED_BOOTROM);

	length = length > 8192 ? 8192 : length;  

	while(length > 0) {
		*flushadr; 
		flushadr += (32/sizeof(short));  
		length -= 32;
	}
}


void
prepare_rx_descriptor(struct etrax_dma_descr *desc)
{
	flush_etrax_cacherange((void *)desc->buf, desc->sw_len ? desc->sw_len : 65536);
}


void
flush_etrax_cache(void)
{
	flush_etrax_cacherange(0, 8192);
}
