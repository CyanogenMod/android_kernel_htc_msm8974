/*
 * board initialization should put one of these structures into platform_data
 * and place the bfin-rotary onto platform_bus named "bfin-rotary".
 *
 * Copyright 2008-2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef _BFIN_ROTARY_H
#define _BFIN_ROTARY_H

#define ROT_QUAD_ENC	CNTMODE_QUADENC	
#define ROT_BIN_ENC	CNTMODE_BINENC	
#define ROT_UD_CNT	CNTMODE_UDCNT	
#define ROT_DIR_CNT	CNTMODE_DIRCNT	

#define ROT_DEBE	DEBE		

#define ROT_CDGINV	CDGINV		
#define ROT_CUDINV	CUDINV		
#define ROT_CZMINV	CZMINV		

struct bfin_rotary_platform_data {
	unsigned int rotary_up_key;
	unsigned int rotary_down_key;
	
	unsigned int rotary_button_key;
	unsigned int rotary_rel_code;
	unsigned short debounce;	
	unsigned short mode;
};

#define CNTE		(1 << 0)	
#define DEBE		(1 << 1)	
#define CDGINV		(1 << 4)	
#define CUDINV		(1 << 5)	
#define CZMINV		(1 << 6)	
#define CNTMODE_SHIFT	8
#define CNTMODE		(0x7 << CNTMODE_SHIFT)	
#define ZMZC		(1 << 1)	
#define BNDMODE_SHIFT	12
#define BNDMODE		(0x3 << BNDMODE_SHIFT)	
#define INPDIS		(1 << 15)	

#define CNTMODE_QUADENC	(0 << CNTMODE_SHIFT)	
#define CNTMODE_BINENC	(1 << CNTMODE_SHIFT)	
#define CNTMODE_UDCNT	(2 << CNTMODE_SHIFT)	
#define CNTMODE_DIRCNT	(4 << CNTMODE_SHIFT)	
#define CNTMODE_DIRTMR	(5 << CNTMODE_SHIFT)	

#define BNDMODE_COMP	(0 << BNDMODE_SHIFT)	
#define BNDMODE_ZERO	(1 << BNDMODE_SHIFT)	
#define BNDMODE_CAPT	(2 << BNDMODE_SHIFT)	
#define BNDMODE_AEXT	(3 << BNDMODE_SHIFT)	

#define ICIE		(1 << 0)	
#define UCIE		(1 << 1)	
#define DCIE		(1 << 2)	
#define MINCIE		(1 << 3)	
#define MAXCIE		(1 << 4)	
#define COV31IE		(1 << 5)	
#define COV15IE		(1 << 6)	
#define CZEROIE		(1 << 7)	
#define CZMIE		(1 << 8)	
#define CZMEIE		(1 << 9)	
#define CZMZIE		(1 << 10)	

#define ICII		(1 << 0)	
#define UCII		(1 << 1)	
#define DCII		(1 << 2)	
#define MINCII		(1 << 3)	
#define MAXCII		(1 << 4)	
#define COV31II		(1 << 5)	
#define COV15II		(1 << 6)	
#define CZEROII		(1 << 7)	
#define CZMII		(1 << 8)	
#define CZMEII		(1 << 9)	
#define CZMZII		(1 << 10)	

#define W1LCNT		0xf		
#define W1LMIN		0xf0		
#define W1LMAX		0xf00		
#define W1ZMONCE	(1 << 12)	

#define W1LCNT_ZERO	(1 << 0)	
#define W1LCNT_MIN	(1 << 2)	
#define W1LCNT_MAX	(1 << 3)	

#define W1LMIN_ZERO	(1 << 4)	
#define W1LMIN_CNT	(1 << 5)	
#define W1LMIN_MAX	(1 << 7)	

#define W1LMAX_ZERO	(1 << 8)	
#define W1LMAX_CNT	(1 << 9)	
#define W1LMAX_MIN	(1 << 10)	

#define DPRESCALE	0xf		

#endif
