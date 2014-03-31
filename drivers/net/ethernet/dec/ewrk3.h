/*
    Written 1994 by David C. Davies.

    Copyright 1994 Digital Equipment Corporation.

    This software may be used and distributed according to  the terms of the
    GNU General Public License, incorporated herein by reference.

    The author may    be  reached as davies@wanton.lkg.dec.com  or   Digital
    Equipment Corporation, 550 King Street, Littleton MA 01460.

    =========================================================================
*/

#define EWRK3_CSR    iobase+0x00   
#define EWRK3_CR     iobase+0x01   
#define EWRK3_ICR    iobase+0x02   
#define EWRK3_TSR    iobase+0x03   
#define EWRK3_RSVD1  iobase+0x04   
#define EWRK3_RSVD2  iobase+0x05   
#define EWRK3_FMQ    iobase+0x06   
#define EWRK3_FMQC   iobase+0x07   
#define EWRK3_RQ     iobase+0x08   
#define EWRK3_RQC    iobase+0x09   
#define EWRK3_TQ     iobase+0x0a   
#define EWRK3_TQC    iobase+0x0b   
#define EWRK3_TDQ    iobase+0x0c   
#define EWRK3_TDQC   iobase+0x0d   
#define EWRK3_PIR1   iobase+0x0e   
#define EWRK3_PIR2   iobase+0x0f   
#define EWRK3_DATA   iobase+0x10   
#define EWRK3_IOPR   iobase+0x11   
#define EWRK3_IOBR   iobase+0x12   
#define EWRK3_MPR    iobase+0x13   
#define EWRK3_MBR    iobase+0x14   
#define EWRK3_APROM  iobase+0x15   
#define EWRK3_EPROM1 iobase+0x16   
#define EWRK3_EPROM2 iobase+0x17   
#define EWRK3_PAR0   iobase+0x18   
#define EWRK3_PAR1   iobase+0x19   
#define EWRK3_PAR2   iobase+0x1a   
#define EWRK3_PAR3   iobase+0x1b   
#define EWRK3_PAR4   iobase+0x1c   
#define EWRK3_PAR5   iobase+0x1d   
#define EWRK3_CMR    iobase+0x1e   

#define PAGE0_FMQ     0x000         
#define PAGE0_RQ      0x080         
#define PAGE0_TQ      0x100         
#define PAGE0_TDQ     0x180         
#define PAGE0_HTE     0x200         
#define PAGE0_RSVD    0x240         
#define PAGE0_USRD    0x600         

#define CSR_RA		0x80	    
#define CSR_PME		0x40	    
#define CSR_MCE		0x20	    
#define CSR_TNE		0x08	    
#define CSR_RNE		0x04	    
#define CSR_TXD		0x02	    
#define CSR_RXD		0x01	    

#define CR_APD		0x80	
#define CR_PSEL		0x40	
#define CR_LBCK		0x20	
#define CR_FDUP		0x10	
#define CR_FBUS		0x08	
#define CR_EN_16	0x04	
#define CR_LED		0x02	

#define ICR_IE		0x80	
#define ICR_IS		0x60	
#define ICR_TNEM	0x08	
#define ICR_RNEM	0x04	
#define ICR_TXDM	0x02	
#define ICR_RXDM	0x01	

#define TSR_NCL		0x80	
#define TSR_ID		0x40	
#define TSR_LCL		0x20	
#define TSR_ECL		0x10	
#define TSR_RCNTR	0x0f	

#define EEPROM_INIT	0xc0	
#define EEPROM_WR_EN	0xc8	
#define EEPROM_WR	0xd0	
#define EEPROM_WR_DIS	0xd8	
#define EEPROM_RD	0xe0	

#define EISA_REGS_EN	0x20	
#define EISA_IOB        0x1f	

#define CMR_RA          0x80    
#define CMR_WB          0x40    
#define CMR_LINK        0x20	
#define CMR_POLARITY    0x10	
#define CMR_NO_EEPROM	0x0c	
#define CMR_HS          0x08	
#define CMR_PNP         0x04    
#define CMR_DRAM        0x02	
#define CMR_0WS         0x01    


#define R_ROK     	0x80 	
#define R_IAM     	0x10 	
#define R_MCM     	0x08 	
#define R_DBE     	0x04 	
#define R_CRC     	0x02 	
#define R_PLL     	0x01 	


#define TCR_SQEE    	0x40 	
#define TCR_SED     	0x20 	
#define TCR_QMODE     	0x10 	
#define TCR_LAB         0x08 	
#define TCR_PAD     	0x04 	
#define TCR_IFC     	0x02 	
#define TCR_ISA     	0x01 	


