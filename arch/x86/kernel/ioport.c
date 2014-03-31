/*
 * This contains the io-permission bitmap code - written by obz, with changes
 * by Linus. 32/64 bits code unification by Miguel Botón.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/smp.h>
#include <linux/stddef.h>
#include <linux/slab.h>
#include <linux/thread_info.h>
#include <linux/syscalls.h>
#include <linux/bitmap.h>
#include <asm/syscalls.h>

asmlinkage long sys_ioperm(unsigned long from, unsigned long num, int turn_on)
{
	struct thread_struct *t = &current->thread;
	struct tss_struct *tss;
	unsigned int i, max_long, bytes, bytes_updated;

	if ((from + num <= from) || (from + num > IO_BITMAP_BITS))
		return -EINVAL;
	if (turn_on && !capable(CAP_SYS_RAWIO))
		return -EPERM;

	if (!t->io_bitmap_ptr) {
		unsigned long *bitmap = kmalloc(IO_BITMAP_BYTES, GFP_KERNEL);

		if (!bitmap)
			return -ENOMEM;

		memset(bitmap, 0xff, IO_BITMAP_BYTES);
		t->io_bitmap_ptr = bitmap;
		set_thread_flag(TIF_IO_BITMAP);
	}

	tss = &per_cpu(init_tss, get_cpu());

	if (turn_on)
		bitmap_clear(t->io_bitmap_ptr, from, num);
	else
		bitmap_set(t->io_bitmap_ptr, from, num);

	max_long = 0;
	for (i = 0; i < IO_BITMAP_LONGS; i++)
		if (t->io_bitmap_ptr[i] != ~0UL)
			max_long = i;

	bytes = (max_long + 1) * sizeof(unsigned long);
	bytes_updated = max(bytes, t->io_bitmap_max);

	t->io_bitmap_max = bytes;

	
	memcpy(tss->io_bitmap, t->io_bitmap_ptr, bytes_updated);

	put_cpu();

	return 0;
}

long sys_iopl(unsigned int level, struct pt_regs *regs)
{
	unsigned int old = (regs->flags >> 12) & 3;
	struct thread_struct *t = &current->thread;

	if (level > 3)
		return -EINVAL;
	
	if (level > old) {
		if (!capable(CAP_SYS_RAWIO))
			return -EPERM;
	}
	regs->flags = (regs->flags & ~X86_EFLAGS_IOPL) | (level << 12);
	t->iopl = level << 12;
	set_iopl_mask(t->iopl);

	return 0;
}
