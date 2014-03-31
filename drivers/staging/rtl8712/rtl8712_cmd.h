/******************************************************************************
 *
 * Copyright(c) 2007 - 2010 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * Modifications for inclusion into the Linux staging tree are
 * Copyright(c) 2010 Larry Finger. All rights reserved.
 *
 * Contact information:
 * WLAN FAE <wlanfae@realtek.com>
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 ******************************************************************************/
#ifndef __RTL8712_CMD_H_
#define __RTL8712_CMD_H_

#define CMD_HDR_SZ      8

u8 r8712_fw_cmd(struct _adapter *pAdapter, u32 cmd);
void r8712_fw_cmd_data(struct _adapter *pAdapter, u32 *value, u8 flag);

struct cmd_hdr {
	u32 cmd_dw0;
	u32 cmd_dw1;
};

enum rtl8712_h2c_cmd {
	GEN_CMD_CODE(_Read_MACREG),	
	GEN_CMD_CODE(_Write_MACREG),
	GEN_CMD_CODE(_Read_BBREG),
	GEN_CMD_CODE(_Write_BBREG),
	GEN_CMD_CODE(_Read_RFREG),
	GEN_CMD_CODE(_Write_RFREG), 
	GEN_CMD_CODE(_Read_EEPROM),
	GEN_CMD_CODE(_Write_EEPROM),
	GEN_CMD_CODE(_Read_EFUSE),
	GEN_CMD_CODE(_Write_EFUSE),

	GEN_CMD_CODE(_Read_CAM),	
	GEN_CMD_CODE(_Write_CAM),
	GEN_CMD_CODE(_setBCNITV),
	GEN_CMD_CODE(_setMBIDCFG),
	GEN_CMD_CODE(_JoinBss),   
	GEN_CMD_CODE(_DisConnect), 
	GEN_CMD_CODE(_CreateBss),
	GEN_CMD_CODE(_SetOpMode),
	GEN_CMD_CODE(_SiteSurvey),  
	GEN_CMD_CODE(_SetAuth),

	GEN_CMD_CODE(_SetKey),	
	GEN_CMD_CODE(_SetStaKey),
	GEN_CMD_CODE(_SetAssocSta),
	GEN_CMD_CODE(_DelAssocSta),
	GEN_CMD_CODE(_SetStaPwrState),
	GEN_CMD_CODE(_SetBasicRate), 
	GEN_CMD_CODE(_GetBasicRate),
	GEN_CMD_CODE(_SetDataRate),
	GEN_CMD_CODE(_GetDataRate),
	GEN_CMD_CODE(_SetPhyInfo),

	GEN_CMD_CODE(_GetPhyInfo),	
	GEN_CMD_CODE(_SetPhy),
	GEN_CMD_CODE(_GetPhy),
	GEN_CMD_CODE(_readRssi),
	GEN_CMD_CODE(_readGain),
	GEN_CMD_CODE(_SetAtim), 
	GEN_CMD_CODE(_SetPwrMode),
	GEN_CMD_CODE(_JoinbssRpt),
	GEN_CMD_CODE(_SetRaTable),
	GEN_CMD_CODE(_GetRaTable),

	GEN_CMD_CODE(_GetCCXReport), 
	GEN_CMD_CODE(_GetDTMReport),
	GEN_CMD_CODE(_GetTXRateStatistics),
	GEN_CMD_CODE(_SetUsbSuspend),
	GEN_CMD_CODE(_SetH2cLbk),
	GEN_CMD_CODE(_AddBAReq), 

	GEN_CMD_CODE(_SetChannel), 
	GEN_CMD_CODE(_SetTxPower),
	GEN_CMD_CODE(_SwitchAntenna),
	GEN_CMD_CODE(_SetCrystalCap),
	GEN_CMD_CODE(_SetSingleCarrierTx), 
	GEN_CMD_CODE(_SetSingleToneTx),
	GEN_CMD_CODE(_SetCarrierSuppressionTx),
	GEN_CMD_CODE(_SetContinuousTx),
	GEN_CMD_CODE(_SwitchBandwidth), 
	GEN_CMD_CODE(_TX_Beacon), 
	GEN_CMD_CODE(_SetPowerTracking),
	GEN_CMD_CODE(_AMSDU_TO_AMPDU), 
	GEN_CMD_CODE(_SetMacAddress), 

