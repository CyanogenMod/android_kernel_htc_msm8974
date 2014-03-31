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
#include <linux/module.h>
#include <linux/io.h>
#include <rdma/ib_verbs.h>

#include "qib.h"
#include "qib_7220.h"

static void qib_setup_7220_setextled(struct qib_pportdata *, u32);
static void qib_7220_handle_hwerrors(struct qib_devdata *, char *, size_t);
static void sendctrl_7220_mod(struct qib_pportdata *ppd, u32 op);
static u32 qib_7220_iblink_state(u64);
static u8 qib_7220_phys_portstate(u64);
static void qib_sdma_update_7220_tail(struct qib_pportdata *, u16);
static void qib_set_ib_7220_lstate(struct qib_pportdata *, u16, u16);


#define KREG_IDX(regname) (QIB_7220_##regname##_OFFS / sizeof(u64))

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
#define kr_hrtbt_guid KREG_IDX(HRTBT_GUID)
#define kr_hwdiagctrl KREG_IDX(HwDiagCtrl)
#define kr_hwerrclear KREG_IDX(HwErrClear)
#define kr_hwerrmask KREG_IDX(HwErrMask)
#define kr_hwerrstatus KREG_IDX(HwErrStatus)
#define kr_ibcctrl KREG_IDX(IBCCtrl)
#define kr_ibcddrctrl KREG_IDX(IBCDDRCtrl)
#define kr_ibcddrstatus KREG_IDX(IBCDDRStatus)
#define kr_ibcstatus KREG_IDX(IBCStatus)
#define kr_ibserdesctrl KREG_IDX(IBSerDesCtrl)
#define kr_intclear KREG_IDX(IntClear)
#define kr_intmask KREG_IDX(IntMask)
#define kr_intstatus KREG_IDX(IntStatus)
#define kr_ncmodectrl KREG_IDX(IBNCModeCtrl)
#define kr_palign KREG_IDX(PageAlign)
#define kr_partitionkey KREG_IDX(RcvPartitionKey)
#define kr_portcnt KREG_IDX(PortCnt)
#define kr_rcvbthqp KREG_IDX(RcvBTHQP)
#define kr_rcvctrl KREG_IDX(RcvCtrl)
#define kr_rcvegrbase KREG_IDX(RcvEgrBase)
#define kr_rcvegrcnt KREG_IDX(RcvEgrCnt)
#define kr_rcvhdrcnt KREG_IDX(RcvHdrCnt)
#define kr_rcvhdrentsize KREG_IDX(RcvHdrEntSize)
#define kr_rcvhdrsize KREG_IDX(RcvHdrSize)
#define kr_rcvpktledcnt KREG_IDX(RcvPktLEDCnt)
#define kr_rcvtidbase KREG_IDX(RcvTIDBase)
#define kr_rcvtidcnt KREG_IDX(RcvTIDCnt)
#define kr_revision KREG_IDX(Revision)
#define kr_scratch KREG_IDX(Scratch)
#define kr_sendbuffererror KREG_IDX(SendBufErr0)
#define kr_sendctrl KREG_IDX(SendCtrl)
#define kr_senddmabase KREG_IDX(SendDmaBase)
#define kr_senddmabufmask0 KREG_IDX(SendDmaBufMask0)
#define kr_senddmabufmask1 (KREG_IDX(SendDmaBufMask0) + 1)
#define kr_senddmabufmask2 (KREG_IDX(SendDmaBufMask0) + 2)
#define kr_senddmahead KREG_IDX(SendDmaHead)
#define kr_senddmaheadaddr KREG_IDX(SendDmaHeadAddr)
#define kr_senddmalengen KREG_IDX(SendDmaLenGen)
#define kr_senddmastatus KREG_IDX(SendDmaStatus)
#define kr_senddmatail KREG_IDX(SendDmaTail)
#define kr_sendpioavailaddr KREG_IDX(SendBufAvailAddr)
#define kr_sendpiobufbase KREG_IDX(SendBufBase)
#define kr_sendpiobufcnt KREG_IDX(SendBufCnt)
#define kr_sendpiosize KREG_IDX(SendBufSize)
#define kr_sendregbase KREG_IDX(SendRegBase)
#define kr_userregbase KREG_IDX(UserRegBase)
#define kr_xgxs_cfg KREG_IDX(XGXSCfg)

/* These must only be written via qib_write_kreg_ctxt() */
#define kr_rcvhdraddr KREG_IDX(RcvHdrAddr0)
#define kr_rcvhdrtailaddr KREG_IDX(RcvHdrTailAddr0)


#define CREG_IDX(regname) ((QIB_7220_##regname##_OFFS - \
			QIB_7220_LBIntCnt_OFFS) / sizeof(u64))

#define cr_badformat CREG_IDX(RxVersionErrCnt)
#define cr_erricrc CREG_IDX(RxICRCErrCnt)
#define cr_errlink CREG_IDX(RxLinkMalformCnt)
#define cr_errlpcrc CREG_IDX(RxLPCRCErrCnt)
#define cr_errpkey CREG_IDX(RxPKeyMismatchCnt)
#define cr_rcvflowctrl_err CREG_IDX(RxFlowCtrlViolCnt)
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
#define cr_vl15droppedpkt CREG_IDX(RxVL15DroppedPktCnt)
#define cr_rxotherlocalphyerr CREG_IDX(RxOtherLocalPhyErrCnt)
#define cr_excessbufferovfl CREG_IDX(ExcessBufferOvflCnt)
#define cr_locallinkintegrityerr CREG_IDX(LocalLinkIntegrityErrCnt)
#define cr_rxvlerr CREG_IDX(RxVlErrCnt)
#define cr_rxdlidfltr CREG_IDX(RxDlidFltrCnt)
#define cr_psstat CREG_IDX(PSStat)
#define cr_psstart CREG_IDX(PSStart)
#define cr_psinterval CREG_IDX(PSInterval)
#define cr_psrcvdatacount CREG_IDX(PSRcvDataCount)
#define cr_psrcvpktscount CREG_IDX(PSRcvPktsCount)
#define cr_psxmitdatacount CREG_IDX(PSXmitDataCount)
#define cr_psxmitpktscount CREG_IDX(PSXmitPktsCount)
#define cr_psxmitwaitcount CREG_IDX(PSXmitWaitCount)
#define cr_txsdmadesc CREG_IDX(TxSDmaDescCnt)
#define cr_pcieretrydiag CREG_IDX(PcieRetryBufDiagQwordCnt)

#define SYM_RMASK(regname, fldname) ((u64)              \
	QIB_7220_##regname##_##fldname##_RMASK)
#define SYM_MASK(regname, fldname) ((u64)               \
	QIB_7220_##regname##_##fldname##_RMASK <<       \
	 QIB_7220_##regname##_##fldname##_LSB)
#define SYM_LSB(regname, fldname) (QIB_7220_##regname##_##fldname##_LSB)
#define SYM_FIELD(value, regname, fldname) ((u64) \
	(((value) >> SYM_LSB(regname, fldname)) & \
	 SYM_RMASK(regname, fldname)))
#define ERR_MASK(fldname) SYM_MASK(ErrMask, fldname##Mask)
#define HWE_MASK(fldname) SYM_MASK(HwErrMask, fldname##Mask)

#define QLOGIC_IB_IBCC_LINKINITCMD_DISABLE 1
#define QLOGIC_IB_IBCC_LINKINITCMD_POLL 2
#define QLOGIC_IB_IBCC_LINKINITCMD_SLEEP 3
#define QLOGIC_IB_IBCC_LINKINITCMD_SHIFT 16

#define QLOGIC_IB_IBCC_LINKCMD_DOWN 1           
#define QLOGIC_IB_IBCC_LINKCMD_ARMED 2          
#define QLOGIC_IB_IBCC_LINKCMD_ACTIVE 3 

#define BLOB_7220_IBCHG 0x81


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

static inline void qib_write_kreg_ctxt(const struct qib_devdata *dd,
				       const u16 regno, unsigned ctxt,
				       u64 value)
{
	qib_write_kreg(dd, regno + ctxt, value);
}

static inline void write_7220_creg(const struct qib_devdata *dd,
				   u16 regno, u64 value)
{
	if (dd->cspec->cregbase && (dd->flags & QIB_PRESENT))
		writeq(value, &dd->cspec->cregbase[regno]);
}

static inline u64 read_7220_creg(const struct qib_devdata *dd, u16 regno)
{
	if (!dd->cspec->cregbase || !(dd->flags & QIB_PRESENT))
		return 0;
	return readq(&dd->cspec->cregbase[regno]);
}

static inline u32 read_7220_creg32(const struct qib_devdata *dd, u16 regno)
{
	if (!dd->cspec->cregbase || !(dd->flags & QIB_PRESENT))
		return 0;
	return readl(&dd->cspec->cregbase[regno]);
}

#define QLOGIC_IB_R_EMULATORREV_MASK ((1ULL << 22) - 1)
#define QLOGIC_IB_R_EMULATORREV_SHIFT 40

#define QLOGIC_IB_C_RESET (1U << 7)

#define QLOGIC_IB_I_RCVURG_MASK ((1ULL << 17) - 1)
#define QLOGIC_IB_I_RCVURG_SHIFT 32
#define QLOGIC_IB_I_RCVAVAIL_MASK ((1ULL << 17) - 1)
#define QLOGIC_IB_I_RCVAVAIL_SHIFT 0
#define QLOGIC_IB_I_SERDESTRIMDONE (1ULL << 27)

#define QLOGIC_IB_C_FREEZEMODE 0x00000002
#define QLOGIC_IB_C_LINKENABLE 0x00000004

#define QLOGIC_IB_I_SDMAINT             0x8000000000000000ULL
#define QLOGIC_IB_I_SDMADISABLED        0x4000000000000000ULL
#define QLOGIC_IB_I_ERROR               0x0000000080000000ULL
#define QLOGIC_IB_I_SPIOSENT            0x0000000040000000ULL
#define QLOGIC_IB_I_SPIOBUFAVAIL        0x0000000020000000ULL
#define QLOGIC_IB_I_GPIO                0x0000000010000000ULL

#define QLOGIC_IB_I_BITSEXTANT \
		(QLOGIC_IB_I_SDMAINT | QLOGIC_IB_I_SDMADISABLED | \
		(QLOGIC_IB_I_RCVURG_MASK << QLOGIC_IB_I_RCVURG_SHIFT) | \
		(QLOGIC_IB_I_RCVAVAIL_MASK << \
		 QLOGIC_IB_I_RCVAVAIL_SHIFT) | \
		QLOGIC_IB_I_ERROR | QLOGIC_IB_I_SPIOSENT | \
		QLOGIC_IB_I_SPIOBUFAVAIL | QLOGIC_IB_I_GPIO | \
		QLOGIC_IB_I_SERDESTRIMDONE)

#define IB_HWE_BITSEXTANT \
	       (HWE_MASK(RXEMemParityErr) | \
		HWE_MASK(TXEMemParityErr) | \
		(QLOGIC_IB_HWE_PCIEMEMPARITYERR_MASK <<	 \
		 QLOGIC_IB_HWE_PCIEMEMPARITYERR_SHIFT) | \
		QLOGIC_IB_HWE_PCIE1PLLFAILED | \
		QLOGIC_IB_HWE_PCIE0PLLFAILED | \
		QLOGIC_IB_HWE_PCIEPOISONEDTLP | \
		QLOGIC_IB_HWE_PCIECPLTIMEOUT | \
		QLOGIC_IB_HWE_PCIEBUSPARITYXTLH | \
		QLOGIC_IB_HWE_PCIEBUSPARITYXADM | \
		QLOGIC_IB_HWE_PCIEBUSPARITYRADM | \
		HWE_MASK(PowerOnBISTFailed) |	  \
		QLOGIC_IB_HWE_COREPLL_FBSLIP | \
		QLOGIC_IB_HWE_COREPLL_RFSLIP | \
		QLOGIC_IB_HWE_SERDESPLLFAILED | \
		HWE_MASK(IBCBusToSPCParityErr) | \
		HWE_MASK(IBCBusFromSPCParityErr) | \
		QLOGIC_IB_HWE_PCIECPLDATAQUEUEERR | \
		QLOGIC_IB_HWE_PCIECPLHDRQUEUEERR | \
		QLOGIC_IB_HWE_SDMAMEMREADERR | \
		QLOGIC_IB_HWE_CLK_UC_PLLNOTLOCKED | \
		QLOGIC_IB_HWE_PCIESERDESQ0PCLKNOTDETECT | \
		QLOGIC_IB_HWE_PCIESERDESQ1PCLKNOTDETECT | \
		QLOGIC_IB_HWE_PCIESERDESQ2PCLKNOTDETECT | \
		QLOGIC_IB_HWE_PCIESERDESQ3PCLKNOTDETECT | \
		QLOGIC_IB_HWE_DDSRXEQMEMORYPARITYERR | \
		QLOGIC_IB_HWE_IB_UC_MEMORYPARITYERR | \
		QLOGIC_IB_HWE_PCIE_UC_OCT0MEMORYPARITYERR | \
		QLOGIC_IB_HWE_PCIE_UC_OCT1MEMORYPARITYERR)

#define IB_E_BITSEXTANT							\
	(ERR_MASK(RcvFormatErr) | ERR_MASK(RcvVCRCErr) |		\
	 ERR_MASK(RcvICRCErr) | ERR_MASK(RcvMinPktLenErr) |		\
	 ERR_MASK(RcvMaxPktLenErr) | ERR_MASK(RcvLongPktLenErr) |	\
	 ERR_MASK(RcvShortPktLenErr) | ERR_MASK(RcvUnexpectedCharErr) | \
	 ERR_MASK(RcvUnsupportedVLErr) | ERR_MASK(RcvEBPErr) |		\
	 ERR_MASK(RcvIBFlowErr) | ERR_MASK(RcvBadVersionErr) |		\
	 ERR_MASK(RcvEgrFullErr) | ERR_MASK(RcvHdrFullErr) |		\
	 ERR_MASK(RcvBadTidErr) | ERR_MASK(RcvHdrLenErr) |		\
	 ERR_MASK(RcvHdrErr) | ERR_MASK(RcvIBLostLinkErr) |		\
	 ERR_MASK(SendSpecialTriggerErr) |				\
	 ERR_MASK(SDmaDisabledErr) | ERR_MASK(SendMinPktLenErr) |	\
	 ERR_MASK(SendMaxPktLenErr) | ERR_MASK(SendUnderRunErr) |	\
	 ERR_MASK(SendPktLenErr) | ERR_MASK(SendDroppedSmpPktErr) |	\
	 ERR_MASK(SendDroppedDataPktErr) |				\
	 ERR_MASK(SendPioArmLaunchErr) |				\
	 ERR_MASK(SendUnexpectedPktNumErr) |				\
	 ERR_MASK(SendUnsupportedVLErr) | ERR_MASK(SendBufMisuseErr) |	\
	 ERR_MASK(SDmaGenMismatchErr) | ERR_MASK(SDmaOutOfBoundErr) |	\
	 ERR_MASK(SDmaTailOutOfBoundErr) | ERR_MASK(SDmaBaseErr) |	\
	 ERR_MASK(SDma1stDescErr) | ERR_MASK(SDmaRpyTagErr) |		\
	 ERR_MASK(SDmaDwEnErr) | ERR_MASK(SDmaMissingDwErr) |		\
	 ERR_MASK(SDmaUnexpDataErr) |					\
	 ERR_MASK(IBStatusChanged) | ERR_MASK(InvalidAddrErr) |		\
	 ERR_MASK(ResetNegated) | ERR_MASK(HardwareErr) |		\
	 ERR_MASK(SDmaDescAddrMisalignErr) |				\
	 ERR_MASK(InvalidEEPCmd))

#define QLOGIC_IB_HWE_PCIEMEMPARITYERR_MASK  0x00000000000000ffULL
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
#define QLOGIC_IB_HWE_PCIECPLDATAQUEUEERR         0x0000000000000040ULL
#define QLOGIC_IB_HWE_PCIECPLHDRQUEUEERR          0x0000000000000080ULL
#define QLOGIC_IB_HWE_SDMAMEMREADERR              0x0000000010000000ULL
#define QLOGIC_IB_HWE_CLK_UC_PLLNOTLOCKED          0x2000000000000000ULL
#define QLOGIC_IB_HWE_PCIESERDESQ0PCLKNOTDETECT   0x0100000000000000ULL
#define QLOGIC_IB_HWE_PCIESERDESQ1PCLKNOTDETECT   0x0200000000000000ULL
#define QLOGIC_IB_HWE_PCIESERDESQ2PCLKNOTDETECT   0x0400000000000000ULL
#define QLOGIC_IB_HWE_PCIESERDESQ3PCLKNOTDETECT   0x0800000000000000ULL
#define QLOGIC_IB_HWE_DDSRXEQMEMORYPARITYERR       0x0000008000000000ULL
#define QLOGIC_IB_HWE_IB_UC_MEMORYPARITYERR        0x0000004000000000ULL
#define QLOGIC_IB_HWE_PCIE_UC_OCT0MEMORYPARITYERR 0x0000001000000000ULL
#define QLOGIC_IB_HWE_PCIE_UC_OCT1MEMORYPARITYERR 0x0000002000000000ULL

