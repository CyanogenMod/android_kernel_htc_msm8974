/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1992-1999,2001-2005 Silicon Graphics, Inc. All rights reserved.
 */

#ifndef _ASM_IA64_SN_ADDRS_H
#define _ASM_IA64_SN_ADDRS_H

#include <asm/percpu.h>
#include <asm/sn/types.h>
#include <asm/sn/arch.h>
#include <asm/sn/pda.h>



#define NASID_BITMASK		(sn_hub_info->nasid_bitmask)
#define NASID_SHIFT		(sn_hub_info->nasid_shift)
#define AS_SHIFT		(sn_hub_info->as_shift)
#define AS_BITMASK		0x3UL

#define NASID_MASK              ((u64)NASID_BITMASK << NASID_SHIFT)
#define AS_MASK			((u64)AS_BITMASK << AS_SHIFT)


#define AS_GET_VAL		1UL
#define AS_AMO_VAL		2UL
#define AS_CAC_VAL		3UL
#define AS_GET_SPACE		(AS_GET_VAL << AS_SHIFT)
#define AS_AMO_SPACE		(AS_AMO_VAL << AS_SHIFT)
#define AS_CAC_SPACE		(AS_CAC_VAL << AS_SHIFT)


#define SH1_LOCAL_MMR_OFFSET	0x8000000000UL
#define SH2_LOCAL_MMR_OFFSET	0x0200000000UL
#define LOCAL_MMR_OFFSET	(is_shub2() ? SH2_LOCAL_MMR_OFFSET : SH1_LOCAL_MMR_OFFSET)
#define LOCAL_MMR_SPACE		(__IA64_UNCACHED_OFFSET | LOCAL_MMR_OFFSET)
#define LOCAL_PHYS_MMR_SPACE	(RGN_BASE(RGN_HPAGE) | LOCAL_MMR_OFFSET)

#define SH1_GLOBAL_MMR_OFFSET	0x0800000000UL
#define SH2_GLOBAL_MMR_OFFSET	0x0300000000UL
#define GLOBAL_MMR_OFFSET	(is_shub2() ? SH2_GLOBAL_MMR_OFFSET : SH1_GLOBAL_MMR_OFFSET)
#define GLOBAL_MMR_SPACE	(__IA64_UNCACHED_OFFSET | GLOBAL_MMR_OFFSET)

#define GLOBAL_PHYS_MMR_SPACE	(RGN_BASE(RGN_HPAGE) | GLOBAL_MMR_OFFSET)


#define TO_PHYS_MASK		(~(RGN_BITS | AS_MASK))


#define NASID_SPACE(n)		((u64)(n) << NASID_SHIFT)
#define REMOTE_ADDR(n,a)	(NASID_SPACE(n) | (a))
#define NODE_OFFSET(x)		((x) & (NODE_ADDRSPACE_SIZE - 1))
#define NODE_ADDRSPACE_SIZE     (1UL << AS_SHIFT)
#define NASID_GET(x)		(int) (((u64) (x) >> NASID_SHIFT) & NASID_BITMASK)
#define LOCAL_MMR_ADDR(a)	(LOCAL_MMR_SPACE | (a))
#define GLOBAL_MMR_ADDR(n,a)	(GLOBAL_MMR_SPACE | REMOTE_ADDR(n,a))
#define GLOBAL_MMR_PHYS_ADDR(n,a) (GLOBAL_PHYS_MMR_SPACE | REMOTE_ADDR(n,a))
#define GLOBAL_CAC_ADDR(n,a)	(CAC_BASE | REMOTE_ADDR(n,a))
#define CHANGE_NASID(n,x)	((void *)(((u64)(x) & ~NASID_MASK) | NASID_SPACE(n)))
#define IS_TIO_NASID(n)		((n) & 1)


#define BWIN_TOP		0x0000000100000000UL

#define CAC_BASE		(PAGE_OFFSET | AS_CAC_SPACE)
#define AMO_BASE		(__IA64_UNCACHED_OFFSET | AS_AMO_SPACE)
#define AMO_PHYS_BASE		(RGN_BASE(RGN_HPAGE) | AS_AMO_SPACE)
#define GET_BASE		(PAGE_OFFSET | AS_GET_SPACE)

#define TO_PHYS(x)		(TO_PHYS_MASK & (x))
#define TO_CAC(x)		(CAC_BASE     | TO_PHYS(x))
#ifdef CONFIG_SGI_SN
#define TO_AMO(x)		(AMO_BASE     | TO_PHYS(x))
#define TO_GET(x)		(GET_BASE     | TO_PHYS(x))
#else
#define TO_AMO(x)		({ BUG(); x; })
#define TO_GET(x)		({ BUG(); x; })
#endif

#define SH1_TIO_PHYS_TO_DMA(x) 						\
	((((u64)(NASID_GET(x))) << 40) | NODE_OFFSET(x))

#define SH2_NETWORK_BANK_OFFSET(x) 					\
        ((u64)(x) & ((1UL << (sn_hub_info->nasid_shift - 4)) -1))

#define SH2_NETWORK_BANK_SELECT(x) 					\
        ((((u64)(x) & (0x3UL << (sn_hub_info->nasid_shift - 4)))	\
        	>> (sn_hub_info->nasid_shift - 4)) << 36)

#define SH2_NETWORK_ADDRESS(x) 						\
	(SH2_NETWORK_BANK_OFFSET(x) | SH2_NETWORK_BANK_SELECT(x))

#define SH2_TIO_PHYS_TO_DMA(x) 						\
        (((u64)(NASID_GET(x)) << 40) | 	SH2_NETWORK_ADDRESS(x))

