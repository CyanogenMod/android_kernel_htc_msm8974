#include "headers.h"

static UINT CreateSFToClassifierRuleMapping(B_UINT16 uiVcid,B_UINT16  uiClsId,S_SERVICEFLOW_TABLE *psServiceFlowTable,S_PHS_RULE *psPhsRule,B_UINT8 u8AssociatedPHSI);

static UINT CreateClassiferToPHSRuleMapping(B_UINT16 uiVcid,B_UINT16  uiClsId,S_SERVICEFLOW_ENTRY *pstServiceFlowEntry,S_PHS_RULE *psPhsRule,B_UINT8 u8AssociatedPHSI);

static UINT CreateClassifierPHSRule(B_UINT16  uiClsId,S_CLASSIFIER_TABLE *psaClassifiertable ,S_PHS_RULE *psPhsRule,E_CLASSIFIER_ENTRY_CONTEXT eClsContext,B_UINT8 u8AssociatedPHSI);

static UINT UpdateClassifierPHSRule(B_UINT16  uiClsId,S_CLASSIFIER_ENTRY *pstClassifierEntry,S_CLASSIFIER_TABLE *psaClassifiertable ,S_PHS_RULE *psPhsRule,B_UINT8 u8AssociatedPHSI);

static BOOLEAN ValidatePHSRuleComplete(S_PHS_RULE *psPhsRule);

static BOOLEAN DerefPhsRule(B_UINT16  uiClsId,S_CLASSIFIER_TABLE *psaClassifiertable,S_PHS_RULE *pstPhsRule);

static UINT GetClassifierEntry(S_CLASSIFIER_TABLE *pstClassifierTable,B_UINT32 uiClsid,E_CLASSIFIER_ENTRY_CONTEXT eClsContext, S_CLASSIFIER_ENTRY **ppstClassifierEntry);

static UINT GetPhsRuleEntry(S_CLASSIFIER_TABLE *pstClassifierTable,B_UINT32 uiPHSI,E_CLASSIFIER_ENTRY_CONTEXT eClsContext,S_PHS_RULE **ppstPhsRule);

static void free_phs_serviceflow_rules(S_SERVICEFLOW_TABLE *psServiceFlowRulesTable);

static int phs_compress(S_PHS_RULE   *phs_members,unsigned char *in_buf,
						unsigned char *out_buf,unsigned int *header_size,UINT *new_header_size );


static int verify_suppress_phsf(unsigned char *in_buffer,unsigned char *out_buffer,
								unsigned char *phsf,unsigned char *phsm,unsigned int phss,unsigned int phsv,UINT *new_header_size );

static int phs_decompress(unsigned char *in_buf,unsigned char *out_buf,\
						  S_PHS_RULE   *phs_rules,UINT *header_size);


static ULONG PhsCompress(void* pvContext,
				  B_UINT16 uiVcid,
				  B_UINT16 uiClsId,
				  void *pvInputBuffer,
				  void *pvOutputBuffer,
				  UINT *pOldHeaderSize,
				  UINT *pNewHeaderSize );

static ULONG PhsDeCompress(void* pvContext,
				  B_UINT16 uiVcid,
				  void *pvInputBuffer,
				  void *pvOutputBuffer,
				  UINT *pInHeaderSize,
				  UINT *pOutHeaderSize);



#define IN
#define OUT


int PHSTransmit(PMINI_ADAPTER Adapter,
					 struct sk_buff	**pPacket,
					 USHORT Vcid,
					 B_UINT16 uiClassifierRuleID,
					 BOOLEAN bHeaderSuppressionEnabled,
					 UINT *PacketLen,
					 UCHAR bEthCSSupport)
{

	
	UINT    unPHSPktHdrBytesCopied = 0;
	UINT	unPhsOldHdrSize = 0;
	UINT	unPHSNewPktHeaderLen = 0;
	
	PUCHAR pucPHSPktHdrInBuf =
				Adapter->stPhsTxContextInfo.ucaHdrSupressionInBuf;
	
	PUCHAR  pucPHSPktHdrOutBuf =
					Adapter->stPhsTxContextInfo.ucaHdrSupressionOutBuf;
	UINT       usPacketType;
	UINT       BytesToRemove=0;
	BOOLEAN  bPHSI = 0;
	LONG ulPhsStatus = 0;
	UINT 	numBytesCompressed = 0;
	struct sk_buff *newPacket = NULL;
	struct sk_buff *Packet = *pPacket;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL, "In PHSTransmit");

	if(!bEthCSSupport)
		BytesToRemove=ETH_HLEN;

	usPacketType=((struct ethhdr *)(Packet->data))->h_proto;


	pucPHSPktHdrInBuf = Packet->data + BytesToRemove;
	
	if((*PacketLen - BytesToRemove) < MAX_PHS_LENGTHS)
	{

		unPHSPktHdrBytesCopied = (*PacketLen - BytesToRemove);
	}
	else
	{
		unPHSPktHdrBytesCopied = MAX_PHS_LENGTHS;
	}

	if( (unPHSPktHdrBytesCopied > 0 ) &&
		(unPHSPktHdrBytesCopied <= MAX_PHS_LENGTHS))
	{


		
	
		if(((usPacketType == ETHERNET_FRAMETYPE_IPV4) ||
			(usPacketType == ETHERNET_FRAMETYPE_IPV6)) &&
			(bHeaderSuppressionEnabled))
		{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nTrying to PHS Compress Using Classifier rule 0x%X",uiClassifierRuleID);


				unPHSNewPktHeaderLen = unPHSPktHdrBytesCopied;
				ulPhsStatus = PhsCompress(&Adapter->stBCMPhsContext,
					Vcid,
					uiClassifierRuleID,
					pucPHSPktHdrInBuf,
					pucPHSPktHdrOutBuf,
					&unPhsOldHdrSize,
					&unPHSNewPktHeaderLen);
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nPHS Old header Size : %d New Header Size  %d\n",unPhsOldHdrSize,unPHSNewPktHeaderLen);

				if(unPHSNewPktHeaderLen == unPhsOldHdrSize)
				{
					if(  ulPhsStatus == STATUS_PHS_COMPRESSED)
							bPHSI = *pucPHSPktHdrOutBuf;
					ulPhsStatus = STATUS_PHS_NOCOMPRESSION;
				}

				if(  ulPhsStatus == STATUS_PHS_COMPRESSED)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"PHS Sending packet Compressed");

					if(skb_cloned(Packet))
					{
						newPacket = skb_copy(Packet, GFP_ATOMIC);

						if(newPacket == NULL)
							return STATUS_FAILURE;

						dev_kfree_skb(Packet);
						*pPacket = Packet = newPacket;
						pucPHSPktHdrInBuf = Packet->data  + BytesToRemove;
					}

					numBytesCompressed = unPhsOldHdrSize - (unPHSNewPktHeaderLen+PHSI_LEN);

					memcpy(pucPHSPktHdrInBuf + numBytesCompressed, pucPHSPktHdrOutBuf, unPHSNewPktHeaderLen + PHSI_LEN);
					memcpy(Packet->data + numBytesCompressed, Packet->data, BytesToRemove);
					skb_pull(Packet, numBytesCompressed);

					return STATUS_SUCCESS;
				}

				else
				{
					
					if(!(skb_headroom(Packet) > 0))
					{
						if(skb_cow(Packet, 1))
						{
							BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "SKB Cow Failed\n");
							return STATUS_FAILURE;
						}
					}
					skb_push(Packet, 1);

					
					*(Packet->data + BytesToRemove) = bPHSI;
					return STATUS_SUCCESS;
			}
		}
		else
		{
			if(!bHeaderSuppressionEnabled)
			{
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nHeader Suppression Disabled For SF: No PHS\n");
			}

			return STATUS_SUCCESS;
		}
	}

	
	return STATUS_SUCCESS;
}

