/******************************************************************************
 *
 *	(C)Copyright 1998,1999 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/



typedef	u_long	Counter ;
typedef u_char	TimeStamp[8] ;
typedef struct fddi_addr LongAddr ;
typedef	u_long	Timer_2 ;
typedef	u_long	Timer ;
typedef	u_short	ResId ;
typedef u_short	SMTEnum ;
typedef	u_char	SMTFlag ;

typedef struct {
	Counter		count ;
	TimeStamp	timestamp ;
} SetCountType ;

#define MIB_PATH_P	(1<<0)
#define MIB_PATH_S	(1<<1)
#define MIB_PATH_L	(1<<2)

#define MIB_P_PATH_LOCAL	(1<<0)
#define MIB_P_PATH_SEC_ALTER	(1<<1)
#define MIB_P_PATH_PRIM_ALTER	(1<<2)
#define MIB_P_PATH_CON_ALTER	(1<<3)
#define MIB_P_PATH_SEC_PREFER	(1<<4)
#define MIB_P_PATH_PRIM_PREFER	(1<<5)
#define MIB_P_PATH_CON_PREFER	(1<<6)
#define MIB_P_PATH_THRU		(1<<7)

#define MIB_PATH_ISOLATED	0
#define MIB_PATH_LOCAL		1
#define MIB_PATH_SECONDARY	2
#define MIB_PATH_PRIMARY	3
#define MIB_PATH_CONCATENATED	4
#define MIB_PATH_THRU		5

#define MIB_PMDCLASS_MULTI	0
#define MIB_PMDCLASS_SINGLE1	1
#define MIB_PMDCLASS_SINGLE2	2
#define MIB_PMDCLASS_SONET	3
#define MIB_PMDCLASS_LCF	4
#define MIB_PMDCLASS_TP		5
#define MIB_PMDCLASS_UNKNOWN	6
#define MIB_PMDCLASS_UNSPEC	7

#define MIB_SMT_STASTA_CON	0
#define MIB_SMT_STASTA_SEPA	1
#define MIB_SMT_STASTA_THRU	2


struct fddi_mib {
	u_char			fddiPRPMFPasswd[8] ;
	struct smt_sid		fddiPRPMFStation ;

#ifdef	ESS
	u_long	fddiESSPayload ;	
	u_long	fddiESSOverhead ;	
	u_long	fddiESSMaxTNeg ;	
	u_long	fddiESSMinSegmentSize ;	
	u_long	fddiESSCategory ;	
	short	fddiESSSynchTxMode ;	
#endif	
#ifdef	SBA
	char	fddiSBACommand ;	
	u_char	fddiSBAAvailable ;	
#endif	

	struct smt_sid		fddiSMTStationId ;
	u_short			fddiSMTOpVersionId ;
	u_short			fddiSMTHiVersionId ;
	u_short			fddiSMTLoVersionId ;
	u_char			fddiSMTManufacturerData[32] ;
	u_char			fddiSMTUserData[32] ;
	u_short			fddiSMTMIBVersionId ;

	u_char			fddiSMTMac_Ct ;
	u_char			fddiSMTNonMaster_Ct ;
	u_char			fddiSMTMaster_Ct ;
	u_char			fddiSMTAvailablePaths ;
	u_short			fddiSMTConfigCapabilities ;
	u_short			fddiSMTConfigPolicy ;
	u_short			fddiSMTConnectionPolicy ;
	u_short			fddiSMTTT_Notify ;
	u_char			fddiSMTStatRptPolicy ;
	u_long			fddiSMTTrace_MaxExpiration ;
	u_short			fddiSMTPORTIndexes[NUMPHYS] ;
	u_short			fddiSMTMACIndexes ;
	u_char			fddiSMTBypassPresent ;

	SMTEnum			fddiSMTECMState ;
	SMTEnum			fddiSMTCF_State ;
	SMTEnum			fddiSMTStationStatus ;
	u_char			fddiSMTRemoteDisconnectFlag ;
	u_char			fddiSMTPeerWrapFlag ;

	TimeStamp		fddiSMTTimeStamp ;
	TimeStamp		fddiSMTTransitionTimeStamp ;
	SetCountType		fddiSMTSetCount ;
	struct smt_sid		fddiSMTLastSetStationId ;

	struct fddi_mib_m {
		u_short		fddiMACFrameStatusFunctions ;
		Timer_2		fddiMACT_MaxCapabilitiy ;
		Timer_2		fddiMACTVXCapabilitiy ;

		
		u_char		fddiMACMultiple_N ;	
		u_char		fddiMACMultiple_P ;	
		u_char		fddiMACDuplicateAddressCond ;
		u_char		fddiMACAvailablePaths ;
		u_short		fddiMACCurrentPath ;
		LongAddr	fddiMACUpstreamNbr ;
		LongAddr	fddiMACDownstreamNbr ;
		LongAddr	fddiMACOldUpstreamNbr ;
		LongAddr	fddiMACOldDownstreamNbr ;
		SMTEnum		fddiMACDupAddressTest ;
		u_short		fddiMACRequestedPaths ;
		SMTEnum		fddiMACDownstreamPORTType ;
		ResId		fddiMACIndex ;

		
		LongAddr	fddiMACSMTAddress ;

		
		Timer_2		fddiMACT_Min ;	
		Timer_2		fddiMACT_ReqMIB ;
		Timer_2		fddiMACT_Req ;	
		Timer_2		fddiMACT_Neg ;
		Timer_2		fddiMACT_MaxMIB ;
		Timer_2		fddiMACT_Max ;	
		Timer_2		fddiMACTvxValueMIB ;
		Timer_2		fddiMACTvxValue ; 
		Timer_2		fddiMACT_Pri0 ;
		Timer_2		fddiMACT_Pri1 ;
		Timer_2		fddiMACT_Pri2 ;
		Timer_2		fddiMACT_Pri3 ;
		Timer_2		fddiMACT_Pri4 ;
		Timer_2		fddiMACT_Pri5 ;
		Timer_2		fddiMACT_Pri6 ;

		
		Counter		fddiMACFrame_Ct ;
		Counter		fddiMACCopied_Ct ;
		Counter		fddiMACTransmit_Ct ;
		Counter		fddiMACToken_Ct ;
		Counter		fddiMACError_Ct ;
		Counter		fddiMACLost_Ct ;
		Counter		fddiMACTvxExpired_Ct ;
		Counter		fddiMACNotCopied_Ct ;
		Counter		fddiMACRingOp_Ct ;

		Counter		fddiMACSMTCopied_Ct ;		
		Counter		fddiMACSMTTransmit_Ct ;		

		
		Counter		fddiMACOld_Frame_Ct ;
		Counter		fddiMACOld_Copied_Ct ;
		Counter		fddiMACOld_Error_Ct ;
		Counter		fddiMACOld_Lost_Ct ;
		Counter		fddiMACOld_NotCopied_Ct ;

		
		u_short		fddiMACFrameErrorThreshold ;
		u_short		fddiMACFrameErrorRatio ;

		
		u_short		fddiMACNotCopiedThreshold ;
		u_short		fddiMACNotCopiedRatio ;

		
		SMTEnum		fddiMACRMTState ;
		SMTFlag		fddiMACDA_Flag ;
		SMTFlag		fddiMACUNDA_Flag ;
		SMTFlag		fddiMACFrameErrorFlag ;
		SMTFlag		fddiMACNotCopiedFlag ;
		SMTFlag		fddiMACMA_UnitdataAvailable ;
		SMTFlag		fddiMACHardwarePresent ;
		SMTFlag		fddiMACMA_UnitdataEnable ;

	} m[NUMMACS] ;
#define MAC0	0

	struct fddi_mib_a {
		ResId		fddiPATHIndex ;
		u_long		fddiPATHSbaPayload ;
		u_long		fddiPATHSbaOverhead ;
		
		
		Timer		fddiPATHT_Rmode ;
		u_long		fddiPATHSbaAvailable ;
		Timer_2		fddiPATHTVXLowerBound ;
		Timer_2		fddiPATHT_MaxLowerBound ;
		Timer_2		fddiPATHMaxT_Req ;
	} a[NUMPATHS] ;
#define PATH0	0

	struct fddi_mib_p {
		
		SMTEnum		fddiPORTMy_Type ;
		SMTEnum		fddiPORTNeighborType ;
		u_char		fddiPORTConnectionPolicies ;
		struct {
			u_char	T_val ;
			u_char	R_val ;
		} fddiPORTMacIndicated ;
		SMTEnum		fddiPORTCurrentPath ;
		u_char		fddiPORTRequestedPaths[4] ;
		u_short		fddiPORTMACPlacement ;
		u_char		fddiPORTAvailablePaths ;
		u_char		fddiPORTConnectionCapabilities ;
		SMTEnum		fddiPORTPMDClass ;
		ResId		fddiPORTIndex ;

		
		SMTEnum		fddiPORTMaint_LS ;
		SMTEnum		fddiPORTPC_LS ;
		u_char		fddiPORTBS_Flag ;

		
		Counter		fddiPORTLCTFail_Ct ;
		Counter		fddiPORTEBError_Ct ;
		Counter		fddiPORTOldEBError_Ct ;

		
		Counter		fddiPORTLem_Reject_Ct ;
		Counter		fddiPORTLem_Ct ;
		u_char		fddiPORTLer_Estimate ;
		u_char		fddiPORTLer_Cutoff ;
		u_char		fddiPORTLer_Alarm ;

		
		SMTEnum		fddiPORTConnectState ;
		SMTEnum		fddiPORTPCMState ;	
		SMTEnum		fddiPORTPCMStateX ;	
		SMTEnum		fddiPORTPC_Withhold ;
		SMTFlag		fddiPORTHardwarePresent ;
		u_char		fddiPORTLerFlag ;

		u_char		fddiPORTMultiple_U ;	
		u_char		fddiPORTMultiple_P ;	
		u_char		fddiPORTEB_Condition ;	
	} p[NUMPHYS] ;
	struct {
		Counter		fddiPRIVECF_Req_Rx ;	
		Counter		fddiPRIVECF_Reply_Rx ;	
		Counter		fddiPRIVECF_Req_Tx ;	
		Counter		fddiPRIVECF_Reply_Tx ;	
		Counter		fddiPRIVPMF_Get_Rx ;	
		Counter		fddiPRIVPMF_Set_Rx ;	
		Counter		fddiPRIVRDF_Rx ;	
		Counter		fddiPRIVRDF_Tx ;	
	} priv ;
} ;

#define	SMT_OID_CF_STATE	1	
#define	SMT_OID_PCM_STATE_A	2	
#define	SMT_OID_PCM_STATE_B	17	
#define	SMT_OID_RMT_STATE	3	
#define	SMT_OID_UNA		4	
#define	SMT_OID_DNA		5	
#define	SMT_OID_ERROR_CT	6	
#define	SMT_OID_LOST_CT		7	
#define	SMT_OID_LEM_CT		8	
#define	SMT_OID_LEM_CT_A	11	
#define	SMT_OID_LEM_CT_B	12	
#define	SMT_OID_LCT_FAIL_CT	9	
#define	SMT_OID_LCT_FAIL_CT_A	13	
#define	SMT_OID_LCT_FAIL_CT_B	14	
#define	SMT_OID_LEM_REJECT_CT	10	
#define	SMT_OID_LEM_REJECT_CT_A	15	
#define	SMT_OID_LEM_REJECT_CT_B	16	

#define SMT_OID_ECF_REQ_RX	20	
#define SMT_OID_ECF_REPLY_RX	21	
#define SMT_OID_ECF_REQ_TX	22	
#define SMT_OID_ECF_REPLY_TX	23	
#define SMT_OID_PMF_GET_RX	24	
#define SMT_OID_PMF_SET_RX	25	
#define SMT_OID_RDF_RX		26	
#define SMT_OID_RDF_TX		27	
