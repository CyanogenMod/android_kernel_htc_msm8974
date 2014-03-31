/*
** atariints.h -- Atari Linux interrupt handling structs and prototypes
**
** Copyright 1994 by Björn Brauel
**
** 5/2/94 Roman Hodek:
**   TT interrupt definitions added.
**
** 12/02/96: (Roman)
**   Adapted to new int handling scheme (see ataints.c); revised numbering
**
** This file is subject to the terms and conditions of the GNU General Public
** License.  See the file COPYING in the main directory of this archive
** for more details.
**
*/

#ifndef _LINUX_ATARIINTS_H_
#define _LINUX_ATARIINTS_H_

#include <asm/irq.h>
#include <asm/atarihw.h>


#define STMFP_SOURCE_BASE  8
#define TTMFP_SOURCE_BASE  24
#define SCC_SOURCE_BASE    40
#define VME_SOURCE_BASE    56
#define VME_MAX_SOURCES    16

#define NUM_ATARI_SOURCES   (VME_SOURCE_BASE+VME_MAX_SOURCES-STMFP_SOURCE_BASE)

#define IRQ_VECTOR_TO_SOURCE(v)	((v) - ((v) < 0x20 ? 0x18 : (0x40-8)))

#define IRQ_SOURCE_TO_VECTOR(i)	((i) + ((i) < 8 ? 0x18 : (0x40-8)))

#define IRQ_TYPE_SLOW     0
#define IRQ_TYPE_FAST     1
#define IRQ_TYPE_PRIO     2

#define IRQ_MFP_BUSY      (8)
#define IRQ_MFP_DCD       (9)
#define IRQ_MFP_CTS	  (10)
#define IRQ_MFP_GPU	  (11)
#define IRQ_MFP_TIMD      (12)
#define IRQ_MFP_TIMC	  (13)
#define IRQ_MFP_ACIA	  (14)
#define IRQ_MFP_FDC       (15)
#define IRQ_MFP_ACSI      IRQ_MFP_FDC
#define IRQ_MFP_FSCSI     IRQ_MFP_FDC
#define IRQ_MFP_IDE       IRQ_MFP_FDC
#define IRQ_MFP_TIMB      (16)
#define IRQ_MFP_SERERR    (17)
#define IRQ_MFP_SEREMPT   (18)
#define IRQ_MFP_RECERR    (19)
#define IRQ_MFP_RECFULL   (20)
#define IRQ_MFP_TIMA      (21)
#define IRQ_MFP_RI        (22)
#define IRQ_MFP_MMD       (23)

#define IRQ_TT_MFP_IO0       (24)
#define IRQ_TT_MFP_IO1       (25)
#define IRQ_TT_MFP_SCC	     (26)
#define IRQ_TT_MFP_RI	     (27)
#define IRQ_TT_MFP_TIMD      (28)
#define IRQ_TT_MFP_TIMC	     (29)
#define IRQ_TT_MFP_DRVRDY    (30)
#define IRQ_TT_MFP_SCSIDMA   (31)
#define IRQ_TT_MFP_TIMB      (32)
#define IRQ_TT_MFP_SERERR    (33)
#define IRQ_TT_MFP_SEREMPT   (34)
#define IRQ_TT_MFP_RECERR    (35)
#define IRQ_TT_MFP_RECFULL   (36)
#define IRQ_TT_MFP_TIMA      (37)
#define IRQ_TT_MFP_RTC       (38)
#define IRQ_TT_MFP_SCSI      (39)

#define IRQ_SCCB_TX	     (40)
#define IRQ_SCCB_STAT	     (42)
#define IRQ_SCCB_RX	     (44)
#define IRQ_SCCB_SPCOND	     (46)
#define IRQ_SCCA_TX	     (48)
#define IRQ_SCCA_STAT	     (50)
#define IRQ_SCCA_RX	     (52)
#define IRQ_SCCA_SPCOND	     (54)


#define INT_CLK   24576	    
#define INT_TICKS 246	    


#define MFP_ENABLE	0
#define MFP_PENDING	1
#define MFP_SERVICE	2
#define MFP_MASK	3


static inline int get_mfp_bit( unsigned irq, int type )

{	unsigned char	mask, *reg;

	mask = 1 << (irq & 7);
	reg = (unsigned char *)&st_mfp.int_en_a + type*4 +
		  ((irq & 8) >> 2) + (((irq-8) & 16) << 3);
	return( *reg & mask );
}

static inline void set_mfp_bit( unsigned irq, int type )

{	unsigned char	mask, *reg;

	mask = 1 << (irq & 7);
	reg = (unsigned char *)&st_mfp.int_en_a + type*4 +
		  ((irq & 8) >> 2) + (((irq-8) & 16) << 3);
	__asm__ __volatile__ ( "orb %0,%1"
			      : : "di" (mask), "m" (*reg) : "memory" );
}

static inline void clear_mfp_bit( unsigned irq, int type )

{	unsigned char	mask, *reg;

	mask = ~(1 << (irq & 7));
	reg = (unsigned char *)&st_mfp.int_en_a + type*4 +
		  ((irq & 8) >> 2) + (((irq-8) & 16) << 3);
	if (type == MFP_PENDING || type == MFP_SERVICE)
		__asm__ __volatile__ ( "moveb %0,%1"
				      : : "di" (mask), "m" (*reg) : "memory" );
	else
		__asm__ __volatile__ ( "andb %0,%1"
				      : : "di" (mask), "m" (*reg) : "memory" );
}


static inline void atari_enable_irq( unsigned irq )

{
	if (irq < STMFP_SOURCE_BASE || irq >= SCC_SOURCE_BASE) return;
	set_mfp_bit( irq, MFP_MASK );
}

static inline void atari_disable_irq( unsigned irq )

{
	if (irq < STMFP_SOURCE_BASE || irq >= SCC_SOURCE_BASE) return;
	clear_mfp_bit( irq, MFP_MASK );
}


static inline void atari_turnon_irq( unsigned irq )

{
	if (irq < STMFP_SOURCE_BASE || irq >= SCC_SOURCE_BASE) return;
	set_mfp_bit( irq, MFP_ENABLE );
}

static inline void atari_turnoff_irq( unsigned irq )

{
	if (irq < STMFP_SOURCE_BASE || irq >= SCC_SOURCE_BASE) return;
	clear_mfp_bit( irq, MFP_ENABLE );
	clear_mfp_bit( irq, MFP_PENDING );
}

static inline void atari_clear_pending_irq( unsigned irq )

{
	if (irq < STMFP_SOURCE_BASE || irq >= SCC_SOURCE_BASE) return;
	clear_mfp_bit( irq, MFP_PENDING );
}

static inline int atari_irq_pending( unsigned irq )

{
	if (irq < STMFP_SOURCE_BASE || irq >= SCC_SOURCE_BASE) return( 0 );
	return( get_mfp_bit( irq, MFP_PENDING ) );
}

unsigned long atari_register_vme_int( void );
void atari_unregister_vme_int( unsigned long );

#endif 
