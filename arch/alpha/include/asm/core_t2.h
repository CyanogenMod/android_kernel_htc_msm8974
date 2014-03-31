#ifndef __ALPHA_T2__H__
#define __ALPHA_T2__H__

#define T2_ONE_HAE_WINDOW 1

#include <linux/types.h>
#include <linux/spinlock.h>
#include <asm/compiler.h>


#define T2_MEM_R1_MASK 0x07ffffff  

#define _GAMMA_BIAS		0x8000000000UL

#if defined(CONFIG_ALPHA_GENERIC)
#define GAMMA_BIAS		alpha_mv.sys.t2.gamma_bias
#elif defined(CONFIG_ALPHA_GAMMA)
#define GAMMA_BIAS		_GAMMA_BIAS
#else
#define GAMMA_BIAS		0
#endif

#define T2_CONF		        (IDENT_ADDR + GAMMA_BIAS + 0x390000000UL)
#define T2_IO			(IDENT_ADDR + GAMMA_BIAS + 0x3a0000000UL)
#define T2_SPARSE_MEM		(IDENT_ADDR + GAMMA_BIAS + 0x200000000UL)
#define T2_DENSE_MEM	        (IDENT_ADDR + GAMMA_BIAS + 0x3c0000000UL)

#define T2_IOCSR		(IDENT_ADDR + GAMMA_BIAS + 0x38e000000UL)
#define T2_CERR1		(IDENT_ADDR + GAMMA_BIAS + 0x38e000020UL)
#define T2_CERR2		(IDENT_ADDR + GAMMA_BIAS + 0x38e000040UL)
#define T2_CERR3		(IDENT_ADDR + GAMMA_BIAS + 0x38e000060UL)
#define T2_PERR1		(IDENT_ADDR + GAMMA_BIAS + 0x38e000080UL)
#define T2_PERR2		(IDENT_ADDR + GAMMA_BIAS + 0x38e0000a0UL)
#define T2_PSCR			(IDENT_ADDR + GAMMA_BIAS + 0x38e0000c0UL)
#define T2_HAE_1		(IDENT_ADDR + GAMMA_BIAS + 0x38e0000e0UL)
#define T2_HAE_2		(IDENT_ADDR + GAMMA_BIAS + 0x38e000100UL)
#define T2_HBASE		(IDENT_ADDR + GAMMA_BIAS + 0x38e000120UL)
#define T2_WBASE1		(IDENT_ADDR + GAMMA_BIAS + 0x38e000140UL)
#define T2_WMASK1		(IDENT_ADDR + GAMMA_BIAS + 0x38e000160UL)
#define T2_TBASE1		(IDENT_ADDR + GAMMA_BIAS + 0x38e000180UL)
#define T2_WBASE2		(IDENT_ADDR + GAMMA_BIAS + 0x38e0001a0UL)
#define T2_WMASK2		(IDENT_ADDR + GAMMA_BIAS + 0x38e0001c0UL)
#define T2_TBASE2		(IDENT_ADDR + GAMMA_BIAS + 0x38e0001e0UL)
#define T2_TLBBR		(IDENT_ADDR + GAMMA_BIAS + 0x38e000200UL)
#define T2_IVR			(IDENT_ADDR + GAMMA_BIAS + 0x38e000220UL)
#define T2_HAE_3		(IDENT_ADDR + GAMMA_BIAS + 0x38e000240UL)
#define T2_HAE_4		(IDENT_ADDR + GAMMA_BIAS + 0x38e000260UL)

#define T2_WBASE3		(IDENT_ADDR + GAMMA_BIAS + 0x38e000280UL)
#define T2_WMASK3		(IDENT_ADDR + GAMMA_BIAS + 0x38e0002a0UL)
#define T2_TBASE3		(IDENT_ADDR + GAMMA_BIAS + 0x38e0002c0UL)

#define T2_TDR0			(IDENT_ADDR + GAMMA_BIAS + 0x38e000300UL)
#define T2_TDR1			(IDENT_ADDR + GAMMA_BIAS + 0x38e000320UL)
#define T2_TDR2			(IDENT_ADDR + GAMMA_BIAS + 0x38e000340UL)
#define T2_TDR3			(IDENT_ADDR + GAMMA_BIAS + 0x38e000360UL)
#define T2_TDR4			(IDENT_ADDR + GAMMA_BIAS + 0x38e000380UL)
#define T2_TDR5			(IDENT_ADDR + GAMMA_BIAS + 0x38e0003a0UL)
#define T2_TDR6			(IDENT_ADDR + GAMMA_BIAS + 0x38e0003c0UL)
#define T2_TDR7			(IDENT_ADDR + GAMMA_BIAS + 0x38e0003e0UL)

