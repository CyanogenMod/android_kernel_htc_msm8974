/* bnx2x_reg.h: Broadcom Everest network driver.
 *
 * Copyright (c) 2007-2012 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * The registers description starts with the register Access type followed
 * by size in bits. For example [RW 32]. The access types are:
 * R  - Read only
 * RC - Clear on read
 * RW - Read/Write
 * ST - Statistics register (clear on read)
 * W  - Write only
 * WB - Wide bus register - the size is over 32 bits and it should be
 *      read/write in consecutive 32 bits accesses
 * WR - Write Clear (write 1 to clear the bit)
 *
 */
#ifndef BNX2X_REG_H
#define BNX2X_REG_H

#define ATC_ATC_INT_STS_REG_ADDRESS_ERROR			 (0x1<<0)
#define ATC_ATC_INT_STS_REG_ATC_GPA_MULTIPLE_HITS		 (0x1<<2)
#define ATC_ATC_INT_STS_REG_ATC_IREQ_LESS_THAN_STU		 (0x1<<5)
#define ATC_ATC_INT_STS_REG_ATC_RCPL_TO_EMPTY_CNT		 (0x1<<3)
#define ATC_ATC_INT_STS_REG_ATC_TCPL_ERROR			 (0x1<<4)
#define ATC_ATC_INT_STS_REG_ATC_TCPL_TO_NOT_PEND		 (0x1<<1)
#define ATC_REG_ATC_INIT_ARRAY					 0x1100b8
#define ATC_REG_ATC_INIT_DONE					 0x1100bc
#define ATC_REG_ATC_INT_STS_CLR					 0x1101c0
#define ATC_REG_ATC_PRTY_MASK					 0x1101d8
#define ATC_REG_ATC_PRTY_STS_CLR				 0x1101d0
#define BRB1_REG_BRB1_INT_MASK					 0x60128
#define BRB1_REG_BRB1_INT_STS					 0x6011c
#define BRB1_REG_BRB1_PRTY_MASK 				 0x60138
#define BRB1_REG_BRB1_PRTY_STS					 0x6012c
#define BRB1_REG_BRB1_PRTY_STS_CLR				 0x60130
#define BRB1_REG_FREE_LIST_PRS_CRDT				 0x60200
#define BRB1_REG_FULL_0_XOFF_THRESHOLD_0			 0x601d0
#define BRB1_REG_FULL_0_XOFF_THRESHOLD_1			 0x60230
#define BRB1_REG_FULL_0_XON_THRESHOLD_0				 0x601d4
#define BRB1_REG_FULL_0_XON_THRESHOLD_1				 0x60234
#define BRB1_REG_FULL_1_XOFF_THRESHOLD_0			 0x601d8
#define BRB1_REG_FULL_1_XOFF_THRESHOLD_1			 0x60238
#define BRB1_REG_FULL_1_XON_THRESHOLD_0				 0x601dc
#define BRB1_REG_FULL_1_XON_THRESHOLD_1				 0x6023c
#define BRB1_REG_FULL_LB_XOFF_THRESHOLD				 0x601e0
#define BRB1_REG_FULL_LB_XON_THRESHOLD				 0x601e4
#define BRB1_REG_HIGH_LLFC_HIGH_THRESHOLD_0			 0x6014c
#define BRB1_REG_HIGH_LLFC_LOW_THRESHOLD_0			 0x6013c
#define BRB1_REG_LB_GUARANTIED					 0x601ec
#define BRB1_REG_LB_GUARANTIED_HYST				 0x60264
#define BRB1_REG_LL_RAM						 0x61000
#define BRB1_REG_LOW_LLFC_HIGH_THRESHOLD_0			 0x6016c
#define BRB1_REG_LOW_LLFC_LOW_THRESHOLD_0			 0x6015c
#define BRB1_REG_MAC_0_CLASS_0_GUARANTIED			 0x60244
#define BRB1_REG_MAC_0_CLASS_0_GUARANTIED_HYST			 0x60254
#define BRB1_REG_MAC_0_CLASS_1_GUARANTIED			 0x60248
#define BRB1_REG_MAC_0_CLASS_1_GUARANTIED_HYST			 0x60258
#define BRB1_REG_MAC_1_CLASS_0_GUARANTIED			 0x6024c
#define BRB1_REG_MAC_1_CLASS_0_GUARANTIED_HYST			 0x6025c
#define BRB1_REG_MAC_1_CLASS_1_GUARANTIED			 0x60250
#define BRB1_REG_MAC_1_CLASS_1_GUARANTIED_HYST			 0x60260
#define BRB1_REG_MAC_GUARANTIED_0				 0x601e8
#define BRB1_REG_MAC_GUARANTIED_1				 0x60240
#define BRB1_REG_NUM_OF_FULL_BLOCKS				 0x60090
#define BRB1_REG_NUM_OF_FULL_CYCLES_0				 0x600c8
#define BRB1_REG_NUM_OF_FULL_CYCLES_1				 0x600cc
#define BRB1_REG_NUM_OF_FULL_CYCLES_4				 0x600d8
#define BRB1_REG_NUM_OF_PAUSE_CYCLES_0				 0x600b8
#define BRB1_REG_NUM_OF_PAUSE_CYCLES_1				 0x600bc
#define BRB1_REG_PAUSE_0_XOFF_THRESHOLD_0			 0x601c0
#define BRB1_REG_PAUSE_0_XOFF_THRESHOLD_1			 0x60220
#define BRB1_REG_PAUSE_0_XON_THRESHOLD_0			 0x601c4
#define BRB1_REG_PAUSE_0_XON_THRESHOLD_1			 0x60224
#define BRB1_REG_PAUSE_1_XOFF_THRESHOLD_0			 0x601c8
#define BRB1_REG_PAUSE_1_XOFF_THRESHOLD_1			 0x60228
#define BRB1_REG_PAUSE_1_XON_THRESHOLD_0			 0x601cc
#define BRB1_REG_PAUSE_1_XON_THRESHOLD_1			 0x6022c
#define BRB1_REG_PAUSE_HIGH_THRESHOLD_0 			 0x60078
#define BRB1_REG_PAUSE_HIGH_THRESHOLD_1 			 0x6007c
#define BRB1_REG_PAUSE_LOW_THRESHOLD_0				 0x60068
#define BRB1_REG_PER_CLASS_GUARANTY_MODE			 0x60268
#define BRB1_REG_PORT_NUM_OCC_BLOCKS_0				 0x60094
#define BRB1_REG_SOFT_RESET					 0x600dc
#define CCM_REG_CAM_OCCUP					 0xd0188
#define CCM_REG_CCM_CFC_IFEN					 0xd003c
#define CCM_REG_CCM_CQM_IFEN					 0xd000c
#define CCM_REG_CCM_CQM_USE_Q					 0xd00c0
#define CCM_REG_CCM_INT_MASK					 0xd01e4
#define CCM_REG_CCM_INT_STS					 0xd01d8
#define CCM_REG_CCM_PRTY_MASK					 0xd01f4
#define CCM_REG_CCM_PRTY_STS					 0xd01e8
#define CCM_REG_CCM_PRTY_STS_CLR				 0xd01ec
/* [RW 3] The size of AG context region 0 in REG-pairs. Designates the MS
   REG-pair number (e.g. if region 0 is 6 REG-pairs; the value should be 5).
   Is used to determine the number of the AG context REG-pairs written back;
   when the input message Reg1WbFlg isn't set. */
#define CCM_REG_CCM_REG0_SZ					 0xd00c4
#define CCM_REG_CCM_STORM0_IFEN 				 0xd0004
#define CCM_REG_CCM_STORM1_IFEN 				 0xd0008
#define CCM_REG_CDU_AG_RD_IFEN					 0xd0030
#define CCM_REG_CDU_AG_WR_IFEN					 0xd002c
#define CCM_REG_CDU_SM_RD_IFEN					 0xd0038
#define CCM_REG_CDU_SM_WR_IFEN					 0xd0034
#define CCM_REG_CFC_INIT_CRD					 0xd0204
#define CCM_REG_CNT_AUX1_Q					 0xd00c8
#define CCM_REG_CNT_AUX2_Q					 0xd00cc
#define CCM_REG_CQM_CCM_HDR_P					 0xd008c
#define CCM_REG_CQM_CCM_HDR_S					 0xd0090
#define CCM_REG_CQM_CCM_IFEN					 0xd0014
#define CCM_REG_CQM_INIT_CRD					 0xd020c
#define CCM_REG_CQM_P_WEIGHT					 0xd00b8
#define CCM_REG_CQM_S_WEIGHT					 0xd00bc
#define CCM_REG_CSDM_IFEN					 0xd0018
#define CCM_REG_CSDM_LENGTH_MIS 				 0xd0170
#define CCM_REG_CSDM_WEIGHT					 0xd00b4
#define CCM_REG_ERR_CCM_HDR					 0xd0094
#define CCM_REG_ERR_EVNT_ID					 0xd0098
#define CCM_REG_FIC0_INIT_CRD					 0xd0210
#define CCM_REG_FIC1_INIT_CRD					 0xd0214
#define CCM_REG_GR_ARB_TYPE					 0xd015c
#define CCM_REG_GR_LD0_PR					 0xd0164
#define CCM_REG_GR_LD1_PR					 0xd0168
#define CCM_REG_INV_DONE_Q					 0xd0108
#define CCM_REG_N_SM_CTX_LD_0					 0xd004c
#define CCM_REG_N_SM_CTX_LD_1					 0xd0050
#define CCM_REG_N_SM_CTX_LD_2					 0xd0054
#define CCM_REG_N_SM_CTX_LD_3					 0xd0058
#define CCM_REG_N_SM_CTX_LD_4					 0xd005c
#define CCM_REG_PBF_IFEN					 0xd0028
#define CCM_REG_PBF_LENGTH_MIS					 0xd0180
#define CCM_REG_PBF_WEIGHT					 0xd00ac
#define CCM_REG_PHYS_QNUM1_0					 0xd0134
#define CCM_REG_PHYS_QNUM1_1					 0xd0138
#define CCM_REG_PHYS_QNUM2_0					 0xd013c
#define CCM_REG_PHYS_QNUM2_1					 0xd0140
#define CCM_REG_PHYS_QNUM3_0					 0xd0144
#define CCM_REG_PHYS_QNUM3_1					 0xd0148
#define CCM_REG_QOS_PHYS_QNUM0_0				 0xd0114
#define CCM_REG_QOS_PHYS_QNUM0_1				 0xd0118
#define CCM_REG_QOS_PHYS_QNUM1_0				 0xd011c
#define CCM_REG_QOS_PHYS_QNUM1_1				 0xd0120
#define CCM_REG_QOS_PHYS_QNUM2_0				 0xd0124
#define CCM_REG_QOS_PHYS_QNUM2_1				 0xd0128
#define CCM_REG_QOS_PHYS_QNUM3_0				 0xd012c
#define CCM_REG_QOS_PHYS_QNUM3_1				 0xd0130
#define CCM_REG_STORM_CCM_IFEN					 0xd0010
#define CCM_REG_STORM_LENGTH_MIS				 0xd016c
#define CCM_REG_STORM_WEIGHT					 0xd009c
#define CCM_REG_TSEM_IFEN					 0xd001c
#define CCM_REG_TSEM_LENGTH_MIS 				 0xd0174
#define CCM_REG_TSEM_WEIGHT					 0xd00a0
#define CCM_REG_USEM_IFEN					 0xd0024
#define CCM_REG_USEM_LENGTH_MIS 				 0xd017c
#define CCM_REG_USEM_WEIGHT					 0xd00a8
#define CCM_REG_XSEM_IFEN					 0xd0020
#define CCM_REG_XSEM_LENGTH_MIS 				 0xd0178
#define CCM_REG_XSEM_WEIGHT					 0xd00a4
#define CCM_REG_XX_DESCR_TABLE					 0xd0300
#define CCM_REG_XX_DESCR_TABLE_SIZE				 24
#define CCM_REG_XX_FREE 					 0xd0184
#define CCM_REG_XX_INIT_CRD					 0xd0220
#define CCM_REG_XX_MSG_NUM					 0xd0224
#define CCM_REG_XX_OVFL_EVNT_ID 				 0xd0044
#define CCM_REG_XX_TABLE					 0xd0280
#define CDU_REG_CDU_CHK_MASK0					 0x101000
#define CDU_REG_CDU_CHK_MASK1					 0x101004
#define CDU_REG_CDU_CONTROL0					 0x101008
#define CDU_REG_CDU_DEBUG					 0x101010
#define CDU_REG_CDU_GLOBAL_PARAMS				 0x101020
#define CDU_REG_CDU_INT_MASK					 0x10103c
#define CDU_REG_CDU_INT_STS					 0x101030
#define CDU_REG_CDU_PRTY_MASK					 0x10104c
#define CDU_REG_CDU_PRTY_STS					 0x101040
#define CDU_REG_CDU_PRTY_STS_CLR				 0x101044
#define CDU_REG_ERROR_DATA					 0x101014
#define CDU_REG_L1TT						 0x101800
#define CDU_REG_MATT						 0x101100
#define CDU_REG_MF_MODE 					 0x101050
#define CFC_REG_AC_INIT_DONE					 0x104078
#define CFC_REG_ACTIVITY_COUNTER				 0x104400
#define CFC_REG_ACTIVITY_COUNTER_SIZE				 256
#define CFC_REG_CAM_INIT_DONE					 0x10407c
#define CFC_REG_CFC_INT_MASK					 0x104108
#define CFC_REG_CFC_INT_STS					 0x1040fc
#define CFC_REG_CFC_INT_STS_CLR 				 0x104100
#define CFC_REG_CFC_PRTY_MASK					 0x104118
#define CFC_REG_CFC_PRTY_STS					 0x10410c
#define CFC_REG_CFC_PRTY_STS_CLR				 0x104110
#define CFC_REG_CID_CAM 					 0x104800
#define CFC_REG_CONTROL0					 0x104028
#define CFC_REG_DEBUG0						 0x104050
#define CFC_REG_DISABLE_ON_ERROR				 0x104044
#define CFC_REG_ERROR_VECTOR					 0x10403c
#define CFC_REG_INFO_RAM					 0x105000
#define CFC_REG_INFO_RAM_SIZE					 1024
#define CFC_REG_INIT_REG					 0x10404c
#define CFC_REG_INTERFACES					 0x104058
#define CFC_REG_LCREQ_WEIGHTS					 0x104084
#define CFC_REG_LINK_LIST					 0x104c00
#define CFC_REG_LINK_LIST_SIZE					 256
#define CFC_REG_LL_INIT_DONE					 0x104074
#define CFC_REG_NUM_LCIDS_ALLOC 				 0x104020
#define CFC_REG_NUM_LCIDS_ARRIVING				 0x104004
#define CFC_REG_NUM_LCIDS_INSIDE_PF				 0x104120
#define CFC_REG_NUM_LCIDS_LEAVING				 0x104018
#define CFC_REG_WEAK_ENABLE_PF					 0x104124
#define CSDM_REG_AGG_INT_EVENT_0				 0xc2038
#define CSDM_REG_AGG_INT_EVENT_10				 0xc2060
#define CSDM_REG_AGG_INT_EVENT_11				 0xc2064
#define CSDM_REG_AGG_INT_EVENT_12				 0xc2068
#define CSDM_REG_AGG_INT_EVENT_13				 0xc206c
#define CSDM_REG_AGG_INT_EVENT_14				 0xc2070
#define CSDM_REG_AGG_INT_EVENT_15				 0xc2074
#define CSDM_REG_AGG_INT_EVENT_16				 0xc2078
#define CSDM_REG_AGG_INT_EVENT_2				 0xc2040
#define CSDM_REG_AGG_INT_EVENT_3				 0xc2044
#define CSDM_REG_AGG_INT_EVENT_4				 0xc2048
#define CSDM_REG_AGG_INT_EVENT_5				 0xc204c
#define CSDM_REG_AGG_INT_EVENT_6				 0xc2050
#define CSDM_REG_AGG_INT_EVENT_7				 0xc2054
#define CSDM_REG_AGG_INT_EVENT_8				 0xc2058
#define CSDM_REG_AGG_INT_EVENT_9				 0xc205c
#define CSDM_REG_AGG_INT_MODE_10				 0xc21e0
#define CSDM_REG_AGG_INT_MODE_11				 0xc21e4
#define CSDM_REG_AGG_INT_MODE_12				 0xc21e8
#define CSDM_REG_AGG_INT_MODE_13				 0xc21ec
#define CSDM_REG_AGG_INT_MODE_14				 0xc21f0
#define CSDM_REG_AGG_INT_MODE_15				 0xc21f4
#define CSDM_REG_AGG_INT_MODE_16				 0xc21f8
#define CSDM_REG_AGG_INT_MODE_6 				 0xc21d0
#define CSDM_REG_AGG_INT_MODE_7 				 0xc21d4
#define CSDM_REG_AGG_INT_MODE_8 				 0xc21d8
#define CSDM_REG_AGG_INT_MODE_9 				 0xc21dc
#define CSDM_REG_CFC_RSP_START_ADDR				 0xc2008
#define CSDM_REG_CMP_COUNTER_MAX0				 0xc201c
#define CSDM_REG_CMP_COUNTER_MAX1				 0xc2020
#define CSDM_REG_CMP_COUNTER_MAX2				 0xc2024
#define CSDM_REG_CMP_COUNTER_MAX3				 0xc2028
#define CSDM_REG_CMP_COUNTER_START_ADDR 			 0xc200c
#define CSDM_REG_CSDM_INT_MASK_0				 0xc229c
#define CSDM_REG_CSDM_INT_MASK_1				 0xc22ac
#define CSDM_REG_CSDM_INT_STS_0 				 0xc2290
#define CSDM_REG_CSDM_INT_STS_1 				 0xc22a0
#define CSDM_REG_CSDM_PRTY_MASK 				 0xc22bc
#define CSDM_REG_CSDM_PRTY_STS					 0xc22b0
#define CSDM_REG_CSDM_PRTY_STS_CLR				 0xc22b4
#define CSDM_REG_ENABLE_IN1					 0xc2238
#define CSDM_REG_ENABLE_IN2					 0xc223c
#define CSDM_REG_ENABLE_OUT1					 0xc2240
#define CSDM_REG_ENABLE_OUT2					 0xc2244
#define CSDM_REG_INIT_CREDIT_PXP_CTRL				 0xc24bc
#define CSDM_REG_NUM_OF_ACK_AFTER_PLACE 			 0xc227c
#define CSDM_REG_NUM_OF_PKT_END_MSG				 0xc2274
#define CSDM_REG_NUM_OF_PXP_ASYNC_REQ				 0xc2278
#define CSDM_REG_NUM_OF_Q0_CMD					 0xc2248
#define CSDM_REG_NUM_OF_Q10_CMD 				 0xc226c
#define CSDM_REG_NUM_OF_Q11_CMD 				 0xc2270
#define CSDM_REG_NUM_OF_Q1_CMD					 0xc224c
#define CSDM_REG_NUM_OF_Q3_CMD					 0xc2250
#define CSDM_REG_NUM_OF_Q4_CMD					 0xc2254
#define CSDM_REG_NUM_OF_Q5_CMD					 0xc2258
#define CSDM_REG_NUM_OF_Q6_CMD					 0xc225c
#define CSDM_REG_NUM_OF_Q7_CMD					 0xc2260
#define CSDM_REG_NUM_OF_Q8_CMD					 0xc2264
#define CSDM_REG_NUM_OF_Q9_CMD					 0xc2268
#define CSDM_REG_Q_COUNTER_START_ADDR				 0xc2010
#define CSDM_REG_RSP_PXP_CTRL_RDATA_EMPTY			 0xc2548
#define CSDM_REG_SYNC_PARSER_EMPTY				 0xc2550
#define CSDM_REG_SYNC_SYNC_EMPTY				 0xc2558
#define CSDM_REG_TIMER_TICK					 0xc2000
#define CSEM_REG_ARB_CYCLE_SIZE 				 0x200034
#define CSEM_REG_ARB_ELEMENT0					 0x200020
#define CSEM_REG_ARB_ELEMENT1					 0x200024
#define CSEM_REG_ARB_ELEMENT2					 0x200028
#define CSEM_REG_ARB_ELEMENT3					 0x20002c
#define CSEM_REG_ARB_ELEMENT4					 0x200030
#define CSEM_REG_CSEM_INT_MASK_0				 0x200110
#define CSEM_REG_CSEM_INT_MASK_1				 0x200120
#define CSEM_REG_CSEM_INT_STS_0 				 0x200104
#define CSEM_REG_CSEM_INT_STS_1 				 0x200114
#define CSEM_REG_CSEM_PRTY_MASK_0				 0x200130
#define CSEM_REG_CSEM_PRTY_MASK_1				 0x200140
#define CSEM_REG_CSEM_PRTY_STS_0				 0x200124
#define CSEM_REG_CSEM_PRTY_STS_1				 0x200134
#define CSEM_REG_CSEM_PRTY_STS_CLR_0				 0x200128
#define CSEM_REG_CSEM_PRTY_STS_CLR_1				 0x200138
#define CSEM_REG_ENABLE_IN					 0x2000a4
#define CSEM_REG_ENABLE_OUT					 0x2000a8
#define CSEM_REG_FAST_MEMORY					 0x220000
#define CSEM_REG_FIC0_DISABLE					 0x200224
#define CSEM_REG_FIC1_DISABLE					 0x200234
#define CSEM_REG_INT_TABLE					 0x200400
#define CSEM_REG_MSG_NUM_FIC0					 0x200000
#define CSEM_REG_MSG_NUM_FIC1					 0x200004
#define CSEM_REG_MSG_NUM_FOC0					 0x200008
#define CSEM_REG_MSG_NUM_FOC1					 0x20000c
#define CSEM_REG_MSG_NUM_FOC2					 0x200010
#define CSEM_REG_MSG_NUM_FOC3					 0x200014
#define CSEM_REG_PAS_DISABLE					 0x20024c
#define CSEM_REG_PASSIVE_BUFFER 				 0x202000
#define CSEM_REG_PRAM						 0x240000
#define CSEM_REG_SLEEP_THREADS_VALID				 0x20026c
#define CSEM_REG_SLOW_EXT_STORE_EMPTY				 0x2002a0
#define CSEM_REG_THREADS_LIST					 0x2002e4
#define CSEM_REG_TS_0_AS					 0x200038
#define CSEM_REG_TS_10_AS					 0x200060
#define CSEM_REG_TS_11_AS					 0x200064
#define CSEM_REG_TS_12_AS					 0x200068
#define CSEM_REG_TS_13_AS					 0x20006c
#define CSEM_REG_TS_14_AS					 0x200070
#define CSEM_REG_TS_15_AS					 0x200074
#define CSEM_REG_TS_16_AS					 0x200078
#define CSEM_REG_TS_17_AS					 0x20007c
#define CSEM_REG_TS_18_AS					 0x200080
#define CSEM_REG_TS_1_AS					 0x20003c
#define CSEM_REG_TS_2_AS					 0x200040
#define CSEM_REG_TS_3_AS					 0x200044
#define CSEM_REG_TS_4_AS					 0x200048
#define CSEM_REG_TS_5_AS					 0x20004c
#define CSEM_REG_TS_6_AS					 0x200050
#define CSEM_REG_TS_7_AS					 0x200054
#define CSEM_REG_TS_8_AS					 0x200058
#define CSEM_REG_TS_9_AS					 0x20005c
#define CSEM_REG_VFPF_ERR_NUM					 0x200380
#define DBG_REG_DBG_PRTY_MASK					 0xc0a8
#define DBG_REG_DBG_PRTY_STS					 0xc09c
#define DBG_REG_DBG_PRTY_STS_CLR				 0xc0a0
#define DMAE_REG_BACKWARD_COMP_EN				 0x10207c
#define DMAE_REG_CMD_MEM					 0x102400
#define DMAE_REG_CMD_MEM_SIZE					 224
#define DMAE_REG_CRC16C_INIT					 0x10201c
#define DMAE_REG_CRC16T10_INIT					 0x102020
#define DMAE_REG_DMAE_INT_MASK					 0x102054
#define DMAE_REG_DMAE_PRTY_MASK 				 0x102064
#define DMAE_REG_DMAE_PRTY_STS					 0x102058
#define DMAE_REG_DMAE_PRTY_STS_CLR				 0x10205c
#define DMAE_REG_GO_C0						 0x102080
#define DMAE_REG_GO_C1						 0x102084
#define DMAE_REG_GO_C10 					 0x102088
#define DMAE_REG_GO_C11 					 0x10208c
#define DMAE_REG_GO_C12 					 0x102090
#define DMAE_REG_GO_C13 					 0x102094
#define DMAE_REG_GO_C14 					 0x102098
#define DMAE_REG_GO_C15 					 0x10209c
#define DMAE_REG_GO_C2						 0x1020a0
#define DMAE_REG_GO_C3						 0x1020a4
#define DMAE_REG_GO_C4						 0x1020a8
#define DMAE_REG_GO_C5						 0x1020ac
#define DMAE_REG_GO_C6						 0x1020b0
#define DMAE_REG_GO_C7						 0x1020b4
#define DMAE_REG_GO_C8						 0x1020b8
#define DMAE_REG_GO_C9						 0x1020bc
#define DMAE_REG_GRC_IFEN					 0x102008
#define DMAE_REG_PCI_IFEN					 0x102004
#define DMAE_REG_PXP_REQ_INIT_CRD				 0x1020c0
#define DORQ_REG_AGG_CMD0					 0x170060
#define DORQ_REG_AGG_CMD1					 0x170064
#define DORQ_REG_AGG_CMD2					 0x170068
#define DORQ_REG_AGG_CMD3					 0x17006c
#define DORQ_REG_CMHEAD_RX					 0x170050
#define DORQ_REG_DB_ADDR0					 0x17008c
#define DORQ_REG_DORQ_INT_MASK					 0x170180
#define DORQ_REG_DORQ_INT_STS					 0x170174
#define DORQ_REG_DORQ_INT_STS_CLR				 0x170178
#define DORQ_REG_DORQ_PRTY_MASK 				 0x170190
#define DORQ_REG_DORQ_PRTY_STS					 0x170184
#define DORQ_REG_DORQ_PRTY_STS_CLR				 0x170188
#define DORQ_REG_DPM_CID_ADDR					 0x170044
#define DORQ_REG_DPM_CID_OFST					 0x170030
#define DORQ_REG_DQ_FIFO_AFULL_TH				 0x17007c
#define DORQ_REG_DQ_FIFO_FULL_TH				 0x170078
#define DORQ_REG_DQ_FILL_LVLF					 0x1700a4
#define DORQ_REG_DQ_FULL_ST					 0x1700c0
#define DORQ_REG_ERR_CMHEAD					 0x170058
#define DORQ_REG_IF_EN						 0x170004
#define DORQ_REG_MODE_ACT					 0x170008
#define DORQ_REG_NORM_CID_OFST					 0x17002c
#define DORQ_REG_NORM_CMHEAD_TX 				 0x17004c
#define DORQ_REG_OUTST_REQ					 0x17003c
#define DORQ_REG_PF_USAGE_CNT					 0x1701d0
#define DORQ_REG_REGN						 0x170038
#define DORQ_REG_RSPA_CRD_CNT					 0x1700ac
#define DORQ_REG_RSPB_CRD_CNT					 0x1700b0
/* [RW 4] The initial credit at the Doorbell Response Interface. The write
   writes the same initial credit to the rspa_crd_cnt and rspb_crd_cnt. The
   read reads this written value. */
#define DORQ_REG_RSP_INIT_CRD					 0x170048
#define DORQ_REG_SHRT_ACT_CNT					 0x170070
#define DORQ_REG_SHRT_CMHEAD					 0x170054
#define HC_CONFIG_0_REG_ATTN_BIT_EN_0				 (0x1<<4)
#define HC_CONFIG_0_REG_BLOCK_DISABLE_0				 (0x1<<0)
#define HC_CONFIG_0_REG_INT_LINE_EN_0				 (0x1<<3)
#define HC_CONFIG_0_REG_MSI_ATTN_EN_0				 (0x1<<7)
#define HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0			 (0x1<<2)
#define HC_CONFIG_0_REG_SINGLE_ISR_EN_0				 (0x1<<1)
#define HC_CONFIG_1_REG_BLOCK_DISABLE_1				 (0x1<<0)
#define HC_REG_AGG_INT_0					 0x108050
#define HC_REG_AGG_INT_1					 0x108054
#define HC_REG_ATTN_BIT 					 0x108120
#define HC_REG_ATTN_IDX 					 0x108100
#define HC_REG_ATTN_MSG0_ADDR_L 				 0x108018
#define HC_REG_ATTN_MSG1_ADDR_L 				 0x108020
#define HC_REG_ATTN_NUM_P0					 0x108038
#define HC_REG_ATTN_NUM_P1					 0x10803c
#define HC_REG_COMMAND_REG					 0x108180
#define HC_REG_CONFIG_0 					 0x108000
#define HC_REG_CONFIG_1 					 0x108004
#define HC_REG_FUNC_NUM_P0					 0x1080ac
#define HC_REG_FUNC_NUM_P1					 0x1080b0
#define HC_REG_HC_PRTY_MASK					 0x1080a0
#define HC_REG_HC_PRTY_STS					 0x108094
#define HC_REG_HC_PRTY_STS_CLR					 0x108098
#define HC_REG_INT_MASK						 0x108108
#define HC_REG_LEADING_EDGE_0					 0x108040
#define HC_REG_LEADING_EDGE_1					 0x108048
#define HC_REG_MAIN_MEMORY					 0x108800
#define HC_REG_MAIN_MEMORY_SIZE					 152
#define HC_REG_P0_PROD_CONS					 0x108200
#define HC_REG_P1_PROD_CONS					 0x108400
#define HC_REG_PBA_COMMAND					 0x108140
#define HC_REG_PCI_CONFIG_0					 0x108010
#define HC_REG_PCI_CONFIG_1					 0x108014
#define HC_REG_STATISTIC_COUNTERS				 0x109000
#define HC_REG_TRAILING_EDGE_0					 0x108044
#define HC_REG_TRAILING_EDGE_1					 0x10804c
#define HC_REG_UC_RAM_ADDR_0					 0x108028
#define HC_REG_UC_RAM_ADDR_1					 0x108030
#define HC_REG_USTORM_ADDR_FOR_COALESCE 			 0x108068
#define HC_REG_VQID_0						 0x108008
#define HC_REG_VQID_1						 0x10800c
#define IGU_BLOCK_CONFIGURATION_REG_BACKWARD_COMP_EN		 (0x1<<1)
#define IGU_BLOCK_CONFIGURATION_REG_BLOCK_ENABLE		 (0x1<<0)
#define IGU_REG_ATTENTION_ACK_BITS				 0x130108
#define IGU_REG_ATTN_FSM					 0x130054
#define IGU_REG_ATTN_MSG_ADDR_H				 0x13011c
#define IGU_REG_ATTN_MSG_ADDR_L				 0x130120
#define IGU_REG_ATTN_WRITE_DONE_PENDING			 0x130030
#define IGU_REG_BLOCK_CONFIGURATION				 0x130000
#define IGU_REG_COMMAND_REG_32LSB_DATA				 0x130124
#define IGU_REG_COMMAND_REG_CTRL				 0x13012c
#define IGU_REG_CSTORM_TYPE_0_SB_CLEANUP			 0x130200
#define IGU_REG_CTRL_FSM					 0x130064
#define IGU_REG_ERROR_HANDLING_DATA_VALID			 0x130130
#define IGU_REG_IGU_PRTY_MASK					 0x1300a8
#define IGU_REG_IGU_PRTY_STS					 0x13009c
#define IGU_REG_IGU_PRTY_STS_CLR				 0x1300a0
#define IGU_REG_INT_HANDLE_FSM					 0x130050
#define IGU_REG_LEADING_EDGE_LATCH				 0x130134
#define IGU_REG_MAPPING_MEMORY					 0x131000
#define IGU_REG_MAPPING_MEMORY_SIZE				 136
#define IGU_REG_PBA_STATUS_LSB					 0x130138
#define IGU_REG_PBA_STATUS_MSB					 0x13013c
#define IGU_REG_PCI_PF_MSI_EN					 0x130140
#define IGU_REG_PCI_PF_MSIX_EN					 0x130144
#define IGU_REG_PCI_PF_MSIX_FUNC_MASK				 0x130148
#define IGU_REG_PENDING_BITS_STATUS				 0x130300
#define IGU_REG_PF_CONFIGURATION				 0x130154
#define IGU_REG_PROD_CONS_MEMORY				 0x132000
#define IGU_REG_PXP_ARB_FSM					 0x130068
#define IGU_REG_RESET_MEMORIES					 0x130158
#define IGU_REG_SB_CTRL_FSM					 0x13004c
#define IGU_REG_SB_INT_BEFORE_MASK_LSB				 0x13015c
#define IGU_REG_SB_INT_BEFORE_MASK_MSB				 0x130160
#define IGU_REG_SB_MASK_LSB					 0x130164
#define IGU_REG_SB_MASK_MSB					 0x130168
#define IGU_REG_SILENT_DROP					 0x13016c
#define IGU_REG_STATISTIC_NUM_MESSAGE_SENT			 0x130800
#define IGU_REG_TIMER_MASKING_VALUE				 0x13003c
#define IGU_REG_TRAILING_EDGE_LATCH				 0x130104
#define IGU_REG_VF_CONFIGURATION				 0x130170
#define IGU_REG_WRITE_DONE_PENDING				 0x130480
#define MCP_A_REG_MCPR_SCRATCH					 0x3a0000
#define MCP_REG_MCPR_ACCESS_LOCK				 0x8009c
#define MCP_REG_MCPR_CPU_PROGRAM_COUNTER			 0x8501c
#define MCP_REG_MCPR_GP_INPUTS					 0x800c0
#define MCP_REG_MCPR_GP_OENABLE					 0x800c8
#define MCP_REG_MCPR_GP_OUTPUTS					 0x800c4
#define MCP_REG_MCPR_IMC_COMMAND				 0x85900
#define MCP_REG_MCPR_IMC_DATAREG0				 0x85920
#define MCP_REG_MCPR_IMC_SLAVE_CONTROL				 0x85904
#define MCP_REG_MCPR_CPU_PROGRAM_COUNTER			 0x8501c
#define MCP_REG_MCPR_NVM_ACCESS_ENABLE				 0x86424
#define MCP_REG_MCPR_NVM_ADDR					 0x8640c
#define MCP_REG_MCPR_NVM_CFG4					 0x8642c
#define MCP_REG_MCPR_NVM_COMMAND				 0x86400
#define MCP_REG_MCPR_NVM_READ					 0x86410
#define MCP_REG_MCPR_NVM_SW_ARB 				 0x86420
#define MCP_REG_MCPR_NVM_WRITE					 0x86408
#define MCP_REG_MCPR_SCRATCH					 0xa0000
#define MISC_AEU_GENERAL_MASK_REG_AEU_NIG_CLOSE_MASK		 (0x1<<1)
#define MISC_AEU_GENERAL_MASK_REG_AEU_PXP_CLOSE_MASK		 (0x1<<0)
#define MISC_REG_AEU_AFTER_INVERT_1_FUNC_0			 0xa42c
#define MISC_REG_AEU_AFTER_INVERT_1_FUNC_1			 0xa430
#define MISC_REG_AEU_AFTER_INVERT_1_MCP 			 0xa434
#define MISC_REG_AEU_AFTER_INVERT_2_FUNC_0			 0xa438
#define MISC_REG_AEU_AFTER_INVERT_2_FUNC_1			 0xa43c
#define MISC_REG_AEU_AFTER_INVERT_2_MCP 			 0xa440
#define MISC_REG_AEU_AFTER_INVERT_3_FUNC_0			 0xa444
#define MISC_REG_AEU_AFTER_INVERT_3_FUNC_1			 0xa448
#define MISC_REG_AEU_AFTER_INVERT_3_MCP 			 0xa44c
#define MISC_REG_AEU_AFTER_INVERT_4_FUNC_0			 0xa450
#define MISC_REG_AEU_AFTER_INVERT_4_FUNC_1			 0xa454
#define MISC_REG_AEU_AFTER_INVERT_4_MCP 			 0xa458
#define MISC_REG_AEU_AFTER_INVERT_5_FUNC_0			 0xa700
#define MISC_REG_AEU_CLR_LATCH_SIGNAL				 0xa45c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_0			 0xa06c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1			 0xa07c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_2			 0xa08c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_3			 0xa09c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_5			 0xa0bc
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_6			 0xa0cc
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_7			 0xa0dc
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_0			 0xa10c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_1			 0xa11c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_2			 0xa12c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_3			 0xa13c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_5			 0xa15c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_6			 0xa16c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_7			 0xa17c
#define MISC_REG_AEU_ENABLE1_NIG_0				 0xa0ec
#define MISC_REG_AEU_ENABLE1_NIG_1				 0xa18c
#define MISC_REG_AEU_ENABLE1_PXP_0				 0xa0fc
#define MISC_REG_AEU_ENABLE1_PXP_1				 0xa19c
#define MISC_REG_AEU_ENABLE2_FUNC_0_OUT_0			 0xa070
#define MISC_REG_AEU_ENABLE2_FUNC_0_OUT_1			 0xa080
#define MISC_REG_AEU_ENABLE2_FUNC_1_OUT_0			 0xa110
#define MISC_REG_AEU_ENABLE2_FUNC_1_OUT_1			 0xa120
#define MISC_REG_AEU_ENABLE2_NIG_0				 0xa0f0
#define MISC_REG_AEU_ENABLE2_NIG_1				 0xa190
#define MISC_REG_AEU_ENABLE2_PXP_0				 0xa100
#define MISC_REG_AEU_ENABLE2_PXP_1				 0xa1a0
#define MISC_REG_AEU_ENABLE3_FUNC_0_OUT_0			 0xa074
#define MISC_REG_AEU_ENABLE3_FUNC_0_OUT_1			 0xa084
#define MISC_REG_AEU_ENABLE3_FUNC_1_OUT_0			 0xa114
#define MISC_REG_AEU_ENABLE3_FUNC_1_OUT_1			 0xa124
#define MISC_REG_AEU_ENABLE3_NIG_0				 0xa0f4
#define MISC_REG_AEU_ENABLE3_NIG_1				 0xa194
#define MISC_REG_AEU_ENABLE3_PXP_0				 0xa104
#define MISC_REG_AEU_ENABLE3_PXP_1				 0xa1a4
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_0			 0xa078
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_2			 0xa098
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_4			 0xa0b8
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_5			 0xa0c8
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_6			 0xa0d8
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_7			 0xa0e8
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_0			 0xa118
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_2			 0xa138
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_4			 0xa158
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_5			 0xa168
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_6			 0xa178
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_7			 0xa188
#define MISC_REG_AEU_ENABLE4_NIG_0				 0xa0f8
#define MISC_REG_AEU_ENABLE4_NIG_1				 0xa198
#define MISC_REG_AEU_ENABLE4_PXP_0				 0xa108
#define MISC_REG_AEU_ENABLE4_PXP_1				 0xa1a8
#define MISC_REG_AEU_ENABLE5_FUNC_0_OUT_0			 0xa688
#define MISC_REG_AEU_ENABLE5_FUNC_1_OUT_0			 0xa6b0
#define MISC_REG_AEU_GENERAL_ATTN_0				 0xa000
#define MISC_REG_AEU_GENERAL_ATTN_1				 0xa004
#define MISC_REG_AEU_GENERAL_ATTN_10				 0xa028
#define MISC_REG_AEU_GENERAL_ATTN_11				 0xa02c
#define MISC_REG_AEU_GENERAL_ATTN_12				 0xa030
#define MISC_REG_AEU_GENERAL_ATTN_2				 0xa008
#define MISC_REG_AEU_GENERAL_ATTN_3				 0xa00c
#define MISC_REG_AEU_GENERAL_ATTN_4				 0xa010
#define MISC_REG_AEU_GENERAL_ATTN_5				 0xa014
#define MISC_REG_AEU_GENERAL_ATTN_6				 0xa018
#define MISC_REG_AEU_GENERAL_ATTN_7				 0xa01c
#define MISC_REG_AEU_GENERAL_ATTN_8				 0xa020
#define MISC_REG_AEU_GENERAL_ATTN_9				 0xa024
#define MISC_REG_AEU_GENERAL_MASK				 0xa61c
#define MISC_REG_AEU_INVERTER_1_FUNC_0				 0xa22c
#define MISC_REG_AEU_INVERTER_1_FUNC_1				 0xa23c
#define MISC_REG_AEU_INVERTER_2_FUNC_0				 0xa230
#define MISC_REG_AEU_INVERTER_2_FUNC_1				 0xa240
#define MISC_REG_AEU_MASK_ATTN_FUNC_0				 0xa060
#define MISC_REG_AEU_MASK_ATTN_FUNC_1				 0xa064
#define MISC_REG_AEU_SYS_KILL_OCCURRED				 0xa610
#define MISC_REG_AEU_SYS_KILL_STATUS_0				 0xa600
#define MISC_REG_AEU_SYS_KILL_STATUS_1				 0xa604
#define MISC_REG_AEU_SYS_KILL_STATUS_2				 0xa608
#define MISC_REG_AEU_SYS_KILL_STATUS_3				 0xa60c
#define MISC_REG_BOND_ID					 0xa400
#define MISC_REG_CHIP_METAL					 0xa404
#define MISC_REG_CHIP_NUM					 0xa408
#define MISC_REG_CHIP_REV					 0xa40c
/* [RW 32] The following driver registers(1...16) represent 16 drivers and
   32 clients. Each client can be controlled by one driver only. One in each
   bit represent that this driver control the appropriate client (Ex: bit 5
   is set means this driver control client number 5). addr1 = set; addr0 =
   clear; read from both addresses will give the same result = status. write
   to address 1 will set a request to control all the clients that their
   appropriate bit (in the write command) is set. if the client is free (the
   appropriate bit in all the other drivers is clear) one will be written to
   that driver register; if the client isn't free the bit will remain zero.
   if the appropriate bit is set (the driver request to gain control on a
   client it already controls the ~MISC_REGISTERS_INT_STS.GENERIC_SW
   interrupt will be asserted). write to address 0 will set a request to
   free all the clients that their appropriate bit (in the write command) is
   set. if the appropriate bit is clear (the driver request to free a client
   it doesn't controls the ~MISC_REGISTERS_INT_STS.GENERIC_SW interrupt will
   be asserted). */
