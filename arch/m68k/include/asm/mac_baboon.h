
#define BABOON_BASE (0x50F1A000)	

#ifndef __ASSEMBLY__

struct baboon {
	char	pad1[208];	
	short	mb_control;	
	char	pad2[2];
	short	mb_status;	
	char	pad3[2];	
	short	mb_ifr;		
};

extern int baboon_present;

extern void baboon_register_interrupts(void);
extern void baboon_irq_enable(int);
extern void baboon_irq_disable(int);

#endif 
