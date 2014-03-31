#include "headers.h"

static void EThCSGetPktInfo(PMINI_ADAPTER Adapter,PVOID pvEthPayload,PS_ETHCS_PKT_INFO pstEthCsPktInfo);
static BOOLEAN EThCSClassifyPkt(PMINI_ADAPTER Adapter,struct sk_buff* skb,PS_ETHCS_PKT_INFO pstEthCsPktInfo,S_CLASSIFIER_RULE *pstClassifierRule, B_UINT8 EthCSCupport);

static USHORT	IpVersion4(PMINI_ADAPTER Adapter, struct iphdr *iphd,
			   S_CLASSIFIER_RULE *pstClassifierRule );

static VOID PruneQueue(PMINI_ADAPTER Adapter, INT iIndex);


BOOLEAN MatchSrcIpAddress(S_CLASSIFIER_RULE *pstClassifierRule,ULONG ulSrcIP)
{
    UCHAR 	ucLoopIndex=0;

    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);

    ulSrcIP=ntohl(ulSrcIP);
    if(0 == pstClassifierRule->ucIPSourceAddressLength)
       	return TRUE;
    for(ucLoopIndex=0; ucLoopIndex < (pstClassifierRule->ucIPSourceAddressLength);ucLoopIndex++)
    {
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Src Ip Address Mask:0x%x PacketIp:0x%x and Classification:0x%x", (UINT)pstClassifierRule->stSrcIpAddress.ulIpv4Mask[ucLoopIndex], (UINT)ulSrcIP, (UINT)pstClassifierRule->stSrcIpAddress.ulIpv6Addr[ucLoopIndex]);
		if((pstClassifierRule->stSrcIpAddress.ulIpv4Mask[ucLoopIndex] & ulSrcIP)==
				(pstClassifierRule->stSrcIpAddress.ulIpv4Addr[ucLoopIndex] & pstClassifierRule->stSrcIpAddress.ulIpv4Mask[ucLoopIndex] ))
       	{
       		return TRUE;
       	}
    }
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Src Ip Address Not Matched");
   	return FALSE;
}


BOOLEAN MatchDestIpAddress(S_CLASSIFIER_RULE *pstClassifierRule,ULONG ulDestIP)
{
	UCHAR 	ucLoopIndex=0;
    PMINI_ADAPTER	Adapter = GET_BCM_ADAPTER(gblpnetdev);

	ulDestIP=ntohl(ulDestIP);
    if(0 == pstClassifierRule->ucIPDestinationAddressLength)
       	return TRUE;
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Destination Ip Address 0x%x 0x%x 0x%x  ", (UINT)ulDestIP, (UINT)pstClassifierRule->stDestIpAddress.ulIpv4Mask[ucLoopIndex], (UINT)pstClassifierRule->stDestIpAddress.ulIpv4Addr[ucLoopIndex]);

    for(ucLoopIndex=0;ucLoopIndex<(pstClassifierRule->ucIPDestinationAddressLength);ucLoopIndex++)
    {
		if((pstClassifierRule->stDestIpAddress.ulIpv4Mask[ucLoopIndex] & ulDestIP)==
					(pstClassifierRule->stDestIpAddress.ulIpv4Addr[ucLoopIndex] & pstClassifierRule->stDestIpAddress.ulIpv4Mask[ucLoopIndex]))
       	{
       		return TRUE;
       	}
    }
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Destination Ip Address Not Matched");
    return FALSE;
}


BOOLEAN MatchTos(S_CLASSIFIER_RULE *pstClassifierRule,UCHAR ucTypeOfService)
{

	PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    if( 3 != pstClassifierRule->ucIPTypeOfServiceLength )
       	return TRUE;

    if(((pstClassifierRule->ucTosMask & ucTypeOfService)<=pstClassifierRule->ucTosHigh) && ((pstClassifierRule->ucTosMask & ucTypeOfService)>=pstClassifierRule->ucTosLow))
    {
       	return TRUE;
    }
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Type Of Service Not Matched");
    return FALSE;
}


