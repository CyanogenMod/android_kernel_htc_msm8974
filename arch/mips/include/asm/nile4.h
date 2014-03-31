/*
 *  asm-mips/nile4.h -- NEC Vrc-5074 Nile 4 definitions
 *
 *  Copyright (C) 2000 Geert Uytterhoeven <geert@sonycom.com>
 *                     Sony Software Development Center Europe (SDCE), Brussels
 *
 *  This file is based on the following documentation:
 *
 *	NEC Vrc 5074 System Controller Data Sheet, June 1998
 */

#ifndef _ASM_NILE4_H
#define _ASM_NILE4_H

#define NILE4_BASE		0xbfa00000
#define NILE4_SIZE		0x00200000		



#define NILE4_SDRAM0	0x0000	
#define NILE4_SDRAM1	0x0008	
#define NILE4_DCS2	0x0010	
#define NILE4_DCS3	0x0018	
#define NILE4_DCS4	0x0020	
#define NILE4_DCS5	0x0028	
#define NILE4_DCS6	0x0030	
#define NILE4_DCS7	0x0038	
#define NILE4_DCS8	0x0040	
#define NILE4_PCIW0	0x0060	
#define NILE4_PCIW1	0x0068	
#define NILE4_INTCS	0x0070	
				
#define NILE4_BOOTCS	0x0078	



#define NILE4_CPUSTAT	0x0080	
#define NILE4_INTCTRL	0x0088	
#define NILE4_INTSTAT0	0x0090	
#define NILE4_INTSTAT1	0x0098	
				
#define NILE4_INTCLR	0x00A0	
#define NILE4_INTPPES	0x00A8	



#define NILE4_MEMCTRL	0x00C0	
#define NILE4_ACSTIME	0x00C8	
#define NILE4_CHKERR	0x00D0	



#define NILE4_PCICTRL	0x00E0	
#define NILE4_PCIARB	0x00E8	
#define NILE4_PCIINIT0	0x00F0	
#define NILE4_PCIINIT1	0x00F8	
#define NILE4_PCIERR	0x00B8	



#define NILE4_LCNFG	0x0100	
#define NILE4_LCST2	0x0110	
#define NILE4_LCST3	0x0118	
#define NILE4_LCST4	0x0120	
#define NILE4_LCST5	0x0128	
#define NILE4_LCST6	0x0130	
#define NILE4_LCST7	0x0138	
#define NILE4_LCST8	0x0140	
#define NILE4_DCSFN	0x0150	
				
#define NILE4_DCSIO	0x0158	
#define NILE4_BCST	0x0178	



#define NILE4_DMACTRL0	0x0180	
#define NILE4_DMASRCA0	0x0188	
#define NILE4_DMADESA0	0x0190	
#define NILE4_DMACTRL1	0x0198	
#define NILE4_DMASRCA1	0x01A0	
#define NILE4_DMADESA1	0x01A8	



#define NILE4_T0CTRL	0x01C0	
#define NILE4_T0CNTR	0x01C8	
#define NILE4_T1CTRL	0x01D0	
#define NILE4_T1CNTR	0x01D8	
#define NILE4_T2CTRL	0x01E0	
#define NILE4_T2CNTR	0x01E8	
#define NILE4_T3CTRL	0x01F0	
#define NILE4_T3CNTR	0x01F8	



#define NILE4_PCI_BASE	0x0200

#define NILE4_VID	0x0200	
#define NILE4_DID	0x0202	
#define NILE4_PCICMD	0x0204	
#define NILE4_PCISTS	0x0206	
#define NILE4_REVID	0x0208	
#define NILE4_CLASS	0x0209	
#define NILE4_CLSIZ	0x020C	
#define NILE4_MLTIM	0x020D	
#define NILE4_HTYPE	0x020E	
#define NILE4_BIST	0x020F	
#define NILE4_BARC	0x0210	
#define NILE4_BAR0	0x0218	
#define NILE4_BAR1	0x0220	
#define NILE4_CIS	0x0228	
				
#define NILE4_SSVID	0x022C	
#define NILE4_SSID	0x022E	
#define NILE4_ROM	0x0230	
				
#define NILE4_INTLIN	0x023C	
#define NILE4_INTPIN	0x023D	
#define NILE4_MINGNT	0x023E	
#define NILE4_MAXLAT	0x023F	
#define NILE4_BAR2	0x0240	
#define NILE4_BAR3	0x0248	
#define NILE4_BAR4	0x0250	
#define NILE4_BAR5	0x0258	
#define NILE4_BAR6	0x0260	
#define NILE4_BAR7	0x0268	
#define NILE4_BAR8	0x0270	
#define NILE4_BARB	0x0278	