#define T2_WBASE4		(IDENT_ADDR + GAMMA_BIAS + 0x38e000400UL)
#define T2_WMASK4		(IDENT_ADDR + GAMMA_BIAS + 0x38e000420UL)
#define T2_TBASE4		(IDENT_ADDR + GAMMA_BIAS + 0x38e000440UL)

#define T2_AIR			(IDENT_ADDR + GAMMA_BIAS + 0x38e000460UL)
#define T2_VAR			(IDENT_ADDR + GAMMA_BIAS + 0x38e000480UL)
#define T2_DIR			(IDENT_ADDR + GAMMA_BIAS + 0x38e0004a0UL)
#define T2_ICE			(IDENT_ADDR + GAMMA_BIAS + 0x38e0004c0UL)

#ifndef T2_ONE_HAE_WINDOW
#define T2_HAE_ADDRESS		T2_HAE_1
#endif

#define T2_CPU0_BASE            (IDENT_ADDR + GAMMA_BIAS + 0x380000000L)
#define T2_CPU1_BASE            (IDENT_ADDR + GAMMA_BIAS + 0x381000000L)
#define T2_CPU2_BASE            (IDENT_ADDR + GAMMA_BIAS + 0x382000000L)
#define T2_CPU3_BASE            (IDENT_ADDR + GAMMA_BIAS + 0x383000000L)

#define T2_CPUn_BASE(n)		(T2_CPU0_BASE + (((n)&3) * 0x001000000L))

#define T2_MEM0_BASE            (IDENT_ADDR + GAMMA_BIAS + 0x388000000L)
#define T2_MEM1_BASE            (IDENT_ADDR + GAMMA_BIAS + 0x389000000L)
#define T2_MEM2_BASE            (IDENT_ADDR + GAMMA_BIAS + 0x38a000000L)
#define T2_MEM3_BASE            (IDENT_ADDR + GAMMA_BIAS + 0x38b000000L)



struct sable_cpu_csr {
  unsigned long bcc;     long fill_00[3]; 
  unsigned long bcce;    long fill_01[3]; 
  unsigned long bccea;   long fill_02[3]; 
  unsigned long bcue;    long fill_03[3]; 
  unsigned long bcuea;   long fill_04[3]; 
  unsigned long dter;    long fill_05[3]; 
  unsigned long cbctl;   long fill_06[3]; 
  unsigned long cbe;     long fill_07[3]; 
  unsigned long cbeal;   long fill_08[3]; 
  unsigned long cbeah;   long fill_09[3]; 
  unsigned long pmbx;    long fill_10[3]; 
  unsigned long ipir;    long fill_11[3]; 
  unsigned long sic;     long fill_12[3]; 
  unsigned long adlk;    long fill_13[3]; 
  unsigned long madrl;   long fill_14[3]; 
  unsigned long rev;     long fill_15[3]; 
};

struct el_t2_frame_header {
	unsigned int	elcf_fid;	
	unsigned int	elcf_size;	
};

struct el_t2_procdata_mcheck {
	unsigned long	elfmc_paltemp[32];	
	
	unsigned long	elfmc_exc_addr;	
	unsigned long	elfmc_exc_sum;	
	unsigned long	elfmc_exc_mask;	
	unsigned long	elfmc_iccsr;	
	unsigned long	elfmc_pal_base;	
	unsigned long	elfmc_hier;	
	unsigned long	elfmc_hirr;	
	unsigned long	elfmc_mm_csr;	
	unsigned long	elfmc_dc_stat;	
	unsigned long	elfmc_dc_addr;	
	unsigned long	elfmc_abox_ctl;	
	unsigned long	elfmc_biu_stat;	
	unsigned long	elfmc_biu_addr;	
	unsigned long	elfmc_biu_ctl;	
	unsigned long	elfmc_fill_syndrome; 
	unsigned long	elfmc_fill_addr;
	unsigned long	elfmc_va;	
	unsigned long	elfmc_bc_tag;	
};


struct el_t2_logout_header {
	unsigned int	elfl_size;	
	unsigned int	elfl_sbz1:31;	
	unsigned int	elfl_retry:1;	
	unsigned int	elfl_procoffset; 
	unsigned int	elfl_sysoffset;	 
	unsigned int	elfl_error_type;	
	unsigned int	elfl_frame_rev;		
};
struct el_t2_sysdata_mcheck {
	unsigned long    elcmc_bcc;	      
	unsigned long    elcmc_bcce;	      
	unsigned long    elcmc_bccea;      
	unsigned long    elcmc_bcue;	      
	unsigned long    elcmc_bcuea;      
	unsigned long    elcmc_dter;	      
	unsigned long    elcmc_cbctl;      
	unsigned long    elcmc_cbe;	      
	unsigned long    elcmc_cbeal;      
	unsigned long    elcmc_cbeah;      
	unsigned long    elcmc_pmbx;	      
	unsigned long    elcmc_ipir;	      
	unsigned long    elcmc_sic;	      
	unsigned long    elcmc_adlk;	      
	unsigned long    elcmc_madrl;      
	unsigned long    elcmc_crrev4;     
};