BOOLEAN MatchProtocol(S_CLASSIFIER_RULE *pstClassifierRule,UCHAR ucProtocol)
{
   	UCHAR 	ucLoopIndex=0;
	PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	if(0 == pstClassifierRule->ucProtocolLength)
      	return TRUE;
	for(ucLoopIndex=0;ucLoopIndex<pstClassifierRule->ucProtocolLength;ucLoopIndex++)
    {
       	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Protocol:0x%X Classification Protocol:0x%X",ucProtocol,pstClassifierRule->ucProtocol[ucLoopIndex]);
       	if(pstClassifierRule->ucProtocol[ucLoopIndex]==ucProtocol)
       	{
       		return TRUE;
       	}
    }
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Protocol Not Matched");
   	return FALSE;
}


BOOLEAN MatchSrcPort(S_CLASSIFIER_RULE *pstClassifierRule,USHORT ushSrcPort)
{
    	UCHAR 	ucLoopIndex=0;

		PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);


    	if(0 == pstClassifierRule->ucSrcPortRangeLength)
        	return TRUE;
    	for(ucLoopIndex=0;ucLoopIndex<pstClassifierRule->ucSrcPortRangeLength;ucLoopIndex++)
    	{
        	if(ushSrcPort <= pstClassifierRule->usSrcPortRangeHi[ucLoopIndex] &&
		    ushSrcPort >= pstClassifierRule->usSrcPortRangeLo[ucLoopIndex])
	    	{
		    	return TRUE;
	    	}
    	}
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Src Port: %x Not Matched ",ushSrcPort);
    	return FALSE;
}


BOOLEAN MatchDestPort(S_CLASSIFIER_RULE *pstClassifierRule,USHORT ushDestPort)
{
    	UCHAR 	ucLoopIndex=0;
		PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);

    	if(0 == pstClassifierRule->ucDestPortRangeLength)
        	return TRUE;

    	for(ucLoopIndex=0;ucLoopIndex<pstClassifierRule->ucDestPortRangeLength;ucLoopIndex++)
    	{
        	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Matching Port:0x%X   0x%X  0x%X",ushDestPort,pstClassifierRule->usDestPortRangeLo[ucLoopIndex],pstClassifierRule->usDestPortRangeHi[ucLoopIndex]);

 		if(ushDestPort <= pstClassifierRule->usDestPortRangeHi[ucLoopIndex] &&
		    ushDestPort >= pstClassifierRule->usDestPortRangeLo[ucLoopIndex])
	    	{
		    return TRUE;
	    	}
    	}
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Dest Port: %x Not Matched",ushDestPort);
    	return FALSE;
}
static USHORT	IpVersion4(PMINI_ADAPTER Adapter,
			   struct iphdr *iphd,
			   S_CLASSIFIER_RULE *pstClassifierRule )
{
	xporthdr     		*xprt_hdr=NULL;
	BOOLEAN	bClassificationSucceed=FALSE;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "========>");

	xprt_hdr=(xporthdr *)((PUCHAR)iphd + sizeof(struct iphdr));

	do {
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Trying to see Direction = %d %d",
			pstClassifierRule->ucDirection,
			pstClassifierRule->usVCID_Value);

		
		if(!pstClassifierRule->bUsed || pstClassifierRule->ucDirection == DOWNLINK_DIR)
		{
			bClassificationSucceed = FALSE;
			break;
		}

		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "is IPv6 check!");
		if(pstClassifierRule->bIpv6Protocol)
			break;

		
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Trying to match Source IP Address");
		if(FALSE == (bClassificationSucceed =
			MatchSrcIpAddress(pstClassifierRule, iphd->saddr)))
			break;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Source IP Address Matched");

		if(FALSE == (bClassificationSucceed =
			MatchDestIpAddress(pstClassifierRule, iphd->daddr)))
			break;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Destination IP Address Matched");

		if(FALSE == (bClassificationSucceed =
			MatchTos(pstClassifierRule, iphd->tos)))
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "TOS Match failed\n");
			break;
		}
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "TOS Matched");

		if(FALSE == (bClassificationSucceed =
			MatchProtocol(pstClassifierRule,iphd->protocol)))
			break;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Protocol Matched");

		
		if(iphd->protocol!=TCP && iphd->protocol!=UDP)
			break;
		
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Source Port %04x",
			(iphd->protocol==UDP)?xprt_hdr->uhdr.source:xprt_hdr->thdr.source);

		if(FALSE == (bClassificationSucceed =
			MatchSrcPort(pstClassifierRule,
				ntohs((iphd->protocol == UDP)?
				xprt_hdr->uhdr.source:xprt_hdr->thdr.source))))
			break;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Src Port Matched");

		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "Destination Port %04x",
			(iphd->protocol==UDP)?xprt_hdr->uhdr.dest:
			xprt_hdr->thdr.dest);
		if(FALSE == (bClassificationSucceed =
			MatchDestPort(pstClassifierRule,
			ntohs((iphd->protocol == UDP)?
			xprt_hdr->uhdr.dest:xprt_hdr->thdr.dest))))
			break;
	} while(0);

	if(TRUE==bClassificationSucceed)
	{
		INT iMatchedSFQueueIndex = 0;
		iMatchedSFQueueIndex = SearchSfid(Adapter,pstClassifierRule->ulSFID);
		if(iMatchedSFQueueIndex >= NO_OF_QUEUES)
		{
			bClassificationSucceed = FALSE;
		}
		else
		{
			if(FALSE == Adapter->PackInfo[iMatchedSFQueueIndex].bActive)
			{
				bClassificationSucceed = FALSE;
			}
		}
	}

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "IpVersion4 <==========");

	return bClassificationSucceed;
}

