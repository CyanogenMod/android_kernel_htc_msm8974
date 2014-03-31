/*
 * Copyright (c) 2008, 2009, 2010 QLogic Corporation. All rights reserved.
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


#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_smi.h>

#include "qib.h"
#include "qib_7322_regs.h"
#include "qib_qsfp.h"

#include "qib_mad.h"

static void qib_setup_7322_setextled(struct qib_pportdata *, u32);
static void qib_7322_handle_hwerrors(struct qib_devdata *, char *, size_t);
static void sendctrl_7322_mod(struct qib_pportdata *ppd, u32 op);
static irqreturn_t qib_7322intr(int irq, void *data);
static irqreturn_t qib_7322bufavail(int irq, void *data);
static irqreturn_t sdma_intr(int irq, void *data);
static irqreturn_t sdma_idle_intr(int irq, void *data);
static irqreturn_t sdma_progress_intr(int irq, void *data);
static irqreturn_t sdma_cleanup_intr(int irq, void *data);
static void qib_7322_txchk_change(struct qib_devdata *, u32, u32, u32,
				  struct qib_ctxtdata *rcd);
static u8 qib_7322_phys_portstate(u64);
static u32 qib_7322_iblink_state(u64);
static void qib_set_ib_7322_lstate(struct qib_pportdata *ppd, u16 linkcmd,
				   u16 linitcmd);
static void force_h1(struct qib_pportdata *);
static void adj_tx_serdes(struct qib_pportdata *);
static u32 qib_7322_setpbc_control(struct qib_pportdata *, u32, u8, u8);
static void qib_7322_mini_pcs_reset(struct qib_pportdata *);

static u32 ahb_mod(struct qib_devdata *, int, int, int, u32, u32);
static void ibsd_wr_allchans(struct qib_pportdata *, int, unsigned, unsigned);
static void serdes_7322_los_enable(struct qib_pportdata *, int);
static int serdes_7322_init_old(struct qib_pportdata *);
static int serdes_7322_init_new(struct qib_pportdata *);

#define BMASK(msb, lsb) (((1 << ((msb) + 1 - (lsb))) - 1) << (lsb))

#define LE2_DEFAULT 5
#define LE2_5m 4
#define LE2_QME 0

#define IBSD(hw_pidx) (hw_pidx + 2)

static const unsigned rcv_int_timeout = 375;
static const unsigned rcv_int_count = 16;
static const unsigned sdma_idle_cnt = 64;

#define RXEQ_DISABLE_MSECS 2500

ushort qib_num_cfg_vls = 2;
module_param_named(num_vls, qib_num_cfg_vls, ushort, S_IRUGO);
MODULE_PARM_DESC(num_vls, "Set number of Virtual Lanes to use (1-8)");

static ushort qib_chase = 1;
module_param_named(chase, qib_chase, ushort, S_IRUGO);
MODULE_PARM_DESC(chase, "Enable state chase handling");

static ushort qib_long_atten = 10; 
module_param_named(long_attenuation, qib_long_atten, ushort, S_IRUGO);
MODULE_PARM_DESC(long_attenuation, \
		 "attenuation cutoff (dB) for long copper cable setup");

static ushort qib_singleport;
module_param_named(singleport, qib_singleport, ushort, S_IRUGO);
MODULE_PARM_DESC(singleport, "Use only IB port 1; more per-port buffer space");

static ushort qib_krcvq01_no_msi;
module_param_named(krcvq01_no_msi, qib_krcvq01_no_msi, ushort, S_IRUGO);
MODULE_PARM_DESC(krcvq01_no_msi, "No MSI for kctx < 2");

static unsigned qib_rcvhdrcnt;
module_param_named(rcvhdrcnt, qib_rcvhdrcnt, uint, S_IRUGO);
MODULE_PARM_DESC(rcvhdrcnt, "receive header count");

static unsigned qib_rcvhdrsize;
module_param_named(rcvhdrsize, qib_rcvhdrsize, uint, S_IRUGO);
MODULE_PARM_DESC(rcvhdrsize, "receive header size in 32-bit words");

static unsigned qib_rcvhdrentsize;
module_param_named(rcvhdrentsize, qib_rcvhdrentsize, uint, S_IRUGO);
MODULE_PARM_DESC(rcvhdrentsize, "receive header entry size in 32-bit words");

#define MAX_ATTEN_LEN 64 
static char txselect_list[MAX_ATTEN_LEN] = "10";
static struct kparam_string kp_txselect = {
	.string = txselect_list,
	.maxlen = MAX_ATTEN_LEN
};
static int  setup_txselect(const char *, struct kernel_param *);
module_param_call(txselect, setup_txselect, param_get_string,
		  &kp_txselect, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(txselect, \
		 "Tx serdes indices (for no QSFP or invalid QSFP data)");

#define BOARD_QME7342 5
#define BOARD_QMH7342 6
#define IS_QMH(dd) (SYM_FIELD((dd)->revision, Revision, BoardID) == \
		    BOARD_QMH7342)
#define IS_QME(dd) (SYM_FIELD((dd)->revision, Revision, BoardID) == \
		    BOARD_QME7342)

#define KREG_IDX(regname)     (QIB_7322_##regname##_OFFS / sizeof(u64))

#define KREG_IBPORT_IDX(regname) ((QIB_7322_##regname##_0_OFFS / sizeof(u64)))

#define MASK_ACROSS(lsb, msb) \
	(((1ULL << ((msb) + 1 - (lsb))) - 1) << (lsb))

#define SYM_RMASK(regname, fldname) ((u64)              \
	QIB_7322_##regname##_##fldname##_RMASK)

#define SYM_MASK(regname, fldname) ((u64)               \
	QIB_7322_##regname##_##fldname##_RMASK <<       \
	 QIB_7322_##regname##_##fldname##_LSB)

#define SYM_FIELD(value, regname, fldname) ((u64)	\
	(((value) >> SYM_LSB(regname, fldname)) &	\
	 SYM_RMASK(regname, fldname)))

#define SYM_FIELD_ACROSS(value, regname, fldname, nbits) \
	(((value) >> SYM_LSB(regname, fldname)) & MASK_ACROSS(0, nbits))

#define HWE_MASK(fldname) SYM_MASK(HwErrMask, fldname##Mask)
#define ERR_MASK(fldname) SYM_MASK(ErrMask, fldname##Mask)
#define ERR_MASK_N(fldname) SYM_MASK(ErrMask_0, fldname##Mask)
#define INT_MASK(fldname) SYM_MASK(IntMask, fldname##IntMask)
#define INT_MASK_P(fldname, port) SYM_MASK(IntMask, fldname##IntMask##_##port)
#define INT_MASK_PM(fldname, port) SYM_MASK(IntMask, fldname##Mask##_##port)


#define SYM_LSB(regname, fldname) (QIB_7322_##regname##_##fldname##_LSB)

#define IBA7322_TID_SZ_SHIFT QIB_7322_RcvTIDArray0_RT_BufSize_LSB
#define IBA7322_TID_SZ_2K (1UL<<IBA7322_TID_SZ_SHIFT) 
#define IBA7322_TID_SZ_4K (2UL<<IBA7322_TID_SZ_SHIFT) 
#define IBA7322_TID_PA_SHIFT 11U 

#define SendIBSLIDAssignMask \
	QIB_7322_SendIBSLIDAssign_0_SendIBSLIDAssign_15_0_RMASK
#define SendIBSLMCMask \
	QIB_7322_SendIBSLIDMask_0_SendIBSLIDMask_15_0_RMASK

#define ExtLED_IB1_YEL SYM_MASK(EXTCtrl, LEDPort0YellowOn)
#define ExtLED_IB1_GRN SYM_MASK(EXTCtrl, LEDPort0GreenOn)
#define ExtLED_IB2_YEL SYM_MASK(EXTCtrl, LEDPort1YellowOn)
#define ExtLED_IB2_GRN SYM_MASK(EXTCtrl, LEDPort1GreenOn)
#define ExtLED_IB1_MASK (ExtLED_IB1_YEL | ExtLED_IB1_GRN)
#define ExtLED_IB2_MASK (ExtLED_IB2_YEL | ExtLED_IB2_GRN)

#define _QIB_GPIO_SDA_NUM 1
#define _QIB_GPIO_SCL_NUM 0
#define QIB_EEPROM_WEN_NUM 14
#define QIB_TWSI_EEPROM_DEV 0xA2 

#define QIB_7322_PSXMITWAIT_CHECK_RATE 4000

#define PORT_SPD_CAP (QIB_IB_SDR | QIB_IB_DDR | QIB_IB_QDR)
#define PORT_SPD_CAP_SHIFT 3

#define DUAL_PORT_CAP (PORT_SPD_CAP | (PORT_SPD_CAP << PORT_SPD_CAP_SHIFT))


#define kr_contextcnt KREG_IDX(ContextCnt)
#define kr_control KREG_IDX(Control)
#define kr_counterregbase KREG_IDX(CntrRegBase)
#define kr_errclear KREG_IDX(ErrClear)
#define kr_errmask KREG_IDX(ErrMask)
#define kr_errstatus KREG_IDX(ErrStatus)
#define kr_extctrl KREG_IDX(EXTCtrl)
#define kr_extstatus KREG_IDX(EXTStatus)
#define kr_gpio_clear KREG_IDX(GPIOClear)
#define kr_gpio_mask KREG_IDX(GPIOMask)
#define kr_gpio_out KREG_IDX(GPIOOut)
#define kr_gpio_status KREG_IDX(GPIOStatus)
#define kr_hwdiagctrl KREG_IDX(HwDiagCtrl)
#define kr_debugportval KREG_IDX(DebugPortValueReg)
#define kr_fmask KREG_IDX(feature_mask)
#define kr_act_fmask KREG_IDX(active_feature_mask)
#define kr_hwerrclear KREG_IDX(HwErrClear)
#define kr_hwerrmask KREG_IDX(HwErrMask)
#define kr_hwerrstatus KREG_IDX(HwErrStatus)
#define kr_intclear KREG_IDX(IntClear)
#define kr_intmask KREG_IDX(IntMask)
#define kr_intredirect KREG_IDX(IntRedirect0)
#define kr_intstatus KREG_IDX(IntStatus)
#define kr_pagealign KREG_IDX(PageAlign)
#define kr_rcvavailtimeout KREG_IDX(RcvAvailTimeOut0)
#define kr_rcvctrl KREG_IDX(RcvCtrl) 
#define kr_rcvegrbase KREG_IDX(RcvEgrBase)
#define kr_rcvegrcnt KREG_IDX(RcvEgrCnt)
#define kr_rcvhdrcnt KREG_IDX(RcvHdrCnt)
#define kr_rcvhdrentsize KREG_IDX(RcvHdrEntSize)
#define kr_rcvhdrsize KREG_IDX(RcvHdrSize)
#define kr_rcvtidbase KREG_IDX(RcvTIDBase)
#define kr_rcvtidcnt KREG_IDX(RcvTIDCnt)
#define kr_revision KREG_IDX(Revision)
#define kr_scratch KREG_IDX(Scratch)
#define kr_sendbuffererror KREG_IDX(SendBufErr0) 
#define kr_sendcheckmask KREG_IDX(SendCheckMask0) 
#define kr_sendctrl KREG_IDX(SendCtrl)
#define kr_sendgrhcheckmask KREG_IDX(SendGRHCheckMask0) 
#define kr_sendibpktmask KREG_IDX(SendIBPacketMask0) 
#define kr_sendpioavailaddr KREG_IDX(SendBufAvailAddr)
#define kr_sendpiobufbase KREG_IDX(SendBufBase)
#define kr_sendpiobufcnt KREG_IDX(SendBufCnt)
#define kr_sendpiosize KREG_IDX(SendBufSize)
#define kr_sendregbase KREG_IDX(SendRegBase)
#define kr_sendbufavail0 KREG_IDX(SendBufAvail0)
#define kr_userregbase KREG_IDX(UserRegBase)
#define kr_intgranted KREG_IDX(Int_Granted)
#define kr_vecclr_wo_int KREG_IDX(vec_clr_without_int)
#define kr_intblocked KREG_IDX(IntBlocked)
#define kr_r_access KREG_IDX(SPC_JTAG_ACCESS_REG)

#define krp_errclear KREG_IBPORT_IDX(ErrClear)
#define krp_errmask KREG_IBPORT_IDX(ErrMask)
#define krp_errstatus KREG_IBPORT_IDX(ErrStatus)
#define krp_highprio_0 KREG_IBPORT_IDX(HighPriority0)
#define krp_highprio_limit KREG_IBPORT_IDX(HighPriorityLimit)
#define krp_hrtbt_guid KREG_IBPORT_IDX(HRTBT_GUID)
#define krp_ib_pcsconfig KREG_IBPORT_IDX(IBPCSConfig)
#define krp_ibcctrl_a KREG_IBPORT_IDX(IBCCtrlA)
#define krp_ibcctrl_b KREG_IBPORT_IDX(IBCCtrlB)
#define krp_ibcctrl_c KREG_IBPORT_IDX(IBCCtrlC)
#define krp_ibcstatus_a KREG_IBPORT_IDX(IBCStatusA)
#define krp_ibcstatus_b KREG_IBPORT_IDX(IBCStatusB)
#define krp_txestatus KREG_IBPORT_IDX(TXEStatus)
#define krp_lowprio_0 KREG_IBPORT_IDX(LowPriority0)
#define krp_ncmodectrl KREG_IBPORT_IDX(IBNCModeCtrl)
#define krp_partitionkey KREG_IBPORT_IDX(RcvPartitionKey)
#define krp_psinterval KREG_IBPORT_IDX(PSInterval)
#define krp_psstart KREG_IBPORT_IDX(PSStart)
#define krp_psstat KREG_IBPORT_IDX(PSStat)
#define krp_rcvbthqp KREG_IBPORT_IDX(RcvBTHQP)
#define krp_rcvctrl KREG_IBPORT_IDX(RcvCtrl)
#define krp_rcvpktledcnt KREG_IBPORT_IDX(RcvPktLEDCnt)
#define krp_rcvqpmaptable KREG_IBPORT_IDX(RcvQPMapTableA)
#define krp_rxcreditvl0 KREG_IBPORT_IDX(RxCreditVL0)
#define krp_rxcreditvl15 (KREG_IBPORT_IDX(RxCreditVL0)+15)
#define krp_sendcheckcontrol KREG_IBPORT_IDX(SendCheckControl)
#define krp_sendctrl KREG_IBPORT_IDX(SendCtrl)
#define krp_senddmabase KREG_IBPORT_IDX(SendDmaBase)
#define krp_senddmabufmask0 KREG_IBPORT_IDX(SendDmaBufMask0)
#define krp_senddmabufmask1 (KREG_IBPORT_IDX(SendDmaBufMask0) + 1)
#define krp_senddmabufmask2 (KREG_IBPORT_IDX(SendDmaBufMask0) + 2)
#define krp_senddmabuf_use0 KREG_IBPORT_IDX(SendDmaBufUsed0)
#define krp_senddmabuf_use1 (KREG_IBPORT_IDX(SendDmaBufUsed0) + 1)
#define krp_senddmabuf_use2 (KREG_IBPORT_IDX(SendDmaBufUsed0) + 2)
#define krp_senddmadesccnt KREG_IBPORT_IDX(SendDmaDescCnt)
#define krp_senddmahead KREG_IBPORT_IDX(SendDmaHead)
#define krp_senddmaheadaddr KREG_IBPORT_IDX(SendDmaHeadAddr)
#define krp_senddmaidlecnt KREG_IBPORT_IDX(SendDmaIdleCnt)
#define krp_senddmalengen KREG_IBPORT_IDX(SendDmaLenGen)
#define krp_senddmaprioritythld KREG_IBPORT_IDX(SendDmaPriorityThld)
#define krp_senddmareloadcnt KREG_IBPORT_IDX(SendDmaReloadCnt)
#define krp_senddmastatus KREG_IBPORT_IDX(SendDmaStatus)
#define krp_senddmatail KREG_IBPORT_IDX(SendDmaTail)
#define krp_sendhdrsymptom KREG_IBPORT_IDX(SendHdrErrSymptom)
#define krp_sendslid KREG_IBPORT_IDX(SendIBSLIDAssign)
#define krp_sendslidmask KREG_IBPORT_IDX(SendIBSLIDMask)
#define krp_ibsdtestiftx KREG_IBPORT_IDX(IB_SDTEST_IF_TX)
#define krp_adapt_dis_timer KREG_IBPORT_IDX(ADAPT_DISABLE_TIMER_THRESHOLD)
#define krp_tx_deemph_override KREG_IBPORT_IDX(IBSD_TX_DEEMPHASIS_OVERRIDE)
#define krp_serdesctrl KREG_IBPORT_IDX(IBSerdesCtrl)

#define krc_rcvhdraddr KREG_IDX(RcvHdrAddr0)
#define krc_rcvhdrtailaddr KREG_IDX(RcvHdrTailAddr0)

#define NUM_TIDFLOWS_CTXT 0x20 
#define ur_rcvflowtable (KREG_IDX(RcvTIDFlowTable0) - KREG_IDX(RcvHdrTail0))

#define TIDFLOW_ERRBITS  ( \
	(SYM_MASK(RcvTIDFlowTable0, GenMismatch) << \
	SYM_LSB(RcvTIDFlowTable0, GenMismatch)) | \
	(SYM_MASK(RcvTIDFlowTable0, SeqMismatch) << \
	SYM_LSB(RcvTIDFlowTable0, SeqMismatch)))

#define CREG_IDX(regname) \
((QIB_7322_##regname##_0_OFFS - QIB_7322_LBIntCnt_OFFS) / sizeof(u64))

#define crp_badformat CREG_IDX(RxVersionErrCnt)
#define crp_err_rlen CREG_IDX(RxLenErrCnt)
#define crp_erricrc CREG_IDX(RxICRCErrCnt)
#define crp_errlink CREG_IDX(RxLinkMalformCnt)
#define crp_errlpcrc CREG_IDX(RxLPCRCErrCnt)
#define crp_errpkey CREG_IDX(RxPKeyMismatchCnt)
#define crp_errvcrc CREG_IDX(RxVCRCErrCnt)
#define crp_excessbufferovfl CREG_IDX(ExcessBufferOvflCnt)
#define crp_iblinkdown CREG_IDX(IBLinkDownedCnt)
#define crp_iblinkerrrecov CREG_IDX(IBLinkErrRecoveryCnt)
#define crp_ibstatuschange CREG_IDX(IBStatusChangeCnt)
#define crp_ibsymbolerr CREG_IDX(IBSymbolErrCnt)
#define crp_invalidrlen CREG_IDX(RxMaxMinLenErrCnt)
#define crp_locallinkintegrityerr CREG_IDX(LocalLinkIntegrityErrCnt)
#define crp_pktrcv CREG_IDX(RxDataPktCnt)
#define crp_pktrcvflowctrl CREG_IDX(RxFlowPktCnt)
#define crp_pktsend CREG_IDX(TxDataPktCnt)
#define crp_pktsendflow CREG_IDX(TxFlowPktCnt)
#define crp_psrcvdatacount CREG_IDX(PSRcvDataCount)
#define crp_psrcvpktscount CREG_IDX(PSRcvPktsCount)
#define crp_psxmitdatacount CREG_IDX(PSXmitDataCount)
#define crp_psxmitpktscount CREG_IDX(PSXmitPktsCount)
#define crp_psxmitwaitcount CREG_IDX(PSXmitWaitCount)
#define crp_rcvebp CREG_IDX(RxEBPCnt)
#define crp_rcvflowctrlviol CREG_IDX(RxFlowCtrlViolCnt)
#define crp_rcvovfl CREG_IDX(RxBufOvflCnt)
#define crp_rxdlidfltr CREG_IDX(RxDlidFltrCnt)
#define crp_rxdroppkt CREG_IDX(RxDroppedPktCnt)
#define crp_rxotherlocalphyerr CREG_IDX(RxOtherLocalPhyErrCnt)
#define crp_rxqpinvalidctxt CREG_IDX(RxQPInvalidContextCnt)
#define crp_rxvlerr CREG_IDX(RxVlErrCnt)
#define crp_sendstall CREG_IDX(TxFlowStallCnt)
#define crp_txdroppedpkt CREG_IDX(TxDroppedPktCnt)
#define crp_txhdrerr CREG_IDX(TxHeadersErrCnt)
#define crp_txlenerr CREG_IDX(TxLenErrCnt)
#define crp_txminmaxlenerr CREG_IDX(TxMaxMinLenErrCnt)
#define crp_txsdmadesc CREG_IDX(TxSDmaDescCnt)
#define crp_txunderrun CREG_IDX(TxUnderrunCnt)
#define crp_txunsupvl CREG_IDX(TxUnsupVLErrCnt)
#define crp_vl15droppedpkt CREG_IDX(RxVL15DroppedPktCnt)
#define crp_wordrcv CREG_IDX(RxDwordCnt)
#define crp_wordsend CREG_IDX(TxDwordCnt)
#define crp_tx_creditstalls CREG_IDX(TxCreditUpToDateTimeOut)

#define CREG_DEVIDX(regname) ((QIB_7322_##regname##_OFFS - \
			QIB_7322_LBIntCnt_OFFS) / sizeof(u64))
#define cr_base_egrovfl CREG_DEVIDX(RxP0HdrEgrOvflCnt)
#define cr_lbint CREG_DEVIDX(LBIntCnt)
#define cr_lbstall CREG_DEVIDX(LBFlowStallCnt)
#define cr_pcieretrydiag CREG_DEVIDX(PcieRetryBufDiagQwordCnt)
#define cr_rxtidflowdrop CREG_DEVIDX(RxTidFlowDropCnt)
#define cr_tidfull CREG_DEVIDX(RxTIDFullErrCnt)
#define cr_tidinvalid CREG_DEVIDX(RxTIDValidErrCnt)

#define NUM_IB_PORTS 2

#define NUM_VL15_BUFS NUM_IB_PORTS

#define KCTXT0_EGRCNT 2048

#define PBC_PORT_SEL_LSB 26
#define PBC_PORT_SEL_RMASK 1
#define PBC_VL_NUM_LSB 27
#define PBC_VL_NUM_RMASK 7
#define PBC_7322_VL15_SEND (1ULL << 63) 
#define PBC_7322_VL15_SEND_CTRL (1ULL << 31) 

static u8 ib_rate_to_delay[IB_RATE_120_GBPS + 1] = {
	[IB_RATE_2_5_GBPS] = 16,
	[IB_RATE_5_GBPS] = 8,
	[IB_RATE_10_GBPS] = 4,
	[IB_RATE_20_GBPS] = 2,
	[IB_RATE_30_GBPS] = 2,
	[IB_RATE_40_GBPS] = 1
};

#define IBA7322_LINKSPEED_SHIFT SYM_LSB(IBCStatusA_0, LinkSpeedActive)
#define IBA7322_LINKWIDTH_SHIFT SYM_LSB(IBCStatusA_0, LinkWidthActive)

#define IB_7322_LT_STATE_DISABLED        0x00
#define IB_7322_LT_STATE_LINKUP          0x01
#define IB_7322_LT_STATE_POLLACTIVE      0x02
#define IB_7322_LT_STATE_POLLQUIET       0x03
#define IB_7322_LT_STATE_SLEEPDELAY      0x04
#define IB_7322_LT_STATE_SLEEPQUIET      0x05
#define IB_7322_LT_STATE_CFGDEBOUNCE     0x08
#define IB_7322_LT_STATE_CFGRCVFCFG      0x09
#define IB_7322_LT_STATE_CFGWAITRMT      0x0a
#define IB_7322_LT_STATE_CFGIDLE         0x0b
#define IB_7322_LT_STATE_RECOVERRETRAIN  0x0c
#define IB_7322_LT_STATE_TXREVLANES      0x0d
#define IB_7322_LT_STATE_RECOVERWAITRMT  0x0e
#define IB_7322_LT_STATE_RECOVERIDLE     0x0f
#define IB_7322_LT_STATE_CFGENH          0x10
#define IB_7322_LT_STATE_CFGTEST         0x11
#define IB_7322_LT_STATE_CFGWAITRMTTEST  0x12
#define IB_7322_LT_STATE_CFGWAITENH      0x13

#define IB_7322_L_STATE_DOWN             0x0
#define IB_7322_L_STATE_INIT             0x1
#define IB_7322_L_STATE_ARM              0x2
#define IB_7322_L_STATE_ACTIVE           0x3
#define IB_7322_L_STATE_ACT_DEFER        0x4

static const u8 qib_7322_physportstate[0x20] = {
	[IB_7322_LT_STATE_DISABLED] = IB_PHYSPORTSTATE_DISABLED,
	[IB_7322_LT_STATE_LINKUP] = IB_PHYSPORTSTATE_LINKUP,
	[IB_7322_LT_STATE_POLLACTIVE] = IB_PHYSPORTSTATE_POLL,
	[IB_7322_LT_STATE_POLLQUIET] = IB_PHYSPORTSTATE_POLL,
	[IB_7322_LT_STATE_SLEEPDELAY] = IB_PHYSPORTSTATE_SLEEP,
	[IB_7322_LT_STATE_SLEEPQUIET] = IB_PHYSPORTSTATE_SLEEP,
	[IB_7322_LT_STATE_CFGDEBOUNCE] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_7322_LT_STATE_CFGRCVFCFG] =
		IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_7322_LT_STATE_CFGWAITRMT] =
		IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_7322_LT_STATE_CFGIDLE] = IB_PHYSPORTSTATE_CFG_IDLE,
	[IB_7322_LT_STATE_RECOVERRETRAIN] =
		IB_PHYSPORTSTATE_LINK_ERR_RECOVER,
	[IB_7322_LT_STATE_RECOVERWAITRMT] =
		IB_PHYSPORTSTATE_LINK_ERR_RECOVER,
	[IB_7322_LT_STATE_RECOVERIDLE] =
		IB_PHYSPORTSTATE_LINK_ERR_RECOVER,
	[IB_7322_LT_STATE_CFGENH] = IB_PHYSPORTSTATE_CFG_ENH,
	[IB_7322_LT_STATE_CFGTEST] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_7322_LT_STATE_CFGWAITRMTTEST] =
		IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_7322_LT_STATE_CFGWAITENH] =
		IB_PHYSPORTSTATE_CFG_WAIT_ENH,
	[0x14] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[0x15] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[0x16] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[0x17] = IB_PHYSPORTSTATE_CFG_TRAIN
};

struct qib_chip_specific {
	u64 __iomem *cregbase;
	u64 *cntrs;
	spinlock_t rcvmod_lock; 
	spinlock_t gpio_lock; 
	u64 main_int_mask;      
	u64 int_enable_mask;  
	u64 errormask;
	u64 hwerrmask;
	u64 gpio_out; 
	u64 gpio_mask; 
	u64 extctrl; 
	u32 ncntrs;
	u32 nportcntrs;
	u32 cntrnamelen;
	u32 portcntrnamelen;
	u32 numctxts;
	u32 rcvegrcnt;
	u32 updthresh; 
	u32 updthresh_dflt; 
	u32 r1;
	int irq;
	u32 num_msix_entries;
	u32 sdmabufcnt;
	u32 lastbuf_for_pio;
	u32 stay_in_freeze;
	u32 recovery_ports_initted;
	struct qib_msix_entry *msix_entries;
	unsigned long *sendchkenable;
	unsigned long *sendgrhchk;
	unsigned long *sendibchk;
	u32 rcvavail_timeout[18];
	char emsgbuf[128]; 
};

struct txdds_ent {
	u8 amp;
	u8 pre;
	u8 main;
	u8 post;
};

struct vendor_txdds_ent {
	u8 oui[QSFP_VOUI_LEN];
	u8 *partnum;
	struct txdds_ent sdr;
	struct txdds_ent ddr;
	struct txdds_ent qdr;
};

static void write_tx_serdes_param(struct qib_pportdata *, struct txdds_ent *);

#define TXDDS_TABLE_SZ 16 
#define TXDDS_EXTRA_SZ 13 
#define TXDDS_MFG_SZ 2    
#define SERDES_CHANS 4 

#define H1_FORCE_VAL 8
#define H1_FORCE_QME 1 
#define H1_FORCE_QMH 7 

#define krp_static_adapt_dis(spd) (KREG_IBPORT_IDX(ADAPT_DISABLE_STATIC_SDR) \
	+ ((spd) * 2))

#define QDR_DFE_DISABLE_DELAY 4000 
#define QDR_STATIC_ADAPT_DOWN 0xf0f0f0f0ULL 
#define QDR_STATIC_ADAPT_DOWN_R1 0ULL 
#define QDR_STATIC_ADAPT_INIT 0xffffffffffULL 
#define QDR_STATIC_ADAPT_INIT_R1 0xf0ffffffffULL 

struct qib_chippport_specific {
	u64 __iomem *kpregbase;
	u64 __iomem *cpregbase;
	u64 *portcntrs;
	struct qib_pportdata *ppd;
	wait_queue_head_t autoneg_wait;
	struct delayed_work autoneg_work;
	struct delayed_work ipg_work;
	struct timer_list chase_timer;
	u64 ibdeltainprog;
	u64 ibsymdelta;
	u64 ibsymsnap;
	u64 iblnkerrdelta;
	u64 iblnkerrsnap;
	u64 iblnkdownsnap;
	u64 iblnkdowndelta;
	u64 ibmalfdelta;
	u64 ibmalfsnap;
	u64 ibcctrl_a; 
	u64 ibcctrl_b; 
	unsigned long qdr_dfe_time;
	unsigned long chase_end;
	u32 autoneg_tries;
	u32 recovery_init;
	u32 qdr_dfe_on;
	u32 qdr_reforce;
	u8 h1_val;
	u8 no_eep;  
	u8 ipg_tries;
	u8 ibmalfusesnap;
	struct qib_qsfp_data qsfp_data;
	char epmsgbuf[192]; 
};

static struct {
	const char *name;
	irq_handler_t handler;
	int lsb;
	int port; 
} irq_table[] = {
	{ "", qib_7322intr, -1, 0 },
	{ " (buf avail)", qib_7322bufavail,
		SYM_LSB(IntStatus, SendBufAvail), 0 },
	{ " (sdma 0)", sdma_intr,
		SYM_LSB(IntStatus, SDmaInt_0), 1 },
	{ " (sdma 1)", sdma_intr,
		SYM_LSB(IntStatus, SDmaInt_1), 2 },
	{ " (sdmaI 0)", sdma_idle_intr,
		SYM_LSB(IntStatus, SDmaIdleInt_0), 1 },
	{ " (sdmaI 1)", sdma_idle_intr,
		SYM_LSB(IntStatus, SDmaIdleInt_1), 2 },
	{ " (sdmaP 0)", sdma_progress_intr,
		SYM_LSB(IntStatus, SDmaProgressInt_0), 1 },
	{ " (sdmaP 1)", sdma_progress_intr,
		SYM_LSB(IntStatus, SDmaProgressInt_1), 2 },
	{ " (sdmaC 0)", sdma_cleanup_intr,
		SYM_LSB(IntStatus, SDmaCleanupDone_0), 1 },
	{ " (sdmaC 1)", sdma_cleanup_intr,
		SYM_LSB(IntStatus, SDmaCleanupDone_1), 2 },
};

#define QLOGIC_IB_IBCC_LINKINITCMD_DISABLE 1
#define QLOGIC_IB_IBCC_LINKINITCMD_POLL 2
#define QLOGIC_IB_IBCC_LINKINITCMD_SLEEP 3
#define QLOGIC_IB_IBCC_LINKINITCMD_SHIFT 16

#define QLOGIC_IB_IBCC_LINKCMD_DOWN 1           
#define QLOGIC_IB_IBCC_LINKCMD_ARMED 2          
#define QLOGIC_IB_IBCC_LINKCMD_ACTIVE 3 

#define BLOB_7322_IBCHG 0x101

static inline void qib_write_kreg(const struct qib_devdata *dd,
				  const u32 regno, u64 value);
static inline u32 qib_read_kreg32(const struct qib_devdata *, const u32);
static void write_7322_initregs(struct qib_devdata *);
static void write_7322_init_portregs(struct qib_pportdata *);
static void setup_7322_link_recovery(struct qib_pportdata *, u32);
static void check_7322_rxe_status(struct qib_pportdata *);
static u32 __iomem *qib_7322_getsendbuf(struct qib_pportdata *, u64, u32 *);

static inline u32 qib_read_ureg32(const struct qib_devdata *dd,
				  enum qib_ureg regno, int ctxt)
{
	if (!dd->kregbase || !(dd->flags & QIB_PRESENT))
		return 0;
	return readl(regno + (u64 __iomem *)(
		(dd->ureg_align * ctxt) + (dd->userbase ?
		 (char __iomem *)dd->userbase :
		 (char __iomem *)dd->kregbase + dd->uregbase)));
}

static inline u64 qib_read_ureg(const struct qib_devdata *dd,
				enum qib_ureg regno, int ctxt)
{

	if (!dd->kregbase || !(dd->flags & QIB_PRESENT))
		return 0;
	return readq(regno + (u64 __iomem *)(
		(dd->ureg_align * ctxt) + (dd->userbase ?
		 (char __iomem *)dd->userbase :
		 (char __iomem *)dd->kregbase + dd->uregbase)));
}

static inline void qib_write_ureg(const struct qib_devdata *dd,
				  enum qib_ureg regno, u64 value, int ctxt)
{
	u64 __iomem *ubase;
	if (dd->userbase)
		ubase = (u64 __iomem *)
			((char __iomem *) dd->userbase +
			 dd->ureg_align * ctxt);
	else
		ubase = (u64 __iomem *)
			(dd->uregbase +
			 (char __iomem *) dd->kregbase +
			 dd->ureg_align * ctxt);

	if (dd->kregbase && (dd->flags & QIB_PRESENT))
		writeq(value, &ubase[regno]);
}

static inline u32 qib_read_kreg32(const struct qib_devdata *dd,
				  const u32 regno)
{
	if (!dd->kregbase || !(dd->flags & QIB_PRESENT))
		return -1;
	return readl((u32 __iomem *) &dd->kregbase[regno]);
}

static inline u64 qib_read_kreg64(const struct qib_devdata *dd,
				  const u32 regno)
{
	if (!dd->kregbase || !(dd->flags & QIB_PRESENT))
		return -1;
	return readq(&dd->kregbase[regno]);
}

static inline void qib_write_kreg(const struct qib_devdata *dd,
				  const u32 regno, u64 value)
{
	if (dd->kregbase && (dd->flags & QIB_PRESENT))
		writeq(value, &dd->kregbase[regno]);
}

static inline u64 qib_read_kreg_port(const struct qib_pportdata *ppd,
				     const u16 regno)
{
	if (!ppd->cpspec->kpregbase || !(ppd->dd->flags & QIB_PRESENT))
		return 0ULL;
	return readq(&ppd->cpspec->kpregbase[regno]);
}

static inline void qib_write_kreg_port(const struct qib_pportdata *ppd,
				       const u16 regno, u64 value)
{
	if (ppd->cpspec && ppd->dd && ppd->cpspec->kpregbase &&
	    (ppd->dd->flags & QIB_PRESENT))
		writeq(value, &ppd->cpspec->kpregbase[regno]);
}

static inline void qib_write_kreg_ctxt(const struct qib_devdata *dd,
				       const u16 regno, unsigned ctxt,
				       u64 value)
{
	qib_write_kreg(dd, regno + ctxt, value);
}

static inline u64 read_7322_creg(const struct qib_devdata *dd, u16 regno)
{
	if (!dd->cspec->cregbase || !(dd->flags & QIB_PRESENT))
		return 0;
	return readq(&dd->cspec->cregbase[regno]);


}

static inline u32 read_7322_creg32(const struct qib_devdata *dd, u16 regno)
{
	if (!dd->cspec->cregbase || !(dd->flags & QIB_PRESENT))
		return 0;
	return readl(&dd->cspec->cregbase[regno]);


}

static inline void write_7322_creg_port(const struct qib_pportdata *ppd,
					u16 regno, u64 value)
{
	if (ppd->cpspec && ppd->cpspec->cpregbase &&
	    (ppd->dd->flags & QIB_PRESENT))
		writeq(value, &ppd->cpspec->cpregbase[regno]);
}

static inline u64 read_7322_creg_port(const struct qib_pportdata *ppd,
				      u16 regno)
{
	if (!ppd->cpspec || !ppd->cpspec->cpregbase ||
	    !(ppd->dd->flags & QIB_PRESENT))
		return 0;
	return readq(&ppd->cpspec->cpregbase[regno]);
}

static inline u32 read_7322_creg32_port(const struct qib_pportdata *ppd,
					u16 regno)
{
	if (!ppd->cpspec || !ppd->cpspec->cpregbase ||
	    !(ppd->dd->flags & QIB_PRESENT))
		return 0;
	return readl(&ppd->cpspec->cpregbase[regno]);
}

#define QLOGIC_IB_C_RESET SYM_MASK(Control, SyncReset)
#define QLOGIC_IB_C_SDMAFETCHPRIOEN SYM_MASK(Control, SDmaDescFetchPriorityEn)

#define QIB_I_RCVURG_LSB SYM_LSB(IntMask, RcvUrg0IntMask)
#define QIB_I_RCVURG_RMASK MASK_ACROSS(0, 17)
#define QIB_I_RCVURG_MASK (QIB_I_RCVURG_RMASK << QIB_I_RCVURG_LSB)
#define QIB_I_RCVAVAIL_LSB SYM_LSB(IntMask, RcvAvail0IntMask)
#define QIB_I_RCVAVAIL_RMASK MASK_ACROSS(0, 17)
#define QIB_I_RCVAVAIL_MASK (QIB_I_RCVAVAIL_RMASK << QIB_I_RCVAVAIL_LSB)
#define QIB_I_C_ERROR INT_MASK(Err)

#define QIB_I_SPIOSENT (INT_MASK_P(SendDone, 0) | INT_MASK_P(SendDone, 1))
#define QIB_I_SPIOBUFAVAIL INT_MASK(SendBufAvail)
#define QIB_I_GPIO INT_MASK(AssertGPIO)
#define QIB_I_P_SDMAINT(pidx) \
	(INT_MASK_P(SDma, pidx) | INT_MASK_P(SDmaIdle, pidx) | \
	 INT_MASK_P(SDmaProgress, pidx) | \
	 INT_MASK_PM(SDmaCleanupDone, pidx))

#define QIB_I_P_BITSEXTANT(pidx) \
	(INT_MASK_P(Err, pidx) | INT_MASK_P(SendDone, pidx) | \
	INT_MASK_P(SDma, pidx) | INT_MASK_P(SDmaIdle, pidx) | \
	INT_MASK_P(SDmaProgress, pidx) | \
	INT_MASK_PM(SDmaCleanupDone, pidx))

#define QIB_I_C_BITSEXTANT \
	(QIB_I_RCVURG_MASK | QIB_I_RCVAVAIL_MASK | \
	QIB_I_SPIOSENT | \
	QIB_I_C_ERROR | QIB_I_SPIOBUFAVAIL | QIB_I_GPIO)

#define QIB_I_BITSEXTANT (QIB_I_C_BITSEXTANT | \
	QIB_I_P_BITSEXTANT(0) | QIB_I_P_BITSEXTANT(1))

#define QIB_E_P_IBSTATUSCHANGED ERR_MASK_N(IBStatusChanged)
#define QIB_E_P_SHDR ERR_MASK_N(SHeadersErr)
#define QIB_E_P_VL15_BUF_MISUSE ERR_MASK_N(VL15BufMisuseErr)
#define QIB_E_P_SND_BUF_MISUSE ERR_MASK_N(SendBufMisuseErr)
#define QIB_E_P_SUNSUPVL ERR_MASK_N(SendUnsupportedVLErr)
#define QIB_E_P_SUNEXP_PKTNUM ERR_MASK_N(SendUnexpectedPktNumErr)
#define QIB_E_P_SDROP_DATA ERR_MASK_N(SendDroppedDataPktErr)
#define QIB_E_P_SDROP_SMP ERR_MASK_N(SendDroppedSmpPktErr)
#define QIB_E_P_SPKTLEN ERR_MASK_N(SendPktLenErr)
#define QIB_E_P_SUNDERRUN ERR_MASK_N(SendUnderRunErr)
#define QIB_E_P_SMAXPKTLEN ERR_MASK_N(SendMaxPktLenErr)
#define QIB_E_P_SMINPKTLEN ERR_MASK_N(SendMinPktLenErr)
#define QIB_E_P_RIBLOSTLINK ERR_MASK_N(RcvIBLostLinkErr)
#define QIB_E_P_RHDR ERR_MASK_N(RcvHdrErr)
#define QIB_E_P_RHDRLEN ERR_MASK_N(RcvHdrLenErr)
#define QIB_E_P_RBADTID ERR_MASK_N(RcvBadTidErr)
#define QIB_E_P_RBADVERSION ERR_MASK_N(RcvBadVersionErr)
#define QIB_E_P_RIBFLOW ERR_MASK_N(RcvIBFlowErr)
#define QIB_E_P_REBP ERR_MASK_N(RcvEBPErr)
#define QIB_E_P_RUNSUPVL ERR_MASK_N(RcvUnsupportedVLErr)
#define QIB_E_P_RUNEXPCHAR ERR_MASK_N(RcvUnexpectedCharErr)
#define QIB_E_P_RSHORTPKTLEN ERR_MASK_N(RcvShortPktLenErr)
#define QIB_E_P_RLONGPKTLEN ERR_MASK_N(RcvLongPktLenErr)
#define QIB_E_P_RMAXPKTLEN ERR_MASK_N(RcvMaxPktLenErr)
#define QIB_E_P_RMINPKTLEN ERR_MASK_N(RcvMinPktLenErr)
#define QIB_E_P_RICRC ERR_MASK_N(RcvICRCErr)
#define QIB_E_P_RVCRC ERR_MASK_N(RcvVCRCErr)
#define QIB_E_P_RFORMATERR ERR_MASK_N(RcvFormatErr)

#define QIB_E_P_SDMA1STDESC ERR_MASK_N(SDma1stDescErr)
#define QIB_E_P_SDMABASE ERR_MASK_N(SDmaBaseErr)
#define QIB_E_P_SDMADESCADDRMISALIGN ERR_MASK_N(SDmaDescAddrMisalignErr)
#define QIB_E_P_SDMADWEN ERR_MASK_N(SDmaDwEnErr)
#define QIB_E_P_SDMAGENMISMATCH ERR_MASK_N(SDmaGenMismatchErr)
#define QIB_E_P_SDMAHALT ERR_MASK_N(SDmaHaltErr)
#define QIB_E_P_SDMAMISSINGDW ERR_MASK_N(SDmaMissingDwErr)
#define QIB_E_P_SDMAOUTOFBOUND ERR_MASK_N(SDmaOutOfBoundErr)
#define QIB_E_P_SDMARPYTAG ERR_MASK_N(SDmaRpyTagErr)
#define QIB_E_P_SDMATAILOUTOFBOUND ERR_MASK_N(SDmaTailOutOfBoundErr)
#define QIB_E_P_SDMAUNEXPDATA ERR_MASK_N(SDmaUnexpDataErr)

#define QIB_E_RESET ERR_MASK(ResetNegated)
#define QIB_E_HARDWARE ERR_MASK(HardwareErr)
#define QIB_E_INVALIDADDR ERR_MASK(InvalidAddrErr)


#define QIB_E_SBUF_VL15_MISUSE ERR_MASK(SBufVL15MisUseErr)
#define QIB_E_BADEEP ERR_MASK(InvalidEEPCmd)
#define QIB_E_VLMISMATCH ERR_MASK(SendVLMismatchErr)
#define QIB_E_ARMLAUNCH ERR_MASK(SendArmLaunchErr)
#define QIB_E_SPCLTRIG ERR_MASK(SendSpecialTriggerErr)
#define QIB_E_RRCVHDRFULL ERR_MASK(RcvHdrFullErr)
#define QIB_E_RRCVEGRFULL ERR_MASK(RcvEgrFullErr)
#define QIB_E_RCVCTXTSHARE ERR_MASK(RcvContextShareErr)

#define QIB_E_SDMA_VL15 ERR_MASK(SDmaVL15Err)
#define QIB_E_SDMA_WRONG_PORT ERR_MASK(SDmaWrongPortErr)
#define QIB_E_SDMA_BUF_DUP ERR_MASK(SDmaBufMaskDuplicateErr)

#define QIB_E_P_PKTERRS (QIB_E_P_SPKTLEN |\
	QIB_E_P_SDROP_DATA | QIB_E_P_RVCRC |\
	QIB_E_P_RICRC | QIB_E_P_RSHORTPKTLEN |\
	QIB_E_P_VL15_BUF_MISUSE | QIB_E_P_SHDR | \
	QIB_E_P_REBP)

#define QIB_E_P_RPKTERRS (\
	QIB_E_P_RHDRLEN | QIB_E_P_RBADTID | \
	QIB_E_P_RBADVERSION | QIB_E_P_RHDR | \
	QIB_E_P_RLONGPKTLEN | QIB_E_P_RSHORTPKTLEN |\
	QIB_E_P_RMAXPKTLEN | QIB_E_P_RMINPKTLEN | \
	QIB_E_P_RFORMATERR | QIB_E_P_RUNSUPVL | \
	QIB_E_P_RUNEXPCHAR | QIB_E_P_RIBFLOW | QIB_E_P_REBP)

#define QIB_E_P_SPKTERRS (\
	QIB_E_P_SUNEXP_PKTNUM |\
	QIB_E_P_SDROP_DATA | QIB_E_P_SDROP_SMP |\
	QIB_E_P_SMAXPKTLEN |\
	QIB_E_P_VL15_BUF_MISUSE | QIB_E_P_SHDR | \
	QIB_E_P_SMINPKTLEN | QIB_E_P_SPKTLEN | \
	QIB_E_P_SND_BUF_MISUSE | QIB_E_P_SUNSUPVL)

#define QIB_E_SPKTERRS ( \
		QIB_E_SBUF_VL15_MISUSE | QIB_E_VLMISMATCH | \
		ERR_MASK_N(SendUnsupportedVLErr) |			\
		QIB_E_SPCLTRIG | QIB_E_SDMA_VL15 | QIB_E_SDMA_WRONG_PORT)

#define QIB_E_P_SDMAERRS ( \
	QIB_E_P_SDMAHALT | \
	QIB_E_P_SDMADESCADDRMISALIGN | \
	QIB_E_P_SDMAUNEXPDATA | \
	QIB_E_P_SDMAMISSINGDW | \
	QIB_E_P_SDMADWEN | \
	QIB_E_P_SDMARPYTAG | \
	QIB_E_P_SDMA1STDESC | \
	QIB_E_P_SDMABASE | \
	QIB_E_P_SDMATAILOUTOFBOUND | \
	QIB_E_P_SDMAOUTOFBOUND | \
	QIB_E_P_SDMAGENMISMATCH)

#define QIB_E_P_BITSEXTANT ( \
	QIB_E_P_SPKTERRS | QIB_E_P_PKTERRS | QIB_E_P_RPKTERRS | \
	QIB_E_P_RIBLOSTLINK | QIB_E_P_IBSTATUSCHANGED | \
	QIB_E_P_SND_BUF_MISUSE | QIB_E_P_SUNDERRUN | \
	QIB_E_P_SHDR | QIB_E_P_VL15_BUF_MISUSE | QIB_E_P_SDMAERRS \
	)

#define QIB_E_P_LINK_PKTERRS (\
	QIB_E_P_SDROP_DATA | QIB_E_P_SDROP_SMP |\
	QIB_E_P_SMINPKTLEN | QIB_E_P_SPKTLEN |\
	QIB_E_P_RSHORTPKTLEN | QIB_E_P_RMINPKTLEN |\
	QIB_E_P_RUNEXPCHAR)

#define QIB_E_C_BITSEXTANT (\
	QIB_E_HARDWARE | QIB_E_INVALIDADDR | QIB_E_BADEEP |\
	QIB_E_ARMLAUNCH | QIB_E_VLMISMATCH | QIB_E_RRCVHDRFULL |\
	QIB_E_RRCVEGRFULL | QIB_E_RESET | QIB_E_SBUF_VL15_MISUSE)

#define E_SPKT_ERRS_IGNORE 0

#define QIB_EXTS_MEMBIST_DISABLED \
	SYM_MASK(EXTStatus, MemBISTDisabled)
#define QIB_EXTS_MEMBIST_ENDTEST \
	SYM_MASK(EXTStatus, MemBISTEndTest)

#define QIB_E_SPIOARMLAUNCH \
	ERR_MASK(SendArmLaunchErr)

#define IBA7322_IBCC_LINKINITCMD_MASK SYM_RMASK(IBCCtrlA_0, LinkInitCmd)
#define IBA7322_IBCC_LINKCMD_SHIFT SYM_LSB(IBCCtrlA_0, LinkCmd)

#define IBA7322_IBC_IBTA_1_2_MASK SYM_MASK(IBCCtrlB_0, IB_ENHANCED_MODE)
#define IBA7322_IBC_MAX_SPEED_MASK SYM_MASK(IBCCtrlB_0, SD_SPEED)
#define IBA7322_IBC_SPEED_QDR SYM_MASK(IBCCtrlB_0, SD_SPEED_QDR)
#define IBA7322_IBC_SPEED_DDR SYM_MASK(IBCCtrlB_0, SD_SPEED_DDR)
#define IBA7322_IBC_SPEED_SDR SYM_MASK(IBCCtrlB_0, SD_SPEED_SDR)
#define IBA7322_IBC_SPEED_MASK (SYM_MASK(IBCCtrlB_0, SD_SPEED_SDR) | \
	SYM_MASK(IBCCtrlB_0, SD_SPEED_DDR) | SYM_MASK(IBCCtrlB_0, SD_SPEED_QDR))
#define IBA7322_IBC_SPEED_LSB SYM_LSB(IBCCtrlB_0, SD_SPEED_SDR)

#define IBA7322_LEDBLINK_OFF_SHIFT SYM_LSB(RcvPktLEDCnt_0, OFFperiod)
#define IBA7322_LEDBLINK_ON_SHIFT SYM_LSB(RcvPktLEDCnt_0, ONperiod)

#define IBA7322_IBC_WIDTH_AUTONEG SYM_MASK(IBCCtrlB_0, IB_NUM_CHANNELS)
#define IBA7322_IBC_WIDTH_4X_ONLY (1<<SYM_LSB(IBCCtrlB_0, IB_NUM_CHANNELS))
#define IBA7322_IBC_WIDTH_1X_ONLY (0<<SYM_LSB(IBCCtrlB_0, IB_NUM_CHANNELS))

#define IBA7322_IBC_RXPOL_MASK SYM_MASK(IBCCtrlB_0, IB_POLARITY_REV_SUPP)
#define IBA7322_IBC_RXPOL_LSB SYM_LSB(IBCCtrlB_0, IB_POLARITY_REV_SUPP)
#define IBA7322_IBC_HRTBT_MASK (SYM_MASK(IBCCtrlB_0, HRTBT_AUTO) | \
	SYM_MASK(IBCCtrlB_0, HRTBT_ENB))
#define IBA7322_IBC_HRTBT_RMASK (IBA7322_IBC_HRTBT_MASK >> \
	SYM_LSB(IBCCtrlB_0, HRTBT_ENB))
#define IBA7322_IBC_HRTBT_LSB SYM_LSB(IBCCtrlB_0, HRTBT_ENB)

#define IBA7322_REDIRECT_VEC_PER_REG 12

#define IBA7322_SENDCHK_PKEY SYM_MASK(SendCheckControl_0, PKey_En)
#define IBA7322_SENDCHK_BTHQP SYM_MASK(SendCheckControl_0, BTHQP_En)
#define IBA7322_SENDCHK_SLID SYM_MASK(SendCheckControl_0, SLID_En)
#define IBA7322_SENDCHK_RAW_IPV6 SYM_MASK(SendCheckControl_0, RawIPV6_En)
#define IBA7322_SENDCHK_MINSZ SYM_MASK(SendCheckControl_0, PacketTooSmall_En)

#define AUTONEG_TRIES 3 

#define HWE_AUTO(fldname) { .mask = SYM_MASK(HwErrMask, fldname##Mask), \
	.msg = #fldname , .sz = sizeof(#fldname) }
#define HWE_AUTO_P(fldname, port) { .mask = SYM_MASK(HwErrMask, \
	fldname##Mask##_##port), .msg = #fldname , .sz = sizeof(#fldname) }
static const struct qib_hwerror_msgs qib_7322_hwerror_msgs[] = {
	HWE_AUTO_P(IBSerdesPClkNotDetect, 1),
	HWE_AUTO_P(IBSerdesPClkNotDetect, 0),
	HWE_AUTO(PCIESerdesPClkNotDetect),
	HWE_AUTO(PowerOnBISTFailed),
	HWE_AUTO(TempsenseTholdReached),
	HWE_AUTO(MemoryErr),
	HWE_AUTO(PCIeBusParityErr),
	HWE_AUTO(PcieCplTimeout),
	HWE_AUTO(PciePoisonedTLP),
	HWE_AUTO_P(SDmaMemReadErr, 1),
	HWE_AUTO_P(SDmaMemReadErr, 0),
	HWE_AUTO_P(IBCBusFromSPCParityErr, 1),
	HWE_AUTO_P(IBCBusToSPCParityErr, 1),
	HWE_AUTO_P(IBCBusFromSPCParityErr, 0),
	HWE_AUTO(statusValidNoEop),
	HWE_AUTO(LATriggered),
	{ .mask = 0, .sz = 0 }
};

#define E_AUTO(fldname) { .mask = SYM_MASK(ErrMask, fldname##Mask), \
	.msg = #fldname, .sz = sizeof(#fldname) }
#define E_P_AUTO(fldname) { .mask = SYM_MASK(ErrMask_0, fldname##Mask), \
	.msg = #fldname, .sz = sizeof(#fldname) }
static const struct qib_hwerror_msgs qib_7322error_msgs[] = {
	E_AUTO(RcvEgrFullErr),
	E_AUTO(RcvHdrFullErr),
	E_AUTO(ResetNegated),
	E_AUTO(HardwareErr),
	E_AUTO(InvalidAddrErr),
	E_AUTO(SDmaVL15Err),
	E_AUTO(SBufVL15MisUseErr),
	E_AUTO(InvalidEEPCmd),
	E_AUTO(RcvContextShareErr),
	E_AUTO(SendVLMismatchErr),
	E_AUTO(SendArmLaunchErr),
	E_AUTO(SendSpecialTriggerErr),
	E_AUTO(SDmaWrongPortErr),
	E_AUTO(SDmaBufMaskDuplicateErr),
	{ .mask = 0, .sz = 0 }
};

static const struct  qib_hwerror_msgs qib_7322p_error_msgs[] = {
	E_P_AUTO(IBStatusChanged),
	E_P_AUTO(SHeadersErr),
	E_P_AUTO(VL15BufMisuseErr),
	{.mask = SYM_MASK(ErrMask_0, SDmaHaltErrMask), .msg = "SDmaHalted",
		.sz = 11},
	E_P_AUTO(SDmaDescAddrMisalignErr),
	E_P_AUTO(SDmaUnexpDataErr),
	E_P_AUTO(SDmaMissingDwErr),
	E_P_AUTO(SDmaDwEnErr),
	E_P_AUTO(SDmaRpyTagErr),
	E_P_AUTO(SDma1stDescErr),
	E_P_AUTO(SDmaBaseErr),
	E_P_AUTO(SDmaTailOutOfBoundErr),
	E_P_AUTO(SDmaOutOfBoundErr),
	E_P_AUTO(SDmaGenMismatchErr),
	E_P_AUTO(SendBufMisuseErr),
	E_P_AUTO(SendUnsupportedVLErr),
	E_P_AUTO(SendUnexpectedPktNumErr),
	E_P_AUTO(SendDroppedDataPktErr),
	E_P_AUTO(SendDroppedSmpPktErr),
	E_P_AUTO(SendPktLenErr),
	E_P_AUTO(SendUnderRunErr),
	E_P_AUTO(SendMaxPktLenErr),
	E_P_AUTO(SendMinPktLenErr),
	E_P_AUTO(RcvIBLostLinkErr),
	E_P_AUTO(RcvHdrErr),
	E_P_AUTO(RcvHdrLenErr),
	E_P_AUTO(RcvBadTidErr),
	E_P_AUTO(RcvBadVersionErr),
	E_P_AUTO(RcvIBFlowErr),
	E_P_AUTO(RcvEBPErr),
	E_P_AUTO(RcvUnsupportedVLErr),
	E_P_AUTO(RcvUnexpectedCharErr),
	E_P_AUTO(RcvShortPktLenErr),
	E_P_AUTO(RcvLongPktLenErr),
	E_P_AUTO(RcvMaxPktLenErr),
	E_P_AUTO(RcvMinPktLenErr),
	E_P_AUTO(RcvICRCErr),
	E_P_AUTO(RcvVCRCErr),
	E_P_AUTO(RcvFormatErr),
	{ .mask = 0, .sz = 0 }
};

#define INTR_AUTO(fldname) { .mask = SYM_MASK(IntMask, fldname##Mask), \
	.msg = #fldname, .sz = sizeof(#fldname) }
#define INTR_AUTO_P(fldname) { .mask = MASK_ACROSS(\
	SYM_LSB(IntMask, fldname##Mask##_0), \
	SYM_LSB(IntMask, fldname##Mask##_1)), \
	.msg = #fldname "_P", .sz = sizeof(#fldname "_P") }
#define INTR_AUTO_PI(fldname) { .mask = MASK_ACROSS(\
	SYM_LSB(IntMask, fldname##Mask##_1), \
	SYM_LSB(IntMask, fldname##Mask##_0)), \
	.msg = #fldname "_P", .sz = sizeof(#fldname "_P") }
#define INTR_AUTO_C(fldname) { .mask = MASK_ACROSS(\
	SYM_LSB(IntMask, fldname##0IntMask), \
	SYM_LSB(IntMask, fldname##17IntMask)), \
	.msg = #fldname "_C", .sz = sizeof(#fldname "_C") }

static const struct  qib_hwerror_msgs qib_7322_intr_msgs[] = {
	INTR_AUTO_P(SDmaInt),
	INTR_AUTO_P(SDmaProgressInt),
	INTR_AUTO_P(SDmaIdleInt),
	INTR_AUTO_P(SDmaCleanupDone),
	INTR_AUTO_C(RcvUrg),
	INTR_AUTO_P(ErrInt),
	INTR_AUTO(ErrInt),      
	INTR_AUTO(AssertGPIOInt),
	INTR_AUTO_P(SendDoneInt),
	INTR_AUTO(SendBufAvailInt),
	INTR_AUTO_C(RcvAvail),
	{ .mask = 0, .sz = 0 }
};

#define TXSYMPTOM_AUTO_P(fldname) \
	{ .mask = SYM_MASK(SendHdrErrSymptom_0, fldname), \
	.msg = #fldname, .sz = sizeof(#fldname) }
static const struct  qib_hwerror_msgs hdrchk_msgs[] = {
	TXSYMPTOM_AUTO_P(NonKeyPacket),
	TXSYMPTOM_AUTO_P(GRHFail),
	TXSYMPTOM_AUTO_P(PkeyFail),
	TXSYMPTOM_AUTO_P(QPFail),
	TXSYMPTOM_AUTO_P(SLIDFail),
	TXSYMPTOM_AUTO_P(RawIPV6),
	TXSYMPTOM_AUTO_P(PacketTooSmall),
	{ .mask = 0, .sz = 0 }
};

#define IBA7322_HDRHEAD_PKTINT_SHIFT 32 

static void qib_disarm_7322_senderrbufs(struct qib_pportdata *ppd)
{
	struct qib_devdata *dd = ppd->dd;
	u32 i;
	int any;
	u32 piobcnt = dd->piobcnt2k + dd->piobcnt4k + NUM_VL15_BUFS;
	u32 regcnt = (piobcnt + BITS_PER_LONG - 1) / BITS_PER_LONG;
	unsigned long sbuf[4];

	any = 0;
	for (i = 0; i < regcnt; ++i) {
		sbuf[i] = qib_read_kreg64(dd, kr_sendbuffererror + i);
		if (sbuf[i]) {
			any = 1;
			qib_write_kreg(dd, kr_sendbuffererror + i, sbuf[i]);
		}
	}

	if (any)
		qib_disarm_piobufs_set(dd, sbuf, piobcnt);
}


static void err_decode(char *msg, size_t len, u64 errs,
		       const struct qib_hwerror_msgs *msp)
{
	u64 these, lmask;
	int took, multi, n = 0;

	while (errs && msp && msp->mask) {
		multi = (msp->mask & (msp->mask - 1));
		while (errs & msp->mask) {
			these = (errs & msp->mask);
			lmask = (these & (these - 1)) ^ these;
			if (len) {
				if (n++) {
					
					*msg++ = ',';
					len--;
				}
				BUG_ON(!msp->sz);
				
				took = min_t(size_t, msp->sz - (size_t)1, len);
				memcpy(msg,  msp->msg, took);
				len -= took;
				msg += took;
				if (len)
					*msg = '\0';
			}
			errs &= ~lmask;
			if (len && multi) {
				
				int idx = -1;

				while (lmask & msp->mask) {
					++idx;
					lmask >>= 1;
				}
				took = scnprintf(msg, len, "_%d", idx);
				len -= took;
				msg += took;
			}
		}
		++msp;
	}
	
	if (len && errs)
		snprintf(msg, len, "%sMORE:%llX", n ? "," : "",
			(unsigned long long) errs);
}

static void flush_fifo(struct qib_pportdata *ppd)
{
	struct qib_devdata *dd = ppd->dd;
	u32 __iomem *piobuf;
	u32 bufn;
	u32 *hdr;
	u64 pbc;
	const unsigned hdrwords = 7;
	static struct qib_ib_header ibhdr = {
		.lrh[0] = cpu_to_be16(0xF000 | QIB_LRH_BTH),
		.lrh[1] = IB_LID_PERMISSIVE,
		.lrh[2] = cpu_to_be16(hdrwords + SIZE_OF_CRC),
		.lrh[3] = IB_LID_PERMISSIVE,
		.u.oth.bth[0] = cpu_to_be32(
			(IB_OPCODE_UD_SEND_ONLY << 24) | QIB_DEFAULT_P_KEY),
		.u.oth.bth[1] = cpu_to_be32(0),
		.u.oth.bth[2] = cpu_to_be32(0),
		.u.oth.u.ud.deth[0] = cpu_to_be32(0),
		.u.oth.u.ud.deth[1] = cpu_to_be32(0),
	};

	pbc = PBC_7322_VL15_SEND |
		(((u64)ppd->hw_pidx) << (PBC_PORT_SEL_LSB + 32)) |
		(hdrwords + SIZE_OF_CRC);
	piobuf = qib_7322_getsendbuf(ppd, pbc, &bufn);
	if (!piobuf)
		return;
	writeq(pbc, piobuf);
	hdr = (u32 *) &ibhdr;
	if (dd->flags & QIB_PIO_FLUSH_WC) {
		qib_flush_wc();
		qib_pio_copy(piobuf + 2, hdr, hdrwords - 1);
		qib_flush_wc();
		__raw_writel(hdr[hdrwords - 1], piobuf + hdrwords + 1);
		qib_flush_wc();
	} else
		qib_pio_copy(piobuf + 2, hdr, hdrwords);
	qib_sendbuf_done(dd, bufn);
}

static void qib_7322_sdma_sendctrl(struct qib_pportdata *ppd, unsigned op)
{
	struct qib_devdata *dd = ppd->dd;
	u64 set_sendctrl = 0;
	u64 clr_sendctrl = 0;

	if (op & QIB_SDMA_SENDCTRL_OP_ENABLE)
		set_sendctrl |= SYM_MASK(SendCtrl_0, SDmaEnable);
	else
		clr_sendctrl |= SYM_MASK(SendCtrl_0, SDmaEnable);

	if (op & QIB_SDMA_SENDCTRL_OP_INTENABLE)
		set_sendctrl |= SYM_MASK(SendCtrl_0, SDmaIntEnable);
	else
		clr_sendctrl |= SYM_MASK(SendCtrl_0, SDmaIntEnable);

	if (op & QIB_SDMA_SENDCTRL_OP_HALT)
		set_sendctrl |= SYM_MASK(SendCtrl_0, SDmaHalt);
	else
		clr_sendctrl |= SYM_MASK(SendCtrl_0, SDmaHalt);

	if (op & QIB_SDMA_SENDCTRL_OP_DRAIN)
		set_sendctrl |= SYM_MASK(SendCtrl_0, TxeBypassIbc) |
				SYM_MASK(SendCtrl_0, TxeAbortIbc) |
				SYM_MASK(SendCtrl_0, TxeDrainRmFifo);
	else
		clr_sendctrl |= SYM_MASK(SendCtrl_0, TxeBypassIbc) |
				SYM_MASK(SendCtrl_0, TxeAbortIbc) |
				SYM_MASK(SendCtrl_0, TxeDrainRmFifo);

	spin_lock(&dd->sendctrl_lock);

	
	if (op & QIB_SDMA_SENDCTRL_OP_DRAIN) {
		ppd->p_sendctrl &= ~SYM_MASK(SendCtrl_0, SendEnable);
		qib_write_kreg_port(ppd, krp_sendctrl, ppd->p_sendctrl);
		qib_write_kreg(dd, kr_scratch, 0);
	}

	ppd->p_sendctrl |= set_sendctrl;
	ppd->p_sendctrl &= ~clr_sendctrl;

	if (op & QIB_SDMA_SENDCTRL_OP_CLEANUP)
		qib_write_kreg_port(ppd, krp_sendctrl,
				    ppd->p_sendctrl |
				    SYM_MASK(SendCtrl_0, SDmaCleanup));
	else
		qib_write_kreg_port(ppd, krp_sendctrl, ppd->p_sendctrl);
	qib_write_kreg(dd, kr_scratch, 0);

	if (op & QIB_SDMA_SENDCTRL_OP_DRAIN) {
		ppd->p_sendctrl |= SYM_MASK(SendCtrl_0, SendEnable);
		qib_write_kreg_port(ppd, krp_sendctrl, ppd->p_sendctrl);
		qib_write_kreg(dd, kr_scratch, 0);
	}

	spin_unlock(&dd->sendctrl_lock);

	if ((op & QIB_SDMA_SENDCTRL_OP_DRAIN) && ppd->dd->cspec->r1)
		flush_fifo(ppd);
}

static void qib_7322_sdma_hw_clean_up(struct qib_pportdata *ppd)
{
	__qib_sdma_process_event(ppd, qib_sdma_event_e50_hw_cleaned);
}

static void qib_sdma_7322_setlengen(struct qib_pportdata *ppd)
{
	qib_write_kreg_port(ppd, krp_senddmalengen, ppd->sdma_descq_cnt);
	qib_write_kreg_port(ppd, krp_senddmalengen,
			    ppd->sdma_descq_cnt |
			    (1ULL << QIB_7322_SendDmaLenGen_0_Generation_MSB));
}

static void qib_sdma_update_7322_tail(struct qib_pportdata *ppd, u16 tail)
{
	
	wmb();
	ppd->sdma_descq_tail = tail;
	qib_write_kreg_port(ppd, krp_senddmatail, tail);
}

static void qib_7322_sdma_hw_start_up(struct qib_pportdata *ppd)
{
	sendctrl_7322_mod(ppd, QIB_SENDCTRL_FLUSH);

	qib_sdma_7322_setlengen(ppd);
	qib_sdma_update_7322_tail(ppd, 0); 
	ppd->sdma_head_dma[0] = 0;
	qib_7322_sdma_sendctrl(ppd,
		ppd->sdma_state.current_op | QIB_SDMA_SENDCTRL_OP_CLEANUP);
}

#define DISABLES_SDMA ( \
	QIB_E_P_SDMAHALT | \
	QIB_E_P_SDMADESCADDRMISALIGN | \
	QIB_E_P_SDMAMISSINGDW | \
	QIB_E_P_SDMADWEN | \
	QIB_E_P_SDMARPYTAG | \
	QIB_E_P_SDMA1STDESC | \
	QIB_E_P_SDMABASE | \
	QIB_E_P_SDMATAILOUTOFBOUND | \
	QIB_E_P_SDMAOUTOFBOUND | \
	QIB_E_P_SDMAGENMISMATCH)

static void sdma_7322_p_errors(struct qib_pportdata *ppd, u64 errs)
{
	unsigned long flags;
	struct qib_devdata *dd = ppd->dd;

	errs &= QIB_E_P_SDMAERRS;

	if (errs & QIB_E_P_SDMAUNEXPDATA)
		qib_dev_err(dd, "IB%u:%u SDmaUnexpData\n", dd->unit,
			    ppd->port);

	spin_lock_irqsave(&ppd->sdma_lock, flags);

	switch (ppd->sdma_state.current_state) {
	case qib_sdma_state_s00_hw_down:
		break;

	case qib_sdma_state_s10_hw_start_up_wait:
		if (errs & QIB_E_P_SDMAHALT)
			__qib_sdma_process_event(ppd,
				qib_sdma_event_e20_hw_started);
		break;

	case qib_sdma_state_s20_idle:
		break;

	case qib_sdma_state_s30_sw_clean_up_wait:
		break;

	case qib_sdma_state_s40_hw_clean_up_wait:
		if (errs & QIB_E_P_SDMAHALT)
			__qib_sdma_process_event(ppd,
				qib_sdma_event_e50_hw_cleaned);
		break;

	case qib_sdma_state_s50_hw_halt_wait:
		if (errs & QIB_E_P_SDMAHALT)
			__qib_sdma_process_event(ppd,
				qib_sdma_event_e60_hw_halted);
		break;

	case qib_sdma_state_s99_running:
		__qib_sdma_process_event(ppd, qib_sdma_event_e7322_err_halted);
		__qib_sdma_process_event(ppd, qib_sdma_event_e60_hw_halted);
		break;
	}

	spin_unlock_irqrestore(&ppd->sdma_lock, flags);
}

static noinline void handle_7322_errors(struct qib_devdata *dd)
{
	char *msg;
	u64 iserr = 0;
	u64 errs;
	u64 mask;
	int log_idx;

	qib_stats.sps_errints++;
	errs = qib_read_kreg64(dd, kr_errstatus);
	if (!errs) {
		qib_devinfo(dd->pcidev, "device error interrupt, "
			 "but no error bits set!\n");
		goto done;
	}

	
	errs &= dd->cspec->errormask;
	msg = dd->cspec->emsgbuf;

	
	if (errs & QIB_E_HARDWARE) {
		*msg = '\0';
		qib_7322_handle_hwerrors(dd, msg, sizeof dd->cspec->emsgbuf);
	} else
		for (log_idx = 0; log_idx < QIB_EEP_LOG_CNT; ++log_idx)
			if (errs & dd->eep_st_masks[log_idx].errs_to_log)
				qib_inc_eeprom_err(dd, log_idx, 1);

	if (errs & QIB_E_SPKTERRS) {
		qib_disarm_7322_senderrbufs(dd->pport);
		qib_stats.sps_txerrs++;
	} else if (errs & QIB_E_INVALIDADDR)
		qib_stats.sps_txerrs++;
	else if (errs & QIB_E_ARMLAUNCH) {
		qib_stats.sps_txerrs++;
		qib_disarm_7322_senderrbufs(dd->pport);
	}
	qib_write_kreg(dd, kr_errclear, errs);

	mask = QIB_E_HARDWARE;
	*msg = '\0';

	err_decode(msg, sizeof dd->cspec->emsgbuf, errs & ~mask,
		   qib_7322error_msgs);

	if (errs & QIB_E_RESET) {
		int pidx;

		qib_dev_err(dd, "Got reset, requires re-init "
			    "(unload and reload driver)\n");
		dd->flags &= ~QIB_INITTED;  
		
		*dd->devstatusp |= QIB_STATUS_HWERROR;
		for (pidx = 0; pidx < dd->num_pports; ++pidx)
			if (dd->pport[pidx].link_speed_supported)
				*dd->pport[pidx].statusp &= ~QIB_STATUS_IB_CONF;
	}

	if (*msg && iserr)
		qib_dev_err(dd, "%s error\n", msg);

	if (errs & (ERR_MASK(RcvEgrFullErr) | ERR_MASK(RcvHdrFullErr))) {
		qib_handle_urcv(dd, ~0U);
		if (errs & ERR_MASK(RcvEgrFullErr))
			qib_stats.sps_buffull++;
		else
			qib_stats.sps_hdrfull++;
	}

done:
	return;
}

static void qib_error_tasklet(unsigned long data)
{
	struct qib_devdata *dd = (struct qib_devdata *)data;

	handle_7322_errors(dd);
	qib_write_kreg(dd, kr_errmask, dd->cspec->errormask);
}

static void reenable_chase(unsigned long opaque)
{
	struct qib_pportdata *ppd = (struct qib_pportdata *)opaque;

	ppd->cpspec->chase_timer.expires = 0;
	qib_set_ib_7322_lstate(ppd, QLOGIC_IB_IBCC_LINKCMD_DOWN,
		QLOGIC_IB_IBCC_LINKINITCMD_POLL);
}

static void disable_chase(struct qib_pportdata *ppd, unsigned long tnow,
		u8 ibclt)
{
	ppd->cpspec->chase_end = 0;

	if (!qib_chase)
		return;

	qib_set_ib_7322_lstate(ppd, QLOGIC_IB_IBCC_LINKCMD_DOWN,
		QLOGIC_IB_IBCC_LINKINITCMD_DISABLE);
	ppd->cpspec->chase_timer.expires = jiffies + QIB_CHASE_DIS_TIME;
	add_timer(&ppd->cpspec->chase_timer);
}

static void handle_serdes_issues(struct qib_pportdata *ppd, u64 ibcst)
{
	u8 ibclt;
	unsigned long tnow;

	ibclt = (u8)SYM_FIELD(ibcst, IBCStatusA_0, LinkTrainingState);

	switch (ibclt) {
	case IB_7322_LT_STATE_CFGRCVFCFG:
	case IB_7322_LT_STATE_CFGWAITRMT:
	case IB_7322_LT_STATE_TXREVLANES:
	case IB_7322_LT_STATE_CFGENH:
		tnow = jiffies;
		if (ppd->cpspec->chase_end &&
		     time_after(tnow, ppd->cpspec->chase_end))
			disable_chase(ppd, tnow, ibclt);
		else if (!ppd->cpspec->chase_end)
			ppd->cpspec->chase_end = tnow + QIB_CHASE_TIME;
		break;
	default:
		ppd->cpspec->chase_end = 0;
		break;
	}

	if (((ibclt >= IB_7322_LT_STATE_CFGTEST &&
	      ibclt <= IB_7322_LT_STATE_CFGWAITENH) ||
	     ibclt == IB_7322_LT_STATE_LINKUP) &&
	    (ibcst & SYM_MASK(IBCStatusA_0, LinkSpeedQDR))) {
		force_h1(ppd);
		ppd->cpspec->qdr_reforce = 1;
		if (!ppd->dd->cspec->r1)
			serdes_7322_los_enable(ppd, 0);
	} else if (ppd->cpspec->qdr_reforce &&
		(ibcst & SYM_MASK(IBCStatusA_0, LinkSpeedQDR)) &&
		 (ibclt == IB_7322_LT_STATE_CFGENH ||
		ibclt == IB_7322_LT_STATE_CFGIDLE ||
		ibclt == IB_7322_LT_STATE_LINKUP))
		force_h1(ppd);

	if ((IS_QMH(ppd->dd) || IS_QME(ppd->dd)) &&
	    ppd->link_speed_enabled == QIB_IB_QDR &&
	    (ibclt == IB_7322_LT_STATE_CFGTEST ||
	     ibclt == IB_7322_LT_STATE_CFGENH ||
	     (ibclt >= IB_7322_LT_STATE_POLLACTIVE &&
	      ibclt <= IB_7322_LT_STATE_SLEEPQUIET)))
		adj_tx_serdes(ppd);

	if (ibclt != IB_7322_LT_STATE_LINKUP) {
		u8 ltstate = qib_7322_phys_portstate(ibcst);
		u8 pibclt = (u8)SYM_FIELD(ppd->lastibcstat, IBCStatusA_0,
					  LinkTrainingState);
		if (!ppd->dd->cspec->r1 &&
		    pibclt == IB_7322_LT_STATE_LINKUP &&
		    ltstate != IB_PHYSPORTSTATE_LINK_ERR_RECOVER &&
		    ltstate != IB_PHYSPORTSTATE_RECOVERY_RETRAIN &&
		    ltstate != IB_PHYSPORTSTATE_RECOVERY_WAITRMT &&
		    ltstate != IB_PHYSPORTSTATE_RECOVERY_IDLE)
			serdes_7322_los_enable(ppd, 1);
		if (!ppd->cpspec->qdr_dfe_on &&
		    ibclt <= IB_7322_LT_STATE_SLEEPQUIET) {
			ppd->cpspec->qdr_dfe_on = 1;
			ppd->cpspec->qdr_dfe_time = 0;
			
			qib_write_kreg_port(ppd, krp_static_adapt_dis(2),
					    ppd->dd->cspec->r1 ?
					    QDR_STATIC_ADAPT_DOWN_R1 :
					    QDR_STATIC_ADAPT_DOWN);
			printk(KERN_INFO QIB_DRV_NAME
				" IB%u:%u re-enabled QDR adaptation "
				"ibclt %x\n", ppd->dd->unit, ppd->port, ibclt);
		}
	}
}

static int qib_7322_set_ib_cfg(struct qib_pportdata *, int, u32);

static noinline void handle_7322_p_errors(struct qib_pportdata *ppd)
{
	char *msg;
	u64 ignore_this_time = 0, iserr = 0, errs, fmask;
	struct qib_devdata *dd = ppd->dd;

	
	fmask = qib_read_kreg64(dd, kr_act_fmask);
	if (!fmask)
		check_7322_rxe_status(ppd);

	errs = qib_read_kreg_port(ppd, krp_errstatus);
	if (!errs)
		qib_devinfo(dd->pcidev,
			 "Port%d error interrupt, but no error bits set!\n",
			 ppd->port);
	if (!fmask)
		errs &= ~QIB_E_P_IBSTATUSCHANGED;
	if (!errs)
		goto done;

	msg = ppd->cpspec->epmsgbuf;
	*msg = '\0';

	if (errs & ~QIB_E_P_BITSEXTANT) {
		err_decode(msg, sizeof ppd->cpspec->epmsgbuf,
			   errs & ~QIB_E_P_BITSEXTANT, qib_7322p_error_msgs);
		if (!*msg)
			snprintf(msg, sizeof ppd->cpspec->epmsgbuf,
				 "no others");
		qib_dev_porterr(dd, ppd->port, "error interrupt with unknown"
				" errors 0x%016Lx set (and %s)\n",
				(errs & ~QIB_E_P_BITSEXTANT), msg);
		*msg = '\0';
	}

	if (errs & QIB_E_P_SHDR) {
		u64 symptom;

		
		symptom = qib_read_kreg_port(ppd, krp_sendhdrsymptom);
		qib_write_kreg_port(ppd, krp_sendhdrsymptom, 0);
		err_decode(msg, sizeof ppd->cpspec->epmsgbuf, symptom,
			   hdrchk_msgs);
		*msg = '\0';
		
	}

	if (errs & QIB_E_P_SPKTERRS) {
		if ((errs & QIB_E_P_LINK_PKTERRS) &&
		    !(ppd->lflags & QIBL_LINKACTIVE)) {
			err_decode(msg, sizeof ppd->cpspec->epmsgbuf,
				   (errs & QIB_E_P_LINK_PKTERRS),
				   qib_7322p_error_msgs);
			*msg = '\0';
			ignore_this_time = errs & QIB_E_P_LINK_PKTERRS;
		}
		qib_disarm_7322_senderrbufs(ppd);
	} else if ((errs & QIB_E_P_LINK_PKTERRS) &&
		   !(ppd->lflags & QIBL_LINKACTIVE)) {
		err_decode(msg, sizeof ppd->cpspec->epmsgbuf, errs,
			   qib_7322p_error_msgs);
		ignore_this_time = errs & QIB_E_P_LINK_PKTERRS;
		*msg = '\0';
	}

	qib_write_kreg_port(ppd, krp_errclear, errs);

	errs &= ~ignore_this_time;
	if (!errs)
		goto done;

	if (errs & QIB_E_P_RPKTERRS)
		qib_stats.sps_rcverrs++;
	if (errs & QIB_E_P_SPKTERRS)
		qib_stats.sps_txerrs++;

	iserr = errs & ~(QIB_E_P_RPKTERRS | QIB_E_P_PKTERRS);

	if (errs & QIB_E_P_SDMAERRS)
		sdma_7322_p_errors(ppd, errs);

	if (errs & QIB_E_P_IBSTATUSCHANGED) {
		u64 ibcs;
		u8 ltstate;

		ibcs = qib_read_kreg_port(ppd, krp_ibcstatus_a);
		ltstate = qib_7322_phys_portstate(ibcs);

		if (!(ppd->lflags & QIBL_IB_AUTONEG_INPROG))
			handle_serdes_issues(ppd, ibcs);
		if (!(ppd->cpspec->ibcctrl_a &
		      SYM_MASK(IBCCtrlA_0, IBStatIntReductionEn))) {
			ppd->cpspec->ibcctrl_a |=
				SYM_MASK(IBCCtrlA_0, IBStatIntReductionEn);
			qib_write_kreg_port(ppd, krp_ibcctrl_a,
					    ppd->cpspec->ibcctrl_a);
		}

		
		ppd->link_width_active =
			(ibcs & SYM_MASK(IBCStatusA_0, LinkWidthActive)) ?
			    IB_WIDTH_4X : IB_WIDTH_1X;
		ppd->link_speed_active = (ibcs & SYM_MASK(IBCStatusA_0,
			LinkSpeedQDR)) ? QIB_IB_QDR : (ibcs &
			  SYM_MASK(IBCStatusA_0, LinkSpeedActive)) ?
				   QIB_IB_DDR : QIB_IB_SDR;

		if ((ppd->lflags & QIBL_IB_LINK_DISABLED) && ltstate !=
		    IB_PHYSPORTSTATE_DISABLED)
			qib_set_ib_7322_lstate(ppd, 0,
			       QLOGIC_IB_IBCC_LINKINITCMD_DISABLE);
		else
			if (ltstate != IB_PHYSPORTSTATE_LINK_ERR_RECOVER &&
			    ltstate != IB_PHYSPORTSTATE_RECOVERY_RETRAIN &&
			    ltstate != IB_PHYSPORTSTATE_RECOVERY_WAITRMT &&
			    ltstate != IB_PHYSPORTSTATE_RECOVERY_IDLE)
				qib_handle_e_ibstatuschanged(ppd, ibcs);
	}
	if (*msg && iserr)
		qib_dev_porterr(dd, ppd->port, "%s error\n", msg);

	if (ppd->state_wanted & ppd->lflags)
		wake_up_interruptible(&ppd->state_wait);
done:
	return;
}

static void qib_7322_set_intr_state(struct qib_devdata *dd, u32 enable)
{
	if (enable) {
		if (dd->flags & QIB_BADINTR)
			return;
		qib_write_kreg(dd, kr_intmask, dd->cspec->int_enable_mask);
		
		qib_write_kreg(dd, kr_intclear, 0ULL);
		if (dd->cspec->num_msix_entries) {
			
			u64 val = qib_read_kreg64(dd, kr_intgranted);
			if (val)
				qib_write_kreg(dd, kr_intgranted, val);
		}
	} else
		qib_write_kreg(dd, kr_intmask, 0ULL);
}

/*
 * Try to cleanup as much as possible for anything that might have gone
 * wrong while in freeze mode, such as pio buffers being written by user
 * processes (causing armlaunch), send errors due to going into freeze mode,
 * etc., and try to avoid causing extra interrupts while doing so.
 * Forcibly update the in-memory pioavail register copies after cleanup
 * because the chip won't do it while in freeze mode (the register values
 * themselves are kept correct).
 * Make sure that we don't lose any important interrupts by using the chip
 * feature that says that writing 0 to a bit in *clear that is set in
 * *status will cause an interrupt to be generated again (if allowed by
 * the *mask value).
 * This is in chip-specific code because of all of the register accesses,
 * even though the details are similar on most chips.
 */
