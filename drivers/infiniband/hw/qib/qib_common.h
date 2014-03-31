/*
 * Copyright (c) 2006, 2007, 2008, 2009, 2010 QLogic Corporation.
 * All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _QIB_COMMON_H
#define _QIB_COMMON_H


#define QIB_SRC_OUI_1 0x00
#define QIB_SRC_OUI_2 0x11
#define QIB_SRC_OUI_3 0x75

#define IPS_PROTO_VERSION 2


#define QIB_KD_QP 0x656b78

#define QIB_STATUS_INITTED       0x1    
#define QIB_STATUS_CHIP_PRESENT 0x20
#define QIB_STATUS_IB_READY     0x40
#define QIB_STATUS_IB_CONF      0x80
#define QIB_STATUS_HWERROR     0x200

enum qib_ureg {
	
	ur_rcvhdrtail = 0,
	
	ur_rcvhdrhead = 1,
	
	ur_rcvegrindextail = 2,
	
	ur_rcvegrindexhead = 3,
	
	_QIB_UregMax
};

#define QIB_RUNTIME_PCIE                0x0002
#define QIB_RUNTIME_FORCE_WC_ORDER      0x0004
#define QIB_RUNTIME_RCVHDR_COPY         0x0008
#define QIB_RUNTIME_MASTER              0x0010
#define QIB_RUNTIME_RCHK                0x0020
#define QIB_RUNTIME_NODMA_RTAIL         0x0080
#define QIB_RUNTIME_SPECIAL_TRIGGER     0x0100
#define QIB_RUNTIME_SDMA                0x0200
#define QIB_RUNTIME_FORCE_PIOAVAIL      0x0400
#define QIB_RUNTIME_PIO_REGSWAPPED      0x0800
#define QIB_RUNTIME_CTXT_MSB_IN_QP      0x1000
#define QIB_RUNTIME_CTXT_REDIRECT       0x2000
#define QIB_RUNTIME_HDRSUPP             0x4000

struct qib_base_info {
	
	__u32 spi_hw_version;
	
	__u32 spi_sw_version;
	
	__u16 spi_ctxt;
	__u16 spi_subctxt;
	__u32 spi_mtu;
	__u32 spi_piosize;
	
	__u32 spi_tidcnt;
	
	__u32 spi_tidegrcnt;
	
	__u32 spi_rcvhdrent_size;
	__u32 spi_rcvhdr_cnt;

	
	__u32 spi_runtime_flags;

	
	__u64 spi_rcvhdr_base;

	

	
	__u64 spi_rcv_egrbufs;

	

	__u32 spi_rcv_egrbufsize;
	__u32 spi_qpair;

	__u64 spi_uregbase;
	/*
	 * Maximum buffer size in bytes that can be used in a single TID
	 * entry (assuming the buffer is aligned to this boundary).  This is
	 * the minimum of what the hardware and software support Guaranteed
	 * to be a power of 2.
	 */
	__u32 spi_tid_maxsize;
	__u32 spi_pioalign;
	__u32 spi_pioindex;
	 
	__u32 spi_piocnt;

	__u64 spi_piobufbase;

	__u64 spi_pioavailaddr;

	__u64 spi_status;

	
	__u32 spi_nctxts;
	__u16 spi_unit; 
	__u16 spi_port; 
	
	__u32 spi_rcv_egrperchunk;
	
	__u32 spi_rcv_egrchunksize;
	
	__u32 spi_rcv_egrbuftotlen;
	__u32 spi_rhf_offset; 
	
	__u64 spi_rcvhdr_tailaddr;

	__u64 spi_subctxt_uregbase;
	__u64 spi_subctxt_rcvegrbuf;
	__u64 spi_subctxt_rcvhdr_base;

	
	__u64 spi_sendbuf_status;
} __attribute__ ((aligned(8)));

#define QIB_USER_SWMAJOR 1

#define QIB_USER_SWMINOR 11

#define QIB_USER_SWVERSION ((QIB_USER_SWMAJOR << 16) | QIB_USER_SWMINOR)

#ifndef QIB_KERN_TYPE
#define QIB_KERN_TYPE 0
#define QIB_IDSTR "QLogic kernel.org driver"
#endif

#define QIB_KERN_SWVERSION ((QIB_KERN_TYPE << 31) | QIB_USER_SWVERSION)

#define QIB_PORT_ALG_ACROSS 0 
#define QIB_PORT_ALG_WITHIN 1 
#define QIB_PORT_ALG_COUNT 2 

struct qib_user_info {
	__u32 spu_userversion;

	__u32 _spu_unused2;

	
	__u32 spu_base_info_size;

	__u32 spu_port_alg; 

