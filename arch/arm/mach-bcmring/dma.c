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



#include <linux/module.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/irqreturn.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>

#include <mach/timer.h>

#include <linux/pfn.h>
#include <linux/atomic.h>
#include <mach/dma.h>



#define MAKE_HANDLE(controllerIdx, channelIdx)    (((controllerIdx) << 4) | (channelIdx))

#define CONTROLLER_FROM_HANDLE(handle)    (((handle) >> 4) & 0x0f)
#define CHANNEL_FROM_HANDLE(handle)       ((handle) & 0x0f)



static DMA_Global_t gDMA;
static struct proc_dir_entry *gDmaDir;

#include "dma_device.c"




static int dma_proc_read_channels(char *buf, char **start, off_t offset,
				  int count, int *eof, void *data)
{
	int controllerIdx;
	int channelIdx;
	int limit = count - 200;
	int len = 0;
	DMA_Channel_t *channel;

	if (down_interruptible(&gDMA.lock) < 0) {
		return -ERESTARTSYS;
	}

	for (controllerIdx = 0; controllerIdx < DMA_NUM_CONTROLLERS;
	     controllerIdx++) {
		for (channelIdx = 0; channelIdx < DMA_NUM_CHANNELS;
		     channelIdx++) {
			if (len >= limit) {
				break;
			}

			channel =
			    &gDMA.controller[controllerIdx].channel[channelIdx];

			len +=
			    sprintf(buf + len, "%d:%d ", controllerIdx,
				    channelIdx);

			if ((channel->flags & DMA_CHANNEL_FLAG_IS_DEDICATED) !=
			    0) {
				len +=
				    sprintf(buf + len, "Dedicated for %s ",
					    DMA_gDeviceAttribute[channel->
								 devType].name);
			} else {
				len += sprintf(buf + len, "Shared ");
			}

			if ((channel->flags & DMA_CHANNEL_FLAG_NO_ISR) != 0) {
				len += sprintf(buf + len, "No ISR ");
			}

			if ((channel->flags & DMA_CHANNEL_FLAG_LARGE_FIFO) != 0) {
				len += sprintf(buf + len, "Fifo: 128 ");
			} else {
				len += sprintf(buf + len, "Fifo: 64  ");
			}

			if ((channel->flags & DMA_CHANNEL_FLAG_IN_USE) != 0) {
				len +=
				    sprintf(buf + len, "InUse by %s",
					    DMA_gDeviceAttribute[channel->
								 devType].name);
#if (DMA_DEBUG_TRACK_RESERVATION)
				len +=
				    sprintf(buf + len, " (%s:%d)",
					    channel->fileName,
					    channel->lineNum);
#endif
			} else {
				len += sprintf(buf + len, "Avail ");
			}

			if (channel->lastDevType != DMA_DEVICE_NONE) {
				len +=
				    sprintf(buf + len, "Last use: %s ",
					    DMA_gDeviceAttribute[channel->
								 lastDevType].
					    name);
			}

			len += sprintf(buf + len, "\n");
		}
	}
	up(&gDMA.lock);
	*eof = 1;

	return len;
}


static int dma_proc_read_devices(char *buf, char **start, off_t offset,
				 int count, int *eof, void *data)
{
	int limit = count - 200;
	int len = 0;
	int devIdx;

	if (down_interruptible(&gDMA.lock) < 0) {
		return -ERESTARTSYS;
	}

	for (devIdx = 0; devIdx < DMA_NUM_DEVICE_ENTRIES; devIdx++) {
		DMA_DeviceAttribute_t *devAttr = &DMA_gDeviceAttribute[devIdx];

		if (devAttr->name == NULL) {
			continue;
		}

		if (len >= limit) {
			break;
		}

		len += sprintf(buf + len, "%-12s ", devAttr->name);

		if ((devAttr->flags & DMA_DEVICE_FLAG_IS_DEDICATED) != 0) {
			len +=
			    sprintf(buf + len, "Dedicated %d:%d ",
				    devAttr->dedicatedController,
				    devAttr->dedicatedChannel);
		} else {
			len += sprintf(buf + len, "Shared DMA:");
			if ((devAttr->flags & DMA_DEVICE_FLAG_ON_DMA0) != 0) {
				len += sprintf(buf + len, "0");
			}
			if ((devAttr->flags & DMA_DEVICE_FLAG_ON_DMA1) != 0) {
				len += sprintf(buf + len, "1");
			}
			len += sprintf(buf + len, " ");
		}
		if ((devAttr->flags & DMA_DEVICE_FLAG_NO_ISR) != 0) {
			len += sprintf(buf + len, "NoISR ");
		}
		if ((devAttr->flags & DMA_DEVICE_FLAG_ALLOW_LARGE_FIFO) != 0) {
			len += sprintf(buf + len, "Allow-128 ");
		}

		len +=
		    sprintf(buf + len,
			    "Xfer #: %Lu Ticks: %Lu Bytes: %Lu DescLen: %u\n",
			    devAttr->numTransfers, devAttr->transferTicks,
			    devAttr->transferBytes,
			    devAttr->ring.bytesAllocated);

	}

	up(&gDMA.lock);
	*eof = 1;

	return len;
}


