#ifndef __ALPHA_MCPCIA__H__
#define __ALPHA_MCPCIA__H__

#define MCPCIA_ONE_HAE_WINDOW 1

#include <linux/types.h>
#include <asm/compiler.h>
#include <asm/mce.h>





#define MCPCIA_MAX_HOSES 4

#define MCPCIA_MID(m)		((unsigned long)(m) << 33)

#define MCPCIA_HOSE2MID(h)	((h) + 4)

#define MCPCIA_MEM_MASK 0x07ffffff 

#define MCPCIA_SPARSE(m)	(IDENT_ADDR + 0xf000000000UL + MCPCIA_MID(m))
#define MCPCIA_DENSE(m)		(IDENT_ADDR + 0xf100000000UL + MCPCIA_MID(m))
#define MCPCIA_IO(m)		(IDENT_ADDR + 0xf180000000UL + MCPCIA_MID(m))
#define MCPCIA_CONF(m)		(IDENT_ADDR + 0xf1c0000000UL + MCPCIA_MID(m))
#define MCPCIA_CSR(m)		(IDENT_ADDR + 0xf1e0000000UL + MCPCIA_MID(m))
#define MCPCIA_IO_IACK(m)	(IDENT_ADDR + 0xf1f0000000UL + MCPCIA_MID(m))
#define MCPCIA_DENSE_IO(m)	(IDENT_ADDR + 0xe1fc000000UL + MCPCIA_MID(m))
#define MCPCIA_DENSE_CONF(m)	(IDENT_ADDR + 0xe1fe000000UL + MCPCIA_MID(m))

#define MCPCIA_REV(m)		(MCPCIA_CSR(m) + 0x000)
#define MCPCIA_WHOAMI(m)	(MCPCIA_CSR(m) + 0x040)
#define MCPCIA_PCI_LAT(m)	(MCPCIA_CSR(m) + 0x080)
#define MCPCIA_CAP_CTRL(m)	(MCPCIA_CSR(m) + 0x100)
#define MCPCIA_HAE_MEM(m)	(MCPCIA_CSR(m) + 0x400)
#define MCPCIA_HAE_IO(m)	(MCPCIA_CSR(m) + 0x440)
#define _MCPCIA_IACK_SC(m)	(MCPCIA_CSR(m) + 0x480)
#define MCPCIA_HAE_DENSE(m)	(MCPCIA_CSR(m) + 0x4C0)

#define MCPCIA_INT_CTL(m)	(MCPCIA_CSR(m) + 0x500)
#define MCPCIA_INT_REQ(m)	(MCPCIA_CSR(m) + 0x540)
#define MCPCIA_INT_TARG(m)	(MCPCIA_CSR(m) + 0x580)
#define MCPCIA_INT_ADR(m)	(MCPCIA_CSR(m) + 0x5C0)
#define MCPCIA_INT_ADR_EXT(m)	(MCPCIA_CSR(m) + 0x600)
#define MCPCIA_INT_MASK0(m)	(MCPCIA_CSR(m) + 0x640)
#define MCPCIA_INT_MASK1(m)	(MCPCIA_CSR(m) + 0x680)
#define MCPCIA_INT_ACK0(m)	(MCPCIA_CSR(m) + 0x10003f00)
#define MCPCIA_INT_ACK1(m)	(MCPCIA_CSR(m) + 0x10003f40)

#define MCPCIA_PERF_MON(m)	(MCPCIA_CSR(m) + 0x300)
#define MCPCIA_PERF_CONT(m)	(MCPCIA_CSR(m) + 0x340)

#define MCPCIA_CAP_DIAG(m)	(MCPCIA_CSR(m) + 0x700)
#define MCPCIA_TOP_OF_MEM(m)	(MCPCIA_CSR(m) + 0x7C0)

