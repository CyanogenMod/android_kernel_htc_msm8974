
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>


static long jdo_fork(unsigned long clone_flags, unsigned long stack_start,
	      struct pt_regs *regs, unsigned long stack_size,
	      int __user *parent_tidptr, int __user *child_tidptr)
{
	printk(KERN_INFO "jprobe: clone_flags = 0x%lx, stack_size = 0x%lx,"
			" regs = 0x%p\n",
	       clone_flags, stack_size, regs);

	
	jprobe_return();
	return 0;
}

static struct jprobe my_jprobe = {
	.entry			= jdo_fork,
	.kp = {
		.symbol_name	= "do_fork",
	},
};

static int __init jprobe_init(void)
{
	int ret;

	ret = register_jprobe(&my_jprobe);
	if (ret < 0) {
		printk(KERN_INFO "register_jprobe failed, returned %d\n", ret);
		return -1;
	}
	printk(KERN_INFO "Planted jprobe at %p, handler addr %p\n",
	       my_jprobe.kp.addr, my_jprobe.entry);
	return 0;
}

static void __exit jprobe_exit(void)
{
	unregister_jprobe(&my_jprobe);
	printk(KERN_INFO "jprobe at %p unregistered\n", my_jprobe.kp.addr);
}

module_init(jprobe_init)
module_exit(jprobe_exit)
MODULE_LICENSE("GPL");