VOID PruneQueueAllSF(PMINI_ADAPTER Adapter)
{
	UINT iIndex = 0;

	for(iIndex = 0; iIndex < HiPriority; iIndex++)
	{
		if(!Adapter->PackInfo[iIndex].bValid)
			continue;

		PruneQueue(Adapter, iIndex);
	}
}


static VOID PruneQueue(PMINI_ADAPTER Adapter, INT iIndex)
{
	struct sk_buff* PacketToDrop=NULL;
	struct net_device_stats *netstats;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, PRUNE_QUEUE, DBG_LVL_ALL, "=====> Index %d",iIndex);

   	if(iIndex == HiPriority)
		return;

	if(!Adapter || (iIndex < 0) || (iIndex > HiPriority))
		return;

	
	netstats = &Adapter->dev->stats;

	spin_lock_bh(&Adapter->PackInfo[iIndex].SFQueueLock);

	while(1)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, PRUNE_QUEUE, DBG_LVL_ALL, "uiCurrentBytesOnHost:%x uiMaxBucketSize :%x",
		Adapter->PackInfo[iIndex].uiCurrentBytesOnHost,
		Adapter->PackInfo[iIndex].uiMaxBucketSize);

		PacketToDrop = Adapter->PackInfo[iIndex].FirstTxQueue;

		if(PacketToDrop == NULL)
			break;
		if((Adapter->PackInfo[iIndex].uiCurrentPacketsOnHost < SF_MAX_ALLOWED_PACKETS_TO_BACKUP) &&
			((1000*(jiffies - *((B_UINT32 *)(PacketToDrop->cb)+SKB_CB_LATENCY_OFFSET))/HZ) <= Adapter->PackInfo[iIndex].uiMaxLatency))
			break;

		if(PacketToDrop)
		{
			if (netif_msg_tx_err(Adapter))
				pr_info(PFX "%s: tx queue %d overlimit\n", 
					Adapter->dev->name, iIndex);

			netstats->tx_dropped++;

			DEQUEUEPACKET(Adapter->PackInfo[iIndex].FirstTxQueue,
						Adapter->PackInfo[iIndex].LastTxQueue);
			
			Adapter->PackInfo[iIndex].uiCurrentBytesOnHost -=
				PacketToDrop->len;
			Adapter->PackInfo[iIndex].uiCurrentPacketsOnHost--;
			
			Adapter->PackInfo[iIndex].uiDroppedCountBytes += PacketToDrop->len;
			Adapter->PackInfo[iIndex].uiDroppedCountPackets++;
			dev_kfree_skb(PacketToDrop);

		}

		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, PRUNE_QUEUE, DBG_LVL_ALL, "Dropped Bytes:%x Dropped Packets:%x",
			Adapter->PackInfo[iIndex].uiDroppedCountBytes,
			Adapter->PackInfo[iIndex].uiDroppedCountPackets);

		atomic_dec(&Adapter->TotalPacketCount);
	}

	spin_unlock_bh(&Adapter->PackInfo[iIndex].SFQueueLock);

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, PRUNE_QUEUE, DBG_LVL_ALL, "TotalPacketCount:%x",
		atomic_read(&Adapter->TotalPacketCount));
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, PRUNE_QUEUE, DBG_LVL_ALL, "<=====");
}

