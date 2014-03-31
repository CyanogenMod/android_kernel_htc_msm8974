/*
 * ============================================================================
 *  Copyright (c) 1996-2002 Winbond Electronic Corporation
 *
 *  Module Name:
 *    Wb35Rx.c
 *
 *  Abstract:
 *    Processing the Rx message from down layer
 *
 * ============================================================================
 */
#include <linux/usb.h>
#include <linux/slab.h>

#include "core.h"
#include "wb35rx_f.h"

static void packet_came(struct ieee80211_hw *hw, char *pRxBufferAddress, int PacketSize)
{
	struct wbsoft_priv *priv = hw->priv;
	struct sk_buff *skb;
	struct ieee80211_rx_status rx_status = {0};

	if (!priv->enabled)
		return;

	skb = dev_alloc_skb(PacketSize);
	if (!skb) {
		printk("Not enough memory for packet, FIXME\n");
		return;
	}

	memcpy(skb_put(skb, PacketSize), pRxBufferAddress, PacketSize);

	memcpy(IEEE80211_SKB_RXCB(skb), &rx_status, sizeof(rx_status));
	ieee80211_rx_irqsafe(hw, skb);
}

static void Wb35Rx_adjust(struct wb35_descriptor *pRxDes)
{
	u32	*pRxBufferAddress;
	u32	DecryptionMethod;
	u32	i;
	u16	BufferSize;

	DecryptionMethod = pRxDes->R01.R01_decryption_method;
	pRxBufferAddress = pRxDes->buffer_address[0];
	BufferSize = pRxDes->buffer_size[0];

	
	BufferSize -= 4; 
	if (DecryptionMethod)
		BufferSize -= 4;
	if (DecryptionMethod == 3) 
		BufferSize -= 4;

	
	if (DecryptionMethod == 1) { 
		for (i = 6; i > 0; i--)
			pRxBufferAddress[i] = pRxBufferAddress[i - 1];
		pRxDes->buffer_address[0] = pRxBufferAddress + 1;
		BufferSize -= 4; 
	} else if (DecryptionMethod) { 
		for (i = 7; i > 1; i--)
			pRxBufferAddress[i] = pRxBufferAddress[i - 2];
		pRxDes->buffer_address[0] = pRxBufferAddress + 2; 
		BufferSize -= 8; 
	}
	pRxDes->buffer_size[0] = BufferSize;
}

static u16 Wb35Rx_indicate(struct ieee80211_hw *hw)
{
	struct wbsoft_priv	*priv = hw->priv;
	struct hw_data		*pHwData = &priv->sHwData;
	struct wb35_descriptor	RxDes;
	struct wb35_rx		*pWb35Rx = &pHwData->Wb35Rx;
	u8			*pRxBufferAddress;
	u16			PacketSize;
	u16			stmp, BufferSize, stmp2 = 0;
	u32			RxBufferId;

	
	do {
		RxBufferId = pWb35Rx->RxProcessIndex;
		if (pWb35Rx->RxOwner[RxBufferId]) 
			break;

		pWb35Rx->RxProcessIndex++;
		pWb35Rx->RxProcessIndex %= MAX_USB_RX_BUFFER_NUMBER;

		pRxBufferAddress = pWb35Rx->pDRx;
		BufferSize = pWb35Rx->RxBufferSize[RxBufferId];

		
		while (BufferSize >= 4) {
			if ((cpu_to_le32(*(u32 *)pRxBufferAddress) & 0x0fffffff) == RX_END_TAG) 
				break;

			
			RxDes.R00.value = le32_to_cpu(*(u32 *)pRxBufferAddress);
			PacketSize = (u16)RxDes.R00.R00_receive_byte_count;
			RxDes.R01.value = le32_to_cpu(*((u32 *)(pRxBufferAddress + 4)));
			
			if ((PacketSize & 0x03) > 0)
				PacketSize -= 4;

			
			if (PacketSize > MAX_PACKET_SIZE) {
				pr_debug("Serious ERROR : Rx data size too long, size =%d\n", PacketSize);
				pWb35Rx->EP3vm_state = VM_STOP;
				pWb35Rx->Ep3ErrorCount2++;
				break;
			}

			BufferSize -= 8; 
			pRxBufferAddress += 8;

			RxDes.buffer_address[0] = pRxBufferAddress;
			RxDes.buffer_size[0] = PacketSize;
			RxDes.buffer_number = 1;
			RxDes.buffer_start_index = 0;
			RxDes.buffer_total_size = RxDes.buffer_size[0];
			Wb35Rx_adjust(&RxDes);

			packet_came(hw, pRxBufferAddress, PacketSize);

			
			stmp = PacketSize + 3;
			stmp &= ~0x03; 
			pRxBufferAddress += stmp;
			BufferSize -= stmp;
			stmp2 += stmp;
		}

		
		pWb35Rx->RxOwner[RxBufferId] = 1;
	} while (true);
	return stmp2;
}

