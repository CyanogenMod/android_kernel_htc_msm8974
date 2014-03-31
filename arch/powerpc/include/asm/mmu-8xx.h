#ifndef _ASM_POWERPC_MMU_8XX_H_
#define _ASM_POWERPC_MMU_8XX_H_

/* Control/status registers for the MPC8xx.
 * A write operation to these registers causes serialized access.
 * During software tablewalk, the registers used perform mask/shift-add
 * operations when written/read.  A TLB entry is created when the Mx_RPN
 * is written, and the contents of several registers are used to
 * create the entry.
 */
#define SPRN_MI_CTR	784	
#define MI_GPM		0x80000000	
#define MI_PPM		0x40000000	
#define MI_CIDEF	0x20000000	
#define MI_RSV4I	0x08000000	
#define MI_PPCS		0x02000000	
#define MI_IDXMASK	0x00001f00	
#define MI_RESETVAL	0x00000000	

#define SPRN_MI_AP	786
#define MI_Ks		0x80000000	
#define MI_Kp		0x40000000	

/* The effective page number register.  When read, contains the information
 * about the last instruction TLB miss.  When MI_RPN is written, bits in
 * this register are used to create the TLB entry.
 */
#define SPRN_MI_EPN	787
#define MI_EPNMASK	0xfffff000	
#define MI_EVALID	0x00000200	
#define MI_ASIDMASK	0x0000000f	
					

/* A "level 1" or "segment" or whatever you want to call it register.
 * For the instruction TLB, it contains bits that get loaded into the
 * TLB entry when the MI_RPN is written.
 */
#define SPRN_MI_TWC	789
#define MI_APG		0x000001e0	
#define MI_GUARDED	0x00000010	
#define MI_PSMASK	0x0000000c	
#define MI_PS8MEG	0x0000000c	
#define MI_PS512K	0x00000004	
#define MI_PS4K_16K	0x00000000	
#define MI_SVALID	0x00000001	
					

#define SPRN_MI_RPN	790

#define MI_BOOTINIT	0x000001fd

#define SPRN_MD_CTR	792	
#define MD_GPM		0x80000000	
#define MD_PPM		0x40000000	
#define MD_CIDEF	0x20000000	
#define MD_WTDEF	0x10000000	
#define MD_RSV4I	0x08000000	
#define MD_TWAM		0x04000000	
#define MD_PPCS		0x02000000	
#define MD_IDXMASK	0x00001f00	
#define MD_RESETVAL	0x04000000	

#define SPRN_M_CASID	793	
#define MC_ASIDMASK	0x0000000f	


#define SPRN_MD_AP	794
#define MD_Ks		0x80000000	
#define MD_Kp		0x40000000	

/* The effective page number register.  When read, contains the information
 * about the last instruction TLB miss.  When MD_RPN is written, bits in
 * this register are used to create the TLB entry.
 */
#define SPRN_MD_EPN	795
#define MD_EPNMASK	0xfffff000	
#define MD_EVALID	0x00000200	
#define MD_ASIDMASK	0x0000000f	
					

#define SPRN_M_TWB	796
#define	M_L1TB		0xfffff000	
#define M_L1INDX	0x00000ffc	
					

/* A "level 1" or "segment" or whatever you want to call it register.
 * For the data TLB, it contains bits that get loaded into the TLB entry
 * when the MD_RPN is written.  It is also provides the hardware assist
 * for finding the PTE address during software tablewalk.
 */
#define SPRN_MD_TWC	797
#define MD_L2TB		0xfffff000	
#define MD_L2INDX	0xfffffe00	
#define MD_APG		0x000001e0	
#define MD_GUARDED	0x00000010	
#define MD_PSMASK	0x0000000c	
#define MD_PS8MEG	0x0000000c	
#define MD_PS512K	0x00000004	
#define MD_PS4K_16K	0x00000000	
#define MD_WT		0x00000002	
#define MD_SVALID	0x00000001	
					


#define SPRN_MD_RPN	798

#define SPRN_M_TW	799

#ifndef __ASSEMBLY__
typedef struct {
	unsigned int id;
	unsigned int active;
	unsigned long vdso_base;
} mm_context_t;
#endif 

#define mmu_virtual_psize	MMU_PAGE_4K
#define mmu_linear_psize	MMU_PAGE_8M

#endif 
