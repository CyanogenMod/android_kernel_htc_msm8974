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


#if !defined(ASM_ARM_ARCH_BCMRING_DMA_H)
#define ASM_ARM_ARCH_BCMRING_DMA_H


#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <csp/dmacHw.h>
#include <mach/timer.h>



#define DMA_DEBUG_TRACK_RESERVATION   1

#define DMA_NUM_CONTROLLERS     2
#define DMA_NUM_CHANNELS        8	

typedef enum {
	DMA_DEVICE_MEM_TO_MEM,	
	DMA_DEVICE_I2S0_DEV_TO_MEM,
	DMA_DEVICE_I2S0_MEM_TO_DEV,
	DMA_DEVICE_I2S1_DEV_TO_MEM,
	DMA_DEVICE_I2S1_MEM_TO_DEV,
	DMA_DEVICE_APM_CODEC_A_DEV_TO_MEM,
	DMA_DEVICE_APM_CODEC_A_MEM_TO_DEV,
	DMA_DEVICE_APM_CODEC_B_DEV_TO_MEM,
	DMA_DEVICE_APM_CODEC_B_MEM_TO_DEV,
	DMA_DEVICE_APM_CODEC_C_DEV_TO_MEM,	
	DMA_DEVICE_APM_PCM0_DEV_TO_MEM,
	DMA_DEVICE_APM_PCM0_MEM_TO_DEV,
	DMA_DEVICE_APM_PCM1_DEV_TO_MEM,
	DMA_DEVICE_APM_PCM1_MEM_TO_DEV,
	DMA_DEVICE_SPUM_DEV_TO_MEM,
	DMA_DEVICE_SPUM_MEM_TO_DEV,
	DMA_DEVICE_SPIH_DEV_TO_MEM,
	DMA_DEVICE_SPIH_MEM_TO_DEV,
	DMA_DEVICE_UART_A_DEV_TO_MEM,
	DMA_DEVICE_UART_A_MEM_TO_DEV,
	DMA_DEVICE_UART_B_DEV_TO_MEM,
	DMA_DEVICE_UART_B_MEM_TO_DEV,
	DMA_DEVICE_PIF_MEM_TO_DEV,
	DMA_DEVICE_PIF_DEV_TO_MEM,
	DMA_DEVICE_ESW_DEV_TO_MEM,
	DMA_DEVICE_ESW_MEM_TO_DEV,
	DMA_DEVICE_VPM_MEM_TO_MEM,
	DMA_DEVICE_CLCD_MEM_TO_MEM,
	DMA_DEVICE_NAND_MEM_TO_MEM,
	DMA_DEVICE_MEM_TO_VRAM,
	DMA_DEVICE_VRAM_TO_MEM,

	

	DMA_NUM_DEVICE_ENTRIES,
	DMA_DEVICE_NONE = 0xff,	

} DMA_Device_t;


#define DMA_INVALID_HANDLE  ((DMA_Handle_t) -1)

typedef int DMA_Handle_t;


typedef struct {
	void *virtAddr;		
	dma_addr_t physAddr;	
	int descriptorsAllocated;	
	size_t bytesAllocated;	

} DMA_DescriptorRing_t;



#define DMA_HANDLER_REASON_BLOCK_COMPLETE       dmacHw_INTERRUPT_STATUS_BLOCK
#define DMA_HANDLER_REASON_TRANSFER_COMPLETE    dmacHw_INTERRUPT_STATUS_TRANS
#define DMA_HANDLER_REASON_ERROR                dmacHw_INTERRUPT_STATUS_ERROR

typedef void (*DMA_DeviceHandler_t) (DMA_Device_t dev, int reason,
				     void *userData);

#define DMA_DEVICE_FLAG_ON_DMA0             0x00000001
#define DMA_DEVICE_FLAG_ON_DMA1             0x00000002
#define DMA_DEVICE_FLAG_PORT_PER_DMAC       0x00000004	
#define DMA_DEVICE_FLAG_ALLOC_DMA1_FIRST    0x00000008	
#define DMA_DEVICE_FLAG_IS_DEDICATED        0x00000100
#define DMA_DEVICE_FLAG_NO_ISR              0x00000200
#define DMA_DEVICE_FLAG_ALLOW_LARGE_FIFO    0x00000400
#define DMA_DEVICE_FLAG_IN_USE              0x00000800	


typedef struct {
	uint32_t flags;		
	uint8_t dedicatedController;	
	uint8_t dedicatedChannel;	
	const char *name;	

	uint32_t dmacPort[DMA_NUM_CONTROLLERS];	

	dmacHw_CONFIG_t config;	

	void *userData;		
	DMA_DeviceHandler_t devHandler;	

	timer_tick_count_t transferStartTime;	

	
	
	

	uint64_t numTransfers;	
	uint64_t transferTicks;	
	uint64_t transferBytes;	
	uint32_t timesBlocked;	
	uint32_t numBytes;	

	
	
	

	DMA_DescriptorRing_t ring;	

	
	
	

	uint32_t prevNumBytes;
	dma_addr_t prevSrcData;
	dma_addr_t prevDstData;

} DMA_DeviceAttribute_t;



