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

#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/firmware.h>

#include "qib.h"
#include "qib_7220.h"

#define SD7220_FW_NAME "qlogic/sd7220.fw"
MODULE_FIRMWARE(SD7220_FW_NAME);

#define KREG_IDX(regname) (QIB_7220_##regname##_OFFS / sizeof(u64))
#define kr_hwerrclear KREG_IDX(HwErrClear)
#define kr_hwerrmask KREG_IDX(HwErrMask)
#define kr_hwerrstatus KREG_IDX(HwErrStatus)
#define kr_ibcstatus KREG_IDX(IBCStatus)
#define kr_ibserdesctrl KREG_IDX(IBSerDesCtrl)
#define kr_scratch KREG_IDX(Scratch)
#define kr_xgxs_cfg KREG_IDX(XGXSCfg)
#define kr_ibsd_epb_access_ctrl KREG_IDX(ibsd_epb_access_ctrl)
#define kr_ibsd_epb_transaction_reg KREG_IDX(ibsd_epb_transaction_reg)
#define kr_pciesd_epb_transaction_reg KREG_IDX(pciesd_epb_transaction_reg)
#define kr_pciesd_epb_access_ctrl KREG_IDX(pciesd_epb_access_ctrl)
#define kr_serdes_ddsrxeq0 KREG_IDX(SerDes_DDSRXEQ0)

#define kr_serdes_maptable KREG_IDX(IBSerDesMappTable)

#define PCIE_SERDES0 0
#define PCIE_SERDES1 1

#define EPB_ADDR_SHF 8
#define EPB_LOC(chn, elt, reg) \
	(((elt & 0xf) | ((chn & 7) << 4) | ((reg & 0x3f) << 9)) << \
	 EPB_ADDR_SHF)
#define EPB_IB_QUAD0_CS_SHF (25)
#define EPB_IB_QUAD0_CS (1U <<  EPB_IB_QUAD0_CS_SHF)
#define EPB_IB_UC_CS_SHF (26)
#define EPB_PCIE_UC_CS_SHF (27)
#define EPB_GLOBAL_WR (1U << (EPB_ADDR_SHF + 8))

static int qib_sd7220_reg_mod(struct qib_devdata *dd, int sdnum, u32 loc,
			      u32 data, u32 mask);
static int ibsd_mod_allchnls(struct qib_devdata *dd, int loc, int val,
			     int mask);
static int qib_sd_trimdone_poll(struct qib_devdata *dd);
static void qib_sd_trimdone_monitor(struct qib_devdata *dd, const char *where);
static int qib_sd_setvals(struct qib_devdata *dd);
static int qib_sd_early(struct qib_devdata *dd);
static int qib_sd_dactrim(struct qib_devdata *dd);
static int qib_internal_presets(struct qib_devdata *dd);
static int qib_sd_trimself(struct qib_devdata *dd, int val);
static int epb_access(struct qib_devdata *dd, int sdnum, int claim);
static int qib_sd7220_ib_load(struct qib_devdata *dd,
			      const struct firmware *fw);
static int qib_sd7220_ib_vfy(struct qib_devdata *dd,
			     const struct firmware *fw);

static int qib_ibsd_ucode_loaded(struct qib_pportdata *ppd,
				 const struct firmware *fw)
{
	struct qib_devdata *dd = ppd->dd;

	if (!dd->cspec->serdes_first_init_done &&
	    qib_sd7220_ib_vfy(dd, fw) > 0)
		dd->cspec->serdes_first_init_done = 1;
	return dd->cspec->serdes_first_init_done;
}

#define QLOGIC_IB_HWE_IB_UC_MEMORYPARITYERR      0x0000004000000000ULL
#define IB_MPREG5 (EPB_LOC(6, 0, 0xE) | (1L << EPB_IB_UC_CS_SHF))
#define IB_MPREG6 (EPB_LOC(6, 0, 0xF) | (1U << EPB_IB_UC_CS_SHF))
#define UC_PAR_CLR_D 8
#define UC_PAR_CLR_M 0xC
#define IB_CTRL2(chn) (EPB_LOC(chn, 7, 3) | EPB_IB_QUAD0_CS)
#define START_EQ1(chan) EPB_LOC(chan, 7, 0x27)

void qib_sd7220_clr_ibpar(struct qib_devdata *dd)
{
	int ret;

	
	ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, IB_MPREG6,
		UC_PAR_CLR_D, UC_PAR_CLR_M);
	if (ret < 0) {
		qib_dev_err(dd, "Failed clearing IBSerDes Parity err\n");
		goto bail;
	}
	ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, IB_MPREG6, 0,
		UC_PAR_CLR_M);

	qib_read_kreg32(dd, kr_scratch);
	udelay(4);
	qib_write_kreg(dd, kr_hwerrclear,
		QLOGIC_IB_HWE_IB_UC_MEMORYPARITYERR);
	qib_read_kreg32(dd, kr_scratch);
bail:
	return;
}

#define IBSD_RESYNC_TRIES 3
#define IB_PGUDP(chn) (EPB_LOC((chn), 2, 1) | EPB_IB_QUAD0_CS)
#define IB_CMUDONE(chn) (EPB_LOC((chn), 7, 0xF) | EPB_IB_QUAD0_CS)