int PHSReceive(PMINI_ADAPTER Adapter,
					USHORT usVcid,
					struct sk_buff *packet,
					UINT *punPacketLen,
					UCHAR *pucEthernetHdr,
					UINT	bHeaderSuppressionEnabled)
{
	u32   nStandardPktHdrLen            		= 0;
	u32   nTotalsupressedPktHdrBytes  = 0;
	int     ulPhsStatus 		= 0;
	PUCHAR pucInBuff = NULL ;
	UINT TotalBytesAdded = 0;
	if(!bHeaderSuppressionEnabled)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECEIVE,DBG_LVL_ALL,"\nPhs Disabled for incoming packet");
		return ulPhsStatus;
	}

	pucInBuff = packet->data;

	
	nStandardPktHdrLen = packet->len;
	ulPhsStatus = PhsDeCompress(&Adapter->stBCMPhsContext,
		usVcid,
		pucInBuff,
		Adapter->ucaPHSPktRestoreBuf,
		&nTotalsupressedPktHdrBytes,
		&nStandardPktHdrLen);

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECEIVE,DBG_LVL_ALL,"\nSupressed PktHdrLen : 0x%x Restored PktHdrLen : 0x%x",
					nTotalsupressedPktHdrBytes,nStandardPktHdrLen);

	if(ulPhsStatus != STATUS_PHS_COMPRESSED)
	{
		skb_pull(packet, 1);
		return STATUS_SUCCESS;
	}
	else
	{
		TotalBytesAdded = nStandardPktHdrLen - nTotalsupressedPktHdrBytes - PHSI_LEN;
		if(TotalBytesAdded)
		{
			if(skb_headroom(packet) >= (SKB_RESERVE_ETHERNET_HEADER + TotalBytesAdded))
				skb_push(packet, TotalBytesAdded);
			else
			{
				if(skb_cow(packet, skb_headroom(packet) + TotalBytesAdded))
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_PRINTK, 0, 0, "cow failed in receive\n");
					return STATUS_FAILURE;
				}

				skb_push(packet, TotalBytesAdded);
			}
		}

		memcpy(packet->data, Adapter->ucaPHSPktRestoreBuf, nStandardPktHdrLen);
	}

	return STATUS_SUCCESS;
}

void DumpFullPacket(UCHAR *pBuf,UINT nPktLen)
{
	PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,"Dumping Data Packet");
    BCM_DEBUG_PRINT_BUFFER(Adapter,DBG_TYPE_TX, IPV4_DBG, DBG_LVL_ALL,pBuf,nPktLen);
}

int phs_init(PPHS_DEVICE_EXTENSION pPhsdeviceExtension,PMINI_ADAPTER Adapter)
{
	int i;
	S_SERVICEFLOW_TABLE *pstServiceFlowTable;
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nPHS:phs_init function ");

	if(pPhsdeviceExtension->pstServiceFlowPhsRulesTable)
		return -EINVAL;

	pPhsdeviceExtension->pstServiceFlowPhsRulesTable =
		kzalloc(sizeof(S_SERVICEFLOW_TABLE), GFP_KERNEL);

    if(!pPhsdeviceExtension->pstServiceFlowPhsRulesTable)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nAllocation ServiceFlowPhsRulesTable failed");
		return -ENOMEM;
	}

	pstServiceFlowTable = pPhsdeviceExtension->pstServiceFlowPhsRulesTable;
	for(i=0;i<MAX_SERVICEFLOWS;i++)
	{
		S_SERVICEFLOW_ENTRY sServiceFlow = pstServiceFlowTable->stSFList[i];
		sServiceFlow.pstClassifierTable = kzalloc(sizeof(S_CLASSIFIER_TABLE), GFP_KERNEL);
		if(!sServiceFlow.pstClassifierTable)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nAllocation failed");
			free_phs_serviceflow_rules(pPhsdeviceExtension->
                pstServiceFlowPhsRulesTable);
			pPhsdeviceExtension->pstServiceFlowPhsRulesTable = NULL;
			return -ENOMEM;
		}
	}

	pPhsdeviceExtension->CompressedTxBuffer = kmalloc(PHS_BUFFER_SIZE, GFP_KERNEL);

    if(pPhsdeviceExtension->CompressedTxBuffer == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nAllocation failed");
		free_phs_serviceflow_rules(pPhsdeviceExtension->pstServiceFlowPhsRulesTable);
		pPhsdeviceExtension->pstServiceFlowPhsRulesTable = NULL;
		return -ENOMEM;
	}

    pPhsdeviceExtension->UnCompressedRxBuffer = kmalloc(PHS_BUFFER_SIZE, GFP_KERNEL);
	if(pPhsdeviceExtension->UnCompressedRxBuffer == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nAllocation failed");
		kfree(pPhsdeviceExtension->CompressedTxBuffer);
		free_phs_serviceflow_rules(pPhsdeviceExtension->pstServiceFlowPhsRulesTable);
		pPhsdeviceExtension->pstServiceFlowPhsRulesTable = NULL;
		return -ENOMEM;
	}



	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\n phs_init Successfull");
	return STATUS_SUCCESS;
}