#define MISC_REG_DRIVER_CONTROL_1				 0xa510
#define MISC_REG_DRIVER_CONTROL_7				 0xa3c8
#define MISC_REG_E1HMF_MODE					 0xa5f8
#define MISC_REG_FOUR_PORT_PATH_SWAP				 0xa75c
#define MISC_REG_FOUR_PORT_PATH_SWAP_OVWR			 0xa738
#define MISC_REG_FOUR_PORT_PORT_SWAP				 0xa754
#define MISC_REG_FOUR_PORT_PORT_SWAP_OVWR			 0xa734
#define MISC_REG_GENERIC_CR_0					 0xa460
#define MISC_REG_GENERIC_CR_1					 0xa464
#define MISC_REG_GENERIC_POR_1					 0xa474
#define MISC_REG_GEN_PURP_HWG					 0xa9a0
/* [RW 32] GPIO. [31-28] FLOAT port 0; [27-24] FLOAT port 0; When any of
   these bits is written as a '1'; the corresponding SPIO bit will turn off
   it's drivers and become an input. This is the reset state of all GPIO
   pins. The read value of these bits will be a '1' if that last command
   (#SET; #CLR; or #FLOAT) for this bit was a #FLOAT. (reset value 0xff).
   [23-20] CLR port 1; 19-16] CLR port 0; When any of these bits is written
   as a '1'; the corresponding GPIO bit will drive low. The read value of
   these bits will be a '1' if that last command (#SET; #CLR; or #FLOAT) for
   this bit was a #CLR. (reset value 0). [15-12] SET port 1; 11-8] port 0;
   SET When any of these bits is written as a '1'; the corresponding GPIO
   bit will drive high (if it has that capability). The read value of these
   bits will be a '1' if that last command (#SET; #CLR; or #FLOAT) for this
   bit was a #SET. (reset value 0). [7-4] VALUE port 1; [3-0] VALUE port 0;
   RO; These bits indicate the read value of each of the eight GPIO pins.
   This is the result value of the pin; not the drive value. Writing these
   bits will have not effect. */
#define MISC_REG_GPIO						 0xa490
#define MISC_REG_GPIO_EVENT_EN					 0xa2bc
/* [RW 32] GPIO INT. [31-28] OLD_CLR port1; [27-24] OLD_CLR port0; Writing a
   '1' to these bit clears the corresponding bit in the #OLD_VALUE register.
   This will acknowledge an interrupt on the falling edge of corresponding
   GPIO input (reset value 0). [23-16] OLD_SET [23-16] port1; OLD_SET port0;
   Writing a '1' to these bit sets the corresponding bit in the #OLD_VALUE
   register. This will acknowledge an interrupt on the rising edge of
   corresponding SPIO input (reset value 0). [15-12] OLD_VALUE [11-8] port1;
   OLD_VALUE port0; RO; These bits indicate the old value of the GPIO input
   value. When the ~INT_STATE bit is set; this bit indicates the OLD value
   of the pin such that if ~INT_STATE is set and this bit is '0'; then the
   interrupt is due to a low to high edge. If ~INT_STATE is set and this bit
   is '1'; then the interrupt is due to a high to low edge (reset value 0).
   [7-4] INT_STATE port1; [3-0] INT_STATE RO port0; These bits indicate the
   current GPIO interrupt state for each GPIO pin. This bit is cleared when
   the appropriate #OLD_SET or #OLD_CLR command bit is written. This bit is
   set when the GPIO input does not match the current value in #OLD_VALUE
   (reset value 0). */
#define MISC_REG_GPIO_INT					 0xa494
#define MISC_REG_GRC_RSV_ATTN					 0xa3c0
#define MISC_REG_GRC_TIMEOUT_ATTN				 0xa3c4
#define MISC_REG_GRC_TIMEOUT_EN 				 0xa280
#define MISC_REG_LCPLL_CTRL_1					 0xa2a4
#define MISC_REG_LCPLL_CTRL_REG_2				 0xa2a8
#define MISC_REG_LCPLL_E40_PWRDWN				 0xaa74
#define MISC_REG_LCPLL_E40_RESETB_ANA				 0xaa78
#define MISC_REG_LCPLL_E40_RESETB_DIG				 0xaa7c
#define MISC_REG_MISC_INT_MASK					 0xa388
#define MISC_REG_MISC_PRTY_MASK 				 0xa398
#define MISC_REG_MISC_PRTY_STS					 0xa38c
#define MISC_REG_MISC_PRTY_STS_CLR				 0xa390
#define MISC_REG_NIG_WOL_P0					 0xa270
#define MISC_REG_NIG_WOL_P1					 0xa274
#define MISC_REG_PCIE_HOT_RESET 				 0xa618
#define MISC_REG_PLL_STORM_CTRL_1				 0xa294
#define MISC_REG_PLL_STORM_CTRL_2				 0xa298
#define MISC_REG_PLL_STORM_CTRL_3				 0xa29c
#define MISC_REG_PLL_STORM_CTRL_4				 0xa2a0
#define MISC_REG_PORT4MODE_EN					 0xa750
#define MISC_REG_PORT4MODE_EN_OVWR				 0xa720
/* [RW 32] reset reg#2; rite/read one = the specific block is out of reset;
   write/read zero = the specific block is in reset; addr 0-wr- the write
   value will be written to the register; addr 1-set - one will be written
   to all the bits that have the value of one in the data written (bits that
   have the value of zero will not be change) ; addr 2-clear - zero will be
   written to all the bits that have the value of one in the data written
   (bits that have the value of zero will not be change); addr 3-ignore;
   read ignore from all addr except addr 00; inside order of the bits is:
   [0] rst_bmac0; [1] rst_bmac1; [2] rst_emac0; [3] rst_emac1; [4] rst_grc;
   [5] rst_mcp_n_reset_reg_hard_core; [6] rst_ mcp_n_hard_core_rst_b; [7]
   rst_ mcp_n_reset_cmn_cpu; [8] rst_ mcp_n_reset_cmn_core; [9] rst_rbcn;
   [10] rst_dbg; [11] rst_misc_core; [12] rst_dbue (UART); [13]
   Pci_resetmdio_n; [14] rst_emac0_hard_core; [15] rst_emac1_hard_core; 16]
   rst_pxp_rq_rd_wr; 31:17] reserved */
#define MISC_REG_RESET_REG_1					 0xa580
#define MISC_REG_RESET_REG_2					 0xa590
#define MISC_REG_SHARED_MEM_ADDR				 0xa2b4
/* [RW 32] SPIO. [31-24] FLOAT When any of these bits is written as a '1';
   the corresponding SPIO bit will turn off it's drivers and become an
   input. This is the reset state of all SPIO pins. The read value of these
   bits will be a '1' if that last command (#SET; #CL; or #FLOAT) for this
   bit was a #FLOAT. (reset value 0xff). [23-16] CLR When any of these bits
   is written as a '1'; the corresponding SPIO bit will drive low. The read
   value of these bits will be a '1' if that last command (#SET; #CLR; or
#FLOAT) for this bit was a #CLR. (reset value 0). [15-8] SET When any of
   these bits is written as a '1'; the corresponding SPIO bit will drive
   high (if it has that capability). The read value of these bits will be a
   '1' if that last command (#SET; #CLR; or #FLOAT) for this bit was a #SET.
   (reset value 0). [7-0] VALUE RO; These bits indicate the read value of
   each of the eight SPIO pins. This is the result value of the pin; not the
   drive value. Writing these bits will have not effect. Each 8 bits field
   is divided as follows: [0] VAUX Enable; when pulsed low; enables supply
   from VAUX. (This is an output pin only; the FLOAT field is not applicable
   for this pin); [1] VAUX Disable; when pulsed low; disables supply form
   VAUX. (This is an output pin only; FLOAT field is not applicable for this
   pin); [2] SEL_VAUX_B - Control to power switching logic. Drive low to
   select VAUX supply. (This is an output pin only; it is not controlled by
   the SET and CLR fields; it is controlled by the Main Power SM; the FLOAT
   field is not applicable for this pin; only the VALUE fields is relevant -
   it reflects the output value); [3] port swap [4] spio_4; [5] spio_5; [6]
   Bit 0 of UMP device ID select; read by UMP firmware; [7] Bit 1 of UMP
   device ID select; read by UMP firmware. */
#define MISC_REG_SPIO						 0xa4fc
#define MISC_REG_SPIO_EVENT_EN					 0xa2b8
/* [RW 32] SPIO INT. [31-24] OLD_CLR Writing a '1' to these bit clears the
   corresponding bit in the #OLD_VALUE register. This will acknowledge an
   interrupt on the falling edge of corresponding SPIO input (reset value
   0). [23-16] OLD_SET Writing a '1' to these bit sets the corresponding bit
   in the #OLD_VALUE register. This will acknowledge an interrupt on the
   rising edge of corresponding SPIO input (reset value 0). [15-8] OLD_VALUE
   RO; These bits indicate the old value of the SPIO input value. When the
   ~INT_STATE bit is set; this bit indicates the OLD value of the pin such
   that if ~INT_STATE is set and this bit is '0'; then the interrupt is due
   to a low to high edge. If ~INT_STATE is set and this bit is '1'; then the
   interrupt is due to a high to low edge (reset value 0). [7-0] INT_STATE
   RO; These bits indicate the current SPIO interrupt state for each SPIO
   pin. This bit is cleared when the appropriate #OLD_SET or #OLD_CLR
   command bit is written. This bit is set when the SPIO input does not
   match the current value in #OLD_VALUE (reset value 0). */
#define MISC_REG_SPIO_INT					 0xa500
#define MISC_REG_SW_TIMER_RELOAD_VAL_4				 0xa2fc
#define MISC_REG_SW_TIMER_VAL					 0xa5c0
#define MISC_REG_TWO_PORT_PATH_SWAP				 0xa758
#define MISC_REG_TWO_PORT_PATH_SWAP_OVWR			 0xa72c
#define MISC_REG_UNPREPARED					 0xa424
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_BRCST	 (0x1<<0)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_MLCST	 (0x1<<1)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_NO_VLAN	 (0x1<<4)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_UNCST	 (0x1<<2)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_VLAN	 (0x1<<3)
#define MISC_REG_WC0_CTRL_PHY_ADDR				 0xa9cc
#define MISC_REG_WC0_RESET					 0xac30
#define MISC_REG_XMAC_CORE_PORT_MODE				 0xa964
#define MISC_REG_XMAC_PHY_PORT_MODE				 0xa960
#define MSTAT_REG_RX_STAT_GR64_LO				 0x200
#define MSTAT_REG_TX_STAT_GTXPOK_LO				 0
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_BRCST	 (0x1<<0)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_MLCST	 (0x1<<1)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_NO_VLAN	 (0x1<<4)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_UNCST	 (0x1<<2)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_VLAN	 (0x1<<3)
#define NIG_LLH0_XCM_MASK_REG_LLH0_XCM_MASK_BCN			 (0x1<<0)
#define NIG_LLH1_XCM_MASK_REG_LLH1_XCM_MASK_BCN			 (0x1<<0)
#define NIG_MASK_INTERRUPT_PORT0_REG_MASK_EMAC0_MISC_MI_INT	 (0x1<<0)
#define NIG_MASK_INTERRUPT_PORT0_REG_MASK_SERDES0_LINK_STATUS	 (0x1<<9)
#define NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK10G 	 (0x1<<15)
#define NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK_STATUS	 (0xf<<18)
#define NIG_REG_BMAC0_IN_EN					 0x100ac
#define NIG_REG_BMAC0_OUT_EN					 0x100e0
#define NIG_REG_BMAC0_PAUSE_OUT_EN				 0x10110
#define NIG_REG_BMAC0_REGS_OUT_EN				 0x100e8
#define NIG_REG_BRB0_OUT_EN					 0x100f8
#define NIG_REG_BRB0_PAUSE_IN_EN				 0x100c4
#define NIG_REG_BRB1_OUT_EN					 0x100fc
#define NIG_REG_BRB1_PAUSE_IN_EN				 0x100c8
#define NIG_REG_BRB_LB_OUT_EN					 0x10100
#define NIG_REG_DEBUG_PACKET_LB 				 0x10800
#define NIG_REG_EGRESS_DEBUG_IN_EN				 0x100dc
#define NIG_REG_EGRESS_DRAIN0_MODE				 0x10060
#define NIG_REG_EGRESS_EMAC0_OUT_EN				 0x10120
#define NIG_REG_EGRESS_EMAC0_PORT				 0x10058
#define NIG_REG_EGRESS_PBF0_IN_EN				 0x100cc
#define NIG_REG_EGRESS_PBF1_IN_EN				 0x100d0
#define NIG_REG_EGRESS_UMP0_IN_EN				 0x100d4
#define NIG_REG_EMAC0_IN_EN					 0x100a4
#define NIG_REG_EMAC0_PAUSE_OUT_EN				 0x10118
#define NIG_REG_EMAC0_STATUS_MISC_MI_INT			 0x10494
#define NIG_REG_INGRESS_BMAC0_MEM				 0x10c00
#define NIG_REG_INGRESS_BMAC1_MEM				 0x11000
#define NIG_REG_INGRESS_EOP_LB_EMPTY				 0x104e0
#define NIG_REG_INGRESS_EOP_LB_FIFO				 0x104e4
#define NIG_REG_LATCH_BC_0					 0x16210
#define NIG_REG_LATCH_STATUS_0					 0x18000
#define NIG_REG_LED_10G_P0					 0x10320
#define NIG_REG_LED_10G_P1					 0x10324
#define NIG_REG_LED_CONTROL_BLINK_RATE_ENA_P0			 0x10318
#define NIG_REG_LED_CONTROL_BLINK_RATE_P0			 0x10310
#define NIG_REG_LED_CONTROL_BLINK_TRAFFIC_P0			 0x10308
#define NIG_REG_LED_CONTROL_OVERRIDE_TRAFFIC_P0 		 0x102f8
#define NIG_REG_LED_CONTROL_TRAFFIC_P0				 0x10300
#define NIG_REG_LED_MODE_P0					 0x102f0
#define NIG_REG_LLFC_EGRESS_SRC_ENABLE_0			 0x16070
#define NIG_REG_LLFC_EGRESS_SRC_ENABLE_1			 0x16074
#define NIG_REG_LLFC_ENABLE_0					 0x16208
#define NIG_REG_LLFC_ENABLE_1					 0x1620c
#define NIG_REG_LLFC_HIGH_PRIORITY_CLASSES_0			 0x16058
#define NIG_REG_LLFC_HIGH_PRIORITY_CLASSES_1			 0x1605c
#define NIG_REG_LLFC_LOW_PRIORITY_CLASSES_0			 0x16060
#define NIG_REG_LLFC_LOW_PRIORITY_CLASSES_1			 0x16064
#define NIG_REG_LLFC_OUT_EN_0					 0x160c8
#define NIG_REG_LLFC_OUT_EN_1					 0x160cc
#define NIG_REG_LLH0_ACPI_PAT_0_CRC				 0x1015c
#define NIG_REG_LLH0_ACPI_PAT_6_LEN				 0x10154
#define NIG_REG_LLH0_BRB1_DRV_MASK				 0x10244
#define NIG_REG_LLH0_BRB1_DRV_MASK_MF				 0x16048
#define NIG_REG_LLH0_BRB1_NOT_MCP				 0x1025c
#define NIG_REG_LLH0_CLS_TYPE					 0x16080
#define NIG_REG_LLH0_CM_HEADER					 0x1007c
#define NIG_REG_LLH0_DEST_IP_0_1				 0x101dc
#define NIG_REG_LLH0_DEST_MAC_0_0				 0x101c0
#define NIG_REG_LLH0_DEST_TCP_0 				 0x10220
#define NIG_REG_LLH0_DEST_UDP_0 				 0x10214
#define NIG_REG_LLH0_ERROR_MASK 				 0x1008c
#define NIG_REG_LLH0_EVENT_ID					 0x10084
#define NIG_REG_LLH0_FUNC_EN					 0x160fc
#define NIG_REG_LLH0_FUNC_MEM					 0x16180
#define NIG_REG_LLH0_FUNC_MEM_ENABLE				 0x16140
#define NIG_REG_LLH0_FUNC_VLAN_ID				 0x16100
#define NIG_REG_LLH0_IPV4_IPV6_0				 0x10208
#define NIG_REG_LLH0_T_BIT					 0x10074
#define NIG_REG_LLH0_VLAN_ID_0					 0x1022c
#define NIG_REG_LLH0_XCM_INIT_CREDIT				 0x10554
#define NIG_REG_LLH0_XCM_MASK					 0x10130
#define NIG_REG_LLH1_BRB1_DRV_MASK				 0x10248
#define NIG_REG_LLH1_BRB1_NOT_MCP				 0x102dc
#define NIG_REG_LLH1_CLS_TYPE					 0x16084
#define NIG_REG_LLH1_CM_HEADER					 0x10080
#define NIG_REG_LLH1_ERROR_MASK 				 0x10090
#define NIG_REG_LLH1_EVENT_ID					 0x10088
#define NIG_REG_LLH1_FUNC_MEM					 0x161c0
#define NIG_REG_LLH1_FUNC_MEM_ENABLE				 0x16160
#define NIG_REG_LLH1_FUNC_MEM_SIZE				 16
#define NIG_REG_LLH1_MF_MODE					 0x18614
#define NIG_REG_LLH1_XCM_INIT_CREDIT				 0x10564
#define NIG_REG_LLH1_XCM_MASK					 0x10134
#define NIG_REG_LLH_E1HOV_MODE					 0x160d8
#define NIG_REG_LLH_MF_MODE					 0x16024
#define NIG_REG_MASK_INTERRUPT_PORT0				 0x10330
#define NIG_REG_MASK_INTERRUPT_PORT1				 0x10334
#define NIG_REG_NIG_EMAC0_EN					 0x1003c
#define NIG_REG_NIG_EMAC1_EN					 0x10040
#define NIG_REG_NIG_INGRESS_EMAC0_NO_CRC			 0x10044
#define NIG_REG_NIG_INT_STS_0					 0x103b0
#define NIG_REG_NIG_INT_STS_1					 0x103c0
#define NIG_REG_NIG_PRTY_MASK					 0x103dc
#define NIG_REG_NIG_PRTY_MASK_0					 0x183c8
#define NIG_REG_NIG_PRTY_MASK_1					 0x183d8
#define NIG_REG_NIG_PRTY_STS					 0x103d0
#define NIG_REG_NIG_PRTY_STS_0					 0x183bc
#define NIG_REG_NIG_PRTY_STS_1					 0x183cc
#define NIG_REG_NIG_PRTY_STS_CLR				 0x103d4
#define NIG_REG_NIG_PRTY_STS_CLR_0				 0x183c0
#define NIG_REG_NIG_PRTY_STS_CLR_1				 0x183d0
#define MCPR_IMC_COMMAND_ENABLE					 (1L<<31)
#define MCPR_IMC_COMMAND_IMC_STATUS_BITSHIFT			 16
#define MCPR_IMC_COMMAND_OPERATION_BITSHIFT			 28
#define MCPR_IMC_COMMAND_TRANSFER_ADDRESS_BITSHIFT		 8
#define NIG_REG_P0_HDRS_AFTER_BASIC				 0x18038
#define NIG_REG_P0_HWPFC_ENABLE				 0x18078
#define NIG_REG_P0_LLH_FUNC_MEM2				 0x18480
#define NIG_REG_P0_LLH_FUNC_MEM2_ENABLE			 0x18440
#define NIG_REG_P0_MAC_IN_EN					 0x185ac
#define NIG_REG_P0_MAC_OUT_EN					 0x185b0
#define NIG_REG_P0_MAC_PAUSE_OUT_EN				 0x185b4
#define NIG_REG_P0_PKT_PRIORITY_TO_COS				 0x18054
#define NIG_REG_P0_RX_COS0_PRIORITY_MASK			 0x18058
#define NIG_REG_P0_RX_COS1_PRIORITY_MASK			 0x1805c
#define NIG_REG_P0_RX_COS2_PRIORITY_MASK			 0x186b0
#define NIG_REG_P0_RX_COS3_PRIORITY_MASK			 0x186b4
#define NIG_REG_P0_RX_COS4_PRIORITY_MASK			 0x186b8
#define NIG_REG_P0_RX_COS5_PRIORITY_MASK			 0x186bc
#define NIG_REG_P0_TX_ARB_CLIENT_CREDIT_MAP			 0x180f0
#define NIG_REG_P0_TX_ARB_CLIENT_CREDIT_MAP2_LSB		 0x18688
#define NIG_REG_P0_TX_ARB_CLIENT_CREDIT_MAP2_MSB		 0x1868c
#define NIG_REG_P0_TX_ARB_CLIENT_IS_STRICT			 0x180e8
#define NIG_REG_P0_TX_ARB_CLIENT_IS_SUBJECT2WFQ		 0x180ec
#define NIG_REG_P0_TX_ARB_CREDIT_UPPER_BOUND_0			 0x1810c
#define NIG_REG_P0_TX_ARB_CREDIT_UPPER_BOUND_1			 0x18110
#define NIG_REG_P0_TX_ARB_CREDIT_UPPER_BOUND_2			 0x18114
#define NIG_REG_P0_TX_ARB_CREDIT_UPPER_BOUND_3			 0x18118
#define NIG_REG_P0_TX_ARB_CREDIT_UPPER_BOUND_4			 0x1811c
#define NIG_REG_P0_TX_ARB_CREDIT_UPPER_BOUND_5			 0x186a0
#define NIG_REG_P0_TX_ARB_CREDIT_UPPER_BOUND_6			 0x186a4
#define NIG_REG_P0_TX_ARB_CREDIT_UPPER_BOUND_7			 0x186a8
#define NIG_REG_P0_TX_ARB_CREDIT_UPPER_BOUND_8			 0x186ac
#define NIG_REG_P0_TX_ARB_CREDIT_WEIGHT_0			 0x180f8
#define NIG_REG_P0_TX_ARB_CREDIT_WEIGHT_1			 0x180fc
#define NIG_REG_P0_TX_ARB_CREDIT_WEIGHT_2			 0x18100
#define NIG_REG_P0_TX_ARB_CREDIT_WEIGHT_3			 0x18104
#define NIG_REG_P0_TX_ARB_CREDIT_WEIGHT_4			 0x18108
#define NIG_REG_P0_TX_ARB_CREDIT_WEIGHT_5			 0x18690
#define NIG_REG_P0_TX_ARB_CREDIT_WEIGHT_6			 0x18694
#define NIG_REG_P0_TX_ARB_CREDIT_WEIGHT_7			 0x18698
#define NIG_REG_P0_TX_ARB_CREDIT_WEIGHT_8			 0x1869c
#define NIG_REG_P0_TX_ARB_NUM_STRICT_ARB_SLOTS			 0x180f4
#define NIG_REG_P0_TX_ARB_PRIORITY_CLIENT			 0x180e4
#define NIG_REG_P1_HDRS_AFTER_BASIC				 0x1818c
#define NIG_REG_P1_LLH_FUNC_MEM2				 0x184c0
#define NIG_REG_P1_LLH_FUNC_MEM2_ENABLE			 0x18460
#define NIG_REG_P0_TX_ARB_PRIORITY_CLIENT2_LSB			 0x18680
#define NIG_REG_P0_TX_ARB_PRIORITY_CLIENT2_MSB			 0x18684
#define NIG_REG_P1_HWPFC_ENABLE					 0x181d0
#define NIG_REG_P1_MAC_IN_EN					 0x185c0
#define NIG_REG_P1_MAC_OUT_EN					 0x185c4
#define NIG_REG_P1_MAC_PAUSE_OUT_EN				 0x185c8
#define NIG_REG_P1_PKT_PRIORITY_TO_COS				 0x181a8
#define NIG_REG_P1_RX_COS0_PRIORITY_MASK			 0x181ac
#define NIG_REG_P1_RX_COS1_PRIORITY_MASK			 0x181b0
#define NIG_REG_P1_RX_COS2_PRIORITY_MASK			 0x186f8
#define NIG_REG_P1_RX_MACFIFO_EMPTY				 0x1858c
#define NIG_REG_P1_TLLH_FIFO_EMPTY				 0x18338
#define NIG_REG_P1_TX_ARB_CLIENT_CREDIT_MAP2_LSB		 0x186e8
#define NIG_REG_P1_TX_ARB_CLIENT_CREDIT_MAP2_MSB		 0x186ec
#define NIG_REG_P1_TX_ARB_CLIENT_IS_STRICT			 0x18234
#define NIG_REG_P1_TX_ARB_CLIENT_IS_SUBJECT2WFQ			 0x18238
#define NIG_REG_P1_TX_ARB_CREDIT_UPPER_BOUND_0			 0x18258
#define NIG_REG_P1_TX_ARB_CREDIT_UPPER_BOUND_1			 0x1825c
#define NIG_REG_P1_TX_ARB_CREDIT_UPPER_BOUND_2			 0x18260
#define NIG_REG_P1_TX_ARB_CREDIT_UPPER_BOUND_3			 0x18264
#define NIG_REG_P1_TX_ARB_CREDIT_UPPER_BOUND_4			 0x18268
#define NIG_REG_P1_TX_ARB_CREDIT_UPPER_BOUND_5			 0x186f4
#define NIG_REG_P1_TX_ARB_CREDIT_WEIGHT_0			 0x18244
#define NIG_REG_P1_TX_ARB_CREDIT_WEIGHT_1			 0x18248
#define NIG_REG_P1_TX_ARB_CREDIT_WEIGHT_2			 0x1824c
#define NIG_REG_P1_TX_ARB_CREDIT_WEIGHT_3			 0x18250
#define NIG_REG_P1_TX_ARB_CREDIT_WEIGHT_4			 0x18254
#define NIG_REG_P1_TX_ARB_CREDIT_WEIGHT_5			 0x186f0
#define NIG_REG_P1_TX_ARB_NUM_STRICT_ARB_SLOTS			 0x18240
#define NIG_REG_P1_TX_ARB_PRIORITY_CLIENT2_LSB			 0x186e0
#define NIG_REG_P1_TX_ARB_PRIORITY_CLIENT2_MSB			 0x186e4
#define NIG_REG_P1_TX_MACFIFO_EMPTY				 0x18594
#define NIG_REG_P1_TX_MNG_HOST_FIFO_EMPTY			 0x182b8
#define NIG_REG_PAUSE_ENABLE_0					 0x160c0
#define NIG_REG_PAUSE_ENABLE_1					 0x160c4
#define NIG_REG_PBF_LB_IN_EN					 0x100b4
#define NIG_REG_PORT_SWAP					 0x10394
#define NIG_REG_PPP_ENABLE_0					 0x160b0
#define NIG_REG_PPP_ENABLE_1					 0x160b4
#define NIG_REG_PRS_EOP_OUT_EN					 0x10104
#define NIG_REG_PRS_REQ_IN_EN					 0x100b8
#define NIG_REG_SERDES0_CTRL_MD_DEVAD				 0x10370
#define NIG_REG_SERDES0_CTRL_MD_ST				 0x1036c
#define NIG_REG_SERDES0_CTRL_PHY_ADDR				 0x10374
#define NIG_REG_SERDES0_STATUS_LINK_STATUS			 0x10578
#define NIG_REG_STAT0_BRB_DISCARD				 0x105f0
#define NIG_REG_STAT0_BRB_TRUNCATE				 0x105f8
#define NIG_REG_STAT0_EGRESS_MAC_PKT0				 0x10750
#define NIG_REG_STAT0_EGRESS_MAC_PKT1				 0x10760
#define NIG_REG_STAT1_BRB_DISCARD				 0x10628
#define NIG_REG_STAT1_EGRESS_MAC_PKT0				 0x107a0
#define NIG_REG_STAT1_EGRESS_MAC_PKT1				 0x107b0
#define NIG_REG_STAT2_BRB_OCTET 				 0x107e0
#define NIG_REG_STATUS_INTERRUPT_PORT0				 0x10328
#define NIG_REG_STATUS_INTERRUPT_PORT1				 0x1032c
#define NIG_REG_STRAP_OVERRIDE					 0x10398
#define NIG_REG_XCM0_OUT_EN					 0x100f0
#define NIG_REG_XCM1_OUT_EN					 0x100f4
#define NIG_REG_XGXS0_CTRL_EXTREMOTEMDIOST			 0x10348
#define NIG_REG_XGXS0_CTRL_MD_DEVAD				 0x1033c
#define NIG_REG_XGXS0_CTRL_MD_ST				 0x10338
#define NIG_REG_XGXS0_CTRL_PHY_ADDR				 0x10340
#define NIG_REG_XGXS0_STATUS_LINK10G				 0x10680
#define NIG_REG_XGXS0_STATUS_LINK_STATUS			 0x10684
#define NIG_REG_XGXS_LANE_SEL_P0				 0x102e8
#define NIG_REG_XGXS_SERDES0_MODE_SEL				 0x102e0
#define NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_EMAC0_MISC_MI_INT  (0x1<<0)
#define NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_SERDES0_LINK_STATUS (0x1<<9)
#define NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK10G	 (0x1<<15)
#define NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK_STATUS  (0xf<<18)
#define NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK_STATUS_SIZE 18
#define PBF_REG_COS0_UPPER_BOUND				 0x15c05c
#define PBF_REG_COS0_UPPER_BOUND_P0				 0x15c2cc
#define PBF_REG_COS0_UPPER_BOUND_P1				 0x15c2e4
#define PBF_REG_COS0_WEIGHT					 0x15c054
#define PBF_REG_COS0_WEIGHT_P0					 0x15c2a8
#define PBF_REG_COS0_WEIGHT_P1					 0x15c2c0
#define PBF_REG_COS1_UPPER_BOUND				 0x15c060
#define PBF_REG_COS1_WEIGHT					 0x15c058
#define PBF_REG_COS1_WEIGHT_P0					 0x15c2ac
#define PBF_REG_COS1_WEIGHT_P1					 0x15c2c4
#define PBF_REG_COS2_WEIGHT_P0					 0x15c2b0
#define PBF_REG_COS2_WEIGHT_P1					 0x15c2c8
#define PBF_REG_COS3_WEIGHT_P0					 0x15c2b4
#define PBF_REG_COS4_WEIGHT_P0					 0x15c2b8
#define PBF_REG_COS5_WEIGHT_P0					 0x15c2bc
#define PBF_REG_CREDIT_LB_Q					 0x140338
#define PBF_REG_CREDIT_Q0					 0x14033c
#define PBF_REG_CREDIT_Q1					 0x140340
#define PBF_REG_DISABLE_NEW_TASK_PROC_P0			 0x14005c
#define PBF_REG_DISABLE_NEW_TASK_PROC_P1			 0x140060
#define PBF_REG_DISABLE_NEW_TASK_PROC_P4			 0x14006c
#define PBF_REG_DISABLE_PF					 0x1402e8
#define PBF_REG_ETS_ARB_CLIENT_CREDIT_MAP_P0			 0x15c288
#define PBF_REG_ETS_ARB_CLIENT_CREDIT_MAP_P1			 0x15c28c
#define PBF_REG_ETS_ARB_CLIENT_IS_STRICT_P0			 0x15c278
#define PBF_REG_ETS_ARB_CLIENT_IS_STRICT_P1			 0x15c27c
#define PBF_REG_ETS_ARB_CLIENT_IS_SUBJECT2WFQ_P0		 0x15c280
#define PBF_REG_ETS_ARB_CLIENT_IS_SUBJECT2WFQ_P1		 0x15c284
#define PBF_REG_ETS_ARB_NUM_STRICT_ARB_SLOTS_P0			 0x15c2a0
#define PBF_REG_ETS_ARB_NUM_STRICT_ARB_SLOTS_P1			 0x15c2a4
#define PBF_REG_ETS_ARB_PRIORITY_CLIENT_P0			 0x15c270
#define PBF_REG_ETS_ARB_PRIORITY_CLIENT_P1			 0x15c274
#define PBF_REG_ETS_ENABLED					 0x15c050
#define PBF_REG_HDRS_AFTER_BASIC				 0x15c0a8
#define PBF_REG_HDRS_AFTER_TAG_0				 0x15c0b8
#define PBF_REG_HIGH_PRIORITY_COS_NUM				 0x15c04c
#define PBF_REG_IF_ENABLE_REG					 0x140044
#define PBF_REG_INIT						 0x140000
#define PBF_REG_INIT_CRD_LB_Q					 0x15c248
#define PBF_REG_INIT_CRD_Q0					 0x15c230
#define PBF_REG_INIT_CRD_Q1					 0x15c234
#define PBF_REG_INIT_P0 					 0x140004
#define PBF_REG_INIT_P1 					 0x140008
#define PBF_REG_INIT_P4 					 0x14000c
#define PBF_REG_INTERNAL_CRD_FREED_CNT_LB_Q			 0x140354
#define PBF_REG_INTERNAL_CRD_FREED_CNT_Q0			 0x140358
#define PBF_REG_INTERNAL_CRD_FREED_CNT_Q1			 0x14035c
#define PBF_REG_MAC_IF0_ENABLE					 0x140030
#define PBF_REG_MAC_IF1_ENABLE					 0x140034
#define PBF_REG_MAC_LB_ENABLE					 0x140040
#define PBF_REG_MUST_HAVE_HDRS					 0x15c0c4
#define PBF_REG_NUM_STRICT_ARB_SLOTS				 0x15c064
#define PBF_REG_P0_ARB_THRSH					 0x1400e4
#define PBF_REG_P0_CREDIT					 0x140200
#define PBF_REG_P0_INIT_CRD					 0x1400d0
#define PBF_REG_P0_INTERNAL_CRD_FREED_CNT			 0x140308
#define PBF_REG_P0_PAUSE_ENABLE					 0x140014
#define PBF_REG_P0_TASK_CNT					 0x140204
#define PBF_REG_P0_TQ_LINES_FREED_CNT				 0x1402f0
#define PBF_REG_P0_TQ_OCCUPANCY					 0x1402fc
#define PBF_REG_P1_CREDIT					 0x140208
#define PBF_REG_P1_INIT_CRD					 0x1400d4
#define PBF_REG_P1_INTERNAL_CRD_FREED_CNT			 0x14030c
#define PBF_REG_P1_TASK_CNT					 0x14020c
#define PBF_REG_P1_TQ_LINES_FREED_CNT				 0x1402f4
#define PBF_REG_P1_TQ_OCCUPANCY					 0x140300
#define PBF_REG_P4_CREDIT					 0x140210
#define PBF_REG_P4_INIT_CRD					 0x1400e0
#define PBF_REG_P4_INTERNAL_CRD_FREED_CNT			 0x140310
#define PBF_REG_P4_TASK_CNT					 0x140214
#define PBF_REG_P4_TQ_LINES_FREED_CNT				 0x1402f8
#define PBF_REG_P4_TQ_OCCUPANCY					 0x140304
#define PBF_REG_PBF_INT_MASK					 0x1401d4
#define PBF_REG_PBF_INT_STS					 0x1401c8
#define PBF_REG_PBF_PRTY_MASK					 0x1401e4
#define PBF_REG_PBF_PRTY_STS_CLR				 0x1401dc
#define PBF_REG_TAG_ETHERTYPE_0					 0x15c090
#define PBF_REG_TAG_LEN_0					 0x15c09c
#define PBF_REG_TQ_LINES_FREED_CNT_LB_Q				 0x14038c
#define PBF_REG_TQ_LINES_FREED_CNT_Q0				 0x140390
#define PBF_REG_TQ_LINES_FREED_CNT_Q1				 0x140394
#define PBF_REG_TQ_OCCUPANCY_LB_Q				 0x1403a8
#define PBF_REG_TQ_OCCUPANCY_Q0					 0x1403ac
#define PBF_REG_TQ_OCCUPANCY_Q1					 0x1403b0
#define PB_REG_CONTROL						 0
#define PB_REG_PB_INT_MASK					 0x28
#define PB_REG_PB_INT_STS					 0x1c
#define PB_REG_PB_PRTY_MASK					 0x38
#define PB_REG_PB_PRTY_STS					 0x2c
#define PB_REG_PB_PRTY_STS_CLR					 0x30
#define PGLUE_B_PGLUE_B_INT_STS_REG_ADDRESS_ERROR		 (0x1<<0)
#define PGLUE_B_PGLUE_B_INT_STS_REG_CSSNOOP_FIFO_OVERFLOW	 (0x1<<8)
#define PGLUE_B_PGLUE_B_INT_STS_REG_INCORRECT_RCV_BEHAVIOR	 (0x1<<1)
#define PGLUE_B_PGLUE_B_INT_STS_REG_TCPL_ERROR_ATTN		 (0x1<<6)
#define PGLUE_B_PGLUE_B_INT_STS_REG_TCPL_IN_TWO_RCBS_ATTN	 (0x1<<7)
#define PGLUE_B_PGLUE_B_INT_STS_REG_VF_GRC_SPACE_VIOLATION_ATTN  (0x1<<4)
#define PGLUE_B_PGLUE_B_INT_STS_REG_VF_LENGTH_VIOLATION_ATTN	 (0x1<<3)
#define PGLUE_B_PGLUE_B_INT_STS_REG_VF_MSIX_BAR_VIOLATION_ATTN	 (0x1<<5)
#define PGLUE_B_PGLUE_B_INT_STS_REG_WAS_ERROR_ATTN		 (0x1<<2)
#define PGLUE_B_REG_CFG_SPACE_A_REQUEST			 0x9010
#define PGLUE_B_REG_CFG_SPACE_B_REQUEST			 0x9014
#define PGLUE_B_REG_CSDM_INB_INT_A_PF_ENABLE			 0x9194
#define PGLUE_B_REG_CSDM_INB_INT_B_VF				 0x916c
#define PGLUE_B_REG_CSDM_INB_INT_B_VF_ENABLE			 0x919c
#define PGLUE_B_REG_CSDM_START_OFFSET_A			 0x9100
#define PGLUE_B_REG_CSDM_START_OFFSET_B			 0x9108
#define PGLUE_B_REG_CSDM_VF_SHIFT_B				 0x9110
#define PGLUE_B_REG_CSDM_ZONE_A_SIZE_PF			 0x91ac
#define PGLUE_B_REG_FLR_REQUEST_PF_7_0				 0x9028
#define PGLUE_B_REG_FLR_REQUEST_PF_7_0_CLR			 0x9418
#define PGLUE_B_REG_FLR_REQUEST_VF_127_96			 0x9024
#define PGLUE_B_REG_FLR_REQUEST_VF_31_0			 0x9018
#define PGLUE_B_REG_FLR_REQUEST_VF_63_32			 0x901c
#define PGLUE_B_REG_FLR_REQUEST_VF_95_64			 0x9020
#define PGLUE_B_REG_INCORRECT_RCV_DETAILS			 0x9068
#define PGLUE_B_REG_INTERNAL_PFID_ENABLE_MASTER		 0x942c
#define PGLUE_B_REG_INTERNAL_PFID_ENABLE_TARGET_READ		 0x9430
#define PGLUE_B_REG_INTERNAL_PFID_ENABLE_TARGET_WRITE		 0x9434
#define PGLUE_B_REG_INTERNAL_VFID_ENABLE			 0x9438
#define PGLUE_B_REG_PGLUE_B_INT_STS				 0x9298
#define PGLUE_B_REG_PGLUE_B_INT_STS_CLR			 0x929c
#define PGLUE_B_REG_PGLUE_B_PRTY_MASK				 0x92b4
#define PGLUE_B_REG_PGLUE_B_PRTY_STS				 0x92a8
#define PGLUE_B_REG_PGLUE_B_PRTY_STS_CLR			 0x92ac
#define PGLUE_B_REG_RX_ERR_DETAILS				 0x9080
#define PGLUE_B_REG_RX_TCPL_ERR_DETAILS			 0x9084
#define PGLUE_B_REG_SHADOW_BME_PF_7_0_CLR			 0x9458
/* [R 8] SR IOV disabled attention dirty bits. Each bit indicates that the
 * VF enable register of the corresponding PF is written to 0 and was
 * previously 1. Set by PXP. Reset by MCP writing 1 to
 * sr_iov_disabled_request_clr. Note: register contains bits from both
 * paths. */