	GEN_CMD_CODE(_DisconnectCtrl), 
	GEN_CMD_CODE(_SetChannelPlan), 
	GEN_CMD_CODE(_DisconnectCtrlEx), 

	
	GEN_CMD_CODE(_GetH2cLbk) ,

	
	GEN_CMD_CODE(_SetProbeReqExtraIE) ,
	GEN_CMD_CODE(_SetAssocReqExtraIE) ,
	GEN_CMD_CODE(_SetProbeRspExtraIE) ,
	GEN_CMD_CODE(_SetAssocRspExtraIE) ,

	
	GEN_CMD_CODE(_GetCurDataRate) ,

	GEN_CMD_CODE(_GetTxRetrycnt),  
	GEN_CMD_CODE(_GetRxRetrycnt),  

	GEN_CMD_CODE(_GetBCNOKcnt),
	GEN_CMD_CODE(_GetBCNERRcnt),
	GEN_CMD_CODE(_GetCurTxPwrLevel),

	GEN_CMD_CODE(_SetDIG),
	GEN_CMD_CODE(_SetRA),
	GEN_CMD_CODE(_SetPT),
	GEN_CMD_CODE(_ReadTSSI),

	MAX_H2CCMD
};


#define _GetBBReg_CMD_		_Read_BBREG_CMD_
#define _SetBBReg_CMD_		_Write_BBREG_CMD_
#define _GetRFReg_CMD_		_Read_RFREG_CMD_
#define _SetRFReg_CMD_		_Write_RFREG_CMD_
#define _DRV_INT_CMD_		(MAX_H2CCMD+1)
#define _SetRFIntFs_CMD_	(MAX_H2CCMD+2)

#ifdef _RTL8712_CMD_C_
static struct _cmd_callback	cmd_callback[] = {
	{GEN_CMD_CODE(_Read_MACREG), NULL}, 
	{GEN_CMD_CODE(_Write_MACREG), NULL},
	{GEN_CMD_CODE(_Read_BBREG), &r8712_getbbrfreg_cmdrsp_callback},
	{GEN_CMD_CODE(_Write_BBREG), NULL},
	{GEN_CMD_CODE(_Read_RFREG), &r8712_getbbrfreg_cmdrsp_callback},
	{GEN_CMD_CODE(_Write_RFREG), NULL}, 
	{GEN_CMD_CODE(_Read_EEPROM), NULL},
	{GEN_CMD_CODE(_Write_EEPROM), NULL},
	{GEN_CMD_CODE(_Read_EFUSE), NULL},
	{GEN_CMD_CODE(_Write_EFUSE), NULL},

	{GEN_CMD_CODE(_Read_CAM),	NULL},	
	{GEN_CMD_CODE(_Write_CAM),	 NULL},
	{GEN_CMD_CODE(_setBCNITV), NULL},
	{GEN_CMD_CODE(_setMBIDCFG), NULL},
	{GEN_CMD_CODE(_JoinBss), &r8712_joinbss_cmd_callback},  
	{GEN_CMD_CODE(_DisConnect), &r8712_disassoc_cmd_callback}, 
	{GEN_CMD_CODE(_CreateBss), &r8712_createbss_cmd_callback},
	{GEN_CMD_CODE(_SetOpMode), NULL},
	{GEN_CMD_CODE(_SiteSurvey), &r8712_survey_cmd_callback}, 
	{GEN_CMD_CODE(_SetAuth), NULL},

