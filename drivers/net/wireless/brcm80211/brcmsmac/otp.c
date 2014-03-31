/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/io.h>
#include <linux/errno.h>
#include <linux/string.h>

#include <brcm_hw_ids.h>
#include <chipcommon.h>
#include "aiutils.h"
#include "otp.h"

#define OTPS_GUP_MASK		0x00000f00
#define OTPS_GUP_SHIFT		8
#define OTPS_GUP_HW		0x00000100
#define OTPS_GUP_SW		0x00000200
#define OTPS_GUP_CI		0x00000400
#define OTPS_GUP_FUSE		0x00000800

#define OTPP_COL_MASK		0x000000ff
#define OTPP_COL_SHIFT		0
#define OTPP_ROW_MASK		0x0000ff00
#define OTPP_ROW_SHIFT		8
#define OTPP_OC_MASK		0x0f000000
#define OTPP_OC_SHIFT		24
#define OTPP_READERR		0x10000000
#define OTPP_VALUE_MASK		0x20000000
#define OTPP_VALUE_SHIFT	29
#define OTPP_START_BUSY		0x80000000
#define	OTPP_READ		0x40000000

#define OTPPOC_READ		0
#define OTPPOC_BIT_PROG		1
#define OTPPOC_VERIFY		3
#define OTPPOC_INIT		4
#define OTPPOC_SET		5
#define OTPPOC_RESET		6
#define OTPPOC_OCST		7
#define OTPPOC_ROW_LOCK		8
#define OTPPOC_PRESCN_TEST	9

#define OTPTYPE_IPX(ccrev)	((ccrev) == 21 || (ccrev) >= 23)

#define OTPP_TRIES	10000000	

#define MAXNUMRDES		9	

#define OTPGU_CI_SZ		2

struct otpinfo;

struct otp_fn_s {
	int (*init)(struct si_pub *sih, struct otpinfo *oi);
	int (*read_region)(struct otpinfo *oi, int region, u16 *data,
			   uint *wlen);
};

struct otpinfo {
	struct bcma_device *core; 
	const struct otp_fn_s *fn;	
	struct si_pub *sih;		

	
	u16 wsize;		
	u16 rows;		
	u16 cols;		
	u32 status;		
	u16 hwbase;		
	u16 hwlim;		
	u16 swbase;		
	u16 swlim;		
	u16 fbase;		
	u16 flim;		
	int otpgu_base;		
};

#define REVA4_OTPGU_BASE	12

#define REVB8_OTPGU_BASE	20

#define REV36_OTPGU_BASE	12

#define OTPGU_HSB_OFF		0
#define OTPGU_SFB_OFF		1
#define OTPGU_CI_OFF		2
#define OTPGU_P_OFF		3
#define OTPGU_SROM_OFF		4

#define OTPGU_HWP_OFF		60
#define OTPGU_SWP_OFF		61
#define OTPGU_CIP_OFF		62
#define OTPGU_FUSEP_OFF		63
#define OTPGU_CIP_MSK		0x4000
#define OTPGU_P_MSK		0xf000
#define OTPGU_P_SHIFT		(OTPGU_HWP_OFF % 16)

#define OTP_SZ_FU_324		((roundup(324, 8))/8)	
#define OTP_SZ_FU_288		(288/8)	
#define OTP_SZ_FU_216		(216/8)	
#define OTP_SZ_FU_72		(72/8)	
#define OTP_SZ_CHECKSUM		(16/8)	
#define OTP4315_SWREG_SZ	178	
#define OTP_SZ_FU_144		(144/8)	

static u16
ipxotp_otpr(struct otpinfo *oi, uint wn)
{
	return bcma_read16(oi->core,
			   CHIPCREGOFFS(sromotp[wn]));
}

static int ipxotp_max_rgnsz(struct si_pub *sih, int osizew)
{
	int ret = 0;

	switch (ai_get_chip_id(sih)) {
	case BCM43224_CHIP_ID:
	case BCM43225_CHIP_ID:
		ret = osizew * 2 - OTP_SZ_FU_72 - OTP_SZ_CHECKSUM;
		break;
	case BCM4313_CHIP_ID:
		ret = osizew * 2 - OTP_SZ_FU_72 - OTP_SZ_CHECKSUM;
		break;
	default:
		break;	
	}

	return ret;
}

static void _ipxotp_init(struct otpinfo *oi)
{
	uint k;
	u32 otpp, st;
	int ccrev = ai_get_ccrev(oi->sih);


	if (ccrev == 21 || ccrev == 24
	    || ccrev == 27) {
		oi->otpgu_base = REVA4_OTPGU_BASE;
	} else if (ccrev == 36) {
		if (oi->wsize >= 128)
			oi->otpgu_base = REVB8_OTPGU_BASE;
		else
			oi->otpgu_base = REV36_OTPGU_BASE;
	} else if (ccrev == 23 || ccrev >= 25) {
		oi->otpgu_base = REVB8_OTPGU_BASE;
	}

	
	otpp =
	    OTPP_START_BUSY | ((OTPPOC_INIT << OTPP_OC_SHIFT) & OTPP_OC_MASK);

	bcma_write32(oi->core, CHIPCREGOFFS(otpprog), otpp);
	st = bcma_read32(oi->core, CHIPCREGOFFS(otpprog));
	for (k = 0; (st & OTPP_START_BUSY) && (k < OTPP_TRIES); k++)
		st = bcma_read32(oi->core, CHIPCREGOFFS(otpprog));
	if (k >= OTPP_TRIES)
		return;

	
	oi->status = bcma_read32(oi->core, CHIPCREGOFFS(otpstatus));

	if ((ai_get_chip_id(oi->sih) == BCM43224_CHIP_ID)
	    || (ai_get_chip_id(oi->sih) == BCM43225_CHIP_ID)) {
		u32 p_bits;
		p_bits = (ipxotp_otpr(oi, oi->otpgu_base + OTPGU_P_OFF) &
			  OTPGU_P_MSK) >> OTPGU_P_SHIFT;
		oi->status |= (p_bits << OTPS_GUP_SHIFT);
	}

	oi->hwbase = oi->otpgu_base + OTPGU_SROM_OFF;
	oi->hwlim = oi->wsize;
	if (oi->status & OTPS_GUP_HW) {
		oi->hwlim =
		    ipxotp_otpr(oi, oi->otpgu_base + OTPGU_HSB_OFF) / 16;
		oi->swbase = oi->hwlim;
	} else
		oi->swbase = oi->hwbase;

	
	oi->swlim = ipxotp_max_rgnsz(oi->sih, oi->wsize) / 2;

	if (oi->status & OTPS_GUP_SW) {
		oi->swlim =
		    ipxotp_otpr(oi, oi->otpgu_base + OTPGU_SFB_OFF) / 16;
		oi->fbase = oi->swlim;
	} else
		oi->fbase = oi->swbase;

	oi->flim = oi->wsize;
}