static void Wb35Rx(struct ieee80211_hw *hw);

static void Wb35Rx_Complete(struct urb *urb)
{
	struct ieee80211_hw	*hw = urb->context;
	struct wbsoft_priv	*priv = hw->priv;
	struct hw_data		*pHwData = &priv->sHwData;
	struct wb35_rx		*pWb35Rx = &pHwData->Wb35Rx;
	u8			*pRxBufferAddress;
	u32			SizeCheck;
	u16			BulkLength;
	u32			RxBufferId;
	struct R00_descriptor		R00;

	
	pWb35Rx->EP3vm_state = VM_COMPLETED;
	pWb35Rx->EP3VM_status = urb->status; 

	RxBufferId = pWb35Rx->CurrentRxBufferId;

	pRxBufferAddress = pWb35Rx->pDRx;
	BulkLength = (u16)urb->actual_length;

	
	pWb35Rx->EP3vm_state = VM_COMPLETED;

	if (pHwData->SurpriseRemove) 
		goto error;

	if (pWb35Rx->rx_halt)
		goto error;

	
	pWb35Rx->RxOwner[RxBufferId] = 0; 
	R00.value = le32_to_cpu(*(u32 *)pRxBufferAddress);

	
	if (pWb35Rx->EP3VM_status != 0) {
		pr_debug("EP3 IoCompleteRoutine return error\n");
		pWb35Rx->EP3vm_state = VM_STOP;
		goto error;
	}

	
	if (!HAL_USB_MODE_BURST(pHwData)) {
		SizeCheck = R00.R00_receive_byte_count;
		if ((SizeCheck & 0x03) > 0)
			SizeCheck -= 4;
		SizeCheck = (SizeCheck + 3) & ~0x03;
		SizeCheck += 12; 
		if ((BulkLength > 1600) ||
			(SizeCheck > 1600) ||
			(BulkLength != SizeCheck) ||
			(BulkLength == 0)) { 
			pWb35Rx->EP3vm_state = VM_STOP;
			pWb35Rx->Ep3ErrorCount2++;
		}
	}

	
	pWb35Rx->ByteReceived += BulkLength;
	pWb35Rx->RxBufferSize[RxBufferId] = BulkLength;

	if (!pWb35Rx->RxOwner[RxBufferId])
		Wb35Rx_indicate(hw);

	kfree(pWb35Rx->pDRx);
	
	Wb35Rx(hw);
	return;

error:
	pWb35Rx->RxOwner[RxBufferId] = 1; 
	atomic_dec(&pWb35Rx->RxFireCounter);
	pWb35Rx->EP3vm_state = VM_STOP;
}

