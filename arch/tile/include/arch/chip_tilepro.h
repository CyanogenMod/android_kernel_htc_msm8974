/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */


#ifndef __ARCH_CHIP_H__
#define __ARCH_CHIP_H__

#define TILE_CHIP 1

#define TILE_CHIP_REV 0

#define CHIP_ARCH_NAME "tilepro"

#define CHIP_ELF_TYPE() EM_TILEPRO

#define CHIP_COMPAT_ELF_TYPE() 0x2507

#define CHIP_WORD_SIZE() 32

#define CHIP_VA_WIDTH() 32

#define CHIP_PA_WIDTH() 36

#define CHIP_L2_CACHE_SIZE() 65536

#define CHIP_L2_LOG_LINE_SIZE() 6

#define CHIP_L2_LINE_SIZE() (1 << CHIP_L2_LOG_LINE_SIZE())

#define CHIP_L2_ASSOC() 4

#define CHIP_L1D_CACHE_SIZE() 8192

#define CHIP_L1D_LOG_LINE_SIZE() 4

#define CHIP_L1D_LINE_SIZE() (1 << CHIP_L1D_LOG_LINE_SIZE())

#define CHIP_L1D_ASSOC() 2

#define CHIP_L1I_CACHE_SIZE() 16384

#define CHIP_L1I_LOG_LINE_SIZE() 6

#define CHIP_L1I_LINE_SIZE() (1 << CHIP_L1I_LOG_LINE_SIZE())

#define CHIP_L1I_ASSOC() 1

#define CHIP_FLUSH_STRIDE() CHIP_L2_LINE_SIZE()

#define CHIP_INV_STRIDE() CHIP_L2_LINE_SIZE()

#define CHIP_FINV_STRIDE() CHIP_L2_LINE_SIZE()

#define CHIP_HAS_COHERENT_LOCAL_CACHE() 1

#define CHIP_MAX_OUTSTANDING_VICTIMS() 4

#define CHIP_HAS_NC_AND_NOALLOC_BITS() 1

#define CHIP_HAS_CBOX_HOME_MAP() 1

#define CHIP_CBOX_HOME_MAP_SIZE() 64

#define CHIP_HAS_ENFORCED_UNCACHEABLE_REQUESTS() 1

#define CHIP_HAS_MF_WAITS_FOR_VICTIMS() 0

#define CHIP_HAS_INV() 1

#define CHIP_HAS_WH64() 1

#define CHIP_HAS_DWORD_ALIGN() 1

#define CHIP_PERFORMANCE_COUNTERS() 4

#define CHIP_HAS_AUX_PERF_COUNTERS() 1

#define CHIP_HAS_CBOX_MSR1() 1

#define CHIP_HAS_TILE_RTF_HWM() 1

#define CHIP_HAS_TILE_WRITE_PENDING() 1

#define CHIP_HAS_PROC_STATUS_SPR() 1

#define CHIP_HAS_DSTREAM_PF() 0

#define CHIP_LOG_NUM_MSHIMS() 2

#define CHIP_HAS_FIXED_INTVEC_BASE() 1

#define CHIP_HAS_SPLIT_INTR_MASK() 1

#define CHIP_HAS_SPLIT_CYCLE() 1

#define CHIP_HAS_SN() 1

#define CHIP_HAS_SN_PROC() 0


#define CHIP_HAS_TILE_DMA() 1

#define CHIP_HAS_REV1_XDN() 0

#define CHIP_HAS_CMPEXCH() 0

#define CHIP_HAS_MMIO() 0

#define CHIP_HAS_POST_COMPLETION_INTERRUPTS() 0

#define CHIP_HAS_SINGLE_STEP() 0

#ifndef __OPEN_SOURCE__  

#define CHIP_ITLB_ENTRIES() 16

#define CHIP_DTLB_ENTRIES() 16

#define CHIP_XAUI_MAF_ENTRIES() 32

#define CHIP_HAS_MSHIM_SRCID_TABLE() 0

#define CHIP_HAS_L1I_CLEAR_ON_RESET() 1

#define CHIP_HAS_VALID_TILE_COORD_RESET() 1

#define CHIP_HAS_UNIFIED_PACKET_FORMATS() 1

#define CHIP_HAS_WRITE_REORDERING() 1

#define CHIP_HAS_Y_X_ROUTING() 1

#define CHIP_HAS_INTCTRL_3_STATUS_FIX() 1

#define CHIP_HAS_BIG_ENDIAN_CONFIG() 1

#define CHIP_HAS_CACHE_RED_WAY_OVERRIDDEN() 1

#define CHIP_HAS_DIAG_TRACE_WAY() 1

#define CHIP_HAS_MEM_STRIPE_CONFIG() 1

#define CHIP_HAS_TLB_PERF() 1

#define CHIP_HAS_VDN_SNOOP_SHIM_CTL() 1

#define CHIP_HAS_REV1_DMA_PACKETS() 1

#define CHIP_HAS_IPI() 0

#endif 
#endif 
