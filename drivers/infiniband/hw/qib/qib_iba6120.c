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

#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <rdma/ib_verbs.h>

#include "qib.h"
#include "qib_6120_regs.h"

static void qib_6120_setup_setextled(struct qib_pportdata *, u32);
static void sendctrl_6120_mod(struct qib_pportdata *ppd, u32 op);
static u8 qib_6120_phys_portstate(u64);
static u32 qib_6120_iblink_state(u64);


#define KREG_IDX(regname) (QIB_6120_##regname##_OFFS / sizeof(u64))

#define kr_extctrl KREG_IDX(EXTCtrl)
#define kr_extstatus KREG_IDX(EXTStatus)
#define kr_gpio_clear KREG_IDX(GPIOClear)
#define kr_gpio_mask KREG_IDX(GPIOMask)
#define kr_gpio_out KREG_IDX(GPIOOut)
#define kr_gpio_status KREG_IDX(GPIOStatus)
#define kr_rcvctrl KREG_IDX(RcvCtrl)
#define kr_sendctrl KREG_IDX(SendCtrl)
#define kr_partitionkey KREG_IDX(RcvPartitionKey)
#define kr_hwdiagctrl KREG_IDX(HwDiagCtrl)
#define kr_ibcstatus KREG_IDX(IBCStatus)
#define kr_ibcctrl KREG_IDX(IBCCtrl)
#define kr_sendbuffererror KREG_IDX(SendBufErr0)
#define kr_rcvbthqp KREG_IDX(RcvBTHQP)
#define kr_counterregbase KREG_IDX(CntrRegBase)
#define kr_palign KREG_IDX(PageAlign)
#define kr_rcvegrbase KREG_IDX(RcvEgrBase)
#define kr_rcvegrcnt KREG_IDX(RcvEgrCnt)
#define kr_rcvhdrcnt KREG_IDX(RcvHdrCnt)
#define kr_rcvhdrentsize KREG_IDX(RcvHdrEntSize)
#define kr_rcvhdrsize KREG_IDX(RcvHdrSize)
#define kr_rcvtidbase KREG_IDX(RcvTIDBase)
#define kr_rcvtidcnt KREG_IDX(RcvTIDCnt)
#define kr_scratch KREG_IDX(Scratch)
#define kr_sendctrl KREG_IDX(SendCtrl)
#define kr_sendpioavailaddr KREG_IDX(SendPIOAvailAddr)
#define kr_sendpiobufbase KREG_IDX(SendPIOBufBase)
#define kr_sendpiobufcnt KREG_IDX(SendPIOBufCnt)
#define kr_sendpiosize KREG_IDX(SendPIOSize)
#define kr_sendregbase KREG_IDX(SendRegBase)
#define kr_userregbase KREG_IDX(UserRegBase)
#define kr_control KREG_IDX(Control)
#define kr_intclear KREG_IDX(IntClear)
#define kr_intmask KREG_IDX(IntMask)
#define kr_intstatus KREG_IDX(IntStatus)
#define kr_errclear KREG_IDX(ErrClear)
#define kr_errmask KREG_IDX(ErrMask)
#define kr_errstatus KREG_IDX(ErrStatus)
#define kr_hwerrclear KREG_IDX(HwErrClear)
#define kr_hwerrmask KREG_IDX(HwErrMask)
#define kr_hwerrstatus KREG_IDX(HwErrStatus)
#define kr_revision KREG_IDX(Revision)
#define kr_portcnt KREG_IDX(PortCnt)
#define kr_serdes_cfg0 KREG_IDX(SerdesCfg0)
#define kr_serdes_cfg1 (kr_serdes_cfg0 + 1)
#define kr_serdes_stat KREG_IDX(SerdesStat)
#define kr_xgxs_cfg KREG_IDX(XGXSCfg)

/* These must only be written via qib_write_kreg_ctxt() */
#define kr_rcvhdraddr KREG_IDX(RcvHdrAddr0)
#define kr_rcvhdrtailaddr KREG_IDX(RcvHdrTailAddr0)

#define CREG_IDX(regname) ((QIB_6120_##regname##_OFFS - \
			QIB_6120_LBIntCnt_OFFS) / sizeof(u64))

#define cr_badformat CREG_IDX(RxBadFormatCnt)
#define cr_erricrc CREG_IDX(RxICRCErrCnt)
#define cr_errlink CREG_IDX(RxLinkProblemCnt)
#define cr_errlpcrc CREG_IDX(RxLPCRCErrCnt)
#define cr_errpkey CREG_IDX(RxPKeyMismatchCnt)
#define cr_rcvflowctrl_err CREG_IDX(RxFlowCtrlErrCnt)
#define cr_err_rlen CREG_IDX(RxLenErrCnt)
#define cr_errslen CREG_IDX(TxLenErrCnt)
#define cr_errtidfull CREG_IDX(RxTIDFullErrCnt)
#define cr_errtidvalid CREG_IDX(RxTIDValidErrCnt)
#define cr_errvcrc CREG_IDX(RxVCRCErrCnt)
#define cr_ibstatuschange CREG_IDX(IBStatusChangeCnt)
#define cr_lbint CREG_IDX(LBIntCnt)
#define cr_invalidrlen CREG_IDX(RxMaxMinLenErrCnt)
#define cr_invalidslen CREG_IDX(TxMaxMinLenErrCnt)
#define cr_lbflowstall CREG_IDX(LBFlowStallCnt)
#define cr_pktrcv CREG_IDX(RxDataPktCnt)
#define cr_pktrcvflowctrl CREG_IDX(RxFlowPktCnt)
#define cr_pktsend CREG_IDX(TxDataPktCnt)
#define cr_pktsendflow CREG_IDX(TxFlowPktCnt)
#define cr_portovfl CREG_IDX(RxP0HdrEgrOvflCnt)
#define cr_rcvebp CREG_IDX(RxEBPCnt)
#define cr_rcvovfl CREG_IDX(RxBufOvflCnt)
#define cr_senddropped CREG_IDX(TxDroppedPktCnt)
#define cr_sendstall CREG_IDX(TxFlowStallCnt)
#define cr_sendunderrun CREG_IDX(TxUnderrunCnt)
#define cr_wordrcv CREG_IDX(RxDwordCnt)
#define cr_wordsend CREG_IDX(TxDwordCnt)
#define cr_txunsupvl CREG_IDX(TxUnsupVLErrCnt)
#define cr_rxdroppkt CREG_IDX(RxDroppedPktCnt)
#define cr_iblinkerrrecov CREG_IDX(IBLinkErrRecoveryCnt)
#define cr_iblinkdown CREG_IDX(IBLinkDownedCnt)
#define cr_ibsymbolerr CREG_IDX(IBSymbolErrCnt)

#define SYM_RMASK(regname, fldname) ((u64)              \
	QIB_6120_##regname##_##fldname##_RMASK)
#define SYM_MASK(regname, fldname) ((u64)               \
	QIB_6120_##regname##_##fldname##_RMASK <<       \
	 QIB_6120_##regname##_##fldname##_LSB)
#define SYM_LSB(regname, fldname) (QIB_6120_##regname##_##fldname##_LSB)

#define SYM_FIELD(value, regname, fldname) ((u64) \
	(((value) >> SYM_LSB(regname, fldname)) & \
	 SYM_RMASK(regname, fldname)))
#define ERR_MASK(fldname) SYM_MASK(ErrMask, fldname##Mask)
#define HWE_MASK(fldname) SYM_MASK(HwErrMask, fldname##Mask)

#define IB_6120_LT_STATE_DISABLED        0x00
#define IB_6120_LT_STATE_LINKUP          0x01
#define IB_6120_LT_STATE_POLLACTIVE      0x02
#define IB_6120_LT_STATE_POLLQUIET       0x03
#define IB_6120_LT_STATE_SLEEPDELAY      0x04
#define IB_6120_LT_STATE_SLEEPQUIET      0x05
#define IB_6120_LT_STATE_CFGDEBOUNCE     0x08
#define IB_6120_LT_STATE_CFGRCVFCFG      0x09
#define IB_6120_LT_STATE_CFGWAITRMT      0x0a
#define IB_6120_LT_STATE_CFGIDLE 0x0b
#define IB_6120_LT_STATE_RECOVERRETRAIN  0x0c
#define IB_6120_LT_STATE_RECOVERWAITRMT  0x0e
#define IB_6120_LT_STATE_RECOVERIDLE     0x0f

#define IB_6120_L_STATE_DOWN             0x0
#define IB_6120_L_STATE_INIT             0x1
#define IB_6120_L_STATE_ARM              0x2
#define IB_6120_L_STATE_ACTIVE           0x3
#define IB_6120_L_STATE_ACT_DEFER        0x4

static const u8 qib_6120_physportstate[0x20] = {
	[IB_6120_LT_STATE_DISABLED] = IB_PHYSPORTSTATE_DISABLED,
	[IB_6120_LT_STATE_LINKUP] = IB_PHYSPORTSTATE_LINKUP,
	[IB_6120_LT_STATE_POLLACTIVE] = IB_PHYSPORTSTATE_POLL,
	[IB_6120_LT_STATE_POLLQUIET] = IB_PHYSPORTSTATE_POLL,
	[IB_6120_LT_STATE_SLEEPDELAY] = IB_PHYSPORTSTATE_SLEEP,
	[IB_6120_LT_STATE_SLEEPQUIET] = IB_PHYSPORTSTATE_SLEEP,
	[IB_6120_LT_STATE_CFGDEBOUNCE] =
		IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_6120_LT_STATE_CFGRCVFCFG] =
		IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_6120_LT_STATE_CFGWAITRMT] =
		IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_6120_LT_STATE_CFGIDLE] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_6120_LT_STATE_RECOVERRETRAIN] =
		IB_PHYSPORTSTATE_LINK_ERR_RECOVER,
	[IB_6120_LT_STATE_RECOVERWAITRMT] =
		IB_PHYSPORTSTATE_LINK_ERR_RECOVER,
	[IB_6120_LT_STATE_RECOVERIDLE] =
		IB_PHYSPORTSTATE_LINK_ERR_RECOVER,
	[0x10] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[0x11] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[0x12] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[0x13] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[0x14] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[0x15] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[0x16] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[0x17] = IB_PHYSPORTSTATE_CFG_TRAIN
};


struct qib_chip_specific {
	u64 __iomem *cregbase;
	u64 *cntrs;
	u64 *portcntrs;
	void *dummy_hdrq;   
	dma_addr_t dummy_hdrq_phys;
	spinlock_t kernel_tid_lock; 
	spinlock_t user_tid_lock; 
	spinlock_t rcvmod_lock; 
	spinlock_t gpio_lock; 
	u64 hwerrmask;
	u64 errormask;
	u64 gpio_out; 
	u64 gpio_mask; 
	u64 extctrl; 
	u64 ibdeltainprog;
	u64 ibsymdelta;
	u64 ibsymsnap;
	u64 iblnkerrdelta;
	u64 iblnkerrsnap;
	u64 ibcctrl; 
	u32 lastlinkrecov; 
	int irq;
	u32 cntrnamelen;
	u32 portcntrnamelen;
	u32 ncntrs;
	u32 nportcntrs;
	
	u32 rxfc_unsupvl_errs;
	u32 overrun_thresh_errs;
	u32 lli_errs;
	u32 lli_counter;
	u64 lli_thresh;
	u64 sword; 
	u64 rword; 
	u64 spkts; 
	u64 rpkts; 
	u64 xmit_wait; 
	struct timer_list pma_timer;
	char emsgbuf[128];
	char bitsmsgbuf[64];
	u8 pma_sample_status;
};

#define QLOGIC_IB_IBCC_LINKINITCMD_DISABLE 1
#define QLOGIC_IB_IBCC_LINKINITCMD_POLL 2
#define QLOGIC_IB_IBCC_LINKINITCMD_SLEEP 3
#define QLOGIC_IB_IBCC_LINKINITCMD_SHIFT 16

#define QLOGIC_IB_IBCC_LINKCMD_DOWN 1           
#define QLOGIC_IB_IBCC_LINKCMD_ARMED 2          
#define QLOGIC_IB_IBCC_LINKCMD_ACTIVE 3 
#define QLOGIC_IB_IBCC_LINKCMD_SHIFT 18


static inline u32 qib_read_ureg32(const struct qib_devdata *dd,
				  enum qib_ureg regno, int ctxt)
{
	if (!dd->kregbase || !(dd->flags & QIB_PRESENT))
		return 0;

	if (dd->userbase)
		return readl(regno + (u64 __iomem *)
			     ((char __iomem *)dd->userbase +
			      dd->ureg_align * ctxt));
	else
		return readl(regno + (u64 __iomem *)
			     (dd->uregbase +
			      (char __iomem *)dd->kregbase +
			      dd->ureg_align * ctxt));
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
				  const u16 regno)
{
	if (!dd->kregbase || !(dd->flags & QIB_PRESENT))
		return -1;
	return readl((u32 __iomem *)&dd->kregbase[regno]);
}

static inline u64 qib_read_kreg64(const struct qib_devdata *dd,
				  const u16 regno)
{
	if (!dd->kregbase || !(dd->flags & QIB_PRESENT))
		return -1;

	return readq(&dd->kregbase[regno]);
}

static inline void qib_write_kreg(const struct qib_devdata *dd,
				  const u16 regno, u64 value)
{
	if (dd->kregbase && (dd->flags & QIB_PRESENT))
		writeq(value, &dd->kregbase[regno]);
}

static inline void qib_write_kreg_ctxt(const struct qib_devdata *dd,
				       const u16 regno, unsigned ctxt,
				       u64 value)
{
	qib_write_kreg(dd, regno + ctxt, value);
}

static inline void write_6120_creg(const struct qib_devdata *dd,
				   u16 regno, u64 value)
{
	if (dd->cspec->cregbase && (dd->flags & QIB_PRESENT))
		writeq(value, &dd->cspec->cregbase[regno]);
}

static inline u64 read_6120_creg(const struct qib_devdata *dd, u16 regno)
{
	if (!dd->cspec->cregbase || !(dd->flags & QIB_PRESENT))
		return 0;
	return readq(&dd->cspec->cregbase[regno]);
}

static inline u32 read_6120_creg32(const struct qib_devdata *dd, u16 regno)
{
	if (!dd->cspec->cregbase || !(dd->flags & QIB_PRESENT))
		return 0;
	return readl(&dd->cspec->cregbase[regno]);
}

#define QLOGIC_IB_C_RESET 1U

#define QLOGIC_IB_I_RCVURG_MASK ((1U << 5) - 1)
#define QLOGIC_IB_I_RCVURG_SHIFT 0
#define QLOGIC_IB_I_RCVAVAIL_MASK ((1U << 5) - 1)
#define QLOGIC_IB_I_RCVAVAIL_SHIFT 12