static int qib_resync_ibepb(struct qib_devdata *dd)
{
	int ret, pat, tries, chn;
	u32 loc;

	ret = -1;
	chn = 0;
	for (tries = 0; tries < (4 * IBSD_RESYNC_TRIES); ++tries) {
		loc = IB_PGUDP(chn);
		ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, loc, 0, 0);
		if (ret < 0) {
			qib_dev_err(dd, "Failed read in resync\n");
			continue;
		}
		if (ret != 0xF0 && ret != 0x55 && tries == 0)
			qib_dev_err(dd, "unexpected pattern in resync\n");
		pat = ret ^ 0xA5; 
		ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, loc, pat, 0xFF);
		if (ret < 0) {
			qib_dev_err(dd, "Failed write in resync\n");
			continue;
		}
		ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, loc, 0, 0);
		if (ret < 0) {
			qib_dev_err(dd, "Failed re-read in resync\n");
			continue;
		}
		if (ret != pat) {
			qib_dev_err(dd, "Failed compare1 in resync\n");
			continue;
		}
		loc = IB_CMUDONE(chn);
		ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, loc, 0, 0);
		if (ret < 0) {
			qib_dev_err(dd, "Failed CMUDONE rd in resync\n");
			continue;
		}
		if ((ret & 0x70) != ((chn << 4) | 0x40)) {
			qib_dev_err(dd, "Bad CMUDONE value %02X, chn %d\n",
				    ret, chn);
			continue;
		}
		if (++chn == 4)
			break;  
	}
	return (ret > 0) ? 0 : ret;
}

static int qib_ibsd_reset(struct qib_devdata *dd, int assert_rst)
{
	u64 rst_val;
	int ret = 0;
	unsigned long flags;

	rst_val = qib_read_kreg64(dd, kr_ibserdesctrl);
	if (assert_rst) {
		spin_lock_irqsave(&dd->cspec->sdepb_lock, flags);
		epb_access(dd, IB_7220_SERDES, 1);
		rst_val |= 1ULL;
		
		qib_write_kreg(dd, kr_hwerrmask,
			       dd->cspec->hwerrmask &
			       ~QLOGIC_IB_HWE_IB_UC_MEMORYPARITYERR);
		qib_write_kreg(dd, kr_ibserdesctrl, rst_val);
		
		qib_read_kreg32(dd, kr_scratch);
		udelay(2);
		
		epb_access(dd, IB_7220_SERDES, -1);
		spin_unlock_irqrestore(&dd->cspec->sdepb_lock, flags);
	} else {
		u64 val;
		rst_val &= ~(1ULL);
		qib_write_kreg(dd, kr_hwerrmask,
			       dd->cspec->hwerrmask &
			       ~QLOGIC_IB_HWE_IB_UC_MEMORYPARITYERR);

		ret = qib_resync_ibepb(dd);
		if (ret < 0)
			qib_dev_err(dd, "unable to re-sync IB EPB\n");

		
		ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, IB_MPREG5, 1, 1);
		if (ret < 0)
			goto bail;
		
		ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, IB_MPREG6, 0x80,
			0x80);
		if (ret < 0) {
			qib_dev_err(dd, "Failed to set WDOG disable\n");
			goto bail;
		}
		qib_write_kreg(dd, kr_ibserdesctrl, rst_val);
		
		qib_read_kreg32(dd, kr_scratch);
		udelay(1);
		
		qib_sd7220_clr_ibpar(dd);
		val = qib_read_kreg64(dd, kr_hwerrstatus);
		if (val & QLOGIC_IB_HWE_IB_UC_MEMORYPARITYERR) {
			qib_dev_err(dd, "IBUC Parity still set after RST\n");
			dd->cspec->hwerrmask &=
				~QLOGIC_IB_HWE_IB_UC_MEMORYPARITYERR;
		}
		qib_write_kreg(dd, kr_hwerrmask,
			dd->cspec->hwerrmask);
	}

bail:
	return ret;
}

static void qib_sd_trimdone_monitor(struct qib_devdata *dd,
	const char *where)
{
	int ret, chn, baduns;
	u64 val;

	if (!where)
		where = "?";

	
	udelay(2);

	ret = qib_resync_ibepb(dd);
	if (ret < 0)
		qib_dev_err(dd, "not able to re-sync IB EPB (%s)\n", where);

	
	ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, IB_CTRL2(0), 0, 0);
	if (ret < 0)
		qib_dev_err(dd, "Failed TRIMDONE 1st read, (%s)\n", where);

	
	val = qib_read_kreg64(dd, kr_ibcstatus);
	if (!(val & (1ULL << 11)))
		qib_dev_err(dd, "IBCS TRIMDONE clear (%s)\n", where);
	udelay(2);

	ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, IB_MPREG6, 0x80, 0x80);
	if (ret < 0)
		qib_dev_err(dd, "Failed Dummy RMW, (%s)\n", where);
	udelay(10);

	baduns = 0;

	for (chn = 3; chn >= 0; --chn) {
		
		ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES,
			IB_CTRL2(chn), 0, 0);
		if (ret < 0)
			qib_dev_err(dd, "Failed checking TRIMDONE, chn %d"
				    " (%s)\n", chn, where);

		if (!(ret & 0x10)) {
			int probe;

			baduns |= (1 << chn);
			qib_dev_err(dd, "TRIMDONE cleared on chn %d (%02X)."
				" (%s)\n", chn, ret, where);
			probe = qib_sd7220_reg_mod(dd, IB_7220_SERDES,
				IB_PGUDP(0), 0, 0);
			qib_dev_err(dd, "probe is %d (%02X)\n",
				probe, probe);
			probe = qib_sd7220_reg_mod(dd, IB_7220_SERDES,
				IB_CTRL2(chn), 0, 0);
			qib_dev_err(dd, "re-read: %d (%02X)\n",
				probe, probe);
			ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES,
				IB_CTRL2(chn), 0x10, 0x10);
			if (ret < 0)
				qib_dev_err(dd,
					"Err on TRIMDONE rewrite1\n");
		}
	}
	for (chn = 3; chn >= 0; --chn) {
		
		if (baduns & (1 << chn)) {
			qib_dev_err(dd,
				"Reseting TRIMDONE on chn %d (%s)\n",
				chn, where);
			ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES,
				IB_CTRL2(chn), 0x10, 0x10);
			if (ret < 0)
				qib_dev_err(dd, "Failed re-setting "
					"TRIMDONE, chn %d (%s)\n",
					chn, where);
		}
	}
}