VOID flush_all_queues(PMINI_ADAPTER Adapter)
{
	INT		iQIndex;
	UINT	uiTotalPacketLength;
	struct sk_buff*			PacketToDrop=NULL;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "=====>");

	for(iQIndex=LowPriority; iQIndex<HiPriority; iQIndex++)
	{
		struct net_device_stats *netstats = &Adapter->dev->stats;

		spin_lock_bh(&Adapter->PackInfo[iQIndex].SFQueueLock);
		while(Adapter->PackInfo[iQIndex].FirstTxQueue)
		{
			PacketToDrop = Adapter->PackInfo[iQIndex].FirstTxQueue;
			if(PacketToDrop)
			{
				uiTotalPacketLength = PacketToDrop->len;
				netstats->tx_dropped++;
			}
			else
				uiTotalPacketLength = 0;

			DEQUEUEPACKET(Adapter->PackInfo[iQIndex].FirstTxQueue,
						Adapter->PackInfo[iQIndex].LastTxQueue);

			
			dev_kfree_skb(PacketToDrop);

			
			Adapter->PackInfo[iQIndex].uiCurrentBytesOnHost -= uiTotalPacketLength;
			Adapter->PackInfo[iQIndex].uiCurrentPacketsOnHost--;

			
			Adapter->PackInfo[iQIndex].uiDroppedCountBytes += uiTotalPacketLength;
			Adapter->PackInfo[iQIndex].uiDroppedCountPackets++;

			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "Dropped Bytes:%x Dropped Packets:%x",
					Adapter->PackInfo[iQIndex].uiDroppedCountBytes,
					Adapter->PackInfo[iQIndex].uiDroppedCountPackets);
			atomic_dec(&Adapter->TotalPacketCount);
		}
		spin_unlock_bh(&Adapter->PackInfo[iQIndex].SFQueueLock);
	}
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "<=====");
}

