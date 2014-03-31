/*
 *  longhaul.h
 *  (C) 2003 Dave Jones.
 *
 *  Licensed under the terms of the GNU GPL License version 2.
 *
 *  VIA-specific information
 */

union msr_bcr2 {
	struct {
		unsigned Reseved:19,	
		ESOFTBF:1,		
		Reserved2:3,		
		CLOCKMUL:4,		
		Reserved3:5;		
	} bits;
	unsigned long val;
};

union msr_longhaul {
	struct {
		unsigned RevisionID:4,	
		RevisionKey:4,		
		EnableSoftBusRatio:1,	
		EnableSoftVID:1,	
		EnableSoftBSEL:1,	
		Reserved:3,		
		SoftBusRatio4:1,	
		VRMRev:1,		
		SoftBusRatio:4,		
		SoftVID:5,		
		Reserved2:3,		
		SoftBSEL:2,		
		Reserved3:2,		
		MaxMHzBR:4,		
		MaximumVID:5,		
		MaxMHzFSB:2,		
		MaxMHzBR4:1,		
		Reserved4:4,		
		MinMHzBR:4,		
		MinimumVID:5,		
		MinMHzFSB:2,		
		MinMHzBR4:1,		
		Reserved5:4;		
	} bits;
	unsigned long long val;
};


static const int __cpuinitdata samuel1_mults[16] = {
	-1, 
	30, 
	40, 
	-1, 
	-1, 
	35, 
	45, 
	55, 
	60, 
	70, 
	80, 
	50, 
	65, 
	75, 
	-1, 
	-1, 
};

static const int __cpuinitdata samuel1_eblcr[16] = {
	50, 
	30, 
	40, 
	-1, 
	55, 
	35, 
	45, 
	-1, 
	-1, 
	70, 
	80, 
	60, 
	-1, 
	75, 
	-1, 
	65, 
};

static const int __cpuinitdata samuel2_eblcr[16] = {
	50,  
	30,  
	40,  
	100, 
	55,  
	35,  
	45,  
	110, 
	90,  
	70,  
	80,  
	60,  
	120, 
	75,  
	130, 
	65,  
};

static const int __cpuinitdata ezra_mults[16] = {
	100, 
	30,  
	40,  
	90,  
	95,  
	35,  
	45,  
	55,  
	60,  
	70,  
	80,  
	50,  
	65,  
	75,  
	85,  
	120, 
};

static const int __cpuinitdata ezra_eblcr[16] = {
	50,  
	30,  
	40,  
	100, 
	55,  
	35,  
	45,  
	95,  
	90,  
	70,  
	80,  
	60,  
	120, 
	75,  
	85,  
	65,  
};

static const int __cpuinitdata ezrat_mults[32] = {
	100, 
	30,  
	40,  
	90,  
	95,  
	35,  
	45,  
	55,  
	60,  
	70,  
	80,  
	50,  
	65,  
	75,  
	85,  
	120, 

	-1,  
	110, 
	-1, 
	-1,  
	105, 
	115, 
	125, 
	135, 
	140, 
	150, 
	160, 
	130, 
	145, 
	155, 
	-1,  
	-1,  
};

static const int __cpuinitdata ezrat_eblcr[32] = {
	50,  
	30,  
	40,  
	100, 
	55,  
	35,  
	45,  
	95,  
	90,  
	70,  
	80,  
	60,  
	120, 
	75,  
	85,  
	65,  

	-1,  
	110, 
	120, 
	-1,  
	135, 
	115, 
	125, 
	105, 
	130, 
	150, 
	160, 
	140, 
	-1,  
	155, 
	-1,  
	145, 
};


static const int __cpuinitdata nehemiah_mults[32] = {
	100, 
	-1, 
	40,  
	90,  
	95,  
	-1,  
	45,  
	55,  
	60,  
	70,  
	80,  
	50,  
	65,  
	75,  
	85,  
	120, 
	-1, 
	110, 
	-1, 
	-1,  
	105, 
	115, 
	125, 
	135, 
	140, 
	150, 
	160, 
	130, 
	145, 
	155, 
	-1,  
	-1, 
};

static const int __cpuinitdata nehemiah_eblcr[32] = {
	50,  
	160, 
	40,  
	100, 
	55,  
	-1,  
	45,  
	95,  
	90,  
	70,  
	80,  
	60,  
	120, 
	75,  
	85,  
	65,  
	90,  
	110, 
	120, 
	100, 
	135, 
	115, 
	125, 
	105, 
	130, 
	150, 
	160, 
	140, 
	120, 
	155, 
	-1,  
	145 
};


struct mV_pos {
	unsigned short mV;
	unsigned short pos;
};

static const struct mV_pos __cpuinitdata vrm85_mV[32] = {
	{1250, 8},	{1200, 6},	{1150, 4},	{1100, 2},
	{1050, 0},	{1800, 30},	{1750, 28},	{1700, 26},
	{1650, 24},	{1600, 22},	{1550, 20},	{1500, 18},
	{1450, 16},	{1400, 14},	{1350, 12},	{1300, 10},
	{1275, 9},	{1225, 7},	{1175, 5},	{1125, 3},
	{1075, 1},	{1825, 31},	{1775, 29},	{1725, 27},
	{1675, 25},	{1625, 23},	{1575, 21},	{1525, 19},
	{1475, 17},	{1425, 15},	{1375, 13},	{1325, 11}
};

static const unsigned char __cpuinitdata mV_vrm85[32] = {
	0x04,	0x14,	0x03,	0x13,	0x02,	0x12,	0x01,	0x11,
	0x00,	0x10,	0x0f,	0x1f,	0x0e,	0x1e,	0x0d,	0x1d,
	0x0c,	0x1c,	0x0b,	0x1b,	0x0a,	0x1a,	0x09,	0x19,
	0x08,	0x18,	0x07,	0x17,	0x06,	0x16,	0x05,	0x15
};

static const struct mV_pos __cpuinitdata mobilevrm_mV[32] = {
	{1750, 31},	{1700, 30},	{1650, 29},	{1600, 28},
	{1550, 27},	{1500, 26},	{1450, 25},	{1400, 24},
	{1350, 23},	{1300, 22},	{1250, 21},	{1200, 20},
	{1150, 19},	{1100, 18},	{1050, 17},	{1000, 16},
	{975, 15},	{950, 14},	{925, 13},	{900, 12},
	{875, 11},	{850, 10},	{825, 9},	{800, 8},
	{775, 7},	{750, 6},	{725, 5},	{700, 4},
	{675, 3},	{650, 2},	{625, 1},	{600, 0}
};

static const unsigned char __cpuinitdata mV_mobilevrm[32] = {
	0x1f,	0x1e,	0x1d,	0x1c,	0x1b,	0x1a,	0x19,	0x18,
	0x17,	0x16,	0x15,	0x14,	0x13,	0x12,	0x11,	0x10,
	0x0f,	0x0e,	0x0d,	0x0c,	0x0b,	0x0a,	0x09,	0x08,
	0x07,	0x06,	0x05,	0x04,	0x03,	0x02,	0x01,	0x00
};

