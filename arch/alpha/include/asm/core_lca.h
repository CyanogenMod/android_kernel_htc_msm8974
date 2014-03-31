#ifndef __ALPHA_LCA__H__
#define __ALPHA_LCA__H__

#include <asm/compiler.h>
#include <asm/mce.h>




#define LCA_MEM_BCR0		(IDENT_ADDR + 0x120000000UL)
#define LCA_MEM_BCR1		(IDENT_ADDR + 0x120000008UL)
#define LCA_MEM_BCR2		(IDENT_ADDR + 0x120000010UL)
#define LCA_MEM_BCR3		(IDENT_ADDR + 0x120000018UL)
#define LCA_MEM_BMR0		(IDENT_ADDR + 0x120000020UL)
#define LCA_MEM_BMR1		(IDENT_ADDR + 0x120000028UL)
#define LCA_MEM_BMR2		(IDENT_ADDR + 0x120000030UL)
#define LCA_MEM_BMR3		(IDENT_ADDR + 0x120000038UL)
#define LCA_MEM_BTR0		(IDENT_ADDR + 0x120000040UL)
#define LCA_MEM_BTR1		(IDENT_ADDR + 0x120000048UL)
#define LCA_MEM_BTR2		(IDENT_ADDR + 0x120000050UL)
#define LCA_MEM_BTR3		(IDENT_ADDR + 0x120000058UL)
#define LCA_MEM_GTR		(IDENT_ADDR + 0x120000060UL)
#define LCA_MEM_ESR		(IDENT_ADDR + 0x120000068UL)
#define LCA_MEM_EAR		(IDENT_ADDR + 0x120000070UL)
#define LCA_MEM_CAR		(IDENT_ADDR + 0x120000078UL)
#define LCA_MEM_VGR		(IDENT_ADDR + 0x120000080UL)
#define LCA_MEM_PLM		(IDENT_ADDR + 0x120000088UL)
#define LCA_MEM_FOR		(IDENT_ADDR + 0x120000090UL)

#define LCA_IOC_HAE		(IDENT_ADDR + 0x180000000UL)
#define LCA_IOC_CONF		(IDENT_ADDR + 0x180000020UL)
#define LCA_IOC_STAT0		(IDENT_ADDR + 0x180000040UL)
#define LCA_IOC_STAT1		(IDENT_ADDR + 0x180000060UL)
#define LCA_IOC_TBIA		(IDENT_ADDR + 0x180000080UL)
#define LCA_IOC_TB_ENA		(IDENT_ADDR + 0x1800000a0UL)
#define LCA_IOC_SFT_RST		(IDENT_ADDR + 0x1800000c0UL)
#define LCA_IOC_PAR_DIS		(IDENT_ADDR + 0x1800000e0UL)
#define LCA_IOC_W_BASE0		(IDENT_ADDR + 0x180000100UL)
#define LCA_IOC_W_BASE1		(IDENT_ADDR + 0x180000120UL)
#define LCA_IOC_W_MASK0		(IDENT_ADDR + 0x180000140UL)
#define LCA_IOC_W_MASK1		(IDENT_ADDR + 0x180000160UL)
#define LCA_IOC_T_BASE0		(IDENT_ADDR + 0x180000180UL)
#define LCA_IOC_T_BASE1		(IDENT_ADDR + 0x1800001a0UL)
#define LCA_IOC_TB_TAG0		(IDENT_ADDR + 0x188000000UL)
#define LCA_IOC_TB_TAG1		(IDENT_ADDR + 0x188000020UL)
#define LCA_IOC_TB_TAG2		(IDENT_ADDR + 0x188000040UL)
#define LCA_IOC_TB_TAG3		(IDENT_ADDR + 0x188000060UL)
#define LCA_IOC_TB_TAG4		(IDENT_ADDR + 0x188000070UL)
#define LCA_IOC_TB_TAG5		(IDENT_ADDR + 0x1880000a0UL)
#define LCA_IOC_TB_TAG6		(IDENT_ADDR + 0x1880000c0UL)
#define LCA_IOC_TB_TAG7		(IDENT_ADDR + 0x1880000e0UL)