static inline int IsDeviceValid(DMA_Device_t device)
{
	return (device >= 0) && (device < DMA_NUM_DEVICE_ENTRIES);
}


static inline DMA_Channel_t *HandleToChannel(DMA_Handle_t handle)
{
	int controllerIdx;
	int channelIdx;

	controllerIdx = CONTROLLER_FROM_HANDLE(handle);
	channelIdx = CHANNEL_FROM_HANDLE(handle);

	if ((controllerIdx > DMA_NUM_CONTROLLERS)
	    || (channelIdx > DMA_NUM_CHANNELS)) {
		return NULL;
	}
	return &gDMA.controller[controllerIdx].channel[channelIdx];
}


static irqreturn_t dma_interrupt_handler(int irq, void *dev_id)
{
	DMA_Channel_t *channel;
	DMA_DeviceAttribute_t *devAttr;
	int irqStatus;

	channel = (DMA_Channel_t *) dev_id;

	

	irqStatus = dmacHw_getInterruptStatus(channel->dmacHwHandle);
	dmacHw_clearInterrupt(channel->dmacHwHandle);

	if ((channel->devType < 0)
	    || (channel->devType > DMA_NUM_DEVICE_ENTRIES)) {
		printk(KERN_ERR "dma_interrupt_handler: Invalid devType: %d\n",
		       channel->devType);
		return IRQ_NONE;
	}
	devAttr = &DMA_gDeviceAttribute[channel->devType];

	

	if ((irqStatus & dmacHw_INTERRUPT_STATUS_TRANS) != 0) {
		devAttr->transferTicks +=
		    (timer_get_tick_count() - devAttr->transferStartTime);
	}

	if ((irqStatus & dmacHw_INTERRUPT_STATUS_ERROR) != 0) {
		printk(KERN_ERR
		       "dma_interrupt_handler: devType :%d DMA error (%s)\n",
		       channel->devType, devAttr->name);
	} else {
		devAttr->numTransfers++;
		devAttr->transferBytes += devAttr->numBytes;
	}

	

	if (devAttr->devHandler != NULL) {
		devAttr->devHandler(channel->devType, irqStatus,
				    devAttr->userData);
	}

	return IRQ_HANDLED;
}


int dma_alloc_descriptor_ring(DMA_DescriptorRing_t *ring,	
			      int numDescriptors	
    ) {
	size_t bytesToAlloc = dmacHw_descriptorLen(numDescriptors);

	if ((ring == NULL) || (numDescriptors <= 0)) {
		return -EINVAL;
	}

	ring->physAddr = 0;
	ring->descriptorsAllocated = 0;
	ring->bytesAllocated = 0;

	ring->virtAddr = dma_alloc_writecombine(NULL,
						     bytesToAlloc,
						     &ring->physAddr,
						     GFP_KERNEL);
	if (ring->virtAddr == NULL) {
		return -ENOMEM;
	}

	ring->bytesAllocated = bytesToAlloc;
	ring->descriptorsAllocated = numDescriptors;

	return dma_init_descriptor_ring(ring, numDescriptors);
}

EXPORT_SYMBOL(dma_alloc_descriptor_ring);


void dma_free_descriptor_ring(DMA_DescriptorRing_t *ring	
    ) {
	if (ring->virtAddr != NULL) {
		dma_free_writecombine(NULL,
				      ring->bytesAllocated,
				      ring->virtAddr, ring->physAddr);
	}

	ring->bytesAllocated = 0;
	ring->descriptorsAllocated = 0;
	ring->virtAddr = NULL;
	ring->physAddr = 0;
}