#define PHYS_TO_TIODMA(x)						\
	(is_shub1() ? SH1_TIO_PHYS_TO_DMA(x) : SH2_TIO_PHYS_TO_DMA(x))

#define PHYS_TO_DMA(x)							\
	((((u64)(x) & NASID_MASK) >> 2) | NODE_OFFSET(x))


#define IS_AMO_ADDRESS(x)	(((u64)(x) & (RGN_BITS | AS_MASK)) == AMO_BASE)
#define IS_AMO_PHYS_ADDRESS(x)	(((u64)(x) & (RGN_BITS | AS_MASK)) == AMO_PHYS_BASE)


#define BWIN_SIZE_BITS			29	
#define TIO_BWIN_SIZE_BITS		30	
#define NODE_SWIN_BASE(n, w)		((w == 0) ? NODE_BWIN_BASE((n), SWIN0_BIGWIN) \
		: RAW_NODE_SWIN_BASE(n, w))
#define TIO_SWIN_BASE(n, w) 		(TIO_IO_BASE(n) + \
					    ((u64) (w) << TIO_SWIN_SIZE_BITS))
#define NODE_IO_BASE(n)			(GLOBAL_MMR_SPACE | NASID_SPACE(n))
#define TIO_IO_BASE(n)                  (__IA64_UNCACHED_OFFSET | NASID_SPACE(n))
#define BWIN_SIZE			(1UL << BWIN_SIZE_BITS)
#define NODE_BWIN_BASE0(n)		(NODE_IO_BASE(n) + BWIN_SIZE)
#define NODE_BWIN_BASE(n, w)		(NODE_BWIN_BASE0(n) + ((u64) (w) << BWIN_SIZE_BITS))
#define RAW_NODE_SWIN_BASE(n, w)	(NODE_IO_BASE(n) + ((u64) (w) << SWIN_SIZE_BITS))
#define BWIN_WIDGET_MASK		0x7
#define BWIN_WINDOWNUM(x)		(((x) >> BWIN_SIZE_BITS) & BWIN_WIDGET_MASK)
#define SH1_IS_BIG_WINDOW_ADDR(x)	((x) & BWIN_TOP)

#define TIO_BWIN_WINDOW_SELECT_MASK	0x7
#define TIO_BWIN_WINDOWNUM(x)		(((x) >> TIO_BWIN_SIZE_BITS) & TIO_BWIN_WINDOW_SELECT_MASK)

#define TIO_HWIN_SHIFT_BITS		33
#define TIO_HWIN(x)			(NODE_OFFSET(x) >> TIO_HWIN_SHIFT_BITS)


#define SWIN_SIZE_BITS			24
#define	SWIN_WIDGET_MASK		0xF

#define TIO_SWIN_SIZE_BITS		28
#define TIO_SWIN_SIZE			(1UL << TIO_SWIN_SIZE_BITS)
#define TIO_SWIN_WIDGET_MASK		0x3

#define	SWIN_WIDGETNUM(x)		(((x)  >> SWIN_SIZE_BITS) & SWIN_WIDGET_MASK)
#define TIO_SWIN_WIDGETNUM(x)		(((x)  >> TIO_SWIN_SIZE_BITS) & TIO_SWIN_WIDGET_MASK)


#define SH1_TIO_IOSPACE_ADDR(n,x)					\
	GLOBAL_MMR_ADDR(n,x)

#define SH1_REMOTE_BWIN_MMR(n,x)					\
	GLOBAL_MMR_ADDR(n,x)

#define SH1_REMOTE_SWIN_MMR(n,x)					\
	(NODE_SWIN_BASE(n,1) + 0x800000UL + (x))

#define SH1_REMOTE_MMR(n,x)						\
	(SH1_IS_BIG_WINDOW_ADDR(x) ? SH1_REMOTE_BWIN_MMR(n,x) :		\
	 	SH1_REMOTE_SWIN_MMR(n,x))

#define SH2_TIO_IOSPACE_ADDR(n,x)					\
	((__IA64_UNCACHED_OFFSET | REMOTE_ADDR(n,x) | 1UL << (NASID_SHIFT - 2)))

#define SH2_REMOTE_MMR(n,x)						\
	GLOBAL_MMR_ADDR(n,x)


#define TIO_IOSPACE_ADDR(n,x)						\
	((u64 *)(is_shub1() ? SH1_TIO_IOSPACE_ADDR(n,x) :		\
		 SH2_TIO_IOSPACE_ADDR(n,x)))

#define SH_REMOTE_MMR(n,x)						\
	(is_shub1() ? SH1_REMOTE_MMR(n,x) : SH2_REMOTE_MMR(n,x))

#define REMOTE_HUB_ADDR(n,x)						\
	(IS_TIO_NASID(n) ?  ((volatile u64*)TIO_IOSPACE_ADDR(n,x)) :	\
	 ((volatile u64*)SH_REMOTE_MMR(n,x)))


#define HUB_L(x)			(*((volatile typeof(*x) *)x))
#define	HUB_S(x,d)			(*((volatile typeof(*x) *)x) = (d))

#define REMOTE_HUB_L(n, a)		HUB_L(REMOTE_HUB_ADDR((n), (a)))
#define REMOTE_HUB_S(n, a, d)		HUB_S(REMOTE_HUB_ADDR((n), (a)), (d))

#define CTALK_NASID_SHFT		40
#define CTALK_NASID_MASK		(0x3FFFULL << CTALK_NASID_SHFT)
#define CTALK_CID_SHFT			38
#define CTALK_CID_MASK			(0x3ULL << CTALK_CID_SHFT)
#define CTALK_NODE_OFFSET		0x3FFFFFFFFF

#endif 