int qib_sd7220_init(struct qib_devdata *dd)
{
	const struct firmware *fw;
	int ret = 1; 
	int first_reset, was_reset;

	
	was_reset = (qib_read_kreg64(dd, kr_ibserdesctrl) & 1);
	if (!was_reset) {
		
		qib_ibsd_reset(dd, 1);
		qib_sd_trimdone_monitor(dd, "Driver-reload");
	}

	ret = request_firmware(&fw, SD7220_FW_NAME, &dd->pcidev->dev);
	if (ret) {
		qib_dev_err(dd, "Failed to load IB SERDES image\n");
		goto done;
	}

	
	ret = qib_ibsd_ucode_loaded(dd->pport, fw);
	if (ret < 0)
		goto bail;

	first_reset = !ret; 
	ret = qib_sd_early(dd);
	if (ret < 0) {
		qib_dev_err(dd, "Failed to set IB SERDES early defaults\n");
		goto bail;
	}
	if (first_reset) {
		ret = qib_sd_dactrim(dd);
		if (ret < 0) {
			qib_dev_err(dd, "Failed IB SERDES DAC trim\n");
			goto bail;
		}
	}
	ret = qib_internal_presets(dd);
	if (ret < 0) {
		qib_dev_err(dd, "Failed to set IB SERDES presets\n");
		goto bail;
	}
	ret = qib_sd_trimself(dd, 0x80);
	if (ret < 0) {
		qib_dev_err(dd, "Failed to set IB SERDES TRIMSELF\n");
		goto bail;
	}

	
	ret = 0;        
	if (first_reset) {
		int vfy;
		int trim_done;

		ret = qib_sd7220_ib_load(dd, fw);
		if (ret < 0) {
			qib_dev_err(dd, "Failed to load IB SERDES image\n");
			goto bail;
		} else {
			
			vfy = qib_sd7220_ib_vfy(dd, fw);
			if (vfy != ret) {
				qib_dev_err(dd, "SERDES PRAM VFY failed\n");
				goto bail;
			} 
		} 

		ret = 0;
		ret = ibsd_mod_allchnls(dd, START_EQ1(0), 0, 0x38);
		if (ret < 0) {
			qib_dev_err(dd, "Failed clearing START_EQ1\n");
			goto bail;
		}

		qib_ibsd_reset(dd, 0);
		trim_done = qib_sd_trimdone_poll(dd);
		qib_ibsd_reset(dd, 1);

		if (!trim_done) {
			qib_dev_err(dd, "No TRIMDONE seen\n");
			goto bail;
		}
		qib_sd_trimdone_monitor(dd, "First-reset");
		
		dd->cspec->serdes_first_init_done = 1;
	}
	ret = 0;
	if (qib_sd_setvals(dd) >= 0)
		goto done;
bail:
	ret = 1;
done:
	
	set_7220_relock_poll(dd, -1);

	release_firmware(fw);
	return ret;
}

#define EPB_ACC_REQ 1
#define EPB_ACC_GNT 0x100
#define EPB_DATA_MASK 0xFF
#define EPB_RD (1ULL << 24)
#define EPB_TRANS_RDY (1ULL << 31)
#define EPB_TRANS_ERR (1ULL << 30)
#define EPB_TRANS_TRIES 5

static int epb_access(struct qib_devdata *dd, int sdnum, int claim)
{
	u16 acc;
	u64 accval;
	int owned = 0;
	u64 oct_sel = 0;

	switch (sdnum) {
	case IB_7220_SERDES:
		acc = kr_ibsd_epb_access_ctrl;
		break;

	case PCIE_SERDES0:
	case PCIE_SERDES1:
		
		acc = kr_pciesd_epb_access_ctrl;
		oct_sel = (2 << (sdnum - PCIE_SERDES0));
		break;

	default:
		return 0;
	}

	
	qib_read_kreg32(dd, kr_scratch);
	udelay(15);

	accval = qib_read_kreg32(dd, acc);

	owned = !!(accval & EPB_ACC_GNT);
	if (claim < 0) {
		
		u64 pollval;
		u64 newval = 0;
		qib_write_kreg(dd, acc, newval);
		
		pollval = qib_read_kreg32(dd, acc);
		udelay(5);
		pollval = qib_read_kreg32(dd, acc);
		if (pollval & EPB_ACC_GNT)
			owned = -1;
	} else if (claim > 0) {
		
		u64 pollval;
		u64 newval = EPB_ACC_REQ | oct_sel;
		qib_write_kreg(dd, acc, newval);
		
		pollval = qib_read_kreg32(dd, acc);
		udelay(5);
		pollval = qib_read_kreg32(dd, acc);
		if (!(pollval & EPB_ACC_GNT))
			owned = -1;
	}
	return owned;
}

