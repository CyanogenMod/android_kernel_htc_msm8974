/***********************license start***************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2009 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Networks for more information
 ***********************license end**************************************/

#ifndef __CVMX_ADDRESS_H__
#define __CVMX_ADDRESS_H__

#if 0
typedef enum {
	CVMX_MIPS_SPACE_XKSEG = 3LL,
	CVMX_MIPS_SPACE_XKPHYS = 2LL,
	CVMX_MIPS_SPACE_XSSEG = 1LL,
	CVMX_MIPS_SPACE_XUSEG = 0LL
} cvmx_mips_space_t;
#endif

typedef enum {
	CVMX_MIPS_XKSEG_SPACE_KSEG0 = 0LL,
	CVMX_MIPS_XKSEG_SPACE_KSEG1 = 1LL,
	CVMX_MIPS_XKSEG_SPACE_SSEG = 2LL,
	CVMX_MIPS_XKSEG_SPACE_KSEG3 = 3LL
} cvmx_mips_xkseg_space_t;

typedef enum {
	CVMX_ADD_WIN_SCR = 0L,
	
	CVMX_ADD_WIN_DMA = 1L,
	CVMX_ADD_WIN_UNUSED = 2L,
	CVMX_ADD_WIN_UNUSED2 = 3L
} cvmx_add_win_dec_t;

typedef enum {
	CVMX_ADD_WIN_DMA_ADD = 0L,
	
	CVMX_ADD_WIN_DMA_SENDMEM = 1L,
	
	
	CVMX_ADD_WIN_DMA_SENDDMA = 2L,
	
	
	CVMX_ADD_WIN_DMA_SENDIO = 3L,
	
	
	CVMX_ADD_WIN_DMA_SENDSINGLE = 4L,
	
} cvmx_add_win_dma_dec_t;

typedef union {

	uint64_t u64;
	
	struct {
		uint64_t R:2;
		uint64_t offset:62;
	} sva;

	
	struct {
		uint64_t zeroes:33;
		uint64_t offset:31;
	} suseg;

	
	struct {
		uint64_t ones:33;
		uint64_t sp:2;
		uint64_t offset:29;
	} sxkseg;

	struct {
		uint64_t R:2;	
		uint64_t cca:3;	
		uint64_t mbz:10;
		uint64_t pa:49;	
	} sxkphys;

	
	struct {
		uint64_t mbz:15;
		
		uint64_t is_io:1;
		uint64_t did:8;
		
		uint64_t unaddr:4;
		uint64_t offset:36;
	} sphys;

	
	struct {
		
		uint64_t zeroes:24;
		
		uint64_t unaddr:4;
		uint64_t offset:36;
	} smem;

	
	struct {
		uint64_t mem_region:2;
		uint64_t mbz:13;
		
		uint64_t is_io:1;
		uint64_t did:8;
		
		uint64_t unaddr:4;
		uint64_t offset:36;
	} sio;

	struct {
		uint64_t ones:49;
		
		cvmx_add_win_dec_t csrdec:2;
		uint64_t addr:13;
	} sscr;

	
	struct {
		uint64_t ones:49;
		uint64_t csrdec:2;	
		uint64_t unused2:3;
		uint64_t type:3;
		uint64_t addr:7;
	} sdma;

	struct {
		uint64_t didspace:24;
		uint64_t unused:40;
	} sfilldidspace;

} cvmx_addr_t;


#define CVMX_MIPS32_SPACE_KSEG0 1l
#define CVMX_ADD_SEG32(segment, add) \
	(((int32_t)segment << 31) | (int32_t)(add))

#define CVMX_IO_SEG CVMX_MIPS_SPACE_XKPHYS