	__u16 spu_subctxt_cnt;
	__u16 spu_subctxt_id;

	__u32 spu_port; 

	__u64 spu_base_info;

} __attribute__ ((aligned(8)));


#define QIB_CMD_CTXT_INFO       17      
#define QIB_CMD_RECV_CTRL       18      
#define QIB_CMD_TID_UPDATE      19      
#define QIB_CMD_TID_FREE        20      
#define QIB_CMD_SET_PART_KEY    21      
#define QIB_CMD_ASSIGN_CTXT     23      
#define QIB_CMD_USER_INIT       24      
#define QIB_CMD_UNUSED_1        25
#define QIB_CMD_UNUSED_2        26
#define QIB_CMD_PIOAVAILUPD     27      
#define QIB_CMD_POLL_TYPE       28      
#define QIB_CMD_ARMLAUNCH_CTRL  29      
#define QIB_CMD_SDMA_INFLIGHT   31      
#define QIB_CMD_SDMA_COMPLETE   32      
#define QIB_CMD_DISARM_BUFS     34      
#define QIB_CMD_ACK_EVENT       35      
#define QIB_CMD_CPUS_LIST       36      

#define _QIB_EVENT_DISARM_BUFS_BIT	0
#define _QIB_EVENT_LINKDOWN_BIT		1
#define _QIB_EVENT_LID_CHANGE_BIT	2
#define _QIB_EVENT_LMC_CHANGE_BIT	3
#define _QIB_EVENT_SL2VL_CHANGE_BIT	4
#define _QIB_MAX_EVENT_BIT _QIB_EVENT_SL2VL_CHANGE_BIT

#define QIB_EVENT_DISARM_BUFS_BIT	(1UL << _QIB_EVENT_DISARM_BUFS_BIT)
#define QIB_EVENT_LINKDOWN_BIT		(1UL << _QIB_EVENT_LINKDOWN_BIT)
#define QIB_EVENT_LID_CHANGE_BIT	(1UL << _QIB_EVENT_LID_CHANGE_BIT)
#define QIB_EVENT_LMC_CHANGE_BIT	(1UL << _QIB_EVENT_LMC_CHANGE_BIT)
#define QIB_EVENT_SL2VL_CHANGE_BIT	(1UL << _QIB_EVENT_SL2VL_CHANGE_BIT)


#define QIB_POLL_TYPE_ANYRCV     0x0
#define QIB_POLL_TYPE_URGENT     0x1

struct qib_ctxt_info {
	__u16 num_active;       
	__u16 unit;             
	__u16 port;             
	__u16 ctxt;             
	__u16 subctxt;          
	__u16 num_ctxts;        
	__u16 num_subctxts;     
	__u16 rec_cpu;          
};

struct qib_tid_info {
	__u32 tidcnt;
	
	__u32 tid__unused;
	
	__u64 tidvaddr;
	
	__u64 tidlist;

	__u64 tidmap;
};

struct qib_cmd {
	__u32 type;                     
	union {
		struct qib_tid_info tid_info;
		struct qib_user_info user_info;

		__u64 sdma_inflight;
		__u64 sdma_complete;
		__u64 ctxt_info;
		
		__u32 recv_ctrl;
		
		__u32 armlaunch_ctrl;
		
		__u16 part_key;
		
		__u64 slave_mask_addr;
		
		__u16 poll_type;
		
		__u8 ctxt_bp;
		
		__u64 event_mask;
	} cmd;
};

struct qib_iovec {
	
	__u64 iov_base;

	__u64 iov_len;
};

struct __qib_sendpkt {
	__u32 sps_flags;        
	__u32 sps_cnt;          
	
	struct qib_iovec sps_iov[4];
};

#define _DIAG_XPKT_VERS 3
struct qib_diag_xpkt {
	__u16 version;
	__u16 unit;
	__u16 port;
	__u16 len;
	__u64 data;
	__u64 pbc_wd;
};

#define QIB_FLASH_VERSION 2
struct qib_flash {
	
	__u8 if_fversion;
	
	__u8 if_csum;
	__u8 if_length;
	
	__u8 if_guid[8];
	
	__u8 if_numguid;
	
	char if_serial[12];
	
	char if_mfgdate[8];
	
	char if_testdate[8];
	
	__u8 if_errcntp[4];
	
	__u8 if_powerhour[2];
	
	char if_comment[32];
	
	char if_sprefix[4];
	
	__u8 if_future[46];
};

