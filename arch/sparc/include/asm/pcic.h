/*
 * pcic.h: JavaEngine 1 specific PCI definitions.
 *
 * Copyright (C) 1998 V. Roganov and G. Raiko
 */

#ifndef __SPARC_PCIC_H
#define __SPARC_PCIC_H

#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <linux/smp.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <asm/pbm.h>

struct linux_pcic {
        void __iomem            *pcic_regs;
        unsigned long           pcic_io;
        void __iomem            *pcic_config_space_addr;
        void __iomem            *pcic_config_space_data;
	struct resource		pcic_res_regs;
	struct resource		pcic_res_io;
	struct resource		pcic_res_cfg_addr;
	struct resource		pcic_res_cfg_data;
        struct linux_pbm_info   pbm;
	struct pcic_ca2irq	*pcic_imap;
	int			pcic_imdim;
};

#ifdef CONFIG_PCIC_PCI
extern int pcic_present(void);
extern int pcic_probe(void);
extern void pci_time_init(void);
extern void sun4m_pci_init_IRQ(void);
#else
static inline int pcic_present(void) { return 0; }
static inline int pcic_probe(void) { return 0; }
static inline void pci_time_init(void) {}
static inline void sun4m_pci_init_IRQ(void) {}
#endif
#endif

#define PCI_SPACE_SIZE                  0x1000000       

#define PCI_DIAGNOSTIC_0                0x40    
#define PCI_SIZE_0                      0x44    
#define PCI_SIZE_1                      0x48    
#define PCI_SIZE_2                      0x4c    
#define PCI_SIZE_3                      0x50    
#define PCI_SIZE_4                      0x54    
#define PCI_SIZE_5                      0x58    
#define PCI_PIO_CONTROL                 0x60    
#define PCI_DVMA_CONTROL                0x62    
#define  PCI_DVMA_CONTROL_INACTIVITY_REQ        (1<<0)
#define  PCI_DVMA_CONTROL_IOTLB_ENABLE          (1<<0)
#define  PCI_DVMA_CONTROL_IOTLB_DISABLE         0
#define  PCI_DVMA_CONTROL_INACTIVITY_ACK        (1<<4)
#define PCI_INTERRUPT_CONTROL           0x63    
#define PCI_CPU_INTERRUPT_PENDING       0x64    
#define PCI_DIAGNOSTIC_1                0x68    
#define PCI_SOFTWARE_INT_CLEAR          0x6a    
#define PCI_SOFTWARE_INT_SET            0x6e    
#define PCI_SYS_INT_PENDING             0x70    
#define  PCI_SYS_INT_PENDING_PIO		0x40000000
#define  PCI_SYS_INT_PENDING_DMA		0x20000000
#define  PCI_SYS_INT_PENDING_PCI		0x10000000
#define  PCI_SYS_INT_PENDING_APSR		0x08000000
#define PCI_SYS_INT_TARGET_MASK         0x74    
#define PCI_SYS_INT_TARGET_MASK_CLEAR   0x78    
#define PCI_SYS_INT_TARGET_MASK_SET     0x7c    
#define PCI_SYS_INT_PENDING_CLEAR       0x83    
#define  PCI_SYS_INT_PENDING_CLEAR_ALL		0x80
#define  PCI_SYS_INT_PENDING_CLEAR_PIO		0x40
#define  PCI_SYS_INT_PENDING_CLEAR_DMA		0x20
#define  PCI_SYS_INT_PENDING_CLEAR_PCI		0x10
#define PCI_IOTLB_CONTROL               0x84    
#define PCI_INT_SELECT_LO               0x88    
#define PCI_ARBITRATION_SELECT          0x8a    
#define PCI_INT_SELECT_HI               0x8c    
#define PCI_HW_INT_OUTPUT               0x8e    
#define PCI_IOTLB_RAM_INPUT             0x90    
#define PCI_IOTLB_CAM_INPUT             0x94    
#define PCI_IOTLB_RAM_OUTPUT            0x98    
#define PCI_IOTLB_CAM_OUTPUT            0x9c    
#define PCI_SMBAR0                      0xa0    
#define PCI_MSIZE0                      0xa1    
#define PCI_PMBAR0                      0xa2    
#define PCI_SMBAR1                      0xa4    
#define PCI_MSIZE1                      0xa5    
#define PCI_PMBAR1                      0xa6    
#define PCI_SIBAR                       0xa8    
#define   PCI_SIBAR_ADDRESS_MASK        0xf
#define PCI_ISIZE                       0xa9    
#define   PCI_ISIZE_16M                 0xf
#define   PCI_ISIZE_32M                 0xe
#define   PCI_ISIZE_64M                 0xc
#define   PCI_ISIZE_128M                0x8
#define   PCI_ISIZE_256M                0x0
#define PCI_PIBAR                       0xaa    
#define PCI_CPU_COUNTER_LIMIT_HI        0xac    
#define PCI_CPU_COUNTER_LIMIT_LO        0xb0    
#define PCI_CPU_COUNTER_LIMIT           0xb4    
#define PCI_SYS_LIMIT                   0xb8    
#define PCI_SYS_COUNTER                 0xbc    
#define   PCI_SYS_COUNTER_OVERFLOW      (1<<31) 
#define PCI_SYS_LIMIT_PSEUDO            0xc0    
#define PCI_USER_TIMER_CONTROL          0xc4    
#define PCI_USER_TIMER_CONFIG           0xc5    
#define PCI_COUNTER_IRQ                 0xc6    
#define  PCI_COUNTER_IRQ_SET(sys_irq, cpu_irq)  ((((sys_irq) & 0xf) << 4) | \
                                                  ((cpu_irq) & 0xf))
#define  PCI_COUNTER_IRQ_SYS(v)                 (((v) >> 4) & 0xf)
#define  PCI_COUNTER_IRQ_CPU(v)                 ((v) & 0xf)
#define PCI_PIO_ERROR_COMMAND           0xc7    
#define PCI_PIO_ERROR_ADDRESS           0xc8    
#define PCI_IOTLB_ERROR_ADDRESS         0xcc    
#define PCI_SYS_STATUS                  0xd0    
#define   PCI_SYS_STATUS_RESET_ENABLE           (1<<0)
#define   PCI_SYS_STATUS_RESET                  (1<<1)
#define   PCI_SYS_STATUS_WATCHDOG_RESET         (1<<4)
#define   PCI_SYS_STATUS_PCI_RESET              (1<<5)
#define   PCI_SYS_STATUS_PCI_RESET_ENABLE       (1<<6)
#define   PCI_SYS_STATUS_PCI_SATTELITE_MODE     (1<<7)

#endif 