static void Wb35Rx(struct ieee80211_hw *hw)
{
	struct wbsoft_priv	*priv = hw->priv;
	struct hw_data		*pHwData = &priv->sHwData;
	struct wb35_rx		*pWb35Rx = &pHwData->Wb35Rx;
	u8			*pRxBufferAddress;
	struct urb		*urb = pWb35Rx->RxUrb;
	int			retv;
	u32			RxBufferId;

	
	if (pHwData->SurpriseRemove)
		goto error;

	if (pWb35Rx->rx_halt)
		goto error;

	
	RxBufferId = pWb35Rx->RxBufferId;
	if (!pWb35Rx->RxOwner[RxBufferId]) {
		
		pr_debug("Rx driver fifo unavailable\n");
		goto error;
	}

	
	pWb35Rx->RxBufferId++;
	pWb35Rx->RxBufferId %= MAX_USB_RX_BUFFER_NUMBER;

	pWb35Rx->CurrentRxBufferId = RxBufferId;

	pWb35Rx->pDRx = kzalloc(MAX_USB_RX_BUFFER, GFP_ATOMIC);
	if (!pWb35Rx->pDRx) {
		printk("w35und: Rx memory alloc failed\n");
		goto error;
	}
	pRxBufferAddress = pWb35Rx->pDRx;

	usb_fill_bulk_urb(urb, pHwData->udev,
			  usb_rcvbulkpipe(pHwData->udev, 3),
			  pRxBufferAddress, MAX_USB_RX_BUFFER,
			  Wb35Rx_Complete, hw);

	pWb35Rx->EP3vm_state = VM_RUNNING;

	retv = usb_submit_urb(urb, GFP_ATOMIC);

	if (retv != 0) {
		printk("Rx URB sending error\n");
		goto error;
	}
	return;

error:
	
	pWb35Rx->EP3vm_state = VM_STOP;
	atomic_dec(&pWb35Rx->RxFireCounter);
}

void Wb35Rx_start(struct ieee80211_hw *hw)
{
	struct wbsoft_priv	*priv = hw->priv;
	struct hw_data		*pHwData = &priv->sHwData;
	struct wb35_rx		*pWb35Rx = &pHwData->Wb35Rx;

	
	if (atomic_inc_return(&pWb35Rx->RxFireCounter) == 1) {
		pWb35Rx->EP3vm_state = VM_RUNNING;
		Wb35Rx(hw);
	} else
		atomic_dec(&pWb35Rx->RxFireCounter);
}

static void Wb35Rx_reset_descriptor(struct hw_data *pHwData)
{
	struct wb35_rx	*pWb35Rx = &pHwData->Wb35Rx;
	u32		i;

	pWb35Rx->ByteReceived = 0;
	pWb35Rx->RxProcessIndex = 0;
	pWb35Rx->RxBufferId = 0;
	pWb35Rx->EP3vm_state = VM_STOP;
	pWb35Rx->rx_halt = 0;

	
	for (i = 0; i < MAX_USB_RX_BUFFER_NUMBER; i++)
		pWb35Rx->RxOwner[i] = 1;
}

unsigned char Wb35Rx_initial(struct hw_data *pHwData)
{
	struct wb35_rx *pWb35Rx = &pHwData->Wb35Rx;

	
	Wb35Rx_reset_descriptor(pHwData);

	pWb35Rx->RxUrb = usb_alloc_urb(0, GFP_ATOMIC);
	return !!pWb35Rx->RxUrb;
}

void Wb35Rx_stop(struct hw_data *pHwData)
{
	struct wb35_rx *pWb35Rx = &pHwData->Wb35Rx;

	
	if (pWb35Rx->EP3vm_state == VM_RUNNING) {
		usb_unlink_urb(pWb35Rx->RxUrb); 
		pr_debug("EP3 Rx stop\n");
	}
}

void Wb35Rx_destroy(struct hw_data *pHwData)
{
	struct wb35_rx *pWb35Rx = &pHwData->Wb35Rx;

	do {
		msleep(10); 
	} while (pWb35Rx->EP3vm_state != VM_STOP);
	msleep(10); 

	if (pWb35Rx->RxUrb)
		usb_free_urb(pWb35Rx->RxUrb);
	pr_debug("Wb35Rx_destroy OK\n");
}