USHORT ClassifyPacket(PMINI_ADAPTER Adapter,struct sk_buff* skb)
{
	INT			uiLoopIndex=0;
	S_CLASSIFIER_RULE *pstClassifierRule = NULL;
	S_ETHCS_PKT_INFO stEthCsPktInfo;
	PVOID pvEThPayload = NULL;
	struct iphdr 		*pIpHeader = NULL;
	INT	  uiSfIndex=0;
	USHORT	usIndex=Adapter->usBestEffortQueueIndex;
	BOOLEAN	bFragmentedPkt=FALSE,bClassificationSucceed=FALSE;
	USHORT	usCurrFragment =0;

	PTCP_HEADER pTcpHeader;
	UCHAR IpHeaderLength;
	UCHAR TcpHeaderLength;

	pvEThPayload = skb->data;
	*((UINT32*) (skb->cb) +SKB_CB_TCPACK_OFFSET ) = 0;
	EThCSGetPktInfo(Adapter,pvEThPayload,&stEthCsPktInfo);

	switch(stEthCsPktInfo.eNwpktEthFrameType)
	{
		case eEth802LLCFrame:
		{
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "ClassifyPacket : 802LLCFrame\n");
            pIpHeader = pvEThPayload + sizeof(ETH_CS_802_LLC_FRAME);
			break;
		}

		case eEth802LLCSNAPFrame:
		{
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "ClassifyPacket : 802LLC SNAP Frame\n");
			pIpHeader = pvEThPayload + sizeof(ETH_CS_802_LLC_SNAP_FRAME);
			break;
		}
		case eEth802QVLANFrame:
		{
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "ClassifyPacket : 802.1Q VLANFrame\n");
			pIpHeader = pvEThPayload + sizeof(ETH_CS_802_Q_FRAME);
			break;
		}
		case eEthOtherFrame:
		{
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "ClassifyPacket : ETH Other Frame\n");
			pIpHeader = pvEThPayload + sizeof(ETH_CS_ETH2_FRAME);
			break;
		}
		default:
		{
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "ClassifyPacket : Unrecognized ETH Frame\n");
			pIpHeader = pvEThPayload + sizeof(ETH_CS_ETH2_FRAME);
			break;
		}
	}

	if(stEthCsPktInfo.eNwpktIPFrameType == eIPv4Packet)
	{
		usCurrFragment = (ntohs(pIpHeader->frag_off) & IP_OFFSET);
		if((ntohs(pIpHeader->frag_off) & IP_MF) || usCurrFragment)
			bFragmentedPkt = TRUE;

		if(bFragmentedPkt)
		{
				
			pstClassifierRule = GetFragIPClsEntry(Adapter,pIpHeader->id, pIpHeader->saddr);
			if(pstClassifierRule)
			{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,"It is next Fragmented pkt");
					bClassificationSucceed=TRUE;
			}
			if(!(ntohs(pIpHeader->frag_off) & IP_MF))
			{
				
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,"This is the last fragmented Pkt");
				DelFragIPClsEntry(Adapter,pIpHeader->id, pIpHeader->saddr);
			}
		}
	}

	for(uiLoopIndex = MAX_CLASSIFIERS - 1; uiLoopIndex >= 0; uiLoopIndex--)
	{
		if(bClassificationSucceed)
			break;
		
		
		do
		{
			if(FALSE==Adapter->astClassifierTable[uiLoopIndex].bUsed)
			{
				bClassificationSucceed=FALSE;
				break;
			}
			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "Adapter->PackInfo[%d].bvalid=True\n",uiLoopIndex);

			if(0 == Adapter->astClassifierTable[uiLoopIndex].ucDirection)
			{
				bClassificationSucceed=FALSE;
				break;						
			}

			pstClassifierRule = &Adapter->astClassifierTable[uiLoopIndex];

			uiSfIndex = SearchSfid(Adapter,pstClassifierRule->ulSFID);
			if (uiSfIndex >= NO_OF_QUEUES) {
				BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "Queue Not Valid. SearchSfid for this classifier Failed\n");
				break;
			}

			if(Adapter->PackInfo[uiSfIndex].bEthCSSupport)
			{

				if(eEthUnsupportedFrame==stEthCsPktInfo.eNwpktEthFrameType)
				{
					BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  " ClassifyPacket : Packet Not a Valid Supported Ethernet Frame \n");
					bClassificationSucceed = FALSE;
					break;
				}



				BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "Performing ETH CS Classification on Classifier Rule ID : %x Service Flow ID : %lx\n",pstClassifierRule->uiClassifierRuleIndex,Adapter->PackInfo[uiSfIndex].ulSFID);
				bClassificationSucceed = EThCSClassifyPkt(Adapter,skb,&stEthCsPktInfo,pstClassifierRule, Adapter->PackInfo[uiSfIndex].bEthCSSupport);

				if(!bClassificationSucceed)
				{
					BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ClassifyPacket : Ethernet CS Classification Failed\n");
					break;
				}
			}

			else 
			{
				if(eEthOtherFrame != stEthCsPktInfo.eNwpktEthFrameType)
				{
					BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  " ClassifyPacket : Packet Not a 802.3 Ethernet Frame... hence not allowed over non-ETH CS SF \n");
					bClassificationSucceed = FALSE;
					break;
				}
			}

			BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "Proceeding to IP CS Clasification");

			if(Adapter->PackInfo[uiSfIndex].bIPCSSupport)
			{

				if(stEthCsPktInfo.eNwpktIPFrameType == eNonIPPacket)
				{
					BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  " ClassifyPacket : Packet is Not an IP Packet \n");
					bClassificationSucceed = FALSE;
					break;
				}
				BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "Dump IP Header : \n");
				DumpFullPacket((PUCHAR)pIpHeader,20);

				if(stEthCsPktInfo.eNwpktIPFrameType == eIPv4Packet)
					bClassificationSucceed = IpVersion4(Adapter,pIpHeader,pstClassifierRule);
				else if(stEthCsPktInfo.eNwpktIPFrameType == eIPv6Packet)
					bClassificationSucceed = IpVersion6(Adapter,pIpHeader,pstClassifierRule);
			}

		}while(0);
	}

	if(bClassificationSucceed == TRUE)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "CF id : %d, SF ID is =%lu",pstClassifierRule->uiClassifierRuleIndex, pstClassifierRule->ulSFID);

		
		*((UINT32*)(skb->cb)+SKB_CB_CLASSIFICATION_OFFSET) = pstClassifierRule->uiClassifierRuleIndex;
		if((TCP == pIpHeader->protocol ) && !bFragmentedPkt && (ETH_AND_IP_HEADER_LEN + TCP_HEADER_LEN <= skb->len) )
		{
			 IpHeaderLength   = pIpHeader->ihl;
			 pTcpHeader = (PTCP_HEADER)(((PUCHAR)pIpHeader)+(IpHeaderLength*4));
			 TcpHeaderLength  = GET_TCP_HEADER_LEN(pTcpHeader->HeaderLength);

			if((pTcpHeader->ucFlags & TCP_ACK) &&
			   (ntohs(pIpHeader->tot_len) == (IpHeaderLength*4)+(TcpHeaderLength*4)))
			{
    			*((UINT32*) (skb->cb) +SKB_CB_TCPACK_OFFSET ) = TCP_ACK;
			}
		}

		usIndex = SearchSfid(Adapter, pstClassifierRule->ulSFID);
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL, "index is	=%d", usIndex);

		
		if(bFragmentedPkt && (usCurrFragment == 0))
		{
			
			S_FRAGMENTED_PACKET_INFO stFragPktInfo;
			stFragPktInfo.bUsed = TRUE;
			stFragPktInfo.ulSrcIpAddress = pIpHeader->saddr;
			stFragPktInfo.usIpIdentification = pIpHeader->id;
			stFragPktInfo.pstMatchedClassifierEntry = pstClassifierRule;
			stFragPktInfo.bOutOfOrderFragment = FALSE;
			AddFragIPClsEntry(Adapter,&stFragPktInfo);
		}


	}

	if(bClassificationSucceed)
		return usIndex;
	else
		return INVALID_QUEUE_INDEX;
}

