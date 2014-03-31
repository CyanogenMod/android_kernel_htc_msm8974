
#include <linux/sched.h>
#include <linux/kbuild.h>

#ifdef CONFIG_SPARC32
int sparc32_foo(void)
{
	DEFINE(AOFF_thread_fork_kpsr,
			offsetof(struct thread_struct, fork_kpsr));
	return 0;
}
#else
int sparc64_foo(void)
{
	return 0;
}
#endif

int foo(void)
{
	BLANK();
	DEFINE(AOFF_task_thread, offsetof(struct task_struct, thread));
	BLANK();
	DEFINE(AOFF_mm_context, offsetof(struct mm_struct, context));

	
	return 0;
}