#define PGLUE_B_REG_SR_IOV_DISABLED_REQUEST			 0x9030
#define PGLUE_B_REG_TAGS_63_32					 0x9244
#define PGLUE_B_REG_TSDM_INB_INT_A_PF_ENABLE			 0x9170
#define PGLUE_B_REG_TSDM_START_OFFSET_A			 0x90c4
#define PGLUE_B_REG_TSDM_START_OFFSET_B			 0x90cc
#define PGLUE_B_REG_TSDM_VF_SHIFT_B				 0x90d4
#define PGLUE_B_REG_TSDM_ZONE_A_SIZE_PF			 0x91a0
#define PGLUE_B_REG_TX_ERR_RD_ADD_31_0				 0x9098
#define PGLUE_B_REG_TX_ERR_RD_ADD_63_32			 0x909c
#define PGLUE_B_REG_TX_ERR_RD_DETAILS				 0x90a0
#define PGLUE_B_REG_TX_ERR_RD_DETAILS2				 0x90a4
#define PGLUE_B_REG_TX_ERR_WR_ADD_31_0				 0x9088
#define PGLUE_B_REG_TX_ERR_WR_ADD_63_32			 0x908c
#define PGLUE_B_REG_TX_ERR_WR_DETAILS				 0x9090
#define PGLUE_B_REG_TX_ERR_WR_DETAILS2				 0x9094
#define PGLUE_B_REG_USDM_INB_INT_A_0				 0x9128
#define PGLUE_B_REG_USDM_INB_INT_A_1				 0x912c
#define PGLUE_B_REG_USDM_INB_INT_A_2				 0x9130
#define PGLUE_B_REG_USDM_INB_INT_A_3				 0x9134
#define PGLUE_B_REG_USDM_INB_INT_A_4				 0x9138
#define PGLUE_B_REG_USDM_INB_INT_A_5				 0x913c
#define PGLUE_B_REG_USDM_INB_INT_A_6				 0x9140
#define PGLUE_B_REG_USDM_INB_INT_A_PF_ENABLE			 0x917c
#define PGLUE_B_REG_USDM_INB_INT_A_VF_ENABLE			 0x9180
#define PGLUE_B_REG_USDM_INB_INT_B_VF_ENABLE			 0x9184
#define PGLUE_B_REG_USDM_START_OFFSET_A			 0x90d8
#define PGLUE_B_REG_USDM_START_OFFSET_B			 0x90e0
#define PGLUE_B_REG_USDM_VF_SHIFT_B				 0x90e8
#define PGLUE_B_REG_USDM_ZONE_A_SIZE_PF			 0x91a4
#define PGLUE_B_REG_VF_GRC_SPACE_VIOLATION_DETAILS		 0x9234
#define PGLUE_B_REG_VF_LENGTH_VIOLATION_DETAILS		 0x9230
#define PGLUE_B_REG_WAS_ERROR_PF_7_0				 0x907c
#define PGLUE_B_REG_WAS_ERROR_PF_7_0_CLR			 0x9470
#define PGLUE_B_REG_WAS_ERROR_VF_127_96			 0x9078
#define PGLUE_B_REG_WAS_ERROR_VF_127_96_CLR			 0x9474
#define PGLUE_B_REG_WAS_ERROR_VF_31_0				 0x906c
#define PGLUE_B_REG_WAS_ERROR_VF_31_0_CLR			 0x9478
#define PGLUE_B_REG_WAS_ERROR_VF_63_32				 0x9070
#define PGLUE_B_REG_WAS_ERROR_VF_63_32_CLR			 0x947c
#define PGLUE_B_REG_WAS_ERROR_VF_95_64				 0x9074
#define PGLUE_B_REG_WAS_ERROR_VF_95_64_CLR			 0x9480
#define PGLUE_B_REG_XSDM_INB_INT_A_PF_ENABLE			 0x9188
#define PGLUE_B_REG_XSDM_START_OFFSET_A			 0x90ec
#define PGLUE_B_REG_XSDM_START_OFFSET_B			 0x90f4
#define PGLUE_B_REG_XSDM_VF_SHIFT_B				 0x90fc
#define PGLUE_B_REG_XSDM_ZONE_A_SIZE_PF			 0x91a8
#define PRS_REG_A_PRSU_20					 0x40134
#define PRS_REG_CFC_LD_CURRENT_CREDIT				 0x40164
#define PRS_REG_CFC_SEARCH_CURRENT_CREDIT			 0x40168
#define PRS_REG_CFC_SEARCH_INITIAL_CREDIT			 0x4011c
#define PRS_REG_CID_PORT_0					 0x400fc
#define PRS_REG_CM_HDR_FLUSH_LOAD_TYPE_0			 0x400dc
#define PRS_REG_CM_HDR_FLUSH_LOAD_TYPE_1			 0x400e0
#define PRS_REG_CM_HDR_FLUSH_LOAD_TYPE_2			 0x400e4
#define PRS_REG_CM_HDR_FLUSH_LOAD_TYPE_3			 0x400e8
#define PRS_REG_CM_HDR_FLUSH_LOAD_TYPE_4			 0x400ec
#define PRS_REG_CM_HDR_FLUSH_LOAD_TYPE_5			 0x400f0
#define PRS_REG_CM_HDR_FLUSH_NO_LOAD_TYPE_0			 0x400bc
#define PRS_REG_CM_HDR_FLUSH_NO_LOAD_TYPE_1			 0x400c0
#define PRS_REG_CM_HDR_FLUSH_NO_LOAD_TYPE_2			 0x400c4
#define PRS_REG_CM_HDR_FLUSH_NO_LOAD_TYPE_3			 0x400c8
#define PRS_REG_CM_HDR_FLUSH_NO_LOAD_TYPE_4			 0x400cc
#define PRS_REG_CM_HDR_FLUSH_NO_LOAD_TYPE_5			 0x400d0
#define PRS_REG_CM_HDR_LOOPBACK_TYPE_1				 0x4009c
#define PRS_REG_CM_HDR_LOOPBACK_TYPE_2				 0x400a0
#define PRS_REG_CM_HDR_LOOPBACK_TYPE_3				 0x400a4
#define PRS_REG_CM_HDR_LOOPBACK_TYPE_4				 0x400a8
#define PRS_REG_CM_HDR_TYPE_0					 0x40078
#define PRS_REG_CM_HDR_TYPE_1					 0x4007c
#define PRS_REG_CM_HDR_TYPE_2					 0x40080
#define PRS_REG_CM_HDR_TYPE_3					 0x40084
#define PRS_REG_CM_HDR_TYPE_4					 0x40088
#define PRS_REG_CM_NO_MATCH_HDR 				 0x400b8
#define PRS_REG_E1HOV_MODE					 0x401c8
#define PRS_REG_EVENT_ID_1					 0x40054
#define PRS_REG_EVENT_ID_2					 0x40058
#define PRS_REG_EVENT_ID_3					 0x4005c
#define PRS_REG_FCOE_TYPE					 0x401d0
#define PRS_REG_FLUSH_REGIONS_TYPE_0				 0x40004
#define PRS_REG_FLUSH_REGIONS_TYPE_1				 0x40008
#define PRS_REG_FLUSH_REGIONS_TYPE_2				 0x4000c
#define PRS_REG_FLUSH_REGIONS_TYPE_3				 0x40010
#define PRS_REG_FLUSH_REGIONS_TYPE_4				 0x40014
#define PRS_REG_FLUSH_REGIONS_TYPE_5				 0x40018
#define PRS_REG_FLUSH_REGIONS_TYPE_6				 0x4001c
#define PRS_REG_FLUSH_REGIONS_TYPE_7				 0x40020
#define PRS_REG_HDRS_AFTER_BASIC				 0x40238
#define PRS_REG_HDRS_AFTER_BASIC_PORT_0				 0x40270
#define PRS_REG_HDRS_AFTER_BASIC_PORT_1				 0x40290
#define PRS_REG_HDRS_AFTER_TAG_0				 0x40248
#define PRS_REG_HDRS_AFTER_TAG_0_PORT_0				 0x40280
#define PRS_REG_HDRS_AFTER_TAG_0_PORT_1				 0x402a0
#define PRS_REG_INC_VALUE					 0x40048
#define PRS_REG_MUST_HAVE_HDRS					 0x40254
#define PRS_REG_MUST_HAVE_HDRS_PORT_0				 0x4028c
#define PRS_REG_MUST_HAVE_HDRS_PORT_1				 0x402ac
#define PRS_REG_NIC_MODE					 0x40138
#define PRS_REG_NO_MATCH_EVENT_ID				 0x40070
#define PRS_REG_NUM_OF_CFC_FLUSH_MESSAGES			 0x40128
#define PRS_REG_NUM_OF_DEAD_CYCLES				 0x40130
#define PRS_REG_NUM_OF_PACKETS					 0x40124
#define PRS_REG_NUM_OF_TRANSPARENT_FLUSH_MESSAGES		 0x4012c
#define PRS_REG_PACKET_REGIONS_TYPE_0				 0x40028
#define PRS_REG_PACKET_REGIONS_TYPE_1				 0x4002c
#define PRS_REG_PACKET_REGIONS_TYPE_2				 0x40030
#define PRS_REG_PACKET_REGIONS_TYPE_3				 0x40034
#define PRS_REG_PACKET_REGIONS_TYPE_4				 0x40038
#define PRS_REG_PACKET_REGIONS_TYPE_5				 0x4003c
#define PRS_REG_PACKET_REGIONS_TYPE_6				 0x40040
#define PRS_REG_PACKET_REGIONS_TYPE_7				 0x40044
#define PRS_REG_PENDING_BRB_CAC0_RQ				 0x40174
#define PRS_REG_PENDING_BRB_PRS_RQ				 0x40170
#define PRS_REG_PRS_INT_STS					 0x40188
#define PRS_REG_PRS_PRTY_MASK					 0x401a4
#define PRS_REG_PRS_PRTY_STS					 0x40198
#define PRS_REG_PRS_PRTY_STS_CLR				 0x4019c
#define PRS_REG_PURE_REGIONS					 0x40024
#define PRS_REG_SERIAL_NUM_STATUS_LSB				 0x40154
#define PRS_REG_SERIAL_NUM_STATUS_MSB				 0x40158
#define PRS_REG_SRC_CURRENT_CREDIT				 0x4016c
#define PRS_REG_TAG_ETHERTYPE_0					 0x401d4
#define PRS_REG_TAG_LEN_0					 0x4022c
#define PRS_REG_TCM_CURRENT_CREDIT				 0x40160
#define PRS_REG_TSDM_CURRENT_CREDIT				 0x4015c
#define PXP2_PXP2_INT_MASK_0_REG_PGL_CPL_AFT			 (0x1<<19)
#define PXP2_PXP2_INT_MASK_0_REG_PGL_CPL_OF			 (0x1<<20)
#define PXP2_PXP2_INT_MASK_0_REG_PGL_PCIE_ATTN			 (0x1<<22)
#define PXP2_PXP2_INT_MASK_0_REG_PGL_READ_BLOCKED		 (0x1<<23)
#define PXP2_PXP2_INT_MASK_0_REG_PGL_WRITE_BLOCKED		 (0x1<<24)
#define PXP2_PXP2_INT_STS_0_REG_WR_PGLUE_EOP_ERROR		 (0x1<<7)
#define PXP2_PXP2_INT_STS_CLR_0_REG_WR_PGLUE_EOP_ERROR		 (0x1<<7)
#define PXP2_REG_HST_DATA_FIFO_STATUS				 0x12047c
#define PXP2_REG_HST_HEADER_FIFO_STATUS				 0x120478
#define PXP2_REG_PGL_ADDR_88_F0					 0x120534
#define PXP2_REG_PGL_ADDR_88_F1					 0x120544
#define PXP2_REG_PGL_ADDR_8C_F0					 0x120538
#define PXP2_REG_PGL_ADDR_8C_F1					 0x120548
#define PXP2_REG_PGL_ADDR_90_F0					 0x12053c
#define PXP2_REG_PGL_ADDR_90_F1					 0x12054c
#define PXP2_REG_PGL_ADDR_94_F0					 0x120540
#define PXP2_REG_PGL_ADDR_94_F1					 0x120550
#define PXP2_REG_PGL_CONTROL0					 0x120490
#define PXP2_REG_PGL_CONTROL1					 0x120514
#define PXP2_REG_PGL_DEBUG					 0x120520
#define PXP2_REG_PGL_EXP_ROM2					 0x120808
#define PXP2_REG_PGL_INT_CSDM_0 				 0x1204f4
#define PXP2_REG_PGL_INT_CSDM_1 				 0x1204f8
#define PXP2_REG_PGL_INT_CSDM_2 				 0x1204fc
#define PXP2_REG_PGL_INT_CSDM_3 				 0x120500
#define PXP2_REG_PGL_INT_CSDM_4 				 0x120504
#define PXP2_REG_PGL_INT_CSDM_5 				 0x120508
#define PXP2_REG_PGL_INT_CSDM_6 				 0x12050c
#define PXP2_REG_PGL_INT_CSDM_7 				 0x120510
#define PXP2_REG_PGL_INT_TSDM_0 				 0x120494
#define PXP2_REG_PGL_INT_TSDM_1 				 0x120498
#define PXP2_REG_PGL_INT_TSDM_2 				 0x12049c
#define PXP2_REG_PGL_INT_TSDM_3 				 0x1204a0
#define PXP2_REG_PGL_INT_TSDM_4 				 0x1204a4
#define PXP2_REG_PGL_INT_TSDM_5 				 0x1204a8
#define PXP2_REG_PGL_INT_TSDM_6 				 0x1204ac
#define PXP2_REG_PGL_INT_TSDM_7 				 0x1204b0
#define PXP2_REG_PGL_INT_USDM_0 				 0x1204b4
#define PXP2_REG_PGL_INT_USDM_1 				 0x1204b8
#define PXP2_REG_PGL_INT_USDM_2 				 0x1204bc
#define PXP2_REG_PGL_INT_USDM_3 				 0x1204c0
#define PXP2_REG_PGL_INT_USDM_4 				 0x1204c4
#define PXP2_REG_PGL_INT_USDM_5 				 0x1204c8
#define PXP2_REG_PGL_INT_USDM_6 				 0x1204cc
#define PXP2_REG_PGL_INT_USDM_7 				 0x1204d0
#define PXP2_REG_PGL_INT_XSDM_0 				 0x1204d4
#define PXP2_REG_PGL_INT_XSDM_1 				 0x1204d8
#define PXP2_REG_PGL_INT_XSDM_2 				 0x1204dc
#define PXP2_REG_PGL_INT_XSDM_3 				 0x1204e0
#define PXP2_REG_PGL_INT_XSDM_4 				 0x1204e4
#define PXP2_REG_PGL_INT_XSDM_5 				 0x1204e8
#define PXP2_REG_PGL_INT_XSDM_6 				 0x1204ec
#define PXP2_REG_PGL_INT_XSDM_7 				 0x1204f0
#define PXP2_REG_PGL_PRETEND_FUNC_F0				 0x120674
#define PXP2_REG_PGL_PRETEND_FUNC_F1				 0x120678
#define PXP2_REG_PGL_PRETEND_FUNC_F2				 0x12067c
#define PXP2_REG_PGL_PRETEND_FUNC_F3				 0x120680
#define PXP2_REG_PGL_PRETEND_FUNC_F4				 0x120684
#define PXP2_REG_PGL_PRETEND_FUNC_F5				 0x120688
#define PXP2_REG_PGL_PRETEND_FUNC_F6				 0x12068c
#define PXP2_REG_PGL_PRETEND_FUNC_F7				 0x120690
#define PXP2_REG_PGL_READ_BLOCKED				 0x120568
#define PXP2_REG_PGL_TAGS_LIMIT 				 0x1205a8
#define PXP2_REG_PGL_TXW_CDTS					 0x12052c
#define PXP2_REG_PGL_WRITE_BLOCKED				 0x120564
#define PXP2_REG_PSWRQ_BW_ADD1					 0x1201c0
#define PXP2_REG_PSWRQ_BW_ADD10 				 0x1201e4
#define PXP2_REG_PSWRQ_BW_ADD11 				 0x1201e8
#define PXP2_REG_PSWRQ_BW_ADD2					 0x1201c4
#define PXP2_REG_PSWRQ_BW_ADD28 				 0x120228
#define PXP2_REG_PSWRQ_BW_ADD3					 0x1201c8
#define PXP2_REG_PSWRQ_BW_ADD6					 0x1201d4
#define PXP2_REG_PSWRQ_BW_ADD7					 0x1201d8
#define PXP2_REG_PSWRQ_BW_ADD8					 0x1201dc
#define PXP2_REG_PSWRQ_BW_ADD9					 0x1201e0
#define PXP2_REG_PSWRQ_BW_CREDIT				 0x12032c
#define PXP2_REG_PSWRQ_BW_L1					 0x1202b0
#define PXP2_REG_PSWRQ_BW_L10					 0x1202d4
#define PXP2_REG_PSWRQ_BW_L11					 0x1202d8
#define PXP2_REG_PSWRQ_BW_L2					 0x1202b4
#define PXP2_REG_PSWRQ_BW_L28					 0x120318
#define PXP2_REG_PSWRQ_BW_L3					 0x1202b8
#define PXP2_REG_PSWRQ_BW_L6					 0x1202c4
#define PXP2_REG_PSWRQ_BW_L7					 0x1202c8
#define PXP2_REG_PSWRQ_BW_L8					 0x1202cc
#define PXP2_REG_PSWRQ_BW_L9					 0x1202d0
#define PXP2_REG_PSWRQ_BW_RD					 0x120324
#define PXP2_REG_PSWRQ_BW_UB1					 0x120238
#define PXP2_REG_PSWRQ_BW_UB10					 0x12025c
#define PXP2_REG_PSWRQ_BW_UB11					 0x120260
#define PXP2_REG_PSWRQ_BW_UB2					 0x12023c
#define PXP2_REG_PSWRQ_BW_UB28					 0x1202a0
#define PXP2_REG_PSWRQ_BW_UB3					 0x120240
#define PXP2_REG_PSWRQ_BW_UB6					 0x12024c
#define PXP2_REG_PSWRQ_BW_UB7					 0x120250
#define PXP2_REG_PSWRQ_BW_UB8					 0x120254
#define PXP2_REG_PSWRQ_BW_UB9					 0x120258
#define PXP2_REG_PSWRQ_BW_WR					 0x120328
#define PXP2_REG_PSWRQ_CDU0_L2P 				 0x120000
#define PXP2_REG_PSWRQ_QM0_L2P					 0x120038
#define PXP2_REG_PSWRQ_SRC0_L2P 				 0x120054
#define PXP2_REG_PSWRQ_TM0_L2P					 0x12001c
#define PXP2_REG_PSWRQ_TSDM0_L2P				 0x1200e0
#define PXP2_REG_PXP2_INT_MASK_0				 0x120578
#define PXP2_REG_PXP2_INT_STS_0 				 0x12056c
#define PXP2_REG_PXP2_INT_STS_1 				 0x120608
#define PXP2_REG_PXP2_INT_STS_CLR_0				 0x120570
#define PXP2_REG_PXP2_PRTY_MASK_0				 0x120588
#define PXP2_REG_PXP2_PRTY_MASK_1				 0x120598
#define PXP2_REG_PXP2_PRTY_STS_0				 0x12057c
#define PXP2_REG_PXP2_PRTY_STS_1				 0x12058c
#define PXP2_REG_PXP2_PRTY_STS_CLR_0				 0x120580
#define PXP2_REG_PXP2_PRTY_STS_CLR_1				 0x120590
#define PXP2_REG_RD_ALMOST_FULL_0				 0x120424
#define PXP2_REG_RD_BLK_CNT					 0x120418
#define PXP2_REG_RD_BLK_NUM_CFG 				 0x12040c
#define PXP2_REG_RD_CDURD_SWAP_MODE				 0x120404
#define PXP2_REG_RD_DISABLE_INPUTS				 0x120374
#define PXP2_REG_RD_INIT_DONE					 0x120370
#define PXP2_REG_RD_MAX_BLKS_VQ10				 0x1203a0
#define PXP2_REG_RD_MAX_BLKS_VQ11				 0x1203a4
#define PXP2_REG_RD_MAX_BLKS_VQ17				 0x1203bc
#define PXP2_REG_RD_MAX_BLKS_VQ18				 0x1203c0
#define PXP2_REG_RD_MAX_BLKS_VQ19				 0x1203c4
#define PXP2_REG_RD_MAX_BLKS_VQ22				 0x1203d0
#define PXP2_REG_RD_MAX_BLKS_VQ25				 0x1203dc
#define PXP2_REG_RD_MAX_BLKS_VQ6				 0x120390
#define PXP2_REG_RD_MAX_BLKS_VQ9				 0x12039c
#define PXP2_REG_RD_PBF_SWAP_MODE				 0x1203f4
#define PXP2_REG_RD_PORT_IS_IDLE_0				 0x12041c
#define PXP2_REG_RD_PORT_IS_IDLE_1				 0x120420
#define PXP2_REG_RD_QM_SWAP_MODE				 0x1203f8
#define PXP2_REG_RD_SR_CNT					 0x120414
#define PXP2_REG_RD_SRC_SWAP_MODE				 0x120400
#define PXP2_REG_RD_SR_NUM_CFG					 0x120408
#define PXP2_REG_RD_START_INIT					 0x12036c
#define PXP2_REG_RD_TM_SWAP_MODE				 0x1203fc
#define PXP2_REG_RQ_BW_RD_ADD0					 0x1201bc
#define PXP2_REG_RQ_BW_RD_ADD12 				 0x1201ec
#define PXP2_REG_RQ_BW_RD_ADD13 				 0x1201f0
#define PXP2_REG_RQ_BW_RD_ADD14 				 0x1201f4
#define PXP2_REG_RQ_BW_RD_ADD15 				 0x1201f8
#define PXP2_REG_RQ_BW_RD_ADD16 				 0x1201fc
#define PXP2_REG_RQ_BW_RD_ADD17 				 0x120200
#define PXP2_REG_RQ_BW_RD_ADD18 				 0x120204
#define PXP2_REG_RQ_BW_RD_ADD19 				 0x120208
#define PXP2_REG_RQ_BW_RD_ADD20 				 0x12020c
#define PXP2_REG_RQ_BW_RD_ADD22 				 0x120210
#define PXP2_REG_RQ_BW_RD_ADD23 				 0x120214
#define PXP2_REG_RQ_BW_RD_ADD24 				 0x120218
#define PXP2_REG_RQ_BW_RD_ADD25 				 0x12021c
#define PXP2_REG_RQ_BW_RD_ADD26 				 0x120220
#define PXP2_REG_RQ_BW_RD_ADD27 				 0x120224
#define PXP2_REG_RQ_BW_RD_ADD4					 0x1201cc
#define PXP2_REG_RQ_BW_RD_ADD5					 0x1201d0
#define PXP2_REG_RQ_BW_RD_L0					 0x1202ac
#define PXP2_REG_RQ_BW_RD_L12					 0x1202dc
#define PXP2_REG_RQ_BW_RD_L13					 0x1202e0
#define PXP2_REG_RQ_BW_RD_L14					 0x1202e4
#define PXP2_REG_RQ_BW_RD_L15					 0x1202e8
#define PXP2_REG_RQ_BW_RD_L16					 0x1202ec
#define PXP2_REG_RQ_BW_RD_L17					 0x1202f0
#define PXP2_REG_RQ_BW_RD_L18					 0x1202f4
#define PXP2_REG_RQ_BW_RD_L19					 0x1202f8
#define PXP2_REG_RQ_BW_RD_L20					 0x1202fc
#define PXP2_REG_RQ_BW_RD_L22					 0x120300
#define PXP2_REG_RQ_BW_RD_L23					 0x120304
#define PXP2_REG_RQ_BW_RD_L24					 0x120308
#define PXP2_REG_RQ_BW_RD_L25					 0x12030c
#define PXP2_REG_RQ_BW_RD_L26					 0x120310
#define PXP2_REG_RQ_BW_RD_L27					 0x120314
#define PXP2_REG_RQ_BW_RD_L4					 0x1202bc
#define PXP2_REG_RQ_BW_RD_L5					 0x1202c0
#define PXP2_REG_RQ_BW_RD_UBOUND0				 0x120234
#define PXP2_REG_RQ_BW_RD_UBOUND12				 0x120264
#define PXP2_REG_RQ_BW_RD_UBOUND13				 0x120268
#define PXP2_REG_RQ_BW_RD_UBOUND14				 0x12026c
#define PXP2_REG_RQ_BW_RD_UBOUND15				 0x120270
#define PXP2_REG_RQ_BW_RD_UBOUND16				 0x120274
#define PXP2_REG_RQ_BW_RD_UBOUND17				 0x120278
#define PXP2_REG_RQ_BW_RD_UBOUND18				 0x12027c
#define PXP2_REG_RQ_BW_RD_UBOUND19				 0x120280
#define PXP2_REG_RQ_BW_RD_UBOUND20				 0x120284
#define PXP2_REG_RQ_BW_RD_UBOUND22				 0x120288
#define PXP2_REG_RQ_BW_RD_UBOUND23				 0x12028c
#define PXP2_REG_RQ_BW_RD_UBOUND24				 0x120290
#define PXP2_REG_RQ_BW_RD_UBOUND25				 0x120294
#define PXP2_REG_RQ_BW_RD_UBOUND26				 0x120298
#define PXP2_REG_RQ_BW_RD_UBOUND27				 0x12029c
#define PXP2_REG_RQ_BW_RD_UBOUND4				 0x120244
#define PXP2_REG_RQ_BW_RD_UBOUND5				 0x120248
#define PXP2_REG_RQ_BW_WR_ADD29 				 0x12022c
#define PXP2_REG_RQ_BW_WR_ADD30 				 0x120230
#define PXP2_REG_RQ_BW_WR_L29					 0x12031c
#define PXP2_REG_RQ_BW_WR_L30					 0x120320
#define PXP2_REG_RQ_BW_WR_UBOUND29				 0x1202a4
#define PXP2_REG_RQ_BW_WR_UBOUND30				 0x1202a8
#define PXP2_REG_RQ_CDU0_EFIRST_MEM_ADDR			 0x120008
#define PXP2_REG_RQ_CDU_ENDIAN_M				 0x1201a0
#define PXP2_REG_RQ_CDU_FIRST_ILT				 0x12061c
#define PXP2_REG_RQ_CDU_LAST_ILT				 0x120620
#define PXP2_REG_RQ_CDU_P_SIZE					 0x120018
#define PXP2_REG_RQ_CFG_DONE					 0x1201b4
#define PXP2_REG_RQ_DBG_ENDIAN_M				 0x1201a4
#define PXP2_REG_RQ_DISABLE_INPUTS				 0x120330
#define PXP2_REG_RQ_DRAM_ALIGN					 0x1205b0
#define PXP2_REG_RQ_DRAM_ALIGN_RD				 0x12092c
#define PXP2_REG_RQ_DRAM_ALIGN_SEL				 0x120930
#define PXP2_REG_RQ_ELT_DISABLE 				 0x12066c
#define PXP2_REG_RQ_HC_ENDIAN_M 				 0x1201a8
#define PXP2_REG_RQ_ILT_MODE					 0x1205b4
#define PXP2_REG_RQ_ONCHIP_AT					 0x122000
#define PXP2_REG_RQ_ONCHIP_AT_B0				 0x128000
#define PXP2_REG_RQ_PDR_LIMIT					 0x12033c
#define PXP2_REG_RQ_QM_ENDIAN_M 				 0x120194
#define PXP2_REG_RQ_QM_FIRST_ILT				 0x120634
#define PXP2_REG_RQ_QM_LAST_ILT 				 0x120638
#define PXP2_REG_RQ_QM_P_SIZE					 0x120050
#define PXP2_REG_RQ_RBC_DONE					 0x1201b0
#define PXP2_REG_RQ_RD_MBS0					 0x120160
#define PXP2_REG_RQ_RD_MBS1					 0x120168
#define PXP2_REG_RQ_SRC_ENDIAN_M				 0x12019c
#define PXP2_REG_RQ_SRC_FIRST_ILT				 0x12063c
#define PXP2_REG_RQ_SRC_LAST_ILT				 0x120640
#define PXP2_REG_RQ_SRC_P_SIZE					 0x12006c
#define PXP2_REG_RQ_TM_ENDIAN_M 				 0x120198
#define PXP2_REG_RQ_TM_FIRST_ILT				 0x120644
#define PXP2_REG_RQ_TM_LAST_ILT 				 0x120648
#define PXP2_REG_RQ_TM_P_SIZE					 0x120034
#define PXP2_REG_RQ_UFIFO_NUM_OF_ENTRY				 0x12080c
#define PXP2_REG_RQ_USDM0_EFIRST_MEM_ADDR			 0x120094
#define PXP2_REG_RQ_VQ0_ENTRY_CNT				 0x120810
#define PXP2_REG_RQ_VQ10_ENTRY_CNT				 0x120818
#define PXP2_REG_RQ_VQ11_ENTRY_CNT				 0x120820
#define PXP2_REG_RQ_VQ12_ENTRY_CNT				 0x120828
#define PXP2_REG_RQ_VQ13_ENTRY_CNT				 0x120830
#define PXP2_REG_RQ_VQ14_ENTRY_CNT				 0x120838
#define PXP2_REG_RQ_VQ15_ENTRY_CNT				 0x120840
#define PXP2_REG_RQ_VQ16_ENTRY_CNT				 0x120848
#define PXP2_REG_RQ_VQ17_ENTRY_CNT				 0x120850
#define PXP2_REG_RQ_VQ18_ENTRY_CNT				 0x120858
#define PXP2_REG_RQ_VQ19_ENTRY_CNT				 0x120860
#define PXP2_REG_RQ_VQ1_ENTRY_CNT				 0x120868
#define PXP2_REG_RQ_VQ20_ENTRY_CNT				 0x120870
#define PXP2_REG_RQ_VQ21_ENTRY_CNT				 0x120878
#define PXP2_REG_RQ_VQ22_ENTRY_CNT				 0x120880
#define PXP2_REG_RQ_VQ23_ENTRY_CNT				 0x120888
#define PXP2_REG_RQ_VQ24_ENTRY_CNT				 0x120890
#define PXP2_REG_RQ_VQ25_ENTRY_CNT				 0x120898
#define PXP2_REG_RQ_VQ26_ENTRY_CNT				 0x1208a0
#define PXP2_REG_RQ_VQ27_ENTRY_CNT				 0x1208a8
#define PXP2_REG_RQ_VQ28_ENTRY_CNT				 0x1208b0
#define PXP2_REG_RQ_VQ29_ENTRY_CNT				 0x1208b8
#define PXP2_REG_RQ_VQ2_ENTRY_CNT				 0x1208c0
#define PXP2_REG_RQ_VQ30_ENTRY_CNT				 0x1208c8
#define PXP2_REG_RQ_VQ31_ENTRY_CNT				 0x1208d0
#define PXP2_REG_RQ_VQ3_ENTRY_CNT				 0x1208d8
#define PXP2_REG_RQ_VQ4_ENTRY_CNT				 0x1208e0
#define PXP2_REG_RQ_VQ5_ENTRY_CNT				 0x1208e8
#define PXP2_REG_RQ_VQ6_ENTRY_CNT				 0x1208f0
#define PXP2_REG_RQ_VQ7_ENTRY_CNT				 0x1208f8
#define PXP2_REG_RQ_VQ8_ENTRY_CNT				 0x120900
#define PXP2_REG_RQ_VQ9_ENTRY_CNT				 0x120908
#define PXP2_REG_RQ_WR_MBS0					 0x12015c
#define PXP2_REG_RQ_WR_MBS1					 0x120164
#define PXP2_REG_WR_CDU_MPS					 0x1205f0
#define PXP2_REG_WR_CSDM_MPS					 0x1205d0
#define PXP2_REG_WR_DBG_MPS					 0x1205e8
#define PXP2_REG_WR_DMAE_MPS					 0x1205ec
#define PXP2_REG_WR_DMAE_TH					 0x120368
#define PXP2_REG_WR_HC_MPS					 0x1205c8
#define PXP2_REG_WR_QM_MPS					 0x1205dc
#define PXP2_REG_WR_REV_MODE					 0x120670
#define PXP2_REG_WR_SRC_MPS					 0x1205e4
#define PXP2_REG_WR_TM_MPS					 0x1205e0
#define PXP2_REG_WR_TSDM_MPS					 0x1205d4
#define PXP2_REG_WR_USDMDP_TH					 0x120348
#define PXP2_REG_WR_USDM_MPS					 0x1205cc
#define PXP2_REG_WR_XSDM_MPS					 0x1205d8
#define PXP_REG_HST_ARB_IS_IDLE 				 0x103004
#define PXP_REG_HST_CLIENTS_WAITING_TO_ARB			 0x103008
#define PXP_REG_HST_DISCARD_DOORBELLS				 0x1030a4
#define PXP_REG_HST_DISCARD_DOORBELLS_STATUS			 0x1030a0
#define PXP_REG_HST_DISCARD_INTERNAL_WRITES			 0x1030a8
#define PXP_REG_HST_DISCARD_INTERNAL_WRITES_STATUS		 0x10309c
#define PXP_REG_HST_INBOUND_INT 				 0x103800
#define PXP_REG_PXP_INT_MASK_0					 0x103074
#define PXP_REG_PXP_INT_MASK_1					 0x103084
#define PXP_REG_PXP_INT_STS_0					 0x103068
#define PXP_REG_PXP_INT_STS_1					 0x103078
#define PXP_REG_PXP_INT_STS_CLR_0				 0x10306c
#define PXP_REG_PXP_INT_STS_CLR_1				 0x10307c
#define PXP_REG_PXP_PRTY_MASK					 0x103094
#define PXP_REG_PXP_PRTY_STS					 0x103088
#define PXP_REG_PXP_PRTY_STS_CLR				 0x10308c
#define QM_REG_ACTCTRINITVAL_0					 0x168040
#define QM_REG_ACTCTRINITVAL_1					 0x168044
#define QM_REG_ACTCTRINITVAL_2					 0x168048
#define QM_REG_ACTCTRINITVAL_3					 0x16804c
#define QM_REG_BASEADDR 					 0x168900
#define QM_REG_BASEADDR_EXT_A					 0x16e100
#define QM_REG_BYTECRDCOST					 0x168234
#define QM_REG_BYTECRDINITVAL					 0x168238
#define QM_REG_BYTECRDPORT_LSB					 0x168228
#define QM_REG_BYTECRDPORT_LSB_EXT_A				 0x16e520
#define QM_REG_BYTECRDPORT_MSB					 0x168224
#define QM_REG_BYTECRDPORT_MSB_EXT_A				 0x16e51c
#define QM_REG_BYTECREDITAFULLTHR				 0x168094
#define QM_REG_CMINITCRD_0					 0x1680cc
#define QM_REG_BYTECRDCMDQ_0					 0x16e6e8
#define QM_REG_CMINITCRD_1					 0x1680d0
#define QM_REG_CMINITCRD_2					 0x1680d4
#define QM_REG_CMINITCRD_3					 0x1680d8
#define QM_REG_CMINITCRD_4					 0x1680dc
#define QM_REG_CMINITCRD_5					 0x1680e0
#define QM_REG_CMINITCRD_6					 0x1680e4
#define QM_REG_CMINITCRD_7					 0x1680e8
#define QM_REG_CMINTEN						 0x1680ec
#define QM_REG_CMINTVOQMASK_0					 0x1681f4
#define QM_REG_CMINTVOQMASK_1					 0x1681f8
#define QM_REG_CMINTVOQMASK_2					 0x1681fc
#define QM_REG_CMINTVOQMASK_3					 0x168200
#define QM_REG_CMINTVOQMASK_4					 0x168204
#define QM_REG_CMINTVOQMASK_5					 0x168208
#define QM_REG_CMINTVOQMASK_6					 0x16820c
#define QM_REG_CMINTVOQMASK_7					 0x168210
#define QM_REG_CONNNUM_0					 0x168020
#define QM_REG_CQM_WRC_FIFOLVL					 0x168018
#define QM_REG_CTXREG_0 					 0x168030
#define QM_REG_CTXREG_1 					 0x168034
#define QM_REG_CTXREG_2 					 0x168038
#define QM_REG_CTXREG_3 					 0x16803c
#define QM_REG_ENBYPVOQMASK					 0x16823c
#define QM_REG_ENBYTECRD_LSB					 0x168220
#define QM_REG_ENBYTECRD_LSB_EXT_A				 0x16e518
#define QM_REG_ENBYTECRD_MSB					 0x16821c
#define QM_REG_ENBYTECRD_MSB_EXT_A				 0x16e514
#define QM_REG_ENSEC						 0x1680f0
#define QM_REG_FUNCNUMSEL_LSB					 0x168230
#define QM_REG_FUNCNUMSEL_MSB					 0x16822c
#define QM_REG_HWAEMPTYMASK_LSB 				 0x168218
#define QM_REG_HWAEMPTYMASK_LSB_EXT_A				 0x16e510
#define QM_REG_HWAEMPTYMASK_MSB 				 0x168214
#define QM_REG_HWAEMPTYMASK_MSB_EXT_A				 0x16e50c
#define QM_REG_OUTLDREQ 					 0x168804
#define QM_REG_OVFERROR 					 0x16805c
#define QM_REG_OVFQNUM						 0x168058
#define QM_REG_PAUSESTATE0					 0x168410
#define QM_REG_PAUSESTATE1					 0x168414
#define QM_REG_PAUSESTATE2					 0x16e684
#define QM_REG_PAUSESTATE3					 0x16e688
#define QM_REG_PAUSESTATE4					 0x16e68c
#define QM_REG_PAUSESTATE5					 0x16e690
#define QM_REG_PAUSESTATE6					 0x16e694
#define QM_REG_PAUSESTATE7					 0x16e698
#define QM_REG_PCIREQAT 					 0x168054
#define QM_REG_PF_EN						 0x16e70c
#define QM_REG_PF_USG_CNT_0					 0x16e040
#define QM_REG_PORT0BYTECRD					 0x168300
#define QM_REG_PORT1BYTECRD					 0x168304
#define QM_REG_PQ2PCIFUNC_0					 0x16e6bc
#define QM_REG_PQ2PCIFUNC_1					 0x16e6c0
#define QM_REG_PQ2PCIFUNC_2					 0x16e6c4
#define QM_REG_PQ2PCIFUNC_3					 0x16e6c8
#define QM_REG_PQ2PCIFUNC_4					 0x16e6cc
#define QM_REG_PQ2PCIFUNC_5					 0x16e6d0
#define QM_REG_PQ2PCIFUNC_6					 0x16e6d4
#define QM_REG_PQ2PCIFUNC_7					 0x16e6d8
#define QM_REG_PTRTBL						 0x168a00
#define QM_REG_PTRTBL_EXT_A					 0x16e200
#define QM_REG_QM_INT_MASK					 0x168444
#define QM_REG_QM_INT_STS					 0x168438
#define QM_REG_QM_PRTY_MASK					 0x168454
#define QM_REG_QM_PRTY_STS					 0x168448
#define QM_REG_QM_PRTY_STS_CLR					 0x16844c
#define QM_REG_QSTATUS_HIGH					 0x16802c
#define QM_REG_QSTATUS_HIGH_EXT_A				 0x16e408
#define QM_REG_QSTATUS_LOW					 0x168028
#define QM_REG_QSTATUS_LOW_EXT_A				 0x16e404
#define QM_REG_QTASKCTR_0					 0x168308
#define QM_REG_QTASKCTR_EXT_A_0 				 0x16e584
#define QM_REG_QVOQIDX_0					 0x1680f4
#define QM_REG_QVOQIDX_10					 0x16811c
#define QM_REG_QVOQIDX_100					 0x16e49c
#define QM_REG_QVOQIDX_101					 0x16e4a0
#define QM_REG_QVOQIDX_102					 0x16e4a4
#define QM_REG_QVOQIDX_103					 0x16e4a8
#define QM_REG_QVOQIDX_104					 0x16e4ac
#define QM_REG_QVOQIDX_105					 0x16e4b0
#define QM_REG_QVOQIDX_106					 0x16e4b4
#define QM_REG_QVOQIDX_107					 0x16e4b8
#define QM_REG_QVOQIDX_108					 0x16e4bc
#define QM_REG_QVOQIDX_109					 0x16e4c0
#define QM_REG_QVOQIDX_11					 0x168120
#define QM_REG_QVOQIDX_110					 0x16e4c4
#define QM_REG_QVOQIDX_111					 0x16e4c8
#define QM_REG_QVOQIDX_112					 0x16e4cc
#define QM_REG_QVOQIDX_113					 0x16e4d0
#define QM_REG_QVOQIDX_114					 0x16e4d4
#define QM_REG_QVOQIDX_115					 0x16e4d8
#define QM_REG_QVOQIDX_116					 0x16e4dc
#define QM_REG_QVOQIDX_117					 0x16e4e0
#define QM_REG_QVOQIDX_118					 0x16e4e4
#define QM_REG_QVOQIDX_119					 0x16e4e8
#define QM_REG_QVOQIDX_12					 0x168124
#define QM_REG_QVOQIDX_120					 0x16e4ec
#define QM_REG_QVOQIDX_121					 0x16e4f0
#define QM_REG_QVOQIDX_122					 0x16e4f4
#define QM_REG_QVOQIDX_123					 0x16e4f8
#define QM_REG_QVOQIDX_124					 0x16e4fc
#define QM_REG_QVOQIDX_125					 0x16e500
#define QM_REG_QVOQIDX_126					 0x16e504
#define QM_REG_QVOQIDX_127					 0x16e508
#define QM_REG_QVOQIDX_13					 0x168128
#define QM_REG_QVOQIDX_14					 0x16812c
#define QM_REG_QVOQIDX_15					 0x168130
#define QM_REG_QVOQIDX_16					 0x168134
#define QM_REG_QVOQIDX_17					 0x168138
#define QM_REG_QVOQIDX_21					 0x168148
#define QM_REG_QVOQIDX_22					 0x16814c
#define QM_REG_QVOQIDX_23					 0x168150
#define QM_REG_QVOQIDX_24					 0x168154
#define QM_REG_QVOQIDX_25					 0x168158
#define QM_REG_QVOQIDX_26					 0x16815c
#define QM_REG_QVOQIDX_27					 0x168160
#define QM_REG_QVOQIDX_28					 0x168164
#define QM_REG_QVOQIDX_29					 0x168168
#define QM_REG_QVOQIDX_30					 0x16816c
#define QM_REG_QVOQIDX_31					 0x168170
#define QM_REG_QVOQIDX_32					 0x168174
#define QM_REG_QVOQIDX_33					 0x168178
#define QM_REG_QVOQIDX_34					 0x16817c
#define QM_REG_QVOQIDX_35					 0x168180
#define QM_REG_QVOQIDX_36					 0x168184
#define QM_REG_QVOQIDX_37					 0x168188
#define QM_REG_QVOQIDX_38					 0x16818c
#define QM_REG_QVOQIDX_39					 0x168190
#define QM_REG_QVOQIDX_40					 0x168194
#define QM_REG_QVOQIDX_41					 0x168198
#define QM_REG_QVOQIDX_42					 0x16819c
#define QM_REG_QVOQIDX_43					 0x1681a0
#define QM_REG_QVOQIDX_44					 0x1681a4
#define QM_REG_QVOQIDX_45					 0x1681a8
#define QM_REG_QVOQIDX_46					 0x1681ac
#define QM_REG_QVOQIDX_47					 0x1681b0
#define QM_REG_QVOQIDX_48					 0x1681b4
#define QM_REG_QVOQIDX_49					 0x1681b8
#define QM_REG_QVOQIDX_5					 0x168108
#define QM_REG_QVOQIDX_50					 0x1681bc
#define QM_REG_QVOQIDX_51					 0x1681c0
#define QM_REG_QVOQIDX_52					 0x1681c4
#define QM_REG_QVOQIDX_53					 0x1681c8
#define QM_REG_QVOQIDX_54					 0x1681cc
#define QM_REG_QVOQIDX_55					 0x1681d0
#define QM_REG_QVOQIDX_56					 0x1681d4
#define QM_REG_QVOQIDX_57					 0x1681d8
#define QM_REG_QVOQIDX_58					 0x1681dc
#define QM_REG_QVOQIDX_59					 0x1681e0
#define QM_REG_QVOQIDX_6					 0x16810c
#define QM_REG_QVOQIDX_60					 0x1681e4
#define QM_REG_QVOQIDX_61					 0x1681e8
#define QM_REG_QVOQIDX_62					 0x1681ec
#define QM_REG_QVOQIDX_63					 0x1681f0
#define QM_REG_QVOQIDX_64					 0x16e40c
#define QM_REG_QVOQIDX_65					 0x16e410
#define QM_REG_QVOQIDX_69					 0x16e420
#define QM_REG_QVOQIDX_7					 0x168110
#define QM_REG_QVOQIDX_70					 0x16e424
#define QM_REG_QVOQIDX_71					 0x16e428
#define QM_REG_QVOQIDX_72					 0x16e42c
#define QM_REG_QVOQIDX_73					 0x16e430
#define QM_REG_QVOQIDX_74					 0x16e434
#define QM_REG_QVOQIDX_75					 0x16e438
#define QM_REG_QVOQIDX_76					 0x16e43c
#define QM_REG_QVOQIDX_77					 0x16e440
#define QM_REG_QVOQIDX_78					 0x16e444
#define QM_REG_QVOQIDX_79					 0x16e448
#define QM_REG_QVOQIDX_8					 0x168114
#define QM_REG_QVOQIDX_80					 0x16e44c
#define QM_REG_QVOQIDX_81					 0x16e450
#define QM_REG_QVOQIDX_85					 0x16e460
#define QM_REG_QVOQIDX_86					 0x16e464
#define QM_REG_QVOQIDX_87					 0x16e468
#define QM_REG_QVOQIDX_88					 0x16e46c
#define QM_REG_QVOQIDX_89					 0x16e470
#define QM_REG_QVOQIDX_9					 0x168118
#define QM_REG_QVOQIDX_90					 0x16e474
#define QM_REG_QVOQIDX_91					 0x16e478
#define QM_REG_QVOQIDX_92					 0x16e47c
#define QM_REG_QVOQIDX_93					 0x16e480
#define QM_REG_QVOQIDX_94					 0x16e484
#define QM_REG_QVOQIDX_95					 0x16e488
#define QM_REG_QVOQIDX_96					 0x16e48c
#define QM_REG_QVOQIDX_97					 0x16e490
#define QM_REG_QVOQIDX_98					 0x16e494
#define QM_REG_QVOQIDX_99					 0x16e498
#define QM_REG_SOFT_RESET					 0x168428
#define QM_REG_TASKCRDCOST_0					 0x16809c
#define QM_REG_TASKCRDCOST_1					 0x1680a0
#define QM_REG_TASKCRDCOST_2					 0x1680a4
#define QM_REG_TASKCRDCOST_4					 0x1680ac
#define QM_REG_TASKCRDCOST_5					 0x1680b0
#define QM_REG_TQM_WRC_FIFOLVL					 0x168010
#define QM_REG_UQM_WRC_FIFOLVL					 0x168008
#define QM_REG_VOQCRDERRREG					 0x168408
#define QM_REG_VOQCREDIT_0					 0x1682d0
#define QM_REG_VOQCREDIT_1					 0x1682d4
#define QM_REG_VOQCREDIT_4					 0x1682e0
#define QM_REG_VOQCREDITAFULLTHR				 0x168090
#define QM_REG_VOQINITCREDIT_0					 0x168060
#define QM_REG_VOQINITCREDIT_1					 0x168064
#define QM_REG_VOQINITCREDIT_2					 0x168068
#define QM_REG_VOQINITCREDIT_4					 0x168070
#define QM_REG_VOQINITCREDIT_5					 0x168074
#define QM_REG_VOQPORT_0					 0x1682a0
#define QM_REG_VOQPORT_1					 0x1682a4
#define QM_REG_VOQPORT_2					 0x1682a8
#define QM_REG_VOQQMASK_0_LSB					 0x168240
#define QM_REG_VOQQMASK_0_LSB_EXT_A				 0x16e524
#define QM_REG_VOQQMASK_0_MSB					 0x168244
#define QM_REG_VOQQMASK_0_MSB_EXT_A				 0x16e528
#define QM_REG_VOQQMASK_10_LSB					 0x168290
#define QM_REG_VOQQMASK_10_LSB_EXT_A				 0x16e574
#define QM_REG_VOQQMASK_10_MSB					 0x168294
#define QM_REG_VOQQMASK_10_MSB_EXT_A				 0x16e578
#define QM_REG_VOQQMASK_11_LSB					 0x168298
#define QM_REG_VOQQMASK_11_LSB_EXT_A				 0x16e57c
#define QM_REG_VOQQMASK_11_MSB					 0x16829c
#define QM_REG_VOQQMASK_11_MSB_EXT_A				 0x16e580
#define QM_REG_VOQQMASK_1_LSB					 0x168248
#define QM_REG_VOQQMASK_1_LSB_EXT_A				 0x16e52c
#define QM_REG_VOQQMASK_1_MSB					 0x16824c
#define QM_REG_VOQQMASK_1_MSB_EXT_A				 0x16e530
#define QM_REG_VOQQMASK_2_LSB					 0x168250
#define QM_REG_VOQQMASK_2_LSB_EXT_A				 0x16e534
#define QM_REG_VOQQMASK_2_MSB					 0x168254
#define QM_REG_VOQQMASK_2_MSB_EXT_A				 0x16e538
#define QM_REG_VOQQMASK_3_LSB					 0x168258
#define QM_REG_VOQQMASK_3_LSB_EXT_A				 0x16e53c
#define QM_REG_VOQQMASK_3_MSB_EXT_A				 0x16e540
#define QM_REG_VOQQMASK_4_LSB					 0x168260
#define QM_REG_VOQQMASK_4_LSB_EXT_A				 0x16e544
#define QM_REG_VOQQMASK_4_MSB					 0x168264
#define QM_REG_VOQQMASK_4_MSB_EXT_A				 0x16e548
#define QM_REG_VOQQMASK_5_LSB					 0x168268
#define QM_REG_VOQQMASK_5_LSB_EXT_A				 0x16e54c
#define QM_REG_VOQQMASK_5_MSB					 0x16826c
#define QM_REG_VOQQMASK_5_MSB_EXT_A				 0x16e550
#define QM_REG_VOQQMASK_6_LSB					 0x168270
#define QM_REG_VOQQMASK_6_LSB_EXT_A				 0x16e554
#define QM_REG_VOQQMASK_6_MSB					 0x168274
#define QM_REG_VOQQMASK_6_MSB_EXT_A				 0x16e558
#define QM_REG_VOQQMASK_7_LSB					 0x168278
#define QM_REG_VOQQMASK_7_LSB_EXT_A				 0x16e55c
#define QM_REG_VOQQMASK_7_MSB					 0x16827c
#define QM_REG_VOQQMASK_7_MSB_EXT_A				 0x16e560
#define QM_REG_VOQQMASK_8_LSB					 0x168280
#define QM_REG_VOQQMASK_8_LSB_EXT_A				 0x16e564
#define QM_REG_VOQQMASK_8_MSB					 0x168284
#define QM_REG_VOQQMASK_8_MSB_EXT_A				 0x16e568
#define QM_REG_VOQQMASK_9_LSB					 0x168288
#define QM_REG_VOQQMASK_9_LSB_EXT_A				 0x16e56c
#define QM_REG_VOQQMASK_9_MSB_EXT_A				 0x16e570
#define QM_REG_WRRWEIGHTS_0					 0x16880c
#define QM_REG_WRRWEIGHTS_1					 0x168810
#define QM_REG_WRRWEIGHTS_10					 0x168814
#define QM_REG_WRRWEIGHTS_11					 0x168818
#define QM_REG_WRRWEIGHTS_12					 0x16881c
#define QM_REG_WRRWEIGHTS_13					 0x168820
#define QM_REG_WRRWEIGHTS_14					 0x168824
#define QM_REG_WRRWEIGHTS_15					 0x168828
#define QM_REG_WRRWEIGHTS_16					 0x16e000
#define QM_REG_WRRWEIGHTS_17					 0x16e004
#define QM_REG_WRRWEIGHTS_18					 0x16e008
#define QM_REG_WRRWEIGHTS_19					 0x16e00c
#define QM_REG_WRRWEIGHTS_2					 0x16882c
#define QM_REG_WRRWEIGHTS_20					 0x16e010
#define QM_REG_WRRWEIGHTS_21					 0x16e014
#define QM_REG_WRRWEIGHTS_22					 0x16e018
#define QM_REG_WRRWEIGHTS_23					 0x16e01c
#define QM_REG_WRRWEIGHTS_24					 0x16e020
#define QM_REG_WRRWEIGHTS_25					 0x16e024
#define QM_REG_WRRWEIGHTS_26					 0x16e028
#define QM_REG_WRRWEIGHTS_27					 0x16e02c
#define QM_REG_WRRWEIGHTS_28					 0x16e030
#define QM_REG_WRRWEIGHTS_29					 0x16e034
#define QM_REG_WRRWEIGHTS_3					 0x168830
#define QM_REG_WRRWEIGHTS_30					 0x16e038
#define QM_REG_WRRWEIGHTS_31					 0x16e03c
#define QM_REG_WRRWEIGHTS_4					 0x168834
#define QM_REG_WRRWEIGHTS_5					 0x168838
#define QM_REG_WRRWEIGHTS_6					 0x16883c
#define QM_REG_WRRWEIGHTS_7					 0x168840
#define QM_REG_WRRWEIGHTS_8					 0x168844
#define QM_REG_WRRWEIGHTS_9					 0x168848
#define QM_REG_XQM_WRC_FIFOLVL					 0x168000
#define SEM_FAST_REG_PARITY_RST					 0x18840
#define SRC_REG_COUNTFREE0					 0x40500
#define SRC_REG_E1HMF_ENABLE					 0x404cc
#define SRC_REG_FIRSTFREE0					 0x40510
#define SRC_REG_KEYRSS0_0					 0x40408
#define SRC_REG_KEYRSS0_7					 0x40424
#define SRC_REG_KEYRSS1_9					 0x40454
#define SRC_REG_KEYSEARCH_0					 0x40458
#define SRC_REG_KEYSEARCH_1					 0x4045c
#define SRC_REG_KEYSEARCH_2					 0x40460
#define SRC_REG_KEYSEARCH_3					 0x40464
#define SRC_REG_KEYSEARCH_4					 0x40468
#define SRC_REG_KEYSEARCH_5					 0x4046c
#define SRC_REG_KEYSEARCH_6					 0x40470
#define SRC_REG_KEYSEARCH_7					 0x40474
#define SRC_REG_KEYSEARCH_8					 0x40478
#define SRC_REG_KEYSEARCH_9					 0x4047c
#define SRC_REG_LASTFREE0					 0x40530
#define SRC_REG_NUMBER_HASH_BITS0				 0x40400
#define SRC_REG_SOFT_RST					 0x4049c
#define SRC_REG_SRC_INT_STS					 0x404ac
#define SRC_REG_SRC_PRTY_MASK					 0x404c8
#define SRC_REG_SRC_PRTY_STS					 0x404bc
#define SRC_REG_SRC_PRTY_STS_CLR				 0x404c0
#define TCM_REG_CAM_OCCUP					 0x5017c
#define TCM_REG_CDU_AG_RD_IFEN					 0x50034
#define TCM_REG_CDU_AG_WR_IFEN					 0x50030
#define TCM_REG_CDU_SM_RD_IFEN					 0x5003c
#define TCM_REG_CDU_SM_WR_IFEN					 0x50038
#define TCM_REG_CFC_INIT_CRD					 0x50204
#define TCM_REG_CP_WEIGHT					 0x500c0
#define TCM_REG_CSEM_IFEN					 0x5002c
#define TCM_REG_CSEM_LENGTH_MIS 				 0x50174
#define TCM_REG_CSEM_WEIGHT					 0x500bc
#define TCM_REG_ERR_EVNT_ID					 0x500a0
#define TCM_REG_ERR_TCM_HDR					 0x5009c
#define TCM_REG_EXPR_EVNT_ID					 0x500a4
#define TCM_REG_FIC0_INIT_CRD					 0x5020c
#define TCM_REG_FIC1_INIT_CRD					 0x50210
#define TCM_REG_GR_ARB_TYPE					 0x50114
#define TCM_REG_GR_LD0_PR					 0x5011c
#define TCM_REG_GR_LD1_PR					 0x50120
#define TCM_REG_N_SM_CTX_LD_0					 0x50050
#define TCM_REG_N_SM_CTX_LD_1					 0x50054
#define TCM_REG_N_SM_CTX_LD_2					 0x50058
#define TCM_REG_N_SM_CTX_LD_3					 0x5005c
#define TCM_REG_N_SM_CTX_LD_4					 0x50060
#define TCM_REG_N_SM_CTX_LD_5					 0x50064
#define TCM_REG_PBF_IFEN					 0x50024
#define TCM_REG_PBF_LENGTH_MIS					 0x5016c
#define TCM_REG_PBF_WEIGHT					 0x500b4
#define TCM_REG_PHYS_QNUM0_0					 0x500e0
#define TCM_REG_PHYS_QNUM0_1					 0x500e4
#define TCM_REG_PHYS_QNUM1_0					 0x500e8
#define TCM_REG_PHYS_QNUM1_1					 0x500ec
#define TCM_REG_PHYS_QNUM2_0					 0x500f0
#define TCM_REG_PHYS_QNUM2_1					 0x500f4
#define TCM_REG_PHYS_QNUM3_0					 0x500f8
#define TCM_REG_PHYS_QNUM3_1					 0x500fc
#define TCM_REG_PRS_IFEN					 0x50020
#define TCM_REG_PRS_LENGTH_MIS					 0x50168
#define TCM_REG_PRS_WEIGHT					 0x500b0
#define TCM_REG_STOP_EVNT_ID					 0x500a8
#define TCM_REG_STORM_LENGTH_MIS				 0x50160
#define TCM_REG_STORM_TCM_IFEN					 0x50010
#define TCM_REG_STORM_WEIGHT					 0x500ac
#define TCM_REG_TCM_CFC_IFEN					 0x50040
#define TCM_REG_TCM_INT_MASK					 0x501dc
#define TCM_REG_TCM_INT_STS					 0x501d0
#define TCM_REG_TCM_PRTY_MASK					 0x501ec
#define TCM_REG_TCM_PRTY_STS					 0x501e0
#define TCM_REG_TCM_PRTY_STS_CLR				 0x501e4
/* [RW 3] The size of AG context region 0 in REG-pairs. Designates the MS
   REG-pair number (e.g. if region 0 is 6 REG-pairs; the value should be 5).
   Is used to determine the number of the AG context REG-pairs written back;
   when the input message Reg1WbFlg isn't set. */
