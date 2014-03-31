/***************************************************************************
 *           WT register offsets.
 *
 *  Wed Oct 22 13:50:20 2003
 *  Copyright  2003  mjander
 *  mjander@users.sourceforge.org
 ****************************************************************************/
#ifndef _AU88X0_WT_H
#define _AU88X0_WT_H


#define NR_WT_PB 0x20

#define WT_BAR(x) (((x)&0xffe0)<<0x8)
#define WT_BANK(x) (x>>5)
#define WT_CTRL(bank)	(((((bank)&1)<<0xd) + 0x00)<<2)	
#define WT_SRAMP(bank)	(((((bank)&1)<<0xd) + 0x01)<<2)	
#define WT_DSREG(bank)	(((((bank)&1)<<0xd) + 0x02)<<2)	
#define WT_MRAMP(bank)	(((((bank)&1)<<0xd) + 0x03)<<2)	
#define WT_GMODE(bank)	(((((bank)&1)<<0xd) + 0x04)<<2)	
#define WT_ARAMP(bank)	(((((bank)&1)<<0xd) + 0x05)<<2)	
#define WT_STEREO(voice)	((WT_BAR(voice)+ 0x20 +(((voice)&0x1f)>>1))<<2)	
#define WT_MUTE(voice)		((WT_BAR(voice)+ 0x40 +((voice)&0x1f))<<2)	
#define WT_RUN(voice)		((WT_BAR(voice)+ 0x60 +((voice)&0x1f))<<2)	
#define WT_PARM(x,y)	(((WT_BAR(x))+ 0x80 +(((x)&0x1f)<<2)+(y))<<2)	
#define WT_DELAY(x,y)	(((WT_BAR(x))+ 0x100 +(((x)&0x1f)<<2)+(y))<<2)	

#if 0
enum {
	run = 0,		
	parm0,			
	parm1,			
	parm2,			
	parm3,			
	sramp,			
	mute,			
	gmode,			
	aramp,			
	mramp,			
	ctrl,			
	delay,			/* b  W All 4 values are written at once with same value. */
	dsreg,			
} wt_reg;
#endif

typedef struct {
	u32 parm0;	
	u32 parm1;	
	u32 parm2;	
	u32 parm3;	
	u32 this_1D0;
} wt_voice_t;

#endif				

