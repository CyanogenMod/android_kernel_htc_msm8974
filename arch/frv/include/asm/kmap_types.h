
#ifndef _ASM_KMAP_TYPES_H
#define _ASM_KMAP_TYPES_H

enum km_type {
	
	__KM_CACHE,		
	__KM_PGD,		
	__KM_ITLB_PTD,		
	__KM_DTLB_PTD,		

	
        KM_BOUNCE_READ,
        KM_SKB_SUNRPC_DATA,
        KM_SKB_DATA_SOFTIRQ,
        KM_USER0,
        KM_USER1,
	KM_BIO_SRC_IRQ,
	KM_BIO_DST_IRQ,
	KM_PTE0,
	KM_PTE1,
	KM_IRQ0,
	KM_IRQ1,
	KM_SOFTIRQ0,
	KM_SOFTIRQ1,
	KM_TYPE_NR
};

#endif
