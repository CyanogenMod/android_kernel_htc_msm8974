
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
#ifndef _INC_CAPI20
#define _INC_CAPI20
#define FILE_DEVICE_CAPI20 0x8001
#define CAPI20_CTL_REGISTER             0x0801
#define CAPI20_CTL_RELEASE              0x0802
#define CAPI20_CTL_GET_MANUFACTURER     0x0805
#define CAPI20_CTL_GET_VERSION          0x0806
#define CAPI20_CTL_GET_SERIAL           0x0807
#define CAPI20_CTL_GET_PROFILE          0x0808
#define CAPI20_CTL_PUT_MESSAGE          0x0803
#define CAPI20_CTL_GET_MESSAGE          0x0804
#define CAPI_CTL_CODE(f, m)             CTL_CODE(FILE_DEVICE_CAPI20, f, m, FILE_ANY_ACCESS)
#define IOCTL_CAPI_REGISTER             CAPI_CTL_CODE(CAPI20_CTL_REGISTER, METHOD_BUFFERED)
#define IOCTL_CAPI_RELEASE              CAPI_CTL_CODE(CAPI20_CTL_RELEASE, METHOD_BUFFERED)
#define IOCTL_CAPI_GET_MANUFACTURER     CAPI_CTL_CODE(CAPI20_CTL_GET_MANUFACTURER, METHOD_BUFFERED)
#define IOCTL_CAPI_GET_VERSION          CAPI_CTL_CODE(CAPI20_CTL_GET_VERSION, METHOD_BUFFERED)
#define IOCTL_CAPI_GET_SERIAL           CAPI_CTL_CODE(CAPI20_CTL_GET_SERIAL, METHOD_BUFFERED)
#define IOCTL_CAPI_GET_PROFILE          CAPI_CTL_CODE(CAPI20_CTL_GET_PROFILE, METHOD_BUFFERED)
#define IOCTL_CAPI_PUT_MESSAGE          CAPI_CTL_CODE(CAPI20_CTL_PUT_MESSAGE, METHOD_BUFFERED)
#define IOCTL_CAPI_GET_MESSAGE          CAPI_CTL_CODE(CAPI20_CTL_GET_MESSAGE, METHOD_BUFFERED)
struct divas_capi_register_params  {
	word MessageBufferSize;
	word maxLogicalConnection;
	word maxBDataBlocks;
	word maxBDataLen;
};
struct divas_capi_version  {
	word CapiMajor;
	word CapiMinor;
	word ManuMajor;
	word ManuMinor;
};
typedef struct api_profile_s {
	word          Number;
	word          Channels;
	dword         Global_Options;
	dword         B1_Protocols;
	dword         B2_Protocols;
	dword         B3_Protocols;
} API_PROFILE;
#define _ALERT_R                        0x8001
#define _CONNECT_R                      0x8002
#define _CONNECT_I                      0x8202
#define _CONNECT_ACTIVE_I               0x8203
#define _DISCONNECT_R                   0x8004
#define _DISCONNECT_I                   0x8204
#define _LISTEN_R                       0x8005
#define _INFO_R                         0x8008
#define _INFO_I                         0x8208
#define _SELECT_B_REQ                   0x8041
#define _FACILITY_R                     0x8080
#define _FACILITY_I                     0x8280
#define _CONNECT_B3_R                   0x8082
#define _CONNECT_B3_I                   0x8282
#define _CONNECT_B3_ACTIVE_I            0x8283
#define _DISCONNECT_B3_R                0x8084
#define _DISCONNECT_B3_I                0x8284
#define _DATA_B3_R                      0x8086
#define _DATA_B3_I                      0x8286
#define _RESET_B3_R                     0x8087
#define _RESET_B3_I                     0x8287
#define _CONNECT_B3_T90_ACTIVE_I        0x8288
#define _MANUFACTURER_R                 0x80ff
#define _MANUFACTURER_I                 0x82ff
#define CONFIRM                 0x0100
#define RESPONSE                0x0100
#define _DI_MANU_ID             0x44444944
#define _DI_ASSIGN_PLCI         0x0001
#define _DI_ADV_CODEC           0x0002
#define _DI_DSP_CTRL            0x0003
#define _DI_SIG_CTRL            0x0004
#define _DI_RXT_CTRL            0x0005
#define _DI_IDI_CTRL            0x0006
#define _DI_CFG_CTRL            0x0007
#define _DI_REMOVE_CODEC        0x0008
#define _DI_OPTIONS_REQUEST     0x0009
#define _DI_SSEXT_CTRL          0x000a
#define _DI_NEGOTIATE_B3        0x000b
typedef struct {
	byte structs[0];      
} _ALT_REQP;
typedef struct {
	word Info;
} _ALT_CONP;
typedef struct {
	word CIP_Value;
	byte structs[0];      
} _CON_REQP;
typedef struct {
	word Info;
} _CON_CONP;
typedef struct {
	word CIP_Value;
	byte structs[0];      
} _CON_INDP;
typedef struct {
	word Accept;
	byte structs[0];      
} _CON_RESP;
typedef struct {
	byte structs[0];      
} _CON_A_INDP;
typedef struct {
	byte structs[0];      
} _CON_A_RESP;
typedef struct {
	byte structs[0];      
} _DIS_REQP;
typedef struct {
	word Info;
} _DIS_CONP;
typedef struct {
	word Info;
} _DIS_INDP;
typedef struct {
	byte structs[0];      
} _DIS_RESP;
typedef struct {
	dword Info_Mask;
	dword CIP_Mask;
	byte structs[0];      
} _LIS_REQP;
typedef struct {
	word Info;
} _LIS_CONP;
typedef struct {
	byte structs[0];      
} _INF_REQP;
typedef struct {
	word Info;
} _INF_CONP;
typedef struct {
	word Number;
	byte structs[0];      
} _INF_INDP;
typedef struct {
	byte structs[0];      
} _INF_RESP;
typedef struct {
	byte structs[0];      
} _SEL_B_REQP;
typedef struct {
	word Info;
} _SEL_B_CONP;
typedef struct {
	word Selector;
	byte structs[0];      
} _FAC_REQP;
typedef struct {
	byte  struct_length;
	word  function;
	byte  length;
	word  SupplementaryServiceInfo;
	dword SupportedServices;
} _FAC_CON_STRUCTS;
typedef struct {
	word Info;
	word Selector;
	byte structs[0];      
} _FAC_CONP;
typedef struct {
	word Selector;
	byte structs[0];      
} _FAC_INDP;
typedef struct {
	word Selector;
	byte structs[0];      
} _FAC_RESP;
typedef struct {
	byte structs[0];      
} _CON_B3_REQP;
typedef struct {
	word Info;
} _CON_B3_CONP;
typedef struct {
	byte structs[0];      
} _CON_B3_INDP;
typedef struct {
	word Accept;
	byte structs[0];      
} _CON_B3_RESP;
typedef struct {
	byte structs[0];      
} _CON_B3_A_INDP;
typedef struct {
	byte structs[0];      
} _CON_B3_A_RESP;
typedef struct {
	byte structs[0];      
} _DIS_B3_REQP;
typedef struct {
	word Info;
} _DIS_B3_CONP;
typedef struct {
	word Info;
	byte structs[0];      
} _DIS_B3_INDP;
typedef struct {
	byte structs[0];      
} _DIS_B3_RESP;
typedef struct {
	dword         Data;
	word          Data_Length;
	word          Number;
	word          Flags;
} _DAT_B3_REQP;
typedef struct {
	dword         Data;
	word          Data_Length;
	word          Number;
	word          Flags;
	void          *pData;
} _DAT_B3_REQ64P;
typedef struct {
	word          Number;
	word          Info;
} _DAT_B3_CONP;
typedef struct {
	dword         Data;
	word          Data_Length;
	word          Number;
	word          Flags;
} _DAT_B3_INDP;
typedef struct {
	dword         Data;
	word          Data_Length;
	word          Number;
	word          Flags;
	void          *pData;
} _DAT_B3_IND64P;
typedef struct {
	word          Number;
} _DAT_B3_RESP;
typedef struct {
	byte structs[0];      
} _RES_B3_REQP;
typedef struct {
	word Info;
} _RES_B3_CONP;
typedef struct {
	byte structs[0];      
} _RES_B3_INDP;
typedef struct {
	byte structs[0];      
} _RES_B3_RESP;
typedef struct {
	byte structs[0];      
} _CON_B3_T90_A_INDP;
typedef struct {
	word Reject;
	byte structs[0];      
} _CON_B3_T90_A_RESP;
typedef struct _API_MSG CAPI_MSG;
typedef struct _MSG_HEADER CAPI_MSG_HEADER;
struct _API_MSG {
	struct _MSG_HEADER {
		word        length;
		word        appl_id;
		word        command;
		word        number;
		byte        controller;
		byte        plci;
		word        ncci;
	} header;
	union {
		_ALT_REQP           alert_req;
		_ALT_CONP           alert_con;
		_CON_REQP           connect_req;
		_CON_CONP           connect_con;
		_CON_INDP           connect_ind;
		_CON_RESP           connect_res;
		_CON_A_INDP         connect_a_ind;
		_CON_A_RESP         connect_a_res;
		_DIS_REQP           disconnect_req;
		_DIS_CONP           disconnect_con;
		_DIS_INDP           disconnect_ind;
		_DIS_RESP           disconnect_res;
		_LIS_REQP           listen_req;
		_LIS_CONP           listen_con;
		_INF_REQP           info_req;
		_INF_CONP           info_con;
		_INF_INDP           info_ind;
		_INF_RESP           info_res;
		_SEL_B_REQP         select_b_req;
		_SEL_B_CONP         select_b_con;
		_FAC_REQP           facility_req;
		_FAC_CONP           facility_con;
		_FAC_INDP           facility_ind;
		_FAC_RESP           facility_res;
		_CON_B3_REQP        connect_b3_req;
		_CON_B3_CONP        connect_b3_con;
		_CON_B3_INDP        connect_b3_ind;
		_CON_B3_RESP        connect_b3_res;
		_CON_B3_A_INDP      connect_b3_a_ind;
		_CON_B3_A_RESP      connect_b3_a_res;
		_DIS_B3_REQP        disconnect_b3_req;
		_DIS_B3_CONP        disconnect_b3_con;
		_DIS_B3_INDP        disconnect_b3_ind;
		_DIS_B3_RESP        disconnect_b3_res;
		_DAT_B3_REQP        data_b3_req;
		_DAT_B3_REQ64P      data_b3_req64;
		_DAT_B3_CONP        data_b3_con;
		_DAT_B3_INDP        data_b3_ind;
		_DAT_B3_IND64P      data_b3_ind64;
		_DAT_B3_RESP        data_b3_res;
		_RES_B3_REQP        reset_b3_req;
		_RES_B3_CONP        reset_b3_con;
		_RES_B3_INDP        reset_b3_ind;
		_RES_B3_RESP        reset_b3_res;
		_CON_B3_T90_A_INDP  connect_b3_t90_a_ind;
		_CON_B3_T90_A_RESP  connect_b3_t90_a_res;
		byte                b[200];
	} info;
};
#define _NCPI_IGNORED           0x0001
#define _FLAGS_IGNORED          0x0002
#define _ALERT_IGNORED          0x0003
#define GOOD                            0x0000
#define _TOO_MANY_APPLICATIONS          0x1001
#define _BLOCK_TOO_SMALL                0x1002
#define _BUFFER_TOO_BIG                 0x1003
#define _MSG_BUFFER_TOO_SMALL           0x1004
#define _TOO_MANY_CONNECTIONS           0x1005
#define _REG_CAPI_BUSY                  0x1007
#define _REG_RESOURCE_ERROR             0x1008
#define _REG_CAPI_NOT_INSTALLED         0x1009
#define _WRONG_APPL_ID                  0x1101
#define _BAD_MSG                        0x1102
#define _QUEUE_FULL                     0x1103
#define _GET_NO_MSG                     0x1104
#define _MSG_LOST                       0x1105
#define _WRONG_NOTIFY                   0x1106
#define _CAPI_BUSY                      0x1107
#define _RESOURCE_ERROR                 0x1108
#define _CAPI_NOT_INSTALLED             0x1109
#define _NO_EXTERNAL_EQUIPMENT          0x110a
#define _ONLY_EXTERNAL_EQUIPMENT        0x110b
#define _WRONG_STATE                    0x2001
#define _WRONG_IDENTIFIER               0x2002
#define _OUT_OF_PLCI                    0x2003
#define _OUT_OF_NCCI                    0x2004
#define _OUT_OF_LISTEN                  0x2005
#define _OUT_OF_FAX                     0x2006
#define _WRONG_MESSAGE_FORMAT           0x2007
#define _OUT_OF_INTERCONNECT_RESOURCES  0x2008
#define _B1_NOT_SUPPORTED                    0x3001
#define _B2_NOT_SUPPORTED                    0x3002
#define _B3_NOT_SUPPORTED                    0x3003
#define _B1_PARM_NOT_SUPPORTED               0x3004
#define _B2_PARM_NOT_SUPPORTED               0x3005
#define _B3_PARM_NOT_SUPPORTED               0x3006
#define _B_STACK_NOT_SUPPORTED               0x3007
#define _NCPI_NOT_SUPPORTED                  0x3008
#define _CIP_NOT_SUPPORTED                   0x3009
#define _FLAGS_NOT_SUPPORTED                 0x300a
#define _FACILITY_NOT_SUPPORTED              0x300b
#define _DATA_LEN_NOT_SUPPORTED              0x300c
#define _RESET_NOT_SUPPORTED                 0x300d
#define _SUPPLEMENTARY_SERVICE_NOT_SUPPORTED 0x300e
#define _REQUEST_NOT_ALLOWED_IN_THIS_STATE   0x3010
#define _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP 0x3011
#define _L1_ERROR                       0x3301
#define _L2_ERROR                       0x3302
#define _L3_ERROR                       0x3303
#define _OTHER_APPL_CONNECTED           0x3304
#define _CAPI_GUARD_ERROR               0x3305
#define _L3_CAUSE                       0x3400
#define _B_CHANNEL_LOST                 0x3301
#define _B2_ERROR                       0x3302
#define _B3_ERROR                       0x3303
#define _FAX_NO_CONNECTION              0x3311
#define _FAX_TRAINING_ERROR             0x3312
#define _FAX_REMOTE_REJECT              0x3313
#define _FAX_REMOTE_ABORT               0x3314
#define _FAX_PROTOCOL_ERROR             0x3315
#define _FAX_TX_UNDERRUN                0x3316
#define _FAX_RX_OVERFLOW                0x3317
#define _FAX_LOCAL_ABORT                0x3318
#define _FAX_PARAMETER_ERROR            0x3319
#define _LI_USER_INITIATED               0x0000
#define _LI_LINE_NO_LONGER_AVAILABLE     0x3805
#define _LI_INTERCONNECT_NOT_ESTABLISHED 0x3806
#define _LI_LINES_NOT_COMPATIBLE         0x3807
#define _LI2_USER_INITIATED              0x0000
#define _LI2_PLCI_HAS_NO_BCHANNEL        0x3800
#define _LI2_LINES_NOT_COMPATIBLE        0x3801
#define _LI2_NOT_IN_SAME_INTERCONNECTION 0x3802
#define GL_INTERNAL_CONTROLLER_SUPPORTED     0x00000001L
#define GL_EXTERNAL_EQUIPMENT_SUPPORTED      0x00000002L
#define GL_HANDSET_SUPPORTED                 0x00000004L
#define GL_DTMF_SUPPORTED                    0x00000008L
#define GL_SUPPLEMENTARY_SERVICES_SUPPORTED  0x00000010L
#define GL_CHANNEL_ALLOCATION_SUPPORTED      0x00000020L
#define GL_BCHANNEL_OPERATION_SUPPORTED      0x00000040L
#define GL_LINE_INTERCONNECT_SUPPORTED       0x00000080L
#define GL_ECHO_CANCELLER_SUPPORTED          0x00000100L
#define B1_HDLC                 0
#define B1_TRANSPARENT          1
#define B1_V110_ASYNC           2
#define B1_V110_SYNC            3
#define B1_T30                  4
#define B1_HDLC_INVERTED        5
#define B1_TRANSPARENT_R        6
#define B1_MODEM_ALL_NEGOTIATE  7
#define B1_MODEM_ASYNC          8
#define B1_MODEM_SYNC_HDLC      9
#define B2_X75                  0
#define B2_TRANSPARENT          1
#define B2_SDLC                 2
#define B2_LAPD                 3
#define B2_T30                  4
#define B2_PPP                  5
#define B2_TRANSPARENT_NO_CRC   6
#define B2_MODEM_EC_COMPRESSION 7
#define B2_X75_V42BIS           8
#define B2_V120_ASYNC           9
#define B2_V120_ASYNC_V42BIS    10
#define B2_V120_BIT_TRANSPARENT 11
#define B2_LAPD_FREE_SAPI_SEL   12
#define B3_TRANSPARENT          0
#define B3_T90NL                1
#define B3_ISO8208              2
#define B3_X25_DCE              3
#define B3_T30                  4
#define B3_T30_WITH_EXTENSIONS  5
#define B3_RESERVED             6
#define B3_MODEM                7
#define SELECTOR_HANDSET            0
#define SELECTOR_DTMF               1
#define SELECTOR_V42BIS             2
#define SELECTOR_SU_SERV            3
#define SELECTOR_POWER_MANAGEMENT   4
#define SELECTOR_LINE_INTERCONNECT  5
#define SELECTOR_ECHO_CANCELLER     6
#define S_GET_SUPPORTED_SERVICES  0x0000
#define S_LISTEN                  0x0001
#define S_HOLD                    0x0002
#define S_RETRIEVE                0x0003
#define S_SUSPEND                 0x0004
#define S_RESUME                  0x0005
#define S_ECT                     0x0006
#define S_3PTY_BEGIN              0x0007
#define S_3PTY_END                0x0008
#define S_CALL_DEFLECTION         0x000d
#define S_CALL_FORWARDING_START   0x0009
#define S_CALL_FORWARDING_STOP    0x000a
#define S_INTERROGATE_DIVERSION   0x000b 
#define S_INTERROGATE_NUMBERS     0x000c
#define S_CCBS_REQUEST            0x000f
#define S_CCBS_DEACTIVATE         0x0010
#define S_CCBS_INTERROGATE        0x0011
#define S_CCBS_CALL               0x0012
#define S_MWI_ACTIVATE            0x0013
#define S_MWI_DEACTIVATE          0x0014
#define S_CONF_BEGIN           0x0017
#define S_CONF_ADD                0x0018
#define S_CONF_SPLIT           0x0019
#define S_CONF_DROP               0x001a
#define S_CONF_ISOLATE           0x001b
#define S_CONF_REATTACH           0x001c
#define S_CCBS_ERASECALLLINKAGEID 0x800d
#define S_CCBS_STOP_ALERTING      0x8012
#define S_CCBS_INFO_RETAIN        0x8013
#define S_MWI_INDICATE            0x8014
#define S_CONF_PARTYDISC          0x8016
#define S_CONF_NOTIFICATION       0x8017
#define MASK_HOLD_RETRIEVE        0x00000001
#define MASK_TERMINAL_PORTABILITY 0x00000002
#define MASK_ECT                  0x00000004
#define MASK_3PTY                 0x00000008
#define MASK_CALL_FORWARDING      0x00000010
#define MASK_CALL_DEFLECTION      0x00000020
#define MASK_MWI                  0x00000100
#define MASK_CCNR                 0x00000200
#define MASK_CONF                 0x00000400
#define DTMF_LISTEN_START     1
#define DTMF_LISTEN_STOP      2
#define DTMF_DIGITS_SEND      3
#define DTMF_SUCCESS          0
#define DTMF_INCORRECT_DIGIT  1
#define DTMF_UNKNOWN_REQUEST  2
#define LI_GET_SUPPORTED_SERVICES       0
#define LI_REQ_CONNECT                  1
#define LI_REQ_DISCONNECT               2
#define LI_IND_CONNECT_ACTIVE           1
#define LI_IND_DISCONNECT               2
#define LI_FLAG_CONFERENCE_A_B          ((dword) 0x00000001L)
#define LI_FLAG_CONFERENCE_B_A          ((dword) 0x00000002L)
#define LI_FLAG_MONITOR_A               ((dword) 0x00000004L)
#define LI_FLAG_MONITOR_B               ((dword) 0x00000008L)
#define LI_FLAG_ANNOUNCEMENT_A          ((dword) 0x00000010L)
#define LI_FLAG_ANNOUNCEMENT_B          ((dword) 0x00000020L)
#define LI_FLAG_MIX_A                   ((dword) 0x00000040L)
#define LI_FLAG_MIX_B                   ((dword) 0x00000080L)
#define LI_CONFERENCING_SUPPORTED       ((dword) 0x00000001L)
#define LI_MONITORING_SUPPORTED         ((dword) 0x00000002L)
#define LI_ANNOUNCEMENTS_SUPPORTED      ((dword) 0x00000004L)
#define LI_MIXING_SUPPORTED             ((dword) 0x00000008L)
#define LI_CROSS_CONTROLLER_SUPPORTED   ((dword) 0x00000010L)
#define LI2_GET_SUPPORTED_SERVICES      0
#define LI2_REQ_CONNECT                 1
#define LI2_REQ_DISCONNECT              2
#define LI2_IND_CONNECT_ACTIVE          1
#define LI2_IND_DISCONNECT              2
#define LI2_FLAG_INTERCONNECT_A_B       ((dword) 0x00000001L)
#define LI2_FLAG_INTERCONNECT_B_A       ((dword) 0x00000002L)
#define LI2_FLAG_MONITOR_B              ((dword) 0x00000004L)
#define LI2_FLAG_MIX_B                  ((dword) 0x00000008L)
#define LI2_FLAG_MONITOR_X              ((dword) 0x00000010L)
#define LI2_FLAG_MIX_X                  ((dword) 0x00000020L)
#define LI2_FLAG_LOOP_B                 ((dword) 0x00000040L)
#define LI2_FLAG_LOOP_PC                ((dword) 0x00000080L)
#define LI2_FLAG_LOOP_X                 ((dword) 0x00000100L)
#define LI2_CROSS_CONTROLLER_SUPPORTED  ((dword) 0x00000001L)
#define LI2_ASYMMETRIC_SUPPORTED        ((dword) 0x00000002L)
#define LI2_MONITORING_SUPPORTED        ((dword) 0x00000004L)
#define LI2_MIXING_SUPPORTED            ((dword) 0x00000008L)
#define LI2_REMOTE_MONITORING_SUPPORTED ((dword) 0x00000010L)
#define LI2_REMOTE_MIXING_SUPPORTED     ((dword) 0x00000020L)
#define LI2_B_LOOPING_SUPPORTED         ((dword) 0x00000040L)
#define LI2_PC_LOOPING_SUPPORTED        ((dword) 0x00000080L)
#define LI2_X_LOOPING_SUPPORTED         ((dword) 0x00000100L)
#define EC_GET_SUPPORTED_SERVICES            0
#define EC_ENABLE_OPERATION                  1
#define EC_DISABLE_OPERATION                 2
#define EC_ENABLE_NON_LINEAR_PROCESSING      0x0001
#define EC_DO_NOT_REQUIRE_REVERSALS          0x0002
#define EC_DETECT_DISABLE_TONE               0x0004
#define EC_ENABLE_ADAPTIVE_PREDELAY          0x0008
#define EC_NON_LINEAR_PROCESSING_SUPPORTED   0x0001
#define EC_BYPASS_ON_ANY_2100HZ_SUPPORTED    0x0002
#define EC_BYPASS_ON_REV_2100HZ_SUPPORTED    0x0004
#define EC_ADAPTIVE_PREDELAY_SUPPORTED       0x0008
#define EC_BYPASS_INDICATION                 1
#define EC_BYPASS_DUE_TO_CONTINUOUS_2100HZ   1
#define EC_BYPASS_DUE_TO_REVERSED_2100HZ     2
#define EC_BYPASS_RELEASED                   3
#endif 