#define DMA_CHANNEL_FLAG_IN_USE         0x00000001
#define DMA_CHANNEL_FLAG_IS_DEDICATED   0x00000002
#define DMA_CHANNEL_FLAG_NO_ISR         0x00000004
#define DMA_CHANNEL_FLAG_LARGE_FIFO     0x00000008

typedef struct {
	uint32_t flags;		
	DMA_Device_t devType;	
	DMA_Device_t lastDevType;	
	char name[20];		

#if (DMA_DEBUG_TRACK_RESERVATION)
	const char *fileName;	
	int lineNum;		
#endif
	dmacHw_HANDLE_t dmacHwHandle;	

} DMA_Channel_t;


typedef struct {
	DMA_Channel_t channel[DMA_NUM_CHANNELS];

} DMA_Controller_t;


typedef struct {
	struct semaphore lock;	
	wait_queue_head_t freeChannelQ;

	DMA_Controller_t controller[DMA_NUM_CONTROLLERS];

} DMA_Global_t;


extern DMA_DeviceAttribute_t DMA_gDeviceAttribute[DMA_NUM_DEVICE_ENTRIES];


#if defined(__KERNEL__)


int dma_init(void);

#if (DMA_DEBUG_TRACK_RESERVATION)
DMA_Handle_t dma_request_channel_dbg(DMA_Device_t dev, const char *fileName,
				     int lineNum);
#define dma_request_channel(dev)  dma_request_channel_dbg(dev, __FILE__, __LINE__)
#else


DMA_Handle_t dma_request_channel(DMA_Device_t dev	
    );
#endif


int dma_free_channel(DMA_Handle_t channel	
    );


int dma_device_is_channel_shared(DMA_Device_t dev	
    );


int dma_alloc_descriptor_ring(DMA_DescriptorRing_t *ring,	
			      int numDescriptors	
    );


void dma_free_descriptor_ring(DMA_DescriptorRing_t *ring	
    );


int dma_init_descriptor_ring(DMA_DescriptorRing_t *ring,	
			     int numDescriptors	
    );


int dma_calculate_descriptor_count(DMA_Device_t device,	
				   dma_addr_t srcData,	
				   dma_addr_t dstData,	
				   size_t numBytes	
    );


int dma_add_descriptors(DMA_DescriptorRing_t *ring,	
			DMA_Device_t device,	
			dma_addr_t srcData,	
			dma_addr_t dstData,	
			size_t numBytes	
    );


int dma_set_device_descriptor_ring(DMA_Device_t device,	
				   DMA_DescriptorRing_t *ring	
    );


int dma_get_device_descriptor_ring(DMA_Device_t device,	
				   DMA_DescriptorRing_t *ring	
    );


int dma_alloc_descriptors(DMA_Handle_t handle,	
			  dmacHw_TRANSFER_TYPE_e transferType,	
			  dma_addr_t srcData,	
			  dma_addr_t dstData,	
			  size_t numBytes	
    );


int dma_alloc_double_dst_descriptors(DMA_Handle_t handle,	
				     dma_addr_t srcData,	
				     dma_addr_t dstData1,	
				     dma_addr_t dstData2,	
				     size_t numBytes	
    );


int dma_start_transfer(DMA_Handle_t handle);


int dma_stop_transfer(DMA_Handle_t handle);


int dma_wait_transfer_done(DMA_Handle_t handle);


int dma_transfer(DMA_Handle_t handle,	
		 dmacHw_TRANSFER_TYPE_e transferType,	
		 dma_addr_t srcData,	
		 dma_addr_t dstData,	
		 size_t numBytes	
    );


static inline int dma_transfer_to_device(DMA_Handle_t handle,	
					 dma_addr_t srcData,	
					 dma_addr_t dstData,	
					 size_t numBytes	
    ) {
	return dma_transfer(handle,
			    dmacHw_TRANSFER_TYPE_MEM_TO_PERIPHERAL,
			    srcData, dstData, numBytes);
}


static inline int dma_transfer_from_device(DMA_Handle_t handle,	
					   dma_addr_t srcData,	
					   dma_addr_t dstData,	
					   size_t numBytes	
    ) {
	return dma_transfer(handle,
			    dmacHw_TRANSFER_TYPE_PERIPHERAL_TO_MEM,
			    srcData, dstData, numBytes);
}


static inline int dma_transfer_mem_to_mem(DMA_Handle_t handle,	
					  dma_addr_t srcData,	
					  dma_addr_t dstData,	
					  size_t numBytes	
    ) {
	return dma_transfer(handle,
			    dmacHw_TRANSFER_TYPE_MEM_TO_MEM,
			    srcData, dstData, numBytes);
}


int dma_set_device_handler(DMA_Device_t dev,	
			   DMA_DeviceHandler_t devHandler,	
			   void *userData	
    );

#endif

#endif 