struct el_t2_data_memory {
	struct	el_t2_frame_header elcm_hdr;	
	unsigned int  elcm_module;	
	unsigned int  elcm_res04;	
	unsigned long elcm_merr;	
	unsigned long elcm_mcmd1;	
	unsigned long elcm_mcmd2;	
	unsigned long elcm_mconf;	
	unsigned long elcm_medc1;	
	unsigned long elcm_medc2;	
	unsigned long elcm_medcc;	
	unsigned long elcm_msctl;	
	unsigned long elcm_mref;	
	unsigned long elcm_filter;	
};


struct el_t2_data_other_cpu {
	short	      elco_cpuid;	
	short	      elco_res02[3];
	unsigned long elco_bcc;	
	unsigned long elco_bcce;	
	unsigned long elco_bccea;	
	unsigned long elco_bcue;	
	unsigned long elco_bcuea;	
	unsigned long elco_dter;	
	unsigned long elco_cbctl;	
	unsigned long elco_cbe;	
	unsigned long elco_cbeal;	
	unsigned long elco_cbeah;	
	unsigned long elco_pmbx;	
	unsigned long elco_ipir;	
	unsigned long elco_sic;	
	unsigned long elco_adlk;	
	unsigned long elco_madrl;	
	unsigned long elco_crrev4;	
};

struct el_t2_data_t2{
	struct el_t2_frame_header elct_hdr;	
	unsigned long elct_iocsr;	
	unsigned long elct_cerr1;	
	unsigned long elct_cerr2;	
	unsigned long elct_cerr3;	
	unsigned long elct_perr1;	
	unsigned long elct_perr2;	
	unsigned long elct_hae0_1;	
	unsigned long elct_hae0_2;	
	unsigned long elct_hbase;	
	unsigned long elct_wbase1;	
	unsigned long elct_wmask1;	
	unsigned long elct_tbase1;	
	unsigned long elct_wbase2;	
	unsigned long elct_wmask2;	
	unsigned long elct_tbase2;	
	unsigned long elct_tdr0;	
	unsigned long elct_tdr1;	
	unsigned long elct_tdr2;	
	unsigned long elct_tdr3;	
	unsigned long elct_tdr4;	
	unsigned long elct_tdr5;	
	unsigned long elct_tdr6;	
	unsigned long elct_tdr7;	
};

struct el_t2_data_corrected {
	unsigned long elcpb_biu_stat;
	unsigned long elcpb_biu_addr;
	unsigned long elcpb_biu_ctl;
	unsigned long elcpb_fill_syndrome;
	unsigned long elcpb_fill_addr;
	unsigned long elcpb_bc_tag;
};

struct el_t2_frame_mcheck {
	struct el_t2_frame_header elfmc_header;	
	struct el_t2_logout_header elfmc_hdr;
	struct el_t2_procdata_mcheck elfmc_procdata;
	struct el_t2_sysdata_mcheck elfmc_sysdata;
	struct el_t2_data_t2 elfmc_t2data;
	struct el_t2_data_memory elfmc_memdata[4];
	struct el_t2_frame_header elfmc_footer;	
};


struct el_t2_frame_corrected {
	struct el_t2_frame_header elfcc_header;	
	struct el_t2_logout_header elfcc_hdr;
	struct el_t2_data_corrected elfcc_procdata;
	struct el_t2_frame_header elfcc_footer;	
};


#ifdef __KERNEL__

#ifndef __EXTERN_INLINE
#define __EXTERN_INLINE extern inline
#define __IO_EXTERN_INLINE
#endif


#define vip	volatile int *
#define vuip	volatile unsigned int *

extern inline u8 t2_inb(unsigned long addr)
{
	long result = *(vip) ((addr << 5) + T2_IO + 0x00);
	return __kernel_extbl(result, addr & 3);
}

extern inline void t2_outb(u8 b, unsigned long addr)
{
	unsigned long w;

	w = __kernel_insbl(b, addr & 3);
	*(vuip) ((addr << 5) + T2_IO + 0x00) = w;
	mb();
}

extern inline u16 t2_inw(unsigned long addr)
{
	long result = *(vip) ((addr << 5) + T2_IO + 0x08);
	return __kernel_extwl(result, addr & 3);
}

extern inline void t2_outw(u16 b, unsigned long addr)
{
	unsigned long w;

	w = __kernel_inswl(b, addr & 3);
	*(vuip) ((addr << 5) + T2_IO + 0x08) = w;
	mb();
}

extern inline u32 t2_inl(unsigned long addr)
{
	return *(vuip) ((addr << 5) + T2_IO + 0x18);
}

