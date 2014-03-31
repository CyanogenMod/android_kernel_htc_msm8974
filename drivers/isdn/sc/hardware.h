/*
 * Hardware specific macros, defines and structures
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef HARDWARE_H
#define HARDWARE_H

#include <asm/param.h>			


#define MAX_CARDS	4		

#define SIGNATURE	0x87654321	
#define SIG_OFFSET	0x1004		
#define TRACE_OFFSET	0x1008		
#define BUFFER_OFFSET	0x1800		

#define IOBASE_MIN	0x180		
#define IOBASE_MAX	0x3C0		
#define IOBASE_OFFSET	0x20		
#define FIFORD_OFFSET	0x0
#define FIFOWR_OFFSET	0x400
#define FIFOSTAT_OFFSET	0x1000
#define RESET_OFFSET	0x2800
#define PG0_OFFSET	0x3000		
#define PG1_OFFSET	0x3400		
#define PG2_OFFSET	0x3800		
#define PG3_OFFSET	0x3C00		

#define FIFO_READ	0		
#define FIFO_WRITE	1		
#define LO_ADDR_PTR	2		
#define HI_ADDR_PTR	3		
#define NOT_USED_1	4
#define FIFO_STATUS	5		
#define NOT_USED_2	6
#define MEM_OFFSET	7
#define SFT_RESET	10		
#define EXP_BASE	11		
#define EXP_PAGE0	12		
#define EXP_PAGE1	13		
#define EXP_PAGE2	14		
#define EXP_PAGE3	15		
#define IRQ_SELECT	16		
#define MAX_IO_REGS	17		

#define RF_HAS_DATA	0x01		
#define RF_QUART_FULL	0x02		
#define RF_HALF_FULL	0x04		
#define RF_NOT_FULL	0x08		
#define WF_HAS_DATA	0x10		
#define WF_QUART_FULL	0x20		
#define WF_HALF_FULL	0x40		
#define WF_NOT_FULL	0x80		

#define SRAM_MIN	0xC0000         
#define SRAM_MAX	0xEFFFF         
#define SRAM_PAGESIZE	0x4000		

#define BUFFER_SIZE	0x800		
#define BUFFER_BASE	BUFFER_OFFSET	
#define BUFFERS_MAX	16		
#define HDLC_PROTO	0x01		

#define BRI_BOARD	0
#define POTS_BOARD	1
#define PRI_BOARD	2

#define BRI_CHANNELS	2		
#define BRI_BASEPG_VAL	0x98
#define BRI_MAGIC	0x60000		
#define BRI_MEMSIZE	0x10000		
#define BRI_PARTNO	"72-029"
#define BRI_FEATURES	ISDN_FEATURE_L2_HDLC | ISDN_FEATURE_L3_TRANS;
#define PRI_CHANNELS	23		
#define PRI_BASEPG_VAL	0x88
#define PRI_MAGIC	0x20000		
#define PRI_MEMSIZE	0x100000	
#define PRI_PARTNO	"72-030"
#define PRI_FEATURES	ISDN_FEATURE_L2_HDLC | ISDN_FEATURE_L3_TRANS;


#define IS_VALID_CHANNEL(y, x)	((x > 0) && (x <= sc_adapter[y]->channels))

#endif
