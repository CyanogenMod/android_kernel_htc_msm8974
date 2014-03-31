
#ifndef _ASM_ARCH_IRQ_H
#define _ASM_ARCH_IRQ_H

#include <arch/sv_addr_ag.h>

#define NR_IRQS 32

#define FIRST_IRQ 0

#define SOME_IRQ_NBR        IO_BITNR(R_VECT_MASK_RD, some)   
#define NMI_IRQ_NBR         IO_BITNR(R_VECT_MASK_RD, nmi)    
#define TIMER0_IRQ_NBR      IO_BITNR(R_VECT_MASK_RD, timer0) 
#define TIMER1_IRQ_NBR      IO_BITNR(R_VECT_MASK_RD, timer1) 
#define NETWORK_STATUS_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, network) 

#define SERIAL_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, serial) 
#define PA_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, pa) 
#define EXTDMA0_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, ext_dma0)
#define EXTDMA1_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, ext_dma1)

#define DMA0_TX_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, dma0)
#define DMA1_RX_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, dma1)
#define NETWORK_DMA_TX_IRQ_NBR DMA0_TX_IRQ_NBR
#define NETWORK_DMA_RX_IRQ_NBR DMA1_RX_IRQ_NBR

#define DMA2_TX_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, dma2)
#define DMA3_RX_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, dma3)
#define SER2_DMA_TX_IRQ_NBR DMA2_TX_IRQ_NBR
#define SER2_DMA_RX_IRQ_NBR DMA3_RX_IRQ_NBR

#define DMA4_TX_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, dma4)
#define DMA5_RX_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, dma5)
#define SER3_DMA_TX_IRQ_NBR DMA4_TX_IRQ_NBR
#define SER3_DMA_RX_IRQ_NBR DMA5_RX_IRQ_NBR

#define DMA6_TX_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, dma6)
#define DMA7_RX_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, dma7)
#define SER0_DMA_TX_IRQ_NBR DMA6_TX_IRQ_NBR
#define SER0_DMA_RX_IRQ_NBR DMA7_RX_IRQ_NBR
#define MEM2MEM_DMA_TX_IRQ_NBR DMA6_TX_IRQ_NBR
#define MEM2MEM_DMA_RX_IRQ_NBR DMA7_RX_IRQ_NBR

#define DMA8_TX_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, dma8)
#define DMA9_RX_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, dma9)
#define SER1_DMA_TX_IRQ_NBR DMA8_TX_IRQ_NBR
#define SER1_DMA_RX_IRQ_NBR DMA9_RX_IRQ_NBR
#define USB_DMA_TX_IRQ_NBR DMA8_TX_IRQ_NBR
#define USB_DMA_RX_IRQ_NBR DMA9_RX_IRQ_NBR

#define USB_HC_IRQ_NBR IO_BITNR(R_VECT_MASK_RD, usb)


typedef void (*irqvectptr)(void);

struct etrax_interrupt_vector {
	irqvectptr v[256];
};

extern struct etrax_interrupt_vector *etrax_irv;
void set_int_vector(int n, irqvectptr addr);
void set_break_vector(int n, irqvectptr addr);

#define __STR(x) #x
#define STR(x) __STR(x)
 

#define SAVE_ALL \
  "move $irp,[$sp=$sp-16]\n\t"  \
  "push $srp\n\t"        \
  "push $dccr\n\t"       \
  "push $mof\n\t"        \
  "di\n\t"             \
  "subq 14*4,$sp\n\t"    \
  "movem $r13,[$sp]\n\t"  \
  "push $r10\n\t"        \
  "clear.d [$sp=$sp-4]\n\t"  


#define BLOCK_IRQ(mask,nr) \
  "move.d " #mask ",$r0\n\t" \
  "move.d $r0,[0xb00000d8]\n\t"

#define UNBLOCK_IRQ(mask) \
  "move.d " #mask ",$r0\n\t" \
  "move.d $r0,[0xb00000dc]\n\t"

#define IRQ_NAME2(nr) nr##_interrupt(void)
#define IRQ_NAME(nr) IRQ_NAME2(IRQ##nr)
#define sIRQ_NAME(nr) IRQ_NAME2(sIRQ##nr)
#define BAD_IRQ_NAME(nr) IRQ_NAME2(bad_IRQ##nr)


#define BUILD_IRQ(nr,mask) \
void IRQ_NAME(nr); \
__asm__ ( \
          ".text\n\t" \
          "IRQ" #nr "_interrupt:\n\t" \
	  SAVE_ALL \
	  BLOCK_IRQ(mask,nr)  \
	  "moveq "#nr",$r10\n\t" \
	  "move.d $sp,$r11\n\t" \
	  "jsr do_IRQ\n\t"  \
	  UNBLOCK_IRQ(mask) \
	  "moveq 0,$r9\n\t"  \
	  "jump ret_from_intr\n\t");


#define BUILD_TIMER_IRQ(nr,mask) \
void IRQ_NAME(nr); \
__asm__ ( \
          ".text\n\t" \
          "IRQ" #nr "_interrupt:\n\t" \
	  SAVE_ALL \
	  "moveq "#nr",$r10\n\t" \
	  "move.d $sp,$r11\n\t" \
	  "jsr do_IRQ\n\t"  \
	  "moveq 0,$r9\n\t"  \
	  "jump ret_from_intr\n\t");

#endif
