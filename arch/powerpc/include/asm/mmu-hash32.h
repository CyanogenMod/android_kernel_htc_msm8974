#ifndef _ASM_POWERPC_MMU_HASH32_H_
#define _ASM_POWERPC_MMU_HASH32_H_


#define BL_128K	0x000
#define BL_256K 0x001
#define BL_512K 0x003
#define BL_1M   0x007
#define BL_2M   0x00F
#define BL_4M   0x01F
#define BL_8M   0x03F
#define BL_16M  0x07F
#define BL_32M  0x0FF
#define BL_64M  0x1FF
#define BL_128M 0x3FF
#define BL_256M 0x7FF

#define BPP_XX	0x00		
#define BPP_RX	0x01		
#define BPP_RW	0x02		

#ifndef __ASSEMBLY__
#ifdef CONFIG_PHYS_64BIT
#define BAT_PHYS_ADDR(x) ((u32)((x & 0x00000000fffe0000ULL) | \
				((x & 0x0000000e00000000ULL) >> 24) | \
				((x & 0x0000000100000000ULL) >> 30)))
#else
#define BAT_PHYS_ADDR(x) (x)
#endif

struct ppc_bat {
	u32 batu;
	u32 batl;
};
#endif 


#define PP_RWXX	0	
#define PP_RWRX 1	
#define PP_RWRW 2	
#define PP_RXRX 3	

#ifndef __ASSEMBLY__

struct hash_pte {
	unsigned long v:1;	
	unsigned long vsid:24;	
	unsigned long h:1;	
	unsigned long api:6;	
	unsigned long rpn:20;	
	unsigned long xpn:3;	
	unsigned long r:1;	
	unsigned long c:1;	
	unsigned long w:1;	
	unsigned long i:1;	
	unsigned long m:1;	
	unsigned long g:1;	
	unsigned long x:1;	
	unsigned long pp:2;	
};

typedef struct {
	unsigned long id;
	unsigned long vdso_base;
} mm_context_t;

#endif 

#define mmu_virtual_psize	MMU_PAGE_4K
#define mmu_linear_psize	MMU_PAGE_256M

#endif 
