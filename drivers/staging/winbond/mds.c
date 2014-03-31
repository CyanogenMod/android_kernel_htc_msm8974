#include "mds_f.h"
#include "mto.h"
#include "wbhal.h"
#include "wb35tx_f.h"

unsigned char
Mds_initial(struct wbsoft_priv *adapter)
{
	struct wb35_mds *pMds = &adapter->Mds;

	pMds->TxPause = false;
	pMds->TxRTSThreshold = DEFAULT_RTSThreshold;
	pMds->TxFragmentThreshold = DEFAULT_FRAGMENT_THRESHOLD;

	return hal_get_tx_buffer(&adapter->sHwData, &pMds->pTxBuffer);
}

static void Mds_DurationSet(struct wbsoft_priv *adapter,  struct wb35_descriptor *pDes,  u8 *buffer)
{
	struct T00_descriptor *pT00;
	struct T01_descriptor *pT01;
	u16	Duration, NextBodyLen, OffsetSize;
	u8	Rate, i;
	unsigned char	CTS_on = false, RTS_on = false;
	struct T00_descriptor *pNextT00;
	u16 BodyLen = 0;
	unsigned char boGroupAddr = false;

	OffsetSize = pDes->FragmentThreshold + 32 + 3;
	OffsetSize &= ~0x03;
	Rate = pDes->TxRate >> 1;
	if (!Rate)
		Rate = 1;

	pT00 = (struct T00_descriptor *)buffer;
	pT01 = (struct T01_descriptor *)(buffer+4);
	pNextT00 = (struct T00_descriptor *)(buffer+OffsetSize);

	if (buffer[DOT_11_DA_OFFSET+8] & 0x1) 
		boGroupAddr = true;

	if (!boGroupAddr) {
		BodyLen = (u16)pT00->T00_frame_length;	
		BodyLen += 4;	

		if (BodyLen >= CURRENT_RTS_THRESHOLD)
			RTS_on = true; 
		else {
			if (pT01->T01_modulation_type) { 
				if (CURRENT_PROTECT_MECHANISM) 
					CTS_on = true; 
			}
		}
	}

	if (RTS_on || CTS_on) {
		if (pT01->T01_modulation_type) { 
			Duration = 2*DEFAULT_SIFSTIME +
					   2*PREAMBLE_PLUS_SIGNAL_PLUS_SIGNALEXTENSION +
					   ((BodyLen*8 + 22 + Rate*4 - 1)/(Rate*4))*Tsym +
					   ((112 + 22 + 95)/96)*Tsym;
		} else	{ 
			if (pT01->T01_plcp_header_length) 
				Duration = LONG_PREAMBLE_PLUS_PLCPHEADER_TIME*2;
			else
				Duration = SHORT_PREAMBLE_PLUS_PLCPHEADER_TIME*2;

			Duration += (((BodyLen + 14)*8 + Rate-1) / Rate +
						DEFAULT_SIFSTIME*2);
		}

		if (RTS_on) {
			if (pT01->T01_modulation_type) { 
				Duration += (DEFAULT_SIFSTIME +
								PREAMBLE_PLUS_SIGNAL_PLUS_SIGNALEXTENSION +
								((112 + 22 + 95)/96)*Tsym);
			} else {
				if (pT01->T01_plcp_header_length) 
					Duration += LONG_PREAMBLE_PLUS_PLCPHEADER_TIME;
				else
					Duration += SHORT_PREAMBLE_PLUS_PLCPHEADER_TIME;

				Duration += (((112 + Rate-1) / Rate) + DEFAULT_SIFSTIME);
			}
		}

		
		pT01->T01_add_rts = RTS_on ? 1 : 0;
		pT01->T01_add_cts = CTS_on ? 1 : 0;
		pT01->T01_rts_cts_duration = Duration;
	}

	if (boGroupAddr)
		Duration = 0;
	else {
		for (i = pDes->FragmentCount-1; i > 0; i--) {
			NextBodyLen = (u16)pNextT00->T00_frame_length;
			NextBodyLen += 4;	

			if (pT01->T01_modulation_type) {
				Duration = PREAMBLE_PLUS_SIGNAL_PLUS_SIGNALEXTENSION * 3;
				Duration += (((NextBodyLen*8 + 22 + Rate*4 - 1)/(Rate*4)) * Tsym +
							(((2*14)*8 + 22 + 95)/96)*Tsym +
							DEFAULT_SIFSTIME*3);
			} else {
				if (pT01->T01_plcp_header_length) 
					Duration = LONG_PREAMBLE_PLUS_PLCPHEADER_TIME*3;
				else
					Duration = SHORT_PREAMBLE_PLUS_PLCPHEADER_TIME*3;

				Duration += (((NextBodyLen + (2*14))*8 + Rate-1) / Rate +
							DEFAULT_SIFSTIME*3);
			}

			((u16 *)buffer)[5] = cpu_to_le16(Duration); 

			
			pNextT00->value = cpu_to_le32(pNextT00->value);
			pT01->value = cpu_to_le32(pT01->value);
			

			buffer += OffsetSize;
			pT01 = (struct T01_descriptor *)(buffer+4);
			if (i != 1)	
				pNextT00 = (struct T00_descriptor *)(buffer+OffsetSize);
		}

		if (pT01->T01_modulation_type) {
			Duration = PREAMBLE_PLUS_SIGNAL_PLUS_SIGNALEXTENSION;
			
			Duration += (((112 + 22 + 95)/96)*Tsym + DEFAULT_SIFSTIME);
		} else {
			if (pT01->T01_plcp_header_length) 
				Duration = LONG_PREAMBLE_PLUS_PLCPHEADER_TIME;
			else
				Duration = SHORT_PREAMBLE_PLUS_PLCPHEADER_TIME;

			Duration += ((112 + Rate-1)/Rate + DEFAULT_SIFSTIME);
		}
	}

	((u16 *)buffer)[5] = cpu_to_le16(Duration); 
	pT00->value = cpu_to_le32(pT00->value);
	pT01->value = cpu_to_le32(pT01->value);
	

}

