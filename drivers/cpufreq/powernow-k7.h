/*
 *  (C) 2003 Dave Jones.
 *
 *  Licensed under the terms of the GNU GPL License version 2.
 *
 *  AMD-specific information
 *
 */

union msr_fidvidctl {
	struct {
		unsigned FID:5,			
		reserved1:3,	
		VID:5,			
		reserved2:3,	
		FIDC:1,			
		VIDC:1,			
		reserved3:2,	
		FIDCHGRATIO:1,	
		reserved4:11,	
		SGTC:20,		
		reserved5:12;	
	} bits;
	unsigned long long val;
};

union msr_fidvidstatus {
	struct {
		unsigned CFID:5,			
		reserved1:3,	
		SFID:5,			
		reserved2:3,	
		MFID:5,			
		reserved3:11,	
		CVID:5,			
		reserved4:3,	
		SVID:5,			
		reserved5:3,	
		MVID:5,			
		reserved6:11;	
	} bits;
	unsigned long long val;
};
