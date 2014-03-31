
#include <linux/init.h>
#include <asm/system.h>
#include <asm/proc-fns.h>

static void gemini_idle(void)
{
	local_irq_enable();
	cpu_do_idle();
}

static int __init gemini_idle_init(void)
{
	arm_pm_idle = gemini_idle;
	return 0;
}

arch_initcall(gemini_idle_init);
