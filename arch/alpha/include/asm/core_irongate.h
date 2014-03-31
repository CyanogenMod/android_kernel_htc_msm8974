#ifndef __ALPHA_IRONGATE__H__
#define __ALPHA_IRONGATE__H__

#include <linux/types.h>
#include <asm/compiler.h>

/*
 * IRONGATE is the internal name for the AMD-751 K7 core logic chipset
 * which provides memory controller and PCI access for NAUTILUS-based
 * EV6 (21264) systems.
 *
 * This file is based on:
 *
 * IronGate management library, (c) 1999 Alpha Processor, Inc.
 * Copyright (C) 1999 Alpha Processor, Inc.,
 *	(David Daniel, Stig Telfer, Soohoon Lee)
 */



typedef volatile __u32	igcsr32;

typedef struct {
	igcsr32 dev_vendor;		
	igcsr32 stat_cmd;		
	igcsr32 class;			
	igcsr32 latency;		
	igcsr32 bar0;			
	igcsr32 bar1;			
	igcsr32 bar2;			

	igcsr32 rsrvd0[6];		

	igcsr32 capptr;			

	igcsr32 rsrvd1[2];		

	igcsr32 bacsr10;		
	igcsr32 bacsr32;		
	igcsr32 bacsr54_eccms761;	

	igcsr32 rsrvd2[1];		

	igcsr32 drammap;		
	igcsr32 dramtm;			
	igcsr32 dramms;			

	igcsr32 rsrvd3[1];		

	igcsr32 biu0;			
	igcsr32 biusip;			

	igcsr32 rsrvd4[2];		

	igcsr32 mro;			

	igcsr32 rsrvd5[3];		

	igcsr32 whami;			
	igcsr32 pciarb;			
	igcsr32 pcicfg;			

	igcsr32 rsrvd6[4];		

	igcsr32 pci_mem;		

	
	igcsr32 agpcap;			
	igcsr32 agpstat;		
	igcsr32 agpcmd;			
	igcsr32 agpva;			
	igcsr32 agpmode;		
} Irongate0;


typedef struct {

	igcsr32 dev_vendor;		
	igcsr32 stat_cmd;		
	igcsr32 class;			
	igcsr32 htype;			
	igcsr32 rsrvd0[2];		
	igcsr32 busnos;			
	igcsr32 io_baselim_regs;	
	igcsr32	mem_baselim;		
	igcsr32 pfmem_baselim;		
	igcsr32 rsrvd1[2];		
	igcsr32 io_baselim;		
	igcsr32 rsrvd2[2];		
	igcsr32 interrupt;		

} Irongate1;

extern igcsr32 *IronECC;


#ifdef USE_48_BIT_KSEG
#define IRONGATE_BIAS 0x80000000000UL
#else
#define IRONGATE_BIAS 0x10000000000UL
#endif


#define IRONGATE_MEM		(IDENT_ADDR | IRONGATE_BIAS | 0x000000000UL)
#define IRONGATE_IACK_SC	(IDENT_ADDR | IRONGATE_BIAS | 0x1F8000000UL)
#define IRONGATE_IO		(IDENT_ADDR | IRONGATE_BIAS | 0x1FC000000UL)
#define IRONGATE_CONF		(IDENT_ADDR | IRONGATE_BIAS | 0x1FE000000UL)


#define IGCSR(dev,fun,reg)	( IRONGATE_CONF | \
				((dev)<<11) | \
				((fun)<<8) | \
				(reg) )

#define IRONGATE0		((Irongate0 *) IGCSR(0, 0, 0))
#define IRONGATE1		((Irongate1 *) IGCSR(1, 0, 0))


#define SCB_Q_SYSERR	0x620			
#define SCB_Q_PROCERR	0x630
#define SCB_Q_SYSMCHK	0x660
#define SCB_Q_PROCMCHK	0x670

struct el_IRONGATE_sysdata_mcheck {
	__u32 FrameSize;                 
	__u32 FrameFlags;                
	__u32 CpuOffset;                 
	__u32 SystemOffset;              
	__u32 MCHK_Code;
	__u32 MCHK_Frame_Rev;
	__u64 I_STAT;
	__u64 DC_STAT;
	__u64 C_ADDR;
	__u64 DC1_SYNDROME;
	__u64 DC0_SYNDROME;
	__u64 C_STAT;
	__u64 C_STS;
	__u64 RESERVED0;
	__u64 EXC_ADDR;
	__u64 IER_CM;
	__u64 ISUM;
	__u64 MM_STAT;
	__u64 PAL_BASE;
	__u64 I_CTL;
	__u64 PCTX;
};


#ifdef __KERNEL__

#ifndef __EXTERN_INLINE
#define __EXTERN_INLINE extern inline
#define __IO_EXTERN_INLINE
#endif



__EXTERN_INLINE void __iomem *irongate_ioportmap(unsigned long addr)
{
	return (void __iomem *)(addr + IRONGATE_IO);
}

extern void __iomem *irongate_ioremap(unsigned long addr, unsigned long size);
extern void irongate_iounmap(volatile void __iomem *addr);

__EXTERN_INLINE int irongate_is_ioaddr(unsigned long addr)
{
	return addr >= IRONGATE_MEM;
}

__EXTERN_INLINE int irongate_is_mmio(const volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long)xaddr;
	return addr < IRONGATE_IO || addr >= IRONGATE_CONF;
}

#undef __IO_PREFIX
#define __IO_PREFIX			irongate
#define irongate_trivial_rw_bw		1
#define irongate_trivial_rw_lq		1
#define irongate_trivial_io_bw		1
#define irongate_trivial_io_lq		1
#define irongate_trivial_iounmap	0
#include <asm/io_trivial.h>

#ifdef __IO_EXTERN_INLINE
#undef __EXTERN_INLINE
#undef __IO_EXTERN_INLINE
#endif

#endif 

#endif 
