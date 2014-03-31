/*
 *      FarSync X21 driver for Linux
 *
 *      Actually sync driver for X.21, V.35 and V.24 on FarSync T-series cards
 *
 *      Copyright (C) 2001 FarSite Communications Ltd.
 *      www.farsite.co.uk
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Author: R.J.Dunlop      <bob.dunlop@farsite.co.uk>
 *
 *      For the most part this file only contains structures and information
 *      that is visible to applications outside the driver. Shared memory
 *      layout etc is internal to the driver and described within farsync.c.
 *      Overlap exists in that the values used for some fields within the
 *      ioctl interface extend into the cards firmware interface so values in
 *      this file may not be changed arbitrarily.
 */

#define FST_NAME                "fst"           
#define FST_NDEV_NAME           "sync"          
#define FST_DEV_NAME            "farsync"       


#define FST_USER_VERSION        "1.04"


#define FSTWRITE        (SIOCDEVPRIVATE+10)
#define FSTCPURESET     (SIOCDEVPRIVATE+11)
#define FSTCPURELEASE   (SIOCDEVPRIVATE+12)
#define FSTGETCONF      (SIOCDEVPRIVATE+13)
#define FSTSETCONF      (SIOCDEVPRIVATE+14)


struct fstioc_write {
        unsigned int  size;
        unsigned int  offset;
        unsigned char data[0];
};



struct fstioc_info {
        unsigned int   valid;           
        unsigned int   nports;          
        unsigned int   type;            
        unsigned int   state;           
        unsigned int   index;           
        unsigned int   smcFirmwareVersion;
        unsigned long  kernelVersion;   
        unsigned short lineInterface;   
        unsigned char  proto;           
        unsigned char  internalClock;   
        unsigned int   lineSpeed;       
        unsigned int   v24IpSts;        
        unsigned int   v24OpSts;        
        unsigned short clockStatus;     
        unsigned short cableStatus;     
        unsigned short cardMode;        
        unsigned short debug;           
        unsigned char  transparentMode; 
        unsigned char  invertClock;     
        unsigned char  startingSlot;    
        unsigned char  clockSource;     
        unsigned char  framing;         
        unsigned char  structure;       
                                        
        unsigned char  interface;       
        unsigned char  coding;          
        unsigned char  lineBuildOut;    
        unsigned char  equalizer;       
        unsigned char  loopMode;        
        unsigned char  range;           
        unsigned char  txBufferMode;    
        unsigned char  rxBufferMode;    
        unsigned char  losThreshold;    
        unsigned char  idleCode;        
        unsigned int   receiveBufferDelay; 
        unsigned int   framingErrorCount; 
        unsigned int   codeViolationCount; 
        unsigned int   crcErrorCount;   
        int            lineAttenuation; 
        unsigned short lossOfSignal;
        unsigned short receiveRemoteAlarm;
        unsigned short alarmIndicationSignal;
};

#define FSTVAL_NONE     0x00000000      
#define FSTVAL_OMODEM   0x0000001F      
#define FSTVAL_SPEED    0x00000020      
#define FSTVAL_CABLE    0x00000040      
#define FSTVAL_IMODEM   0x00000080      
#define FSTVAL_CARD     0x00000100      
#define FSTVAL_PROTO    0x00000200      
#define FSTVAL_MODE     0x00000400      
#define FSTVAL_PHASE    0x00000800      
#define FSTVAL_TE1      0x00001000      
#define FSTVAL_DEBUG    0x80000000      
#define FSTVAL_ALL      0x00001FFF      

#define FST_TYPE_NONE   0               
#define FST_TYPE_T2P    1               
#define FST_TYPE_T4P    2               
#define FST_TYPE_T1U    3               
#define FST_TYPE_T2U    4               
#define FST_TYPE_T4U    5               
#define FST_TYPE_TE1    6               

#define FST_FAMILY_TXP  0               
#define FST_FAMILY_TXU  1               

#define FST_UNINIT      0               
#define FST_RESET       1               
#define FST_DOWNLOAD    2               
#define FST_STARTING    3               
#define FST_RUNNING     4               
#define FST_BADVERSION  5               
#define FST_HALTED      6               
#define FST_IFAILED     7               
#define V24             1
#define X21             2
#define V35             3
#define X21D            4
#define T1              5
#define E1              6
#define J1              7

#define FST_RAW         4               
#define FST_GEN_HDLC    5               

#define INTCLK          1
#define EXTCLK          0

#define IPSTS_CTS       0x00000001      
#define IPSTS_INDICATE  IPSTS_CTS
#define IPSTS_DSR       0x00000002      
#define IPSTS_DCD       0x00000004      
#define IPSTS_RI        0x00000008      
#define IPSTS_TMI       0x00000010      

#define OPSTS_RTS       0x00000001      
#define OPSTS_CONTROL   OPSTS_RTS
#define OPSTS_DTR       0x00000002      
#define OPSTS_DSRS      0x00000004      
#define OPSTS_SS        0x00000008      
#define OPSTS_LL        0x00000010      

#define CARD_MODE_IDENTIFY      0x0001


#define CLOCKING_SLAVE       0
#define CLOCKING_MASTER      1

#define FRAMING_E1           0
#define FRAMING_J1           1
#define FRAMING_T1           2

#define STRUCTURE_UNFRAMED   0
#define STRUCTURE_E1_DOUBLE  1
#define STRUCTURE_E1_CRC4    2
#define STRUCTURE_E1_CRC4M   3
#define STRUCTURE_T1_4       4
#define STRUCTURE_T1_12      5
#define STRUCTURE_T1_24      6
#define STRUCTURE_T1_72      7

#define INTERFACE_RJ48C      0
#define INTERFACE_BNC        1


#define CODING_HDB3          0
#define CODING_NRZ           1
#define CODING_CMI           2
#define CODING_CMI_HDB3      3
#define CODING_CMI_B8ZS      4
#define CODING_AMI           5
#define CODING_AMI_ZCS       6
#define CODING_B8ZS          7

#define LBO_0dB              0
#define LBO_7dB5             1
#define LBO_15dB             2
#define LBO_22dB5            3

#define RANGE_0_133_FT       0
#define RANGE_0_40_M         RANGE_0_133_FT
#define RANGE_133_266_FT     1
#define RANGE_40_81_M        RANGE_133_266_FT
#define RANGE_266_399_FT     2
#define RANGE_81_122_M       RANGE_266_399_FT
#define RANGE_399_533_FT     3
#define RANGE_122_162_M       RANGE_399_533_FT
#define RANGE_533_655_FT     4
#define RANGE_162_200_M      RANGE_533_655_FT
#define EQUALIZER_SHORT      0
#define EQUALIZER_LONG       1

#define LOOP_NONE            0
#define LOOP_LOCAL           1
#define LOOP_PAYLOAD_EXC_TS0 2
#define LOOP_PAYLOAD_INC_TS0 3
#define LOOP_REMOTE          4

#define BUFFER_2_FRAME       0
#define BUFFER_1_FRAME       1
#define BUFFER_96_BIT        2
#define BUFFER_NONE          3

#define FST_DEBUG       0x0000
#if FST_DEBUG

extern int fst_debug_mask;              
#define DBG_INIT        0x0002          
#define DBG_OPEN        0x0004          
#define DBG_PCI         0x0008          
#define DBG_IOCTL       0x0010          
#define DBG_INTR        0x0020          
#define DBG_TX          0x0040          
#define DBG_RX          0x0080          
#define DBG_CMD         0x0100          

#define DBG_ASS         0xFFFF          
#endif  