static void qib_7322_clear_freeze(struct qib_devdata *dd)
{
	int pidx;

	
	qib_write_kreg(dd, kr_errmask, 0ULL);

	for (pidx = 0; pidx < dd->num_pports; ++pidx)
		if (dd->pport[pidx].link_speed_supported)
			qib_write_kreg_port(dd->pport + pidx, krp_errmask,
					    0ULL);

	
	qib_7322_set_intr_state(dd, 0);

	
	qib_write_kreg(dd, kr_control, dd->control);
	qib_read_kreg32(dd, kr_scratch);

	qib_write_kreg(dd, kr_hwerrclear, 0ULL);
	qib_write_kreg(dd, kr_errclear, E_SPKT_ERRS_IGNORE);
	qib_write_kreg(dd, kr_errmask, dd->cspec->errormask);
	
	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		if (!dd->pport[pidx].link_speed_supported)
			continue;
		qib_write_kreg_port(dd->pport + pidx, krp_errclear, ~0Ull);
		qib_write_kreg_port(dd->pport + pidx, krp_errmask, ~0Ull);
	}
	qib_7322_set_intr_state(dd, 1);
}

static void qib_7322_handle_hwerrors(struct qib_devdata *dd, char *msg,
				     size_t msgl)
{
	u64 hwerrs;
	u32 ctrl;
	int isfatal = 0;

	hwerrs = qib_read_kreg64(dd, kr_hwerrstatus);
	if (!hwerrs)
		goto bail;
	if (hwerrs == ~0ULL) {
		qib_dev_err(dd, "Read of hardware error status failed "
			    "(all bits set); ignoring\n");
		goto bail;
	}
	qib_stats.sps_hwerrs++;

	
	qib_write_kreg(dd, kr_hwerrclear, hwerrs &
		       ~HWE_MASK(PowerOnBISTFailed));

	hwerrs &= dd->cspec->hwerrmask;

	

	if (hwerrs)
		qib_devinfo(dd->pcidev, "Hardware error: hwerr=0x%llx "
			    "(cleared)\n", (unsigned long long) hwerrs);

	ctrl = qib_read_kreg32(dd, kr_control);
	if ((ctrl & SYM_MASK(Control, FreezeMode)) && !dd->diag_client) {
		if ((hwerrs & ~HWE_MASK(LATriggered)) ||
		    dd->cspec->stay_in_freeze) {
			if (dd->flags & QIB_INITTED)
				isfatal = 1;
		} else
			qib_7322_clear_freeze(dd);
	}

	if (hwerrs & HWE_MASK(PowerOnBISTFailed)) {
		isfatal = 1;
		strlcpy(msg, "[Memory BIST test failed, "
			"InfiniPath hardware unusable]", msgl);
		
		dd->cspec->hwerrmask &= ~HWE_MASK(PowerOnBISTFailed);
		qib_write_kreg(dd, kr_hwerrmask, dd->cspec->hwerrmask);
	}

	err_decode(msg, msgl, hwerrs, qib_7322_hwerror_msgs);

	

	qib_dev_err(dd, "%s hardware error\n", msg);

	if (isfatal && !dd->diag_client) {
		qib_dev_err(dd, "Fatal Hardware Error, no longer"
			    " usable, SN %.16s\n", dd->serial);
		if (dd->freezemsg)
			snprintf(dd->freezemsg, dd->freezelen,
				 "{%s}", msg);
		qib_disable_after_error(dd);
	}
bail:;
}