#define QLOGIC_IB_C_FREEZEMODE 0x00000002
#define QLOGIC_IB_C_LINKENABLE 0x00000004
#define QLOGIC_IB_I_ERROR               0x0000000080000000ULL
#define QLOGIC_IB_I_SPIOSENT            0x0000000040000000ULL
#define QLOGIC_IB_I_SPIOBUFAVAIL        0x0000000020000000ULL
#define QLOGIC_IB_I_GPIO                0x0000000010000000ULL
#define QLOGIC_IB_I_BITSEXTANT \
		((QLOGIC_IB_I_RCVURG_MASK << QLOGIC_IB_I_RCVURG_SHIFT) | \
		(QLOGIC_IB_I_RCVAVAIL_MASK << \
		 QLOGIC_IB_I_RCVAVAIL_SHIFT) | \
		QLOGIC_IB_I_ERROR | QLOGIC_IB_I_SPIOSENT | \
		QLOGIC_IB_I_SPIOBUFAVAIL | QLOGIC_IB_I_GPIO)

#define QLOGIC_IB_HWE_PCIEMEMPARITYERR_MASK  0x000000000000003fULL
#define QLOGIC_IB_HWE_PCIEMEMPARITYERR_SHIFT 0
#define QLOGIC_IB_HWE_PCIEPOISONEDTLP      0x0000000010000000ULL
#define QLOGIC_IB_HWE_PCIECPLTIMEOUT       0x0000000020000000ULL
#define QLOGIC_IB_HWE_PCIEBUSPARITYXTLH    0x0000000040000000ULL
#define QLOGIC_IB_HWE_PCIEBUSPARITYXADM    0x0000000080000000ULL
#define QLOGIC_IB_HWE_PCIEBUSPARITYRADM    0x0000000100000000ULL
#define QLOGIC_IB_HWE_COREPLL_FBSLIP       0x0080000000000000ULL
#define QLOGIC_IB_HWE_COREPLL_RFSLIP       0x0100000000000000ULL
#define QLOGIC_IB_HWE_PCIE1PLLFAILED       0x0400000000000000ULL
#define QLOGIC_IB_HWE_PCIE0PLLFAILED       0x0800000000000000ULL
#define QLOGIC_IB_HWE_SERDESPLLFAILED      0x1000000000000000ULL


#define QLOGIC_IB_EXTS_FREQSEL 0x2
#define QLOGIC_IB_EXTS_SERDESSEL 0x4
#define QLOGIC_IB_EXTS_MEMBIST_ENDTEST     0x0000000000004000
#define QLOGIC_IB_EXTS_MEMBIST_FOUND       0x0000000000008000

#define QLOGIC_IB_XGXS_RESET          0x5ULL

#define _QIB_GPIO_SDA_NUM 1
#define _QIB_GPIO_SCL_NUM 0

#define GPIO_RXUVL_BIT 3
#define GPIO_OVRUN_BIT 4
#define GPIO_LLI_BIT 5
#define GPIO_ERRINTR_MASK 0x38


#define QLOGIC_IB_RT_BUFSIZE_MASK 0xe0000000ULL
#define QLOGIC_IB_RT_BUFSIZE_SHIFTVAL(tid) \
	((((tid) & QLOGIC_IB_RT_BUFSIZE_MASK) >> 29) + 11 - 1)
#define QLOGIC_IB_RT_BUFSIZE(tid) (1 << QLOGIC_IB_RT_BUFSIZE_SHIFTVAL(tid))
#define QLOGIC_IB_RT_IS_VALID(tid) \
	(((tid) & QLOGIC_IB_RT_BUFSIZE_MASK) && \
	 ((((tid) & QLOGIC_IB_RT_BUFSIZE_MASK) != QLOGIC_IB_RT_BUFSIZE_MASK)))
#define QLOGIC_IB_RT_ADDR_MASK 0x1FFFFFFFULL 
#define QLOGIC_IB_RT_ADDR_SHIFT 10

#define QLOGIC_IB_R_INTRAVAIL_SHIFT 16
#define QLOGIC_IB_R_TAILUPD_SHIFT 31
#define IBA6120_R_PKEY_DIS_SHIFT 30

#define PBC_6120_VL15_SEND_CTRL (1ULL << 31) 

#define IBCBUSFRSPCPARITYERR HWE_MASK(IBCBusFromSPCParityErr)
#define IBCBUSTOSPCPARITYERR HWE_MASK(IBCBusToSPCParityErr)

#define SYM_MASK_BIT(regname, fldname, bit) ((u64) \
	((1ULL << (SYM_LSB(regname, fldname) + (bit)))))

#define TXEMEMPARITYERR_PIOBUF \
	SYM_MASK_BIT(HwErrMask, TXEMemParityErrMask, 0)
#define TXEMEMPARITYERR_PIOPBC \
	SYM_MASK_BIT(HwErrMask, TXEMemParityErrMask, 1)
#define TXEMEMPARITYERR_PIOLAUNCHFIFO \
	SYM_MASK_BIT(HwErrMask, TXEMemParityErrMask, 2)

#define RXEMEMPARITYERR_RCVBUF \
	SYM_MASK_BIT(HwErrMask, RXEMemParityErrMask, 0)
#define RXEMEMPARITYERR_LOOKUPQ \
	SYM_MASK_BIT(HwErrMask, RXEMemParityErrMask, 1)
#define RXEMEMPARITYERR_EXPTID \
	SYM_MASK_BIT(HwErrMask, RXEMemParityErrMask, 2)
#define RXEMEMPARITYERR_EAGERTID \
	SYM_MASK_BIT(HwErrMask, RXEMemParityErrMask, 3)
#define RXEMEMPARITYERR_FLAGBUF \
	SYM_MASK_BIT(HwErrMask, RXEMemParityErrMask, 4)
#define RXEMEMPARITYERR_DATAINFO \
	SYM_MASK_BIT(HwErrMask, RXEMemParityErrMask, 5)
#define RXEMEMPARITYERR_HDRINFO \
	SYM_MASK_BIT(HwErrMask, RXEMemParityErrMask, 6)

static const struct qib_hwerror_msgs qib_6120_hwerror_msgs[] = {
	
	QLOGIC_IB_HWE_MSG(IBCBUSFRSPCPARITYERR, "QIB2IB Parity"),
	QLOGIC_IB_HWE_MSG(IBCBUSTOSPCPARITYERR, "IB2QIB Parity"),

	QLOGIC_IB_HWE_MSG(TXEMEMPARITYERR_PIOBUF,
			  "TXE PIOBUF Memory Parity"),
	QLOGIC_IB_HWE_MSG(TXEMEMPARITYERR_PIOPBC,
			  "TXE PIOPBC Memory Parity"),
	QLOGIC_IB_HWE_MSG(TXEMEMPARITYERR_PIOLAUNCHFIFO,
			  "TXE PIOLAUNCHFIFO Memory Parity"),

	QLOGIC_IB_HWE_MSG(RXEMEMPARITYERR_RCVBUF,
			  "RXE RCVBUF Memory Parity"),
	QLOGIC_IB_HWE_MSG(RXEMEMPARITYERR_LOOKUPQ,
			  "RXE LOOKUPQ Memory Parity"),
	QLOGIC_IB_HWE_MSG(RXEMEMPARITYERR_EAGERTID,
			  "RXE EAGERTID Memory Parity"),
	QLOGIC_IB_HWE_MSG(RXEMEMPARITYERR_EXPTID,
			  "RXE EXPTID Memory Parity"),
	QLOGIC_IB_HWE_MSG(RXEMEMPARITYERR_FLAGBUF,
			  "RXE FLAGBUF Memory Parity"),
	QLOGIC_IB_HWE_MSG(RXEMEMPARITYERR_DATAINFO,
			  "RXE DATAINFO Memory Parity"),
	QLOGIC_IB_HWE_MSG(RXEMEMPARITYERR_HDRINFO,
			  "RXE HDRINFO Memory Parity"),

	
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIEPOISONEDTLP,
			  "PCIe Poisoned TLP"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIECPLTIMEOUT,
			  "PCIe completion timeout"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIE1PLLFAILED,
			  "PCIePLL1"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIE0PLLFAILED,
			  "PCIePLL0"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIEBUSPARITYXTLH,
			  "PCIe XTLH core parity"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIEBUSPARITYXADM,
			  "PCIe ADM TX core parity"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIEBUSPARITYRADM,
			  "PCIe ADM RX core parity"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_SERDESPLLFAILED,
			  "SerDes PLL"),
};

#define TXE_PIO_PARITY (TXEMEMPARITYERR_PIOBUF | TXEMEMPARITYERR_PIOPBC)
#define _QIB_PLL_FAIL (QLOGIC_IB_HWE_COREPLL_FBSLIP |   \
		QLOGIC_IB_HWE_COREPLL_RFSLIP)

	
#define IB_HWE_BITSEXTANT \
	(HWE_MASK(RXEMemParityErr) |					\
	 HWE_MASK(TXEMemParityErr) |					\
	 (QLOGIC_IB_HWE_PCIEMEMPARITYERR_MASK <<			\
	  QLOGIC_IB_HWE_PCIEMEMPARITYERR_SHIFT) |			\
	 QLOGIC_IB_HWE_PCIE1PLLFAILED |					\
	 QLOGIC_IB_HWE_PCIE0PLLFAILED |					\
	 QLOGIC_IB_HWE_PCIEPOISONEDTLP |				\
	 QLOGIC_IB_HWE_PCIECPLTIMEOUT |					\
	 QLOGIC_IB_HWE_PCIEBUSPARITYXTLH |				\
	 QLOGIC_IB_HWE_PCIEBUSPARITYXADM |				\
	 QLOGIC_IB_HWE_PCIEBUSPARITYRADM |				\
	 HWE_MASK(PowerOnBISTFailed) |					\
	 QLOGIC_IB_HWE_COREPLL_FBSLIP |					\
	 QLOGIC_IB_HWE_COREPLL_RFSLIP |					\
	 QLOGIC_IB_HWE_SERDESPLLFAILED |				\
	 HWE_MASK(IBCBusToSPCParityErr) |				\
	 HWE_MASK(IBCBusFromSPCParityErr))

#define IB_E_BITSEXTANT \
	(ERR_MASK(RcvFormatErr) | ERR_MASK(RcvVCRCErr) |		\
	 ERR_MASK(RcvICRCErr) | ERR_MASK(RcvMinPktLenErr) |		\
	 ERR_MASK(RcvMaxPktLenErr) | ERR_MASK(RcvLongPktLenErr) |	\
	 ERR_MASK(RcvShortPktLenErr) | ERR_MASK(RcvUnexpectedCharErr) | \
	 ERR_MASK(RcvUnsupportedVLErr) | ERR_MASK(RcvEBPErr) |		\
	 ERR_MASK(RcvIBFlowErr) | ERR_MASK(RcvBadVersionErr) |		\
	 ERR_MASK(RcvEgrFullErr) | ERR_MASK(RcvHdrFullErr) |		\
	 ERR_MASK(RcvBadTidErr) | ERR_MASK(RcvHdrLenErr) |		\
	 ERR_MASK(RcvHdrErr) | ERR_MASK(RcvIBLostLinkErr) |		\
	 ERR_MASK(SendMinPktLenErr) | ERR_MASK(SendMaxPktLenErr) |	\
	 ERR_MASK(SendUnderRunErr) | ERR_MASK(SendPktLenErr) |		\
	 ERR_MASK(SendDroppedSmpPktErr) |				\
	 ERR_MASK(SendDroppedDataPktErr) |				\
	 ERR_MASK(SendPioArmLaunchErr) |				\
	 ERR_MASK(SendUnexpectedPktNumErr) |				\
	 ERR_MASK(SendUnsupportedVLErr) | ERR_MASK(IBStatusChanged) |	\
	 ERR_MASK(InvalidAddrErr) | ERR_MASK(ResetNegated) |		\
	 ERR_MASK(HardwareErr))

#define QLOGIC_IB_E_PKTERRS ( \
		ERR_MASK(SendPktLenErr) |				\
		ERR_MASK(SendDroppedDataPktErr) |			\
		ERR_MASK(RcvVCRCErr) |					\
		ERR_MASK(RcvICRCErr) |					\
		ERR_MASK(RcvShortPktLenErr) |				\
		ERR_MASK(RcvEBPErr))

#define E_SUM_PKTERRS						\
	(ERR_MASK(RcvHdrLenErr) | ERR_MASK(RcvBadTidErr) |		\
	 ERR_MASK(RcvBadVersionErr) | ERR_MASK(RcvHdrErr) |		\
	 ERR_MASK(RcvLongPktLenErr) | ERR_MASK(RcvShortPktLenErr) |	\
	 ERR_MASK(RcvMaxPktLenErr) | ERR_MASK(RcvMinPktLenErr) |	\
	 ERR_MASK(RcvFormatErr) | ERR_MASK(RcvUnsupportedVLErr) |	\
	 ERR_MASK(RcvUnexpectedCharErr) | ERR_MASK(RcvEBPErr))

#define E_SUM_ERRS							\
	(ERR_MASK(SendPioArmLaunchErr) |				\
	 ERR_MASK(SendUnexpectedPktNumErr) |				\
	 ERR_MASK(SendDroppedDataPktErr) |				\
	 ERR_MASK(SendDroppedSmpPktErr) |				\
	 ERR_MASK(SendMaxPktLenErr) | ERR_MASK(SendUnsupportedVLErr) |	\
	 ERR_MASK(SendMinPktLenErr) | ERR_MASK(SendPktLenErr) |		\
	 ERR_MASK(InvalidAddrErr))

#define E_SPKT_ERRS_IGNORE \
	(ERR_MASK(SendDroppedDataPktErr) |				\
	 ERR_MASK(SendDroppedSmpPktErr) |				\
	 ERR_MASK(SendMaxPktLenErr) | ERR_MASK(SendMinPktLenErr) |	\
	 ERR_MASK(SendPktLenErr))

#define E_SUM_LINK_PKTERRS		\
	(ERR_MASK(SendDroppedDataPktErr) |				\
	 ERR_MASK(SendDroppedSmpPktErr) |				\
	 ERR_MASK(SendMinPktLenErr) | ERR_MASK(SendPktLenErr) |		\
	 ERR_MASK(RcvShortPktLenErr) | ERR_MASK(RcvMinPktLenErr) |	\
	 ERR_MASK(RcvUnexpectedCharErr))

static void qib_6120_put_tid_2(struct qib_devdata *, u64 __iomem *,
			       u32, unsigned long);

static void qib_6120_txe_recover(struct qib_devdata *dd)
{
	if (!qib_unordered_wc())
		qib_devinfo(dd->pcidev,
			    "Recovering from TXE PIO parity error\n");
}