EXPORT_SYMBOL(dma_free_descriptor_ring);


int dma_init_descriptor_ring(DMA_DescriptorRing_t *ring,	
			     int numDescriptors	
    ) {
	if (ring->virtAddr == NULL) {
		return -EINVAL;
	}
	if (dmacHw_initDescriptor(ring->virtAddr,
				  ring->physAddr,
				  ring->bytesAllocated, numDescriptors) < 0) {
		printk(KERN_ERR
		       "dma_init_descriptor_ring: dmacHw_initDescriptor failed\n");
		return -ENOMEM;
	}

	return 0;
}

EXPORT_SYMBOL(dma_init_descriptor_ring);


int dma_calculate_descriptor_count(DMA_Device_t device,	
				   dma_addr_t srcData,	
				   dma_addr_t dstData,	
				   size_t numBytes	
    ) {
	int numDescriptors;
	DMA_DeviceAttribute_t *devAttr;

	if (!IsDeviceValid(device)) {
		return -ENODEV;
	}
	devAttr = &DMA_gDeviceAttribute[device];

	numDescriptors = dmacHw_calculateDescriptorCount(&devAttr->config,
							      (void *)srcData,
							      (void *)dstData,
							      numBytes);
	if (numDescriptors < 0) {
		printk(KERN_ERR
		       "dma_calculate_descriptor_count: dmacHw_calculateDescriptorCount failed\n");
		return -EINVAL;
	}

	return numDescriptors;
}

EXPORT_SYMBOL(dma_calculate_descriptor_count);


int dma_add_descriptors(DMA_DescriptorRing_t *ring,	
			DMA_Device_t device,	
			dma_addr_t srcData,	
			dma_addr_t dstData,	
			size_t numBytes	
    ) {
	int rc;
	DMA_DeviceAttribute_t *devAttr;

	if (!IsDeviceValid(device)) {
		return -ENODEV;
	}
	devAttr = &DMA_gDeviceAttribute[device];

	rc = dmacHw_setDataDescriptor(&devAttr->config,
				      ring->virtAddr,
				      (void *)srcData,
				      (void *)dstData, numBytes);
	if (rc < 0) {
		printk(KERN_ERR
		       "dma_add_descriptors: dmacHw_setDataDescriptor failed with code: %d\n",
		       rc);
		return -ENOMEM;
	}

	return 0;
}

EXPORT_SYMBOL(dma_add_descriptors);


int dma_set_device_descriptor_ring(DMA_Device_t device,	
				   DMA_DescriptorRing_t *ring	
    ) {
	DMA_DeviceAttribute_t *devAttr;

	if (!IsDeviceValid(device)) {
		return -ENODEV;
	}
	devAttr = &DMA_gDeviceAttribute[device];

	

	dma_free_descriptor_ring(&devAttr->ring);

	if (ring != NULL) {
		

		devAttr->ring = *ring;
	}

	
	

	devAttr->prevSrcData = 0;
	devAttr->prevDstData = 0;
	devAttr->prevNumBytes = 0;

	return 0;
}

EXPORT_SYMBOL(dma_set_device_descriptor_ring);


int dma_get_device_descriptor_ring(DMA_Device_t device,	
				   DMA_DescriptorRing_t *ring	
    ) {
	DMA_DeviceAttribute_t *devAttr;

	memset(ring, 0, sizeof(*ring));

	if (!IsDeviceValid(device)) {
		return -ENODEV;
	}
	devAttr = &DMA_gDeviceAttribute[device];

	*ring = devAttr->ring;

	return 0;
}

EXPORT_SYMBOL(dma_get_device_descriptor_ring);


static int ConfigChannel(DMA_Handle_t handle)
{
	DMA_Channel_t *channel;
	DMA_DeviceAttribute_t *devAttr;
	int controllerIdx;

	channel = HandleToChannel(handle);
	if (channel == NULL) {
		return -ENODEV;
	}
	devAttr = &DMA_gDeviceAttribute[channel->devType];
	controllerIdx = CONTROLLER_FROM_HANDLE(handle);

	if ((devAttr->flags & DMA_DEVICE_FLAG_PORT_PER_DMAC) != 0) {
		if (devAttr->config.transferType ==
		    dmacHw_TRANSFER_TYPE_MEM_TO_PERIPHERAL) {
			devAttr->config.dstPeripheralPort =
			    devAttr->dmacPort[controllerIdx];
		} else if (devAttr->config.transferType ==
			   dmacHw_TRANSFER_TYPE_PERIPHERAL_TO_MEM) {
			devAttr->config.srcPeripheralPort =
			    devAttr->dmacPort[controllerIdx];
		}
	}

	if (dmacHw_configChannel(channel->dmacHwHandle, &devAttr->config) != 0) {
		printk(KERN_ERR "ConfigChannel: dmacHw_configChannel failed\n");
		return -EIO;
	}

	return 0;
}


