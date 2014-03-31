/*
 * Copyright (c) 2011 Broadcom Corporation
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

#ifndef _BRCMFMAC_SDIO_CHIP_H_
#define _BRCMFMAC_SDIO_CHIP_H_

#define CORE_CC_REG(base, field) \
		(base + offsetof(struct chipcregs, field))
#define CORE_BUS_REG(base, field) \
		(base + offsetof(struct sdpcmd_regs, field))
#define CORE_SB(base, field) \
		(base + SBCONFIGOFF + offsetof(struct sbconfig, field))

#define SBSDIO_FORCE_ALP		0x01
#define SBSDIO_FORCE_HT			0x02
#define SBSDIO_FORCE_ILP		0x04
#define SBSDIO_ALP_AVAIL_REQ		0x08
#define SBSDIO_HT_AVAIL_REQ		0x10
#define SBSDIO_FORCE_HW_CLKREQ_OFF	0x20
#define SBSDIO_ALP_AVAIL		0x40
#define SBSDIO_HT_AVAIL			0x80
#define SBSDIO_AVBITS		(SBSDIO_HT_AVAIL | SBSDIO_ALP_AVAIL)
#define SBSDIO_ALPAV(regval)	((regval) & SBSDIO_AVBITS)
#define SBSDIO_HTAV(regval)	(((regval) & SBSDIO_AVBITS) == SBSDIO_AVBITS)
#define SBSDIO_ALPONLY(regval)	(SBSDIO_ALPAV(regval) && !SBSDIO_HTAV(regval))
#define SBSDIO_CLKAV(regval, alponly) \
	(SBSDIO_ALPAV(regval) && (alponly ? 1 : SBSDIO_HTAV(regval)))

#define BRCMF_MAX_CORENUM	6

struct chip_core_info {
	u16 id;
	u16 rev;
	u32 base;
	u32 wrapbase;
	u32 caps;
	u32 cib;
};

struct chip_info {
	u32 chip;
	u32 chiprev;
	u32 socitype;
	
	
	struct chip_core_info c_inf[BRCMF_MAX_CORENUM];
	u32 pmurev;
	u32 pmucaps;
	u32 ramsize;

	bool (*iscoreup)(struct brcmf_sdio_dev *sdiodev, struct chip_info *ci,
			 u16 coreid);
	u32 (*corerev)(struct brcmf_sdio_dev *sdiodev, struct chip_info *ci,
			 u16 coreid);
	void (*coredisable)(struct brcmf_sdio_dev *sdiodev,
			struct chip_info *ci, u16 coreid);
	void (*resetcore)(struct brcmf_sdio_dev *sdiodev,
			struct chip_info *ci, u16 coreid);
};

struct sbconfig {
	u32 PAD[2];
	u32 sbipsflag;	
	u32 PAD[3];
	u32 sbtpsflag;	
	u32 PAD[11];
	u32 sbtmerrloga;	
	u32 PAD;
	u32 sbtmerrlog;	
	u32 PAD[3];
	u32 sbadmatch3;	
	u32 PAD;
	u32 sbadmatch2;	
	u32 PAD;
	u32 sbadmatch1;	
	u32 PAD[7];
	u32 sbimstate;	
	u32 sbintvec;	
	u32 sbtmstatelow;	
	u32 sbtmstatehigh;	
	u32 sbbwa0;		
	u32 PAD;
	u32 sbimconfiglow;	
	u32 sbimconfighigh;	
	u32 sbadmatch0;	
	u32 PAD;
	u32 sbtmconfiglow;	
	u32 sbtmconfighigh;	
	u32 sbbconfig;	
	u32 PAD;
	u32 sbbstate;	
	u32 PAD[3];
	u32 sbactcnfg;	
	u32 PAD[3];
	u32 sbflagst;	
	u32 PAD[3];
	u32 sbidlow;		
	u32 sbidhigh;	
};

extern int brcmf_sdio_chip_attach(struct brcmf_sdio_dev *sdiodev,
				  struct chip_info **ci_ptr, u32 regs);
extern void brcmf_sdio_chip_detach(struct chip_info **ci_ptr);
extern void brcmf_sdio_chip_drivestrengthinit(struct brcmf_sdio_dev *sdiodev,
					      struct chip_info *ci,
					      u32 drivestrength);
extern u8 brcmf_sdio_chip_getinfidx(struct chip_info *ci, u16 coreid);


#endif		
