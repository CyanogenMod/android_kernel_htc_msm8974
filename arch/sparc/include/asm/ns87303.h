/* ns87303.h: Configuration Register Description for the
 *            National Semiconductor PC87303 (SuperIO).
 *
 * Copyright (C) 1997  Eddie C. Dost  (ecd@skynet.be)
 */

#ifndef _SPARC_NS87303_H
#define _SPARC_NS87303_H 1

#define FER	0x00
#define FAR	0x01
#define PTR	0x02
#define FCR	0x03
#define PCR	0x04
#define KRR	0x05
#define PMC	0x06
#define TUP	0x07
#define SID	0x08
#define ASC	0x09
#define CS0CF0	0x0a
#define CS0CF1	0x0b
#define CS1CF0	0x0c
#define CS1CF1	0x0d

#define FER_EDM		0x10	

#define FAR_LPT_MASK	0x03
#define FAR_LPTB	0x00
#define FAR_LPTA	0x01
#define FAR_LPTC	0x02

#define PTR_LPTB_IRQ7	0x08
#define PTR_LEVEL_IRQ	0x80	
#define PTR_LPT_REG_DIR	0x80	
				

#define FCR_LDE		0x10	
#define FCR_ZWS_ENA	0x20	

#define PCR_EPP_ENABLE	0x01
#define PCR_EPP_IEEE	0x02	
#define PCR_ECP_ENABLE	0x04
#define PCR_ECP_CLK_ENA	0x08	
#define PCR_IRQ_POLAR	0x20	
				
#define PCR_IRQ_ODRAIN	0x40	

#define TUP_EPP_TIMO	0x02	

#define ASC_LPT_IRQ7	0x01	
#define ASC_DRV2_SEL	0x02	

#define FER_RESERVED	0x00
#define FAR_RESERVED	0x00
#define PTR_RESERVED	0x73
#define FCR_RESERVED	0xc4
#define PCR_RESERVED	0x10
#define KRR_RESERVED	0x00
#define PMC_RESERVED	0x98
#define TUP_RESERVED	0xfb
#define SIP_RESERVED	0x00
#define ASC_RESERVED	0x18
#define CS0CF0_RESERVED	0x00
#define CS0CF1_RESERVED	0x08
#define CS1CF0_RESERVED	0x00
#define CS1CF1_RESERVED	0x08

#ifdef __KERNEL__

#include <linux/spinlock.h>

#include <asm/io.h>

extern spinlock_t ns87303_lock;

static inline int ns87303_modify(unsigned long port, unsigned int index,
				     unsigned char clr, unsigned char set)
{
	static unsigned char reserved[] = {
		FER_RESERVED, FAR_RESERVED, PTR_RESERVED, FCR_RESERVED,
		PCR_RESERVED, KRR_RESERVED, PMC_RESERVED, TUP_RESERVED,
		SIP_RESERVED, ASC_RESERVED, CS0CF0_RESERVED, CS0CF1_RESERVED,
		CS1CF0_RESERVED, CS1CF1_RESERVED
	};
	unsigned long flags;
	unsigned char value;

	if (index > 0x0d)
		return -EINVAL;

	spin_lock_irqsave(&ns87303_lock, flags);

	outb(index, port);
	value = inb(port + 1);
	value &= ~(reserved[index] | clr);
	value |= set;
	outb(value, port + 1);
	outb(value, port + 1);

	spin_unlock_irqrestore(&ns87303_lock, flags);

	return 0;
}

#endif 

#endif 