int dma_init(void)
{
	int rc = 0;
	int controllerIdx;
	int channelIdx;
	DMA_Device_t devIdx;
	DMA_Channel_t *channel;
	DMA_Handle_t dedicatedHandle;

	memset(&gDMA, 0, sizeof(gDMA));

	sema_init(&gDMA.lock, 0);
	init_waitqueue_head(&gDMA.freeChannelQ);

	

	dmacHw_initDma();

	

	for (controllerIdx = 0; controllerIdx < DMA_NUM_CONTROLLERS;
	     controllerIdx++) {
		for (channelIdx = 0; channelIdx < DMA_NUM_CHANNELS;
		     channelIdx++) {
			channel =
			    &gDMA.controller[controllerIdx].channel[channelIdx];

			channel->flags = 0;
			channel->devType = DMA_DEVICE_NONE;
			channel->lastDevType = DMA_DEVICE_NONE;

#if (DMA_DEBUG_TRACK_RESERVATION)
			channel->fileName = "";
			channel->lineNum = 0;
#endif

			channel->dmacHwHandle =
			    dmacHw_getChannelHandle(dmacHw_MAKE_CHANNEL_ID
						    (controllerIdx,
						     channelIdx));
			dmacHw_initChannel(channel->dmacHwHandle);
		}
	}

	

	gDMA.controller[0].channel[0].flags |= DMA_CHANNEL_FLAG_LARGE_FIFO;
	gDMA.controller[0].channel[1].flags |= DMA_CHANNEL_FLAG_LARGE_FIFO;
	gDMA.controller[1].channel[0].flags |= DMA_CHANNEL_FLAG_LARGE_FIFO;
	gDMA.controller[1].channel[1].flags |= DMA_CHANNEL_FLAG_LARGE_FIFO;

	

	for (devIdx = 0; devIdx < DMA_NUM_DEVICE_ENTRIES; devIdx++) {
		DMA_DeviceAttribute_t *devAttr = &DMA_gDeviceAttribute[devIdx];

		if (((devAttr->flags & DMA_DEVICE_FLAG_NO_ISR) != 0)
		    && ((devAttr->flags & DMA_DEVICE_FLAG_IS_DEDICATED) == 0)) {
			printk(KERN_ERR
			       "DMA Device: %s Can only request NO_ISR for dedicated devices\n",
			       devAttr->name);
			rc = -EINVAL;
			goto out;
		}

		if ((devAttr->flags & DMA_DEVICE_FLAG_IS_DEDICATED) != 0) {
			

			if (devAttr->dedicatedController >= DMA_NUM_CONTROLLERS) {
				printk(KERN_ERR
				       "DMA Device: %s DMA Controller %d is out of range\n",
				       devAttr->name,
				       devAttr->dedicatedController);
				rc = -EINVAL;
				goto out;
			}

			if (devAttr->dedicatedChannel >= DMA_NUM_CHANNELS) {
				printk(KERN_ERR
				       "DMA Device: %s DMA Channel %d is out of range\n",
				       devAttr->name,
				       devAttr->dedicatedChannel);
				rc = -EINVAL;
				goto out;
			}

			dedicatedHandle =
			    MAKE_HANDLE(devAttr->dedicatedController,
					devAttr->dedicatedChannel);
			channel = HandleToChannel(dedicatedHandle);

			if ((channel->flags & DMA_CHANNEL_FLAG_IS_DEDICATED) !=
			    0) {
				printk
				    ("DMA Device: %s attempting to use same DMA Controller:Channel (%d:%d) as %s\n",
				     devAttr->name,
				     devAttr->dedicatedController,
				     devAttr->dedicatedChannel,
				     DMA_gDeviceAttribute[channel->devType].
				     name);
				rc = -EBUSY;
				goto out;
			}

			channel->flags |= DMA_CHANNEL_FLAG_IS_DEDICATED;
			channel->devType = devIdx;

			if (devAttr->flags & DMA_DEVICE_FLAG_NO_ISR) {
				channel->flags |= DMA_CHANNEL_FLAG_NO_ISR;
			}

			
			

			ConfigChannel(dedicatedHandle);
		}
	}

	

	for (controllerIdx = 0; controllerIdx < DMA_NUM_CONTROLLERS;
	     controllerIdx++) {
		for (channelIdx = 0; channelIdx < DMA_NUM_CHANNELS;
		     channelIdx++) {
			channel =
			    &gDMA.controller[controllerIdx].channel[channelIdx];

			if ((channel->flags & DMA_CHANNEL_FLAG_NO_ISR) == 0) {
				snprintf(channel->name, sizeof(channel->name),
					 "dma %d:%d %s", controllerIdx,
					 channelIdx,
					 channel->devType ==
					 DMA_DEVICE_NONE ? "" :
					 DMA_gDeviceAttribute[channel->devType].
					 name);

				rc =
				     request_irq(IRQ_DMA0C0 +
						 (controllerIdx *
						  DMA_NUM_CHANNELS) +
						 channelIdx,
						 dma_interrupt_handler,
						 IRQF_DISABLED, channel->name,
						 channel);
				if (rc != 0) {
					printk(KERN_ERR
					       "request_irq for IRQ_DMA%dC%d failed\n",
					       controllerIdx, channelIdx);
				}
			}
		}
	}

	

	gDmaDir = proc_mkdir("dma", NULL);

	if (gDmaDir == NULL) {
		printk(KERN_ERR "Unable to create /proc/dma\n");
	} else {
		create_proc_read_entry("channels", 0, gDmaDir,
				       dma_proc_read_channels, NULL);
		create_proc_read_entry("devices", 0, gDmaDir,
				       dma_proc_read_devices, NULL);
	}

out:

	up(&gDMA.lock);

	return rc;
}


