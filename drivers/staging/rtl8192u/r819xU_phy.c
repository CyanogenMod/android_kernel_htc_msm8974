#include "r8192U.h"
#include "r8192U_hw.h"
#include "r819xU_phy.h"
#include "r819xU_phyreg.h"
#include "r8190_rtl8256.h"
#include "r8192U_dm.h"
#include "r819xU_firmware_img.h"

#include "dot11d.h"
static u32 RF_CHANNEL_TABLE_ZEBRA[] = {
	0,
	0x085c, 
	0x08dc, 
	0x095c, 
	0x09dc, 
	0x0a5c, 
	0x0adc, 
	0x0b5c, 
	0x0bdc, 
	0x0c5c, 
	0x0cdc, 
	0x0d5c, 
	0x0ddc, 
	0x0e5c, 
	0x0f72, 
};


#define rtl819XPHY_REG_1T2RArray Rtl8192UsbPHY_REG_1T2RArray
#define rtl819XMACPHY_Array_PG Rtl8192UsbMACPHY_Array_PG
#define rtl819XMACPHY_Array Rtl8192UsbMACPHY_Array
#define rtl819XRadioA_Array  Rtl8192UsbRadioA_Array
#define rtl819XRadioB_Array Rtl8192UsbRadioB_Array
#define rtl819XRadioC_Array Rtl8192UsbRadioC_Array
#define rtl819XRadioD_Array Rtl8192UsbRadioD_Array
#define rtl819XAGCTAB_Array Rtl8192UsbAGCTAB_Array

u32 rtl8192_CalculateBitShift(u32 dwBitMask)
{
	u32 i;
	for (i=0; i<=31; i++)
	{
		if (((dwBitMask>>i)&0x1) == 1)
			break;
	}
	return i;
}
u8 rtl8192_phy_CheckIsLegalRFPath(struct net_device* dev, u32 eRFPath)
{
	u8 ret = 1;
	struct r8192_priv *priv = ieee80211_priv(dev);
	if (priv->rf_type == RF_2T4R)
		ret = 0;
	else if (priv->rf_type == RF_1T2R)
	{
		if (eRFPath == RF90_PATH_A || eRFPath == RF90_PATH_B)
			ret = 1;
		else if (eRFPath == RF90_PATH_C || eRFPath == RF90_PATH_D)
			ret = 0;
	}
	return ret;
}
void rtl8192_setBBreg(struct net_device* dev, u32 dwRegAddr, u32 dwBitMask, u32 dwData)
{

	u32 OriginalValue, BitShift, NewValue;

	if(dwBitMask!= bMaskDWord)
	{
		OriginalValue = read_nic_dword(dev, dwRegAddr);
		BitShift = rtl8192_CalculateBitShift(dwBitMask);
		NewValue = (((OriginalValue) & (~dwBitMask)) | (dwData << BitShift));
		write_nic_dword(dev, dwRegAddr, NewValue);
	}else
		write_nic_dword(dev, dwRegAddr, dwData);
	return;
}
u32 rtl8192_QueryBBReg(struct net_device* dev, u32 dwRegAddr, u32 dwBitMask)
{
	u32 Ret = 0, OriginalValue, BitShift;

	OriginalValue = read_nic_dword(dev, dwRegAddr);
	BitShift = rtl8192_CalculateBitShift(dwBitMask);
	Ret =(OriginalValue & dwBitMask) >> BitShift;

	return (Ret);
}
static  u32 phy_FwRFSerialRead( struct net_device* dev, RF90_RADIO_PATH_E       eRFPath, u32 Offset  );

static void phy_FwRFSerialWrite( struct net_device* dev, RF90_RADIO_PATH_E       eRFPath, u32  Offset, u32  Data);

u32 rtl8192_phy_RFSerialRead(struct net_device* dev, RF90_RADIO_PATH_E eRFPath, u32 Offset)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32 ret = 0;
	u32 NewOffset = 0;
	BB_REGISTER_DEFINITION_T* pPhyReg = &priv->PHYRegDef[eRFPath];
	rtl8192_setBBreg(dev, pPhyReg->rfLSSIReadBack, bLSSIReadBackData, 0);
	
	Offset &= 0x3f;

	
	if (priv->rf_chip == RF_8256)
	{
		if (Offset >= 31)
		{
			priv->RfReg0Value[eRFPath] |= 0x140;
			
			rtl8192_setBBreg(dev, pPhyReg->rf3wireOffset, bMaskDWord, (priv->RfReg0Value[eRFPath]<<16) );
			
			NewOffset = Offset -30;
		}
		else if (Offset >= 16)
		{
			priv->RfReg0Value[eRFPath] |= 0x100;
			priv->RfReg0Value[eRFPath] &= (~0x40);
			
			rtl8192_setBBreg(dev, pPhyReg->rf3wireOffset, bMaskDWord, (priv->RfReg0Value[eRFPath]<<16) );

			NewOffset = Offset - 15;
		}
		else
			NewOffset = Offset;
	}
	else
	{
		RT_TRACE((COMP_PHY|COMP_ERR), "check RF type here, need to be 8256\n");
		NewOffset = Offset;
	}
	
	rtl8192_setBBreg(dev, pPhyReg->rfHSSIPara2, bLSSIReadAddress, NewOffset);
	
	
	rtl8192_setBBreg(dev, pPhyReg->rfHSSIPara2,  bLSSIReadEdge, 0x0);
	rtl8192_setBBreg(dev, pPhyReg->rfHSSIPara2,  bLSSIReadEdge, 0x1);


	
	msleep(1);

	ret = rtl8192_QueryBBReg(dev, pPhyReg->rfLSSIReadBack, bLSSIReadBackData);


	
	if(priv->rf_chip == RF_8256)
	{
		priv->RfReg0Value[eRFPath] &= 0xebf;

		rtl8192_setBBreg(
			dev,
			pPhyReg->rf3wireOffset,
			bMaskDWord,
			(priv->RfReg0Value[eRFPath] << 16));
	}

	return ret;

}

/******************************************************************************
 *function:  This function write data to RF register
 *   input:  net_device dev
 *   	     RF90_RADIO_PATH_E eRFPath //radio path of A/B/C/D
 *           u32	Offset     //target address to be written
 *           u32	Data	//The new register data to be written
 *  output:  none
 *  return:  none
 *  notice:  For RF8256 only.
  ===========================================================
 *Reg Mode	RegCTL[1]	RegCTL[0]		Note
 *		(Reg00[12])	(Reg00[10])
 *===========================================================
 *Reg_Mode0	0		x			Reg 0 ~15(0x0 ~ 0xf)
 *------------------------------------------------------------------
 *Reg_Mode1	1		0			Reg 16 ~30(0x1 ~ 0xf)
 *------------------------------------------------------------------
 * Reg_Mode2	1		1			Reg 31 ~ 45(0x1 ~ 0xf)
 *------------------------------------------------------------------
 * ****************************************************************************/