int PhsCleanup(IN PPHS_DEVICE_EXTENSION pPHSDeviceExt)
{
	if(pPHSDeviceExt->pstServiceFlowPhsRulesTable)
	{
		free_phs_serviceflow_rules(pPHSDeviceExt->pstServiceFlowPhsRulesTable);
		pPHSDeviceExt->pstServiceFlowPhsRulesTable = NULL;
	}

	kfree(pPHSDeviceExt->CompressedTxBuffer);
	pPHSDeviceExt->CompressedTxBuffer = NULL;

	kfree(pPHSDeviceExt->UnCompressedRxBuffer);
	pPHSDeviceExt->UnCompressedRxBuffer = NULL;

	return 0;
}



ULONG PhsUpdateClassifierRule(IN void* pvContext,
								IN B_UINT16  uiVcid ,
								IN B_UINT16  uiClsId   ,
								IN S_PHS_RULE *psPhsRule,
								IN B_UINT8  u8AssociatedPHSI)
{
	ULONG lStatus =0;
	UINT nSFIndex =0 ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);



	PPHS_DEVICE_EXTENSION pDeviceExtension= (PPHS_DEVICE_EXTENSION)pvContext;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"PHS With Corr2 Changes \n");

	if(pDeviceExtension == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"Invalid Device Extension\n");
		return ERR_PHS_INVALID_DEVICE_EXETENSION;
	}


	if(u8AssociatedPHSI == 0)
	{
		return ERR_PHS_INVALID_PHS_RULE;
	}

	

	nSFIndex = GetServiceFlowEntry(pDeviceExtension->pstServiceFlowPhsRulesTable,
	                uiVcid,&pstServiceFlowEntry);

    if(nSFIndex == PHS_INVALID_TABLE_INDEX)
	{
		
		lStatus = CreateSFToClassifierRuleMapping(uiVcid, uiClsId,
		      pDeviceExtension->pstServiceFlowPhsRulesTable, psPhsRule, u8AssociatedPHSI);
		return lStatus;
	}

	
  	lStatus = CreateClassiferToPHSRuleMapping(uiVcid, uiClsId,
  	          pstServiceFlowEntry, psPhsRule, u8AssociatedPHSI);

    return lStatus;
}


ULONG PhsDeletePHSRule(IN void* pvContext,IN B_UINT16 uiVcid,IN B_UINT8 u8PHSI)
{
	ULONG lStatus =0;
	UINT nSFIndex =0, nClsidIndex =0 ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
	S_CLASSIFIER_TABLE *pstClassifierRulesTable = NULL;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);


	PPHS_DEVICE_EXTENSION pDeviceExtension= (PPHS_DEVICE_EXTENSION)pvContext;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "======>\n");

	if(pDeviceExtension)
	{

		
		nSFIndex = GetServiceFlowEntry(pDeviceExtension
		      ->pstServiceFlowPhsRulesTable,uiVcid,&pstServiceFlowEntry);

       if(nSFIndex == PHS_INVALID_TABLE_INDEX)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "SFID Match Failed\n");
			return ERR_SF_MATCH_FAIL;
		}

		pstClassifierRulesTable=pstServiceFlowEntry->pstClassifierTable;
		if(pstClassifierRulesTable)
		{
			for(nClsidIndex=0;nClsidIndex<MAX_PHSRULE_PER_SF;nClsidIndex++)
			{
				if(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].bUsed && pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule)
				{
					if(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule->u8PHSI == u8PHSI)					{
						if(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule->u8RefCnt)
							pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule->u8RefCnt--;
						if(0 == pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule->u8RefCnt)
							kfree(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule);
						memset(&pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex], 0,
							sizeof(S_CLASSIFIER_ENTRY));
					}
				}
			}
		}

	}
	return lStatus;
}

ULONG PhsDeleteClassifierRule(IN void* pvContext,IN B_UINT16 uiVcid ,IN B_UINT16  uiClsId)
{
	ULONG lStatus =0;
	UINT nSFIndex =0, nClsidIndex =0 ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
	S_CLASSIFIER_ENTRY *pstClassifierEntry = NULL;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	PPHS_DEVICE_EXTENSION pDeviceExtension= (PPHS_DEVICE_EXTENSION)pvContext;

	if(pDeviceExtension)
	{
		
		nSFIndex = GetServiceFlowEntry(pDeviceExtension
		      ->pstServiceFlowPhsRulesTable, uiVcid, &pstServiceFlowEntry);
		if(nSFIndex == PHS_INVALID_TABLE_INDEX)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"SFID Match Failed\n");
			return ERR_SF_MATCH_FAIL;
		}

		nClsidIndex = GetClassifierEntry(pstServiceFlowEntry->pstClassifierTable,
                  uiClsId, eActiveClassifierRuleContext, &pstClassifierEntry);
		if((nClsidIndex != PHS_INVALID_TABLE_INDEX) && (!pstClassifierEntry->bUnclassifiedPHSRule))
		{
			if(pstClassifierEntry->pstPhsRule)
			{
				if(pstClassifierEntry->pstPhsRule->u8RefCnt)
				pstClassifierEntry->pstPhsRule->u8RefCnt--;
				if(0==pstClassifierEntry->pstPhsRule->u8RefCnt)
					kfree(pstClassifierEntry->pstPhsRule);

			}
			memset(pstClassifierEntry, 0, sizeof(S_CLASSIFIER_ENTRY));
		}

		nClsidIndex = GetClassifierEntry(pstServiceFlowEntry->pstClassifierTable,
                    uiClsId,eOldClassifierRuleContext,&pstClassifierEntry);

	   if((nClsidIndex != PHS_INVALID_TABLE_INDEX) && (!pstClassifierEntry->bUnclassifiedPHSRule))
		{
			kfree(pstClassifierEntry->pstPhsRule);
			memset(pstClassifierEntry, 0, sizeof(S_CLASSIFIER_ENTRY));
		}
	}
	return lStatus;
}