#if (DMA_DEBUG_TRACK_RESERVATION)
DMA_Handle_t dma_request_channel_dbg
    (DMA_Device_t dev, const char *fileName, int lineNum)
#else
DMA_Handle_t dma_request_channel(DMA_Device_t dev)
#endif
{
	DMA_Handle_t handle;
	DMA_DeviceAttribute_t *devAttr;
	DMA_Channel_t *channel;
	int controllerIdx;
	int controllerIdx2;
	int channelIdx;

	if (down_interruptible(&gDMA.lock) < 0) {
		return -ERESTARTSYS;
	}

	if ((dev < 0) || (dev >= DMA_NUM_DEVICE_ENTRIES)) {
		handle = -ENODEV;
		goto out;
	}
	devAttr = &DMA_gDeviceAttribute[dev];

#if (DMA_DEBUG_TRACK_RESERVATION)
	{
		char *s;

		s = strrchr(fileName, '/');
		if (s != NULL) {
			fileName = s + 1;
		}
	}
#endif
	if ((devAttr->flags & DMA_DEVICE_FLAG_IN_USE) != 0) {
		

		printk(KERN_ERR "%s: device %s is already requested\n",
		       __func__, devAttr->name);
		handle = -EBUSY;
		goto out;
	}

	if ((devAttr->flags & DMA_DEVICE_FLAG_IS_DEDICATED) != 0) {
		

		channel =
		    &gDMA.controller[devAttr->dedicatedController].
		    channel[devAttr->dedicatedChannel];
		if ((channel->flags & DMA_CHANNEL_FLAG_IN_USE) != 0) {
			handle = -EBUSY;
			goto out;
		}

		channel->flags |= DMA_CHANNEL_FLAG_IN_USE;
		devAttr->flags |= DMA_DEVICE_FLAG_IN_USE;

#if (DMA_DEBUG_TRACK_RESERVATION)
		channel->fileName = fileName;
		channel->lineNum = lineNum;
#endif
		handle =
		    MAKE_HANDLE(devAttr->dedicatedController,
				devAttr->dedicatedChannel);
		goto out;
	}

	

	handle = DMA_INVALID_HANDLE;
	while (handle == DMA_INVALID_HANDLE) {
		

		for (controllerIdx2 = 0; controllerIdx2 < DMA_NUM_CONTROLLERS;
		     controllerIdx2++) {
			

			controllerIdx = controllerIdx2;
			if ((devAttr->
			     flags & DMA_DEVICE_FLAG_ALLOC_DMA1_FIRST) != 0) {
				controllerIdx = 1 - controllerIdx;
			}

			

			if ((devAttr->
			     flags & (DMA_DEVICE_FLAG_ON_DMA0 << controllerIdx))
			    != 0) {
				for (channelIdx = 0;
				     channelIdx < DMA_NUM_CHANNELS;
				     channelIdx++) {
					channel =
					    &gDMA.controller[controllerIdx].
					    channel[channelIdx];

					if (((channel->
					      flags &
					      DMA_CHANNEL_FLAG_IS_DEDICATED) ==
					     0)
					    &&
					    ((channel->
					      flags & DMA_CHANNEL_FLAG_IN_USE)
					     == 0)) {
						if (((channel->
						      flags &
						      DMA_CHANNEL_FLAG_LARGE_FIFO)
						     != 0)
						    &&
						    ((devAttr->
						      flags &
						      DMA_DEVICE_FLAG_ALLOW_LARGE_FIFO)
						     == 0)) {
							
							

							continue;
						}

						channel->flags |=
						    DMA_CHANNEL_FLAG_IN_USE;
						channel->devType = dev;
						devAttr->flags |=
						    DMA_DEVICE_FLAG_IN_USE;

#if (DMA_DEBUG_TRACK_RESERVATION)
						channel->fileName = fileName;
						channel->lineNum = lineNum;
#endif
						handle =
						    MAKE_HANDLE(controllerIdx,
								channelIdx);

						

						if (ConfigChannel(handle) != 0) {
							handle = -EIO;
							printk(KERN_ERR
							       "dma_request_channel: ConfigChannel failed\n");
						}
						goto out;
					}
				}
			}
		}

		

		{
			DEFINE_WAIT(wait);

			prepare_to_wait(&gDMA.freeChannelQ, &wait,
					TASK_INTERRUPTIBLE);
			up(&gDMA.lock);
			schedule();
			finish_wait(&gDMA.freeChannelQ, &wait);

			if (signal_pending(current)) {
				

				return -ERESTARTSYS;
			}
		}

		if (down_interruptible(&gDMA.lock)) {
			return -ERESTARTSYS;
		}
	}

out:
	up(&gDMA.lock);

	return handle;
}