#define T_VSTS    	0x80 	
#define T_CTU     	0x40 	
#define T_SQE     	0x20 	
#define T_NCL     	0x10 	
#define T_LCL           0x08 	
#define T_ID      	0x04 	
#define T_COLL     	0x03 	
#define T_XCOLL         0x03    
#define T_MCOLL         0x02    
#define T_OCOLL         0x01    
#define T_NOCOLL        0x00    
#define T_XUR           0x03    
#define T_TXE           0x7f    


#define EISA_ID       iobase + 0x0c80  
#define EISA_ID0      iobase + 0x0c80  
#define EISA_ID1      iobase + 0x0c81  
#define EISA_ID2      iobase + 0x0c82  
#define EISA_ID3      iobase + 0x0c83  
#define EISA_CR       iobase + 0x0c84  

#define EEPROM_MEMB     0x00
#define EEPROM_IOB      0x01
#define EEPROM_EISA_ID0 0x02
#define EEPROM_EISA_ID1 0x03
#define EEPROM_EISA_ID2 0x04
#define EEPROM_EISA_ID3 0x05
#define EEPROM_MISC0    0x06
#define EEPROM_MISC1    0x07
#define EEPROM_PNAME7   0x08
#define EEPROM_PNAME6   0x09
#define EEPROM_PNAME5   0x0a
#define EEPROM_PNAME4   0x0b
#define EEPROM_PNAME3   0x0c
#define EEPROM_PNAME2   0x0d
#define EEPROM_PNAME1   0x0e
#define EEPROM_PNAME0   0x0f
#define EEPROM_SWFLAGS  0x10
#define EEPROM_HWCAT    0x11
#define EEPROM_NETMAN2  0x12
#define EEPROM_REVLVL   0x13
#define EEPROM_NETMAN0  0x14
#define EEPROM_NETMAN1  0x15
#define EEPROM_CHIPVER  0x16
#define EEPROM_SETUP    0x17
#define EEPROM_PADDR0   0x18
#define EEPROM_PADDR1   0x19
#define EEPROM_PADDR2   0x1a
#define EEPROM_PADDR3   0x1b
#define EEPROM_PADDR4   0x1c
#define EEPROM_PADDR5   0x1d
#define EEPROM_PA_CRC   0x1e
#define EEPROM_CHKSUM   0x1f

#define EEPROM_MAX      32             

#define RBE_SHADOW	0x0100	
#define READ_AHEAD      0x0080  
#define IRQ_SEL2        0x0070  
#define IRQ_SEL         0x0060  
#define FAST_BUS        0x0008  
#define ENA_16          0x0004  
#define WRITE_BEHIND    0x0002  
#define _0WS_ENA        0x0001  

#define NETMAN_POL      0x04    
#define NETMAN_LINK     0x02    
#define NETMAN_CCE      0x01    

#define SW_SQE		0x10	
#define SW_LAB		0x08	
#define SW_INIT		0x04	
#define SW_TIMEOUT     	0x02	
#define SW_REMOTE      	0x01    

#define SETUP_APD	0x80	
#define SETUP_PS	0x40	
#define SETUP_MP	0x20	
#define SETUP_1TP	0x10	
#define SETUP_1COAX	0x00	
#define SETUP_DRAM	0x02	

#define MGMT_CCE	0x01	

#define LeMAC           0x11
#define LeMAC2          0x12


#define EEPROM_WAIT_TIME 1000    
#define EISA_EN         0x0001   

#define HASH_TABLE_LEN   512     

#define XCT 0x80                 
#define PRELOAD 16               

#define MASK_INTERRUPTS   1
#define UNMASK_INTERRUPTS 0

#define EEPROM_OFFSET(a) ((u_short)((u_long)(a)))

#include <linux/sockios.h>

#define	EWRK3IOCTL	SIOCDEVPRIVATE

struct ewrk3_ioctl {
	unsigned short cmd;                
	unsigned short len;                
	unsigned char  __user *data;       
};

#define EWRK3_GET_HWADDR	0x01 
#define EWRK3_SET_HWADDR	0x02 
#define EWRK3_SET_PROM  	0x03 
#define EWRK3_CLR_PROM  	0x04 
#define EWRK3_SAY_BOO	        0x05 
#define EWRK3_GET_MCA   	0x06 
#define EWRK3_SET_MCA   	0x07 
#define EWRK3_CLR_MCA    	0x08 
#define EWRK3_MCA_EN    	0x09 
#define EWRK3_GET_STATS  	0x0a 
#define EWRK3_CLR_STATS 	0x0b 
#define EWRK3_GET_CSR   	0x0c 
#define EWRK3_SET_CSR   	0x0d 
#define EWRK3_GET_EEPROM   	0x0e 
#define EWRK3_SET_EEPROM	0x0f 
#define EWRK3_GET_CMR   	0x10 
#define EWRK3_CLR_TX_CUT_THRU  	0x11 
#define EWRK3_SET_TX_CUT_THRU	0x12 