void rtl8192_phy_RFSerialWrite(struct net_device* dev, RF90_RADIO_PATH_E eRFPath, u32 Offset, u32 Data)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32 DataAndAddr = 0, NewOffset = 0;
	BB_REGISTER_DEFINITION_T	*pPhyReg = &priv->PHYRegDef[eRFPath];

	Offset &= 0x3f;
	
	if (priv->rf_chip == RF_8256)
	{

		if (Offset >= 31)
		{
			priv->RfReg0Value[eRFPath] |= 0x140;
			rtl8192_setBBreg(dev, pPhyReg->rf3wireOffset, bMaskDWord, (priv->RfReg0Value[eRFPath] << 16));
			NewOffset = Offset - 30;
		}
		else if (Offset >= 16)
		{
			priv->RfReg0Value[eRFPath] |= 0x100;
			priv->RfReg0Value[eRFPath] &= (~0x40);
			rtl8192_setBBreg(dev, pPhyReg->rf3wireOffset, bMaskDWord, (priv->RfReg0Value[eRFPath]<<16));
			NewOffset = Offset - 15;
		}
		else
			NewOffset = Offset;
	}
	else
	{
		RT_TRACE((COMP_PHY|COMP_ERR), "check RF type here, need to be 8256\n");
		NewOffset = Offset;
	}

	
	DataAndAddr = (Data<<16) | (NewOffset&0x3f);

	
	rtl8192_setBBreg(dev, pPhyReg->rf3wireOffset, bMaskDWord, DataAndAddr);


	if(Offset==0x0)
		priv->RfReg0Value[eRFPath] = Data;

	
	if(priv->rf_chip == RF_8256)
	{
		if(Offset != 0)
		{
			priv->RfReg0Value[eRFPath] &= 0xebf;
			rtl8192_setBBreg(
				dev,
				pPhyReg->rf3wireOffset,
				bMaskDWord,
				(priv->RfReg0Value[eRFPath] << 16));
		}
	}
	
	return;
}

void rtl8192_phy_SetRFReg(struct net_device* dev, RF90_RADIO_PATH_E eRFPath, u32 RegAddr, u32 BitMask, u32 Data)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32 Original_Value, BitShift, New_Value;

	if (!rtl8192_phy_CheckIsLegalRFPath(dev, eRFPath))
		return;

	if (priv->Rf_Mode == RF_OP_By_FW)
	{
		if (BitMask != bMask12Bits) 
		{
			Original_Value = phy_FwRFSerialRead(dev, eRFPath, RegAddr);
			BitShift =  rtl8192_CalculateBitShift(BitMask);
			New_Value = ((Original_Value) & (~BitMask)) | (Data<< BitShift);

			phy_FwRFSerialWrite(dev, eRFPath, RegAddr, New_Value);
		}else
			phy_FwRFSerialWrite(dev, eRFPath, RegAddr, Data);

		udelay(200);

	}
	else
	{
		if (BitMask != bMask12Bits) 
		{
			Original_Value = rtl8192_phy_RFSerialRead(dev, eRFPath, RegAddr);
			BitShift =  rtl8192_CalculateBitShift(BitMask);
			New_Value = (((Original_Value) & (~BitMask)) | (Data<< BitShift));

			rtl8192_phy_RFSerialWrite(dev, eRFPath, RegAddr, New_Value);
		}else
			rtl8192_phy_RFSerialWrite(dev, eRFPath, RegAddr, Data);
	}
	return;
}

u32 rtl8192_phy_QueryRFReg(struct net_device* dev, RF90_RADIO_PATH_E eRFPath, u32 RegAddr, u32 BitMask)
{
	u32 Original_Value, Readback_Value, BitShift;
	struct r8192_priv *priv = ieee80211_priv(dev);


	if (!rtl8192_phy_CheckIsLegalRFPath(dev, eRFPath))
		return 0;
	if (priv->Rf_Mode == RF_OP_By_FW)
	{
		Original_Value = phy_FwRFSerialRead(dev, eRFPath, RegAddr);
		BitShift =  rtl8192_CalculateBitShift(BitMask);
		Readback_Value = (Original_Value & BitMask) >> BitShift;
		udelay(200);
		return (Readback_Value);
	}
	else
	{
		Original_Value = rtl8192_phy_RFSerialRead(dev, eRFPath, RegAddr);
		BitShift =  rtl8192_CalculateBitShift(BitMask);
		Readback_Value = (Original_Value & BitMask) >> BitShift;
		return (Readback_Value);
	}
}
static	u32
phy_FwRFSerialRead(
	struct net_device* dev,
	RF90_RADIO_PATH_E	eRFPath,
	u32				Offset	)
{
	u32		retValue = 0;
	u32		Data = 0;
	u8		time = 0;
	
	
	
	
	Data |= ((Offset&0xFF)<<12);
	
	Data |= ((eRFPath&0x3)<<20);
	
	
	
	Data |= 0x80000000;
	
	while (read_nic_dword(dev, QPNR)&0x80000000)
	{
		
		if (time++ < 100)
		{
			
			udelay(10);
		}
		else
			break;
	}
	
	write_nic_dword(dev, QPNR, Data);
	
	while (read_nic_dword(dev, QPNR)&0x80000000)
	{
		
		if (time++ < 100)
		{
			
			udelay(10);
		}
		else
			return	(0);
	}
	retValue = read_nic_dword(dev, RF_DATA);

	return	(retValue);

}	

static void
phy_FwRFSerialWrite(
		struct net_device* dev,
		RF90_RADIO_PATH_E	eRFPath,
		u32				Offset,
		u32				Data	)
{
	u8	time = 0;

	

	
	
	
	Data |= ((Offset&0xFF)<<12);
	
	Data |= ((eRFPath&0x3)<<20);
	
	Data |= 0x400000;
	
	Data |= 0x80000000;

	
	while (read_nic_dword(dev, QPNR)&0x80000000)
	{
		
		if (time++ < 100)
		{
			
			udelay(10);
		}
		else
			break;
	}
	
	
	write_nic_dword(dev, QPNR, Data);
	
	

}	


void rtl8192_phy_configmac(struct net_device* dev)
{
	u32 dwArrayLen = 0, i;
	u32* pdwArray = NULL;
	struct r8192_priv *priv = ieee80211_priv(dev);

	if(priv->btxpowerdata_readfromEEPORM)
	{
		RT_TRACE(COMP_PHY, "Rtl819XMACPHY_Array_PG\n");
		dwArrayLen = MACPHY_Array_PGLength;
		pdwArray = rtl819XMACPHY_Array_PG;

	}
	else
	{
		RT_TRACE(COMP_PHY, "Rtl819XMACPHY_Array\n");
		dwArrayLen = MACPHY_ArrayLength;
		pdwArray = rtl819XMACPHY_Array;
	}
	for(i = 0; i<dwArrayLen; i=i+3){
		if(pdwArray[i] == 0x318)
		{
			pdwArray[i+2] = 0x00000800;
			
			
		}

		RT_TRACE(COMP_DBG, "The Rtl8190MACPHY_Array[0] is %x Rtl8190MACPHY_Array[1] is %x Rtl8190MACPHY_Array[2] is %x\n",
				pdwArray[i], pdwArray[i+1], pdwArray[i+2]);
		rtl8192_setBBreg(dev, pdwArray[i], pdwArray[i+1], pdwArray[i+2]);
	}
	return;

}