static void qib_7322_init_hwerrors(struct qib_devdata *dd)
{
	int pidx;
	u64 extsval;

	extsval = qib_read_kreg64(dd, kr_extstatus);
	if (!(extsval & (QIB_EXTS_MEMBIST_DISABLED |
			 QIB_EXTS_MEMBIST_ENDTEST)))
		qib_dev_err(dd, "MemBIST did not complete!\n");

	
	qib_write_kreg(dd, kr_hwerrclear, ~HWE_MASK(PowerOnBISTFailed));
	qib_write_kreg(dd, kr_hwerrmask, dd->cspec->hwerrmask);

	
	qib_write_kreg(dd, kr_errclear, ~0ULL);
	
	qib_write_kreg(dd, kr_errmask, ~0ULL);
	dd->cspec->errormask = qib_read_kreg64(dd, kr_errmask);
	for (pidx = 0; pidx < dd->num_pports; ++pidx)
		if (dd->pport[pidx].link_speed_supported)
			qib_write_kreg_port(dd->pport + pidx, krp_errmask,
					    ~0ULL);
}

static void qib_set_7322_armlaunch(struct qib_devdata *dd, u32 enable)
{
	if (enable) {
		qib_write_kreg(dd, kr_errclear, QIB_E_SPIOARMLAUNCH);
		dd->cspec->errormask |= QIB_E_SPIOARMLAUNCH;
	} else
		dd->cspec->errormask &= ~QIB_E_SPIOARMLAUNCH;
	qib_write_kreg(dd, kr_errmask, dd->cspec->errormask);
}

static void qib_set_ib_7322_lstate(struct qib_pportdata *ppd, u16 linkcmd,
				   u16 linitcmd)
{
	u64 mod_wd;
	struct qib_devdata *dd = ppd->dd;
	unsigned long flags;

	if (linitcmd == QLOGIC_IB_IBCC_LINKINITCMD_DISABLE) {
		qib_7322_mini_pcs_reset(ppd);
		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags |= QIBL_IB_LINK_DISABLED;
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	} else if (linitcmd || linkcmd == QLOGIC_IB_IBCC_LINKCMD_DOWN) {
		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags &= ~QIBL_IB_LINK_DISABLED;
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
		ppd->cpspec->ibcctrl_a &=
			~SYM_MASK(IBCCtrlA_0, IBStatIntReductionEn);
	}

	mod_wd = (linkcmd << IBA7322_IBCC_LINKCMD_SHIFT) |
		(linitcmd << QLOGIC_IB_IBCC_LINKINITCMD_SHIFT);

	qib_write_kreg_port(ppd, krp_ibcctrl_a, ppd->cpspec->ibcctrl_a |
			    mod_wd);
	
	qib_write_kreg(dd, kr_scratch, 0);

}

#define RCV_BUF_UNITSZ 64
#define NUM_RCV_BUF_UNITS(dd) ((64 * 1024) / (RCV_BUF_UNITSZ * dd->num_pports))

static void set_vls(struct qib_pportdata *ppd)
{
	int i, numvls, totcred, cred_vl, vl0extra;
	struct qib_devdata *dd = ppd->dd;
	u64 val;

	numvls = qib_num_vls(ppd->vls_operational);

	
	totcred = NUM_RCV_BUF_UNITS(dd);
	cred_vl = (2 * 288 + RCV_BUF_UNITSZ - 1) / RCV_BUF_UNITSZ;
	totcred -= cred_vl;
	qib_write_kreg_port(ppd, krp_rxcreditvl15, (u64) cred_vl);
	cred_vl = totcred / numvls;
	vl0extra = totcred - cred_vl * numvls;
	qib_write_kreg_port(ppd, krp_rxcreditvl0, cred_vl + vl0extra);
	for (i = 1; i < numvls; i++)
		qib_write_kreg_port(ppd, krp_rxcreditvl0 + i, cred_vl);
	for (; i < 8; i++) 
		qib_write_kreg_port(ppd, krp_rxcreditvl0 + i, 0);

	
	val = qib_read_kreg_port(ppd, krp_ibsdtestiftx);
	val |= SYM_MASK(IB_SDTEST_IF_TX_0, CREDIT_CHANGE);
	qib_write_kreg_port(ppd, krp_ibsdtestiftx, val);
	qib_write_kreg(dd, kr_scratch, 0ULL);
	val &= ~SYM_MASK(IB_SDTEST_IF_TX_0, CREDIT_CHANGE);
	qib_write_kreg_port(ppd, krp_ibsdtestiftx, val);

	for (i = 0; i < numvls; i++)
		val = qib_read_kreg_port(ppd, krp_rxcreditvl0 + i);
	val = qib_read_kreg_port(ppd, krp_rxcreditvl15);

	
	ppd->cpspec->ibcctrl_a = (ppd->cpspec->ibcctrl_a &
				~SYM_MASK(IBCCtrlA_0, NumVLane)) |
		((u64)(numvls - 1) << SYM_LSB(IBCCtrlA_0, NumVLane));
	qib_write_kreg_port(ppd, krp_ibcctrl_a, ppd->cpspec->ibcctrl_a);
	qib_write_kreg(dd, kr_scratch, 0ULL);
}

static int serdes_7322_init(struct qib_pportdata *ppd);

static int qib_7322_bringup_serdes(struct qib_pportdata *ppd)
{
	struct qib_devdata *dd = ppd->dd;
	u64 val, guid, ibc;
	unsigned long flags;
	int ret = 0;

	
	ppd->cpspec->ibcctrl_a &= ~SYM_MASK(IBCCtrlA_0, IBLinkEn);
	qib_write_kreg_port(ppd, krp_ibcctrl_a, ppd->cpspec->ibcctrl_a);
	qib_write_kreg(dd, kr_scratch, 0ULL);

	if (qib_compat_ddr_negotiate) {
		ppd->cpspec->ibdeltainprog = 1;
		ppd->cpspec->ibsymsnap = read_7322_creg32_port(ppd,
						crp_ibsymbolerr);
		ppd->cpspec->iblnkerrsnap = read_7322_creg32_port(ppd,
						crp_iblinkerrrecov);
	}

	
	ibc = 0x5ULL << SYM_LSB(IBCCtrlA_0, FlowCtrlWaterMark);
	ibc |= 24ULL << SYM_LSB(IBCCtrlA_0, FlowCtrlPeriod);
	
	ibc |= 0xfULL << SYM_LSB(IBCCtrlA_0, PhyerrThreshold);
	
	ibc |= 0xfULL << SYM_LSB(IBCCtrlA_0, OverrunThreshold);
	ibc |= ((u64)(ppd->ibmaxlen >> 2) + 1) <<
		SYM_LSB(IBCCtrlA_0, MaxPktLen);
	ppd->cpspec->ibcctrl_a = ibc; 

	qib_7322_mini_pcs_reset(ppd);

	if (!ppd->cpspec->ibcctrl_b) {
		unsigned lse = ppd->link_speed_enabled;

		ppd->cpspec->ibcctrl_b = qib_read_kreg_port(ppd,
							     krp_ibcctrl_b);
		ppd->cpspec->ibcctrl_b &= ~(IBA7322_IBC_SPEED_QDR |
				IBA7322_IBC_SPEED_DDR |
				IBA7322_IBC_SPEED_SDR |
				IBA7322_IBC_WIDTH_AUTONEG |
				SYM_MASK(IBCCtrlB_0, IB_LANE_REV_SUPPORTED));
		if (lse & (lse - 1)) 
			ppd->cpspec->ibcctrl_b |=
				(lse << IBA7322_IBC_SPEED_LSB) |
				IBA7322_IBC_IBTA_1_2_MASK |
				IBA7322_IBC_MAX_SPEED_MASK;
		else
			ppd->cpspec->ibcctrl_b |= (lse == QIB_IB_QDR) ?
				IBA7322_IBC_SPEED_QDR |
				 IBA7322_IBC_IBTA_1_2_MASK :
				(lse == QIB_IB_DDR) ?
					IBA7322_IBC_SPEED_DDR :
					IBA7322_IBC_SPEED_SDR;
		if ((ppd->link_width_enabled & (IB_WIDTH_1X | IB_WIDTH_4X)) ==
		    (IB_WIDTH_1X | IB_WIDTH_4X))
			ppd->cpspec->ibcctrl_b |= IBA7322_IBC_WIDTH_AUTONEG;
		else
			ppd->cpspec->ibcctrl_b |=
				ppd->link_width_enabled == IB_WIDTH_4X ?
				IBA7322_IBC_WIDTH_4X_ONLY :
				IBA7322_IBC_WIDTH_1X_ONLY;

		
		ppd->cpspec->ibcctrl_b |= (IBA7322_IBC_RXPOL_MASK |
			IBA7322_IBC_HRTBT_MASK);
	}
	qib_write_kreg_port(ppd, krp_ibcctrl_b, ppd->cpspec->ibcctrl_b);

	
	val = qib_read_kreg_port(ppd, krp_ibcctrl_c);
	val &= ~SYM_MASK(IBCCtrlC_0, IB_FRONT_PORCH);
	val |= 0xfULL << SYM_LSB(IBCCtrlC_0, IB_FRONT_PORCH);
	qib_write_kreg_port(ppd, krp_ibcctrl_c, val);

	serdes_7322_init(ppd);

	guid = be64_to_cpu(ppd->guid);
	if (!guid) {
		if (dd->base_guid)
			guid = be64_to_cpu(dd->base_guid) + ppd->port - 1;
		ppd->guid = cpu_to_be64(guid);
	}

	qib_write_kreg_port(ppd, krp_hrtbt_guid, guid);
	
	qib_write_kreg(dd, kr_scratch, 0);

	
	ppd->cpspec->ibcctrl_a |= SYM_MASK(IBCCtrlA_0, IBLinkEn);
	set_vls(ppd);

	
	val = ppd->cpspec->ibcctrl_a | (QLOGIC_IB_IBCC_LINKINITCMD_DISABLE <<
					QLOGIC_IB_IBCC_LINKINITCMD_SHIFT);
	qib_write_kreg_port(ppd, krp_ibcctrl_a, val);
	qib_write_kreg(dd, kr_scratch, 0ULL);
	
	ppd->cpspec->ibcctrl_a = val & ~SYM_MASK(IBCCtrlA_0, LinkInitCmd);

	
	spin_lock_irqsave(&dd->cspec->rcvmod_lock, flags);
	ppd->p_rcvctrl |= SYM_MASK(RcvCtrl_0, RcvIBPortEnable);
	qib_write_kreg_port(ppd, krp_rcvctrl, ppd->p_rcvctrl);
	spin_unlock_irqrestore(&dd->cspec->rcvmod_lock, flags);

	
	val = qib_read_kreg_port(ppd, krp_errmask);
	qib_write_kreg_port(ppd, krp_errmask,
		val | ERR_MASK_N(IBStatusChanged));

	
	return ret;
}

static void qib_7322_mini_quiet_serdes(struct qib_pportdata *ppd)
{
	u64 val;
	unsigned long flags;

	qib_set_ib_7322_lstate(ppd, 0, QLOGIC_IB_IBCC_LINKINITCMD_DISABLE);

	spin_lock_irqsave(&ppd->lflags_lock, flags);
	ppd->lflags &= ~QIBL_IB_AUTONEG_INPROG;
	spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	wake_up(&ppd->cpspec->autoneg_wait);
	cancel_delayed_work_sync(&ppd->cpspec->autoneg_work);
	if (ppd->dd->cspec->r1)
		cancel_delayed_work_sync(&ppd->cpspec->ipg_work);

	ppd->cpspec->chase_end = 0;
	if (ppd->cpspec->chase_timer.data) 
		del_timer_sync(&ppd->cpspec->chase_timer);

	ppd->cpspec->ibcctrl_a &= ~SYM_MASK(IBCCtrlA_0, IBLinkEn);
	qib_7322_mini_pcs_reset(ppd);

	if (ppd->cpspec->ibsymdelta || ppd->cpspec->iblnkerrdelta ||
	    ppd->cpspec->ibdeltainprog || ppd->cpspec->iblnkdowndelta) {
		struct qib_devdata *dd = ppd->dd;
		u64 diagc;

		
		diagc = qib_read_kreg64(dd, kr_hwdiagctrl);
		qib_write_kreg(dd, kr_hwdiagctrl,
			       diagc | SYM_MASK(HwDiagCtrl, CounterWrEnable));

		if (ppd->cpspec->ibsymdelta || ppd->cpspec->ibdeltainprog) {
			val = read_7322_creg32_port(ppd, crp_ibsymbolerr);
			if (ppd->cpspec->ibdeltainprog)
				val -= val - ppd->cpspec->ibsymsnap;
			val -= ppd->cpspec->ibsymdelta;
			write_7322_creg_port(ppd, crp_ibsymbolerr, val);
		}
		if (ppd->cpspec->iblnkerrdelta || ppd->cpspec->ibdeltainprog) {
			val = read_7322_creg32_port(ppd, crp_iblinkerrrecov);
			if (ppd->cpspec->ibdeltainprog)
				val -= val - ppd->cpspec->iblnkerrsnap;
			val -= ppd->cpspec->iblnkerrdelta;
			write_7322_creg_port(ppd, crp_iblinkerrrecov, val);
		}
		if (ppd->cpspec->iblnkdowndelta) {
			val = read_7322_creg32_port(ppd, crp_iblinkdown);
			val += ppd->cpspec->iblnkdowndelta;
			write_7322_creg_port(ppd, crp_iblinkdown, val);
		}

		
		qib_write_kreg(dd, kr_hwdiagctrl, diagc);
	}
}

static void qib_setup_7322_setextled(struct qib_pportdata *ppd, u32 on)
{
	struct qib_devdata *dd = ppd->dd;
	u64 extctl, ledblink = 0, val;
	unsigned long flags;
	int yel, grn;

	if (dd->diag_client)
		return;

	
	if (ppd->led_override) {
		grn = (ppd->led_override & QIB_LED_PHYS);
		yel = (ppd->led_override & QIB_LED_LOG);
	} else if (on) {
		val = qib_read_kreg_port(ppd, krp_ibcstatus_a);
		grn = qib_7322_phys_portstate(val) ==
			IB_PHYSPORTSTATE_LINKUP;
		yel = qib_7322_iblink_state(val) == IB_PORT_ACTIVE;
	} else {
		grn = 0;
		yel = 0;
	}

	spin_lock_irqsave(&dd->cspec->gpio_lock, flags);
	extctl = dd->cspec->extctrl & (ppd->port == 1 ?
		~ExtLED_IB1_MASK : ~ExtLED_IB2_MASK);
	if (grn) {
		extctl |= ppd->port == 1 ? ExtLED_IB1_GRN : ExtLED_IB2_GRN;
		ledblink = ((66600 * 1000UL / 4) << IBA7322_LEDBLINK_ON_SHIFT) |
			((187500 * 1000UL / 4) << IBA7322_LEDBLINK_OFF_SHIFT);
	}
	if (yel)
		extctl |= ppd->port == 1 ? ExtLED_IB1_YEL : ExtLED_IB2_YEL;
	dd->cspec->extctrl = extctl;
	qib_write_kreg(dd, kr_extctrl, dd->cspec->extctrl);
	spin_unlock_irqrestore(&dd->cspec->gpio_lock, flags);

	if (ledblink) 
		qib_write_kreg_port(ppd, krp_rcvpktledcnt, ledblink);
}

static void qib_7322_nomsix(struct qib_devdata *dd)
{
	u64 intgranted;
	int n;

	dd->cspec->main_int_mask = ~0ULL;
	n = dd->cspec->num_msix_entries;
	if (n) {
		int i;

		dd->cspec->num_msix_entries = 0;
		for (i = 0; i < n; i++) {
			irq_set_affinity_hint(
			  dd->cspec->msix_entries[i].msix.vector, NULL);
			free_cpumask_var(dd->cspec->msix_entries[i].mask);
			free_irq(dd->cspec->msix_entries[i].msix.vector,
			   dd->cspec->msix_entries[i].arg);
		}
		qib_nomsix(dd);
	}
	
	intgranted = qib_read_kreg64(dd, kr_intgranted);
	if (intgranted)
		qib_write_kreg(dd, kr_intgranted, intgranted);
}

static void qib_7322_free_irq(struct qib_devdata *dd)
{
	if (dd->cspec->irq) {
		free_irq(dd->cspec->irq, dd);
		dd->cspec->irq = 0;
	}
	qib_7322_nomsix(dd);
}

static void qib_setup_7322_cleanup(struct qib_devdata *dd)
{
	int i;

	qib_7322_free_irq(dd);
	kfree(dd->cspec->cntrs);
	kfree(dd->cspec->sendchkenable);
	kfree(dd->cspec->sendgrhchk);
	kfree(dd->cspec->sendibchk);
	kfree(dd->cspec->msix_entries);
	for (i = 0; i < dd->num_pports; i++) {
		unsigned long flags;
		u32 mask = QSFP_GPIO_MOD_PRS_N |
			(QSFP_GPIO_MOD_PRS_N << QSFP_GPIO_PORT2_SHIFT);

		kfree(dd->pport[i].cpspec->portcntrs);
		if (dd->flags & QIB_HAS_QSFP) {
			spin_lock_irqsave(&dd->cspec->gpio_lock, flags);
			dd->cspec->gpio_mask &= ~mask;
			qib_write_kreg(dd, kr_gpio_mask, dd->cspec->gpio_mask);
			spin_unlock_irqrestore(&dd->cspec->gpio_lock, flags);
			qib_qsfp_deinit(&dd->pport[i].cpspec->qsfp_data);
		}
		if (dd->pport[i].ibport_data.smi_ah)
			ib_destroy_ah(&dd->pport[i].ibport_data.smi_ah->ibah);
	}
}

static void sdma_7322_intr(struct qib_devdata *dd, u64 istat)
{
	struct qib_pportdata *ppd0 = &dd->pport[0];
	struct qib_pportdata *ppd1 = &dd->pport[1];
	u64 intr0 = istat & (INT_MASK_P(SDma, 0) |
		INT_MASK_P(SDmaIdle, 0) | INT_MASK_P(SDmaProgress, 0));
	u64 intr1 = istat & (INT_MASK_P(SDma, 1) |
		INT_MASK_P(SDmaIdle, 1) | INT_MASK_P(SDmaProgress, 1));

	if (intr0)
		qib_sdma_intr(ppd0);
	if (intr1)
		qib_sdma_intr(ppd1);

	if (istat & INT_MASK_PM(SDmaCleanupDone, 0))
		qib_sdma_process_event(ppd0, qib_sdma_event_e20_hw_started);
	if (istat & INT_MASK_PM(SDmaCleanupDone, 1))
		qib_sdma_process_event(ppd1, qib_sdma_event_e20_hw_started);
}

static void qib_wantpiobuf_7322_intr(struct qib_devdata *dd, u32 needint)
{
	unsigned long flags;

	spin_lock_irqsave(&dd->sendctrl_lock, flags);
	if (needint)
		dd->sendctrl |= SYM_MASK(SendCtrl, SendIntBufAvail);
	else
		dd->sendctrl &= ~SYM_MASK(SendCtrl, SendIntBufAvail);
	qib_write_kreg(dd, kr_sendctrl, dd->sendctrl);
	qib_write_kreg(dd, kr_scratch, 0ULL);
	spin_unlock_irqrestore(&dd->sendctrl_lock, flags);
}

static noinline void unknown_7322_ibits(struct qib_devdata *dd, u64 istat)
{
	u64 kills;
	char msg[128];

	kills = istat & ~QIB_I_BITSEXTANT;
	qib_dev_err(dd, "Clearing reserved interrupt(s) 0x%016llx:"
		    " %s\n", (unsigned long long) kills, msg);
	qib_write_kreg(dd, kr_intmask, (dd->cspec->int_enable_mask & ~kills));
}

static noinline void unknown_7322_gpio_intr(struct qib_devdata *dd)
{
	u32 gpiostatus;
	int handled = 0;
	int pidx;

	gpiostatus = qib_read_kreg32(dd, kr_gpio_status);
	qib_write_kreg(dd, kr_gpio_clear, gpiostatus);
	for (pidx = 0; pidx < dd->num_pports && (dd->flags & QIB_HAS_QSFP);
	     ++pidx) {
		struct qib_pportdata *ppd;
		struct qib_qsfp_data *qd;
		u32 mask;
		if (!dd->pport[pidx].link_speed_supported)
			continue;
		mask = QSFP_GPIO_MOD_PRS_N;
		ppd = dd->pport + pidx;
		mask <<= (QSFP_GPIO_PORT2_SHIFT * ppd->hw_pidx);
		if (gpiostatus & dd->cspec->gpio_mask & mask) {
			u64 pins;
			qd = &ppd->cpspec->qsfp_data;
			gpiostatus &= ~mask;
			pins = qib_read_kreg64(dd, kr_extstatus);
			pins >>= SYM_LSB(EXTStatus, GPIOIn);
			if (!(pins & mask)) {
				++handled;
				qd->t_insert = jiffies;
				queue_work(ib_wq, &qd->work);
			}
		}
	}

	if (gpiostatus && !handled) {
		const u32 mask = qib_read_kreg32(dd, kr_gpio_mask);
		u32 gpio_irq = mask & gpiostatus;

		dd->cspec->gpio_mask &= ~gpio_irq;
		qib_write_kreg(dd, kr_gpio_mask, dd->cspec->gpio_mask);
	}
}

static noinline void unlikely_7322_intr(struct qib_devdata *dd, u64 istat)
{
	if (istat & ~QIB_I_BITSEXTANT)
		unknown_7322_ibits(dd, istat);
	if (istat & QIB_I_GPIO)
		unknown_7322_gpio_intr(dd);
	if (istat & QIB_I_C_ERROR) {
		qib_write_kreg(dd, kr_errmask, 0ULL);
		tasklet_schedule(&dd->error_tasklet);
	}
	if (istat & INT_MASK_P(Err, 0) && dd->rcd[0])
		handle_7322_p_errors(dd->rcd[0]->ppd);
	if (istat & INT_MASK_P(Err, 1) && dd->rcd[1])
		handle_7322_p_errors(dd->rcd[1]->ppd);
}

static void adjust_rcv_timeout(struct qib_ctxtdata *rcd, int npkts)
{
	struct qib_devdata *dd = rcd->dd;
	u32 timeout = dd->cspec->rcvavail_timeout[rcd->ctxt];

	if (npkts < rcv_int_count && timeout > 2)
		timeout >>= 1;
	else if (npkts >= rcv_int_count && timeout < rcv_int_timeout)
		timeout = min(timeout << 1, rcv_int_timeout);
	else
		return;

	dd->cspec->rcvavail_timeout[rcd->ctxt] = timeout;
	qib_write_kreg(dd, kr_rcvavailtimeout + rcd->ctxt, timeout);
}

