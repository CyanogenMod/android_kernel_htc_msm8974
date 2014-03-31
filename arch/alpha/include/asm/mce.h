#ifndef __ALPHA_MCE_H
#define __ALPHA_MCE_H

struct el_common {
	unsigned int	size;		
	unsigned int	sbz1	: 30;	
	unsigned int	err2	:  1;	
	unsigned int	retry	:  1;	
	unsigned int	proc_offset;	
	unsigned int	sys_offset;	
	unsigned int	code;		
	unsigned int	frame_rev;	
};

struct el_common_EV5_uncorrectable_mcheck {
        unsigned long   shadow[8];        
        unsigned long   paltemp[24];      
        unsigned long   exc_addr;         
        unsigned long   exc_sum;          
        unsigned long   exc_mask;         
        unsigned long   pal_base;         
        unsigned long   isr;              
        unsigned long   icsr;             
        unsigned long   ic_perr_stat;     
        unsigned long   dc_perr_stat;     
        unsigned long   va;               
        unsigned long   mm_stat;          
        unsigned long   sc_addr;          
        unsigned long   sc_stat;          
        unsigned long   bc_tag_addr;      
        unsigned long   ei_addr;          
        unsigned long   fill_syndrome;    
        unsigned long   ei_stat;          
        unsigned long   ld_lock;          
};

struct el_common_EV6_mcheck {
	unsigned int FrameSize;		
	unsigned int FrameFlags;	
	unsigned int CpuOffset;		
	unsigned int SystemOffset;	
	unsigned int MCHK_Code;
	unsigned int MCHK_Frame_Rev;
	unsigned long I_STAT;		
	unsigned long DC_STAT;		
	unsigned long C_ADDR;
	unsigned long DC1_SYNDROME;
	unsigned long DC0_SYNDROME;
	unsigned long C_STAT;
	unsigned long C_STS;
	unsigned long MM_STAT;
	unsigned long EXC_ADDR;
	unsigned long IER_CM;
	unsigned long ISUM;
	unsigned long RESERVED0;
	unsigned long PAL_BASE;
	unsigned long I_CTL;
	unsigned long PCTX;
};


#endif 