void rtl8192_phyConfigBB(struct net_device* dev, u8 ConfigType)
{
	u32 i;

#ifdef TO_DO_LIST
	u32 *rtl8192PhyRegArrayTable = NULL, *rtl8192AgcTabArrayTable = NULL;
	if(Adapter->bInHctTest)
	{
		PHY_REGArrayLen = PHY_REGArrayLengthDTM;
		AGCTAB_ArrayLen = AGCTAB_ArrayLengthDTM;
		Rtl8190PHY_REGArray_Table = Rtl819XPHY_REGArrayDTM;
		Rtl8190AGCTAB_Array_Table = Rtl819XAGCTAB_ArrayDTM;
	}
#endif
	if (ConfigType == BaseBand_Config_PHY_REG)
	{
		for (i=0; i<PHY_REG_1T2RArrayLength; i+=2)
		{
			rtl8192_setBBreg(dev, rtl819XPHY_REG_1T2RArray[i], bMaskDWord, rtl819XPHY_REG_1T2RArray[i+1]);
			RT_TRACE(COMP_DBG, "i: %x, The Rtl819xUsbPHY_REGArray[0] is %x Rtl819xUsbPHY_REGArray[1] is %x \n",i, rtl819XPHY_REG_1T2RArray[i], rtl819XPHY_REG_1T2RArray[i+1]);
		}
	}
	else if (ConfigType == BaseBand_Config_AGC_TAB)
	{
		for (i=0; i<AGCTAB_ArrayLength; i+=2)
		{
			rtl8192_setBBreg(dev, rtl819XAGCTAB_Array[i], bMaskDWord, rtl819XAGCTAB_Array[i+1]);
			RT_TRACE(COMP_DBG, "i:%x, The rtl819XAGCTAB_Array[0] is %x rtl819XAGCTAB_Array[1] is %x \n",i, rtl819XAGCTAB_Array[i], rtl819XAGCTAB_Array[i+1]);
		}
	}
	return;


}
void rtl8192_InitBBRFRegDef(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	priv->PHYRegDef[RF90_PATH_A].rfintfs = rFPGA0_XAB_RFInterfaceSW; 
	priv->PHYRegDef[RF90_PATH_B].rfintfs = rFPGA0_XAB_RFInterfaceSW; 
	priv->PHYRegDef[RF90_PATH_C].rfintfs = rFPGA0_XCD_RFInterfaceSW;
	priv->PHYRegDef[RF90_PATH_D].rfintfs = rFPGA0_XCD_RFInterfaceSW;

	
	priv->PHYRegDef[RF90_PATH_A].rfintfi = rFPGA0_XAB_RFInterfaceRB; 
	priv->PHYRegDef[RF90_PATH_B].rfintfi = rFPGA0_XAB_RFInterfaceRB;
	priv->PHYRegDef[RF90_PATH_C].rfintfi = rFPGA0_XCD_RFInterfaceRB;
	priv->PHYRegDef[RF90_PATH_D].rfintfi = rFPGA0_XCD_RFInterfaceRB;

	
	priv->PHYRegDef[RF90_PATH_A].rfintfo = rFPGA0_XA_RFInterfaceOE; 
	priv->PHYRegDef[RF90_PATH_B].rfintfo = rFPGA0_XB_RFInterfaceOE; 
	priv->PHYRegDef[RF90_PATH_C].rfintfo = rFPGA0_XC_RFInterfaceOE;
	priv->PHYRegDef[RF90_PATH_D].rfintfo = rFPGA0_XD_RFInterfaceOE;

	
	priv->PHYRegDef[RF90_PATH_A].rfintfe = rFPGA0_XA_RFInterfaceOE; 
	priv->PHYRegDef[RF90_PATH_B].rfintfe = rFPGA0_XB_RFInterfaceOE; 
	priv->PHYRegDef[RF90_PATH_C].rfintfe = rFPGA0_XC_RFInterfaceOE;
	priv->PHYRegDef[RF90_PATH_D].rfintfe = rFPGA0_XD_RFInterfaceOE;

	
	priv->PHYRegDef[RF90_PATH_A].rf3wireOffset = rFPGA0_XA_LSSIParameter; 
	priv->PHYRegDef[RF90_PATH_B].rf3wireOffset = rFPGA0_XB_LSSIParameter;
	priv->PHYRegDef[RF90_PATH_C].rf3wireOffset = rFPGA0_XC_LSSIParameter;
	priv->PHYRegDef[RF90_PATH_D].rf3wireOffset = rFPGA0_XD_LSSIParameter;

	
	priv->PHYRegDef[RF90_PATH_A].rfLSSI_Select = rFPGA0_XAB_RFParameter;  
	priv->PHYRegDef[RF90_PATH_B].rfLSSI_Select = rFPGA0_XAB_RFParameter;
	priv->PHYRegDef[RF90_PATH_C].rfLSSI_Select = rFPGA0_XCD_RFParameter;
	priv->PHYRegDef[RF90_PATH_D].rfLSSI_Select = rFPGA0_XCD_RFParameter;

	
	priv->PHYRegDef[RF90_PATH_A].rfTxGainStage = rFPGA0_TxGainStage; 
	priv->PHYRegDef[RF90_PATH_B].rfTxGainStage = rFPGA0_TxGainStage; 
	priv->PHYRegDef[RF90_PATH_C].rfTxGainStage = rFPGA0_TxGainStage; 
	priv->PHYRegDef[RF90_PATH_D].rfTxGainStage = rFPGA0_TxGainStage; 

	
	priv->PHYRegDef[RF90_PATH_A].rfHSSIPara1 = rFPGA0_XA_HSSIParameter1;  
	priv->PHYRegDef[RF90_PATH_B].rfHSSIPara1 = rFPGA0_XB_HSSIParameter1;  
	priv->PHYRegDef[RF90_PATH_C].rfHSSIPara1 = rFPGA0_XC_HSSIParameter1;  
	priv->PHYRegDef[RF90_PATH_D].rfHSSIPara1 = rFPGA0_XD_HSSIParameter1;  

	
	priv->PHYRegDef[RF90_PATH_A].rfHSSIPara2 = rFPGA0_XA_HSSIParameter2;  
	priv->PHYRegDef[RF90_PATH_B].rfHSSIPara2 = rFPGA0_XB_HSSIParameter2;  
	priv->PHYRegDef[RF90_PATH_C].rfHSSIPara2 = rFPGA0_XC_HSSIParameter2;  
	priv->PHYRegDef[RF90_PATH_D].rfHSSIPara2 = rFPGA0_XD_HSSIParameter2;  

	
	priv->PHYRegDef[RF90_PATH_A].rfSwitchControl = rFPGA0_XAB_SwitchControl; 
	priv->PHYRegDef[RF90_PATH_B].rfSwitchControl = rFPGA0_XAB_SwitchControl;
	priv->PHYRegDef[RF90_PATH_C].rfSwitchControl = rFPGA0_XCD_SwitchControl;
	priv->PHYRegDef[RF90_PATH_D].rfSwitchControl = rFPGA0_XCD_SwitchControl;

	
	priv->PHYRegDef[RF90_PATH_A].rfAGCControl1 = rOFDM0_XAAGCCore1;
	priv->PHYRegDef[RF90_PATH_B].rfAGCControl1 = rOFDM0_XBAGCCore1;
	priv->PHYRegDef[RF90_PATH_C].rfAGCControl1 = rOFDM0_XCAGCCore1;
	priv->PHYRegDef[RF90_PATH_D].rfAGCControl1 = rOFDM0_XDAGCCore1;

	
	priv->PHYRegDef[RF90_PATH_A].rfAGCControl2 = rOFDM0_XAAGCCore2;
	priv->PHYRegDef[RF90_PATH_B].rfAGCControl2 = rOFDM0_XBAGCCore2;
	priv->PHYRegDef[RF90_PATH_C].rfAGCControl2 = rOFDM0_XCAGCCore2;
	priv->PHYRegDef[RF90_PATH_D].rfAGCControl2 = rOFDM0_XDAGCCore2;

	
	priv->PHYRegDef[RF90_PATH_A].rfRxIQImbalance = rOFDM0_XARxIQImbalance;
	priv->PHYRegDef[RF90_PATH_B].rfRxIQImbalance = rOFDM0_XBRxIQImbalance;
	priv->PHYRegDef[RF90_PATH_C].rfRxIQImbalance = rOFDM0_XCRxIQImbalance;
	priv->PHYRegDef[RF90_PATH_D].rfRxIQImbalance = rOFDM0_XDRxIQImbalance;

	
	priv->PHYRegDef[RF90_PATH_A].rfRxAFE = rOFDM0_XARxAFE;
	priv->PHYRegDef[RF90_PATH_B].rfRxAFE = rOFDM0_XBRxAFE;
	priv->PHYRegDef[RF90_PATH_C].rfRxAFE = rOFDM0_XCRxAFE;
	priv->PHYRegDef[RF90_PATH_D].rfRxAFE = rOFDM0_XDRxAFE;

	
	priv->PHYRegDef[RF90_PATH_A].rfTxIQImbalance = rOFDM0_XATxIQImbalance;
	priv->PHYRegDef[RF90_PATH_B].rfTxIQImbalance = rOFDM0_XBTxIQImbalance;
	priv->PHYRegDef[RF90_PATH_C].rfTxIQImbalance = rOFDM0_XCTxIQImbalance;
	priv->PHYRegDef[RF90_PATH_D].rfTxIQImbalance = rOFDM0_XDTxIQImbalance;

	
	priv->PHYRegDef[RF90_PATH_A].rfTxAFE = rOFDM0_XATxAFE;
	priv->PHYRegDef[RF90_PATH_B].rfTxAFE = rOFDM0_XBTxAFE;
	priv->PHYRegDef[RF90_PATH_C].rfTxAFE = rOFDM0_XCTxAFE;
	priv->PHYRegDef[RF90_PATH_D].rfTxAFE = rOFDM0_XDTxAFE;

	
	priv->PHYRegDef[RF90_PATH_A].rfLSSIReadBack = rFPGA0_XA_LSSIReadBack;
	priv->PHYRegDef[RF90_PATH_B].rfLSSIReadBack = rFPGA0_XB_LSSIReadBack;
	priv->PHYRegDef[RF90_PATH_C].rfLSSIReadBack = rFPGA0_XC_LSSIReadBack;
	priv->PHYRegDef[RF90_PATH_D].rfLSSIReadBack = rFPGA0_XD_LSSIReadBack;

}
u8 rtl8192_phy_checkBBAndRF(struct net_device* dev, HW90_BLOCK_E CheckBlock, RF90_RADIO_PATH_E eRFPath)
{
	u8 ret = 0;
	u32 i, CheckTimes = 4, dwRegRead = 0;
	u32 WriteAddr[4];
	u32 WriteData[] = {0xfffff027, 0xaa55a02f, 0x00000027, 0x55aa502f};
	
	WriteAddr[HW90_BLOCK_MAC] = 0x100;
	WriteAddr[HW90_BLOCK_PHY0] = 0x900;
	WriteAddr[HW90_BLOCK_PHY1] = 0x800;
	WriteAddr[HW90_BLOCK_RF] = 0x3;
	RT_TRACE(COMP_PHY, "=======>%s(), CheckBlock:%d\n", __FUNCTION__, CheckBlock);
	for(i=0 ; i < CheckTimes ; i++)
	{

		
		
		
		switch(CheckBlock)
		{
		case HW90_BLOCK_MAC:
			RT_TRACE(COMP_ERR, "PHY_CheckBBRFOK(): Never Write 0x100 here!");
			break;

		case HW90_BLOCK_PHY0:
		case HW90_BLOCK_PHY1:
			write_nic_dword(dev, WriteAddr[CheckBlock], WriteData[i]);
			dwRegRead = read_nic_dword(dev, WriteAddr[CheckBlock]);
			break;

		case HW90_BLOCK_RF:
			WriteData[i] &= 0xfff;
			rtl8192_phy_SetRFReg(dev, eRFPath, WriteAddr[HW90_BLOCK_RF], bMask12Bits, WriteData[i]);
			
			msleep(1);
			dwRegRead = rtl8192_phy_QueryRFReg(dev, eRFPath, WriteAddr[HW90_BLOCK_RF], bMask12Bits);
			msleep(1);
			break;

		default:
			ret = 1;
			break;
		}


		
		
		
		if(dwRegRead != WriteData[i])
		{
			RT_TRACE((COMP_PHY|COMP_ERR), "====>error=====dwRegRead: %x, WriteData: %x \n", dwRegRead, WriteData[i]);
			ret = 1;
			break;
		}
	}

	return ret;
}