#if (DMA_DEBUG_TRACK_RESERVATION)
#undef dma_request_channel
DMA_Handle_t dma_request_channel(DMA_Device_t dev)
{
	return dma_request_channel_dbg(dev, __FILE__, __LINE__);
}

EXPORT_SYMBOL(dma_request_channel_dbg);
#endif
EXPORT_SYMBOL(dma_request_channel);


int dma_free_channel(DMA_Handle_t handle	
    ) {
	int rc = 0;
	DMA_Channel_t *channel;
	DMA_DeviceAttribute_t *devAttr;

	if (down_interruptible(&gDMA.lock) < 0) {
		return -ERESTARTSYS;
	}

	channel = HandleToChannel(handle);
	if (channel == NULL) {
		rc = -EINVAL;
		goto out;
	}

	devAttr = &DMA_gDeviceAttribute[channel->devType];

	if ((channel->flags & DMA_CHANNEL_FLAG_IS_DEDICATED) == 0) {
		channel->lastDevType = channel->devType;
		channel->devType = DMA_DEVICE_NONE;
	}
	channel->flags &= ~DMA_CHANNEL_FLAG_IN_USE;
	devAttr->flags &= ~DMA_DEVICE_FLAG_IN_USE;

out:
	up(&gDMA.lock);

	wake_up_interruptible(&gDMA.freeChannelQ);

	return rc;
}

EXPORT_SYMBOL(dma_free_channel);


int dma_device_is_channel_shared(DMA_Device_t device	
    ) {
	DMA_DeviceAttribute_t *devAttr;

	if (!IsDeviceValid(device)) {
		return -ENODEV;
	}
	devAttr = &DMA_gDeviceAttribute[device];

	return ((devAttr->flags & DMA_DEVICE_FLAG_IS_DEDICATED) == 0);
}

EXPORT_SYMBOL(dma_device_is_channel_shared);


