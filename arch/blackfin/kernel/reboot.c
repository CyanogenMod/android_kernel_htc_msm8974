/*
 * arch/blackfin/kernel/reboot.c - handle shutdown/reboot
 *
 * Copyright 2004-2007 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/interrupt.h>
#include <asm/bfin-global.h>
#include <asm/reboot.h>
#include <asm/bfrom.h>

__attribute__ ((__l1_text__, __noreturn__))
static void bfin_reset(void)
{
	if (!ANOMALY_05000353 && !ANOMALY_05000386)
		bfrom_SoftReset((void *)(L1_SCRATCH_START + L1_SCRATCH_LENGTH - 20));

	__builtin_bfin_ssync();

	
	bfin_write_SWRST(0x7);

	asm(
		"LSETUP (1f, 1f) LC0 = %0\n"
		"1: nop;"
		:
		: "a" (15 * 10)
		: "LC0", "LB0", "LT0"
	);

	
	bfin_write_SWRST(0);

	
#if defined(__ADSPBF522__) || defined(__ADSPBF524__) || defined(__ADSPBF526__)
	
	if (__SILICON_REVISION__ < 1 && bfin_revid() < 1)
		bfin_read_SWRST();
#endif

	asm(
		"LSETUP (1f, 1f) LC1 = %0\n"
		"1: nop;"
		:
		: "a" (15 * 1)
		: "LC1", "LB1", "LT1"
	);

	while (1)
		
		asm("raise 1");
}

__attribute__((weak))
void native_machine_restart(char *cmd)
{
}

void machine_restart(char *cmd)
{
	native_machine_restart(cmd);
	local_irq_disable();
	if (smp_processor_id())
		smp_call_function((void *)bfin_reset, 0, 1);
	else
		bfin_reset();
}

__attribute__((weak))
void native_machine_halt(void)
{
	idle_with_irq_disabled();
}

void machine_halt(void)
{
	native_machine_halt();
}

__attribute__((weak))
void native_machine_power_off(void)
{
	idle_with_irq_disabled();
}

void machine_power_off(void)
{
	native_machine_power_off();
}