void rtl8192_BB_Config_ParaFile(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8 bRegValue = 0, eCheckItem = 0, rtStatus = 0;
	u32 dwRegValue = 0;

	
	bRegValue = read_nic_byte(dev, BB_GLOBAL_RESET);
	write_nic_byte(dev, BB_GLOBAL_RESET,(bRegValue|BB_GLOBAL_RESET_BIT));
	mdelay(50);
	
	dwRegValue = read_nic_dword(dev, CPU_GEN);
	write_nic_dword(dev, CPU_GEN, (dwRegValue&(~CPU_GEN_BB_RST)));

	
	
	for(eCheckItem=(HW90_BLOCK_E)HW90_BLOCK_PHY0; eCheckItem<=HW90_BLOCK_PHY1; eCheckItem++)
	{
		rtStatus  = rtl8192_phy_checkBBAndRF(dev, (HW90_BLOCK_E)eCheckItem, (RF90_RADIO_PATH_E)0); 
		if(rtStatus != 0)
		{
			RT_TRACE((COMP_ERR | COMP_PHY), "PHY_RF8256_Config():Check PHY%d Fail!!\n", eCheckItem-1);
			return ;
		}
	}
	
	rtl8192_setBBreg(dev, rFPGA0_RFMOD, bCCKEn|bOFDMEn, 0x0);
	
	
	rtl8192_phyConfigBB(dev, BaseBand_Config_PHY_REG);

	
	dwRegValue = read_nic_dword(dev, CPU_GEN);
	write_nic_dword(dev, CPU_GEN, (dwRegValue|CPU_GEN_BB_RST));

	
	
	rtl8192_phyConfigBB(dev, BaseBand_Config_AGC_TAB);

	
	write_nic_byte_E(dev, 0x5e, 0x00);
	if (priv->card_8192_version == (u8)VERSION_819xU_A)
	{
		
		dwRegValue = (priv->AntennaTxPwDiff[1]<<4 | priv->AntennaTxPwDiff[0]);
		rtl8192_setBBreg(dev, rFPGA0_TxGainStage, (bXBTxAGC|bXCTxAGC), dwRegValue);

		
		dwRegValue = priv->CrystalCap & 0xf;
		rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, bXtalCap, dwRegValue);
	}

	
	
	priv->bCckHighPower = (u8)(rtl8192_QueryBBReg(dev, rFPGA0_XA_HSSIParameter2, 0x200));
	return;
}
void rtl8192_BBConfig(struct net_device* dev)
{
	rtl8192_InitBBRFRegDef(dev);
	
	
	rtl8192_BB_Config_ParaFile(dev);
	return;
}

