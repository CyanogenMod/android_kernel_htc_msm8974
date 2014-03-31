/*
 * mace.h - definitions for the registers in the Am79C940 MACE
 * (Medium Access Control for Ethernet) controller.
 *
 * Copyright (C) 1996 Paul Mackerras.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define REG(x)	volatile unsigned char x; char x ## _pad[15]

struct mace {
	REG(rcvfifo);		
	REG(xmtfifo);		
	REG(xmtfc);		
	REG(xmtfs);		
	REG(xmtrc);		
	REG(rcvfc);		
	REG(rcvfs);		
	REG(fifofc);		
	REG(ir);		
	REG(imr);		
	REG(pr);		
	REG(biucc);		
	REG(fifocc);		
	REG(maccc);		
	REG(plscc);		
	REG(phycc);		
	REG(chipid_lo);		
	REG(chipid_hi);		
	REG(iac);		
	REG(reg19);
	REG(ladrf);		
	REG(padr);		
	REG(reg22);
	REG(reg23);
	REG(mpc);		
	REG(reg25);
	REG(rntpc);		
	REG(rcvcc);		
	REG(reg28);
	REG(utr);		
	REG(reg30);
	REG(reg31);
};

#define DRTRY		0x80	
#define DXMTFCS		0x08	
#define AUTO_PAD_XMIT	0x01	

#define XMTSV		0x80	
#define UFLO		0x40	
#define LCOL		0x20	
#define MORE		0x10	
#define ONE		0x08	
#define DEFER		0x04	
#define LCAR		0x02	
#define RTRY		0x01	

#define EXDEF		0x80	
#define RETRY_MASK	0x0f	

#define LLRCV		0x08	
#define M_RBAR		0x04	
#define AUTO_STRIP_RCV	0x01	

#define RS_OFLO		0x8000	
#define RS_CLSN		0x4000	
#define RS_FRAMERR	0x2000	
#define RS_FCSERR	0x1000	
#define RS_COUNT	0x0fff	

#define RCVFC_SH	4	
#define RCVFC_MASK	0x0f
#define XMTFC_SH	0	
#define XMTFC_MASK	0x0f

#define JABBER		0x80	
#define BABBLE		0x40	
#define CERR		0x20	
#define RCVCCO		0x10	
#define RNTPCO		0x08	
#define MPCO		0x04	
#define RCVINT		0x02	
#define XMTINT		0x01	

#define XMTSV		0x80	
#define TDTREQ		0x40	
#define RDTREQ		0x20	

#define BSWP		0x40	
#define XMTSP_4		0x00	
#define XMTSP_16	0x10	
#define XMTSP_64	0x20	
#define XMTSP_112	0x30	
#define SWRST		0x01	

#define XMTFW_8		0x00	
#define XMTFW_16	0x40	
#define XMTFW_32	0x80	
#define RCVFW_16	0x00	
#define RCVFW_32	0x10	
#define RCVFW_64	0x20	
#define XMTFWU		0x08	
#define RCVFWU		0x04	
#define XMTBRST		0x02	
#define RCVBRST		0x01	

#define PROM		0x80	
#define DXMT2PD		0x40	
#define EMBA		0x20	
#define DRCVPA		0x08	
#define DRCVBC		0x04	
#define ENXMT		0x02	
#define ENRCV		0x01	

#define XMTSEL		0x08	
#define PORTSEL_AUI	0x00	
#define PORTSEL_10T	0x02	
#define PORTSEL_DAI	0x04	
#define PORTSEL_GPSI	0x06	
#define ENPLSIO		0x01	

#define LNKFL		0x80	
#define DLNKTST		0x40	
#define REVPOL		0x20	
#define DAPC		0x10	
#define LRT		0x08	
#define ASEL		0x04	
#define RWAKE		0x02	
#define AWAKE		0x01	

#define ADDRCHG		0x80	
#define PHYADDR		0x04	
#define LOGADDR		0x02	

#define RTRE		0x80	
#define RTRD		0x40	
#define RPAC		0x20	
#define FCOLL		0x10	
#define RCVFCSE		0x08	
#define LOOP_NONE	0x00	
#define LOOP_EXT	0x02	
#define LOOP_INT	0x04	
#define LOOP_MENDEC	0x06	