#define NILE4_UART_BASE	0x0300

#define NILE4_UARTRBR	0x0300	
#define NILE4_UARTTHR	0x0300	
#define NILE4_UARTIER	0x0308	
#define NILE4_UARTDLL	0x0300	
#define NILE4_UARTDLM	0x0308	
#define NILE4_UARTIIR	0x0310	
#define NILE4_UARTFCR	0x0310	
#define NILE4_UARTLCR	0x0318	
#define NILE4_UARTMCR	0x0320	
#define NILE4_UARTLSR	0x0328	
#define NILE4_UARTMSR	0x0330	
#define NILE4_UARTSCR	0x0338	

#define NILE4_UART_BASE_BAUD	520833	



#define NILE4_INT_CPCE	0	
#define NILE4_INT_CNTD	1	
#define NILE4_INT_MCE	2	
#define NILE4_INT_DMA	3	
#define NILE4_INT_UART	4	
#define NILE4_INT_WDOG	5	
#define NILE4_INT_GPT	6	
#define NILE4_INT_LBRTD	7	
#define NILE4_INT_INTA	8	
#define NILE4_INT_INTB	9	
#define NILE4_INT_INTC	10	
#define NILE4_INT_INTD	11	
#define NILE4_INT_INTE	12	
#define NILE4_INT_RESV	13	
#define NILE4_INT_PCIS	14	
#define NILE4_INT_PCIE	15	



static inline void nile4_sync(void)
{
    volatile u32 *p = (volatile u32 *)0xbfc00000;
    (void)(*p);
}

static inline void nile4_out32(u32 offset, u32 val)
{
    *(volatile u32 *)(NILE4_BASE+offset) = val;
    nile4_sync();
}

static inline u32 nile4_in32(u32 offset)
{
    u32 val = *(volatile u32 *)(NILE4_BASE+offset);
    nile4_sync();
    return val;
}

static inline void nile4_out16(u32 offset, u16 val)
{
    *(volatile u16 *)(NILE4_BASE+offset) = val;
    nile4_sync();
}

static inline u16 nile4_in16(u32 offset)
{
    u16 val = *(volatile u16 *)(NILE4_BASE+offset);
    nile4_sync();
    return val;
}

static inline void nile4_out8(u32 offset, u8 val)
{
    *(volatile u8 *)(NILE4_BASE+offset) = val;
    nile4_sync();
}

static inline u8 nile4_in8(u32 offset)
{
    u8 val = *(volatile u8 *)(NILE4_BASE+offset);
    nile4_sync();
    return val;
}



extern void nile4_set_pdar(u32 pdar, u32 phys, u32 size, int width,
			   int on_memory_bus, int visible);



#define NILE4_PCICMD_IACK	0	
#define NILE4_PCICMD_IO		1	
#define NILE4_PCICMD_MEM	3	
#define NILE4_PCICMD_CFG	5	



#define NILE4_PCI_IO_BASE	0xa6000000
#define NILE4_PCI_MEM_BASE	0xa8000000
#define NILE4_PCI_CFG_BASE	NILE4_PCI_MEM_BASE
#define NILE4_PCI_IACK_BASE	NILE4_PCI_IO_BASE


extern void nile4_set_pmr(u32 pmr, u32 type, u32 addr);



#define NUM_I8259_INTERRUPTS	16
#define NUM_NILE4_INTERRUPTS	16

#define IRQ_I8259_CASCADE	NILE4_INT_INTE
#define is_i8259_irq(irq)	((irq) < NUM_I8259_INTERRUPTS)
#define nile4_to_irq(n)		((n)+NUM_I8259_INTERRUPTS)
#define irq_to_nile4(n)		((n)-NUM_I8259_INTERRUPTS)

extern void nile4_map_irq(int nile4_irq, int cpu_irq);
extern void nile4_map_irq_all(int cpu_irq);
extern void nile4_enable_irq(unsigned int nile4_irq);
extern void nile4_disable_irq(unsigned int nile4_irq);
extern void nile4_disable_irq_all(void);
extern u16 nile4_get_irq_stat(int cpu_irq);
extern void nile4_enable_irq_output(int cpu_irq);
extern void nile4_disable_irq_output(int cpu_irq);
extern void nile4_set_pci_irq_polarity(int pci_irq, int high);
extern void nile4_set_pci_irq_level_or_edge(int pci_irq, int level);
extern void nile4_clear_irq(int nile4_irq);
extern void nile4_clear_irq_mask(u32 mask);
extern u8 nile4_i8259_iack(void);
extern void nile4_dump_irq_status(void);	

#endif