static u16 Mds_BodyCopy(struct wbsoft_priv *adapter, struct wb35_descriptor *pDes, u8 *TargetBuffer)
{
	struct T00_descriptor *pT00;
	struct wb35_mds *pMds = &adapter->Mds;
	u8	*buffer;
	u8	*src_buffer;
	u8	*pctmp;
	u16	Size = 0;
	u16	SizeLeft, CopySize, CopyLeft, stmp;
	u8	buf_index, FragmentCount = 0;


	
	buffer = TargetBuffer; 
	SizeLeft = pDes->buffer_total_size;
	buf_index = pDes->buffer_start_index;

	pT00 = (struct T00_descriptor *)buffer;
	while (SizeLeft) {
		pT00 = (struct T00_descriptor *)buffer;
		CopySize = SizeLeft;
		if (SizeLeft > pDes->FragmentThreshold) {
			CopySize = pDes->FragmentThreshold;
			pT00->T00_frame_length = 24 + CopySize; 
		} else
			pT00->T00_frame_length = 24 + SizeLeft; 

		SizeLeft -= CopySize;

		
		pctmp = (u8 *)(buffer + 8 + DOT_11_SEQUENCE_OFFSET);
		*pctmp &= 0xf0;
		*pctmp |= FragmentCount; 
		if (!FragmentCount)
			pT00->T00_first_mpdu = 1;

		buffer += 32; 
		Size += 32;

		
		stmp = CopySize + 3;
		stmp &= ~0x03; 
		Size += stmp; 

		while (CopySize) {
			
			src_buffer = pDes->buffer_address[buf_index];
			CopyLeft = CopySize;
			if (CopySize >= pDes->buffer_size[buf_index]) {
				CopyLeft = pDes->buffer_size[buf_index];

				
				buf_index++;
				buf_index %= MAX_DESCRIPTOR_BUFFER_INDEX;
			} else {
				u8	*pctmp = pDes->buffer_address[buf_index];
				pctmp += CopySize;
				pDes->buffer_address[buf_index] = pctmp;
				pDes->buffer_size[buf_index] -= CopySize;
			}

			memcpy(buffer, src_buffer, CopyLeft);
			buffer += CopyLeft;
			CopySize -= CopyLeft;
		}

		
		if (pMds->MicAdd) {
			if (!SizeLeft) {
				pMds->MicWriteAddress[pMds->MicWriteIndex] = buffer - pMds->MicAdd;
				pMds->MicWriteSize[pMds->MicWriteIndex] = pMds->MicAdd;
				pMds->MicAdd = 0;
			} else if (SizeLeft < 8) { 
				pMds->MicAdd = SizeLeft;
				pMds->MicWriteAddress[pMds->MicWriteIndex] = buffer - (8 - SizeLeft);
				pMds->MicWriteSize[pMds->MicWriteIndex] = 8 - SizeLeft;
				pMds->MicWriteIndex++;
			}
		}

		
		if (SizeLeft) {
			buffer = TargetBuffer + Size; 
			memcpy(buffer, TargetBuffer, 32); 
			pT00 = (struct T00_descriptor *)buffer;
			pT00->T00_first_mpdu = 0;
		}

		FragmentCount++;
	}

	pT00->T00_last_mpdu = 1;
	pT00->T00_IsLastMpdu = 1;
	buffer = (u8 *)pT00 + 8; 
	buffer[1] &= ~0x04; 
	pDes->FragmentCount = FragmentCount; 
	return Size;
}

