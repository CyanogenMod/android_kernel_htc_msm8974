#ifndef _LMC_IOCTL_H_
#define _LMC_IOCTL_H_

 /*
  * Copyright (c) 1997-2000 LAN Media Corporation (LMC)
  * All rights reserved.  www.lanmedia.com
  *
  * This code is written by:
  * Andrew Stanley-Jones (asj@cban.com)
  * Rob Braun (bbraun@vix.com),
  * Michael Graff (explorer@vix.com) and
  * Matt Thomas (matt@3am-software.com).
  *
  * This software may be used and distributed according to the terms
  * of the GNU General Public License version 2, incorporated herein by reference.
  */

#define LMCIOCGINFO             SIOCDEVPRIVATE+3 
#define LMCIOCSINFO             SIOCDEVPRIVATE+4 
#define LMCIOCGETLMCSTATS       SIOCDEVPRIVATE+5
#define LMCIOCCLEARLMCSTATS     SIOCDEVPRIVATE+6
#define LMCIOCDUMPEVENTLOG      SIOCDEVPRIVATE+7
#define LMCIOCGETXINFO          SIOCDEVPRIVATE+8
#define LMCIOCSETCIRCUIT        SIOCDEVPRIVATE+9
#define LMCIOCUNUSEDATM         SIOCDEVPRIVATE+10
#define LMCIOCRESET             SIOCDEVPRIVATE+11
#define LMCIOCT1CONTROL         SIOCDEVPRIVATE+12
#define LMCIOCIFTYPE            SIOCDEVPRIVATE+13
#define LMCIOCXILINX            SIOCDEVPRIVATE+14

#define LMC_CARDTYPE_UNKNOWN            -1
#define LMC_CARDTYPE_HSSI               1       
#define LMC_CARDTYPE_DS3                2       
#define LMC_CARDTYPE_SSI                3       
#define LMC_CARDTYPE_T1                 4       

#define LMC_CTL_CARDTYPE_LMC5200	0	
#define LMC_CTL_CARDTYPE_LMC5245	1	
#define LMC_CTL_CARDTYPE_LMC1000	2	
#define LMC_CTL_CARDTYPE_LMC1200        3       

#define LMC_CTL_OFF			0	
#define LMC_CTL_ON			1	

#define LMC_CTL_CLOCK_SOURCE_EXT	0	
#define LMC_CTL_CLOCK_SOURCE_INT	1	

#define LMC_CTL_CRC_LENGTH_16		16
#define LMC_CTL_CRC_LENGTH_32		32
#define LMC_CTL_CRC_BYTESIZE_2          2
#define LMC_CTL_CRC_BYTESIZE_4          4


#define LMC_CTL_CABLE_LENGTH_LT_100FT	0	
#define LMC_CTL_CABLE_LENGTH_GT_100FT	1	

#define LMC_CTL_CIRCUIT_TYPE_E1 0
#define LMC_CTL_CIRCUIT_TYPE_T1 1

#define LMC_PPP         1               
#define LMC_NET         2               
#define LMC_RAW         3               

#define LMC_GEP_INIT		0x01 
#define LMC_GEP_RESET		0x02 
#define LMC_GEP_MODE		0x10 
#define LMC_GEP_DP		0x20 
#define LMC_GEP_DATA		0x40 
#define LMC_GEP_CLK	        0x80 

#define LMC_GEP_HSSI_ST		0x04 
#define LMC_GEP_HSSI_CLOCK	0x08 

#define LMC_GEP_SSI_GENERATOR	0x04 
#define LMC_GEP_SSI_TXCLOCK	0x08 

#define LMC_MII16_LED0         0x0080
#define LMC_MII16_LED1         0x0100
#define LMC_MII16_LED2         0x0200
#define LMC_MII16_LED3         0x0400  
#define LMC_MII16_LED_ALL      0x0780  
#define LMC_MII16_FIFO_RESET   0x0800

#define LMC_MII16_HSSI_TA      0x0001
#define LMC_MII16_HSSI_CA      0x0002
#define LMC_MII16_HSSI_LA      0x0004
#define LMC_MII16_HSSI_LB      0x0008
#define LMC_MII16_HSSI_LC      0x0010
#define LMC_MII16_HSSI_TM      0x0020
#define LMC_MII16_HSSI_CRC     0x0040