static int epb_trans(struct qib_devdata *dd, u16 reg, u64 i_val, u64 *o_vp)
{
	int tries;
	u64 transval;

	qib_write_kreg(dd, reg, i_val);
	
	transval = qib_read_kreg64(dd, reg);

	for (tries = EPB_TRANS_TRIES; tries; --tries) {
		transval = qib_read_kreg32(dd, reg);
		if (transval & EPB_TRANS_RDY)
			break;
		udelay(5);
	}
	if (transval & EPB_TRANS_ERR)
		return -1;
	if (tries > 0 && o_vp)
		*o_vp = transval;
	return tries;
}

static int qib_sd7220_reg_mod(struct qib_devdata *dd, int sdnum, u32 loc,
			      u32 wd, u32 mask)
{
	u16 trans;
	u64 transval;
	int owned;
	int tries, ret;
	unsigned long flags;

	switch (sdnum) {
	case IB_7220_SERDES:
		trans = kr_ibsd_epb_transaction_reg;
		break;

	case PCIE_SERDES0:
	case PCIE_SERDES1:
		trans = kr_pciesd_epb_transaction_reg;
		break;

	default:
		return -1;
	}

	spin_lock_irqsave(&dd->cspec->sdepb_lock, flags);

	owned = epb_access(dd, sdnum, 1);
	if (owned < 0) {
		spin_unlock_irqrestore(&dd->cspec->sdepb_lock, flags);
		return -1;
	}
	ret = 0;
	for (tries = EPB_TRANS_TRIES; tries; --tries) {
		transval = qib_read_kreg32(dd, trans);
		if (transval & EPB_TRANS_RDY)
			break;
		udelay(5);
	}

	if (tries > 0) {
		tries = 1;      
		if (mask != 0xFF) {
			transval = loc | EPB_RD;
			tries = epb_trans(dd, trans, transval, &transval);
		}
		if (tries > 0 && mask != 0) {
			wd = (wd & mask) | (transval & ~mask);
			transval = loc | (wd & EPB_DATA_MASK);
			tries = epb_trans(dd, trans, transval, &transval);
		}
	}
	

	if (epb_access(dd, sdnum, -1) < 0)
		ret = -1;
	else
		ret = transval & EPB_DATA_MASK;

	spin_unlock_irqrestore(&dd->cspec->sdepb_lock, flags);
	if (tries <= 0)
		ret = -1;
	return ret;
}

#define EPB_ROM_R (2)
#define EPB_ROM_W (1)
#define EPB_UC_CTL EPB_LOC(6, 0, 0)
#define EPB_MADDRL EPB_LOC(6, 0, 2)
#define EPB_MADDRH EPB_LOC(6, 0, 3)
#define EPB_ROMDATA EPB_LOC(6, 0, 4)
#define EPB_RAMDATA EPB_LOC(6, 0, 5)

static int qib_sd7220_ram_xfer(struct qib_devdata *dd, int sdnum, u32 loc,
			       u8 *buf, int cnt, int rd_notwr)
{
	u16 trans;
	u64 transval;
	u64 csbit;
	int owned;
	int tries;
	int sofar;
	int addr;
	int ret;
	unsigned long flags;
	const char *op;

	
	switch (sdnum) {
	case IB_7220_SERDES:
		csbit = 1ULL << EPB_IB_UC_CS_SHF;
		trans = kr_ibsd_epb_transaction_reg;
		break;

	case PCIE_SERDES0:
	case PCIE_SERDES1:
		
		csbit = 1ULL << EPB_PCIE_UC_CS_SHF;
		trans = kr_pciesd_epb_transaction_reg;
		break;

	default:
		return -1;
	}

	op = rd_notwr ? "Rd" : "Wr";
	spin_lock_irqsave(&dd->cspec->sdepb_lock, flags);

	owned = epb_access(dd, sdnum, 1);
	if (owned < 0) {
		spin_unlock_irqrestore(&dd->cspec->sdepb_lock, flags);
		return -1;
	}

	addr = loc & 0x1FFF;
	for (tries = EPB_TRANS_TRIES; tries; --tries) {
		transval = qib_read_kreg32(dd, trans);
		if (transval & EPB_TRANS_RDY)
			break;
		udelay(5);
	}

	sofar = 0;
	if (tries > 0) {

		
		transval = csbit | EPB_UC_CTL |
			(rd_notwr ? EPB_ROM_R : EPB_ROM_W);
		tries = epb_trans(dd, trans, transval, &transval);
		while (tries > 0 && sofar < cnt) {
			if (!sofar) {
				
				int addrbyte = (addr + sofar) >> 8;
				transval = csbit | EPB_MADDRH | addrbyte;
				tries = epb_trans(dd, trans, transval,
						  &transval);
				if (tries <= 0)
					break;
				addrbyte = (addr + sofar) & 0xFF;
				transval = csbit | EPB_MADDRL | addrbyte;
				tries = epb_trans(dd, trans, transval,
						 &transval);
				if (tries <= 0)
					break;
			}

			if (rd_notwr)
				transval = csbit | EPB_ROMDATA | EPB_RD;
			else
				transval = csbit | EPB_ROMDATA | buf[sofar];
			tries = epb_trans(dd, trans, transval, &transval);
			if (tries <= 0)
				break;
			if (rd_notwr)
				buf[sofar] = transval & EPB_DATA_MASK;
			++sofar;
		}
		
		transval = csbit | EPB_UC_CTL;
		tries = epb_trans(dd, trans, transval, &transval);
	}

	ret = sofar;
	
	if (epb_access(dd, sdnum, -1) < 0)
		ret = -1;

	spin_unlock_irqrestore(&dd->cspec->sdepb_lock, flags);
	if (tries <= 0)
		ret = -1;
	return ret;
}

