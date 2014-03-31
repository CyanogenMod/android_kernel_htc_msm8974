#ifndef _CAN_PLATFORM_CC770_H_
#define _CAN_PLATFORM_CC770_H_

#define CPUIF_CEN	0x01	
#define CPUIF_MUX	0x04	
#define CPUIF_SLP	0x08	
#define CPUIF_PWD	0x10	
#define CPUIF_DMC	0x20	
#define CPUIF_DSC	0x40	
#define CPUIF_RST	0x80	

#define CLKOUT_CD_MASK  0x0f	
#define CLKOUT_SL_MASK	0x30	
#define CLKOUT_SL_SHIFT	4

#define BUSCFG_DR0	0x01	
#define BUSCFG_DR1	0x02	
#define BUSCFG_DT1	0x08	
#define BUSCFG_POL	0x20	
#define BUSCFG_CBY	0x40	

struct cc770_platform_data {
	u32 osc_freq;	

	u8 cir;		
	u8 cor;		
	u8 bcr;		
};

#endif	