#define LCA_IACK_SC		(IDENT_ADDR + 0x1a0000000UL)
#define LCA_CONF		(IDENT_ADDR + 0x1e0000000UL)
#define LCA_IO			(IDENT_ADDR + 0x1c0000000UL)
#define LCA_SPARSE_MEM		(IDENT_ADDR + 0x200000000UL)
#define LCA_DENSE_MEM		(IDENT_ADDR + 0x300000000UL)

#define LCA_IOC_STAT0_CMD		0xf
#define LCA_IOC_STAT0_ERR		(1<<4)
#define LCA_IOC_STAT0_LOST		(1<<5)
#define LCA_IOC_STAT0_THIT		(1<<6)
#define LCA_IOC_STAT0_TREF		(1<<7)
#define LCA_IOC_STAT0_CODE_SHIFT	8
#define LCA_IOC_STAT0_CODE_MASK		0x7
#define LCA_IOC_STAT0_P_NBR_SHIFT	13
#define LCA_IOC_STAT0_P_NBR_MASK	0x7ffff

#define LCA_HAE_ADDRESS		LCA_IOC_HAE

#define LCA_PMR_ADDR	(IDENT_ADDR + 0x120000098UL)
#define LCA_PMR_PDIV    0x7                     
#define LCA_PMR_ODIV    0x38                    
#define LCA_PMR_INTO    0x40                    
#define LCA_PMR_DMAO    0x80                    
#define LCA_PMR_OCCEB   0xffff0000L             
#define LCA_PMR_OCCOB   0xffff000000000000L     
#define LCA_PMR_PRIMARY_MASK    0xfffffffffffffff8L


#define LCA_READ_PMR        (*(volatile unsigned long *)LCA_PMR_ADDR)
#define LCA_WRITE_PMR(d)    (*((volatile unsigned long *)LCA_PMR_ADDR) = (d))

#define LCA_GET_PRIMARY(r)  ((r) & LCA_PMR_PDIV)
#define LCA_GET_OVERRIDE(r) (((r) >> 3) & LCA_PMR_PDIV)
#define LCA_SET_PRIMARY_CLOCK(r, c) ((r) = (((r) & LCA_PMR_PRIMARY_MASK)|(c)))

#define LCA_PMR_DIV_1   0x0
#define LCA_PMR_DIV_1_5 0x1
#define LCA_PMR_DIV_2   0x2
#define LCA_PMR_DIV_4   0x3
#define LCA_PMR_DIV_8   0x4
#define LCA_PMR_DIV_16  0x5
#define LCA_PMR_DIV_MIN DIV_1
#define LCA_PMR_DIV_MAX DIV_16


struct el_lca_mcheck_short {
	struct el_common	h;		
	unsigned long		esr;		
	unsigned long		ear;		
	unsigned long		dc_stat;	
	unsigned long		ioc_stat0;	
	unsigned long		ioc_stat1;	
};

struct el_lca_mcheck_long {
	struct el_common	h;		
	unsigned long		pt[31];		
	unsigned long		exc_addr;	
	unsigned long		pad1[3];
	unsigned long		pal_base;	
	unsigned long		hier;		
	unsigned long		hirr;		
	unsigned long		mm_csr;		
	unsigned long		dc_stat;	
	unsigned long		dc_addr;	
	unsigned long		abox_ctl;	
	unsigned long		esr;		
	unsigned long		ear;		
	unsigned long		car;		
	unsigned long		ioc_stat0;	
	unsigned long		ioc_stat1;	
	unsigned long		va;		
};

union el_lca {
	struct el_common *		c;
	struct el_lca_mcheck_long *	l;
	struct el_lca_mcheck_short *	s;
};

#ifdef __KERNEL__

#ifndef __EXTERN_INLINE
#define __EXTERN_INLINE extern inline
#define __IO_EXTERN_INLINE
#endif


#define vip	volatile int __force *
#define vuip	volatile unsigned int __force *
#define vulp	volatile unsigned long __force *