static irqreturn_t qib_7322intr(int irq, void *data)
{
	struct qib_devdata *dd = data;
	irqreturn_t ret;
	u64 istat;
	u64 ctxtrbits;
	u64 rmask;
	unsigned i;
	u32 npkts;

	if ((dd->flags & (QIB_PRESENT | QIB_BADINTR)) != QIB_PRESENT) {
		ret = IRQ_HANDLED;
		goto bail;
	}

	istat = qib_read_kreg64(dd, kr_intstatus);

	if (unlikely(istat == ~0ULL)) {
		qib_bad_intrstatus(dd);
		qib_dev_err(dd, "Interrupt status all f's, skipping\n");
		
		ret = IRQ_NONE;
		goto bail;
	}

	istat &= dd->cspec->main_int_mask;
	if (unlikely(!istat)) {
		
		ret = IRQ_NONE;
		goto bail;
	}

	qib_stats.sps_ints++;
	if (dd->int_counter != (u32) -1)
		dd->int_counter++;

	
	if (unlikely(istat & (~QIB_I_BITSEXTANT | QIB_I_GPIO |
			      QIB_I_C_ERROR | INT_MASK_P(Err, 0) |
			      INT_MASK_P(Err, 1))))
		unlikely_7322_intr(dd, istat);

	qib_write_kreg(dd, kr_intclear, istat);

	ctxtrbits = istat & (QIB_I_RCVAVAIL_MASK | QIB_I_RCVURG_MASK);
	if (ctxtrbits) {
		rmask = (1ULL << QIB_I_RCVAVAIL_LSB) |
			(1ULL << QIB_I_RCVURG_LSB);
		for (i = 0; i < dd->first_user_ctxt; i++) {
			if (ctxtrbits & rmask) {
				ctxtrbits &= ~rmask;
				if (dd->rcd[i])
					qib_kreceive(dd->rcd[i], NULL, &npkts);
			}
			rmask <<= 1;
		}
		if (ctxtrbits) {
			ctxtrbits = (ctxtrbits >> QIB_I_RCVAVAIL_LSB) |
				(ctxtrbits >> QIB_I_RCVURG_LSB);
			qib_handle_urcv(dd, ctxtrbits);
		}
	}

	if (istat & (QIB_I_P_SDMAINT(0) | QIB_I_P_SDMAINT(1)))
		sdma_7322_intr(dd, istat);

	if ((istat & QIB_I_SPIOBUFAVAIL) && (dd->flags & QIB_INITTED))
		qib_ib_piobufavail(dd);

	ret = IRQ_HANDLED;
bail:
	return ret;
}

static irqreturn_t qib_7322pintr(int irq, void *data)
{
	struct qib_ctxtdata *rcd = data;
	struct qib_devdata *dd = rcd->dd;
	u32 npkts;

	if ((dd->flags & (QIB_PRESENT | QIB_BADINTR)) != QIB_PRESENT)
		return IRQ_HANDLED;

	qib_stats.sps_ints++;
	if (dd->int_counter != (u32) -1)
		dd->int_counter++;

	
	qib_write_kreg(dd, kr_intclear, ((1ULL << QIB_I_RCVAVAIL_LSB) |
		       (1ULL << QIB_I_RCVURG_LSB)) << rcd->ctxt);

	qib_kreceive(rcd, NULL, &npkts);

	return IRQ_HANDLED;
}

static irqreturn_t qib_7322bufavail(int irq, void *data)
{
	struct qib_devdata *dd = data;

	if ((dd->flags & (QIB_PRESENT | QIB_BADINTR)) != QIB_PRESENT)
		return IRQ_HANDLED;

	qib_stats.sps_ints++;
	if (dd->int_counter != (u32) -1)
		dd->int_counter++;

	
	qib_write_kreg(dd, kr_intclear, QIB_I_SPIOBUFAVAIL);

	
	if (dd->flags & QIB_INITTED)
		qib_ib_piobufavail(dd);
	else
		qib_wantpiobuf_7322_intr(dd, 0);

	return IRQ_HANDLED;
}

static irqreturn_t sdma_intr(int irq, void *data)
{
	struct qib_pportdata *ppd = data;
	struct qib_devdata *dd = ppd->dd;

	if ((dd->flags & (QIB_PRESENT | QIB_BADINTR)) != QIB_PRESENT)
		return IRQ_HANDLED;

	qib_stats.sps_ints++;
	if (dd->int_counter != (u32) -1)
		dd->int_counter++;

	
	qib_write_kreg(dd, kr_intclear, ppd->hw_pidx ?
		       INT_MASK_P(SDma, 1) : INT_MASK_P(SDma, 0));
	qib_sdma_intr(ppd);

	return IRQ_HANDLED;
}

static irqreturn_t sdma_idle_intr(int irq, void *data)
{
	struct qib_pportdata *ppd = data;
	struct qib_devdata *dd = ppd->dd;

	if ((dd->flags & (QIB_PRESENT | QIB_BADINTR)) != QIB_PRESENT)
		return IRQ_HANDLED;

	qib_stats.sps_ints++;
	if (dd->int_counter != (u32) -1)
		dd->int_counter++;

	
	qib_write_kreg(dd, kr_intclear, ppd->hw_pidx ?
		       INT_MASK_P(SDmaIdle, 1) : INT_MASK_P(SDmaIdle, 0));
	qib_sdma_intr(ppd);

	return IRQ_HANDLED;
}

static irqreturn_t sdma_progress_intr(int irq, void *data)
{
	struct qib_pportdata *ppd = data;
	struct qib_devdata *dd = ppd->dd;

	if ((dd->flags & (QIB_PRESENT | QIB_BADINTR)) != QIB_PRESENT)
		return IRQ_HANDLED;

	qib_stats.sps_ints++;
	if (dd->int_counter != (u32) -1)
		dd->int_counter++;

	
	qib_write_kreg(dd, kr_intclear, ppd->hw_pidx ?
		       INT_MASK_P(SDmaProgress, 1) :
		       INT_MASK_P(SDmaProgress, 0));
	qib_sdma_intr(ppd);

	return IRQ_HANDLED;
}

static irqreturn_t sdma_cleanup_intr(int irq, void *data)
{
	struct qib_pportdata *ppd = data;
	struct qib_devdata *dd = ppd->dd;

	if ((dd->flags & (QIB_PRESENT | QIB_BADINTR)) != QIB_PRESENT)
		return IRQ_HANDLED;

	qib_stats.sps_ints++;
	if (dd->int_counter != (u32) -1)
		dd->int_counter++;

	
	qib_write_kreg(dd, kr_intclear, ppd->hw_pidx ?
		       INT_MASK_PM(SDmaCleanupDone, 1) :
		       INT_MASK_PM(SDmaCleanupDone, 0));
	qib_sdma_process_event(ppd, qib_sdma_event_e20_hw_started);

	return IRQ_HANDLED;
}

static void qib_setup_7322_interrupt(struct qib_devdata *dd, int clearpend)
{
	int ret, i, msixnum;
	u64 redirect[6];
	u64 mask;
	const struct cpumask *local_mask;
	int firstcpu, secondcpu = 0, currrcvcpu = 0;

	if (!dd->num_pports)
		return;

	if (clearpend) {
		qib_7322_set_intr_state(dd, 0);

		
		qib_7322_init_hwerrors(dd);

		
		qib_write_kreg(dd, kr_intclear, ~0ULL);

		
		qib_write_kreg(dd, kr_intgranted, ~0ULL);
		qib_write_kreg(dd, kr_vecclr_wo_int, ~0ULL);
	}

	if (!dd->cspec->num_msix_entries) {
		
try_intx:
		if (!dd->pcidev->irq) {
			qib_dev_err(dd, "irq is 0, BIOS error?  "
				    "Interrupts won't work\n");
			goto bail;
		}
		ret = request_irq(dd->pcidev->irq, qib_7322intr,
				  IRQF_SHARED, QIB_DRV_NAME, dd);
		if (ret) {
			qib_dev_err(dd, "Couldn't setup INTx "
				    "interrupt (irq=%d): %d\n",
				    dd->pcidev->irq, ret);
			goto bail;
		}
		dd->cspec->irq = dd->pcidev->irq;
		dd->cspec->main_int_mask = ~0ULL;
		goto bail;
	}

	
	memset(redirect, 0, sizeof redirect);
	mask = ~0ULL;
	msixnum = 0;
	local_mask = cpumask_of_pcibus(dd->pcidev->bus);
	firstcpu = cpumask_first(local_mask);
	if (firstcpu >= nr_cpu_ids ||
			cpumask_weight(local_mask) == num_online_cpus()) {
		local_mask = topology_core_cpumask(0);
		firstcpu = cpumask_first(local_mask);
	}
	if (firstcpu < nr_cpu_ids) {
		secondcpu = cpumask_next(firstcpu, local_mask);
		if (secondcpu >= nr_cpu_ids)
			secondcpu = firstcpu;
		currrcvcpu = secondcpu;
	}
	for (i = 0; msixnum < dd->cspec->num_msix_entries; i++) {
		irq_handler_t handler;
		void *arg;
		u64 val;
		int lsb, reg, sh;

		dd->cspec->msix_entries[msixnum].
			name[sizeof(dd->cspec->msix_entries[msixnum].name) - 1]
			= '\0';
		if (i < ARRAY_SIZE(irq_table)) {
			if (irq_table[i].port) {
				
				if (irq_table[i].port > dd->num_pports)
					continue;
				arg = dd->pport + irq_table[i].port - 1;
			} else
				arg = dd;
			lsb = irq_table[i].lsb;
			handler = irq_table[i].handler;
			snprintf(dd->cspec->msix_entries[msixnum].name,
				sizeof(dd->cspec->msix_entries[msixnum].name)
				 - 1,
				QIB_DRV_NAME "%d%s", dd->unit,
				irq_table[i].name);
		} else {
			unsigned ctxt;

			ctxt = i - ARRAY_SIZE(irq_table);
			
			arg = dd->rcd[ctxt];
			if (!arg)
				continue;
			if (qib_krcvq01_no_msi && ctxt < 2)
				continue;
			lsb = QIB_I_RCVAVAIL_LSB + ctxt;
			handler = qib_7322pintr;
			snprintf(dd->cspec->msix_entries[msixnum].name,
				sizeof(dd->cspec->msix_entries[msixnum].name)
				 - 1,
				QIB_DRV_NAME "%d (kctx)", dd->unit);
		}
		ret = request_irq(
			dd->cspec->msix_entries[msixnum].msix.vector,
			handler, 0, dd->cspec->msix_entries[msixnum].name,
			arg);
		if (ret) {
			qib_dev_err(dd, "Couldn't setup MSIx "
				"interrupt (vec=%d, irq=%d): %d\n", msixnum,
				dd->cspec->msix_entries[msixnum].msix.vector,
				ret);
			qib_7322_nomsix(dd);
			goto try_intx;
		}
		dd->cspec->msix_entries[msixnum].arg = arg;
		if (lsb >= 0) {
			reg = lsb / IBA7322_REDIRECT_VEC_PER_REG;
			sh = (lsb % IBA7322_REDIRECT_VEC_PER_REG) *
				SYM_LSB(IntRedirect0, vec1);
			mask &= ~(1ULL << lsb);
			redirect[reg] |= ((u64) msixnum) << sh;
		}
		val = qib_read_kreg64(dd, 2 * msixnum + 1 +
			(QIB_7322_MsixTable_OFFS / sizeof(u64)));
		if (firstcpu < nr_cpu_ids &&
			zalloc_cpumask_var(
				&dd->cspec->msix_entries[msixnum].mask,
				GFP_KERNEL)) {
			if (handler == qib_7322pintr) {
				cpumask_set_cpu(currrcvcpu,
					dd->cspec->msix_entries[msixnum].mask);
				currrcvcpu = cpumask_next(currrcvcpu,
					local_mask);
				if (currrcvcpu >= nr_cpu_ids)
					currrcvcpu = secondcpu;
			} else {
				cpumask_set_cpu(firstcpu,
					dd->cspec->msix_entries[msixnum].mask);
			}
			irq_set_affinity_hint(
				dd->cspec->msix_entries[msixnum].msix.vector,
				dd->cspec->msix_entries[msixnum].mask);
		}
		msixnum++;
	}
	
	for (i = 0; i < ARRAY_SIZE(redirect); i++)
		qib_write_kreg(dd, kr_intredirect + i, redirect[i]);
	dd->cspec->main_int_mask = mask;
	tasklet_init(&dd->error_tasklet, qib_error_tasklet,
		(unsigned long)dd);
bail:;
}

static unsigned qib_7322_boardname(struct qib_devdata *dd)
{
	
	char *n;
	u32 boardid, namelen;
	unsigned features = DUAL_PORT_CAP;

	boardid = SYM_FIELD(dd->revision, Revision, BoardID);

	switch (boardid) {
	case 0:
		n = "InfiniPath_QLE7342_Emulation";
		break;
	case 1:
		n = "InfiniPath_QLE7340";
		dd->flags |= QIB_HAS_QSFP;
		features = PORT_SPD_CAP;
		break;
	case 2:
		n = "InfiniPath_QLE7342";
		dd->flags |= QIB_HAS_QSFP;
		break;
	case 3:
		n = "InfiniPath_QMI7342";
		break;
	case 4:
		n = "InfiniPath_Unsupported7342";
		qib_dev_err(dd, "Unsupported version of QMH7342\n");
		features = 0;
		break;
	case BOARD_QMH7342:
		n = "InfiniPath_QMH7342";
		features = 0x24;
		break;
	case BOARD_QME7342:
		n = "InfiniPath_QME7342";
		break;
	case 8:
		n = "InfiniPath_QME7362";
		dd->flags |= QIB_HAS_QSFP;
		break;
	case 15:
		n = "InfiniPath_QLE7342_TEST";
		dd->flags |= QIB_HAS_QSFP;
		break;
	default:
		n = "InfiniPath_QLE73xy_UNKNOWN";
		qib_dev_err(dd, "Unknown 7322 board type %u\n", boardid);
		break;
	}
	dd->board_atten = 1; 

	namelen = strlen(n) + 1;
	dd->boardname = kmalloc(namelen, GFP_KERNEL);
	if (!dd->boardname)
		qib_dev_err(dd, "Failed allocation for board name: %s\n", n);
	else
		snprintf(dd->boardname, namelen, "%s", n);

	snprintf(dd->boardversion, sizeof(dd->boardversion),
		 "ChipABI %u.%u, %s, InfiniPath%u %u.%u, SW Compat %u\n",
		 QIB_CHIP_VERS_MAJ, QIB_CHIP_VERS_MIN, dd->boardname,
		 (unsigned)SYM_FIELD(dd->revision, Revision_R, Arch),
		 dd->majrev, dd->minrev,
		 (unsigned)SYM_FIELD(dd->revision, Revision_R, SW));

	if (qib_singleport && (features >> PORT_SPD_CAP_SHIFT) & PORT_SPD_CAP) {
		qib_devinfo(dd->pcidev, "IB%u: Forced to single port mode"
			    " by module parameter\n", dd->unit);
		features &= PORT_SPD_CAP;
	}

	return features;
}

static int qib_do_7322_reset(struct qib_devdata *dd)
{
	u64 val;
	u64 *msix_vecsave;
	int i, msix_entries, ret = 1;
	u16 cmdval;
	u8 int_line, clinesz;
	unsigned long flags;

	
	qib_dev_err(dd, "Resetting InfiniPath unit %u\n", dd->unit);

	qib_pcie_getcmd(dd, &cmdval, &int_line, &clinesz);

	msix_entries = dd->cspec->num_msix_entries;

	
	qib_7322_set_intr_state(dd, 0);

	if (msix_entries) {
		qib_7322_nomsix(dd);
		
		msix_vecsave = kmalloc(2 * dd->cspec->num_msix_entries *
			sizeof(u64), GFP_KERNEL);
		if (!msix_vecsave)
			qib_dev_err(dd, "No mem to save MSIx data\n");
	} else
		msix_vecsave = NULL;

	for (i = 0; i < msix_entries; i++) {
		u64 vecaddr, vecdata;
		vecaddr = qib_read_kreg64(dd, 2 * i +
				  (QIB_7322_MsixTable_OFFS / sizeof(u64)));
		vecdata = qib_read_kreg64(dd, 1 + 2 * i +
				  (QIB_7322_MsixTable_OFFS / sizeof(u64)));
		if (msix_vecsave) {
			msix_vecsave[2 * i] = vecaddr;
			
			msix_vecsave[1 + 2 * i] = vecdata & ~0x100000000ULL;
		}
	}

	dd->pport->cpspec->ibdeltainprog = 0;
	dd->pport->cpspec->ibsymdelta = 0;
	dd->pport->cpspec->iblnkerrdelta = 0;
	dd->pport->cpspec->ibmalfdelta = 0;
	dd->int_counter = 0; 

	dd->flags &= ~(QIB_INITTED | QIB_PRESENT | QIB_BADINTR);
	dd->flags |= QIB_DOING_RESET;
	val = dd->control | QLOGIC_IB_C_RESET;
	writeq(val, &dd->kregbase[kr_control]);

	for (i = 1; i <= 5; i++) {
		msleep(1000 + (1 + i) * 3000);

		qib_pcie_reenable(dd, cmdval, int_line, clinesz);

		val = readq(&dd->kregbase[kr_revision]);
		if (val == dd->revision)
			break;
		if (i == 5) {
			qib_dev_err(dd, "Failed to initialize after reset, "
				    "unusable\n");
			ret = 0;
			goto  bail;
		}
	}

	dd->flags |= QIB_PRESENT; 

	if (msix_entries) {
		
		for (i = 0; i < msix_entries; i++) {
			dd->cspec->msix_entries[i].msix.entry = i;
			if (!msix_vecsave || !msix_vecsave[2 * i])
				continue;
			qib_write_kreg(dd, 2 * i +
				(QIB_7322_MsixTable_OFFS / sizeof(u64)),
				msix_vecsave[2 * i]);
			qib_write_kreg(dd, 1 + 2 * i +
				(QIB_7322_MsixTable_OFFS / sizeof(u64)),
				msix_vecsave[1 + 2 * i]);
		}
	}

	
	for (i = 0; i < dd->num_pports; ++i)
		write_7322_init_portregs(&dd->pport[i]);
	write_7322_initregs(dd);

	if (qib_pcie_params(dd, dd->lbus_width,
			    &dd->cspec->num_msix_entries,
			    dd->cspec->msix_entries))
		qib_dev_err(dd, "Reset failed to setup PCIe or interrupts; "
				"continuing anyway\n");

	qib_setup_7322_interrupt(dd, 1);

	for (i = 0; i < dd->num_pports; ++i) {
		struct qib_pportdata *ppd = &dd->pport[i];

		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags |= QIBL_IB_FORCE_NOTIFY;
		ppd->lflags &= ~QIBL_IB_AUTONEG_FAILED;
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	}

bail:
	dd->flags &= ~QIB_DOING_RESET; 
	kfree(msix_vecsave);
	return ret;
}

static void qib_7322_put_tid(struct qib_devdata *dd, u64 __iomem *tidptr,
			     u32 type, unsigned long pa)
{
	if (!(dd->flags & QIB_PRESENT))
		return;
	if (pa != dd->tidinvalid) {
		u64 chippa = pa >> IBA7322_TID_PA_SHIFT;

		
		if (pa != (chippa << IBA7322_TID_PA_SHIFT)) {
			qib_dev_err(dd, "Physaddr %lx not 2KB aligned!\n",
				    pa);
			return;
		}
		if (chippa >= (1UL << IBA7322_TID_SZ_SHIFT)) {
			qib_dev_err(dd, "Physical page address 0x%lx "
				"larger than supported\n", pa);
			return;
		}

		if (type == RCVHQ_RCV_TYPE_EAGER)
			chippa |= dd->tidtemplate;
		else 
			chippa |= IBA7322_TID_SZ_4K;
		pa = chippa;
	}
	writeq(pa, tidptr);
	mmiowb();
}

static void qib_7322_clear_tids(struct qib_devdata *dd,
				struct qib_ctxtdata *rcd)
{
	u64 __iomem *tidbase;
	unsigned long tidinv;
	u32 ctxt;
	int i;

	if (!dd->kregbase || !rcd)
		return;

	ctxt = rcd->ctxt;

	tidinv = dd->tidinvalid;
	tidbase = (u64 __iomem *)
		((char __iomem *) dd->kregbase +
		 dd->rcvtidbase +
		 ctxt * dd->rcvtidcnt * sizeof(*tidbase));

	for (i = 0; i < dd->rcvtidcnt; i++)
		qib_7322_put_tid(dd, &tidbase[i], RCVHQ_RCV_TYPE_EXPECTED,
				 tidinv);

	tidbase = (u64 __iomem *)
		((char __iomem *) dd->kregbase +
		 dd->rcvegrbase +
		 rcd->rcvegr_tid_base * sizeof(*tidbase));

	for (i = 0; i < rcd->rcvegrcnt; i++)
		qib_7322_put_tid(dd, &tidbase[i], RCVHQ_RCV_TYPE_EAGER,
				 tidinv);
}

static void qib_7322_tidtemplate(struct qib_devdata *dd)
{
	if (dd->rcvegrbufsize == 2048)
		dd->tidtemplate = IBA7322_TID_SZ_2K;
	else if (dd->rcvegrbufsize == 4096)
		dd->tidtemplate = IBA7322_TID_SZ_4K;
	dd->tidinvalid = 0;
}


static int qib_7322_get_base_info(struct qib_ctxtdata *rcd,
				  struct qib_base_info *kinfo)
{
	kinfo->spi_runtime_flags |= QIB_RUNTIME_CTXT_MSB_IN_QP |
		QIB_RUNTIME_PCIE | QIB_RUNTIME_NODMA_RTAIL |
		QIB_RUNTIME_HDRSUPP | QIB_RUNTIME_SDMA;
	if (rcd->dd->cspec->r1)
		kinfo->spi_runtime_flags |= QIB_RUNTIME_RCHK;
	if (rcd->dd->flags & QIB_USE_SPCL_TRIG)
		kinfo->spi_runtime_flags |= QIB_RUNTIME_SPECIAL_TRIGGER;

	return 0;
}

static struct qib_message_header *
qib_7322_get_msgheader(struct qib_devdata *dd, __le32 *rhf_addr)
{
	u32 offset = qib_hdrget_offset(rhf_addr);

	return (struct qib_message_header *)
		(rhf_addr - dd->rhf_offset + offset);
}

static void qib_7322_config_ctxts(struct qib_devdata *dd)
{
	unsigned long flags;
	u32 nchipctxts;

	nchipctxts = qib_read_kreg32(dd, kr_contextcnt);
	dd->cspec->numctxts = nchipctxts;
	if (qib_n_krcv_queues > 1 && dd->num_pports) {
		dd->first_user_ctxt = NUM_IB_PORTS +
			(qib_n_krcv_queues - 1) * dd->num_pports;
		if (dd->first_user_ctxt > nchipctxts)
			dd->first_user_ctxt = nchipctxts;
		dd->n_krcv_queues = dd->first_user_ctxt / dd->num_pports;
	} else {
		dd->first_user_ctxt = NUM_IB_PORTS;
		dd->n_krcv_queues = 1;
	}

	if (!qib_cfgctxts) {
		int nctxts = dd->first_user_ctxt + num_online_cpus();

		if (nctxts <= 6)
			dd->ctxtcnt = 6;
		else if (nctxts <= 10)
			dd->ctxtcnt = 10;
		else if (nctxts <= nchipctxts)
			dd->ctxtcnt = nchipctxts;
	} else if (qib_cfgctxts < dd->num_pports)
		dd->ctxtcnt = dd->num_pports;
	else if (qib_cfgctxts <= nchipctxts)
		dd->ctxtcnt = qib_cfgctxts;
	if (!dd->ctxtcnt) 
		dd->ctxtcnt = nchipctxts;

	spin_lock_irqsave(&dd->cspec->rcvmod_lock, flags);
	if (dd->ctxtcnt > 10)
		dd->rcvctrl |= 2ULL << SYM_LSB(RcvCtrl, ContextCfg);
	else if (dd->ctxtcnt > 6)
		dd->rcvctrl |= 1ULL << SYM_LSB(RcvCtrl, ContextCfg);
	

	
	dd->rcvctrl |= 5ULL << SYM_LSB(RcvCtrl, XrcTypeCode);

	/*
	 * RcvCtrl *must* be written here so that the
	 * chip understands how to change rcvegrcnt below.
	 */
	qib_write_kreg(dd, kr_rcvctrl, dd->rcvctrl);
	spin_unlock_irqrestore(&dd->cspec->rcvmod_lock, flags);

	
	dd->cspec->rcvegrcnt = qib_read_kreg32(dd, kr_rcvegrcnt);
	if (qib_rcvhdrcnt)
		dd->rcvhdrcnt = max(dd->cspec->rcvegrcnt, qib_rcvhdrcnt);
	else
		dd->rcvhdrcnt = 2 * max(dd->cspec->rcvegrcnt,
				    dd->num_pports > 1 ? 1024U : 2048U);
}

static int qib_7322_get_ib_cfg(struct qib_pportdata *ppd, int which)
{

	int lsb, ret = 0;
	u64 maskr; 

	switch (which) {

	case QIB_IB_CFG_LWID_ENB: 
		ret = ppd->link_width_enabled;
		goto done;

	case QIB_IB_CFG_LWID: 
		ret = ppd->link_width_active;
		goto done;

	case QIB_IB_CFG_SPD_ENB: 
		ret = ppd->link_speed_enabled;
		goto done;

	case QIB_IB_CFG_SPD: 
		ret = ppd->link_speed_active;
		goto done;

	case QIB_IB_CFG_RXPOL_ENB: 
		lsb = SYM_LSB(IBCCtrlB_0, IB_POLARITY_REV_SUPP);
		maskr = SYM_RMASK(IBCCtrlB_0, IB_POLARITY_REV_SUPP);
		break;

	case QIB_IB_CFG_LREV_ENB: 
		lsb = SYM_LSB(IBCCtrlB_0, IB_LANE_REV_SUPPORTED);
		maskr = SYM_RMASK(IBCCtrlB_0, IB_LANE_REV_SUPPORTED);
		break;

	case QIB_IB_CFG_LINKLATENCY:
		ret = qib_read_kreg_port(ppd, krp_ibcstatus_b) &
			SYM_MASK(IBCStatusB_0, LinkRoundTripLatency);
		goto done;

	case QIB_IB_CFG_OP_VLS:
		ret = ppd->vls_operational;
		goto done;

	case QIB_IB_CFG_VL_HIGH_CAP:
		ret = 16;
		goto done;

	case QIB_IB_CFG_VL_LOW_CAP:
		ret = 16;
		goto done;

	case QIB_IB_CFG_OVERRUN_THRESH: 
		ret = SYM_FIELD(ppd->cpspec->ibcctrl_a, IBCCtrlA_0,
				OverrunThreshold);
		goto done;

	case QIB_IB_CFG_PHYERR_THRESH: 
		ret = SYM_FIELD(ppd->cpspec->ibcctrl_a, IBCCtrlA_0,
				PhyerrThreshold);
		goto done;

	case QIB_IB_CFG_LINKDEFAULT: 
		
		ret = (ppd->cpspec->ibcctrl_a &
		       SYM_MASK(IBCCtrlA_0, LinkDownDefaultState)) ?
			IB_LINKINITCMD_SLEEP : IB_LINKINITCMD_POLL;
		goto done;

	case QIB_IB_CFG_HRTBT: 
		lsb = IBA7322_IBC_HRTBT_LSB;
		maskr = IBA7322_IBC_HRTBT_RMASK; 
		break;

	case QIB_IB_CFG_PMA_TICKS:
		if (ppd->link_speed_active == QIB_IB_QDR)
			ret = 3;
		else if (ppd->link_speed_active == QIB_IB_DDR)
			ret = 1;
		else
			ret = 0;
		goto done;

	default:
		ret = -EINVAL;
		goto done;
	}
	ret = (int)((ppd->cpspec->ibcctrl_b >> lsb) & maskr);
done:
	return ret;
}

#define IBA7322_IBC_DLIDLMC_SHIFT QIB_7322_IBCCtrlB_0_IB_DLID_LSB
#define IBA7322_IBC_DLIDLMC_MASK (QIB_7322_IBCCtrlB_0_IB_DLID_RMASK \
	| (QIB_7322_IBCCtrlB_0_IB_DLID_MASK_RMASK << 16))

static int qib_7322_set_ib_cfg(struct qib_pportdata *ppd, int which, u32 val)
{
	struct qib_devdata *dd = ppd->dd;
	u64 maskr; 
	int lsb, ret = 0;
	u16 lcmd, licmd;
	unsigned long flags;

	switch (which) {
	case QIB_IB_CFG_LIDLMC:
		lsb = IBA7322_IBC_DLIDLMC_SHIFT;
		maskr = IBA7322_IBC_DLIDLMC_MASK;
		qib_write_kreg_port(ppd, krp_sendslid,
				    val & (val >> 16) & SendIBSLIDAssignMask);
		qib_write_kreg_port(ppd, krp_sendslidmask,
				    (val >> 16) & SendIBSLMCMask);
		break;

	case QIB_IB_CFG_LWID_ENB: 
		ppd->link_width_enabled = val;
		
		if (val == IB_WIDTH_1X)
			val = 0;
		else if (val == IB_WIDTH_4X)
			val = 1;
		else
			val = 3;
		maskr = SYM_RMASK(IBCCtrlB_0, IB_NUM_CHANNELS);
		lsb = SYM_LSB(IBCCtrlB_0, IB_NUM_CHANNELS);
		break;

	case QIB_IB_CFG_SPD_ENB: 
		ppd->link_speed_enabled = val;
		val <<= IBA7322_IBC_SPEED_LSB;
		maskr = IBA7322_IBC_SPEED_MASK | IBA7322_IBC_IBTA_1_2_MASK |
			IBA7322_IBC_MAX_SPEED_MASK;
		if (val & (val - 1)) {
			
			val |= IBA7322_IBC_IBTA_1_2_MASK |
				IBA7322_IBC_MAX_SPEED_MASK;
			spin_lock_irqsave(&ppd->lflags_lock, flags);
			ppd->lflags &= ~QIBL_IB_AUTONEG_FAILED;
			spin_unlock_irqrestore(&ppd->lflags_lock, flags);
		} else if (val & IBA7322_IBC_SPEED_QDR)
			val |= IBA7322_IBC_IBTA_1_2_MASK;
		
		lsb = SYM_LSB(IBCCtrlB_0, IB_ENHANCED_MODE);
		break;

	case QIB_IB_CFG_RXPOL_ENB: 
		lsb = SYM_LSB(IBCCtrlB_0, IB_POLARITY_REV_SUPP);
		maskr = SYM_RMASK(IBCCtrlB_0, IB_POLARITY_REV_SUPP);
		break;

	case QIB_IB_CFG_LREV_ENB: 
		lsb = SYM_LSB(IBCCtrlB_0, IB_LANE_REV_SUPPORTED);
		maskr = SYM_RMASK(IBCCtrlB_0, IB_LANE_REV_SUPPORTED);
		break;

	case QIB_IB_CFG_OVERRUN_THRESH: 
		maskr = SYM_FIELD(ppd->cpspec->ibcctrl_a, IBCCtrlA_0,
				  OverrunThreshold);
		if (maskr != val) {
			ppd->cpspec->ibcctrl_a &=
				~SYM_MASK(IBCCtrlA_0, OverrunThreshold);
			ppd->cpspec->ibcctrl_a |= (u64) val <<
				SYM_LSB(IBCCtrlA_0, OverrunThreshold);
			qib_write_kreg_port(ppd, krp_ibcctrl_a,
					    ppd->cpspec->ibcctrl_a);
			qib_write_kreg(dd, kr_scratch, 0ULL);
		}
		goto bail;

	case QIB_IB_CFG_PHYERR_THRESH: 
		maskr = SYM_FIELD(ppd->cpspec->ibcctrl_a, IBCCtrlA_0,
				  PhyerrThreshold);
		if (maskr != val) {
			ppd->cpspec->ibcctrl_a &=
				~SYM_MASK(IBCCtrlA_0, PhyerrThreshold);
			ppd->cpspec->ibcctrl_a |= (u64) val <<
				SYM_LSB(IBCCtrlA_0, PhyerrThreshold);
			qib_write_kreg_port(ppd, krp_ibcctrl_a,
					    ppd->cpspec->ibcctrl_a);
			qib_write_kreg(dd, kr_scratch, 0ULL);
		}
		goto bail;

	case QIB_IB_CFG_PKEYS: 
		maskr = (u64) ppd->pkeys[0] | ((u64) ppd->pkeys[1] << 16) |
			((u64) ppd->pkeys[2] << 32) |
			((u64) ppd->pkeys[3] << 48);
		qib_write_kreg_port(ppd, krp_partitionkey, maskr);
		goto bail;

	case QIB_IB_CFG_LINKDEFAULT: 
		
		if (val == IB_LINKINITCMD_POLL)
			ppd->cpspec->ibcctrl_a &=
				~SYM_MASK(IBCCtrlA_0, LinkDownDefaultState);
		else 
			ppd->cpspec->ibcctrl_a |=
				SYM_MASK(IBCCtrlA_0, LinkDownDefaultState);
		qib_write_kreg_port(ppd, krp_ibcctrl_a, ppd->cpspec->ibcctrl_a);
		qib_write_kreg(dd, kr_scratch, 0ULL);
		goto bail;

	case QIB_IB_CFG_MTU: 
		val = (ppd->ibmaxlen >> 2) + 1;
		ppd->cpspec->ibcctrl_a &= ~SYM_MASK(IBCCtrlA_0, MaxPktLen);
		ppd->cpspec->ibcctrl_a |= (u64)val <<
			SYM_LSB(IBCCtrlA_0, MaxPktLen);
		qib_write_kreg_port(ppd, krp_ibcctrl_a,
				    ppd->cpspec->ibcctrl_a);
		qib_write_kreg(dd, kr_scratch, 0ULL);
		goto bail;

	case QIB_IB_CFG_LSTATE: 
		switch (val & 0xffff0000) {
		case IB_LINKCMD_DOWN:
			lcmd = QLOGIC_IB_IBCC_LINKCMD_DOWN;
			ppd->cpspec->ibmalfusesnap = 1;
			ppd->cpspec->ibmalfsnap = read_7322_creg32_port(ppd,
				crp_errlink);
			if (!ppd->cpspec->ibdeltainprog &&
			    qib_compat_ddr_negotiate) {
				ppd->cpspec->ibdeltainprog = 1;
				ppd->cpspec->ibsymsnap =
					read_7322_creg32_port(ppd,
							      crp_ibsymbolerr);
				ppd->cpspec->iblnkerrsnap =
					read_7322_creg32_port(ppd,
						      crp_iblinkerrrecov);
			}
			break;

		case IB_LINKCMD_ARMED:
			lcmd = QLOGIC_IB_IBCC_LINKCMD_ARMED;
			if (ppd->cpspec->ibmalfusesnap) {
				ppd->cpspec->ibmalfusesnap = 0;
				ppd->cpspec->ibmalfdelta +=
					read_7322_creg32_port(ppd,
							      crp_errlink) -
					ppd->cpspec->ibmalfsnap;
			}
			break;

		case IB_LINKCMD_ACTIVE:
			lcmd = QLOGIC_IB_IBCC_LINKCMD_ACTIVE;
			break;

		default:
			ret = -EINVAL;
			qib_dev_err(dd, "bad linkcmd req 0x%x\n", val >> 16);
			goto bail;
		}
		switch (val & 0xffff) {
		case IB_LINKINITCMD_NOP:
			licmd = 0;
			break;

		case IB_LINKINITCMD_POLL:
			licmd = QLOGIC_IB_IBCC_LINKINITCMD_POLL;
			break;

		case IB_LINKINITCMD_SLEEP:
			licmd = QLOGIC_IB_IBCC_LINKINITCMD_SLEEP;
			break;

		case IB_LINKINITCMD_DISABLE:
			licmd = QLOGIC_IB_IBCC_LINKINITCMD_DISABLE;
			ppd->cpspec->chase_end = 0;
			if (ppd->cpspec->chase_timer.expires) {
				del_timer_sync(&ppd->cpspec->chase_timer);
				ppd->cpspec->chase_timer.expires = 0;
			}
			break;

		default:
			ret = -EINVAL;
			qib_dev_err(dd, "bad linkinitcmd req 0x%x\n",
				    val & 0xffff);
			goto bail;
		}
		qib_set_ib_7322_lstate(ppd, lcmd, licmd);
		goto bail;

	case QIB_IB_CFG_OP_VLS:
		if (ppd->vls_operational != val) {
			ppd->vls_operational = val;
			set_vls(ppd);
		}
		goto bail;

	case QIB_IB_CFG_VL_HIGH_LIMIT:
		qib_write_kreg_port(ppd, krp_highprio_limit, val);
		goto bail;

	case QIB_IB_CFG_HRTBT: 
		if (val > 3) {
			ret = -EINVAL;
			goto bail;
		}
		lsb = IBA7322_IBC_HRTBT_LSB;
		maskr = IBA7322_IBC_HRTBT_RMASK; 
		break;

	case QIB_IB_CFG_PORT:
		
		if (ppd->dd->cspec->r1) {
			cancel_delayed_work(&ppd->cpspec->ipg_work);
			ppd->cpspec->ipg_tries = 0;
		}
		goto bail;

	default:
		ret = -EINVAL;
		goto bail;
	}
	ppd->cpspec->ibcctrl_b &= ~(maskr << lsb);
	ppd->cpspec->ibcctrl_b |= (((u64) val & maskr) << lsb);
	qib_write_kreg_port(ppd, krp_ibcctrl_b, ppd->cpspec->ibcctrl_b);
	qib_write_kreg(dd, kr_scratch, 0);
bail:
	return ret;
}