static void Mds_HeaderCopy(struct wbsoft_priv *adapter, struct wb35_descriptor *pDes, u8 *TargetBuffer)
{
	struct wb35_mds *pMds = &adapter->Mds;
	u8	*src_buffer = pDes->buffer_address[0]; 
	struct T00_descriptor *pT00;
	struct T01_descriptor *pT01;
	u16	stmp;
	u8	i, ctmp1, ctmp2, ctmpf;
	u16	FragmentThreshold = CURRENT_FRAGMENT_THRESHOLD;


	stmp = pDes->buffer_total_size;
	pT00 = (struct T00_descriptor *)TargetBuffer;
	TargetBuffer += 4;
	pT01 = (struct T01_descriptor *)TargetBuffer;
	TargetBuffer += 4;

	pT00->value = 0; 
	pT01->value = 0; 

	pT00->T00_tx_packet_id = pDes->Descriptor_ID; 
	pT00->T00_header_length = 24; 
	pT01->T01_retry_abort_ebable = 1; 

	
	pT01->T01_wep_id = 0;

	FragmentThreshold = DEFAULT_FRAGMENT_THRESHOLD;	
	
	memcpy(TargetBuffer, src_buffer, DOT_11_MAC_HEADER_SIZE); 
	pDes->buffer_address[0] = src_buffer + DOT_11_MAC_HEADER_SIZE;
	pDes->buffer_total_size -= DOT_11_MAC_HEADER_SIZE;
	pDes->buffer_size[0] = pDes->buffer_total_size;

	
	FragmentThreshold -= (DOT_11_MAC_HEADER_SIZE + 4);
	pDes->FragmentThreshold = FragmentThreshold;

	
	TargetBuffer[1] |= 0x04; 

	stmp = *(u16 *)(TargetBuffer+30); 

	
	ctmp1 = ctmpf = CURRENT_TX_RATE_FOR_MNG;

	pDes->TxRate = ctmp1;
	pr_debug("Tx rate =%x\n", ctmp1);

	pT01->T01_modulation_type = (ctmp1%3) ? 0 : 1;

	for (i = 0; i < 2; i++) {
		if (i == 1)
			ctmp1 = ctmpf;

		pMds->TxRate[pDes->Descriptor_ID][i] = ctmp1; 

		if (ctmp1 == 108)
			ctmp2 = 7;
		else if (ctmp1 == 96)
			ctmp2 = 6; 
		else if (ctmp1 == 72)
			ctmp2 = 5;
		else if (ctmp1 == 48)
			ctmp2 = 4;
		else if (ctmp1 == 36)
			ctmp2 = 3;
		else if (ctmp1 == 24)
			ctmp2 = 2;
		else if (ctmp1 == 18)
			ctmp2 = 1;
		else if (ctmp1 == 12)
			ctmp2 = 0;
		else if (ctmp1 == 22)
			ctmp2 = 3;
		else if (ctmp1 == 11)
			ctmp2 = 2;
		else if (ctmp1 == 4)
			ctmp2 = 1;
		else
			ctmp2 = 0; 

		if (i == 0)
			pT01->T01_transmit_rate = ctmp2;
		else
			pT01->T01_fall_back_rate = ctmp2;
	}

	if ((pT01->T01_modulation_type == 0) && (pT01->T01_transmit_rate == 0))	
		pDes->PreambleMode =  WLAN_PREAMBLE_TYPE_LONG;
	else
		pDes->PreambleMode =  CURRENT_PREAMBLE_MODE;
	pT01->T01_plcp_header_length = pDes->PreambleMode;	

}