ULONG PhsDeleteSFRules(IN void* pvContext,IN B_UINT16 uiVcid)
{

	ULONG lStatus =0;
	UINT nSFIndex =0, nClsidIndex =0  ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
	S_CLASSIFIER_TABLE *pstClassifierRulesTable = NULL;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	PPHS_DEVICE_EXTENSION pDeviceExtension= (PPHS_DEVICE_EXTENSION)pvContext;
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"====> \n");

	if(pDeviceExtension)
	{
		
		nSFIndex = GetServiceFlowEntry(pDeviceExtension->pstServiceFlowPhsRulesTable,
		                  uiVcid,&pstServiceFlowEntry);
		if(nSFIndex == PHS_INVALID_TABLE_INDEX)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "SFID Match Failed\n");
			return ERR_SF_MATCH_FAIL;
		}

		pstClassifierRulesTable=pstServiceFlowEntry->pstClassifierTable;
		if(pstClassifierRulesTable)
		{
			for(nClsidIndex=0;nClsidIndex<MAX_PHSRULE_PER_SF;nClsidIndex++)
			{
				if(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule)
				{
					if(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex]
                                                        .pstPhsRule->u8RefCnt)
						pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex]
						                                    .pstPhsRule->u8RefCnt--;
					if(0==pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex]
                                                          .pstPhsRule->u8RefCnt)
						kfree(pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex].pstPhsRule);
					    pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex]
                                        .pstPhsRule = NULL;
				}
				memset(&pstClassifierRulesTable->stActivePhsRulesList[nClsidIndex], 0, sizeof(S_CLASSIFIER_ENTRY));
				if(pstClassifierRulesTable->stOldPhsRulesList[nClsidIndex].pstPhsRule)
				{
					if(pstClassifierRulesTable->stOldPhsRulesList[nClsidIndex]
                                        .pstPhsRule->u8RefCnt)
						pstClassifierRulesTable->stOldPhsRulesList[nClsidIndex]
						                  .pstPhsRule->u8RefCnt--;
					if(0 == pstClassifierRulesTable->stOldPhsRulesList[nClsidIndex]
                                        .pstPhsRule->u8RefCnt)
						kfree(pstClassifierRulesTable
						      ->stOldPhsRulesList[nClsidIndex].pstPhsRule);
					pstClassifierRulesTable->stOldPhsRulesList[nClsidIndex]
                              .pstPhsRule = NULL;
				}
				memset(&pstClassifierRulesTable->stOldPhsRulesList[nClsidIndex], 0, sizeof(S_CLASSIFIER_ENTRY));
			}
		}
		pstServiceFlowEntry->bUsed = FALSE;
		pstServiceFlowEntry->uiVcid = 0;

	}

	return lStatus;
}


ULONG PhsCompress(IN void* pvContext,
				  IN B_UINT16 uiVcid,
				  IN B_UINT16 uiClsId,
				  IN void *pvInputBuffer,
				  OUT void *pvOutputBuffer,
				  OUT UINT *pOldHeaderSize,
				  OUT UINT *pNewHeaderSize )
{
	UINT nSFIndex =0, nClsidIndex =0  ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
	S_CLASSIFIER_ENTRY *pstClassifierEntry = NULL;
	S_PHS_RULE *pstPhsRule = NULL;
	ULONG lStatus =0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);



	PPHS_DEVICE_EXTENSION pDeviceExtension= (PPHS_DEVICE_EXTENSION)pvContext;


	if(pDeviceExtension == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"Invalid Device Extension\n");
		lStatus =  STATUS_PHS_NOCOMPRESSION ;
		return lStatus;

	}

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"Suppressing header \n");


	
	nSFIndex = GetServiceFlowEntry(pDeviceExtension->pstServiceFlowPhsRulesTable,
	                  uiVcid,&pstServiceFlowEntry);
	if(nSFIndex == PHS_INVALID_TABLE_INDEX)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"SFID Match Failed\n");
		lStatus =  STATUS_PHS_NOCOMPRESSION ;
		return lStatus;
	}

	nClsidIndex = GetClassifierEntry(pstServiceFlowEntry->pstClassifierTable,
                uiClsId,eActiveClassifierRuleContext,&pstClassifierEntry);

    if(nClsidIndex == PHS_INVALID_TABLE_INDEX)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"No PHS Rule Defined For Classifier\n");
		lStatus =  STATUS_PHS_NOCOMPRESSION ;
		return lStatus;
	}


	
	pstPhsRule =  pstClassifierEntry->pstPhsRule;

	if(!ValidatePHSRuleComplete(pstPhsRule))
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"PHS Rule Defined For Classifier But Not Complete\n");
		lStatus =  STATUS_PHS_NOCOMPRESSION ;
		return lStatus;
	}

	
	lStatus = phs_compress(pstPhsRule,(PUCHAR)pvInputBuffer,
	      (PUCHAR)pvOutputBuffer, pOldHeaderSize,pNewHeaderSize);

	if(lStatus == STATUS_PHS_COMPRESSED)
	{
		pstPhsRule->PHSModifiedBytes += *pOldHeaderSize - *pNewHeaderSize - 1;
		pstPhsRule->PHSModifiedNumPackets++;
	}
	else
		pstPhsRule->PHSErrorNumPackets++;

	return lStatus;
}