#define PROG_CHUNK 64

static int qib_sd7220_prog_ld(struct qib_devdata *dd, int sdnum,
			      const u8 *img, int len, int offset)
{
	int cnt, sofar, req;

	sofar = 0;
	while (sofar < len) {
		req = len - sofar;
		if (req > PROG_CHUNK)
			req = PROG_CHUNK;
		cnt = qib_sd7220_ram_xfer(dd, sdnum, offset + sofar,
					  (u8 *)img + sofar, req, 0);
		if (cnt < req) {
			sofar = -1;
			break;
		}
		sofar += req;
	}
	return sofar;
}

#define VFY_CHUNK 64
#define SD_PRAM_ERROR_LIMIT 42

static int qib_sd7220_prog_vfy(struct qib_devdata *dd, int sdnum,
			       const u8 *img, int len, int offset)
{
	int cnt, sofar, req, idx, errors;
	unsigned char readback[VFY_CHUNK];

	errors = 0;
	sofar = 0;
	while (sofar < len) {
		req = len - sofar;
		if (req > VFY_CHUNK)
			req = VFY_CHUNK;
		cnt = qib_sd7220_ram_xfer(dd, sdnum, sofar + offset,
					  readback, req, 1);
		if (cnt < req) {
			
			sofar = -1;
			break;
		}
		for (idx = 0; idx < cnt; ++idx) {
			if (readback[idx] != img[idx+sofar])
				++errors;
		}
		sofar += cnt;
	}
	return errors ? -errors : sofar;
}

static int
qib_sd7220_ib_load(struct qib_devdata *dd, const struct firmware *fw)
{
	return qib_sd7220_prog_ld(dd, IB_7220_SERDES, fw->data, fw->size, 0);
}

static int
qib_sd7220_ib_vfy(struct qib_devdata *dd, const struct firmware *fw)
{
	return qib_sd7220_prog_vfy(dd, IB_7220_SERDES, fw->data, fw->size, 0);
}

#define IB_SERDES_TRIM_DONE (1ULL << 11)
#define TRIM_TMO (30)

static int qib_sd_trimdone_poll(struct qib_devdata *dd)
{
	int trim_tmo, ret;
	uint64_t val;

	ret = 0;
	for (trim_tmo = 0; trim_tmo < TRIM_TMO; ++trim_tmo) {
		val = qib_read_kreg64(dd, kr_ibcstatus);
		if (val & IB_SERDES_TRIM_DONE) {
			ret = 1;
			break;
		}
		msleep(10);
	}
	if (trim_tmo >= TRIM_TMO) {
		qib_dev_err(dd, "No TRIMDONE in %d tries\n", trim_tmo);
		ret = 0;
	}
	return ret;
}

#define TX_FAST_ELT (9)


#define NUM_DDS_REGS 6
#define DDS_REG_MAP 0x76A910 

#define DDS_VAL(amp_d, main_d, ipst_d, ipre_d, amp_s, main_s, ipst_s, ipre_s) \
	{ { ((amp_d & 0x1F) << 1) | 1, ((amp_s & 0x1F) << 1) | 1, \
	  (main_d << 3) | 4 | (ipre_d >> 2), \
	  (main_s << 3) | 4 | (ipre_s >> 2), \
	  ((ipst_d & 0xF) << 1) | ((ipre_d & 3) << 6) | 0x21, \
	  ((ipst_s & 0xF) << 1) | ((ipre_s & 3) << 6) | 0x21 } }

