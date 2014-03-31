
#ifndef __ETHER_H
#define __ETHER_H

#include "quicc_simple.h"

#define T_R     0x8000          
#define E_T_PAD 0x4000          
#define T_W     0x2000          
#define T_I     0x1000          
#define T_L     0x0800          
#define T_TC    0x0400          

#define T_DEF   0x0200          
#define T_HB    0x0100          
#define T_LC    0x0080          
#define T_RL    0x0040          
#define T_RC    0x003c          
#define T_UN    0x0002          
#define T_CSL   0x0001          
#define T_ERROR (T_HB | T_LC | T_RL | T_UN | T_CSL)

#define R_E     0x8000          
#define R_W     0x2000          
#define R_I     0x1000          
#define R_L     0x0800          
#define R_F     0x0400          
#define R_M     0x0100          

#define R_LG    0x0020          
#define R_NO    0x0010          
#define R_SH    0x0008          
#define R_CR    0x0004          
#define R_OV    0x0002          
#define R_CL    0x0001          
#define ETHER_R_ERROR (R_LG | R_NO | R_SH | R_CR | R_OV | R_CL)


#define ETHERNET_GRA    0x0080  
#define ETHERNET_TXE    0x0010  
#define ETHERNET_RXF    0x0008  
#define ETHERNET_BSY    0x0004  
#define ETHERNET_TXB    0x0002  
#define ETHERNET_RXB    0x0001  

#define ETHER_HBC       0x8000    
#define ETHER_FC        0x4000    
#define ETHER_RSH       0x2000    
#define ETHER_IAM       0x1000    
#define ETHER_CRC_32    (0x2<<10) 
#define ETHER_PRO       0x0200    
#define ETHER_BRO       0x0100    
#define ETHER_SBT       0x0080    
#define ETHER_LPB       0x0040    
#define ETHER_SIP       0x0020    
#define ETHER_LCW       0x0010    
#define ETHER_NIB_13    (0x0<<1)  
#define ETHER_NIB_14    (0x1<<1)  
#define ETHER_NIB_15    (0x2<<1)  
#define ETHER_NIB_16    (0x3<<1)  
#define ETHER_NIB_21    (0x4<<1)  
#define ETHER_NIB_22    (0x5<<1)  
#define ETHER_NIB_23    (0x6<<1)  
#define ETHER_NIB_24    (0x7<<1)  

#define CRC_WORD 4                         
#define C_PRES   0xffffffff 
#define C_MASK   0xdebb20e3        
#define CRCEC    0x00000000
#define ALEC     0x00000000
#define DISFC    0x00000000
#define PADS     0x00000000
#define RET_LIM  0x000f     
#define ETH_MFLR 0x05ee     
#define MINFLR   0x0040     
#define MAXD1    0x05ee     
#define MAXD2    0x05ee
#define GADDR1   0x00000000   
#define GADDR2   0x00000000
#define GADDR3   0x00000000    
#define GADDR4   0x00000000    
#define P_PER    0x00000000               
#define IADDR1   0x00000000        
#define IADDR2   0x00000000
#define IADDR3   0x00000000    
#define IADDR4   0x00000000            
#define TADDR_H  0x00000000               
#define TADDR_M  0x00000000            
#define TADDR_L  0x00000000            

#define RFCR    0x18 
#define TFCR    0x18 
#define E_MRBLR 1518 

typedef union {
        unsigned char b[6];
        struct {
            unsigned short high;
            unsigned short middl;
            unsigned short low;
        } w;
} ETHER_ADDR;

typedef struct {
    int        max_frame_length;
    int        promisc_mode;
    int        reject_broadcast;
    ETHER_ADDR phys_adr;
} ETHER_SPECIFIC;

typedef struct {
    ETHER_ADDR     dst_addr;
    ETHER_ADDR     src_addr;
    unsigned short type_or_len;
    unsigned char  data[1];
} ETHER_FRAME;

#define MAX_DATALEN 1500
typedef struct {
    ETHER_ADDR     dst_addr;
    ETHER_ADDR     src_addr;
    unsigned short type_or_len;
    unsigned char  data[MAX_DATALEN];
    unsigned char  fcs[CRC_WORD];
} ETHER_MAX_FRAME;


void        ether_interrupt(int scc_num);

void ethernet_init(int                       scc_number,
                   alloc_routine             *alloc_buffer,
                   free_routine              *free_buffer,
                   store_rx_buffer_routine   *store_rx_buffer,
                   handle_tx_error_routine   *handle_tx_error,
                   handle_rx_error_routine   *handle_rx_error,
                   handle_lost_error_routine *handle_lost_error,
                   ETHER_SPECIFIC            *ether_spec);
int  ethernet_tx(int scc_number, void *buf, int length);

#endif