#define IBA7220_IBCC_LINKCMD_SHIFT 19

#define IBA7220_IBC_DLIDLMC_MASK        0xFFFFFFFFUL
#define IBA7220_IBC_DLIDLMC_SHIFT       32

#define IBA7220_IBC_HRTBT_MASK  (SYM_RMASK(IBCDDRCtrl, HRTBT_AUTO) | \
				 SYM_RMASK(IBCDDRCtrl, HRTBT_ENB))
#define IBA7220_IBC_HRTBT_SHIFT SYM_LSB(IBCDDRCtrl, HRTBT_ENB)

#define IBA7220_IBC_LANE_REV_SUPPORTED (1<<8)
#define IBA7220_IBC_LREV_MASK   1
#define IBA7220_IBC_LREV_SHIFT  8
#define IBA7220_IBC_RXPOL_MASK  1
#define IBA7220_IBC_RXPOL_SHIFT 7
#define IBA7220_IBC_WIDTH_SHIFT 5
#define IBA7220_IBC_WIDTH_MASK  0x3
#define IBA7220_IBC_WIDTH_1X_ONLY       (0 << IBA7220_IBC_WIDTH_SHIFT)
#define IBA7220_IBC_WIDTH_4X_ONLY       (1 << IBA7220_IBC_WIDTH_SHIFT)
#define IBA7220_IBC_WIDTH_AUTONEG       (2 << IBA7220_IBC_WIDTH_SHIFT)
#define IBA7220_IBC_SPEED_AUTONEG       (1 << 1)
#define IBA7220_IBC_SPEED_SDR           (1 << 2)
#define IBA7220_IBC_SPEED_DDR           (1 << 3)
#define IBA7220_IBC_SPEED_AUTONEG_MASK  (0x7 << 1)
#define IBA7220_IBC_IBTA_1_2_MASK       (1)

#define IBA7220_DDRSTAT_LINKLAT_MASK    0x3ffffff

#define QLOGIC_IB_EXTS_FREQSEL 0x2
#define QLOGIC_IB_EXTS_SERDESSEL 0x4
#define QLOGIC_IB_EXTS_MEMBIST_ENDTEST     0x0000000000004000
#define QLOGIC_IB_EXTS_MEMBIST_DISABLED    0x0000000000008000

#define QLOGIC_IB_XGXS_RESET          0x5ULL
#define QLOGIC_IB_XGXS_FC_SAFE        (1ULL << 63)

#define IBA7220_LEDBLINK_ON_SHIFT 32 
#define IBA7220_LEDBLINK_OFF_SHIFT 0 

#define _QIB_GPIO_SDA_NUM 1
#define _QIB_GPIO_SCL_NUM 0
#define QIB_TWSI_EEPROM_DEV 0xA2 
#define QIB_TWSI_TEMP_DEV 0x98

#define QIB_7220_PSXMITWAIT_CHECK_RATE 4000

#define IBA7220_R_INTRAVAIL_SHIFT 17
#define IBA7220_R_PKEY_DIS_SHIFT 34
#define IBA7220_R_TAILUPD_SHIFT 35
#define IBA7220_R_CTXTCFG_SHIFT 36

#define IBA7220_HDRHEAD_PKTINT_SHIFT 32 

#define IBA7220_TID_SZ_SHIFT 37 
#define IBA7220_TID_SZ_2K (1UL << IBA7220_TID_SZ_SHIFT) 
#define IBA7220_TID_SZ_4K (2UL << IBA7220_TID_SZ_SHIFT) 
#define IBA7220_TID_PA_SHIFT 11U 
#define PBC_7220_VL15_SEND (1ULL << 63) 
#define PBC_7220_VL15_SEND_CTRL (1ULL << 31) 

#define AUTONEG_TRIES 5 

static u8 rate_to_delay[2][2] = {
	
	{   8, 2 }, 
	{   4, 1 }  
};

static u8 ib_rate_to_delay[IB_RATE_120_GBPS + 1] = {
	[IB_RATE_2_5_GBPS] = 8,
	[IB_RATE_5_GBPS] = 4,
	[IB_RATE_10_GBPS] = 2,
	[IB_RATE_20_GBPS] = 1
};

#define IBA7220_LINKSPEED_SHIFT SYM_LSB(IBCStatus, LinkSpeedActive)
#define IBA7220_LINKWIDTH_SHIFT SYM_LSB(IBCStatus, LinkWidthActive)

#define IB_7220_LT_STATE_DISABLED        0x00
#define IB_7220_LT_STATE_LINKUP          0x01
#define IB_7220_LT_STATE_POLLACTIVE      0x02
#define IB_7220_LT_STATE_POLLQUIET       0x03
#define IB_7220_LT_STATE_SLEEPDELAY      0x04
#define IB_7220_LT_STATE_SLEEPQUIET      0x05
#define IB_7220_LT_STATE_CFGDEBOUNCE     0x08
#define IB_7220_LT_STATE_CFGRCVFCFG      0x09
#define IB_7220_LT_STATE_CFGWAITRMT      0x0a
#define IB_7220_LT_STATE_CFGIDLE 0x0b
#define IB_7220_LT_STATE_RECOVERRETRAIN  0x0c
#define IB_7220_LT_STATE_RECOVERWAITRMT  0x0e
#define IB_7220_LT_STATE_RECOVERIDLE     0x0f

#define IB_7220_L_STATE_DOWN             0x0
#define IB_7220_L_STATE_INIT             0x1
#define IB_7220_L_STATE_ARM              0x2
#define IB_7220_L_STATE_ACTIVE           0x3
#define IB_7220_L_STATE_ACT_DEFER        0x4

static const u8 qib_7220_physportstate[0x20] = {
	[IB_7220_LT_STATE_DISABLED] = IB_PHYSPORTSTATE_DISABLED,
	[IB_7220_LT_STATE_LINKUP] = IB_PHYSPORTSTATE_LINKUP,
	[IB_7220_LT_STATE_POLLACTIVE] = IB_PHYSPORTSTATE_POLL,
	[IB_7220_LT_STATE_POLLQUIET] = IB_PHYSPORTSTATE_POLL,
	[IB_7220_LT_STATE_SLEEPDELAY] = IB_PHYSPORTSTATE_SLEEP,
	[IB_7220_LT_STATE_SLEEPQUIET] = IB_PHYSPORTSTATE_SLEEP,
	[IB_7220_LT_STATE_CFGDEBOUNCE] =
		IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_7220_LT_STATE_CFGRCVFCFG] =
		IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_7220_LT_STATE_CFGWAITRMT] =
		IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_7220_LT_STATE_CFGIDLE] = IB_PHYSPORTSTATE_CFG_TRAIN,
	[IB_7220_LT_STATE_RECOVERRETRAIN] =
		IB_PHYSPORTSTATE_LINK_ERR_RECOVER,
	[IB_7220_LT_STATE_RECOVERWAITRMT] =
		IB_PHYSPORTSTATE_LINK_ERR_RECOVER,
	[IB_7220_LT_STATE_RECOVERIDLE] =
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

int qib_special_trigger;
module_param_named(special_trigger, qib_special_trigger, int, S_IRUGO);
MODULE_PARM_DESC(special_trigger, "Enable SpecialTrigger arm/launch");

#define IBCBUSFRSPCPARITYERR HWE_MASK(IBCBusFromSPCParityErr)
#define IBCBUSTOSPCPARITYERR HWE_MASK(IBCBusToSPCParityErr)

#define SYM_MASK_BIT(regname, fldname, bit) ((u64) \
	(1ULL << (SYM_LSB(regname, fldname) + (bit))))

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

static const struct qib_hwerror_msgs qib_7220_hwerror_msgs[] = {
	
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
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIECPLDATAQUEUEERR,
			  "PCIe cpl header queue"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIECPLHDRQUEUEERR,
			  "PCIe cpl data queue"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_SDMAMEMREADERR,
			  "Send DMA memory read"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_CLK_UC_PLLNOTLOCKED,
			  "uC PLL clock not locked"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIESERDESQ0PCLKNOTDETECT,
			  "PCIe serdes Q0 no clock"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIESERDESQ1PCLKNOTDETECT,
			  "PCIe serdes Q1 no clock"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIESERDESQ2PCLKNOTDETECT,
			  "PCIe serdes Q2 no clock"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIESERDESQ3PCLKNOTDETECT,
			  "PCIe serdes Q3 no clock"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_DDSRXEQMEMORYPARITYERR,
			  "DDS RXEQ memory parity"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_IB_UC_MEMORYPARITYERR,
			  "IB uC memory parity"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIE_UC_OCT0MEMORYPARITYERR,
			  "PCIe uC oct0 memory parity"),
	QLOGIC_IB_HWE_MSG(QLOGIC_IB_HWE_PCIE_UC_OCT1MEMORYPARITYERR,
			  "PCIe uC oct1 memory parity"),
};

#define RXE_PARITY (RXEMEMPARITYERR_EAGERTID|RXEMEMPARITYERR_EXPTID)

#define QLOGIC_IB_E_PKTERRS (\
		ERR_MASK(SendPktLenErr) |				\
		ERR_MASK(SendDroppedDataPktErr) |			\
		ERR_MASK(RcvVCRCErr) |					\
		ERR_MASK(RcvICRCErr) |					\
		ERR_MASK(RcvShortPktLenErr) |				\
		ERR_MASK(RcvEBPErr))

#define QLOGIC_IB_E_SDMAERRS ( \
		ERR_MASK(SDmaGenMismatchErr) |				\
		ERR_MASK(SDmaOutOfBoundErr) |				\
		ERR_MASK(SDmaTailOutOfBoundErr) | ERR_MASK(SDmaBaseErr) | \
		ERR_MASK(SDma1stDescErr) | ERR_MASK(SDmaRpyTagErr) |	\
		ERR_MASK(SDmaDwEnErr) | ERR_MASK(SDmaMissingDwErr) |	\
		ERR_MASK(SDmaUnexpDataErr) |				\
		ERR_MASK(SDmaDescAddrMisalignErr) |			\
		ERR_MASK(SDmaDisabledErr) |				\
		ERR_MASK(SendBufMisuseErr))

#define E_SUM_PKTERRS \
	(ERR_MASK(RcvHdrLenErr) | ERR_MASK(RcvBadTidErr) |		\
	 ERR_MASK(RcvBadVersionErr) | ERR_MASK(RcvHdrErr) |		\
	 ERR_MASK(RcvLongPktLenErr) | ERR_MASK(RcvShortPktLenErr) |	\
	 ERR_MASK(RcvMaxPktLenErr) | ERR_MASK(RcvMinPktLenErr) |	\
	 ERR_MASK(RcvFormatErr) | ERR_MASK(RcvUnsupportedVLErr) |	\
	 ERR_MASK(RcvUnexpectedCharErr) | ERR_MASK(RcvEBPErr))

#define E_SUM_ERRS \
	(ERR_MASK(SendPioArmLaunchErr) | ERR_MASK(SendUnexpectedPktNumErr) | \
	 ERR_MASK(SendDroppedDataPktErr) | ERR_MASK(SendDroppedSmpPktErr) | \
	 ERR_MASK(SendMaxPktLenErr) | ERR_MASK(SendUnsupportedVLErr) |	\
	 ERR_MASK(SendMinPktLenErr) | ERR_MASK(SendPktLenErr) |		\
	 ERR_MASK(InvalidAddrErr))

#define E_SPKT_ERRS_IGNORE \
	(ERR_MASK(SendDroppedDataPktErr) | ERR_MASK(SendDroppedSmpPktErr) | \
	 ERR_MASK(SendMaxPktLenErr) | ERR_MASK(SendMinPktLenErr) |	\
	 ERR_MASK(SendPktLenErr))

#define E_SUM_LINK_PKTERRS \
	(ERR_MASK(SendDroppedDataPktErr) | ERR_MASK(SendDroppedSmpPktErr) | \
	 ERR_MASK(SendMinPktLenErr) | ERR_MASK(SendPktLenErr) |		\
	 ERR_MASK(RcvShortPktLenErr) | ERR_MASK(RcvMinPktLenErr) |	\
	 ERR_MASK(RcvUnexpectedCharErr))

static void autoneg_7220_work(struct work_struct *);
static u32 __iomem *qib_7220_getsendbuf(struct qib_pportdata *, u64, u32 *);

static void qib_disarm_7220_senderrbufs(struct qib_pportdata *ppd)
{
	unsigned long sbuf[3];
	struct qib_devdata *dd = ppd->dd;

	
	sbuf[0] = qib_read_kreg64(dd, kr_sendbuffererror);
	sbuf[1] = qib_read_kreg64(dd, kr_sendbuffererror + 1);
	sbuf[2] = qib_read_kreg64(dd, kr_sendbuffererror + 2);

	if (sbuf[0] || sbuf[1] || sbuf[2])
		qib_disarm_piobufs_set(dd, sbuf,
				       dd->piobcnt2k + dd->piobcnt4k);
}

static void qib_7220_txe_recover(struct qib_devdata *dd)
{
	qib_devinfo(dd->pcidev, "Recovering from TXE PIO parity error\n");
	qib_disarm_7220_senderrbufs(dd->pport);
}

static void qib_7220_sdma_sendctrl(struct qib_pportdata *ppd, unsigned op)
{
	struct qib_devdata *dd = ppd->dd;
	u64 set_sendctrl = 0;
	u64 clr_sendctrl = 0;

	if (op & QIB_SDMA_SENDCTRL_OP_ENABLE)
		set_sendctrl |= SYM_MASK(SendCtrl, SDmaEnable);
	else
		clr_sendctrl |= SYM_MASK(SendCtrl, SDmaEnable);

	if (op & QIB_SDMA_SENDCTRL_OP_INTENABLE)
		set_sendctrl |= SYM_MASK(SendCtrl, SDmaIntEnable);
	else
		clr_sendctrl |= SYM_MASK(SendCtrl, SDmaIntEnable);

	if (op & QIB_SDMA_SENDCTRL_OP_HALT)
		set_sendctrl |= SYM_MASK(SendCtrl, SDmaHalt);
	else
		clr_sendctrl |= SYM_MASK(SendCtrl, SDmaHalt);

	spin_lock(&dd->sendctrl_lock);

	dd->sendctrl |= set_sendctrl;
	dd->sendctrl &= ~clr_sendctrl;

	qib_write_kreg(dd, kr_sendctrl, dd->sendctrl);
	qib_write_kreg(dd, kr_scratch, 0);

	spin_unlock(&dd->sendctrl_lock);
}

static void qib_decode_7220_sdma_errs(struct qib_pportdata *ppd,
				      u64 err, char *buf, size_t blen)
{
	static const struct {
		u64 err;
		const char *msg;
	} errs[] = {
		{ ERR_MASK(SDmaGenMismatchErr),
		  "SDmaGenMismatch" },
		{ ERR_MASK(SDmaOutOfBoundErr),
		  "SDmaOutOfBound" },
		{ ERR_MASK(SDmaTailOutOfBoundErr),
		  "SDmaTailOutOfBound" },
		{ ERR_MASK(SDmaBaseErr),
		  "SDmaBase" },
		{ ERR_MASK(SDma1stDescErr),
		  "SDma1stDesc" },
		{ ERR_MASK(SDmaRpyTagErr),
		  "SDmaRpyTag" },
		{ ERR_MASK(SDmaDwEnErr),
		  "SDmaDwEn" },
		{ ERR_MASK(SDmaMissingDwErr),
		  "SDmaMissingDw" },
		{ ERR_MASK(SDmaUnexpDataErr),
		  "SDmaUnexpData" },
		{ ERR_MASK(SDmaDescAddrMisalignErr),
		  "SDmaDescAddrMisalign" },
		{ ERR_MASK(SendBufMisuseErr),
		  "SendBufMisuse" },
		{ ERR_MASK(SDmaDisabledErr),
		  "SDmaDisabled" },
	};
	int i;
	size_t bidx = 0;

	for (i = 0; i < ARRAY_SIZE(errs); i++) {
		if (err & errs[i].err)
			bidx += scnprintf(buf + bidx, blen - bidx,
					 "%s ", errs[i].msg);
	}
}

static void qib_7220_sdma_hw_clean_up(struct qib_pportdata *ppd)
{
	
	sendctrl_7220_mod(ppd, QIB_SENDCTRL_DISARM_ALL | QIB_SENDCTRL_FLUSH |
			  QIB_SENDCTRL_AVAIL_BLIP);
	ppd->dd->upd_pio_shadow  = 1; 
}