	{GEN_CMD_CODE(_SetKey), NULL},	
	{GEN_CMD_CODE(_SetStaKey), &r8712_setstaKey_cmdrsp_callback},
	{GEN_CMD_CODE(_SetAssocSta), &r8712_setassocsta_cmdrsp_callback},
	{GEN_CMD_CODE(_DelAssocSta), NULL},
	{GEN_CMD_CODE(_SetStaPwrState), NULL},
	{GEN_CMD_CODE(_SetBasicRate), NULL}, 
	{GEN_CMD_CODE(_GetBasicRate), NULL},
	{GEN_CMD_CODE(_SetDataRate), NULL},
	{GEN_CMD_CODE(_GetDataRate), NULL},
	{GEN_CMD_CODE(_SetPhyInfo), NULL},

	{GEN_CMD_CODE(_GetPhyInfo), NULL}, 
	{GEN_CMD_CODE(_SetPhy), NULL},
	{GEN_CMD_CODE(_GetPhy), NULL},
	{GEN_CMD_CODE(_readRssi), NULL},
	{GEN_CMD_CODE(_readGain), NULL},
	{GEN_CMD_CODE(_SetAtim), NULL}, 
	{GEN_CMD_CODE(_SetPwrMode), NULL},
	{GEN_CMD_CODE(_JoinbssRpt), NULL},
	{GEN_CMD_CODE(_SetRaTable), NULL},
	{GEN_CMD_CODE(_GetRaTable), NULL},

	{GEN_CMD_CODE(_GetCCXReport), NULL}, 
	{GEN_CMD_CODE(_GetDTMReport),	NULL},
	{GEN_CMD_CODE(_GetTXRateStatistics), NULL},
	{GEN_CMD_CODE(_SetUsbSuspend), NULL},
	{GEN_CMD_CODE(_SetH2cLbk), NULL},
	{GEN_CMD_CODE(_AddBAReq), NULL}, 

	{GEN_CMD_CODE(_SetChannel), NULL},		
	{GEN_CMD_CODE(_SetTxPower), NULL},
	{GEN_CMD_CODE(_SwitchAntenna), NULL},
	{GEN_CMD_CODE(_SetCrystalCap), NULL},
	{GEN_CMD_CODE(_SetSingleCarrierTx), NULL},	
	{GEN_CMD_CODE(_SetSingleToneTx), NULL},
	{GEN_CMD_CODE(_SetCarrierSuppressionTx), NULL},
	{GEN_CMD_CODE(_SetContinuousTx), NULL},
	{GEN_CMD_CODE(_SwitchBandwidth), NULL},		
	{GEN_CMD_CODE(_TX_Beacon), NULL}, 
	{GEN_CMD_CODE(_SetPowerTracking), NULL},
	{GEN_CMD_CODE(_AMSDU_TO_AMPDU), NULL}, 
	{GEN_CMD_CODE(_SetMacAddress), NULL}, 

	{GEN_CMD_CODE(_DisconnectCtrl), NULL}, 
	{GEN_CMD_CODE(_SetChannelPlan), NULL}, 
	{GEN_CMD_CODE(_DisconnectCtrlEx), NULL}, 

	
	{GEN_CMD_CODE(_GetH2cLbk), NULL},

	{_SetProbeReqExtraIE_CMD_, NULL},
	{_SetAssocReqExtraIE_CMD_, NULL},
	{_SetProbeRspExtraIE_CMD_, NULL},
	{_SetAssocRspExtraIE_CMD_, NULL},
	{_GetCurDataRate_CMD_, NULL},
	{_GetTxRetrycnt_CMD_, NULL},
	{_GetRxRetrycnt_CMD_, NULL},
	{_GetBCNOKcnt_CMD_, NULL},
	{_GetBCNERRcnt_CMD_, NULL},
	{_GetCurTxPwrLevel_CMD_, NULL},
	{_SetDIG_CMD_, NULL},
	{_SetRA_CMD_, NULL},
	{_SetPT_CMD_, NULL},
	{GEN_CMD_CODE(_ReadTSSI), &r8712_readtssi_cmdrsp_callback}
};
#endif

#endif