static int qib_7322_set_loopback(struct qib_pportdata *ppd, const char *what)
{
	int ret = 0;
	u64 val, ctrlb;

	
	if (!strncmp(what, "ibc", 3)) {
		ppd->cpspec->ibcctrl_a |= SYM_MASK(IBCCtrlA_0,
						       Loopback);
		val = 0; 
		qib_devinfo(ppd->dd->pcidev, "Enabling IB%u:%u IBC loopback\n",
			 ppd->dd->unit, ppd->port);
	} else if (!strncmp(what, "off", 3)) {
		ppd->cpspec->ibcctrl_a &= ~SYM_MASK(IBCCtrlA_0,
							Loopback);
		
		val = IBA7322_IBC_HRTBT_RMASK << IBA7322_IBC_HRTBT_LSB;
		qib_devinfo(ppd->dd->pcidev, "Disabling IB%u:%u IBC loopback "
			    "(normal)\n", ppd->dd->unit, ppd->port);
	} else
		ret = -EINVAL;
	if (!ret) {
		qib_write_kreg_port(ppd, krp_ibcctrl_a,
				    ppd->cpspec->ibcctrl_a);
		ctrlb = ppd->cpspec->ibcctrl_b & ~(IBA7322_IBC_HRTBT_MASK
					     << IBA7322_IBC_HRTBT_LSB);
		ppd->cpspec->ibcctrl_b = ctrlb | val;
		qib_write_kreg_port(ppd, krp_ibcctrl_b,
				    ppd->cpspec->ibcctrl_b);
		qib_write_kreg(ppd->dd, kr_scratch, 0);
	}
	return ret;
}

static void get_vl_weights(struct qib_pportdata *ppd, unsigned regno,
			   struct ib_vl_weight_elem *vl)
{
	unsigned i;

	for (i = 0; i < 16; i++, regno++, vl++) {
		u32 val = qib_read_kreg_port(ppd, regno);

		vl->vl = (val >> SYM_LSB(LowPriority0_0, VirtualLane)) &
			SYM_RMASK(LowPriority0_0, VirtualLane);
		vl->weight = (val >> SYM_LSB(LowPriority0_0, Weight)) &
			SYM_RMASK(LowPriority0_0, Weight);
	}
}

static void set_vl_weights(struct qib_pportdata *ppd, unsigned regno,
			   struct ib_vl_weight_elem *vl)
{
	unsigned i;

	for (i = 0; i < 16; i++, regno++, vl++) {
		u64 val;

		val = ((vl->vl & SYM_RMASK(LowPriority0_0, VirtualLane)) <<
			SYM_LSB(LowPriority0_0, VirtualLane)) |
		      ((vl->weight & SYM_RMASK(LowPriority0_0, Weight)) <<
			SYM_LSB(LowPriority0_0, Weight));
		qib_write_kreg_port(ppd, regno, val);
	}
	if (!(ppd->p_sendctrl & SYM_MASK(SendCtrl_0, IBVLArbiterEn))) {
		struct qib_devdata *dd = ppd->dd;
		unsigned long flags;

		spin_lock_irqsave(&dd->sendctrl_lock, flags);
		ppd->p_sendctrl |= SYM_MASK(SendCtrl_0, IBVLArbiterEn);
		qib_write_kreg_port(ppd, krp_sendctrl, ppd->p_sendctrl);
		qib_write_kreg(dd, kr_scratch, 0);
		spin_unlock_irqrestore(&dd->sendctrl_lock, flags);
	}
}

static int qib_7322_get_ib_table(struct qib_pportdata *ppd, int which, void *t)
{
	switch (which) {
	case QIB_IB_TBL_VL_HIGH_ARB:
		get_vl_weights(ppd, krp_highprio_0, t);
		break;

	case QIB_IB_TBL_VL_LOW_ARB:
		get_vl_weights(ppd, krp_lowprio_0, t);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int qib_7322_set_ib_table(struct qib_pportdata *ppd, int which, void *t)
{
	switch (which) {
	case QIB_IB_TBL_VL_HIGH_ARB:
		set_vl_weights(ppd, krp_highprio_0, t);
		break;

	case QIB_IB_TBL_VL_LOW_ARB:
		set_vl_weights(ppd, krp_lowprio_0, t);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static void qib_update_7322_usrhead(struct qib_ctxtdata *rcd, u64 hd,
				    u32 updegr, u32 egrhd, u32 npkts)
{
	if (hd >> IBA7322_HDRHEAD_PKTINT_SHIFT)
		adjust_rcv_timeout(rcd, npkts);
	if (updegr)
		qib_write_ureg(rcd->dd, ur_rcvegrindexhead, egrhd, rcd->ctxt);
	mmiowb();
	qib_write_ureg(rcd->dd, ur_rcvhdrhead, hd, rcd->ctxt);
	qib_write_ureg(rcd->dd, ur_rcvhdrhead, hd, rcd->ctxt);
	mmiowb();
}

static u32 qib_7322_hdrqempty(struct qib_ctxtdata *rcd)
{
	u32 head, tail;

	head = qib_read_ureg32(rcd->dd, ur_rcvhdrhead, rcd->ctxt);
	if (rcd->rcvhdrtail_kvaddr)
		tail = qib_get_rcvhdrtail(rcd);
	else
		tail = qib_read_ureg32(rcd->dd, ur_rcvhdrtail, rcd->ctxt);
	return head == tail;
}

#define RCVCTRL_COMMON_MODS (QIB_RCVCTRL_CTXT_ENB | \
	QIB_RCVCTRL_CTXT_DIS | \
	QIB_RCVCTRL_TIDFLOW_ENB | \
	QIB_RCVCTRL_TIDFLOW_DIS | \
	QIB_RCVCTRL_TAILUPD_ENB | \
	QIB_RCVCTRL_TAILUPD_DIS | \
	QIB_RCVCTRL_INTRAVAIL_ENB | \
	QIB_RCVCTRL_INTRAVAIL_DIS | \
	QIB_RCVCTRL_BP_ENB | \
	QIB_RCVCTRL_BP_DIS)

#define RCVCTRL_PORT_MODS (QIB_RCVCTRL_CTXT_ENB | \
	QIB_RCVCTRL_CTXT_DIS | \
	QIB_RCVCTRL_PKEY_DIS | \
	QIB_RCVCTRL_PKEY_ENB)

static void rcvctrl_7322_mod(struct qib_pportdata *ppd, unsigned int op,
			     int ctxt)
{
	struct qib_devdata *dd = ppd->dd;
	struct qib_ctxtdata *rcd;
	u64 mask, val;
	unsigned long flags;

	spin_lock_irqsave(&dd->cspec->rcvmod_lock, flags);

	if (op & QIB_RCVCTRL_TIDFLOW_ENB)
		dd->rcvctrl |= SYM_MASK(RcvCtrl, TidFlowEnable);
	if (op & QIB_RCVCTRL_TIDFLOW_DIS)
		dd->rcvctrl &= ~SYM_MASK(RcvCtrl, TidFlowEnable);
	if (op & QIB_RCVCTRL_TAILUPD_ENB)
		dd->rcvctrl |= SYM_MASK(RcvCtrl, TailUpd);
	if (op & QIB_RCVCTRL_TAILUPD_DIS)
		dd->rcvctrl &= ~SYM_MASK(RcvCtrl, TailUpd);
	if (op & QIB_RCVCTRL_PKEY_ENB)
		ppd->p_rcvctrl &= ~SYM_MASK(RcvCtrl_0, RcvPartitionKeyDisable);
	if (op & QIB_RCVCTRL_PKEY_DIS)
		ppd->p_rcvctrl |= SYM_MASK(RcvCtrl_0, RcvPartitionKeyDisable);
	if (ctxt < 0) {
		mask = (1ULL << dd->ctxtcnt) - 1;
		rcd = NULL;
	} else {
		mask = (1ULL << ctxt);
		rcd = dd->rcd[ctxt];
	}
	if ((op & QIB_RCVCTRL_CTXT_ENB) && rcd) {
		ppd->p_rcvctrl |=
			(mask << SYM_LSB(RcvCtrl_0, ContextEnableKernel));
		if (!(dd->flags & QIB_NODMA_RTAIL)) {
			op |= QIB_RCVCTRL_TAILUPD_ENB; 
			dd->rcvctrl |= SYM_MASK(RcvCtrl, TailUpd);
		}
		
		qib_write_kreg_ctxt(dd, krc_rcvhdrtailaddr, ctxt,
				    rcd->rcvhdrqtailaddr_phys);
		qib_write_kreg_ctxt(dd, krc_rcvhdraddr, ctxt,
				    rcd->rcvhdrq_phys);
		rcd->seq_cnt = 1;
	}
	if (op & QIB_RCVCTRL_CTXT_DIS)
		ppd->p_rcvctrl &=
			~(mask << SYM_LSB(RcvCtrl_0, ContextEnableKernel));
	if (op & QIB_RCVCTRL_BP_ENB)
		dd->rcvctrl |= mask << SYM_LSB(RcvCtrl, dontDropRHQFull);
	if (op & QIB_RCVCTRL_BP_DIS)
		dd->rcvctrl &= ~(mask << SYM_LSB(RcvCtrl, dontDropRHQFull));
	if (op & QIB_RCVCTRL_INTRAVAIL_ENB)
		dd->rcvctrl |= (mask << SYM_LSB(RcvCtrl, IntrAvail));
	if (op & QIB_RCVCTRL_INTRAVAIL_DIS)
		dd->rcvctrl &= ~(mask << SYM_LSB(RcvCtrl, IntrAvail));
	if (op == 0 || (op & RCVCTRL_COMMON_MODS))
		qib_write_kreg(dd, kr_rcvctrl, dd->rcvctrl);
	if (op == 0 || (op & RCVCTRL_PORT_MODS))
		qib_write_kreg_port(ppd, krp_rcvctrl, ppd->p_rcvctrl);
	if ((op & QIB_RCVCTRL_CTXT_ENB) && dd->rcd[ctxt]) {
		val = qib_read_ureg32(dd, ur_rcvegrindextail, ctxt);
		qib_write_ureg(dd, ur_rcvegrindexhead, val, ctxt);

		
		(void) qib_read_kreg32(dd, kr_scratch);
		val = qib_read_ureg32(dd, ur_rcvhdrtail, ctxt);
		dd->rcd[ctxt]->head = val;
		
		if (ctxt < dd->first_user_ctxt)
			val |= dd->rhdrhead_intr_off;
		qib_write_ureg(dd, ur_rcvhdrhead, val, ctxt);
	} else if ((op & QIB_RCVCTRL_INTRAVAIL_ENB) &&
		dd->rcd[ctxt] && dd->rhdrhead_intr_off) {
		
		val = dd->rcd[ctxt]->head | dd->rhdrhead_intr_off;
		qib_write_ureg(dd, ur_rcvhdrhead, val, ctxt);
	}
	if (op & QIB_RCVCTRL_CTXT_DIS) {
		unsigned f;

		
		if (ctxt >= 0) {
			qib_write_kreg_ctxt(dd, krc_rcvhdrtailaddr, ctxt, 0);
			qib_write_kreg_ctxt(dd, krc_rcvhdraddr, ctxt, 0);
			for (f = 0; f < NUM_TIDFLOWS_CTXT; f++)
				qib_write_ureg(dd, ur_rcvflowtable + f,
					       TIDFLOW_ERRBITS, ctxt);
		} else {
			unsigned i;

			for (i = 0; i < dd->cfgctxts; i++) {
				qib_write_kreg_ctxt(dd, krc_rcvhdrtailaddr,
						    i, 0);
				qib_write_kreg_ctxt(dd, krc_rcvhdraddr, i, 0);
				for (f = 0; f < NUM_TIDFLOWS_CTXT; f++)
					qib_write_ureg(dd, ur_rcvflowtable + f,
						       TIDFLOW_ERRBITS, i);
			}
		}
	}
	spin_unlock_irqrestore(&dd->cspec->rcvmod_lock, flags);
}

/*
 * Modify the SENDCTRL register in chip-specific way. This
 * is a function where there are multiple such registers with
 * slightly different layouts.
 * The chip doesn't allow back-to-back sendctrl writes, so write
 * the scratch register after writing sendctrl.
 *
 * Which register is written depends on the operation.
 * Most operate on the common register, while
 * SEND_ENB and SEND_DIS operate on the per-port ones.
 * SEND_ENB is included in common because it can change SPCL_TRIG
 */
#define SENDCTRL_COMMON_MODS (\
	QIB_SENDCTRL_CLEAR | \
	QIB_SENDCTRL_AVAIL_DIS | \
	QIB_SENDCTRL_AVAIL_ENB | \
	QIB_SENDCTRL_AVAIL_BLIP | \
	QIB_SENDCTRL_DISARM | \
	QIB_SENDCTRL_DISARM_ALL | \
	QIB_SENDCTRL_SEND_ENB)

#define SENDCTRL_PORT_MODS (\
	QIB_SENDCTRL_CLEAR | \
	QIB_SENDCTRL_SEND_ENB | \
	QIB_SENDCTRL_SEND_DIS | \
	QIB_SENDCTRL_FLUSH)

static void sendctrl_7322_mod(struct qib_pportdata *ppd, u32 op)
{
	struct qib_devdata *dd = ppd->dd;
	u64 tmp_dd_sendctrl;
	unsigned long flags;

	spin_lock_irqsave(&dd->sendctrl_lock, flags);

	
	if (op & QIB_SENDCTRL_CLEAR)
		dd->sendctrl = 0;
	if (op & QIB_SENDCTRL_AVAIL_DIS)
		dd->sendctrl &= ~SYM_MASK(SendCtrl, SendBufAvailUpd);
	else if (op & QIB_SENDCTRL_AVAIL_ENB) {
		dd->sendctrl |= SYM_MASK(SendCtrl, SendBufAvailUpd);
		if (dd->flags & QIB_USE_SPCL_TRIG)
			dd->sendctrl |= SYM_MASK(SendCtrl, SpecialTriggerEn);
	}

	
	if (op & QIB_SENDCTRL_SEND_DIS)
		ppd->p_sendctrl &= ~SYM_MASK(SendCtrl_0, SendEnable);
	else if (op & QIB_SENDCTRL_SEND_ENB)
		ppd->p_sendctrl |= SYM_MASK(SendCtrl_0, SendEnable);

	if (op & QIB_SENDCTRL_DISARM_ALL) {
		u32 i, last;

		tmp_dd_sendctrl = dd->sendctrl;
		last = dd->piobcnt2k + dd->piobcnt4k + NUM_VL15_BUFS;
		tmp_dd_sendctrl &= ~SYM_MASK(SendCtrl, SendBufAvailUpd);
		for (i = 0; i < last; i++) {
			qib_write_kreg(dd, kr_sendctrl,
				       tmp_dd_sendctrl |
				       SYM_MASK(SendCtrl, Disarm) | i);
			qib_write_kreg(dd, kr_scratch, 0);
		}
	}

	if (op & QIB_SENDCTRL_FLUSH) {
		u64 tmp_ppd_sendctrl = ppd->p_sendctrl;

		tmp_ppd_sendctrl |=
			SYM_MASK(SendCtrl_0, TxeDrainRmFifo) |
			SYM_MASK(SendCtrl_0, TxeDrainLaFifo) |
			SYM_MASK(SendCtrl_0, TxeBypassIbc);
		qib_write_kreg_port(ppd, krp_sendctrl, tmp_ppd_sendctrl);
		qib_write_kreg(dd, kr_scratch, 0);
	}

	tmp_dd_sendctrl = dd->sendctrl;

	if (op & QIB_SENDCTRL_DISARM)
		tmp_dd_sendctrl |= SYM_MASK(SendCtrl, Disarm) |
			((op & QIB_7322_SendCtrl_DisarmSendBuf_RMASK) <<
			 SYM_LSB(SendCtrl, DisarmSendBuf));
	if ((op & QIB_SENDCTRL_AVAIL_BLIP) &&
	    (dd->sendctrl & SYM_MASK(SendCtrl, SendBufAvailUpd)))
		tmp_dd_sendctrl &= ~SYM_MASK(SendCtrl, SendBufAvailUpd);

	if (op == 0 || (op & SENDCTRL_COMMON_MODS)) {
		qib_write_kreg(dd, kr_sendctrl, tmp_dd_sendctrl);
		qib_write_kreg(dd, kr_scratch, 0);
	}

	if (op == 0 || (op & SENDCTRL_PORT_MODS)) {
		qib_write_kreg_port(ppd, krp_sendctrl, ppd->p_sendctrl);
		qib_write_kreg(dd, kr_scratch, 0);
	}

	if (op & QIB_SENDCTRL_AVAIL_BLIP) {
		qib_write_kreg(dd, kr_sendctrl, dd->sendctrl);
		qib_write_kreg(dd, kr_scratch, 0);
	}

	spin_unlock_irqrestore(&dd->sendctrl_lock, flags);

	if (op & QIB_SENDCTRL_FLUSH) {
		u32 v;
		v = qib_read_kreg32(dd, kr_scratch);
		qib_write_kreg(dd, kr_scratch, v);
		v = qib_read_kreg32(dd, kr_scratch);
		qib_write_kreg(dd, kr_scratch, v);
		qib_read_kreg32(dd, kr_scratch);
	}
}

#define _PORT_VIRT_FLAG 0x8000U 
#define _PORT_64BIT_FLAG 0x10000U 
#define _PORT_CNTR_IDXMASK 0x7fffU 

static u64 qib_portcntr_7322(struct qib_pportdata *ppd, u32 reg)
{
	struct qib_devdata *dd = ppd->dd;
	u64 ret = 0ULL;
	u16 creg;
	
	static const u32 xlator[] = {
		[QIBPORTCNTR_PKTSEND] = crp_pktsend | _PORT_64BIT_FLAG,
		[QIBPORTCNTR_WORDSEND] = crp_wordsend | _PORT_64BIT_FLAG,
		[QIBPORTCNTR_PSXMITDATA] = crp_psxmitdatacount,
		[QIBPORTCNTR_PSXMITPKTS] = crp_psxmitpktscount,
		[QIBPORTCNTR_PSXMITWAIT] = crp_psxmitwaitcount,
		[QIBPORTCNTR_SENDSTALL] = crp_sendstall,
		[QIBPORTCNTR_PKTRCV] = crp_pktrcv | _PORT_64BIT_FLAG,
		[QIBPORTCNTR_PSRCVDATA] = crp_psrcvdatacount,
		[QIBPORTCNTR_PSRCVPKTS] = crp_psrcvpktscount,
		[QIBPORTCNTR_RCVEBP] = crp_rcvebp,
		[QIBPORTCNTR_RCVOVFL] = crp_rcvovfl,
		[QIBPORTCNTR_WORDRCV] = crp_wordrcv | _PORT_64BIT_FLAG,
		[QIBPORTCNTR_RXDROPPKT] = 0xffff, 
		[QIBPORTCNTR_RXLOCALPHYERR] = crp_rxotherlocalphyerr,
		[QIBPORTCNTR_RXVLERR] = crp_rxvlerr,
		[QIBPORTCNTR_ERRICRC] = crp_erricrc,
		[QIBPORTCNTR_ERRVCRC] = crp_errvcrc,
		[QIBPORTCNTR_ERRLPCRC] = crp_errlpcrc,
		[QIBPORTCNTR_BADFORMAT] = crp_badformat,
		[QIBPORTCNTR_ERR_RLEN] = crp_err_rlen,
		[QIBPORTCNTR_IBSYMBOLERR] = crp_ibsymbolerr,
		[QIBPORTCNTR_INVALIDRLEN] = crp_invalidrlen,
		[QIBPORTCNTR_UNSUPVL] = crp_txunsupvl,
		[QIBPORTCNTR_EXCESSBUFOVFL] = crp_excessbufferovfl,
		[QIBPORTCNTR_ERRLINK] = crp_errlink,
		[QIBPORTCNTR_IBLINKDOWN] = crp_iblinkdown,
		[QIBPORTCNTR_IBLINKERRRECOV] = crp_iblinkerrrecov,
		[QIBPORTCNTR_LLI] = crp_locallinkintegrityerr,
		[QIBPORTCNTR_VL15PKTDROP] = crp_vl15droppedpkt,
		[QIBPORTCNTR_ERRPKEY] = crp_errpkey,
		[QIBPORTCNTR_PSINTERVAL] = krp_psinterval,
		[QIBPORTCNTR_PSSTART] = krp_psstart,
		[QIBPORTCNTR_PSSTAT] = krp_psstat,
		
		[QIBPORTCNTR_KHDROVFL] = 0xffff,
	};

	if (reg >= ARRAY_SIZE(xlator)) {
		qib_devinfo(ppd->dd->pcidev,
			 "Unimplemented portcounter %u\n", reg);
		goto done;
	}
	creg = xlator[reg] & _PORT_CNTR_IDXMASK;

	
	if (reg == QIBPORTCNTR_KHDROVFL) {
		int i;

		
		for (i = 0; dd->rcd && i < dd->first_user_ctxt; i++) {
			struct qib_ctxtdata *rcd = dd->rcd[i];

			if (!rcd || rcd->ppd != ppd)
				continue;
			ret += read_7322_creg32(dd, cr_base_egrovfl + i);
		}
		goto done;
	} else if (reg == QIBPORTCNTR_RXDROPPKT) {
		goto done;
	} else if (reg == QIBPORTCNTR_PSINTERVAL ||
		   reg == QIBPORTCNTR_PSSTART || reg == QIBPORTCNTR_PSSTAT) {
		
		ret = qib_read_kreg_port(ppd, creg);
		goto done;
	}

	if (xlator[reg] & _PORT_64BIT_FLAG)
		ret = read_7322_creg_port(ppd, creg);
	else
		ret = read_7322_creg32_port(ppd, creg);
	if (creg == crp_ibsymbolerr) {
		if (ppd->cpspec->ibdeltainprog)
			ret -= ret - ppd->cpspec->ibsymsnap;
		ret -= ppd->cpspec->ibsymdelta;
	} else if (creg == crp_iblinkerrrecov) {
		if (ppd->cpspec->ibdeltainprog)
			ret -= ret - ppd->cpspec->iblnkerrsnap;
		ret -= ppd->cpspec->iblnkerrdelta;
	} else if (creg == crp_errlink)
		ret -= ppd->cpspec->ibmalfdelta;
	else if (creg == crp_iblinkdown)
		ret += ppd->cpspec->iblnkdowndelta;
done:
	return ret;
}

static const char cntr7322names[] =
	"Interrupts\n"
	"HostBusStall\n"
	"E RxTIDFull\n"
	"RxTIDInvalid\n"
	"RxTIDFloDrop\n" 
	"Ctxt0EgrOvfl\n"
	"Ctxt1EgrOvfl\n"
	"Ctxt2EgrOvfl\n"
	"Ctxt3EgrOvfl\n"
	"Ctxt4EgrOvfl\n"
	"Ctxt5EgrOvfl\n"
	"Ctxt6EgrOvfl\n"
	"Ctxt7EgrOvfl\n"
	"Ctxt8EgrOvfl\n"
	"Ctxt9EgrOvfl\n"
	"Ctx10EgrOvfl\n"
	"Ctx11EgrOvfl\n"
	"Ctx12EgrOvfl\n"
	"Ctx13EgrOvfl\n"
	"Ctx14EgrOvfl\n"
	"Ctx15EgrOvfl\n"
	"Ctx16EgrOvfl\n"
	"Ctx17EgrOvfl\n"
	;

static const u32 cntr7322indices[] = {
	cr_lbint | _PORT_64BIT_FLAG,
	cr_lbstall | _PORT_64BIT_FLAG,
	cr_tidfull,
	cr_tidinvalid,
	cr_rxtidflowdrop,
	cr_base_egrovfl + 0,
	cr_base_egrovfl + 1,
	cr_base_egrovfl + 2,
	cr_base_egrovfl + 3,
	cr_base_egrovfl + 4,
	cr_base_egrovfl + 5,
	cr_base_egrovfl + 6,
	cr_base_egrovfl + 7,
	cr_base_egrovfl + 8,
	cr_base_egrovfl + 9,
	cr_base_egrovfl + 10,
	cr_base_egrovfl + 11,
	cr_base_egrovfl + 12,
	cr_base_egrovfl + 13,
	cr_base_egrovfl + 14,
	cr_base_egrovfl + 15,
	cr_base_egrovfl + 16,
	cr_base_egrovfl + 17,
};

static const char portcntr7322names[] =
	"TxPkt\n"
	"TxFlowPkt\n"
	"TxWords\n"
	"RxPkt\n"
	"RxFlowPkt\n"
	"RxWords\n"
	"TxFlowStall\n"
	"TxDmaDesc\n"  
	"E RxDlidFltr\n"  
	"IBStatusChng\n"
	"IBLinkDown\n"
	"IBLnkRecov\n"
	"IBRxLinkErr\n"
	"IBSymbolErr\n"
	"RxLLIErr\n"
	"RxBadFormat\n"
	"RxBadLen\n"
	"RxBufOvrfl\n"
	"RxEBP\n"
	"RxFlowCtlErr\n"
	"RxICRCerr\n"
	"RxLPCRCerr\n"
	"RxVCRCerr\n"
	"RxInvalLen\n"
	"RxInvalPKey\n"
	"RxPktDropped\n"
	"TxBadLength\n"
	"TxDropped\n"
	"TxInvalLen\n"
	"TxUnderrun\n"
	"TxUnsupVL\n"
	"RxLclPhyErr\n" 
	"RxVL15Drop\n"
	"RxVlErr\n"
	"XcessBufOvfl\n"
	"RxQPBadCtxt\n" 
	"TXBadHeader\n"
	;

static const u32 portcntr7322indices[] = {
	QIBPORTCNTR_PKTSEND | _PORT_VIRT_FLAG,
	crp_pktsendflow,
	QIBPORTCNTR_WORDSEND | _PORT_VIRT_FLAG,
	QIBPORTCNTR_PKTRCV | _PORT_VIRT_FLAG,
	crp_pktrcvflowctrl,
	QIBPORTCNTR_WORDRCV | _PORT_VIRT_FLAG,
	QIBPORTCNTR_SENDSTALL | _PORT_VIRT_FLAG,
	crp_txsdmadesc | _PORT_64BIT_FLAG,
	crp_rxdlidfltr,
	crp_ibstatuschange,
	QIBPORTCNTR_IBLINKDOWN | _PORT_VIRT_FLAG,
	QIBPORTCNTR_IBLINKERRRECOV | _PORT_VIRT_FLAG,
	QIBPORTCNTR_ERRLINK | _PORT_VIRT_FLAG,
	QIBPORTCNTR_IBSYMBOLERR | _PORT_VIRT_FLAG,
	QIBPORTCNTR_LLI | _PORT_VIRT_FLAG,
	QIBPORTCNTR_BADFORMAT | _PORT_VIRT_FLAG,
	QIBPORTCNTR_ERR_RLEN | _PORT_VIRT_FLAG,
	QIBPORTCNTR_RCVOVFL | _PORT_VIRT_FLAG,
	QIBPORTCNTR_RCVEBP | _PORT_VIRT_FLAG,
	crp_rcvflowctrlviol,
	QIBPORTCNTR_ERRICRC | _PORT_VIRT_FLAG,
	QIBPORTCNTR_ERRLPCRC | _PORT_VIRT_FLAG,
	QIBPORTCNTR_ERRVCRC | _PORT_VIRT_FLAG,
	QIBPORTCNTR_INVALIDRLEN | _PORT_VIRT_FLAG,
	QIBPORTCNTR_ERRPKEY | _PORT_VIRT_FLAG,
	QIBPORTCNTR_RXDROPPKT | _PORT_VIRT_FLAG,
	crp_txminmaxlenerr,
	crp_txdroppedpkt,
	crp_txlenerr,
	crp_txunderrun,
	crp_txunsupvl,
	QIBPORTCNTR_RXLOCALPHYERR | _PORT_VIRT_FLAG,
	QIBPORTCNTR_VL15PKTDROP | _PORT_VIRT_FLAG,
	QIBPORTCNTR_RXVLERR | _PORT_VIRT_FLAG,
	QIBPORTCNTR_EXCESSBUFOVFL | _PORT_VIRT_FLAG,
	crp_rxqpinvalidctxt,
	crp_txhdrerr,
};

static void init_7322_cntrnames(struct qib_devdata *dd)
{
	int i, j = 0;
	char *s;

	for (i = 0, s = (char *)cntr7322names; s && j <= dd->cfgctxts;
	     i++) {
		
		if (!j && !strncmp("Ctxt0EgrOvfl", s + 1, 12))
			j = 1;
		s = strchr(s + 1, '\n');
		if (s && j)
			j++;
	}
	dd->cspec->ncntrs = i;
	if (!s)
		
		dd->cspec->cntrnamelen = sizeof(cntr7322names) - 1;
	else
		dd->cspec->cntrnamelen = 1 + s - cntr7322names;
	dd->cspec->cntrs = kmalloc(dd->cspec->ncntrs
		* sizeof(u64), GFP_KERNEL);
	if (!dd->cspec->cntrs)
		qib_dev_err(dd, "Failed allocation for counters\n");

	for (i = 0, s = (char *)portcntr7322names; s; i++)
		s = strchr(s + 1, '\n');
	dd->cspec->nportcntrs = i - 1;
	dd->cspec->portcntrnamelen = sizeof(portcntr7322names) - 1;
	for (i = 0; i < dd->num_pports; ++i) {
		dd->pport[i].cpspec->portcntrs = kmalloc(dd->cspec->nportcntrs
			* sizeof(u64), GFP_KERNEL);
		if (!dd->pport[i].cpspec->portcntrs)
			qib_dev_err(dd, "Failed allocation for"
				    " portcounters\n");
	}
}

static u32 qib_read_7322cntrs(struct qib_devdata *dd, loff_t pos, char **namep,
			      u64 **cntrp)
{
	u32 ret;

	if (namep) {
		ret = dd->cspec->cntrnamelen;
		if (pos >= ret)
			ret = 0; 
		else
			*namep = (char *) cntr7322names;
	} else {
		u64 *cntr = dd->cspec->cntrs;
		int i;

		ret = dd->cspec->ncntrs * sizeof(u64);
		if (!cntr || pos >= ret) {
			
			ret = 0;
			goto done;
		}
		*cntrp = cntr;
		for (i = 0; i < dd->cspec->ncntrs; i++)
			if (cntr7322indices[i] & _PORT_64BIT_FLAG)
				*cntr++ = read_7322_creg(dd,
							 cntr7322indices[i] &
							 _PORT_CNTR_IDXMASK);
			else
				*cntr++ = read_7322_creg32(dd,
							   cntr7322indices[i]);
	}
done:
	return ret;
}

static u32 qib_read_7322portcntrs(struct qib_devdata *dd, loff_t pos, u32 port,
				  char **namep, u64 **cntrp)
{
	u32 ret;

	if (namep) {
		ret = dd->cspec->portcntrnamelen;
		if (pos >= ret)
			ret = 0; 
		else
			*namep = (char *)portcntr7322names;
	} else {
		struct qib_pportdata *ppd = &dd->pport[port];
		u64 *cntr = ppd->cpspec->portcntrs;
		int i;

		ret = dd->cspec->nportcntrs * sizeof(u64);
		if (!cntr || pos >= ret) {
			
			ret = 0;
			goto done;
		}
		*cntrp = cntr;
		for (i = 0; i < dd->cspec->nportcntrs; i++) {
			if (portcntr7322indices[i] & _PORT_VIRT_FLAG)
				*cntr++ = qib_portcntr_7322(ppd,
					portcntr7322indices[i] &
					_PORT_CNTR_IDXMASK);
			else if (portcntr7322indices[i] & _PORT_64BIT_FLAG)
				*cntr++ = read_7322_creg_port(ppd,
					   portcntr7322indices[i] &
					    _PORT_CNTR_IDXMASK);
			else
				*cntr++ = read_7322_creg32_port(ppd,
					   portcntr7322indices[i]);
		}
	}
done:
	return ret;
}

static void qib_get_7322_faststats(unsigned long opaque)
{
	struct qib_devdata *dd = (struct qib_devdata *) opaque;
	struct qib_pportdata *ppd;
	unsigned long flags;
	u64 traffic_wds;
	int pidx;

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		ppd = dd->pport + pidx;

		if (!ppd->link_speed_supported || !(dd->flags & QIB_INITTED)
		    || dd->diag_client)
			continue;

		traffic_wds = qib_portcntr_7322(ppd, QIBPORTCNTR_WORDRCV) +
			qib_portcntr_7322(ppd, QIBPORTCNTR_WORDSEND);
		spin_lock_irqsave(&ppd->dd->eep_st_lock, flags);
		traffic_wds -= ppd->dd->traffic_wds;
		ppd->dd->traffic_wds += traffic_wds;
		if (traffic_wds >= QIB_TRAFFIC_ACTIVE_THRESHOLD)
			atomic_add(ACTIVITY_TIMER, &ppd->dd->active_time);
		spin_unlock_irqrestore(&ppd->dd->eep_st_lock, flags);
		if (ppd->cpspec->qdr_dfe_on && (ppd->link_speed_active &
						QIB_IB_QDR) &&
		    (ppd->lflags & (QIBL_LINKINIT | QIBL_LINKARMED |
				    QIBL_LINKACTIVE)) &&
		    ppd->cpspec->qdr_dfe_time &&
		    time_is_before_jiffies(ppd->cpspec->qdr_dfe_time)) {
			ppd->cpspec->qdr_dfe_on = 0;

			qib_write_kreg_port(ppd, krp_static_adapt_dis(2),
					    ppd->dd->cspec->r1 ?
					    QDR_STATIC_ADAPT_INIT_R1 :
					    QDR_STATIC_ADAPT_INIT);
			force_h1(ppd);
		}
	}
	mod_timer(&dd->stats_timer, jiffies + HZ * ACTIVITY_TIMER);
}

static int qib_7322_intr_fallback(struct qib_devdata *dd)
{
	if (!dd->cspec->num_msix_entries)
		return 0; 

	qib_devinfo(dd->pcidev, "MSIx interrupt not detected,"
		 " trying INTx interrupts\n");
	qib_7322_nomsix(dd);
	qib_enable_intx(dd->pcidev);
	qib_setup_7322_interrupt(dd, 0);
	return 1;
}

static void qib_7322_mini_pcs_reset(struct qib_pportdata *ppd)
{
	u64 val;
	struct qib_devdata *dd = ppd->dd;
	const u64 reset_bits = SYM_MASK(IBPCSConfig_0, xcv_rreset) |
		SYM_MASK(IBPCSConfig_0, xcv_treset) |
		SYM_MASK(IBPCSConfig_0, tx_rx_reset);

	val = qib_read_kreg_port(ppd, krp_ib_pcsconfig);
	qib_write_kreg(dd, kr_hwerrmask,
		       dd->cspec->hwerrmask & ~HWE_MASK(statusValidNoEop));
	qib_write_kreg_port(ppd, krp_ibcctrl_a,
			    ppd->cpspec->ibcctrl_a &
			    ~SYM_MASK(IBCCtrlA_0, IBLinkEn));

	qib_write_kreg_port(ppd, krp_ib_pcsconfig, val | reset_bits);
	qib_read_kreg32(dd, kr_scratch);
	qib_write_kreg_port(ppd, krp_ib_pcsconfig, val & ~reset_bits);
	qib_write_kreg_port(ppd, krp_ibcctrl_a, ppd->cpspec->ibcctrl_a);
	qib_write_kreg(dd, kr_scratch, 0ULL);
	qib_write_kreg(dd, kr_hwerrclear,
		       SYM_MASK(HwErrClear, statusValidNoEopClear));
	qib_write_kreg(dd, kr_hwerrmask, dd->cspec->hwerrmask);
}

static void autoneg_7322_sendpkt(struct qib_pportdata *ppd, u32 *hdr,
				 u32 dcnt, u32 *data)
{
	int i;
	u64 pbc;
	u32 __iomem *piobuf;
	u32 pnum, control, len;
	struct qib_devdata *dd = ppd->dd;

	i = 0;
	len = 7 + dcnt + 1; 
	control = qib_7322_setpbc_control(ppd, len, 0, 15);
	pbc = ((u64) control << 32) | len;
	while (!(piobuf = qib_7322_getsendbuf(ppd, pbc, &pnum))) {
		if (i++ > 15)
			return;
		udelay(2);
	}
	
	dd->f_txchk_change(dd, pnum, 1, TXCHK_CHG_TYPE_DIS1, NULL);
	writeq(pbc, piobuf);
	qib_flush_wc();
	qib_pio_copy(piobuf + 2, hdr, 7);
	qib_pio_copy(piobuf + 9, data, dcnt);
	if (dd->flags & QIB_USE_SPCL_TRIG) {
		u32 spcl_off = (pnum >= dd->piobcnt2k) ? 2047 : 1023;

		qib_flush_wc();
		__raw_writel(0xaebecede, piobuf + spcl_off);
	}
	qib_flush_wc();
	qib_sendbuf_done(dd, pnum);
	
	dd->f_txchk_change(dd, pnum, 1, TXCHK_CHG_TYPE_ENAB1, NULL);
}