static int ipxotp_init(struct si_pub *sih, struct otpinfo *oi)
{
	
	if (!OTPTYPE_IPX(ai_get_ccrev(sih)))
		return -EBADE;

	
	if (ai_is_otp_disabled(sih))
		return -EBADE;

	
	switch ((ai_get_cccaps(sih) & CC_CAP_OTPSIZE) >> CC_CAP_OTPSIZE_SHIFT) {
	case 0:
		
		return -EBADE;
	case 1:		
		oi->rows = 32;
		oi->cols = 64;
		oi->wsize = 128;
		break;
	case 2:		
		oi->rows = 64;
		oi->cols = 64;
		oi->wsize = 256;
		break;
	case 5:		
		oi->rows = 96;
		oi->cols = 64;
		oi->wsize = 384;
		break;
	case 7:		
		oi->rows = 16;
		oi->cols = 64;
		oi->wsize = 64;
		break;
	default:
		
		return -EBADE;
	}

	
	_ipxotp_init(oi);
	return 0;
}

static int
ipxotp_read_region(struct otpinfo *oi, int region, u16 *data, uint *wlen)
{
	uint base, i, sz;

	
	switch (region) {
	case OTP_HW_RGN:
		sz = (uint) oi->hwlim - oi->hwbase;
		if (!(oi->status & OTPS_GUP_HW)) {
			*wlen = sz;
			return -ENODATA;
		}
		if (*wlen < sz) {
			*wlen = sz;
			return -EOVERFLOW;
		}
		base = oi->hwbase;
		break;
	case OTP_SW_RGN:
		sz = ((uint) oi->swlim - oi->swbase);
		if (!(oi->status & OTPS_GUP_SW)) {
			*wlen = sz;
			return -ENODATA;
		}
		if (*wlen < sz) {
			*wlen = sz;
			return -EOVERFLOW;
		}
		base = oi->swbase;
		break;
	case OTP_CI_RGN:
		sz = OTPGU_CI_SZ;
		if (!(oi->status & OTPS_GUP_CI)) {
			*wlen = sz;
			return -ENODATA;
		}
		if (*wlen < sz) {
			*wlen = sz;
			return -EOVERFLOW;
		}
		base = oi->otpgu_base + OTPGU_CI_OFF;
		break;
	case OTP_FUSE_RGN:
		sz = (uint) oi->flim - oi->fbase;
		if (!(oi->status & OTPS_GUP_FUSE)) {
			*wlen = sz;
			return -ENODATA;
		}
		if (*wlen < sz) {
			*wlen = sz;
			return -EOVERFLOW;
		}
		base = oi->fbase;
		break;
	case OTP_ALL_RGN:
		sz = ((uint) oi->flim - oi->hwbase);
		if (!(oi->status & (OTPS_GUP_HW | OTPS_GUP_SW))) {
			*wlen = sz;
			return -ENODATA;
		}
		if (*wlen < sz) {
			*wlen = sz;
			return -EOVERFLOW;
		}
		base = oi->hwbase;
		break;
	default:
		return -EINVAL;
	}

	
	for (i = 0; i < sz; i++)
		data[i] = ipxotp_otpr(oi, base + i);

	*wlen = sz;
	return 0;
}

static const struct otp_fn_s ipxotp_fn = {
	(int (*)(struct si_pub *, struct otpinfo *)) ipxotp_init,
	(int (*)(struct otpinfo *, int, u16 *, uint *)) ipxotp_read_region,
};

static int otp_init(struct si_pub *sih, struct otpinfo *oi)
{
	int ret;

	memset(oi, 0, sizeof(struct otpinfo));

	oi->core = ai_findcore(sih, BCMA_CORE_CHIPCOMMON, 0);

	if (OTPTYPE_IPX(ai_get_ccrev(sih)))
		oi->fn = &ipxotp_fn;

	if (oi->fn == NULL)
		return -EBADE;

	oi->sih = sih;

	ret = (oi->fn->init)(sih, oi);

	return ret;
}

int
otp_read_region(struct si_pub *sih, int region, u16 *data, uint *wlen) {
	struct otpinfo otpinfo;
	struct otpinfo *oi = &otpinfo;
	int err = 0;

	if (ai_is_otp_disabled(sih)) {
		err = -EPERM;
		goto out;
	}

	err = otp_init(sih, oi);
	if (err)
		goto out;

	err = ((oi)->fn->read_region)(oi, region, data, wlen);

 out:
	return err;
}