#define LCA_SET_HAE						\
	do {							\
		if (addr >= (1UL << 24)) {			\
			unsigned long msb = addr & 0xf8000000;	\
			addr -= msb;				\
			set_hae(msb);				\
		}						\
	} while (0)


__EXTERN_INLINE unsigned int lca_ioread8(void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	unsigned long result, base_and_type;

	if (addr >= LCA_DENSE_MEM) {
		addr -= LCA_DENSE_MEM;
		LCA_SET_HAE;
		base_and_type = LCA_SPARSE_MEM + 0x00;
	} else {
		addr -= LCA_IO;
		base_and_type = LCA_IO + 0x00;
	}

	result = *(vip) ((addr << 5) + base_and_type);
	return __kernel_extbl(result, addr & 3);
}

__EXTERN_INLINE void lca_iowrite8(u8 b, void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	unsigned long w, base_and_type;

	if (addr >= LCA_DENSE_MEM) {
		addr -= LCA_DENSE_MEM;
		LCA_SET_HAE;
		base_and_type = LCA_SPARSE_MEM + 0x00;
	} else {
		addr -= LCA_IO;
		base_and_type = LCA_IO + 0x00;
	}

	w = __kernel_insbl(b, addr & 3);
	*(vuip) ((addr << 5) + base_and_type) = w;
}

__EXTERN_INLINE unsigned int lca_ioread16(void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	unsigned long result, base_and_type;

	if (addr >= LCA_DENSE_MEM) {
		addr -= LCA_DENSE_MEM;
		LCA_SET_HAE;
		base_and_type = LCA_SPARSE_MEM + 0x08;
	} else {
		addr -= LCA_IO;
		base_and_type = LCA_IO + 0x08;
	}

	result = *(vip) ((addr << 5) + base_and_type);
	return __kernel_extwl(result, addr & 3);
}

__EXTERN_INLINE void lca_iowrite16(u16 b, void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	unsigned long w, base_and_type;

	if (addr >= LCA_DENSE_MEM) {
		addr -= LCA_DENSE_MEM;
		LCA_SET_HAE;
		base_and_type = LCA_SPARSE_MEM + 0x08;
	} else {
		addr -= LCA_IO;
		base_and_type = LCA_IO + 0x08;
	}

	w = __kernel_inswl(b, addr & 3);
	*(vuip) ((addr << 5) + base_and_type) = w;
}

__EXTERN_INLINE unsigned int lca_ioread32(void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	if (addr < LCA_DENSE_MEM)
		addr = ((addr - LCA_IO) << 5) + LCA_IO + 0x18;
	return *(vuip)addr;
}

__EXTERN_INLINE void lca_iowrite32(u32 b, void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	if (addr < LCA_DENSE_MEM)
		addr = ((addr - LCA_IO) << 5) + LCA_IO + 0x18;
	*(vuip)addr = b;
}

__EXTERN_INLINE void __iomem *lca_ioportmap(unsigned long addr)
{
	return (void __iomem *)(addr + LCA_IO);
}

__EXTERN_INLINE void __iomem *lca_ioremap(unsigned long addr,
					  unsigned long size)
{
	return (void __iomem *)(addr + LCA_DENSE_MEM);
}

__EXTERN_INLINE int lca_is_ioaddr(unsigned long addr)
{
	return addr >= IDENT_ADDR + 0x120000000UL;
}

__EXTERN_INLINE int lca_is_mmio(const volatile void __iomem *addr)
{
	return (unsigned long)addr >= LCA_DENSE_MEM;
}

#undef vip
#undef vuip
#undef vulp

#undef __IO_PREFIX
#define __IO_PREFIX		lca
#define lca_trivial_rw_bw	2
#define lca_trivial_rw_lq	1
#define lca_trivial_io_bw	0
#define lca_trivial_io_lq	0
#define lca_trivial_iounmap	1
#include <asm/io_trivial.h>

#ifdef __IO_EXTERN_INLINE
#undef __EXTERN_INLINE
#undef __IO_EXTERN_INLINE
#endif

#endif 

#endif 
