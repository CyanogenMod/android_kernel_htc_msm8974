/*
 * Copyright (C) Freescale Semicondutor, Inc. 2006-2009. All rights reserved.
 *
 * Author: Shlomi Gridish <gridish@freescale.com>
 *
 * Description:
 * Internal header file for UCC Gigabit Ethernet unit routines.
 *
 * Changelog:
 * Jun 28, 2006 Li Yang <LeoLi@freescale.com>
 * - Rearrange code and style fixes
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef __UCC_GETH_H__
#define __UCC_GETH_H__

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/if_ether.h>

#include <asm/immap_qe.h>
#include <asm/qe.h>

#include <asm/ucc.h>
#include <asm/ucc_fast.h>

#define DRV_DESC "QE UCC Gigabit Ethernet Controller"
#define DRV_NAME "ucc_geth"
#define DRV_VERSION "1.1"

#define NUM_TX_QUEUES                   8
#define NUM_RX_QUEUES                   8
#define NUM_BDS_IN_PREFETCHED_BDS       4
#define TX_IP_OFFSET_ENTRY_MAX          8
#define NUM_OF_PADDRS                   4
#define ENET_INIT_PARAM_MAX_ENTRIES_RX  9
#define ENET_INIT_PARAM_MAX_ENTRIES_TX  8

struct ucc_geth {
	struct ucc_fast uccf;
	u8 res0[0x100 - sizeof(struct ucc_fast)];

	u32 maccfg1;		
	u32 maccfg2;		
	u32 ipgifg;		
	u32 hafdup;		
	u8 res1[0x10];
	u8 miimng[0x18];	
	u32 ifctl;		
	u32 ifstat;		
	u32 macstnaddr1;	
	u32 macstnaddr2;	
	u8 res2[0x8];
	u32 uempr;		
	u32 utbipar;		
	u16 uescr;		
	u8 res3[0x180 - 0x15A];
	u32 tx64;		
	u32 tx127;		
	u32 tx255;		
	u32 rx64;		
	u32 rx127;		
	u32 rx255;		
	u32 txok;		
	u16 txcf;		
	u8 res4[0x2];
	u32 tmca;		
	u32 tbca;		
	u32 rxfok;		
	u32 rxbok;		
	u32 rbyt;		
	u32 rmca;		
	u32 rbca;		
	u32 scar;		
	u32 scam;		
	u8 res5[0x200 - 0x1c4];
} __packed;

#define TEMODER_TX_RMON_STATISTICS_ENABLE       0x0100	
#define TEMODER_SCHEDULER_ENABLE                0x2000	
#define TEMODER_IP_CHECKSUM_GENERATE            0x0400	
#define TEMODER_PERFORMANCE_OPTIMIZATION_MODE1  0x0200	
#define TEMODER_RMON_STATISTICS                 0x0100	
#define TEMODER_NUM_OF_QUEUES_SHIFT             (15-15)	

#define REMODER_RX_RMON_STATISTICS_ENABLE       0x00001000	
#define REMODER_RX_EXTENDED_FEATURES            0x80000000	
#define REMODER_VLAN_OPERATION_TAGGED_SHIFT     (31-9 )	
#define REMODER_VLAN_OPERATION_NON_TAGGED_SHIFT (31-10)	
#define REMODER_RX_QOS_MODE_SHIFT               (31-15)	
#define REMODER_RMON_STATISTICS                 0x00001000	
#define REMODER_RX_EXTENDED_FILTERING           0x00000800	
#define REMODER_NUM_OF_QUEUES_SHIFT             (31-23)	
#define REMODER_DYNAMIC_MAX_FRAME_LENGTH        0x00000008	
#define REMODER_DYNAMIC_MIN_FRAME_LENGTH        0x00000004	
#define REMODER_IP_CHECKSUM_CHECK               0x00000002	
#define REMODER_IP_ADDRESS_ALIGNMENT            0x00000001	

#define UCCE_TXB   (UCC_GETH_UCCE_TXB7 | UCC_GETH_UCCE_TXB6 | \
		    UCC_GETH_UCCE_TXB5 | UCC_GETH_UCCE_TXB4 | \
		    UCC_GETH_UCCE_TXB3 | UCC_GETH_UCCE_TXB2 | \
		    UCC_GETH_UCCE_TXB1 | UCC_GETH_UCCE_TXB0)

#define UCCE_RXB   (UCC_GETH_UCCE_RXB7 | UCC_GETH_UCCE_RXB6 | \
		    UCC_GETH_UCCE_RXB5 | UCC_GETH_UCCE_RXB4 | \
		    UCC_GETH_UCCE_RXB3 | UCC_GETH_UCCE_RXB2 | \
		    UCC_GETH_UCCE_RXB1 | UCC_GETH_UCCE_RXB0)

#define UCCE_RXF   (UCC_GETH_UCCE_RXF7 | UCC_GETH_UCCE_RXF6 | \
		    UCC_GETH_UCCE_RXF5 | UCC_GETH_UCCE_RXF4 | \
		    UCC_GETH_UCCE_RXF3 | UCC_GETH_UCCE_RXF2 | \
		    UCC_GETH_UCCE_RXF1 | UCC_GETH_UCCE_RXF0)

#define UCCE_OTHER (UCC_GETH_UCCE_SCAR | UCC_GETH_UCCE_GRA | \
		    UCC_GETH_UCCE_CBPR | UCC_GETH_UCCE_BSY | \
		    UCC_GETH_UCCE_RXC  | UCC_GETH_UCCE_TXC | UCC_GETH_UCCE_TXE)

#define UCCE_RX_EVENTS  (UCCE_RXF | UCC_GETH_UCCE_BSY)
#define UCCE_TX_EVENTS	(UCCE_TXB | UCC_GETH_UCCE_TXE)

#define	ENET_TBI_MII_CR		0x00	
#define	ENET_TBI_MII_SR		0x01	
#define	ENET_TBI_MII_ANA	0x04	
#define	ENET_TBI_MII_ANLPBPA	0x05	
#define	ENET_TBI_MII_ANEX	0x06	
#define	ENET_TBI_MII_ANNPT	0x07	
#define	ENET_TBI_MII_ANLPANP	0x08	
#define	ENET_TBI_MII_EXST	0x0F	
#define	ENET_TBI_MII_JD		0x10	
#define	ENET_TBI_MII_TBICON	0x11	

#define TBISR_LSTATUS          0x0004
#define TBICON_CLK_SELECT       0x0020
#define TBIANA_ASYMMETRIC_PAUSE 0x0100
#define TBIANA_SYMMETRIC_PAUSE  0x0080
#define TBIANA_HALF_DUPLEX      0x0040
#define TBIANA_FULL_DUPLEX      0x0020
#define TBICR_PHY_RESET         0x8000
#define TBICR_ANEG_ENABLE       0x1000
#define TBICR_RESTART_ANEG      0x0200
#define TBICR_FULL_DUPLEX       0x0100
#define TBICR_SPEED1_SET        0x0040

#define TBIANA_SETTINGS ( \
		TBIANA_ASYMMETRIC_PAUSE \
		| TBIANA_SYMMETRIC_PAUSE \
		| TBIANA_FULL_DUPLEX \
		)
#define TBICR_SETTINGS ( \
		TBICR_PHY_RESET \
		| TBICR_ANEG_ENABLE \
		| TBICR_FULL_DUPLEX \
		| TBICR_SPEED1_SET \
		)

#define MACCFG1_FLOW_RX                         0x00000020	
#define MACCFG1_FLOW_TX                         0x00000010	
#define MACCFG1_ENABLE_SYNCHED_RX               0x00000008	
#define MACCFG1_ENABLE_RX                       0x00000004	
#define MACCFG1_ENABLE_SYNCHED_TX               0x00000002	
#define MACCFG1_ENABLE_TX                       0x00000001	

#define MACCFG2_PREL_SHIFT                      (31 - 19)	
#define MACCFG2_PREL_MASK                       0x0000f000	
#define MACCFG2_SRP                             0x00000080	
#define MACCFG2_STP                             0x00000040	
#define MACCFG2_RESERVED_1                      0x00000020	
#define MACCFG2_LC                              0x00000010	
#define MACCFG2_MPE                             0x00000008	
#define MACCFG2_FDX                             0x00000001	
#define MACCFG2_FDX_MASK                        0x00000001	
#define MACCFG2_PAD_CRC                         0x00000004
#define MACCFG2_CRC_EN                          0x00000002
#define MACCFG2_PAD_AND_CRC_MODE_NONE           0x00000000	
#define MACCFG2_PAD_AND_CRC_MODE_CRC_ONLY       0x00000002	
#define MACCFG2_PAD_AND_CRC_MODE_PAD_AND_CRC    0x00000004
#define MACCFG2_INTERFACE_MODE_NIBBLE           0x00000100	
#define MACCFG2_INTERFACE_MODE_BYTE             0x00000200	
#define MACCFG2_INTERFACE_MODE_MASK             0x00000300	

#define IPGIFG_NON_BACK_TO_BACK_IFG_PART1_SHIFT (31 -  7)	
#define IPGIFG_NON_BACK_TO_BACK_IFG_PART2_SHIFT (31 - 15)	
#define IPGIFG_MINIMUM_IFG_ENFORCEMENT_SHIFT    (31 - 23)	
#define IPGIFG_BACK_TO_BACK_IFG_SHIFT           (31 - 31)	
#define IPGIFG_NON_BACK_TO_BACK_IFG_PART1_MAX   127	
#define IPGIFG_NON_BACK_TO_BACK_IFG_PART2_MAX   127	
#define IPGIFG_MINIMUM_IFG_ENFORCEMENT_MAX      255	
#define IPGIFG_BACK_TO_BACK_IFG_MAX             127	
#define IPGIFG_NBTB_CS_IPG_MASK                 0x7F000000
#define IPGIFG_NBTB_IPG_MASK                    0x007F0000
#define IPGIFG_MIN_IFG_MASK                     0x0000FF00
#define IPGIFG_BTB_IPG_MASK                     0x0000007F

#define HALFDUP_ALT_BEB_TRUNCATION_SHIFT        (31 - 11)	
#define HALFDUP_ALT_BEB_TRUNCATION_MAX          0xf	
#define HALFDUP_ALT_BEB                         0x00080000	
#define HALFDUP_BACK_PRESSURE_NO_BACKOFF        0x00040000	
#define HALFDUP_NO_BACKOFF                      0x00020000	
#define HALFDUP_EXCESSIVE_DEFER                 0x00010000	
#define HALFDUP_MAX_RETRANSMISSION_SHIFT        (31 - 19)	
#define HALFDUP_MAX_RETRANSMISSION_MAX          0xf	
#define HALFDUP_COLLISION_WINDOW_SHIFT          (31 - 31)	
#define HALFDUP_COLLISION_WINDOW_MAX            0x3f	
#define HALFDUP_ALT_BEB_TR_MASK                 0x00F00000
#define HALFDUP_RETRANS_MASK                    0x0000F000
#define HALFDUP_COL_WINDOW_MASK                 0x0000003F

#define UCCS_BPR                                0x02	
#define UCCS_PAU                                0x02	
#define UCCS_MPD                                0x01	

#define IFSTAT_EXCESS_DEFER                     0x00000200	

#define MACSTNADDR1_OCTET_6_SHIFT               (31 -  7)	
#define MACSTNADDR1_OCTET_5_SHIFT               (31 - 15)	
#define MACSTNADDR1_OCTET_4_SHIFT               (31 - 23)	
#define MACSTNADDR1_OCTET_3_SHIFT               (31 - 31)	

#define MACSTNADDR2_OCTET_2_SHIFT               (31 -  7)	
#define MACSTNADDR2_OCTET_1_SHIFT               (31 - 15)	

#define UEMPR_PAUSE_TIME_VALUE_SHIFT            (31 - 15)	
#define UEMPR_EXTENDED_PAUSE_TIME_VALUE_SHIFT   (31 - 31)	

#define UTBIPAR_PHY_ADDRESS_SHIFT               (31 - 31)	
#define UTBIPAR_PHY_ADDRESS_MASK                0x0000001f	

#define UESCR_AUTOZ                             0x8000	
#define UESCR_CLRCNT                            0x4000	
#define UESCR_MAXCOV_SHIFT                      (15 -  7)	
#define UESCR_SCOV_SHIFT                        (15 - 15)	

#define UDSR_MAGIC                              0x067E

struct ucc_geth_thread_data_tx {
	u8 res0[104];
} __packed;

struct ucc_geth_thread_data_rx {
	u8 res0[40];
} __packed;

struct ucc_geth_send_queue_qd {
	u32 bd_ring_base;	
	u8 res0[0x8];
	u32 last_bd_completed_address;
	u8 res1[0x30];
} __packed;

struct ucc_geth_send_queue_mem_region {
	struct ucc_geth_send_queue_qd sqqd[NUM_TX_QUEUES];
} __packed;

struct ucc_geth_thread_tx_pram {
	u8 res0[64];
} __packed;

struct ucc_geth_thread_rx_pram {
	u8 res0[128];
} __packed;

#define THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING        64
#define THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING_8      64
#define THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING_16     96

struct ucc_geth_scheduler {
	u16 cpucount0;		
	u16 cpucount1;		
	u16 cecount0;		
	u16 cecount1;		
	u16 cpucount2;		
	u16 cpucount3;		
	u16 cecount2;		
	u16 cecount3;		
	u16 cpucount4;		
	u16 cpucount5;		
	u16 cecount4;		
	u16 cecount5;		
	u16 cpucount6;		
	u16 cpucount7;		
	u16 cecount6;		
	u16 cecount7;		
	u32 weightstatus[NUM_TX_QUEUES];	
	u32 rtsrshadow;		
	u32 time;		
	u32 ttl;		
	u32 mblinterval;	
	u16 nortsrbytetime;	
	u8 fracsiz;		
	u8 res0[1];
	u8 strictpriorityq;	
	u8 txasap;		
	u8 extrabw;		
	u8 oldwfqmask;		
	u8 weightfactor[NUM_TX_QUEUES];
				      
	u32 minw;		
	u8 res1[0x70 - 0x64];
} __packed;

struct ucc_geth_tx_firmware_statistics_pram {
	u32 sicoltx;		
	u32 mulcoltx;		
	u32 latecoltxfr;	
	u32 frabortduecol;	
	u32 frlostinmactxer;	
	u32 carriersenseertx;	
	u32 frtxok;		
	u32 txfrexcessivedefer;	
	u32 txpkts256;		
	u32 txpkts512;		
	u32 txpkts1024;		
	u32 txpktsjumbo;	
} __packed;

struct ucc_geth_rx_firmware_statistics_pram {
	u32 frrxfcser;		
	u32 fraligner;		
	u32 inrangelenrxer;	
	u32 outrangelenrxer;	
	u32 frtoolong;		
	u32 runt;		
	u32 verylongevent;	
	u32 symbolerror;	
	u32 dropbsy;		
	u8 res0[0x8];
	u32 mismatchdrop;	
	u32 underpkts;		
	u32 pkts256;		
	u32 pkts512;		
	u32 pkts1024;		
	u32 pktsjumbo;		
	u32 frlossinmacer;	
	u32 pausefr;		
	u8 res1[0x4];
	u32 removevlan;		
	u32 replacevlan;	
	u32 insertvlan;		
} __packed;

struct ucc_geth_rx_interrupt_coalescing_entry {
	u32 interruptcoalescingmaxvalue;	
	u32 interruptcoalescingcounter;	
} __packed;

struct ucc_geth_rx_interrupt_coalescing_table {
	struct ucc_geth_rx_interrupt_coalescing_entry coalescingentry[NUM_RX_QUEUES];
				       
} __packed;

struct ucc_geth_rx_prefetched_bds {
	struct qe_bd bd[NUM_BDS_IN_PREFETCHED_BDS];	
} __packed;

struct ucc_geth_rx_bd_queues_entry {
	u32 bdbaseptr;		
	u32 bdptr;		
	u32 externalbdbaseptr;	
	u32 externalbdptr;	
} __packed;

struct ucc_geth_tx_global_pram {
	u16 temoder;
	u8 res0[0x38 - 0x02];
	u32 sqptr;		
	u32 schedulerbasepointer;	
	u32 txrmonbaseptr;	
	u32 tstate;		
	u8 iphoffset[TX_IP_OFFSET_ENTRY_MAX];
	u32 vtagtable[0x8];	
	u32 tqptr;		
	u8 res2[0x80 - 0x74];
} __packed;

struct ucc_geth_exf_global_pram {
	u32 l2pcdptr;		
	u8 res0[0x10 - 0x04];
} __packed;

struct ucc_geth_rx_global_pram {
	u32 remoder;		
	u32 rqptr;		
	u32 res0[0x1];
	u8 res1[0x20 - 0xC];
	u16 typeorlen;		
	u8 res2[0x1];
	u8 rxgstpack;		
	u32 rxrmonbaseptr;	
	u8 res3[0x30 - 0x28];
	u32 intcoalescingptr;	
	u8 res4[0x36 - 0x34];
	u8 rstate;		
	u8 res5[0x46 - 0x37];
	u16 mrblr;		
	u32 rbdqptr;		
	u16 mflr;		
	u16 minflr;		
	u16 maxd1;		
	u16 maxd2;		
	u32 ecamptr;		
	u32 l2qt;		
	u32 l3qt[0x8];		
	u16 vlantype;		
	u16 vlantci;		
	u8 addressfiltering[64];	
	u32 exfGlobalParam;	
	u8 res6[0x100 - 0xC4];	
} __packed;

#define GRACEFUL_STOP_ACKNOWLEDGE_RX            0x01

struct ucc_geth_init_pram {
	u8 resinit1;
	u8 resinit2;
	u8 resinit3;
	u8 resinit4;
	u16 resinit5;
	u8 res1[0x1];
	u8 largestexternallookupkeysize;
	u32 rgftgfrxglobal;
	u32 rxthread[ENET_INIT_PARAM_MAX_ENTRIES_RX];	
	u8 res2[0x38 - 0x30];
	u32 txglobal;		
	u32 txthread[ENET_INIT_PARAM_MAX_ENTRIES_TX];	
	u8 res3[0x1];
} __packed;

#define ENET_INIT_PARAM_RGF_SHIFT               (32 - 4)
#define ENET_INIT_PARAM_TGF_SHIFT               (32 - 8)

#define ENET_INIT_PARAM_RISC_MASK               0x0000003f
#define ENET_INIT_PARAM_PTR_MASK                0x00ffffc0
#define ENET_INIT_PARAM_SNUM_MASK               0xff000000
#define ENET_INIT_PARAM_SNUM_SHIFT              24

#define ENET_INIT_PARAM_MAGIC_RES_INIT1         0x06
#define ENET_INIT_PARAM_MAGIC_RES_INIT2         0x30
#define ENET_INIT_PARAM_MAGIC_RES_INIT3         0xff
#define ENET_INIT_PARAM_MAGIC_RES_INIT4         0x00
#define ENET_INIT_PARAM_MAGIC_RES_INIT5         0x0400

struct ucc_geth_82xx_enet_address {
	u8 res1[0x2];
	u16 h;			
	u16 m;			
	u16 l;			
} __packed;

struct ucc_geth_82xx_address_filtering_pram {
	u32 iaddr_h;		
	u32 iaddr_l;		
	u32 gaddr_h;		
	u32 gaddr_l;		
	struct ucc_geth_82xx_enet_address __iomem taddr;
	struct ucc_geth_82xx_enet_address __iomem paddr[NUM_OF_PADDRS];
	u8 res0[0x40 - 0x38];
} __packed;

struct ucc_geth_tx_firmware_statistics {
	u32 sicoltx;		
	u32 mulcoltx;		
	u32 latecoltxfr;	
	u32 frabortduecol;	
	u32 frlostinmactxer;	
	u32 carriersenseertx;	
	u32 frtxok;		
	u32 txfrexcessivedefer;	
	u32 txpkts256;		
	u32 txpkts512;		
	u32 txpkts1024;		
	u32 txpktsjumbo;	
} __packed;

struct ucc_geth_rx_firmware_statistics {
	u32 frrxfcser;		
	u32 fraligner;		
	u32 inrangelenrxer;	
	u32 outrangelenrxer;	
	u32 frtoolong;		
	u32 runt;		
	u32 verylongevent;	
	u32 symbolerror;	
	u32 dropbsy;		
	u8 res0[0x8];
	u32 mismatchdrop;	
	u32 underpkts;		
	u32 pkts256;		
	u32 pkts512;		
	u32 pkts1024;		
	u32 pktsjumbo;		
	u32 frlossinmacer;	
	u32 pausefr;		
	u8 res1[0x4];
	u32 removevlan;		
	u32 replacevlan;	
	u32 insertvlan;		
} __packed;

struct ucc_geth_hardware_statistics {
	u32 tx64;		
	u32 tx127;		
	u32 tx255;		
	u32 rx64;		
	u32 rx127;		
	u32 rx255;		
	u32 txok;		
	u16 txcf;		
	u32 tmca;		
	u32 tbca;		
	u32 rxfok;		
	u32 rxbok;		
	u32 rbyt;		
	u32 rmca;		
	u32 rbca;		
} __packed;

#define TX_ERRORS_DEF      0x0200
#define TX_ERRORS_EXDEF    0x0100
#define TX_ERRORS_LC       0x0080
#define TX_ERRORS_RL       0x0040
#define TX_ERRORS_RC_MASK  0x003C
#define TX_ERRORS_RC_SHIFT 2
#define TX_ERRORS_UN       0x0002
#define TX_ERRORS_CSL      0x0001

#define RX_ERRORS_CMR      0x0200
#define RX_ERRORS_M        0x0100
#define RX_ERRORS_BC       0x0080
#define RX_ERRORS_MC       0x0040

#define T_VID      0x003c0000	
#define T_DEF      (((u32) TX_ERRORS_DEF     ) << 16)
#define T_EXDEF    (((u32) TX_ERRORS_EXDEF   ) << 16)
#define T_LC       (((u32) TX_ERRORS_LC      ) << 16)
#define T_RL       (((u32) TX_ERRORS_RL      ) << 16)
#define T_RC_MASK  (((u32) TX_ERRORS_RC_MASK ) << 16)
#define T_UN       (((u32) TX_ERRORS_UN      ) << 16)
#define T_CSL      (((u32) TX_ERRORS_CSL     ) << 16)
#define T_ERRORS_REPORT  (T_DEF | T_EXDEF | T_LC | T_RL | T_RC_MASK \
		| T_UN | T_CSL)	

#define R_LG    0x00200000	
#define R_NO    0x00100000	
#define R_SH    0x00080000	
#define R_CR    0x00040000	
#define R_OV    0x00020000	
#define R_IPCH  0x00010000	
#define R_CMR   (((u32) RX_ERRORS_CMR  ) << 16)
#define R_M     (((u32) RX_ERRORS_M    ) << 16)
#define R_BC    (((u32) RX_ERRORS_BC   ) << 16)
#define R_MC    (((u32) RX_ERRORS_MC   ) << 16)
#define R_ERRORS_REPORT (R_CMR | R_M | R_BC | R_MC)	
#define R_ERRORS_FATAL  (R_LG  | R_NO | R_SH | R_CR | \
		R_OV | R_IPCH)	

#define UCC_GETH_RX_GLOBAL_PRAM_ALIGNMENT	256
#define UCC_GETH_TX_GLOBAL_PRAM_ALIGNMENT       128
#define UCC_GETH_THREAD_RX_PRAM_ALIGNMENT       128
#define UCC_GETH_THREAD_TX_PRAM_ALIGNMENT       64
#define UCC_GETH_THREAD_DATA_ALIGNMENT          256	
#define UCC_GETH_SEND_QUEUE_QUEUE_DESCRIPTOR_ALIGNMENT	32
#define UCC_GETH_SCHEDULER_ALIGNMENT		8	
#define UCC_GETH_TX_STATISTICS_ALIGNMENT	4	
#define UCC_GETH_RX_STATISTICS_ALIGNMENT	4	
#define UCC_GETH_RX_INTERRUPT_COALESCING_ALIGNMENT	64
#define UCC_GETH_RX_BD_QUEUES_ALIGNMENT		8	
#define UCC_GETH_RX_PREFETCHED_BDS_ALIGNMENT	128	
#define UCC_GETH_RX_EXTENDED_FILTERING_GLOBAL_PARAMETERS_ALIGNMENT 8	
#define UCC_GETH_RX_BD_RING_ALIGNMENT		32
#define UCC_GETH_TX_BD_RING_ALIGNMENT		32
#define UCC_GETH_MRBLR_ALIGNMENT		128
#define UCC_GETH_RX_BD_RING_SIZE_ALIGNMENT	4
#define UCC_GETH_TX_BD_RING_SIZE_MEMORY_ALIGNMENT	32
#define UCC_GETH_RX_DATA_BUF_ALIGNMENT		64

#define UCC_GETH_TAD_EF                         0x80
#define UCC_GETH_TAD_V                          0x40
#define UCC_GETH_TAD_REJ                        0x20
#define UCC_GETH_TAD_VTAG_OP_RIGHT_SHIFT        2
#define UCC_GETH_TAD_VTAG_OP_SHIFT              6
#define UCC_GETH_TAD_V_NON_VTAG_OP              0x20
#define UCC_GETH_TAD_RQOS_SHIFT                 0
#define UCC_GETH_TAD_V_PRIORITY_SHIFT           5
#define UCC_GETH_TAD_CFI                        0x10

#define UCC_GETH_VLAN_PRIORITY_MAX              8
#define UCC_GETH_IP_PRIORITY_MAX                64
#define UCC_GETH_TX_VTAG_TABLE_ENTRY_MAX        8
#define UCC_GETH_RX_BD_RING_SIZE_MIN            8
#define UCC_GETH_TX_BD_RING_SIZE_MIN            2
#define UCC_GETH_BD_RING_SIZE_MAX		0xffff

#define UCC_GETH_SIZE_OF_BD                     QE_SIZEOF_BD

#define TX_BD_RING_LEN                          0x10
#define RX_BD_RING_LEN                          0x20

#define TX_RING_MOD_MASK(size)                  (size-1)
#define RX_RING_MOD_MASK(size)                  (size-1)

#define ENET_GROUP_ADDR                         0x01	

#define TX_TIMEOUT                              (1*HZ)
#define SKB_ALLOC_TIMEOUT                       100000
#define PHY_INIT_TIMEOUT                        100000
#define PHY_CHANGE_TIME                         2

#define UCC_GETH_URFS_INIT                      512	
#define UCC_GETH_URFET_INIT                     256	
#define UCC_GETH_URFSET_INIT                    384	
#define UCC_GETH_UTFS_INIT                      512	
#define UCC_GETH_UTFET_INIT                     256	
#define UCC_GETH_UTFTT_INIT                     256	
#define UCC_GETH_URFS_GIGA_INIT                 4096	
#define UCC_GETH_URFET_GIGA_INIT                2048	
#define UCC_GETH_URFSET_GIGA_INIT               3072	
#define UCC_GETH_UTFS_GIGA_INIT                 4096	
#define UCC_GETH_UTFET_GIGA_INIT                2048	
#define UCC_GETH_UTFTT_GIGA_INIT                4096	

#define UCC_GETH_REMODER_INIT                   0	
#define UCC_GETH_TEMODER_INIT                   0xC000	

#define UCC_GETH_UPSMR_INIT                     UCC_GETH_UPSMR_RES1

#define UCC_GETH_MACCFG1_INIT                   0
#define UCC_GETH_MACCFG2_INIT                   (MACCFG2_RESERVED_1)

enum enet_addr_type {
	ENET_ADDR_TYPE_INDIVIDUAL,
	ENET_ADDR_TYPE_GROUP,
	ENET_ADDR_TYPE_BROADCAST
};

enum ucc_geth_enet_address_recognition_location {
	UCC_GETH_ENET_ADDRESS_RECOGNITION_LOCATION_STATION_ADDRESS,
	UCC_GETH_ENET_ADDRESS_RECOGNITION_LOCATION_PADDR_FIRST,	
	UCC_GETH_ENET_ADDRESS_RECOGNITION_LOCATION_PADDR2,	
	UCC_GETH_ENET_ADDRESS_RECOGNITION_LOCATION_PADDR3,	
	UCC_GETH_ENET_ADDRESS_RECOGNITION_LOCATION_PADDR_LAST,	
	UCC_GETH_ENET_ADDRESS_RECOGNITION_LOCATION_GROUP_HASH,	
	UCC_GETH_ENET_ADDRESS_RECOGNITION_LOCATION_INDIVIDUAL_HASH 
};

enum ucc_geth_vlan_operation_tagged {
	UCC_GETH_VLAN_OPERATION_TAGGED_NOP = 0x0,	
	UCC_GETH_VLAN_OPERATION_TAGGED_REPLACE_VID_PORTION_OF_Q_TAG
		= 0x1,	
	UCC_GETH_VLAN_OPERATION_TAGGED_IF_VID0_REPLACE_VID_WITH_DEFAULT_VALUE
		= 0x2,	
	UCC_GETH_VLAN_OPERATION_TAGGED_EXTRACT_Q_TAG_FROM_FRAME
		= 0x3	
};

enum ucc_geth_vlan_operation_non_tagged {
	UCC_GETH_VLAN_OPERATION_NON_TAGGED_NOP = 0x0,	
	UCC_GETH_VLAN_OPERATION_NON_TAGGED_Q_TAG_INSERT = 0x1	
};

enum ucc_geth_qos_mode {
	UCC_GETH_QOS_MODE_DEFAULT = 0x0,	
	UCC_GETH_QOS_MODE_QUEUE_NUM_FROM_L2_CRITERIA = 0x1,	
	UCC_GETH_QOS_MODE_QUEUE_NUM_FROM_L3_CRITERIA = 0x2	
};

enum ucc_geth_statistics_gathering_mode {
	UCC_GETH_STATISTICS_GATHERING_MODE_NONE = 0x00000000,	
	UCC_GETH_STATISTICS_GATHERING_MODE_HARDWARE = 0x00000001,
	UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_TX = 0x00000004,
	UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_RX = 0x00000008
};

enum ucc_geth_maccfg2_pad_and_crc_mode {
	UCC_GETH_PAD_AND_CRC_MODE_NONE
		= MACCFG2_PAD_AND_CRC_MODE_NONE,	
	UCC_GETH_PAD_AND_CRC_MODE_CRC_ONLY
		= MACCFG2_PAD_AND_CRC_MODE_CRC_ONLY,	
	UCC_GETH_PAD_AND_CRC_MODE_PAD_AND_CRC =
	    MACCFG2_PAD_AND_CRC_MODE_PAD_AND_CRC
};

enum ucc_geth_flow_control_mode {
	UPSMR_AUTOMATIC_FLOW_CONTROL_MODE_NONE = 0x00000000,	
	UPSMR_AUTOMATIC_FLOW_CONTROL_MODE_PAUSE_WHEN_EMERGENCY
		= 0x00004000	
};

enum ucc_geth_num_of_threads {
	UCC_GETH_NUM_OF_THREADS_1 = 0x1,	
	UCC_GETH_NUM_OF_THREADS_2 = 0x2,	
	UCC_GETH_NUM_OF_THREADS_4 = 0x0,	
	UCC_GETH_NUM_OF_THREADS_6 = 0x3,	
	UCC_GETH_NUM_OF_THREADS_8 = 0x4	
};

enum ucc_geth_num_of_station_addresses {
	UCC_GETH_NUM_OF_STATION_ADDRESSES_1,	
	UCC_GETH_NUM_OF_STATION_ADDRESSES_5	
};

struct enet_addr_container {
	u8 address[ETH_ALEN];	
	enum ucc_geth_enet_address_recognition_location location;	
	struct list_head node;
};

#define ENET_ADDR_CONT_ENTRY(ptr) list_entry(ptr, struct enet_addr_container, node)

struct ucc_geth_tad_params {
	int rx_non_dynamic_extended_features_mode;
	int reject_frame;
	enum ucc_geth_vlan_operation_tagged vtag_op;
	enum ucc_geth_vlan_operation_non_tagged vnontag_op;
	enum ucc_geth_qos_mode rqos;
	u8 vpri;
	u16 vid;
};

struct ucc_geth_info {
	struct ucc_fast_info uf_info;
	u8 numQueuesTx;
	u8 numQueuesRx;
	int ipCheckSumCheck;
	int ipCheckSumGenerate;
	int rxExtendedFiltering;
	u32 extendedFilteringChainPointer;
	u16 typeorlen;
	int dynamicMaxFrameLength;
	int dynamicMinFrameLength;
	u8 nonBackToBackIfgPart1;
	u8 nonBackToBackIfgPart2;
	u8 miminumInterFrameGapEnforcement;
	u8 backToBackInterFrameGap;
	int ipAddressAlignment;
	int lengthCheckRx;
	u32 mblinterval;
	u16 nortsrbytetime;
	u8 fracsiz;
	u8 strictpriorityq;
	u8 txasap;
	u8 extrabw;
	int miiPreambleSupress;
	u8 altBebTruncation;
	int altBeb;
	int backPressureNoBackoff;
	int noBackoff;
	int excessDefer;
	u8 maxRetransmission;
	u8 collisionWindow;
	int pro;
	int cap;
	int rsh;
	int rlpb;
	int cam;
	int bro;
	int ecm;
	int receiveFlowControl;
	int transmitFlowControl;
	u8 maxGroupAddrInHash;
	u8 maxIndAddrInHash;
	u8 prel;
	u16 maxFrameLength;
	u16 minFrameLength;
	u16 maxD1Length;
	u16 maxD2Length;
	u16 vlantype;
	u16 vlantci;
	u32 ecamptr;
	u32 eventRegMask;
	u16 pausePeriod;
	u16 extensionField;
	struct device_node *phy_node;
	struct device_node *tbi_node;
	u8 weightfactor[NUM_TX_QUEUES];
	u8 interruptcoalescingmaxvalue[NUM_RX_QUEUES];
	u8 l2qt[UCC_GETH_VLAN_PRIORITY_MAX];
	u8 l3qt[UCC_GETH_IP_PRIORITY_MAX];
	u32 vtagtable[UCC_GETH_TX_VTAG_TABLE_ENTRY_MAX];
	u8 iphoffset[TX_IP_OFFSET_ENTRY_MAX];
	u16 bdRingLenTx[NUM_TX_QUEUES];
	u16 bdRingLenRx[NUM_RX_QUEUES];
	enum ucc_geth_num_of_station_addresses numStationAddresses;
	enum qe_fltr_largest_external_tbl_lookup_key_size
	    largestexternallookupkeysize;
	enum ucc_geth_statistics_gathering_mode statisticsMode;
	enum ucc_geth_vlan_operation_tagged vlanOperationTagged;
	enum ucc_geth_vlan_operation_non_tagged vlanOperationNonTagged;
	enum ucc_geth_qos_mode rxQoSMode;
	enum ucc_geth_flow_control_mode aufc;
	enum ucc_geth_maccfg2_pad_and_crc_mode padAndCrc;
	enum ucc_geth_num_of_threads numThreadsTx;
	enum ucc_geth_num_of_threads numThreadsRx;
	unsigned int riscTx;
	unsigned int riscRx;
};

struct ucc_geth_private {
	struct ucc_geth_info *ug_info;
	struct ucc_fast_private *uccf;
	struct device *dev;
	struct net_device *ndev;
	struct napi_struct napi;
	struct work_struct timeout_work;
	struct ucc_geth __iomem *ug_regs;
	struct ucc_geth_init_pram *p_init_enet_param_shadow;
	struct ucc_geth_exf_global_pram __iomem *p_exf_glbl_param;
	u32 exf_glbl_param_offset;
	struct ucc_geth_rx_global_pram __iomem *p_rx_glbl_pram;
	u32 rx_glbl_pram_offset;
	struct ucc_geth_tx_global_pram __iomem *p_tx_glbl_pram;
	u32 tx_glbl_pram_offset;
	struct ucc_geth_send_queue_mem_region __iomem *p_send_q_mem_reg;
	u32 send_q_mem_reg_offset;
	struct ucc_geth_thread_data_tx __iomem *p_thread_data_tx;
	u32 thread_dat_tx_offset;
	struct ucc_geth_thread_data_rx __iomem *p_thread_data_rx;
	u32 thread_dat_rx_offset;
	struct ucc_geth_scheduler __iomem *p_scheduler;
	u32 scheduler_offset;
	struct ucc_geth_tx_firmware_statistics_pram __iomem *p_tx_fw_statistics_pram;
	u32 tx_fw_statistics_pram_offset;
	struct ucc_geth_rx_firmware_statistics_pram __iomem *p_rx_fw_statistics_pram;
	u32 rx_fw_statistics_pram_offset;
	struct ucc_geth_rx_interrupt_coalescing_table __iomem *p_rx_irq_coalescing_tbl;
	u32 rx_irq_coalescing_tbl_offset;
	struct ucc_geth_rx_bd_queues_entry __iomem *p_rx_bd_qs_tbl;
	u32 rx_bd_qs_tbl_offset;
	u8 __iomem *p_tx_bd_ring[NUM_TX_QUEUES];
	u32 tx_bd_ring_offset[NUM_TX_QUEUES];
	u8 __iomem *p_rx_bd_ring[NUM_RX_QUEUES];
	u32 rx_bd_ring_offset[NUM_RX_QUEUES];
	u8 __iomem *confBd[NUM_TX_QUEUES];
	u8 __iomem *txBd[NUM_TX_QUEUES];
	u8 __iomem *rxBd[NUM_RX_QUEUES];
	int badFrame[NUM_RX_QUEUES];
	u16 cpucount[NUM_TX_QUEUES];
	u16 __iomem *p_cpucount[NUM_TX_QUEUES];
	int indAddrRegUsed[NUM_OF_PADDRS];
	u8 paddr[NUM_OF_PADDRS][ETH_ALEN];	
	u8 numGroupAddrInHash;
	u8 numIndAddrInHash;
	u8 numIndAddrInReg;
	int rx_extended_features;
	int rx_non_dynamic_extended_features;
	struct list_head conf_skbs;
	struct list_head group_hash_q;
	struct list_head ind_hash_q;
	u32 saved_uccm;
	spinlock_t lock;
	
	struct sk_buff **tx_skbuff[NUM_TX_QUEUES];
	struct sk_buff **rx_skbuff[NUM_RX_QUEUES];
	
	u16 skb_curtx[NUM_TX_QUEUES];
	u16 skb_currx[NUM_RX_QUEUES];
	
	u16 skb_dirtytx[NUM_TX_QUEUES];

	struct sk_buff_head rx_recycle;

	struct ugeth_mii_info *mii_info;
	struct phy_device *phydev;
	phy_interface_t phy_interface;
	int max_speed;
	uint32_t msg_enable;
	int oldspeed;
	int oldduplex;
	int oldlink;
	int wol_en;

	struct device_node *node;
};

void uec_set_ethtool_ops(struct net_device *netdev);
int init_flow_control_params(u32 automatic_flow_control_mode,
		int rx_flow_control_enable, int tx_flow_control_enable,
		u16 pause_period, u16 extension_field,
		u32 __iomem *upsmr_register, u32 __iomem *uempr_register,
		u32 __iomem *maccfg1_register);


#endif				
