/*
 * QUICC Engine (QE) Internal Memory Map.
 * The Internal Memory Map for devices with QE on them. This
 * is the superset of all QE devices (8360, etc.).

 * Copyright (C) 2006. Freescale Semicondutor, Inc. All rights reserved.
 *
 * Authors: 	Shlomi Gridish <gridish@freescale.com>
 * 		Li Yang <leoli@freescale.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef _ASM_POWERPC_IMMAP_QE_H
#define _ASM_POWERPC_IMMAP_QE_H
#ifdef __KERNEL__

#include <linux/kernel.h>
#include <asm/io.h>

#define QE_IMMAP_SIZE	(1024 * 1024)	

struct qe_iram {
	__be32	iadd;		
	__be32	idata;		
	u8	res0[0x78];
} __attribute__ ((packed));

struct qe_ic_regs {
	__be32	qicr;
	__be32	qivec;
	__be32	qripnr;
	__be32	qipnr;
	__be32	qipxcc;
	__be32	qipycc;
	__be32	qipwcc;
	__be32	qipzcc;
	__be32	qimr;
	__be32	qrimr;
	__be32	qicnr;
	u8	res0[0x4];
	__be32	qiprta;
	__be32	qiprtb;
	u8	res1[0x4];
	__be32	qricr;
	u8	res2[0x20];
	__be32	qhivec;
	u8	res3[0x1C];
} __attribute__ ((packed));

struct cp_qe {
	__be32	cecr;		
	__be32	ceccr;		
	__be32	cecdr;		
	u8	res0[0xA];
	__be16	ceter;		
	u8	res1[0x2];
	__be16	cetmr;		
	__be32	cetscr;		
	__be32	cetsr1;		
	__be32	cetsr2;		
	u8	res2[0x8];
	__be32	cevter;		
	__be32	cevtmr;		
	__be16	cercr;		
	u8	res3[0x2];
	u8	res4[0x24];
	__be16	ceexe1;		
	u8	res5[0x2];
	__be16	ceexm1;		
	u8	res6[0x2];
	__be16	ceexe2;		
	u8	res7[0x2];
	__be16	ceexm2;		
	u8	res8[0x2];
	__be16	ceexe3;		
	u8	res9[0x2];
	__be16	ceexm3;		
	u8	res10[0x2];
	__be16	ceexe4;		
	u8	res11[0x2];
	__be16	ceexm4;		
	u8	res12[0x3A];
	__be32	ceurnr;		
	u8	res13[0x244];
} __attribute__ ((packed));

struct qe_mux {
	__be32	cmxgcr;		
	__be32	cmxsi1cr_l;	
	__be32	cmxsi1cr_h;	
	__be32	cmxsi1syr;	
	__be32	cmxucr[4];	
	__be32	cmxupcr;	
	u8	res0[0x1C];
} __attribute__ ((packed));

struct qe_timers {
	u8	gtcfr1;		
	u8	res0[0x3];
	u8	gtcfr2;		
	u8	res1[0xB];
	__be16	gtmdr1;		
	__be16	gtmdr2;		
	__be16	gtrfr1;		
	__be16	gtrfr2;		
	__be16	gtcpr1;		
	__be16	gtcpr2;		
	__be16	gtcnr1;		
	__be16	gtcnr2;		
	__be16	gtmdr3;		
	__be16	gtmdr4;		
	__be16	gtrfr3;		
	__be16	gtrfr4;		
	__be16	gtcpr3;		
	__be16	gtcpr4;		
	__be16	gtcnr3;		
	__be16	gtcnr4;		
	__be16	gtevr1;		
	__be16	gtevr2;		
	__be16	gtevr3;		
	__be16	gtevr4;		
	__be16	gtps;		
	u8 res2[0x46];
} __attribute__ ((packed));

struct qe_brg {
	__be32	brgc[16];	
	u8	res0[0x40];
} __attribute__ ((packed));

struct spi {
	u8	res0[0x20];
	__be32	spmode;		
	u8	res1[0x2];
	u8	spie;		
	u8	res2[0x1];
	u8	res3[0x2];
	u8	spim;		
	u8	res4[0x1];
	u8	res5[0x1];
	u8	spcom;		
	u8	res6[0x2];
	__be32	spitd;		
	__be32	spird;		
	u8	res7[0x8];
} __attribute__ ((packed));

struct si1 {
	__be16	siamr1;		
	__be16	sibmr1;		
	__be16	sicmr1;		
	__be16	sidmr1;		
	u8	siglmr1_h;	
	u8	res0[0x1];
	u8	sicmdr1_h;	
	u8	res2[0x1];
	u8	sistr1_h;	
	u8	res3[0x1];
	__be16	sirsr1_h;	
	u8	sitarc1;	
	u8	sitbrc1;	
	u8	sitcrc1;	
	u8	sitdrc1;	
	u8	sirarc1;	
	u8	sirbrc1;	
	u8	sircrc1;	
	u8	sirdrc1;	
	u8	res4[0x8];
	__be16	siemr1;		
	__be16	sifmr1;		
	__be16	sigmr1;		
	__be16	sihmr1;		
	u8	siglmg1_l;	
	u8	res5[0x1];
	u8	sicmdr1_l;	
	u8	res6[0x1];
	u8	sistr1_l;	
	u8	res7[0x1];
	__be16	sirsr1_l;	
	u8	siterc1;	
	u8	sitfrc1;	
	u8	sitgrc1;	
	u8	sithrc1;	
	u8	sirerc1;	
	u8	sirfrc1;	
	u8	sirgrc1;	
	u8	sirhrc1;	
	u8	res8[0x8];
	__be32	siml1;		
	u8	siedm1;		
	u8	res9[0xBB];
} __attribute__ ((packed));

struct sir {
	u8 	tx[0x400];
	u8	rx[0x400];
	u8	res0[0x800];
} __attribute__ ((packed));

struct qe_usb_ctlr {
	u8	usb_usmod;
	u8	usb_usadr;
	u8	usb_uscom;
	u8	res1[1];
	__be16  usb_usep[4];
	u8	res2[4];
	__be16	usb_usber;
	u8	res3[2];
	__be16	usb_usbmr;
	u8	res4[1];
	u8	usb_usbs;
	__be16	usb_ussft;
	u8	res5[2];
	__be16	usb_usfrn;
	u8	res6[0x22];
} __attribute__ ((packed));

struct qe_mcc {
	__be32	mcce;		
	__be32	mccm;		
	__be32	mccf;		
	__be32	merl;		
	u8	res0[0xF0];
} __attribute__ ((packed));

struct ucc_slow {
	__be32	gumr_l;		
	__be32	gumr_h;		
	__be16	upsmr;		
	u8	res0[0x2];
	__be16	utodr;		
	__be16	udsr;		
	__be16	ucce;		
	u8	res1[0x2];
	__be16	uccm;		
	u8	res2[0x1];
	u8	uccs;		
	u8	res3[0x24];
	__be16	utpt;
	u8	res4[0x52];
	u8	guemr;		
} __attribute__ ((packed));

struct ucc_fast {
	__be32	gumr;		
	__be32	upsmr;		
	__be16	utodr;		
	u8	res0[0x2];
	__be16	udsr;		
	u8	res1[0x2];
	__be32	ucce;		
	__be32	uccm;		
	u8	uccs;		
	u8	res2[0x7];
	__be32	urfb;		
	__be16	urfs;		
	u8	res3[0x2];
	__be16	urfet;		
	__be16	urfset;		
	__be32	utfb;		
	__be16	utfs;		
	u8	res4[0x2];
	__be16	utfet;		
	u8	res5[0x2];
	__be16	utftt;		
	u8	res6[0x2];
	__be16	utpt;		
	u8	res7[0x2];
	__be32	urtry;		
	u8	res8[0x4C];
	u8	guemr;		
} __attribute__ ((packed));

struct ucc {
	union {
		struct	ucc_slow slow;
		struct	ucc_fast fast;
		u8	res[0x200];	
	};
} __attribute__ ((packed));

struct upc {
	__be32	upgcr;		
	__be32	uplpa;		
	__be32	uphec;		
	__be32	upuc;		
	__be32	updc1;		
	__be32	updc2;		
	__be32	updc3;		
	__be32	updc4;		
	__be32	upstpa;		
	u8	res0[0xC];
	__be32	updrs1_h;	
	__be32	updrs1_l;	
	__be32	updrs2_h;	
	__be32	updrs2_l;	
	__be32	updrs3_h;	
	__be32	updrs3_l;	
	__be32	updrs4_h;	
	__be32	updrs4_l;	
	__be32	updrp1;		
	__be32	updrp2;		
	__be32	updrp3;		
	__be32	updrp4;		
	__be32	upde1;		
	__be32	upde2;		
	__be32	upde3;		
	__be32	upde4;		
	__be16	uprp1;
	__be16	uprp2;
	__be16	uprp3;
	__be16	uprp4;
	u8	res1[0x8];
	__be16	uptirr1_0;	
	__be16	uptirr1_1;	
	__be16	uptirr1_2;	
	__be16	uptirr1_3;	
	__be16	uptirr2_0;	
	__be16	uptirr2_1;	
	__be16	uptirr2_2;	
	__be16	uptirr2_3;	
	__be16	uptirr3_0;	
	__be16	uptirr3_1;	
	__be16	uptirr3_2;	
	__be16	uptirr3_3;	
	__be16	uptirr4_0;	
	__be16	uptirr4_1;	
	__be16	uptirr4_2;	
	__be16	uptirr4_3;	
	__be32	uper1;		
	__be32	uper2;		
	__be32	uper3;		
	__be32	uper4;		
	u8	res2[0x150];
} __attribute__ ((packed));

struct sdma {
	__be32	sdsr;		
	__be32	sdmr;		
	__be32	sdtr1;		
	__be32	sdtr2;		
	__be32	sdhy1;		
	__be32	sdhy2;		
	__be32	sdta1;		
	__be32	sdta2;		
	__be32	sdtm1;		
	__be32	sdtm2;		
	u8	res0[0x10];
	__be32	sdaqr;		
	__be32	sdaqmr;		
	u8	res1[0x4];
	__be32	sdebcr;		
	u8	res2[0x38];
} __attribute__ ((packed));

struct dbg {
	__be32	bpdcr;		
	__be32	bpdsr;		
	__be32	bpdmr;		
	__be32	bprmrr0;	
	__be32	bprmrr1;	
	u8	res0[0x8];
	__be32	bprmtr0;	
	__be32	bprmtr1;	
	u8	res1[0x8];
	__be32	bprmir;		
	__be32	bprmsr;		
	__be32	bpemr;		
	u8	res2[0x48];
} __attribute__ ((packed));

struct rsp {
	__be32 tibcr[16];	
	u8 res0[64];
	__be32 ibcr0;
	__be32 ibs0;
	__be32 ibcnr0;
	u8 res1[4];
	__be32 ibcr1;
	__be32 ibs1;
	__be32 ibcnr1;
	__be32 npcr;
	__be32 dbcr;
	__be32 dbar;
	__be32 dbamr;
	__be32 dbsr;
	__be32 dbcnr;
	u8 res2[12];
	__be32 dbdr_h;
	__be32 dbdr_l;
	__be32 dbdmr_h;
	__be32 dbdmr_l;
	__be32 bsr;
	__be32 bor;
	__be32 bior;
	u8 res3[4];
	__be32 iatr[4];
	__be32 eccr;		
	__be32 eicr;
	u8 res4[0x100-0xf8];
} __attribute__ ((packed));

struct qe_immap {
	struct qe_iram		iram;		
	struct qe_ic_regs	ic;		
	struct cp_qe		cp;		
	struct qe_mux		qmx;		
	struct qe_timers	qet;		
	struct spi		spi[0x2];	
	struct qe_mcc		mcc;		
	struct qe_brg		brg;		
	struct qe_usb_ctlr	usb;		
	struct si1		si1;		
	u8			res11[0x800];
	struct sir		sir;		
	struct ucc		ucc1;		
	struct ucc		ucc3;		
	struct ucc		ucc5;		
	struct ucc		ucc7;		
	u8			res12[0x600];
	struct upc		upc1;		
	struct ucc		ucc2;		
	struct ucc		ucc4;		
	struct ucc		ucc6;		
	struct ucc		ucc8;		
	u8			res13[0x600];
	struct upc		upc2;		
	struct sdma		sdma;		
	struct dbg		dbg;		
	struct rsp		rsp[0x2];	
	u8			res14[0x300];	
	u8			res15[0x3A00];	
	u8			res16[0x8000];	
	u8			muram[0xC000];	
	u8			res17[0x24000];	
	u8			res18[0xC0000];	
} __attribute__ ((packed));

extern struct qe_immap __iomem *qe_immr;
extern phys_addr_t get_qe_base(void);

static inline phys_addr_t immrbar_virt_to_phys(void *address)
{
	void *q = (void *)qe_immr;

	
	if ((address >= q) && (address < (q + QE_IMMAP_SIZE)))
		return get_qe_base() + (address - q);

	
	return virt_to_phys(address);
}

#endif 
#endif 