struct qlogic_ib_counters {
	__u64 LBIntCnt;
	__u64 LBFlowStallCnt;
	__u64 TxSDmaDescCnt;    
	__u64 TxUnsupVLErrCnt;
	__u64 TxDataPktCnt;
	__u64 TxFlowPktCnt;
	__u64 TxDwordCnt;
	__u64 TxLenErrCnt;
	__u64 TxMaxMinLenErrCnt;
	__u64 TxUnderrunCnt;
	__u64 TxFlowStallCnt;
	__u64 TxDroppedPktCnt;
	__u64 RxDroppedPktCnt;
	__u64 RxDataPktCnt;
	__u64 RxFlowPktCnt;
	__u64 RxDwordCnt;
	__u64 RxLenErrCnt;
	__u64 RxMaxMinLenErrCnt;
	__u64 RxICRCErrCnt;
	__u64 RxVCRCErrCnt;
	__u64 RxFlowCtrlErrCnt;
	__u64 RxBadFormatCnt;
	__u64 RxLinkProblemCnt;
	__u64 RxEBPCnt;
	__u64 RxLPCRCErrCnt;
	__u64 RxBufOvflCnt;
	__u64 RxTIDFullErrCnt;
	__u64 RxTIDValidErrCnt;
	__u64 RxPKeyMismatchCnt;
	__u64 RxP0HdrEgrOvflCnt;
	__u64 RxP1HdrEgrOvflCnt;
	__u64 RxP2HdrEgrOvflCnt;
	__u64 RxP3HdrEgrOvflCnt;
	__u64 RxP4HdrEgrOvflCnt;
	__u64 RxP5HdrEgrOvflCnt;
	__u64 RxP6HdrEgrOvflCnt;
	__u64 RxP7HdrEgrOvflCnt;
	__u64 RxP8HdrEgrOvflCnt;
	__u64 RxP9HdrEgrOvflCnt;
	__u64 RxP10HdrEgrOvflCnt;
	__u64 RxP11HdrEgrOvflCnt;
	__u64 RxP12HdrEgrOvflCnt;
	__u64 RxP13HdrEgrOvflCnt;
	__u64 RxP14HdrEgrOvflCnt;
	__u64 RxP15HdrEgrOvflCnt;
	__u64 RxP16HdrEgrOvflCnt;
	__u64 IBStatusChangeCnt;
	__u64 IBLinkErrRecoveryCnt;
	__u64 IBLinkDownedCnt;
	__u64 IBSymbolErrCnt;
	__u64 RxVL15DroppedPktCnt;
	__u64 RxOtherLocalPhyErrCnt;
	__u64 PcieRetryBufDiagQwordCnt;
	__u64 ExcessBufferOvflCnt;
	__u64 LocalLinkIntegrityErrCnt;
	__u64 RxVlErrCnt;
	__u64 RxDlidFltrCnt;
};


#define QLOGIC_IB_RHF_LENGTH_MASK 0x7FF
#define QLOGIC_IB_RHF_LENGTH_SHIFT 0
#define QLOGIC_IB_RHF_RCVTYPE_MASK 0x7
#define QLOGIC_IB_RHF_RCVTYPE_SHIFT 11
#define QLOGIC_IB_RHF_EGRINDEX_MASK 0xFFF
#define QLOGIC_IB_RHF_EGRINDEX_SHIFT 16
#define QLOGIC_IB_RHF_SEQ_MASK 0xF
#define QLOGIC_IB_RHF_SEQ_SHIFT 0
#define QLOGIC_IB_RHF_HDRQ_OFFSET_MASK 0x7FF
#define QLOGIC_IB_RHF_HDRQ_OFFSET_SHIFT 4
#define QLOGIC_IB_RHF_H_ICRCERR   0x80000000
#define QLOGIC_IB_RHF_H_VCRCERR   0x40000000
#define QLOGIC_IB_RHF_H_PARITYERR 0x20000000
#define QLOGIC_IB_RHF_H_LENERR    0x10000000
#define QLOGIC_IB_RHF_H_MTUERR    0x08000000
#define QLOGIC_IB_RHF_H_IHDRERR   0x04000000
#define QLOGIC_IB_RHF_H_TIDERR    0x02000000
#define QLOGIC_IB_RHF_H_MKERR     0x01000000
#define QLOGIC_IB_RHF_H_IBERR     0x00800000
#define QLOGIC_IB_RHF_H_ERR_MASK  0xFF800000
#define QLOGIC_IB_RHF_L_USE_EGR   0x80000000
#define QLOGIC_IB_RHF_L_SWA       0x00008000
#define QLOGIC_IB_RHF_L_SWB       0x00004000