static struct dds_init {
	uint8_t reg_vals[NUM_DDS_REGS];
} dds_init_vals[] = {
	
	
#define DDS_3M 0
	DDS_VAL(31, 19, 12, 0, 29, 22,  9, 0),
	DDS_VAL(31, 12, 15, 4, 31, 15, 15, 1),
	DDS_VAL(31, 13, 15, 3, 31, 16, 15, 0),
	DDS_VAL(31, 14, 15, 2, 31, 17, 14, 0),
	DDS_VAL(31, 15, 15, 1, 31, 18, 13, 0),
	DDS_VAL(31, 16, 15, 0, 31, 19, 12, 0),
	DDS_VAL(31, 17, 14, 0, 31, 20, 11, 0),
	DDS_VAL(31, 18, 13, 0, 30, 21, 10, 0),
	DDS_VAL(31, 20, 11, 0, 28, 23,  8, 0),
	DDS_VAL(31, 21, 10, 0, 27, 24,  7, 0),
	DDS_VAL(31, 22,  9, 0, 26, 25,  6, 0),
	DDS_VAL(30, 23,  8, 0, 25, 26,  5, 0),
	DDS_VAL(29, 24,  7, 0, 23, 27,  4, 0),
	
#define DDS_1M 13
	DDS_VAL(28, 25,  6, 0, 21, 28,  3, 0),
	DDS_VAL(27, 26,  5, 0, 19, 29,  2, 0),
	DDS_VAL(25, 27,  4, 0, 17, 30,  1, 0)
};

#define RXEQ_INIT_RDESC(elt, addr) (((elt) & 0xF) | ((addr) << 4))
#define RXEQ_VAL(elt, adr, val0, val1, val2, val3) \
	{RXEQ_INIT_RDESC((elt), (adr)), {(val0), (val1), (val2), (val3)} }

#define RXEQ_VAL_ALL(elt, adr, val)  \
	{RXEQ_INIT_RDESC((elt), (adr)), {(val), (val), (val), (val)} }

#define RXEQ_SDR_DFELTH 0
#define RXEQ_SDR_TLTH 0
#define RXEQ_SDR_G1CNT_Z1CNT 0x11
#define RXEQ_SDR_ZCNT 23

static struct rxeq_init {
	u16 rdesc;      
	u8  rdata[4];
} rxeq_init_vals[] = {
	
	RXEQ_VAL_ALL(7, 0x27, 0x10),
	
	RXEQ_VAL(7, 8,    0, 0, 0, 0), 
	RXEQ_VAL(7, 0x21, 0, 0, 0, 0), 
	
	RXEQ_VAL(7, 9,    2, 2, 2, 2), 
	RXEQ_VAL(7, 0x23, 2, 2, 2, 2), 
	
	RXEQ_VAL(7, 0x1B, 12, 12, 12, 12), 
	RXEQ_VAL(7, 0x1C, 12, 12, 12, 12), 
	
	RXEQ_VAL(7, 0x1E, 16, 16, 16, 16), 
	RXEQ_VAL(7, 0x1F, 16, 16, 16, 16), 
	
	RXEQ_VAL_ALL(6, 6, 0x20), 
	RXEQ_VAL_ALL(6, 6, 0), 
};

#define DDS_ROWS (16)
#define RXEQ_ROWS ARRAY_SIZE(rxeq_init_vals)

static int qib_sd_setvals(struct qib_devdata *dd)
{
	int idx, midx;
	int min_idx;     
	uint32_t dds_reg_map;
	u64 __iomem *taddr, *iaddr;
	uint64_t data;
	uint64_t sdctl;

	taddr = dd->kregbase + kr_serdes_maptable;
	iaddr = dd->kregbase + kr_serdes_ddsrxeq0;

	sdctl = qib_read_kreg64(dd, kr_ibserdesctrl);
	sdctl = (sdctl & ~(0x1f << 8)) | (NUM_DDS_REGS << 8);
	sdctl = (sdctl & ~(0x1f << 13)) | (RXEQ_ROWS << 13);
	qib_write_kreg(dd, kr_ibserdesctrl, sdctl);

	dds_reg_map = DDS_REG_MAP;
	for (idx = 0; idx < NUM_DDS_REGS; ++idx) {
		data = ((dds_reg_map & 0xF) << 4) | TX_FAST_ELT;
		writeq(data, iaddr + idx);
		mmiowb();
		qib_read_kreg32(dd, kr_scratch);
		dds_reg_map >>= 4;
		for (midx = 0; midx < DDS_ROWS; ++midx) {
			u64 __iomem *daddr = taddr + ((midx << 4) + idx);
			data = dds_init_vals[midx].reg_vals[idx];
			writeq(data, daddr);
			mmiowb();
			qib_read_kreg32(dd, kr_scratch);
		} 
	} 

	min_idx = idx; 
	taddr += 0x100; 
	
	for (idx = 0; idx < RXEQ_ROWS; ++idx) {
		int didx; 
		int vidx;

		
		didx = idx + min_idx;
		
		writeq(rxeq_init_vals[idx].rdesc, iaddr + didx);
		mmiowb();
		qib_read_kreg32(dd, kr_scratch);
		
		for (vidx = 0; vidx < 4; vidx++) {
			data = rxeq_init_vals[idx].rdata[vidx];
			writeq(data, taddr + (vidx << 6) + idx);
			mmiowb();
			qib_read_kreg32(dd, kr_scratch);
		}
	} 
	return 0;
}

#define CMUCTRL5 EPB_LOC(7, 0, 0x15)
#define RXHSCTRL0(chan) EPB_LOC(chan, 6, 0)
#define VCDL_DAC2(chan) EPB_LOC(chan, 6, 5)
#define VCDL_CTRL0(chan) EPB_LOC(chan, 6, 6)
#define VCDL_CTRL2(chan) EPB_LOC(chan, 6, 8)
#define START_EQ2(chan) EPB_LOC(chan, 7, 0x28)

