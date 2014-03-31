
/*
 *
 Copyright (c) Eicon Networks, 2002.
 *
 This source file is supplied for the use with
 Eicon Networks range of DIVA Server Adapters.
 *
 Eicon File Revision :    2.1
 *
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.
 *
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
 implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 See the GNU General Public License for more details.
 *
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#define BRI_MEMORY_BASE                 0x1f700000
#define BRI_MEMORY_SIZE                 0x00100000  
#define BRI_SHARED_RAM_SIZE             0x00010000  
#define BRI_RAY_TAYLOR_DSP_CODE_SIZE    0x00020000  
#define BRI_ORG_MAX_DSP_CODE_SIZE       0x00050000  
#define BRI_V90D_MAX_DSP_CODE_SIZE      0x00060000  
#define BRI_CACHED_ADDR(x)              (((x) & 0x1fffffffL) | 0x80000000L)
#define BRI_UNCACHED_ADDR(x)            (((x) & 0x1fffffffL) | 0xa0000000L)
#define ADDR  4
#define ADDRH 6
#define DATA  0
#define RESET 7
#define DEFAULT_ADDRESS 0x240
#define DEFAULT_IRQ     3
#define M_PCI_ADDR   0x04  
#define M_PCI_ADDRH  0x0c  
#define M_PCI_DATA   0x00  
#define M_PCI_RESET  0x10  
#define MP_IRQ_RESET                    0xc18       
#define MP_IRQ_RESET_VAL                0xfe        
#define MP_MEMORY_SIZE                  0x00400000  
#define MP2_MEMORY_SIZE                 0x00800000  
#define MP_SHARED_RAM_OFFSET            0x00001000  
#define MP_SHARED_RAM_SIZE              0x00010000  
#define MP_PROTOCOL_OFFSET              (MP_SHARED_RAM_OFFSET + MP_SHARED_RAM_SIZE)
#define MP_RAY_TAYLOR_DSP_CODE_SIZE     0x00040000  
#define MP_ORG_MAX_DSP_CODE_SIZE        0x00060000  
#define MP_V90D_MAX_DSP_CODE_SIZE       0x00070000  
#define MP_VOIP_MAX_DSP_CODE_SIZE       0x00090000  
#define MP_CACHED_ADDR(x)               (((x) & 0x1fffffffL) | 0x80000000L)
#define MP_UNCACHED_ADDR(x)             (((x) & 0x1fffffffL) | 0xa0000000L)
#define MP_RESET         0x20        
#define _MP_S2M_RESET    0x10        
#define _MP_LED2         0x08        
#define _MP_LED1         0x04        
#define _MP_DSP_RESET    0x02        
#define _MP_RISC_RESET   0x81        
typedef struct mp_xcptcontext_s MP_XCPTC;
struct mp_xcptcontext_s {
	dword       sr;
	dword       cr;
	dword       epc;
	dword       vaddr;
	dword       regs[32];
	dword       mdlo;
	dword       mdhi;
	dword       reseverd;
	dword       xclass;
};
struct mp_load {
	dword     volatile cmd;
	dword     volatile addr;
	dword     volatile len;
	dword     volatile err;
	dword     volatile live;
	dword     volatile res1[0x1b];
	dword     volatile TrapId;    
	dword     volatile res2[0x03];
	MP_XCPTC  volatile xcpt;      
	dword     volatile rest[((0x1020 >> 2) - 6) - 0x1b - 1 - 0x03 - (sizeof(MP_XCPTC) >> 2)];
	dword     volatile signature;
	dword data[60000]; 
};
#define MQ_BOARD_REG_OFFSET             0x800000    
#define MQ_BREG_RISC                    0x1200      
#define MQ_RISC_COLD_RESET_MASK         0x0001      
#define MQ_RISC_WARM_RESET_MASK         0x0002      
#define MQ_BREG_IRQ_TEST                0x0608      
#define MQ_IRQ_REQ_ON                   0x1
#define MQ_IRQ_REQ_OFF                  0x0
#define MQ_BOARD_DSP_OFFSET             0xa00000    
#define MQ_DSP1_ADDR_OFFSET             0x0008      
#define MQ_DSP2_ADDR_OFFSET             0x0208      
#define MQ_DSP1_DATA_OFFSET             0x0000      
#define MQ_DSP2_DATA_OFFSET             0x0200      
#define MQ_DSP_JUNK_OFFSET              0x0400      
#define MQ_ISAC_DSP_RESET               0x0028      
#define MQ_BOARD_ISAC_DSP_RESET         0x800028    
#define MQ_INSTANCE_COUNT               4           
#define MQ_MEMORY_SIZE                  0x00400000  
#define MQ_CTRL_SIZE                    0x00002000  
#define MQ_SHARED_RAM_SIZE              0x00010000  
#define MQ_ORG_MAX_DSP_CODE_SIZE        0x00050000  
#define MQ_V90D_MAX_DSP_CODE_SIZE       0x00060000  
#define MQ_VOIP_MAX_DSP_CODE_SIZE       0x00028000  
#define MQ_CACHED_ADDR(x)               (((x) & 0x1fffffffL) | 0x80000000L)
#define MQ_UNCACHED_ADDR(x)             (((x) & 0x1fffffffL) | 0xa0000000L)
#define MQ2_BREG_RISC                   0x0200      
#define MQ2_BREG_IRQ_TEST               0x0400      
#define MQ2_BOARD_DSP_OFFSET            0x800000    
#define MQ2_DSP1_DATA_OFFSET            0x1800      
#define MQ2_DSP1_ADDR_OFFSET            0x1808      
#define MQ2_DSP2_DATA_OFFSET            0x1810      
#define MQ2_DSP2_ADDR_OFFSET            0x1818      
#define MQ2_DSP_JUNK_OFFSET             0x1000      
#define MQ2_ISAC_DSP_RESET              0x0000      
#define MQ2_BOARD_ISAC_DSP_RESET        0x800000    
#define MQ2_IPACX_CONFIG                0x0300      
#define MQ2_BOARD_IPACX_CONFIG          0x800300    
#define MQ2_MEMORY_SIZE                 0x01000000  
#define MQ2_CTRL_SIZE                   0x00008000  
#define BRI2_MEMORY_SIZE                0x00800000  
#define BRI2_PROTOCOL_MEMORY_SIZE       (MQ2_MEMORY_SIZE >> 2) 
#define BRI2_CTRL_SIZE                  0x00008000  
#define M_INSTANCE_COUNT                1           
#define ID_REG        0x0000      
#define RAS0_BASEREG  0x0010      
#define RAS2_BASEREG  0x0014
#define CS_BASEREG    0x0018
#define BOOT_BASEREG  0x001c
#define GTREGS_BASEREG 0x0024   
				
#define LOW_RAS0_DREG 0x0400    
#define HI_RAS0_DREG  0x0404    
#define LOW_RAS1_DREG 0x0408    
#define HI_RAS1_DREG  0x040c    
#define LOW_RAS2_DREG 0x0410    
#define HI_RAS2_DREG  0x0414    
#define LOW_RAS3_DREG 0x0418    
#define HI_RAS3_DREG  0x041c    
#define LOW_CS0_DREG  0x0420 
#define HI_CS0_DREG   0x0424 
#define LOW_CS1_DREG  0x0428 
#define HI_CS1_DREG   0x042c 
#define LOW_CS2_DREG  0x0430 
#define HI_CS2_DREG   0x0434 
#define LOW_CS3_DREG  0x0438 
#define HI_CS3_DREG   0x043c 
#define LOW_BOOTCS_DREG 0x0440 
#define HI_BOOTCS_DREG 0x0444 
#define LO_RAS10_GREG 0x0008    
#define HI_RAS10_GREG 0x0010    
#define LO_RAS32_GREG 0x0018    
#define HI_RAS32_GREG 0x0020    
#define LO_CS20_GREG  0x0028 
#define HI_CS20_GREG  0x0030 
#define LO_CS3B_GREG  0x0038 
#define HI_CS3B_GREG  0x0040 
#define PCI_TIMEOUT_RET 0x0c04 
#define RAS10_BANKSIZE 0x0c08 
#define RAS32_BANKSIZE 0x0c0c 
#define CS20_BANKSIZE 0x0c10 
#define CS3B_BANKSIZE 0x0c14 
#define DRAM_SIZE     0x0001      
#define PROM_SIZE     0x08000     
#define OFFS_DIVA_INIT_TASK_COUNT 0x68
#define OFFS_DSP_CODE_BASE_ADDR   0x6c
#define OFFS_XLOG_BUF_ADDR        0x70
#define OFFS_XLOG_COUNT_ADDR      0x74
#define OFFS_XLOG_OUT_ADDR        0x78
#define OFFS_PROTOCOL_END_ADDR    0x7c
#define OFFS_PROTOCOL_ID_STRING   0x80