#define TCM_REG_TCM_REG0_SZ					 0x500d8
#define TCM_REG_TCM_STORM0_IFEN 				 0x50004
#define TCM_REG_TCM_STORM1_IFEN 				 0x50008
#define TCM_REG_TCM_TQM_IFEN					 0x5000c
#define TCM_REG_TCM_TQM_USE_Q					 0x500d4
#define TCM_REG_TM_TCM_HDR					 0x50098
#define TCM_REG_TM_TCM_IFEN					 0x5001c
#define TCM_REG_TM_WEIGHT					 0x500d0
#define TCM_REG_TQM_INIT_CRD					 0x5021c
#define TCM_REG_TQM_P_WEIGHT					 0x500c8
#define TCM_REG_TQM_S_WEIGHT					 0x500cc
#define TCM_REG_TQM_TCM_HDR_P					 0x50090
#define TCM_REG_TQM_TCM_HDR_S					 0x50094
#define TCM_REG_TQM_TCM_IFEN					 0x50014
#define TCM_REG_TSDM_IFEN					 0x50018
#define TCM_REG_TSDM_LENGTH_MIS 				 0x50164
#define TCM_REG_TSDM_WEIGHT					 0x500c4
#define TCM_REG_USEM_IFEN					 0x50028
#define TCM_REG_USEM_LENGTH_MIS 				 0x50170
#define TCM_REG_USEM_WEIGHT					 0x500b8
#define TCM_REG_XX_DESCR_TABLE					 0x50280
#define TCM_REG_XX_DESCR_TABLE_SIZE				 29
#define TCM_REG_XX_FREE 					 0x50178
#define TCM_REG_XX_INIT_CRD					 0x50220
#define TCM_REG_XX_MAX_LL_SZ					 0x50044
#define TCM_REG_XX_MSG_NUM					 0x50224
#define TCM_REG_XX_OVFL_EVNT_ID 				 0x50048
#define TCM_REG_XX_TABLE					 0x50240
#define TM_REG_CFC_AC_CRDCNT_VAL				 0x164208
#define TM_REG_CFC_CLD_CRDCNT_VAL				 0x164210
#define TM_REG_CL0_CONT_REGION					 0x164030
#define TM_REG_CL1_CONT_REGION					 0x164034
#define TM_REG_CL2_CONT_REGION					 0x164038
#define TM_REG_CLIN_PRIOR0_CLIENT				 0x164024
#define TM_REG_CLOUT_CRDCNT0_VAL				 0x164220
#define TM_REG_CLOUT_CRDCNT1_VAL				 0x164228
#define TM_REG_CLOUT_CRDCNT2_VAL				 0x164230
#define TM_REG_EN_CL0_INPUT					 0x164008
#define TM_REG_EN_CL1_INPUT					 0x16400c
#define TM_REG_EN_CL2_INPUT					 0x164010
#define TM_REG_EN_LINEAR0_TIMER 				 0x164014
#define TM_REG_EN_REAL_TIME_CNT 				 0x1640d8
#define TM_REG_EN_TIMERS					 0x164000
#define TM_REG_EXP_CRDCNT_VAL					 0x164238
#define TM_REG_LIN0_LOGIC_ADDR					 0x164240
#define TM_REG_LIN0_MAX_ACTIVE_CID				 0x164048
#define TM_REG_LIN0_NUM_SCANS					 0x1640a0
#define TM_REG_LIN0_PHY_ADDR					 0x164270
#define TM_REG_LIN0_PHY_ADDR_VALID				 0x164248
#define TM_REG_LIN0_SCAN_ON					 0x1640d0
#define TM_REG_LIN0_SCAN_TIME					 0x16403c
#define TM_REG_LIN0_VNIC_UC					 0x164128
#define TM_REG_LIN1_LOGIC_ADDR					 0x164250
#define TM_REG_LIN1_PHY_ADDR					 0x164280
#define TM_REG_LIN1_PHY_ADDR_VALID				 0x164258
#define TM_REG_LIN_SETCLR_FIFO_ALFULL_THR			 0x164070
#define TM_REG_PCIARB_CRDCNT_VAL				 0x164260
#define TM_REG_TIMER_TICK_SIZE					 0x16401c
#define TM_REG_TM_CONTEXT_REGION				 0x164044
#define TM_REG_TM_INT_MASK					 0x1640fc
#define TM_REG_TM_INT_STS					 0x1640f0
#define TM_REG_TM_PRTY_MASK					 0x16410c
#define TM_REG_TM_PRTY_STS_CLR					 0x164104
#define TSDM_REG_AGG_INT_EVENT_0				 0x42038
#define TSDM_REG_AGG_INT_EVENT_1				 0x4203c
#define TSDM_REG_AGG_INT_EVENT_2				 0x42040
#define TSDM_REG_AGG_INT_EVENT_3				 0x42044
#define TSDM_REG_AGG_INT_EVENT_4				 0x42048
#define TSDM_REG_AGG_INT_T_0					 0x420b8
#define TSDM_REG_AGG_INT_T_1					 0x420bc
#define TSDM_REG_CFC_RSP_START_ADDR				 0x42008
#define TSDM_REG_CMP_COUNTER_MAX0				 0x4201c
#define TSDM_REG_CMP_COUNTER_MAX1				 0x42020
#define TSDM_REG_CMP_COUNTER_MAX2				 0x42024
#define TSDM_REG_CMP_COUNTER_MAX3				 0x42028
#define TSDM_REG_CMP_COUNTER_START_ADDR 			 0x4200c
#define TSDM_REG_ENABLE_IN1					 0x42238
#define TSDM_REG_ENABLE_IN2					 0x4223c
#define TSDM_REG_ENABLE_OUT1					 0x42240
#define TSDM_REG_ENABLE_OUT2					 0x42244
#define TSDM_REG_INIT_CREDIT_PXP_CTRL				 0x424bc
#define TSDM_REG_NUM_OF_ACK_AFTER_PLACE 			 0x4227c
#define TSDM_REG_NUM_OF_PKT_END_MSG				 0x42274
#define TSDM_REG_NUM_OF_PXP_ASYNC_REQ				 0x42278
#define TSDM_REG_NUM_OF_Q0_CMD					 0x42248
#define TSDM_REG_NUM_OF_Q10_CMD 				 0x4226c
#define TSDM_REG_NUM_OF_Q11_CMD 				 0x42270
#define TSDM_REG_NUM_OF_Q1_CMD					 0x4224c
#define TSDM_REG_NUM_OF_Q3_CMD					 0x42250
#define TSDM_REG_NUM_OF_Q4_CMD					 0x42254
#define TSDM_REG_NUM_OF_Q5_CMD					 0x42258
#define TSDM_REG_NUM_OF_Q6_CMD					 0x4225c
#define TSDM_REG_NUM_OF_Q7_CMD					 0x42260
#define TSDM_REG_NUM_OF_Q8_CMD					 0x42264
#define TSDM_REG_NUM_OF_Q9_CMD					 0x42268
#define TSDM_REG_PCK_END_MSG_START_ADDR 			 0x42014
#define TSDM_REG_Q_COUNTER_START_ADDR				 0x42010
#define TSDM_REG_RSP_PXP_CTRL_RDATA_EMPTY			 0x42548
#define TSDM_REG_SYNC_PARSER_EMPTY				 0x42550
#define TSDM_REG_SYNC_SYNC_EMPTY				 0x42558
#define TSDM_REG_TIMER_TICK					 0x42000
#define TSDM_REG_TSDM_INT_MASK_0				 0x4229c
#define TSDM_REG_TSDM_INT_MASK_1				 0x422ac
#define TSDM_REG_TSDM_INT_STS_0 				 0x42290
#define TSDM_REG_TSDM_INT_STS_1 				 0x422a0
#define TSDM_REG_TSDM_PRTY_MASK 				 0x422bc
#define TSDM_REG_TSDM_PRTY_STS					 0x422b0
#define TSDM_REG_TSDM_PRTY_STS_CLR				 0x422b4
#define TSEM_REG_ARB_CYCLE_SIZE 				 0x180034
#define TSEM_REG_ARB_ELEMENT0					 0x180020
#define TSEM_REG_ARB_ELEMENT1					 0x180024
#define TSEM_REG_ARB_ELEMENT2					 0x180028
#define TSEM_REG_ARB_ELEMENT3					 0x18002c
#define TSEM_REG_ARB_ELEMENT4					 0x180030
#define TSEM_REG_ENABLE_IN					 0x1800a4
#define TSEM_REG_ENABLE_OUT					 0x1800a8
#define TSEM_REG_FAST_MEMORY					 0x1a0000
#define TSEM_REG_FIC0_DISABLE					 0x180224
#define TSEM_REG_FIC1_DISABLE					 0x180234
#define TSEM_REG_INT_TABLE					 0x180400
#define TSEM_REG_MSG_NUM_FIC0					 0x180000
#define TSEM_REG_MSG_NUM_FIC1					 0x180004
#define TSEM_REG_MSG_NUM_FOC0					 0x180008
#define TSEM_REG_MSG_NUM_FOC1					 0x18000c
#define TSEM_REG_MSG_NUM_FOC2					 0x180010
#define TSEM_REG_MSG_NUM_FOC3					 0x180014
#define TSEM_REG_PAS_DISABLE					 0x18024c
#define TSEM_REG_PASSIVE_BUFFER 				 0x181000
#define TSEM_REG_PRAM						 0x1c0000
#define TSEM_REG_SLEEP_THREADS_VALID				 0x18026c
#define TSEM_REG_SLOW_EXT_STORE_EMPTY				 0x1802a0
#define TSEM_REG_THREADS_LIST					 0x1802e4
#define TSEM_REG_TSEM_PRTY_STS_CLR_0				 0x180118
#define TSEM_REG_TSEM_PRTY_STS_CLR_1				 0x180128
#define TSEM_REG_TS_0_AS					 0x180038
#define TSEM_REG_TS_10_AS					 0x180060
#define TSEM_REG_TS_11_AS					 0x180064
#define TSEM_REG_TS_12_AS					 0x180068
#define TSEM_REG_TS_13_AS					 0x18006c
#define TSEM_REG_TS_14_AS					 0x180070
#define TSEM_REG_TS_15_AS					 0x180074
#define TSEM_REG_TS_16_AS					 0x180078
#define TSEM_REG_TS_17_AS					 0x18007c
#define TSEM_REG_TS_18_AS					 0x180080
#define TSEM_REG_TS_1_AS					 0x18003c
#define TSEM_REG_TS_2_AS					 0x180040
#define TSEM_REG_TS_3_AS					 0x180044
#define TSEM_REG_TS_4_AS					 0x180048
#define TSEM_REG_TS_5_AS					 0x18004c
#define TSEM_REG_TS_6_AS					 0x180050
#define TSEM_REG_TS_7_AS					 0x180054
#define TSEM_REG_TS_8_AS					 0x180058
#define TSEM_REG_TS_9_AS					 0x18005c
#define TSEM_REG_TSEM_INT_MASK_0				 0x180100
#define TSEM_REG_TSEM_INT_MASK_1				 0x180110
#define TSEM_REG_TSEM_INT_STS_0 				 0x1800f4
#define TSEM_REG_TSEM_INT_STS_1 				 0x180104
#define TSEM_REG_TSEM_PRTY_MASK_0				 0x180120
#define TSEM_REG_TSEM_PRTY_MASK_1				 0x180130
#define TSEM_REG_TSEM_PRTY_STS_0				 0x180114
#define TSEM_REG_TSEM_PRTY_STS_1				 0x180124
#define TSEM_REG_VFPF_ERR_NUM					 0x180380
#define UCM_REG_AG_CTX						 0xe2000
#define UCM_REG_CAM_OCCUP					 0xe0170
#define UCM_REG_CDU_AG_RD_IFEN					 0xe0038
#define UCM_REG_CDU_AG_WR_IFEN					 0xe0034
#define UCM_REG_CDU_SM_RD_IFEN					 0xe0040
#define UCM_REG_CDU_SM_WR_IFEN					 0xe003c
#define UCM_REG_CFC_INIT_CRD					 0xe0204
#define UCM_REG_CP_WEIGHT					 0xe00c4
#define UCM_REG_CSEM_IFEN					 0xe0028
#define UCM_REG_CSEM_LENGTH_MIS 				 0xe0160
#define UCM_REG_CSEM_WEIGHT					 0xe00b8
#define UCM_REG_DORQ_IFEN					 0xe0030
#define UCM_REG_DORQ_LENGTH_MIS 				 0xe0168
#define UCM_REG_DORQ_WEIGHT					 0xe00c0
#define UCM_REG_ERR_EVNT_ID					 0xe00a4
#define UCM_REG_ERR_UCM_HDR					 0xe00a0
#define UCM_REG_EXPR_EVNT_ID					 0xe00a8
#define UCM_REG_FIC0_INIT_CRD					 0xe020c
#define UCM_REG_FIC1_INIT_CRD					 0xe0210
#define UCM_REG_GR_ARB_TYPE					 0xe0144
#define UCM_REG_GR_LD0_PR					 0xe014c
#define UCM_REG_GR_LD1_PR					 0xe0150
#define UCM_REG_INV_CFLG_Q					 0xe00e4
#define UCM_REG_N_SM_CTX_LD_0					 0xe0054
#define UCM_REG_N_SM_CTX_LD_1					 0xe0058
#define UCM_REG_N_SM_CTX_LD_2					 0xe005c
#define UCM_REG_N_SM_CTX_LD_3					 0xe0060
#define UCM_REG_N_SM_CTX_LD_4					 0xe0064
#define UCM_REG_N_SM_CTX_LD_5					 0xe0068
#define UCM_REG_PHYS_QNUM0_0					 0xe0110
#define UCM_REG_PHYS_QNUM0_1					 0xe0114
#define UCM_REG_PHYS_QNUM1_0					 0xe0118
#define UCM_REG_PHYS_QNUM1_1					 0xe011c
#define UCM_REG_PHYS_QNUM2_0					 0xe0120
#define UCM_REG_PHYS_QNUM2_1					 0xe0124
#define UCM_REG_PHYS_QNUM3_0					 0xe0128
#define UCM_REG_PHYS_QNUM3_1					 0xe012c
#define UCM_REG_STOP_EVNT_ID					 0xe00ac
#define UCM_REG_STORM_LENGTH_MIS				 0xe0154
#define UCM_REG_STORM_UCM_IFEN					 0xe0010
#define UCM_REG_STORM_WEIGHT					 0xe00b0
#define UCM_REG_TM_INIT_CRD					 0xe021c
#define UCM_REG_TM_UCM_HDR					 0xe009c
#define UCM_REG_TM_UCM_IFEN					 0xe001c
#define UCM_REG_TM_WEIGHT					 0xe00d4
#define UCM_REG_TSEM_IFEN					 0xe0024
#define UCM_REG_TSEM_LENGTH_MIS 				 0xe015c
#define UCM_REG_TSEM_WEIGHT					 0xe00b4
#define UCM_REG_UCM_CFC_IFEN					 0xe0044
#define UCM_REG_UCM_INT_MASK					 0xe01d4
#define UCM_REG_UCM_INT_STS					 0xe01c8
#define UCM_REG_UCM_PRTY_MASK					 0xe01e4
#define UCM_REG_UCM_PRTY_STS					 0xe01d8
#define UCM_REG_UCM_PRTY_STS_CLR				 0xe01dc
/* [RW 2] The size of AG context region 0 in REG-pairs. Designates the MS
   REG-pair number (e.g. if region 0 is 6 REG-pairs; the value should be 5).
   Is used to determine the number of the AG context REG-pairs written back;
   when the Reg1WbFlg isn't set. */