static void qib_sdma_7220_setlengen(struct qib_pportdata *ppd)
{
	qib_write_kreg(ppd->dd, kr_senddmalengen, ppd->sdma_descq_cnt);
	qib_write_kreg(ppd->dd, kr_senddmalengen,
		       ppd->sdma_descq_cnt |
		       (1ULL << QIB_7220_SendDmaLenGen_Generation_MSB));
}

static void qib_7220_sdma_hw_start_up(struct qib_pportdata *ppd)
{
	qib_sdma_7220_setlengen(ppd);
	qib_sdma_update_7220_tail(ppd, 0); 
	ppd->sdma_head_dma[0] = 0;
}

#define DISABLES_SDMA (							\
		ERR_MASK(SDmaDisabledErr) |				\
		ERR_MASK(SDmaBaseErr) |					\
		ERR_MASK(SDmaTailOutOfBoundErr) |			\
		ERR_MASK(SDmaOutOfBoundErr) |				\
		ERR_MASK(SDma1stDescErr) |				\
		ERR_MASK(SDmaRpyTagErr) |				\
		ERR_MASK(SDmaGenMismatchErr) |				\
		ERR_MASK(SDmaDescAddrMisalignErr) |			\
		ERR_MASK(SDmaMissingDwErr) |				\
		ERR_MASK(SDmaDwEnErr))

static void sdma_7220_errors(struct qib_pportdata *ppd, u64 errs)
{
	unsigned long flags;
	struct qib_devdata *dd = ppd->dd;
	char *msg;

	errs &= QLOGIC_IB_E_SDMAERRS;

	msg = dd->cspec->sdmamsgbuf;
	qib_decode_7220_sdma_errs(ppd, errs, msg, sizeof dd->cspec->sdmamsgbuf);
	spin_lock_irqsave(&ppd->sdma_lock, flags);

	if (errs & ERR_MASK(SendBufMisuseErr)) {
		unsigned long sbuf[3];

		sbuf[0] = qib_read_kreg64(dd, kr_sendbuffererror);
		sbuf[1] = qib_read_kreg64(dd, kr_sendbuffererror + 1);
		sbuf[2] = qib_read_kreg64(dd, kr_sendbuffererror + 2);

		qib_dev_err(ppd->dd,
			    "IB%u:%u SendBufMisuse: %04lx %016lx %016lx\n",
			    ppd->dd->unit, ppd->port, sbuf[2], sbuf[1],
			    sbuf[0]);
	}

	if (errs & ERR_MASK(SDmaUnexpDataErr))
		qib_dev_err(dd, "IB%u:%u SDmaUnexpData\n", ppd->dd->unit,
			    ppd->port);

	switch (ppd->sdma_state.current_state) {
	case qib_sdma_state_s00_hw_down:
		
		break;

	case qib_sdma_state_s10_hw_start_up_wait:
		
		break;

	case qib_sdma_state_s20_idle:
		
		break;

	case qib_sdma_state_s30_sw_clean_up_wait:
		
		break;

	case qib_sdma_state_s40_hw_clean_up_wait:
		if (errs & ERR_MASK(SDmaDisabledErr))
			__qib_sdma_process_event(ppd,
				qib_sdma_event_e50_hw_cleaned);
		break;

	case qib_sdma_state_s50_hw_halt_wait:
		
		break;

	case qib_sdma_state_s99_running:
		if (errs & DISABLES_SDMA)
			__qib_sdma_process_event(ppd,
				qib_sdma_event_e7220_err_halted);
		break;
	}

	spin_unlock_irqrestore(&ppd->sdma_lock, flags);
}

