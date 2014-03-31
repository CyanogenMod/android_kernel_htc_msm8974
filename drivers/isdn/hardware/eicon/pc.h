
/*
 *
 Copyright (c) Eicon Networks, 2002.
 *
 This source file is supplied for the use with
 Eicon Networks range of DIVA Server Adapters.
 *
 Eicon File Revision :    2.1
 *
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.
 *
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
 implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 See the GNU General Public License for more details.
 *
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef PC_H_INCLUDED  
#define PC_H_INCLUDED
typedef struct {
	word length;          
	byte P[270];          
} PBUFFER;
struct dual
{
	byte Req;             
	byte ReqId;           
	byte Rc;              
	byte RcId;            
	byte Ind;             
	byte IndId;           
	byte IMask;           
	byte RNR;             
	byte XLock;           
	byte Int;             
	byte ReqCh;           
	byte RcCh;            
	byte IndCh;           
	byte MInd;            
	word MLength;         
	byte ReadyInt;        
	byte SWReg;           
	byte Reserved[11];    
	byte InterfaceType;   
	word Signature;       
	PBUFFER XBuffer;      
	PBUFFER RBuffer;      
};
#define SWREG_DIE_WITH_LEDON  0x01
#define SWREG_HALT_CPU        0x02 
#define ID_MASK 0xe0    
#define GL_ERR_ID 0x1f  
#define DSIG_ID  0x00   
#define NL_ID    0x20   
#define BLLC_ID  0x60   
#define TASK_ID  0x80   
#define TIMER_ID 0xa0   
#define TEL_ID   0xc0   
#define MAN_ID   0xe0   
#define ASSIGN  0x01
#define UREMOVE 0xfe 
#define REMOVE  0xff
#define ASSIGN_TIM 0x01
#define REMOVE_TIM 0xff
#define ASSIGN_TSK 0x01
#define REMOVE_TSK 0xff
#define LOAD 0xf0
#define RELOCATE 0xf1
#define START 0xf2
#define LOAD2 0xf3
#define RELOCATE2 0xf4
#define TSK_B2          0x0000
#define TSK_WAKEUP      0x2000
#define TSK_TIMER       0x4000
#define TSK_TSK         0x6000
#define TSK_PC          0xe000
#define ASSIGN_LL 1     
#define REMOVE_LL 0xff  
#define LL_UDATA 1      
#define LL_ESTABLISH 2  
#define LL_RELEASE 3    
#define LL_DATA 4       
#define LL_LOCAL 5      
#define LL_DATA_PEND 5  
#define LL_REMOTE 6     
#define LL_TEST 8       
#define LL_MDATA 9      
#define LL_BUDATA 10    
#define LL_XID 12       
#define LL_XID_R 13     
#define N_MDATA         1       
#define N_CONNECT       2       
#define N_CONNECT_ACK   3       
#define N_DISC          4       
#define N_DISC_ACK      5       
#define N_RESET         6       
#define N_RESET_ACK     7       
#define N_DATA          8       
#define N_EDATA         9       
#define N_UDATA         10      
#define N_BDATA         11      
#define N_DATA_ACK      12      
#define N_EDATA_ACK     13      
#define N_XON           15      
#define N_COMBI_IND     N_XON   
#define N_Q_BIT         0x10    
#define N_M_BIT         0x20    
#define N_D_BIT         0x40    
#define ASSIGN_SIG  1    
#define UREMOVE_SIG 0xfe 
#define REMOVE_SIG  0xff 
#define CALL_REQ 1      
#define CALL_CON 1      
#define CALL_IND 2      
#define LISTEN_REQ 2    
#define HANGUP 3        
#define SUSPEND 4       
#define RESUME 5        
#define SUSPEND_REJ 6   
#define USER_DATA 8     
#define CONGESTION 9    
#define INDICATE_REQ 10 
#define INDICATE_IND 10 
#define CALL_RES 11     
#define CALL_ALERT 12   
#define INFO_REQ 13     
#define INFO_IND 13     
#define REJECT 14       
#define RESOURCES 15    
#define HW_CTRL 16      
#define TEL_CTRL 16     
#define STATUS_REQ 17   
#define FAC_REG_REQ 18  
#define FAC_REG_ACK 19  
#define FAC_REG_REJ 20  
#define CALL_COMPLETE 21
#define SW_CTRL 22      
#define REGISTER_REQ 23 
#define REGISTER_IND 24 
#define FACILITY_REQ 25 
#define FACILITY_IND 26 
#define NCR_INFO_REQ 27 
#define GCR_MIM_REQ 28  
#define SIG_CTRL    29  
#define DSP_CTRL    30  
#define LAW_REQ      31 
#define SPID_CTRL    32 
#define NCR_FACILITY 33 
#define CALL_HOLD    34 
#define CALL_RETRIEVE 35 
#define CALL_HOLD_ACK 36 
#define CALL_RETRIEVE_ACK 37 
#define CALL_HOLD_REJ 38 
#define CALL_RETRIEVE_REJ 39 
#define GCR_RESTART   40 
#define S_SERVICE     41 
#define S_SERVICE_REJ 42 
#define S_SUPPORTED   43 
#define STATUS_ENQ    44 
#define CALL_GUARD    45 
#define CALL_GUARD_HP 46 
#define CALL_GUARD_IF 47 
#define SSEXT_REQ     48 
#define SSEXT_IND     49 
#define INT_3PTY_NIND 50 
#define INT_CF_NIND   51 
#define INT_3PTY_DROP 52 
#define INT_MOVE_CONF 53 
#define INT_MOVE_RC   54 
#define INT_MOVE_FLIPPED_CONF 55 
#define INT_X5NI_OK   56 
#define INT_XDMS_START 57 
#define INT_XDMS_STOP 58 
#define INT_XDMS_STOP2 59 
#define INT_CUSTCONF_REJ 60 
#define INT_CUSTXFER 61 
#define INT_CUSTX_NIND 62 
#define INT_CUSTXREJ_NIND 63 
#define INT_X5NI_CF_XFER  64 
#define VSWITCH_REQ 65        
#define VSWITCH_IND 66        
#define MWI_POLL 67     
#define CALL_PEND_NOTIFY 68 
#define DO_NOTHING 69       
#define INT_CT_REJ 70       
#define CALL_HOLD_COMPLETE 71 
#define CALL_RETRIEVE_COMPLETE 72 
#define MAN_READ        2
#define MAN_WRITE       3
#define MAN_EXECUTE     4
#define MAN_EVENT_ON    5
#define MAN_EVENT_OFF   6
#define MAN_LOCK        7
#define MAN_UNLOCK      8
#define MAN_INFO_IND    2
#define MAN_EVENT_IND   3
#define MAN_TRACE_IND   4
#define MAN_COMBI_IND   9
#define MAN_ESC         0x80
#define UNKNOWN_COMMAND         0x01    
#define WRONG_COMMAND           0x02    
#define WRONG_ID                0x03    
#define WRONG_CH                0x04    
#define UNKNOWN_IE              0x05    
#define WRONG_IE                0x06    
#define OUT_OF_RESOURCES        0x07    
#define ISDN_GUARD_REJ          0x09    
#define N_FLOW_CONTROL          0x10    
#define ASSIGN_RC               0xe0    
#define ASSIGN_OK               0xef    
#define OK_FC                   0xfc    
#define READY_INT               0xfd    
#define TIMER_INT               0xfe    
#define OK                      0xff    
#define SHIFT 0x90              
#define MORE 0xa0               
#define SDNCMPL 0xa1            
#define CL 0xb0                 
#define SMSG 0x00               
#define BC  0x04                
#define CAU 0x08                
#define CAD 0x0c                
#define CAI 0x10                
#define CHI 0x18                
#define LLI 0x19                
#define CHA 0x1a                
#define FTY 0x1c                
#define DT  0x29                
#define KEY 0x2c                
#define UID 0x2d                
#define DSP 0x28                
#define SIG 0x34                
#define OAD 0x6c                
#define OSA 0x6d                
#define CPN 0x70                
#define DSA 0x71                
#define RDX 0x73                
#define RDN 0x74                
#define RIN 0x76                
#define IUP 0x76                
#define IPU 0x77                
#define RI  0x79                
#define MIE 0x7a                
#define LLC 0x7c                
#define HLC 0x7d                
#define UUI 0x7e                
#define ESC 0x7f                
#define DLC 0x20                
#define NLC 0x21                
#define REDIRECT_IE     0x22    
#define REDIRECT_NET_IE 0x23    
#define SIN 0x01                
#define CIF 0x02                
#define DATE 0x03               
#define CPS 0x07                
#define MSGTYPEIE        0x7a   
#define CRIE             0x7b   
#define CODESET6IE       0xec   
#define VSWITCHIE        0xed   
#define SSEXTIE          0xee   
#define PROFILEIE        0xef   
#define RING_ON         0x01
#define RING_OFF        0x02
#define HANDS_FREE_ON   0x03
#define HANDS_FREE_OFF  0x04
#define ON_HOOK         0x80
#define OFF_HOOK        0x90
#define THREE_PTY_BEGIN           0x04
#define THREE_PTY_END             0x05
#define ECT_EXECUTE               0x06
#define ACTIVATION_DIVERSION      0x07
#define DEACTIVATION_DIVERSION    0x08
#define CALL_DEFLECTION           0x0D
#define INTERROGATION_DIVERSION   0x0B
#define INTERROGATION_SERV_USR_NR 0x11
#define ACTIVATION_MWI            0x20
#define DEACTIVATION_MWI          0x21
#define MWI_INDICATION            0x22
#define MWI_RESPONSE              0x23
#define CONF_BEGIN                0x28
#define CONF_ADD                  0x29
#define CONF_SPLIT                0x2a
#define CONF_DROP                 0x2b
#define CONF_ISOLATE              0x2c
#define CONF_REATTACH             0x2d
#define CONF_PARTYDISC            0x2e
#define CCBS_INFO_RETAIN          0x2f
#define CCBS_ERASECALLLINKAGEID   0x30
#define CCBS_STOP_ALERTING        0x31
#define CCBS_REQUEST              0x32
#define CCBS_DEACTIVATE           0x33
#define CCBS_INTERROGATE          0x34
#define CCBS_STATUS               0x35
#define CCBS_ERASE                0x36
#define CCBS_B_FREE               0x37
#define CCNR_INFO_RETAIN          0x38
#define CCBS_REMOTE_USER_FREE     0x39
#define CCNR_REQUEST              0x3a
#define CCNR_INTERROGATE          0x3b
#define GET_SUPPORTED_SERVICES    0xff
#define DIVERSION_PROCEDURE_CFU     0x70
#define DIVERSION_PROCEDURE_CFB     0x71
#define DIVERSION_PROCEDURE_CFNR    0x72
#define DIVERSION_DEACTIVATION_CFU  0x80
#define DIVERSION_DEACTIVATION_CFB  0x81
#define DIVERSION_DEACTIVATION_CFNR 0x82
#define DIVERSION_INTERROGATE_NUM   0x11
#define DIVERSION_INTERROGATE_CFU   0x60
#define DIVERSION_INTERROGATE_CFB   0x61
#define DIVERSION_INTERROGATE_CFNR  0x62
#define SMASK_HOLD_RETRIEVE        0x00000001
#define SMASK_TERMINAL_PORTABILITY 0x00000002
#define SMASK_ECT                  0x00000004
#define SMASK_3PTY                 0x00000008
#define SMASK_CALL_FORWARDING      0x00000010
#define SMASK_CALL_DEFLECTION      0x00000020
#define SMASK_MCID                 0x00000040
#define SMASK_CCBS                 0x00000080
#define SMASK_MWI                  0x00000100
#define SMASK_CCNR                 0x00000200
#define SMASK_CONF                 0x00000400
#define DIVA_RC_TYPE_NONE              0x00000000
#define DIVA_RC_TYPE_REMOVE_COMPLETE   0x00000008
#define DIVA_RC_TYPE_STREAM_PTR        0x00000009
#define DIVA_RC_TYPE_CMA_PTR           0x0000000a
#define DIVA_RC_TYPE_OK_FC             0x0000000b
#define DIVA_RC_TYPE_RX_DMA            0x0000000c
#define CTRL_L1_SET_SIG_ID        5
#define CTRL_L1_SET_DAD           6
#define CTRL_L1_RESOURCES         7
#define X75T            1       
#define TRF             2       
#define TRF_IN          3       
#define SDLC            4       
#define X75             5       
#define LAPD            6       
#define X25_L2          7       
#define V120_L2         8       
#define V42_IN          9       
#define V42            10       
#define MDM_ATP        11       
#define X75_V42BIS     12       
#define RTPL2_IN       13       
#define RTPL2          14       
#define V120_V42BIS    15       
#define LISTENER       27       
#define MTP2           28       
#define PIAFS_CRC      29       
#define PIAFS_64K            0x01
#define PIAFS_VARIABLE_SPEED 0x02
#define PIAFS_CHINESE_SPEED    0x04
#define PIAFS_UDATA_ABILITY_ID    0x80
#define PIAFS_UDATA_ABILITY_DCDON 0x01
#define PIAFS_UDATA_ABILITY_DDI   0x80
#define LISTENER_FEATURE_MASK_CUMMULATIVE            0x0001
#define META_CODE_LL_UDATA_RX 0x01
#define META_CODE_LL_UDATA_TX 0x02
#define META_CODE_LL_DATA_RX  0x03
#define META_CODE_LL_DATA_TX  0x04
#define META_CODE_LL_MDATA_RX 0x05
#define META_CODE_LL_MDATA_TX 0x06
#define META_CODE_EMPTY       0x10
#define META_CODE_LOST_FRAMES 0x11
#define META_FLAG_TRUNCATED   0x0001
#define GL_INTERNAL_CONTROLLER_SUPPORTED     0x00000001L
#define GL_EXTERNAL_EQUIPMENT_SUPPORTED      0x00000002L
#define GL_HANDSET_SUPPORTED                 0x00000004L
#define GL_DTMF_SUPPORTED                    0x00000008L
#define GL_SUPPLEMENTARY_SERVICES_SUPPORTED  0x00000010L
#define GL_CHANNEL_ALLOCATION_SUPPORTED      0x00000020L
#define GL_BCHANNEL_OPERATION_SUPPORTED      0x00000040L
#define GL_LINE_INTERCONNECT_SUPPORTED       0x00000080L
#define B1_HDLC_SUPPORTED                    0x00000001L
#define B1_TRANSPARENT_SUPPORTED             0x00000002L
#define B1_V110_ASYNC_SUPPORTED              0x00000004L
#define B1_V110_SYNC_SUPPORTED               0x00000008L
#define B1_T30_SUPPORTED                     0x00000010L
#define B1_HDLC_INVERTED_SUPPORTED           0x00000020L
#define B1_TRANSPARENT_R_SUPPORTED           0x00000040L
#define B1_MODEM_ALL_NEGOTIATE_SUPPORTED     0x00000080L
#define B1_MODEM_ASYNC_SUPPORTED             0x00000100L
#define B1_MODEM_SYNC_HDLC_SUPPORTED         0x00000200L
#define B2_X75_SUPPORTED                     0x00000001L
#define B2_TRANSPARENT_SUPPORTED             0x00000002L
#define B2_SDLC_SUPPORTED                    0x00000004L
#define B2_LAPD_SUPPORTED                    0x00000008L
#define B2_T30_SUPPORTED                     0x00000010L
#define B2_PPP_SUPPORTED                     0x00000020L
#define B2_TRANSPARENT_NO_CRC_SUPPORTED      0x00000040L
#define B2_MODEM_EC_COMPRESSION_SUPPORTED    0x00000080L
#define B2_X75_V42BIS_SUPPORTED              0x00000100L
#define B2_V120_ASYNC_SUPPORTED              0x00000200L
#define B2_V120_ASYNC_V42BIS_SUPPORTED       0x00000400L
#define B2_V120_BIT_TRANSPARENT_SUPPORTED    0x00000800L
#define B2_LAPD_FREE_SAPI_SEL_SUPPORTED      0x00001000L
#define B3_TRANSPARENT_SUPPORTED             0x00000001L
#define B3_T90NL_SUPPORTED                   0x00000002L
#define B3_ISO8208_SUPPORTED                 0x00000004L
#define B3_X25_DCE_SUPPORTED                 0x00000008L
#define B3_T30_SUPPORTED                     0x00000010L
#define B3_T30_WITH_EXTENSIONS_SUPPORTED     0x00000020L
#define B3_RESERVED_SUPPORTED                0x00000040L
#define B3_MODEM_SUPPORTED                   0x00000080L
#define MANUFACTURER_FEATURE_SLAVE_CODEC          0x00000001L
#define MANUFACTURER_FEATURE_FAX_MORE_DOCUMENTS   0x00000002L
#define MANUFACTURER_FEATURE_HARDDTMF             0x00000004L
#define MANUFACTURER_FEATURE_SOFTDTMF_SEND        0x00000008L
#define MANUFACTURER_FEATURE_DTMF_PARAMETERS      0x00000010L
#define MANUFACTURER_FEATURE_SOFTDTMF_RECEIVE     0x00000020L
#define MANUFACTURER_FEATURE_FAX_SUB_SEP_PWD      0x00000040L
#define MANUFACTURER_FEATURE_V18                  0x00000080L
#define MANUFACTURER_FEATURE_MIXER_CH_CH          0x00000100L
#define MANUFACTURER_FEATURE_MIXER_CH_PC          0x00000200L
#define MANUFACTURER_FEATURE_MIXER_PC_CH          0x00000400L
#define MANUFACTURER_FEATURE_MIXER_PC_PC          0x00000800L
#define MANUFACTURER_FEATURE_ECHO_CANCELLER       0x00001000L
#define MANUFACTURER_FEATURE_RTP                  0x00002000L
#define MANUFACTURER_FEATURE_T38                  0x00004000L
#define MANUFACTURER_FEATURE_TRANSP_DELIVERY_CONF 0x00008000L
#define MANUFACTURER_FEATURE_XONOFF_FLOW_CONTROL  0x00010000L
#define MANUFACTURER_FEATURE_OOB_CHANNEL          0x00020000L
#define MANUFACTURER_FEATURE_IN_BAND_CHANNEL      0x00040000L
#define MANUFACTURER_FEATURE_IN_BAND_FEATURE      0x00080000L
#define MANUFACTURER_FEATURE_PIAFS                0x00100000L
#define MANUFACTURER_FEATURE_DTMF_TONE            0x00200000L
#define MANUFACTURER_FEATURE_FAX_PAPER_FORMATS    0x00400000L
#define MANUFACTURER_FEATURE_OK_FC_LABEL          0x00800000L
#define MANUFACTURER_FEATURE_VOWN                 0x01000000L
#define MANUFACTURER_FEATURE_XCONNECT             0x02000000L
#define MANUFACTURER_FEATURE_DMACONNECT           0x04000000L
#define MANUFACTURER_FEATURE_AUDIO_TAP            0x08000000L
#define MANUFACTURER_FEATURE_FAX_NONSTANDARD      0x10000000L
#define MANUFACTURER_FEATURE_SS7                  0x20000000L
#define MANUFACTURER_FEATURE_MADAPTER             0x40000000L
#define MANUFACTURER_FEATURE_MEASURE              0x80000000L
#define MANUFACTURER_FEATURE2_LISTENING           0x00000001L
#define MANUFACTURER_FEATURE2_SS_DIFFCONTPOSSIBLE 0x00000002L
#define MANUFACTURER_FEATURE2_GENERIC_TONE        0x00000004L
#define MANUFACTURER_FEATURE2_COLOR_FAX           0x00000008L
#define MANUFACTURER_FEATURE2_SS_ECT_DIFFCONTPOSSIBLE 0x00000010L
#define RTP_PRIM_PAYLOAD_PCMU_8000     0
#define RTP_PRIM_PAYLOAD_1016_8000     1
#define RTP_PRIM_PAYLOAD_G726_32_8000  2
#define RTP_PRIM_PAYLOAD_GSM_8000      3
#define RTP_PRIM_PAYLOAD_G723_8000     4
#define RTP_PRIM_PAYLOAD_DVI4_8000     5
#define RTP_PRIM_PAYLOAD_DVI4_16000    6
#define RTP_PRIM_PAYLOAD_LPC_8000      7
#define RTP_PRIM_PAYLOAD_PCMA_8000     8
#define RTP_PRIM_PAYLOAD_G722_16000    9
#define RTP_PRIM_PAYLOAD_QCELP_8000    12
#define RTP_PRIM_PAYLOAD_G728_8000     14
#define RTP_PRIM_PAYLOAD_G729_8000     18
#define RTP_PRIM_PAYLOAD_GSM_HR_8000   30
#define RTP_PRIM_PAYLOAD_GSM_EFR_8000  31
#define RTP_ADD_PAYLOAD_BASE           32
#define RTP_ADD_PAYLOAD_RED            32
#define RTP_ADD_PAYLOAD_CN_8000        33
#define RTP_ADD_PAYLOAD_DTMF           34
#define RTP_PRIM_PAYLOAD_PCMU_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_PCMU_8000)
#define RTP_PRIM_PAYLOAD_1016_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_1016_8000)
#define RTP_PRIM_PAYLOAD_G726_32_8000_SUPPORTED  (1L << RTP_PRIM_PAYLOAD_G726_32_8000)
#define RTP_PRIM_PAYLOAD_GSM_8000_SUPPORTED      (1L << RTP_PRIM_PAYLOAD_GSM_8000)
#define RTP_PRIM_PAYLOAD_G723_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_G723_8000)
#define RTP_PRIM_PAYLOAD_DVI4_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_DVI4_8000)
#define RTP_PRIM_PAYLOAD_DVI4_16000_SUPPORTED    (1L << RTP_PRIM_PAYLOAD_DVI4_16000)
#define RTP_PRIM_PAYLOAD_LPC_8000_SUPPORTED      (1L << RTP_PRIM_PAYLOAD_LPC_8000)
#define RTP_PRIM_PAYLOAD_PCMA_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_PCMA_8000)
#define RTP_PRIM_PAYLOAD_G722_16000_SUPPORTED    (1L << RTP_PRIM_PAYLOAD_G722_16000)
#define RTP_PRIM_PAYLOAD_QCELP_8000_SUPPORTED    (1L << RTP_PRIM_PAYLOAD_QCELP_8000)
#define RTP_PRIM_PAYLOAD_G728_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_G728_8000)
#define RTP_PRIM_PAYLOAD_G729_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_G729_8000)
#define RTP_PRIM_PAYLOAD_GSM_HR_8000_SUPPORTED   (1L << RTP_PRIM_PAYLOAD_GSM_HR_8000)
#define RTP_PRIM_PAYLOAD_GSM_EFR_8000_SUPPORTED  (1L << RTP_PRIM_PAYLOAD_GSM_EFR_8000)
#define RTP_ADD_PAYLOAD_RED_SUPPORTED            (1L << (RTP_ADD_PAYLOAD_RED - RTP_ADD_PAYLOAD_BASE))
#define RTP_ADD_PAYLOAD_CN_8000_SUPPORTED        (1L << (RTP_ADD_PAYLOAD_CN_8000 - RTP_ADD_PAYLOAD_BASE))
#define RTP_ADD_PAYLOAD_DTMF_SUPPORTED           (1L << (RTP_ADD_PAYLOAD_DTMF - RTP_ADD_PAYLOAD_BASE))
#define VSJOIN         1
#define VSTRANSPORT    2
#define VSGETPARAMS    3
#define VSCAD          1
#define VSRXCPNAME     2
#define VSCALLSTAT     3
#define VSINVOKEID    4
#define VSCLMRKS       5
#define VSTBCTIDENT    6
#define VSETSILINKID   7
#define VSSAMECONTROLLER 8
#define VSETSILINKIDRRWC      1
#define VSETSILINKIDREJECT    2
#define VSETSILINKIDTIMEOUT   3
#define VSETSILINKIDFAILCOUNT 4
#define VSETSILINKIDERROR     5
#define PROTCAP_TELINDUS  0x0001  
#define PROTCAP_MAN_IF    0x0002  
#define PROTCAP_V_42      0x0004  
#define PROTCAP_V90D      0x0008  
#define PROTCAP_EXTD_FAX  0x0010  
#define PROTCAP_EXTD_RXFC 0x0020  
#define PROTCAP_VOIP      0x0040  
#define PROTCAP_CMA_ALLPR 0x0080  
#define PROTCAP_FREE8     0x0100  
#define PROTCAP_FREE9     0x0200  
#define PROTCAP_FREE10    0x0400  
#define PROTCAP_FREE11    0x0800  
#define PROTCAP_FREE12    0x1000  
#define PROTCAP_FREE13    0x2000  
#define PROTCAP_FREE14    0x4000  
#define PROTCAP_EXTENSION 0x8000  
#define CALL_SETUP                0x80
#define MESSAGE_WAITING_INDICATOR 0x82
#define ADVICE_OF_CHARGE          0x86
#define DATE_AND_TIME                                           1
#define CLI_PARAMETER_TYPE                                      2
#define CALLED_DIRECTORY_NUMBER_PARAMETER_TYPE                  3
#define REASON_FOR_ABSENCE_OF_CLI_PARAMETER_TYPE                4
#define NAME_PARAMETER_TYPE                                     7
#define REASON_FOR_ABSENCE_OF_CALLING_PARTY_NAME_PARAMETER_TYPE 8
#define VISUAL_INDICATOR_PARAMETER_TYPE                         0xb
#define COMPLEMENTARY_CLI_PARAMETER_TYPE                        0x10
#define CALL_TYPE_PARAMETER_TYPE                                0x11
#define FIRST_CALLED_LINE_DIRECTORY_NUMBER_PARAMETER_TYPE       0x12
#define NETWORK_MESSAGE_SYSTEM_STATUS_PARAMETER_TYPE            0x13
#define FORWARDED_CALL_TYPE_PARAMETER_TYPE                      0x15
#define TYPE_OF_CALLING_USER_PARAMETER_TYPE                     0x16
#define REDIRECTING_NUMBER_PARAMETER_TYPE                       0x1a
#define EXTENSION_FOR_NETWORK_OPERATOR_USE_PARAMETER_TYPE       0xe0
#else
#endif 