#define UCM_REG_UCM_REG0_SZ					 0xe00dc
#define UCM_REG_UCM_STORM0_IFEN 				 0xe0004
#define UCM_REG_UCM_STORM1_IFEN 				 0xe0008
#define UCM_REG_UCM_TM_IFEN					 0xe0020
#define UCM_REG_UCM_UQM_IFEN					 0xe000c
#define UCM_REG_UCM_UQM_USE_Q					 0xe00d8
#define UCM_REG_UQM_INIT_CRD					 0xe0220
#define UCM_REG_UQM_P_WEIGHT					 0xe00cc
#define UCM_REG_UQM_S_WEIGHT					 0xe00d0
#define UCM_REG_UQM_UCM_HDR_P					 0xe0094
#define UCM_REG_UQM_UCM_HDR_S					 0xe0098
#define UCM_REG_UQM_UCM_IFEN					 0xe0014
#define UCM_REG_USDM_IFEN					 0xe0018
#define UCM_REG_USDM_LENGTH_MIS 				 0xe0158
#define UCM_REG_USDM_WEIGHT					 0xe00c8
#define UCM_REG_XSEM_IFEN					 0xe002c
#define UCM_REG_XSEM_LENGTH_MIS 				 0xe0164
#define UCM_REG_XSEM_WEIGHT					 0xe00bc
#define UCM_REG_XX_DESCR_TABLE					 0xe0280
#define UCM_REG_XX_DESCR_TABLE_SIZE				 27
#define UCM_REG_XX_FREE 					 0xe016c
#define UCM_REG_XX_INIT_CRD					 0xe0224
#define UCM_REG_XX_MSG_NUM					 0xe0228
#define UCM_REG_XX_OVFL_EVNT_ID 				 0xe004c
#define UCM_REG_XX_TABLE					 0xe0300
#define UMAC_COMMAND_CONFIG_REG_HD_ENA				 (0x1<<10)
#define UMAC_COMMAND_CONFIG_REG_IGNORE_TX_PAUSE			 (0x1<<28)
#define UMAC_COMMAND_CONFIG_REG_LOOP_ENA			 (0x1<<15)
#define UMAC_COMMAND_CONFIG_REG_NO_LGTH_CHECK			 (0x1<<24)
#define UMAC_COMMAND_CONFIG_REG_PAD_EN				 (0x1<<5)
#define UMAC_COMMAND_CONFIG_REG_PAUSE_IGNORE			 (0x1<<8)
#define UMAC_COMMAND_CONFIG_REG_PROMIS_EN			 (0x1<<4)
#define UMAC_COMMAND_CONFIG_REG_RX_ENA				 (0x1<<1)
#define UMAC_COMMAND_CONFIG_REG_SW_RESET			 (0x1<<13)
#define UMAC_COMMAND_CONFIG_REG_TX_ENA				 (0x1<<0)
#define UMAC_REG_COMMAND_CONFIG					 0x8
#define UMAC_REG_MAC_ADDR0					 0xc
#define UMAC_REG_MAC_ADDR1					 0x10
#define UMAC_REG_MAXFR						 0x14
#define USDM_REG_AGG_INT_EVENT_0				 0xc4038
#define USDM_REG_AGG_INT_EVENT_1				 0xc403c
#define USDM_REG_AGG_INT_EVENT_2				 0xc4040
#define USDM_REG_AGG_INT_EVENT_4				 0xc4048
#define USDM_REG_AGG_INT_EVENT_5				 0xc404c
#define USDM_REG_AGG_INT_EVENT_6				 0xc4050
#define USDM_REG_AGG_INT_MODE_0 				 0xc41b8
#define USDM_REG_AGG_INT_MODE_1 				 0xc41bc
#define USDM_REG_AGG_INT_MODE_4 				 0xc41c8
#define USDM_REG_AGG_INT_MODE_5 				 0xc41cc
#define USDM_REG_AGG_INT_MODE_6 				 0xc41d0
#define USDM_REG_AGG_INT_T_5					 0xc40cc
#define USDM_REG_AGG_INT_T_6					 0xc40d0
#define USDM_REG_CFC_RSP_START_ADDR				 0xc4008
#define USDM_REG_CMP_COUNTER_MAX0				 0xc401c
#define USDM_REG_CMP_COUNTER_MAX1				 0xc4020
#define USDM_REG_CMP_COUNTER_MAX2				 0xc4024
#define USDM_REG_CMP_COUNTER_MAX3				 0xc4028
#define USDM_REG_CMP_COUNTER_START_ADDR 			 0xc400c
#define USDM_REG_ENABLE_IN1					 0xc4238
#define USDM_REG_ENABLE_IN2					 0xc423c
#define USDM_REG_ENABLE_OUT1					 0xc4240
#define USDM_REG_ENABLE_OUT2					 0xc4244
#define USDM_REG_INIT_CREDIT_PXP_CTRL				 0xc44c0
#define USDM_REG_NUM_OF_ACK_AFTER_PLACE 			 0xc4280
#define USDM_REG_NUM_OF_PKT_END_MSG				 0xc4278
#define USDM_REG_NUM_OF_PXP_ASYNC_REQ				 0xc427c
#define USDM_REG_NUM_OF_Q0_CMD					 0xc4248
#define USDM_REG_NUM_OF_Q10_CMD 				 0xc4270
#define USDM_REG_NUM_OF_Q11_CMD 				 0xc4274
#define USDM_REG_NUM_OF_Q1_CMD					 0xc424c
#define USDM_REG_NUM_OF_Q2_CMD					 0xc4250
#define USDM_REG_NUM_OF_Q3_CMD					 0xc4254
#define USDM_REG_NUM_OF_Q4_CMD					 0xc4258
#define USDM_REG_NUM_OF_Q5_CMD					 0xc425c
#define USDM_REG_NUM_OF_Q6_CMD					 0xc4260
#define USDM_REG_NUM_OF_Q7_CMD					 0xc4264
#define USDM_REG_NUM_OF_Q8_CMD					 0xc4268
#define USDM_REG_NUM_OF_Q9_CMD					 0xc426c
#define USDM_REG_PCK_END_MSG_START_ADDR 			 0xc4014
#define USDM_REG_Q_COUNTER_START_ADDR				 0xc4010
#define USDM_REG_RSP_PXP_CTRL_RDATA_EMPTY			 0xc4550
#define USDM_REG_SYNC_PARSER_EMPTY				 0xc4558
#define USDM_REG_SYNC_SYNC_EMPTY				 0xc4560
#define USDM_REG_TIMER_TICK					 0xc4000
#define USDM_REG_USDM_INT_MASK_0				 0xc42a0
#define USDM_REG_USDM_INT_MASK_1				 0xc42b0
#define USDM_REG_USDM_INT_STS_0 				 0xc4294
#define USDM_REG_USDM_INT_STS_1 				 0xc42a4
#define USDM_REG_USDM_PRTY_MASK 				 0xc42c0
#define USDM_REG_USDM_PRTY_STS					 0xc42b4
#define USDM_REG_USDM_PRTY_STS_CLR				 0xc42b8
#define USEM_REG_ARB_CYCLE_SIZE 				 0x300034
#define USEM_REG_ARB_ELEMENT0					 0x300020
#define USEM_REG_ARB_ELEMENT1					 0x300024
#define USEM_REG_ARB_ELEMENT2					 0x300028
#define USEM_REG_ARB_ELEMENT3					 0x30002c
#define USEM_REG_ARB_ELEMENT4					 0x300030
#define USEM_REG_ENABLE_IN					 0x3000a4
#define USEM_REG_ENABLE_OUT					 0x3000a8
#define USEM_REG_FAST_MEMORY					 0x320000
#define USEM_REG_FIC0_DISABLE					 0x300224
#define USEM_REG_FIC1_DISABLE					 0x300234
#define USEM_REG_INT_TABLE					 0x300400
#define USEM_REG_MSG_NUM_FIC0					 0x300000
#define USEM_REG_MSG_NUM_FIC1					 0x300004
#define USEM_REG_MSG_NUM_FOC0					 0x300008
#define USEM_REG_MSG_NUM_FOC1					 0x30000c
#define USEM_REG_MSG_NUM_FOC2					 0x300010
#define USEM_REG_MSG_NUM_FOC3					 0x300014
#define USEM_REG_PAS_DISABLE					 0x30024c
#define USEM_REG_PASSIVE_BUFFER 				 0x302000
#define USEM_REG_PRAM						 0x340000
#define USEM_REG_SLEEP_THREADS_VALID				 0x30026c
#define USEM_REG_SLOW_EXT_STORE_EMPTY				 0x3002a0
#define USEM_REG_THREADS_LIST					 0x3002e4
#define USEM_REG_TS_0_AS					 0x300038
#define USEM_REG_TS_10_AS					 0x300060
#define USEM_REG_TS_11_AS					 0x300064
#define USEM_REG_TS_12_AS					 0x300068
#define USEM_REG_TS_13_AS					 0x30006c
#define USEM_REG_TS_14_AS					 0x300070
#define USEM_REG_TS_15_AS					 0x300074
#define USEM_REG_TS_16_AS					 0x300078
#define USEM_REG_TS_17_AS					 0x30007c
#define USEM_REG_TS_18_AS					 0x300080
#define USEM_REG_TS_1_AS					 0x30003c
#define USEM_REG_TS_2_AS					 0x300040
#define USEM_REG_TS_3_AS					 0x300044
#define USEM_REG_TS_4_AS					 0x300048
#define USEM_REG_TS_5_AS					 0x30004c
#define USEM_REG_TS_6_AS					 0x300050
#define USEM_REG_TS_7_AS					 0x300054
#define USEM_REG_TS_8_AS					 0x300058
#define USEM_REG_TS_9_AS					 0x30005c
#define USEM_REG_USEM_INT_MASK_0				 0x300110
#define USEM_REG_USEM_INT_MASK_1				 0x300120
#define USEM_REG_USEM_INT_STS_0 				 0x300104
#define USEM_REG_USEM_INT_STS_1 				 0x300114
#define USEM_REG_USEM_PRTY_MASK_0				 0x300130
#define USEM_REG_USEM_PRTY_MASK_1				 0x300140
#define USEM_REG_USEM_PRTY_STS_0				 0x300124
#define USEM_REG_USEM_PRTY_STS_1				 0x300134
#define USEM_REG_USEM_PRTY_STS_CLR_0				 0x300128
#define USEM_REG_USEM_PRTY_STS_CLR_1				 0x300138
#define USEM_REG_VFPF_ERR_NUM					 0x300380
#define VFC_MEMORIES_RST_REG_CAM_RST				 (0x1<<0)
#define VFC_MEMORIES_RST_REG_RAM_RST				 (0x1<<1)
#define VFC_REG_MEMORIES_RST					 0x1943c
#define XCM_REG_AG_CTX						 0x28000
#define XCM_REG_AUX1_Q						 0x20134
#define XCM_REG_AUX_CNT_FLG_Q_19				 0x201b0
#define XCM_REG_CAM_OCCUP					 0x20244
#define XCM_REG_CDU_AG_RD_IFEN					 0x20044
#define XCM_REG_CDU_AG_WR_IFEN					 0x20040
#define XCM_REG_CDU_SM_RD_IFEN					 0x2004c
#define XCM_REG_CDU_SM_WR_IFEN					 0x20048
#define XCM_REG_CFC_INIT_CRD					 0x20404
#define XCM_REG_CP_WEIGHT					 0x200dc
#define XCM_REG_CSEM_IFEN					 0x20028
#define XCM_REG_CSEM_LENGTH_MIS 				 0x20228
#define XCM_REG_CSEM_WEIGHT					 0x200c4
#define XCM_REG_DORQ_IFEN					 0x20030
#define XCM_REG_DORQ_LENGTH_MIS 				 0x20230
#define XCM_REG_DORQ_WEIGHT					 0x200cc
#define XCM_REG_ERR_EVNT_ID					 0x200b0
#define XCM_REG_ERR_XCM_HDR					 0x200ac
#define XCM_REG_EXPR_EVNT_ID					 0x200b4
#define XCM_REG_FIC0_INIT_CRD					 0x2040c
#define XCM_REG_FIC1_INIT_CRD					 0x20410
#define XCM_REG_GLB_DEL_ACK_MAX_CNT_0				 0x20118
#define XCM_REG_GLB_DEL_ACK_MAX_CNT_1				 0x2011c
#define XCM_REG_GLB_DEL_ACK_TMR_VAL_0				 0x20108
#define XCM_REG_GLB_DEL_ACK_TMR_VAL_1				 0x2010c
#define XCM_REG_GR_ARB_TYPE					 0x2020c
#define XCM_REG_GR_LD0_PR					 0x20214
#define XCM_REG_GR_LD1_PR					 0x20218
#define XCM_REG_NIG0_IFEN					 0x20038
#define XCM_REG_NIG0_LENGTH_MIS 				 0x20238
#define XCM_REG_NIG0_WEIGHT					 0x200d4
#define XCM_REG_NIG1_IFEN					 0x2003c
#define XCM_REG_NIG1_LENGTH_MIS 				 0x2023c
#define XCM_REG_N_SM_CTX_LD_0					 0x20060
#define XCM_REG_N_SM_CTX_LD_1					 0x20064
#define XCM_REG_N_SM_CTX_LD_2					 0x20068
#define XCM_REG_N_SM_CTX_LD_3					 0x2006c
#define XCM_REG_N_SM_CTX_LD_4					 0x20070
#define XCM_REG_N_SM_CTX_LD_5					 0x20074
#define XCM_REG_PBF_IFEN					 0x20034
#define XCM_REG_PBF_LENGTH_MIS					 0x20234
#define XCM_REG_PBF_WEIGHT					 0x200d0
#define XCM_REG_PHYS_QNUM3_0					 0x20100
#define XCM_REG_PHYS_QNUM3_1					 0x20104
#define XCM_REG_STOP_EVNT_ID					 0x200b8
#define XCM_REG_STORM_LENGTH_MIS				 0x2021c
#define XCM_REG_STORM_WEIGHT					 0x200bc
#define XCM_REG_STORM_XCM_IFEN					 0x20010
#define XCM_REG_TM_INIT_CRD					 0x2041c
#define XCM_REG_TM_WEIGHT					 0x200ec
#define XCM_REG_TM_XCM_HDR					 0x200a8
#define XCM_REG_TM_XCM_IFEN					 0x2001c
#define XCM_REG_TSEM_IFEN					 0x20024
#define XCM_REG_TSEM_LENGTH_MIS 				 0x20224
#define XCM_REG_TSEM_WEIGHT					 0x200c0
#define XCM_REG_UNA_GT_NXT_Q					 0x20120
#define XCM_REG_USEM_IFEN					 0x2002c
#define XCM_REG_USEM_LENGTH_MIS 				 0x2022c
#define XCM_REG_USEM_WEIGHT					 0x200c8
#define XCM_REG_WU_DA_CNT_CMD00 				 0x201d4
#define XCM_REG_WU_DA_CNT_CMD01 				 0x201d8
#define XCM_REG_WU_DA_CNT_CMD10 				 0x201dc
#define XCM_REG_WU_DA_CNT_CMD11 				 0x201e0
#define XCM_REG_WU_DA_CNT_UPD_VAL00				 0x201e4
#define XCM_REG_WU_DA_CNT_UPD_VAL01				 0x201e8
#define XCM_REG_WU_DA_CNT_UPD_VAL10				 0x201ec
#define XCM_REG_WU_DA_CNT_UPD_VAL11				 0x201f0
#define XCM_REG_WU_DA_SET_TMR_CNT_FLG_CMD00			 0x201c4
#define XCM_REG_WU_DA_SET_TMR_CNT_FLG_CMD01			 0x201c8
#define XCM_REG_WU_DA_SET_TMR_CNT_FLG_CMD10			 0x201cc
#define XCM_REG_WU_DA_SET_TMR_CNT_FLG_CMD11			 0x201d0
#define XCM_REG_XCM_CFC_IFEN					 0x20050
#define XCM_REG_XCM_INT_MASK					 0x202b4
#define XCM_REG_XCM_INT_STS					 0x202a8
#define XCM_REG_XCM_PRTY_MASK					 0x202c4
#define XCM_REG_XCM_PRTY_STS					 0x202b8
#define XCM_REG_XCM_PRTY_STS_CLR				 0x202bc

/* [RW 4] The size of AG context region 0 in REG-pairs. Designates the MS
   REG-pair number (e.g. if region 0 is 6 REG-pairs; the value should be 5).
   Is used to determine the number of the AG context REG-pairs written back;
   when the Reg1WbFlg isn't set. */