static void MLME_GetNextPacket(struct wbsoft_priv *adapter, struct wb35_descriptor *desc)
{
	desc->InternalUsed = desc->buffer_start_index + desc->buffer_number;
	desc->InternalUsed %= MAX_DESCRIPTOR_BUFFER_INDEX;
	desc->buffer_address[desc->InternalUsed] = adapter->sMlmeFrame.pMMPDU;
	desc->buffer_size[desc->InternalUsed] = adapter->sMlmeFrame.len;
	desc->buffer_total_size += adapter->sMlmeFrame.len;
	desc->buffer_number++;
	desc->Type = adapter->sMlmeFrame.DataType;
}

static void MLMEfreeMMPDUBuffer(struct wbsoft_priv *adapter, s8 *pData)
{
	int i;

	
	for (i = 0; i < MAX_NUM_TX_MMPDU; i++) {
		if (pData == (s8 *)&(adapter->sMlmeFrame.TxMMPDU[i]))
			break;
	}
	if (adapter->sMlmeFrame.TxMMPDUInUse[i])
		adapter->sMlmeFrame.TxMMPDUInUse[i] = false;
	else  {
	}
}

static void MLME_SendComplete(struct wbsoft_priv *adapter, u8 PacketID, unsigned char SendOK)
{
    
	adapter->sMlmeFrame.len = 0;
	MLMEfreeMMPDUBuffer(adapter, adapter->sMlmeFrame.pMMPDU);

	
	adapter->sMlmeFrame.IsInUsed = PACKET_FREE_TO_USE;
}