static int qib_decode_7220_err(struct qib_devdata *dd, char *buf, size_t blen,
			       u64 err)
{
	int iserr = 1;

	*buf = '\0';
	if (err & QLOGIC_IB_E_PKTERRS) {
		if (!(err & ~QLOGIC_IB_E_PKTERRS))
			iserr = 0;
		if ((err & ERR_MASK(RcvICRCErr)) &&
		    !(err & (ERR_MASK(RcvVCRCErr) | ERR_MASK(RcvEBPErr))))
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
	if (err & ERR_MASK(SendSpecialTriggerErr))
		strlcat(buf, "sendspecialtrigger ", blen);
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
	if (err & QLOGIC_IB_E_SDMAERRS)
		qib_decode_7220_sdma_errs(dd->pport, err, buf, blen);
	if (err & ERR_MASK(InvalidEEPCmd))
		strlcat(buf, "invalideepromcmd ", blen);
done:
	return iserr;
}

static void reenable_7220_chase(unsigned long opaque)
{
	struct qib_pportdata *ppd = (struct qib_pportdata *)opaque;
	ppd->cpspec->chase_timer.expires = 0;
	qib_set_ib_7220_lstate(ppd, QLOGIC_IB_IBCC_LINKCMD_DOWN,
		QLOGIC_IB_IBCC_LINKINITCMD_POLL);
}

static void handle_7220_chase(struct qib_pportdata *ppd, u64 ibcst)
{
	u8 ibclt;
	unsigned long tnow;

	ibclt = (u8)SYM_FIELD(ibcst, IBCStatus, LinkTrainingState);

	switch (ibclt) {
	case IB_7220_LT_STATE_CFGRCVFCFG:
	case IB_7220_LT_STATE_CFGWAITRMT:
	case IB_7220_LT_STATE_TXREVLANES:
	case IB_7220_LT_STATE_CFGENH:
		tnow = jiffies;
		if (ppd->cpspec->chase_end &&
		    time_after(tnow, ppd->cpspec->chase_end)) {
			ppd->cpspec->chase_end = 0;
			qib_set_ib_7220_lstate(ppd,
				QLOGIC_IB_IBCC_LINKCMD_DOWN,
				QLOGIC_IB_IBCC_LINKINITCMD_DISABLE);
			ppd->cpspec->chase_timer.expires = jiffies +
				QIB_CHASE_DIS_TIME;
			add_timer(&ppd->cpspec->chase_timer);
		} else if (!ppd->cpspec->chase_end)
			ppd->cpspec->chase_end = tnow + QIB_CHASE_TIME;
		break;

	default:
		ppd->cpspec->chase_end = 0;
		break;
	}
}

static void handle_7220_errors(struct qib_devdata *dd, u64 errs)
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
		qib_7220_handle_hwerrors(dd, msg, sizeof dd->cspec->emsgbuf);
	else
		for (log_idx = 0; log_idx < QIB_EEP_LOG_CNT; ++log_idx)
			if (errs & dd->eep_st_masks[log_idx].errs_to_log)
				qib_inc_eeprom_err(dd, log_idx, 1);

	if (errs & QLOGIC_IB_E_SDMAERRS)
		sdma_7220_errors(ppd, errs);

	if (errs & ~IB_E_BITSEXTANT)
		qib_dev_err(dd, "error interrupt with unknown errors "
			    "%llx set\n", (unsigned long long)
			    (errs & ~IB_E_BITSEXTANT));

	if (errs & E_SUM_ERRS) {
		qib_disarm_7220_senderrbufs(ppd);
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

	mask = ERR_MASK(IBStatusChanged) |
		ERR_MASK(RcvEgrFullErr) | ERR_MASK(RcvHdrFullErr) |
		ERR_MASK(HardwareErr) | ERR_MASK(SDmaDisabledErr);

	qib_decode_7220_err(dd, msg, sizeof dd->cspec->emsgbuf, errs & ~mask);

	if (errs & E_SUM_PKTERRS)
		qib_stats.sps_rcverrs++;
	if (errs & E_SUM_ERRS)
		qib_stats.sps_txerrs++;
	iserr = errs & ~(E_SUM_PKTERRS | QLOGIC_IB_E_PKTERRS |
			 ERR_MASK(SDmaDisabledErr));

	if (errs & ERR_MASK(IBStatusChanged)) {
		u64 ibcs;

		ibcs = qib_read_kreg64(dd, kr_ibcstatus);
		if (!(ppd->lflags & QIBL_IB_AUTONEG_INPROG))
			handle_7220_chase(ppd, ibcs);

		
		ppd->link_width_active =
			((ibcs >> IBA7220_LINKWIDTH_SHIFT) & 1) ?
			    IB_WIDTH_4X : IB_WIDTH_1X;
		ppd->link_speed_active =
			((ibcs >> IBA7220_LINKSPEED_SHIFT) & 1) ?
			    QIB_IB_DDR : QIB_IB_SDR;

		if (qib_7220_phys_portstate(ibcs) !=
					    IB_PHYSPORTSTATE_LINK_ERR_RECOVER)
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

static void qib_7220_set_intr_state(struct qib_devdata *dd, u32 enable)
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
 * even though the details are similar on most chips.
 */
static void qib_7220_clear_freeze(struct qib_devdata *dd)
{
	
	qib_write_kreg(dd, kr_errmask, 0ULL);

	
	qib_7220_set_intr_state(dd, 0);

	qib_cancel_sends(dd->pport);

	
	qib_write_kreg(dd, kr_control, dd->control);
	qib_read_kreg32(dd, kr_scratch);

	
	qib_force_pio_avail_update(dd);

	qib_write_kreg(dd, kr_hwerrclear, 0ULL);
	qib_write_kreg(dd, kr_errclear, E_SPKT_ERRS_IGNORE);
	qib_write_kreg(dd, kr_errmask, dd->cspec->errormask);
	qib_7220_set_intr_state(dd, 1);
}

static void qib_7220_handle_hwerrors(struct qib_devdata *dd, char *msg,
				     size_t msgl)
{
	u64 hwerrs;
	u32 bits, ctrl;
	int isfatal = 0;
	char *bitsmsg;
	int log_idx;

	hwerrs = qib_read_kreg64(dd, kr_hwerrstatus);
	if (!hwerrs)
		goto bail;
	if (hwerrs == ~0ULL) {
		qib_dev_err(dd, "Read of hardware error status failed "
			    "(all bits set); ignoring\n");
		goto bail;
	}
	qib_stats.sps_hwerrs++;

	qib_write_kreg(dd, kr_hwerrclear,
		       hwerrs & ~HWE_MASK(PowerOnBISTFailed));

	hwerrs &= dd->cspec->hwerrmask;

	
	for (log_idx = 0; log_idx < QIB_EEP_LOG_CNT; ++log_idx)
		if (hwerrs & dd->eep_st_masks[log_idx].hwerrs_to_log)
			qib_inc_eeprom_err(dd, log_idx, 1);
	if (hwerrs & ~(TXEMEMPARITYERR_PIOBUF | TXEMEMPARITYERR_PIOPBC |
		       RXE_PARITY))
		qib_devinfo(dd->pcidev, "Hardware error: hwerr=0x%llx "
			 "(cleared)\n", (unsigned long long) hwerrs);

	if (hwerrs & ~IB_HWE_BITSEXTANT)
		qib_dev_err(dd, "hwerror interrupt with unknown errors "
			    "%llx set\n", (unsigned long long)
			    (hwerrs & ~IB_HWE_BITSEXTANT));

	if (hwerrs & QLOGIC_IB_HWE_IB_UC_MEMORYPARITYERR)
		qib_sd7220_clr_ibpar(dd);

	ctrl = qib_read_kreg32(dd, kr_control);
	if ((ctrl & QLOGIC_IB_C_FREEZEMODE) && !dd->diag_client) {
		if (hwerrs & (TXEMEMPARITYERR_PIOBUF |
			      TXEMEMPARITYERR_PIOPBC)) {
			qib_7220_txe_recover(dd);
			hwerrs &= ~(TXEMEMPARITYERR_PIOBUF |
				    TXEMEMPARITYERR_PIOPBC);
		}
		if (hwerrs)
			isfatal = 1;
		else
			qib_7220_clear_freeze(dd);
	}

	*msg = '\0';

	if (hwerrs & HWE_MASK(PowerOnBISTFailed)) {
		isfatal = 1;
		strlcat(msg, "[Memory BIST test failed, "
			"InfiniPath hardware unusable]", msgl);
		
		dd->cspec->hwerrmask &= ~HWE_MASK(PowerOnBISTFailed);
		qib_write_kreg(dd, kr_hwerrmask, dd->cspec->hwerrmask);
	}

	qib_format_hwerrors(hwerrs, qib_7220_hwerror_msgs,
			    ARRAY_SIZE(qib_7220_hwerror_msgs), msg, msgl);

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

#define _QIB_PLL_FAIL (QLOGIC_IB_HWE_COREPLL_FBSLIP |   \
			 QLOGIC_IB_HWE_COREPLL_RFSLIP)

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

static void qib_7220_init_hwerrors(struct qib_devdata *dd)
{
	u64 val;
	u64 extsval;

	extsval = qib_read_kreg64(dd, kr_extstatus);

	if (!(extsval & (QLOGIC_IB_EXTS_MEMBIST_ENDTEST |
			 QLOGIC_IB_EXTS_MEMBIST_DISABLED)))
		qib_dev_err(dd, "MemBIST did not complete!\n");
	if (extsval & QLOGIC_IB_EXTS_MEMBIST_DISABLED)
		qib_devinfo(dd->pcidev, "MemBIST is disabled.\n");

	val = ~0ULL;    

	val &= ~QLOGIC_IB_HWE_IB_UC_MEMORYPARITYERR;
	dd->cspec->hwerrmask = val;

	qib_write_kreg(dd, kr_hwerrclear, ~HWE_MASK(PowerOnBISTFailed));
	qib_write_kreg(dd, kr_hwerrmask, dd->cspec->hwerrmask);

	
	qib_write_kreg(dd, kr_errclear, ~0ULL);
	
	qib_write_kreg(dd, kr_errmask, ~0ULL);
	dd->cspec->errormask = qib_read_kreg64(dd, kr_errmask);
	
	qib_write_kreg(dd, kr_intclear, ~0ULL);
}

static void qib_set_7220_armlaunch(struct qib_devdata *dd, u32 enable)
{
	if (enable) {
		qib_write_kreg(dd, kr_errclear, ERR_MASK(SendPioArmLaunchErr));
		dd->cspec->errormask |= ERR_MASK(SendPioArmLaunchErr);
	} else
		dd->cspec->errormask &= ~ERR_MASK(SendPioArmLaunchErr);
	qib_write_kreg(dd, kr_errmask, dd->cspec->errormask);
}

static void qib_set_ib_7220_lstate(struct qib_pportdata *ppd, u16 linkcmd,
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

	mod_wd = (linkcmd << IBA7220_IBCC_LINKCMD_SHIFT) |
		(linitcmd << QLOGIC_IB_IBCC_LINKINITCMD_SHIFT);

	qib_write_kreg(dd, kr_ibcctrl, ppd->cpspec->ibcctrl | mod_wd);
	
	qib_write_kreg(dd, kr_scratch, 0);
}


static int qib_7220_bringup_serdes(struct qib_pportdata *ppd)
{
	struct qib_devdata *dd = ppd->dd;
	u64 val, prev_val, guid, ibc;
	int ret = 0;

	
	dd->control &= ~QLOGIC_IB_C_LINKENABLE;
	qib_write_kreg(dd, kr_control, 0ULL);

	if (qib_compat_ddr_negotiate) {
		ppd->cpspec->ibdeltainprog = 1;
		ppd->cpspec->ibsymsnap = read_7220_creg32(dd, cr_ibsymbolerr);
		ppd->cpspec->iblnkerrsnap =
			read_7220_creg32(dd, cr_iblinkerrrecov);
	}

	
	ibc = 0x5ULL << SYM_LSB(IBCCtrl, FlowCtrlWaterMark);
	ibc |= 0x3ULL << SYM_LSB(IBCCtrl, FlowCtrlPeriod);
	
	ibc |= 0xfULL << SYM_LSB(IBCCtrl, PhyerrThreshold);
	
	ibc |= 4ULL << SYM_LSB(IBCCtrl, CreditScale);
	
	ibc |= 0xfULL << SYM_LSB(IBCCtrl, OverrunThreshold);
	ibc |= ((u64)(ppd->ibmaxlen >> 2) + 1) << SYM_LSB(IBCCtrl, MaxPktLen);
	ppd->cpspec->ibcctrl = ibc; 

	
	val = ppd->cpspec->ibcctrl | (QLOGIC_IB_IBCC_LINKINITCMD_DISABLE <<
		QLOGIC_IB_IBCC_LINKINITCMD_SHIFT);
	qib_write_kreg(dd, kr_ibcctrl, val);

	if (!ppd->cpspec->ibcddrctrl) {
		
		ppd->cpspec->ibcddrctrl = qib_read_kreg64(dd, kr_ibcddrctrl);

		if (ppd->link_speed_enabled == (QIB_IB_SDR | QIB_IB_DDR))
			ppd->cpspec->ibcddrctrl |=
				IBA7220_IBC_SPEED_AUTONEG_MASK |
				IBA7220_IBC_IBTA_1_2_MASK;
		else
			ppd->cpspec->ibcddrctrl |=
				ppd->link_speed_enabled == QIB_IB_DDR ?
				IBA7220_IBC_SPEED_DDR : IBA7220_IBC_SPEED_SDR;
		if ((ppd->link_width_enabled & (IB_WIDTH_1X | IB_WIDTH_4X)) ==
		    (IB_WIDTH_1X | IB_WIDTH_4X))
			ppd->cpspec->ibcddrctrl |= IBA7220_IBC_WIDTH_AUTONEG;
		else
			ppd->cpspec->ibcddrctrl |=
				ppd->link_width_enabled == IB_WIDTH_4X ?
				IBA7220_IBC_WIDTH_4X_ONLY :
				IBA7220_IBC_WIDTH_1X_ONLY;

		
		ppd->cpspec->ibcddrctrl |=
			IBA7220_IBC_RXPOL_MASK << IBA7220_IBC_RXPOL_SHIFT;
		ppd->cpspec->ibcddrctrl |=
			IBA7220_IBC_HRTBT_MASK << IBA7220_IBC_HRTBT_SHIFT;

		
		ppd->cpspec->ibcddrctrl |= IBA7220_IBC_LANE_REV_SUPPORTED;
	} else
		
		qib_write_kreg(dd, kr_scratch, 0);

	qib_write_kreg(dd, kr_ibcddrctrl, ppd->cpspec->ibcddrctrl);
	qib_write_kreg(dd, kr_scratch, 0);

	qib_write_kreg(dd, kr_ncmodectrl, 0Ull);
	qib_write_kreg(dd, kr_scratch, 0);

	ret = qib_sd7220_init(dd);

	val = qib_read_kreg64(dd, kr_xgxs_cfg);
	prev_val = val;
	val |= QLOGIC_IB_XGXS_FC_SAFE;
	if (val != prev_val) {
		qib_write_kreg(dd, kr_xgxs_cfg, val);
		qib_read_kreg32(dd, kr_scratch);
	}
	if (val & QLOGIC_IB_XGXS_RESET)
		val &= ~QLOGIC_IB_XGXS_RESET;
	if (val != prev_val)
		qib_write_kreg(dd, kr_xgxs_cfg, val);

	
	if (!ppd->guid)
		ppd->guid = dd->base_guid;
	guid = be64_to_cpu(ppd->guid);

	qib_write_kreg(dd, kr_hrtbt_guid, guid);
	if (!ret) {
		dd->control |= QLOGIC_IB_C_LINKENABLE;
		qib_write_kreg(dd, kr_control, dd->control);
	} else
		
		qib_write_kreg(dd, kr_scratch, 0);
	return ret;
}

static void qib_7220_quiet_serdes(struct qib_pportdata *ppd)
{
	u64 val;
	struct qib_devdata *dd = ppd->dd;
	unsigned long flags;

	
	dd->control &= ~QLOGIC_IB_C_LINKENABLE;
	qib_write_kreg(dd, kr_control,
		       dd->control | QLOGIC_IB_C_FREEZEMODE);

	ppd->cpspec->chase_end = 0;
	if (ppd->cpspec->chase_timer.data) 
		del_timer_sync(&ppd->cpspec->chase_timer);

	if (ppd->cpspec->ibsymdelta || ppd->cpspec->iblnkerrdelta ||
	    ppd->cpspec->ibdeltainprog) {
		u64 diagc;

		
		diagc = qib_read_kreg64(dd, kr_hwdiagctrl);
		qib_write_kreg(dd, kr_hwdiagctrl,
			       diagc | SYM_MASK(HwDiagCtrl, CounterWrEnable));

		if (ppd->cpspec->ibsymdelta || ppd->cpspec->ibdeltainprog) {
			val = read_7220_creg32(dd, cr_ibsymbolerr);
			if (ppd->cpspec->ibdeltainprog)
				val -= val - ppd->cpspec->ibsymsnap;
			val -= ppd->cpspec->ibsymdelta;
			write_7220_creg(dd, cr_ibsymbolerr, val);
		}
		if (ppd->cpspec->iblnkerrdelta || ppd->cpspec->ibdeltainprog) {
			val = read_7220_creg32(dd, cr_iblinkerrrecov);
			if (ppd->cpspec->ibdeltainprog)
				val -= val - ppd->cpspec->iblnkerrsnap;
			val -= ppd->cpspec->iblnkerrdelta;
			write_7220_creg(dd, cr_iblinkerrrecov, val);
		}

		
		qib_write_kreg(dd, kr_hwdiagctrl, diagc);
	}
	qib_set_ib_7220_lstate(ppd, 0, QLOGIC_IB_IBCC_LINKINITCMD_DISABLE);

	spin_lock_irqsave(&ppd->lflags_lock, flags);
	ppd->lflags &= ~QIBL_IB_AUTONEG_INPROG;
	spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	wake_up(&ppd->cpspec->autoneg_wait);
	cancel_delayed_work_sync(&ppd->cpspec->autoneg_work);

	shutdown_7220_relock_poll(ppd->dd);
	val = qib_read_kreg64(ppd->dd, kr_xgxs_cfg);
	val |= QLOGIC_IB_XGXS_RESET;
	qib_write_kreg(ppd->dd, kr_xgxs_cfg, val);
}

static void qib_setup_7220_setextled(struct qib_pportdata *ppd, u32 on)
{
	struct qib_devdata *dd = ppd->dd;
	u64 extctl, ledblink = 0, val, lst, ltst;
	unsigned long flags;

	if (dd->diag_client)
		return;

	if (ppd->led_override) {
		ltst = (ppd->led_override & QIB_LED_PHYS) ?
			IB_PHYSPORTSTATE_LINKUP : IB_PHYSPORTSTATE_DISABLED,
		lst = (ppd->led_override & QIB_LED_LOG) ?
			IB_PORT_ACTIVE : IB_PORT_DOWN;
	} else if (on) {
		val = qib_read_kreg64(dd, kr_ibcstatus);
		ltst = qib_7220_phys_portstate(val);
		lst = qib_7220_iblink_state(val);
	} else {
		ltst = 0;
		lst = 0;
	}

	spin_lock_irqsave(&dd->cspec->gpio_lock, flags);
	extctl = dd->cspec->extctrl & ~(SYM_MASK(EXTCtrl, LEDPriPortGreenOn) |
				 SYM_MASK(EXTCtrl, LEDPriPortYellowOn));
	if (ltst == IB_PHYSPORTSTATE_LINKUP) {
		extctl |= SYM_MASK(EXTCtrl, LEDPriPortGreenOn);
		ledblink = ((66600 * 1000UL / 4) << IBA7220_LEDBLINK_ON_SHIFT)
			| ((187500 * 1000UL / 4) << IBA7220_LEDBLINK_OFF_SHIFT);
	}
	if (lst == IB_PORT_ACTIVE)
		extctl |= SYM_MASK(EXTCtrl, LEDPriPortYellowOn);
	dd->cspec->extctrl = extctl;
	qib_write_kreg(dd, kr_extctrl, extctl);
	spin_unlock_irqrestore(&dd->cspec->gpio_lock, flags);

	if (ledblink) 
		qib_write_kreg(dd, kr_rcvpktledcnt, ledblink);
}

static void qib_7220_free_irq(struct qib_devdata *dd)
{
	if (dd->cspec->irq) {
		free_irq(dd->cspec->irq, dd);
		dd->cspec->irq = 0;
	}
	qib_nomsi(dd);
}

static void qib_setup_7220_cleanup(struct qib_devdata *dd)
{
	qib_7220_free_irq(dd);
	kfree(dd->cspec->cntrs);
	kfree(dd->cspec->portcntrs);
}

static void sdma_7220_intr(struct qib_pportdata *ppd, u64 istat)
{
	unsigned long flags;

	spin_lock_irqsave(&ppd->sdma_lock, flags);

	switch (ppd->sdma_state.current_state) {
	case qib_sdma_state_s00_hw_down:
		break;

	case qib_sdma_state_s10_hw_start_up_wait:
		__qib_sdma_process_event(ppd, qib_sdma_event_e20_hw_started);
		break;

	case qib_sdma_state_s20_idle:
		break;

	case qib_sdma_state_s30_sw_clean_up_wait:
		break;

	case qib_sdma_state_s40_hw_clean_up_wait:
		break;

	case qib_sdma_state_s50_hw_halt_wait:
		__qib_sdma_process_event(ppd, qib_sdma_event_e60_hw_halted);
		break;

	case qib_sdma_state_s99_running:
		
		__qib_sdma_intr(ppd);
		break;
	}
	spin_unlock_irqrestore(&ppd->sdma_lock, flags);
}

static void qib_wantpiobuf_7220_intr(struct qib_devdata *dd, u32 needint)
{
	unsigned long flags;

	spin_lock_irqsave(&dd->sendctrl_lock, flags);
	if (needint) {
		if (!(dd->sendctrl & SYM_MASK(SendCtrl, SendBufAvailUpd)))
			goto done;
		qib_write_kreg(dd, kr_sendctrl, dd->sendctrl &
			~SYM_MASK(SendCtrl, SendBufAvailUpd));
		qib_write_kreg(dd, kr_scratch, 0ULL);
		dd->sendctrl |= SYM_MASK(SendCtrl, SendIntBufAvail);
	} else
		dd->sendctrl &= ~SYM_MASK(SendCtrl, SendIntBufAvail);
	qib_write_kreg(dd, kr_sendctrl, dd->sendctrl);
	qib_write_kreg(dd, kr_scratch, 0ULL);
done:
	spin_unlock_irqrestore(&dd->sendctrl_lock, flags);
}

static noinline void unlikely_7220_intr(struct qib_devdata *dd, u64 istat)
{
	if (unlikely(istat & ~QLOGIC_IB_I_BITSEXTANT))
		qib_dev_err(dd,
			    "interrupt with unknown interrupts %Lx set\n",
			    istat & ~QLOGIC_IB_I_BITSEXTANT);

	if (istat & QLOGIC_IB_I_GPIO) {
		u32 gpiostatus;

		gpiostatus = qib_read_kreg32(dd, kr_gpio_status);
		qib_write_kreg(dd, kr_gpio_clear, gpiostatus);

		if (gpiostatus) {
			const u32 mask = qib_read_kreg32(dd, kr_gpio_mask);
			u32 gpio_irq = mask & gpiostatus;

			dd->cspec->gpio_mask &= ~gpio_irq;
			qib_write_kreg(dd, kr_gpio_mask, dd->cspec->gpio_mask);
		}
	}

	if (istat & QLOGIC_IB_I_ERROR) {
		u64 estat;

		qib_stats.sps_errints++;
		estat = qib_read_kreg64(dd, kr_errstatus);
		if (!estat)
			qib_devinfo(dd->pcidev, "error interrupt (%Lx), "
				 "but no error bits set!\n", istat);
		else
			handle_7220_errors(dd, estat);
	}
}

static irqreturn_t qib_7220intr(int irq, void *data)
{
	struct qib_devdata *dd = data;
	irqreturn_t ret;
	u64 istat;
	u64 ctxtrbits;
	u64 rmask;
	unsigned i;

	if ((dd->flags & (QIB_PRESENT | QIB_BADINTR)) != QIB_PRESENT) {
		ret = IRQ_HANDLED;
		goto bail;
	}

	istat = qib_read_kreg64(dd, kr_intstatus);

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
		unlikely_7220_intr(dd, istat);

	qib_write_kreg(dd, kr_intclear, istat);

	ctxtrbits = istat &
		((QLOGIC_IB_I_RCVAVAIL_MASK << QLOGIC_IB_I_RCVAVAIL_SHIFT) |
		 (QLOGIC_IB_I_RCVURG_MASK << QLOGIC_IB_I_RCVURG_SHIFT));
	if (ctxtrbits) {
		rmask = (1ULL << QLOGIC_IB_I_RCVAVAIL_SHIFT) |
			(1ULL << QLOGIC_IB_I_RCVURG_SHIFT);
		for (i = 0; i < dd->first_user_ctxt; i++) {
			if (ctxtrbits & rmask) {
				ctxtrbits &= ~rmask;
				qib_kreceive(dd->rcd[i], NULL, NULL);
			}
			rmask <<= 1;
		}
		if (ctxtrbits) {
			ctxtrbits =
				(ctxtrbits >> QLOGIC_IB_I_RCVAVAIL_SHIFT) |
				(ctxtrbits >> QLOGIC_IB_I_RCVURG_SHIFT);
			qib_handle_urcv(dd, ctxtrbits);
		}
	}

	
	if (istat & QLOGIC_IB_I_SDMAINT)
		sdma_7220_intr(dd->pport, istat);

	if ((istat & QLOGIC_IB_I_SPIOBUFAVAIL) && (dd->flags & QIB_INITTED))
		qib_ib_piobufavail(dd);

	ret = IRQ_HANDLED;
bail:
	return ret;
}

static void qib_setup_7220_interrupt(struct qib_devdata *dd)
{
	if (!dd->cspec->irq)
		qib_dev_err(dd, "irq is 0, BIOS error?  Interrupts won't "
			    "work\n");
	else {
		int ret = request_irq(dd->cspec->irq, qib_7220intr,
			dd->msi_lo ? 0 : IRQF_SHARED,
			QIB_DRV_NAME, dd);

		if (ret)
			qib_dev_err(dd, "Couldn't setup %s interrupt "
				    "(irq=%d): %d\n", dd->msi_lo ?
				    "MSI" : "INTx", dd->cspec->irq, ret);
	}
}

static void qib_7220_boardname(struct qib_devdata *dd)
{
	char *n;
	u32 boardid, namelen;

	boardid = SYM_FIELD(dd->revision, Revision,
			    BoardID);

	switch (boardid) {
	case 1:
		n = "InfiniPath_QLE7240";
		break;
	case 2:
		n = "InfiniPath_QLE7280";
		break;
	default:
		qib_dev_err(dd, "Unknown 7220 board with ID %u\n", boardid);
		n = "Unknown_InfiniPath_7220";
		break;
	}

	namelen = strlen(n) + 1;
	dd->boardname = kmalloc(namelen, GFP_KERNEL);
	if (!dd->boardname)
		qib_dev_err(dd, "Failed allocation for board name: %s\n", n);
	else
		snprintf(dd->boardname, namelen, "%s", n);

	if (dd->majrev != 5 || !dd->minrev || dd->minrev > 2)
		qib_dev_err(dd, "Unsupported InfiniPath hardware "
			    "revision %u.%u!\n",
			    dd->majrev, dd->minrev);

	snprintf(dd->boardversion, sizeof(dd->boardversion),
		 "ChipABI %u.%u, %s, InfiniPath%u %u.%u, SW Compat %u\n",
		 QIB_CHIP_VERS_MAJ, QIB_CHIP_VERS_MIN, dd->boardname,
		 (unsigned)SYM_FIELD(dd->revision, Revision_R, Arch),
		 dd->majrev, dd->minrev,
		 (unsigned)SYM_FIELD(dd->revision, Revision_R, SW));
}

static int qib_setup_7220_reset(struct qib_devdata *dd)
{
	u64 val;
	int i;
	int ret;
	u16 cmdval;
	u8 int_line, clinesz;
	unsigned long flags;

	qib_pcie_getcmd(dd, &cmdval, &int_line, &clinesz);

	
	qib_dev_err(dd, "Resetting InfiniPath unit %u\n", dd->unit);

	
	qib_7220_set_intr_state(dd, 0);

	dd->pport->cpspec->ibdeltainprog = 0;
	dd->pport->cpspec->ibsymdelta = 0;
	dd->pport->cpspec->iblnkerrdelta = 0;

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

		
		qib_write_kreg(dd, kr_control, 0ULL);

		
		qib_7220_init_hwerrors(dd);

		
		if (dd->pport->cpspec->ibcddrctrl & IBA7220_IBC_IBTA_1_2_MASK)
			dd->cspec->presets_needed = 1;
		spin_lock_irqsave(&dd->pport->lflags_lock, flags);
		dd->pport->lflags |= QIBL_IB_FORCE_NOTIFY;
		dd->pport->lflags &= ~QIBL_IB_AUTONEG_FAILED;
		spin_unlock_irqrestore(&dd->pport->lflags_lock, flags);
	}

	return ret;
}

static void qib_7220_put_tid(struct qib_devdata *dd, u64 __iomem *tidptr,
			     u32 type, unsigned long pa)
{
	if (pa != dd->tidinvalid) {
		u64 chippa = pa >> IBA7220_TID_PA_SHIFT;

		
		if (pa != (chippa << IBA7220_TID_PA_SHIFT)) {
			qib_dev_err(dd, "Physaddr %lx not 2KB aligned!\n",
				    pa);
			return;
		}
		if (chippa >= (1UL << IBA7220_TID_SZ_SHIFT)) {
			qib_dev_err(dd, "Physical page address 0x%lx "
				"larger than supported\n", pa);
			return;
		}

		if (type == RCVHQ_RCV_TYPE_EAGER)
			chippa |= dd->tidtemplate;
		else 
			chippa |= IBA7220_TID_SZ_4K;
		pa = chippa;
	}
	writeq(pa, tidptr);
	mmiowb();
}

static void qib_7220_clear_tids(struct qib_devdata *dd,
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
		qib_7220_put_tid(dd, &tidbase[i], RCVHQ_RCV_TYPE_EXPECTED,
				 tidinv);

	tidbase = (u64 __iomem *)
		((char __iomem *)(dd->kregbase) +
		 dd->rcvegrbase +
		 rcd->rcvegr_tid_base * sizeof(*tidbase));

	for (i = 0; i < rcd->rcvegrcnt; i++)
		qib_7220_put_tid(dd, &tidbase[i], RCVHQ_RCV_TYPE_EAGER,
				 tidinv);
}

static void qib_7220_tidtemplate(struct qib_devdata *dd)
{
	if (dd->rcvegrbufsize == 2048)
		dd->tidtemplate = IBA7220_TID_SZ_2K;
	else if (dd->rcvegrbufsize == 4096)
		dd->tidtemplate = IBA7220_TID_SZ_4K;
	dd->tidinvalid = 0;
}

static int qib_7220_get_base_info(struct qib_ctxtdata *rcd,
				  struct qib_base_info *kinfo)
{
	kinfo->spi_runtime_flags |= QIB_RUNTIME_PCIE |
		QIB_RUNTIME_NODMA_RTAIL | QIB_RUNTIME_SDMA;

	if (rcd->dd->flags & QIB_USE_SPCL_TRIG)
		kinfo->spi_runtime_flags |= QIB_RUNTIME_SPECIAL_TRIGGER;

	return 0;
}

static struct qib_message_header *
qib_7220_get_msgheader(struct qib_devdata *dd, __le32 *rhf_addr)
{
	u32 offset = qib_hdrget_offset(rhf_addr);

	return (struct qib_message_header *)
		(rhf_addr - dd->rhf_offset + offset);
}

static void qib_7220_config_ctxts(struct qib_devdata *dd)
{
	unsigned long flags;
	u32 nchipctxts;

	nchipctxts = qib_read_kreg32(dd, kr_portcnt);
	dd->cspec->numctxts = nchipctxts;
	if (qib_n_krcv_queues > 1) {
		dd->qpn_mask = 0x3e;
		dd->first_user_ctxt = qib_n_krcv_queues * dd->num_pports;
		if (dd->first_user_ctxt > nchipctxts)
			dd->first_user_ctxt = nchipctxts;
	} else
		dd->first_user_ctxt = dd->num_pports;
	dd->n_krcv_queues = dd->first_user_ctxt;

	if (!qib_cfgctxts) {
		int nctxts = dd->first_user_ctxt + num_online_cpus();

		if (nctxts <= 5)
			dd->ctxtcnt = 5;
		else if (nctxts <= 9)
			dd->ctxtcnt = 9;
		else if (nctxts <= nchipctxts)
			dd->ctxtcnt = nchipctxts;
	} else if (qib_cfgctxts <= nchipctxts)
		dd->ctxtcnt = qib_cfgctxts;
	if (!dd->ctxtcnt) 
		dd->ctxtcnt = nchipctxts;

	spin_lock_irqsave(&dd->cspec->rcvmod_lock, flags);
	if (dd->ctxtcnt > 9)
		dd->rcvctrl |= 2ULL << IBA7220_R_CTXTCFG_SHIFT;
	else if (dd->ctxtcnt > 5)
		dd->rcvctrl |= 1ULL << IBA7220_R_CTXTCFG_SHIFT;
	
	if (dd->qpn_mask)
		dd->rcvctrl |= 1ULL << QIB_7220_RcvCtrl_RcvQPMapEnable_LSB;
	qib_write_kreg(dd, kr_rcvctrl, dd->rcvctrl);
	spin_unlock_irqrestore(&dd->cspec->rcvmod_lock, flags);

	
	dd->cspec->rcvegrcnt = qib_read_kreg32(dd, kr_rcvegrcnt);
	dd->rcvhdrcnt = max(dd->cspec->rcvegrcnt, IBA7220_KRCVEGRCNT);
}

static int qib_7220_get_ib_cfg(struct qib_pportdata *ppd, int which)
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
		lsb = IBA7220_IBC_RXPOL_SHIFT;
		maskr = IBA7220_IBC_RXPOL_MASK;
		break;

	case QIB_IB_CFG_LREV_ENB: 
		lsb = IBA7220_IBC_LREV_SHIFT;
		maskr = IBA7220_IBC_LREV_MASK;
		break;

	case QIB_IB_CFG_LINKLATENCY:
		ret = qib_read_kreg64(ppd->dd, kr_ibcddrstatus)
			& IBA7220_DDRSTAT_LINKLAT_MASK;
		goto done;

	case QIB_IB_CFG_OP_VLS:
		ret = ppd->vls_operational;
		goto done;

	case QIB_IB_CFG_VL_HIGH_CAP:
		ret = 0;
		goto done;

	case QIB_IB_CFG_VL_LOW_CAP:
		ret = 0;
		goto done;

	case QIB_IB_CFG_OVERRUN_THRESH: 
		ret = SYM_FIELD(ppd->cpspec->ibcctrl, IBCCtrl,
				OverrunThreshold);
		goto done;

	case QIB_IB_CFG_PHYERR_THRESH: 
		ret = SYM_FIELD(ppd->cpspec->ibcctrl, IBCCtrl,
				PhyerrThreshold);
		goto done;

	case QIB_IB_CFG_LINKDEFAULT: 
		
		ret = (ppd->cpspec->ibcctrl &
		       SYM_MASK(IBCCtrl, LinkDownDefaultState)) ?
			IB_LINKINITCMD_SLEEP : IB_LINKINITCMD_POLL;
		goto done;

	case QIB_IB_CFG_HRTBT: 
		lsb = IBA7220_IBC_HRTBT_SHIFT;
		maskr = IBA7220_IBC_HRTBT_MASK;
		break;

	case QIB_IB_CFG_PMA_TICKS:
		ret = (ppd->link_speed_active == QIB_IB_DDR);
		goto done;

	default:
		ret = -EINVAL;
		goto done;
	}
	ret = (int)((ppd->cpspec->ibcddrctrl >> lsb) & maskr);
done:
	return ret;
}