static BOOLEAN EthCSMatchSrcMACAddress(S_CLASSIFIER_RULE *pstClassifierRule,PUCHAR Mac)
{
	UINT i=0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	if(pstClassifierRule->ucEthCSSrcMACLen==0)
		return TRUE;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s \n",__FUNCTION__);
	for(i=0;i<MAC_ADDRESS_SIZE;i++)
	{
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "SRC MAC[%x] = %x ClassifierRuleSrcMAC = %x Mask : %x\n",i,Mac[i],pstClassifierRule->au8EThCSSrcMAC[i],pstClassifierRule->au8EThCSSrcMACMask[i]);
		if((pstClassifierRule->au8EThCSSrcMAC[i] & pstClassifierRule->au8EThCSSrcMACMask[i])!=
			(Mac[i] & pstClassifierRule->au8EThCSSrcMACMask[i]))
			return FALSE;
	}
	return TRUE;
}

static BOOLEAN EthCSMatchDestMACAddress(S_CLASSIFIER_RULE *pstClassifierRule,PUCHAR Mac)
{
	UINT i=0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	if(pstClassifierRule->ucEthCSDestMACLen==0)
		return TRUE;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s \n",__FUNCTION__);
	for(i=0;i<MAC_ADDRESS_SIZE;i++)
	{
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "SRC MAC[%x] = %x ClassifierRuleSrcMAC = %x Mask : %x\n",i,Mac[i],pstClassifierRule->au8EThCSDestMAC[i],pstClassifierRule->au8EThCSDestMACMask[i]);
		if((pstClassifierRule->au8EThCSDestMAC[i] & pstClassifierRule->au8EThCSDestMACMask[i])!=
			(Mac[i] & pstClassifierRule->au8EThCSDestMACMask[i]))
			return FALSE;
	}
	return TRUE;
}

