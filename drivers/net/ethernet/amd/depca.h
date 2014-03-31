/*
    Written 1994 by David C. Davies.

    Copyright 1994 David C. Davies. This software may be used and distributed
    according to the terms of the GNU General Public License, incorporated herein by
    reference.
*/

#define DEPCA_NICSR ioaddr+0x00   
#define DEPCA_RBI   ioaddr+0x02   
#define DEPCA_DATA  ioaddr+0x04   
#define DEPCA_ADDR  ioaddr+0x06   
#define DEPCA_HBASE ioaddr+0x08   
#define DEPCA_PROM  ioaddr+0x0c   
#define DEPCA_CNFG  ioaddr+0x0c   
#define DEPCA_RBSA  ioaddr+0x0e   

#define CSR0       0
#define CSR1       1
#define CSR2       2
#define CSR3       3


#define TO       	0x0100	
#define SHE      	0x0080  
#define BS       	0x0040  
#define BUF      	0x0020	
#define RBE      	0x0010	
#define AAC      	0x0008  
#define _128KB      	0x0008  
#define IM       	0x0004	
#define IEN      	0x0002	
#define LED      	0x0001	


#define ERR     	0x8000 	
#define BABL    	0x4000 	
#define CERR    	0x2000 	
#define MISS    	0x1000 	
#define MERR    	0x0800 	
#define RINT    	0x0400 	
#define TINT    	0x0200 	
#define IDON    	0x0100 	
#define INTR    	0x0080 	
#define INEA    	0x0040 	
#define RXON    	0x0020 	
#define TXON    	0x0010 	
#define TDMD    	0x0008 	
#define STOP    	0x0004 	
#define STRT    	0x0002 	
#define INIT    	0x0001 	
#define INTM            0xff00  
#define INTE            0xfff0  


#define BSWP    	0x0004	
#define ACON    	0x0002	
#define BCON    	0x0001	


#define PROM       	0x8000 	
#define EMBA       	0x0080	
#define INTL       	0x0040 	
#define DRTY       	0x0020 	
#define COLL       	0x0010 	
#define DTCR       	0x0008 	
#define LOOP       	0x0004 	
#define DTX        	0x0002 	
#define DRX        	0x0001 	


#define R_OWN       0x80000000 	
#define R_ERR     	0x4000 	
#define R_FRAM    	0x2000 	
#define R_OFLO    	0x1000 	
#define R_CRC     	0x0800 	
#define R_BUFF    	0x0400 	
#define R_STP     	0x0200 	
#define R_ENP     	0x0100 	


#define T_OWN       0x80000000 	
#define T_ERR     	0x4000 	
#define T_ADD_FCS 	0x2000 	
#define T_MORE    	0x1000	
#define T_ONE     	0x0800 	
#define T_DEF     	0x0400 	
#define T_STP       0x02000000 	
#define T_ENP       0x01000000	
#define T_FLAGS     0xff000000  


#define TMD3_BUFF    0x8000	
#define TMD3_UFLO    0x4000	
#define TMD3_RES     0x2000	
#define TMD3_LCOL    0x1000	
#define TMD3_LCAR    0x0800	
#define TMD3_RTRY    0x0400	


#define TIMEOUT       	0x0100	
#define REMOTE      	0x0080  
#define IRQ11       	0x0040  
#define IRQ10    	0x0020	
#define IRQ9    	0x0010	
#define IRQ5      	0x0008  
#define BUFF     	0x0004	
#define PADR16   	0x0002	
#define PADR17    	0x0001	

#define HASH_TABLE_LEN   64           
#define HASH_BITS        0x003f       

#define MASK_INTERRUPTS   1
#define UNMASK_INTERRUPTS 0

#define EISA_EN         0x0001        
#define EISA_ID         iobase+0x0080 
#define EISA_CTRL       iobase+0x0084 

#include <linux/sockios.h>

struct depca_ioctl {
	unsigned short cmd;                
	unsigned short len;                
	unsigned char  __user *data;       
};

#define DEPCA_GET_HWADDR	0x01 
#define DEPCA_SET_HWADDR	0x02 
#define DEPCA_SET_PROM  	0x03 
#define DEPCA_CLR_PROM  	0x04 
#define DEPCA_SAY_BOO	        0x05 
#define DEPCA_GET_MCA   	0x06 
#define DEPCA_SET_MCA   	0x07 
#define DEPCA_CLR_MCA    	0x08 
#define DEPCA_MCA_EN    	0x09 
#define DEPCA_GET_STATS  	0x0a 
#define DEPCA_CLR_STATS 	0x0b 
#define DEPCA_GET_REG   	0x0c 
#define DEPCA_SET_REG   	0x0d 
#define DEPCA_DUMP              0x0f 

