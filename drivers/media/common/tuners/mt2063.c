/*
 * Driver for mt2063 Micronas tuner
 *
 * Copyright (c) 2011 Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * This driver came from a driver originally written by:
 *		Henry Wang <Henry.wang@AzureWave.com>
 * Made publicly available by Terratec, at:
 *	http://linux.terratec.de/files/TERRATEC_H7/20110323_TERRATEC_H7_Linux.tar.gz
 * The original driver's license is GPL, as declared with MODULE_LICENSE()
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/videodev2.h>

#include "mt2063.h"

static unsigned int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Set Verbosity level");

#define dprintk(level, fmt, arg...) do {				\
if (debug >= level)							\
	printk(KERN_DEBUG "mt2063 %s: " fmt, __func__, ## arg);	\
} while (0)



#define MT2063_SPUR_PRESENT_ERR             (0x00800000)

#define MT2063_SPUR_CNT_MASK                (0x001f0000)
#define MT2063_SPUR_SHIFT                   (16)

#define MT2063_UPC_RANGE                    (0x04000000)

#define MT2063_DNC_RANGE                    (0x08000000)


#define MT2063_DECT_AVOID_US_FREQS      0x00000001

#define MT2063_DECT_AVOID_EURO_FREQS    0x00000002

#define MT2063_EXCLUDE_US_DECT_FREQUENCIES(s) (((s) & MT2063_DECT_AVOID_US_FREQS) != 0)

#define MT2063_EXCLUDE_EURO_DECT_FREQUENCIES(s) (((s) & MT2063_DECT_AVOID_EURO_FREQS) != 0)

enum MT2063_DECT_Avoid_Type {
	MT2063_NO_DECT_AVOIDANCE = 0,				
	MT2063_AVOID_US_DECT = MT2063_DECT_AVOID_US_FREQS,	
	MT2063_AVOID_EURO_DECT = MT2063_DECT_AVOID_EURO_FREQS,	
	MT2063_AVOID_BOTH					
};

#define MT2063_MAX_ZONES 48

struct MT2063_ExclZone_t {
	u32 min_;
	u32 max_;
	struct MT2063_ExclZone_t *next_;
};

struct MT2063_AvoidSpursData_t {
	u32 f_ref;
	u32 f_in;
	u32 f_LO1;
	u32 f_if1_Center;
	u32 f_if1_Request;
	u32 f_if1_bw;
	u32 f_LO2;
	u32 f_out;
	u32 f_out_bw;
	u32 f_LO1_Step;
	u32 f_LO2_Step;
	u32 f_LO1_FracN_Avoid;
	u32 f_LO2_FracN_Avoid;
	u32 f_zif_bw;
	u32 f_min_LO_Separation;
	u32 maxH1;
	u32 maxH2;
	enum MT2063_DECT_Avoid_Type avoidDECT;
	u32 bSpurPresent;
	u32 bSpurAvoided;
	u32 nSpursFound;
	u32 nZones;
	struct MT2063_ExclZone_t *freeZones;
	struct MT2063_ExclZone_t *usedZones;
	struct MT2063_ExclZone_t MT2063_ExclZones[MT2063_MAX_ZONES];
};

enum MT2063_Mask_Bits {
	MT2063_REG_SD = 0x0040,		
	MT2063_SRO_SD = 0x0020,		
	MT2063_AFC_SD = 0x0010,		
	MT2063_PD_SD = 0x0002,		
	MT2063_PDADC_SD = 0x0001,	
	MT2063_VCO_SD = 0x8000,		
	MT2063_LTX_SD = 0x4000,		
	MT2063_LT1_SD = 0x2000,		
	MT2063_LNA_SD = 0x1000,		
	MT2063_UPC_SD = 0x0800,		
	MT2063_DNC_SD = 0x0400,		
	MT2063_VGA_SD = 0x0200,		
	MT2063_AMP_SD = 0x0100,		
	MT2063_ALL_SD = 0xFF73,		
	MT2063_NONE_SD = 0x0000		
};

enum MT2063_DNC_Output_Enable {
	MT2063_DNC_NONE = 0,
	MT2063_DNC_1,
	MT2063_DNC_2,
	MT2063_DNC_BOTH
};

enum MT2063_Register_Offsets {
	MT2063_REG_PART_REV = 0,	
	MT2063_REG_LO1CQ_1,		
	MT2063_REG_LO1CQ_2,		
	MT2063_REG_LO2CQ_1,		
	MT2063_REG_LO2CQ_2,		
	MT2063_REG_LO2CQ_3,		
	MT2063_REG_RSVD_06,		
	MT2063_REG_LO_STATUS,		
	MT2063_REG_FIFFC,		
	MT2063_REG_CLEARTUNE,		
	MT2063_REG_ADC_OUT,		
	MT2063_REG_LO1C_1,		
	MT2063_REG_LO1C_2,		
	MT2063_REG_LO2C_1,		
	MT2063_REG_LO2C_2,		
	MT2063_REG_LO2C_3,		
	MT2063_REG_RSVD_10,		
	MT2063_REG_PWR_1,		
	MT2063_REG_PWR_2,		
	MT2063_REG_TEMP_STATUS,		
	MT2063_REG_XO_STATUS,		
	MT2063_REG_RF_STATUS,		
	MT2063_REG_FIF_STATUS,		
	MT2063_REG_LNA_OV,		
	MT2063_REG_RF_OV,		
	MT2063_REG_FIF_OV,		
	MT2063_REG_LNA_TGT,		
	MT2063_REG_PD1_TGT,		
	MT2063_REG_PD2_TGT,		
	MT2063_REG_RSVD_1D,		
	MT2063_REG_RSVD_1E,		
	MT2063_REG_RSVD_1F,		
	MT2063_REG_RSVD_20,		
	MT2063_REG_BYP_CTRL,		
	MT2063_REG_RSVD_22,		
	MT2063_REG_RSVD_23,		
	MT2063_REG_RSVD_24,		
	MT2063_REG_RSVD_25,		
	MT2063_REG_RSVD_26,		
	MT2063_REG_RSVD_27,		
	MT2063_REG_FIFF_CTRL,		
	MT2063_REG_FIFF_OFFSET,		
	MT2063_REG_CTUNE_CTRL,		
	MT2063_REG_CTUNE_OV,		
	MT2063_REG_CTRL_2C,		
	MT2063_REG_FIFF_CTRL2,		
	MT2063_REG_RSVD_2E,		
	MT2063_REG_DNC_GAIN,		
	MT2063_REG_VGA_GAIN,		
	MT2063_REG_RSVD_31,		
	MT2063_REG_TEMP_SEL,		
	MT2063_REG_RSVD_33,		
	MT2063_REG_RSVD_34,		
	MT2063_REG_RSVD_35,		
	MT2063_REG_RSVD_36,		
	MT2063_REG_RSVD_37,		
	MT2063_REG_RSVD_38,		
	MT2063_REG_RSVD_39,		
	MT2063_REG_RSVD_3A,		
	MT2063_REG_RSVD_3B,		
	MT2063_REG_RSVD_3C,		
	MT2063_REG_END_REGS
};

struct mt2063_state {
	struct i2c_adapter *i2c;

	bool init;

	const struct mt2063_config *config;
	struct dvb_tuner_ops ops;
	struct dvb_frontend *frontend;
	struct tuner_state status;

	u32 frequency;
	u32 srate;
	u32 bandwidth;
	u32 reference;

	u32 tuner_id;
	struct MT2063_AvoidSpursData_t AS_Data;
	u32 f_IF1_actual;
	u32 rcvr_mode;
	u32 ctfilt_sw;
	u32 CTFiltMax[31];
	u32 num_regs;
	u8 reg[MT2063_REG_END_REGS];
};

static u32 mt2063_write(struct mt2063_state *state, u8 reg, u8 *data, u32 len)
{
	struct dvb_frontend *fe = state->frontend;
	int ret;
	u8 buf[60];
	struct i2c_msg msg = {
		.addr = state->config->tuner_address,
		.flags = 0,
		.buf = buf,
		.len = len + 1
	};

	dprintk(2, "\n");

	msg.buf[0] = reg;
	memcpy(msg.buf + 1, data, len);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	ret = i2c_transfer(state->i2c, &msg, 1);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	if (ret < 0)
		printk(KERN_ERR "%s error ret=%d\n", __func__, ret);

	return ret;
}

static u32 mt2063_setreg(struct mt2063_state *state, u8 reg, u8 val)
{
	u32 status;

	dprintk(2, "\n");

	if (reg >= MT2063_REG_END_REGS)
		return -ERANGE;

	status = mt2063_write(state, reg, &val, 1);
	if (status < 0)
		return status;

	state->reg[reg] = val;

	return 0;
}

static u32 mt2063_read(struct mt2063_state *state,
			   u8 subAddress, u8 *pData, u32 cnt)
{
	u32 status = 0;	
	struct dvb_frontend *fe = state->frontend;
	u32 i = 0;

	dprintk(2, "addr 0x%02x, cnt %d\n", subAddress, cnt);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	for (i = 0; i < cnt; i++) {
		u8 b0[] = { subAddress + i };
		struct i2c_msg msg[] = {
			{
				.addr = state->config->tuner_address,
				.flags = 0,
				.buf = b0,
				.len = 1
			}, {
				.addr = state->config->tuner_address,
				.flags = I2C_M_RD,
				.buf = pData + i,
				.len = 1
			}
		};

		status = i2c_transfer(state->i2c, msg, 2);
		dprintk(2, "addr 0x%02x, ret = %d, val = 0x%02x\n",
			   subAddress + i, status, *(pData + i));
		if (status < 0)
			break;
	}
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	if (status < 0)
		printk(KERN_ERR "Can't read from address 0x%02x,\n",
		       subAddress + i);

	return status;
}

static int MT2063_Sleep(struct dvb_frontend *fe)
{
	msleep(100);

	return 0;
}


#define ceil(n, d) (((n) < 0) ? (-((-(n))/(d))) : (n)/(d) + ((n)%(d) != 0))
#define floor(n, d) (((n) < 0) ? (-((-(n))/(d))) - ((n)%(d) != 0) : (n)/(d))

struct MT2063_FIFZone_t {
	s32 min_;
	s32 max_;
};

static struct MT2063_ExclZone_t *InsertNode(struct MT2063_AvoidSpursData_t
					    *pAS_Info,
					    struct MT2063_ExclZone_t *pPrevNode)
{
	struct MT2063_ExclZone_t *pNode;

	dprintk(2, "\n");

	
	if (pAS_Info->freeZones != NULL) {
		
		pNode = pAS_Info->freeZones;
		pAS_Info->freeZones = pNode->next_;
	} else {
		
		pNode = &pAS_Info->MT2063_ExclZones[pAS_Info->nZones];
	}

	if (pPrevNode != NULL) {
		pNode->next_ = pPrevNode->next_;
		pPrevNode->next_ = pNode;
	} else {		

		pNode->next_ = pAS_Info->usedZones;
		pAS_Info->usedZones = pNode;
	}

	pAS_Info->nZones++;
	return pNode;
}

static struct MT2063_ExclZone_t *RemoveNode(struct MT2063_AvoidSpursData_t
					    *pAS_Info,
					    struct MT2063_ExclZone_t *pPrevNode,
					    struct MT2063_ExclZone_t
					    *pNodeToRemove)
{
	struct MT2063_ExclZone_t *pNext = pNodeToRemove->next_;

	dprintk(2, "\n");

	
	if (pPrevNode != NULL)
		pPrevNode->next_ = pNext;

	
	pNodeToRemove->next_ = pAS_Info->freeZones;
	pAS_Info->freeZones = pNodeToRemove;

	
	pAS_Info->nZones--;

	return pNext;
}

static void MT2063_AddExclZone(struct MT2063_AvoidSpursData_t *pAS_Info,
			       u32 f_min, u32 f_max)
{
	struct MT2063_ExclZone_t *pNode = pAS_Info->usedZones;
	struct MT2063_ExclZone_t *pPrev = NULL;
	struct MT2063_ExclZone_t *pNext = NULL;

	dprintk(2, "\n");

	
	if ((f_max > (pAS_Info->f_if1_Center - (pAS_Info->f_if1_bw / 2)))
	    && (f_min < (pAS_Info->f_if1_Center + (pAS_Info->f_if1_bw / 2)))
	    && (f_min < f_max)) {

		
		while ((pNode != NULL) && (pNode->max_ < f_min)) {
			pPrev = pNode;
			pNode = pNode->next_;
		}

		if ((pNode != NULL) && (pNode->min_ < f_max)) {
			
			if (f_min < pNode->min_)
				pNode->min_ = f_min;
			if (f_max > pNode->max_)
				pNode->max_ = f_max;
		} else {
			pNode = InsertNode(pAS_Info, pPrev);
			pNode->min_ = f_min;
			pNode->max_ = f_max;
		}

		
		pNext = pNode->next_;
		while ((pNext != NULL) && (pNext->min_ < pNode->max_)) {
			if (pNext->max_ > pNode->max_)
				pNode->max_ = pNext->max_;
			
			pNext = RemoveNode(pAS_Info, pNode, pNext);
		}
	}
}

static void MT2063_ResetExclZones(struct MT2063_AvoidSpursData_t *pAS_Info)
{
	u32 center;

	dprintk(2, "\n");

	pAS_Info->nZones = 0;	
	pAS_Info->usedZones = NULL;	
	pAS_Info->freeZones = NULL;	

	center =
	    pAS_Info->f_ref *
	    ((pAS_Info->f_if1_Center - pAS_Info->f_if1_bw / 2 +
	      pAS_Info->f_in) / pAS_Info->f_ref) - pAS_Info->f_in;
	while (center <
	       pAS_Info->f_if1_Center + pAS_Info->f_if1_bw / 2 +
	       pAS_Info->f_LO1_FracN_Avoid) {
		
		MT2063_AddExclZone(pAS_Info,
				   center - pAS_Info->f_LO1_FracN_Avoid,
				   center - 1);
		MT2063_AddExclZone(pAS_Info, center + 1,
				   center + pAS_Info->f_LO1_FracN_Avoid);
		center += pAS_Info->f_ref;
	}

	center =
	    pAS_Info->f_ref *
	    ((pAS_Info->f_if1_Center - pAS_Info->f_if1_bw / 2 -
	      pAS_Info->f_out) / pAS_Info->f_ref) + pAS_Info->f_out;
	while (center <
	       pAS_Info->f_if1_Center + pAS_Info->f_if1_bw / 2 +
	       pAS_Info->f_LO2_FracN_Avoid) {
		
		MT2063_AddExclZone(pAS_Info,
				   center - pAS_Info->f_LO2_FracN_Avoid,
				   center - 1);
		MT2063_AddExclZone(pAS_Info, center + 1,
				   center + pAS_Info->f_LO2_FracN_Avoid);
		center += pAS_Info->f_ref;
	}

	if (MT2063_EXCLUDE_US_DECT_FREQUENCIES(pAS_Info->avoidDECT)) {
		
		MT2063_AddExclZone(pAS_Info, 1920836000 - pAS_Info->f_in, 1922236000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1922564000 - pAS_Info->f_in, 1923964000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1924292000 - pAS_Info->f_in, 1925692000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1926020000 - pAS_Info->f_in, 1927420000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1927748000 - pAS_Info->f_in, 1929148000 - pAS_Info->f_in);	
	}

	if (MT2063_EXCLUDE_EURO_DECT_FREQUENCIES(pAS_Info->avoidDECT)) {
		MT2063_AddExclZone(pAS_Info, 1896644000 - pAS_Info->f_in, 1898044000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1894916000 - pAS_Info->f_in, 1896316000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1893188000 - pAS_Info->f_in, 1894588000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1891460000 - pAS_Info->f_in, 1892860000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1889732000 - pAS_Info->f_in, 1891132000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1888004000 - pAS_Info->f_in, 1889404000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1886276000 - pAS_Info->f_in, 1887676000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1884548000 - pAS_Info->f_in, 1885948000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1882820000 - pAS_Info->f_in, 1884220000 - pAS_Info->f_in);	
		MT2063_AddExclZone(pAS_Info, 1881092000 - pAS_Info->f_in, 1882492000 - pAS_Info->f_in);	
	}
}

static u32 MT2063_ChooseFirstIF(struct MT2063_AvoidSpursData_t *pAS_Info)
{
	const u32 f_Desired =
	    pAS_Info->f_LO1_Step *
	    ((pAS_Info->f_if1_Request + pAS_Info->f_in +
	      pAS_Info->f_LO1_Step / 2) / pAS_Info->f_LO1_Step) -
	    pAS_Info->f_in;
	const u32 f_Step =
	    (pAS_Info->f_LO1_Step >
	     pAS_Info->f_LO2_Step) ? pAS_Info->f_LO1_Step : pAS_Info->
	    f_LO2_Step;
	u32 f_Center;
	s32 i;
	s32 j = 0;
	u32 bDesiredExcluded = 0;
	u32 bZeroExcluded = 0;
	s32 tmpMin, tmpMax;
	s32 bestDiff;
	struct MT2063_ExclZone_t *pNode = pAS_Info->usedZones;
	struct MT2063_FIFZone_t zones[MT2063_MAX_ZONES];

	dprintk(2, "\n");

	if (pAS_Info->nZones == 0)
		return f_Desired;

	if (pAS_Info->f_if1_Center > f_Desired)
		f_Center =
		    f_Desired +
		    f_Step *
		    ((pAS_Info->f_if1_Center - f_Desired +
		      f_Step / 2) / f_Step);
	else
		f_Center =
		    f_Desired -
		    f_Step *
		    ((f_Desired - pAS_Info->f_if1_Center +
		      f_Step / 2) / f_Step);

	while (pNode != NULL) {
		
		tmpMin =
		    floor((s32) (pNode->min_ - f_Center), (s32) f_Step);

		
		tmpMax =
		    ceil((s32) (pNode->max_ - f_Center), (s32) f_Step);

		if ((pNode->min_ < f_Desired) && (pNode->max_ > f_Desired))
			bDesiredExcluded = 1;

		if ((tmpMin < 0) && (tmpMax > 0))
			bZeroExcluded = 1;

		
		if ((j > 0) && (tmpMin < zones[j - 1].max_))
			zones[j - 1].max_ = tmpMax;
		else {
			
			zones[j].min_ = tmpMin;
			zones[j].max_ = tmpMax;
			j++;
		}
		pNode = pNode->next_;
	}

	if (bDesiredExcluded == 0)
		return f_Desired;

	if (bZeroExcluded == 0)
		return f_Center;

	
	bestDiff = zones[0].min_;
	for (i = 0; i < j; i++) {
		if (abs(zones[i].min_) < abs(bestDiff))
			bestDiff = zones[i].min_;
		if (abs(zones[i].max_) < abs(bestDiff))
			bestDiff = zones[i].max_;
	}

	if (bestDiff < 0)
		return f_Center - ((u32) (-bestDiff) * f_Step);

	return f_Center + (bestDiff * f_Step);
}

static u32 MT2063_gcd(u32 u, u32 v)
{
	u32 r;

	while (v != 0) {
		r = u % v;
		u = v;
		v = r;
	}

	return u;
}

static u32 IsSpurInBand(struct MT2063_AvoidSpursData_t *pAS_Info,
			u32 *fm, u32 * fp)
{
	u32 n, n0;
	const u32 f_LO1 = pAS_Info->f_LO1;
	const u32 f_LO2 = pAS_Info->f_LO2;
	const u32 d = pAS_Info->f_out + pAS_Info->f_out_bw / 2;
	const u32 c = d - pAS_Info->f_out_bw;
	const u32 f = pAS_Info->f_zif_bw / 2;
	const u32 f_Scale = (f_LO1 / (UINT_MAX / 2 / pAS_Info->maxH1)) + 1;
	s32 f_nsLO1, f_nsLO2;
	s32 f_Spur;
	u32 ma, mb, mc, md, me, mf;
	u32 lo_gcd, gd_Scale, gc_Scale, gf_Scale, hgds, hgfs, hgcs;

	dprintk(2, "\n");

	*fm = 0;

	lo_gcd = MT2063_gcd(f_LO1, f_LO2);
	gd_Scale = max((u32) MT2063_gcd(lo_gcd, d), f_Scale);
	hgds = gd_Scale / 2;
	gc_Scale = max((u32) MT2063_gcd(lo_gcd, c), f_Scale);
	hgcs = gc_Scale / 2;
	gf_Scale = max((u32) MT2063_gcd(lo_gcd, f), f_Scale);
	hgfs = gf_Scale / 2;

	n0 = DIV_ROUND_UP(f_LO2 - d, f_LO1 - f_LO2);

	
	for (n = n0; n <= pAS_Info->maxH1; ++n) {
		md = (n * ((f_LO1 + hgds) / gd_Scale) -
		      ((d + hgds) / gd_Scale)) / ((f_LO2 + hgds) / gd_Scale);

		
		if (md >= pAS_Info->maxH1)
			break;

		ma = (n * ((f_LO1 + hgds) / gd_Scale) +
		      ((d + hgds) / gd_Scale)) / ((f_LO2 + hgds) / gd_Scale);

		
		if (md == ma)
			continue;

		mc = (n * ((f_LO1 + hgcs) / gc_Scale) -
		      ((c + hgcs) / gc_Scale)) / ((f_LO2 + hgcs) / gc_Scale);
		if (mc != md) {
			f_nsLO1 = (s32) (n * (f_LO1 / gc_Scale));
			f_nsLO2 = (s32) (mc * (f_LO2 / gc_Scale));
			f_Spur =
			    (gc_Scale * (f_nsLO1 - f_nsLO2)) +
			    n * (f_LO1 % gc_Scale) - mc * (f_LO2 % gc_Scale);

			*fp = ((f_Spur - (s32) c) / (mc - n)) + 1;
			*fm = (((s32) d - f_Spur) / (mc - n)) + 1;
			return 1;
		}

		
		me = (n * ((f_LO1 + hgfs) / gf_Scale) +
		      ((f + hgfs) / gf_Scale)) / ((f_LO2 + hgfs) / gf_Scale);
		mf = (n * ((f_LO1 + hgfs) / gf_Scale) -
		      ((f + hgfs) / gf_Scale)) / ((f_LO2 + hgfs) / gf_Scale);
		if (me != mf) {
			f_nsLO1 = n * (f_LO1 / gf_Scale);
			f_nsLO2 = me * (f_LO2 / gf_Scale);
			f_Spur =
			    (gf_Scale * (f_nsLO1 - f_nsLO2)) +
			    n * (f_LO1 % gf_Scale) - me * (f_LO2 % gf_Scale);

			*fp = ((f_Spur + (s32) f) / (me - n)) + 1;
			*fm = (((s32) f - f_Spur) / (me - n)) + 1;
			return 1;
		}

		mb = (n * ((f_LO1 + hgcs) / gc_Scale) +
		      ((c + hgcs) / gc_Scale)) / ((f_LO2 + hgcs) / gc_Scale);
		if (ma != mb) {
			f_nsLO1 = n * (f_LO1 / gc_Scale);
			f_nsLO2 = ma * (f_LO2 / gc_Scale);
			f_Spur =
			    (gc_Scale * (f_nsLO1 - f_nsLO2)) +
			    n * (f_LO1 % gc_Scale) - ma * (f_LO2 % gc_Scale);

			*fp = (((s32) d + f_Spur) / (ma - n)) + 1;
			*fm = (-(f_Spur + (s32) c) / (ma - n)) + 1;
			return 1;
		}
	}

	
	return 0;
}

static u32 MT2063_AvoidSpurs(struct MT2063_AvoidSpursData_t *pAS_Info)
{
	u32 status = 0;
	u32 fm, fp;		
	pAS_Info->bSpurAvoided = 0;
	pAS_Info->nSpursFound = 0;

	dprintk(2, "\n");

	if (pAS_Info->maxH1 == 0)
		return 0;

	pAS_Info->bSpurPresent = IsSpurInBand(pAS_Info, &fm, &fp);
	if (pAS_Info->bSpurPresent) {
		u32 zfIF1 = pAS_Info->f_LO1 - pAS_Info->f_in;	
		u32 zfLO1 = pAS_Info->f_LO1;	
		u32 zfLO2 = pAS_Info->f_LO2;	
		u32 delta_IF1;
		u32 new_IF1;

		do {
			pAS_Info->nSpursFound++;

			
			MT2063_AddExclZone(pAS_Info, zfIF1 - fm, zfIF1 + fp);

			
			new_IF1 = MT2063_ChooseFirstIF(pAS_Info);

			if (new_IF1 > zfIF1) {
				pAS_Info->f_LO1 += (new_IF1 - zfIF1);
				pAS_Info->f_LO2 += (new_IF1 - zfIF1);
			} else {
				pAS_Info->f_LO1 -= (zfIF1 - new_IF1);
				pAS_Info->f_LO2 -= (zfIF1 - new_IF1);
			}
			zfIF1 = new_IF1;

			if (zfIF1 > pAS_Info->f_if1_Center)
				delta_IF1 = zfIF1 - pAS_Info->f_if1_Center;
			else
				delta_IF1 = pAS_Info->f_if1_Center - zfIF1;

			pAS_Info->bSpurPresent = IsSpurInBand(pAS_Info, &fm, &fp);
		} while ((2 * delta_IF1 + pAS_Info->f_out_bw <= pAS_Info->f_if1_bw) && pAS_Info->bSpurPresent);

		if (pAS_Info->bSpurPresent == 1) {
			status |= MT2063_SPUR_PRESENT_ERR;
			pAS_Info->f_LO1 = zfLO1;
			pAS_Info->f_LO2 = zfLO2;
		} else
			pAS_Info->bSpurAvoided = 1;
	}

	status |=
	    ((pAS_Info->
	      nSpursFound << MT2063_SPUR_SHIFT) & MT2063_SPUR_CNT_MASK);

	return status;
}

#define MT2063_REF_FREQ          (16000000UL)	
#define MT2063_IF1_BW            (22000000UL)	
#define MT2063_TUNE_STEP_SIZE       (50000UL)	
#define MT2063_SPUR_STEP_HZ        (250000UL)	
#define MT2063_ZIF_BW             (2000000UL)	
#define MT2063_MAX_HARMONICS_1         (15UL)	
#define MT2063_MAX_HARMONICS_2          (5UL)	
#define MT2063_MIN_LO_SEP         (1000000UL)	
#define MT2063_LO1_FRACN_AVOID          (0UL)	
#define MT2063_LO2_FRACN_AVOID     (199999UL)	
#define MT2063_MIN_FIN_FREQ      (44000000UL)	
#define MT2063_MAX_FIN_FREQ    (1100000000UL)	
#define MT2063_MIN_FOUT_FREQ     (36000000UL)	
#define MT2063_MAX_FOUT_FREQ     (57000000UL)	
#define MT2063_MIN_DNC_FREQ    (1293000000UL)	
#define MT2063_MAX_DNC_FREQ    (1614000000UL)	
#define MT2063_MIN_UPC_FREQ    (1396000000UL)	
#define MT2063_MAX_UPC_FREQ    (2750000000UL)	

#define MT2063_B0       (0x9B)
#define MT2063_B1       (0x9C)
#define MT2063_B2       (0x9D)
#define MT2063_B3       (0x9E)

static unsigned int mt2063_lockStatus(struct mt2063_state *state)
{
	const u32 nMaxWait = 100;	
	const u32 nPollRate = 2;	
	const u32 nMaxLoops = nMaxWait / nPollRate;
	const u8 LO1LK = 0x80;
	u8 LO2LK = 0x08;
	u32 status;
	u32 nDelays = 0;

	dprintk(2, "\n");

	
	if (state->tuner_id == MT2063_B0)
		LO2LK = 0x40;

	do {
		status = mt2063_read(state, MT2063_REG_LO_STATUS,
				     &state->reg[MT2063_REG_LO_STATUS], 1);

		if (status < 0)
			return status;

		if ((state->reg[MT2063_REG_LO_STATUS] & (LO1LK | LO2LK)) ==
		    (LO1LK | LO2LK)) {
			return TUNER_STATUS_LOCKED | TUNER_STATUS_STEREO;
		}
		msleep(nPollRate);	
	} while (++nDelays < nMaxLoops);

	return 0;
}


enum mt2063_delivery_sys {
	MT2063_CABLE_QAM = 0,
	MT2063_CABLE_ANALOG,
	MT2063_OFFAIR_COFDM,
	MT2063_OFFAIR_COFDM_SAWLESS,
	MT2063_OFFAIR_ANALOG,
	MT2063_OFFAIR_8VSB,
	MT2063_NUM_RCVR_MODES
};

static const char *mt2063_mode_name[] = {
	[MT2063_CABLE_QAM]		= "digital cable",
	[MT2063_CABLE_ANALOG]		= "analog cable",
	[MT2063_OFFAIR_COFDM]		= "digital offair",
	[MT2063_OFFAIR_COFDM_SAWLESS]	= "digital offair without SAW",
	[MT2063_OFFAIR_ANALOG]		= "analog offair",
	[MT2063_OFFAIR_8VSB]		= "analog offair 8vsb",
};

static const u8 RFAGCEN[]	= {  0,  0,  0,  0,  0,  0 };
static const u8 LNARIN[]	= {  0,  0,  3,  3,  3,  3 };
static const u8 FIFFQEN[]	= {  1,  1,  1,  1,  1,  1 };
static const u8 FIFFQ[]		= {  0,  0,  0,  0,  0,  0 };
static const u8 DNC1GC[]	= {  0,  0,  0,  0,  0,  0 };
static const u8 DNC2GC[]	= {  0,  0,  0,  0,  0,  0 };
static const u8 ACLNAMAX[]	= { 31, 31, 31, 31, 31, 31 };
static const u8 LNATGT[]	= { 44, 43, 43, 43, 43, 43 };
static const u8 RFOVDIS[]	= {  0,  0,  0,  0,  0,  0 };
static const u8 ACRFMAX[]	= { 31, 31, 31, 31, 31, 31 };
static const u8 PD1TGT[]	= { 36, 36, 38, 38, 36, 38 };
static const u8 FIFOVDIS[]	= {  0,  0,  0,  0,  0,  0 };
static const u8 ACFIFMAX[]	= { 29, 29, 29, 29, 29, 29 };
static const u8 PD2TGT[]	= { 40, 33, 38, 42, 30, 38 };

static u32 mt2063_get_dnc_output_enable(struct mt2063_state *state,
					enum MT2063_DNC_Output_Enable *pValue)
{
	dprintk(2, "\n");

	if ((state->reg[MT2063_REG_DNC_GAIN] & 0x03) == 0x03) {	
		if ((state->reg[MT2063_REG_VGA_GAIN] & 0x03) == 0x03)	
			*pValue = MT2063_DNC_NONE;
		else
			*pValue = MT2063_DNC_2;
	} else {	
		if ((state->reg[MT2063_REG_VGA_GAIN] & 0x03) == 0x03)	
			*pValue = MT2063_DNC_1;
		else
			*pValue = MT2063_DNC_BOTH;
	}
	return 0;
}

static u32 mt2063_set_dnc_output_enable(struct mt2063_state *state,
					enum MT2063_DNC_Output_Enable nValue)
{
	u32 status = 0;	
	u8 val = 0;

	dprintk(2, "\n");

	
	switch (nValue) {
	case MT2063_DNC_NONE:
		val = (state->reg[MT2063_REG_DNC_GAIN] & 0xFC) | 0x03;	
		if (state->reg[MT2063_REG_DNC_GAIN] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_DNC_GAIN,
					  val);

		val = (state->reg[MT2063_REG_VGA_GAIN] & 0xFC) | 0x03;	
		if (state->reg[MT2063_REG_VGA_GAIN] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_VGA_GAIN,
					  val);

		val = (state->reg[MT2063_REG_RSVD_20] & ~0x40);	
		if (state->reg[MT2063_REG_RSVD_20] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_RSVD_20,
					  val);

		break;
	case MT2063_DNC_1:
		val = (state->reg[MT2063_REG_DNC_GAIN] & 0xFC) | (DNC1GC[state->rcvr_mode] & 0x03);	
		if (state->reg[MT2063_REG_DNC_GAIN] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_DNC_GAIN,
					  val);

		val = (state->reg[MT2063_REG_VGA_GAIN] & 0xFC) | 0x03;	
		if (state->reg[MT2063_REG_VGA_GAIN] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_VGA_GAIN,
					  val);

		val = (state->reg[MT2063_REG_RSVD_20] & ~0x40);	
		if (state->reg[MT2063_REG_RSVD_20] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_RSVD_20,
					  val);

		break;
	case MT2063_DNC_2:
		val = (state->reg[MT2063_REG_DNC_GAIN] & 0xFC) | 0x03;	
		if (state->reg[MT2063_REG_DNC_GAIN] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_DNC_GAIN,
					  val);

		val = (state->reg[MT2063_REG_VGA_GAIN] & 0xFC) | (DNC2GC[state->rcvr_mode] & 0x03);	
		if (state->reg[MT2063_REG_VGA_GAIN] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_VGA_GAIN,
					  val);

		val = (state->reg[MT2063_REG_RSVD_20] | 0x40);	
		if (state->reg[MT2063_REG_RSVD_20] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_RSVD_20,
					  val);

		break;
	case MT2063_DNC_BOTH:
		val = (state->reg[MT2063_REG_DNC_GAIN] & 0xFC) | (DNC1GC[state->rcvr_mode] & 0x03);	
		if (state->reg[MT2063_REG_DNC_GAIN] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_DNC_GAIN,
					  val);

		val = (state->reg[MT2063_REG_VGA_GAIN] & 0xFC) | (DNC2GC[state->rcvr_mode] & 0x03);	
		if (state->reg[MT2063_REG_VGA_GAIN] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_VGA_GAIN,
					  val);

		val = (state->reg[MT2063_REG_RSVD_20] | 0x40);	
		if (state->reg[MT2063_REG_RSVD_20] !=
		    val)
			status |=
			    mt2063_setreg(state,
					  MT2063_REG_RSVD_20,
					  val);

		break;
	default:
		break;
	}

	return status;
}


static u32 MT2063_SetReceiverMode(struct mt2063_state *state,
				  enum mt2063_delivery_sys Mode)
{
	u32 status = 0;	
	u8 val;
	u32 longval;

	dprintk(2, "\n");

	if (Mode >= MT2063_NUM_RCVR_MODES)
		status = -ERANGE;

	
	if (status >= 0) {
		val =
		    (state->
		     reg[MT2063_REG_PD1_TGT] & (u8) ~0x40) | (RFAGCEN[Mode]
								   ? 0x40 :
								   0x00);
		if (state->reg[MT2063_REG_PD1_TGT] != val)
			status |= mt2063_setreg(state, MT2063_REG_PD1_TGT, val);
	}

	
	if (status >= 0) {
		u8 val = (state->reg[MT2063_REG_CTRL_2C] & (u8) ~0x03) |
			 (LNARIN[Mode] & 0x03);
		if (state->reg[MT2063_REG_CTRL_2C] != val)
			status |= mt2063_setreg(state, MT2063_REG_CTRL_2C, val);
	}

	
	if (status >= 0) {
		val =
		    (state->
		     reg[MT2063_REG_FIFF_CTRL2] & (u8) ~0xF0) |
		    (FIFFQEN[Mode] << 7) | (FIFFQ[Mode] << 4);
		if (state->reg[MT2063_REG_FIFF_CTRL2] != val) {
			status |=
			    mt2063_setreg(state, MT2063_REG_FIFF_CTRL2, val);
			
			val =
			    (state->reg[MT2063_REG_FIFF_CTRL] | (u8) 0x01);
			status |=
			    mt2063_setreg(state, MT2063_REG_FIFF_CTRL, val);
			val =
			    (state->
			     reg[MT2063_REG_FIFF_CTRL] & (u8) ~0x01);
			status |=
			    mt2063_setreg(state, MT2063_REG_FIFF_CTRL, val);
		}
	}

	
	status |= mt2063_get_dnc_output_enable(state, &longval);
	status |= mt2063_set_dnc_output_enable(state, longval);

	
	if (status >= 0) {
		u8 val = (state->reg[MT2063_REG_LNA_OV] & (u8) ~0x1F) |
			 (ACLNAMAX[Mode] & 0x1F);
		if (state->reg[MT2063_REG_LNA_OV] != val)
			status |= mt2063_setreg(state, MT2063_REG_LNA_OV, val);
	}

	
	if (status >= 0) {
		u8 val = (state->reg[MT2063_REG_LNA_TGT] & (u8) ~0x3F) |
			 (LNATGT[Mode] & 0x3F);
		if (state->reg[MT2063_REG_LNA_TGT] != val)
			status |= mt2063_setreg(state, MT2063_REG_LNA_TGT, val);
	}

	
	if (status >= 0) {
		u8 val = (state->reg[MT2063_REG_RF_OV] & (u8) ~0x1F) |
			 (ACRFMAX[Mode] & 0x1F);
		if (state->reg[MT2063_REG_RF_OV] != val)
			status |= mt2063_setreg(state, MT2063_REG_RF_OV, val);
	}

	
	if (status >= 0) {
		u8 val = (state->reg[MT2063_REG_PD1_TGT] & (u8) ~0x3F) |
			 (PD1TGT[Mode] & 0x3F);
		if (state->reg[MT2063_REG_PD1_TGT] != val)
			status |= mt2063_setreg(state, MT2063_REG_PD1_TGT, val);
	}

	
	if (status >= 0) {
		u8 val = ACFIFMAX[Mode];
		if (state->reg[MT2063_REG_PART_REV] != MT2063_B3 && val > 5)
			val = 5;
		val = (state->reg[MT2063_REG_FIF_OV] & (u8) ~0x1F) |
		      (val & 0x1F);
		if (state->reg[MT2063_REG_FIF_OV] != val)
			status |= mt2063_setreg(state, MT2063_REG_FIF_OV, val);
	}

	
	if (status >= 0) {
		u8 val = (state->reg[MT2063_REG_PD2_TGT] & (u8) ~0x3F) |
		    (PD2TGT[Mode] & 0x3F);
		if (state->reg[MT2063_REG_PD2_TGT] != val)
			status |= mt2063_setreg(state, MT2063_REG_PD2_TGT, val);
	}

	
	if (status >= 0) {
		val = (state->reg[MT2063_REG_LNA_TGT] & (u8) ~0x80) |
		      (RFOVDIS[Mode] ? 0x80 : 0x00);
		if (state->reg[MT2063_REG_LNA_TGT] != val)
			status |= mt2063_setreg(state, MT2063_REG_LNA_TGT, val);
	}

	
	if (status >= 0) {
		val = (state->reg[MT2063_REG_PD1_TGT] & (u8) ~0x80) |
		      (FIFOVDIS[Mode] ? 0x80 : 0x00);
		if (state->reg[MT2063_REG_PD1_TGT] != val)
			status |= mt2063_setreg(state, MT2063_REG_PD1_TGT, val);
	}

	if (status >= 0) {
		state->rcvr_mode = Mode;
		dprintk(1, "mt2063 mode changed to %s\n",
			mt2063_mode_name[state->rcvr_mode]);
	}

	return status;
}

static u32 MT2063_ClearPowerMaskBits(struct mt2063_state *state,
				     enum MT2063_Mask_Bits Bits)
{
	u32 status = 0;

	dprintk(2, "\n");
	Bits = (enum MT2063_Mask_Bits)(Bits & MT2063_ALL_SD);	
	if ((Bits & 0xFF00) != 0) {
		state->reg[MT2063_REG_PWR_2] &= ~(u8) (Bits >> 8);
		status |=
		    mt2063_write(state,
				    MT2063_REG_PWR_2,
				    &state->reg[MT2063_REG_PWR_2], 1);
	}
	if ((Bits & 0xFF) != 0) {
		state->reg[MT2063_REG_PWR_1] &= ~(u8) (Bits & 0xFF);
		status |=
		    mt2063_write(state,
				    MT2063_REG_PWR_1,
				    &state->reg[MT2063_REG_PWR_1], 1);
	}

	return status;
}

static u32 MT2063_SoftwareShutdown(struct mt2063_state *state, u8 Shutdown)
{
	u32 status;

	dprintk(2, "\n");
	if (Shutdown == 1)
		state->reg[MT2063_REG_PWR_1] |= 0x04;
	else
		state->reg[MT2063_REG_PWR_1] &= ~0x04;

	status = mt2063_write(state,
			    MT2063_REG_PWR_1,
			    &state->reg[MT2063_REG_PWR_1], 1);

	if (Shutdown != 1) {
		state->reg[MT2063_REG_BYP_CTRL] =
		    (state->reg[MT2063_REG_BYP_CTRL] & 0x9F) | 0x40;
		status |=
		    mt2063_write(state,
				    MT2063_REG_BYP_CTRL,
				    &state->reg[MT2063_REG_BYP_CTRL],
				    1);
		state->reg[MT2063_REG_BYP_CTRL] =
		    (state->reg[MT2063_REG_BYP_CTRL] & 0x9F);
		status |=
		    mt2063_write(state,
				    MT2063_REG_BYP_CTRL,
				    &state->reg[MT2063_REG_BYP_CTRL],
				    1);
	}

	return status;
}

static u32 MT2063_Round_fLO(u32 f_LO, u32 f_LO_Step, u32 f_ref)
{
	return f_ref * (f_LO / f_ref)
	    + f_LO_Step * (((f_LO % f_ref) + (f_LO_Step / 2)) / f_LO_Step);
}

static u32 MT2063_fLO_FractionalTerm(u32 f_ref, u32 num, u32 denom)
{
	u32 t1 = (f_ref >> 14) * num;
	u32 term1 = t1 / denom;
	u32 loss = t1 % denom;
	u32 term2 =
	    (((f_ref & 0x00003FFF) * num + (loss << 14)) + (denom / 2)) / denom;
	return (term1 << 14) + term2;
}

static u32 MT2063_CalcLO1Mult(u32 *Div,
			      u32 *FracN,
			      u32 f_LO,
			      u32 f_LO_Step, u32 f_Ref)
{
	
	*Div = f_LO / f_Ref;

	
	*FracN =
	    (64 * (((f_LO % f_Ref) + (f_LO_Step / 2)) / f_LO_Step) +
	     (f_Ref / f_LO_Step / 2)) / (f_Ref / f_LO_Step);

	return (f_Ref * (*Div)) + MT2063_fLO_FractionalTerm(f_Ref, *FracN, 64);
}

static u32 MT2063_CalcLO2Mult(u32 *Div,
			      u32 *FracN,
			      u32 f_LO,
			      u32 f_LO_Step, u32 f_Ref)
{
	
	*Div = f_LO / f_Ref;

	
	*FracN =
	    (8191 * (((f_LO % f_Ref) + (f_LO_Step / 2)) / f_LO_Step) +
	     (f_Ref / f_LO_Step / 2)) / (f_Ref / f_LO_Step);

	return (f_Ref * (*Div)) + MT2063_fLO_FractionalTerm(f_Ref, *FracN,
							    8191);
}

static u32 FindClearTuneFilter(struct mt2063_state *state, u32 f_in)
{
	u32 RFBand;
	u32 idx;		

	RFBand = 31;		
	for (idx = 0; idx < 31; ++idx) {
		if (state->CTFiltMax[idx] >= f_in) {
			RFBand = idx;
			break;
		}
	}
	return RFBand;
}

static u32 MT2063_Tune(struct mt2063_state *state, u32 f_in)
{				

	u32 status = 0;
	u32 LO1;		
	u32 Num1;		
	u32 f_IF1;		
	u32 LO2;		
	u32 Num2;		
	u32 ofLO1, ofLO2;	
	u8 fiffc = 0x80;	
	u32 fiffof;		
	const u8 LO1LK = 0x80;	
	u8 LO2LK = 0x08;	
	u8 val;
	u32 RFBand;

	dprintk(2, "\n");
	
	if ((f_in < MT2063_MIN_FIN_FREQ) || (f_in > MT2063_MAX_FIN_FREQ))
		return -EINVAL;

	if ((state->AS_Data.f_out < MT2063_MIN_FOUT_FREQ)
	    || (state->AS_Data.f_out > MT2063_MAX_FOUT_FREQ))
		return -EINVAL;

	ofLO1 = state->AS_Data.f_LO1;
	ofLO2 = state->AS_Data.f_LO2; 

	if (state->ctfilt_sw == 1) {
		val = (state->reg[MT2063_REG_CTUNE_CTRL] | 0x08);
		if (state->reg[MT2063_REG_CTUNE_CTRL] != val) {
			status |=
			    mt2063_setreg(state, MT2063_REG_CTUNE_CTRL, val);
		}
		val = state->reg[MT2063_REG_CTUNE_OV];
		RFBand = FindClearTuneFilter(state, f_in);
		state->reg[MT2063_REG_CTUNE_OV] =
		    (u8) ((state->reg[MT2063_REG_CTUNE_OV] & ~0x1F)
			      | RFBand);
		if (state->reg[MT2063_REG_CTUNE_OV] != val) {
			status |=
			    mt2063_setreg(state, MT2063_REG_CTUNE_OV, val);
		}
	}

	if (status >= 0) {
		status |=
		    mt2063_read(state,
				   MT2063_REG_FIFFC,
				   &state->reg[MT2063_REG_FIFFC], 1);
		fiffc = state->reg[MT2063_REG_FIFFC];
	}
	state->AS_Data.f_in = f_in;
	
	state->AS_Data.f_if1_Request =
	    MT2063_Round_fLO(state->AS_Data.f_if1_Request + f_in,
			     state->AS_Data.f_LO1_Step,
			     state->AS_Data.f_ref) - f_in;

	MT2063_ResetExclZones(&state->AS_Data);

	f_IF1 = MT2063_ChooseFirstIF(&state->AS_Data);

	state->AS_Data.f_LO1 =
	    MT2063_Round_fLO(f_IF1 + f_in, state->AS_Data.f_LO1_Step,
			     state->AS_Data.f_ref);

	state->AS_Data.f_LO2 =
	    MT2063_Round_fLO(state->AS_Data.f_LO1 - state->AS_Data.f_out - f_in,
			     state->AS_Data.f_LO2_Step, state->AS_Data.f_ref);

	status |= MT2063_AvoidSpurs(&state->AS_Data);
	state->AS_Data.f_LO1 =
	    MT2063_CalcLO1Mult(&LO1, &Num1, state->AS_Data.f_LO1,
			       state->AS_Data.f_LO1_Step, state->AS_Data.f_ref);
	state->AS_Data.f_LO2 =
	    MT2063_Round_fLO(state->AS_Data.f_LO1 - state->AS_Data.f_out - f_in,
			     state->AS_Data.f_LO2_Step, state->AS_Data.f_ref);
	state->AS_Data.f_LO2 =
	    MT2063_CalcLO2Mult(&LO2, &Num2, state->AS_Data.f_LO2,
			       state->AS_Data.f_LO2_Step, state->AS_Data.f_ref);

	if ((state->AS_Data.f_LO1 < MT2063_MIN_UPC_FREQ)
	    || (state->AS_Data.f_LO1 > MT2063_MAX_UPC_FREQ))
		status |= MT2063_UPC_RANGE;
	if ((state->AS_Data.f_LO2 < MT2063_MIN_DNC_FREQ)
	    || (state->AS_Data.f_LO2 > MT2063_MAX_DNC_FREQ))
		status |= MT2063_DNC_RANGE;
	
	if (state->tuner_id == MT2063_B0)
		LO2LK = 0x40;

	if ((ofLO1 != state->AS_Data.f_LO1)
	    || (ofLO2 != state->AS_Data.f_LO2)
	    || ((state->reg[MT2063_REG_LO_STATUS] & (LO1LK | LO2LK)) !=
		(LO1LK | LO2LK))) {
		fiffof =
		    (state->AS_Data.f_LO1 -
		     f_in) / (state->AS_Data.f_ref / 64) - 8 * (u32) fiffc -
		    4992;
		if (fiffof > 0xFF)
			fiffof = 0xFF;

		if (status >= 0) {
			state->reg[MT2063_REG_LO1CQ_1] = (u8) (LO1 & 0xFF);	
			state->reg[MT2063_REG_LO1CQ_2] = (u8) (Num1 & 0x3F);	
			state->reg[MT2063_REG_LO2CQ_1] = (u8) (((LO2 & 0x7F) << 1)	
								   |(Num2 >> 12));	
			state->reg[MT2063_REG_LO2CQ_2] = (u8) ((Num2 & 0x0FF0) >> 4);	
			state->reg[MT2063_REG_LO2CQ_3] = (u8) (0xE0 | (Num2 & 0x000F));	

			status |= mt2063_write(state, MT2063_REG_LO1CQ_1, &state->reg[MT2063_REG_LO1CQ_1], 5);	
			if (state->tuner_id == MT2063_B0) {
				
				status |= mt2063_write(state, MT2063_REG_LO2CQ_3, &state->reg[MT2063_REG_LO2CQ_3], 1);	
			}
			
			if (state->reg[MT2063_REG_FIFF_OFFSET] !=
			    (u8) fiffof) {
				state->reg[MT2063_REG_FIFF_OFFSET] =
				    (u8) fiffof;
				status |=
				    mt2063_write(state,
						    MT2063_REG_FIFF_OFFSET,
						    &state->
						    reg[MT2063_REG_FIFF_OFFSET],
						    1);
			}
		}


		if (status < 0)
			return status;

		status = mt2063_lockStatus(state);
		if (status < 0)
			return status;
		if (!status)
			return -EINVAL;		

		state->f_IF1_actual = state->AS_Data.f_LO1 - f_in;
	}

	return status;
}

static const u8 MT2063B0_defaults[] = {
	
	0x19, 0x05,
	0x1B, 0x1D,
	0x1C, 0x1F,
	0x1D, 0x0F,
	0x1E, 0x3F,
	0x1F, 0x0F,
	0x20, 0x3F,
	0x22, 0x21,
	0x23, 0x3F,
	0x24, 0x20,
	0x25, 0x3F,
	0x27, 0xEE,
	0x2C, 0x27,	
	0x30, 0x03,
	0x2C, 0x07,	
	0x2D, 0x87,
	0x2E, 0xAA,
	0x28, 0xE1,	
	0x28, 0xE0,	
	0x00
};

static const u8 MT2063B1_defaults[] = {
	
	0x05, 0xF0,
	0x11, 0x10,	
	0x19, 0x05,
	0x1A, 0x6C,
	0x1B, 0x24,
	0x1C, 0x28,
	0x1D, 0x8F,
	0x1E, 0x14,
	0x1F, 0x8F,
	0x20, 0x57,
	0x22, 0x21,	
	0x23, 0x3C,	
	0x24, 0x20,	
	0x2C, 0x24,	
	0x2D, 0x87,	
	0x2F, 0xF3,
	0x30, 0x0C,	
	0x31, 0x1B,	
	0x2C, 0x04,	
	0x28, 0xE1,	
	0x28, 0xE0,	
	0x00
};

static const u8 MT2063B3_defaults[] = {
	
	0x05, 0xF0,
	0x19, 0x3D,
	0x2C, 0x24,	
	0x2C, 0x04,	
	0x28, 0xE1,	
	0x28, 0xE0,	
	0x00
};

static int mt2063_init(struct dvb_frontend *fe)
{
	u32 status;
	struct mt2063_state *state = fe->tuner_priv;
	u8 all_resets = 0xF0;	
	const u8 *def = NULL;
	char *step;
	u32 FCRUN;
	s32 maxReads;
	u32 fcu_osc;
	u32 i;

	dprintk(2, "\n");

	state->rcvr_mode = MT2063_CABLE_QAM;

	
	status = mt2063_read(state, MT2063_REG_PART_REV,
			     &state->reg[MT2063_REG_PART_REV], 1);
	if (status < 0) {
		printk(KERN_ERR "Can't read mt2063 part ID\n");
		return status;
	}

	
	switch (state->reg[MT2063_REG_PART_REV]) {
	case MT2063_B0:
		step = "B0";
		break;
	case MT2063_B1:
		step = "B1";
		break;
	case MT2063_B2:
		step = "B2";
		break;
	case MT2063_B3:
		step = "B3";
		break;
	default:
		printk(KERN_ERR "mt2063: Unknown mt2063 device ID (0x%02x)\n",
		       state->reg[MT2063_REG_PART_REV]);
		return -ENODEV;	
	}

	
	status = mt2063_read(state, MT2063_REG_RSVD_3B,
			     &state->reg[MT2063_REG_RSVD_3B], 1);

	
	if (status < 0 || ((state->reg[MT2063_REG_RSVD_3B] & 0x80) != 0x00)) {
		printk(KERN_ERR "mt2063: Unknown part ID (0x%02x%02x)\n",
		       state->reg[MT2063_REG_PART_REV],
		       state->reg[MT2063_REG_RSVD_3B]);
		return -ENODEV;	
	}

	printk(KERN_INFO "mt2063: detected a mt2063 %s\n", step);

	
	status = mt2063_write(state, MT2063_REG_LO2CQ_3, &all_resets, 1);
	if (status < 0)
		return status;

	
	
	switch (state->reg[MT2063_REG_PART_REV]) {
	case MT2063_B3:
		def = MT2063B3_defaults;
		break;

	case MT2063_B1:
		def = MT2063B1_defaults;
		break;

	case MT2063_B0:
		def = MT2063B0_defaults;
		break;

	default:
		return -ENODEV;
		break;
	}

	while (status >= 0 && *def) {
		u8 reg = *def++;
		u8 val = *def++;
		status = mt2063_write(state, reg, &val, 1);
	}
	if (status < 0)
		return status;

	
	FCRUN = 1;
	maxReads = 10;
	while (status >= 0 && (FCRUN != 0) && (maxReads-- > 0)) {
		msleep(2);
		status = mt2063_read(state,
					 MT2063_REG_XO_STATUS,
					 &state->
					 reg[MT2063_REG_XO_STATUS], 1);
		FCRUN = (state->reg[MT2063_REG_XO_STATUS] & 0x40) >> 6;
	}

	if (FCRUN != 0 || status < 0)
		return -ENODEV;

	status = mt2063_read(state,
			   MT2063_REG_FIFFC,
			   &state->reg[MT2063_REG_FIFFC], 1);
	if (status < 0)
		return status;

	
	status = mt2063_read(state,
				MT2063_REG_PART_REV,
				state->reg, MT2063_REG_END_REGS);
	if (status < 0)
		return status;

	
	state->tuner_id = state->reg[MT2063_REG_PART_REV];
	state->AS_Data.f_ref = MT2063_REF_FREQ;
	state->AS_Data.f_if1_Center = (state->AS_Data.f_ref / 8) *
				      ((u32) state->reg[MT2063_REG_FIFFC] + 640);
	state->AS_Data.f_if1_bw = MT2063_IF1_BW;
	state->AS_Data.f_out = 43750000UL;
	state->AS_Data.f_out_bw = 6750000UL;
	state->AS_Data.f_zif_bw = MT2063_ZIF_BW;
	state->AS_Data.f_LO1_Step = state->AS_Data.f_ref / 64;
	state->AS_Data.f_LO2_Step = MT2063_TUNE_STEP_SIZE;
	state->AS_Data.maxH1 = MT2063_MAX_HARMONICS_1;
	state->AS_Data.maxH2 = MT2063_MAX_HARMONICS_2;
	state->AS_Data.f_min_LO_Separation = MT2063_MIN_LO_SEP;
	state->AS_Data.f_if1_Request = state->AS_Data.f_if1_Center;
	state->AS_Data.f_LO1 = 2181000000UL;
	state->AS_Data.f_LO2 = 1486249786UL;
	state->f_IF1_actual = state->AS_Data.f_if1_Center;
	state->AS_Data.f_in = state->AS_Data.f_LO1 - state->f_IF1_actual;
	state->AS_Data.f_LO1_FracN_Avoid = MT2063_LO1_FRACN_AVOID;
	state->AS_Data.f_LO2_FracN_Avoid = MT2063_LO2_FRACN_AVOID;
	state->num_regs = MT2063_REG_END_REGS;
	state->AS_Data.avoidDECT = MT2063_AVOID_BOTH;
	state->ctfilt_sw = 0;

	state->CTFiltMax[0] = 69230000;
	state->CTFiltMax[1] = 105770000;
	state->CTFiltMax[2] = 140350000;
	state->CTFiltMax[3] = 177110000;
	state->CTFiltMax[4] = 212860000;
	state->CTFiltMax[5] = 241130000;
	state->CTFiltMax[6] = 274370000;
	state->CTFiltMax[7] = 309820000;
	state->CTFiltMax[8] = 342450000;
	state->CTFiltMax[9] = 378870000;
	state->CTFiltMax[10] = 416210000;
	state->CTFiltMax[11] = 456500000;
	state->CTFiltMax[12] = 495790000;
	state->CTFiltMax[13] = 534530000;
	state->CTFiltMax[14] = 572610000;
	state->CTFiltMax[15] = 598970000;
	state->CTFiltMax[16] = 635910000;
	state->CTFiltMax[17] = 672130000;
	state->CTFiltMax[18] = 714840000;
	state->CTFiltMax[19] = 739660000;
	state->CTFiltMax[20] = 770410000;
	state->CTFiltMax[21] = 814660000;
	state->CTFiltMax[22] = 846950000;
	state->CTFiltMax[23] = 867820000;
	state->CTFiltMax[24] = 915980000;
	state->CTFiltMax[25] = 947450000;
	state->CTFiltMax[26] = 983110000;
	state->CTFiltMax[27] = 1021630000;
	state->CTFiltMax[28] = 1061870000;
	state->CTFiltMax[29] = 1098330000;
	state->CTFiltMax[30] = 1138990000;


	state->reg[MT2063_REG_CTUNE_CTRL] = 0x0A;
	status = mt2063_write(state, MT2063_REG_CTUNE_CTRL,
			      &state->reg[MT2063_REG_CTUNE_CTRL], 1);
	if (status < 0)
		return status;

	
	status = mt2063_read(state, MT2063_REG_FIFFC,
			     &state->reg[MT2063_REG_FIFFC], 1);
	if (status < 0)
		return status;

	fcu_osc = state->reg[MT2063_REG_FIFFC];

	state->reg[MT2063_REG_CTUNE_CTRL] = 0x00;
	status = mt2063_write(state, MT2063_REG_CTUNE_CTRL,
			      &state->reg[MT2063_REG_CTUNE_CTRL], 1);
	if (status < 0)
		return status;

	
	for (i = 0; i < 31; i++)
		state->CTFiltMax[i] = (state->CTFiltMax[i] / 768) * (fcu_osc + 640);

	status = MT2063_SoftwareShutdown(state, 1);
	if (status < 0)
		return status;
	status = MT2063_ClearPowerMaskBits(state, MT2063_ALL_SD);
	if (status < 0)
		return status;

	state->init = true;

	return 0;
}

static int mt2063_get_status(struct dvb_frontend *fe, u32 *tuner_status)
{
	struct mt2063_state *state = fe->tuner_priv;
	int status;

	dprintk(2, "\n");

	if (!state->init)
		return -ENODEV;

	*tuner_status = 0;
	status = mt2063_lockStatus(state);
	if (status < 0)
		return status;
	if (status)
		*tuner_status = TUNER_STATUS_LOCKED;

	dprintk(1, "Tuner status: %d", *tuner_status);

	return 0;
}

static int mt2063_release(struct dvb_frontend *fe)
{
	struct mt2063_state *state = fe->tuner_priv;

	dprintk(2, "\n");

	fe->tuner_priv = NULL;
	kfree(state);

	return 0;
}

static int mt2063_set_analog_params(struct dvb_frontend *fe,
				    struct analog_parameters *params)
{
	struct mt2063_state *state = fe->tuner_priv;
	s32 pict_car;
	s32 pict2chanb_vsb;
	s32 ch_bw;
	s32 if_mid;
	s32 rcvr_mode;
	int status;

	dprintk(2, "\n");

	if (!state->init) {
		status = mt2063_init(fe);
		if (status < 0)
			return status;
	}

	switch (params->mode) {
	case V4L2_TUNER_RADIO:
		pict_car = 38900000;
		ch_bw = 8000000;
		pict2chanb_vsb = -(ch_bw / 2);
		rcvr_mode = MT2063_OFFAIR_ANALOG;
		break;
	case V4L2_TUNER_ANALOG_TV:
		rcvr_mode = MT2063_CABLE_ANALOG;
		if (params->std & ~V4L2_STD_MN) {
			pict_car = 38900000;
			ch_bw = 6000000;
			pict2chanb_vsb = -1250000;
		} else if (params->std & V4L2_STD_PAL_G) {
			pict_car = 38900000;
			ch_bw = 7000000;
			pict2chanb_vsb = -1250000;
		} else {		
			pict_car = 38900000;
			ch_bw = 8000000;
			pict2chanb_vsb = -1250000;
		}
		break;
	default:
		return -EINVAL;
	}
	if_mid = pict_car - (pict2chanb_vsb + (ch_bw / 2));

	state->AS_Data.f_LO2_Step = 125000;	
	state->AS_Data.f_out = if_mid;
	state->AS_Data.f_out_bw = ch_bw + 750000;
	status = MT2063_SetReceiverMode(state, rcvr_mode);
	if (status < 0)
		return status;

	dprintk(1, "Tuning to frequency: %d, bandwidth %d, foffset %d\n",
		params->frequency, ch_bw, pict2chanb_vsb);

	status = MT2063_Tune(state, (params->frequency + (pict2chanb_vsb + (ch_bw / 2))));
	if (status < 0)
		return status;

	state->frequency = params->frequency;
	return 0;
}

#define MAX_SYMBOL_RATE_6MHz	5217391

static int mt2063_set_params(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	struct mt2063_state *state = fe->tuner_priv;
	int status;
	s32 pict_car;
	s32 pict2chanb_vsb;
	s32 ch_bw;
	s32 if_mid;
	s32 rcvr_mode;

	if (!state->init) {
		status = mt2063_init(fe);
		if (status < 0)
			return status;
	}

	dprintk(2, "\n");

	if (c->bandwidth_hz == 0)
		return -EINVAL;
	if (c->bandwidth_hz <= 6000000)
		ch_bw = 6000000;
	else if (c->bandwidth_hz <= 7000000)
		ch_bw = 7000000;
	else
		ch_bw = 8000000;

	switch (c->delivery_system) {
	case SYS_DVBT:
		rcvr_mode = MT2063_OFFAIR_COFDM;
		pict_car = 36125000;
		pict2chanb_vsb = -(ch_bw / 2);
		break;
	case SYS_DVBC_ANNEX_A:
	case SYS_DVBC_ANNEX_C:
		rcvr_mode = MT2063_CABLE_QAM;
		pict_car = 36125000;
		pict2chanb_vsb = -(ch_bw / 2);
		break;
	default:
		return -EINVAL;
	}
	if_mid = pict_car - (pict2chanb_vsb + (ch_bw / 2));

	state->AS_Data.f_LO2_Step = 125000;	
	state->AS_Data.f_out = if_mid;
	state->AS_Data.f_out_bw = ch_bw + 750000;
	status = MT2063_SetReceiverMode(state, rcvr_mode);
	if (status < 0)
		return status;

	dprintk(1, "Tuning to frequency: %d, bandwidth %d, foffset %d\n",
		c->frequency, ch_bw, pict2chanb_vsb);

	status = MT2063_Tune(state, (c->frequency + (pict2chanb_vsb + (ch_bw / 2))));

	if (status < 0)
		return status;

	state->frequency = c->frequency;
	return 0;
}

static int mt2063_get_if_frequency(struct dvb_frontend *fe, u32 *freq)
{
	struct mt2063_state *state = fe->tuner_priv;

	dprintk(2, "\n");

	if (!state->init)
		return -ENODEV;

	*freq = state->AS_Data.f_out;

	dprintk(1, "IF frequency: %d\n", *freq);

	return 0;
}

static int mt2063_get_bandwidth(struct dvb_frontend *fe, u32 *bw)
{
	struct mt2063_state *state = fe->tuner_priv;

	dprintk(2, "\n");

	if (!state->init)
		return -ENODEV;

	*bw = state->AS_Data.f_out_bw - 750000;

	dprintk(1, "bandwidth: %d\n", *bw);

	return 0;
}

static struct dvb_tuner_ops mt2063_ops = {
	.info = {
		 .name = "MT2063 Silicon Tuner",
		 .frequency_min = 45000000,
		 .frequency_max = 865000000,
		 .frequency_step = 0,
		 },

	.init = mt2063_init,
	.sleep = MT2063_Sleep,
	.get_status = mt2063_get_status,
	.set_analog_params = mt2063_set_analog_params,
	.set_params    = mt2063_set_params,
	.get_if_frequency = mt2063_get_if_frequency,
	.get_bandwidth = mt2063_get_bandwidth,
	.release = mt2063_release,
};

struct dvb_frontend *mt2063_attach(struct dvb_frontend *fe,
				   struct mt2063_config *config,
				   struct i2c_adapter *i2c)
{
	struct mt2063_state *state = NULL;

	dprintk(2, "\n");

	state = kzalloc(sizeof(struct mt2063_state), GFP_KERNEL);
	if (state == NULL)
		goto error;

	state->config = config;
	state->i2c = i2c;
	state->frontend = fe;
	state->reference = config->refclock / 1000;	
	fe->tuner_priv = state;
	fe->ops.tuner_ops = mt2063_ops;

	printk(KERN_INFO "%s: Attaching MT2063\n", __func__);
	return fe;

error:
	kfree(state);
	return NULL;
}
EXPORT_SYMBOL_GPL(mt2063_attach);

unsigned int tuner_MT2063_SoftwareShutdown(struct dvb_frontend *fe)
{
	struct mt2063_state *state = fe->tuner_priv;
	int err = 0;

	dprintk(2, "\n");

	err = MT2063_SoftwareShutdown(state, 1);
	if (err < 0)
		printk(KERN_ERR "%s: Couldn't shutdown\n", __func__);

	return err;
}
EXPORT_SYMBOL_GPL(tuner_MT2063_SoftwareShutdown);

unsigned int tuner_MT2063_ClearPowerMaskBits(struct dvb_frontend *fe)
{
	struct mt2063_state *state = fe->tuner_priv;
	int err = 0;

	dprintk(2, "\n");

	err = MT2063_ClearPowerMaskBits(state, MT2063_ALL_SD);
	if (err < 0)
		printk(KERN_ERR "%s: Invalid parameter\n", __func__);

	return err;
}
EXPORT_SYMBOL_GPL(tuner_MT2063_ClearPowerMaskBits);

MODULE_AUTHOR("Mauro Carvalho Chehab <mchehab@redhat.com>");
MODULE_DESCRIPTION("MT2063 Silicon tuner");
MODULE_LICENSE("GPL");