#define QLOGIC_IB_I_VERS_MASK 0xF
#define QLOGIC_IB_I_VERS_SHIFT 28
#define QLOGIC_IB_I_CTXT_MASK 0xF
#define QLOGIC_IB_I_CTXT_SHIFT 24
#define QLOGIC_IB_I_TID_MASK 0x7FF
#define QLOGIC_IB_I_TID_SHIFT 13
#define QLOGIC_IB_I_OFFSET_MASK 0x1FFF
#define QLOGIC_IB_I_OFFSET_SHIFT 0

#define QLOGIC_IB_KPF_INTR 0x1
#define QLOGIC_IB_KPF_SUBCTXT_MASK 0x3
#define QLOGIC_IB_KPF_SUBCTXT_SHIFT 1

#define QLOGIC_IB_MAX_SUBCTXT   4

#define QLOGIC_IB_SP_TEST    0x40
#define QLOGIC_IB_SP_TESTEBP 0x20
#define QLOGIC_IB_SP_TRIGGER_SHIFT  15

#define QLOGIC_IB_SENDPIOAVAIL_BUSY_SHIFT 1
#define QLOGIC_IB_SENDPIOAVAIL_CHECK_SHIFT 0

struct qib_header {
	__le32 ver_ctxt_tid_offset;
	__le16 chksum;
	__le16 pkt_flags;
};

struct qib_message_header {
	__be16 lrh[4];
	__be32 bth[3];
	
	struct qib_header iph;
	__u8 sub_opcode;
};

#define QIB_LRH_GRH 0x0003      
#define QIB_LRH_BTH 0x0002      

#define SIZE_OF_CRC 1

#define QIB_DEFAULT_P_KEY 0xFFFF
#define QIB_PERMISSIVE_LID 0xFFFF
#define QIB_AETH_CREDIT_SHIFT 24
#define QIB_AETH_CREDIT_MASK 0x1F
#define QIB_AETH_CREDIT_INVAL 0x1F
#define QIB_PSN_MASK 0xFFFFFF
#define QIB_MSN_MASK 0xFFFFFF
#define QIB_QPN_MASK 0xFFFFFF
#define QIB_MULTICAST_LID_BASE 0xC000
#define QIB_EAGER_TID_ID QLOGIC_IB_I_TID_MASK
#define QIB_MULTICAST_QPN 0xFFFFFF

#define RCVHQ_RCV_TYPE_EXPECTED  0
#define RCVHQ_RCV_TYPE_EAGER     1
#define RCVHQ_RCV_TYPE_NON_KD    2
#define RCVHQ_RCV_TYPE_ERROR     3

#define QIB_HEADER_QUEUE_WORDS 9

static inline __u32 qib_hdrget_err_flags(const __le32 *rbuf)
{
	return __le32_to_cpu(rbuf[1]) & QLOGIC_IB_RHF_H_ERR_MASK;
}

static inline __u32 qib_hdrget_rcv_type(const __le32 *rbuf)
{
	return (__le32_to_cpu(rbuf[0]) >> QLOGIC_IB_RHF_RCVTYPE_SHIFT) &
		QLOGIC_IB_RHF_RCVTYPE_MASK;
}

static inline __u32 qib_hdrget_length_in_bytes(const __le32 *rbuf)
{
	return ((__le32_to_cpu(rbuf[0]) >> QLOGIC_IB_RHF_LENGTH_SHIFT) &
		QLOGIC_IB_RHF_LENGTH_MASK) << 2;
}

static inline __u32 qib_hdrget_index(const __le32 *rbuf)
{
	return (__le32_to_cpu(rbuf[0]) >> QLOGIC_IB_RHF_EGRINDEX_SHIFT) &
		QLOGIC_IB_RHF_EGRINDEX_MASK;
}

static inline __u32 qib_hdrget_seq(const __le32 *rbuf)
{
	return (__le32_to_cpu(rbuf[1]) >> QLOGIC_IB_RHF_SEQ_SHIFT) &
		QLOGIC_IB_RHF_SEQ_MASK;
}

static inline __u32 qib_hdrget_offset(const __le32 *rbuf)
{
	return (__le32_to_cpu(rbuf[1]) >> QLOGIC_IB_RHF_HDRQ_OFFSET_SHIFT) &
		QLOGIC_IB_RHF_HDRQ_OFFSET_MASK;
}

static inline __u32 qib_hdrget_use_egr_buf(const __le32 *rbuf)
{
	return __le32_to_cpu(rbuf[0]) & QLOGIC_IB_RHF_L_USE_EGR;
}

static inline __u32 qib_hdrget_qib_ver(__le32 hdrword)
{
	return (__le32_to_cpu(hdrword) >> QLOGIC_IB_I_VERS_SHIFT) &
		QLOGIC_IB_I_VERS_MASK;
}

#endif                          
