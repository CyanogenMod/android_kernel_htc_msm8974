/*
 * Copyright (C) 2001 David J. Mckay (david.mckay@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Definitions for the SH5 PCI hardware.
 */
#ifndef __PCI_SH5_H
#define __PCI_SH5_H

#define PCISH5_PID		0x350d

#define PCISH5_VID		0x1054

#define ST_TYPE0                0x00    
#define ST_TYPE1                0x01    

#define PCISH5_VCR_STATUS      0x00
#define PCISH5_VCR_VERSION     0x08

#define PCISH5_ICR_CR          0x100   
#define CR_PBAM                 (1<<12)
#define CR_PFCS                 (1<<11)
#define CR_FTO                  (1<<10)
#define CR_PFE                  (1<<9)
#define CR_TBS                  (1<<8)
#define CR_SPUE                 (1<<7)
#define CR_BMAM                 (1<<6)
#define CR_HOST                 (1<<5)
#define CR_CLKEN                (1<<4)
#define CR_SOCS                 (1<<3)
#define CR_IOCS                 (1<<2)
#define CR_RSTCTL               (1<<1)
#define CR_CFINT                (1<<0)
#define CR_LOCK_MASK            0xa5000000

#define PCISH5_ICR_INT         0x114   
#define INT_MADIM               (1<<2)

#define PCISH5_ICR_LSR0        0X104   
#define PCISH5_ICR_LSR1        0X108   
#define PCISH5_ICR_LAR0        0x10c   
#define PCISH5_ICR_LAR1        0x110   
#define PCISH5_ICR_INTM        0x118   
#define PCISH5_ICR_AIR         0x11c   
#define PCISH5_ICR_CIR         0x120   
#define PCISH5_ICR_AINT        0x130   
#define PCISH5_ICR_AINTM       0x134   
#define PCISH5_ICR_BMIR        0x138   
#define PCISH5_ICR_PAR         0x1c0   
#define PCISH5_ICR_MBR         0x1c4   
#define PCISH5_ICR_IOBR        0x1c8   
#define PCISH5_ICR_PINT        0x1cc   
#define PCISH5_ICR_PINTM       0x1d0   
#define PCISH5_ICR_MBMR        0x1d8   
#define PCISH5_ICR_IOBMR       0x1dc   
#define PCISH5_ICR_CSCR0       0x210   
#define PCISH5_ICR_CSCR1       0x214   
#define PCISH5_ICR_PDR         0x220   

#define PCISH5_ICR_CSR_VID     0x000	
#define PCISH5_ICR_CSR_DID     0x002   
#define PCISH5_ICR_CSR_CMD     0x004   
#define PCISH5_ICR_CSR_STATUS  0x006   
#define PCISH5_ICR_CSR_IBAR0   0x010   
#define PCISH5_ICR_CSR_MBAR0   0x014   
#define PCISH5_ICR_CSR_MBAR1   0x018   

#define SH5PCI_ICR_BASE (PHYS_PCI_BLOCK + 0x00040000)
#define SH5PCI_IO_BASE  (PHYS_PCI_BLOCK + 0x00800000)

extern unsigned long pcicr_virt;
#define PCISH5_ICR_REG(x)                ( pcicr_virt + (PCISH5_ICR_##x))

#define SH5PCI_WRITE(reg,val)        __raw_writel((u32)(val),PCISH5_ICR_REG(reg))
#define SH5PCI_WRITE_SHORT(reg,val)  __raw_writew((u16)(val),PCISH5_ICR_REG(reg))
#define SH5PCI_WRITE_BYTE(reg,val)   __raw_writeb((u8)(val),PCISH5_ICR_REG(reg))

#define SH5PCI_READ(reg)             __raw_readl(PCISH5_ICR_REG(reg))
#define SH5PCI_READ_SHORT(reg)       __raw_readw(PCISH5_ICR_REG(reg))
#define SH5PCI_READ_BYTE(reg)        __raw_readb(PCISH5_ICR_REG(reg))

#define SET_CONFIG_BITS(bus,devfn,where)  ((((bus) << 16) | ((devfn) << 8) | ((where) & ~3)) | 0x80000000)

#define CONFIG_CMD(bus, devfn, where)            SET_CONFIG_BITS(bus->number,devfn,where)

#define PCISH5_MEM_SIZCONV(x)		  (((x / 0x40000) - 1) << 18)
#define PCISH5_IO_SIZCONV(x)		  (((x / 0x40000) - 1) << 18)

extern struct pci_ops sh5_pci_ops;

#endif 