void
Mds_Tx(struct wbsoft_priv *adapter)
{
	struct hw_data *pHwData = &adapter->sHwData;
	struct wb35_mds *pMds = &adapter->Mds;
	struct wb35_descriptor	TxDes;
	struct wb35_descriptor *pTxDes = &TxDes;
	u8		*XmitBufAddress;
	u16		XmitBufSize, PacketSize, stmp, CurrentSize, FragmentThreshold;
	u8		FillIndex, TxDesIndex, FragmentCount, FillCount;
	unsigned char	BufferFilled = false;


	if (pMds->TxPause)
		return;
	if (!hal_driver_init_OK(pHwData))
		return;

	
	if (atomic_inc_return(&pMds->TxThreadCount) != 1)
		goto cleanup;

	
	do {
		FillIndex = pMds->TxFillIndex;
		if (pMds->TxOwner[FillIndex]) { 
			pr_debug("[Mds_Tx] Tx Owner is H/W.\n");
			break;
		}

		XmitBufAddress = pMds->pTxBuffer + (MAX_USB_TX_BUFFER * FillIndex); 
		XmitBufSize = 0;
		FillCount = 0;
		do {
			PacketSize = adapter->sMlmeFrame.len;
			if (!PacketSize)
				break;

			
			FragmentThreshold = CURRENT_FRAGMENT_THRESHOLD;
			
			FragmentCount = PacketSize/FragmentThreshold + 1;
			stmp = PacketSize + FragmentCount*32 + 8; 
			if ((XmitBufSize + stmp) >= MAX_USB_TX_BUFFER) {
				printk("[Mds_Tx] Excess max tx buffer.\n");
				break; 
			}


			BufferFilled = true;

			
			memset((u8 *)pTxDes + 1, 0, sizeof(struct wb35_descriptor) - 1);

			TxDesIndex = pMds->TxDesIndex; 
			pTxDes->Descriptor_ID = TxDesIndex;
			pMds->TxDesFrom[TxDesIndex] = 2; 
			pMds->TxDesIndex++;
			pMds->TxDesIndex %= MAX_USB_TX_DESCRIPTOR;

			MLME_GetNextPacket(adapter, pTxDes);

			
			Mds_HeaderCopy(adapter, pTxDes, XmitBufAddress);

			
			if (pTxDes->EapFix) {
				pr_debug("35: EPA 4th frame detected. Size = %d\n", PacketSize);
				pHwData->IsKeyPreSet = 1;
			}

			
			CurrentSize = Mds_BodyCopy(adapter, pTxDes, XmitBufAddress);

			
			Mds_DurationSet(adapter, pTxDes, XmitBufAddress);

			
			XmitBufSize += CurrentSize;
			XmitBufAddress += CurrentSize;

			
			MLME_SendComplete(adapter, 0, true);

			
			pMds->TxTsc++;
			if (pMds->TxTsc == 0)
				pMds->TxTsc_2++;

			FillCount++; 
		} while (HAL_USB_MODE_BURST(pHwData)); 

		
		if (BufferFilled) {
			
			pMds->TxBufferSize[FillIndex] = XmitBufSize;

			
			pMds->TxCountInBuffer[FillIndex] = FillCount;

			
			pMds->TxOwner[FillIndex] = 1;

			pMds->TxFillIndex++;
			pMds->TxFillIndex %= MAX_USB_TX_BUFFER_NUMBER;
			BufferFilled = false;
		} else
			break;

		if (!PacketSize) 
			break;

	} while (true);

	if (!pHwData->IsKeyPreSet)
		Wb35Tx_start(adapter);

cleanup:
		atomic_dec(&pMds->TxThreadCount);
}

void
Mds_SendComplete(struct wbsoft_priv *adapter, struct T02_descriptor *pT02)
{
	struct wb35_mds *pMds = &adapter->Mds;
	struct hw_data *pHwData = &adapter->sHwData;
	u8	PacketId = (u8)pT02->T02_Tx_PktID;
	unsigned char	SendOK = true;
	u8	RetryCount, TxRate;

	if (pT02->T02_IgnoreResult) 
		return;
	if (pT02->T02_IsLastMpdu) {
		
		
		TxRate = pMds->TxRate[PacketId][0];
		RetryCount = (u8)pT02->T02_MPDU_Cnt;
		if (pT02->value & FLAG_ERROR_TX_MASK) {
			SendOK = false;

			if (pT02->T02_transmit_abort || pT02->T02_out_of_MaxTxMSDULiftTime) {
				
				pHwData->dto_tx_retry_count += (RetryCount+1);
				
				if (RetryCount < 7)
					pHwData->tx_retry_count[RetryCount] += RetryCount;
				else
					pHwData->tx_retry_count[7] += RetryCount;
				pr_debug("dto_tx_retry_count =%d\n", pHwData->dto_tx_retry_count);
				MTO_SetTxCount(adapter, TxRate, RetryCount);
			}
			pHwData->dto_tx_frag_count += (RetryCount+1);

			
			if (pT02->T02_transmit_abort_due_to_TBTT)
				pHwData->tx_TBTT_start_count++;
			if (pT02->T02_transmit_without_encryption_due_to_wep_on_false)
				pHwData->tx_WepOn_false_count++;
			if (pT02->T02_discard_due_to_null_wep_key)
				pHwData->tx_Null_key_count++;
		} else {
			if (pT02->T02_effective_transmission_rate)
				pHwData->tx_ETR_count++;
			MTO_SetTxCount(adapter, TxRate, RetryCount);
		}

		
		pMds->TxResult[PacketId] = 0;
	} else
		pMds->TxResult[PacketId] |= ((u16)(pT02->value & 0x0ffff));
}