static BOOLEAN EthCSMatchEThTypeSAP(S_CLASSIFIER_RULE *pstClassifierRule,struct sk_buff* skb,PS_ETHCS_PKT_INFO pstEthCsPktInfo)
{
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	if((pstClassifierRule->ucEtherTypeLen==0)||
		(pstClassifierRule->au8EthCSEtherType[0] == 0))
		return TRUE;

	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s SrcEtherType:%x CLS EtherType[0]:%x\n",__FUNCTION__,pstEthCsPktInfo->usEtherType,pstClassifierRule->au8EthCSEtherType[0]);
	if(pstClassifierRule->au8EthCSEtherType[0] == 1)
	{
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s  CLS EtherType[1]:%x EtherType[2]:%x\n",__FUNCTION__,pstClassifierRule->au8EthCSEtherType[1],pstClassifierRule->au8EthCSEtherType[2]);

		if(memcmp(&pstEthCsPktInfo->usEtherType,&pstClassifierRule->au8EthCSEtherType[1],2)==0)
			return TRUE;
		else
			return FALSE;
	}

	if(pstClassifierRule->au8EthCSEtherType[0] == 2)
	{
		if(eEth802LLCFrame != pstEthCsPktInfo->eNwpktEthFrameType)
			return FALSE;

		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s  EthCS DSAP:%x EtherType[2]:%x\n",__FUNCTION__,pstEthCsPktInfo->ucDSAP,pstClassifierRule->au8EthCSEtherType[2]);
		if(pstEthCsPktInfo->ucDSAP == pstClassifierRule->au8EthCSEtherType[2])
			return TRUE;
		else
			return FALSE;

	}

	return FALSE;

}

static BOOLEAN EthCSMatchVLANRules(S_CLASSIFIER_RULE *pstClassifierRule,struct sk_buff* skb,PS_ETHCS_PKT_INFO pstEthCsPktInfo)
{
	BOOLEAN bClassificationSucceed = FALSE;
	USHORT usVLANID;
	B_UINT8 uPriority = 0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);

	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s  CLS UserPrio:%x CLS VLANID:%x\n",__FUNCTION__,ntohs(*((USHORT *)pstClassifierRule->usUserPriority)),pstClassifierRule->usVLANID);

	
	if(pstClassifierRule->usValidityBitMap & (1<<PKT_CLASSIFICATION_USER_PRIORITY_VALID))
	{
		if(pstEthCsPktInfo->eNwpktEthFrameType!=eEth802QVLANFrame)
				return FALSE;

		uPriority = (ntohs(*(USHORT *)(skb->data + sizeof(ETH_HEADER_STRUC))) & 0xF000) >> 13;

		if((uPriority >= pstClassifierRule->usUserPriority[0]) && (uPriority <= pstClassifierRule->usUserPriority[1]))
				bClassificationSucceed = TRUE;

		if(!bClassificationSucceed)
			return FALSE;
	}

	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS 802.1 D  User Priority Rule Matched\n");

	bClassificationSucceed = FALSE;

	if(pstClassifierRule->usValidityBitMap & (1<<PKT_CLASSIFICATION_VLANID_VALID))
	{
		if(pstEthCsPktInfo->eNwpktEthFrameType!=eEth802QVLANFrame)
				return FALSE;

		usVLANID = ntohs(*(USHORT *)(skb->data + sizeof(ETH_HEADER_STRUC))) & 0xFFF;

		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "%s  Pkt VLANID %x Priority: %d\n",__FUNCTION__,usVLANID, uPriority);

		if(usVLANID == ((pstClassifierRule->usVLANID & 0xFFF0) >> 4))
			bClassificationSucceed = TRUE;

		if(!bClassificationSucceed)
			return FALSE;
	}

	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS 802.1 Q VLAN ID Rule Matched\n");

	return TRUE;
}


