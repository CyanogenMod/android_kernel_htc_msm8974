/*****************************************************************************
* Copyright 2004 - 2008 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/


#ifndef _DMACHW_PRIV_H
#define _DMACHW_PRIV_H

#include <csp/stdint.h>

typedef struct {
	uint32_t sar;		
	uint32_t dar;		
	uint32_t llpPhy;	
	dmacHw_REG64_t ctl;	
	uint32_t sstat;		
	uint32_t dstat;		
	uint32_t devCtl;	
	uint32_t llp;		
} dmacHw_DESC_t;

typedef struct {
	int num;		
	dmacHw_DESC_t *pHead;	
	dmacHw_DESC_t *pTail;	
	dmacHw_DESC_t *pProg;	
	dmacHw_DESC_t *pEnd;	
	dmacHw_DESC_t *pFree;	
	uint32_t virt2PhyOffset;	
} dmacHw_DESC_RING_t;

typedef struct {
	uint32_t module;	
	uint32_t channel;	
	volatile uint32_t varDataStarted;	
	volatile uint32_t descUpdated;	
	void *userData;		
} dmacHw_CBLK_t;

#define dmacHw_ASSERT(a)                  if (!(a)) while (1)
#define dmacHw_MAX_CHANNEL_COUNT          16
#define dmacHw_FREE_USER_MEMORY           0xFFFFFFFF
#define dmacHw_DESC_FREE                  dmacHw_REG_CTL_DONE
#define dmacHw_DESC_INIT                  ((dmacHw_DESC_t *) 0xFFFFFFFF)
#define dmacHw_MAX_BLOCKSIZE              4064
#define dmacHw_GET_DESC_RING(addr)        (dmacHw_DESC_RING_t *)(addr)
#define dmacHw_ADDRESS_MASK(byte)         ((byte) - 1)
#define dmacHw_NEXT_DESC(rp, dp)           ((rp)->dp = (dmacHw_DESC_t *)(rp)->dp->llp)
#define dmacHw_HANDLE_TO_CBLK(handle)     ((dmacHw_CBLK_t *) (handle))
#define dmacHw_CBLK_TO_HANDLE(cblkp)      ((dmacHw_HANDLE_t) (cblkp))
#define dmacHw_DST_IS_MEMORY(tt)          (((tt) ==  dmacHw_TRANSFER_TYPE_PERIPHERAL_TO_MEM) || ((tt) == dmacHw_TRANSFER_TYPE_MEM_TO_MEM)) ? 1 : 0

static inline dmacHw_TRANSACTION_WIDTH_e dmacHw_GetNextTrWidth(dmacHw_TRANSACTION_WIDTH_e tw	
    ) {
	if (tw & dmacHw_REG_CTL_SRC_TR_WIDTH_MASK) {
		return ((tw >> dmacHw_REG_CTL_SRC_TR_WIDTH_SHIFT) -
			 1) << dmacHw_REG_CTL_SRC_TR_WIDTH_SHIFT;
	} else if (tw & dmacHw_REG_CTL_DST_TR_WIDTH_MASK) {
		return ((tw >> dmacHw_REG_CTL_DST_TR_WIDTH_SHIFT) -
			 1) << dmacHw_REG_CTL_DST_TR_WIDTH_SHIFT;
	}

	
	return dmacHw_SRC_TRANSACTION_WIDTH_8;
}

static inline int dmacHw_GetTrWidthInBytes(dmacHw_TRANSACTION_WIDTH_e tw	
    ) {
	int width = 1;
	switch (tw) {
	case dmacHw_SRC_TRANSACTION_WIDTH_8:
		width = 1;
		break;
	case dmacHw_SRC_TRANSACTION_WIDTH_16:
	case dmacHw_DST_TRANSACTION_WIDTH_16:
		width = 2;
		break;
	case dmacHw_SRC_TRANSACTION_WIDTH_32:
	case dmacHw_DST_TRANSACTION_WIDTH_32:
		width = 4;
		break;
	case dmacHw_SRC_TRANSACTION_WIDTH_64:
	case dmacHw_DST_TRANSACTION_WIDTH_64:
		width = 8;
		break;
	default:
		dmacHw_ASSERT(0);
	}

	
	return width;
}

#endif 