int dma_alloc_descriptors(DMA_Handle_t handle,	
			  dmacHw_TRANSFER_TYPE_e transferType,	
			  dma_addr_t srcData,	
			  dma_addr_t dstData,	
			  size_t numBytes	
    ) {
	DMA_Channel_t *channel;
	DMA_DeviceAttribute_t *devAttr;
	int numDescriptors;
	size_t ringBytesRequired;
	int rc = 0;

	channel = HandleToChannel(handle);
	if (channel == NULL) {
		return -ENODEV;
	}

	devAttr = &DMA_gDeviceAttribute[channel->devType];

	if (devAttr->config.transferType != transferType) {
		return -EINVAL;
	}

	

	
	

	numDescriptors = dmacHw_calculateDescriptorCount(&devAttr->config,
							      (void *)srcData,
							      (void *)dstData,
							      numBytes);
	if (numDescriptors < 0) {
		printk(KERN_ERR "%s: dmacHw_calculateDescriptorCount failed\n",
		       __func__);
		return -EINVAL;
	}

	
	

	ringBytesRequired = dmacHw_descriptorLen(numDescriptors);

	

	if (ringBytesRequired > devAttr->ring.bytesAllocated) {
		
		
		

		might_sleep();

		

		dma_free_descriptor_ring(&devAttr->ring);

		

		rc =
		     dma_alloc_descriptor_ring(&devAttr->ring,
					       numDescriptors);
		if (rc < 0) {
			printk(KERN_ERR
			       "%s: dma_alloc_descriptor_ring(%d) failed\n",
			       __func__, numDescriptors);
			return rc;
		}
		

		if (dmacHw_initDescriptor(devAttr->ring.virtAddr,
					  devAttr->ring.physAddr,
					  devAttr->ring.bytesAllocated,
					  numDescriptors) < 0) {
			printk(KERN_ERR "%s: dmacHw_initDescriptor failed\n",
			       __func__);
			return -EINVAL;
		}
	} else {
		
		

		dmacHw_resetDescriptorControl(devAttr->ring.virtAddr);
	}

	
	

	if (dmacHw_setDataDescriptor(&devAttr->config,
				     devAttr->ring.virtAddr,
				     (void *)srcData,
				     (void *)dstData, numBytes) < 0) {
		printk(KERN_ERR "%s: dmacHw_setDataDescriptor failed\n",
		       __func__);
		return -EINVAL;
	}

	
	

	devAttr->prevSrcData = srcData;
	devAttr->prevDstData = dstData;
	devAttr->prevNumBytes = numBytes;

	return 0;
}

EXPORT_SYMBOL(dma_alloc_descriptors);


int dma_alloc_double_dst_descriptors(DMA_Handle_t handle,	
				     dma_addr_t srcData,	
				     dma_addr_t dstData1,	
				     dma_addr_t dstData2,	
				     size_t numBytes	
    ) {
	DMA_Channel_t *channel;
	DMA_DeviceAttribute_t *devAttr;
	int numDst1Descriptors;
	int numDst2Descriptors;
	int numDescriptors;
	size_t ringBytesRequired;
	int rc = 0;

	channel = HandleToChannel(handle);
	if (channel == NULL) {
		return -ENODEV;
	}

	devAttr = &DMA_gDeviceAttribute[channel->devType];

	

	
	

	numDst1Descriptors =
	     dmacHw_calculateDescriptorCount(&devAttr->config, (void *)srcData,
					     (void *)dstData1, numBytes);
	if (numDst1Descriptors < 0) {
		return -EINVAL;
	}
	numDst2Descriptors =
	     dmacHw_calculateDescriptorCount(&devAttr->config, (void *)srcData,
					     (void *)dstData2, numBytes);
	if (numDst2Descriptors < 0) {
		return -EINVAL;
	}
	numDescriptors = numDst1Descriptors + numDst2Descriptors;
	

	
	

	ringBytesRequired = dmacHw_descriptorLen(numDescriptors);

	

	if (ringBytesRequired > devAttr->ring.bytesAllocated) {
		
		
		

		might_sleep();

		

		dma_free_descriptor_ring(&devAttr->ring);

		

		rc =
		     dma_alloc_descriptor_ring(&devAttr->ring,
					       numDescriptors);
		if (rc < 0) {
			printk(KERN_ERR
			       "%s: dma_alloc_descriptor_ring(%d) failed\n",
			       __func__, ringBytesRequired);
			return rc;
		}
	}

	
	
	

	if (dmacHw_initDescriptor(devAttr->ring.virtAddr,
				  devAttr->ring.physAddr,
				  devAttr->ring.bytesAllocated,
				  numDescriptors) < 0) {
		printk(KERN_ERR "%s: dmacHw_initDescriptor failed\n", __func__);
		return -EINVAL;
	}

	
	

	if (dmacHw_setDataDescriptor(&devAttr->config,
				     devAttr->ring.virtAddr,
				     (void *)srcData,
				     (void *)dstData1, numBytes) < 0) {
		printk(KERN_ERR "%s: dmacHw_setDataDescriptor 1 failed\n",
		       __func__);
		return -EINVAL;
	}
	if (dmacHw_setDataDescriptor(&devAttr->config,
				     devAttr->ring.virtAddr,
				     (void *)srcData,
				     (void *)dstData2, numBytes) < 0) {
		printk(KERN_ERR "%s: dmacHw_setDataDescriptor 2 failed\n",
		       __func__);
		return -EINVAL;
	}

	
	

	devAttr->prevSrcData = 0;
	devAttr->prevDstData = 0;
	devAttr->prevNumBytes = 0;

	return numDescriptors;
}