static BOOLEAN EThCSClassifyPkt(PMINI_ADAPTER Adapter,struct sk_buff* skb,
				PS_ETHCS_PKT_INFO pstEthCsPktInfo,
				S_CLASSIFIER_RULE *pstClassifierRule,
				B_UINT8 EthCSCupport)
{
	BOOLEAN bClassificationSucceed = FALSE;
	bClassificationSucceed = EthCSMatchSrcMACAddress(pstClassifierRule,((ETH_HEADER_STRUC *)(skb->data))->au8SourceAddress);
	if(!bClassificationSucceed)
		return FALSE;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS SrcMAC Matched\n");

	bClassificationSucceed = EthCSMatchDestMACAddress(pstClassifierRule,((ETH_HEADER_STRUC*)(skb->data))->au8DestinationAddress);
	if(!bClassificationSucceed)
		return FALSE;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS DestMAC Matched\n");

	
	bClassificationSucceed = EthCSMatchEThTypeSAP(pstClassifierRule,skb,pstEthCsPktInfo);
	if(!bClassificationSucceed)
		return FALSE;

	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS EthType/802.2SAP Matched\n");

	

	bClassificationSucceed = EthCSMatchVLANRules(pstClassifierRule,skb,pstEthCsPktInfo);
	if(!bClassificationSucceed)
		return FALSE;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "ETH CS 802.1 VLAN Rules Matched\n");

	return bClassificationSucceed;
}

static void EThCSGetPktInfo(PMINI_ADAPTER Adapter,PVOID pvEthPayload,
			    PS_ETHCS_PKT_INFO pstEthCsPktInfo)
{
	USHORT u16Etype = ntohs(((ETH_HEADER_STRUC*)pvEthPayload)->u16Etype);

	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "EthCSGetPktInfo : Eth Hdr Type : %X\n",u16Etype);
	if(u16Etype > 0x5dc)
	{
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "EthCSGetPktInfo : ETH2 Frame \n");
		
		if(u16Etype == ETHERNET_FRAMETYPE_802QVLAN)
		{
			
			pstEthCsPktInfo->eNwpktEthFrameType = eEth802QVLANFrame;
			u16Etype = ((ETH_CS_802_Q_FRAME*)pvEthPayload)->EthType;
			
		}
		else
		{
			pstEthCsPktInfo->eNwpktEthFrameType = eEthOtherFrame;
			u16Etype = ntohs(u16Etype);
		}

	}
	else
	{
		
		BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "802.2 LLC Frame \n");
		pstEthCsPktInfo->eNwpktEthFrameType = eEth802LLCFrame;
		pstEthCsPktInfo->ucDSAP = ((ETH_CS_802_LLC_FRAME*)pvEthPayload)->DSAP;
		if(pstEthCsPktInfo->ucDSAP == 0xAA && ((ETH_CS_802_LLC_FRAME*)pvEthPayload)->SSAP == 0xAA)
		{
			
			pstEthCsPktInfo->eNwpktEthFrameType = eEth802LLCSNAPFrame;
			u16Etype = ((ETH_CS_802_LLC_SNAP_FRAME*)pvEthPayload)->usEtherType;
		}
	}
	if(u16Etype == ETHERNET_FRAMETYPE_IPV4)
		pstEthCsPktInfo->eNwpktIPFrameType = eIPv4Packet;
	else if(u16Etype == ETHERNET_FRAMETYPE_IPV6)
		pstEthCsPktInfo->eNwpktIPFrameType = eIPv6Packet;
	else
		pstEthCsPktInfo->eNwpktIPFrameType = eNonIPPacket;

	pstEthCsPktInfo->usEtherType = ((ETH_HEADER_STRUC*)pvEthPayload)->u16Etype;
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "EthCsPktInfo->eNwpktIPFrameType : %x\n",pstEthCsPktInfo->eNwpktIPFrameType);
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "EthCsPktInfo->eNwpktEthFrameType : %x\n",pstEthCsPktInfo->eNwpktEthFrameType);
	BCM_DEBUG_PRINT(Adapter, DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,  "EthCsPktInfo->usEtherType : %x\n",pstEthCsPktInfo->usEtherType);
}