static int qib_7220_set_ib_cfg(struct qib_pportdata *ppd, int which, u32 val)
{
	struct qib_devdata *dd = ppd->dd;
	u64 maskr; 
	int lsb, ret = 0, setforce = 0;
	u16 lcmd, licmd;
	unsigned long flags;
	u32 tmp = 0;

	switch (which) {
	case QIB_IB_CFG_LIDLMC:
		lsb = IBA7220_IBC_DLIDLMC_SHIFT;
		maskr = IBA7220_IBC_DLIDLMC_MASK;
		break;

	case QIB_IB_CFG_LWID_ENB: 
		ppd->link_width_enabled = val;
		if (!(ppd->lflags & QIBL_LINKDOWN))
			goto bail;
		val--; 
		maskr = IBA7220_IBC_WIDTH_MASK;
		lsb = IBA7220_IBC_WIDTH_SHIFT;
		setforce = 1;
		break;

	case QIB_IB_CFG_SPD_ENB: 
		ppd->link_speed_enabled = val;
		if ((ppd->cpspec->ibcddrctrl & IBA7220_IBC_IBTA_1_2_MASK) &&
		    !(val & (val - 1)))
			dd->cspec->presets_needed = 1;
		if (!(ppd->lflags & QIBL_LINKDOWN))
			goto bail;
		if (val == (QIB_IB_SDR | QIB_IB_DDR)) {
			val = IBA7220_IBC_SPEED_AUTONEG_MASK |
				IBA7220_IBC_IBTA_1_2_MASK;
			spin_lock_irqsave(&ppd->lflags_lock, flags);
			ppd->lflags &= ~QIBL_IB_AUTONEG_FAILED;
			spin_unlock_irqrestore(&ppd->lflags_lock, flags);
		} else
			val = val == QIB_IB_DDR ?
				IBA7220_IBC_SPEED_DDR : IBA7220_IBC_SPEED_SDR;
		maskr = IBA7220_IBC_SPEED_AUTONEG_MASK |
			IBA7220_IBC_IBTA_1_2_MASK;
		
		lsb = SYM_LSB(IBCDDRCtrl, IB_ENHANCED_MODE);
		setforce = 1;
		break;

	case QIB_IB_CFG_RXPOL_ENB: 
		lsb = IBA7220_IBC_RXPOL_SHIFT;
		maskr = IBA7220_IBC_RXPOL_MASK;
		break;

	case QIB_IB_CFG_LREV_ENB: 
		lsb = IBA7220_IBC_LREV_SHIFT;
		maskr = IBA7220_IBC_LREV_MASK;
		break;

	case QIB_IB_CFG_OVERRUN_THRESH: 
		maskr = SYM_FIELD(ppd->cpspec->ibcctrl, IBCCtrl,
				  OverrunThreshold);
		if (maskr != val) {
			ppd->cpspec->ibcctrl &=
				~SYM_MASK(IBCCtrl, OverrunThreshold);
			ppd->cpspec->ibcctrl |= (u64) val <<
				SYM_LSB(IBCCtrl, OverrunThreshold);
			qib_write_kreg(dd, kr_ibcctrl, ppd->cpspec->ibcctrl);
			qib_write_kreg(dd, kr_scratch, 0);
		}
		goto bail;

	case QIB_IB_CFG_PHYERR_THRESH: 
		maskr = SYM_FIELD(ppd->cpspec->ibcctrl, IBCCtrl,
				  PhyerrThreshold);
		if (maskr != val) {
			ppd->cpspec->ibcctrl &=
				~SYM_MASK(IBCCtrl, PhyerrThreshold);
			ppd->cpspec->ibcctrl |= (u64) val <<
				SYM_LSB(IBCCtrl, PhyerrThreshold);
			qib_write_kreg(dd, kr_ibcctrl, ppd->cpspec->ibcctrl);
			qib_write_kreg(dd, kr_scratch, 0);
		}
		goto bail;

	case QIB_IB_CFG_PKEYS: 
		maskr = (u64) ppd->pkeys[0] | ((u64) ppd->pkeys[1] << 16) |
			((u64) ppd->pkeys[2] << 32) |
			((u64) ppd->pkeys[3] << 48);
		qib_write_kreg(dd, kr_partitionkey, maskr);
		goto bail;

	case QIB_IB_CFG_LINKDEFAULT: 
		
		if (val == IB_LINKINITCMD_POLL)
			ppd->cpspec->ibcctrl &=
				~SYM_MASK(IBCCtrl, LinkDownDefaultState);
		else 
			ppd->cpspec->ibcctrl |=
				SYM_MASK(IBCCtrl, LinkDownDefaultState);
		qib_write_kreg(dd, kr_ibcctrl, ppd->cpspec->ibcctrl);
		qib_write_kreg(dd, kr_scratch, 0);
		goto bail;

	case QIB_IB_CFG_MTU: 
		val = (ppd->ibmaxlen >> 2) + 1;
		ppd->cpspec->ibcctrl &= ~SYM_MASK(IBCCtrl, MaxPktLen);
		ppd->cpspec->ibcctrl |= (u64)val << SYM_LSB(IBCCtrl, MaxPktLen);
		qib_write_kreg(dd, kr_ibcctrl, ppd->cpspec->ibcctrl);
		qib_write_kreg(dd, kr_scratch, 0);
		goto bail;

	case QIB_IB_CFG_LSTATE: 
		switch (val & 0xffff0000) {
		case IB_LINKCMD_DOWN:
			lcmd = QLOGIC_IB_IBCC_LINKCMD_DOWN;
			if (!ppd->cpspec->ibdeltainprog &&
			    qib_compat_ddr_negotiate) {
				ppd->cpspec->ibdeltainprog = 1;
				ppd->cpspec->ibsymsnap =
					read_7220_creg32(dd, cr_ibsymbolerr);
				ppd->cpspec->iblnkerrsnap =
					read_7220_creg32(dd, cr_iblinkerrrecov);
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
		qib_set_ib_7220_lstate(ppd, lcmd, licmd);

		maskr = IBA7220_IBC_WIDTH_MASK;
		lsb = IBA7220_IBC_WIDTH_SHIFT;
		tmp = (ppd->cpspec->ibcddrctrl >> lsb) & maskr;
		if (ppd->link_width_enabled-1 != tmp) {
			ppd->cpspec->ibcddrctrl &= ~(maskr << lsb);
			ppd->cpspec->ibcddrctrl |=
				(((u64)(ppd->link_width_enabled-1) & maskr) <<
				 lsb);
			qib_write_kreg(dd, kr_ibcddrctrl,
				       ppd->cpspec->ibcddrctrl);
			qib_write_kreg(dd, kr_scratch, 0);
			spin_lock_irqsave(&ppd->lflags_lock, flags);
			ppd->lflags |= QIBL_IB_FORCE_NOTIFY;
			spin_unlock_irqrestore(&ppd->lflags_lock, flags);
		}
		goto bail;

	case QIB_IB_CFG_HRTBT: 
		if (val > IBA7220_IBC_HRTBT_MASK) {
			ret = -EINVAL;
			goto bail;
		}
		lsb = IBA7220_IBC_HRTBT_SHIFT;
		maskr = IBA7220_IBC_HRTBT_MASK;
		break;

	default:
		ret = -EINVAL;
		goto bail;
	}
	ppd->cpspec->ibcddrctrl &= ~(maskr << lsb);
	ppd->cpspec->ibcddrctrl |= (((u64) val & maskr) << lsb);
	qib_write_kreg(dd, kr_ibcddrctrl, ppd->cpspec->ibcddrctrl);
	qib_write_kreg(dd, kr_scratch, 0);
	if (setforce) {
		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags |= QIBL_IB_FORCE_NOTIFY;
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	}
bail:
	return ret;
}

static int qib_7220_set_loopback(struct qib_pportdata *ppd, const char *what)
{
	int ret = 0;
	u64 val, ddr;

	if (!strncmp(what, "ibc", 3)) {
		ppd->cpspec->ibcctrl |= SYM_MASK(IBCCtrl, Loopback);
		val = 0; 
		qib_devinfo(ppd->dd->pcidev, "Enabling IB%u:%u IBC loopback\n",
			 ppd->dd->unit, ppd->port);
	} else if (!strncmp(what, "off", 3)) {
		ppd->cpspec->ibcctrl &= ~SYM_MASK(IBCCtrl, Loopback);
		
		val = IBA7220_IBC_HRTBT_MASK << IBA7220_IBC_HRTBT_SHIFT;
		qib_devinfo(ppd->dd->pcidev, "Disabling IB%u:%u IBC loopback "
			    "(normal)\n", ppd->dd->unit, ppd->port);
	} else
		ret = -EINVAL;
	if (!ret) {
		qib_write_kreg(ppd->dd, kr_ibcctrl, ppd->cpspec->ibcctrl);
		ddr = ppd->cpspec->ibcddrctrl & ~(IBA7220_IBC_HRTBT_MASK
					     << IBA7220_IBC_HRTBT_SHIFT);
		ppd->cpspec->ibcddrctrl = ddr | val;
		qib_write_kreg(ppd->dd, kr_ibcddrctrl,
			       ppd->cpspec->ibcddrctrl);
		qib_write_kreg(ppd->dd, kr_scratch, 0);
	}
	return ret;
}

static void qib_update_7220_usrhead(struct qib_ctxtdata *rcd, u64 hd,
				    u32 updegr, u32 egrhd, u32 npkts)
{
	if (updegr)
		qib_write_ureg(rcd->dd, ur_rcvegrindexhead, egrhd, rcd->ctxt);
	mmiowb();
	qib_write_ureg(rcd->dd, ur_rcvhdrhead, hd, rcd->ctxt);
	mmiowb();
}

static u32 qib_7220_hdrqempty(struct qib_ctxtdata *rcd)
{
	u32 head, tail;

	head = qib_read_ureg32(rcd->dd, ur_rcvhdrhead, rcd->ctxt);
	if (rcd->rcvhdrtail_kvaddr)
		tail = qib_get_rcvhdrtail(rcd);
	else
		tail = qib_read_ureg32(rcd->dd, ur_rcvhdrtail, rcd->ctxt);
	return head == tail;
}

static void rcvctrl_7220_mod(struct qib_pportdata *ppd, unsigned int op,
			     int ctxt)
{
	struct qib_devdata *dd = ppd->dd;
	u64 mask, val;
	unsigned long flags;

	spin_lock_irqsave(&dd->cspec->rcvmod_lock, flags);
	if (op & QIB_RCVCTRL_TAILUPD_ENB)
		dd->rcvctrl |= (1ULL << IBA7220_R_TAILUPD_SHIFT);
	if (op & QIB_RCVCTRL_TAILUPD_DIS)
		dd->rcvctrl &= ~(1ULL << IBA7220_R_TAILUPD_SHIFT);
	if (op & QIB_RCVCTRL_PKEY_ENB)
		dd->rcvctrl &= ~(1ULL << IBA7220_R_PKEY_DIS_SHIFT);
	if (op & QIB_RCVCTRL_PKEY_DIS)
		dd->rcvctrl |= (1ULL << IBA7220_R_PKEY_DIS_SHIFT);
	if (ctxt < 0)
		mask = (1ULL << dd->ctxtcnt) - 1;
	else
		mask = (1ULL << ctxt);
	if (op & QIB_RCVCTRL_CTXT_ENB) {
		
		dd->rcvctrl |= (mask << SYM_LSB(RcvCtrl, PortEnable));
		if (!(dd->flags & QIB_NODMA_RTAIL))
			dd->rcvctrl |= 1ULL << IBA7220_R_TAILUPD_SHIFT;
		
		qib_write_kreg_ctxt(dd, kr_rcvhdrtailaddr, ctxt,
			dd->rcd[ctxt]->rcvhdrqtailaddr_phys);
		qib_write_kreg_ctxt(dd, kr_rcvhdraddr, ctxt,
			dd->rcd[ctxt]->rcvhdrq_phys);
		dd->rcd[ctxt]->seq_cnt = 1;
	}
	if (op & QIB_RCVCTRL_CTXT_DIS)
		dd->rcvctrl &= ~(mask << SYM_LSB(RcvCtrl, PortEnable));
	if (op & QIB_RCVCTRL_INTRAVAIL_ENB)
		dd->rcvctrl |= (mask << IBA7220_R_INTRAVAIL_SHIFT);
	if (op & QIB_RCVCTRL_INTRAVAIL_DIS)
		dd->rcvctrl &= ~(mask << IBA7220_R_INTRAVAIL_SHIFT);
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
			qib_write_kreg_ctxt(dd, kr_rcvhdrtailaddr, ctxt, 0);
			qib_write_kreg_ctxt(dd, kr_rcvhdraddr, ctxt, 0);
		} else {
			unsigned i;

			for (i = 0; i < dd->cfgctxts; i++) {
				qib_write_kreg_ctxt(dd, kr_rcvhdrtailaddr,
						    i, 0);
				qib_write_kreg_ctxt(dd, kr_rcvhdraddr, i, 0);
			}
		}
	}
	spin_unlock_irqrestore(&dd->cspec->rcvmod_lock, flags);
}