#define XCM_REG_XCM_REG0_SZ					 0x200f4
#define XCM_REG_XCM_STORM0_IFEN 				 0x20004
#define XCM_REG_XCM_STORM1_IFEN 				 0x20008
#define XCM_REG_XCM_TM_IFEN					 0x20020
#define XCM_REG_XCM_XQM_IFEN					 0x2000c
#define XCM_REG_XCM_XQM_USE_Q					 0x200f0
#define XCM_REG_XQM_BYP_ACT_UPD 				 0x200fc
#define XCM_REG_XQM_INIT_CRD					 0x20420
#define XCM_REG_XQM_P_WEIGHT					 0x200e4
#define XCM_REG_XQM_S_WEIGHT					 0x200e8
#define XCM_REG_XQM_XCM_HDR_P					 0x200a0
#define XCM_REG_XQM_XCM_HDR_S					 0x200a4
#define XCM_REG_XQM_XCM_IFEN					 0x20014
#define XCM_REG_XSDM_IFEN					 0x20018
#define XCM_REG_XSDM_LENGTH_MIS 				 0x20220
#define XCM_REG_XSDM_WEIGHT					 0x200e0
#define XCM_REG_XX_DESCR_TABLE					 0x20480
#define XCM_REG_XX_DESCR_TABLE_SIZE				 32
#define XCM_REG_XX_FREE 					 0x20240
#define XCM_REG_XX_INIT_CRD					 0x20424
#define XCM_REG_XX_MSG_NUM					 0x20428
#define XCM_REG_XX_OVFL_EVNT_ID 				 0x20058
#define XMAC_CLEAR_RX_LSS_STATUS_REG_CLEAR_LOCAL_FAULT_STATUS	 (0x1<<0)
#define XMAC_CLEAR_RX_LSS_STATUS_REG_CLEAR_REMOTE_FAULT_STATUS	 (0x1<<1)
#define XMAC_CTRL_REG_LINE_LOCAL_LPBK				 (0x1<<2)
#define XMAC_CTRL_REG_RX_EN					 (0x1<<1)
#define XMAC_CTRL_REG_SOFT_RESET				 (0x1<<6)
#define XMAC_CTRL_REG_TX_EN					 (0x1<<0)
#define XMAC_PAUSE_CTRL_REG_RX_PAUSE_EN				 (0x1<<18)
#define XMAC_PAUSE_CTRL_REG_TX_PAUSE_EN				 (0x1<<17)
#define XMAC_PFC_CTRL_HI_REG_FORCE_PFC_XON			 (0x1<<1)
#define XMAC_PFC_CTRL_HI_REG_PFC_REFRESH_EN			 (0x1<<0)
#define XMAC_PFC_CTRL_HI_REG_PFC_STATS_EN			 (0x1<<3)
#define XMAC_PFC_CTRL_HI_REG_RX_PFC_EN				 (0x1<<4)
#define XMAC_PFC_CTRL_HI_REG_TX_PFC_EN				 (0x1<<5)
#define XMAC_REG_CLEAR_RX_LSS_STATUS				 0x60
#define XMAC_REG_CTRL						 0
#define XMAC_REG_CTRL_SA_HI					 0x2c
#define XMAC_REG_CTRL_SA_LO					 0x28
#define XMAC_REG_PAUSE_CTRL					 0x68
#define XMAC_REG_PFC_CTRL					 0x70
#define XMAC_REG_PFC_CTRL_HI					 0x74
#define XMAC_REG_RX_LSS_STATUS					 0x58
#define XMAC_REG_RX_MAX_SIZE					 0x40
#define XMAC_REG_TX_CTRL					 0x20
#define XCM_REG_XX_TABLE					 0x20500
#define XSDM_REG_AGG_INT_EVENT_0				 0x166038
#define XSDM_REG_AGG_INT_EVENT_1				 0x16603c
#define XSDM_REG_AGG_INT_EVENT_10				 0x166060
#define XSDM_REG_AGG_INT_EVENT_11				 0x166064
#define XSDM_REG_AGG_INT_EVENT_12				 0x166068
#define XSDM_REG_AGG_INT_EVENT_13				 0x16606c
#define XSDM_REG_AGG_INT_EVENT_14				 0x166070
#define XSDM_REG_AGG_INT_EVENT_2				 0x166040
#define XSDM_REG_AGG_INT_EVENT_3				 0x166044
#define XSDM_REG_AGG_INT_EVENT_4				 0x166048
#define XSDM_REG_AGG_INT_EVENT_5				 0x16604c
#define XSDM_REG_AGG_INT_EVENT_6				 0x166050
#define XSDM_REG_AGG_INT_EVENT_7				 0x166054
#define XSDM_REG_AGG_INT_EVENT_8				 0x166058
#define XSDM_REG_AGG_INT_EVENT_9				 0x16605c
#define XSDM_REG_AGG_INT_MODE_0 				 0x1661b8
#define XSDM_REG_AGG_INT_MODE_1 				 0x1661bc
#define XSDM_REG_CFC_RSP_START_ADDR				 0x166008
#define XSDM_REG_CMP_COUNTER_MAX0				 0x16601c
#define XSDM_REG_CMP_COUNTER_MAX1				 0x166020
#define XSDM_REG_CMP_COUNTER_MAX2				 0x166024
#define XSDM_REG_CMP_COUNTER_MAX3				 0x166028
#define XSDM_REG_CMP_COUNTER_START_ADDR 			 0x16600c
#define XSDM_REG_ENABLE_IN1					 0x166238
#define XSDM_REG_ENABLE_IN2					 0x16623c
#define XSDM_REG_ENABLE_OUT1					 0x166240
#define XSDM_REG_ENABLE_OUT2					 0x166244
#define XSDM_REG_INIT_CREDIT_PXP_CTRL				 0x1664bc
#define XSDM_REG_NUM_OF_ACK_AFTER_PLACE 			 0x16627c
#define XSDM_REG_NUM_OF_PKT_END_MSG				 0x166274
#define XSDM_REG_NUM_OF_PXP_ASYNC_REQ				 0x166278
#define XSDM_REG_NUM_OF_Q0_CMD					 0x166248
#define XSDM_REG_NUM_OF_Q10_CMD 				 0x16626c
#define XSDM_REG_NUM_OF_Q11_CMD 				 0x166270
#define XSDM_REG_NUM_OF_Q1_CMD					 0x16624c
#define XSDM_REG_NUM_OF_Q3_CMD					 0x166250
#define XSDM_REG_NUM_OF_Q4_CMD					 0x166254
#define XSDM_REG_NUM_OF_Q5_CMD					 0x166258
#define XSDM_REG_NUM_OF_Q6_CMD					 0x16625c
#define XSDM_REG_NUM_OF_Q7_CMD					 0x166260
#define XSDM_REG_NUM_OF_Q8_CMD					 0x166264
#define XSDM_REG_NUM_OF_Q9_CMD					 0x166268
#define XSDM_REG_Q_COUNTER_START_ADDR				 0x166010
#define XSDM_REG_OPERATION_GEN					 0x1664c4
#define XSDM_REG_RSP_PXP_CTRL_RDATA_EMPTY			 0x166548
#define XSDM_REG_SYNC_PARSER_EMPTY				 0x166550
#define XSDM_REG_SYNC_SYNC_EMPTY				 0x166558
#define XSDM_REG_TIMER_TICK					 0x166000
#define XSDM_REG_XSDM_INT_MASK_0				 0x16629c
#define XSDM_REG_XSDM_INT_MASK_1				 0x1662ac
#define XSDM_REG_XSDM_INT_STS_0 				 0x166290
#define XSDM_REG_XSDM_INT_STS_1 				 0x1662a0
#define XSDM_REG_XSDM_PRTY_MASK 				 0x1662bc
#define XSDM_REG_XSDM_PRTY_STS					 0x1662b0
#define XSDM_REG_XSDM_PRTY_STS_CLR				 0x1662b4
#define XSEM_REG_ARB_CYCLE_SIZE 				 0x280034
#define XSEM_REG_ARB_ELEMENT0					 0x280020
#define XSEM_REG_ARB_ELEMENT1					 0x280024
#define XSEM_REG_ARB_ELEMENT2					 0x280028
#define XSEM_REG_ARB_ELEMENT3					 0x28002c
#define XSEM_REG_ARB_ELEMENT4					 0x280030
#define XSEM_REG_ENABLE_IN					 0x2800a4
#define XSEM_REG_ENABLE_OUT					 0x2800a8
#define XSEM_REG_FAST_MEMORY					 0x2a0000
#define XSEM_REG_FIC0_DISABLE					 0x280224
#define XSEM_REG_FIC1_DISABLE					 0x280234
#define XSEM_REG_INT_TABLE					 0x280400
#define XSEM_REG_MSG_NUM_FIC0					 0x280000
#define XSEM_REG_MSG_NUM_FIC1					 0x280004
#define XSEM_REG_MSG_NUM_FOC0					 0x280008
#define XSEM_REG_MSG_NUM_FOC1					 0x28000c
#define XSEM_REG_MSG_NUM_FOC2					 0x280010
#define XSEM_REG_MSG_NUM_FOC3					 0x280014
#define XSEM_REG_PAS_DISABLE					 0x28024c
#define XSEM_REG_PASSIVE_BUFFER 				 0x282000
#define XSEM_REG_PRAM						 0x2c0000
#define XSEM_REG_SLEEP_THREADS_VALID				 0x28026c
#define XSEM_REG_SLOW_EXT_STORE_EMPTY				 0x2802a0
#define XSEM_REG_THREADS_LIST					 0x2802e4
#define XSEM_REG_TS_0_AS					 0x280038
#define XSEM_REG_TS_10_AS					 0x280060
#define XSEM_REG_TS_11_AS					 0x280064
#define XSEM_REG_TS_12_AS					 0x280068
#define XSEM_REG_TS_13_AS					 0x28006c
#define XSEM_REG_TS_14_AS					 0x280070
#define XSEM_REG_TS_15_AS					 0x280074
#define XSEM_REG_TS_16_AS					 0x280078
#define XSEM_REG_TS_17_AS					 0x28007c
#define XSEM_REG_TS_18_AS					 0x280080
#define XSEM_REG_TS_1_AS					 0x28003c
#define XSEM_REG_TS_2_AS					 0x280040
#define XSEM_REG_TS_3_AS					 0x280044
#define XSEM_REG_TS_4_AS					 0x280048
#define XSEM_REG_TS_5_AS					 0x28004c
#define XSEM_REG_TS_6_AS					 0x280050
#define XSEM_REG_TS_7_AS					 0x280054
#define XSEM_REG_TS_8_AS					 0x280058
#define XSEM_REG_TS_9_AS					 0x28005c
#define XSEM_REG_VFPF_ERR_NUM					 0x280380
#define XSEM_REG_XSEM_INT_MASK_0				 0x280110
#define XSEM_REG_XSEM_INT_MASK_1				 0x280120
#define XSEM_REG_XSEM_INT_STS_0 				 0x280104
#define XSEM_REG_XSEM_INT_STS_1 				 0x280114
#define XSEM_REG_XSEM_PRTY_MASK_0				 0x280130
#define XSEM_REG_XSEM_PRTY_MASK_1				 0x280140
#define XSEM_REG_XSEM_PRTY_STS_0				 0x280124
#define XSEM_REG_XSEM_PRTY_STS_1				 0x280134
#define XSEM_REG_XSEM_PRTY_STS_CLR_0				 0x280128
#define XSEM_REG_XSEM_PRTY_STS_CLR_1				 0x280138
#define MCPR_ACCESS_LOCK_LOCK					 (1L<<31)
#define MCPR_NVM_ACCESS_ENABLE_EN				 (1L<<0)
#define MCPR_NVM_ACCESS_ENABLE_WR_EN				 (1L<<1)
#define MCPR_NVM_ADDR_NVM_ADDR_VALUE				 (0xffffffL<<0)
#define MCPR_NVM_CFG4_FLASH_SIZE				 (0x7L<<0)
#define MCPR_NVM_COMMAND_DOIT					 (1L<<4)
#define MCPR_NVM_COMMAND_DONE					 (1L<<3)
#define MCPR_NVM_COMMAND_FIRST					 (1L<<7)
#define MCPR_NVM_COMMAND_LAST					 (1L<<8)
#define MCPR_NVM_COMMAND_WR					 (1L<<5)
#define MCPR_NVM_SW_ARB_ARB_ARB1				 (1L<<9)
#define MCPR_NVM_SW_ARB_ARB_REQ_CLR1				 (1L<<5)
#define MCPR_NVM_SW_ARB_ARB_REQ_SET1				 (1L<<1)
#define BIGMAC_REGISTER_BMAC_CONTROL				 (0x00<<3)
#define BIGMAC_REGISTER_BMAC_XGXS_CONTROL			 (0x01<<3)
#define BIGMAC_REGISTER_CNT_MAX_SIZE				 (0x05<<3)
#define BIGMAC_REGISTER_RX_CONTROL				 (0x21<<3)
#define BIGMAC_REGISTER_RX_LLFC_MSG_FLDS			 (0x46<<3)
#define BIGMAC_REGISTER_RX_LSS_STATUS				 (0x43<<3)
#define BIGMAC_REGISTER_RX_MAX_SIZE				 (0x23<<3)
#define BIGMAC_REGISTER_RX_STAT_GR64				 (0x26<<3)
#define BIGMAC_REGISTER_RX_STAT_GRIPJ				 (0x42<<3)
#define BIGMAC_REGISTER_TX_CONTROL				 (0x07<<3)
#define BIGMAC_REGISTER_TX_MAX_SIZE				 (0x09<<3)
#define BIGMAC_REGISTER_TX_PAUSE_THRESHOLD			 (0x0A<<3)
#define BIGMAC_REGISTER_TX_SOURCE_ADDR				 (0x08<<3)
#define BIGMAC_REGISTER_TX_STAT_GTBYT				 (0x20<<3)
#define BIGMAC_REGISTER_TX_STAT_GTPKT				 (0x0C<<3)
#define BIGMAC2_REGISTER_BMAC_CONTROL				 (0x00<<3)
#define BIGMAC2_REGISTER_BMAC_XGXS_CONTROL			 (0x01<<3)
#define BIGMAC2_REGISTER_CNT_MAX_SIZE				 (0x05<<3)
#define BIGMAC2_REGISTER_PFC_CONTROL				 (0x06<<3)
#define BIGMAC2_REGISTER_RX_CONTROL				 (0x3A<<3)
#define BIGMAC2_REGISTER_RX_LLFC_MSG_FLDS			 (0x62<<3)
#define BIGMAC2_REGISTER_RX_LSS_STAT				 (0x3E<<3)
#define BIGMAC2_REGISTER_RX_MAX_SIZE				 (0x3C<<3)
#define BIGMAC2_REGISTER_RX_STAT_GR64				 (0x40<<3)
#define BIGMAC2_REGISTER_RX_STAT_GRIPJ				 (0x5f<<3)
#define BIGMAC2_REGISTER_RX_STAT_GRPP				 (0x51<<3)
#define BIGMAC2_REGISTER_TX_CONTROL				 (0x1C<<3)
#define BIGMAC2_REGISTER_TX_MAX_SIZE				 (0x1E<<3)
#define BIGMAC2_REGISTER_TX_PAUSE_CONTROL			 (0x20<<3)
#define BIGMAC2_REGISTER_TX_SOURCE_ADDR			 (0x1D<<3)
#define BIGMAC2_REGISTER_TX_STAT_GTBYT				 (0x39<<3)
#define BIGMAC2_REGISTER_TX_STAT_GTPOK				 (0x22<<3)
#define BIGMAC2_REGISTER_TX_STAT_GTPP				 (0x24<<3)
#define EMAC_LED_1000MB_OVERRIDE				 (1L<<1)
#define EMAC_LED_100MB_OVERRIDE 				 (1L<<2)
#define EMAC_LED_10MB_OVERRIDE					 (1L<<3)
#define EMAC_LED_2500MB_OVERRIDE				 (1L<<12)
#define EMAC_LED_OVERRIDE					 (1L<<0)
#define EMAC_LED_TRAFFIC					 (1L<<6)
#define EMAC_MDIO_COMM_COMMAND_ADDRESS				 (0L<<26)
#define EMAC_MDIO_COMM_COMMAND_READ_22				 (2L<<26)
#define EMAC_MDIO_COMM_COMMAND_READ_45				 (3L<<26)
#define EMAC_MDIO_COMM_COMMAND_WRITE_22				 (1L<<26)
#define EMAC_MDIO_COMM_COMMAND_WRITE_45 			 (1L<<26)
#define EMAC_MDIO_COMM_DATA					 (0xffffL<<0)
#define EMAC_MDIO_COMM_START_BUSY				 (1L<<29)
#define EMAC_MDIO_MODE_AUTO_POLL				 (1L<<4)
#define EMAC_MDIO_MODE_CLAUSE_45				 (1L<<31)
#define EMAC_MDIO_MODE_CLOCK_CNT				 (0x3ffL<<16)
#define EMAC_MDIO_MODE_CLOCK_CNT_BITSHIFT			 16
#define EMAC_MDIO_STATUS_10MB					 (1L<<1)
#define EMAC_MODE_25G_MODE					 (1L<<5)
#define EMAC_MODE_HALF_DUPLEX					 (1L<<1)
#define EMAC_MODE_PORT_GMII					 (2L<<2)
#define EMAC_MODE_PORT_MII					 (1L<<2)
#define EMAC_MODE_PORT_MII_10M					 (3L<<2)
#define EMAC_MODE_RESET 					 (1L<<0)
#define EMAC_REG_EMAC_LED					 0xc
#define EMAC_REG_EMAC_MAC_MATCH 				 0x10
#define EMAC_REG_EMAC_MDIO_COMM 				 0xac
#define EMAC_REG_EMAC_MDIO_MODE 				 0xb4
#define EMAC_REG_EMAC_MDIO_STATUS				 0xb0
#define EMAC_REG_EMAC_MODE					 0x0
#define EMAC_REG_EMAC_RX_MODE					 0xc8
#define EMAC_REG_EMAC_RX_MTU_SIZE				 0x9c
#define EMAC_REG_EMAC_RX_STAT_AC				 0x180
#define EMAC_REG_EMAC_RX_STAT_AC_28				 0x1f4
#define EMAC_REG_EMAC_RX_STAT_AC_COUNT				 23
#define EMAC_REG_EMAC_TX_MODE					 0xbc
#define EMAC_REG_EMAC_TX_STAT_AC				 0x280
#define EMAC_REG_EMAC_TX_STAT_AC_COUNT				 22
#define EMAC_REG_RX_PFC_MODE					 0x320
#define EMAC_REG_RX_PFC_MODE_PRIORITIES				 (1L<<2)
#define EMAC_REG_RX_PFC_MODE_RX_EN				 (1L<<1)
#define EMAC_REG_RX_PFC_MODE_TX_EN				 (1L<<0)
#define EMAC_REG_RX_PFC_PARAM					 0x324
#define EMAC_REG_RX_PFC_PARAM_OPCODE_BITSHIFT			 0
#define EMAC_REG_RX_PFC_PARAM_PRIORITY_EN_BITSHIFT		 16
#define EMAC_REG_RX_PFC_STATS_XOFF_RCVD				 0x328
#define EMAC_REG_RX_PFC_STATS_XOFF_RCVD_COUNT			 (0xffff<<0)
#define EMAC_REG_RX_PFC_STATS_XOFF_SENT				 0x330
#define EMAC_REG_RX_PFC_STATS_XOFF_SENT_COUNT			 (0xffff<<0)
#define EMAC_REG_RX_PFC_STATS_XON_RCVD				 0x32c
#define EMAC_REG_RX_PFC_STATS_XON_RCVD_COUNT			 (0xffff<<0)
#define EMAC_REG_RX_PFC_STATS_XON_SENT				 0x334
#define EMAC_REG_RX_PFC_STATS_XON_SENT_COUNT			 (0xffff<<0)
#define EMAC_RX_MODE_FLOW_EN					 (1L<<2)
#define EMAC_RX_MODE_KEEP_MAC_CONTROL				 (1L<<3)
#define EMAC_RX_MODE_KEEP_VLAN_TAG				 (1L<<10)
#define EMAC_RX_MODE_PROMISCUOUS				 (1L<<8)
#define EMAC_RX_MODE_RESET					 (1L<<0)
#define EMAC_RX_MTU_SIZE_JUMBO_ENA				 (1L<<31)
#define EMAC_TX_MODE_EXT_PAUSE_EN				 (1L<<3)
#define EMAC_TX_MODE_FLOW_EN					 (1L<<4)
#define EMAC_TX_MODE_RESET					 (1L<<0)
#define MISC_REGISTERS_GPIO_0					 0
#define MISC_REGISTERS_GPIO_1					 1
#define MISC_REGISTERS_GPIO_2					 2
#define MISC_REGISTERS_GPIO_3					 3
#define MISC_REGISTERS_GPIO_CLR_POS				 16
#define MISC_REGISTERS_GPIO_FLOAT				 (0xffL<<24)
#define MISC_REGISTERS_GPIO_FLOAT_POS				 24
#define MISC_REGISTERS_GPIO_HIGH				 1
#define MISC_REGISTERS_GPIO_INPUT_HI_Z				 2
#define MISC_REGISTERS_GPIO_INT_CLR_POS 			 24
#define MISC_REGISTERS_GPIO_INT_OUTPUT_CLR			 0
#define MISC_REGISTERS_GPIO_INT_OUTPUT_SET			 1
#define MISC_REGISTERS_GPIO_INT_SET_POS 			 16
#define MISC_REGISTERS_GPIO_LOW 				 0
#define MISC_REGISTERS_GPIO_OUTPUT_HIGH 			 1
#define MISC_REGISTERS_GPIO_OUTPUT_LOW				 0
#define MISC_REGISTERS_GPIO_PORT_SHIFT				 4
#define MISC_REGISTERS_GPIO_SET_POS				 8
#define MISC_REGISTERS_RESET_REG_1_CLEAR			 0x588
#define MISC_REGISTERS_RESET_REG_1_RST_BRB1			 (0x1<<0)
#define MISC_REGISTERS_RESET_REG_1_RST_DORQ			 (0x1<<19)
#define MISC_REGISTERS_RESET_REG_1_RST_HC			 (0x1<<29)
#define MISC_REGISTERS_RESET_REG_1_RST_NIG			 (0x1<<7)
#define MISC_REGISTERS_RESET_REG_1_RST_PXP			 (0x1<<26)
#define MISC_REGISTERS_RESET_REG_1_RST_PXPV			 (0x1<<27)
#define MISC_REGISTERS_RESET_REG_1_SET				 0x584
#define MISC_REGISTERS_RESET_REG_2_CLEAR			 0x598
#define MISC_REGISTERS_RESET_REG_2_MSTAT0			 (0x1<<24)
#define MISC_REGISTERS_RESET_REG_2_MSTAT1			 (0x1<<25)
#define MISC_REGISTERS_RESET_REG_2_PGLC				 (0x1<<19)
#define MISC_REGISTERS_RESET_REG_2_RST_ATC			 (0x1<<17)
#define MISC_REGISTERS_RESET_REG_2_RST_BMAC0			 (0x1<<0)
#define MISC_REGISTERS_RESET_REG_2_RST_BMAC1			 (0x1<<1)
#define MISC_REGISTERS_RESET_REG_2_RST_EMAC0			 (0x1<<2)
#define MISC_REGISTERS_RESET_REG_2_RST_EMAC0_HARD_CORE		 (0x1<<14)
#define MISC_REGISTERS_RESET_REG_2_RST_EMAC1			 (0x1<<3)
#define MISC_REGISTERS_RESET_REG_2_RST_EMAC1_HARD_CORE		 (0x1<<15)
#define MISC_REGISTERS_RESET_REG_2_RST_GRC			 (0x1<<4)
#define MISC_REGISTERS_RESET_REG_2_RST_MCP_N_HARD_CORE_RST_B	 (0x1<<6)
#define MISC_REGISTERS_RESET_REG_2_RST_MCP_N_RESET_CMN_CORE	 (0x1<<8)
#define MISC_REGISTERS_RESET_REG_2_RST_MCP_N_RESET_CMN_CPU	 (0x1<<7)
#define MISC_REGISTERS_RESET_REG_2_RST_MCP_N_RESET_REG_HARD_CORE (0x1<<5)
#define MISC_REGISTERS_RESET_REG_2_RST_MDIO			 (0x1<<13)
#define MISC_REGISTERS_RESET_REG_2_RST_MISC_CORE		 (0x1<<11)
#define MISC_REGISTERS_RESET_REG_2_RST_PCI_MDIO			 (0x1<<13)
#define MISC_REGISTERS_RESET_REG_2_RST_RBCN			 (0x1<<9)
#define MISC_REGISTERS_RESET_REG_2_SET				 0x594
#define MISC_REGISTERS_RESET_REG_2_UMAC0			 (0x1<<20)
#define MISC_REGISTERS_RESET_REG_2_UMAC1			 (0x1<<21)
#define MISC_REGISTERS_RESET_REG_2_XMAC				 (0x1<<22)
#define MISC_REGISTERS_RESET_REG_2_XMAC_SOFT			 (0x1<<23)
#define MISC_REGISTERS_RESET_REG_3_CLEAR			 0x5a8
#define MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_IDDQ	 (0x1<<1)
#define MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_PWRDWN	 (0x1<<2)
#define MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_PWRDWN_SD (0x1<<3)
#define MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_SERDES0_RSTB_HW  (0x1<<0)
#define MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_IDDQ	 (0x1<<5)
#define MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_PWRDWN	 (0x1<<6)
#define MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_PWRDWN_SD  (0x1<<7)
#define MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_RSTB_HW	 (0x1<<4)
#define MISC_REGISTERS_RESET_REG_3_MISC_NIG_MUX_XGXS0_TXD_FIFO_RSTB (0x1<<8)
#define MISC_REGISTERS_RESET_REG_3_SET				 0x5a4
#define MISC_REGISTERS_SPIO_4					 4
#define MISC_REGISTERS_SPIO_5					 5
#define MISC_REGISTERS_SPIO_7					 7
#define MISC_REGISTERS_SPIO_CLR_POS				 16
#define MISC_REGISTERS_SPIO_FLOAT				 (0xffL<<24)
#define MISC_REGISTERS_SPIO_FLOAT_POS				 24
#define MISC_REGISTERS_SPIO_INPUT_HI_Z				 2
#define MISC_REGISTERS_SPIO_INT_OLD_SET_POS			 16
#define MISC_REGISTERS_SPIO_OUTPUT_HIGH 			 1
#define MISC_REGISTERS_SPIO_OUTPUT_LOW				 0
#define MISC_REGISTERS_SPIO_SET_POS				 8
#define HW_LOCK_MAX_RESOURCE_VALUE				 31
#define HW_LOCK_RESOURCE_DRV_FLAGS				 10
#define HW_LOCK_RESOURCE_GPIO					 1
#define HW_LOCK_RESOURCE_MDIO					 0
#define HW_LOCK_RESOURCE_NVRAM					 12
#define HW_LOCK_RESOURCE_PORT0_ATT_MASK				 3
#define HW_LOCK_RESOURCE_RECOVERY_LEADER_0			 8
#define HW_LOCK_RESOURCE_RECOVERY_LEADER_1			 9
#define HW_LOCK_RESOURCE_RECOVERY_REG				 11
#define HW_LOCK_RESOURCE_RESET					 5
#define HW_LOCK_RESOURCE_SPIO					 2
#define AEU_INPUTS_ATTN_BITS_ATC_HW_INTERRUPT			 (0x1<<4)
#define AEU_INPUTS_ATTN_BITS_ATC_PARITY_ERROR			 (0x1<<5)
#define AEU_INPUTS_ATTN_BITS_BRB_PARITY_ERROR			 (0x1<<18)
#define AEU_INPUTS_ATTN_BITS_CCM_HW_INTERRUPT			 (0x1<<31)
#define AEU_INPUTS_ATTN_BITS_CCM_PARITY_ERROR			 (0x1<<30)
#define AEU_INPUTS_ATTN_BITS_CDU_HW_INTERRUPT			 (0x1<<9)
#define AEU_INPUTS_ATTN_BITS_CDU_PARITY_ERROR			 (0x1<<8)
#define AEU_INPUTS_ATTN_BITS_CFC_HW_INTERRUPT			 (0x1<<7)
#define AEU_INPUTS_ATTN_BITS_CFC_PARITY_ERROR			 (0x1<<6)
#define AEU_INPUTS_ATTN_BITS_CSDM_HW_INTERRUPT			 (0x1<<29)
#define AEU_INPUTS_ATTN_BITS_CSDM_PARITY_ERROR			 (0x1<<28)
#define AEU_INPUTS_ATTN_BITS_CSEMI_HW_INTERRUPT			 (0x1<<1)
#define AEU_INPUTS_ATTN_BITS_CSEMI_PARITY_ERROR			 (0x1<<0)
#define AEU_INPUTS_ATTN_BITS_DEBUG_PARITY_ERROR			 (0x1<<18)
#define AEU_INPUTS_ATTN_BITS_DMAE_HW_INTERRUPT			 (0x1<<11)
#define AEU_INPUTS_ATTN_BITS_DMAE_PARITY_ERROR			 (0x1<<10)
#define AEU_INPUTS_ATTN_BITS_DOORBELLQ_HW_INTERRUPT		 (0x1<<13)
#define AEU_INPUTS_ATTN_BITS_DOORBELLQ_PARITY_ERROR		 (0x1<<12)
#define AEU_INPUTS_ATTN_BITS_GPIO0_FUNCTION_0			 (0x1<<2)
#define AEU_INPUTS_ATTN_BITS_IGU_PARITY_ERROR			 (0x1<<12)
#define AEU_INPUTS_ATTN_BITS_MCP_LATCHED_ROM_PARITY		 (0x1<<28)
#define AEU_INPUTS_ATTN_BITS_MCP_LATCHED_SCPAD_PARITY		 (0x1<<31)
#define AEU_INPUTS_ATTN_BITS_MCP_LATCHED_UMP_RX_PARITY		 (0x1<<29)
#define AEU_INPUTS_ATTN_BITS_MCP_LATCHED_UMP_TX_PARITY		 (0x1<<30)
#define AEU_INPUTS_ATTN_BITS_MISC_HW_INTERRUPT			 (0x1<<15)
#define AEU_INPUTS_ATTN_BITS_MISC_PARITY_ERROR			 (0x1<<14)
#define AEU_INPUTS_ATTN_BITS_NIG_PARITY_ERROR			 (0x1<<14)
#define AEU_INPUTS_ATTN_BITS_PARSER_PARITY_ERROR		 (0x1<<20)
#define AEU_INPUTS_ATTN_BITS_PBCLIENT_HW_INTERRUPT		 (0x1<<31)
#define AEU_INPUTS_ATTN_BITS_PBCLIENT_PARITY_ERROR		 (0x1<<30)
#define AEU_INPUTS_ATTN_BITS_PBF_PARITY_ERROR			 (0x1<<0)
#define AEU_INPUTS_ATTN_BITS_PGLUE_HW_INTERRUPT			 (0x1<<2)
#define AEU_INPUTS_ATTN_BITS_PGLUE_PARITY_ERROR			 (0x1<<3)
#define AEU_INPUTS_ATTN_BITS_PXPPCICLOCKCLIENT_HW_INTERRUPT	 (0x1<<5)
#define AEU_INPUTS_ATTN_BITS_PXPPCICLOCKCLIENT_PARITY_ERROR	 (0x1<<4)
#define AEU_INPUTS_ATTN_BITS_PXP_HW_INTERRUPT			 (0x1<<3)
#define AEU_INPUTS_ATTN_BITS_PXP_PARITY_ERROR			 (0x1<<2)
#define AEU_INPUTS_ATTN_BITS_QM_HW_INTERRUPT			 (0x1<<3)
#define AEU_INPUTS_ATTN_BITS_QM_PARITY_ERROR			 (0x1<<2)
#define AEU_INPUTS_ATTN_BITS_SEARCHER_PARITY_ERROR		 (0x1<<22)
#define AEU_INPUTS_ATTN_BITS_SPIO5				 (0x1<<15)
#define AEU_INPUTS_ATTN_BITS_TCM_HW_INTERRUPT			 (0x1<<27)
#define AEU_INPUTS_ATTN_BITS_TCM_PARITY_ERROR			 (0x1<<26)
#define AEU_INPUTS_ATTN_BITS_TIMERS_HW_INTERRUPT		 (0x1<<5)
#define AEU_INPUTS_ATTN_BITS_TIMERS_PARITY_ERROR		 (0x1<<4)
#define AEU_INPUTS_ATTN_BITS_TSDM_HW_INTERRUPT			 (0x1<<25)
#define AEU_INPUTS_ATTN_BITS_TSDM_PARITY_ERROR			 (0x1<<24)
#define AEU_INPUTS_ATTN_BITS_TSEMI_HW_INTERRUPT			 (0x1<<29)
#define AEU_INPUTS_ATTN_BITS_TSEMI_PARITY_ERROR			 (0x1<<28)
#define AEU_INPUTS_ATTN_BITS_UCM_HW_INTERRUPT			 (0x1<<23)
#define AEU_INPUTS_ATTN_BITS_UCM_PARITY_ERROR			 (0x1<<22)
#define AEU_INPUTS_ATTN_BITS_UPB_HW_INTERRUPT			 (0x1<<27)
#define AEU_INPUTS_ATTN_BITS_UPB_PARITY_ERROR			 (0x1<<26)
#define AEU_INPUTS_ATTN_BITS_USDM_HW_INTERRUPT			 (0x1<<21)
#define AEU_INPUTS_ATTN_BITS_USDM_PARITY_ERROR			 (0x1<<20)
#define AEU_INPUTS_ATTN_BITS_USEMI_HW_INTERRUPT			 (0x1<<25)
#define AEU_INPUTS_ATTN_BITS_USEMI_PARITY_ERROR			 (0x1<<24)
#define AEU_INPUTS_ATTN_BITS_VAUX_PCI_CORE_PARITY_ERROR		 (0x1<<16)
#define AEU_INPUTS_ATTN_BITS_XCM_HW_INTERRUPT			 (0x1<<9)
#define AEU_INPUTS_ATTN_BITS_XCM_PARITY_ERROR			 (0x1<<8)
#define AEU_INPUTS_ATTN_BITS_XSDM_HW_INTERRUPT			 (0x1<<7)
#define AEU_INPUTS_ATTN_BITS_XSDM_PARITY_ERROR			 (0x1<<6)
#define AEU_INPUTS_ATTN_BITS_XSEMI_HW_INTERRUPT			 (0x1<<11)
#define AEU_INPUTS_ATTN_BITS_XSEMI_PARITY_ERROR			 (0x1<<10)

#define AEU_INPUTS_ATTN_BITS_GPIO3_FUNCTION_0			(0x1<<5)
#define AEU_INPUTS_ATTN_BITS_GPIO3_FUNCTION_1			(0x1<<9)

#define RESERVED_GENERAL_ATTENTION_BIT_0	0

#define EVEREST_GEN_ATTN_IN_USE_MASK		0x7ffe0
#define EVEREST_LATCHED_ATTN_IN_USE_MASK	0xffe00000

#define RESERVED_GENERAL_ATTENTION_BIT_6	6
#define RESERVED_GENERAL_ATTENTION_BIT_7	7
#define RESERVED_GENERAL_ATTENTION_BIT_8	8
#define RESERVED_GENERAL_ATTENTION_BIT_9	9
#define RESERVED_GENERAL_ATTENTION_BIT_10	10
#define RESERVED_GENERAL_ATTENTION_BIT_11	11
#define RESERVED_GENERAL_ATTENTION_BIT_12	12
#define RESERVED_GENERAL_ATTENTION_BIT_13	13
#define RESERVED_GENERAL_ATTENTION_BIT_14	14
#define RESERVED_GENERAL_ATTENTION_BIT_15	15
#define RESERVED_GENERAL_ATTENTION_BIT_16	16
#define RESERVED_GENERAL_ATTENTION_BIT_17	17
#define RESERVED_GENERAL_ATTENTION_BIT_18	18
#define RESERVED_GENERAL_ATTENTION_BIT_19	19
#define RESERVED_GENERAL_ATTENTION_BIT_20	20
#define RESERVED_GENERAL_ATTENTION_BIT_21	21

#define TSTORM_FATAL_ASSERT_ATTENTION_BIT     RESERVED_GENERAL_ATTENTION_BIT_7
#define USTORM_FATAL_ASSERT_ATTENTION_BIT     RESERVED_GENERAL_ATTENTION_BIT_8
#define CSTORM_FATAL_ASSERT_ATTENTION_BIT     RESERVED_GENERAL_ATTENTION_BIT_9
#define XSTORM_FATAL_ASSERT_ATTENTION_BIT     RESERVED_GENERAL_ATTENTION_BIT_10

#define MCP_FATAL_ASSERT_ATTENTION_BIT	      RESERVED_GENERAL_ATTENTION_BIT_11

#define LINK_SYNC_ATTENTION_BIT_FUNC_0	    RESERVED_GENERAL_ATTENTION_BIT_12
#define LINK_SYNC_ATTENTION_BIT_FUNC_1	    RESERVED_GENERAL_ATTENTION_BIT_13
#define LINK_SYNC_ATTENTION_BIT_FUNC_2	    RESERVED_GENERAL_ATTENTION_BIT_14
#define LINK_SYNC_ATTENTION_BIT_FUNC_3	    RESERVED_GENERAL_ATTENTION_BIT_15
#define LINK_SYNC_ATTENTION_BIT_FUNC_4	    RESERVED_GENERAL_ATTENTION_BIT_16
#define LINK_SYNC_ATTENTION_BIT_FUNC_5	    RESERVED_GENERAL_ATTENTION_BIT_17
#define LINK_SYNC_ATTENTION_BIT_FUNC_6	    RESERVED_GENERAL_ATTENTION_BIT_18
#define LINK_SYNC_ATTENTION_BIT_FUNC_7	    RESERVED_GENERAL_ATTENTION_BIT_19


#define LATCHED_ATTN_RBCR			23
#define LATCHED_ATTN_RBCT			24
#define LATCHED_ATTN_RBCN			25
#define LATCHED_ATTN_RBCU			26
#define LATCHED_ATTN_RBCP			27
#define LATCHED_ATTN_TIMEOUT_GRC		28
#define LATCHED_ATTN_RSVD_GRC			29
#define LATCHED_ATTN_ROM_PARITY_MCP		30
#define LATCHED_ATTN_UM_RX_PARITY_MCP		31
#define LATCHED_ATTN_UM_TX_PARITY_MCP		32
#define LATCHED_ATTN_SCPAD_PARITY_MCP		33

#define GENERAL_ATTEN_WORD(atten_name)	       ((94 + atten_name) / 32)
#define GENERAL_ATTEN_OFFSET(atten_name)\
	(1UL << ((94 + atten_name) % 32))

#define GRCBASE_PXPCS		0x000000
#define GRCBASE_PCICONFIG	0x002000
#define GRCBASE_PCIREG		0x002400
#define GRCBASE_EMAC0		0x008000
#define GRCBASE_EMAC1		0x008400
#define GRCBASE_DBU		0x008800
#define GRCBASE_MISC		0x00A000
#define GRCBASE_DBG		0x00C000
#define GRCBASE_NIG		0x010000
#define GRCBASE_XCM		0x020000
#define GRCBASE_PRS		0x040000
#define GRCBASE_SRCH		0x040400
#define GRCBASE_TSDM		0x042000
#define GRCBASE_TCM		0x050000
#define GRCBASE_BRB1		0x060000
#define GRCBASE_MCP		0x080000
#define GRCBASE_UPB		0x0C1000
#define GRCBASE_CSDM		0x0C2000
#define GRCBASE_USDM		0x0C4000
#define GRCBASE_CCM		0x0D0000
#define GRCBASE_UCM		0x0E0000
#define GRCBASE_CDU		0x101000
#define GRCBASE_DMAE		0x102000
#define GRCBASE_PXP		0x103000
#define GRCBASE_CFC		0x104000
#define GRCBASE_HC		0x108000
#define GRCBASE_PXP2		0x120000
#define GRCBASE_PBF		0x140000
#define GRCBASE_UMAC0		0x160000
#define GRCBASE_UMAC1		0x160400
#define GRCBASE_XPB		0x161000
#define GRCBASE_MSTAT0	    0x162000
#define GRCBASE_MSTAT1	    0x162800
#define GRCBASE_XMAC0		0x163000
#define GRCBASE_XMAC1		0x163800
#define GRCBASE_TIMERS		0x164000
#define GRCBASE_XSDM		0x166000
#define GRCBASE_QM		0x168000
#define GRCBASE_DQ		0x170000
#define GRCBASE_TSEM		0x180000
#define GRCBASE_CSEM		0x200000
#define GRCBASE_XSEM		0x280000
#define GRCBASE_USEM		0x300000
#define GRCBASE_MISC_AEU	GRCBASE_MISC


#define PCICFG_OFFSET					0x2000
#define PCICFG_VENDOR_ID_OFFSET 			0x00
#define PCICFG_DEVICE_ID_OFFSET 			0x02
#define PCICFG_COMMAND_OFFSET				0x04
#define PCICFG_COMMAND_IO_SPACE 		(1<<0)
#define PCICFG_COMMAND_MEM_SPACE		(1<<1)
#define PCICFG_COMMAND_BUS_MASTER		(1<<2)
#define PCICFG_COMMAND_SPECIAL_CYCLES		(1<<3)
#define PCICFG_COMMAND_MWI_CYCLES		(1<<4)
#define PCICFG_COMMAND_VGA_SNOOP		(1<<5)
#define PCICFG_COMMAND_PERR_ENA 		(1<<6)
#define PCICFG_COMMAND_STEPPING 		(1<<7)
#define PCICFG_COMMAND_SERR_ENA 		(1<<8)
#define PCICFG_COMMAND_FAST_B2B 		(1<<9)
#define PCICFG_COMMAND_INT_DISABLE		(1<<10)
#define PCICFG_COMMAND_RESERVED 		(0x1f<<11)
#define PCICFG_STATUS_OFFSET				0x06
#define PCICFG_REVESION_ID_OFFSET			0x08
#define PCICFG_CACHE_LINE_SIZE				0x0c
#define PCICFG_LATENCY_TIMER				0x0d
#define PCICFG_BAR_1_LOW				0x10
#define PCICFG_BAR_1_HIGH				0x14
#define PCICFG_BAR_2_LOW				0x18
#define PCICFG_BAR_2_HIGH				0x1c
#define PCICFG_SUBSYSTEM_VENDOR_ID_OFFSET		0x2c
#define PCICFG_SUBSYSTEM_ID_OFFSET			0x2e
#define PCICFG_INT_LINE 				0x3c
#define PCICFG_INT_PIN					0x3d
#define PCICFG_PM_CAPABILITY				0x48
#define PCICFG_PM_CAPABILITY_VERSION		(0x3<<16)
#define PCICFG_PM_CAPABILITY_CLOCK		(1<<19)
#define PCICFG_PM_CAPABILITY_RESERVED		(1<<20)
#define PCICFG_PM_CAPABILITY_DSI		(1<<21)
#define PCICFG_PM_CAPABILITY_AUX_CURRENT	(0x7<<22)
#define PCICFG_PM_CAPABILITY_D1_SUPPORT 	(1<<25)
#define PCICFG_PM_CAPABILITY_D2_SUPPORT 	(1<<26)
#define PCICFG_PM_CAPABILITY_PME_IN_D0		(1<<27)
#define PCICFG_PM_CAPABILITY_PME_IN_D1		(1<<28)
#define PCICFG_PM_CAPABILITY_PME_IN_D2		(1<<29)
#define PCICFG_PM_CAPABILITY_PME_IN_D3_HOT	(1<<30)
#define PCICFG_PM_CAPABILITY_PME_IN_D3_COLD	(1<<31)
#define PCICFG_PM_CSR_OFFSET				0x4c
#define PCICFG_PM_CSR_STATE			(0x3<<0)
#define PCICFG_PM_CSR_PME_ENABLE		(1<<8)
#define PCICFG_PM_CSR_PME_STATUS		(1<<15)
#define PCICFG_MSI_CAP_ID_OFFSET			0x58
#define PCICFG_MSI_CONTROL_ENABLE		(0x1<<16)
#define PCICFG_MSI_CONTROL_MCAP 		(0x7<<17)
#define PCICFG_MSI_CONTROL_MENA 		(0x7<<20)
#define PCICFG_MSI_CONTROL_64_BIT_ADDR_CAP	(0x1<<23)
#define PCICFG_MSI_CONTROL_MSI_PVMASK_CAPABLE	(0x1<<24)
#define PCICFG_GRC_ADDRESS				0x78
#define PCICFG_GRC_DATA				0x80
#define PCICFG_ME_REGISTER				0x98
#define PCICFG_MSIX_CAP_ID_OFFSET			0xa0
#define PCICFG_MSIX_CONTROL_TABLE_SIZE		(0x7ff<<16)
#define PCICFG_MSIX_CONTROL_RESERVED		(0x7<<27)
#define PCICFG_MSIX_CONTROL_FUNC_MASK		(0x1<<30)
#define PCICFG_MSIX_CONTROL_MSIX_ENABLE 	(0x1<<31)

