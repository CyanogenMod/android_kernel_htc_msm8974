/*
 * Copyright (C) 2007 Lemote Inc. & Insititute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/delay.h>
#include <linux/interrupt.h>

#include <loongson.h>
void bonito_irqdispatch(void)
{
	u32 int_status;
	int i;

	
	int_status = LOONGSON_INTISR;
	while (int_status & (1 << 10)) {
		udelay(1);
		int_status = LOONGSON_INTISR;
	}

	
	int_status = LOONGSON_INTISR & LOONGSON_INTEN;

	if (int_status) {
		i = __ffs(int_status);
		do_IRQ(LOONGSON_IRQ_BASE + i);
	}
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending;

	pending = read_c0_cause() & read_c0_status() & ST0_IM;

	
	mach_irq_dispatch(pending);
}

void __init arch_init_irq(void)
{
	clear_c0_status(ST0_IM | ST0_BEV);

	
	LOONGSON_INTSTEER = 0;

	LOONGSON_INTENCLR = ~0;

	
	mach_init_irq();
}
