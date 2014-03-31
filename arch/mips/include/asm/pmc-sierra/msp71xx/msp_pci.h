/*
 * Copyright (c) 2000-2006 PMC-Sierra INC.
 *
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General
 *     Public License as published by the Free Software
 *     Foundation; either version 2 of the License, or (at your
 *     option) any later version.
 *
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 675 Mass Ave, Cambridge, MA
 *     02139, USA.
 *
 * PMC-SIERRA INC. DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS
 * SOFTWARE.
 */

#ifndef _MSP_PCI_H_
#define _MSP_PCI_H_

#define MSP_HAS_PCI(ID)	(((u32)(ID) <= 0x4236) && ((u32)(ID) >= 0x4220))

#define MSP_PCI_OATRAN		0xB8000000UL

#define MSP_PCI_SPACE_BASE	(MSP_PCI_OATRAN + 0x1002000UL)
#define MSP_PCI_SPACE_SIZE	(0x3000000UL - 0x2000)
#define MSP_PCI_SPACE_END \
		(MSP_PCI_SPACE_BASE + MSP_PCI_SPACE_SIZE - 1)
#define MSP_PCI_IOSPACE_BASE	(MSP_PCI_OATRAN + 0x1001000UL)
#define MSP_PCI_IOSPACE_SIZE	0x1000
#define MSP_PCI_IOSPACE_END  \
		(MSP_PCI_IOSPACE_BASE + MSP_PCI_IOSPACE_SIZE - 1)

#define PCI_STAT_IRQ	20

#define QFLUSH_REG_1	0xB7F40000

typedef volatile unsigned int pcireg;
typedef void * volatile ppcireg;

struct pci_block_copy
{
    pcireg   unused1; 
    pcireg   unused2; 
    ppcireg  unused3; 
    ppcireg  unused4; 
    pcireg   unused5; 
    pcireg   unused6; 
    pcireg   unused7; 
    ppcireg  unused8; 
    ppcireg  unused9; 
    pcireg   unusedA; 
    ppcireg  unusedB; 
    ppcireg  unusedC; 
};

enum
{
    config_device_vendor,  
    config_status_command, 
    config_class_revision, 
    config_BIST_header_latency_cache, 
    config_BAR0,           
    config_BAR1,           
    config_BAR2,           
    config_not_used7,      
    config_not_used8,      
    config_not_used9,      
    config_CIS,            
    config_subsystem,      
    config_not_used12,     
    config_capabilities,   
    config_not_used14,     
    config_lat_grant_irq,  
    config_message_control,
    config_message_addr,   
    config_message_data,   
    config_VPD_addr,       
    config_VPD_data,       
    config_maxregs         
};

struct msp_pci_regs
{
    pcireg hop_unused_00; 
    pcireg hop_unused_04; 
    pcireg hop_unused_08; 
    pcireg hop_unused_0C; 
    pcireg hop_unused_10; 
    pcireg hop_unused_14; 
    pcireg hop_unused_18; 
    pcireg hop_unused_1C; 
    pcireg hop_unused_20; 
    pcireg hop_unused_24; 
    pcireg hop_unused_28; 
    pcireg hop_unused_2C; 
    pcireg hop_unused_30; 
    pcireg hop_unused_34; 
    pcireg if_control;    
    pcireg oatran;        
    pcireg reset_ctl;     
    pcireg config_addr;   
    pcireg hop_unused_48; 
    pcireg msg_signaled_int_status; 
    pcireg msg_signaled_int_mask;   
    pcireg if_status;     
    pcireg if_mask;       
    pcireg hop_unused_5C; 
    pcireg hop_unused_60; 
    pcireg hop_unused_64; 
    pcireg hop_unused_68; 
    pcireg hop_unused_6C; 
    pcireg hop_unused_70; 

    struct pci_block_copy pci_bc[2] __attribute__((aligned(64)));

    pcireg error_hdr1; 
    pcireg error_hdr2; 

    pcireg config[config_maxregs] __attribute__((aligned(256)));

};

#define BPCI_CFGADDR_BUSNUM_SHF 16
#define BPCI_CFGADDR_FUNCTNUM_SHF 8
#define BPCI_CFGADDR_REGNUM_SHF 2
#define BPCI_CFGADDR_ENABLE (1<<31)

#define BPCI_IFCONTROL_RTO (1<<20) 
#define BPCI_IFCONTROL_HCE (1<<16) 
#define BPCI_IFCONTROL_CTO_SHF 12  
#define BPCI_IFCONTROL_SE  (1<<5)  
#define BPCI_IFCONTROL_BIST (1<<4) 
#define BPCI_IFCONTROL_CAP (1<<3)  
#define BPCI_IFCONTROL_MMC_SHF 0   

#define BPCI_IFSTATUS_MGT  (1<<8)  
#define BPCI_IFSTATUS_MTT  (1<<9)  
#define BPCI_IFSTATUS_MRT  (1<<10) 
#define BPCI_IFSTATUS_BC0F (1<<13) 
#define BPCI_IFSTATUS_BC1F (1<<14) 
#define BPCI_IFSTATUS_PCIU (1<<15) 
#define BPCI_IFSTATUS_BSIZ (1<<16) 
#define BPCI_IFSTATUS_BADD (1<<17) 
#define BPCI_IFSTATUS_RTO  (1<<18) 
#define BPCI_IFSTATUS_SER  (1<<19) 
#define BPCI_IFSTATUS_PER  (1<<20) 
#define BPCI_IFSTATUS_LCA  (1<<21) 
#define BPCI_IFSTATUS_MEM  (1<<22) 
#define BPCI_IFSTATUS_ARB  (1<<23) 
#define BPCI_IFSTATUS_STA  (1<<27) 
#define BPCI_IFSTATUS_TA   (1<<28) 
#define BPCI_IFSTATUS_MA   (1<<29) 
#define BPCI_IFSTATUS_PEI  (1<<30) 
#define BPCI_IFSTATUS_PET  (1<<31) 

#define BPCI_RESETCTL_PR (1<<0)    
#define BPCI_RESETCTL_RT (1<<4)    
#define BPCI_RESETCTL_CT (1<<8)    
#define BPCI_RESETCTL_PE (1<<12)   
#define BPCI_RESETCTL_HM (1<<13)   
#define BPCI_RESETCTL_RI (1<<14)   

extern struct msp_pci_regs msp_pci_regs
			__attribute__((section(".register")));
extern unsigned long msp_pci_config_space
			__attribute__((section(".register")));

#endif 