EXPORT_SYMBOL(dma_alloc_double_dst_descriptors);


int dma_start_transfer(DMA_Handle_t handle)
{
	DMA_Channel_t *channel;
	DMA_DeviceAttribute_t *devAttr;

	channel = HandleToChannel(handle);
	if (channel == NULL) {
		return -ENODEV;
	}
	devAttr = &DMA_gDeviceAttribute[channel->devType];

	dmacHw_initiateTransfer(channel->dmacHwHandle, &devAttr->config,
				devAttr->ring.virtAddr);

	

	return 0;
}

EXPORT_SYMBOL(dma_start_transfer);


int dma_stop_transfer(DMA_Handle_t handle)
{
	DMA_Channel_t *channel;

	channel = HandleToChannel(handle);
	if (channel == NULL) {
		return -ENODEV;
	}

	dmacHw_stopTransfer(channel->dmacHwHandle);

	return 0;
}

EXPORT_SYMBOL(dma_stop_transfer);


int dma_wait_transfer_done(DMA_Handle_t handle)
{
	DMA_Channel_t *channel;
	dmacHw_TRANSFER_STATUS_e status;

	channel = HandleToChannel(handle);
	if (channel == NULL) {
		return -ENODEV;
	}

	while ((status =
		dmacHw_transferCompleted(channel->dmacHwHandle)) ==
	       dmacHw_TRANSFER_STATUS_BUSY) {
		;
	}

	if (status == dmacHw_TRANSFER_STATUS_ERROR) {
		printk(KERN_ERR "%s: DMA transfer failed\n", __func__);
		return -EIO;
	}
	return 0;
}

EXPORT_SYMBOL(dma_wait_transfer_done);


int dma_transfer(DMA_Handle_t handle,	
		 dmacHw_TRANSFER_TYPE_e transferType,	
		 dma_addr_t srcData,	
		 dma_addr_t dstData,	
		 size_t numBytes	
    ) {
	DMA_Channel_t *channel;
	DMA_DeviceAttribute_t *devAttr;
	int rc = 0;

	channel = HandleToChannel(handle);
	if (channel == NULL) {
		return -ENODEV;
	}

	devAttr = &DMA_gDeviceAttribute[channel->devType];

	if (devAttr->config.transferType != transferType) {
		return -EINVAL;
	}

	
	
	

	{
		rc =
		     dma_alloc_descriptors(handle, transferType, srcData,
					   dstData, numBytes);
		if (rc != 0) {
			return rc;
		}
	}

	

	devAttr->numBytes = numBytes;
	devAttr->transferStartTime = timer_get_tick_count();

	dmacHw_initiateTransfer(channel->dmacHwHandle, &devAttr->config,
				devAttr->ring.virtAddr);

	

	return 0;
}

EXPORT_SYMBOL(dma_transfer);


int dma_set_device_handler(DMA_Device_t dev,	
			   DMA_DeviceHandler_t devHandler,	
			   void *userData	
    ) {
	DMA_DeviceAttribute_t *devAttr;
	unsigned long flags;

	if (!IsDeviceValid(dev)) {
		return -ENODEV;
	}
	devAttr = &DMA_gDeviceAttribute[dev];

	local_irq_save(flags);

	devAttr->userData = userData;
	devAttr->devHandler = devHandler;

	local_irq_restore(flags);

	return 0;
}

EXPORT_SYMBOL(dma_set_device_handler);