static void qib_6120_set_intr_state(struct qib_devdata *dd, u32 enable)
{
	if (enable) {
		if (dd->flags & QIB_BADINTR)
			return;
		qib_write_kreg(dd, kr_intmask, ~0ULL);
		
		qib_write_kreg(dd, kr_intclear, 0ULL);
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
 * even though the details are similar on most chips
 */
static void qib_6120_clear_freeze(struct qib_devdata *dd)
{
	
	qib_write_kreg(dd, kr_errmask, 0ULL);

	
	qib_6120_set_intr_state(dd, 0);

	qib_cancel_sends(dd->pport);

	
	qib_write_kreg(dd, kr_control, dd->control);
	qib_read_kreg32(dd, kr_scratch);

	
	qib_force_pio_avail_update(dd);

	qib_write_kreg(dd, kr_hwerrclear, 0ULL);
	qib_write_kreg(dd, kr_errclear, E_SPKT_ERRS_IGNORE);
	qib_write_kreg(dd, kr_errmask, dd->cspec->errormask);
	qib_6120_set_intr_state(dd, 1);
}

static void qib_handle_6120_hwerrors(struct qib_devdata *dd, char *msg,
				     size_t msgl)
{
	u64 hwerrs;
	u32 bits, ctrl;
	int isfatal = 0;
	char *bitsmsg;
	int log_idx;

	hwerrs = qib_read_kreg64(dd, kr_hwerrstatus);
	if (!hwerrs)
		return;
	if (hwerrs == ~0ULL) {
		qib_dev_err(dd, "Read of hardware error status failed "
			    "(all bits set); ignoring\n");
		return;
	}
	qib_stats.sps_hwerrs++;

	qib_write_kreg(dd, kr_hwerrclear,
		       hwerrs & ~HWE_MASK(PowerOnBISTFailed));

	hwerrs &= dd->cspec->hwerrmask;

	
	for (log_idx = 0; log_idx < QIB_EEP_LOG_CNT; ++log_idx)
		if (hwerrs & dd->eep_st_masks[log_idx].hwerrs_to_log)
			qib_inc_eeprom_err(dd, log_idx, 1);

	if (hwerrs & ~(TXE_PIO_PARITY | RXEMEMPARITYERR_EAGERTID))
		qib_devinfo(dd->pcidev, "Hardware error: hwerr=0x%llx "
			 "(cleared)\n", (unsigned long long) hwerrs);

	if (hwerrs & ~IB_HWE_BITSEXTANT)
		qib_dev_err(dd, "hwerror interrupt with unknown errors "
			    "%llx set\n", (unsigned long long)
			    (hwerrs & ~IB_HWE_BITSEXTANT));

	ctrl = qib_read_kreg32(dd, kr_control);
	if ((ctrl & QLOGIC_IB_C_FREEZEMODE) && !dd->diag_client) {
		if (hwerrs & TXE_PIO_PARITY) {
			qib_6120_txe_recover(dd);
			hwerrs &= ~TXE_PIO_PARITY;
		}

		if (!hwerrs) {
			static u32 freeze_cnt;

			freeze_cnt++;
			qib_6120_clear_freeze(dd);
		} else
			isfatal = 1;
	}

	*msg = '\0';

	if (hwerrs & HWE_MASK(PowerOnBISTFailed)) {
		isfatal = 1;
		strlcat(msg, "[Memory BIST test failed, InfiniPath hardware"
			" unusable]", msgl);
		
		dd->cspec->hwerrmask &= ~HWE_MASK(PowerOnBISTFailed);
		qib_write_kreg(dd, kr_hwerrmask, dd->cspec->hwerrmask);
	}

	qib_format_hwerrors(hwerrs, qib_6120_hwerror_msgs,
			    ARRAY_SIZE(qib_6120_hwerror_msgs), msg, msgl);

	bitsmsg = dd->cspec->bitsmsgbuf;
	if (hwerrs & (QLOGIC_IB_HWE_PCIEMEMPARITYERR_MASK <<
		      QLOGIC_IB_HWE_PCIEMEMPARITYERR_SHIFT)) {
		bits = (u32) ((hwerrs >>
			       QLOGIC_IB_HWE_PCIEMEMPARITYERR_SHIFT) &
			      QLOGIC_IB_HWE_PCIEMEMPARITYERR_MASK);
		snprintf(bitsmsg, sizeof dd->cspec->bitsmsgbuf,
			 "[PCIe Mem Parity Errs %x] ", bits);
		strlcat(msg, bitsmsg, msgl);
	}

	if (hwerrs & _QIB_PLL_FAIL) {
		isfatal = 1;
		snprintf(bitsmsg, sizeof dd->cspec->bitsmsgbuf,
			 "[PLL failed (%llx), InfiniPath hardware unusable]",
			 (unsigned long long) hwerrs & _QIB_PLL_FAIL);
		strlcat(msg, bitsmsg, msgl);
		
		dd->cspec->hwerrmask &= ~(hwerrs & _QIB_PLL_FAIL);
		qib_write_kreg(dd, kr_hwerrmask, dd->cspec->hwerrmask);
	}

	if (hwerrs & QLOGIC_IB_HWE_SERDESPLLFAILED) {
		dd->cspec->hwerrmask &= ~QLOGIC_IB_HWE_SERDESPLLFAILED;
		qib_write_kreg(dd, kr_hwerrmask, dd->cspec->hwerrmask);
	}

	if (hwerrs)
		qib_dev_err(dd, "%s hardware error\n", msg);
	else
		*msg = 0; 

	if (isfatal && !dd->diag_client) {
		qib_dev_err(dd, "Fatal Hardware Error, no longer"
			    " usable, SN %.16s\n", dd->serial);
		if (dd->freezemsg)
			snprintf(dd->freezemsg, dd->freezelen,
				 "{%s}", msg);
		qib_disable_after_error(dd);
	}
}

static int qib_decode_6120_err(struct qib_devdata *dd, char *buf, size_t blen,
			       u64 err)
{
	int iserr = 1;

	*buf = '\0';
	if (err & QLOGIC_IB_E_PKTERRS) {
		if (!(err & ~QLOGIC_IB_E_PKTERRS))
			iserr = 0;
		if ((err & ERR_MASK(RcvICRCErr)) &&
		    !(err&(ERR_MASK(RcvVCRCErr)|ERR_MASK(RcvEBPErr))))
			strlcat(buf, "CRC ", blen);
		if (!iserr)
			goto done;
	}
	if (err & ERR_MASK(RcvHdrLenErr))
		strlcat(buf, "rhdrlen ", blen);
	if (err & ERR_MASK(RcvBadTidErr))
		strlcat(buf, "rbadtid ", blen);
	if (err & ERR_MASK(RcvBadVersionErr))
		strlcat(buf, "rbadversion ", blen);
	if (err & ERR_MASK(RcvHdrErr))
		strlcat(buf, "rhdr ", blen);
	if (err & ERR_MASK(RcvLongPktLenErr))
		strlcat(buf, "rlongpktlen ", blen);
	if (err & ERR_MASK(RcvMaxPktLenErr))
		strlcat(buf, "rmaxpktlen ", blen);
	if (err & ERR_MASK(RcvMinPktLenErr))
		strlcat(buf, "rminpktlen ", blen);
	if (err & ERR_MASK(SendMinPktLenErr))
		strlcat(buf, "sminpktlen ", blen);
	if (err & ERR_MASK(RcvFormatErr))
		strlcat(buf, "rformaterr ", blen);
	if (err & ERR_MASK(RcvUnsupportedVLErr))
		strlcat(buf, "runsupvl ", blen);
	if (err & ERR_MASK(RcvUnexpectedCharErr))
		strlcat(buf, "runexpchar ", blen);
	if (err & ERR_MASK(RcvIBFlowErr))
		strlcat(buf, "ribflow ", blen);
	if (err & ERR_MASK(SendUnderRunErr))
		strlcat(buf, "sunderrun ", blen);
	if (err & ERR_MASK(SendPioArmLaunchErr))
		strlcat(buf, "spioarmlaunch ", blen);
	if (err & ERR_MASK(SendUnexpectedPktNumErr))
		strlcat(buf, "sunexperrpktnum ", blen);
	if (err & ERR_MASK(SendDroppedSmpPktErr))
		strlcat(buf, "sdroppedsmppkt ", blen);
	if (err & ERR_MASK(SendMaxPktLenErr))
		strlcat(buf, "smaxpktlen ", blen);
	if (err & ERR_MASK(SendUnsupportedVLErr))
		strlcat(buf, "sunsupVL ", blen);
	if (err & ERR_MASK(InvalidAddrErr))
		strlcat(buf, "invalidaddr ", blen);
	if (err & ERR_MASK(RcvEgrFullErr))
		strlcat(buf, "rcvegrfull ", blen);
	if (err & ERR_MASK(RcvHdrFullErr))
		strlcat(buf, "rcvhdrfull ", blen);
	if (err & ERR_MASK(IBStatusChanged))
		strlcat(buf, "ibcstatuschg ", blen);
	if (err & ERR_MASK(RcvIBLostLinkErr))
		strlcat(buf, "riblostlink ", blen);
	if (err & ERR_MASK(HardwareErr))
		strlcat(buf, "hardware ", blen);
	if (err & ERR_MASK(ResetNegated))
		strlcat(buf, "reset ", blen);
done:
	return iserr;
}

static void qib_disarm_6120_senderrbufs(struct qib_pportdata *ppd)
{
	unsigned long sbuf[2];
	struct qib_devdata *dd = ppd->dd;

	sbuf[0] = qib_read_kreg64(dd, kr_sendbuffererror);
	sbuf[1] = qib_read_kreg64(dd, kr_sendbuffererror + 1);

	if (sbuf[0] || sbuf[1])
		qib_disarm_piobufs_set(dd, sbuf,
				       dd->piobcnt2k + dd->piobcnt4k);
}

static int chk_6120_linkrecovery(struct qib_devdata *dd, u64 ibcs)
{
	int ret = 1;
	u32 ibstate = qib_6120_iblink_state(ibcs);
	u32 linkrecov = read_6120_creg32(dd, cr_iblinkerrrecov);

	if (linkrecov != dd->cspec->lastlinkrecov) {
		
		dd->cspec->lastlinkrecov = 0;
		qib_set_linkstate(dd->pport, QIB_IB_LINKDOWN);
		ret = 0;
	}
	if (ibstate == IB_PORT_ACTIVE)
		dd->cspec->lastlinkrecov =
			read_6120_creg32(dd, cr_iblinkerrrecov);
	return ret;
}

static void handle_6120_errors(struct qib_devdata *dd, u64 errs)
{
	char *msg;
	u64 ignore_this_time = 0;
	u64 iserr = 0;
	int log_idx;
	struct qib_pportdata *ppd = dd->pport;
	u64 mask;

	
	errs &= dd->cspec->errormask;
	msg = dd->cspec->emsgbuf;

	
	if (errs & ERR_MASK(HardwareErr))
		qib_handle_6120_hwerrors(dd, msg, sizeof dd->cspec->emsgbuf);
	else
		for (log_idx = 0; log_idx < QIB_EEP_LOG_CNT; ++log_idx)
			if (errs & dd->eep_st_masks[log_idx].errs_to_log)
				qib_inc_eeprom_err(dd, log_idx, 1);

	if (errs & ~IB_E_BITSEXTANT)
		qib_dev_err(dd, "error interrupt with unknown errors "
			    "%llx set\n",
			    (unsigned long long) (errs & ~IB_E_BITSEXTANT));

	if (errs & E_SUM_ERRS) {
		qib_disarm_6120_senderrbufs(ppd);
		if ((errs & E_SUM_LINK_PKTERRS) &&
		    !(ppd->lflags & QIBL_LINKACTIVE)) {
			ignore_this_time = errs & E_SUM_LINK_PKTERRS;
		}
	} else if ((errs & E_SUM_LINK_PKTERRS) &&
		   !(ppd->lflags & QIBL_LINKACTIVE)) {
		ignore_this_time = errs & E_SUM_LINK_PKTERRS;
	}

	qib_write_kreg(dd, kr_errclear, errs);

	errs &= ~ignore_this_time;
	if (!errs)
		goto done;

	mask = ERR_MASK(IBStatusChanged) | ERR_MASK(RcvEgrFullErr) |
		ERR_MASK(RcvHdrFullErr) | ERR_MASK(HardwareErr);
	qib_decode_6120_err(dd, msg, sizeof dd->cspec->emsgbuf, errs & ~mask);

	if (errs & E_SUM_PKTERRS)
		qib_stats.sps_rcverrs++;
	if (errs & E_SUM_ERRS)
		qib_stats.sps_txerrs++;

	iserr = errs & ~(E_SUM_PKTERRS | QLOGIC_IB_E_PKTERRS);

	if (errs & ERR_MASK(IBStatusChanged)) {
		u64 ibcs = qib_read_kreg64(dd, kr_ibcstatus);
		u32 ibstate = qib_6120_iblink_state(ibcs);
		int handle = 1;

		if (ibstate != IB_PORT_INIT && dd->cspec->lastlinkrecov)
			handle = chk_6120_linkrecovery(dd, ibcs);
		if (handle && qib_6120_phys_portstate(ibcs) ==
					    IB_PHYSPORTSTATE_LINK_ERR_RECOVER)
			handle = 0;
		if (handle)
			qib_handle_e_ibstatuschanged(ppd, ibcs);
	}

	if (errs & ERR_MASK(ResetNegated)) {
		qib_dev_err(dd, "Got reset, requires re-init "
			      "(unload and reload driver)\n");
		dd->flags &= ~QIB_INITTED;  
		
		*dd->devstatusp |= QIB_STATUS_HWERROR;
		*dd->pport->statusp &= ~QIB_STATUS_IB_CONF;
	}

	if (*msg && iserr)
		qib_dev_porterr(dd, ppd->port, "%s error\n", msg);

	if (ppd->state_wanted & ppd->lflags)
		wake_up_interruptible(&ppd->state_wait);

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

static void qib_6120_init_hwerrors(struct qib_devdata *dd)
{
	u64 val;
	u64 extsval;

	extsval = qib_read_kreg64(dd, kr_extstatus);

	if (!(extsval & QLOGIC_IB_EXTS_MEMBIST_ENDTEST))
		qib_dev_err(dd, "MemBIST did not complete!\n");

	
	val = ~0ULL;
	if (dd->minrev < 2) {
		val &= ~QLOGIC_IB_HWE_PCIEBUSPARITYRADM;
	}
	
	val &= ~TXEMEMPARITYERR_PIOBUF;

	dd->cspec->hwerrmask = val;

	qib_write_kreg(dd, kr_hwerrclear, ~HWE_MASK(PowerOnBISTFailed));
	qib_write_kreg(dd, kr_hwerrmask, dd->cspec->hwerrmask);

	
	qib_write_kreg(dd, kr_errclear, ~0ULL);
	
	qib_write_kreg(dd, kr_errmask, ~0ULL);
	dd->cspec->errormask = qib_read_kreg64(dd, kr_errmask);
	
	qib_write_kreg(dd, kr_intclear, ~0ULL);

	qib_write_kreg(dd, kr_rcvbthqp,
		       dd->qpn_mask << (QIB_6120_RcvBTHQP_BTHQP_Mask_LSB - 1) |
		       QIB_KD_QP);
}

static void qib_set_6120_armlaunch(struct qib_devdata *dd, u32 enable)
{
	if (enable) {
		qib_write_kreg(dd, kr_errclear,
			       ERR_MASK(SendPioArmLaunchErr));
		dd->cspec->errormask |= ERR_MASK(SendPioArmLaunchErr);
	} else
		dd->cspec->errormask &= ~ERR_MASK(SendPioArmLaunchErr);
	qib_write_kreg(dd, kr_errmask, dd->cspec->errormask);
}

static void qib_set_ib_6120_lstate(struct qib_pportdata *ppd, u16 linkcmd,
				   u16 linitcmd)
{
	u64 mod_wd;
	struct qib_devdata *dd = ppd->dd;
	unsigned long flags;

	if (linitcmd == QLOGIC_IB_IBCC_LINKINITCMD_DISABLE) {
		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags |= QIBL_IB_LINK_DISABLED;
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	} else if (linitcmd || linkcmd == QLOGIC_IB_IBCC_LINKCMD_DOWN) {
		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags &= ~QIBL_IB_LINK_DISABLED;
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	}

	mod_wd = (linkcmd << QLOGIC_IB_IBCC_LINKCMD_SHIFT) |
		(linitcmd << QLOGIC_IB_IBCC_LINKINITCMD_SHIFT);

	qib_write_kreg(dd, kr_ibcctrl, dd->cspec->ibcctrl | mod_wd);
	
	qib_write_kreg(dd, kr_scratch, 0);
}

static int qib_6120_bringup_serdes(struct qib_pportdata *ppd)
{
	struct qib_devdata *dd = ppd->dd;
	u64 val, config1, prev_val, hwstat, ibc;

	
	dd->control &= ~QLOGIC_IB_C_LINKENABLE;
	qib_write_kreg(dd, kr_control, 0ULL);

	dd->cspec->ibdeltainprog = 1;
	dd->cspec->ibsymsnap = read_6120_creg32(dd, cr_ibsymbolerr);
	dd->cspec->iblnkerrsnap = read_6120_creg32(dd, cr_iblinkerrrecov);

	
	ibc = 0x5ULL << SYM_LSB(IBCCtrl, FlowCtrlWaterMark);
	ibc |= 0x3ULL << SYM_LSB(IBCCtrl, FlowCtrlPeriod);
	
	dd->cspec->lli_thresh = 0xf;
	ibc |= (u64) dd->cspec->lli_thresh << SYM_LSB(IBCCtrl, PhyerrThreshold);
	
	ibc |= 4ULL << SYM_LSB(IBCCtrl, CreditScale);
	
	ibc |= 0xfULL << SYM_LSB(IBCCtrl, OverrunThreshold);
	ibc |= ((u64)(ppd->ibmaxlen >> 2) + 1) << SYM_LSB(IBCCtrl, MaxPktLen);
	dd->cspec->ibcctrl = ibc; 

	
	val = dd->cspec->ibcctrl | (QLOGIC_IB_IBCC_LINKINITCMD_DISABLE <<
		QLOGIC_IB_IBCC_LINKINITCMD_SHIFT);
	qib_write_kreg(dd, kr_ibcctrl, val);

	val = qib_read_kreg64(dd, kr_serdes_cfg0);
	config1 = qib_read_kreg64(dd, kr_serdes_cfg1);

	val |= SYM_MASK(SerdesCfg0, ResetPLL) |
		SYM_MASK(SerdesCfg0, RxDetEnX) |
		(SYM_MASK(SerdesCfg0, L1PwrDnA) |
		 SYM_MASK(SerdesCfg0, L1PwrDnB) |
		 SYM_MASK(SerdesCfg0, L1PwrDnC) |
		 SYM_MASK(SerdesCfg0, L1PwrDnD));
	qib_write_kreg(dd, kr_serdes_cfg0, val);
	
	qib_read_kreg64(dd, kr_scratch);
	udelay(5);              
	val &= ~(SYM_MASK(SerdesCfg0, RxDetEnX) |
		 SYM_MASK(SerdesCfg0, ResetPLL) |
		 (SYM_MASK(SerdesCfg0, L1PwrDnA) |
		  SYM_MASK(SerdesCfg0, L1PwrDnB) |
		  SYM_MASK(SerdesCfg0, L1PwrDnC) |
		  SYM_MASK(SerdesCfg0, L1PwrDnD)));
	val |= (SYM_MASK(SerdesCfg0, ResetA) |
		SYM_MASK(SerdesCfg0, ResetB) |
		SYM_MASK(SerdesCfg0, ResetC) |
		SYM_MASK(SerdesCfg0, ResetD)) |
		SYM_MASK(SerdesCfg0, TxIdeEnX);
	qib_write_kreg(dd, kr_serdes_cfg0, val);
	
	(void) qib_read_kreg64(dd, kr_scratch);
	udelay(15);
	val &= ~((SYM_MASK(SerdesCfg0, ResetA) |
		  SYM_MASK(SerdesCfg0, ResetB) |
		  SYM_MASK(SerdesCfg0, ResetC) |
		  SYM_MASK(SerdesCfg0, ResetD)) |
		 SYM_MASK(SerdesCfg0, TxIdeEnX));

	qib_write_kreg(dd, kr_serdes_cfg0, val);
	
	(void) qib_read_kreg64(dd, kr_scratch);

	val = qib_read_kreg64(dd, kr_xgxs_cfg);
	prev_val = val;
	if (val & QLOGIC_IB_XGXS_RESET)
		val &= ~QLOGIC_IB_XGXS_RESET;
	if (SYM_FIELD(val, XGXSCfg, polarity_inv) != ppd->rx_pol_inv) {
		
		val &= ~SYM_MASK(XGXSCfg, polarity_inv);
		val |= (u64)ppd->rx_pol_inv << SYM_LSB(XGXSCfg, polarity_inv);
	}
	if (val != prev_val)
		qib_write_kreg(dd, kr_xgxs_cfg, val);

	val = qib_read_kreg64(dd, kr_serdes_cfg0);

	
	config1 &= ~0x0ffffffff00ULL;
	
	config1 |= 0x00000000000ULL;
	
	config1 |= 0x0cccc000000ULL;
	qib_write_kreg(dd, kr_serdes_cfg1, config1);

	
	ppd->guid = dd->base_guid;

	hwstat = qib_read_kreg64(dd, kr_hwerrstatus);
	if (hwstat) {
		
		qib_write_kreg(dd, kr_hwerrclear, hwstat);
		qib_write_kreg(dd, kr_errclear, ERR_MASK(HardwareErr));
	}

	dd->control |= QLOGIC_IB_C_LINKENABLE;
	dd->control &= ~QLOGIC_IB_C_FREEZEMODE;
	qib_write_kreg(dd, kr_control, dd->control);

	return 0;
}

static void qib_6120_quiet_serdes(struct qib_pportdata *ppd)
{
	struct qib_devdata *dd = ppd->dd;
	u64 val;

	qib_set_ib_6120_lstate(ppd, 0, QLOGIC_IB_IBCC_LINKINITCMD_DISABLE);

	
	dd->control &= ~QLOGIC_IB_C_LINKENABLE;
	qib_write_kreg(dd, kr_control,
		       dd->control | QLOGIC_IB_C_FREEZEMODE);

	if (dd->cspec->ibsymdelta || dd->cspec->iblnkerrdelta ||
	    dd->cspec->ibdeltainprog) {
		u64 diagc;

		
		diagc = qib_read_kreg64(dd, kr_hwdiagctrl);
		qib_write_kreg(dd, kr_hwdiagctrl,
			       diagc | SYM_MASK(HwDiagCtrl, CounterWrEnable));

		if (dd->cspec->ibsymdelta || dd->cspec->ibdeltainprog) {
			val = read_6120_creg32(dd, cr_ibsymbolerr);
			if (dd->cspec->ibdeltainprog)
				val -= val - dd->cspec->ibsymsnap;
			val -= dd->cspec->ibsymdelta;
			write_6120_creg(dd, cr_ibsymbolerr, val);
		}
		if (dd->cspec->iblnkerrdelta || dd->cspec->ibdeltainprog) {
			val = read_6120_creg32(dd, cr_iblinkerrrecov);
			if (dd->cspec->ibdeltainprog)
				val -= val - dd->cspec->iblnkerrsnap;
			val -= dd->cspec->iblnkerrdelta;
			write_6120_creg(dd, cr_iblinkerrrecov, val);
		}

		
		qib_write_kreg(dd, kr_hwdiagctrl, diagc);
	}

	val = qib_read_kreg64(dd, kr_serdes_cfg0);
	val |= SYM_MASK(SerdesCfg0, TxIdeEnX);
	qib_write_kreg(dd, kr_serdes_cfg0, val);
}

static void qib_6120_setup_setextled(struct qib_pportdata *ppd, u32 on)
{
	u64 extctl, val, lst, ltst;
	unsigned long flags;
	struct qib_devdata *dd = ppd->dd;

	if (dd->diag_client)
		return;

	
	if (ppd->led_override) {
		ltst = (ppd->led_override & QIB_LED_PHYS) ?
			IB_PHYSPORTSTATE_LINKUP : IB_PHYSPORTSTATE_DISABLED,
		lst = (ppd->led_override & QIB_LED_LOG) ?
			IB_PORT_ACTIVE : IB_PORT_DOWN;
	} else if (on) {
		val = qib_read_kreg64(dd, kr_ibcstatus);
		ltst = qib_6120_phys_portstate(val);
		lst = qib_6120_iblink_state(val);
	} else {
		ltst = 0;
		lst = 0;
	}

	spin_lock_irqsave(&dd->cspec->gpio_lock, flags);
	extctl = dd->cspec->extctrl & ~(SYM_MASK(EXTCtrl, LEDPriPortGreenOn) |
				 SYM_MASK(EXTCtrl, LEDPriPortYellowOn));

	if (ltst == IB_PHYSPORTSTATE_LINKUP)
		extctl |= SYM_MASK(EXTCtrl, LEDPriPortYellowOn);
	if (lst == IB_PORT_ACTIVE)
		extctl |= SYM_MASK(EXTCtrl, LEDPriPortGreenOn);
	dd->cspec->extctrl = extctl;
	qib_write_kreg(dd, kr_extctrl, extctl);
	spin_unlock_irqrestore(&dd->cspec->gpio_lock, flags);
}

static void qib_6120_free_irq(struct qib_devdata *dd)
{
	if (dd->cspec->irq) {
		free_irq(dd->cspec->irq, dd);
		dd->cspec->irq = 0;
	}
	qib_nomsi(dd);
}

static void qib_6120_setup_cleanup(struct qib_devdata *dd)
{
	qib_6120_free_irq(dd);
	kfree(dd->cspec->cntrs);
	kfree(dd->cspec->portcntrs);
	if (dd->cspec->dummy_hdrq) {
		dma_free_coherent(&dd->pcidev->dev,
				  ALIGN(dd->rcvhdrcnt *
					dd->rcvhdrentsize *
					sizeof(u32), PAGE_SIZE),
				  dd->cspec->dummy_hdrq,
				  dd->cspec->dummy_hdrq_phys);
		dd->cspec->dummy_hdrq = NULL;
	}
}

static void qib_wantpiobuf_6120_intr(struct qib_devdata *dd, u32 needint)
{
	unsigned long flags;

	spin_lock_irqsave(&dd->sendctrl_lock, flags);
	if (needint)
		dd->sendctrl |= SYM_MASK(SendCtrl, PIOIntBufAvail);
	else
		dd->sendctrl &= ~SYM_MASK(SendCtrl, PIOIntBufAvail);
	qib_write_kreg(dd, kr_sendctrl, dd->sendctrl);
	qib_write_kreg(dd, kr_scratch, 0ULL);
	spin_unlock_irqrestore(&dd->sendctrl_lock, flags);
}

static noinline void unlikely_6120_intr(struct qib_devdata *dd, u64 istat)
{
	if (unlikely(istat & ~QLOGIC_IB_I_BITSEXTANT))
		qib_dev_err(dd, "interrupt with unknown interrupts %Lx set\n",
			    istat & ~QLOGIC_IB_I_BITSEXTANT);

	if (istat & QLOGIC_IB_I_ERROR) {
		u64 estat = 0;

		qib_stats.sps_errints++;
		estat = qib_read_kreg64(dd, kr_errstatus);
		if (!estat)
			qib_devinfo(dd->pcidev, "error interrupt (%Lx), "
				 "but no error bits set!\n", istat);
		handle_6120_errors(dd, estat);
	}

	if (istat & QLOGIC_IB_I_GPIO) {
		u32 gpiostatus;
		u32 to_clear = 0;

		gpiostatus = qib_read_kreg32(dd, kr_gpio_status);
		
		if (gpiostatus & GPIO_ERRINTR_MASK) {
			
			to_clear |= (gpiostatus & GPIO_ERRINTR_MASK);

			if (gpiostatus & (1 << GPIO_RXUVL_BIT))
				dd->cspec->rxfc_unsupvl_errs++;
			if (gpiostatus & (1 << GPIO_OVRUN_BIT))
				dd->cspec->overrun_thresh_errs++;
			if (gpiostatus & (1 << GPIO_LLI_BIT))
				dd->cspec->lli_errs++;
			gpiostatus &= ~GPIO_ERRINTR_MASK;
		}
		if (gpiostatus) {
			const u32 mask = qib_read_kreg32(dd, kr_gpio_mask);

			if (mask & gpiostatus) {
				to_clear |= (gpiostatus & mask);
				dd->cspec->gpio_mask &= ~(gpiostatus & mask);
				qib_write_kreg(dd, kr_gpio_mask,
					       dd->cspec->gpio_mask);
			}
		}
		if (to_clear)
			qib_write_kreg(dd, kr_gpio_clear, (u64) to_clear);
	}
}

static irqreturn_t qib_6120intr(int irq, void *data)
{
	struct qib_devdata *dd = data;
	irqreturn_t ret;
	u32 istat, ctxtrbits, rmask, crcs = 0;
	unsigned i;

	if ((dd->flags & (QIB_PRESENT | QIB_BADINTR)) != QIB_PRESENT) {
		ret = IRQ_HANDLED;
		goto bail;
	}

	istat = qib_read_kreg32(dd, kr_intstatus);

	if (unlikely(!istat)) {
		ret = IRQ_NONE; 
		goto bail;
	}
	if (unlikely(istat == -1)) {
		qib_bad_intrstatus(dd);
		
		ret = IRQ_NONE;
		goto bail;
	}

	qib_stats.sps_ints++;
	if (dd->int_counter != (u32) -1)
		dd->int_counter++;

	if (unlikely(istat & (~QLOGIC_IB_I_BITSEXTANT |
			      QLOGIC_IB_I_GPIO | QLOGIC_IB_I_ERROR)))
		unlikely_6120_intr(dd, istat);

	qib_write_kreg(dd, kr_intclear, istat);

	ctxtrbits = istat &
		((QLOGIC_IB_I_RCVAVAIL_MASK << QLOGIC_IB_I_RCVAVAIL_SHIFT) |
		 (QLOGIC_IB_I_RCVURG_MASK << QLOGIC_IB_I_RCVURG_SHIFT));
	if (ctxtrbits) {
		rmask = (1U << QLOGIC_IB_I_RCVAVAIL_SHIFT) |
			(1U << QLOGIC_IB_I_RCVURG_SHIFT);
		for (i = 0; i < dd->first_user_ctxt; i++) {
			if (ctxtrbits & rmask) {
				ctxtrbits &= ~rmask;
				crcs += qib_kreceive(dd->rcd[i],
						     &dd->cspec->lli_counter,
						     NULL);
			}
			rmask <<= 1;
		}
		if (crcs) {
			u32 cntr = dd->cspec->lli_counter;
			cntr += crcs;
			if (cntr) {
				if (cntr > dd->cspec->lli_thresh) {
					dd->cspec->lli_counter = 0;
					dd->cspec->lli_errs++;
				} else
					dd->cspec->lli_counter += cntr;
			}
		}


		if (ctxtrbits) {
			ctxtrbits =
				(ctxtrbits >> QLOGIC_IB_I_RCVAVAIL_SHIFT) |
				(ctxtrbits >> QLOGIC_IB_I_RCVURG_SHIFT);
			qib_handle_urcv(dd, ctxtrbits);
		}
	}

	if ((istat & QLOGIC_IB_I_SPIOBUFAVAIL) && (dd->flags & QIB_INITTED))
		qib_ib_piobufavail(dd);

	ret = IRQ_HANDLED;
bail:
	return ret;
}

static void qib_setup_6120_interrupt(struct qib_devdata *dd)
{
	if (SYM_FIELD(dd->revision, Revision_R,
		      ChipRevMinor) > 1) {
		
		dd->cspec->gpio_mask |= GPIO_ERRINTR_MASK;
		qib_write_kreg(dd, kr_gpio_mask, dd->cspec->gpio_mask);
	}

	if (!dd->cspec->irq)
		qib_dev_err(dd, "irq is 0, BIOS error?  Interrupts won't "
			    "work\n");
	else {
		int ret;
		ret = request_irq(dd->cspec->irq, qib_6120intr, 0,
				  QIB_DRV_NAME, dd);
		if (ret)
			qib_dev_err(dd, "Couldn't setup interrupt "
				    "(irq=%d): %d\n", dd->cspec->irq,
				    ret);
	}
}

static void pe_boardname(struct qib_devdata *dd)
{
	char *n;
	u32 boardid, namelen;

	boardid = SYM_FIELD(dd->revision, Revision,
			    BoardID);

	switch (boardid) {
	case 2:
		n = "InfiniPath_QLE7140";
		break;
	default:
		qib_dev_err(dd, "Unknown 6120 board with ID %u\n", boardid);
		n = "Unknown_InfiniPath_6120";
		break;
	}
	namelen = strlen(n) + 1;
	dd->boardname = kmalloc(namelen, GFP_KERNEL);
	if (!dd->boardname)
		qib_dev_err(dd, "Failed allocation for board name: %s\n", n);
	else
		snprintf(dd->boardname, namelen, "%s", n);

	if (dd->majrev != 4 || !dd->minrev || dd->minrev > 2)
		qib_dev_err(dd, "Unsupported InfiniPath hardware revision "
			    "%u.%u!\n", dd->majrev, dd->minrev);

	snprintf(dd->boardversion, sizeof(dd->boardversion),
		 "ChipABI %u.%u, %s, InfiniPath%u %u.%u, SW Compat %u\n",
		 QIB_CHIP_VERS_MAJ, QIB_CHIP_VERS_MIN, dd->boardname,
		 (unsigned)SYM_FIELD(dd->revision, Revision_R, Arch),
		 dd->majrev, dd->minrev,
		 (unsigned)SYM_FIELD(dd->revision, Revision_R, SW));

}

static int qib_6120_setup_reset(struct qib_devdata *dd)
{
	u64 val;
	int i;
	int ret;
	u16 cmdval;
	u8 int_line, clinesz;

	qib_pcie_getcmd(dd, &cmdval, &int_line, &clinesz);

	
	qib_dev_err(dd, "Resetting InfiniPath unit %u\n", dd->unit);

	
	qib_6120_set_intr_state(dd, 0);

	dd->cspec->ibdeltainprog = 0;
	dd->cspec->ibsymdelta = 0;
	dd->cspec->iblnkerrdelta = 0;

	dd->flags &= ~(QIB_INITTED | QIB_PRESENT);
	dd->int_counter = 0; 
	val = dd->control | QLOGIC_IB_C_RESET;
	writeq(val, &dd->kregbase[kr_control]);
	mb(); 

	for (i = 1; i <= 5; i++) {
		msleep(1000 + (1 + i) * 2000);

		qib_pcie_reenable(dd, cmdval, int_line, clinesz);

		val = readq(&dd->kregbase[kr_revision]);
		if (val == dd->revision) {
			dd->flags |= QIB_PRESENT; 
			ret = qib_reinit_intr(dd);
			goto bail;
		}
	}
	ret = 0; 

bail:
	if (ret) {
		if (qib_pcie_params(dd, dd->lbus_width, NULL, NULL))
			qib_dev_err(dd, "Reset failed to setup PCIe or "
				    "interrupts; continuing anyway\n");
		
		qib_6120_init_hwerrors(dd);
		
		qib_write_kreg(dd, kr_gpio_mask, dd->cspec->gpio_mask);
		
		qib_6120_init_hwerrors(dd);
	}
	return ret;
}

static void qib_6120_put_tid(struct qib_devdata *dd, u64 __iomem *tidptr,
			     u32 type, unsigned long pa)
{
	u32 __iomem *tidp32 = (u32 __iomem *)tidptr;
	unsigned long flags;
	int tidx;
	spinlock_t *tidlockp; 

	if (!dd->kregbase)
		return;

	if (pa != dd->tidinvalid) {
		if (pa & ((1U << 11) - 1)) {
			qib_dev_err(dd, "Physaddr %lx not 2KB aligned!\n",
				    pa);
			return;
		}
		pa >>= 11;
		if (pa & ~QLOGIC_IB_RT_ADDR_MASK) {
			qib_dev_err(dd, "Physical page address 0x%lx "
				    "larger than supported\n", pa);
			return;
		}

		if (type == RCVHQ_RCV_TYPE_EAGER)
			pa |= dd->tidtemplate;
		else 
			pa |= 2 << 29;
	}

	tidx = tidptr - dd->egrtidbase;

	tidlockp = (type == RCVHQ_RCV_TYPE_EAGER && tidx < dd->rcvhdrcnt)
		? &dd->cspec->kernel_tid_lock : &dd->cspec->user_tid_lock;
	spin_lock_irqsave(tidlockp, flags);
	qib_write_kreg(dd, kr_scratch, 0xfeeddeaf);
	writel(pa, tidp32);
	qib_write_kreg(dd, kr_scratch, 0xdeadbeef);
	mmiowb();
	spin_unlock_irqrestore(tidlockp, flags);
}

static void qib_6120_put_tid_2(struct qib_devdata *dd, u64 __iomem *tidptr,
			       u32 type, unsigned long pa)
{
	u32 __iomem *tidp32 = (u32 __iomem *)tidptr;
	u32 tidx;

	if (!dd->kregbase)
		return;

	if (pa != dd->tidinvalid) {
		if (pa & ((1U << 11) - 1)) {
			qib_dev_err(dd, "Physaddr %lx not 2KB aligned!\n",
				    pa);
			return;
		}
		pa >>= 11;
		if (pa & ~QLOGIC_IB_RT_ADDR_MASK) {
			qib_dev_err(dd, "Physical page address 0x%lx "
				    "larger than supported\n", pa);
			return;
		}

		if (type == RCVHQ_RCV_TYPE_EAGER)
			pa |= dd->tidtemplate;
		else 
			pa |= 2 << 29;
	}
	tidx = tidptr - dd->egrtidbase;
	writel(pa, tidp32);
	mmiowb();
}


static void qib_6120_clear_tids(struct qib_devdata *dd,
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
		((char __iomem *)(dd->kregbase) +
		 dd->rcvtidbase +
		 ctxt * dd->rcvtidcnt * sizeof(*tidbase));

	for (i = 0; i < dd->rcvtidcnt; i++)
		
		dd->f_put_tid(dd, &tidbase[i], RCVHQ_RCV_TYPE_EXPECTED,
				  tidinv);

	tidbase = (u64 __iomem *)
		((char __iomem *)(dd->kregbase) +
		 dd->rcvegrbase +
		 rcd->rcvegr_tid_base * sizeof(*tidbase));

	for (i = 0; i < rcd->rcvegrcnt; i++)
		
		dd->f_put_tid(dd, &tidbase[i], RCVHQ_RCV_TYPE_EAGER,
				  tidinv);
}

static void qib_6120_tidtemplate(struct qib_devdata *dd)
{
	u32 egrsize = dd->rcvegrbufsize;

	if (egrsize == 2048)
		dd->tidtemplate = 1U << 29;
	else if (egrsize == 4096)
		dd->tidtemplate = 2U << 29;
	dd->tidinvalid = 0;
}

int __attribute__((weak)) qib_unordered_wc(void)
{
	return 0;
}

static int qib_6120_get_base_info(struct qib_ctxtdata *rcd,
				  struct qib_base_info *kinfo)
{
	if (qib_unordered_wc())
		kinfo->spi_runtime_flags |= QIB_RUNTIME_FORCE_WC_ORDER;

	kinfo->spi_runtime_flags |= QIB_RUNTIME_PCIE |
		QIB_RUNTIME_FORCE_PIOAVAIL | QIB_RUNTIME_PIO_REGSWAPPED;
	return 0;
}


static struct qib_message_header *
qib_6120_get_msgheader(struct qib_devdata *dd, __le32 *rhf_addr)
{
	return (struct qib_message_header *)
		&rhf_addr[sizeof(u64) / sizeof(u32)];
}

static void qib_6120_config_ctxts(struct qib_devdata *dd)
{
	dd->ctxtcnt = qib_read_kreg32(dd, kr_portcnt);
	if (qib_n_krcv_queues > 1) {
		dd->first_user_ctxt = qib_n_krcv_queues * dd->num_pports;
		if (dd->first_user_ctxt > dd->ctxtcnt)
			dd->first_user_ctxt = dd->ctxtcnt;
		dd->qpn_mask = dd->first_user_ctxt <= 2 ? 2 : 6;
	} else
		dd->first_user_ctxt = dd->num_pports;
	dd->n_krcv_queues = dd->first_user_ctxt;
}

static void qib_update_6120_usrhead(struct qib_ctxtdata *rcd, u64 hd,
				    u32 updegr, u32 egrhd, u32 npkts)
{
	if (updegr)
		qib_write_ureg(rcd->dd, ur_rcvegrindexhead, egrhd, rcd->ctxt);
	mmiowb();
	qib_write_ureg(rcd->dd, ur_rcvhdrhead, hd, rcd->ctxt);
	mmiowb();
}

static u32 qib_6120_hdrqempty(struct qib_ctxtdata *rcd)
{
	u32 head, tail;

	head = qib_read_ureg32(rcd->dd, ur_rcvhdrhead, rcd->ctxt);
	if (rcd->rcvhdrtail_kvaddr)
		tail = qib_get_rcvhdrtail(rcd);
	else
		tail = qib_read_ureg32(rcd->dd, ur_rcvhdrtail, rcd->ctxt);
	return head == tail;
}

static void alloc_dummy_hdrq(struct qib_devdata *dd)
{
	dd->cspec->dummy_hdrq = dma_alloc_coherent(&dd->pcidev->dev,
					dd->rcd[0]->rcvhdrq_size,
					&dd->cspec->dummy_hdrq_phys,
					GFP_ATOMIC | __GFP_COMP);
	if (!dd->cspec->dummy_hdrq) {
		qib_devinfo(dd->pcidev, "Couldn't allocate dummy hdrq\n");
		
		dd->cspec->dummy_hdrq_phys = 0UL;
	}
}

static void rcvctrl_6120_mod(struct qib_pportdata *ppd, unsigned int op,
			     int ctxt)
{
	struct qib_devdata *dd = ppd->dd;
	u64 mask, val;
	unsigned long flags;

	spin_lock_irqsave(&dd->cspec->rcvmod_lock, flags);

	if (op & QIB_RCVCTRL_TAILUPD_ENB)
		dd->rcvctrl |= (1ULL << QLOGIC_IB_R_TAILUPD_SHIFT);
	if (op & QIB_RCVCTRL_TAILUPD_DIS)
		dd->rcvctrl &= ~(1ULL << QLOGIC_IB_R_TAILUPD_SHIFT);
	if (op & QIB_RCVCTRL_PKEY_ENB)
		dd->rcvctrl &= ~(1ULL << IBA6120_R_PKEY_DIS_SHIFT);
	if (op & QIB_RCVCTRL_PKEY_DIS)
		dd->rcvctrl |= (1ULL << IBA6120_R_PKEY_DIS_SHIFT);
	if (ctxt < 0)
		mask = (1ULL << dd->ctxtcnt) - 1;
	else
		mask = (1ULL << ctxt);
	if (op & QIB_RCVCTRL_CTXT_ENB) {
		
		dd->rcvctrl |= (mask << SYM_LSB(RcvCtrl, PortEnable));
		if (!(dd->flags & QIB_NODMA_RTAIL))
			dd->rcvctrl |= 1ULL << QLOGIC_IB_R_TAILUPD_SHIFT;
		
		qib_write_kreg_ctxt(dd, kr_rcvhdrtailaddr, ctxt,
			dd->rcd[ctxt]->rcvhdrqtailaddr_phys);
		qib_write_kreg_ctxt(dd, kr_rcvhdraddr, ctxt,
			dd->rcd[ctxt]->rcvhdrq_phys);

		if (ctxt == 0 && !dd->cspec->dummy_hdrq)
			alloc_dummy_hdrq(dd);
	}
	if (op & QIB_RCVCTRL_CTXT_DIS)
		dd->rcvctrl &= ~(mask << SYM_LSB(RcvCtrl, PortEnable));
	if (op & QIB_RCVCTRL_INTRAVAIL_ENB)
		dd->rcvctrl |= (mask << QLOGIC_IB_R_INTRAVAIL_SHIFT);
	if (op & QIB_RCVCTRL_INTRAVAIL_DIS)
		dd->rcvctrl &= ~(mask << QLOGIC_IB_R_INTRAVAIL_SHIFT);
	qib_write_kreg(dd, kr_rcvctrl, dd->rcvctrl);
	if ((op & QIB_RCVCTRL_INTRAVAIL_ENB) && dd->rhdrhead_intr_off) {
		
		val = qib_read_ureg32(dd, ur_rcvhdrhead, ctxt) |
			dd->rhdrhead_intr_off;
		qib_write_ureg(dd, ur_rcvhdrhead, val, ctxt);
	}
	if (op & QIB_RCVCTRL_CTXT_ENB) {
		val = qib_read_ureg32(dd, ur_rcvegrindextail, ctxt);
		qib_write_ureg(dd, ur_rcvegrindexhead, val, ctxt);

		val = qib_read_ureg32(dd, ur_rcvhdrtail, ctxt);
		dd->rcd[ctxt]->head = val;
		
		if (ctxt < dd->first_user_ctxt)
			val |= dd->rhdrhead_intr_off;
		qib_write_ureg(dd, ur_rcvhdrhead, val, ctxt);
	}
	if (op & QIB_RCVCTRL_CTXT_DIS) {
		if (ctxt >= 0) {
			qib_write_kreg_ctxt(dd, kr_rcvhdrtailaddr, ctxt,
					    dd->cspec->dummy_hdrq_phys);
			qib_write_kreg_ctxt(dd, kr_rcvhdraddr, ctxt,
					    dd->cspec->dummy_hdrq_phys);
		} else {
			unsigned i;

			for (i = 0; i < dd->cfgctxts; i++) {
				qib_write_kreg_ctxt(dd, kr_rcvhdrtailaddr,
					    i, dd->cspec->dummy_hdrq_phys);
				qib_write_kreg_ctxt(dd, kr_rcvhdraddr,
					    i, dd->cspec->dummy_hdrq_phys);
			}
		}
	}
	spin_unlock_irqrestore(&dd->cspec->rcvmod_lock, flags);
}

static void sendctrl_6120_mod(struct qib_pportdata *ppd, u32 op)
{
	struct qib_devdata *dd = ppd->dd;
	u64 tmp_dd_sendctrl;
	unsigned long flags;

	spin_lock_irqsave(&dd->sendctrl_lock, flags);

	
	if (op & QIB_SENDCTRL_CLEAR)
		dd->sendctrl = 0;
	if (op & QIB_SENDCTRL_SEND_DIS)
		dd->sendctrl &= ~SYM_MASK(SendCtrl, PIOEnable);
	else if (op & QIB_SENDCTRL_SEND_ENB)
		dd->sendctrl |= SYM_MASK(SendCtrl, PIOEnable);
	if (op & QIB_SENDCTRL_AVAIL_DIS)
		dd->sendctrl &= ~SYM_MASK(SendCtrl, PIOBufAvailUpd);
	else if (op & QIB_SENDCTRL_AVAIL_ENB)
		dd->sendctrl |= SYM_MASK(SendCtrl, PIOBufAvailUpd);

	if (op & QIB_SENDCTRL_DISARM_ALL) {
		u32 i, last;

		tmp_dd_sendctrl = dd->sendctrl;
		last = dd->piobcnt2k + dd->piobcnt4k;
		tmp_dd_sendctrl &=
			~(SYM_MASK(SendCtrl, PIOEnable) |
			  SYM_MASK(SendCtrl, PIOBufAvailUpd));
		for (i = 0; i < last; i++) {
			qib_write_kreg(dd, kr_sendctrl, tmp_dd_sendctrl |
				       SYM_MASK(SendCtrl, Disarm) | i);
			qib_write_kreg(dd, kr_scratch, 0);
		}
	}

	tmp_dd_sendctrl = dd->sendctrl;

	if (op & QIB_SENDCTRL_FLUSH)
		tmp_dd_sendctrl |= SYM_MASK(SendCtrl, Abort);
	if (op & QIB_SENDCTRL_DISARM)
		tmp_dd_sendctrl |= SYM_MASK(SendCtrl, Disarm) |
			((op & QIB_6120_SendCtrl_DisarmPIOBuf_RMASK) <<
			 SYM_LSB(SendCtrl, DisarmPIOBuf));
	if (op & QIB_SENDCTRL_AVAIL_BLIP)
		tmp_dd_sendctrl &= ~SYM_MASK(SendCtrl, PIOBufAvailUpd);

	qib_write_kreg(dd, kr_sendctrl, tmp_dd_sendctrl);
	qib_write_kreg(dd, kr_scratch, 0);

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

static u64 qib_portcntr_6120(struct qib_pportdata *ppd, u32 reg)
{
	u64 ret = 0ULL;
	struct qib_devdata *dd = ppd->dd;
	u16 creg;
	
	static const u16 xlator[] = {
		[QIBPORTCNTR_PKTSEND] = cr_pktsend,
		[QIBPORTCNTR_WORDSEND] = cr_wordsend,
		[QIBPORTCNTR_PSXMITDATA] = 0xffff,
		[QIBPORTCNTR_PSXMITPKTS] = 0xffff,
		[QIBPORTCNTR_PSXMITWAIT] = 0xffff,
		[QIBPORTCNTR_SENDSTALL] = cr_sendstall,
		[QIBPORTCNTR_PKTRCV] = cr_pktrcv,
		[QIBPORTCNTR_PSRCVDATA] = 0xffff,
		[QIBPORTCNTR_PSRCVPKTS] = 0xffff,
		[QIBPORTCNTR_RCVEBP] = cr_rcvebp,
		[QIBPORTCNTR_RCVOVFL] = cr_rcvovfl,
		[QIBPORTCNTR_WORDRCV] = cr_wordrcv,
		[QIBPORTCNTR_RXDROPPKT] = cr_rxdroppkt,
		[QIBPORTCNTR_RXLOCALPHYERR] = 0xffff,
		[QIBPORTCNTR_RXVLERR] = 0xffff,
		[QIBPORTCNTR_ERRICRC] = cr_erricrc,
		[QIBPORTCNTR_ERRVCRC] = cr_errvcrc,
		[QIBPORTCNTR_ERRLPCRC] = cr_errlpcrc,
		[QIBPORTCNTR_BADFORMAT] = cr_badformat,
		[QIBPORTCNTR_ERR_RLEN] = cr_err_rlen,
		[QIBPORTCNTR_IBSYMBOLERR] = cr_ibsymbolerr,
		[QIBPORTCNTR_INVALIDRLEN] = cr_invalidrlen,
		[QIBPORTCNTR_UNSUPVL] = cr_txunsupvl,
		[QIBPORTCNTR_EXCESSBUFOVFL] = 0xffff,
		[QIBPORTCNTR_ERRLINK] = cr_errlink,
		[QIBPORTCNTR_IBLINKDOWN] = cr_iblinkdown,
		[QIBPORTCNTR_IBLINKERRRECOV] = cr_iblinkerrrecov,
		[QIBPORTCNTR_LLI] = 0xffff,
		[QIBPORTCNTR_PSINTERVAL] = 0xffff,
		[QIBPORTCNTR_PSSTART] = 0xffff,
		[QIBPORTCNTR_PSSTAT] = 0xffff,
		[QIBPORTCNTR_VL15PKTDROP] = 0xffff,
		[QIBPORTCNTR_ERRPKEY] = cr_errpkey,
		[QIBPORTCNTR_KHDROVFL] = 0xffff,
	};

	if (reg >= ARRAY_SIZE(xlator)) {
		qib_devinfo(ppd->dd->pcidev,
			 "Unimplemented portcounter %u\n", reg);
		goto done;
	}
	creg = xlator[reg];

	
	if (reg == QIBPORTCNTR_LLI)
		ret = dd->cspec->lli_errs;
	else if (reg == QIBPORTCNTR_EXCESSBUFOVFL)
		ret = dd->cspec->overrun_thresh_errs;
	else if (reg == QIBPORTCNTR_KHDROVFL) {
		int i;

		
		for (i = 0; i < dd->first_user_ctxt; i++)
			ret += read_6120_creg32(dd, cr_portovfl + i);
	} else if (reg == QIBPORTCNTR_PSSTAT)
		ret = dd->cspec->pma_sample_status;
	if (creg == 0xffff)
		goto done;

	if (creg == cr_wordsend || creg == cr_wordrcv ||
	    creg == cr_pktsend || creg == cr_pktrcv)
		ret = read_6120_creg(dd, creg);
	else
		ret = read_6120_creg32(dd, creg);
	if (creg == cr_ibsymbolerr) {
		if (dd->cspec->ibdeltainprog)
			ret -= ret - dd->cspec->ibsymsnap;
		ret -= dd->cspec->ibsymdelta;
	} else if (creg == cr_iblinkerrrecov) {
		if (dd->cspec->ibdeltainprog)
			ret -= ret - dd->cspec->iblnkerrsnap;
		ret -= dd->cspec->iblnkerrdelta;
	}
	if (reg == QIBPORTCNTR_RXDROPPKT) 
		ret += dd->cspec->rxfc_unsupvl_errs;

done:
	return ret;
}

static const char cntr6120names[] =
	"Interrupts\n"
	"HostBusStall\n"
	"E RxTIDFull\n"
	"RxTIDInvalid\n"
	"Ctxt0EgrOvfl\n"
	"Ctxt1EgrOvfl\n"
	"Ctxt2EgrOvfl\n"
	"Ctxt3EgrOvfl\n"
	"Ctxt4EgrOvfl\n";

static const size_t cntr6120indices[] = {
	cr_lbint,
	cr_lbflowstall,
	cr_errtidfull,
	cr_errtidvalid,
	cr_portovfl + 0,
	cr_portovfl + 1,
	cr_portovfl + 2,
	cr_portovfl + 3,
	cr_portovfl + 4,
};

static const char portcntr6120names[] =
	"TxPkt\n"
	"TxFlowPkt\n"
	"TxWords\n"
	"RxPkt\n"
	"RxFlowPkt\n"
	"RxWords\n"
	"TxFlowStall\n"
	"E IBStatusChng\n"
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
	;

#define _PORT_VIRT_FLAG 0x8000 
static const size_t portcntr6120indices[] = {
	QIBPORTCNTR_PKTSEND | _PORT_VIRT_FLAG,
	cr_pktsendflow,
	QIBPORTCNTR_WORDSEND | _PORT_VIRT_FLAG,
	QIBPORTCNTR_PKTRCV | _PORT_VIRT_FLAG,
	cr_pktrcvflowctrl,
	QIBPORTCNTR_WORDRCV | _PORT_VIRT_FLAG,
	QIBPORTCNTR_SENDSTALL | _PORT_VIRT_FLAG,
	cr_ibstatuschange,
	QIBPORTCNTR_IBLINKDOWN | _PORT_VIRT_FLAG,
	QIBPORTCNTR_IBLINKERRRECOV | _PORT_VIRT_FLAG,
	QIBPORTCNTR_ERRLINK | _PORT_VIRT_FLAG,
	QIBPORTCNTR_IBSYMBOLERR | _PORT_VIRT_FLAG,
	QIBPORTCNTR_LLI | _PORT_VIRT_FLAG,
	QIBPORTCNTR_BADFORMAT | _PORT_VIRT_FLAG,
	QIBPORTCNTR_ERR_RLEN | _PORT_VIRT_FLAG,
	QIBPORTCNTR_RCVOVFL | _PORT_VIRT_FLAG,
	QIBPORTCNTR_RCVEBP | _PORT_VIRT_FLAG,
	cr_rcvflowctrl_err,
	QIBPORTCNTR_ERRICRC | _PORT_VIRT_FLAG,
	QIBPORTCNTR_ERRLPCRC | _PORT_VIRT_FLAG,
	QIBPORTCNTR_ERRVCRC | _PORT_VIRT_FLAG,
	QIBPORTCNTR_INVALIDRLEN | _PORT_VIRT_FLAG,
	QIBPORTCNTR_ERRPKEY | _PORT_VIRT_FLAG,
	QIBPORTCNTR_RXDROPPKT | _PORT_VIRT_FLAG,
	cr_invalidslen,
	cr_senddropped,
	cr_errslen,
	cr_sendunderrun,
	cr_txunsupvl,
};

static void init_6120_cntrnames(struct qib_devdata *dd)
{
	int i, j = 0;
	char *s;

	for (i = 0, s = (char *)cntr6120names; s && j <= dd->cfgctxts;
	     i++) {
		
		if (!j && !strncmp("Ctxt0EgrOvfl", s + 1, 12))
			j = 1;
		s = strchr(s + 1, '\n');
		if (s && j)
			j++;
	}
	dd->cspec->ncntrs = i;
	if (!s)
		
		dd->cspec->cntrnamelen = sizeof(cntr6120names) - 1;
	else
		dd->cspec->cntrnamelen = 1 + s - cntr6120names;
	dd->cspec->cntrs = kmalloc(dd->cspec->ncntrs
		* sizeof(u64), GFP_KERNEL);
	if (!dd->cspec->cntrs)
		qib_dev_err(dd, "Failed allocation for counters\n");

	for (i = 0, s = (char *)portcntr6120names; s; i++)
		s = strchr(s + 1, '\n');
	dd->cspec->nportcntrs = i - 1;
	dd->cspec->portcntrnamelen = sizeof(portcntr6120names) - 1;
	dd->cspec->portcntrs = kmalloc(dd->cspec->nportcntrs
		* sizeof(u64), GFP_KERNEL);
	if (!dd->cspec->portcntrs)
		qib_dev_err(dd, "Failed allocation for portcounters\n");
}

static u32 qib_read_6120cntrs(struct qib_devdata *dd, loff_t pos, char **namep,
			      u64 **cntrp)
{
	u32 ret;

	if (namep) {
		ret = dd->cspec->cntrnamelen;
		if (pos >= ret)
			ret = 0; 
		else
			*namep = (char *)cntr6120names;
	} else {
		u64 *cntr = dd->cspec->cntrs;
		int i;

		ret = dd->cspec->ncntrs * sizeof(u64);
		if (!cntr || pos >= ret) {
			
			ret = 0;
			goto done;
		}
		if (pos >= ret) {
			ret = 0; 
			goto done;
		}
		*cntrp = cntr;
		for (i = 0; i < dd->cspec->ncntrs; i++)
			*cntr++ = read_6120_creg32(dd, cntr6120indices[i]);
	}
done:
	return ret;
}

static u32 qib_read_6120portcntrs(struct qib_devdata *dd, loff_t pos, u32 port,
				  char **namep, u64 **cntrp)
{
	u32 ret;

	if (namep) {
		ret = dd->cspec->portcntrnamelen;
		if (pos >= ret)
			ret = 0; 
		else
			*namep = (char *)portcntr6120names;
	} else {
		u64 *cntr = dd->cspec->portcntrs;
		struct qib_pportdata *ppd = &dd->pport[port];
		int i;

		ret = dd->cspec->nportcntrs * sizeof(u64);
		if (!cntr || pos >= ret) {
			
			ret = 0;
			goto done;
		}
		*cntrp = cntr;
		for (i = 0; i < dd->cspec->nportcntrs; i++) {
			if (portcntr6120indices[i] & _PORT_VIRT_FLAG)
				*cntr++ = qib_portcntr_6120(ppd,
					portcntr6120indices[i] &
					~_PORT_VIRT_FLAG);
			else
				*cntr++ = read_6120_creg32(dd,
					   portcntr6120indices[i]);
		}
	}
done:
	return ret;
}

static void qib_chk_6120_errormask(struct qib_devdata *dd)
{
	static u32 fixed;
	u32 ctrl;
	unsigned long errormask;
	unsigned long hwerrs;

	if (!dd->cspec->errormask || !(dd->flags & QIB_INITTED))
		return;

	errormask = qib_read_kreg64(dd, kr_errmask);

	if (errormask == dd->cspec->errormask)
		return;
	fixed++;

	hwerrs = qib_read_kreg64(dd, kr_hwerrstatus);
	ctrl = qib_read_kreg32(dd, kr_control);

	qib_write_kreg(dd, kr_errmask,
		dd->cspec->errormask);

	if ((hwerrs & dd->cspec->hwerrmask) ||
	    (ctrl & QLOGIC_IB_C_FREEZEMODE)) {
		qib_write_kreg(dd, kr_hwerrclear, 0ULL);
		qib_write_kreg(dd, kr_errclear, 0ULL);
		
		qib_write_kreg(dd, kr_intclear, 0ULL);
		qib_devinfo(dd->pcidev,
			 "errormask fixed(%u) %lx->%lx, ctrl %x hwerr %lx\n",
			 fixed, errormask, (unsigned long)dd->cspec->errormask,
			 ctrl, hwerrs);
	}
}

static void qib_get_6120_faststats(unsigned long opaque)
{
	struct qib_devdata *dd = (struct qib_devdata *) opaque;
	struct qib_pportdata *ppd = dd->pport;
	unsigned long flags;
	u64 traffic_wds;

	if (!(dd->flags & QIB_INITTED) || dd->diag_client)
		
		goto done;

	traffic_wds = qib_portcntr_6120(ppd, cr_wordsend) +
		qib_portcntr_6120(ppd, cr_wordrcv);
	spin_lock_irqsave(&dd->eep_st_lock, flags);
	traffic_wds -= dd->traffic_wds;
	dd->traffic_wds += traffic_wds;
	if (traffic_wds  >= QIB_TRAFFIC_ACTIVE_THRESHOLD)
		atomic_add(5, &dd->active_time); 
	spin_unlock_irqrestore(&dd->eep_st_lock, flags);

	qib_chk_6120_errormask(dd);
done:
	mod_timer(&dd->stats_timer, jiffies + HZ * ACTIVITY_TIMER);
}

static int qib_6120_nointr_fallback(struct qib_devdata *dd)
{
	return 0;
}

static void qib_6120_xgxs_reset(struct qib_pportdata *ppd)
{
	u64 val, prev_val;
	struct qib_devdata *dd = ppd->dd;

	prev_val = qib_read_kreg64(dd, kr_xgxs_cfg);
	val = prev_val | QLOGIC_IB_XGXS_RESET;
	prev_val &= ~QLOGIC_IB_XGXS_RESET; 
	qib_write_kreg(dd, kr_control,
		       dd->control & ~QLOGIC_IB_C_LINKENABLE);
	qib_write_kreg(dd, kr_xgxs_cfg, val);
	qib_read_kreg32(dd, kr_scratch);
	qib_write_kreg(dd, kr_xgxs_cfg, prev_val);
	qib_write_kreg(dd, kr_control, dd->control);
}

static int qib_6120_get_ib_cfg(struct qib_pportdata *ppd, int which)
{
	int ret;

	switch (which) {
	case QIB_IB_CFG_LWID:
		ret = ppd->link_width_active;
		break;

	case QIB_IB_CFG_SPD:
		ret = ppd->link_speed_active;
		break;

	case QIB_IB_CFG_LWID_ENB:
		ret = ppd->link_width_enabled;
		break;

	case QIB_IB_CFG_SPD_ENB:
		ret = ppd->link_speed_enabled;
		break;

	case QIB_IB_CFG_OP_VLS:
		ret = ppd->vls_operational;
		break;

	case QIB_IB_CFG_VL_HIGH_CAP:
		ret = 0;
		break;

	case QIB_IB_CFG_VL_LOW_CAP:
		ret = 0;
		break;

	case QIB_IB_CFG_OVERRUN_THRESH: 
		ret = SYM_FIELD(ppd->dd->cspec->ibcctrl, IBCCtrl,
				OverrunThreshold);
		break;

	case QIB_IB_CFG_PHYERR_THRESH: 
		ret = SYM_FIELD(ppd->dd->cspec->ibcctrl, IBCCtrl,
				PhyerrThreshold);
		break;

	case QIB_IB_CFG_LINKDEFAULT: 
		
		ret = (ppd->dd->cspec->ibcctrl &
		       SYM_MASK(IBCCtrl, LinkDownDefaultState)) ?
			IB_LINKINITCMD_SLEEP : IB_LINKINITCMD_POLL;
		break;

	case QIB_IB_CFG_HRTBT: 
		ret = 0; 
		break;

	case QIB_IB_CFG_PMA_TICKS:
		ret = 250; 
		break;

	default:
		ret =  -EINVAL;
		break;
	}
	return ret;
}

static int qib_6120_set_ib_cfg(struct qib_pportdata *ppd, int which, u32 val)
{
	struct qib_devdata *dd = ppd->dd;
	int ret = 0;
	u64 val64;
	u16 lcmd, licmd;

	switch (which) {
	case QIB_IB_CFG_LWID_ENB:
		ppd->link_width_enabled = val;
		break;

	case QIB_IB_CFG_SPD_ENB:
		ppd->link_speed_enabled = val;
		break;

	case QIB_IB_CFG_OVERRUN_THRESH: 
		val64 = SYM_FIELD(dd->cspec->ibcctrl, IBCCtrl,
				  OverrunThreshold);
		if (val64 != val) {
			dd->cspec->ibcctrl &=
				~SYM_MASK(IBCCtrl, OverrunThreshold);
			dd->cspec->ibcctrl |= (u64) val <<
				SYM_LSB(IBCCtrl, OverrunThreshold);
			qib_write_kreg(dd, kr_ibcctrl, dd->cspec->ibcctrl);
			qib_write_kreg(dd, kr_scratch, 0);
		}
		break;

	case QIB_IB_CFG_PHYERR_THRESH: 
		val64 = SYM_FIELD(dd->cspec->ibcctrl, IBCCtrl,
				  PhyerrThreshold);
		if (val64 != val) {
			dd->cspec->ibcctrl &=
				~SYM_MASK(IBCCtrl, PhyerrThreshold);
			dd->cspec->ibcctrl |= (u64) val <<
				SYM_LSB(IBCCtrl, PhyerrThreshold);
			qib_write_kreg(dd, kr_ibcctrl, dd->cspec->ibcctrl);
			qib_write_kreg(dd, kr_scratch, 0);
		}
		break;

	case QIB_IB_CFG_PKEYS: 
		val64 = (u64) ppd->pkeys[0] | ((u64) ppd->pkeys[1] << 16) |
			((u64) ppd->pkeys[2] << 32) |
			((u64) ppd->pkeys[3] << 48);
		qib_write_kreg(dd, kr_partitionkey, val64);
		break;

	case QIB_IB_CFG_LINKDEFAULT: 
		
		if (val == IB_LINKINITCMD_POLL)
			dd->cspec->ibcctrl &=
				~SYM_MASK(IBCCtrl, LinkDownDefaultState);
		else 
			dd->cspec->ibcctrl |=
				SYM_MASK(IBCCtrl, LinkDownDefaultState);
		qib_write_kreg(dd, kr_ibcctrl, dd->cspec->ibcctrl);
		qib_write_kreg(dd, kr_scratch, 0);
		break;

	case QIB_IB_CFG_MTU: 
		val = (ppd->ibmaxlen >> 2) + 1;
		dd->cspec->ibcctrl &= ~SYM_MASK(IBCCtrl, MaxPktLen);
		dd->cspec->ibcctrl |= (u64)val <<
			SYM_LSB(IBCCtrl, MaxPktLen);
		qib_write_kreg(dd, kr_ibcctrl, dd->cspec->ibcctrl);
		qib_write_kreg(dd, kr_scratch, 0);
		break;

	case QIB_IB_CFG_LSTATE: 
		switch (val & 0xffff0000) {
		case IB_LINKCMD_DOWN:
			lcmd = QLOGIC_IB_IBCC_LINKCMD_DOWN;
			if (!dd->cspec->ibdeltainprog) {
				dd->cspec->ibdeltainprog = 1;
				dd->cspec->ibsymsnap =
					read_6120_creg32(dd, cr_ibsymbolerr);
				dd->cspec->iblnkerrsnap =
					read_6120_creg32(dd, cr_iblinkerrrecov);
			}
			break;

		case IB_LINKCMD_ARMED:
			lcmd = QLOGIC_IB_IBCC_LINKCMD_ARMED;
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
			break;

		default:
			ret = -EINVAL;
			qib_dev_err(dd, "bad linkinitcmd req 0x%x\n",
				    val & 0xffff);
			goto bail;
		}
		qib_set_ib_6120_lstate(ppd, lcmd, licmd);
		goto bail;

	case QIB_IB_CFG_HRTBT:
		ret = -EINVAL;
		break;

	default:
		ret = -EINVAL;
	}
bail:
	return ret;
}

static int qib_6120_set_loopback(struct qib_pportdata *ppd, const char *what)
{
	int ret = 0;
	if (!strncmp(what, "ibc", 3)) {
		ppd->dd->cspec->ibcctrl |= SYM_MASK(IBCCtrl, Loopback);
		qib_devinfo(ppd->dd->pcidev, "Enabling IB%u:%u IBC loopback\n",
			 ppd->dd->unit, ppd->port);
	} else if (!strncmp(what, "off", 3)) {
		ppd->dd->cspec->ibcctrl &= ~SYM_MASK(IBCCtrl, Loopback);
		qib_devinfo(ppd->dd->pcidev, "Disabling IB%u:%u IBC loopback "
			    "(normal)\n", ppd->dd->unit, ppd->port);
	} else
		ret = -EINVAL;
	if (!ret) {
		qib_write_kreg(ppd->dd, kr_ibcctrl, ppd->dd->cspec->ibcctrl);
		qib_write_kreg(ppd->dd, kr_scratch, 0);
	}
	return ret;
}

static void pma_6120_timer(unsigned long data)
{
	struct qib_pportdata *ppd = (struct qib_pportdata *)data;
	struct qib_chip_specific *cs = ppd->dd->cspec;
	struct qib_ibport *ibp = &ppd->ibport_data;
	unsigned long flags;

	spin_lock_irqsave(&ibp->lock, flags);
	if (cs->pma_sample_status == IB_PMA_SAMPLE_STATUS_STARTED) {
		cs->pma_sample_status = IB_PMA_SAMPLE_STATUS_RUNNING;
		qib_snapshot_counters(ppd, &cs->sword, &cs->rword,
				      &cs->spkts, &cs->rpkts, &cs->xmit_wait);
		mod_timer(&cs->pma_timer,
			  jiffies + usecs_to_jiffies(ibp->pma_sample_interval));
	} else if (cs->pma_sample_status == IB_PMA_SAMPLE_STATUS_RUNNING) {
		u64 ta, tb, tc, td, te;

		cs->pma_sample_status = IB_PMA_SAMPLE_STATUS_DONE;
		qib_snapshot_counters(ppd, &ta, &tb, &tc, &td, &te);

		cs->sword = ta - cs->sword;
		cs->rword = tb - cs->rword;
		cs->spkts = tc - cs->spkts;
		cs->rpkts = td - cs->rpkts;
		cs->xmit_wait = te - cs->xmit_wait;
	}
	spin_unlock_irqrestore(&ibp->lock, flags);
}

static void qib_set_cntr_6120_sample(struct qib_pportdata *ppd, u32 intv,
				     u32 start)
{
	struct qib_chip_specific *cs = ppd->dd->cspec;

	if (start && intv) {
		cs->pma_sample_status = IB_PMA_SAMPLE_STATUS_STARTED;
		mod_timer(&cs->pma_timer, jiffies + usecs_to_jiffies(start));
	} else if (intv) {
		cs->pma_sample_status = IB_PMA_SAMPLE_STATUS_RUNNING;
		qib_snapshot_counters(ppd, &cs->sword, &cs->rword,
				      &cs->spkts, &cs->rpkts, &cs->xmit_wait);
		mod_timer(&cs->pma_timer, jiffies + usecs_to_jiffies(intv));
	} else {
		cs->pma_sample_status = IB_PMA_SAMPLE_STATUS_DONE;
		cs->sword = 0;
		cs->rword = 0;
		cs->spkts = 0;
		cs->rpkts = 0;
		cs->xmit_wait = 0;
	}
}

static u32 qib_6120_iblink_state(u64 ibcs)
{
	u32 state = (u32)SYM_FIELD(ibcs, IBCStatus, LinkState);

	switch (state) {
	case IB_6120_L_STATE_INIT:
		state = IB_PORT_INIT;
		break;
	case IB_6120_L_STATE_ARM:
		state = IB_PORT_ARMED;
		break;
	case IB_6120_L_STATE_ACTIVE:
		
	case IB_6120_L_STATE_ACT_DEFER:
		state = IB_PORT_ACTIVE;
		break;
	default: 
	case IB_6120_L_STATE_DOWN:
		state = IB_PORT_DOWN;
		break;
	}
	return state;
}

static u8 qib_6120_phys_portstate(u64 ibcs)
{
	u8 state = (u8)SYM_FIELD(ibcs, IBCStatus, LinkTrainingState);
	return qib_6120_physportstate[state];
}

static int qib_6120_ib_updown(struct qib_pportdata *ppd, int ibup, u64 ibcs)
{
	unsigned long flags;

	spin_lock_irqsave(&ppd->lflags_lock, flags);
	ppd->lflags &= ~QIBL_IB_FORCE_NOTIFY;
	spin_unlock_irqrestore(&ppd->lflags_lock, flags);

	if (ibup) {
		if (ppd->dd->cspec->ibdeltainprog) {
			ppd->dd->cspec->ibdeltainprog = 0;
			ppd->dd->cspec->ibsymdelta +=
				read_6120_creg32(ppd->dd, cr_ibsymbolerr) -
					ppd->dd->cspec->ibsymsnap;
			ppd->dd->cspec->iblnkerrdelta +=
				read_6120_creg32(ppd->dd, cr_iblinkerrrecov) -
					ppd->dd->cspec->iblnkerrsnap;
		}
		qib_hol_init(ppd);
	} else {
		ppd->dd->cspec->lli_counter = 0;
		if (!ppd->dd->cspec->ibdeltainprog) {
			ppd->dd->cspec->ibdeltainprog = 1;
			ppd->dd->cspec->ibsymsnap =
				read_6120_creg32(ppd->dd, cr_ibsymbolerr);
			ppd->dd->cspec->iblnkerrsnap =
				read_6120_creg32(ppd->dd, cr_iblinkerrrecov);
		}
		qib_hol_down(ppd);
	}

	qib_6120_setup_setextled(ppd, ibup);

	return 0;
}

static int gpio_6120_mod(struct qib_devdata *dd, u32 out, u32 dir, u32 mask)
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

static void get_6120_chip_params(struct qib_devdata *dd)
{
	u64 val;
	u32 piobufs;
	int mtu;

	dd->uregbase = qib_read_kreg32(dd, kr_userregbase);

	dd->rcvtidcnt = qib_read_kreg32(dd, kr_rcvtidcnt);
	dd->rcvtidbase = qib_read_kreg32(dd, kr_rcvtidbase);
	dd->rcvegrbase = qib_read_kreg32(dd, kr_rcvegrbase);
	dd->palign = qib_read_kreg32(dd, kr_palign);
	dd->piobufbase = qib_read_kreg64(dd, kr_sendpiobufbase);
	dd->pio2k_bufbase = dd->piobufbase & 0xffffffff;

	dd->rcvhdrcnt = qib_read_kreg32(dd, kr_rcvegrcnt);

	val = qib_read_kreg64(dd, kr_sendpiosize);
	dd->piosize2k = val & ~0U;
	dd->piosize4k = val >> 32;

	mtu = ib_mtu_enum_to_int(qib_ibmtu);
	if (mtu == -1)
		mtu = QIB_DEFAULT_MTU;
	dd->pport->ibmtu = (u32)mtu;

	val = qib_read_kreg64(dd, kr_sendpiobufcnt);
	dd->piobcnt2k = val & ~0U;
	dd->piobcnt4k = val >> 32;
	
	dd->pio2kbase = (u32 __iomem *)
		(((char __iomem *)dd->kregbase) + dd->pio2k_bufbase);
	if (dd->piobcnt4k) {
		dd->pio4kbase = (u32 __iomem *)
			(((char __iomem *) dd->kregbase) +
			 (dd->piobufbase >> 32));
		dd->align4k = ALIGN(dd->piosize4k, dd->palign);
	}

	piobufs = dd->piobcnt4k + dd->piobcnt2k;

	dd->pioavregs = ALIGN(piobufs, sizeof(u64) * BITS_PER_BYTE / 2) /
		(sizeof(u64) * BITS_PER_BYTE / 2);
}

static void set_6120_baseaddrs(struct qib_devdata *dd)
{
	u32 cregbase;
	cregbase = qib_read_kreg32(dd, kr_counterregbase);
	dd->cspec->cregbase = (u64 __iomem *)
		((char __iomem *) dd->kregbase + cregbase);

	dd->egrtidbase = (u64 __iomem *)
		((char __iomem *) dd->kregbase + dd->rcvegrbase);
}

static int qib_late_6120_initreg(struct qib_devdata *dd)
{
	int ret = 0;
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
	return ret;
}

static int init_6120_variables(struct qib_devdata *dd)
{
	int ret = 0;
	struct qib_pportdata *ppd;
	u32 sbufs;

	ppd = (struct qib_pportdata *)(dd + 1);
	dd->pport = ppd;
	dd->num_pports = 1;

	dd->cspec = (struct qib_chip_specific *)(ppd + dd->num_pports);
	ppd->cpspec = NULL; 

	spin_lock_init(&dd->cspec->kernel_tid_lock);
	spin_lock_init(&dd->cspec->user_tid_lock);
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

	dd->majrev = (u8) SYM_FIELD(dd->revision, Revision_R,
				    ChipRevMajor);
	dd->minrev = (u8) SYM_FIELD(dd->revision, Revision_R,
				    ChipRevMinor);

	get_6120_chip_params(dd);
	pe_boardname(dd); 

	dd->gpio_sda_num = _QIB_GPIO_SDA_NUM;
	dd->gpio_scl_num = _QIB_GPIO_SCL_NUM;
	dd->twsi_eeprom_dev = QIB_TWSI_NO_DEV;

	if (qib_unordered_wc())
		dd->flags |= QIB_PIO_FLUSH_WC;

	dd->eep_st_masks[0].hwerrs_to_log = HWE_MASK(TXEMemParityErr);

	
	if (qib_unordered_wc())
		dd->eep_st_masks[0].hwerrs_to_log &= ~TXE_PIO_PARITY;

	dd->eep_st_masks[1].hwerrs_to_log = HWE_MASK(RXEMemParityErr);

	dd->eep_st_masks[2].errs_to_log = ERR_MASK(ResetNegated);

	qib_init_pportdata(ppd, dd, 0, 1);
	ppd->link_width_supported = IB_WIDTH_1X | IB_WIDTH_4X;
	ppd->link_speed_supported = QIB_IB_SDR;
	ppd->link_width_enabled = IB_WIDTH_4X;
	ppd->link_speed_enabled = ppd->link_speed_supported;
	
	ppd->link_width_active = ppd->link_width_enabled;
	ppd->link_speed_active = ppd->link_speed_enabled;
	ppd->vls_supported = IB_VL_VL0;
	ppd->vls_operational = ppd->vls_supported;

	dd->rcvhdrentsize = QIB_RCVHDR_ENTSIZE;
	dd->rcvhdrsize = QIB_DFLT_RCVHDRSIZE;
	dd->rhf_offset = 0;

	
	ret = ib_mtu_enum_to_int(qib_ibmtu);
	dd->rcvegrbufsize = ret != -1 ? max(ret, 2048) : QIB_DEFAULT_MTU;
	BUG_ON(!is_power_of_2(dd->rcvegrbufsize));
	dd->rcvegrbufsize_shift = ilog2(dd->rcvegrbufsize);

	qib_6120_tidtemplate(dd);

	dd->rhdrhead_intr_off = 1ULL << 32;

	
	init_timer(&dd->stats_timer);
	dd->stats_timer.function = qib_get_6120_faststats;
	dd->stats_timer.data = (unsigned long) dd;

	init_timer(&dd->cspec->pma_timer);
	dd->cspec->pma_timer.function = pma_6120_timer;
	dd->cspec->pma_timer.data = (unsigned long) ppd;

	dd->ureg_align = qib_read_kreg32(dd, kr_palign);

	dd->piosize2kmax_dwords = dd->piosize2k >> 2;
	qib_6120_config_ctxts(dd);
	qib_set_ctxtcnt(dd);

	if (qib_wc_pat) {
		ret = init_chip_wc_pat(dd, 0);
		if (ret)
			goto bail;
	}
	set_6120_baseaddrs(dd); 

	ret = 0;
	if (qib_mini_init)
		goto bail;

	qib_num_cfg_vls = 1; 

	ret = qib_create_ctxts(dd);
	init_6120_cntrnames(dd);

	
	sbufs = dd->piobcnt4k ?  dd->piobcnt4k : 16;

	dd->lastctxt_piobuf = dd->piobcnt2k + dd->piobcnt4k - sbufs;
	dd->pbufsctxt = dd->lastctxt_piobuf /
		(dd->cfgctxts - dd->first_user_ctxt);

	if (ret)
		goto bail;
bail:
	return ret;
}

static u32 __iomem *get_6120_link_buf(struct qib_pportdata *ppd, u32 *bnum)
{
	u32 __iomem *buf;
	u32 lbuf = ppd->dd->piobcnt2k + ppd->dd->piobcnt4k - 1;

	sendctrl_6120_mod(ppd->dd->pport, QIB_SENDCTRL_AVAIL_BLIP);
	qib_read_kreg64(ppd->dd, kr_scratch); 
	buf = qib_getsendbuf_range(ppd->dd, bnum, lbuf, lbuf);
	if (buf)
		goto done;

	sendctrl_6120_mod(ppd, QIB_SENDCTRL_DISARM_ALL | QIB_SENDCTRL_FLUSH |
			  QIB_SENDCTRL_AVAIL_BLIP);
	ppd->dd->upd_pio_shadow  = 1; 
	qib_read_kreg64(ppd->dd, kr_scratch); 
	buf = qib_getsendbuf_range(ppd->dd, bnum, lbuf, lbuf);
done:
	return buf;
}

static u32 __iomem *qib_6120_getsendbuf(struct qib_pportdata *ppd, u64 pbc,
					u32 *pbufnum)
{
	u32 first, last, plen = pbc & QIB_PBC_LENGTH_MASK;
	struct qib_devdata *dd = ppd->dd;
	u32 __iomem *buf;

	if (((pbc >> 32) & PBC_6120_VL15_SEND_CTRL) &&
		!(ppd->lflags & (QIBL_IB_AUTONEG_INPROG | QIBL_LINKACTIVE)))
		buf = get_6120_link_buf(ppd, pbufnum);
	else {

		if ((plen + 1) > dd->piosize2kmax_dwords)
			first = dd->piobcnt2k;
		else
			first = 0;
		
		last = dd->piobcnt2k + dd->piobcnt4k - 1;
		buf = qib_getsendbuf_range(dd, pbufnum, first, last);
	}
	return buf;
}

static int init_sdma_6120_regs(struct qib_pportdata *ppd)
{
	return -ENODEV;
}

static u16 qib_sdma_6120_gethead(struct qib_pportdata *ppd)
{
	return 0;
}

static int qib_sdma_6120_busy(struct qib_pportdata *ppd)
{
	return 0;
}

static void qib_sdma_update_6120_tail(struct qib_pportdata *ppd, u16 tail)
{
}

static void qib_6120_sdma_sendctrl(struct qib_pportdata *ppd, unsigned op)
{
}

static void qib_sdma_set_6120_desc_cnt(struct qib_pportdata *ppd, unsigned cnt)
{
}

static u32 qib_6120_setpbc_control(struct qib_pportdata *ppd, u32 plen,
				   u8 srate, u8 vl)
{
	return vl == 15 ? PBC_6120_VL15_SEND_CTRL : 0;
}

static void qib_6120_initvl15_bufs(struct qib_devdata *dd)
{
}

static void qib_6120_init_ctxt(struct qib_ctxtdata *rcd)
{
	rcd->rcvegrcnt = rcd->dd->rcvhdrcnt;
	rcd->rcvegr_tid_base = rcd->ctxt * rcd->rcvegrcnt;
}

static void qib_6120_txchk_change(struct qib_devdata *dd, u32 start,
	u32 len, u32 avail, struct qib_ctxtdata *rcd)
{
}

static void writescratch(struct qib_devdata *dd, u32 val)
{
	(void) qib_write_kreg(dd, kr_scratch, val);
}

static int qib_6120_tempsense_rd(struct qib_devdata *dd, int regnum)
{
	return -ENXIO;
}

static int qib_6120_eeprom_wen(struct qib_devdata *dd, int wen)
{
	return 1;
}

struct qib_devdata *qib_init_iba6120_funcs(struct pci_dev *pdev,
					   const struct pci_device_id *ent)
{
	struct qib_devdata *dd;
	int ret;

	dd = qib_alloc_devdata(pdev, sizeof(struct qib_pportdata) +
			       sizeof(struct qib_chip_specific));
	if (IS_ERR(dd))
		goto bail;

	dd->f_bringup_serdes    = qib_6120_bringup_serdes;
	dd->f_cleanup           = qib_6120_setup_cleanup;
	dd->f_clear_tids        = qib_6120_clear_tids;
	dd->f_free_irq          = qib_6120_free_irq;
	dd->f_get_base_info     = qib_6120_get_base_info;
	dd->f_get_msgheader     = qib_6120_get_msgheader;
	dd->f_getsendbuf        = qib_6120_getsendbuf;
	dd->f_gpio_mod          = gpio_6120_mod;
	dd->f_eeprom_wen	= qib_6120_eeprom_wen;
	dd->f_hdrqempty         = qib_6120_hdrqempty;
	dd->f_ib_updown         = qib_6120_ib_updown;
	dd->f_init_ctxt         = qib_6120_init_ctxt;
	dd->f_initvl15_bufs     = qib_6120_initvl15_bufs;
	dd->f_intr_fallback     = qib_6120_nointr_fallback;
	dd->f_late_initreg      = qib_late_6120_initreg;
	dd->f_setpbc_control    = qib_6120_setpbc_control;
	dd->f_portcntr          = qib_portcntr_6120;
	dd->f_put_tid           = (dd->minrev >= 2) ?
				      qib_6120_put_tid_2 :
				      qib_6120_put_tid;
	dd->f_quiet_serdes      = qib_6120_quiet_serdes;
	dd->f_rcvctrl           = rcvctrl_6120_mod;
	dd->f_read_cntrs        = qib_read_6120cntrs;
	dd->f_read_portcntrs    = qib_read_6120portcntrs;
	dd->f_reset             = qib_6120_setup_reset;
	dd->f_init_sdma_regs    = init_sdma_6120_regs;
	dd->f_sdma_busy         = qib_sdma_6120_busy;
	dd->f_sdma_gethead      = qib_sdma_6120_gethead;
	dd->f_sdma_sendctrl     = qib_6120_sdma_sendctrl;
	dd->f_sdma_set_desc_cnt = qib_sdma_set_6120_desc_cnt;
	dd->f_sdma_update_tail  = qib_sdma_update_6120_tail;
	dd->f_sendctrl          = sendctrl_6120_mod;
	dd->f_set_armlaunch     = qib_set_6120_armlaunch;
	dd->f_set_cntr_sample   = qib_set_cntr_6120_sample;
	dd->f_iblink_state      = qib_6120_iblink_state;
	dd->f_ibphys_portstate  = qib_6120_phys_portstate;
	dd->f_get_ib_cfg        = qib_6120_get_ib_cfg;
	dd->f_set_ib_cfg        = qib_6120_set_ib_cfg;
	dd->f_set_ib_loopback   = qib_6120_set_loopback;
	dd->f_set_intr_state    = qib_6120_set_intr_state;
	dd->f_setextled         = qib_6120_setup_setextled;
	dd->f_txchk_change      = qib_6120_txchk_change;
	dd->f_update_usrhead    = qib_update_6120_usrhead;
	dd->f_wantpiobuf_intr   = qib_wantpiobuf_6120_intr;
	dd->f_xgxs_reset        = qib_6120_xgxs_reset;
	dd->f_writescratch      = writescratch;
	dd->f_tempsense_rd	= qib_6120_tempsense_rd;
	ret = qib_pcie_ddinit(dd, pdev, ent);
	if (ret < 0)
		goto bail_free;

	
	ret = init_6120_variables(dd);
	if (ret)
		goto bail_cleanup;

	if (qib_mini_init)
		goto bail;

	if (qib_pcie_params(dd, 8, NULL, NULL))
		qib_dev_err(dd, "Failed to setup PCIe or interrupts; "
			    "continuing anyway\n");
	dd->cspec->irq = pdev->irq; 

	
	qib_write_kreg(dd, kr_hwdiagctrl, 0);

	if (qib_read_kreg64(dd, kr_hwerrstatus) &
	    QLOGIC_IB_HWE_SERDESPLLFAILED)
		qib_write_kreg(dd, kr_hwerrclear,
			       QLOGIC_IB_HWE_SERDESPLLFAILED);

	
	qib_setup_6120_interrupt(dd);
	
	qib_6120_init_hwerrors(dd);

	goto bail;

bail_cleanup:
	qib_pcie_ddcleanup(dd);
bail_free:
	qib_free_devdata(dd);
	dd = ERR_PTR(ret);
bail:
	return dd;
}
