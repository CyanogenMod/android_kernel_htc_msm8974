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
#include <csp/string.h>
#include <stddef.h>

#include <csp/dmacHw.h>
#include <mach/csp/dmacHw_reg.h>
#include <mach/csp/dmacHw_priv.h>
#include <mach/csp/chipcHw_inline.h>


dmacHw_CBLK_t dmacHw_gCblk[dmacHw_MAX_CHANNEL_COUNT];

uint32_t dmaChannelCount_0 = dmacHw_MAX_CHANNEL_COUNT / 2;
uint32_t dmaChannelCount_1 = dmacHw_MAX_CHANNEL_COUNT / 2;

static uint32_t GetFifoSize(dmacHw_HANDLE_t handle	
    ) {
	uint32_t val = 0;
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);
	dmacHw_MISC_t *pMiscReg =
	    (dmacHw_MISC_t *) dmacHw_REG_MISC_BASE(pCblk->module);

	switch (pCblk->channel) {
	case 0:
		val = (pMiscReg->CompParm2.lo & 0x70000000) >> 28;
		break;
	case 1:
		val = (pMiscReg->CompParm3.hi & 0x70000000) >> 28;
		break;
	case 2:
		val = (pMiscReg->CompParm3.lo & 0x70000000) >> 28;
		break;
	case 3:
		val = (pMiscReg->CompParm4.hi & 0x70000000) >> 28;
		break;
	case 4:
		val = (pMiscReg->CompParm4.lo & 0x70000000) >> 28;
		break;
	case 5:
		val = (pMiscReg->CompParm5.hi & 0x70000000) >> 28;
		break;
	case 6:
		val = (pMiscReg->CompParm5.lo & 0x70000000) >> 28;
		break;
	case 7:
		val = (pMiscReg->CompParm6.hi & 0x70000000) >> 28;
		break;
	}

	if (val <= 0x4) {
		return 8 << val;
	} else {
		dmacHw_ASSERT(0);
	}
	return 0;
}

void dmacHw_initiateTransfer(dmacHw_HANDLE_t handle,	
			     dmacHw_CONFIG_t *pConfig,	
			     void *pDescriptor	
    ) {
	dmacHw_DESC_RING_t *pRing;
	dmacHw_DESC_t *pProg;
	dmacHw_CBLK_t *pCblk;

	pCblk = dmacHw_HANDLE_TO_CBLK(handle);
	pRing = dmacHw_GET_DESC_RING(pDescriptor);

	if (CHANNEL_BUSY(pCblk->module, pCblk->channel)) {
		
		return;
	}

	if (pCblk->varDataStarted) {
		if (pCblk->descUpdated) {
			pCblk->descUpdated = 0;
			pProg =
			    (dmacHw_DESC_t *) ((uint32_t)
					       dmacHw_REG_LLP(pCblk->module,
							      pCblk->channel) +
					       pRing->virt2PhyOffset);

			
			if (!(pProg->ctl.hi & dmacHw_REG_CTL_DONE)) {
				dmacHw_SET_SAR(pCblk->module, pCblk->channel,
					       pProg->sar);
				dmacHw_SET_DAR(pCblk->module, pCblk->channel,
					       pProg->dar);
				dmacHw_REG_CTL_LO(pCblk->module,
						  pCblk->channel) =
				    pProg->ctl.lo;
				dmacHw_REG_CTL_HI(pCblk->module,
						  pCblk->channel) =
				    pProg->ctl.hi;
			} else if (pProg == (dmacHw_DESC_t *) pRing->pEnd->llp) {
				
				return;
			} else {
				dmacHw_ASSERT(0);
			}
		} else {
			return;
		}
	} else {
		if (pConfig->transferMode == dmacHw_TRANSFER_MODE_PERIODIC) {
			
			pProg = pRing->pHead;
			
			dmacHw_NEXT_DESC(pRing, pHead);
		} else {
			
			if (pRing->pEnd == NULL) {
				return;
			}

			pProg = pRing->pProg;
			if (pConfig->transferMode ==
			    dmacHw_TRANSFER_MODE_CONTINUOUS) {
				
				dmacHw_ASSERT((dmacHw_DESC_t *) pRing->pEnd->
					      llp == pRing->pProg);
				
				dmacHw_ASSERT((dmacHw_DESC_t *) pRing->pProg ==
					      pRing->pHead);
				
				do {
					pRing->pProg->ctl.lo |=
					    (dmacHw_REG_CTL_LLP_DST_EN |
					     dmacHw_REG_CTL_LLP_SRC_EN);
					pRing->pProg =
					    (dmacHw_DESC_t *) pRing->pProg->llp;
				} while (pRing->pProg != pRing->pHead);
			} else {
				
				while (pRing->pProg != pRing->pEnd) {
					pRing->pProg->ctl.lo |=
					    (dmacHw_REG_CTL_LLP_DST_EN |
					     dmacHw_REG_CTL_LLP_SRC_EN);
					pRing->pProg =
					    (dmacHw_DESC_t *) pRing->pProg->llp;
				}
			}
		}

		
		dmacHw_SET_SAR(pCblk->module, pCblk->channel, pProg->sar);
		dmacHw_SET_DAR(pCblk->module, pCblk->channel, pProg->dar);
		dmacHw_SET_LLP(pCblk->module, pCblk->channel,
			       (uint32_t) pProg - pRing->virt2PhyOffset);
		dmacHw_REG_CTL_LO(pCblk->module, pCblk->channel) =
		    pProg->ctl.lo;
		dmacHw_REG_CTL_HI(pCblk->module, pCblk->channel) =
		    pProg->ctl.hi;
		if (pRing->pEnd) {
			
			pRing->pProg = (dmacHw_DESC_t *) pRing->pEnd->llp;
		}
		
		pRing->pEnd = (dmacHw_DESC_t *) NULL;
	}
	
	dmacHw_DMA_START(pCblk->module, pCblk->channel);
}