extern inline void t2_outl(u32 b, unsigned long addr)
{
	*(vuip) ((addr << 5) + T2_IO + 0x18) = b;
	mb();
}



#ifdef T2_ONE_HAE_WINDOW
#define t2_set_hae
#else
#define t2_set_hae { \
	unsigned long msb = addr >> 27; \
	addr &= T2_MEM_R1_MASK; \
	set_hae(msb); \
}
#endif


__EXTERN_INLINE u8 t2_readb(const volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr - T2_DENSE_MEM;
	unsigned long result;

	t2_set_hae;

	result = *(vip) ((addr << 5) + T2_SPARSE_MEM + 0x00);
	return __kernel_extbl(result, addr & 3);
}

__EXTERN_INLINE u16 t2_readw(const volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr - T2_DENSE_MEM;
	unsigned long result;

	t2_set_hae;

	result = *(vuip) ((addr << 5) + T2_SPARSE_MEM + 0x08);
	return __kernel_extwl(result, addr & 3);
}

__EXTERN_INLINE u32 t2_readl(const volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr - T2_DENSE_MEM;
	unsigned long result;

	t2_set_hae;

	result = *(vuip) ((addr << 5) + T2_SPARSE_MEM + 0x18);
	return result & 0xffffffffUL;
}

__EXTERN_INLINE u64 t2_readq(const volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr - T2_DENSE_MEM;
	unsigned long r0, r1, work;

	t2_set_hae;

	work = (addr << 5) + T2_SPARSE_MEM + 0x18;
	r0 = *(vuip)(work);
	r1 = *(vuip)(work + (4 << 5));
	return r1 << 32 | r0;
}

__EXTERN_INLINE void t2_writeb(u8 b, volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr - T2_DENSE_MEM;
	unsigned long w;

	t2_set_hae;

	w = __kernel_insbl(b, addr & 3);
	*(vuip) ((addr << 5) + T2_SPARSE_MEM + 0x00) = w;
}

__EXTERN_INLINE void t2_writew(u16 b, volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr - T2_DENSE_MEM;
	unsigned long w;

	t2_set_hae;

	w = __kernel_inswl(b, addr & 3);
	*(vuip) ((addr << 5) + T2_SPARSE_MEM + 0x08) = w;
}

__EXTERN_INLINE void t2_writel(u32 b, volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr - T2_DENSE_MEM;

	t2_set_hae;

	*(vuip) ((addr << 5) + T2_SPARSE_MEM + 0x18) = b;
}

__EXTERN_INLINE void t2_writeq(u64 b, volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr - T2_DENSE_MEM;
	unsigned long work;

	t2_set_hae;

	work = (addr << 5) + T2_SPARSE_MEM + 0x18;
	*(vuip)work = b;
	*(vuip)(work + (4 << 5)) = b >> 32;
}

__EXTERN_INLINE void __iomem *t2_ioportmap(unsigned long addr)
{
	return (void __iomem *)(addr + T2_IO);
}

__EXTERN_INLINE void __iomem *t2_ioremap(unsigned long addr, 
					 unsigned long size)
{
	return (void __iomem *)(addr + T2_DENSE_MEM);
}

__EXTERN_INLINE int t2_is_ioaddr(unsigned long addr)
{
	return (long)addr >= 0;
}

__EXTERN_INLINE int t2_is_mmio(const volatile void __iomem *addr)
{
	return (unsigned long)addr >= T2_DENSE_MEM;
}


#define IOPORT(OS, NS)							\
__EXTERN_INLINE unsigned int t2_ioread##NS(void __iomem *xaddr)		\
{									\
	if (t2_is_mmio(xaddr))						\
		return t2_read##OS(xaddr);				\
	else								\
		return t2_in##OS((unsigned long)xaddr - T2_IO);		\
}									\
__EXTERN_INLINE void t2_iowrite##NS(u##NS b, void __iomem *xaddr)	\
{									\
	if (t2_is_mmio(xaddr))						\
		t2_write##OS(b, xaddr);					\
	else								\
		t2_out##OS(b, (unsigned long)xaddr - T2_IO);		\
}

IOPORT(b, 8)
IOPORT(w, 16)
IOPORT(l, 32)

#undef IOPORT

#undef vip
#undef vuip

#undef __IO_PREFIX
#define __IO_PREFIX		t2
#define t2_trivial_rw_bw	0
#define t2_trivial_rw_lq	0
#define t2_trivial_io_bw	0
#define t2_trivial_io_lq	0
#define t2_trivial_iounmap	1
#include <asm/io_trivial.h>

#ifdef __IO_EXTERN_INLINE
#undef __EXTERN_INLINE
#undef __IO_EXTERN_INLINE
#endif

#endif 

#endif 
