/*
 *	linux/arch/alpha/kernel/machvec_impl.h
 *
 *	Copyright (C) 1997, 1998  Richard Henderson
 *
 * This file has goodies to help simplify instantiation of machine vectors.
 */

#include <asm/pgalloc.h>

#define IRONGATE_HAE_ADDRESS	(&alpha_mv.hae_cache)
#define MARVEL_HAE_ADDRESS	(&alpha_mv.hae_cache)
#define POLARIS_HAE_ADDRESS	(&alpha_mv.hae_cache)
#define TSUNAMI_HAE_ADDRESS	(&alpha_mv.hae_cache)
#define TITAN_HAE_ADDRESS	(&alpha_mv.hae_cache)
#define WILDFIRE_HAE_ADDRESS	(&alpha_mv.hae_cache)

#ifdef CIA_ONE_HAE_WINDOW
#define CIA_HAE_ADDRESS		(&alpha_mv.hae_cache)
#endif
#ifdef MCPCIA_ONE_HAE_WINDOW
#define MCPCIA_HAE_ADDRESS	(&alpha_mv.hae_cache)
#endif
#ifdef T2_ONE_HAE_WINDOW
#define T2_HAE_ADDRESS		(&alpha_mv.hae_cache)
#endif

#define JENSEN_IACK_SC		1
#define T2_IACK_SC		1
#define WILDFIRE_IACK_SC	1 


#define CAT1(x,y)  x##y
#define CAT(x,y)   CAT1(x,y)

#define DO_DEFAULT_RTC \
	.rtc_port = 0x70, \
	.rtc_get_time = common_get_rtc_time, \
	.rtc_set_time = common_set_rtc_time

#define DO_EV4_MMU							\
	.max_asn =			EV4_MAX_ASN,			\
	.mv_switch_mm =			ev4_switch_mm,			\
	.mv_activate_mm =		ev4_activate_mm,		\
	.mv_flush_tlb_current =		ev4_flush_tlb_current,		\
	.mv_flush_tlb_current_page =	ev4_flush_tlb_current_page

#define DO_EV5_MMU							\
	.max_asn =			EV5_MAX_ASN,			\
	.mv_switch_mm =			ev5_switch_mm,			\
	.mv_activate_mm =		ev5_activate_mm,		\
	.mv_flush_tlb_current =		ev5_flush_tlb_current,		\
	.mv_flush_tlb_current_page =	ev5_flush_tlb_current_page

#define DO_EV6_MMU							\
	.max_asn =			EV6_MAX_ASN,			\
	.mv_switch_mm =			ev5_switch_mm,			\
	.mv_activate_mm =		ev5_activate_mm,		\
	.mv_flush_tlb_current =		ev5_flush_tlb_current,		\
	.mv_flush_tlb_current_page =	ev5_flush_tlb_current_page

#define DO_EV7_MMU							\
	.max_asn =			EV6_MAX_ASN,			\
	.mv_switch_mm =			ev5_switch_mm,			\
	.mv_activate_mm =		ev5_activate_mm,		\
	.mv_flush_tlb_current =		ev5_flush_tlb_current,		\
	.mv_flush_tlb_current_page =	ev5_flush_tlb_current_page

#define IO_LITE(UP,low)							\
	.hae_register =		(unsigned long *) CAT(UP,_HAE_ADDRESS),	\
	.iack_sc =		CAT(UP,_IACK_SC),			\
	.mv_ioread8 =		CAT(low,_ioread8),			\
	.mv_ioread16 =		CAT(low,_ioread16),			\
	.mv_ioread32 =		CAT(low,_ioread32),			\
	.mv_iowrite8 =		CAT(low,_iowrite8),			\
	.mv_iowrite16 =		CAT(low,_iowrite16),			\
	.mv_iowrite32 =		CAT(low,_iowrite32),			\
	.mv_readb =		CAT(low,_readb),			\
	.mv_readw =		CAT(low,_readw),			\
	.mv_readl =		CAT(low,_readl),			\
	.mv_readq =		CAT(low,_readq),			\
	.mv_writeb =		CAT(low,_writeb),			\
	.mv_writew =		CAT(low,_writew),			\
	.mv_writel =		CAT(low,_writel),			\
	.mv_writeq =		CAT(low,_writeq),			\
	.mv_ioportmap =		CAT(low,_ioportmap),			\
	.mv_ioremap =		CAT(low,_ioremap),			\
	.mv_iounmap =		CAT(low,_iounmap),			\
	.mv_is_ioaddr =		CAT(low,_is_ioaddr),			\
	.mv_is_mmio =		CAT(low,_is_mmio)			\

#define IO(UP,low)							\
	IO_LITE(UP,low),						\
	.pci_ops =		&CAT(low,_pci_ops),			\
	.mv_pci_tbi =		CAT(low,_pci_tbi)

#define DO_APECS_IO	IO(APECS,apecs)
#define DO_CIA_IO	IO(CIA,cia)
#define DO_IRONGATE_IO	IO(IRONGATE,irongate)
#define DO_LCA_IO	IO(LCA,lca)
#define DO_MARVEL_IO	IO(MARVEL,marvel)
#define DO_MCPCIA_IO	IO(MCPCIA,mcpcia)
#define DO_POLARIS_IO	IO(POLARIS,polaris)
#define DO_T2_IO	IO(T2,t2)
#define DO_TSUNAMI_IO	IO(TSUNAMI,tsunami)
#define DO_TITAN_IO	IO(TITAN,titan)
#define DO_WILDFIRE_IO	IO(WILDFIRE,wildfire)

#define DO_PYXIS_IO	IO_LITE(CIA,cia_bwx), \
			.pci_ops = &cia_pci_ops, \
			.mv_pci_tbi = cia_pci_tbi


#ifdef CONFIG_ALPHA_GENERIC
#define __initmv __initdata
#define ALIAS_MV(x)
#else
#define __initmv __initdata_refok

#if 0
#define ALIAS_MV(system) \
  struct alpha_machine_vector alpha_mv __attribute__((alias(#system "_mv")));
#else
#define ALIAS_MV(system) \
  asm(".global alpha_mv\nalpha_mv = " #system "_mv");
#endif
#endif 