void rtl8192_phy_getTxPower(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	priv->MCSTxPowerLevelOriginalOffset[0] =
		read_nic_dword(dev, rTxAGC_Rate18_06);
	priv->MCSTxPowerLevelOriginalOffset[1] =
		read_nic_dword(dev, rTxAGC_Rate54_24);
	priv->MCSTxPowerLevelOriginalOffset[2] =
		read_nic_dword(dev, rTxAGC_Mcs03_Mcs00);
	priv->MCSTxPowerLevelOriginalOffset[3] =
		read_nic_dword(dev, rTxAGC_Mcs07_Mcs04);
	priv->MCSTxPowerLevelOriginalOffset[4] =
		read_nic_dword(dev, rTxAGC_Mcs11_Mcs08);
	priv->MCSTxPowerLevelOriginalOffset[5] =
		read_nic_dword(dev, rTxAGC_Mcs15_Mcs12);

	
	priv->DefaultInitialGain[0] = read_nic_byte(dev, rOFDM0_XAAGCCore1);
	priv->DefaultInitialGain[1] = read_nic_byte(dev, rOFDM0_XBAGCCore1);
	priv->DefaultInitialGain[2] = read_nic_byte(dev, rOFDM0_XCAGCCore1);
	priv->DefaultInitialGain[3] = read_nic_byte(dev, rOFDM0_XDAGCCore1);
	RT_TRACE(COMP_INIT, "Default initial gain (c50=0x%x, c58=0x%x, c60=0x%x, c68=0x%x) \n",
		priv->DefaultInitialGain[0], priv->DefaultInitialGain[1],
		priv->DefaultInitialGain[2], priv->DefaultInitialGain[3]);

	
	priv->framesync = read_nic_byte(dev, rOFDM0_RxDetector3);
	priv->framesyncC34 = read_nic_byte(dev, rOFDM0_RxDetector2);
	RT_TRACE(COMP_INIT, "Default framesync (0x%x) = 0x%x \n",
		rOFDM0_RxDetector3, priv->framesync);

	
	priv->SifsTime = read_nic_word(dev, SIFS);

	return;
}

void rtl8192_phy_setTxPower(struct net_device* dev, u8 channel)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8	powerlevel = priv->TxPowerLevelCCK[channel-1];
	u8	powerlevelOFDM24G = priv->TxPowerLevelOFDM24G[channel-1];

	switch(priv->rf_chip)
	{
	case RF_8256:
		PHY_SetRF8256CCKTxPower(dev, powerlevel); 
		PHY_SetRF8256OFDMTxPower(dev, powerlevelOFDM24G);
		break;
	default:
		RT_TRACE((COMP_PHY|COMP_ERR), "error RF chipID(8225 or 8258) in function %s()\n", __FUNCTION__);
		break;
	}
	return;
}

void rtl8192_phy_RFConfig(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	switch(priv->rf_chip)
	{
		case RF_8256:
			PHY_RF8256_Config(dev);
			break;
	
	
		default:
			RT_TRACE(COMP_ERR, "error chip id\n");
			break;
	}
	return;
}

void rtl8192_phy_updateInitGain(struct net_device* dev)
{
	return;
}

u8 rtl8192_phy_ConfigRFWithHeaderFile(struct net_device* dev, RF90_RADIO_PATH_E	eRFPath)
{

	int i;
	
	u8 ret = 0;

	switch(eRFPath){
		case RF90_PATH_A:
			for(i = 0;i<RadioA_ArrayLength; i=i+2){

				if(rtl819XRadioA_Array[i] == 0xfe){
						mdelay(100);
						continue;
				}
				rtl8192_phy_SetRFReg(dev, eRFPath, rtl819XRadioA_Array[i], bMask12Bits, rtl819XRadioA_Array[i+1]);
				mdelay(1);

			}
			break;
		case RF90_PATH_B:
			for(i = 0;i<RadioB_ArrayLength; i=i+2){

				if(rtl819XRadioB_Array[i] == 0xfe){
						mdelay(100);
						continue;
				}
				rtl8192_phy_SetRFReg(dev, eRFPath, rtl819XRadioB_Array[i], bMask12Bits, rtl819XRadioB_Array[i+1]);
				mdelay(1);

			}
			break;
		case RF90_PATH_C:
			for(i = 0;i<RadioC_ArrayLength; i=i+2){

				if(rtl819XRadioC_Array[i] == 0xfe){
						mdelay(100);
						continue;
				}
				rtl8192_phy_SetRFReg(dev, eRFPath, rtl819XRadioC_Array[i], bMask12Bits, rtl819XRadioC_Array[i+1]);
				mdelay(1);

			}
			break;
		case RF90_PATH_D:
			for(i = 0;i<RadioD_ArrayLength; i=i+2){

				if(rtl819XRadioD_Array[i] == 0xfe){
						mdelay(100);
						continue;
				}
				rtl8192_phy_SetRFReg(dev, eRFPath, rtl819XRadioD_Array[i], bMask12Bits, rtl819XRadioD_Array[i+1]);
				mdelay(1);

			}
			break;
		default:
			break;
	}

	return ret;

}
void rtl8192_SetTxPowerLevel(struct net_device *dev, u8 channel)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8	powerlevel = priv->TxPowerLevelCCK[channel-1];
	u8	powerlevelOFDM24G = priv->TxPowerLevelOFDM24G[channel-1];

	switch(priv->rf_chip)
	{
	case RF_8225:
#ifdef TO_DO_LIST
		PHY_SetRF8225CckTxPower(Adapter, powerlevel);
		PHY_SetRF8225OfdmTxPower(Adapter, powerlevelOFDM24G);
#endif
		break;

	case RF_8256:
		PHY_SetRF8256CCKTxPower(dev, powerlevel);
		PHY_SetRF8256OFDMTxPower(dev, powerlevelOFDM24G);
		break;

	case RF_8258:
		break;
	default:
		RT_TRACE(COMP_ERR, "unknown rf chip ID in rtl8192_SetTxPowerLevel()\n");
		break;
	}
	return;
}