#define PCICFG_DEVICE_CONTROL				0xb4
#define PCICFG_DEVICE_STATUS				0xb6
#define PCICFG_DEVICE_STATUS_CORR_ERR_DET	(1<<0)
#define PCICFG_DEVICE_STATUS_NON_FATAL_ERR_DET	(1<<1)
#define PCICFG_DEVICE_STATUS_FATAL_ERR_DET	(1<<2)
#define PCICFG_DEVICE_STATUS_UNSUP_REQ_DET	(1<<3)
#define PCICFG_DEVICE_STATUS_AUX_PWR_DET	(1<<4)
#define PCICFG_DEVICE_STATUS_NO_PEND		(1<<5)
#define PCICFG_LINK_CONTROL				0xbc


#define BAR_USTRORM_INTMEM				0x400000
#define BAR_CSTRORM_INTMEM				0x410000
#define BAR_XSTRORM_INTMEM				0x420000
#define BAR_TSTRORM_INTMEM				0x430000

#define BAR_IGU_INTMEM					0x440000

#define BAR_DOORBELL_OFFSET				0x800000

#define BAR_ME_REGISTER 				0x450000

#define GRC_CONFIG_2_SIZE_REG				0x408
#define PCI_CONFIG_2_BAR1_SIZE			(0xfL<<0)
#define PCI_CONFIG_2_BAR1_SIZE_DISABLED 	(0L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_64K		(1L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_128K		(2L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_256K		(3L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_512K		(4L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_1M		(5L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_2M		(6L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_4M		(7L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_8M		(8L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_16M		(9L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_32M		(10L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_64M		(11L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_128M		(12L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_256M		(13L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_512M		(14L<<0)
#define PCI_CONFIG_2_BAR1_SIZE_1G		(15L<<0)
#define PCI_CONFIG_2_BAR1_64ENA 		(1L<<4)
#define PCI_CONFIG_2_EXP_ROM_RETRY		(1L<<5)
#define PCI_CONFIG_2_CFG_CYCLE_RETRY		(1L<<6)
#define PCI_CONFIG_2_FIRST_CFG_DONE		(1L<<7)
#define PCI_CONFIG_2_EXP_ROM_SIZE		(0xffL<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_DISABLED	(0L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_2K		(1L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_4K		(2L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_8K		(3L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_16K		(4L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_32K		(5L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_64K		(6L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_128K		(7L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_256K		(8L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_512K		(9L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_1M		(10L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_2M		(11L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_4M		(12L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_8M		(13L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_16M		(14L<<8)
#define PCI_CONFIG_2_EXP_ROM_SIZE_32M		(15L<<8)
#define PCI_CONFIG_2_BAR_PREFETCH		(1L<<16)
#define PCI_CONFIG_2_RESERVED0			(0x7fffL<<17)

#define GRC_CONFIG_3_SIZE_REG				0x40c
#define PCI_CONFIG_3_STICKY_BYTE		(0xffL<<0)
#define PCI_CONFIG_3_FORCE_PME			(1L<<24)
#define PCI_CONFIG_3_PME_STATUS 		(1L<<25)
#define PCI_CONFIG_3_PME_ENABLE 		(1L<<26)
#define PCI_CONFIG_3_PM_STATE			(0x3L<<27)
#define PCI_CONFIG_3_VAUX_PRESET		(1L<<30)
#define PCI_CONFIG_3_PCI_POWER			(1L<<31)

#define GRC_BAR2_CONFIG 				0x4e0
#define PCI_CONFIG_2_BAR2_SIZE			(0xfL<<0)
#define PCI_CONFIG_2_BAR2_SIZE_DISABLED 	(0L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_64K		(1L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_128K		(2L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_256K		(3L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_512K		(4L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_1M		(5L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_2M		(6L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_4M		(7L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_8M		(8L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_16M		(9L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_32M		(10L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_64M		(11L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_128M		(12L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_256M		(13L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_512M		(14L<<0)
#define PCI_CONFIG_2_BAR2_SIZE_1G		(15L<<0)
#define PCI_CONFIG_2_BAR2_64ENA 		(1L<<4)

#define PCI_PM_DATA_A					0x410
#define PCI_PM_DATA_B					0x414
#define PCI_ID_VAL1					0x434
#define PCI_ID_VAL2					0x438

#define PXPCS_TL_CONTROL_5		    0x814
#define PXPCS_TL_CONTROL_5_UNKNOWNTYPE_ERR_ATTN    (1 << 29) 
#define PXPCS_TL_CONTROL_5_BOUNDARY4K_ERR_ATTN	   (1 << 28)   
#define PXPCS_TL_CONTROL_5_MRRS_ERR_ATTN   (1 << 27)   
#define PXPCS_TL_CONTROL_5_MPS_ERR_ATTN    (1 << 26)   
#define PXPCS_TL_CONTROL_5_TTX_BRIDGE_FORWARD_ERR  (1 << 25)   
#define PXPCS_TL_CONTROL_5_TTX_TXINTF_OVERFLOW	   (1 << 24)   
#define PXPCS_TL_CONTROL_5_PHY_ERR_ATTN    (1 << 23)   
#define PXPCS_TL_CONTROL_5_DL_ERR_ATTN	   (1 << 22)   
#define PXPCS_TL_CONTROL_5_TTX_ERR_NP_TAG_IN_USE   (1 << 21)   
#define PXPCS_TL_CONTROL_5_TRX_ERR_UNEXP_RTAG  (1 << 20)   
#define PXPCS_TL_CONTROL_5_PRI_SIG_TARGET_ABORT1   (1 << 19)   
#define PXPCS_TL_CONTROL_5_ERR_UNSPPORT1   (1 << 18)   
#define PXPCS_TL_CONTROL_5_ERR_ECRC1   (1 << 17)   
#define PXPCS_TL_CONTROL_5_ERR_MALF_TLP1   (1 << 16)   
#define PXPCS_TL_CONTROL_5_ERR_RX_OFLOW1   (1 << 15)   
#define PXPCS_TL_CONTROL_5_ERR_UNEXP_CPL1  (1 << 14)   
#define PXPCS_TL_CONTROL_5_ERR_MASTER_ABRT1    (1 << 13)   
#define PXPCS_TL_CONTROL_5_ERR_CPL_TIMEOUT1    (1 << 12)   
#define PXPCS_TL_CONTROL_5_ERR_FC_PRTL1    (1 << 11)   
#define PXPCS_TL_CONTROL_5_ERR_PSND_TLP1   (1 << 10)   
#define PXPCS_TL_CONTROL_5_PRI_SIG_TARGET_ABORT    (1 << 9)    
#define PXPCS_TL_CONTROL_5_ERR_UNSPPORT    (1 << 8)    
#define PXPCS_TL_CONTROL_5_ERR_ECRC    (1 << 7)    
#define PXPCS_TL_CONTROL_5_ERR_MALF_TLP    (1 << 6)    
#define PXPCS_TL_CONTROL_5_ERR_RX_OFLOW    (1 << 5)    
#define PXPCS_TL_CONTROL_5_ERR_UNEXP_CPL   (1 << 4)    
#define PXPCS_TL_CONTROL_5_ERR_MASTER_ABRT     (1 << 3)    
#define PXPCS_TL_CONTROL_5_ERR_CPL_TIMEOUT     (1 << 2)    
#define PXPCS_TL_CONTROL_5_ERR_FC_PRTL	   (1 << 1)    
#define PXPCS_TL_CONTROL_5_ERR_PSND_TLP    (1 << 0)    


#define PXPCS_TL_FUNC345_STAT	   0x854
#define PXPCS_TL_FUNC345_STAT_PRI_SIG_TARGET_ABORT4    (1 << 29)   
#define PXPCS_TL_FUNC345_STAT_ERR_UNSPPORT4\
	(1 << 28) 
#define PXPCS_TL_FUNC345_STAT_ERR_ECRC4\
	(1 << 27) 
#define PXPCS_TL_FUNC345_STAT_ERR_MALF_TLP4\
	(1 << 26) 
#define PXPCS_TL_FUNC345_STAT_ERR_RX_OFLOW4\
	(1 << 25) 
#define PXPCS_TL_FUNC345_STAT_ERR_UNEXP_CPL4\
	(1 << 24) 
#define PXPCS_TL_FUNC345_STAT_ERR_MASTER_ABRT4\
	(1 << 23) 
#define PXPCS_TL_FUNC345_STAT_ERR_CPL_TIMEOUT4\
	(1 << 22) 
#define PXPCS_TL_FUNC345_STAT_ERR_FC_PRTL4\
	(1 << 21) 
#define PXPCS_TL_FUNC345_STAT_ERR_PSND_TLP4\
	(1 << 20) 
#define PXPCS_TL_FUNC345_STAT_PRI_SIG_TARGET_ABORT3    (1 << 19)   
#define PXPCS_TL_FUNC345_STAT_ERR_UNSPPORT3\
	(1 << 18) 
#define PXPCS_TL_FUNC345_STAT_ERR_ECRC3\
	(1 << 17) 
#define PXPCS_TL_FUNC345_STAT_ERR_MALF_TLP3\
	(1 << 16) 
#define PXPCS_TL_FUNC345_STAT_ERR_RX_OFLOW3\
	(1 << 15) 
#define PXPCS_TL_FUNC345_STAT_ERR_UNEXP_CPL3\
	(1 << 14) 
#define PXPCS_TL_FUNC345_STAT_ERR_MASTER_ABRT3\
	(1 << 13) 
#define PXPCS_TL_FUNC345_STAT_ERR_CPL_TIMEOUT3\
	(1 << 12) 
#define PXPCS_TL_FUNC345_STAT_ERR_FC_PRTL3\
	(1 << 11) 
#define PXPCS_TL_FUNC345_STAT_ERR_PSND_TLP3\
	(1 << 10) 
#define PXPCS_TL_FUNC345_STAT_PRI_SIG_TARGET_ABORT2    (1 << 9)    
#define PXPCS_TL_FUNC345_STAT_ERR_UNSPPORT2\
	(1 << 8) 
#define PXPCS_TL_FUNC345_STAT_ERR_ECRC2\
	(1 << 7) 
#define PXPCS_TL_FUNC345_STAT_ERR_MALF_TLP2\
	(1 << 6) 
#define PXPCS_TL_FUNC345_STAT_ERR_RX_OFLOW2\
	(1 << 5) 
#define PXPCS_TL_FUNC345_STAT_ERR_UNEXP_CPL2\
	(1 << 4) 
#define PXPCS_TL_FUNC345_STAT_ERR_MASTER_ABRT2\
	(1 << 3) 
#define PXPCS_TL_FUNC345_STAT_ERR_CPL_TIMEOUT2\
	(1 << 2) 
#define PXPCS_TL_FUNC345_STAT_ERR_FC_PRTL2\
	(1 << 1) 
#define PXPCS_TL_FUNC345_STAT_ERR_PSND_TLP2\
	(1 << 0) 


#define PXPCS_TL_FUNC678_STAT  0x85C
#define PXPCS_TL_FUNC678_STAT_PRI_SIG_TARGET_ABORT7    (1 << 29)   
#define PXPCS_TL_FUNC678_STAT_ERR_UNSPPORT7\
	(1 << 28) 
#define PXPCS_TL_FUNC678_STAT_ERR_ECRC7\
	(1 << 27) 
#define PXPCS_TL_FUNC678_STAT_ERR_MALF_TLP7\
	(1 << 26) 
#define PXPCS_TL_FUNC678_STAT_ERR_RX_OFLOW7\
	(1 << 25) 
#define PXPCS_TL_FUNC678_STAT_ERR_UNEXP_CPL7\
	(1 << 24) 
#define PXPCS_TL_FUNC678_STAT_ERR_MASTER_ABRT7\
	(1 << 23) 
#define PXPCS_TL_FUNC678_STAT_ERR_CPL_TIMEOUT7\
	(1 << 22) 
#define PXPCS_TL_FUNC678_STAT_ERR_FC_PRTL7\
	(1 << 21) 
#define PXPCS_TL_FUNC678_STAT_ERR_PSND_TLP7\
	(1 << 20) 
#define PXPCS_TL_FUNC678_STAT_PRI_SIG_TARGET_ABORT6    (1 << 19)    
#define PXPCS_TL_FUNC678_STAT_ERR_UNSPPORT6\
	(1 << 18) 
#define PXPCS_TL_FUNC678_STAT_ERR_ECRC6\
	(1 << 17) 
#define PXPCS_TL_FUNC678_STAT_ERR_MALF_TLP6\
	(1 << 16) 
#define PXPCS_TL_FUNC678_STAT_ERR_RX_OFLOW6\
	(1 << 15) 
#define PXPCS_TL_FUNC678_STAT_ERR_UNEXP_CPL6\
	(1 << 14) 
#define PXPCS_TL_FUNC678_STAT_ERR_MASTER_ABRT6\
	(1 << 13) 
#define PXPCS_TL_FUNC678_STAT_ERR_CPL_TIMEOUT6\
	(1 << 12) 
#define PXPCS_TL_FUNC678_STAT_ERR_FC_PRTL6\
	(1 << 11) 
#define PXPCS_TL_FUNC678_STAT_ERR_PSND_TLP6\
	(1 << 10) 
#define PXPCS_TL_FUNC678_STAT_PRI_SIG_TARGET_ABORT5    (1 << 9) 
#define PXPCS_TL_FUNC678_STAT_ERR_UNSPPORT5\
	(1 << 8) 
#define PXPCS_TL_FUNC678_STAT_ERR_ECRC5\
	(1 << 7) 
#define PXPCS_TL_FUNC678_STAT_ERR_MALF_TLP5\
	(1 << 6) 
#define PXPCS_TL_FUNC678_STAT_ERR_RX_OFLOW5\
	(1 << 5) 
#define PXPCS_TL_FUNC678_STAT_ERR_UNEXP_CPL5\
	(1 << 4) 
#define PXPCS_TL_FUNC678_STAT_ERR_MASTER_ABRT5\
	(1 << 3) 
#define PXPCS_TL_FUNC678_STAT_ERR_CPL_TIMEOUT5\
	(1 << 2) 
#define PXPCS_TL_FUNC678_STAT_ERR_FC_PRTL5\
	(1 << 1) 
#define PXPCS_TL_FUNC678_STAT_ERR_PSND_TLP5\
	(1 << 0) 


#define BAR_USTRORM_INTMEM				0x400000
#define BAR_CSTRORM_INTMEM				0x410000
#define BAR_XSTRORM_INTMEM				0x420000
#define BAR_TSTRORM_INTMEM				0x430000

#define BAR_IGU_INTMEM					0x440000

#define BAR_DOORBELL_OFFSET				0x800000

#define BAR_ME_REGISTER				0x450000
#define ME_REG_PF_NUM_SHIFT		0
#define ME_REG_PF_NUM\
	(7L<<ME_REG_PF_NUM_SHIFT) 
#define ME_REG_VF_VALID		(1<<8)
#define ME_REG_VF_NUM_SHIFT		9
#define ME_REG_VF_NUM_MASK		(0x3f<<ME_REG_VF_NUM_SHIFT)
#define ME_REG_VF_ERR			(0x1<<3)
#define ME_REG_ABS_PF_NUM_SHIFT	16
#define ME_REG_ABS_PF_NUM\
	(7L<<ME_REG_ABS_PF_NUM_SHIFT) 


#define MDIO_REG_BANK_CL73_IEEEB0	0x0
#define MDIO_CL73_IEEEB0_CL73_AN_CONTROL	0x0
#define MDIO_CL73_IEEEB0_CL73_AN_CONTROL_RESTART_AN	0x0200
#define MDIO_CL73_IEEEB0_CL73_AN_CONTROL_AN_EN		0x1000
#define MDIO_CL73_IEEEB0_CL73_AN_CONTROL_MAIN_RST	0x8000

#define MDIO_REG_BANK_CL73_IEEEB1	0x10
#define MDIO_CL73_IEEEB1_AN_ADV1		0x00
#define MDIO_CL73_IEEEB1_AN_ADV1_PAUSE			0x0400
#define MDIO_CL73_IEEEB1_AN_ADV1_ASYMMETRIC		0x0800
#define MDIO_CL73_IEEEB1_AN_ADV1_PAUSE_BOTH		0x0C00
#define MDIO_CL73_IEEEB1_AN_ADV1_PAUSE_MASK		0x0C00
#define MDIO_CL73_IEEEB1_AN_ADV2		0x01
#define MDIO_CL73_IEEEB1_AN_ADV2_ADVR_1000M		0x0000
#define MDIO_CL73_IEEEB1_AN_ADV2_ADVR_1000M_KX		0x0020
#define MDIO_CL73_IEEEB1_AN_ADV2_ADVR_10G_KX4		0x0040
#define MDIO_CL73_IEEEB1_AN_ADV2_ADVR_10G_KR		0x0080
#define MDIO_CL73_IEEEB1_AN_LP_ADV1		0x03
#define MDIO_CL73_IEEEB1_AN_LP_ADV1_PAUSE		0x0400
#define MDIO_CL73_IEEEB1_AN_LP_ADV1_ASYMMETRIC		0x0800
#define MDIO_CL73_IEEEB1_AN_LP_ADV1_PAUSE_BOTH		0x0C00
#define MDIO_CL73_IEEEB1_AN_LP_ADV1_PAUSE_MASK		0x0C00
#define MDIO_CL73_IEEEB1_AN_LP_ADV2			0x04

#define MDIO_REG_BANK_RX0				0x80b0
#define MDIO_RX0_RX_STATUS				0x10
#define MDIO_RX0_RX_STATUS_SIGDET			0x8000
#define MDIO_RX0_RX_STATUS_RX_SEQ_DONE			0x1000
#define MDIO_RX0_RX_EQ_BOOST				0x1c
#define MDIO_RX0_RX_EQ_BOOST_EQUALIZER_CTRL_MASK	0x7
#define MDIO_RX0_RX_EQ_BOOST_OFFSET_CTRL		0x10

#define MDIO_REG_BANK_RX1				0x80c0
#define MDIO_RX1_RX_EQ_BOOST				0x1c
#define MDIO_RX1_RX_EQ_BOOST_EQUALIZER_CTRL_MASK	0x7
#define MDIO_RX1_RX_EQ_BOOST_OFFSET_CTRL		0x10

#define MDIO_REG_BANK_RX2				0x80d0
#define MDIO_RX2_RX_EQ_BOOST				0x1c
#define MDIO_RX2_RX_EQ_BOOST_EQUALIZER_CTRL_MASK	0x7
#define MDIO_RX2_RX_EQ_BOOST_OFFSET_CTRL		0x10

#define MDIO_REG_BANK_RX3				0x80e0
#define MDIO_RX3_RX_EQ_BOOST				0x1c
#define MDIO_RX3_RX_EQ_BOOST_EQUALIZER_CTRL_MASK	0x7
#define MDIO_RX3_RX_EQ_BOOST_OFFSET_CTRL		0x10

#define MDIO_REG_BANK_RX_ALL				0x80f0
#define MDIO_RX_ALL_RX_EQ_BOOST 			0x1c
#define MDIO_RX_ALL_RX_EQ_BOOST_EQUALIZER_CTRL_MASK	0x7
#define MDIO_RX_ALL_RX_EQ_BOOST_OFFSET_CTRL	0x10

#define MDIO_REG_BANK_TX0				0x8060
#define MDIO_TX0_TX_DRIVER				0x17
#define MDIO_TX0_TX_DRIVER_PREEMPHASIS_MASK		0xf000
#define MDIO_TX0_TX_DRIVER_PREEMPHASIS_SHIFT		12
#define MDIO_TX0_TX_DRIVER_IDRIVER_MASK 		0x0f00
#define MDIO_TX0_TX_DRIVER_IDRIVER_SHIFT		8
#define MDIO_TX0_TX_DRIVER_IPREDRIVER_MASK		0x00f0
#define MDIO_TX0_TX_DRIVER_IPREDRIVER_SHIFT		4
#define MDIO_TX0_TX_DRIVER_IFULLSPD_MASK		0x000e
#define MDIO_TX0_TX_DRIVER_IFULLSPD_SHIFT		1
#define MDIO_TX0_TX_DRIVER_ICBUF1T			1

#define MDIO_REG_BANK_TX1				0x8070
#define MDIO_TX1_TX_DRIVER				0x17
#define MDIO_TX0_TX_DRIVER_PREEMPHASIS_MASK		0xf000
#define MDIO_TX0_TX_DRIVER_PREEMPHASIS_SHIFT		12
#define MDIO_TX0_TX_DRIVER_IDRIVER_MASK 		0x0f00
#define MDIO_TX0_TX_DRIVER_IDRIVER_SHIFT		8
#define MDIO_TX0_TX_DRIVER_IPREDRIVER_MASK		0x00f0
#define MDIO_TX0_TX_DRIVER_IPREDRIVER_SHIFT		4
#define MDIO_TX0_TX_DRIVER_IFULLSPD_MASK		0x000e
#define MDIO_TX0_TX_DRIVER_IFULLSPD_SHIFT		1
#define MDIO_TX0_TX_DRIVER_ICBUF1T			1

#define MDIO_REG_BANK_TX2				0x8080
#define MDIO_TX2_TX_DRIVER				0x17
#define MDIO_TX0_TX_DRIVER_PREEMPHASIS_MASK		0xf000
#define MDIO_TX0_TX_DRIVER_PREEMPHASIS_SHIFT		12
#define MDIO_TX0_TX_DRIVER_IDRIVER_MASK 		0x0f00
#define MDIO_TX0_TX_DRIVER_IDRIVER_SHIFT		8
#define MDIO_TX0_TX_DRIVER_IPREDRIVER_MASK		0x00f0
#define MDIO_TX0_TX_DRIVER_IPREDRIVER_SHIFT		4
#define MDIO_TX0_TX_DRIVER_IFULLSPD_MASK		0x000e
#define MDIO_TX0_TX_DRIVER_IFULLSPD_SHIFT		1
#define MDIO_TX0_TX_DRIVER_ICBUF1T			1

#define MDIO_REG_BANK_TX3				0x8090
#define MDIO_TX3_TX_DRIVER				0x17
#define MDIO_TX0_TX_DRIVER_PREEMPHASIS_MASK		0xf000
#define MDIO_TX0_TX_DRIVER_PREEMPHASIS_SHIFT		12
#define MDIO_TX0_TX_DRIVER_IDRIVER_MASK 		0x0f00
#define MDIO_TX0_TX_DRIVER_IDRIVER_SHIFT		8
#define MDIO_TX0_TX_DRIVER_IPREDRIVER_MASK		0x00f0
#define MDIO_TX0_TX_DRIVER_IPREDRIVER_SHIFT		4
#define MDIO_TX0_TX_DRIVER_IFULLSPD_MASK		0x000e
#define MDIO_TX0_TX_DRIVER_IFULLSPD_SHIFT		1
#define MDIO_TX0_TX_DRIVER_ICBUF1T			1

#define MDIO_REG_BANK_XGXS_BLOCK0			0x8000
#define MDIO_BLOCK0_XGXS_CONTROL			0x10

#define MDIO_REG_BANK_XGXS_BLOCK1			0x8010
#define MDIO_BLOCK1_LANE_CTRL0				0x15
#define MDIO_BLOCK1_LANE_CTRL1				0x16
#define MDIO_BLOCK1_LANE_CTRL2				0x17
#define MDIO_BLOCK1_LANE_PRBS				0x19

#define MDIO_REG_BANK_XGXS_BLOCK2			0x8100
#define MDIO_XGXS_BLOCK2_RX_LN_SWAP			0x10
#define MDIO_XGXS_BLOCK2_RX_LN_SWAP_ENABLE		0x8000
#define MDIO_XGXS_BLOCK2_RX_LN_SWAP_FORCE_ENABLE	0x4000
#define MDIO_XGXS_BLOCK2_TX_LN_SWAP		0x11
#define MDIO_XGXS_BLOCK2_TX_LN_SWAP_ENABLE		0x8000
#define MDIO_XGXS_BLOCK2_UNICORE_MODE_10G	0x14
#define MDIO_XGXS_BLOCK2_UNICORE_MODE_10G_CX4_XGXS	0x0001
#define MDIO_XGXS_BLOCK2_UNICORE_MODE_10G_HIGIG_XGXS	0x0010
#define MDIO_XGXS_BLOCK2_TEST_MODE_LANE 	0x15

#define MDIO_REG_BANK_GP_STATUS 			0x8120
#define MDIO_GP_STATUS_TOP_AN_STATUS1				0x1B
#define MDIO_GP_STATUS_TOP_AN_STATUS1_CL73_AUTONEG_COMPLETE	0x0001
#define MDIO_GP_STATUS_TOP_AN_STATUS1_CL37_AUTONEG_COMPLETE	0x0002
#define MDIO_GP_STATUS_TOP_AN_STATUS1_LINK_STATUS		0x0004
#define MDIO_GP_STATUS_TOP_AN_STATUS1_DUPLEX_STATUS		0x0008
#define MDIO_GP_STATUS_TOP_AN_STATUS1_CL73_MR_LP_NP_AN_ABLE	0x0010
#define MDIO_GP_STATUS_TOP_AN_STATUS1_CL73_LP_NP_BAM_ABLE	0x0020
#define MDIO_GP_STATUS_TOP_AN_STATUS1_PAUSE_RSOLUTION_TXSIDE	0x0040
#define MDIO_GP_STATUS_TOP_AN_STATUS1_PAUSE_RSOLUTION_RXSIDE	0x0080
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_MASK 	0x3f00
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10M		0x0000
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_100M 	0x0100
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G		0x0200
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_2_5G 	0x0300
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_5G		0x0400
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_6G		0x0500
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_HIG	0x0600
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_CX4	0x0700
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12G_HIG	0x0800
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_12_5G	0x0900
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_13G		0x0A00
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_15G		0x0B00
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_16G		0x0C00
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_1G_KX	0x0D00
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_KX4	0x0E00
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_KR	0x0F00
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_XFI	0x1B00
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_20G_DXGXS	0x1E00
#define MDIO_GP_STATUS_TOP_AN_STATUS1_ACTUAL_SPEED_10G_SFI	0x1F00


#define MDIO_REG_BANK_10G_PARALLEL_DETECT		0x8130
#define MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_STATUS		0x10
#define MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_STATUS_PD_LINK		0x8000
#define MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_CONTROL		0x11
#define MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_CONTROL_PARDET10G_EN	0x1
#define MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_LINK		0x13
#define MDIO_10G_PARALLEL_DETECT_PAR_DET_10G_LINK_CNT		(0xb71<<1)

#define MDIO_REG_BANK_SERDES_DIGITAL			0x8300
#define MDIO_SERDES_DIGITAL_A_1000X_CONTROL1			0x10
#define MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_FIBER_MODE 		0x0001
#define MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_TBI_IF			0x0002
#define MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_SIGNAL_DETECT_EN		0x0004
#define MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_INVERT_SIGNAL_DETECT	0x0008
#define MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_AUTODET			0x0010
#define MDIO_SERDES_DIGITAL_A_1000X_CONTROL1_MSTR_MODE			0x0020
#define MDIO_SERDES_DIGITAL_A_1000X_CONTROL2			0x11
#define MDIO_SERDES_DIGITAL_A_1000X_CONTROL2_PRL_DT_EN			0x0001
#define MDIO_SERDES_DIGITAL_A_1000X_CONTROL2_AN_FST_TMR 		0x0040
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS1			0x14
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS1_SGMII			0x0001
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS1_LINK			0x0002
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS1_DUPLEX			0x0004
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS1_SPEED_MASK			0x0018
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS1_SPEED_SHIFT 		3
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS1_SPEED_2_5G			0x0018
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS1_SPEED_1G			0x0010
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS1_SPEED_100M			0x0008
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS1_SPEED_10M			0x0000
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS2			0x15
#define MDIO_SERDES_DIGITAL_A_1000X_STATUS2_AN_DISABLED 		0x0002
#define MDIO_SERDES_DIGITAL_MISC1				0x18
#define MDIO_SERDES_DIGITAL_MISC1_REFCLK_SEL_MASK			0xE000
#define MDIO_SERDES_DIGITAL_MISC1_REFCLK_SEL_25M			0x0000
#define MDIO_SERDES_DIGITAL_MISC1_REFCLK_SEL_100M			0x2000
#define MDIO_SERDES_DIGITAL_MISC1_REFCLK_SEL_125M			0x4000
#define MDIO_SERDES_DIGITAL_MISC1_REFCLK_SEL_156_25M			0x6000
#define MDIO_SERDES_DIGITAL_MISC1_REFCLK_SEL_187_5M			0x8000
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_SEL			0x0010
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_MASK			0x000f
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_2_5G			0x0000
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_5G			0x0001
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_6G			0x0002
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_10G_HIG			0x0003
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_10G_CX4			0x0004
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_12G			0x0005
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_12_5G			0x0006
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_13G			0x0007
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_15G			0x0008
#define MDIO_SERDES_DIGITAL_MISC1_FORCE_SPEED_16G			0x0009

#define MDIO_REG_BANK_OVER_1G				0x8320
#define MDIO_OVER_1G_DIGCTL_3_4 				0x14
#define MDIO_OVER_1G_DIGCTL_3_4_MP_ID_MASK				0xffe0
#define MDIO_OVER_1G_DIGCTL_3_4_MP_ID_SHIFT				5
#define MDIO_OVER_1G_UP1					0x19
#define MDIO_OVER_1G_UP1_2_5G						0x0001
#define MDIO_OVER_1G_UP1_5G						0x0002
#define MDIO_OVER_1G_UP1_6G						0x0004
#define MDIO_OVER_1G_UP1_10G						0x0010
#define MDIO_OVER_1G_UP1_10GH						0x0008
#define MDIO_OVER_1G_UP1_12G						0x0020
#define MDIO_OVER_1G_UP1_12_5G						0x0040
#define MDIO_OVER_1G_UP1_13G						0x0080
#define MDIO_OVER_1G_UP1_15G						0x0100
#define MDIO_OVER_1G_UP1_16G						0x0200
#define MDIO_OVER_1G_UP2					0x1A
#define MDIO_OVER_1G_UP2_IPREDRIVER_MASK				0x0007
#define MDIO_OVER_1G_UP2_IDRIVER_MASK					0x0038
#define MDIO_OVER_1G_UP2_PREEMPHASIS_MASK				0x03C0
#define MDIO_OVER_1G_UP3					0x1B
#define MDIO_OVER_1G_UP3_HIGIG2 					0x0001
#define MDIO_OVER_1G_LP_UP1					0x1C
#define MDIO_OVER_1G_LP_UP2					0x1D
#define MDIO_OVER_1G_LP_UP2_MR_ADV_OVER_1G_MASK 			0x03ff
#define MDIO_OVER_1G_LP_UP2_PREEMPHASIS_MASK				0x0780
#define MDIO_OVER_1G_LP_UP2_PREEMPHASIS_SHIFT				7
#define MDIO_OVER_1G_LP_UP3						0x1E

#define MDIO_REG_BANK_REMOTE_PHY			0x8330
#define MDIO_REMOTE_PHY_MISC_RX_STATUS				0x10
#define MDIO_REMOTE_PHY_MISC_RX_STATUS_CL37_FSM_RECEIVED_OVER1G_MSG	0x0010
#define MDIO_REMOTE_PHY_MISC_RX_STATUS_CL37_FSM_RECEIVED_BRCM_OUI_MSG	0x0600

#define MDIO_REG_BANK_BAM_NEXT_PAGE			0x8350
#define MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL			0x10
#define MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL_BAM_MODE			0x0001
#define MDIO_BAM_NEXT_PAGE_MP5_NEXT_PAGE_CTRL_TETON_AN			0x0002

#define MDIO_REG_BANK_CL73_USERB0		0x8370
#define MDIO_CL73_USERB0_CL73_UCTRL				0x10
#define MDIO_CL73_USERB0_CL73_UCTRL_USTAT1_MUXSEL			0x0002
#define MDIO_CL73_USERB0_CL73_USTAT1				0x11
#define MDIO_CL73_USERB0_CL73_USTAT1_LINK_STATUS_CHECK			0x0100
#define MDIO_CL73_USERB0_CL73_USTAT1_AN_GOOD_CHECK_BAM37		0x0400
#define MDIO_CL73_USERB0_CL73_BAM_CTRL1 			0x12
#define MDIO_CL73_USERB0_CL73_BAM_CTRL1_BAM_EN				0x8000
#define MDIO_CL73_USERB0_CL73_BAM_CTRL1_BAM_STATION_MNGR_EN		0x4000
#define MDIO_CL73_USERB0_CL73_BAM_CTRL1_BAM_NP_AFTER_BP_EN		0x2000
#define MDIO_CL73_USERB0_CL73_BAM_CTRL3 			0x14
#define MDIO_CL73_USERB0_CL73_BAM_CTRL3_USE_CL73_HCD_MR 		0x0001

#define MDIO_REG_BANK_AER_BLOCK 		0xFFD0
#define MDIO_AER_BLOCK_AER_REG					0x1E

#define MDIO_REG_BANK_COMBO_IEEE0		0xFFE0
#define MDIO_COMBO_IEEE0_MII_CONTROL				0x10
#define MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_MASK			0x2040
#define MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_10			0x0000
#define MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_100			0x2000
#define MDIO_COMBO_IEEO_MII_CONTROL_MAN_SGMII_SP_1000			0x0040
#define MDIO_COMBO_IEEO_MII_CONTROL_FULL_DUPLEX 			0x0100
#define MDIO_COMBO_IEEO_MII_CONTROL_RESTART_AN				0x0200
#define MDIO_COMBO_IEEO_MII_CONTROL_AN_EN				0x1000
#define MDIO_COMBO_IEEO_MII_CONTROL_LOOPBACK				0x4000
#define MDIO_COMBO_IEEO_MII_CONTROL_RESET				0x8000
#define MDIO_COMBO_IEEE0_MII_STATUS				0x11
#define MDIO_COMBO_IEEE0_MII_STATUS_LINK_PASS				0x0004
#define MDIO_COMBO_IEEE0_MII_STATUS_AUTONEG_COMPLETE			0x0020
#define MDIO_COMBO_IEEE0_AUTO_NEG_ADV				0x14
#define MDIO_COMBO_IEEE0_AUTO_NEG_ADV_FULL_DUPLEX			0x0020
#define MDIO_COMBO_IEEE0_AUTO_NEG_ADV_HALF_DUPLEX			0x0040
#define MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_MASK			0x0180
#define MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_NONE			0x0000
#define MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_SYMMETRIC			0x0080
#define MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_ASYMMETRIC			0x0100
#define MDIO_COMBO_IEEE0_AUTO_NEG_ADV_PAUSE_BOTH			0x0180
#define MDIO_COMBO_IEEE0_AUTO_NEG_ADV_NEXT_PAGE 			0x8000
#define MDIO_COMBO_IEEE0_AUTO_NEG_LINK_PARTNER_ABILITY1 	0x15
#define MDIO_COMBO_IEEE0_AUTO_NEG_LINK_PARTNER_ABILITY1_NEXT_PAGE	0x8000
#define MDIO_COMBO_IEEE0_AUTO_NEG_LINK_PARTNER_ABILITY1_ACK		0x4000
#define MDIO_COMBO_IEEE0_AUTO_NEG_LINK_PARTNER_ABILITY1_PAUSE_MASK	0x0180
#define MDIO_COMBO_IEEE0_AUTO_NEG_LINK_PARTNER_ABILITY1_PAUSE_NONE	0x0000
#define MDIO_COMBO_IEEE0_AUTO_NEG_LINK_PARTNER_ABILITY1_PAUSE_BOTH	0x0180
#define MDIO_COMBO_IEEE0_AUTO_NEG_LINK_PARTNER_ABILITY1_HALF_DUP_CAP	0x0040
#define MDIO_COMBO_IEEE0_AUTO_NEG_LINK_PARTNER_ABILITY1_FULL_DUP_CAP	0x0020
#define MDIO_COMBO_IEEE0_AUTO_NEG_LINK_PARTNER_ABILITY1_SGMII_MODE	0x0001