static void qib_autoneg_7322_send(struct qib_pportdata *ppd, int which)
{
	struct qib_devdata *dd = ppd->dd;
	static u32 swapped;
	u32 dw, i, hcnt, dcnt, *data;
	static u32 hdr[7] = { 0xf002ffff, 0x48ffff, 0x6400abba };
	static u32 madpayload_start[0x40] = {
		0x1810103, 0x1, 0x0, 0x0, 0x2c90000, 0x2c9, 0x0, 0x0,
		0xffffffff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x1, 0x1388, 0x15e, 0x1, 
		};
	static u32 madpayload_done[0x40] = {
		0x1810103, 0x1, 0x0, 0x0, 0x2c90000, 0x2c9, 0x0, 0x0,
		0xffffffff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x40000001, 0x1388, 0x15e, 
		};

	dcnt = ARRAY_SIZE(madpayload_start);
	hcnt = ARRAY_SIZE(hdr);
	if (!swapped) {
		
		for (i = 0; i < hcnt; i++) {
			dw = (__force u32) cpu_to_be32(hdr[i]);
			hdr[i] = dw;
		}
		for (i = 0; i < dcnt; i++) {
			dw = (__force u32) cpu_to_be32(madpayload_start[i]);
			madpayload_start[i] = dw;
			dw = (__force u32) cpu_to_be32(madpayload_done[i]);
			madpayload_done[i] = dw;
		}
		swapped = 1;
	}

	data = which ? madpayload_done : madpayload_start;

	autoneg_7322_sendpkt(ppd, hdr, dcnt, data);
	qib_read_kreg64(dd, kr_scratch);
	udelay(2);
	autoneg_7322_sendpkt(ppd, hdr, dcnt, data);
	qib_read_kreg64(dd, kr_scratch);
	udelay(2);
}

static void set_7322_ibspeed_fast(struct qib_pportdata *ppd, u32 speed)
{
	u64 newctrlb;
	newctrlb = ppd->cpspec->ibcctrl_b & ~(IBA7322_IBC_SPEED_MASK |
				    IBA7322_IBC_IBTA_1_2_MASK |
				    IBA7322_IBC_MAX_SPEED_MASK);

	if (speed & (speed - 1)) 
		newctrlb |= (speed << IBA7322_IBC_SPEED_LSB) |
				    IBA7322_IBC_IBTA_1_2_MASK |
				    IBA7322_IBC_MAX_SPEED_MASK;
	else
		newctrlb |= speed == QIB_IB_QDR ?
			IBA7322_IBC_SPEED_QDR | IBA7322_IBC_IBTA_1_2_MASK :
			((speed == QIB_IB_DDR ?
			  IBA7322_IBC_SPEED_DDR : IBA7322_IBC_SPEED_SDR));

	if (newctrlb == ppd->cpspec->ibcctrl_b)
		return;

	ppd->cpspec->ibcctrl_b = newctrlb;
	qib_write_kreg_port(ppd, krp_ibcctrl_b, ppd->cpspec->ibcctrl_b);
	qib_write_kreg(ppd->dd, kr_scratch, 0);
}

static void try_7322_autoneg(struct qib_pportdata *ppd)
{
	unsigned long flags;

	spin_lock_irqsave(&ppd->lflags_lock, flags);
	ppd->lflags |= QIBL_IB_AUTONEG_INPROG;
	spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	qib_autoneg_7322_send(ppd, 0);
	set_7322_ibspeed_fast(ppd, QIB_IB_DDR);
	qib_7322_mini_pcs_reset(ppd);
	
	queue_delayed_work(ib_wq, &ppd->cpspec->autoneg_work,
			   msecs_to_jiffies(2));
}

static void autoneg_7322_work(struct work_struct *work)
{
	struct qib_pportdata *ppd;
	struct qib_devdata *dd;
	u64 startms;
	u32 i;
	unsigned long flags;

	ppd = container_of(work, struct qib_chippport_specific,
			    autoneg_work.work)->ppd;
	dd = ppd->dd;

	startms = jiffies_to_msecs(jiffies);

	for (i = 0; i < 25; i++) {
		if (SYM_FIELD(ppd->lastibcstat, IBCStatusA_0, LinkState)
		     == IB_7322_LT_STATE_POLLQUIET) {
			qib_set_linkstate(ppd, QIB_IB_LINKDOWN_DISABLE);
			break;
		}
		udelay(100);
	}

	if (!(ppd->lflags & QIBL_IB_AUTONEG_INPROG))
		goto done; 

	
	if (wait_event_timeout(ppd->cpspec->autoneg_wait,
			       !(ppd->lflags & QIBL_IB_AUTONEG_INPROG),
			       msecs_to_jiffies(90)))
		goto done;
	qib_7322_mini_pcs_reset(ppd);

	
	if (wait_event_timeout(ppd->cpspec->autoneg_wait,
			       !(ppd->lflags & QIBL_IB_AUTONEG_INPROG),
			       msecs_to_jiffies(1700)))
		goto done;
	qib_7322_mini_pcs_reset(ppd);

	set_7322_ibspeed_fast(ppd, QIB_IB_SDR);

	wait_event_timeout(ppd->cpspec->autoneg_wait,
		!(ppd->lflags & QIBL_IB_AUTONEG_INPROG),
		msecs_to_jiffies(250));
done:
	if (ppd->lflags & QIBL_IB_AUTONEG_INPROG) {
		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags &= ~QIBL_IB_AUTONEG_INPROG;
		if (ppd->cpspec->autoneg_tries == AUTONEG_TRIES) {
			ppd->lflags |= QIBL_IB_AUTONEG_FAILED;
			ppd->cpspec->autoneg_tries = 0;
		}
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
		set_7322_ibspeed_fast(ppd, ppd->link_speed_enabled);
	}
}

static void try_7322_ipg(struct qib_pportdata *ppd)
{
	struct qib_ibport *ibp = &ppd->ibport_data;
	struct ib_mad_send_buf *send_buf;
	struct ib_mad_agent *agent;
	struct ib_smp *smp;
	unsigned delay;
	int ret;

	agent = ibp->send_agent;
	if (!agent)
		goto retry;

	send_buf = ib_create_send_mad(agent, 0, 0, 0, IB_MGMT_MAD_HDR,
				      IB_MGMT_MAD_DATA, GFP_ATOMIC);
	if (IS_ERR(send_buf))
		goto retry;

	if (!ibp->smi_ah) {
		struct ib_ah_attr attr;
		struct ib_ah *ah;

		memset(&attr, 0, sizeof attr);
		attr.dlid = be16_to_cpu(IB_LID_PERMISSIVE);
		attr.port_num = ppd->port;
		ah = ib_create_ah(ibp->qp0->ibqp.pd, &attr);
		if (IS_ERR(ah))
			ret = -EINVAL;
		else {
			send_buf->ah = ah;
			ibp->smi_ah = to_iah(ah);
			ret = 0;
		}
	} else {
		send_buf->ah = &ibp->smi_ah->ibah;
		ret = 0;
	}

	smp = send_buf->mad;
	smp->base_version = IB_MGMT_BASE_VERSION;
	smp->mgmt_class = IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE;
	smp->class_version = 1;
	smp->method = IB_MGMT_METHOD_SEND;
	smp->hop_cnt = 1;
	smp->attr_id = QIB_VENDOR_IPG;
	smp->attr_mod = 0;

	if (!ret)
		ret = ib_post_send_mad(send_buf, NULL);
	if (ret)
		ib_free_send_mad(send_buf);
retry:
	delay = 2 << ppd->cpspec->ipg_tries;
	queue_delayed_work(ib_wq, &ppd->cpspec->ipg_work,
			   msecs_to_jiffies(delay));
}

static void ipg_7322_work(struct work_struct *work)
{
	struct qib_pportdata *ppd;

	ppd = container_of(work, struct qib_chippport_specific,
			   ipg_work.work)->ppd;
	if ((ppd->lflags & (QIBL_LINKINIT | QIBL_LINKARMED | QIBL_LINKACTIVE))
	    && ++ppd->cpspec->ipg_tries <= 10)
		try_7322_ipg(ppd);
}

static u32 qib_7322_iblink_state(u64 ibcs)
{
	u32 state = (u32)SYM_FIELD(ibcs, IBCStatusA_0, LinkState);

	switch (state) {
	case IB_7322_L_STATE_INIT:
		state = IB_PORT_INIT;
		break;
	case IB_7322_L_STATE_ARM:
		state = IB_PORT_ARMED;
		break;
	case IB_7322_L_STATE_ACTIVE:
		
	case IB_7322_L_STATE_ACT_DEFER:
		state = IB_PORT_ACTIVE;
		break;
	default: 
	case IB_7322_L_STATE_DOWN:
		state = IB_PORT_DOWN;
		break;
	}
	return state;
}

static u8 qib_7322_phys_portstate(u64 ibcs)
{
	u8 state = (u8)SYM_FIELD(ibcs, IBCStatusA_0, LinkTrainingState);
	return qib_7322_physportstate[state];
}

static int qib_7322_ib_updown(struct qib_pportdata *ppd, int ibup, u64 ibcs)
{
	int ret = 0, symadj = 0;
	unsigned long flags;
	int mult;

	spin_lock_irqsave(&ppd->lflags_lock, flags);
	ppd->lflags &= ~QIBL_IB_FORCE_NOTIFY;
	spin_unlock_irqrestore(&ppd->lflags_lock, flags);

	
	if (ibcs & SYM_MASK(IBCStatusA_0, LinkSpeedQDR)) {
		ppd->link_speed_active = QIB_IB_QDR;
		mult = 4;
	} else if (ibcs & SYM_MASK(IBCStatusA_0, LinkSpeedActive)) {
		ppd->link_speed_active = QIB_IB_DDR;
		mult = 2;
	} else {
		ppd->link_speed_active = QIB_IB_SDR;
		mult = 1;
	}
	if (ibcs & SYM_MASK(IBCStatusA_0, LinkWidthActive)) {
		ppd->link_width_active = IB_WIDTH_4X;
		mult *= 4;
	} else
		ppd->link_width_active = IB_WIDTH_1X;
	ppd->delay_mult = ib_rate_to_delay[mult_to_ib_rate(mult)];

	if (!ibup) {
		u64 clr;

		
		
		ppd->cpspec->ipg_tries = 0;
		clr = qib_read_kreg_port(ppd, krp_ibcstatus_b) &
			(SYM_MASK(IBCStatusB_0, heartbeat_timed_out) |
			 SYM_MASK(IBCStatusB_0, heartbeat_crosstalk));
		if (clr)
			qib_write_kreg_port(ppd, krp_ibcstatus_b, clr);
		if (!(ppd->lflags & (QIBL_IB_AUTONEG_FAILED |
				     QIBL_IB_AUTONEG_INPROG)))
			set_7322_ibspeed_fast(ppd, ppd->link_speed_enabled);
		if (!(ppd->lflags & QIBL_IB_AUTONEG_INPROG)) {
			struct qib_qsfp_data *qd =
				&ppd->cpspec->qsfp_data;
			
			qib_write_kreg_port(ppd, krp_tx_deemph_override,
				SYM_MASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
				reset_tx_deemphasis_override));
			qib_cancel_sends(ppd);
			
			qib_7322_mini_pcs_reset(ppd);
			if (ppd->dd->flags & QIB_HAS_QSFP) {
				qd->t_insert = jiffies;
				queue_work(ib_wq, &qd->work);
			}
			spin_lock_irqsave(&ppd->sdma_lock, flags);
			if (__qib_sdma_running(ppd))
				__qib_sdma_process_event(ppd,
					qib_sdma_event_e70_go_idle);
			spin_unlock_irqrestore(&ppd->sdma_lock, flags);
		}
		clr = read_7322_creg32_port(ppd, crp_iblinkdown);
		if (clr == ppd->cpspec->iblnkdownsnap)
			ppd->cpspec->iblnkdowndelta++;
	} else {
		if (qib_compat_ddr_negotiate &&
		    !(ppd->lflags & (QIBL_IB_AUTONEG_FAILED |
				     QIBL_IB_AUTONEG_INPROG)) &&
		    ppd->link_speed_active == QIB_IB_SDR &&
		    (ppd->link_speed_enabled & QIB_IB_DDR)
		    && ppd->cpspec->autoneg_tries < AUTONEG_TRIES) {
			
			++ppd->cpspec->autoneg_tries;
			if (!ppd->cpspec->ibdeltainprog) {
				ppd->cpspec->ibdeltainprog = 1;
				ppd->cpspec->ibsymdelta +=
					read_7322_creg32_port(ppd,
						crp_ibsymbolerr) -
						ppd->cpspec->ibsymsnap;
				ppd->cpspec->iblnkerrdelta +=
					read_7322_creg32_port(ppd,
						crp_iblinkerrrecov) -
						ppd->cpspec->iblnkerrsnap;
			}
			try_7322_autoneg(ppd);
			ret = 1; 
		} else if ((ppd->lflags & QIBL_IB_AUTONEG_INPROG) &&
			   ppd->link_speed_active == QIB_IB_SDR) {
			qib_autoneg_7322_send(ppd, 1);
			set_7322_ibspeed_fast(ppd, QIB_IB_DDR);
			qib_7322_mini_pcs_reset(ppd);
			udelay(2);
			ret = 1; 
		} else if ((ppd->lflags & QIBL_IB_AUTONEG_INPROG) &&
			   (ppd->link_speed_active & QIB_IB_DDR)) {
			spin_lock_irqsave(&ppd->lflags_lock, flags);
			ppd->lflags &= ~(QIBL_IB_AUTONEG_INPROG |
					 QIBL_IB_AUTONEG_FAILED);
			spin_unlock_irqrestore(&ppd->lflags_lock, flags);
			ppd->cpspec->autoneg_tries = 0;
			
			set_7322_ibspeed_fast(ppd, ppd->link_speed_enabled);
			wake_up(&ppd->cpspec->autoneg_wait);
			symadj = 1;
		} else if (ppd->lflags & QIBL_IB_AUTONEG_FAILED) {
			spin_lock_irqsave(&ppd->lflags_lock, flags);
			ppd->lflags &= ~QIBL_IB_AUTONEG_FAILED;
			spin_unlock_irqrestore(&ppd->lflags_lock, flags);
			ppd->cpspec->ibcctrl_b |= IBA7322_IBC_IBTA_1_2_MASK;
			symadj = 1;
		}
		if (!(ppd->lflags & QIBL_IB_AUTONEG_INPROG)) {
			symadj = 1;
			if (ppd->dd->cspec->r1 && ppd->cpspec->ipg_tries <= 10)
				try_7322_ipg(ppd);
			if (!ppd->cpspec->recovery_init)
				setup_7322_link_recovery(ppd, 0);
			ppd->cpspec->qdr_dfe_time = jiffies +
				msecs_to_jiffies(QDR_DFE_DISABLE_DELAY);
		}
		ppd->cpspec->ibmalfusesnap = 0;
		ppd->cpspec->ibmalfsnap = read_7322_creg32_port(ppd,
			crp_errlink);
	}
	if (symadj) {
		ppd->cpspec->iblnkdownsnap =
			read_7322_creg32_port(ppd, crp_iblinkdown);
		if (ppd->cpspec->ibdeltainprog) {
			ppd->cpspec->ibdeltainprog = 0;
			ppd->cpspec->ibsymdelta += read_7322_creg32_port(ppd,
				crp_ibsymbolerr) - ppd->cpspec->ibsymsnap;
			ppd->cpspec->iblnkerrdelta += read_7322_creg32_port(ppd,
				crp_iblinkerrrecov) - ppd->cpspec->iblnkerrsnap;
		}
	} else if (!ibup && qib_compat_ddr_negotiate &&
		   !ppd->cpspec->ibdeltainprog &&
			!(ppd->lflags & QIBL_IB_AUTONEG_INPROG)) {
		ppd->cpspec->ibdeltainprog = 1;
		ppd->cpspec->ibsymsnap = read_7322_creg32_port(ppd,
			crp_ibsymbolerr);
		ppd->cpspec->iblnkerrsnap = read_7322_creg32_port(ppd,
			crp_iblinkerrrecov);
	}

	if (!ret)
		qib_setup_7322_setextled(ppd, ibup);
	return ret;
}

static int gpio_7322_mod(struct qib_devdata *dd, u32 out, u32 dir, u32 mask)
{
	u64 read_val, new_out;
	unsigned long flags;

	if (mask) {
		/* some bits being written, lock access to GPIO */
		dir &= mask;
		out &= mask;
		spin_lock_irqsave(&dd->cspec->gpio_lock, flags);
		dd->cspec->extctrl &= ~((u64)mask << SYM_LSB(EXTCtrl, GPIOOe));
		dd->cspec->extctrl |= ((u64) dir << SYM_LSB(EXTCtrl, GPIOOe));
		new_out = (dd->cspec->gpio_out & ~mask) | out;

		qib_write_kreg(dd, kr_extctrl, dd->cspec->extctrl);
		qib_write_kreg(dd, kr_gpio_out, new_out);
		dd->cspec->gpio_out = new_out;
		spin_unlock_irqrestore(&dd->cspec->gpio_lock, flags);
	}
	read_val = qib_read_kreg64(dd, kr_extstatus);
	return SYM_FIELD(read_val, EXTStatus, GPIOIn);
}

static int qib_7322_eeprom_wen(struct qib_devdata *dd, int wen)
{
	int prev_wen;
	u32 mask;

	mask = 1 << QIB_EEPROM_WEN_NUM;
	prev_wen = ~gpio_7322_mod(dd, 0, 0, 0) >> QIB_EEPROM_WEN_NUM;
	gpio_7322_mod(dd, wen ? 0 : mask, mask, mask);

	return prev_wen & 1;
}

static void get_7322_chip_params(struct qib_devdata *dd)
{
	u64 val;
	u32 piobufs;
	int mtu;

	dd->palign = qib_read_kreg32(dd, kr_pagealign);

	dd->uregbase = qib_read_kreg32(dd, kr_userregbase);

	dd->rcvtidcnt = qib_read_kreg32(dd, kr_rcvtidcnt);
	dd->rcvtidbase = qib_read_kreg32(dd, kr_rcvtidbase);
	dd->rcvegrbase = qib_read_kreg32(dd, kr_rcvegrbase);
	dd->piobufbase = qib_read_kreg64(dd, kr_sendpiobufbase);
	dd->pio2k_bufbase = dd->piobufbase & 0xffffffff;

	val = qib_read_kreg64(dd, kr_sendpiobufcnt);
	dd->piobcnt2k = val & ~0U;
	dd->piobcnt4k = val >> 32;
	val = qib_read_kreg64(dd, kr_sendpiosize);
	dd->piosize2k = val & ~0U;
	dd->piosize4k = val >> 32;

	mtu = ib_mtu_enum_to_int(qib_ibmtu);
	if (mtu == -1)
		mtu = QIB_DEFAULT_MTU;
	dd->pport[0].ibmtu = (u32)mtu;
	dd->pport[1].ibmtu = (u32)mtu;

	
	dd->pio2kbase = (u32 __iomem *)
		((char __iomem *) dd->kregbase + dd->pio2k_bufbase);
	dd->pio4kbase = (u32 __iomem *)
		((char __iomem *) dd->kregbase +
		 (dd->piobufbase >> 32));
	dd->align4k = ALIGN(dd->piosize4k, dd->palign);

	piobufs = dd->piobcnt4k + dd->piobcnt2k + NUM_VL15_BUFS;

	dd->pioavregs = ALIGN(piobufs, sizeof(u64) * BITS_PER_BYTE / 2) /
		(sizeof(u64) * BITS_PER_BYTE / 2);
}

static void qib_7322_set_baseaddrs(struct qib_devdata *dd)
{
	u32 cregbase;
	cregbase = qib_read_kreg32(dd, kr_counterregbase);

	dd->cspec->cregbase = (u64 __iomem *)(cregbase +
		(char __iomem *)dd->kregbase);

	dd->egrtidbase = (u64 __iomem *)
		((char __iomem *) dd->kregbase + dd->rcvegrbase);

	
	dd->pport[0].cpspec->kpregbase =
		(u64 __iomem *)((char __iomem *)dd->kregbase);
	dd->pport[1].cpspec->kpregbase =
		(u64 __iomem *)(dd->palign +
		(char __iomem *)dd->kregbase);
	dd->pport[0].cpspec->cpregbase =
		(u64 __iomem *)(qib_read_kreg_port(&dd->pport[0],
		kr_counterregbase) + (char __iomem *)dd->kregbase);
	dd->pport[1].cpspec->cpregbase =
		(u64 __iomem *)(qib_read_kreg_port(&dd->pport[1],
		kr_counterregbase) + (char __iomem *)dd->kregbase);
}


#define SENDCTRL_SHADOWED (SYM_MASK(SendCtrl_0, SendEnable) |		\
			   SYM_MASK(SendCtrl_0, SDmaEnable) |		\
			   SYM_MASK(SendCtrl_0, SDmaIntEnable) |	\
			   SYM_MASK(SendCtrl_0, SDmaSingleDescriptor) | \
			   SYM_MASK(SendCtrl_0, SDmaHalt) |		\
			   SYM_MASK(SendCtrl_0, IBVLArbiterEn) |	\
			   SYM_MASK(SendCtrl_0, ForceCreditUpToDate))

static int sendctrl_hook(struct qib_devdata *dd,
			 const struct diag_observer *op, u32 offs,
			 u64 *data, u64 mask, int only_32)
{
	unsigned long flags;
	unsigned idx;
	unsigned pidx;
	struct qib_pportdata *ppd = NULL;
	u64 local_data, all_bits;

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		u64 __iomem *psptr;
		u32 psoffs;

		ppd = dd->pport + pidx;
		if (!ppd->cpspec->kpregbase)
			continue;

		psptr = ppd->cpspec->kpregbase + krp_sendctrl;
		psoffs = (u32) (psptr - dd->kregbase) * sizeof(*psptr);
		if (psoffs == offs)
			break;
	}

	
	if (pidx >= dd->num_pports)
		ppd = NULL;

	
	idx = offs / sizeof(u64);

	all_bits = ~0ULL;
	if (only_32)
		all_bits >>= 32;

	spin_lock_irqsave(&dd->sendctrl_lock, flags);
	if (!ppd || (mask & all_bits) != all_bits) {
		if (only_32)
			local_data = (u64)qib_read_kreg32(dd, idx);
		else
			local_data = qib_read_kreg64(dd, idx);
		*data = (local_data & ~mask) | (*data & mask);
	}
	if (mask) {
		u64 sval, tval; 

		if (ppd) {
			sval = ppd->p_sendctrl & ~mask;
			sval |= *data & SENDCTRL_SHADOWED & mask;
			ppd->p_sendctrl = sval;
		} else
			sval = *data & SENDCTRL_SHADOWED & mask;
		tval = sval | (*data & ~SENDCTRL_SHADOWED & mask);
		qib_write_kreg(dd, idx, tval);
		qib_write_kreg(dd, kr_scratch, 0Ull);
	}
	spin_unlock_irqrestore(&dd->sendctrl_lock, flags);
	return only_32 ? 4 : 8;
}

static const struct diag_observer sendctrl_0_observer = {
	sendctrl_hook, KREG_IDX(SendCtrl_0) * sizeof(u64),
	KREG_IDX(SendCtrl_0) * sizeof(u64)
};

static const struct diag_observer sendctrl_1_observer = {
	sendctrl_hook, KREG_IDX(SendCtrl_1) * sizeof(u64),
	KREG_IDX(SendCtrl_1) * sizeof(u64)
};

static ushort sdma_fetch_prio = 8;
module_param_named(sdma_fetch_prio, sdma_fetch_prio, ushort, S_IRUGO);
MODULE_PARM_DESC(sdma_fetch_prio, "SDMA descriptor fetch priority");

static void init_txdds_table(struct qib_pportdata *ppd, int override);

static void qsfp_7322_event(struct work_struct *work)
{
	struct qib_qsfp_data *qd;
	struct qib_pportdata *ppd;
	unsigned long pwrup;
	unsigned long flags;
	int ret;
	u32 le2;

	qd = container_of(work, struct qib_qsfp_data, work);
	ppd = qd->ppd;
	pwrup = qd->t_insert +
		msecs_to_jiffies(QSFP_PWR_LAG_MSEC - QSFP_MODPRS_LAG_MSEC);

	
	mdelay(QSFP_MODPRS_LAG_MSEC);

	if (!qib_qsfp_mod_present(ppd)) {
		ppd->cpspec->qsfp_data.modpresent = 0;
		
		qib_set_ib_7322_lstate(ppd, 0,
				       QLOGIC_IB_IBCC_LINKINITCMD_DISABLE);
		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags &= ~QIBL_LINKV;
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	} else {
		while (1) {
			if (time_is_before_jiffies(pwrup))
				break;
			msleep(20);
		}

		ret = qib_refresh_qsfp_cache(ppd, &qd->cache);

		if (!ret && !ppd->dd->cspec->r1) {
			if (QSFP_IS_ACTIVE_FAR(qd->cache.tech))
				le2 = LE2_QME;
			else if (qd->cache.atten[1] >= qib_long_atten &&
				 QSFP_IS_CU(qd->cache.tech))
				le2 = LE2_5m;
			else
				le2 = LE2_DEFAULT;
		} else
			le2 = LE2_DEFAULT;
		ibsd_wr_allchans(ppd, 13, (le2 << 7), BMASK(9, 7));
		init_txdds_table(ppd, 0);
		if (!ppd->cpspec->qsfp_data.modpresent &&
		    (ppd->lflags & (QIBL_LINKV | QIBL_IB_LINK_DISABLED))) {
			ppd->cpspec->qsfp_data.modpresent = 1;
			qib_set_ib_7322_lstate(ppd, 0,
				QLOGIC_IB_IBCC_LINKINITCMD_SLEEP);
			spin_lock_irqsave(&ppd->lflags_lock, flags);
			ppd->lflags |= QIBL_LINKV;
			spin_unlock_irqrestore(&ppd->lflags_lock, flags);
		}
	}
}

static void qib_init_7322_qsfp(struct qib_pportdata *ppd)
{
	unsigned long flags;
	struct qib_qsfp_data *qd = &ppd->cpspec->qsfp_data;
	struct qib_devdata *dd = ppd->dd;
	u64 mod_prs_bit = QSFP_GPIO_MOD_PRS_N;

	mod_prs_bit <<= (QSFP_GPIO_PORT2_SHIFT * ppd->hw_pidx);
	qd->ppd = ppd;
	qib_qsfp_init(qd, qsfp_7322_event);
	spin_lock_irqsave(&dd->cspec->gpio_lock, flags);
	dd->cspec->extctrl |= (mod_prs_bit << SYM_LSB(EXTCtrl, GPIOInvert));
	dd->cspec->gpio_mask |= mod_prs_bit;
	qib_write_kreg(dd, kr_extctrl, dd->cspec->extctrl);
	qib_write_kreg(dd, kr_gpio_mask, dd->cspec->gpio_mask);
	spin_unlock_irqrestore(&dd->cspec->gpio_lock, flags);
}

static void set_no_qsfp_atten(struct qib_devdata *dd, int change)
{
	char *nxt, *str;
	u32 pidx, unit, port, deflt, h1;
	unsigned long val;
	int any = 0, seth1;
	int txdds_size;

	str = txselect_list;

	
	deflt = simple_strtoul(str, &nxt, 0);
	for (pidx = 0; pidx < dd->num_pports; ++pidx)
		dd->pport[pidx].cpspec->no_eep = deflt;

	txdds_size = TXDDS_TABLE_SZ + TXDDS_EXTRA_SZ;
	if (IS_QME(dd) || IS_QMH(dd))
		txdds_size += TXDDS_MFG_SZ;

	while (*nxt && nxt[1]) {
		str = ++nxt;
		unit = simple_strtoul(str, &nxt, 0);
		if (nxt == str || !*nxt || *nxt != ',') {
			while (*nxt && *nxt++ != ' ') 
				;
			continue;
		}
		str = ++nxt;
		port = simple_strtoul(str, &nxt, 0);
		if (nxt == str || *nxt != '=') {
			while (*nxt && *nxt++ != ' ') 
				;
			continue;
		}
		str = ++nxt;
		val = simple_strtoul(str, &nxt, 0);
		if (nxt == str) {
			while (*nxt && *nxt++ != ' ') 
				;
			continue;
		}
		if (val >= txdds_size)
			continue;
		seth1 = 0;
		h1 = 0; 
		if (*nxt == ',' && nxt[1]) {
			str = ++nxt;
			h1 = (u32)simple_strtoul(str, &nxt, 0);
			if (nxt == str)
				while (*nxt && *nxt++ != ' ') 
					;
			else
				seth1 = 1;
		}
		for (pidx = 0; dd->unit == unit && pidx < dd->num_pports;
		     ++pidx) {
			struct qib_pportdata *ppd = &dd->pport[pidx];

			if (ppd->port != port || !ppd->link_speed_supported)
				continue;
			ppd->cpspec->no_eep = val;
			if (seth1)
				ppd->cpspec->h1_val = h1;
			
			init_txdds_table(ppd, 1);
			if (IS_QMH(dd) || IS_QME(dd))
				qib_set_ib_7322_lstate(ppd, 0,
					    QLOGIC_IB_IBCC_LINKINITCMD_SLEEP);
			any++;
		}
		if (*nxt == '\n')
			break; 
	}
	if (change && !any) {
		for (pidx = 0; pidx < dd->num_pports; ++pidx)
			if (dd->pport[pidx].link_speed_supported)
				init_txdds_table(&dd->pport[pidx], 0);
	}
}

static int setup_txselect(const char *str, struct kernel_param *kp)
{
	struct qib_devdata *dd;
	unsigned long val;
	char *n;
	if (strlen(str) >= MAX_ATTEN_LEN) {
		printk(KERN_INFO QIB_DRV_NAME " txselect_values string "
		       "too long\n");
		return -ENOSPC;
	}
	val = simple_strtoul(str, &n, 0);
	if (n == str || val >= (TXDDS_TABLE_SZ + TXDDS_EXTRA_SZ +
				TXDDS_MFG_SZ)) {
		printk(KERN_INFO QIB_DRV_NAME
		       "txselect_values must start with a number < %d\n",
			TXDDS_TABLE_SZ + TXDDS_EXTRA_SZ + TXDDS_MFG_SZ);
		return -EINVAL;
	}
	strcpy(txselect_list, str);

	list_for_each_entry(dd, &qib_dev_list, list)
		if (dd->deviceid == PCI_DEVICE_ID_QLOGIC_IB_7322)
			set_no_qsfp_atten(dd, 1);
	return 0;
}

static int qib_late_7322_initreg(struct qib_devdata *dd)
{
	int ret = 0, n;
	u64 val;

	qib_write_kreg(dd, kr_rcvhdrentsize, dd->rcvhdrentsize);
	qib_write_kreg(dd, kr_rcvhdrsize, dd->rcvhdrsize);
	qib_write_kreg(dd, kr_rcvhdrcnt, dd->rcvhdrcnt);
	qib_write_kreg(dd, kr_sendpioavailaddr, dd->pioavailregs_phys);
	val = qib_read_kreg64(dd, kr_sendpioavailaddr);
	if (val != dd->pioavailregs_phys) {
		qib_dev_err(dd, "Catastrophic software error, "
			    "SendPIOAvailAddr written as %lx, "
			    "read back as %llx\n",
			    (unsigned long) dd->pioavailregs_phys,
			    (unsigned long long) val);
		ret = -EINVAL;
	}

	n = dd->piobcnt2k + dd->piobcnt4k + NUM_VL15_BUFS;
	qib_7322_txchk_change(dd, 0, n, TXCHK_CHG_TYPE_KERN, NULL);
	
	qib_7322_txchk_change(dd, 0, n, TXCHK_CHG_TYPE_ENAB1, NULL);

	qib_register_observer(dd, &sendctrl_0_observer);
	qib_register_observer(dd, &sendctrl_1_observer);

	dd->control &= ~QLOGIC_IB_C_SDMAFETCHPRIOEN;
	qib_write_kreg(dd, kr_control, dd->control);
	set_no_qsfp_atten(dd, 0);
	for (n = 0; n < dd->num_pports; ++n) {
		struct qib_pportdata *ppd = dd->pport + n;

		qib_write_kreg_port(ppd, krp_senddmaprioritythld,
				    sdma_fetch_prio & 0xf);
		
		if (dd->flags & QIB_HAS_QSFP)
			qib_init_7322_qsfp(ppd);
	}
	dd->control |= QLOGIC_IB_C_SDMAFETCHPRIOEN;
	qib_write_kreg(dd, kr_control, dd->control);

	return ret;
}

#define SENDCTRL_PIBP (MASK_ACROSS(0, 1) | MASK_ACROSS(3, 3) | \
	MASK_ACROSS(8, 15))
#define RCVCTRL_PIBP (MASK_ACROSS(0, 17) | MASK_ACROSS(39, 41))
#define ERRS_PIBP (MASK_ACROSS(57, 58) | MASK_ACROSS(54, 54) | \
	MASK_ACROSS(36, 49) | MASK_ACROSS(29, 34) | MASK_ACROSS(14, 17) | \
	MASK_ACROSS(0, 11))

static void write_7322_init_portregs(struct qib_pportdata *ppd)
{
	u64 val;
	int i;

	if (!ppd->link_speed_supported) {
		
		for (i = 1; i < 8; i++)
			qib_write_kreg_port(ppd, krp_rxcreditvl0 + i, 0);
		qib_write_kreg_port(ppd, krp_ibcctrl_b, 0);
		qib_write_kreg(ppd->dd, kr_scratch, 0);
		return;
	}

	val = qib_read_kreg_port(ppd, krp_ibsdtestiftx);
	val &= ~SYM_MASK(IB_SDTEST_IF_TX_0, VL_CAP);
	val |= (u64)(ppd->vls_supported - 1) <<
		SYM_LSB(IB_SDTEST_IF_TX_0, VL_CAP);
	qib_write_kreg_port(ppd, krp_ibsdtestiftx, val);

	qib_write_kreg_port(ppd, krp_rcvbthqp, QIB_KD_QP);

	
	qib_write_kreg_port(ppd, krp_sendcheckcontrol, IBA7322_SENDCHK_PKEY |
			    IBA7322_SENDCHK_BTHQP | IBA7322_SENDCHK_SLID |
			    IBA7322_SENDCHK_RAW_IPV6 | IBA7322_SENDCHK_MINSZ);

	qib_write_kreg_port(ppd, krp_ncmodectrl,
		SYM_MASK(IBNCModeCtrl_0, ScrambleCapLocal));

	qib_write_kreg_port(ppd, krp_senddmabufmask0, 0);
	qib_write_kreg_port(ppd, krp_senddmabufmask1, 0);
	qib_write_kreg_port(ppd, krp_senddmabufmask2, 0);
	if (ppd->dd->cspec->r1)
		ppd->p_sendctrl |= SYM_MASK(SendCtrl_0, ForceCreditUpToDate);
}

static void write_7322_initregs(struct qib_devdata *dd)
{
	struct qib_pportdata *ppd;
	int i, pidx;
	u64 val;

	
	qib_write_kreg(dd, KREG_IDX(RcvQPMulticastContext_1), 1);

	for (pidx = 0; pidx < dd->num_pports; ++pidx) {
		unsigned n, regno;
		unsigned long flags;

		if (dd->n_krcv_queues < 2 ||
			!dd->pport[pidx].link_speed_supported)
			continue;

		ppd = &dd->pport[pidx];

		
		spin_lock_irqsave(&dd->cspec->rcvmod_lock, flags);
		ppd->p_rcvctrl |= SYM_MASK(RcvCtrl_0, RcvQPMapEnable);
		spin_unlock_irqrestore(&dd->cspec->rcvmod_lock, flags);

		
		regno = krp_rcvqpmaptable;
		val = 0;
		if (dd->num_pports > 1)
			n = dd->first_user_ctxt / dd->num_pports;
		else
			n = dd->first_user_ctxt - 1;
		for (i = 0; i < 32; ) {
			unsigned ctxt;

			if (dd->num_pports > 1)
				ctxt = (i % n) * dd->num_pports + pidx;
			else if (i % n)
				ctxt = (i % n) + 1;
			else
				ctxt = ppd->hw_pidx;
			val |= ctxt << (5 * (i % 6));
			i++;
			if (i % 6 == 0) {
				qib_write_kreg_port(ppd, regno, val);
				val = 0;
				regno++;
			}
		}
		qib_write_kreg_port(ppd, regno, val);
	}

	for (i = 0; i < dd->first_user_ctxt; i++) {
		dd->cspec->rcvavail_timeout[i] = rcv_int_timeout;
		qib_write_kreg(dd, kr_rcvavailtimeout + i, rcv_int_timeout);
	}

	val = TIDFLOW_ERRBITS; 
	for (i = 0; i < dd->cfgctxts; i++) {
		int flow;
		for (flow = 0; flow < NUM_TIDFLOWS_CTXT; flow++)
			qib_write_ureg(dd, ur_rcvflowtable+flow, val, i);
	}

	if (dd->num_pports)
		setup_7322_link_recovery(dd->pport, dd->num_pports > 1);
}

