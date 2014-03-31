/*
 * Micrel KS8695 (Centaur) Ethernet.
 *
 * Copyright 2008 Simtec Electronics
 *		  Daniel Silverstone <dsilvers@simtec.co.uk>
 *		  Vincent Sanders <vince@simtec.co.uk>
 */

#ifndef KS8695NET_H
#define KS8695NET_H

#define RDES_OWN	(1 << 31)	
#define RDES_FS		(1 << 30)	
#define RDES_LS		(1 << 29)	
#define RDES_IPE	(1 << 28)	
#define RDES_TCPE	(1 << 27)	
#define RDES_UDPE	(1 << 26)	
#define RDES_ES		(1 << 25)	
#define RDES_MF		(1 << 24)	
#define RDES_RE		(1 << 19)	
#define RDES_TL		(1 << 18)	
#define RDES_RF		(1 << 17)	
#define RDES_CE		(1 << 16)	
#define RDES_FT		(1 << 15)	
#define RDES_FLEN	(0x7ff)		

#define RDES_RER	(1 << 25)	
#define RDES_RBS	(0x7ff)		


#define TDES_OWN	(1 << 31)	

#define TDES_IC		(1 << 31)	
#define TDES_FS		(1 << 30)	
#define TDES_LS		(1 << 29)	
#define TDES_IPCKG	(1 << 28)	
#define TDES_TCPCKG	(1 << 27)	
#define TDES_UDPCKG	(1 << 26)	
#define TDES_TER	(1 << 25)	
#define TDES_TBS	(0x7ff)		

#define KS8695_DTXC		(0x00)		
#define KS8695_DRXC		(0x04)		
#define KS8695_DTSC		(0x08)		
#define KS8695_DRSC		(0x0c)		
#define KS8695_TDLB		(0x10)		
#define KS8695_RDLB		(0x14)		
#define KS8695_MAL		(0x18)		
#define KS8695_MAH		(0x1c)		
#define KS8695_AAL_(n)		(0x80 + ((n)*8))	
#define KS8695_AAH_(n)		(0x84 + ((n)*8))	


#define DTXC_TRST		(1    << 31)	
#define DTXC_TBS		(0x3f << 24)	
#define DTXC_TUCG		(1    << 18)	
#define DTXC_TTCG		(1    << 17)	
#define DTXC_TICG		(1    << 16)	
#define DTXC_TFCE		(1    <<  9)	
#define DTXC_TLB		(1    <<  8)	
#define DTXC_TEP		(1    <<  2)	
#define DTXC_TAC		(1    <<  1)	
#define DTXC_TE			(1    <<  0)	

#define DRXC_RBS		(0x3f << 24)	
#define DRXC_RUCC		(1    << 18)	
#define DRXC_RTCG		(1    << 17)	
#define DRXC_RICG		(1    << 16)	
#define DRXC_RFCE		(1    <<  9)	
#define DRXC_RB			(1    <<  6)	
#define DRXC_RM			(1    <<  5)	
#define DRXC_RU			(1    <<  4)	
#define DRXC_RERR		(1    <<  3)	
#define DRXC_RA			(1    <<  2)	
#define DRXC_RE			(1    <<  0)	

#define AAH_E			(1    << 31)	

#endif 