ULONG PhsDeCompress(IN void* pvContext,
				  IN B_UINT16 uiVcid,
				  IN void *pvInputBuffer,
				  OUT void *pvOutputBuffer,
				  OUT UINT *pInHeaderSize,
				  OUT UINT *pOutHeaderSize )
{
	UINT nSFIndex =0, nPhsRuleIndex =0 ;
	S_SERVICEFLOW_ENTRY *pstServiceFlowEntry = NULL;
	S_PHS_RULE *pstPhsRule = NULL;
	UINT phsi;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	PPHS_DEVICE_EXTENSION pDeviceExtension=
        (PPHS_DEVICE_EXTENSION)pvContext;

	*pInHeaderSize = 0;

	if(pDeviceExtension == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECEIVE,DBG_LVL_ALL,"Invalid Device Extension\n");
		return ERR_PHS_INVALID_DEVICE_EXETENSION;
	}

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECEIVE,DBG_LVL_ALL,"Restoring header\n");

	phsi = *((unsigned char *)(pvInputBuffer));
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECEIVE,DBG_LVL_ALL,"PHSI To Be Used For restore : %x\n",phsi);
    if(phsi == UNCOMPRESSED_PACKET )
	{
		return STATUS_PHS_NOCOMPRESSION;
	}

	
	nSFIndex = GetServiceFlowEntry(pDeviceExtension->pstServiceFlowPhsRulesTable,
	      uiVcid,&pstServiceFlowEntry);
	if(nSFIndex == PHS_INVALID_TABLE_INDEX)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECEIVE,DBG_LVL_ALL,"SFID Match Failed During Lookup\n");
		return ERR_SF_MATCH_FAIL;
	}

	nPhsRuleIndex = GetPhsRuleEntry(pstServiceFlowEntry->pstClassifierTable,phsi,
          eActiveClassifierRuleContext,&pstPhsRule);
	if(nPhsRuleIndex == PHS_INVALID_TABLE_INDEX)
	{
		
		nPhsRuleIndex = GetPhsRuleEntry(pstServiceFlowEntry->pstClassifierTable,
		      phsi,eOldClassifierRuleContext,&pstPhsRule);
		if(nPhsRuleIndex == PHS_INVALID_TABLE_INDEX)
		{
			return ERR_PHSRULE_MATCH_FAIL;
		}

	}

	*pInHeaderSize = phs_decompress((PUCHAR)pvInputBuffer,
            (PUCHAR)pvOutputBuffer,pstPhsRule,pOutHeaderSize);

	pstPhsRule->PHSModifiedBytes += *pOutHeaderSize - *pInHeaderSize - 1;

	pstPhsRule->PHSModifiedNumPackets++;
	return STATUS_PHS_COMPRESSED;
}



static void free_phs_serviceflow_rules(S_SERVICEFLOW_TABLE *psServiceFlowRulesTable)
{
	int i,j;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "=======>\n");
    if(psServiceFlowRulesTable)
	{
		for(i=0;i<MAX_SERVICEFLOWS;i++)
		{
			S_SERVICEFLOW_ENTRY stServiceFlowEntry =
                psServiceFlowRulesTable->stSFList[i];
			S_CLASSIFIER_TABLE *pstClassifierRulesTable =
                stServiceFlowEntry.pstClassifierTable;

			if(pstClassifierRulesTable)
			{
				for(j=0;j<MAX_PHSRULE_PER_SF;j++)
				{
					if(pstClassifierRulesTable->stActivePhsRulesList[j].pstPhsRule)
					{
						if(pstClassifierRulesTable->stActivePhsRulesList[j].pstPhsRule
                                                                                        ->u8RefCnt)
							pstClassifierRulesTable->stActivePhsRulesList[j].pstPhsRule
  							                                                ->u8RefCnt--;
						if(0==pstClassifierRulesTable->stActivePhsRulesList[j].pstPhsRule
                                                                ->u8RefCnt)
							kfree(pstClassifierRulesTable->stActivePhsRulesList[j].pstPhsRule);
						pstClassifierRulesTable->stActivePhsRulesList[j].pstPhsRule = NULL;
					}
					if(pstClassifierRulesTable->stOldPhsRulesList[j].pstPhsRule)
					{
						if(pstClassifierRulesTable->stOldPhsRulesList[j].pstPhsRule
                                                                ->u8RefCnt)
							pstClassifierRulesTable->stOldPhsRulesList[j].pstPhsRule
							                                          ->u8RefCnt--;
						if(0==pstClassifierRulesTable->stOldPhsRulesList[j].pstPhsRule
                                                                      ->u8RefCnt)
							kfree(pstClassifierRulesTable->stOldPhsRulesList[j].pstPhsRule);
						pstClassifierRulesTable->stOldPhsRulesList[j].pstPhsRule = NULL;
					}
				}
				kfree(pstClassifierRulesTable);
			    stServiceFlowEntry.pstClassifierTable = pstClassifierRulesTable = NULL;
			}
		}
	}

    kfree(psServiceFlowRulesTable);
    psServiceFlowRulesTable = NULL;
}