static int qib_init_7322_variables(struct qib_devdata *dd)
{
	struct qib_pportdata *ppd;
	unsigned features, pidx, sbufcnt;
	int ret, mtu;
	u32 sbufs, updthresh;

	
	ppd = (struct qib_pportdata *)(dd + 1);
	dd->pport = ppd;
	ppd[0].dd = dd;
	ppd[1].dd = dd;

	dd->cspec = (struct qib_chip_specific *)(ppd + 2);

	ppd[0].cpspec = (struct qib_chippport_specific *)(dd->cspec + 1);
	ppd[1].cpspec = &ppd[0].cpspec[1];
	ppd[0].cpspec->ppd = &ppd[0]; 
	ppd[1].cpspec->ppd = &ppd[1]; 

	spin_lock_init(&dd->cspec->rcvmod_lock);
	spin_lock_init(&dd->cspec->gpio_lock);

	
	dd->revision = readq(&dd->kregbase[kr_revision]);

	if ((dd->revision & 0xffffffffU) == 0xffffffffU) {
		qib_dev_err(dd, "Revision register read failure, "
			    "giving up initialization\n");
		ret = -ENODEV;
		goto bail;
	}
	dd->flags |= QIB_PRESENT;  

	dd->majrev = (u8) SYM_FIELD(dd->revision, Revision_R, ChipRevMajor);
	dd->minrev = (u8) SYM_FIELD(dd->revision, Revision_R, ChipRevMinor);
	dd->cspec->r1 = dd->minrev == 1;

	get_7322_chip_params(dd);
	features = qib_7322_boardname(dd);

	
	sbufcnt = dd->piobcnt2k + dd->piobcnt4k +
		NUM_VL15_BUFS + BITS_PER_LONG - 1;
	sbufcnt /= BITS_PER_LONG;
	dd->cspec->sendchkenable = kmalloc(sbufcnt *
		sizeof(*dd->cspec->sendchkenable), GFP_KERNEL);
	dd->cspec->sendgrhchk = kmalloc(sbufcnt *
		sizeof(*dd->cspec->sendgrhchk), GFP_KERNEL);
	dd->cspec->sendibchk = kmalloc(sbufcnt *
		sizeof(*dd->cspec->sendibchk), GFP_KERNEL);
	if (!dd->cspec->sendchkenable || !dd->cspec->sendgrhchk ||
		!dd->cspec->sendibchk) {
		qib_dev_err(dd, "Failed allocation for hdrchk bitmaps\n");
		ret = -ENOMEM;
		goto bail;
	}

	ppd = dd->pport;

	dd->gpio_sda_num = _QIB_GPIO_SDA_NUM;
	dd->gpio_scl_num = _QIB_GPIO_SCL_NUM;
	dd->twsi_eeprom_dev = QIB_TWSI_EEPROM_DEV;

	dd->flags |= QIB_HAS_INTX | QIB_HAS_LINK_LATENCY |
		QIB_NODMA_RTAIL | QIB_HAS_VLSUPP | QIB_HAS_HDRSUPP |
		QIB_HAS_THRESH_UPDATE |
		(sdma_idle_cnt ? QIB_HAS_SDMA_TIMEOUT : 0);
	dd->flags |= qib_special_trigger ?
		QIB_USE_SPCL_TRIG : QIB_HAS_SEND_DMA;

	qib_7322_set_baseaddrs(dd);

	mtu = ib_mtu_enum_to_int(qib_ibmtu);
	if (mtu == -1)
		mtu = QIB_DEFAULT_MTU;

	dd->cspec->int_enable_mask = QIB_I_BITSEXTANT;
	
	dd->cspec->hwerrmask = ~0ULL;
	dd->cspec->hwerrmask &=
		~(SYM_MASK(HwErrMask, IBSerdesPClkNotDetectMask_0) |
		  SYM_MASK(HwErrMask, IBSerdesPClkNotDetectMask_1) |
		  HWE_MASK(LATriggered));

	for (pidx = 0; pidx < NUM_IB_PORTS; ++pidx) {
		struct qib_chippport_specific *cp = ppd->cpspec;
		ppd->link_speed_supported = features & PORT_SPD_CAP;
		features >>=  PORT_SPD_CAP_SHIFT;
		if (!ppd->link_speed_supported) {
			
			dd->skip_kctxt_mask |= 1 << pidx;
			if (pidx == 0) {
				
				qib_write_kreg_port(ppd, krp_rcvctrl, 0);
				qib_write_kreg_port(ppd, krp_ibcctrl_a, 0);
				ppd[0] = ppd[1];
				dd->cspec->hwerrmask &= ~(SYM_MASK(HwErrMask,
						  IBSerdesPClkNotDetectMask_0)
						  | SYM_MASK(HwErrMask,
						  SDmaMemReadErrMask_0));
				dd->cspec->int_enable_mask &= ~(
				     SYM_MASK(IntMask, SDmaCleanupDoneMask_0) |
				     SYM_MASK(IntMask, SDmaIdleIntMask_0) |
				     SYM_MASK(IntMask, SDmaProgressIntMask_0) |
				     SYM_MASK(IntMask, SDmaIntMask_0) |
				     SYM_MASK(IntMask, ErrIntMask_0) |
				     SYM_MASK(IntMask, SendDoneIntMask_0));
			} else {
				
				qib_write_kreg_port(ppd, krp_rcvctrl, 0);
				qib_write_kreg_port(ppd, krp_ibcctrl_a, 0);
				dd->cspec->hwerrmask &= ~(SYM_MASK(HwErrMask,
						  IBSerdesPClkNotDetectMask_1)
						  | SYM_MASK(HwErrMask,
						  SDmaMemReadErrMask_1));
				dd->cspec->int_enable_mask &= ~(
				     SYM_MASK(IntMask, SDmaCleanupDoneMask_1) |
				     SYM_MASK(IntMask, SDmaIdleIntMask_1) |
				     SYM_MASK(IntMask, SDmaProgressIntMask_1) |
				     SYM_MASK(IntMask, SDmaIntMask_1) |
				     SYM_MASK(IntMask, ErrIntMask_1) |
				     SYM_MASK(IntMask, SendDoneIntMask_1));
			}
			continue;
		}

		dd->num_pports++;
		qib_init_pportdata(ppd, dd, pidx, dd->num_pports);

		ppd->link_width_supported = IB_WIDTH_1X | IB_WIDTH_4X;
		ppd->link_width_enabled = IB_WIDTH_4X;
		ppd->link_speed_enabled = ppd->link_speed_supported;
		ppd->link_width_active = IB_WIDTH_4X;
		ppd->link_speed_active = QIB_IB_SDR;
		ppd->delay_mult = ib_rate_to_delay[IB_RATE_10_GBPS];
		switch (qib_num_cfg_vls) {
		case 1:
			ppd->vls_supported = IB_VL_VL0;
			break;
		case 2:
			ppd->vls_supported = IB_VL_VL0_1;
			break;
		default:
			qib_devinfo(dd->pcidev,
				    "Invalid num_vls %u, using 4 VLs\n",
				    qib_num_cfg_vls);
			qib_num_cfg_vls = 4;
			
		case 4:
			ppd->vls_supported = IB_VL_VL0_3;
			break;
		case 8:
			if (mtu <= 2048)
				ppd->vls_supported = IB_VL_VL0_7;
			else {
				qib_devinfo(dd->pcidev,
					    "Invalid num_vls %u for MTU %d "
					    ", using 4 VLs\n",
					    qib_num_cfg_vls, mtu);
				ppd->vls_supported = IB_VL_VL0_3;
				qib_num_cfg_vls = 4;
			}
			break;
		}
		ppd->vls_operational = ppd->vls_supported;

		init_waitqueue_head(&cp->autoneg_wait);
		INIT_DELAYED_WORK(&cp->autoneg_work,
				  autoneg_7322_work);
		if (ppd->dd->cspec->r1)
			INIT_DELAYED_WORK(&cp->ipg_work, ipg_7322_work);

		if (!(dd->flags & QIB_HAS_QSFP)) {
			if (!IS_QMH(dd) && !IS_QME(dd))
				qib_devinfo(dd->pcidev, "IB%u:%u: "
					    "Unknown mezzanine card type\n",
					    dd->unit, ppd->port);
			cp->h1_val = IS_QMH(dd) ? H1_FORCE_QMH : H1_FORCE_QME;
			ppd->cpspec->no_eep = IS_QMH(dd) ?
				TXDDS_TABLE_SZ + 2 : TXDDS_TABLE_SZ + 4;
		} else
			cp->h1_val = H1_FORCE_VAL;

		
		if (!qib_mini_init)
			write_7322_init_portregs(ppd);

		init_timer(&cp->chase_timer);
		cp->chase_timer.function = reenable_chase;
		cp->chase_timer.data = (unsigned long)ppd;

		ppd++;
	}

	dd->rcvhdrentsize = qib_rcvhdrentsize ?
		qib_rcvhdrentsize : QIB_RCVHDR_ENTSIZE;
	dd->rcvhdrsize = qib_rcvhdrsize ?
		qib_rcvhdrsize : QIB_DFLT_RCVHDRSIZE;
	dd->rhf_offset = dd->rcvhdrentsize - sizeof(u64) / sizeof(u32);

	
	dd->rcvegrbufsize = max(mtu, 2048);
	BUG_ON(!is_power_of_2(dd->rcvegrbufsize));
	dd->rcvegrbufsize_shift = ilog2(dd->rcvegrbufsize);

	qib_7322_tidtemplate(dd);

	dd->rhdrhead_intr_off =
		(u64) rcv_int_count << IBA7322_HDRHEAD_PKTINT_SHIFT;

	
	init_timer(&dd->stats_timer);
	dd->stats_timer.function = qib_get_7322_faststats;
	dd->stats_timer.data = (unsigned long) dd;

	dd->ureg_align = 0x10000;  

	dd->piosize2kmax_dwords = dd->piosize2k >> 2;

	qib_7322_config_ctxts(dd);
	qib_set_ctxtcnt(dd);

	if (qib_wc_pat) {
		resource_size_t vl15off;
		ret = init_chip_wc_pat(dd, 0);
		if (ret)
			goto bail;

		
		vl15off = dd->physaddr + (dd->piobufbase >> 32) +
			dd->piobcnt4k * dd->align4k;
		dd->piovl15base	= ioremap_nocache(vl15off,
						  NUM_VL15_BUFS * dd->align4k);
		if (!dd->piovl15base)
			goto bail;
	}
	qib_7322_set_baseaddrs(dd); 

	ret = 0;
	if (qib_mini_init)
		goto bail;
	if (!dd->num_pports) {
		qib_dev_err(dd, "No ports enabled, giving up initialization\n");
		goto bail; 
	}

	write_7322_initregs(dd);
	ret = qib_create_ctxts(dd);
	init_7322_cntrnames(dd);

	updthresh = 8U; 

	if (dd->flags & QIB_HAS_SEND_DMA) {
		dd->cspec->sdmabufcnt = dd->piobcnt4k;
		sbufs = updthresh > 3 ? updthresh : 3;
	} else {
		dd->cspec->sdmabufcnt = 0;
		sbufs = dd->piobcnt4k;
	}
	dd->cspec->lastbuf_for_pio = dd->piobcnt2k + dd->piobcnt4k -
		dd->cspec->sdmabufcnt;
	dd->lastctxt_piobuf = dd->cspec->lastbuf_for_pio - sbufs;
	dd->cspec->lastbuf_for_pio--; 
	dd->pbufsctxt = (dd->cfgctxts > dd->first_user_ctxt) ?
		dd->lastctxt_piobuf / (dd->cfgctxts - dd->first_user_ctxt) : 0;

	if (dd->pbufsctxt >= 2 && dd->pbufsctxt - 2 < updthresh)
		updthresh = dd->pbufsctxt - 2;
	dd->cspec->updthresh_dflt = updthresh;
	dd->cspec->updthresh = updthresh;

	
	dd->sendctrl |= ((updthresh & SYM_RMASK(SendCtrl, AvailUpdThld))
			     << SYM_LSB(SendCtrl, AvailUpdThld)) |
			SYM_MASK(SendCtrl, SendBufAvailPad64Byte);

	dd->psxmitwait_supported = 1;
	dd->psxmitwait_check_rate = QIB_7322_PSXMITWAIT_CHECK_RATE;
bail:
	if (!dd->ctxtcnt)
		dd->ctxtcnt = 1; 

	return ret;
}

static u32 __iomem *qib_7322_getsendbuf(struct qib_pportdata *ppd, u64 pbc,
					u32 *pbufnum)
{
	u32 first, last, plen = pbc & QIB_PBC_LENGTH_MASK;
	struct qib_devdata *dd = ppd->dd;

	
	if (pbc & PBC_7322_VL15_SEND) {
		first = dd->piobcnt2k + dd->piobcnt4k + ppd->hw_pidx;
		last = first;
	} else {
		if ((plen + 1) > dd->piosize2kmax_dwords)
			first = dd->piobcnt2k;
		else
			first = 0;
		last = dd->cspec->lastbuf_for_pio;
	}
	return qib_getsendbuf_range(dd, pbufnum, first, last);
}

static void qib_set_cntr_7322_sample(struct qib_pportdata *ppd, u32 intv,
				     u32 start)
{
	qib_write_kreg_port(ppd, krp_psinterval, intv);
	qib_write_kreg_port(ppd, krp_psstart, start);
}

static void qib_sdma_set_7322_desc_cnt(struct qib_pportdata *ppd, unsigned cnt)
{
	qib_write_kreg_port(ppd, krp_senddmadesccnt, cnt);
}

static struct sdma_set_state_action sdma_7322_action_table[] = {
	[qib_sdma_state_s00_hw_down] = {
		.go_s99_running_tofalse = 1,
		.op_enable = 0,
		.op_intenable = 0,
		.op_halt = 0,
		.op_drain = 0,
	},
	[qib_sdma_state_s10_hw_start_up_wait] = {
		.op_enable = 0,
		.op_intenable = 1,
		.op_halt = 1,
		.op_drain = 0,
	},
	[qib_sdma_state_s20_idle] = {
		.op_enable = 1,
		.op_intenable = 1,
		.op_halt = 1,
		.op_drain = 0,
	},
	[qib_sdma_state_s30_sw_clean_up_wait] = {
		.op_enable = 0,
		.op_intenable = 1,
		.op_halt = 1,
		.op_drain = 0,
	},
	[qib_sdma_state_s40_hw_clean_up_wait] = {
		.op_enable = 1,
		.op_intenable = 1,
		.op_halt = 1,
		.op_drain = 0,
	},
	[qib_sdma_state_s50_hw_halt_wait] = {
		.op_enable = 1,
		.op_intenable = 1,
		.op_halt = 1,
		.op_drain = 1,
	},
	[qib_sdma_state_s99_running] = {
		.op_enable = 1,
		.op_intenable = 1,
		.op_halt = 0,
		.op_drain = 0,
		.go_s99_running_totrue = 1,
	},
};

static void qib_7322_sdma_init_early(struct qib_pportdata *ppd)
{
	ppd->sdma_state.set_state_action = sdma_7322_action_table;
}

static int init_sdma_7322_regs(struct qib_pportdata *ppd)
{
	struct qib_devdata *dd = ppd->dd;
	unsigned lastbuf, erstbuf;
	u64 senddmabufmask[3] = { 0 };
	int n, ret = 0;

	qib_write_kreg_port(ppd, krp_senddmabase, ppd->sdma_descq_phys);
	qib_sdma_7322_setlengen(ppd);
	qib_sdma_update_7322_tail(ppd, 0); 
	qib_write_kreg_port(ppd, krp_senddmareloadcnt, sdma_idle_cnt);
	qib_write_kreg_port(ppd, krp_senddmadesccnt, 0);
	qib_write_kreg_port(ppd, krp_senddmaheadaddr, ppd->sdma_head_phys);

	if (dd->num_pports)
		n = dd->cspec->sdmabufcnt / dd->num_pports; 
	else
		n = dd->cspec->sdmabufcnt; 
	erstbuf = (dd->piobcnt2k + dd->piobcnt4k) -
		((dd->num_pports == 1 || ppd->port == 2) ? n :
		dd->cspec->sdmabufcnt);
	lastbuf = erstbuf + n;

	ppd->sdma_state.first_sendbuf = erstbuf;
	ppd->sdma_state.last_sendbuf = lastbuf;
	for (; erstbuf < lastbuf; ++erstbuf) {
		unsigned word = erstbuf / BITS_PER_LONG;
		unsigned bit = erstbuf & (BITS_PER_LONG - 1);

		BUG_ON(word >= 3);
		senddmabufmask[word] |= 1ULL << bit;
	}
	qib_write_kreg_port(ppd, krp_senddmabufmask0, senddmabufmask[0]);
	qib_write_kreg_port(ppd, krp_senddmabufmask1, senddmabufmask[1]);
	qib_write_kreg_port(ppd, krp_senddmabufmask2, senddmabufmask[2]);
	return ret;
}

static u16 qib_sdma_7322_gethead(struct qib_pportdata *ppd)
{
	struct qib_devdata *dd = ppd->dd;
	int sane;
	int use_dmahead;
	u16 swhead;
	u16 swtail;
	u16 cnt;
	u16 hwhead;

	use_dmahead = __qib_sdma_running(ppd) &&
		(dd->flags & QIB_HAS_SDMA_TIMEOUT);
retry:
	hwhead = use_dmahead ?
		(u16) le64_to_cpu(*ppd->sdma_head_dma) :
		(u16) qib_read_kreg_port(ppd, krp_senddmahead);

	swhead = ppd->sdma_descq_head;
	swtail = ppd->sdma_descq_tail;
	cnt = ppd->sdma_descq_cnt;

	if (swhead < swtail)
		
		sane = (hwhead >= swhead) & (hwhead <= swtail);
	else if (swhead > swtail)
		
		sane = ((hwhead >= swhead) && (hwhead < cnt)) ||
			(hwhead <= swtail);
	else
		
		sane = (hwhead == swhead);

	if (unlikely(!sane)) {
		if (use_dmahead) {
			
			use_dmahead = 0;
			goto retry;
		}
		
		hwhead = swhead;
	}

	return hwhead;
}

static int qib_sdma_7322_busy(struct qib_pportdata *ppd)
{
	u64 hwstatus = qib_read_kreg_port(ppd, krp_senddmastatus);

	return (hwstatus & SYM_MASK(SendDmaStatus_0, ScoreBoardDrainInProg)) ||
	       (hwstatus & SYM_MASK(SendDmaStatus_0, HaltInProg)) ||
	       !(hwstatus & SYM_MASK(SendDmaStatus_0, InternalSDmaHalt)) ||
	       !(hwstatus & SYM_MASK(SendDmaStatus_0, ScbEmpty));
}

static u32 qib_7322_setpbc_control(struct qib_pportdata *ppd, u32 plen,
				   u8 srate, u8 vl)
{
	u8 snd_mult = ppd->delay_mult;
	u8 rcv_mult = ib_rate_to_delay[srate];
	u32 ret;

	ret = rcv_mult > snd_mult ? ((plen + 1) >> 1) * snd_mult : 0;

	
	if (vl == 15)
		ret |= PBC_7322_VL15_SEND_CTRL;
	else
		ret |= vl << PBC_VL_NUM_LSB;
	ret |= ((u32)(ppd->hw_pidx)) << PBC_PORT_SEL_LSB;

	return ret;
}

static void qib_7322_initvl15_bufs(struct qib_devdata *dd)
{
	unsigned vl15bufs;

	vl15bufs = dd->piobcnt2k + dd->piobcnt4k;
	qib_chg_pioavailkernel(dd, vl15bufs, NUM_VL15_BUFS,
			       TXCHK_CHG_TYPE_KERN, NULL);
}

static void qib_7322_init_ctxt(struct qib_ctxtdata *rcd)
{
	if (rcd->ctxt < NUM_IB_PORTS) {
		if (rcd->dd->num_pports > 1) {
			rcd->rcvegrcnt = KCTXT0_EGRCNT / 2;
			rcd->rcvegr_tid_base = rcd->ctxt ? rcd->rcvegrcnt : 0;
		} else {
			rcd->rcvegrcnt = KCTXT0_EGRCNT;
			rcd->rcvegr_tid_base = 0;
		}
	} else {
		rcd->rcvegrcnt = rcd->dd->cspec->rcvegrcnt;
		rcd->rcvegr_tid_base = KCTXT0_EGRCNT +
			(rcd->ctxt - NUM_IB_PORTS) * rcd->rcvegrcnt;
	}
}

#define QTXSLEEPS 5000
static void qib_7322_txchk_change(struct qib_devdata *dd, u32 start,
				  u32 len, u32 which, struct qib_ctxtdata *rcd)
{
	int i;
	const int last = start + len - 1;
	const int lastr = last / BITS_PER_LONG;
	u32 sleeps = 0;
	int wait = rcd != NULL;
	unsigned long flags;

	while (wait) {
		unsigned long shadow;
		int cstart, previ = -1;

		for (cstart = start; cstart <= last; cstart++) {
			i = ((2 * cstart) + QLOGIC_IB_SENDPIOAVAIL_BUSY_SHIFT)
				/ BITS_PER_LONG;
			if (i != previ) {
				shadow = (unsigned long)
					le64_to_cpu(dd->pioavailregs_dma[i]);
				previ = i;
			}
			if (test_bit(((2 * cstart) +
				      QLOGIC_IB_SENDPIOAVAIL_BUSY_SHIFT)
				     % BITS_PER_LONG, &shadow))
				break;
		}

		if (cstart > last)
			break;

		if (sleeps == QTXSLEEPS)
			break;
		
		sendctrl_7322_mod(dd->pport, QIB_SENDCTRL_AVAIL_BLIP);
		sleeps++;
		msleep(20);
	}

	switch (which) {
	case TXCHK_CHG_TYPE_DIS1:
		/*
		 * disable checking on a range; used by diags; just
		 * one buffer, but still written generically
		 */
		for (i = start; i <= last; i++)
			clear_bit(i, dd->cspec->sendchkenable);
		break;

	case TXCHK_CHG_TYPE_ENAB1:
		/*
		 * (re)enable checking on a range; used by diags; just
		 * one buffer, but still written generically; read
		 * scratch to be sure buffer actually triggered, not
		 * just flushed from processor.
		 */
		qib_read_kreg32(dd, kr_scratch);
		for (i = start; i <= last; i++)
			set_bit(i, dd->cspec->sendchkenable);
		break;

	case TXCHK_CHG_TYPE_KERN:
		
		for (i = start; i <= last; i++) {
			set_bit(i, dd->cspec->sendibchk);
			clear_bit(i, dd->cspec->sendgrhchk);
		}
		spin_lock_irqsave(&dd->uctxt_lock, flags);
		
		for (i = dd->first_user_ctxt;
		     dd->cspec->updthresh != dd->cspec->updthresh_dflt
		     && i < dd->cfgctxts; i++)
			if (dd->rcd[i] && dd->rcd[i]->subctxt_cnt &&
			   ((dd->rcd[i]->piocnt / dd->rcd[i]->subctxt_cnt) - 1)
			   < dd->cspec->updthresh_dflt)
				break;
		spin_unlock_irqrestore(&dd->uctxt_lock, flags);
		if (i == dd->cfgctxts) {
			spin_lock_irqsave(&dd->sendctrl_lock, flags);
			dd->cspec->updthresh = dd->cspec->updthresh_dflt;
			dd->sendctrl &= ~SYM_MASK(SendCtrl, AvailUpdThld);
			dd->sendctrl |= (dd->cspec->updthresh &
					 SYM_RMASK(SendCtrl, AvailUpdThld)) <<
					   SYM_LSB(SendCtrl, AvailUpdThld);
			spin_unlock_irqrestore(&dd->sendctrl_lock, flags);
			sendctrl_7322_mod(dd->pport, QIB_SENDCTRL_AVAIL_BLIP);
		}
		break;

	case TXCHK_CHG_TYPE_USER:
		
		for (i = start; i <= last; i++) {
			clear_bit(i, dd->cspec->sendibchk);
			set_bit(i, dd->cspec->sendgrhchk);
		}
		spin_lock_irqsave(&dd->sendctrl_lock, flags);
		if (rcd && rcd->subctxt_cnt && ((rcd->piocnt
			/ rcd->subctxt_cnt) - 1) < dd->cspec->updthresh) {
			dd->cspec->updthresh = (rcd->piocnt /
						rcd->subctxt_cnt) - 1;
			dd->sendctrl &= ~SYM_MASK(SendCtrl, AvailUpdThld);
			dd->sendctrl |= (dd->cspec->updthresh &
					SYM_RMASK(SendCtrl, AvailUpdThld))
					<< SYM_LSB(SendCtrl, AvailUpdThld);
			spin_unlock_irqrestore(&dd->sendctrl_lock, flags);
			sendctrl_7322_mod(dd->pport, QIB_SENDCTRL_AVAIL_BLIP);
		} else
			spin_unlock_irqrestore(&dd->sendctrl_lock, flags);
		break;

	default:
		break;
	}

	for (i = start / BITS_PER_LONG; which >= 2 && i <= lastr; ++i)
		qib_write_kreg(dd, kr_sendcheckmask + i,
			       dd->cspec->sendchkenable[i]);

	for (i = start / BITS_PER_LONG; which < 2 && i <= lastr; ++i) {
		qib_write_kreg(dd, kr_sendgrhcheckmask + i,
			       dd->cspec->sendgrhchk[i]);
		qib_write_kreg(dd, kr_sendibpktmask + i,
			       dd->cspec->sendibchk[i]);
	}

	qib_read_kreg32(dd, kr_scratch);
}


static void writescratch(struct qib_devdata *dd, u32 val)
{
	qib_write_kreg(dd, kr_scratch, val);
}

static int qib_7322_tempsense_rd(struct qib_devdata *dd, int regnum)
{
	return -ENXIO;
}

struct qib_devdata *qib_init_iba7322_funcs(struct pci_dev *pdev,
					   const struct pci_device_id *ent)
{
	struct qib_devdata *dd;
	int ret, i;
	u32 tabsize, actual_cnt = 0;

	dd = qib_alloc_devdata(pdev,
		NUM_IB_PORTS * sizeof(struct qib_pportdata) +
		sizeof(struct qib_chip_specific) +
		NUM_IB_PORTS * sizeof(struct qib_chippport_specific));
	if (IS_ERR(dd))
		goto bail;

	dd->f_bringup_serdes    = qib_7322_bringup_serdes;
	dd->f_cleanup           = qib_setup_7322_cleanup;
	dd->f_clear_tids        = qib_7322_clear_tids;
	dd->f_free_irq          = qib_7322_free_irq;
	dd->f_get_base_info     = qib_7322_get_base_info;
	dd->f_get_msgheader     = qib_7322_get_msgheader;
	dd->f_getsendbuf        = qib_7322_getsendbuf;
	dd->f_gpio_mod          = gpio_7322_mod;
	dd->f_eeprom_wen        = qib_7322_eeprom_wen;
	dd->f_hdrqempty         = qib_7322_hdrqempty;
	dd->f_ib_updown         = qib_7322_ib_updown;
	dd->f_init_ctxt         = qib_7322_init_ctxt;
	dd->f_initvl15_bufs     = qib_7322_initvl15_bufs;
	dd->f_intr_fallback     = qib_7322_intr_fallback;
	dd->f_late_initreg      = qib_late_7322_initreg;
	dd->f_setpbc_control    = qib_7322_setpbc_control;
	dd->f_portcntr          = qib_portcntr_7322;
	dd->f_put_tid           = qib_7322_put_tid;
	dd->f_quiet_serdes      = qib_7322_mini_quiet_serdes;
	dd->f_rcvctrl           = rcvctrl_7322_mod;
	dd->f_read_cntrs        = qib_read_7322cntrs;
	dd->f_read_portcntrs    = qib_read_7322portcntrs;
	dd->f_reset             = qib_do_7322_reset;
	dd->f_init_sdma_regs    = init_sdma_7322_regs;
	dd->f_sdma_busy         = qib_sdma_7322_busy;
	dd->f_sdma_gethead      = qib_sdma_7322_gethead;
	dd->f_sdma_sendctrl     = qib_7322_sdma_sendctrl;
	dd->f_sdma_set_desc_cnt = qib_sdma_set_7322_desc_cnt;
	dd->f_sdma_update_tail  = qib_sdma_update_7322_tail;
	dd->f_sendctrl          = sendctrl_7322_mod;
	dd->f_set_armlaunch     = qib_set_7322_armlaunch;
	dd->f_set_cntr_sample   = qib_set_cntr_7322_sample;
	dd->f_iblink_state      = qib_7322_iblink_state;
	dd->f_ibphys_portstate  = qib_7322_phys_portstate;
	dd->f_get_ib_cfg        = qib_7322_get_ib_cfg;
	dd->f_set_ib_cfg        = qib_7322_set_ib_cfg;
	dd->f_set_ib_loopback   = qib_7322_set_loopback;
	dd->f_get_ib_table      = qib_7322_get_ib_table;
	dd->f_set_ib_table      = qib_7322_set_ib_table;
	dd->f_set_intr_state    = qib_7322_set_intr_state;
	dd->f_setextled         = qib_setup_7322_setextled;
	dd->f_txchk_change      = qib_7322_txchk_change;
	dd->f_update_usrhead    = qib_update_7322_usrhead;
	dd->f_wantpiobuf_intr   = qib_wantpiobuf_7322_intr;
	dd->f_xgxs_reset        = qib_7322_mini_pcs_reset;
	dd->f_sdma_hw_clean_up  = qib_7322_sdma_hw_clean_up;
	dd->f_sdma_hw_start_up  = qib_7322_sdma_hw_start_up;
	dd->f_sdma_init_early   = qib_7322_sdma_init_early;
	dd->f_writescratch      = writescratch;
	dd->f_tempsense_rd	= qib_7322_tempsense_rd;
	ret = qib_pcie_ddinit(dd, pdev, ent);
	if (ret < 0)
		goto bail_free;

	
	ret = qib_init_7322_variables(dd);
	if (ret)
		goto bail_cleanup;

	if (qib_mini_init || !dd->num_pports)
		goto bail;

	tabsize = dd->first_user_ctxt + ARRAY_SIZE(irq_table);
	for (i = 0; i < tabsize; i++)
		if ((i < ARRAY_SIZE(irq_table) &&
		     irq_table[i].port <= dd->num_pports) ||
		    (i >= ARRAY_SIZE(irq_table) &&
		     dd->rcd[i - ARRAY_SIZE(irq_table)]))
			actual_cnt++;
	
	if (qib_krcvq01_no_msi)
		actual_cnt -= dd->num_pports;

	tabsize = actual_cnt;
	dd->cspec->msix_entries = kmalloc(tabsize *
			sizeof(struct qib_msix_entry), GFP_KERNEL);
	if (!dd->cspec->msix_entries) {
		qib_dev_err(dd, "No memory for MSIx table\n");
		tabsize = 0;
	}
	for (i = 0; i < tabsize; i++)
		dd->cspec->msix_entries[i].msix.entry = i;

	if (qib_pcie_params(dd, 8, &tabsize, dd->cspec->msix_entries))
		qib_dev_err(dd, "Failed to setup PCIe or interrupts; "
			    "continuing anyway\n");
	
	dd->cspec->num_msix_entries = tabsize;

	
	qib_setup_7322_interrupt(dd, 1);

	
	qib_write_kreg(dd, kr_hwdiagctrl, 0);

	goto bail;

bail_cleanup:
	qib_pcie_ddcleanup(dd);
bail_free:
	qib_free_devdata(dd);
	dd = ERR_PTR(ret);
bail:
	return dd;
}

#define DDS_ENT_AMP_LSB 14
#define DDS_ENT_MAIN_LSB 9
#define DDS_ENT_POST_LSB 5
#define DDS_ENT_PRE_XTRA_LSB 3
#define DDS_ENT_PRE_LSB 0

static void set_txdds(struct qib_pportdata *ppd, int ridx,
		      const struct txdds_ent *tp)
{
	struct qib_devdata *dd = ppd->dd;
	u32 pack_ent;
	int regidx;

	
	regidx = KREG_IBPORT_IDX(IBSD_DDS_MAP_TABLE) + ridx;
	if (ppd->hw_pidx)
		regidx += (dd->palign / sizeof(u64));

	pack_ent = tp->amp << DDS_ENT_AMP_LSB;
	pack_ent |= tp->main << DDS_ENT_MAIN_LSB;
	pack_ent |= tp->pre << DDS_ENT_PRE_LSB;
	pack_ent |= tp->post << DDS_ENT_POST_LSB;
	qib_write_kreg(dd, regidx, pack_ent);
	
	qib_write_kreg(ppd->dd, kr_scratch, 0);
}

static const struct vendor_txdds_ent vendor_txdds[] = {
	{ 
		{ 0x41, 0x50, 0x48 }, "584470002       ",
		{ 10,  0,  0,  5 }, { 10,  0,  0,  9 }, {  7,  1,  0, 13 },
	},
	{ 
		{ 0x41, 0x50, 0x48 }, "584470004       ",
		{  0,  0,  0,  8 }, {  0,  0,  0, 11 }, {  0,  1,  7, 15 },
	},
	{ 
		{ 0x00, 0x90, 0x65 }, "FCBG410QB1C03-QL",
		{  0,  0,  0,  3 }, {  0,  0,  0,  4 }, {  0,  0,  0, 13 },
	},
	{ 
		{ 0x00, 0x90, 0x65 }, "FCBG410QB1C30-QL",
		{  0,  0,  0,  1 }, {  0,  0,  0,  5 }, {  0,  0,  0, 11 },
	},
	{ 
		{ 0x00, 0x90, 0x65 }, NULL,
		{  0,  0,  0,  2 }, {  0,  0,  0,  5 }, {  0,  0,  0, 12 },
	},
	{ 
		{ 0x00, 0x21, 0x77 }, "QSN3300-1       ",
		{  0,  0,  0,  6 }, {  0,  0,  0,  9 }, {  0,  1,  0, 15 },
	},
	{ 
		{ 0x00, 0x21, 0x77 }, "QSN3300-2       ",
		{  0,  0,  0,  8 }, {  0,  0,  0, 10 }, {  0,  1,  7, 15 },
	},
	{ 
		{ 0x00, 0x21, 0x77 }, "QSN3800-1       ",
		{  0,  0,  0,  6 }, {  0,  0,  0,  8 }, {  0,  1,  0, 15 },
	},
	{ 
		{ 0x00, 0x21, 0x77 }, "QSN3800-3       ",
		{  0,  0,  0,  9 }, {  0,  0,  0, 13 }, {  0,  1,  7, 15 },
	},
	{ 
		{ 0x00, 0x21, 0x77 }, "QSN7000-5       ",
		{  0,  0,  0,  7 }, {  0,  0,  0,  9 }, {  0,  1,  3, 15 },
	},
	{ 
		{ 0x00, 0x21, 0x77 }, "QSN7000-7       ",
		{  0,  0,  0,  9 }, {  0,  0,  0, 11 }, {  0,  2,  6, 15 },
	},
	{ 
		{ 0x00, 0x21, 0x77 }, "QSN7600-5       ",
		{  0,  0,  0,  8 }, {  0,  0,  0, 11 }, {  0,  1,  9, 13 },
	},
	{ 
		{ 0x00, 0x21, 0x77 }, "QSN7600-7       ",
		{  0,  0,  0,  8 }, {  0,  0,  0, 11 }, {  10,  1,  8, 15 },
	},
	{ 
		{ 0x00, 0x30, 0xB4 }, "QLX4000CQSFP1224",
		{  0,  0,  0,  2 }, {  0,  0,  0,  5 }, {  0,  3,  0,  9 },
	},
	{ 
		{ 0x00, 0x30, 0xB4 }, "QLX4000CQSFP1028",
		{  0,  0,  0,  6 }, {  0,  0,  0,  4 }, {  0,  2,  0,  2 },
	},
	{ 
		{ 0x00, 0x30, 0xB4 }, "QLX4000CQSFP0730",
		{  0,  0,  0,  6 }, {  0,  0,  0,  4 }, {  0,  1,  0,  3 },
	},
	{ 
		{ 0x00, 0x30, 0xB4 }, "QLX4000CQSFP0532",
		{  0,  0,  0,  6 }, {  0,  0,  0,  6 }, {  0,  2,  0,  8 },
	},
	{ 
		{ 0x00, 0x30, 0xB4 }, NULL,
		{  0,  0,  0,  6 }, {  0,  0,  0,  5 }, {  0,  2,  0,  5 },
	},
	{ 
		{ 0x00, 0x25, 0x63 }, NULL,
		{  0,  0,  0,  5 }, {  0,  0,  0,  8 }, {  0,  2,  0,  12 },
	},
	{ 
		{ 0x00, 0x09, 0x3A }, "74763-0025      ",
		{  2,  2,  6, 15 }, {  2,  2,  6, 15 }, {  2,  2,  6, 15 },
	},
	{ 
		{ 0x00, 0x09, 0x3A }, "74757-2201      ",
		{  0,  0,  0,  6 }, {  0,  0,  0,  9 }, {  0,  1,  1, 15 },
	},
};

static const struct txdds_ent txdds_sdr[TXDDS_TABLE_SZ] = {
	
	{  2, 2, 15,  6 },	
	{  0, 0,  0,  1 },	
	{  0, 0,  0,  2 },	
	{  0, 0,  0,  3 },	
	{  0, 0,  0,  4 },	
	{  0, 0,  0,  5 },	
	{  0, 0,  0,  6 },	
	{  0, 0,  0,  7 },	
	{  0, 0,  0,  8 },	
	{  0, 0,  0,  9 },	
	{  0, 0,  0, 10 },	
	{  0, 0,  0, 11 },	
	{  0, 0,  0, 12 },	
	{  0, 0,  0, 13 },	
	{  0, 0,  0, 14 },	
	{  0, 0,  0, 15 },	
};