#define MCPCIA_MC_ERR0(m)	(MCPCIA_CSR(m) + 0x800)
#define MCPCIA_MC_ERR1(m)	(MCPCIA_CSR(m) + 0x840)
#define MCPCIA_CAP_ERR(m)	(MCPCIA_CSR(m) + 0x880)
#define MCPCIA_PCI_ERR1(m)	(MCPCIA_CSR(m) + 0x1040)
#define MCPCIA_MDPA_STAT(m)	(MCPCIA_CSR(m) + 0x4000)
#define MCPCIA_MDPA_SYN(m)	(MCPCIA_CSR(m) + 0x4040)
#define MCPCIA_MDPA_DIAG(m)	(MCPCIA_CSR(m) + 0x4080)
#define MCPCIA_MDPB_STAT(m)	(MCPCIA_CSR(m) + 0x8000)
#define MCPCIA_MDPB_SYN(m)	(MCPCIA_CSR(m) + 0x8040)
#define MCPCIA_MDPB_DIAG(m)	(MCPCIA_CSR(m) + 0x8080)

#define MCPCIA_SG_TBIA(m)	(MCPCIA_CSR(m) + 0x1300)
#define MCPCIA_HBASE(m)		(MCPCIA_CSR(m) + 0x1340)

#define MCPCIA_W0_BASE(m)	(MCPCIA_CSR(m) + 0x1400)
#define MCPCIA_W0_MASK(m)	(MCPCIA_CSR(m) + 0x1440)
#define MCPCIA_T0_BASE(m)	(MCPCIA_CSR(m) + 0x1480)

#define MCPCIA_W1_BASE(m)	(MCPCIA_CSR(m) + 0x1500)
#define MCPCIA_W1_MASK(m)	(MCPCIA_CSR(m) + 0x1540)
#define MCPCIA_T1_BASE(m)	(MCPCIA_CSR(m) + 0x1580)

#define MCPCIA_W2_BASE(m)	(MCPCIA_CSR(m) + 0x1600)
#define MCPCIA_W2_MASK(m)	(MCPCIA_CSR(m) + 0x1640)
#define MCPCIA_T2_BASE(m)	(MCPCIA_CSR(m) + 0x1680)

#define MCPCIA_W3_BASE(m)	(MCPCIA_CSR(m) + 0x1700)
#define MCPCIA_W3_MASK(m)	(MCPCIA_CSR(m) + 0x1740)
#define MCPCIA_T3_BASE(m)	(MCPCIA_CSR(m) + 0x1780)


#ifndef MCPCIA_ONE_HAE_WINDOW
#define MCPCIA_HAE_ADDRESS	MCPCIA_HAE_MEM(4)
#endif
#define MCPCIA_IACK_SC		_MCPCIA_IACK_SC(4)


#define MCPCIA_IO_BIAS		MCPCIA_IO(4)
#define MCPCIA_MEM_BIAS		MCPCIA_DENSE(4)

#define MCPCIA_DAC_OFFSET	(1UL << 40)

struct el_MCPCIA_uncorrected_frame_mcheck {
	struct el_common header;
	struct el_common_EV5_uncorrectable_mcheck procdata;
};


#ifdef __KERNEL__

#ifndef __EXTERN_INLINE
#define __EXTERN_INLINE extern inline
#define __IO_EXTERN_INLINE
#endif



#define vip	volatile int __force *
#define vuip	volatile unsigned int __force *

#ifndef MCPCIA_ONE_HAE_WINDOW
#define MCPCIA_FROB_MMIO						\
	if (__mcpcia_is_mmio(hose)) {					\
		set_hae(hose & 0xffffffff);				\
		hose = hose - MCPCIA_DENSE(4) + MCPCIA_SPARSE(4);	\
	}
#else
#define MCPCIA_FROB_MMIO						\
	if (__mcpcia_is_mmio(hose)) {					\
		hose = hose - MCPCIA_DENSE(4) + MCPCIA_SPARSE(4);	\
	}
#endif

extern inline int __mcpcia_is_mmio(unsigned long addr)
{
	return (addr & 0x80000000UL) == 0;
}

