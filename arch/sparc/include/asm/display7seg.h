/*
 *
 * display7seg - Driver interface for the 7-segment display
 * present on Sun Microsystems CP1400 and CP1500
 *
 * Copyright (c) 2000 Eric Brower <ebrower@usa.net>
 *
 */

#ifndef __display7seg_h__
#define __display7seg_h__

#define D7S_IOC	'p'

#define D7SIOCRD _IOR(D7S_IOC, 0x45, int)	
#define D7SIOCWR _IOW(D7S_IOC, 0x46, int)	
#define D7SIOCTM _IO (D7S_IOC, 0x47)		


#define D7S_POINT	(1 << 7)	
#define D7S_ALARM	(1 << 6)	
#define D7S_FLIP	(1 << 5)	

#define D7S_0		0x00		
#define D7S_1		0x01
#define D7S_2		0x02
#define D7S_3		0x03
#define D7S_4		0x04
#define D7S_5		0x05
#define D7S_6		0x06
#define D7S_7		0x07
#define D7S_8		0x08
#define D7S_9		0x09
#define D7S_A		0x0A		
#define D7S_B		0x0B
#define D7S_C		0x0C
#define D7S_D		0x0D
#define D7S_E		0x0E
#define D7S_F		0x0F
#define D7S_H		0x10
#define D7S_E2		0x11
#define D7S_L		0x12
#define D7S_P		0x13
#define D7S_SEGA	0x14		
#define D7S_SEGB	0x15
#define D7S_SEGC	0x16
#define D7S_SEGD	0x17
#define D7S_SEGE	0x18
#define D7S_SEGF	0x19
#define D7S_SEGG	0x1A
#define D7S_SEGABFG 0x1B		
#define D7S_SEGCDEG	0x1C
#define D7S_SEGBCEF 0x1D
#define D7S_SEGADG	0x1E
#define D7S_BLANK	0x1F		

#define D7S_MIN_VAL	0x0
#define D7S_MAX_VAL	0x1F

#endif 