static BOOLEAN ValidatePHSRuleComplete(IN S_PHS_RULE *psPhsRule)
{
	if(psPhsRule)
	{
		if(!psPhsRule->u8PHSI)
		{
			
			return FALSE;
		}

		if(!psPhsRule->u8PHSS)
		{
			
			return FALSE;
		}

		
		if(!psPhsRule->u8PHSFLength) 
		{
			return FALSE;
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

UINT GetServiceFlowEntry(IN S_SERVICEFLOW_TABLE *psServiceFlowTable,
    IN B_UINT16 uiVcid,S_SERVICEFLOW_ENTRY **ppstServiceFlowEntry)
{
	int  i;
	for(i=0;i<MAX_SERVICEFLOWS;i++)
	{
		if(psServiceFlowTable->stSFList[i].bUsed)
		{
			if(psServiceFlowTable->stSFList[i].uiVcid == uiVcid)
			{
				*ppstServiceFlowEntry = &psServiceFlowTable->stSFList[i];
				return i;
			}
		}
	}

	*ppstServiceFlowEntry = NULL;
	return PHS_INVALID_TABLE_INDEX;
}


UINT GetClassifierEntry(IN S_CLASSIFIER_TABLE *pstClassifierTable,
        IN B_UINT32 uiClsid,E_CLASSIFIER_ENTRY_CONTEXT eClsContext,
        OUT S_CLASSIFIER_ENTRY **ppstClassifierEntry)
{
	int  i;
	S_CLASSIFIER_ENTRY *psClassifierRules = NULL;
	for(i=0;i<MAX_PHSRULE_PER_SF;i++)
	{

		if(eClsContext == eActiveClassifierRuleContext)
		{
			psClassifierRules = &pstClassifierTable->stActivePhsRulesList[i];
		}
		else
		{
			psClassifierRules = &pstClassifierTable->stOldPhsRulesList[i];
		}

		if(psClassifierRules->bUsed)
		{
			if(psClassifierRules->uiClassifierRuleId == uiClsid)
			{
				*ppstClassifierEntry = psClassifierRules;
				return i;
			}
		}

	}

	*ppstClassifierEntry = NULL;
	return PHS_INVALID_TABLE_INDEX;
}

static UINT GetPhsRuleEntry(IN S_CLASSIFIER_TABLE *pstClassifierTable,
			    IN B_UINT32 uiPHSI,E_CLASSIFIER_ENTRY_CONTEXT eClsContext,
			    OUT S_PHS_RULE **ppstPhsRule)
{
	int  i;
	S_CLASSIFIER_ENTRY *pstClassifierRule = NULL;
	for(i=0;i<MAX_PHSRULE_PER_SF;i++)
	{
		if(eClsContext == eActiveClassifierRuleContext)
		{
			pstClassifierRule = &pstClassifierTable->stActivePhsRulesList[i];
		}
		else
		{
			pstClassifierRule = &pstClassifierTable->stOldPhsRulesList[i];
		}
		if(pstClassifierRule->bUsed)
		{
			if(pstClassifierRule->u8PHSI == uiPHSI)
			{
				*ppstPhsRule = pstClassifierRule->pstPhsRule;
				return i;
			}
		}

	}

	*ppstPhsRule = NULL;
	return PHS_INVALID_TABLE_INDEX;
}

UINT CreateSFToClassifierRuleMapping(IN B_UINT16 uiVcid,IN B_UINT16  uiClsId,
                      IN S_SERVICEFLOW_TABLE *psServiceFlowTable,S_PHS_RULE *psPhsRule,
                      B_UINT8 u8AssociatedPHSI)
{

    S_CLASSIFIER_TABLE *psaClassifiertable = NULL;
	UINT uiStatus = 0;
	int iSfIndex;
	BOOLEAN bFreeEntryFound =FALSE;
	
	for(iSfIndex=0;iSfIndex < MAX_SERVICEFLOWS;iSfIndex++)
	{
		if(!psServiceFlowTable->stSFList[iSfIndex].bUsed)
		{
			bFreeEntryFound = TRUE;
			break;
		}
	}

	if(!bFreeEntryFound)
		return ERR_SFTABLE_FULL;


	psaClassifiertable = psServiceFlowTable->stSFList[iSfIndex].pstClassifierTable;
	uiStatus = CreateClassifierPHSRule(uiClsId,psaClassifiertable,psPhsRule,
	                      eActiveClassifierRuleContext,u8AssociatedPHSI);
	if(uiStatus == PHS_SUCCESS)
	{
		
		psServiceFlowTable->stSFList[iSfIndex].bUsed = TRUE;
		psServiceFlowTable->stSFList[iSfIndex].uiVcid = uiVcid;
	}

	return uiStatus;

}

UINT CreateClassiferToPHSRuleMapping(IN B_UINT16 uiVcid,
            IN B_UINT16  uiClsId,IN S_SERVICEFLOW_ENTRY *pstServiceFlowEntry,
              S_PHS_RULE *psPhsRule,B_UINT8 u8AssociatedPHSI)
{
	S_CLASSIFIER_ENTRY *pstClassifierEntry = NULL;
	UINT uiStatus =PHS_SUCCESS;
	UINT nClassifierIndex = 0;
	S_CLASSIFIER_TABLE *psaClassifiertable = NULL;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    psaClassifiertable = pstServiceFlowEntry->pstClassifierTable;

	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "==>");

	
	nClassifierIndex =GetClassifierEntry(
	            pstServiceFlowEntry->pstClassifierTable,uiClsId,
	            eActiveClassifierRuleContext,&pstClassifierEntry);
	if(nClassifierIndex == PHS_INVALID_TABLE_INDEX)
	{

		uiStatus = CreateClassifierPHSRule(uiClsId,psaClassifiertable,
		    psPhsRule,eActiveClassifierRuleContext,u8AssociatedPHSI);
		return uiStatus;
	}

	if(pstClassifierEntry->u8PHSI == psPhsRule->u8PHSI)
	{
		if(pstClassifierEntry->pstPhsRule == NULL)
			return ERR_PHS_INVALID_PHS_RULE;

		 
		if(psPhsRule->u8PHSFLength)
		{
			
			memcpy(pstClassifierEntry->pstPhsRule->u8PHSF,
			    psPhsRule->u8PHSF , MAX_PHS_LENGTHS);
		}
		if(psPhsRule->u8PHSFLength)
		{
			
			pstClassifierEntry->pstPhsRule->u8PHSFLength =
			    psPhsRule->u8PHSFLength;
		}
		if(psPhsRule->u8PHSMLength)
		{
			
			memcpy(pstClassifierEntry->pstPhsRule->u8PHSM,
			    psPhsRule->u8PHSM, MAX_PHS_LENGTHS);
		}
		if(psPhsRule->u8PHSMLength)
		{
			
			pstClassifierEntry->pstPhsRule->u8PHSMLength =
			    psPhsRule->u8PHSMLength;
		}
		if(psPhsRule->u8PHSS)
		{
			
			pstClassifierEntry->pstPhsRule->u8PHSS = psPhsRule->u8PHSS;
		}

		
		pstClassifierEntry->pstPhsRule->u8PHSV = psPhsRule->u8PHSV;

	}
	else
	{
		uiStatus=UpdateClassifierPHSRule( uiClsId,  pstClassifierEntry,
		      psaClassifiertable,  psPhsRule, u8AssociatedPHSI);
	}



	return uiStatus;
}

static UINT CreateClassifierPHSRule(IN B_UINT16  uiClsId,
    S_CLASSIFIER_TABLE *psaClassifiertable ,S_PHS_RULE *psPhsRule,
    E_CLASSIFIER_ENTRY_CONTEXT eClsContext,B_UINT8 u8AssociatedPHSI)
{
	UINT iClassifierIndex = 0;
	BOOLEAN bFreeEntryFound = FALSE;
	S_CLASSIFIER_ENTRY *psClassifierRules = NULL;
	UINT nStatus = PHS_SUCCESS;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL,"Inside CreateClassifierPHSRule");
    if(psaClassifiertable == NULL)
	{
		return ERR_INVALID_CLASSIFIERTABLE_FOR_SF;
	}

	if(eClsContext == eOldClassifierRuleContext)
	{

		iClassifierIndex =
		GetClassifierEntry(psaClassifiertable, uiClsId,
		            eClsContext,&psClassifierRules);
		if(iClassifierIndex != PHS_INVALID_TABLE_INDEX)
		{
			bFreeEntryFound = TRUE;
		}
	}

	if(!bFreeEntryFound)
	{
		for(iClassifierIndex = 0; iClassifierIndex <
            MAX_PHSRULE_PER_SF; iClassifierIndex++)
		{
			if(eClsContext == eActiveClassifierRuleContext)
			{
				psClassifierRules =
              &psaClassifiertable->stActivePhsRulesList[iClassifierIndex];
			}
			else
			{
				psClassifierRules =
                &psaClassifiertable->stOldPhsRulesList[iClassifierIndex];
			}

			if(!psClassifierRules->bUsed)
			{
				bFreeEntryFound = TRUE;
				break;
			}
		}
	}

	if(!bFreeEntryFound)
	{
		if(eClsContext == eActiveClassifierRuleContext)
		{
			return ERR_CLSASSIFIER_TABLE_FULL;
		}
		else
		{
			
			if(psaClassifiertable->uiOldestPhsRuleIndex >=
                MAX_PHSRULE_PER_SF)
			{
				psaClassifiertable->uiOldestPhsRuleIndex =0;
			}

			iClassifierIndex = psaClassifiertable->uiOldestPhsRuleIndex;
			psClassifierRules =
              &psaClassifiertable->stOldPhsRulesList[iClassifierIndex];

          (psaClassifiertable->uiOldestPhsRuleIndex)++;
		}
	}

	if(eClsContext == eOldClassifierRuleContext)
	{
		if(psClassifierRules->pstPhsRule == NULL)
		{
			psClassifierRules->pstPhsRule = kmalloc(sizeof(S_PHS_RULE),GFP_KERNEL);

          if(NULL == psClassifierRules->pstPhsRule)
				return ERR_PHSRULE_MEMALLOC_FAIL;
		}

		psClassifierRules->bUsed = TRUE;
		psClassifierRules->uiClassifierRuleId = uiClsId;
		psClassifierRules->u8PHSI = psPhsRule->u8PHSI;
		psClassifierRules->bUnclassifiedPHSRule = psPhsRule->bUnclassifiedPHSRule;

        
		memcpy(psClassifierRules->pstPhsRule,
		    psPhsRule, sizeof(S_PHS_RULE));
	}
	else
	{
		nStatus = UpdateClassifierPHSRule(uiClsId,psClassifierRules,
            psaClassifiertable,psPhsRule,u8AssociatedPHSI);
	}
	return nStatus;
}


static UINT UpdateClassifierPHSRule(IN B_UINT16  uiClsId,
      IN S_CLASSIFIER_ENTRY *pstClassifierEntry,
      S_CLASSIFIER_TABLE *psaClassifiertable ,S_PHS_RULE *psPhsRule,
      B_UINT8 u8AssociatedPHSI)
{
	S_PHS_RULE *pstAddPhsRule = NULL;
	UINT              nPhsRuleIndex = 0;
	BOOLEAN       bPHSRuleOrphaned = FALSE;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	psPhsRule->u8RefCnt =0;

	
	bPHSRuleOrphaned = DerefPhsRule( uiClsId, psaClassifiertable,
	    pstClassifierEntry->pstPhsRule);

	
	nPhsRuleIndex =GetPhsRuleEntry(psaClassifiertable,u8AssociatedPHSI,
	    eActiveClassifierRuleContext, &pstAddPhsRule);
	if(PHS_INVALID_TABLE_INDEX == nPhsRuleIndex)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nAdding New PHSRuleEntry For Classifier");

		if(psPhsRule->u8PHSI == 0)
		{
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nError PHSI is Zero\n");
			return ERR_PHS_INVALID_PHS_RULE;
		}
		
		if(FALSE == bPHSRuleOrphaned)
		{
			pstClassifierEntry->pstPhsRule = kmalloc(sizeof(S_PHS_RULE), GFP_KERNEL);
			if(NULL == pstClassifierEntry->pstPhsRule)
			{
				return ERR_PHSRULE_MEMALLOC_FAIL;
			}
		}
		memcpy(pstClassifierEntry->pstPhsRule, psPhsRule, sizeof(S_PHS_RULE));

	}
	else
	{
		
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_DISPATCH, DBG_LVL_ALL, "\nTying Classifier to Existing PHS Rule");
		if(bPHSRuleOrphaned)
		{
			kfree(pstClassifierEntry->pstPhsRule);
			pstClassifierEntry->pstPhsRule = NULL;
		}
		pstClassifierEntry->pstPhsRule = pstAddPhsRule;

	}
	pstClassifierEntry->bUsed = TRUE;
	pstClassifierEntry->u8PHSI = pstClassifierEntry->pstPhsRule->u8PHSI;
	pstClassifierEntry->uiClassifierRuleId = uiClsId;
	pstClassifierEntry->pstPhsRule->u8RefCnt++;
	pstClassifierEntry->bUnclassifiedPHSRule = pstClassifierEntry->pstPhsRule->bUnclassifiedPHSRule;

	return PHS_SUCCESS;

}