void dmacHw_initDma(void)
{

	uint32_t i = 0;

	dmaChannelCount_0 = dmacHw_GET_NUM_CHANNEL(0);
	dmaChannelCount_1 = dmacHw_GET_NUM_CHANNEL(1);

	
	chipcHw_busInterfaceClockEnable(chipcHw_REG_BUS_CLOCK_DMAC0);
	chipcHw_busInterfaceClockEnable(chipcHw_REG_BUS_CLOCK_DMAC1);

	if ((dmaChannelCount_0 + dmaChannelCount_1) > dmacHw_MAX_CHANNEL_COUNT) {
		dmacHw_ASSERT(0);
	}

	memset((void *)dmacHw_gCblk, 0,
	       sizeof(dmacHw_CBLK_t) * (dmaChannelCount_0 + dmaChannelCount_1));
	for (i = 0; i < dmaChannelCount_0; i++) {
		dmacHw_gCblk[i].module = 0;
		dmacHw_gCblk[i].channel = i;
	}
	for (i = 0; i < dmaChannelCount_1; i++) {
		dmacHw_gCblk[i + dmaChannelCount_0].module = 1;
		dmacHw_gCblk[i + dmaChannelCount_0].channel = i;
	}
}

void dmacHw_exitDma(void)
{
	
	chipcHw_busInterfaceClockDisable(chipcHw_REG_BUS_CLOCK_DMAC0);
	chipcHw_busInterfaceClockDisable(chipcHw_REG_BUS_CLOCK_DMAC1);
}

dmacHw_HANDLE_t dmacHw_getChannelHandle(dmacHw_ID_t channelId	
    ) {
	int idx;

	switch ((channelId >> 8)) {
	case 0:
		dmacHw_ASSERT((channelId & 0xff) < dmaChannelCount_0);
		idx = (channelId & 0xff);
		break;
	case 1:
		dmacHw_ASSERT((channelId & 0xff) < dmaChannelCount_1);
		idx = dmaChannelCount_0 + (channelId & 0xff);
		break;
	default:
		dmacHw_ASSERT(0);
		return (dmacHw_HANDLE_t) -1;
	}

	return dmacHw_CBLK_TO_HANDLE(&dmacHw_gCblk[idx]);
}

