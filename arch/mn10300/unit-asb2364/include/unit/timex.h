/* timex.h: MN2WS0038 architecture timer specifications
 *
 * Copyright (C) 2002, 2010 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _ASM_UNIT_TIMEX_H
#define _ASM_UNIT_TIMEX_H

#ifndef __ASSEMBLY__
#include <linux/irq.h>
#endif 

#include <asm/timer-regs.h>
#include <unit/clock.h>
#include <asm/param.h>


#define	TMJCBR_MAX		0xffffff	
#define	TMJCIRQ			TMTIRQ

#ifndef __ASSEMBLY__

#define MN10300_SRC_IOBCLK	MN10300_IOBCLK

#ifndef HZ
# error HZ undeclared.
#endif 

#define MN10300_JCCLK		(MN10300_SRC_IOBCLK)
#define MN10300_TSCCLK		(MN10300_SRC_IOBCLK)

#define MN10300_JC_PER_HZ	((MN10300_JCCLK + HZ / 2) / HZ)
#define MN10300_TSC_PER_HZ	((MN10300_TSCCLK + HZ / 2) / HZ)

#if (MN10300_JC_PER_HZ - 1) > TMJCBR_MAX
# error MTM tick timer interval value is overflow.
#endif

static inline void stop_jiffies_counter(void)
{
	u16 tmp;
	TMTMD = 0;
	tmp = TMTMD;
}

static inline void reload_jiffies_counter(u32 cnt)
{
	u32 tmp;

	TMTBR = cnt;
	tmp = TMTBR;

	TMTMD = TMTMD_TMTLDE;
	TMTMD = TMTMD_TMTCNE;
	tmp = TMTMD;
}

#if defined(CONFIG_SMP) && defined(CONFIG_GENERIC_CLOCKEVENTS) && \
    !defined(CONFIG_GENERIC_CLOCKEVENTS_BROADCAST)

#define	TMJC1IRQ		TM5IRQ

static inline void stop_jiffies_counter1(void)
{
	u8 tmp;
	TM4MD = 0;
	TM5MD = 0;
	tmp = TM4MD;
	tmp = TM5MD;
}

static inline void reload_jiffies_counter1(u32 cnt)
{
	u32 tmp;

	TM45BR = cnt;
	tmp = TM45BR;

	TM4MD = TM4MD_INIT_COUNTER;
	tmp = TM4MD;

	TM5MD = TM5MD_SRC_TM4CASCADE | TM5MD_INIT_COUNTER;
	TM5MD = TM5MD_SRC_TM4CASCADE | TM5MD_COUNT_ENABLE;
	tmp = TM5MD;

	TM4MD = TM4MD_COUNT_ENABLE;
	tmp = TM4MD;
}
#endif 

#endif 


#define	TMTSCBR_MAX	0xffffffff

#ifndef __ASSEMBLY__

#define	TMTSCMD		TMSMD
#define	TMTSCBR		TMSBR
#define	TMTSCBC		TMSBC
#define	TMTSCICR	TMSICR

static inline void startup_timestamp_counter(void)
{
	u32 sync;


	TMTSCBR = TMTSCBR_MAX;
	sync = TMTSCBR;

	TMTSCICR = 0;
	sync = TMTSCICR;

	TMTSCMD = TMTMD_TMTLDE;
	TMTSCMD = TMTMD_TMTCNE;
	sync = TMTSCMD;
}

static inline void shutdown_timestamp_counter(void)
{
	TMTSCMD = 0;
}

typedef unsigned long cycles_t;

static inline cycles_t read_timestamp_counter(void)
{
	return (cycles_t)~TMTSCBC;
}

#endif 

#endif 
