/*
** amigaints.h -- Amiga Linux interrupt handling structs and prototypes
**
** Copyright 1992 by Greg Harp
**
** This file is subject to the terms and conditions of the GNU General Public
** License.  See the file COPYING in the main directory of this archive
** for more details.
**
** Created 10/2/92 by Greg Harp
*/

#ifndef _ASMm68k_AMIGAINTS_H_
#define _ASMm68k_AMIGAINTS_H_

#include <asm/irq.h>


#define AUTO_IRQS           (8)
#define AMI_STD_IRQS        (14)
#define CIA_IRQS            (5)
#define AMI_IRQS            (32) 

#define IRQ_AMIGA_TBE		(IRQ_USER+0)
#define IRQ_AMIGA_RBF		(IRQ_USER+11)

#define IRQ_AMIGA_DSKBLK	(IRQ_USER+1)
#define IRQ_AMIGA_DSKSYN	(IRQ_USER+12)

#define IRQ_AMIGA_SOFT		(IRQ_USER+2)

#define IRQ_AMIGA_PORTS		IRQ_AUTO_2
#define IRQ_AMIGA_EXTER		IRQ_AUTO_6

#define IRQ_AMIGA_COPPER	(IRQ_USER+4)

#define IRQ_AMIGA_VERTB		(IRQ_USER+5)

#define IRQ_AMIGA_BLIT		(IRQ_USER+6)

#define IRQ_AMIGA_AUD0		(IRQ_USER+7)
#define IRQ_AMIGA_AUD1		(IRQ_USER+8)
#define IRQ_AMIGA_AUD2		(IRQ_USER+9)
#define IRQ_AMIGA_AUD3		(IRQ_USER+10)

#define IRQ_AMIGA_CIAA		(IRQ_USER+14)
#define IRQ_AMIGA_CIAA_TA	(IRQ_USER+14)
#define IRQ_AMIGA_CIAA_TB	(IRQ_USER+15)
#define IRQ_AMIGA_CIAA_ALRM	(IRQ_USER+16)
#define IRQ_AMIGA_CIAA_SP	(IRQ_USER+17)
#define IRQ_AMIGA_CIAA_FLG	(IRQ_USER+18)
#define IRQ_AMIGA_CIAB		(IRQ_USER+19)
#define IRQ_AMIGA_CIAB_TA	(IRQ_USER+19)
#define IRQ_AMIGA_CIAB_TB	(IRQ_USER+20)
#define IRQ_AMIGA_CIAB_ALRM	(IRQ_USER+21)
#define IRQ_AMIGA_CIAB_SP	(IRQ_USER+22)
#define IRQ_AMIGA_CIAB_FLG	(IRQ_USER+23)


#define IF_SETCLR   0x8000      
#define IF_INTEN    0x4000	
#define IF_EXTER    0x2000	
#define IF_DSKSYN   0x1000	
#define IF_RBF	    0x0800	
#define IF_AUD3     0x0400	
#define IF_AUD2     0x0200	
#define IF_AUD1     0x0100	
#define IF_AUD0     0x0080	
#define IF_BLIT     0x0040	
#define IF_VERTB    0x0020	
#define IF_COPER    0x0010	
#define IF_PORTS    0x0008	
#define IF_SOFT     0x0004	
#define IF_DSKBLK   0x0002	
#define IF_TBE	    0x0001	


#define CIA_ICR_TA	0x01
#define CIA_ICR_TB	0x02
#define CIA_ICR_ALRM	0x04
#define CIA_ICR_SP	0x08
#define CIA_ICR_FLG	0x10
#define CIA_ICR_ALL	0x1f
#define CIA_ICR_SETCLR	0x80

extern void amiga_init_IRQ(void);


extern struct ciabase ciaa_base, ciab_base;

extern void cia_init_IRQ(struct ciabase *base);
extern unsigned char cia_set_irq(struct ciabase *base, unsigned char mask);
extern unsigned char cia_able_irq(struct ciabase *base, unsigned char mask);

#endif 