int dmacHw_initChannel(dmacHw_HANDLE_t handle	
    ) {
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);
	int module = pCblk->module;
	int channel = pCblk->channel;

	
	memset((void *)pCblk, 0, sizeof(dmacHw_CBLK_t));
	pCblk->module = module;
	pCblk->channel = channel;

	
	dmacHw_DMA_ENABLE(pCblk->module);
	
	dmacHw_RESET_CONTROL_LO(pCblk->module, pCblk->channel);
	dmacHw_RESET_CONTROL_HI(pCblk->module, pCblk->channel);
	dmacHw_RESET_CONFIG_LO(pCblk->module, pCblk->channel);
	dmacHw_RESET_CONFIG_HI(pCblk->module, pCblk->channel);

	
	dmacHw_TRAN_INT_CLEAR(pCblk->module, pCblk->channel);
	dmacHw_BLOCK_INT_CLEAR(pCblk->module, pCblk->channel);
	dmacHw_ERROR_INT_CLEAR(pCblk->module, pCblk->channel);

	
	dmacHw_TRAN_INT_DISABLE(pCblk->module, pCblk->channel);
	dmacHw_BLOCK_INT_DISABLE(pCblk->module, pCblk->channel);
	dmacHw_STRAN_INT_DISABLE(pCblk->module, pCblk->channel);
	dmacHw_DTRAN_INT_DISABLE(pCblk->module, pCblk->channel);
	dmacHw_ERROR_INT_DISABLE(pCblk->module, pCblk->channel);

	return 0;
}

uint32_t dmacHw_descriptorLen(uint32_t descCnt	
    ) {
	
	return (descCnt * sizeof(dmacHw_DESC_t)) + sizeof(dmacHw_DESC_RING_t) +
		sizeof(uint32_t);
}

int dmacHw_initDescriptor(void *pDescriptorVirt,	
			  uint32_t descriptorPhyAddr,	
			  uint32_t len,	
			  uint32_t num	
    ) {
	uint32_t i;
	dmacHw_DESC_RING_t *pRing;
	dmacHw_DESC_t *pDesc;

	
	if ((uint32_t) pDescriptorVirt & 0x00000003) {
		dmacHw_ASSERT(0);
		return -1;
	}

	
	if (len < dmacHw_descriptorLen(num)) {
		return -1;
	}

	pRing = dmacHw_GET_DESC_RING(pDescriptorVirt);
	pRing->pHead =
	    (dmacHw_DESC_t *) ((uint32_t) pRing + sizeof(dmacHw_DESC_RING_t));
	pRing->pFree = pRing->pTail = pRing->pEnd = pRing->pHead;
	pRing->pProg = dmacHw_DESC_INIT;
	
	pDesc = pRing->pHead;
	
	pRing->virt2PhyOffset = (uint32_t) pDescriptorVirt - descriptorPhyAddr;

	
	for (i = 0; i < num - 1; i++) {
		
		memset((void *)pDesc, 0, sizeof(dmacHw_DESC_t));
		
		pDesc->llpPhy = (uint32_t) (pDesc + 1) - pRing->virt2PhyOffset;
		
		pDesc->llp = (uint32_t) (pDesc + 1);
		
		pDesc->ctl.hi = dmacHw_DESC_FREE;
		
		pDesc++;
	}

	
	memset((void *)pDesc, 0, sizeof(dmacHw_DESC_t));
	pDesc->llpPhy = (uint32_t) pRing->pHead - pRing->virt2PhyOffset;
	pDesc->llp = (uint32_t) pRing->pHead;
	
	pDesc->ctl.hi = dmacHw_DESC_FREE;
	
	pRing->num = num;
	return 0;
}