static void sendctrl_7220_mod(struct qib_pportdata *ppd, u32 op)
{
	struct qib_devdata *dd = ppd->dd;
	u64 tmp_dd_sendctrl;
	unsigned long flags;

	spin_lock_irqsave(&dd->sendctrl_lock, flags);

	
	if (op & QIB_SENDCTRL_CLEAR)
		dd->sendctrl = 0;
	if (op & QIB_SENDCTRL_SEND_DIS)
		dd->sendctrl &= ~SYM_MASK(SendCtrl, SPioEnable);
	else if (op & QIB_SENDCTRL_SEND_ENB) {
		dd->sendctrl |= SYM_MASK(SendCtrl, SPioEnable);
		if (dd->flags & QIB_USE_SPCL_TRIG)
			dd->sendctrl |= SYM_MASK(SendCtrl,
						 SSpecialTriggerEn);
	}
	if (op & QIB_SENDCTRL_AVAIL_DIS)
		dd->sendctrl &= ~SYM_MASK(SendCtrl, SendBufAvailUpd);
	else if (op & QIB_SENDCTRL_AVAIL_ENB)
		dd->sendctrl |= SYM_MASK(SendCtrl, SendBufAvailUpd);

	if (op & QIB_SENDCTRL_DISARM_ALL) {
		u32 i, last;

		tmp_dd_sendctrl = dd->sendctrl;
		last = dd->piobcnt2k + dd->piobcnt4k;
		tmp_dd_sendctrl &=
			~(SYM_MASK(SendCtrl, SPioEnable) |
			  SYM_MASK(SendCtrl, SendBufAvailUpd));
		for (i = 0; i < last; i++) {
			qib_write_kreg(dd, kr_sendctrl,
				       tmp_dd_sendctrl |
				       SYM_MASK(SendCtrl, Disarm) | i);
			qib_write_kreg(dd, kr_scratch, 0);
		}
	}

	tmp_dd_sendctrl = dd->sendctrl;

	if (op & QIB_SENDCTRL_FLUSH)
		tmp_dd_sendctrl |= SYM_MASK(SendCtrl, Abort);
	if (op & QIB_SENDCTRL_DISARM)
		tmp_dd_sendctrl |= SYM_MASK(SendCtrl, Disarm) |
			((op & QIB_7220_SendCtrl_DisarmPIOBuf_RMASK) <<
			 SYM_LSB(SendCtrl, DisarmPIOBuf));
	if ((op & QIB_SENDCTRL_AVAIL_BLIP) &&
	    (dd->sendctrl & SYM_MASK(SendCtrl, SendBufAvailUpd)))
		tmp_dd_sendctrl &= ~SYM_MASK(SendCtrl, SendBufAvailUpd);

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

static u64 qib_portcntr_7220(struct qib_pportdata *ppd, u32 reg)
{
	u64 ret = 0ULL;
	struct qib_devdata *dd = ppd->dd;
	u16 creg;
	
	static const u16 xlator[] = {
		[QIBPORTCNTR_PKTSEND] = cr_pktsend,
		[QIBPORTCNTR_WORDSEND] = cr_wordsend,
		[QIBPORTCNTR_PSXMITDATA] = cr_psxmitdatacount,
		[QIBPORTCNTR_PSXMITPKTS] = cr_psxmitpktscount,
		[QIBPORTCNTR_PSXMITWAIT] = cr_psxmitwaitcount,
		[QIBPORTCNTR_SENDSTALL] = cr_sendstall,
		[QIBPORTCNTR_PKTRCV] = cr_pktrcv,
		[QIBPORTCNTR_PSRCVDATA] = cr_psrcvdatacount,
		[QIBPORTCNTR_PSRCVPKTS] = cr_psrcvpktscount,
		[QIBPORTCNTR_RCVEBP] = cr_rcvebp,
		[QIBPORTCNTR_RCVOVFL] = cr_rcvovfl,
		[QIBPORTCNTR_WORDRCV] = cr_wordrcv,
		[QIBPORTCNTR_RXDROPPKT] = cr_rxdroppkt,
		[QIBPORTCNTR_RXLOCALPHYERR] = cr_rxotherlocalphyerr,
		[QIBPORTCNTR_RXVLERR] = cr_rxvlerr,
		[QIBPORTCNTR_ERRICRC] = cr_erricrc,
		[QIBPORTCNTR_ERRVCRC] = cr_errvcrc,
		[QIBPORTCNTR_ERRLPCRC] = cr_errlpcrc,
		[QIBPORTCNTR_BADFORMAT] = cr_badformat,
		[QIBPORTCNTR_ERR_RLEN] = cr_err_rlen,
		[QIBPORTCNTR_IBSYMBOLERR] = cr_ibsymbolerr,
		[QIBPORTCNTR_INVALIDRLEN] = cr_invalidrlen,
		[QIBPORTCNTR_UNSUPVL] = cr_txunsupvl,
		[QIBPORTCNTR_EXCESSBUFOVFL] = cr_excessbufferovfl,
		[QIBPORTCNTR_ERRLINK] = cr_errlink,
		[QIBPORTCNTR_IBLINKDOWN] = cr_iblinkdown,
		[QIBPORTCNTR_IBLINKERRRECOV] = cr_iblinkerrrecov,
		[QIBPORTCNTR_LLI] = cr_locallinkintegrityerr,
		[QIBPORTCNTR_PSINTERVAL] = cr_psinterval,
		[QIBPORTCNTR_PSSTART] = cr_psstart,
		[QIBPORTCNTR_PSSTAT] = cr_psstat,
		[QIBPORTCNTR_VL15PKTDROP] = cr_vl15droppedpkt,
		[QIBPORTCNTR_ERRPKEY] = cr_errpkey,
		[QIBPORTCNTR_KHDROVFL] = 0xffff,
	};

	if (reg >= ARRAY_SIZE(xlator)) {
		qib_devinfo(ppd->dd->pcidev,
			 "Unimplemented portcounter %u\n", reg);
		goto done;
	}
	creg = xlator[reg];

	if (reg == QIBPORTCNTR_KHDROVFL) {
		int i;

		
		for (i = 0; i < dd->first_user_ctxt; i++)
			ret += read_7220_creg32(dd, cr_portovfl + i);
	}
	if (creg == 0xffff)
		goto done;

	if ((creg == cr_wordsend || creg == cr_wordrcv ||
	     creg == cr_pktsend || creg == cr_pktrcv))
		ret = read_7220_creg(dd, creg);
	else
		ret = read_7220_creg32(dd, creg);
	if (creg == cr_ibsymbolerr) {
		if (dd->pport->cpspec->ibdeltainprog)
			ret -= ret - ppd->cpspec->ibsymsnap;
		ret -= dd->pport->cpspec->ibsymdelta;
	} else if (creg == cr_iblinkerrrecov) {
		if (dd->pport->cpspec->ibdeltainprog)
			ret -= ret - ppd->cpspec->iblnkerrsnap;
		ret -= dd->pport->cpspec->iblnkerrdelta;
	}
done:
	return ret;
}

static const char cntr7220names[] =
	"Interrupts\n"
	"HostBusStall\n"
	"E RxTIDFull\n"
	"RxTIDInvalid\n"
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
	"Ctx16EgrOvfl\n";

static const size_t cntr7220indices[] = {
	cr_lbint,
	cr_lbflowstall,
	cr_errtidfull,
	cr_errtidvalid,
	cr_portovfl + 0,
	cr_portovfl + 1,
	cr_portovfl + 2,
	cr_portovfl + 3,
	cr_portovfl + 4,
	cr_portovfl + 5,
	cr_portovfl + 6,
	cr_portovfl + 7,
	cr_portovfl + 8,
	cr_portovfl + 9,
	cr_portovfl + 10,
	cr_portovfl + 11,
	cr_portovfl + 12,
	cr_portovfl + 13,
	cr_portovfl + 14,
	cr_portovfl + 15,
	cr_portovfl + 16,
};

static const char portcntr7220names[] =
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
	;

#define _PORT_VIRT_FLAG 0x8000 
static const size_t portcntr7220indices[] = {
	QIBPORTCNTR_PKTSEND | _PORT_VIRT_FLAG,
	cr_pktsendflow,
	QIBPORTCNTR_WORDSEND | _PORT_VIRT_FLAG,
	QIBPORTCNTR_PKTRCV | _PORT_VIRT_FLAG,
	cr_pktrcvflowctrl,
	QIBPORTCNTR_WORDRCV | _PORT_VIRT_FLAG,
	QIBPORTCNTR_SENDSTALL | _PORT_VIRT_FLAG,
	cr_txsdmadesc,
	cr_rxdlidfltr,
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
	QIBPORTCNTR_RXLOCALPHYERR | _PORT_VIRT_FLAG,
	QIBPORTCNTR_VL15PKTDROP | _PORT_VIRT_FLAG,
	QIBPORTCNTR_RXVLERR | _PORT_VIRT_FLAG,
	QIBPORTCNTR_EXCESSBUFOVFL | _PORT_VIRT_FLAG,
};

static void init_7220_cntrnames(struct qib_devdata *dd)
{
	int i, j = 0;
	char *s;

	for (i = 0, s = (char *)cntr7220names; s && j <= dd->cfgctxts;
	     i++) {
		
		if (!j && !strncmp("Ctxt0EgrOvfl", s + 1, 12))
			j = 1;
		s = strchr(s + 1, '\n');
		if (s && j)
			j++;
	}
	dd->cspec->ncntrs = i;
	if (!s)
		
		dd->cspec->cntrnamelen = sizeof(cntr7220names) - 1;
	else
		dd->cspec->cntrnamelen = 1 + s - cntr7220names;
	dd->cspec->cntrs = kmalloc(dd->cspec->ncntrs
		* sizeof(u64), GFP_KERNEL);
	if (!dd->cspec->cntrs)
		qib_dev_err(dd, "Failed allocation for counters\n");

	for (i = 0, s = (char *)portcntr7220names; s; i++)
		s = strchr(s + 1, '\n');
	dd->cspec->nportcntrs = i - 1;
	dd->cspec->portcntrnamelen = sizeof(portcntr7220names) - 1;
	dd->cspec->portcntrs = kmalloc(dd->cspec->nportcntrs
		* sizeof(u64), GFP_KERNEL);
	if (!dd->cspec->portcntrs)
		qib_dev_err(dd, "Failed allocation for portcounters\n");
}

static u32 qib_read_7220cntrs(struct qib_devdata *dd, loff_t pos, char **namep,
			      u64 **cntrp)
{
	u32 ret;

	if (!dd->cspec->cntrs) {
		ret = 0;
		goto done;
	}

	if (namep) {
		*namep = (char *)cntr7220names;
		ret = dd->cspec->cntrnamelen;
		if (pos >= ret)
			ret = 0; 
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
			*cntr++ = read_7220_creg32(dd, cntr7220indices[i]);
	}
done:
	return ret;
}

static u32 qib_read_7220portcntrs(struct qib_devdata *dd, loff_t pos, u32 port,
				  char **namep, u64 **cntrp)
{
	u32 ret;

	if (!dd->cspec->portcntrs) {
		ret = 0;
		goto done;
	}
	if (namep) {
		*namep = (char *)portcntr7220names;
		ret = dd->cspec->portcntrnamelen;
		if (pos >= ret)
			ret = 0; 
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
			if (portcntr7220indices[i] & _PORT_VIRT_FLAG)
				*cntr++ = qib_portcntr_7220(ppd,
					portcntr7220indices[i] &
					~_PORT_VIRT_FLAG);
			else
				*cntr++ = read_7220_creg32(dd,
					   portcntr7220indices[i]);
		}
	}
done:
	return ret;
}

static void qib_get_7220_faststats(unsigned long opaque)
{
	struct qib_devdata *dd = (struct qib_devdata *) opaque;
	struct qib_pportdata *ppd = dd->pport;
	unsigned long flags;
	u64 traffic_wds;

	if (!(dd->flags & QIB_INITTED) || dd->diag_client)
		
		goto done;

	traffic_wds = qib_portcntr_7220(ppd, cr_wordsend) +
		qib_portcntr_7220(ppd, cr_wordrcv);
	spin_lock_irqsave(&dd->eep_st_lock, flags);
	traffic_wds -= dd->traffic_wds;
	dd->traffic_wds += traffic_wds;
	if (traffic_wds  >= QIB_TRAFFIC_ACTIVE_THRESHOLD)
		atomic_add(5, &dd->active_time); 
	spin_unlock_irqrestore(&dd->eep_st_lock, flags);
done:
	mod_timer(&dd->stats_timer, jiffies + HZ * ACTIVITY_TIMER);
}

static int qib_7220_intr_fallback(struct qib_devdata *dd)
{
	if (!dd->msi_lo)
		return 0;

	qib_devinfo(dd->pcidev, "MSI interrupt not detected,"
		 " trying INTx interrupts\n");
	qib_7220_free_irq(dd);
	qib_enable_intx(dd->pcidev);
	dd->cspec->irq = dd->pcidev->irq;
	qib_setup_7220_interrupt(dd);
	return 1;
}

static void qib_7220_xgxs_reset(struct qib_pportdata *ppd)
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

static u32 __iomem *get_7220_link_buf(struct qib_pportdata *ppd, u32 *bnum)
{
	u32 __iomem *buf;
	u32 lbuf = ppd->dd->cspec->lastbuf_for_pio;
	int do_cleanup;
	unsigned long flags;

	sendctrl_7220_mod(ppd->dd->pport, QIB_SENDCTRL_AVAIL_BLIP);
	qib_read_kreg64(ppd->dd, kr_scratch); 
	buf = qib_getsendbuf_range(ppd->dd, bnum, lbuf, lbuf);
	if (buf)
		goto done;

	spin_lock_irqsave(&ppd->sdma_lock, flags);
	if (ppd->sdma_state.current_state == qib_sdma_state_s20_idle &&
	    ppd->sdma_state.current_state != qib_sdma_state_s00_hw_down) {
		__qib_sdma_process_event(ppd, qib_sdma_event_e00_go_hw_down);
		do_cleanup = 0;
	} else {
		do_cleanup = 1;
		qib_7220_sdma_hw_clean_up(ppd);
	}
	spin_unlock_irqrestore(&ppd->sdma_lock, flags);

	if (do_cleanup) {
		qib_read_kreg64(ppd->dd, kr_scratch); 
		buf = qib_getsendbuf_range(ppd->dd, bnum, lbuf, lbuf);
	}
done:
	return buf;
}