static BOOLEAN DerefPhsRule(IN B_UINT16  uiClsId,S_CLASSIFIER_TABLE *psaClassifiertable,S_PHS_RULE *pstPhsRule)
{
	if(pstPhsRule==NULL)
		return FALSE;
	if(pstPhsRule->u8RefCnt)
		pstPhsRule->u8RefCnt--;
	if(0==pstPhsRule->u8RefCnt)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void DumpPhsRules(PPHS_DEVICE_EXTENSION pDeviceExtension)
{
	int i,j,k,l;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "\n Dumping PHS Rules : \n");
	for(i=0;i<MAX_SERVICEFLOWS;i++)
	{
		S_SERVICEFLOW_ENTRY stServFlowEntry =
				pDeviceExtension->pstServiceFlowPhsRulesTable->stSFList[i];
		if(stServFlowEntry.bUsed)
		{
			for(j=0;j<MAX_PHSRULE_PER_SF;j++)
			{
				for(l=0;l<2;l++)
				{
					S_CLASSIFIER_ENTRY stClsEntry;
					if(l==0)
					{
						stClsEntry = stServFlowEntry.pstClassifierTable->stActivePhsRulesList[j];
						if(stClsEntry.bUsed)
							BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n Active PHS Rule : \n");
					}
					else
					{
						stClsEntry = stServFlowEntry.pstClassifierTable->stOldPhsRulesList[j];
						if(stClsEntry.bUsed)
							BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n Old PHS Rule : \n");
					}
					if(stClsEntry.bUsed)
					{

						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "\n VCID  : %#X",stServFlowEntry.uiVcid);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n ClassifierID  : %#X",stClsEntry.uiClassifierRuleId);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSRuleID  : %#X",stClsEntry.u8PHSI);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n****************PHS Rule********************\n");
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSI  : %#X",stClsEntry.pstPhsRule->u8PHSI);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSFLength : %#X ",stClsEntry.pstPhsRule->u8PHSFLength);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSF : ");
						for(k=0;k<stClsEntry.pstPhsRule->u8PHSFLength;k++)
						{
							BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "%#X  ",stClsEntry.pstPhsRule->u8PHSF[k]);
						}
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSMLength  : %#X",stClsEntry.pstPhsRule->u8PHSMLength);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSM :");
						for(k=0;k<stClsEntry.pstPhsRule->u8PHSMLength;k++)
						{
							BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "%#X  ",stClsEntry.pstPhsRule->u8PHSM[k]);
						}
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSS : %#X ",stClsEntry.pstPhsRule->u8PHSS);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, (DBG_LVL_ALL|DBG_NO_FUNC_PRINT), "\n PHSV  : %#X",stClsEntry.pstPhsRule->u8PHSV);
						BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, DUMP_INFO, DBG_LVL_ALL, "\n********************************************\n");
					}
				}
			}
		}
	}
}



