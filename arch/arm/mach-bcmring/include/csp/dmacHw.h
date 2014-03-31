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

#ifndef _DMACHW_H
#define _DMACHW_H

#include <stddef.h>

#include <csp/stdint.h>
#include <mach/csp/dmacHw_reg.h>

#define dmacHw_MAKE_CHANNEL_ID(m, c)         (m << 8 | c)

typedef enum {
	dmacHw_CHANNEL_PRIORITY_0 = dmacHw_REG_CFG_LO_CH_PRIORITY_0,	
	dmacHw_CHANNEL_PRIORITY_1 = dmacHw_REG_CFG_LO_CH_PRIORITY_1,	
	dmacHw_CHANNEL_PRIORITY_2 = dmacHw_REG_CFG_LO_CH_PRIORITY_2,	
	dmacHw_CHANNEL_PRIORITY_3 = dmacHw_REG_CFG_LO_CH_PRIORITY_3,	
	dmacHw_CHANNEL_PRIORITY_4 = dmacHw_REG_CFG_LO_CH_PRIORITY_4,	
	dmacHw_CHANNEL_PRIORITY_5 = dmacHw_REG_CFG_LO_CH_PRIORITY_5,	
	dmacHw_CHANNEL_PRIORITY_6 = dmacHw_REG_CFG_LO_CH_PRIORITY_6,	
	dmacHw_CHANNEL_PRIORITY_7 = dmacHw_REG_CFG_LO_CH_PRIORITY_7	
} dmacHw_CHANNEL_PRIORITY_e;

typedef enum {
	dmacHw_SRC_MASTER_INTERFACE_1 = dmacHw_REG_CTL_SMS_1,	
	dmacHw_SRC_MASTER_INTERFACE_2 = dmacHw_REG_CTL_SMS_2,	
	dmacHw_DST_MASTER_INTERFACE_1 = dmacHw_REG_CTL_DMS_1,	
	dmacHw_DST_MASTER_INTERFACE_2 = dmacHw_REG_CTL_DMS_2	
} dmacHw_MASTER_INTERFACE_e;

typedef enum {
	dmacHw_SRC_TRANSACTION_WIDTH_8 = dmacHw_REG_CTL_SRC_TR_WIDTH_8,	
	dmacHw_SRC_TRANSACTION_WIDTH_16 = dmacHw_REG_CTL_SRC_TR_WIDTH_16,	
	dmacHw_SRC_TRANSACTION_WIDTH_32 = dmacHw_REG_CTL_SRC_TR_WIDTH_32,	
	dmacHw_SRC_TRANSACTION_WIDTH_64 = dmacHw_REG_CTL_SRC_TR_WIDTH_64,	
	dmacHw_DST_TRANSACTION_WIDTH_8 = dmacHw_REG_CTL_DST_TR_WIDTH_8,	
	dmacHw_DST_TRANSACTION_WIDTH_16 = dmacHw_REG_CTL_DST_TR_WIDTH_16,	
	dmacHw_DST_TRANSACTION_WIDTH_32 = dmacHw_REG_CTL_DST_TR_WIDTH_32,	
	dmacHw_DST_TRANSACTION_WIDTH_64 = dmacHw_REG_CTL_DST_TR_WIDTH_64	
} dmacHw_TRANSACTION_WIDTH_e;

typedef enum {
	dmacHw_SRC_BURST_WIDTH_0 = dmacHw_REG_CTL_SRC_MSIZE_0,	
	dmacHw_SRC_BURST_WIDTH_4 = dmacHw_REG_CTL_SRC_MSIZE_4,	
	dmacHw_SRC_BURST_WIDTH_8 = dmacHw_REG_CTL_SRC_MSIZE_8,	
	dmacHw_SRC_BURST_WIDTH_16 = dmacHw_REG_CTL_SRC_MSIZE_16,	
	dmacHw_DST_BURST_WIDTH_0 = dmacHw_REG_CTL_DST_MSIZE_0,	
	dmacHw_DST_BURST_WIDTH_4 = dmacHw_REG_CTL_DST_MSIZE_4,	
	dmacHw_DST_BURST_WIDTH_8 = dmacHw_REG_CTL_DST_MSIZE_8,	
	dmacHw_DST_BURST_WIDTH_16 = dmacHw_REG_CTL_DST_MSIZE_16	
} dmacHw_BURST_WIDTH_e;