bool rtl8192_SetRFPowerState(struct net_device *dev, RT_RF_POWER_STATE eRFPowerState)
{
	bool				bResult = true;
	struct r8192_priv *priv = ieee80211_priv(dev);

	if(eRFPowerState == priv->ieee80211->eRFPowerState)
		return false;

	if(priv->SetRFPowerStateInProgress == true)
		return false;

	priv->SetRFPowerStateInProgress = true;

	switch(priv->rf_chip)
	{
		case RF_8256:
		switch( eRFPowerState )
		{
			case eRfOn:
	
					
					rtl8192_setBBreg(dev, rFPGA0_XA_RFInterfaceOE, BIT4, 0x1);	
					
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter4, 0x300, 0x3);
					
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x18, 0x3); 
					
					rtl8192_setBBreg(dev, rOFDM0_TRxPathEnable, 0x3, 0x3);
					
					rtl8192_setBBreg(dev, rOFDM1_TRxPathEnable, 0x3, 0x3);
					
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x60, 0x3); 

				break;

			case eRfSleep:

				break;

			case eRfOff:
					
					
					rtl8192_setBBreg(dev, rFPGA0_XA_RFInterfaceOE, BIT4, 0x0);	
					
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter4, 0xf00, 0x0);
					
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x18, 0x0); 
					
					rtl8192_setBBreg(dev, rOFDM0_TRxPathEnable, 0xf, 0x0);
					
					rtl8192_setBBreg(dev, rOFDM1_TRxPathEnable, 0xf, 0x0);
					
					rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x60, 0x0); 

				break;

			default:
				bResult = false;
				RT_TRACE(COMP_ERR, "SetRFPowerState819xUsb(): unknow state to set: 0x%X!!!\n", eRFPowerState);
				break;
		}
			break;
		default:
			RT_TRACE(COMP_ERR, "Not support rf_chip(%x)\n", priv->rf_chip);
			break;
	}
#ifdef TO_DO_LIST
	if(bResult)
	{
		
		pHalData->eRFPowerState = eRFPowerState;
		switch(pHalData->RFChipID )
		{
			case RF_8256:
		switch(pHalData->eRFPowerState)
				{
				case eRfOff:
					
					
					
					if(pMgntInfo->RfOffReason==RF_CHANGE_BY_IPS )
					{
						Adapter->HalFunc.LedControlHandler(Adapter,LED_CTL_NO_LINK);
					}
					else
					{
						
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_POWER_OFF);
					}
					break;

				case eRfOn:
					
					
					if( pMgntInfo->bMediaConnect == TRUE )
					{
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_LINK);
					}
					else
					{
						
						Adapter->HalFunc.LedControlHandler(Adapter, LED_CTL_NO_LINK);
					}
					break;

				default:
					
					break;
				}
				break;

				default:
					RT_TRACE(COMP_RF, DBG_LOUD, ("SetRFPowerState8190(): Unknown RF type\n"));
					break;
			}

	}
#endif
	priv->SetRFPowerStateInProgress = false;

	return bResult;
}

u8 rtl8192_phy_SetSwChnlCmdArray(
	SwChnlCmd*		CmdTable,
	u32			CmdTableIdx,
	u32			CmdTableSz,
	SwChnlCmdID		CmdID,
	u32			Para1,
	u32			Para2,
	u32			msDelay
	)
{
	SwChnlCmd* pCmd;

	if(CmdTable == NULL)
	{
		RT_TRACE(COMP_ERR, "phy_SetSwChnlCmdArray(): CmdTable cannot be NULL.\n");
		return false;
	}
	if(CmdTableIdx >= CmdTableSz)
	{
		RT_TRACE(COMP_ERR, "phy_SetSwChnlCmdArray(): Access invalid index, please check size of the table, CmdTableIdx:%d, CmdTableSz:%d\n",
				CmdTableIdx, CmdTableSz);
		return false;
	}

	pCmd = CmdTable + CmdTableIdx;
	pCmd->CmdID = CmdID;
	pCmd->Para1 = Para1;
	pCmd->Para2 = Para2;
	pCmd->msDelay = msDelay;

	return true;
}
u8 rtl8192_phy_SwChnlStepByStep(struct net_device *dev, u8 channel, u8* stage, u8* step, u32* delay)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	SwChnlCmd				PreCommonCmd[MAX_PRECMD_CNT];
	u32					PreCommonCmdCnt;
	SwChnlCmd				PostCommonCmd[MAX_POSTCMD_CNT];
	u32					PostCommonCmdCnt;
	SwChnlCmd				RfDependCmd[MAX_RFDEPENDCMD_CNT];
	u32					RfDependCmdCnt;
	SwChnlCmd				*CurrentCmd = NULL;
	
	u8		eRFPath;

	RT_TRACE(COMP_CH, "====>%s()====stage:%d, step:%d, channel:%d\n", __FUNCTION__, *stage, *step, channel);
	if (!IsLegalChannel(priv->ieee80211, channel))
	{
		RT_TRACE(COMP_ERR, "=============>set to illegal channel:%d\n", channel);
		return true; 
	}


	
		
		PreCommonCmdCnt = 0;
		rtl8192_phy_SetSwChnlCmdArray(PreCommonCmd, PreCommonCmdCnt++, MAX_PRECMD_CNT,
					CmdID_SetTxPowerLevel, 0, 0, 0);
		rtl8192_phy_SetSwChnlCmdArray(PreCommonCmd, PreCommonCmdCnt++, MAX_PRECMD_CNT,
					CmdID_End, 0, 0, 0);

		
		PostCommonCmdCnt = 0;

		rtl8192_phy_SetSwChnlCmdArray(PostCommonCmd, PostCommonCmdCnt++, MAX_POSTCMD_CNT,
					CmdID_End, 0, 0, 0);

		
		RfDependCmdCnt = 0;
		switch( priv->rf_chip )
		{
		case RF_8225:
			if (!(channel >= 1 && channel <= 14))
			{
				RT_TRACE(COMP_ERR, "illegal channel for Zebra 8225: %d\n", channel);
				return true;
			}
			rtl8192_phy_SetSwChnlCmdArray(RfDependCmd, RfDependCmdCnt++, MAX_RFDEPENDCMD_CNT,
				CmdID_RF_WriteReg, rZebra1_Channel, RF_CHANNEL_TABLE_ZEBRA[channel], 10);
			rtl8192_phy_SetSwChnlCmdArray(RfDependCmd, RfDependCmdCnt++, MAX_RFDEPENDCMD_CNT,
				CmdID_End, 0, 0, 0);
			break;

		case RF_8256:
			
			if (!(channel >= 1 && channel <= 14))
			{
				RT_TRACE(COMP_ERR, "illegal channel for Zebra 8256: %d\n", channel);
				return true;
			}
			rtl8192_phy_SetSwChnlCmdArray(RfDependCmd, RfDependCmdCnt++, MAX_RFDEPENDCMD_CNT,
				CmdID_RF_WriteReg, rZebra1_Channel, channel, 10);
			rtl8192_phy_SetSwChnlCmdArray(RfDependCmd, RfDependCmdCnt++, MAX_RFDEPENDCMD_CNT,
			CmdID_End, 0, 0, 0);
			break;

		case RF_8258:
			break;

		default:
			RT_TRACE(COMP_ERR, "Unknown RFChipID: %d\n", priv->rf_chip);
			return true;
			break;
		}


		do{
			switch(*stage)
			{
			case 0:
				CurrentCmd=&PreCommonCmd[*step];
				break;
			case 1:
				CurrentCmd=&RfDependCmd[*step];
				break;
			case 2:
				CurrentCmd=&PostCommonCmd[*step];
				break;
			}

			if(CurrentCmd->CmdID==CmdID_End)
			{
				if((*stage)==2)
				{
					(*delay)=CurrentCmd->msDelay;
					return true;
				}
				else
				{
					(*stage)++;
					(*step)=0;
					continue;
				}
			}

			switch(CurrentCmd->CmdID)
			{
			case CmdID_SetTxPowerLevel:
				if(priv->card_8192_version == (u8)VERSION_819xU_A) 
					rtl8192_SetTxPowerLevel(dev,channel);
				break;
			case CmdID_WritePortUlong:
				write_nic_dword(dev, CurrentCmd->Para1, CurrentCmd->Para2);
				break;
			case CmdID_WritePortUshort:
				write_nic_word(dev, CurrentCmd->Para1, (u16)CurrentCmd->Para2);
				break;
			case CmdID_WritePortUchar:
				write_nic_byte(dev, CurrentCmd->Para1, (u8)CurrentCmd->Para2);
				break;
			case CmdID_RF_WriteReg:
				for(eRFPath = 0; eRFPath < RF90_PATH_MAX; eRFPath++)
				{
				rtl8192_phy_SetRFReg(dev, (RF90_RADIO_PATH_E)eRFPath, CurrentCmd->Para1, bZebra1_ChannelNum, CurrentCmd->Para2);
				}
				break;
			default:
				break;
			}

			break;
		}while(true);

	(*delay)=CurrentCmd->msDelay;
	(*step)++;
	return false;
}