int dmacHw_configChannel(dmacHw_HANDLE_t handle,	
			 dmacHw_CONFIG_t *pConfig	
    ) {
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);
	uint32_t cfgHigh = 0;
	int srcTrSize;
	int dstTrSize;

	pCblk->varDataStarted = 0;
	pCblk->userData = NULL;

	cfgHigh =
	    dmacHw_REG_CFG_HI_FIFO_ENOUGH | dmacHw_REG_CFG_HI_AHB_HPROT_1 |
	    dmacHw_SRC_PERI_INTF(pConfig->
				 srcPeripheralPort) |
	    dmacHw_DST_PERI_INTF(pConfig->dstPeripheralPort);
	
	dmacHw_SET_CHANNEL_PRIORITY(pCblk->module, pCblk->channel,
				    pConfig->channelPriority);

	if (pConfig->dstStatusRegisterAddress != 0) {
		
		cfgHigh |= dmacHw_REG_CFG_HI_UPDATE_DST_STAT;
		
		dmacHw_SET_DSTATAR(pCblk->module, pCblk->channel,
				   pConfig->dstStatusRegisterAddress);
	}

	if (pConfig->srcStatusRegisterAddress != 0) {
		
		cfgHigh |= dmacHw_REG_CFG_HI_UPDATE_SRC_STAT;
		
		dmacHw_SET_SSTATAR(pCblk->module, pCblk->channel,
				   pConfig->srcStatusRegisterAddress);
	}
	
	dmacHw_GET_CONFIG_HI(pCblk->module, pCblk->channel) = cfgHigh;

	
	dmacHw_TRAN_INT_CLEAR(pCblk->module, pCblk->channel);
	dmacHw_BLOCK_INT_CLEAR(pCblk->module, pCblk->channel);
	dmacHw_ERROR_INT_CLEAR(pCblk->module, pCblk->channel);

	
	if (pConfig->blockTransferInterrupt == dmacHw_INTERRUPT_ENABLE) {
		dmacHw_BLOCK_INT_ENABLE(pCblk->module, pCblk->channel);
	} else {
		dmacHw_BLOCK_INT_DISABLE(pCblk->module, pCblk->channel);
	}
	
	if (pConfig->completeTransferInterrupt == dmacHw_INTERRUPT_ENABLE) {
		dmacHw_TRAN_INT_ENABLE(pCblk->module, pCblk->channel);
	} else {
		dmacHw_TRAN_INT_DISABLE(pCblk->module, pCblk->channel);
	}
	
	if (pConfig->errorInterrupt == dmacHw_INTERRUPT_ENABLE) {
		dmacHw_ERROR_INT_ENABLE(pCblk->module, pCblk->channel);
	} else {
		dmacHw_ERROR_INT_DISABLE(pCblk->module, pCblk->channel);
	}
	
	if (pConfig->srcGatherWidth) {
		srcTrSize =
		    dmacHw_GetTrWidthInBytes(pConfig->srcMaxTransactionWidth);
		if (!
		    ((pConfig->srcGatherWidth % srcTrSize)
		     && (pConfig->srcGatherJump % srcTrSize))) {
			dmacHw_REG_SGR_LO(pCblk->module, pCblk->channel) =
			    ((pConfig->srcGatherWidth /
			      srcTrSize) << 20) | (pConfig->srcGatherJump /
						   srcTrSize);
		} else {
			return -1;
		}
	}
	
	if (pConfig->dstScatterWidth) {
		dstTrSize =
		    dmacHw_GetTrWidthInBytes(pConfig->dstMaxTransactionWidth);
		if (!
		    ((pConfig->dstScatterWidth % dstTrSize)
		     && (pConfig->dstScatterJump % dstTrSize))) {
			dmacHw_REG_DSR_LO(pCblk->module, pCblk->channel) =
			    ((pConfig->dstScatterWidth /
			      dstTrSize) << 20) | (pConfig->dstScatterJump /
						   dstTrSize);
		} else {
			return -1;
		}
	}
	return 0;
}

dmacHw_TRANSFER_STATUS_e dmacHw_transferCompleted(dmacHw_HANDLE_t handle	
    ) {
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);

	if (CHANNEL_BUSY(pCblk->module, pCblk->channel)) {
		return dmacHw_TRANSFER_STATUS_BUSY;
	} else if (dmacHw_REG_INT_RAW_ERROR(pCblk->module) &
		   (0x00000001 << pCblk->channel)) {
		return dmacHw_TRANSFER_STATUS_ERROR;
	}

	return dmacHw_TRANSFER_STATUS_DONE;
}

