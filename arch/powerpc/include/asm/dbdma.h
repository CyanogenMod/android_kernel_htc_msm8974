/*
 * Definitions for using the Apple Descriptor-Based DMA controller
 * in Power Macintosh computers.
 *
 * Copyright (C) 1996 Paul Mackerras.
 */

#ifdef __KERNEL__
#ifndef _ASM_DBDMA_H_
#define _ASM_DBDMA_H_
struct dbdma_regs {
    unsigned int control;	
    unsigned int status;	
    unsigned int cmdptr_hi;	
    unsigned int cmdptr;	
    unsigned int intr_sel;	
    unsigned int br_sel;	
    unsigned int wait_sel;	
    unsigned int xfer_mode;
    unsigned int data2ptr_hi;
    unsigned int data2ptr;
    unsigned int res1;
    unsigned int address_hi;
    unsigned int br_addr_hi;
    unsigned int res2[3];
};

#define RUN	0x8000
#define PAUSE	0x4000
#define FLUSH	0x2000
#define WAKE	0x1000
#define DEAD	0x0800
#define ACTIVE	0x0400
#define BT	0x0100
#define DEVSTAT	0x00ff

struct dbdma_cmd {
    unsigned short req_count;	
    unsigned short command;	
    unsigned int   phy_addr;	
    unsigned int   cmd_dep;	
    unsigned short res_count;	
    unsigned short xfer_status;	
};

#define OUTPUT_MORE	0	
#define OUTPUT_LAST	0x1000	
#define INPUT_MORE	0x2000	
#define INPUT_LAST	0x3000	
#define STORE_WORD	0x4000	
#define LOAD_WORD	0x5000	
#define DBDMA_NOP	0x6000	
#define DBDMA_STOP	0x7000	

#define KEY_STREAM0	0	
#define KEY_STREAM1	0x100	
#define KEY_STREAM2	0x200	
#define KEY_STREAM3	0x300	
#define KEY_REGS	0x500	
#define KEY_SYSTEM	0x600	
#define KEY_DEVICE	0x700	

#define INTR_NEVER	0	
#define INTR_IFSET	0x10	
#define INTR_IFCLR	0x20	
#define INTR_ALWAYS	0x30	

#define BR_NEVER	0	
#define BR_IFSET	0x4	
#define BR_IFCLR	0x8	
#define BR_ALWAYS	0xc	

#define WAIT_NEVER	0	
#define WAIT_IFSET	1	
#define WAIT_IFCLR	2	
#define WAIT_ALWAYS	3	

#define DBDMA_ALIGN(x)	(((unsigned long)(x) + sizeof(struct dbdma_cmd) - 1) \
			 & -sizeof(struct dbdma_cmd))

#define DBDMA_DO_STOP(regs) do {				\
	out_le32(&((regs)->control), (RUN|FLUSH)<<16);		\
	while(in_le32(&((regs)->status)) & (ACTIVE|FLUSH))	\
		; \
} while(0)

#define DBDMA_DO_RESET(regs) do {				\
	out_le32(&((regs)->control), (ACTIVE|DEAD|WAKE|FLUSH|PAUSE|RUN)<<16);\
	while(in_le32(&((regs)->status)) & (RUN)) \
		; \
} while(0)

#endif 
#endif 