typedef enum {
	dmacHw_TRANSFER_TYPE_MEM_TO_MEM = dmacHw_REG_CTL_TTFC_MM_DMAC,	
	dmacHw_TRANSFER_TYPE_PERIPHERAL_TO_MEM = dmacHw_REG_CTL_TTFC_PM_DMAC,	
	dmacHw_TRANSFER_TYPE_MEM_TO_PERIPHERAL = dmacHw_REG_CTL_TTFC_MP_DMAC,	
	dmacHw_TRANSFER_TYPE_PERIPHERAL_TO_PERIPHERAL = dmacHw_REG_CTL_TTFC_PP_DMAC	
} dmacHw_TRANSFER_TYPE_e;

typedef enum {
	dmacHw_TRANSFER_MODE_PERREQUEST,	
	dmacHw_TRANSFER_MODE_CONTINUOUS,	
	dmacHw_TRANSFER_MODE_PERIODIC	
} dmacHw_TRANSFER_MODE_e;

typedef enum {
	dmacHw_SRC_ADDRESS_UPDATE_MODE_INC = dmacHw_REG_CTL_SINC_INC,	
	dmacHw_SRC_ADDRESS_UPDATE_MODE_DEC = dmacHw_REG_CTL_SINC_DEC,	
	dmacHw_DST_ADDRESS_UPDATE_MODE_INC = dmacHw_REG_CTL_DINC_INC,	
	dmacHw_DST_ADDRESS_UPDATE_MODE_DEC = dmacHw_REG_CTL_DINC_DEC,	
	dmacHw_SRC_ADDRESS_UPDATE_MODE_NC = dmacHw_REG_CTL_SINC_NC,	
	dmacHw_DST_ADDRESS_UPDATE_MODE_NC = dmacHw_REG_CTL_DINC_NC	
} dmacHw_ADDRESS_UPDATE_MODE_e;

typedef enum {
	dmacHw_FLOW_CONTROL_DMA,	
	dmacHw_FLOW_CONTROL_PERIPHERAL	
} dmacHw_FLOW_CONTROL_e;

typedef enum {
	dmacHw_TRANSFER_STATUS_BUSY,	
	dmacHw_TRANSFER_STATUS_DONE,	
	dmacHw_TRANSFER_STATUS_ERROR	
} dmacHw_TRANSFER_STATUS_e;

typedef enum {
	dmacHw_INTERRUPT_DISABLE,	
	dmacHw_INTERRUPT_ENABLE	
} dmacHw_INTERRUPT_e;

typedef enum {
	dmacHw_INTERRUPT_STATUS_NONE = 0x0,	
	dmacHw_INTERRUPT_STATUS_TRANS = 0x1,	
	dmacHw_INTERRUPT_STATUS_BLOCK = 0x2,	
	dmacHw_INTERRUPT_STATUS_ERROR = 0x4	
} dmacHw_INTERRUPT_STATUS_e;

typedef enum {
	dmacHw_CONTROLLER_ATTRIB_CHANNEL_NUM,	
	dmacHw_CONTROLLER_ATTRIB_CHANNEL_MAX_BLOCK_SIZE,	
	dmacHw_CONTROLLER_ATTRIB_MASTER_INTF_NUM,	
	dmacHw_CONTROLLER_ATTRIB_CHANNEL_BUS_WIDTH,	
	dmacHw_CONTROLLER_ATTRIB_CHANNEL_FIFO_SIZE	
} dmacHw_CONTROLLER_ATTRIB_e;