void rtl8192_phy_FinishSwChnlNow(struct net_device *dev, u8 channel)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32	delay = 0;

	while(!rtl8192_phy_SwChnlStepByStep(dev,channel,&priv->SwChnlStage,&priv->SwChnlStep,&delay))
	{
	
	
		if(!priv->up)
			break;
	}
}
void rtl8192_SwChnl_WorkItem(struct net_device *dev)
{

	struct r8192_priv *priv = ieee80211_priv(dev);

	RT_TRACE(COMP_CH, "==> SwChnlCallback819xUsbWorkItem(), chan:%d\n", priv->chan);


	rtl8192_phy_FinishSwChnlNow(dev , priv->chan);

	RT_TRACE(COMP_CH, "<== SwChnlCallback819xUsbWorkItem()\n");
}

u8 rtl8192_phy_SwChnl(struct net_device* dev, u8 channel)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	RT_TRACE(COMP_CH, "=====>%s(), SwChnlInProgress:%d\n", __FUNCTION__, priv->SwChnlInProgress);
	if(!priv->up)
		return false;
	if(priv->SwChnlInProgress)
		return false;

if (0) 
{
	u8		eRFPath;
	for(eRFPath = 0; eRFPath < 2; eRFPath++){
	printk("====>set channel:%x\n",rtl8192_phy_QueryRFReg(dev, (RF90_RADIO_PATH_E)eRFPath, 0x7, bZebra1_ChannelNum));
	udelay(10);
	}
}
	
	switch(priv->ieee80211->mode)
	{
	case WIRELESS_MODE_A:
	case WIRELESS_MODE_N_5G:
		if (channel<=14){
			RT_TRACE(COMP_ERR, "WIRELESS_MODE_A but channel<=14");
			return false;
		}
		break;
	case WIRELESS_MODE_B:
		if (channel>14){
			RT_TRACE(COMP_ERR, "WIRELESS_MODE_B but channel>14");
			return false;
		}
		break;
	case WIRELESS_MODE_G:
	case WIRELESS_MODE_N_24G:
		if (channel>14){
			RT_TRACE(COMP_ERR, "WIRELESS_MODE_G but channel>14");
			return false;
		}
		break;
	}
	

	priv->SwChnlInProgress = true;
	if(channel == 0)
		channel = 1;

	priv->chan=channel;

	priv->SwChnlStage=0;
	priv->SwChnlStep=0;
	if(priv->up) {
	rtl8192_SwChnl_WorkItem(dev);
	}

	priv->SwChnlInProgress = false;
	return true;
}


void rtl8192_SetBWModeWorkItem(struct net_device *dev)
{

	struct r8192_priv *priv = ieee80211_priv(dev);
	u8 regBwOpMode;

	RT_TRACE(COMP_SWBW, "==>rtl8192_SetBWModeWorkItem()  Switch to %s bandwidth\n", \
					priv->CurrentChannelBW == HT_CHANNEL_WIDTH_20?"20MHz":"40MHz")


	if(priv->rf_chip == RF_PSEUDO_11N)
	{
		priv->SetBWModeInProgress= false;
		return;
	}

	
	regBwOpMode = read_nic_byte(dev, BW_OPMODE);

	switch(priv->CurrentChannelBW)
	{
		case HT_CHANNEL_WIDTH_20:
			regBwOpMode |= BW_OPMODE_20MHZ;
		       
			write_nic_byte(dev, BW_OPMODE, regBwOpMode);
			break;

		case HT_CHANNEL_WIDTH_20_40:
			regBwOpMode &= ~BW_OPMODE_20MHZ;
			
			write_nic_byte(dev, BW_OPMODE, regBwOpMode);
			break;

		default:
			RT_TRACE(COMP_ERR, "SetChannelBandwidth819xUsb(): unknown Bandwidth: %#X\n",priv->CurrentChannelBW);
			break;
	}

	
	switch(priv->CurrentChannelBW)
	{
		case HT_CHANNEL_WIDTH_20:
			
			rtl8192_setBBreg(dev, rFPGA0_RFMOD, bRFMOD, 0x0);
			rtl8192_setBBreg(dev, rFPGA1_RFMOD, bRFMOD, 0x0);
			rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x00100000, 1);

			
			priv->cck_present_attentuation =
				priv->cck_present_attentuation_20Mdefault + priv->cck_present_attentuation_difference;

			if(priv->cck_present_attentuation > 22)
				priv->cck_present_attentuation= 22;
			if(priv->cck_present_attentuation< 0)
				priv->cck_present_attentuation = 0;
			RT_TRACE(COMP_INIT, "20M, pHalData->CCKPresentAttentuation = %d\n", priv->cck_present_attentuation);

			if(priv->chan == 14 && !priv->bcck_in_ch14)
			{
				priv->bcck_in_ch14 = TRUE;
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);
			}
			else if(priv->chan != 14 && priv->bcck_in_ch14)
			{
				priv->bcck_in_ch14 = FALSE;
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);
			}
			else
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);

			break;
		case HT_CHANNEL_WIDTH_20_40:
			
			rtl8192_setBBreg(dev, rFPGA0_RFMOD, bRFMOD, 0x1);
			rtl8192_setBBreg(dev, rFPGA1_RFMOD, bRFMOD, 0x1);
			rtl8192_setBBreg(dev, rCCK0_System, bCCKSideBand, (priv->nCur40MhzPrimeSC>>1));
			rtl8192_setBBreg(dev, rFPGA0_AnalogParameter1, 0x00100000, 0);
			rtl8192_setBBreg(dev, rOFDM1_LSTF, 0xC00, priv->nCur40MhzPrimeSC);
			priv->cck_present_attentuation =
				priv->cck_present_attentuation_40Mdefault + priv->cck_present_attentuation_difference;

			if(priv->cck_present_attentuation > 22)
				priv->cck_present_attentuation = 22;
			if(priv->cck_present_attentuation < 0)
				priv->cck_present_attentuation = 0;

			RT_TRACE(COMP_INIT, "40M, pHalData->CCKPresentAttentuation = %d\n", priv->cck_present_attentuation);
			if(priv->chan == 14 && !priv->bcck_in_ch14)
			{
				priv->bcck_in_ch14 = true;
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);
			}
			else if(priv->chan!= 14 && priv->bcck_in_ch14)
			{
				priv->bcck_in_ch14 = false;
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);
			}
			else
				dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);

			break;
		default:
			RT_TRACE(COMP_ERR, "SetChannelBandwidth819xUsb(): unknown Bandwidth: %#X\n" ,priv->CurrentChannelBW);
			break;

	}
	

	
	switch( priv->rf_chip )
	{
		case RF_8225:
#ifdef TO_DO_LIST
			PHY_SetRF8225Bandwidth(Adapter, pHalData->CurrentChannelBW);
#endif
			break;

		case RF_8256:
			PHY_SetRF8256Bandwidth(dev, priv->CurrentChannelBW);
			break;

		case RF_8258:
			
			break;

		case RF_PSEUDO_11N:
			
			break;

		default:
			RT_TRACE(COMP_ERR, "Unknown RFChipID: %d\n", priv->rf_chip);
			break;
	}
	priv->SetBWModeInProgress= false;

	RT_TRACE(COMP_SWBW, "<==SetBWMode819xUsb(), %d", atomic_read(&(priv->ieee80211->atm_swbw)) );
}

