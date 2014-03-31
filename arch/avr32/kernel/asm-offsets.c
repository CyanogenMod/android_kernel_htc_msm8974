
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/thread_info.h>
#include <linux/kbuild.h>

void foo(void)
{
	OFFSET(TI_task, thread_info, task);
	OFFSET(TI_exec_domain, thread_info, exec_domain);
	OFFSET(TI_flags, thread_info, flags);
	OFFSET(TI_cpu, thread_info, cpu);
	OFFSET(TI_preempt_count, thread_info, preempt_count);
	OFFSET(TI_rar_saved, thread_info, rar_saved);
	OFFSET(TI_rsr_saved, thread_info, rsr_saved);
	OFFSET(TI_restart_block, thread_info, restart_block);
	BLANK();
	OFFSET(TSK_active_mm, task_struct, active_mm);
	BLANK();
	OFFSET(MM_pgd, mm_struct, pgd);
}