/*
 * Repeat a "store" across all channels of the IB SerDes.
 * Although nominally it inherits the "read value" of the last
 * channel it modified, the only really useful return is <0 for
 * failure, >= 0 for success. The parameter 'loc' is assumed to
 * be the location in some channel of the register to be modified
 * The caller can specify use of the "gang write" option of EPB,
 * in which case we use the specified channel data for any fields
 * not explicitely written.
 */
static int ibsd_mod_allchnls(struct qib_devdata *dd, int loc, int val,
			     int mask)
{
	int ret = -1;
	int chnl;

	if (loc & EPB_GLOBAL_WR) {
		loc |= (1U << EPB_IB_QUAD0_CS_SHF);
		chnl = (loc >> (4 + EPB_ADDR_SHF)) & 7;
		if (mask != 0xFF) {
			ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES,
						 loc & ~EPB_GLOBAL_WR, 0, 0);
			if (ret < 0) {
				int sloc = loc >> EPB_ADDR_SHF;

				qib_dev_err(dd, "pre-read failed: elt %d,"
					    " addr 0x%X, chnl %d\n",
					    (sloc & 0xF),
					    (sloc >> 9) & 0x3f, chnl);
				return ret;
			}
			val = (ret & ~mask) | (val & mask);
		}
		loc &=  ~(7 << (4+EPB_ADDR_SHF));
		ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, loc, val, 0xFF);
		if (ret < 0) {
			int sloc = loc >> EPB_ADDR_SHF;

			qib_dev_err(dd, "Global WR failed: elt %d,"
				    " addr 0x%X, val %02X\n",
				    (sloc & 0xF), (sloc >> 9) & 0x3f, val);
		}
		return ret;
	}
	
	loc &=  ~(7 << (4+EPB_ADDR_SHF));
	loc |= (1U << EPB_IB_QUAD0_CS_SHF);
	for (chnl = 0; chnl < 4; ++chnl) {
		int cloc = loc | (chnl << (4+EPB_ADDR_SHF));

		ret = qib_sd7220_reg_mod(dd, IB_7220_SERDES, cloc, val, mask);
		if (ret < 0) {
			int sloc = loc >> EPB_ADDR_SHF;

			qib_dev_err(dd, "Write failed: elt %d,"
				    " addr 0x%X, chnl %d, val 0x%02X,"
				    " mask 0x%02X\n",
				    (sloc & 0xF), (sloc >> 9) & 0x3f, chnl,
				    val & 0xFF, mask & 0xFF);
			break;
		}
	}
	return ret;
}

static int set_dds_vals(struct qib_devdata *dd, struct dds_init *ddi)
{
	int ret;
	int idx, reg, data;
	uint32_t regmap;

	regmap = DDS_REG_MAP;
	for (idx = 0; idx < NUM_DDS_REGS; ++idx) {
		reg = (regmap & 0xF);
		regmap >>= 4;
		data = ddi->reg_vals[idx];
		
		ret = ibsd_mod_allchnls(dd, EPB_LOC(0, 9, reg), data, 0xFF);
		if (ret < 0)
			break;
	}
	return ret;
}

static int set_rxeq_vals(struct qib_devdata *dd, int vsel)
{
	int ret;
	int ridx;
	int cnt = ARRAY_SIZE(rxeq_init_vals);

	for (ridx = 0; ridx < cnt; ++ridx) {
		int elt, reg, val, loc;

		elt = rxeq_init_vals[ridx].rdesc & 0xF;
		reg = rxeq_init_vals[ridx].rdesc >> 4;
		loc = EPB_LOC(0, elt, reg);
		val = rxeq_init_vals[ridx].rdata[vsel];
		
		ret = ibsd_mod_allchnls(dd, loc, val, 0xFF);
		if (ret < 0)
			break;
	}
	return ret;
}