void rtl8192_SetBWMode(struct net_device *dev, HT_CHANNEL_WIDTH	Bandwidth, HT_EXTCHNL_OFFSET Offset)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	if(priv->SetBWModeInProgress)
		return;
	priv->SetBWModeInProgress= true;

	priv->CurrentChannelBW = Bandwidth;

	if(Offset==HT_EXTCHNL_OFFSET_LOWER)
		priv->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_UPPER;
	else if(Offset==HT_EXTCHNL_OFFSET_UPPER)
		priv->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_LOWER;
	else
		priv->nCur40MhzPrimeSC = HAL_PRIME_CHNL_OFFSET_DONT_CARE;

	
	
	rtl8192_SetBWModeWorkItem(dev);

}

void InitialGain819xUsb(struct net_device *dev,	u8 Operation)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	priv->InitialGainOperateType = Operation;

	if(priv->up)
	{
		queue_delayed_work(priv->priv_wq,&priv->initialgain_operate_wq,0);
	}
}

extern void InitialGainOperateWorkItemCallBack(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work,struct delayed_work,work);
       struct r8192_priv *priv = container_of(dwork,struct r8192_priv,initialgain_operate_wq);
       struct net_device *dev = priv->ieee80211->dev;
#define SCAN_RX_INITIAL_GAIN	0x17
#define POWER_DETECTION_TH	0x08
	u32	BitMask;
	u8	initial_gain;
	u8	Operation;

	Operation = priv->InitialGainOperateType;

	switch(Operation)
	{
		case IG_Backup:
			RT_TRACE(COMP_SCAN, "IG_Backup, backup the initial gain.\n");
			initial_gain = SCAN_RX_INITIAL_GAIN;
			BitMask = bMaskByte0;
			if(dm_digtable.dig_algorithm == DIG_ALGO_BY_FALSE_ALARM)
				rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x8);	
			priv->initgain_backup.xaagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XAAGCCore1, BitMask);
			priv->initgain_backup.xbagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XBAGCCore1, BitMask);
			priv->initgain_backup.xcagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XCAGCCore1, BitMask);
			priv->initgain_backup.xdagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XDAGCCore1, BitMask);
			BitMask  = bMaskByte2;
			priv->initgain_backup.cca		= (u8)rtl8192_QueryBBReg(dev, rCCK0_CCA, BitMask);

			RT_TRACE(COMP_SCAN, "Scan InitialGainBackup 0xc50 is %x\n",priv->initgain_backup.xaagccore1);
			RT_TRACE(COMP_SCAN, "Scan InitialGainBackup 0xc58 is %x\n",priv->initgain_backup.xbagccore1);
			RT_TRACE(COMP_SCAN, "Scan InitialGainBackup 0xc60 is %x\n",priv->initgain_backup.xcagccore1);
			RT_TRACE(COMP_SCAN, "Scan InitialGainBackup 0xc68 is %x\n",priv->initgain_backup.xdagccore1);
			RT_TRACE(COMP_SCAN, "Scan InitialGainBackup 0xa0a is %x\n",priv->initgain_backup.cca);

			RT_TRACE(COMP_SCAN, "Write scan initial gain = 0x%x \n", initial_gain);
			write_nic_byte(dev, rOFDM0_XAAGCCore1, initial_gain);
			write_nic_byte(dev, rOFDM0_XBAGCCore1, initial_gain);
			write_nic_byte(dev, rOFDM0_XCAGCCore1, initial_gain);
			write_nic_byte(dev, rOFDM0_XDAGCCore1, initial_gain);
			RT_TRACE(COMP_SCAN, "Write scan 0xa0a = 0x%x \n", POWER_DETECTION_TH);
			write_nic_byte(dev, 0xa0a, POWER_DETECTION_TH);
			break;
		case IG_Restore:
			RT_TRACE(COMP_SCAN, "IG_Restore, restore the initial gain.\n");
			BitMask = 0x7f; 
			if(dm_digtable.dig_algorithm == DIG_ALGO_BY_FALSE_ALARM)
				rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x8);	

			rtl8192_setBBreg(dev, rOFDM0_XAAGCCore1, BitMask, (u32)priv->initgain_backup.xaagccore1);
			rtl8192_setBBreg(dev, rOFDM0_XBAGCCore1, BitMask, (u32)priv->initgain_backup.xbagccore1);
			rtl8192_setBBreg(dev, rOFDM0_XCAGCCore1, BitMask, (u32)priv->initgain_backup.xcagccore1);
			rtl8192_setBBreg(dev, rOFDM0_XDAGCCore1, BitMask, (u32)priv->initgain_backup.xdagccore1);
			BitMask  = bMaskByte2;
			rtl8192_setBBreg(dev, rCCK0_CCA, BitMask, (u32)priv->initgain_backup.cca);

			RT_TRACE(COMP_SCAN, "Scan BBInitialGainRestore 0xc50 is %x\n",priv->initgain_backup.xaagccore1);
			RT_TRACE(COMP_SCAN, "Scan BBInitialGainRestore 0xc58 is %x\n",priv->initgain_backup.xbagccore1);
			RT_TRACE(COMP_SCAN, "Scan BBInitialGainRestore 0xc60 is %x\n",priv->initgain_backup.xcagccore1);
			RT_TRACE(COMP_SCAN, "Scan BBInitialGainRestore 0xc68 is %x\n",priv->initgain_backup.xdagccore1);
			RT_TRACE(COMP_SCAN, "Scan BBInitialGainRestore 0xa0a is %x\n",priv->initgain_backup.cca);

#ifdef RTL8190P
			SetTxPowerLevel8190(Adapter,priv->CurrentChannel);
#endif
#ifdef RTL8192E
			SetTxPowerLevel8190(Adapter,priv->CurrentChannel);
#endif
			rtl8192_phy_setTxPower(dev,priv->ieee80211->current_network.channel);

			if(dm_digtable.dig_algorithm == DIG_ALGO_BY_FALSE_ALARM)
				rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x1);	
			break;
		default:
			RT_TRACE(COMP_SCAN, "Unknown IG Operation. \n");
			break;
	}
}