static const struct txdds_ent txdds_ddr[TXDDS_TABLE_SZ] = {
	
	{  2, 2, 15,  6 },	
	{  0, 0,  0,  8 },	
	{  0, 0,  0,  8 },	
	{  0, 0,  0,  9 },	
	{  0, 0,  0,  9 },	
	{  0, 0,  0, 10 },	
	{  0, 0,  0, 10 },	
	{  0, 0,  0, 11 },	
	{  0, 0,  0, 11 },	
	{  0, 0,  0, 12 },	
	{  0, 0,  0, 12 },	
	{  0, 0,  0, 13 },	
	{  0, 0,  0, 13 },	
	{  0, 0,  0, 14 },	
	{  0, 0,  0, 14 },	
	{  0, 0,  0, 15 },	
};

static const struct txdds_ent txdds_qdr[TXDDS_TABLE_SZ] = {
	
	{  2, 2, 15,  6 },	
	{  0, 1,  0,  7 },	
	{  0, 1,  0,  9 },	
	{  0, 1,  0, 11 },	
	{  0, 1,  0, 13 },	
	{  0, 1,  0, 15 },	
	{  0, 1,  3, 15 },	
	{  0, 1,  7, 15 },	
	{  0, 1,  7, 15 },	
	{  0, 1,  8, 15 },	
	{  0, 1,  9, 15 },	
	{  0, 1, 10, 15 },	
	{  0, 2,  6, 15 },	
	{  0, 2,  7, 15 },	
	{  0, 2,  8, 15 },	
	{  0, 2,  9, 15 },	
};

static const struct txdds_ent txdds_extra_sdr[TXDDS_EXTRA_SZ] = {
	
	{  0, 0, 0,  1 },	
	{  0, 0, 0,  1 },	
	{  0, 0, 0,  2 },	
	{  0, 0, 0,  2 },	
	{  0, 0, 0, 11 },	
	{  0, 0, 0, 11 },	
	{  0, 0, 0, 11 },	
	{  0, 0, 0, 11 },	
	{  0, 0, 0, 11 },	
	{  0, 0, 0, 11 },	
	{  0, 0, 0, 11 },	
	{  0, 0, 0,  3 },	
	{  0, 0, 0,  4 },	
};

static const struct txdds_ent txdds_extra_ddr[TXDDS_EXTRA_SZ] = {
	
	{  0, 0, 0,  7 },	
	{  0, 0, 0,  7 },	
	{  0, 0, 0,  8 },	
	{  0, 0, 0,  8 },	
	{  0, 0, 0, 13 },	
	{  0, 0, 0, 13 },	
	{  0, 0, 0, 13 },	
	{  0, 0, 0, 13 },	
	{  0, 0, 0, 13 },	
	{  0, 0, 0, 13 },	
	{  0, 0, 0, 13 },	
	{  0, 0, 0,  9 },	
	{  0, 0, 0, 10 },	
};

static const struct txdds_ent txdds_extra_qdr[TXDDS_EXTRA_SZ] = {
	
	{  0, 1,  0,  4 },	
	{  0, 1,  0,  5 },	
	{  0, 1,  0,  6 },	
	{  0, 1,  0,  8 },	
	{  0, 1, 12, 10 },	
	{  0, 1, 12, 11 },	
	{  0, 1, 12, 12 },	
	{  0, 1, 12, 14 },	
	{  0, 1, 12,  6 },	
	{  0, 1, 12,  7 },	
	{  0, 1, 12,  8 },	
	{  0, 1,  0, 10 },	
	{  0, 1,  0, 12 },	
};

static const struct txdds_ent txdds_extra_mfg[TXDDS_MFG_SZ] = {
	
	{ 0, 0, 0, 0 },         
	{ 0, 0, 0, 6 },         
};

static const struct txdds_ent *get_atten_table(const struct txdds_ent *txdds,
					       unsigned atten)
{
	if (atten <= 2)
		atten = 1;
	else if (atten > TXDDS_TABLE_SZ)
		atten = TXDDS_TABLE_SZ - 1;
	else
		atten--;
	return txdds + atten;
}

static void find_best_ent(struct qib_pportdata *ppd,
			  const struct txdds_ent **sdr_dds,
			  const struct txdds_ent **ddr_dds,
			  const struct txdds_ent **qdr_dds, int override)
{
	struct qib_qsfp_cache *qd = &ppd->cpspec->qsfp_data.cache;
	int idx;

	
	for (idx = 0; !override && idx < ARRAY_SIZE(vendor_txdds); ++idx) {
		const struct vendor_txdds_ent *v = vendor_txdds + idx;

		if (!memcmp(v->oui, qd->oui, QSFP_VOUI_LEN) &&
		    (!v->partnum ||
		     !memcmp(v->partnum, qd->partnum, QSFP_PN_LEN))) {
			*sdr_dds = &v->sdr;
			*ddr_dds = &v->ddr;
			*qdr_dds = &v->qdr;
			return;
		}
	}

	if (!override && QSFP_IS_ACTIVE(qd->tech)) {
		*sdr_dds = txdds_sdr + ppd->dd->board_atten;
		*ddr_dds = txdds_ddr + ppd->dd->board_atten;
		*qdr_dds = txdds_qdr + ppd->dd->board_atten;
		return;
	}

	if (!override && QSFP_HAS_ATTEN(qd->tech) && (qd->atten[0] ||
						      qd->atten[1])) {
		*sdr_dds = get_atten_table(txdds_sdr, qd->atten[0]);
		*ddr_dds = get_atten_table(txdds_ddr, qd->atten[0]);
		*qdr_dds = get_atten_table(txdds_qdr, qd->atten[1]);
		return;
	} else if (ppd->cpspec->no_eep < TXDDS_TABLE_SZ) {
		idx = ppd->cpspec->no_eep;
		*sdr_dds = &txdds_sdr[idx];
		*ddr_dds = &txdds_ddr[idx];
		*qdr_dds = &txdds_qdr[idx];
	} else if (ppd->cpspec->no_eep < (TXDDS_TABLE_SZ + TXDDS_EXTRA_SZ)) {
		
		idx = ppd->cpspec->no_eep - TXDDS_TABLE_SZ;
		*sdr_dds = &txdds_extra_sdr[idx];
		*ddr_dds = &txdds_extra_ddr[idx];
		*qdr_dds = &txdds_extra_qdr[idx];
	} else if ((IS_QME(ppd->dd) || IS_QMH(ppd->dd)) &&
		   ppd->cpspec->no_eep < (TXDDS_TABLE_SZ + TXDDS_EXTRA_SZ +
					  TXDDS_MFG_SZ)) {
		idx = ppd->cpspec->no_eep - (TXDDS_TABLE_SZ + TXDDS_EXTRA_SZ);
		printk(KERN_INFO QIB_DRV_NAME
			" IB%u:%u use idx %u into txdds_mfg\n",
			ppd->dd->unit, ppd->port, idx);
		*sdr_dds = &txdds_extra_mfg[idx];
		*ddr_dds = &txdds_extra_mfg[idx];
		*qdr_dds = &txdds_extra_mfg[idx];
	} else {
		
		*sdr_dds = txdds_sdr + qib_long_atten;
		*ddr_dds = txdds_ddr + qib_long_atten;
		*qdr_dds = txdds_qdr + qib_long_atten;
	}
}

static void init_txdds_table(struct qib_pportdata *ppd, int override)
{
	const struct txdds_ent *sdr_dds, *ddr_dds, *qdr_dds;
	struct txdds_ent *dds;
	int idx;
	int single_ent = 0;

	find_best_ent(ppd, &sdr_dds, &ddr_dds, &qdr_dds, override);

	
	if (!(ppd->dd->flags & QIB_HAS_QSFP) || override)
		single_ent = 1;

	
	set_txdds(ppd, 0, sdr_dds);
	set_txdds(ppd, TXDDS_TABLE_SZ, ddr_dds);
	set_txdds(ppd, 2 * TXDDS_TABLE_SZ, qdr_dds);
	if (ppd->lflags & (QIBL_LINKINIT | QIBL_LINKARMED |
		QIBL_LINKACTIVE)) {
		dds = (struct txdds_ent *)(ppd->link_speed_active ==
					   QIB_IB_QDR ?  qdr_dds :
					   (ppd->link_speed_active ==
					    QIB_IB_DDR ? ddr_dds : sdr_dds));
		write_tx_serdes_param(ppd, dds);
	}

	
	for (idx = 1; idx < ARRAY_SIZE(txdds_sdr); ++idx) {
		set_txdds(ppd, idx, single_ent ? sdr_dds : txdds_sdr + idx);
		set_txdds(ppd, idx + TXDDS_TABLE_SZ,
			  single_ent ? ddr_dds : txdds_ddr + idx);
		set_txdds(ppd, idx + 2 * TXDDS_TABLE_SZ,
			  single_ent ? qdr_dds : txdds_qdr + idx);
	}
}

#define KR_AHB_ACC KREG_IDX(ahb_access_ctrl)
#define KR_AHB_TRANS KREG_IDX(ahb_transaction_reg)
#define AHB_TRANS_RDY SYM_MASK(ahb_transaction_reg, ahb_rdy)
#define AHB_ADDR_LSB SYM_LSB(ahb_transaction_reg, ahb_address)
#define AHB_DATA_LSB SYM_LSB(ahb_transaction_reg, ahb_data)
#define AHB_WR SYM_MASK(ahb_transaction_reg, write_not_read)
#define AHB_TRANS_TRIES 10

static u32 ahb_mod(struct qib_devdata *dd, int quad, int chan, int addr,
		    u32 data, u32 mask)
{
	u32 rd_data, wr_data, sz_mask;
	u64 trans, acc, prev_acc;
	u32 ret = 0xBAD0BAD;
	int tries;

	prev_acc = qib_read_kreg64(dd, KR_AHB_ACC);
	
	acc = (quad << 1) | 1;
	qib_write_kreg(dd, KR_AHB_ACC, acc);

	for (tries = 1; tries < AHB_TRANS_TRIES; ++tries) {
		trans = qib_read_kreg64(dd, KR_AHB_TRANS);
		if (trans & AHB_TRANS_RDY)
			break;
	}
	if (tries >= AHB_TRANS_TRIES) {
		qib_dev_err(dd, "No ahb_rdy in %d tries\n", AHB_TRANS_TRIES);
		goto bail;
	}

	sz_mask = (1UL << ((quad == 1) ? 32 : 16)) - 1;
	wr_data = data & mask & sz_mask;
	if ((~mask & sz_mask) != 0) {
		trans = ((chan << 6) | addr) << (AHB_ADDR_LSB + 1);
		qib_write_kreg(dd, KR_AHB_TRANS, trans);

		for (tries = 1; tries < AHB_TRANS_TRIES; ++tries) {
			trans = qib_read_kreg64(dd, KR_AHB_TRANS);
			if (trans & AHB_TRANS_RDY)
				break;
		}
		if (tries >= AHB_TRANS_TRIES) {
			qib_dev_err(dd, "No Rd ahb_rdy in %d tries\n",
				    AHB_TRANS_TRIES);
			goto bail;
		}
		
		trans = qib_read_kreg64(dd, KR_AHB_TRANS);
		rd_data = (uint32_t)(trans >> AHB_DATA_LSB);
		wr_data |= (rd_data & ~mask & sz_mask);
	}

	
	if (mask & sz_mask) {
		trans = ((chan << 6) | addr) << (AHB_ADDR_LSB + 1);
		trans |= ((uint64_t)wr_data << AHB_DATA_LSB);
		trans |= AHB_WR;
		qib_write_kreg(dd, KR_AHB_TRANS, trans);

		for (tries = 1; tries < AHB_TRANS_TRIES; ++tries) {
			trans = qib_read_kreg64(dd, KR_AHB_TRANS);
			if (trans & AHB_TRANS_RDY)
				break;
		}
		if (tries >= AHB_TRANS_TRIES) {
			qib_dev_err(dd, "No Wr ahb_rdy in %d tries\n",
				    AHB_TRANS_TRIES);
			goto bail;
		}
	}
	ret = wr_data;
bail:
	qib_write_kreg(dd, KR_AHB_ACC, prev_acc);
	return ret;
}

static void ibsd_wr_allchans(struct qib_pportdata *ppd, int addr, unsigned data,
			     unsigned mask)
{
	struct qib_devdata *dd = ppd->dd;
	int chan;
	u32 rbc;

	for (chan = 0; chan < SERDES_CHANS; ++chan) {
		ahb_mod(dd, IBSD(ppd->hw_pidx), (chan + (chan >> 1)), addr,
			data, mask);
		rbc = ahb_mod(dd, IBSD(ppd->hw_pidx), (chan + (chan >> 1)),
			      addr, 0, 0);
	}
}

static void serdes_7322_los_enable(struct qib_pportdata *ppd, int enable)
{
	u64 data = qib_read_kreg_port(ppd, krp_serdesctrl);
	u8 state = SYM_FIELD(data, IBSerdesCtrl_0, RXLOSEN);

	if (enable && !state) {
		printk(KERN_INFO QIB_DRV_NAME " IB%u:%u Turning LOS on\n",
			ppd->dd->unit, ppd->port);
		data |= SYM_MASK(IBSerdesCtrl_0, RXLOSEN);
	} else if (!enable && state) {
		printk(KERN_INFO QIB_DRV_NAME " IB%u:%u Turning LOS off\n",
			ppd->dd->unit, ppd->port);
		data &= ~SYM_MASK(IBSerdesCtrl_0, RXLOSEN);
	}
	qib_write_kreg_port(ppd, krp_serdesctrl, data);
}

static int serdes_7322_init(struct qib_pportdata *ppd)
{
	int ret = 0;
	if (ppd->dd->cspec->r1)
		ret = serdes_7322_init_old(ppd);
	else
		ret = serdes_7322_init_new(ppd);
	return ret;
}

static int serdes_7322_init_old(struct qib_pportdata *ppd)
{
	u32 le_val;

	init_txdds_table(ppd, 0);

	
	qib_write_kreg_port(ppd, krp_tx_deemph_override,
		SYM_MASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
		reset_tx_deemphasis_override));

	
	
	ibsd_wr_allchans(ppd, 2, 0, BMASK(11, 9));

	
	ibsd_wr_allchans(ppd, 11, (1 << 11), BMASK(12, 11));
	
	ibsd_wr_allchans(ppd, 13, (1 << 6), (1 << 6));

	
	le_val = IS_QME(ppd->dd) ? LE2_QME : LE2_DEFAULT;
	ibsd_wr_allchans(ppd, 13, (le_val << 7), BMASK(9, 7));

	
	le_val = IS_QME(ppd->dd) ? 0 : 1;
	ibsd_wr_allchans(ppd, 13, (le_val << 5), (1 << 5));

	
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 10, 0 << 14, 1 << 14);

	
	ibsd_wr_allchans(ppd, 5, (0 << 8), BMASK(9, 8));

	
	
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 5, 8 << 11, BMASK(14, 11));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 7, 8 << 4, BMASK(7, 4));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 8, 8 << 11, BMASK(14, 11));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 10, 8 << 4, BMASK(7, 4));

	
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 6, 4 << 0, BMASK(3, 0));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 7, 4 << 8, BMASK(11, 8));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 9, 4 << 0, BMASK(3, 0));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 10, 4 << 8, BMASK(11, 8));

	
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 9, 1 << 15, 1 << 15);

	
	ibsd_wr_allchans(ppd, 14, (1 << 3), BMASK(5, 3)); 
	ibsd_wr_allchans(ppd, 20, (2 << 10), BMASK(12, 10)); 
	ibsd_wr_allchans(ppd, 20, (4 << 13), BMASK(15, 13)); 

	serdes_7322_los_enable(ppd, 1);

	
	ibsd_wr_allchans(ppd, 9, 0 << 15, 1 << 15);

	
	ibsd_wr_allchans(ppd, 16, 0 << 0, BMASK(1, 0));

	
	le_val = (ppd->dd->cspec->r1 || IS_QME(ppd->dd)) ? 0xb6c0 : 0x6bac;
	ibsd_wr_allchans(ppd, 21, le_val, 0xfffe);

	qib_write_kreg_port(ppd, krp_static_adapt_dis(0), 0ULL);
	qib_write_kreg_port(ppd, krp_static_adapt_dis(1), 0ULL);
	qib_write_kreg_port(ppd, krp_static_adapt_dis(2),
			    ppd->dd->cspec->r1 ?
			    QDR_STATIC_ADAPT_DOWN_R1 : QDR_STATIC_ADAPT_DOWN);
	ppd->cpspec->qdr_dfe_on = 1;

	
	ibsd_wr_allchans(ppd, 38, 0 << 10, 1 << 10);

	
	ibsd_wr_allchans(ppd, 12, 1 << 4, 1 << 4);

	if (!ppd->dd->cspec->r1) {
		ibsd_wr_allchans(ppd, 12, 1 << 12, 1 << 12);
		ibsd_wr_allchans(ppd, 12, 2 << 8, 0x0f << 8);
	}

	
	ibsd_wr_allchans(ppd, 2, 15 << 5, BMASK(8, 5));

	return 0;
}

static int serdes_7322_init_new(struct qib_pportdata *ppd)
{
	unsigned long tend;
	u32 le_val, rxcaldone;
	int chan, chan_done = (1 << SERDES_CHANS) - 1;

	
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 10, 0 << 14, 1 << 14);

	
	qib_write_kreg_port(ppd, krp_tx_deemph_override,
		SYM_MASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
		reset_tx_deemphasis_override));

	
	
	
	ibsd_wr_allchans(ppd, 1, 0, BMASK(9, 1));
	
	ibsd_wr_allchans(ppd, 13, 0, BMASK(5, 5));
	
	ibsd_wr_allchans(ppd, 1, 0, BMASK(15, 15));
	
	ibsd_wr_allchans(ppd, 13, 0, BMASK(6, 6));
	
	ibsd_wr_allchans(ppd, 5, 0, BMASK(0, 0));
	
	ibsd_wr_allchans(ppd, 12, 0, BMASK(12, 12));
	
	ibsd_wr_allchans(ppd, 2, 0, BMASK(3, 3));
	
	ibsd_wr_allchans(ppd, 2, 0, BMASK(4, 4));
	
	ibsd_wr_allchans(ppd, 13, 0, BMASK(13, 13));
	
	ibsd_wr_allchans(ppd, 4, 0, BMASK(10, 10));
	
	ibsd_wr_allchans(ppd, 12, 0, BMASK(4, 4));
	
	ibsd_wr_allchans(ppd, 2, (1 << 15), BMASK(15, 15));
	
	ibsd_wr_allchans(ppd, 5, 0, BMASK(9, 8));
	
	ibsd_wr_allchans(ppd, 12, (1 << 5), BMASK(5, 5));
	
	ibsd_wr_allchans(ppd, 2, (4 << 12), BMASK(14, 12));
	
	ibsd_wr_allchans(ppd, 16, 0, BMASK(1, 0));
	
	if (!ppd->dd->cspec->r1) {
		ibsd_wr_allchans(ppd, 12, 1 << 12, BMASK(12, 12));
		ibsd_wr_allchans(ppd, 12, 2 << 8, BMASK(11, 8));
	} else {
		ibsd_wr_allchans(ppd, 19, (3 << 11), BMASK(13, 11));
	}
	
	
	
	

	
	
	
	
	ibsd_wr_allchans(ppd, 0, 0, BMASK(15, 13));
	msleep(20);
	
	ibsd_wr_allchans(ppd, 0, (1 << 14), BMASK(14, 14));
	msleep(20);
	
	ibsd_wr_allchans(ppd, 0, (1 << 13), BMASK(13, 13));
	msleep(20);

	
	
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 5, 8 << 11, BMASK(14, 11));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 7, 8 << 4, BMASK(7, 4));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 8, 8 << 11, BMASK(14, 11));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 10, 8 << 4, BMASK(7, 4));

	
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 6, 4 << 0, BMASK(3, 0));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 7, 4 << 8, BMASK(11, 8));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 9, 4 << 0, BMASK(3, 0));
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 10, 4 << 8, BMASK(11, 8));

	
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), 5, 9, 1 << 15, 1 << 15);

	
	ibsd_wr_allchans(ppd, 14, (1 << 3), BMASK(5, 3)); 
	ibsd_wr_allchans(ppd, 20, (2 << 10), BMASK(12, 10)); 
	ibsd_wr_allchans(ppd, 20, (4 << 13), BMASK(15, 13)); 

	
	serdes_7322_los_enable(ppd, 1);
	
	ibsd_wr_allchans(ppd, 38, 0 << 10, 1 << 10);

	
	
	ibsd_wr_allchans(ppd, 15, 1, BMASK(0, 0));
	
	ibsd_wr_allchans(ppd, 12, (1 << 4), BMASK(4, 4));
	msleep(20);
	
	ibsd_wr_allchans(ppd, 4, (1 << 10), BMASK(10, 10));
	tend = jiffies + msecs_to_jiffies(500);
	while (chan_done && !time_is_before_jiffies(tend)) {
		msleep(20);
		for (chan = 0; chan < SERDES_CHANS; ++chan) {
			rxcaldone = ahb_mod(ppd->dd, IBSD(ppd->hw_pidx),
					    (chan + (chan >> 1)),
					    25, 0, 0);
			if ((~rxcaldone & (u32)BMASK(9, 9)) == 0 &&
			    (~chan_done & (1 << chan)) == 0)
				chan_done &= ~(1 << chan);
		}
	}
	if (chan_done) {
		printk(KERN_INFO QIB_DRV_NAME
			 " Serdes %d calibration not done after .5 sec: 0x%x\n",
			 IBSD(ppd->hw_pidx), chan_done);
	} else {
		for (chan = 0; chan < SERDES_CHANS; ++chan) {
			rxcaldone = ahb_mod(ppd->dd, IBSD(ppd->hw_pidx),
					    (chan + (chan >> 1)),
					    25, 0, 0);
			if ((~rxcaldone & (u32)BMASK(10, 10)) == 0)
				printk(KERN_INFO QIB_DRV_NAME
					 " Serdes %d chan %d calibration "
					 "failed\n", IBSD(ppd->hw_pidx), chan);
		}
	}

	
	ibsd_wr_allchans(ppd, 4, 0, BMASK(10, 10));
	msleep(20);

	
	
	le_val = IS_QME(ppd->dd) ? LE2_QME : LE2_DEFAULT;
	ibsd_wr_allchans(ppd, 13, (le_val << 7), BMASK(9, 7));
	
	ibsd_wr_allchans(ppd, 3, (7 << 5), BMASK(7, 5));
	
	ibsd_wr_allchans(ppd, 13, (1 << 6), BMASK(6, 6));
	msleep(20);
	
	ibsd_wr_allchans(ppd, 1, 1, BMASK(9, 1));
	
	le_val = (ppd->dd->cspec->r1 || IS_QME(ppd->dd)) ? 0xb6c0 : 0x6bac;
	ibsd_wr_allchans(ppd, 21, le_val, 0xfffe);
	
	ibsd_wr_allchans(ppd, 5, 0, BMASK(0, 0));
	msleep(20);
	
	ibsd_wr_allchans(ppd, 2, (7 << 5), BMASK(8, 5));
	
	ibsd_wr_allchans(ppd, 2, (1 << 4), BMASK(4, 4));
	
	ibsd_wr_allchans(ppd, 2, 0, BMASK(11, 9));
	
	ibsd_wr_allchans(ppd, 2, (1 << 3), BMASK(3, 3));
	msleep(50);
	qib_write_kreg_port(ppd, krp_static_adapt_dis(0), 0ULL);
	qib_write_kreg_port(ppd, krp_static_adapt_dis(1), 0ULL);
	qib_write_kreg_port(ppd, krp_static_adapt_dis(2),
			    ppd->dd->cspec->r1 ?
			    QDR_STATIC_ADAPT_DOWN_R1 : QDR_STATIC_ADAPT_DOWN);
	ppd->cpspec->qdr_dfe_on = 1;
	
	ibsd_wr_allchans(ppd, 13, (0 << 5), (1 << 5));
	
	ibsd_wr_allchans(ppd, 1, (0 << 15), BMASK(15, 15));
	msleep(20);
	
	ibsd_wr_allchans(ppd, 12, (1 << 12), BMASK(12, 12));
	
	ibsd_wr_allchans(ppd, 12, (1 << 13), BMASK(13, 13));
	
	ibsd_wr_allchans(ppd, 11, (1 << 11), BMASK(12, 11));
	
	ibsd_wr_allchans(ppd, 12, (3 << 2), BMASK(3, 2));

	init_txdds_table(ppd, 0);

	return 0;
}


static void set_man_code(struct qib_pportdata *ppd, int chan, int code)
{
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), (chan + (chan >> 1)),
		9, code << 9, 0x3f << 9);
}

static void set_man_mode_h1(struct qib_pportdata *ppd, int chan,
	int enable, u32 tapenable)
{
	if (enable)
		ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), (chan + (chan >> 1)),
			1, 3 << 10, 0x1f << 10);
	else
		ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), (chan + (chan >> 1)),
			1, 0, 0x1f << 10);
}

static void clock_man(struct qib_pportdata *ppd, int chan)
{
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), (chan + (chan >> 1)),
		4, 0x4000, 0x4000);
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), (chan + (chan >> 1)),
		4, 0, 0x4000);
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), (chan + (chan >> 1)),
		4, 0x4000, 0x4000);
	ahb_mod(ppd->dd, IBSD(ppd->hw_pidx), (chan + (chan >> 1)),
		4, 0, 0x4000);
}

static void write_tx_serdes_param(struct qib_pportdata *ppd,
				  struct txdds_ent *txdds)
{
	u64 deemph;

	deemph = qib_read_kreg_port(ppd, krp_tx_deemph_override);
	
	deemph &= ~(SYM_MASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0, txampcntl_d2a) |
		    SYM_MASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0, txc0_ena) |
		    SYM_MASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0, txcp1_ena) |
		    SYM_MASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0, txcn1_ena));

	deemph |= SYM_MASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
			   tx_override_deemphasis_select);
	deemph |= (txdds->amp & SYM_RMASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
		    txampcntl_d2a)) << SYM_LSB(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
				       txampcntl_d2a);
	deemph |= (txdds->main & SYM_RMASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
		     txc0_ena)) << SYM_LSB(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
				   txc0_ena);
	deemph |= (txdds->post & SYM_RMASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
		     txcp1_ena)) << SYM_LSB(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
				    txcp1_ena);
	deemph |= (txdds->pre & SYM_RMASK(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
		     txcn1_ena)) << SYM_LSB(IBSD_TX_DEEMPHASIS_OVERRIDE_0,
				    txcn1_ena);
	qib_write_kreg_port(ppd, krp_tx_deemph_override, deemph);
}

static void adj_tx_serdes(struct qib_pportdata *ppd)
{
	const struct txdds_ent *sdr_dds, *ddr_dds, *qdr_dds;
	struct txdds_ent *dds;

	find_best_ent(ppd, &sdr_dds, &ddr_dds, &qdr_dds, 1);
	dds = (struct txdds_ent *)(ppd->link_speed_active == QIB_IB_QDR ?
		qdr_dds : (ppd->link_speed_active == QIB_IB_DDR ?
				ddr_dds : sdr_dds));
	write_tx_serdes_param(ppd, dds);
}

static void force_h1(struct qib_pportdata *ppd)
{
	int chan;

	ppd->cpspec->qdr_reforce = 0;
	if (!ppd->dd->cspec->r1)
		return;

	for (chan = 0; chan < SERDES_CHANS; chan++) {
		set_man_mode_h1(ppd, chan, 1, 0);
		set_man_code(ppd, chan, ppd->cpspec->h1_val);
		clock_man(ppd, chan);
		set_man_mode_h1(ppd, chan, 0, 0);
	}
}

#define SJA_EN SYM_MASK(SPC_JTAG_ACCESS_REG, SPC_JTAG_ACCESS_EN)
#define BISTEN_LSB SYM_LSB(SPC_JTAG_ACCESS_REG, bist_en)

#define R_OPCODE_LSB 3
#define R_OP_NOP 0
#define R_OP_SHIFT 2
#define R_OP_UPDATE 3
#define R_TDI_LSB 2
#define R_TDO_LSB 1
#define R_RDY 1

static int qib_r_grab(struct qib_devdata *dd)
{
	u64 val;
	val = SJA_EN;
	qib_write_kreg(dd, kr_r_access, val);
	qib_read_kreg32(dd, kr_scratch);
	return 0;
}

static int qib_r_wait_for_rdy(struct qib_devdata *dd)
{
	u64 val;
	int timeout;
	for (timeout = 0; timeout < 100 ; ++timeout) {
		val = qib_read_kreg32(dd, kr_r_access);
		if (val & R_RDY)
			return (val >> R_TDO_LSB) & 1;
	}
	return -1;
}

static int qib_r_shift(struct qib_devdata *dd, int bisten,
		       int len, u8 *inp, u8 *outp)
{
	u64 valbase, val;
	int ret, pos;

	valbase = SJA_EN | (bisten << BISTEN_LSB) |
		(R_OP_SHIFT << R_OPCODE_LSB);
	ret = qib_r_wait_for_rdy(dd);
	if (ret < 0)
		goto bail;
	for (pos = 0; pos < len; ++pos) {
		val = valbase;
		if (outp) {
			outp[pos >> 3] &= ~(1 << (pos & 7));
			outp[pos >> 3] |= (ret << (pos & 7));
		}
		if (inp) {
			int tdi = inp[pos >> 3] >> (pos & 7);
			val |= ((tdi & 1) << R_TDI_LSB);
		}
		qib_write_kreg(dd, kr_r_access, val);
		qib_read_kreg32(dd, kr_scratch);
		ret = qib_r_wait_for_rdy(dd);
		if (ret < 0)
			break;
	}
	
	val =  SJA_EN | (bisten << BISTEN_LSB);
	qib_write_kreg(dd, kr_r_access, val);
	qib_read_kreg32(dd, kr_scratch);
	ret = qib_r_wait_for_rdy(dd);

	if (ret >= 0)
		ret = pos;
bail:
	return ret;
}

static int qib_r_update(struct qib_devdata *dd, int bisten)
{
	u64 val;
	int ret;

	val = SJA_EN | (bisten << BISTEN_LSB) | (R_OP_UPDATE << R_OPCODE_LSB);
	ret = qib_r_wait_for_rdy(dd);
	if (ret >= 0) {
		qib_write_kreg(dd, kr_r_access, val);
		qib_read_kreg32(dd, kr_scratch);
	}
	return ret;
}

#define BISTEN_PORT_SEL 15
#define LEN_PORT_SEL 625
#define BISTEN_AT 17
#define LEN_AT 156
#define BISTEN_ETM 16
#define LEN_ETM 632

#define BIT2BYTE(x) (((x) +  BITS_PER_BYTE - 1) / BITS_PER_BYTE)

static u8 reset_at[BIT2BYTE(LEN_AT)] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00,
};
static u8 reset_atetm[BIT2BYTE(LEN_ETM)] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x80, 0xe3, 0x81, 0x73, 0x3c, 0x70, 0x8e,
	0x07, 0xce, 0xf1, 0xc0, 0x39, 0x1e, 0x38, 0xc7, 0x03, 0xe7,
	0x78, 0xe0, 0x1c, 0x0f, 0x9c, 0x7f, 0x80, 0x73, 0x0f, 0x70,
	0xde, 0x01, 0xce, 0x39, 0xc0, 0xf9, 0x06, 0x38, 0xd7, 0x00,
	0xe7, 0x19, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
};
static u8 at[BIT2BYTE(LEN_AT)] = {
	0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00,
};

static u8 atetm_1port[BIT2BYTE(LEN_ETM)] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0xf2, 0x80, 0x83, 0x1e, 0x38, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x50, 0xf4, 0x41, 0x00, 0x18, 0x78, 0xc8, 0x03,
	0x07, 0x7b, 0xa0, 0x3e, 0x00, 0x02, 0x00, 0x00, 0x18, 0x00,
	0x18, 0x00, 0x00, 0x00, 0x00, 0x4b, 0x00, 0x00, 0x00,
};

static u8 atetm_2port[BIT2BYTE(LEN_ETM)] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79,
	0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xf8, 0x80, 0x83, 0x1e, 0x38, 0xe0, 0x03, 0x05,
	0x7b, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
	0xa2, 0x0f, 0x50, 0xf4, 0x41, 0x00, 0x18, 0x78, 0xd1, 0x07,
	0x02, 0x7c, 0x80, 0x3e, 0x00, 0x02, 0x00, 0x00, 0x3e, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00,
};

static u8 portsel_port1[BIT2BYTE(LEN_PORT_SEL)] = {
	0x32, 0x65, 0xa4, 0x7b, 0x10, 0x98, 0xdc, 0xfe, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x73, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x78, 0x78, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x74, 0x32,
	0x32, 0x32, 0x32, 0x32, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x9f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 portsel_port2[BIT2BYTE(LEN_PORT_SEL)] = {
	0x32, 0x65, 0xa4, 0x7b, 0x10, 0x98, 0xdc, 0xfe, 0x39, 0x39,
	0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x73, 0x32, 0x32, 0x32,
	0x32, 0x32, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
	0x39, 0x78, 0x78, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
	0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x74, 0x32,
	0x32, 0x32, 0x32, 0x32, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a,
	0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a,
	0x3a, 0x3a, 0x9f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
};

static u8 portsel_2port[BIT2BYTE(LEN_PORT_SEL)] = {
	0x32, 0xba, 0x54, 0x76, 0x10, 0x98, 0xdc, 0xfe, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x73, 0x0c, 0x0c, 0x0c,
	0x0c, 0x0c, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x74, 0x32,
	0x32, 0x32, 0x32, 0x32, 0x14, 0x14, 0x14, 0x14, 0x14, 0x3a,
	0x3a, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x9f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void setup_7322_link_recovery(struct qib_pportdata *ppd, u32 both)
{
	u8 *portsel, *etm;
	struct qib_devdata *dd = ppd->dd;

	if (!ppd->dd->cspec->r1)
		return;
	if (!both) {
		dd->cspec->recovery_ports_initted++;
		ppd->cpspec->recovery_init = 1;
	}
	if (!both && dd->cspec->recovery_ports_initted == 1) {
		portsel = ppd->port == 1 ? portsel_port1 : portsel_port2;
		etm = atetm_1port;
	} else {
		portsel = portsel_2port;
		etm = atetm_2port;
	}

	if (qib_r_grab(dd) < 0 ||
		qib_r_shift(dd, BISTEN_ETM, LEN_ETM, reset_atetm, NULL) < 0 ||
		qib_r_update(dd, BISTEN_ETM) < 0 ||
		qib_r_shift(dd, BISTEN_AT, LEN_AT, reset_at, NULL) < 0 ||
		qib_r_update(dd, BISTEN_AT) < 0 ||
		qib_r_shift(dd, BISTEN_PORT_SEL, LEN_PORT_SEL,
			    portsel, NULL) < 0 ||
		qib_r_update(dd, BISTEN_PORT_SEL) < 0 ||
		qib_r_shift(dd, BISTEN_AT, LEN_AT, at, NULL) < 0 ||
		qib_r_update(dd, BISTEN_AT) < 0 ||
		qib_r_shift(dd, BISTEN_ETM, LEN_ETM, etm, NULL) < 0 ||
		qib_r_update(dd, BISTEN_ETM) < 0)
		qib_dev_err(dd, "Failed IB link recovery setup\n");
}

static void check_7322_rxe_status(struct qib_pportdata *ppd)
{
	struct qib_devdata *dd = ppd->dd;
	u64 fmask;

	if (dd->cspec->recovery_ports_initted != 1)
		return; 
	qib_write_kreg(dd, kr_control, dd->control |
		       SYM_MASK(Control, FreezeMode));
	(void)qib_read_kreg64(dd, kr_scratch);
	udelay(3); 
	fmask = qib_read_kreg64(dd, kr_act_fmask);
	if (!fmask) {
		ppd->dd->cspec->stay_in_freeze = 1;
		qib_7322_set_intr_state(ppd->dd, 0);
		qib_write_kreg(dd, kr_fmask, 0ULL);
		qib_dev_err(dd, "HCA unusable until powercycled\n");
		return; 
	}

	qib_write_kreg(ppd->dd, kr_hwerrclear,
	    SYM_MASK(HwErrClear, IBSerdesPClkNotDetectClear_1));

	
	qib_write_kreg(dd, kr_control, dd->control);
	qib_read_kreg32(dd, kr_scratch);
	
	if (ppd->link_speed_supported) {
		ppd->cpspec->ibcctrl_a &=
			~SYM_MASK(IBCCtrlA_0, IBStatIntReductionEn);
		qib_write_kreg_port(ppd, krp_ibcctrl_a,
				    ppd->cpspec->ibcctrl_a);
		qib_read_kreg32(dd, kr_scratch);
		if (ppd->lflags & QIBL_IB_LINK_DISABLED)
			qib_set_ib_7322_lstate(ppd, 0,
				QLOGIC_IB_IBCC_LINKINITCMD_DISABLE);
	}
}