static unsigned qib_rxeq_set = 2;
module_param_named(rxeq_default_set, qib_rxeq_set, uint,
		   S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(rxeq_default_set,
		 "Which set [0..3] of Rx Equalization values is default");

static int qib_internal_presets(struct qib_devdata *dd)
{
	int ret = 0;

	ret = set_dds_vals(dd, dds_init_vals + DDS_3M);

	if (ret < 0)
		qib_dev_err(dd, "Failed to set default DDS values\n");
	ret = set_rxeq_vals(dd, qib_rxeq_set & 3);
	if (ret < 0)
		qib_dev_err(dd, "Failed to set default RXEQ values\n");
	return ret;
}

int qib_sd7220_presets(struct qib_devdata *dd)
{
	int ret = 0;

	if (!dd->cspec->presets_needed)
		return ret;
	dd->cspec->presets_needed = 0;
	
	qib_ibsd_reset(dd, 1);
	udelay(2);
	qib_sd_trimdone_monitor(dd, "link-down");

	ret = qib_internal_presets(dd);
	return ret;
}

static int qib_sd_trimself(struct qib_devdata *dd, int val)
{
	int loc = CMUCTRL5 | (1U << EPB_IB_QUAD0_CS_SHF);

	return qib_sd7220_reg_mod(dd, IB_7220_SERDES, loc, val, 0xFF);
}

static int qib_sd_early(struct qib_devdata *dd)
{
	int ret;

	ret = ibsd_mod_allchnls(dd, RXHSCTRL0(0) | EPB_GLOBAL_WR, 0xD4, 0xFF);
	if (ret < 0)
		goto bail;
	ret = ibsd_mod_allchnls(dd, START_EQ1(0) | EPB_GLOBAL_WR, 0x10, 0xFF);
	if (ret < 0)
		goto bail;
	ret = ibsd_mod_allchnls(dd, START_EQ2(0) | EPB_GLOBAL_WR, 0x30, 0xFF);
bail:
	return ret;
}

#define BACTRL(chnl) EPB_LOC(chnl, 6, 0x0E)
#define LDOUTCTRL1(chnl) EPB_LOC(chnl, 7, 6)
#define RXHSSTATUS(chnl) EPB_LOC(chnl, 6, 0xF)

static int qib_sd_dactrim(struct qib_devdata *dd)
{
	int ret;

	ret = ibsd_mod_allchnls(dd, VCDL_DAC2(0) | EPB_GLOBAL_WR, 0x2D, 0xFF);
	if (ret < 0)
		goto bail;

	
	ret = ibsd_mod_allchnls(dd, VCDL_CTRL2(0), 3, 0xF);
	if (ret < 0)
		goto bail;

	ret = ibsd_mod_allchnls(dd, BACTRL(0) | EPB_GLOBAL_WR, 0x40, 0xFF);
	if (ret < 0)
		goto bail;

	ret = ibsd_mod_allchnls(dd, LDOUTCTRL1(0) | EPB_GLOBAL_WR, 0x04, 0xFF);
	if (ret < 0)
		goto bail;

	ret = ibsd_mod_allchnls(dd, RXHSSTATUS(0) | EPB_GLOBAL_WR, 0x04, 0xFF);
	if (ret < 0)
		goto bail;

	udelay(415);

	ret = ibsd_mod_allchnls(dd, LDOUTCTRL1(0) | EPB_GLOBAL_WR, 0x00, 0xFF);

bail:
	return ret;
}

#define RELOCK_FIRST_MS 3
#define RXLSPPM(chan) EPB_LOC(chan, 0, 2)
void toggle_7220_rclkrls(struct qib_devdata *dd)
{
	int loc = RXLSPPM(0) | EPB_GLOBAL_WR;
	int ret;

	ret = ibsd_mod_allchnls(dd, loc, 0, 0x80);
	if (ret < 0)
		qib_dev_err(dd, "RCLKRLS failed to clear D7\n");
	else {
		udelay(1);
		ibsd_mod_allchnls(dd, loc, 0x80, 0x80);
	}
	
	udelay(1);
	ret = ibsd_mod_allchnls(dd, loc, 0, 0x80);
	if (ret < 0)
		qib_dev_err(dd, "RCLKRLS failed to clear D7\n");
	else {
		udelay(1);
		ibsd_mod_allchnls(dd, loc, 0x80, 0x80);
	}
	
	dd->f_xgxs_reset(dd->pport);
}

void shutdown_7220_relock_poll(struct qib_devdata *dd)
{
	if (dd->cspec->relock_timer_active)
		del_timer_sync(&dd->cspec->relock_timer);
}

static unsigned qib_relock_by_timer = 1;
module_param_named(relock_by_timer, qib_relock_by_timer, uint,
		   S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(relock_by_timer, "Allow relock attempt if link not up");

static void qib_run_relock(unsigned long opaque)
{
	struct qib_devdata *dd = (struct qib_devdata *)opaque;
	struct qib_pportdata *ppd = dd->pport;
	struct qib_chip_specific *cs = dd->cspec;
	int timeoff;

	if ((dd->flags & QIB_INITTED) && !(ppd->lflags &
	    (QIBL_IB_AUTONEG_INPROG | QIBL_LINKINIT | QIBL_LINKARMED |
	     QIBL_LINKACTIVE))) {
		if (qib_relock_by_timer) {
			if (!(ppd->lflags & QIBL_IB_LINK_DISABLED))
				toggle_7220_rclkrls(dd);
		}
		
		timeoff = cs->relock_interval << 1;
		if (timeoff > HZ)
			timeoff = HZ;
		cs->relock_interval = timeoff;
	} else
		timeoff = HZ;
	mod_timer(&cs->relock_timer, jiffies + timeoff);
}

void set_7220_relock_poll(struct qib_devdata *dd, int ibup)
{
	struct qib_chip_specific *cs = dd->cspec;

	if (ibup) {
		
		if (cs->relock_timer_active) {
			cs->relock_interval = HZ;
			mod_timer(&cs->relock_timer, jiffies + HZ);
		}
	} else {
		
		unsigned int timeout;

		timeout = msecs_to_jiffies(RELOCK_FIRST_MS);
		if (timeout == 0)
			timeout = 1;
		
		if (!cs->relock_timer_active) {
			cs->relock_timer_active = 1;
			init_timer(&cs->relock_timer);
			cs->relock_timer.function = qib_run_relock;
			cs->relock_timer.data = (unsigned long) dd;
			cs->relock_interval = timeout;
			cs->relock_timer.expires = jiffies + timeout;
			add_timer(&cs->relock_timer);
		} else {
			cs->relock_interval = timeout;
			mod_timer(&cs->relock_timer, jiffies + timeout);
		}
	}
}