static void autoneg_7220_sendpkt(struct qib_pportdata *ppd, u32 *hdr,
				 u32 dcnt, u32 *data)
{
	int i;
	u64 pbc;
	u32 __iomem *piobuf;
	u32 pnum;
	struct qib_devdata *dd = ppd->dd;

	i = 0;
	pbc = 7 + dcnt + 1; 
	pbc |= PBC_7220_VL15_SEND;
	while (!(piobuf = get_7220_link_buf(ppd, &pnum))) {
		if (i++ > 5)
			return;
		udelay(2);
	}
	sendctrl_7220_mod(dd->pport, QIB_SENDCTRL_DISARM_BUF(pnum));
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
}

static void autoneg_7220_send(struct qib_pportdata *ppd, int which)
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

	autoneg_7220_sendpkt(ppd, hdr, dcnt, data);
	qib_read_kreg64(dd, kr_scratch);
	udelay(2);
	autoneg_7220_sendpkt(ppd, hdr, dcnt, data);
	qib_read_kreg64(dd, kr_scratch);
	udelay(2);
}

static void set_7220_ibspeed_fast(struct qib_pportdata *ppd, u32 speed)
{
	ppd->cpspec->ibcddrctrl &= ~(IBA7220_IBC_SPEED_AUTONEG_MASK |
		IBA7220_IBC_IBTA_1_2_MASK);

	if (speed == (QIB_IB_SDR | QIB_IB_DDR))
		ppd->cpspec->ibcddrctrl |= IBA7220_IBC_SPEED_AUTONEG_MASK |
			IBA7220_IBC_IBTA_1_2_MASK;
	else
		ppd->cpspec->ibcddrctrl |= speed == QIB_IB_DDR ?
			IBA7220_IBC_SPEED_DDR : IBA7220_IBC_SPEED_SDR;

	qib_write_kreg(ppd->dd, kr_ibcddrctrl, ppd->cpspec->ibcddrctrl);
	qib_write_kreg(ppd->dd, kr_scratch, 0);
}

static void try_7220_autoneg(struct qib_pportdata *ppd)
{
	unsigned long flags;

	qib_write_kreg(ppd->dd, kr_ncmodectrl, 0x3b9dc07);

	spin_lock_irqsave(&ppd->lflags_lock, flags);
	ppd->lflags |= QIBL_IB_AUTONEG_INPROG;
	spin_unlock_irqrestore(&ppd->lflags_lock, flags);
	autoneg_7220_send(ppd, 0);
	set_7220_ibspeed_fast(ppd, QIB_IB_DDR);

	toggle_7220_rclkrls(ppd->dd);
	
	queue_delayed_work(ib_wq, &ppd->cpspec->autoneg_work,
			   msecs_to_jiffies(2));
}

static void autoneg_7220_work(struct work_struct *work)
{
	struct qib_pportdata *ppd;
	struct qib_devdata *dd;
	u64 startms;
	u32 i;
	unsigned long flags;

	ppd = &container_of(work, struct qib_chippport_specific,
			    autoneg_work.work)->pportdata;
	dd = ppd->dd;

	startms = jiffies_to_msecs(jiffies);

	for (i = 0; i < 25; i++) {
		if (SYM_FIELD(ppd->lastibcstat, IBCStatus, LinkTrainingState)
		     == IB_7220_LT_STATE_POLLQUIET) {
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

	toggle_7220_rclkrls(dd);

	
	if (wait_event_timeout(ppd->cpspec->autoneg_wait,
			       !(ppd->lflags & QIBL_IB_AUTONEG_INPROG),
			       msecs_to_jiffies(1700)))
		goto done;

	set_7220_ibspeed_fast(ppd, QIB_IB_SDR);
	toggle_7220_rclkrls(dd);

	wait_event_timeout(ppd->cpspec->autoneg_wait,
		!(ppd->lflags & QIBL_IB_AUTONEG_INPROG),
		msecs_to_jiffies(250));
done:
	if (ppd->lflags & QIBL_IB_AUTONEG_INPROG) {
		spin_lock_irqsave(&ppd->lflags_lock, flags);
		ppd->lflags &= ~QIBL_IB_AUTONEG_INPROG;
		if (dd->cspec->autoneg_tries == AUTONEG_TRIES) {
			ppd->lflags |= QIBL_IB_AUTONEG_FAILED;
			dd->cspec->autoneg_tries = 0;
		}
		spin_unlock_irqrestore(&ppd->lflags_lock, flags);
		set_7220_ibspeed_fast(ppd, ppd->link_speed_enabled);
	}
}

static u32 qib_7220_iblink_state(u64 ibcs)
{
	u32 state = (u32)SYM_FIELD(ibcs, IBCStatus, LinkState);

	switch (state) {
	case IB_7220_L_STATE_INIT:
		state = IB_PORT_INIT;
		break;
	case IB_7220_L_STATE_ARM:
		state = IB_PORT_ARMED;
		break;
	case IB_7220_L_STATE_ACTIVE:
		
	case IB_7220_L_STATE_ACT_DEFER:
		state = IB_PORT_ACTIVE;
		break;
	default: 
	case IB_7220_L_STATE_DOWN:
		state = IB_PORT_DOWN;
		break;
	}
	return state;
}

static u8 qib_7220_phys_portstate(u64 ibcs)
{
	u8 state = (u8)SYM_FIELD(ibcs, IBCStatus, LinkTrainingState);
	return qib_7220_physportstate[state];
}

static int qib_7220_ib_updown(struct qib_pportdata *ppd, int ibup, u64 ibcs)
{
	int ret = 0, symadj = 0;
	struct qib_devdata *dd = ppd->dd;
	unsigned long flags;

	spin_lock_irqsave(&ppd->lflags_lock, flags);
	ppd->lflags &= ~QIBL_IB_FORCE_NOTIFY;
	spin_unlock_irqrestore(&ppd->lflags_lock, flags);

	if (!ibup) {
		if (!(ppd->lflags & (QIBL_IB_AUTONEG_FAILED |
				     QIBL_IB_AUTONEG_INPROG)))
			set_7220_ibspeed_fast(ppd, ppd->link_speed_enabled);
		if (!(ppd->lflags & QIBL_IB_AUTONEG_INPROG)) {
			qib_sd7220_presets(dd);
			qib_cancel_sends(ppd); 
			spin_lock_irqsave(&ppd->sdma_lock, flags);
			if (__qib_sdma_running(ppd))
				__qib_sdma_process_event(ppd,
					qib_sdma_event_e70_go_idle);
			spin_unlock_irqrestore(&ppd->sdma_lock, flags);
		}
		
		set_7220_relock_poll(dd, ibup);
	} else {
		if (qib_compat_ddr_negotiate &&
		    !(ppd->lflags & (QIBL_IB_AUTONEG_FAILED |
				     QIBL_IB_AUTONEG_INPROG)) &&
		    ppd->link_speed_active == QIB_IB_SDR &&
		    (ppd->link_speed_enabled & (QIB_IB_DDR | QIB_IB_SDR)) ==
		    (QIB_IB_DDR | QIB_IB_SDR) &&
		    dd->cspec->autoneg_tries < AUTONEG_TRIES) {
			
			++dd->cspec->autoneg_tries;
			if (!ppd->cpspec->ibdeltainprog) {
				ppd->cpspec->ibdeltainprog = 1;
				ppd->cpspec->ibsymsnap = read_7220_creg32(dd,
					cr_ibsymbolerr);
				ppd->cpspec->iblnkerrsnap = read_7220_creg32(dd,
					cr_iblinkerrrecov);
			}
			try_7220_autoneg(ppd);
			ret = 1; 
		} else if ((ppd->lflags & QIBL_IB_AUTONEG_INPROG) &&
			   ppd->link_speed_active == QIB_IB_SDR) {
			autoneg_7220_send(ppd, 1);
			set_7220_ibspeed_fast(ppd, QIB_IB_DDR);
			udelay(2);
			toggle_7220_rclkrls(dd);
			ret = 1; 
		} else {
			if ((ppd->lflags & QIBL_IB_AUTONEG_INPROG) &&
			    (ppd->link_speed_active & QIB_IB_DDR)) {
				spin_lock_irqsave(&ppd->lflags_lock, flags);
				ppd->lflags &= ~(QIBL_IB_AUTONEG_INPROG |
						 QIBL_IB_AUTONEG_FAILED);
				spin_unlock_irqrestore(&ppd->lflags_lock,
						       flags);
				dd->cspec->autoneg_tries = 0;
				
				set_7220_ibspeed_fast(ppd,
						      ppd->link_speed_enabled);
				wake_up(&ppd->cpspec->autoneg_wait);
				symadj = 1;
			} else if (ppd->lflags & QIBL_IB_AUTONEG_FAILED) {
				spin_lock_irqsave(&ppd->lflags_lock, flags);
				ppd->lflags &= ~QIBL_IB_AUTONEG_FAILED;
				spin_unlock_irqrestore(&ppd->lflags_lock,
						       flags);
				ppd->cpspec->ibcddrctrl |=
					IBA7220_IBC_IBTA_1_2_MASK;
				qib_write_kreg(dd, kr_ncmodectrl, 0);
				symadj = 1;
			}
		}

		if (!(ppd->lflags & QIBL_IB_AUTONEG_INPROG))
			symadj = 1;

		if (!ret) {
			ppd->delay_mult = rate_to_delay
			    [(ibcs >> IBA7220_LINKSPEED_SHIFT) & 1]
			    [(ibcs >> IBA7220_LINKWIDTH_SHIFT) & 1];

			set_7220_relock_poll(dd, ibup);
			spin_lock_irqsave(&ppd->sdma_lock, flags);
			if (ppd->sdma_state.current_state !=
			    qib_sdma_state_s20_idle)
				__qib_sdma_process_event(ppd,
					qib_sdma_event_e00_go_hw_down);
			spin_unlock_irqrestore(&ppd->sdma_lock, flags);
		}
	}

	if (symadj) {
		if (ppd->cpspec->ibdeltainprog) {
			ppd->cpspec->ibdeltainprog = 0;
			ppd->cpspec->ibsymdelta += read_7220_creg32(ppd->dd,
				cr_ibsymbolerr) - ppd->cpspec->ibsymsnap;
			ppd->cpspec->iblnkerrdelta += read_7220_creg32(ppd->dd,
				cr_iblinkerrrecov) - ppd->cpspec->iblnkerrsnap;
		}
	} else if (!ibup && qib_compat_ddr_negotiate &&
		   !ppd->cpspec->ibdeltainprog &&
			!(ppd->lflags & QIBL_IB_AUTONEG_INPROG)) {
		ppd->cpspec->ibdeltainprog = 1;
		ppd->cpspec->ibsymsnap = read_7220_creg32(ppd->dd,
							  cr_ibsymbolerr);
		ppd->cpspec->iblnkerrsnap = read_7220_creg32(ppd->dd,
						     cr_iblinkerrrecov);
	}

	if (!ret)
		qib_setup_7220_setextled(ppd, ibup);
	return ret;
}

static int gpio_7220_mod(struct qib_devdata *dd, u32 out, u32 dir, u32 mask)
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

static void get_7220_chip_params(struct qib_devdata *dd)
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
		((char __iomem *) dd->kregbase + dd->pio2k_bufbase);
	if (dd->piobcnt4k) {
		dd->pio4kbase = (u32 __iomem *)
			((char __iomem *) dd->kregbase +
			 (dd->piobufbase >> 32));
		dd->align4k = ALIGN(dd->piosize4k, dd->palign);
	}

	piobufs = dd->piobcnt4k + dd->piobcnt2k;

	dd->pioavregs = ALIGN(piobufs, sizeof(u64) * BITS_PER_BYTE / 2) /
		(sizeof(u64) * BITS_PER_BYTE / 2);
}

static void set_7220_baseaddrs(struct qib_devdata *dd)
{
	u32 cregbase;
	
	cregbase = qib_read_kreg32(dd, kr_counterregbase);
	dd->cspec->cregbase = (u64 __iomem *)
		((char __iomem *) dd->kregbase + cregbase);

	dd->egrtidbase = (u64 __iomem *)
		((char __iomem *) dd->kregbase + dd->rcvegrbase);
}


#define SENDCTRL_SHADOWED (SYM_MASK(SendCtrl, SendIntBufAvail) |	\
			   SYM_MASK(SendCtrl, SPioEnable) |		\
			   SYM_MASK(SendCtrl, SSpecialTriggerEn) |	\
			   SYM_MASK(SendCtrl, SendBufAvailUpd) |	\
			   SYM_MASK(SendCtrl, AvailUpdThld) |		\
			   SYM_MASK(SendCtrl, SDmaEnable) |		\
			   SYM_MASK(SendCtrl, SDmaIntEnable) |		\
			   SYM_MASK(SendCtrl, SDmaHalt) |		\
			   SYM_MASK(SendCtrl, SDmaSingleDescriptor))

static int sendctrl_hook(struct qib_devdata *dd,
			 const struct diag_observer *op,
			 u32 offs, u64 *data, u64 mask, int only_32)
{
	unsigned long flags;
	unsigned idx = offs / sizeof(u64);
	u64 local_data, all_bits;

	if (idx != kr_sendctrl) {
		qib_dev_err(dd, "SendCtrl Hook called with offs %X, %s-bit\n",
			    offs, only_32 ? "32" : "64");
		return 0;
	}

	all_bits = ~0ULL;
	if (only_32)
		all_bits >>= 32;
	spin_lock_irqsave(&dd->sendctrl_lock, flags);
	if ((mask & all_bits) != all_bits) {
		if (only_32)
			local_data = (u64)qib_read_kreg32(dd, idx);
		else
			local_data = qib_read_kreg64(dd, idx);
		qib_dev_err(dd, "Sendctrl -> %X, Shad -> %X\n",
			    (u32)local_data, (u32)dd->sendctrl);
		if ((local_data & SENDCTRL_SHADOWED) !=
		    (dd->sendctrl & SENDCTRL_SHADOWED))
			qib_dev_err(dd, "Sendctrl read: %X shadow is %X\n",
				(u32)local_data, (u32) dd->sendctrl);
		*data = (local_data & ~mask) | (*data & mask);
	}
	if (mask) {
		u64 sval, tval; 

		sval = (dd->sendctrl & ~mask);
		sval |= *data & SENDCTRL_SHADOWED & mask;
		dd->sendctrl = sval;
		tval = sval | (*data & ~SENDCTRL_SHADOWED & mask);
		qib_dev_err(dd, "Sendctrl <- %X, Shad <- %X\n",
			    (u32)tval, (u32)sval);
		qib_write_kreg(dd, kr_sendctrl, tval);
		qib_write_kreg(dd, kr_scratch, 0Ull);
	}
	spin_unlock_irqrestore(&dd->sendctrl_lock, flags);

	return only_32 ? 4 : 8;
}

static const struct diag_observer sendctrl_observer = {
	sendctrl_hook, kr_sendctrl * sizeof(u64),
	kr_sendctrl * sizeof(u64)
};

static int qib_late_7220_initreg(struct qib_devdata *dd)
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
	qib_register_observer(dd, &sendctrl_observer);
	return ret;
}