#define LMC_MII16_DS3_ZERO	0x0001
#define LMC_MII16_DS3_TRLBK	0x0002
#define LMC_MII16_DS3_LNLBK	0x0004
#define LMC_MII16_DS3_RAIS	0x0008
#define LMC_MII16_DS3_TAIS	0x0010
#define LMC_MII16_DS3_BIST	0x0020
#define LMC_MII16_DS3_DLOS	0x0040
#define LMC_MII16_DS3_CRC	0x1000
#define LMC_MII16_DS3_SCRAM	0x2000
#define LMC_MII16_DS3_SCRAM_LARS 0x4000

#define LMC_DS3_LED0    0x0100          
#define LMC_DS3_LED1    0x0080          
#define LMC_DS3_LED2    0x0400          
#define LMC_DS3_LED3    0x0200          

#define LMC_FRAMER_REG0_DLOS            0x80    
#define LMC_FRAMER_REG0_OOFS            0x40    
#define LMC_FRAMER_REG0_AIS             0x20    
#define LMC_FRAMER_REG0_CIS             0x10    
#define LMC_FRAMER_REG0_LOC             0x08    

#define LMC_FRAMER_REG9_RBLUE          0x02     

#define LMC_FRAMER_REG10_XBIT          0x01     

#define LMC_MII16_SSI_DTR	0x0001	
#define LMC_MII16_SSI_DSR	0x0002	
#define LMC_MII16_SSI_RTS	0x0004	
#define LMC_MII16_SSI_CTS	0x0008	
#define LMC_MII16_SSI_DCD	0x0010	
#define LMC_MII16_SSI_RI		0x0020	
#define LMC_MII16_SSI_CRC                0x1000  

#define LMC_MII16_SSI_LL		0x1000	
#define LMC_MII16_SSI_RL		0x2000	
#define LMC_MII16_SSI_TM		0x4000	
#define LMC_MII16_SSI_LOOP	0x8000	

#define LMC_MII17_SSI_CABLE_MASK	0x0038	
#define LMC_MII17_SSI_CABLE_SHIFT 3	

#define LMC_MII16_T1_UNUSED1    0x0003
#define LMC_MII16_T1_XOE                0x0004
#define LMC_MII16_T1_RST                0x0008  
#define LMC_MII16_T1_Z                  0x0010  
#define LMC_MII16_T1_INTR               0x0020  
#define LMC_MII16_T1_ONESEC             0x0040  

#define LMC_MII16_T1_LED0               0x0100
#define LMC_MII16_T1_LED1               0x0080
#define LMC_MII16_T1_LED2               0x0400
#define LMC_MII16_T1_LED3               0x0200
#define LMC_MII16_T1_FIFO_RESET 0x0800

#define LMC_MII16_T1_CRC                0x1000  
#define LMC_MII16_T1_UNUSED2    0xe000



#define T1FRAMER_ALARM1_STATUS  0x47
#define T1FRAMER_ALARM2_STATUS  0x48
#define T1FRAMER_FERR_LSB               0x50
#define T1FRAMER_FERR_MSB               0x51    
#define T1FRAMER_LCV_LSB                0x54
#define T1FRAMER_LCV_MSB                0x55    
#define T1FRAMER_AERR                   0x5A

#define T1FRAMER_LOF_MASK               (0x0f0) 
#define T1FRAMER_COFA_MASK              (0x0c0) 
#define T1FRAMER_SEF_MASK               (0x03)  


#define T1F_SIGFRZ      0x01    
#define T1F_RLOF        0x02    
#define T1F_RLOS        0x04    
#define T1F_RALOS       0x08    
#define T1F_RAIS        0x10    
#define T1F_UNUSED      0x20
#define T1F_RYEL        0x40    
#define T1F_RMYEL       0x80    

#define LMC_T1F_WRITE       0
#define LMC_T1F_READ        1

typedef struct lmc_st1f_control {
  int command;
  int address;
  int value;
  char __user *data;
} lmc_t1f_control;

enum lmc_xilinx_c {
    lmc_xilinx_reset = 1,
    lmc_xilinx_load_prom = 2,
    lmc_xilinx_load = 3
};

struct lmc_xilinx_control {
    enum lmc_xilinx_c command;
    int len;
    char __user *data;
};


#define LMC_MII_LedMask                 0x0780
#define LMC_MII_LedBitPos               7

#endif