int dmacHw_setDataDescriptor(dmacHw_CONFIG_t *pConfig,	
			     void *pDescriptor,	
			     void *pSrcAddr,	
			     void *pDstAddr,	
			     size_t dataLen	
    ) {
	dmacHw_TRANSACTION_WIDTH_e dstTrWidth;
	dmacHw_TRANSACTION_WIDTH_e srcTrWidth;
	dmacHw_DESC_RING_t *pRing = dmacHw_GET_DESC_RING(pDescriptor);
	dmacHw_DESC_t *pStart;
	dmacHw_DESC_t *pProg;
	int srcTs = 0;
	int blkTs = 0;
	int oddSize = 0;
	int descCount = 0;
	int count = 0;
	int dstTrSize = 0;
	int srcTrSize = 0;
	uint32_t maxBlockSize = dmacHw_MAX_BLOCKSIZE;

	dstTrSize = dmacHw_GetTrWidthInBytes(pConfig->dstMaxTransactionWidth);
	srcTrSize = dmacHw_GetTrWidthInBytes(pConfig->srcMaxTransactionWidth);

	
	if ((pSrcAddr == NULL) || (pDstAddr == NULL) || (dataLen == 0)) {
		
		return -1;
	}

	
	if ((pConfig->srcGatherWidth % srcTrSize)
	    || (pConfig->dstScatterWidth % dstTrSize)) {
		return -2;
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

	
	pProg = pRing->pHead;
	for (count = 0; (descCount <= pRing->num) && (count < descCount);
	     count++) {
		if ((pProg->ctl.hi & dmacHw_DESC_FREE) == 0) {
			
			return -3;
		}
		pProg = (dmacHw_DESC_t *) pProg->llp;
	}

	
	pStart = pProg = pRing->pHead;
	
	while (count) {
		
		pProg->ctl.lo = 0;
		
		if (pConfig->srcGatherWidth) {
			pProg->ctl.lo |= dmacHw_REG_CTL_SG_ENABLE;
		}
		
		if (pConfig->dstScatterWidth) {
			pProg->ctl.lo |= dmacHw_REG_CTL_DS_ENABLE;
		}
		
		pProg->sar = (uint32_t) pSrcAddr;
		pProg->dar = (uint32_t) pDstAddr;
		
		if (pProg == pRing->pHead) {
			pProg->devCtl = dmacHw_FREE_USER_MEMORY;
		} else {
			pProg->devCtl = 0;
		}

		blkTs = srcTs;

		
		if (count == 1) {
			
			pProg->ctl.lo &=
			    ~(dmacHw_REG_CTL_LLP_DST_EN |
			      dmacHw_REG_CTL_LLP_SRC_EN);
			
			if (oddSize) {
				
				switch (pConfig->transferType) {
				case dmacHw_TRANSFER_TYPE_PERIPHERAL_TO_MEM:
					dstTrWidth =
					    dmacHw_DST_TRANSACTION_WIDTH_8;
					blkTs =
					    (oddSize / srcTrSize) +
					    ((oddSize % srcTrSize) ? 1 : 0);
					break;
				case dmacHw_TRANSFER_TYPE_MEM_TO_PERIPHERAL:
					srcTrWidth =
					    dmacHw_SRC_TRANSACTION_WIDTH_8;
					blkTs = oddSize;
					break;
				case dmacHw_TRANSFER_TYPE_MEM_TO_MEM:
					srcTrWidth =
					    dmacHw_SRC_TRANSACTION_WIDTH_8;
					dstTrWidth =
					    dmacHw_DST_TRANSACTION_WIDTH_8;
					blkTs = oddSize;
					break;
				case dmacHw_TRANSFER_TYPE_PERIPHERAL_TO_PERIPHERAL:
					
					break;
				}
			} else {
				srcTs -= blkTs;
			}
		} else {
			if (srcTs / maxBlockSize) {
				blkTs = maxBlockSize;
			}
			
			srcTs -= blkTs;
		}
		
		dmacHw_ASSERT(blkTs > 0);
		
		if (pConfig->flowControler == dmacHw_FLOW_CONTROL_DMA) {
			pProg->ctl.lo |= pConfig->transferType |
			    pConfig->srcUpdate |
			    pConfig->dstUpdate |
			    srcTrWidth |
			    dstTrWidth |
			    pConfig->srcMaxBurstWidth |
			    pConfig->dstMaxBurstWidth |
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
			pProg->ctl.lo |= transferType |
			    pConfig->srcUpdate |
			    pConfig->dstUpdate |
			    srcTrWidth |
			    dstTrWidth |
			    pConfig->srcMaxBurstWidth |
			    pConfig->dstMaxBurstWidth |
			    pConfig->srcMasterInterface |
			    pConfig->dstMasterInterface | dmacHw_REG_CTL_INT_EN;
		}

		
		pProg->ctl.hi = blkTs & dmacHw_REG_CTL_BLOCK_TS_MASK;
		
		if (count > 1) {
			
			pProg = (dmacHw_DESC_t *) pProg->llp;

			
			switch (pConfig->transferType) {
			case dmacHw_TRANSFER_TYPE_PERIPHERAL_TO_MEM:
				if (pConfig->dstScatterWidth) {
					pDstAddr =
					    (char *)pDstAddr +
					    blkTs * srcTrSize +
					    (((blkTs * srcTrSize) /
					      pConfig->dstScatterWidth) *
					     pConfig->dstScatterJump);
				} else {
					pDstAddr =
					    (char *)pDstAddr +
					    blkTs * srcTrSize;
				}
				break;
			case dmacHw_TRANSFER_TYPE_MEM_TO_PERIPHERAL:
				if (pConfig->srcGatherWidth) {
					pSrcAddr =
					    (char *)pDstAddr +
					    blkTs * srcTrSize +
					    (((blkTs * srcTrSize) /
					      pConfig->srcGatherWidth) *
					     pConfig->srcGatherJump);
				} else {
					pSrcAddr =
					    (char *)pSrcAddr +
					    blkTs * srcTrSize;
				}
				break;
			case dmacHw_TRANSFER_TYPE_MEM_TO_MEM:
				if (pConfig->dstScatterWidth) {
					pDstAddr =
					    (char *)pDstAddr +
					    blkTs * srcTrSize +
					    (((blkTs * srcTrSize) /
					      pConfig->dstScatterWidth) *
					     pConfig->dstScatterJump);
				} else {
					pDstAddr =
					    (char *)pDstAddr +
					    blkTs * srcTrSize;
				}

				if (pConfig->srcGatherWidth) {
					pSrcAddr =
					    (char *)pDstAddr +
					    blkTs * srcTrSize +
					    (((blkTs * srcTrSize) /
					      pConfig->srcGatherWidth) *
					     pConfig->srcGatherJump);
				} else {
					pSrcAddr =
					    (char *)pSrcAddr +
					    blkTs * srcTrSize;
				}
				break;
			case dmacHw_TRANSFER_TYPE_PERIPHERAL_TO_PERIPHERAL:
				
				break;
			default:
				dmacHw_ASSERT(0);
			}
		} else {
			
			dmacHw_ASSERT(srcTs == 0);
		}
		count--;
	}

	
	if (pRing->pProg == dmacHw_DESC_INIT) {
		pRing->pProg = pStart;
	}
	
	pRing->pEnd = pProg;
	
	pRing->pHead = (dmacHw_DESC_t *) pProg->llp;
	if (!dmacHw_DST_IS_MEMORY(pConfig->transferType)) {
		pRing->pTail = pRing->pHead;
	}
	return 0;
}

uint32_t dmacHw_getDmaControllerAttribute(dmacHw_HANDLE_t handle,	
					  dmacHw_CONTROLLER_ATTRIB_e attr	
    ) {
	dmacHw_CBLK_t *pCblk = dmacHw_HANDLE_TO_CBLK(handle);

	switch (attr) {
	case dmacHw_CONTROLLER_ATTRIB_CHANNEL_NUM:
		return dmacHw_GET_NUM_CHANNEL(pCblk->module);
	case dmacHw_CONTROLLER_ATTRIB_CHANNEL_MAX_BLOCK_SIZE:
		return (1 <<
			 (dmacHw_GET_MAX_BLOCK_SIZE
			  (pCblk->module, pCblk->module) + 2)) - 8;
	case dmacHw_CONTROLLER_ATTRIB_MASTER_INTF_NUM:
		return dmacHw_GET_NUM_INTERFACE(pCblk->module);
	case dmacHw_CONTROLLER_ATTRIB_CHANNEL_BUS_WIDTH:
		return 32 << dmacHw_GET_CHANNEL_DATA_WIDTH(pCblk->module,
							   pCblk->channel);
	case dmacHw_CONTROLLER_ATTRIB_CHANNEL_FIFO_SIZE:
		return GetFifoSize(handle);
	}
	dmacHw_ASSERT(0);
	return 0;
}