#define CVMX_ADD_SEG(segment, add) ((((uint64_t)segment) << 62) | (add))
#ifndef CVMX_ADD_IO_SEG
#define CVMX_ADD_IO_SEG(add) CVMX_ADD_SEG(CVMX_IO_SEG, (add))
#endif
#define CVMX_ADDR_DIDSPACE(did) (((CVMX_IO_SEG) << 22) | ((1ULL) << 8) | (did))
#define CVMX_ADDR_DID(did) (CVMX_ADDR_DIDSPACE(did) << 40)
#define CVMX_FULL_DID(did, subdid) (((did) << 3) | (subdid))

  
#define CVMX_OCT_DID_MIS 0ULL	
#define CVMX_OCT_DID_GMX0 1ULL
#define CVMX_OCT_DID_GMX1 2ULL
#define CVMX_OCT_DID_PCI 3ULL
#define CVMX_OCT_DID_KEY 4ULL
#define CVMX_OCT_DID_FPA 5ULL
#define CVMX_OCT_DID_DFA 6ULL
#define CVMX_OCT_DID_ZIP 7ULL
#define CVMX_OCT_DID_RNG 8ULL
#define CVMX_OCT_DID_IPD 9ULL
#define CVMX_OCT_DID_PKT 10ULL
#define CVMX_OCT_DID_TIM 11ULL
#define CVMX_OCT_DID_TAG 12ULL
  
#define CVMX_OCT_DID_L2C 16ULL
#define CVMX_OCT_DID_LMC 17ULL
#define CVMX_OCT_DID_SPX0 18ULL
#define CVMX_OCT_DID_SPX1 19ULL
#define CVMX_OCT_DID_PIP 20ULL
#define CVMX_OCT_DID_ASX0 22ULL
#define CVMX_OCT_DID_ASX1 23ULL
#define CVMX_OCT_DID_IOB 30ULL

#define CVMX_OCT_DID_PKT_SEND       CVMX_FULL_DID(CVMX_OCT_DID_PKT, 2ULL)
#define CVMX_OCT_DID_TAG_SWTAG      CVMX_FULL_DID(CVMX_OCT_DID_TAG, 0ULL)
#define CVMX_OCT_DID_TAG_TAG1       CVMX_FULL_DID(CVMX_OCT_DID_TAG, 1ULL)
#define CVMX_OCT_DID_TAG_TAG2       CVMX_FULL_DID(CVMX_OCT_DID_TAG, 2ULL)
#define CVMX_OCT_DID_TAG_TAG3       CVMX_FULL_DID(CVMX_OCT_DID_TAG, 3ULL)
#define CVMX_OCT_DID_TAG_NULL_RD    CVMX_FULL_DID(CVMX_OCT_DID_TAG, 4ULL)
#define CVMX_OCT_DID_TAG_CSR        CVMX_FULL_DID(CVMX_OCT_DID_TAG, 7ULL)
#define CVMX_OCT_DID_FAU_FAI        CVMX_FULL_DID(CVMX_OCT_DID_IOB, 0ULL)
#define CVMX_OCT_DID_TIM_CSR        CVMX_FULL_DID(CVMX_OCT_DID_TIM, 0ULL)
#define CVMX_OCT_DID_KEY_RW         CVMX_FULL_DID(CVMX_OCT_DID_KEY, 0ULL)
#define CVMX_OCT_DID_PCI_6          CVMX_FULL_DID(CVMX_OCT_DID_PCI, 6ULL)
#define CVMX_OCT_DID_MIS_BOO        CVMX_FULL_DID(CVMX_OCT_DID_MIS, 0ULL)
#define CVMX_OCT_DID_PCI_RML        CVMX_FULL_DID(CVMX_OCT_DID_PCI, 0ULL)
#define CVMX_OCT_DID_IPD_CSR        CVMX_FULL_DID(CVMX_OCT_DID_IPD, 7ULL)
#define CVMX_OCT_DID_DFA_CSR        CVMX_FULL_DID(CVMX_OCT_DID_DFA, 7ULL)
#define CVMX_OCT_DID_MIS_CSR        CVMX_FULL_DID(CVMX_OCT_DID_MIS, 7ULL)
#define CVMX_OCT_DID_ZIP_CSR        CVMX_FULL_DID(CVMX_OCT_DID_ZIP, 0ULL)

#endif 