#define MDIO_PMA_DEVAD			0x1
#define MDIO_PMA_REG_CTRL		0x0
#define MDIO_PMA_REG_STATUS		0x1
#define MDIO_PMA_REG_10G_CTRL2		0x7
#define MDIO_PMA_REG_TX_DISABLE		0x0009
#define MDIO_PMA_REG_RX_SD		0xa
#define MDIO_PMA_REG_BCM_CTRL		0x0096
#define MDIO_PMA_REG_FEC_CTRL		0x00ab
#define MDIO_PMA_REG_PHY_IDENTIFIER	0xc800
#define MDIO_PMA_REG_DIGITAL_CTRL	0xc808
#define MDIO_PMA_REG_DIGITAL_STATUS	0xc809
#define MDIO_PMA_REG_TX_POWER_DOWN	0xca02
#define MDIO_PMA_REG_CMU_PLL_BYPASS	0xca09
#define MDIO_PMA_REG_MISC_CTRL		0xca0a
#define MDIO_PMA_REG_GEN_CTRL		0xca10
#define MDIO_PMA_REG_GEN_CTRL_ROM_RESET_INTERNAL_MP	0x0188
#define MDIO_PMA_REG_GEN_CTRL_ROM_MICRO_RESET		0x018a
#define MDIO_PMA_REG_M8051_MSGIN_REG	0xca12
#define MDIO_PMA_REG_M8051_MSGOUT_REG	0xca13
#define MDIO_PMA_REG_ROM_VER1		0xca19
#define MDIO_PMA_REG_ROM_VER2		0xca1a
#define MDIO_PMA_REG_EDC_FFE_MAIN	0xca1b
#define MDIO_PMA_REG_PLL_BANDWIDTH	0xca1d
#define MDIO_PMA_REG_PLL_CTRL		0xca1e
#define MDIO_PMA_REG_MISC_CTRL0 	0xca23
#define MDIO_PMA_REG_LRM_MODE		0xca3f
#define MDIO_PMA_REG_CDR_BANDWIDTH	0xca46
#define MDIO_PMA_REG_MISC_CTRL1 	0xca85

#define MDIO_PMA_REG_SFP_TWO_WIRE_CTRL		0x8000
#define MDIO_PMA_REG_SFP_TWO_WIRE_CTRL_STATUS_MASK	0x000c
#define MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_IDLE		0x0000
#define MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_COMPLETE	0x0004
#define MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_IN_PROGRESS	0x0008
#define MDIO_PMA_REG_SFP_TWO_WIRE_STATUS_FAILED 	0x000c
#define MDIO_PMA_REG_SFP_TWO_WIRE_BYTE_CNT	0x8002
#define MDIO_PMA_REG_SFP_TWO_WIRE_MEM_ADDR	0x8003
#define MDIO_PMA_REG_8726_TWO_WIRE_DATA_BUF	0xc820
#define MDIO_PMA_REG_8726_TWO_WIRE_DATA_MASK 0xff
#define MDIO_PMA_REG_8726_TX_CTRL1		0xca01
#define MDIO_PMA_REG_8726_TX_CTRL2		0xca05

#define MDIO_PMA_REG_8727_TWO_WIRE_SLAVE_ADDR	0x8005
#define MDIO_PMA_REG_8727_TWO_WIRE_DATA_BUF	0x8007
#define MDIO_PMA_REG_8727_TWO_WIRE_DATA_MASK 0xff
#define MDIO_PMA_REG_8727_TX_CTRL1		0xca02
#define MDIO_PMA_REG_8727_TX_CTRL2		0xca05
#define MDIO_PMA_REG_8727_PCS_OPT_CTRL		0xc808
#define MDIO_PMA_REG_8727_GPIO_CTRL		0xc80e
#define MDIO_PMA_REG_8727_PCS_GP		0xc842
#define MDIO_PMA_REG_8727_OPT_CFG_REG		0xc8e4

#define MDIO_AN_REG_8727_MISC_CTRL		0x8309

#define MDIO_PMA_REG_8073_CHIP_REV			0xc801
#define MDIO_PMA_REG_8073_SPEED_LINK_STATUS		0xc820
#define MDIO_PMA_REG_8073_XAUI_WA			0xc841
#define MDIO_PMA_REG_8073_OPT_DIGITAL_CTRL		0xcd08

#define MDIO_PMA_REG_7101_RESET 	0xc000
#define MDIO_PMA_REG_7107_LED_CNTL	0xc007
#define MDIO_PMA_REG_7107_LINK_LED_CNTL 0xc009
#define MDIO_PMA_REG_7101_VER1		0xc026
#define MDIO_PMA_REG_7101_VER2		0xc027

#define MDIO_PMA_REG_8481_PMD_SIGNAL			0xa811
#define MDIO_PMA_REG_8481_LED1_MASK			0xa82c
#define MDIO_PMA_REG_8481_LED2_MASK			0xa82f
#define MDIO_PMA_REG_8481_LED3_MASK			0xa832
#define MDIO_PMA_REG_8481_LED3_BLINK			0xa834
#define MDIO_PMA_REG_8481_LED5_MASK			0xa838
#define MDIO_PMA_REG_8481_SIGNAL_MASK			0xa835
#define MDIO_PMA_REG_8481_LINK_SIGNAL			0xa83b
#define MDIO_PMA_REG_8481_LINK_SIGNAL_LED4_ENABLE_MASK	0x800
#define MDIO_PMA_REG_8481_LINK_SIGNAL_LED4_ENABLE_SHIFT 11


#define MDIO_WIS_DEVAD			0x2
#define MDIO_WIS_REG_LASI_CNTL		0x9002
#define MDIO_WIS_REG_LASI_STATUS	0x9005

#define MDIO_PCS_DEVAD			0x3
#define MDIO_PCS_REG_STATUS		0x0020
#define MDIO_PCS_REG_LASI_STATUS	0x9005
#define MDIO_PCS_REG_7101_DSP_ACCESS	0xD000
#define MDIO_PCS_REG_7101_SPI_MUX	0xD008
#define MDIO_PCS_REG_7101_SPI_CTRL_ADDR 0xE12A
#define MDIO_PCS_REG_7101_SPI_RESET_BIT (5)
#define MDIO_PCS_REG_7101_SPI_FIFO_ADDR 0xE02A
#define MDIO_PCS_REG_7101_SPI_FIFO_ADDR_WRITE_ENABLE_CMD (6)
#define MDIO_PCS_REG_7101_SPI_FIFO_ADDR_BULK_ERASE_CMD	 (0xC7)
#define MDIO_PCS_REG_7101_SPI_FIFO_ADDR_PAGE_PROGRAM_CMD (2)
#define MDIO_PCS_REG_7101_SPI_BYTES_TO_TRANSFER_ADDR 0xE028


#define MDIO_XS_DEVAD			0x4
#define MDIO_XS_PLL_SEQUENCER		0x8000
#define MDIO_XS_SFX7101_XGXS_TEST1	0xc00a

#define MDIO_XS_8706_REG_BANK_RX0	0x80bc
#define MDIO_XS_8706_REG_BANK_RX1	0x80cc
#define MDIO_XS_8706_REG_BANK_RX2	0x80dc
#define MDIO_XS_8706_REG_BANK_RX3	0x80ec
#define MDIO_XS_8706_REG_BANK_RXA	0x80fc

#define MDIO_XS_REG_8073_RX_CTRL_PCIE	0x80FA

#define MDIO_AN_DEVAD			0x7
#define MDIO_AN_REG_CTRL		0x0000
#define MDIO_AN_REG_STATUS		0x0001
#define MDIO_AN_REG_STATUS_AN_COMPLETE		0x0020
#define MDIO_AN_REG_ADV_PAUSE		0x0010
#define MDIO_AN_REG_ADV_PAUSE_PAUSE		0x0400
#define MDIO_AN_REG_ADV_PAUSE_ASYMMETRIC	0x0800
#define MDIO_AN_REG_ADV_PAUSE_BOTH		0x0C00
#define MDIO_AN_REG_ADV_PAUSE_MASK		0x0C00
#define MDIO_AN_REG_ADV 		0x0011
#define MDIO_AN_REG_ADV2		0x0012
#define MDIO_AN_REG_LP_AUTO_NEG		0x0013
#define MDIO_AN_REG_LP_AUTO_NEG2	0x0014
#define MDIO_AN_REG_MASTER_STATUS	0x0021
#define MDIO_AN_REG_LINK_STATUS 	0x8304
#define MDIO_AN_REG_CL37_CL73		0x8370
#define MDIO_AN_REG_CL37_AN		0xffe0
#define MDIO_AN_REG_CL37_FC_LD		0xffe4
#define		MDIO_AN_REG_CL37_FC_LP		0xffe5
#define		MDIO_AN_REG_1000T_STATUS	0xffea

#define MDIO_AN_REG_8073_2_5G		0x8329
#define MDIO_AN_REG_8073_BAM		0x8350

#define MDIO_AN_REG_8481_10GBASE_T_AN_CTRL	0x0020
#define MDIO_AN_REG_8481_LEGACY_MII_CTRL	0xffe0
#define MDIO_AN_REG_8481_MII_CTRL_FORCE_1G	0x40
#define MDIO_AN_REG_8481_LEGACY_MII_STATUS	0xffe1
#define MDIO_AN_REG_8481_LEGACY_AN_ADV		0xffe4
#define MDIO_AN_REG_8481_LEGACY_AN_EXPANSION	0xffe6
#define MDIO_AN_REG_8481_1000T_CTRL		0xffe9
#define MDIO_AN_REG_8481_1G_100T_EXT_CTRL	0xfff0
#define MIDO_AN_REG_8481_EXT_CTRL_FORCE_LEDS_OFF	0x0008
#define MDIO_AN_REG_8481_EXPANSION_REG_RD_RW	0xfff5
#define MDIO_AN_REG_8481_EXPANSION_REG_ACCESS	0xfff7
#define MDIO_AN_REG_8481_AUX_CTRL		0xfff8
#define MDIO_AN_REG_8481_LEGACY_SHADOW		0xfffc

#define MDIO_CTL_DEVAD			0x1e
#define MDIO_CTL_REG_84823_MEDIA		0x401a
#define MDIO_CTL_REG_84823_MEDIA_MAC_MASK		0x0018
	
#define MDIO_CTL_REG_84823_CTRL_MAC_XFI			0x0008
#define MDIO_CTL_REG_84823_MEDIA_MAC_XAUI_M		0x0010
	
#define MDIO_CTL_REG_84823_MEDIA_LINE_MASK		0x0060
#define MDIO_CTL_REG_84823_MEDIA_LINE_XAUI_L		0x0020
#define MDIO_CTL_REG_84823_MEDIA_LINE_XFI		0x0040
#define MDIO_CTL_REG_84823_MEDIA_COPPER_CORE_DOWN	0x0080
#define MDIO_CTL_REG_84823_MEDIA_PRIORITY_MASK		0x0100
#define MDIO_CTL_REG_84823_MEDIA_PRIORITY_COPPER	0x0000
#define MDIO_CTL_REG_84823_MEDIA_PRIORITY_FIBER		0x0100
#define MDIO_CTL_REG_84823_MEDIA_FIBER_1G			0x1000
#define MDIO_CTL_REG_84823_USER_CTRL_REG			0x4005
#define MDIO_CTL_REG_84823_USER_CTRL_CMS			0x0080
#define MDIO_PMA_REG_84823_CTL_SLOW_CLK_CNT_HIGH		0xa82b
#define MDIO_PMA_REG_84823_BLINK_RATE_VAL_15P9HZ	0x2f
#define MDIO_PMA_REG_84823_CTL_LED_CTL_1			0xa8e3
#define MDIO_PMA_REG_84833_CTL_LED_CTL_1			0xa8ec
#define MDIO_PMA_REG_84823_LED3_STRETCH_EN			0x0080

#define MDIO_84833_TOP_CFG_XGPHY_STRAP1			0x401a
#define MDIO_84833_SUPER_ISOLATE		0x8000
#define MDIO_84833_TOP_CFG_SCRATCH_REG0			0x4005
#define MDIO_84833_TOP_CFG_SCRATCH_REG1			0x4006
#define MDIO_84833_TOP_CFG_SCRATCH_REG2			0x4007
#define MDIO_84833_TOP_CFG_SCRATCH_REG3			0x4008
#define MDIO_84833_TOP_CFG_SCRATCH_REG4			0x4009
#define MDIO_84833_TOP_CFG_SCRATCH_REG26		0x4037
#define MDIO_84833_TOP_CFG_SCRATCH_REG27		0x4038
#define MDIO_84833_TOP_CFG_SCRATCH_REG28		0x4039
#define MDIO_84833_TOP_CFG_SCRATCH_REG29		0x403a
#define MDIO_84833_TOP_CFG_SCRATCH_REG30		0x403b
#define MDIO_84833_TOP_CFG_SCRATCH_REG31		0x403c
#define MDIO_84833_CMD_HDLR_COMMAND	MDIO_84833_TOP_CFG_SCRATCH_REG0
#define MDIO_84833_CMD_HDLR_STATUS	MDIO_84833_TOP_CFG_SCRATCH_REG26
#define MDIO_84833_CMD_HDLR_DATA1	MDIO_84833_TOP_CFG_SCRATCH_REG27
#define MDIO_84833_CMD_HDLR_DATA2	MDIO_84833_TOP_CFG_SCRATCH_REG28
#define MDIO_84833_CMD_HDLR_DATA3	MDIO_84833_TOP_CFG_SCRATCH_REG29
#define MDIO_84833_CMD_HDLR_DATA4	MDIO_84833_TOP_CFG_SCRATCH_REG30
#define MDIO_84833_CMD_HDLR_DATA5	MDIO_84833_TOP_CFG_SCRATCH_REG31

#define PHY84833_CMD_SET_PAIR_SWAP			0x8001
#define PHY84833_CMD_GET_EEE_MODE			0x8008
#define PHY84833_CMD_SET_EEE_MODE			0x8009
#define PHY84833_STATUS_CMD_RECEIVED			0x0001
#define PHY84833_STATUS_CMD_IN_PROGRESS			0x0002
#define PHY84833_STATUS_CMD_COMPLETE_PASS		0x0004
#define PHY84833_STATUS_CMD_COMPLETE_ERROR		0x0008
#define PHY84833_STATUS_CMD_OPEN_FOR_CMDS		0x0010
#define PHY84833_STATUS_CMD_SYSTEM_BOOT			0x0020
#define PHY84833_STATUS_CMD_NOT_OPEN_FOR_CMDS		0x0040
#define PHY84833_STATUS_CMD_CLEAR_COMPLETE		0x0080
#define PHY84833_STATUS_CMD_OPEN_OVERRIDE		0xa5a5


#define MDIO_WC_DEVAD					0x3
#define MDIO_WC_REG_IEEE0BLK_MIICNTL			0x0
#define MDIO_WC_REG_IEEE0BLK_AUTONEGNP			0x7
#define MDIO_WC_REG_AN_IEEE1BLK_AN_ADVERTISEMENT0	0x10
#define MDIO_WC_REG_AN_IEEE1BLK_AN_ADVERTISEMENT1	0x11
#define MDIO_WC_REG_AN_IEEE1BLK_AN_ADVERTISEMENT2	0x12
#define MDIO_WC_REG_AN_IEEE1BLK_AN_ADV2_FEC_ABILITY	0x4000
#define MDIO_WC_REG_AN_IEEE1BLK_AN_ADV2_FEC_REQ		0x8000
#define MDIO_WC_REG_PMD_IEEE9BLK_TENGBASE_KR_PMD_CONTROL_REGISTER_150  0x96
#define MDIO_WC_REG_XGXSBLK0_XGXSCONTROL		0x8000
#define MDIO_WC_REG_XGXSBLK0_MISCCONTROL1		0x800e
#define MDIO_WC_REG_XGXSBLK1_DESKEW			0x8010
#define MDIO_WC_REG_XGXSBLK1_LANECTRL0			0x8015
#define MDIO_WC_REG_XGXSBLK1_LANECTRL1			0x8016
#define MDIO_WC_REG_XGXSBLK1_LANECTRL2			0x8017
#define MDIO_WC_REG_TX0_ANA_CTRL0			0x8061
#define MDIO_WC_REG_TX1_ANA_CTRL0			0x8071
#define MDIO_WC_REG_TX2_ANA_CTRL0			0x8081
#define MDIO_WC_REG_TX3_ANA_CTRL0			0x8091
#define MDIO_WC_REG_TX0_TX_DRIVER			0x8067
#define MDIO_WC_REG_TX0_TX_DRIVER_IPRE_DRIVER_OFFSET		0x04
#define MDIO_WC_REG_TX0_TX_DRIVER_IPRE_DRIVER_MASK			0x00f0
#define MDIO_WC_REG_TX0_TX_DRIVER_IDRIVER_OFFSET		0x08
#define MDIO_WC_REG_TX0_TX_DRIVER_IDRIVER_MASK				0x0f00
#define MDIO_WC_REG_TX0_TX_DRIVER_POST2_COEFF_OFFSET		0x0c
#define MDIO_WC_REG_TX0_TX_DRIVER_POST2_COEFF_MASK			0x7000
#define MDIO_WC_REG_TX1_TX_DRIVER			0x8077
#define MDIO_WC_REG_TX2_TX_DRIVER			0x8087
#define MDIO_WC_REG_TX3_TX_DRIVER			0x8097
#define MDIO_WC_REG_RX0_ANARXCONTROL1G			0x80b9
#define MDIO_WC_REG_RX2_ANARXCONTROL1G			0x80d9
#define MDIO_WC_REG_RX0_PCI_CTRL			0x80ba
#define MDIO_WC_REG_RX1_PCI_CTRL			0x80ca
#define MDIO_WC_REG_RX2_PCI_CTRL			0x80da
#define MDIO_WC_REG_RX3_PCI_CTRL			0x80ea
#define MDIO_WC_REG_XGXSBLK2_UNICORE_MODE_10G		0x8104
#define MDIO_WC_REG_XGXS_STATUS3			0x8129
#define MDIO_WC_REG_PAR_DET_10G_STATUS			0x8130
#define MDIO_WC_REG_PAR_DET_10G_CTRL			0x8131
#define MDIO_WC_REG_XGXS_X2_CONTROL2			0x8141
#define MDIO_WC_REG_XGXS_RX_LN_SWAP1			0x816B
#define MDIO_WC_REG_XGXS_TX_LN_SWAP1			0x8169
#define MDIO_WC_REG_GP2_STATUS_GP_2_0			0x81d0
#define MDIO_WC_REG_GP2_STATUS_GP_2_1			0x81d1
#define MDIO_WC_REG_GP2_STATUS_GP_2_2			0x81d2
#define MDIO_WC_REG_GP2_STATUS_GP_2_3			0x81d3
#define MDIO_WC_REG_GP2_STATUS_GP_2_4			0x81d4
#define MDIO_WC_REG_GP2_STATUS_GP_2_4_CL73_AN_CMPL 0x1000
#define MDIO_WC_REG_GP2_STATUS_GP_2_4_CL37_AN_CMPL 0x0100
#define MDIO_WC_REG_GP2_STATUS_GP_2_4_CL37_LP_AN_CAP 0x0010
#define MDIO_WC_REG_GP2_STATUS_GP_2_4_CL37_AN_CAP 0x1
#define MDIO_WC_REG_UC_INFO_B0_DEAD_TRAP		0x81EE
#define MDIO_WC_REG_UC_INFO_B1_VERSION			0x81F0
#define MDIO_WC_REG_UC_INFO_B1_FIRMWARE_MODE		0x81F2
#define MDIO_WC_REG_UC_INFO_B1_FIRMWARE_LANE0_OFFSET	0x0
#define MDIO_WC_REG_UC_INFO_B1_FIRMWARE_MODE_DEFAULT	    0x0
#define MDIO_WC_REG_UC_INFO_B1_FIRMWARE_MODE_SFP_OPT_LR	    0x1
#define MDIO_WC_REG_UC_INFO_B1_FIRMWARE_MODE_SFP_DAC	    0x2
#define MDIO_WC_REG_UC_INFO_B1_FIRMWARE_MODE_SFP_XLAUI	    0x3
#define MDIO_WC_REG_UC_INFO_B1_FIRMWARE_MODE_LONG_CH_6G	    0x4
#define MDIO_WC_REG_UC_INFO_B1_FIRMWARE_LANE1_OFFSET	0x4
#define MDIO_WC_REG_UC_INFO_B1_FIRMWARE_LANE2_OFFSET	0x8
#define MDIO_WC_REG_UC_INFO_B1_FIRMWARE_LANE3_OFFSET	0xc
#define MDIO_WC_REG_UC_INFO_B1_CRC			0x81FE
#define MDIO_WC_REG_DSC_SMC				0x8213
#define MDIO_WC_REG_DSC2B0_DSC_MISC_CTRL0		0x821e
#define MDIO_WC_REG_TX_FIR_TAP				0x82e2
#define MDIO_WC_REG_TX_FIR_TAP_PRE_TAP_OFFSET		0x00
#define MDIO_WC_REG_TX_FIR_TAP_PRE_TAP_MASK			0x000f
#define MDIO_WC_REG_TX_FIR_TAP_MAIN_TAP_OFFSET		0x04
#define MDIO_WC_REG_TX_FIR_TAP_MAIN_TAP_MASK		0x03f0
#define MDIO_WC_REG_TX_FIR_TAP_POST_TAP_OFFSET		0x0a
#define MDIO_WC_REG_TX_FIR_TAP_POST_TAP_MASK		0x7c00
#define MDIO_WC_REG_TX_FIR_TAP_ENABLE		0x8000
#define MDIO_WC_REG_CL72_USERB0_CL72_MISC1_CONTROL	0x82e3
#define MDIO_WC_REG_CL72_USERB0_CL72_OS_DEF_CTRL	0x82e6
#define MDIO_WC_REG_CL72_USERB0_CL72_BR_DEF_CTRL	0x82e7
#define MDIO_WC_REG_CL72_USERB0_CL72_2P5_DEF_CTRL	0x82e8
#define MDIO_WC_REG_CL72_USERB0_CL72_MISC4_CONTROL	0x82ec
#define MDIO_WC_REG_SERDESDIGITAL_CONTROL1000X1		0x8300
#define MDIO_WC_REG_SERDESDIGITAL_CONTROL1000X2		0x8301
#define MDIO_WC_REG_SERDESDIGITAL_CONTROL1000X3		0x8302
#define MDIO_WC_REG_SERDESDIGITAL_STATUS1000X1		0x8304
#define MDIO_WC_REG_SERDESDIGITAL_MISC1			0x8308
#define MDIO_WC_REG_SERDESDIGITAL_MISC2			0x8309
#define MDIO_WC_REG_DIGITAL3_UP1			0x8329
#define MDIO_WC_REG_DIGITAL3_LP_UP1			 0x832c
#define MDIO_WC_REG_DIGITAL4_MISC3			0x833c
#define MDIO_WC_REG_DIGITAL5_MISC6			0x8345
#define MDIO_WC_REG_DIGITAL5_MISC7			0x8349
#define MDIO_WC_REG_DIGITAL5_ACTUAL_SPEED		0x834e
#define MDIO_WC_REG_DIGITAL6_MP5_NEXTPAGECTRL		0x8350
#define MDIO_WC_REG_CL49_USERB0_CTRL			0x8368
#define MDIO_WC_REG_TX66_CONTROL			0x83b0
#define MDIO_WC_REG_RX66_CONTROL			0x83c0
#define MDIO_WC_REG_RX66_SCW0				0x83c2
#define MDIO_WC_REG_RX66_SCW1				0x83c3
#define MDIO_WC_REG_RX66_SCW2				0x83c4
#define MDIO_WC_REG_RX66_SCW3				0x83c5
#define MDIO_WC_REG_RX66_SCW0_MASK			0x83c6
#define MDIO_WC_REG_RX66_SCW1_MASK			0x83c7
#define MDIO_WC_REG_RX66_SCW2_MASK			0x83c8
#define MDIO_WC_REG_RX66_SCW3_MASK			0x83c9
#define MDIO_WC_REG_FX100_CTRL1				0x8400
#define MDIO_WC_REG_FX100_CTRL3				0x8402

#define MDIO_WC_REG_MICROBLK_CMD			0xffc2
#define MDIO_WC_REG_MICROBLK_DL_STATUS			0xffc5
#define MDIO_WC_REG_MICROBLK_CMD3			0xffcc

#define MDIO_WC_REG_AERBLK_AER				0xffde
#define MDIO_WC_REG_COMBO_IEEE0_MIICTRL			0xffe0
#define MDIO_WC_REG_COMBO_IEEE0_MIIISTAT		0xffe1

#define MDIO_WC0_XGXS_BLK2_LANE_RESET			0x810A
#define MDIO_WC0_XGXS_BLK2_LANE_RESET_RX_BITSHIFT	0
#define MDIO_WC0_XGXS_BLK2_LANE_RESET_TX_BITSHIFT	4

#define MDIO_WC0_XGXS_BLK6_XGXS_X2_CONTROL2		0x8141

#define DIGITAL5_ACTUAL_SPEED_TX_MASK			0x003f

#define MDIO_REG_GPHY_PHYID_LSB				0x3
#define MDIO_REG_GPHY_ID_54618SE		0x5cd5
#define MDIO_REG_GPHY_CL45_ADDR_REG			0xd
#define MDIO_REG_GPHY_CL45_DATA_REG			0xe
#define MDIO_REG_GPHY_EEE_ADV			0x3c
#define MDIO_REG_GPHY_EEE_1G		(0x1 << 2)
#define MDIO_REG_GPHY_EEE_100		(0x1 << 1)
#define MDIO_REG_GPHY_EEE_RESOLVED		0x803e
#define MDIO_REG_INTR_STATUS				0x1a
#define MDIO_REG_INTR_MASK				0x1b
#define MDIO_REG_INTR_MASK_LINK_STATUS			(0x1 << 1)
#define MDIO_REG_GPHY_SHADOW				0x1c
#define MDIO_REG_GPHY_SHADOW_LED_SEL1			(0x0d << 10)
#define MDIO_REG_GPHY_SHADOW_LED_SEL2			(0x0e << 10)
#define MDIO_REG_GPHY_SHADOW_WR_ENA			(0x1 << 15)
#define MDIO_REG_GPHY_SHADOW_AUTO_DET_MED		(0x1e << 10)
#define MDIO_REG_GPHY_SHADOW_INVERT_FIB_SD		(0x1 << 8)

#define IGU_FUNC_BASE			0x0400

#define IGU_ADDR_MSIX			0x0000
#define IGU_ADDR_INT_ACK		0x0200
#define IGU_ADDR_PROD_UPD		0x0201
#define IGU_ADDR_ATTN_BITS_UPD	0x0202
#define IGU_ADDR_ATTN_BITS_SET	0x0203
#define IGU_ADDR_ATTN_BITS_CLR	0x0204
#define IGU_ADDR_COALESCE_NOW	0x0205
#define IGU_ADDR_SIMD_MASK		0x0206
#define IGU_ADDR_SIMD_NOMASK	0x0207
#define IGU_ADDR_MSI_CTL		0x0210
#define IGU_ADDR_MSI_ADDR_LO	0x0211
#define IGU_ADDR_MSI_ADDR_HI	0x0212
#define IGU_ADDR_MSI_DATA		0x0213

#define IGU_USE_REGISTER_ustorm_type_0_sb_cleanup  0
#define IGU_USE_REGISTER_ustorm_type_1_sb_cleanup  1
#define IGU_USE_REGISTER_cstorm_type_0_sb_cleanup  2
#define IGU_USE_REGISTER_cstorm_type_1_sb_cleanup  3

#define COMMAND_REG_INT_ACK	    0x0
#define COMMAND_REG_PROD_UPD	    0x4
#define COMMAND_REG_ATTN_BITS_UPD   0x8
#define COMMAND_REG_ATTN_BITS_SET   0xc
#define COMMAND_REG_ATTN_BITS_CLR   0x10
#define COMMAND_REG_COALESCE_NOW    0x14
#define COMMAND_REG_SIMD_MASK	    0x18
#define COMMAND_REG_SIMD_NOMASK     0x1c


#define IGU_MEM_BASE						0x0000

#define IGU_MEM_MSIX_BASE					0x0000
#define IGU_MEM_MSIX_UPPER					0x007f
#define IGU_MEM_MSIX_RESERVED_UPPER			0x01ff

#define IGU_MEM_PBA_MSIX_BASE				0x0200
#define IGU_MEM_PBA_MSIX_UPPER				0x0200

#define IGU_CMD_BACKWARD_COMP_PROD_UPD		0x0201
#define IGU_MEM_PBA_MSIX_RESERVED_UPPER 	0x03ff

#define IGU_CMD_INT_ACK_BASE				0x0400
#define IGU_CMD_INT_ACK_UPPER\
	(IGU_CMD_INT_ACK_BASE + MAX_SB_PER_PORT * NUM_OF_PORTS_PER_PATH - 1)
#define IGU_CMD_INT_ACK_RESERVED_UPPER		0x04ff

#define IGU_CMD_E2_PROD_UPD_BASE			0x0500
#define IGU_CMD_E2_PROD_UPD_UPPER\
	(IGU_CMD_E2_PROD_UPD_BASE + MAX_SB_PER_PORT * NUM_OF_PORTS_PER_PATH - 1)
#define IGU_CMD_E2_PROD_UPD_RESERVED_UPPER	0x059f

#define IGU_CMD_ATTN_BIT_UPD_UPPER			0x05a0
#define IGU_CMD_ATTN_BIT_SET_UPPER			0x05a1
#define IGU_CMD_ATTN_BIT_CLR_UPPER			0x05a2

#define IGU_REG_SISR_MDPC_WMASK_UPPER		0x05a3
#define IGU_REG_SISR_MDPC_WMASK_LSB_UPPER	0x05a4
#define IGU_REG_SISR_MDPC_WMASK_MSB_UPPER	0x05a5
#define IGU_REG_SISR_MDPC_WOMASK_UPPER		0x05a6

#define IGU_REG_RESERVED_UPPER				0x05ff
#define IGU_PF_CONF_FUNC_EN	  (0x1<<0)  
#define IGU_PF_CONF_MSI_MSIX_EN   (0x1<<1)  
#define IGU_PF_CONF_INT_LINE_EN   (0x1<<2)  
#define IGU_PF_CONF_ATTN_BIT_EN   (0x1<<3)  
#define IGU_PF_CONF_SINGLE_ISR_EN (0x1<<4)  
#define IGU_PF_CONF_SIMD_MODE	  (0x1<<5)  

#define IGU_VF_CONF_FUNC_EN	   (0x1<<0)  
#define IGU_VF_CONF_MSI_MSIX_EN    (0x1<<1)  
#define IGU_VF_CONF_PARENT_MASK    (0x3<<2)  
#define IGU_VF_CONF_PARENT_SHIFT   2	     
#define IGU_VF_CONF_SINGLE_ISR_EN  (0x1<<4)  


#define IGU_BC_DSB_NUM_SEGS    5
#define IGU_BC_NDSB_NUM_SEGS   2
#define IGU_NORM_DSB_NUM_SEGS  2
#define IGU_NORM_NDSB_NUM_SEGS 1
#define IGU_BC_BASE_DSB_PROD   128
#define IGU_NORM_BASE_DSB_PROD 136

#define IGU_FID_ENCODE_IS_PF	    (0x1<<6)
#define IGU_FID_ENCODE_IS_PF_SHIFT  6
#define IGU_FID_VF_NUM_MASK	    (0x3f)
#define IGU_FID_PF_NUM_MASK	    (0x7)

#define IGU_REG_MAPPING_MEMORY_VALID		(1<<0)
#define IGU_REG_MAPPING_MEMORY_VECTOR_MASK	(0x3F<<1)
#define IGU_REG_MAPPING_MEMORY_VECTOR_SHIFT	1
#define IGU_REG_MAPPING_MEMORY_FID_MASK	(0x7F<<7)
#define IGU_REG_MAPPING_MEMORY_FID_SHIFT	7


#define CDU_REGION_NUMBER_XCM_AG 2
#define CDU_REGION_NUMBER_UCM_AG 4


#define CDU_VALID_DATA(_cid, _region, _type)\
	(((_cid) << 8) | (((_region)&0xf)<<4) | (((_type)&0xf)))
#define CDU_CRC8(_cid, _region, _type)\
	(calc_crc8(CDU_VALID_DATA(_cid, _region, _type), 0xff))
#define CDU_RSRVD_VALUE_TYPE_A(_cid, _region, _type)\
	(0x80 | ((CDU_CRC8(_cid, _region, _type)) & 0x7f))
#define CDU_RSRVD_VALUE_TYPE_B(_crc, _type)\
	(0x80 | ((_type)&0xf << 3) | ((CDU_CRC8(_cid, _region, _type)) & 0x7))
#define CDU_RSRVD_INVALIDATE_CONTEXT_VALUE(_val) ((_val) & ~0x80)

static inline u8 calc_crc8(u32 data, u8 crc)
{
	u8 D[32];
	u8 NewCRC[8];
	u8 C[8];
	u8 crc_res;
	u8 i;

	
	for (i = 0; i < 32; i++) {
		D[i] = (u8)(data & 1);
		data = data >> 1;
	}

	
	for (i = 0; i < 8; i++) {
		C[i] = crc & 1;
		crc = crc >> 1;
	}

	NewCRC[0] = D[31] ^ D[30] ^ D[28] ^ D[23] ^ D[21] ^ D[19] ^ D[18] ^
		    D[16] ^ D[14] ^ D[12] ^ D[8] ^ D[7] ^ D[6] ^ D[0] ^ C[4] ^
		    C[6] ^ C[7];
	NewCRC[1] = D[30] ^ D[29] ^ D[28] ^ D[24] ^ D[23] ^ D[22] ^ D[21] ^
		    D[20] ^ D[18] ^ D[17] ^ D[16] ^ D[15] ^ D[14] ^ D[13] ^
		    D[12] ^ D[9] ^ D[6] ^ D[1] ^ D[0] ^ C[0] ^ C[4] ^ C[5] ^
		    C[6];
	NewCRC[2] = D[29] ^ D[28] ^ D[25] ^ D[24] ^ D[22] ^ D[17] ^ D[15] ^
		    D[13] ^ D[12] ^ D[10] ^ D[8] ^ D[6] ^ D[2] ^ D[1] ^ D[0] ^
		    C[0] ^ C[1] ^ C[4] ^ C[5];
	NewCRC[3] = D[30] ^ D[29] ^ D[26] ^ D[25] ^ D[23] ^ D[18] ^ D[16] ^
		    D[14] ^ D[13] ^ D[11] ^ D[9] ^ D[7] ^ D[3] ^ D[2] ^ D[1] ^
		    C[1] ^ C[2] ^ C[5] ^ C[6];
	NewCRC[4] = D[31] ^ D[30] ^ D[27] ^ D[26] ^ D[24] ^ D[19] ^ D[17] ^
		    D[15] ^ D[14] ^ D[12] ^ D[10] ^ D[8] ^ D[4] ^ D[3] ^ D[2] ^
		    C[0] ^ C[2] ^ C[3] ^ C[6] ^ C[7];
	NewCRC[5] = D[31] ^ D[28] ^ D[27] ^ D[25] ^ D[20] ^ D[18] ^ D[16] ^
		    D[15] ^ D[13] ^ D[11] ^ D[9] ^ D[5] ^ D[4] ^ D[3] ^ C[1] ^
		    C[3] ^ C[4] ^ C[7];
	NewCRC[6] = D[29] ^ D[28] ^ D[26] ^ D[21] ^ D[19] ^ D[17] ^ D[16] ^
		    D[14] ^ D[12] ^ D[10] ^ D[6] ^ D[5] ^ D[4] ^ C[2] ^ C[4] ^
		    C[5];
	NewCRC[7] = D[30] ^ D[29] ^ D[27] ^ D[22] ^ D[20] ^ D[18] ^ D[17] ^
		    D[15] ^ D[13] ^ D[11] ^ D[7] ^ D[6] ^ D[5] ^ C[3] ^ C[5] ^
		    C[6];

	crc_res = 0;
	for (i = 0; i < 8; i++)
		crc_res |= (NewCRC[i] << i);

	return crc_res;
}


#endif 