__EXTERN_INLINE unsigned int mcpcia_ioread8(void __iomem *xaddr)
{
	unsigned long addr = (unsigned long)xaddr & MCPCIA_MEM_MASK;
	unsigned long hose = (unsigned long)xaddr & ~MCPCIA_MEM_MASK;
	unsigned long result;

	MCPCIA_FROB_MMIO;

	result = *(vip) ((addr << 5) + hose + 0x00);
	return __kernel_extbl(result, addr & 3);
}

__EXTERN_INLINE void mcpcia_iowrite8(u8 b, void __iomem *xaddr)
{
	unsigned long addr = (unsigned long)xaddr & MCPCIA_MEM_MASK;
	unsigned long hose = (unsigned long)xaddr & ~MCPCIA_MEM_MASK;
	unsigned long w;

	MCPCIA_FROB_MMIO;

	w = __kernel_insbl(b, addr & 3);
	*(vuip) ((addr << 5) + hose + 0x00) = w;
}

__EXTERN_INLINE unsigned int mcpcia_ioread16(void __iomem *xaddr)
{
	unsigned long addr = (unsigned long)xaddr & MCPCIA_MEM_MASK;
	unsigned long hose = (unsigned long)xaddr & ~MCPCIA_MEM_MASK;
	unsigned long result;

	MCPCIA_FROB_MMIO;

	result = *(vip) ((addr << 5) + hose + 0x08);
	return __kernel_extwl(result, addr & 3);
}

__EXTERN_INLINE void mcpcia_iowrite16(u16 b, void __iomem *xaddr)
{
	unsigned long addr = (unsigned long)xaddr & MCPCIA_MEM_MASK;
	unsigned long hose = (unsigned long)xaddr & ~MCPCIA_MEM_MASK;
	unsigned long w;

	MCPCIA_FROB_MMIO;

	w = __kernel_inswl(b, addr & 3);
	*(vuip) ((addr << 5) + hose + 0x08) = w;
}

__EXTERN_INLINE unsigned int mcpcia_ioread32(void __iomem *xaddr)
{
	unsigned long addr = (unsigned long)xaddr;

	if (!__mcpcia_is_mmio(addr))
		addr = ((addr & 0xffff) << 5) + (addr & ~0xfffful) + 0x18;

	return *(vuip)addr;
}

__EXTERN_INLINE void mcpcia_iowrite32(u32 b, void __iomem *xaddr)
{
	unsigned long addr = (unsigned long)xaddr;

	if (!__mcpcia_is_mmio(addr))
		addr = ((addr & 0xffff) << 5) + (addr & ~0xfffful) + 0x18;

	*(vuip)addr = b;
}


__EXTERN_INLINE void __iomem *mcpcia_ioportmap(unsigned long addr)
{
	return (void __iomem *)(addr + MCPCIA_IO_BIAS);
}

__EXTERN_INLINE void __iomem *mcpcia_ioremap(unsigned long addr,
					     unsigned long size)
{
	return (void __iomem *)(addr + MCPCIA_MEM_BIAS);
}

__EXTERN_INLINE int mcpcia_is_ioaddr(unsigned long addr)
{
	return addr >= MCPCIA_SPARSE(0);
}

__EXTERN_INLINE int mcpcia_is_mmio(const volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	return __mcpcia_is_mmio(addr);
}

#undef MCPCIA_FROB_MMIO

#undef vip
#undef vuip

#undef __IO_PREFIX
#define __IO_PREFIX		mcpcia
#define mcpcia_trivial_rw_bw	2
#define mcpcia_trivial_rw_lq	1
#define mcpcia_trivial_io_bw	0
#define mcpcia_trivial_io_lq	0
#define mcpcia_trivial_iounmap	1
#include <asm/io_trivial.h>

#ifdef __IO_EXTERN_INLINE
#undef __EXTERN_INLINE
#undef __IO_EXTERN_INLINE
#endif

#endif 

#endif 