typedef unsigned long dmacHw_HANDLE_t;	
typedef uint32_t dmacHw_ID_t;	
typedef struct {
	uint32_t srcPeripheralPort;	
	uint32_t dstPeripheralPort;	
	uint32_t srcStatusRegisterAddress;	
	uint32_t dstStatusRegisterAddress;	

	uint32_t srcGatherWidth;	
	uint32_t srcGatherJump;	
	uint32_t dstScatterWidth;	
	uint32_t dstScatterJump;	
	uint32_t maxDataPerBlock;	

	dmacHw_ADDRESS_UPDATE_MODE_e srcUpdate;	
	dmacHw_ADDRESS_UPDATE_MODE_e dstUpdate;	
	dmacHw_TRANSFER_TYPE_e transferType;	
	dmacHw_TRANSFER_MODE_e transferMode;	
	dmacHw_MASTER_INTERFACE_e srcMasterInterface;	
	dmacHw_MASTER_INTERFACE_e dstMasterInterface;	
	dmacHw_TRANSACTION_WIDTH_e srcMaxTransactionWidth;	
	dmacHw_TRANSACTION_WIDTH_e dstMaxTransactionWidth;	
	dmacHw_BURST_WIDTH_e srcMaxBurstWidth;	
	dmacHw_BURST_WIDTH_e dstMaxBurstWidth;	
	dmacHw_INTERRUPT_e blockTransferInterrupt;	
	dmacHw_INTERRUPT_e completeTransferInterrupt;	
	dmacHw_INTERRUPT_e errorInterrupt;	
	dmacHw_CHANNEL_PRIORITY_e channelPriority;	
	dmacHw_FLOW_CONTROL_e flowControler;	
} dmacHw_CONFIG_t;

void dmacHw_initDma(void);

void dmacHw_exitDma(void);

dmacHw_HANDLE_t dmacHw_getChannelHandle(dmacHw_ID_t channelId	
    );

int dmacHw_initChannel(dmacHw_HANDLE_t handle	
    );

int dmacHw_calculateDescriptorCount(dmacHw_CONFIG_t *pConfig,	
				    void *pSrcAddr,	
				    void *pDstAddr,	
				    size_t dataLen	
    );

int dmacHw_initDescriptor(void *pDescriptorVirt,	
			  uint32_t descriptorPhyAddr,	
			  uint32_t len,	
			  uint32_t num	
    );

uint32_t dmacHw_descriptorLen(uint32_t descCnt	
    );

int dmacHw_configChannel(dmacHw_HANDLE_t handle,	
			 dmacHw_CONFIG_t *pConfig	
    );

int dmacHw_setDataDescriptor(dmacHw_CONFIG_t *pConfig,	
			     void *pDescriptor,	
			     void *pSrcAddr,	
			     void *pDstAddr,	
			     size_t dataLen	
    );

dmacHw_TRANSFER_STATUS_e dmacHw_transferCompleted(dmacHw_HANDLE_t handle	
    );

int dmacHw_setControlDescriptor(dmacHw_CONFIG_t *pConfig,	
				void *pDescriptor,	
				uint32_t ctlAddress,	
				uint32_t control	
    );

int dmacHw_readTransferredData(dmacHw_HANDLE_t handle,	
			       dmacHw_CONFIG_t *pConfig,	
			       void *pDescriptor,	
			       void **ppBbuf,	
			       size_t *pLlen	
    );

int dmacHw_setVariableDataDescriptor(dmacHw_HANDLE_t handle,	
				     dmacHw_CONFIG_t *pConfig,	
				     void *pDescriptor,	
				     uint32_t srcAddr,	
				     void *(*fpAlloc) (int len),	
				     int len,	
				     int num	
    );

void dmacHw_initiateTransfer(dmacHw_HANDLE_t handle,	
			     dmacHw_CONFIG_t *pConfig,	
			     void *pDescriptor	
    );

void dmacHw_resetDescriptorControl(void *pDescriptor	
    );

void dmacHw_stopTransfer(dmacHw_HANDLE_t handle	
    );

uint32_t dmacHw_descriptorPending(dmacHw_HANDLE_t handle,	
				  void *pDescriptor	
    );

int dmacHw_freeMem(dmacHw_CONFIG_t *pConfig,	
		   void *pDescriptor,	
		   void (*fpFree) (void *)	
    );

void dmacHw_clearInterrupt(dmacHw_HANDLE_t handle	
    );

dmacHw_INTERRUPT_STATUS_e dmacHw_getInterruptStatus(dmacHw_HANDLE_t handle	
    );

dmacHw_HANDLE_t dmacHw_getInterruptSource(void);

void dmacHw_setChannelUserData(dmacHw_HANDLE_t handle,	
			       void *userData	
    );

void *dmacHw_getChannelUserData(dmacHw_HANDLE_t handle	
    );

void dmacHw_printDebugInfo(dmacHw_HANDLE_t handle,	
			   void *pDescriptor,	
			   int (*fpPrint) (const char *, ...)	
    );

uint32_t dmacHw_getDmaControllerAttribute(dmacHw_HANDLE_t handle,	
					  dmacHw_CONTROLLER_ATTRIB_e attr	
    );

#endif 
