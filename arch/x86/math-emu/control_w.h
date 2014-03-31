/*---------------------------------------------------------------------------+
 |  control_w.h                                                              |
 |                                                                           |
 | Copyright (C) 1992,1993                                                   |
 |                       W. Metzenthen, 22 Parker St, Ormond, Vic 3163,      |
 |                       Australia.  E-mail   billm@vaxc.cc.monash.edu.au    |
 |                                                                           |
 +---------------------------------------------------------------------------*/

#ifndef _CONTROLW_H_
#define _CONTROLW_H_

#ifdef __ASSEMBLY__
#define	_Const_(x)	$##x
#else
#define	_Const_(x)	x
#endif

#define CW_RC		_Const_(0x0C00)	
#define CW_PC		_Const_(0x0300)	

#define CW_Precision	Const_(0x0020)	
#define CW_Underflow	Const_(0x0010)	
#define CW_Overflow	Const_(0x0008)	
#define CW_ZeroDiv	Const_(0x0004)	
#define CW_Denormal	Const_(0x0002)	
#define CW_Invalid	Const_(0x0001)	

#define CW_Exceptions  	_Const_(0x003f)	

#define RC_RND		_Const_(0x0000)
#define RC_DOWN		_Const_(0x0400)
#define RC_UP		_Const_(0x0800)
#define RC_CHOP		_Const_(0x0C00)

#define PR_24_BITS        _Const_(0x000)
#define PR_53_BITS        _Const_(0x200)
#define PR_64_BITS        _Const_(0x300)
#define PR_RESERVED_BITS  _Const_(0x100)
#define FULL_PRECISION  (PR_64_BITS | RC_RND | 0x3f)

#endif 
