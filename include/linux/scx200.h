/* linux/include/linux/scx200.h

   Copyright (c) 2001,2002 Christer Weinigel <wingel@nano-system.com>

   Defines for the National Semiconductor SCx200 Processors
*/


extern unsigned scx200_cb_base;

#define scx200_cb_present() (scx200_cb_base!=0)

#define SCx200_DOCCS_BASE 0x78	
#define SCx200_DOCCS_CTRL 0x7c	

#define SCx200_GPIO_SIZE 0x2c	

#define SCx200_CB_BASE_FIXED 0x9000	

#define SCx200_WDT_OFFSET 0x00	
#define SCx200_WDT_SIZE 0x05	

#define SCx200_WDT_WDTO 0x00	
#define SCx200_WDT_WDCNFG 0x02	
#define SCx200_WDT_WDSTS 0x04	
#define SCx200_WDT_WDSTS_WDOVF (1<<0) 

#define SCx200_TIMER_OFFSET 0x08
#define SCx200_TIMER_SIZE 0x06

#define SCx200_CLOCKGEN_OFFSET 0x10
#define SCx200_CLOCKGEN_SIZE 0x10

#define SCx200_MISC_OFFSET 0x30
#define SCx200_MISC_SIZE 0x10

#define SCx200_PMR 0x30		
#define SCx200_MCR 0x34		
#define SCx200_INTSEL 0x38	
#define SCx200_IID 0x3c		
#define SCx200_REV 0x3d		
#define SCx200_CBA 0x3e		
#define SCx200_CBA_SCRATCH 0x64	