int phs_decompress(unsigned char *in_buf,unsigned char *out_buf,
 S_PHS_RULE   *decomp_phs_rules,UINT *header_size)
{
	int phss,size=0;
	 S_PHS_RULE   *tmp_memb;
	int bit,i=0;
	unsigned char *phsf,*phsm;
	int in_buf_len = *header_size-1;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
	in_buf++;
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECEIVE,DBG_LVL_ALL,"====>\n");
	*header_size = 0;

	if((decomp_phs_rules == NULL ))
		return 0;


	tmp_memb = decomp_phs_rules;
	
	
	phss         = tmp_memb->u8PHSS;
	phsf         = tmp_memb->u8PHSF;
	phsm         = tmp_memb->u8PHSM;

	if(phss > MAX_PHS_LENGTHS)
		phss = MAX_PHS_LENGTHS;
	
	while((phss > 0) && (size < in_buf_len))
	{
		bit =  ((*phsm << i)& SUPPRESS);

		if(bit == SUPPRESS)
		{
			*out_buf = *phsf;
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECEIVE,DBG_LVL_ALL,"\nDECOMP:In phss  %d phsf %d ouput %d",
              phss,*phsf,*out_buf);
		}
		else
		{
			*out_buf = *in_buf;
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_RECEIVE,DBG_LVL_ALL,"\nDECOMP:In phss  %d input %d ouput %d",
            phss,*in_buf,*out_buf);
			in_buf++;
			size++;
		}
		out_buf++;
		phsf++;
		phss--;
		i++;
		*header_size=*header_size + 1;

		if(i > MAX_NO_BIT)
		{
			i=0;
			phsm++;
		}
	}
	return size;
}




static int phs_compress(S_PHS_RULE  *phs_rule,unsigned char *in_buf
			,unsigned char *out_buf,UINT *header_size,UINT *new_header_size)
{
	unsigned char *old_addr = out_buf;
	int supress = 0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    if(phs_rule == NULL)
	{
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nphs_compress(): phs_rule null!");
		*out_buf = ZERO_PHSI;
		return STATUS_PHS_NOCOMPRESSION;
	}


	if(phs_rule->u8PHSS <= *new_header_size)
	{
		*header_size = phs_rule->u8PHSS;
	}
	else
	{
		*header_size = *new_header_size;
	}
	
	out_buf++;
	supress = verify_suppress_phsf(in_buf,out_buf,phs_rule->u8PHSF,
        phs_rule->u8PHSM, phs_rule->u8PHSS, phs_rule->u8PHSV,new_header_size);

	if(supress == STATUS_PHS_COMPRESSED)
	{
		*old_addr = (unsigned char)phs_rule->u8PHSI;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In phs_compress phsi %d",phs_rule->u8PHSI);
	}
	else
	{
		*old_addr = ZERO_PHSI;
		BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In phs_compress PHSV Verification failed");
	}
	return supress;
}



static int verify_suppress_phsf(unsigned char *in_buffer,unsigned char *out_buffer,
				unsigned char *phsf,unsigned char *phsm,unsigned int phss,
				unsigned int phsv,UINT* new_header_size)
{
	unsigned int size=0;
	int bit,i=0;
    PMINI_ADAPTER Adapter = GET_BCM_ADAPTER(gblpnetdev);
    BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In verify_phsf PHSM - 0x%X",*phsm);


	if(phss>(*new_header_size))
	{
		phss=*new_header_size;
	}
	while(phss > 0)
	{
		bit = ((*phsm << i)& SUPPRESS);
		if(bit == SUPPRESS)
		{

			if(*in_buffer != *phsf)
			{
				if(phsv == VERIFY)
				{
					BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In verify_phsf failed for field  %d buf  %d phsf %d",phss,*in_buffer,*phsf);
					return STATUS_PHS_NOCOMPRESSION;
				}
			}
			else
				BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In verify_phsf success for field  %d buf  %d phsf %d",phss,*in_buffer,*phsf);
		}
		else
		{
			*out_buffer = *in_buffer;
			BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In copying_header input %d  out %d",*in_buffer,*out_buffer);
			out_buffer++;
			size++;
		}
		in_buffer++;
		phsf++;
		phss--;
		i++;
		if(i > MAX_NO_BIT)
		{
			i=0;
			phsm++;
		}
	}
	BCM_DEBUG_PRINT(Adapter,DBG_TYPE_OTHERS, PHS_SEND, DBG_LVL_ALL,"\nCOMP:In verify_phsf success");
	*new_header_size = size;
	return STATUS_PHS_COMPRESSED;
}






