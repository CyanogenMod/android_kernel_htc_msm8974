#ifndef _LINUX_AMIFDREG_H
#define _LINUX_AMIFDREG_H


#define DSKRDY      (0x1<<5)        
#define DSKTRACK0   (0x1<<4)        
#define DSKPROT     (0x1<<3)        
#define DSKCHANGE   (0x1<<2)        


#define DSKMOTOR    (0x1<<7)        
#define DSKSEL3     (0x1<<6)        
#define DSKSEL2     (0x1<<5)        
#define DSKSEL1     (0x1<<4)        
#define DSKSEL0     (0x1<<3)        
#define DSKSIDE     (0x1<<2)        
#define DSKDIREC    (0x1<<1)        
#define DSKSTEP     (0x1)           


#define DSKBYT      (1<<15)         
#define DMAON       (1<<14)         
#define DISKWRITE   (1<<13)         
#define WORDEQUAL   (1<<12)         


#ifndef SETCLR
#define ADK_SETCLR      (1<<15)     
#endif
#define ADK_PRECOMP1    (1<<14)     
#define ADK_PRECOMP0    (1<<13)     
#define ADK_MFMPREC     (1<<12)     
#define ADK_WORDSYNC    (1<<10)     
#define ADK_MSBSYNC     (1<<9)      
#define ADK_FAST        (1<<8)      
 

#define DSKLEN_DMAEN    (1<<15)
#define DSKLEN_WRITE    (1<<14)


#define DSKINDEX    (0x1<<4)        

 
#define MFM_SYNC    0x4489          

#define FD_RECALIBRATE		0x07	
#define FD_SEEK			0x0F	
#define FD_READ			0xE6	
#define FD_WRITE		0xC5	
#define FD_SENSEI		0x08	
#define FD_SPECIFY		0x03	
#define FD_FORMAT		0x4D	
#define FD_VERSION		0x10	
#define FD_CONFIGURE		0x13	
#define FD_PERPENDICULAR	0x12	

#endif 