static int qib_init_7220_variables(struct qib_devdata *dd)
{
	struct qib_chippport_specific *cpspec;
	struct qib_pportdata *ppd;
	int ret = 0;
	u32 sbufs, updthresh;

	cpspec = (struct qib_chippport_specific *)(dd + 1);
	ppd = &cpspec->pportdata;
	dd->pport = ppd;
	dd->num_pports = 1;

	dd->cspec = (struct qib_chip_specific *)(cpspec + dd->num_pports);
	ppd->cpspec = cpspec;

	spin_lock_init(&dd->cspec->sdepb_lock);
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

	get_7220_chip_params(dd);
	qib_7220_boardname(dd);

	dd->gpio_sda_num = _QIB_GPIO_SDA_NUM;
	dd->gpio_scl_num = _QIB_GPIO_SCL_NUM;
	dd->twsi_eeprom_dev = QIB_TWSI_EEPROM_DEV;

	dd->flags |= QIB_HAS_INTX | QIB_HAS_LINK_LATENCY |
		QIB_NODMA_RTAIL | QIB_HAS_THRESH_UPDATE;
	dd->flags |= qib_special_trigger ?
		QIB_USE_SPCL_TRIG : QIB_HAS_SEND_DMA;

	dd->eep_st_masks[0].hwerrs_to_log = HWE_MASK(TXEMemParityErr);

	dd->eep_st_masks[1].hwerrs_to_log = HWE_MASK(RXEMemParityErr);

	dd->eep_st_masks[2].errs_to_log = ERR_MASK(ResetNegated);

	init_waitqueue_head(&cpspec->autoneg_wait);
	INIT_DELAYED_WORK(&cpspec->autoneg_work, autoneg_7220_work);

	qib_init_pportdata(ppd, dd, 0, 1);
	ppd->link_width_supported = IB_WIDTH_1X | IB_WIDTH_4X;
	ppd->link_speed_supported = QIB_IB_SDR | QIB_IB_DDR;

	ppd->link_width_enabled = ppd->link_width_supported;
	ppd->link_speed_enabled = ppd->link_speed_supported;
	ppd->link_width_active = IB_WIDTH_4X;
	ppd->link_speed_active = QIB_IB_SDR;
	ppd->delay_mult = rate_to_delay[0][1];
	ppd->vls_supported = IB_VL_VL0;
	ppd->vls_operational = ppd->vls_supported;

	if (!qib_mini_init)
		qib_write_kreg(dd, kr_rcvbthqp, QIB_KD_QP);

	init_timer(&ppd->cpspec->chase_timer);
	ppd->cpspec->chase_timer.function = reenable_7220_chase;
	ppd->cpspec->chase_timer.data = (unsigned long)ppd;

	qib_num_cfg_vls = 1; 

	dd->rcvhdrentsize = QIB_RCVHDR_ENTSIZE;
	dd->rcvhdrsize = QIB_DFLT_RCVHDRSIZE;
	dd->rhf_offset =
		dd->rcvhdrentsize - sizeof(u64) / sizeof(u32);

	
	ret = ib_mtu_enum_to_int(qib_ibmtu);
	dd->rcvegrbufsize = ret != -1 ? max(ret, 2048) : QIB_DEFAULT_MTU;
	BUG_ON(!is_power_of_2(dd->rcvegrbufsize));
	dd->rcvegrbufsize_shift = ilog2(dd->rcvegrbufsize);

	qib_7220_tidtemplate(dd);

	dd->rhdrhead_intr_off = 1ULL << 32;

	
	init_timer(&dd->stats_timer);
	dd->stats_timer.function = qib_get_7220_faststats;
	dd->stats_timer.data = (unsigned long) dd;
	dd->stats_timer.expires = jiffies + ACTIVITY_TIMER * HZ;

	if (qib_sdma_fetch_arb)
		dd->control |= 1 << 4;

	dd->ureg_align = 0x10000;  

	dd->piosize2kmax_dwords = (dd->piosize2k >> 2)-1;
	qib_7220_config_ctxts(dd);
	qib_set_ctxtcnt(dd);  

	if (qib_wc_pat) {
		ret = init_chip_wc_pat(dd, 0);
		if (ret)
			goto bail;
	}
	set_7220_baseaddrs(dd); 

	ret = 0;
	if (qib_mini_init)
		goto bail;

	ret = qib_create_ctxts(dd);
	init_7220_cntrnames(dd);

	updthresh = 8U; 
	if (dd->flags & QIB_HAS_SEND_DMA) {
		dd->cspec->sdmabufcnt =  dd->piobcnt4k;
		sbufs = updthresh > 3 ? updthresh : 3;
	} else {
		dd->cspec->sdmabufcnt = 0;
		sbufs = dd->piobcnt4k;
	}

	dd->cspec->lastbuf_for_pio = dd->piobcnt2k + dd->piobcnt4k -
		dd->cspec->sdmabufcnt;
	dd->lastctxt_piobuf = dd->cspec->lastbuf_for_pio - sbufs;
	dd->cspec->lastbuf_for_pio--; 
	dd->pbufsctxt = dd->lastctxt_piobuf /
		(dd->cfgctxts - dd->first_user_ctxt);

	if ((dd->pbufsctxt - 2) < updthresh)
		updthresh = dd->pbufsctxt - 2;

	dd->cspec->updthresh_dflt = updthresh;
	dd->cspec->updthresh = updthresh;

	
	dd->sendctrl |= (updthresh & SYM_RMASK(SendCtrl, AvailUpdThld))
			     << SYM_LSB(SendCtrl, AvailUpdThld);

	dd->psxmitwait_supported = 1;
	dd->psxmitwait_check_rate = QIB_7220_PSXMITWAIT_CHECK_RATE;
bail:
	return ret;
}

static u32 __iomem *qib_7220_getsendbuf(struct qib_pportdata *ppd, u64 pbc,
					u32 *pbufnum)
{
	u32 first, last, plen = pbc & QIB_PBC_LENGTH_MASK;
	struct qib_devdata *dd = ppd->dd;
	u32 __iomem *buf;

	if (((pbc >> 32) & PBC_7220_VL15_SEND_CTRL) &&
		!(ppd->lflags & (QIBL_IB_AUTONEG_INPROG | QIBL_LINKACTIVE)))
		buf = get_7220_link_buf(ppd, pbufnum);
	else {
		if ((plen + 1) > dd->piosize2kmax_dwords)
			first = dd->piobcnt2k;
		else
			first = 0;
		
		last = dd->cspec->lastbuf_for_pio;
		buf = qib_getsendbuf_range(dd, pbufnum, first, last);
	}
	return buf;
}

static void qib_set_cntr_7220_sample(struct qib_pportdata *ppd, u32 intv,
				     u32 start)
{
	write_7220_creg(ppd->dd, cr_psinterval, intv);
	write_7220_creg(ppd->dd, cr_psstart, start);
}


static void qib_sdma_update_7220_tail(struct qib_pportdata *ppd, u16 tail)
{
	
	wmb();
	ppd->sdma_descq_tail = tail;
	qib_write_kreg(ppd->dd, kr_senddmatail, tail);
}

static void qib_sdma_set_7220_desc_cnt(struct qib_pportdata *ppd, unsigned cnt)
{
}

static struct sdma_set_state_action sdma_7220_action_table[] = {
	[qib_sdma_state_s00_hw_down] = {
		.op_enable = 0,
		.op_intenable = 0,
		.op_halt = 0,
		.go_s99_running_tofalse = 1,
	},
	[qib_sdma_state_s10_hw_start_up_wait] = {
		.op_enable = 1,
		.op_intenable = 1,
		.op_halt = 1,
	},
	[qib_sdma_state_s20_idle] = {
		.op_enable = 1,
		.op_intenable = 1,
		.op_halt = 1,
	},
	[qib_sdma_state_s30_sw_clean_up_wait] = {
		.op_enable = 0,
		.op_intenable = 1,
		.op_halt = 0,
	},
	[qib_sdma_state_s40_hw_clean_up_wait] = {
		.op_enable = 1,
		.op_intenable = 1,
		.op_halt = 1,
	},
	[qib_sdma_state_s50_hw_halt_wait] = {
		.op_enable = 1,
		.op_intenable = 1,
		.op_halt = 1,
	},
	[qib_sdma_state_s99_running] = {
		.op_enable = 1,
		.op_intenable = 1,
		.op_halt = 0,
		.go_s99_running_totrue = 1,
	},
};

static void qib_7220_sdma_init_early(struct qib_pportdata *ppd)
{
	ppd->sdma_state.set_state_action = sdma_7220_action_table;
}

static int init_sdma_7220_regs(struct qib_pportdata *ppd)
{
	struct qib_devdata *dd = ppd->dd;
	unsigned i, n;
	u64 senddmabufmask[3] = { 0 };

	
	qib_write_kreg(dd, kr_senddmabase, ppd->sdma_descq_phys);
	qib_sdma_7220_setlengen(ppd);
	qib_sdma_update_7220_tail(ppd, 0); 
	
	qib_write_kreg(dd, kr_senddmaheadaddr, ppd->sdma_head_phys);

	n = dd->piobcnt2k + dd->piobcnt4k;
	i = n - dd->cspec->sdmabufcnt;

	for (; i < n; ++i) {
		unsigned word = i / 64;
		unsigned bit = i & 63;

		BUG_ON(word >= 3);
		senddmabufmask[word] |= 1ULL << bit;
	}
	qib_write_kreg(dd, kr_senddmabufmask0, senddmabufmask[0]);
	qib_write_kreg(dd, kr_senddmabufmask1, senddmabufmask[1]);
	qib_write_kreg(dd, kr_senddmabufmask2, senddmabufmask[2]);

	ppd->sdma_state.first_sendbuf = i;
	ppd->sdma_state.last_sendbuf = n;

	return 0;
}

static u16 qib_sdma_7220_gethead(struct qib_pportdata *ppd)
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
		(u16)le64_to_cpu(*ppd->sdma_head_dma) :
		(u16)qib_read_kreg32(dd, kr_senddmahead);

	swhead = ppd->sdma_descq_head;
	swtail = ppd->sdma_descq_tail;
	cnt = ppd->sdma_descq_cnt;

	if (swhead < swtail) {
		
		sane = (hwhead >= swhead) & (hwhead <= swtail);
	} else if (swhead > swtail) {
		
		sane = ((hwhead >= swhead) && (hwhead < cnt)) ||
			(hwhead <= swtail);
	} else {
		
		sane = (hwhead == swhead);
	}

	if (unlikely(!sane)) {
		if (use_dmahead) {
			
			use_dmahead = 0;
			goto retry;
		}
		
		hwhead = swhead;
	}

	return hwhead;
}

static int qib_sdma_7220_busy(struct qib_pportdata *ppd)
{
	u64 hwstatus = qib_read_kreg64(ppd->dd, kr_senddmastatus);

	return (hwstatus & SYM_MASK(SendDmaStatus, ScoreBoardDrainInProg)) ||
	       (hwstatus & SYM_MASK(SendDmaStatus, AbortInProg)) ||
	       (hwstatus & SYM_MASK(SendDmaStatus, InternalSDmaEnable)) ||
	       !(hwstatus & SYM_MASK(SendDmaStatus, ScbEmpty));
}

static u32 qib_7220_setpbc_control(struct qib_pportdata *ppd, u32 plen,
				   u8 srate, u8 vl)
{
	u8 snd_mult = ppd->delay_mult;
	u8 rcv_mult = ib_rate_to_delay[srate];
	u32 ret = ppd->cpspec->last_delay_mult;

	ppd->cpspec->last_delay_mult = (rcv_mult > snd_mult) ?
		(plen * (rcv_mult - snd_mult) + 1) >> 1 : 0;

	
	if (vl == 15)
		ret |= PBC_7220_VL15_SEND_CTRL;
	return ret;
}

static void qib_7220_initvl15_bufs(struct qib_devdata *dd)
{
}

static void qib_7220_init_ctxt(struct qib_ctxtdata *rcd)
{
	if (!rcd->ctxt) {
		rcd->rcvegrcnt = IBA7220_KRCVEGRCNT;
		rcd->rcvegr_tid_base = 0;
	} else {
		rcd->rcvegrcnt = rcd->dd->cspec->rcvegrcnt;
		rcd->rcvegr_tid_base = IBA7220_KRCVEGRCNT +
			(rcd->ctxt - 1) * rcd->rcvegrcnt;
	}
}

static void qib_7220_txchk_change(struct qib_devdata *dd, u32 start,
				  u32 len, u32 which, struct qib_ctxtdata *rcd)
{
	int i;
	unsigned long flags;

	switch (which) {
	case TXCHK_CHG_TYPE_KERN:
		
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
			sendctrl_7220_mod(dd->pport, QIB_SENDCTRL_AVAIL_BLIP);
		}
		break;
	case TXCHK_CHG_TYPE_USER:
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
			sendctrl_7220_mod(dd->pport, QIB_SENDCTRL_AVAIL_BLIP);
		} else
			spin_unlock_irqrestore(&dd->sendctrl_lock, flags);
		break;
	}
}

static void writescratch(struct qib_devdata *dd, u32 val)
{
	qib_write_kreg(dd, kr_scratch, val);
}

#define VALID_TS_RD_REG_MASK 0xBF
static int qib_7220_tempsense_rd(struct qib_devdata *dd, int regnum)
{
	int ret;
	u8 rdata;

	if (regnum > 7) {
		ret = -EINVAL;
		goto bail;
	}

	
	if (!((1 << regnum) & VALID_TS_RD_REG_MASK)) {
		ret = 0;
		goto bail;
	}

	ret = mutex_lock_interruptible(&dd->eep_lock);
	if (ret)
		goto bail;

	ret = qib_twsi_blk_rd(dd, QIB_TWSI_TEMP_DEV, regnum, &rdata, 1);
	if (!ret)
		ret = rdata;

	mutex_unlock(&dd->eep_lock);

bail:
	return ret;
}

static int qib_7220_eeprom_wen(struct qib_devdata *dd, int wen)
{
	return 1;
}

struct qib_devdata *qib_init_iba7220_funcs(struct pci_dev *pdev,
					   const struct pci_device_id *ent)
{
	struct qib_devdata *dd;
	int ret;
	u32 boardid, minwidth;

	dd = qib_alloc_devdata(pdev, sizeof(struct qib_chip_specific) +
		sizeof(struct qib_chippport_specific));
	if (IS_ERR(dd))
		goto bail;

	dd->f_bringup_serdes    = qib_7220_bringup_serdes;
	dd->f_cleanup           = qib_setup_7220_cleanup;
	dd->f_clear_tids        = qib_7220_clear_tids;
	dd->f_free_irq          = qib_7220_free_irq;
	dd->f_get_base_info     = qib_7220_get_base_info;
	dd->f_get_msgheader     = qib_7220_get_msgheader;
	dd->f_getsendbuf        = qib_7220_getsendbuf;
	dd->f_gpio_mod          = gpio_7220_mod;
	dd->f_eeprom_wen        = qib_7220_eeprom_wen;
	dd->f_hdrqempty         = qib_7220_hdrqempty;
	dd->f_ib_updown         = qib_7220_ib_updown;
	dd->f_init_ctxt         = qib_7220_init_ctxt;
	dd->f_initvl15_bufs     = qib_7220_initvl15_bufs;
	dd->f_intr_fallback     = qib_7220_intr_fallback;
	dd->f_late_initreg      = qib_late_7220_initreg;
	dd->f_setpbc_control    = qib_7220_setpbc_control;
	dd->f_portcntr          = qib_portcntr_7220;
	dd->f_put_tid           = qib_7220_put_tid;
	dd->f_quiet_serdes      = qib_7220_quiet_serdes;
	dd->f_rcvctrl           = rcvctrl_7220_mod;
	dd->f_read_cntrs        = qib_read_7220cntrs;
	dd->f_read_portcntrs    = qib_read_7220portcntrs;
	dd->f_reset             = qib_setup_7220_reset;
	dd->f_init_sdma_regs    = init_sdma_7220_regs;
	dd->f_sdma_busy         = qib_sdma_7220_busy;
	dd->f_sdma_gethead      = qib_sdma_7220_gethead;
	dd->f_sdma_sendctrl     = qib_7220_sdma_sendctrl;
	dd->f_sdma_set_desc_cnt = qib_sdma_set_7220_desc_cnt;
	dd->f_sdma_update_tail  = qib_sdma_update_7220_tail;
	dd->f_sdma_hw_clean_up  = qib_7220_sdma_hw_clean_up;
	dd->f_sdma_hw_start_up  = qib_7220_sdma_hw_start_up;
	dd->f_sdma_init_early   = qib_7220_sdma_init_early;
	dd->f_sendctrl          = sendctrl_7220_mod;
	dd->f_set_armlaunch     = qib_set_7220_armlaunch;
	dd->f_set_cntr_sample   = qib_set_cntr_7220_sample;
	dd->f_iblink_state      = qib_7220_iblink_state;
	dd->f_ibphys_portstate  = qib_7220_phys_portstate;
	dd->f_get_ib_cfg        = qib_7220_get_ib_cfg;
	dd->f_set_ib_cfg        = qib_7220_set_ib_cfg;
	dd->f_set_ib_loopback   = qib_7220_set_loopback;
	dd->f_set_intr_state    = qib_7220_set_intr_state;
	dd->f_setextled         = qib_setup_7220_setextled;
	dd->f_txchk_change      = qib_7220_txchk_change;
	dd->f_update_usrhead    = qib_update_7220_usrhead;
	dd->f_wantpiobuf_intr   = qib_wantpiobuf_7220_intr;
	dd->f_xgxs_reset        = qib_7220_xgxs_reset;
	dd->f_writescratch      = writescratch;
	dd->f_tempsense_rd	= qib_7220_tempsense_rd;
	ret = qib_pcie_ddinit(dd, pdev, ent);
	if (ret < 0)
		goto bail_free;

	
	ret = qib_init_7220_variables(dd);
	if (ret)
		goto bail_cleanup;

	if (qib_mini_init)
		goto bail;

	boardid = SYM_FIELD(dd->revision, Revision,
			    BoardID);
	switch (boardid) {
	case 0:
	case 2:
	case 10:
	case 12:
		minwidth = 16; 
		break;
	default:
		minwidth = 8; 
		break;
	}
	if (qib_pcie_params(dd, minwidth, NULL, NULL))
		qib_dev_err(dd, "Failed to setup PCIe or interrupts; "
			    "continuing anyway\n");

	
	dd->cspec->irq = pdev->irq;

	if (qib_read_kreg64(dd, kr_hwerrstatus) &
	    QLOGIC_IB_HWE_SERDESPLLFAILED)
		qib_write_kreg(dd, kr_hwerrclear,
			       QLOGIC_IB_HWE_SERDESPLLFAILED);

	
	qib_setup_7220_interrupt(dd);
	qib_7220_init_hwerrors(dd);

	
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
