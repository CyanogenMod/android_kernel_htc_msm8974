/*****************************************************************************
* Copyright 2003 - 2008 Broadcom Corporation.  All rights reserved.
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



#include <csp/stdint.h>
#include <stddef.h>

#include <csp/dmacHw.h>
#include <mach/csp/dmacHw_reg.h>
#include <mach/csp/dmacHw_priv.h>

extern dmacHw_CBLK_t dmacHw_gCblk[dmacHw_MAX_CHANNEL_COUNT];	


void dmacHw_setDataLength(dmacHw_CONFIG_t *pConfig,	
			  void *pDescriptor,	
			  size_t dataLen	
    );

static void DisplayRegisterContents(int module,	
				    int channel,	
				    int (*fpPrint) (const char *, ...)	
    ) {
	int chan;

	(*fpPrint) ("Displaying register content \n\n");
	(*fpPrint) ("Module %d: Interrupt raw transfer              0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_RAW_TRAN(module)));
	(*fpPrint) ("Module %d: Interrupt raw block                 0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_RAW_BLOCK(module)));
	(*fpPrint) ("Module %d: Interrupt raw src transfer          0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_RAW_STRAN(module)));
	(*fpPrint) ("Module %d: Interrupt raw dst transfer          0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_RAW_DTRAN(module)));
	(*fpPrint) ("Module %d: Interrupt raw error                 0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_RAW_ERROR(module)));
	(*fpPrint) ("--------------------------------------------------\n");
	(*fpPrint) ("Module %d: Interrupt stat transfer             0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_STAT_TRAN(module)));
	(*fpPrint) ("Module %d: Interrupt stat block                0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_STAT_BLOCK(module)));
	(*fpPrint) ("Module %d: Interrupt stat src transfer         0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_STAT_STRAN(module)));
	(*fpPrint) ("Module %d: Interrupt stat dst transfer         0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_STAT_DTRAN(module)));
	(*fpPrint) ("Module %d: Interrupt stat error                0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_STAT_ERROR(module)));
	(*fpPrint) ("--------------------------------------------------\n");
	(*fpPrint) ("Module %d: Interrupt mask transfer             0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_MASK_TRAN(module)));
	(*fpPrint) ("Module %d: Interrupt mask block                0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_MASK_BLOCK(module)));
	(*fpPrint) ("Module %d: Interrupt mask src transfer         0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_MASK_STRAN(module)));
	(*fpPrint) ("Module %d: Interrupt mask dst transfer         0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_MASK_DTRAN(module)));
	(*fpPrint) ("Module %d: Interrupt mask error                0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_MASK_ERROR(module)));
	(*fpPrint) ("--------------------------------------------------\n");
	(*fpPrint) ("Module %d: Interrupt clear transfer            0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_CLEAR_TRAN(module)));
	(*fpPrint) ("Module %d: Interrupt clear block               0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_CLEAR_BLOCK(module)));
	(*fpPrint) ("Module %d: Interrupt clear src transfer        0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_CLEAR_STRAN(module)));
	(*fpPrint) ("Module %d: Interrupt clear dst transfer        0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_CLEAR_DTRAN(module)));
	(*fpPrint) ("Module %d: Interrupt clear error               0x%X\n",
		    module, (uint32_t) (dmacHw_REG_INT_CLEAR_ERROR(module)));
	(*fpPrint) ("--------------------------------------------------\n");
	(*fpPrint) ("Module %d: SW source req                       0x%X\n",
		    module, (uint32_t) (dmacHw_REG_SW_HS_SRC_REQ(module)));
	(*fpPrint) ("Module %d: SW dest req                         0x%X\n",
		    module, (uint32_t) (dmacHw_REG_SW_HS_DST_REQ(module)));
	(*fpPrint) ("Module %d: SW source signal                    0x%X\n",
		    module, (uint32_t) (dmacHw_REG_SW_HS_SRC_SGL_REQ(module)));
	(*fpPrint) ("Module %d: SW dest signal                      0x%X\n",
		    module, (uint32_t) (dmacHw_REG_SW_HS_DST_SGL_REQ(module)));
	(*fpPrint) ("Module %d: SW source last                      0x%X\n",
		    module, (uint32_t) (dmacHw_REG_SW_HS_SRC_LST_REQ(module)));
	(*fpPrint) ("Module %d: SW dest last                        0x%X\n",
		    module, (uint32_t) (dmacHw_REG_SW_HS_DST_LST_REQ(module)));
	(*fpPrint) ("--------------------------------------------------\n");
	(*fpPrint) ("Module %d: misc config                         0x%X\n",
		    module, (uint32_t) (dmacHw_REG_MISC_CFG(module)));
	(*fpPrint) ("Module %d: misc channel enable                 0x%X\n",
		    module, (uint32_t) (dmacHw_REG_MISC_CH_ENABLE(module)));
	(*fpPrint) ("Module %d: misc ID                             0x%X\n",
		    module, (uint32_t) (dmacHw_REG_MISC_ID(module)));
	(*fpPrint) ("Module %d: misc test                           0x%X\n",
		    module, (uint32_t) (dmacHw_REG_MISC_TEST(module)));

	if (channel == -1) {
		for (chan = 0; chan < 8; chan++) {
			(*fpPrint)
			    ("--------------------------------------------------\n");
			(*fpPrint)
			    ("Module %d: Channel %d Source                   0x%X\n",
			     module, chan,
			     (uint32_t) (dmacHw_REG_SAR(module, chan)));
			(*fpPrint)
			    ("Module %d: Channel %d Destination              0x%X\n",
			     module, chan,
			     (uint32_t) (dmacHw_REG_DAR(module, chan)));
			(*fpPrint)
			    ("Module %d: Channel %d LLP                      0x%X\n",
			     module, chan,
			     (uint32_t) (dmacHw_REG_LLP(module, chan)));
			(*fpPrint)
			    ("Module %d: Channel %d Control (LO)             0x%X\n",
			     module, chan,
			     (uint32_t) (dmacHw_REG_CTL_LO(module, chan)));
			(*fpPrint)
			    ("Module %d: Channel %d Control (HI)             0x%X\n",
			     module, chan,
			     (uint32_t) (dmacHw_REG_CTL_HI(module, chan)));
			(*fpPrint)
			    ("Module %d: Channel %d Source Stats             0x%X\n",
			     module, chan,
			     (uint32_t) (dmacHw_REG_SSTAT(module, chan)));
			(*fpPrint)
			    ("Module %d: Channel %d Dest Stats               0x%X\n",
			     module, chan,
			     (uint32_t) (dmacHw_REG_DSTAT(module, chan)));
			(*fpPrint)
			    ("Module %d: Channel %d Source Stats Addr        0x%X\n",
			     module, chan,
			     (uint32_t) (dmacHw_REG_SSTATAR(module, chan)));
			(*fpPrint)
			    ("Module %d: Channel %d Dest Stats Addr          0x%X\n",
			     module, chan,
			     (uint32_t) (dmacHw_REG_DSTATAR(module, chan)));
			(*fpPrint)
			    ("Module %d: Channel %d Config (LO)              0x%X\n",
			     module, chan,
			     (uint32_t) (dmacHw_REG_CFG_LO(module, chan)));
			(*fpPrint)
			    ("Module %d: Channel %d Config (HI)              0x%X\n",
			     module, chan,
			     (uint32_t) (dmacHw_REG_CFG_HI(module, chan)));
		}
	} else {
		chan = channel;
		(*fpPrint)
		    ("--------------------------------------------------\n");
		(*fpPrint)
		    ("Module %d: Channel %d Source                   0x%X\n",
		     module, chan, (uint32_t) (dmacHw_REG_SAR(module, chan)));
		(*fpPrint)
		    ("Module %d: Channel %d Destination              0x%X\n",
		     module, chan, (uint32_t) (dmacHw_REG_DAR(module, chan)));
		(*fpPrint)
		    ("Module %d: Channel %d LLP                      0x%X\n",
		     module, chan, (uint32_t) (dmacHw_REG_LLP(module, chan)));
		(*fpPrint)
		    ("Module %d: Channel %d Control (LO)             0x%X\n",
		     module, chan,
		     (uint32_t) (dmacHw_REG_CTL_LO(module, chan)));
		(*fpPrint)
		    ("Module %d: Channel %d Control (HI)             0x%X\n",
		     module, chan,
		     (uint32_t) (dmacHw_REG_CTL_HI(module, chan)));
		(*fpPrint)
		    ("Module %d: Channel %d Source Stats             0x%X\n",
		     module, chan, (uint32_t) (dmacHw_REG_SSTAT(module, chan)));
		(*fpPrint)
		    ("Module %d: Channel %d Dest Stats               0x%X\n",
		     module, chan, (uint32_t) (dmacHw_REG_DSTAT(module, chan)));
		(*fpPrint)
		    ("Module %d: Channel %d Source Stats Addr        0x%X\n",
		     module, chan,
		     (uint32_t) (dmacHw_REG_SSTATAR(module, chan)));
		(*fpPrint)
		    ("Module %d: Channel %d Dest Stats Addr          0x%X\n",
		     module, chan,
		     (uint32_t) (dmacHw_REG_DSTATAR(module, chan)));
		(*fpPrint)
		    ("Module %d: Channel %d Config (LO)              0x%X\n",
		     module, chan,
		     (uint32_t) (dmacHw_REG_CFG_LO(module, chan)));
		(*fpPrint)
		    ("Module %d: Channel %d Config (HI)              0x%X\n",
		     module, chan,
		     (uint32_t) (dmacHw_REG_CFG_HI(module, chan)));
	}
}

static void DisplayDescRing(void *pDescriptor,	
			    int (*fpPrint) (const char *, ...)	
    ) {
	dmacHw_DESC_RING_t *pRing = dmacHw_GET_DESC_RING(pDescriptor);
	dmacHw_DESC_t *pStart;

	if (pRing->pHead == NULL) {
		return;
	}

	pStart = pRing->pHead;

	while ((dmacHw_DESC_t *) pStart->llp != pRing->pHead) {
		if (pStart == pRing->pHead) {
			(*fpPrint) ("Head\n");
		}
		if (pStart == pRing->pTail) {
			(*fpPrint) ("Tail\n");
		}
		if (pStart == pRing->pProg) {
			(*fpPrint) ("Prog\n");
		}
		if (pStart == pRing->pEnd) {
			(*fpPrint) ("End\n");
		}
		if (pStart == pRing->pFree) {
			(*fpPrint) ("Free\n");
		}
		(*fpPrint) ("0x%X:\n", (uint32_t) pStart);
		(*fpPrint) ("sar    0x%0X\n", pStart->sar);
		(*fpPrint) ("dar    0x%0X\n", pStart->dar);
		(*fpPrint) ("llp    0x%0X\n", pStart->llp);
		(*fpPrint) ("ctl.lo 0x%0X\n", pStart->ctl.lo);
		(*fpPrint) ("ctl.hi 0x%0X\n", pStart->ctl.hi);
		(*fpPrint) ("sstat  0x%0X\n", pStart->sstat);
		(*fpPrint) ("dstat  0x%0X\n", pStart->dstat);
		(*fpPrint) ("devCtl 0x%0X\n", pStart->devCtl);

		pStart = (dmacHw_DESC_t *) pStart->llp;
	}
	if (pStart == pRing->pHead) {
		(*fpPrint) ("Head\n");
	}
	if (pStart == pRing->pTail) {
		(*fpPrint) ("Tail\n");
	}
	if (pStart == pRing->pProg) {
		(*fpPrint) ("Prog\n");
	}
	if (pStart == pRing->pEnd) {
		(*fpPrint) ("End\n");
	}
	if (pStart == pRing->pFree) {
		(*fpPrint) ("Free\n");
	}
	(*fpPrint) ("0x%X:\n", (uint32_t) pStart);
	(*fpPrint) ("sar    0x%0X\n", pStart->sar);
	(*fpPrint) ("dar    0x%0X\n", pStart->dar);
	(*fpPrint) ("llp    0x%0X\n", pStart->llp);
	(*fpPrint) ("ctl.lo 0x%0X\n", pStart->ctl.lo);
	(*fpPrint) ("ctl.hi 0x%0X\n", pStart->ctl.hi);
	(*fpPrint) ("sstat  0x%0X\n", pStart->sstat);
	(*fpPrint) ("dstat  0x%0X\n", pStart->dstat);
	(*fpPrint) ("devCtl 0x%0X\n", pStart->devCtl);
}

static inline int DmaIsFlowController(void *pDescriptor	
    ) {
	uint32_t ttfc =
	    (dmacHw_GET_DESC_RING(pDescriptor))->pTail->ctl.
	    lo & dmacHw_REG_CTL_TTFC_MASK;

	switch (ttfc) {
	case dmacHw_REG_CTL_TTFC_MM_DMAC:
	case dmacHw_REG_CTL_TTFC_MP_DMAC:
	case dmacHw_REG_CTL_TTFC_PM_DMAC:
	case dmacHw_REG_CTL_TTFC_PP_DMAC:
		return 1;
	}

	return 0;
}

void dmacHw_setDataLength(dmacHw_CONFIG_t *pConfig,	
			  void *pDescriptor,	
			  size_t dataLen	
    ) {
	dmacHw_DESC_t *pProg;
	dmacHw_DESC_t *pHead;
	int srcTs = 0;
	int srcTrSize = 0;

	pHead = (dmacHw_GET_DESC_RING(pDescriptor))->pHead;
	pProg = pHead;

	srcTrSize = dmacHw_GetTrWidthInBytes(pConfig->srcMaxTransactionWidth);
	srcTs = dataLen / srcTrSize;
	do {
		pProg->ctl.hi = srcTs & dmacHw_REG_CTL_BLOCK_TS_MASK;
		pProg = (dmacHw_DESC_t *) pProg->llp;
	} while (pProg != pHead);
}

void dmacHw_clearInterrupt(dmacHw_HANDLE_t handle	
    ) {
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);

	dmacHw_TRAN_INT_CLEAR(pCblk->module, pCblk->channel);
	dmacHw_BLOCK_INT_CLEAR(pCblk->module, pCblk->channel);
	dmacHw_ERROR_INT_CLEAR(pCblk->module, pCblk->channel);
}

dmacHw_INTERRUPT_STATUS_e dmacHw_getInterruptStatus(dmacHw_HANDLE_t handle	
    ) {
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);
	dmacHw_INTERRUPT_STATUS_e status = dmacHw_INTERRUPT_STATUS_NONE;

	if (dmacHw_REG_INT_STAT_TRAN(pCblk->module) &
	    ((0x00000001 << pCblk->channel))) {
		status |= dmacHw_INTERRUPT_STATUS_TRANS;
	}
	if (dmacHw_REG_INT_STAT_BLOCK(pCblk->module) &
	    ((0x00000001 << pCblk->channel))) {
		status |= dmacHw_INTERRUPT_STATUS_BLOCK;
	}
	if (dmacHw_REG_INT_STAT_ERROR(pCblk->module) &
	    ((0x00000001 << pCblk->channel))) {
		status |= dmacHw_INTERRUPT_STATUS_ERROR;
	}

	return status;
}

dmacHw_HANDLE_t dmacHw_getInterruptSource(void)
{
	uint32_t i;

	for (i = 0; i < dmaChannelCount_0 + dmaChannelCount_1; i++) {
		if ((dmacHw_REG_INT_STAT_TRAN(dmacHw_gCblk[i].module) &
		     ((0x00000001 << dmacHw_gCblk[i].channel)))
		    || (dmacHw_REG_INT_STAT_BLOCK(dmacHw_gCblk[i].module) &
			((0x00000001 << dmacHw_gCblk[i].channel)))
		    || (dmacHw_REG_INT_STAT_ERROR(dmacHw_gCblk[i].module) &
			((0x00000001 << dmacHw_gCblk[i].channel)))
		    ) {
			return dmacHw_CBLK_TO_HANDLE(&dmacHw_gCblk[i]);
		}
	}
	return dmacHw_CBLK_TO_HANDLE(NULL);
}

int dmacHw_calculateDescriptorCount(dmacHw_CONFIG_t *pConfig,	
				    void *pSrcAddr,	
				    void *pDstAddr,	
				    size_t dataLen	
    ) {
	int srcTs = 0;
	int oddSize = 0;
	int descCount = 0;
	int dstTrSize = 0;
	int srcTrSize = 0;
	uint32_t maxBlockSize = dmacHw_MAX_BLOCKSIZE;
	dmacHw_TRANSACTION_WIDTH_e dstTrWidth;
	dmacHw_TRANSACTION_WIDTH_e srcTrWidth;

	dstTrSize = dmacHw_GetTrWidthInBytes(pConfig->dstMaxTransactionWidth);
	srcTrSize = dmacHw_GetTrWidthInBytes(pConfig->srcMaxTransactionWidth);

	
	if ((pSrcAddr == NULL) || (pDstAddr == NULL) || (dataLen == 0)) {
		
		return -1;
	}

	
	if (pConfig->srcGatherWidth % srcTrSize
	    || pConfig->dstScatterWidth % dstTrSize) {
		return -1;
	}


	
	dstTrWidth = pConfig->dstMaxTransactionWidth;
	while (dmacHw_ADDRESS_MASK(dstTrSize) & (uint32_t) pDstAddr) {
		dstTrWidth = dmacHw_GetNextTrWidth(dstTrWidth);
		dstTrSize = dmacHw_GetTrWidthInBytes(dstTrWidth);
	}

	
	srcTrWidth = pConfig->srcMaxTransactionWidth;
	while (dmacHw_ADDRESS_MASK(srcTrSize) & (uint32_t) pSrcAddr) {
		srcTrWidth = dmacHw_GetNextTrWidth(srcTrWidth);
		srcTrSize = dmacHw_GetTrWidthInBytes(srcTrWidth);
	}

	
	if (pConfig->maxDataPerBlock
	    && ((pConfig->maxDataPerBlock / srcTrSize) <
		dmacHw_MAX_BLOCKSIZE)) {
		maxBlockSize = pConfig->maxDataPerBlock / srcTrSize;
	}

	
	srcTs = dataLen / srcTrSize;
	
	if (srcTs && (dstTrSize > srcTrSize)) {
		oddSize = dataLen % dstTrSize;
		
		srcTs = srcTs - (oddSize / srcTrSize);
	} else {
		oddSize = dataLen % srcTrSize;
	}
	
	if (oddSize) {
		descCount++;
	}

	
	if (srcTs) {
		descCount += ((srcTs - 1) / maxBlockSize) + 1;
	}

	return descCount;
}

uint32_t dmacHw_descriptorPending(dmacHw_HANDLE_t handle,	
				  void *pDescriptor	
    ) {
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);
	dmacHw_DESC_RING_t *pRing = dmacHw_GET_DESC_RING(pDescriptor);

	
	if (!CHANNEL_BUSY(pCblk->module, pCblk->channel)) {
		
		if (pRing->pEnd) {
			
			return 1;
		}
	}
	return 0;
}

void dmacHw_stopTransfer(dmacHw_HANDLE_t handle	
    ) {
	dmacHw_CBLK_t *pCblk;

	pCblk = dmacHw_HANDLE_TO_CBLK(handle);

	
	dmacHw_DMA_STOP(pCblk->module, pCblk->channel);
}

int dmacHw_freeMem(dmacHw_CONFIG_t *pConfig,	
		   void *pDescriptor,	
		   void (*fpFree) (void *)	
    ) {
	dmacHw_DESC_RING_t *pRing = dmacHw_GET_DESC_RING(pDescriptor);
	uint32_t count = 0;

	if (fpFree == NULL) {
		return -1;
	}

	while ((pRing->pFree != pRing->pTail)
	       && (pRing->pFree->ctl.lo & dmacHw_DESC_FREE)) {
		if (pRing->pFree->devCtl == dmacHw_FREE_USER_MEMORY) {
			
			if (dmacHw_DST_IS_MEMORY(pConfig->transferType)) {
				(*fpFree) ((void *)pRing->pFree->dar);
			} else {
				
				(*fpFree) ((void *)pRing->pFree->sar);
			}
			
			pRing->pFree->devCtl = ~dmacHw_FREE_USER_MEMORY;
		}
		dmacHw_NEXT_DESC(pRing, pFree);

		count++;
	}

	return count;
}

int dmacHw_setVariableDataDescriptor(dmacHw_HANDLE_t handle,	
				     dmacHw_CONFIG_t *pConfig,	
				     void *pDescriptor,	
				     uint32_t srcAddr,	
				     void *(*fpAlloc) (int len),	
				     int len,	
				     int num	
    ) {
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);
	dmacHw_DESC_t *pProg = NULL;
	dmacHw_DESC_t *pLast = NULL;
	dmacHw_DESC_RING_t *pRing = dmacHw_GET_DESC_RING(pDescriptor);
	uint32_t dstAddr;
	uint32_t controlParam;
	int i;

	dmacHw_ASSERT(pConfig->transferType ==
		      dmacHw_TRANSFER_TYPE_PERIPHERAL_TO_MEM);

	if (num > pRing->num) {
		return -1;
	}

	pLast = pRing->pEnd;	
	pProg = pRing->pHead;	

	controlParam = pConfig->srcUpdate |
	    pConfig->dstUpdate |
	    pConfig->srcMaxTransactionWidth |
	    pConfig->dstMaxTransactionWidth |
	    pConfig->srcMasterInterface |
	    pConfig->dstMasterInterface |
	    pConfig->srcMaxBurstWidth |
	    pConfig->dstMaxBurstWidth |
	    dmacHw_REG_CTL_TTFC_PM_PERI |
	    dmacHw_REG_CTL_LLP_DST_EN |
	    dmacHw_REG_CTL_LLP_SRC_EN | dmacHw_REG_CTL_INT_EN;

	for (i = 0; i < num; i++) {
		
		if (((pRing->pHead->ctl.hi & dmacHw_DESC_FREE) == 0) ||
		    ((dmacHw_DESC_t *) pRing->pHead->llp == pRing->pTail)
		    ) {
			
			break;
		}
		
		pRing->pHead->sar = srcAddr;
		if (fpAlloc) {
			
			dstAddr = (uint32_t) (*fpAlloc) (len);
			
			if (dstAddr == 0) {
				if (i == 0) {
					
					return -1;
				}
				break;
			}
			
			pRing->pHead->dar = dstAddr;
		}
		
		pRing->pHead->ctl.lo = controlParam;
		
		pRing->pHead->devCtl = dmacHw_FREE_USER_MEMORY;
		
		pRing->pHead->ctl.hi = 0;
		
		pRing->pEnd = pRing->pHead;
		
		dmacHw_NEXT_DESC(pRing, pHead);
	}

	
	pRing->pEnd->ctl.lo &=
	    ~(dmacHw_REG_CTL_LLP_DST_EN | dmacHw_REG_CTL_LLP_SRC_EN);
	
	if (pLast != pProg) {
		pLast->ctl.lo |=
		    dmacHw_REG_CTL_LLP_DST_EN | dmacHw_REG_CTL_LLP_SRC_EN;
	}
	
	pCblk->descUpdated = 1;
	if (!pCblk->varDataStarted) {
		
		dmacHw_SET_LLP(pCblk->module, pCblk->channel,
			       (uint32_t) pProg - pRing->virt2PhyOffset);
		
		pCblk->varDataStarted = 1;
	}

	return i;
}

int dmacHw_readTransferredData(dmacHw_HANDLE_t handle,	
			       dmacHw_CONFIG_t *pConfig,	
			       void *pDescriptor,	
			       void **ppBbuf,	
			       size_t *pLlen	
    ) {
	dmacHw_DESC_RING_t *pRing = dmacHw_GET_DESC_RING(pDescriptor);

	(void)handle;

	if (pConfig->transferMode != dmacHw_TRANSFER_MODE_CONTINUOUS) {
		if (((pRing->pTail->ctl.hi & dmacHw_DESC_FREE) == 0) ||
		    (pRing->pTail == pRing->pHead)
		    ) {
			
			*ppBbuf = (char *)NULL;
			*pLlen = 0;

			return 0;
		}
	}

	
	*ppBbuf = (char *)pRing->pTail->dar;

	
	if (DmaIsFlowController(pDescriptor)) {
		uint32_t srcTrSize = 0;

		switch (pRing->pTail->ctl.lo & dmacHw_REG_CTL_SRC_TR_WIDTH_MASK) {
		case dmacHw_REG_CTL_SRC_TR_WIDTH_8:
			srcTrSize = 1;
			break;
		case dmacHw_REG_CTL_SRC_TR_WIDTH_16:
			srcTrSize = 2;
			break;
		case dmacHw_REG_CTL_SRC_TR_WIDTH_32:
			srcTrSize = 4;
			break;
		case dmacHw_REG_CTL_SRC_TR_WIDTH_64:
			srcTrSize = 8;
			break;
		default:
			dmacHw_ASSERT(0);
		}
		
		*pLlen =
		    (pRing->pTail->ctl.hi & dmacHw_REG_CTL_BLOCK_TS_MASK) *
		    srcTrSize;
	} else {
		
		*pLlen = pRing->pTail->sstat;
	}

	
	dmacHw_NEXT_DESC(pRing, pTail);

	return 1;
}

int dmacHw_setControlDescriptor(dmacHw_CONFIG_t *pConfig,	
				void *pDescriptor,	
				uint32_t ctlAddress,	
				uint32_t control	
    ) {
	dmacHw_DESC_RING_t *pRing = dmacHw_GET_DESC_RING(pDescriptor);

	if (ctlAddress == 0) {
		return -1;
	}

	
	if ((pRing->pHead->ctl.hi & dmacHw_DESC_FREE) == 0) {
		return -1;
	}
	
	pRing->pHead->devCtl = control;
	
	pRing->pHead->sar = (uint32_t) &pRing->pHead->devCtl;
	pRing->pHead->dar = ctlAddress;
	
	if (pConfig->flowControler == dmacHw_FLOW_CONTROL_DMA) {
		pRing->pHead->ctl.lo = pConfig->transferType |
		    dmacHw_SRC_ADDRESS_UPDATE_MODE_INC |
		    dmacHw_DST_ADDRESS_UPDATE_MODE_INC |
		    dmacHw_SRC_TRANSACTION_WIDTH_32 |
		    pConfig->dstMaxTransactionWidth |
		    dmacHw_SRC_BURST_WIDTH_0 |
		    dmacHw_DST_BURST_WIDTH_0 |
		    pConfig->srcMasterInterface |
		    pConfig->dstMasterInterface | dmacHw_REG_CTL_INT_EN;
	} else {
		uint32_t transferType = 0;
		switch (pConfig->transferType) {
		case dmacHw_TRANSFER_TYPE_PERIPHERAL_TO_MEM:
			transferType = dmacHw_REG_CTL_TTFC_PM_PERI;
			break;
		case dmacHw_TRANSFER_TYPE_MEM_TO_PERIPHERAL:
			transferType = dmacHw_REG_CTL_TTFC_MP_PERI;
			break;
		default:
			dmacHw_ASSERT(0);
		}
		pRing->pHead->ctl.lo = transferType |
		    dmacHw_SRC_ADDRESS_UPDATE_MODE_INC |
		    dmacHw_DST_ADDRESS_UPDATE_MODE_INC |
		    dmacHw_SRC_TRANSACTION_WIDTH_32 |
		    pConfig->dstMaxTransactionWidth |
		    dmacHw_SRC_BURST_WIDTH_0 |
		    dmacHw_DST_BURST_WIDTH_0 |
		    pConfig->srcMasterInterface |
		    pConfig->dstMasterInterface |
		    pConfig->flowControler | dmacHw_REG_CTL_INT_EN;
	}

	
	pRing->pHead->ctl.hi = dmacHw_REG_CTL_BLOCK_TS_MASK & 1;

	
	if (pRing->pProg == dmacHw_DESC_INIT) {
		pRing->pProg = pRing->pHead;
	}
	pRing->pEnd = pRing->pHead;

	
	dmacHw_NEXT_DESC(pRing, pHead);

	
	if (!dmacHw_DST_IS_MEMORY(pConfig->transferType)) {
		pRing->pTail = pRing->pHead;
	}
	return 0;
}

void dmacHw_setChannelUserData(dmacHw_HANDLE_t handle,	
			       void *userData	
    ) {
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);

	pCblk->userData = userData;
}

void *dmacHw_getChannelUserData(dmacHw_HANDLE_t handle	
    ) {
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);

	return pCblk->userData;
}

void dmacHw_resetDescriptorControl(void *pDescriptor	
    ) {
	int i;
	dmacHw_DESC_RING_t *pRing;
	dmacHw_DESC_t *pDesc;

	pRing = dmacHw_GET_DESC_RING(pDescriptor);
	pDesc = pRing->pHead;

	for (i = 0; i < pRing->num; i++) {
		
		pDesc->ctl.hi = dmacHw_DESC_FREE;
		
		pDesc++;
	}
	pRing->pFree = pRing->pTail = pRing->pEnd = pRing->pHead;
	pRing->pProg = dmacHw_DESC_INIT;
}

void dmacHw_printDebugInfo(dmacHw_HANDLE_t handle,	
			   void *pDescriptor,	
			   int (*fpPrint) (const char *, ...)	
    ) {
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);

	DisplayRegisterContents(pCblk->module, pCblk->channel, fpPrint);
	DisplayDescRing(pDescriptor, fpPrint);
}
