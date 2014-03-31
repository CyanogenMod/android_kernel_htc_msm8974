/*
 *  a.out loader for x86-64
 *
 *  Copyright (C) 1991, 1992, 1996  Linus Torvalds
 *  Hacked together by Andi Kleen
 */

#include <linux/module.h>

#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/a.out.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/binfmts.h>
#include <linux/personality.h>
#include <linux/init.h>
#include <linux/jiffies.h>

#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/cacheflush.h>
#include <asm/user32.h>
#include <asm/ia32.h>

#undef WARN_OLD
#undef CORE_DUMP 

static int load_aout_binary(struct linux_binprm *, struct pt_regs *regs);
static int load_aout_library(struct file *);

#ifdef CORE_DUMP
static int aout_core_dump(long signr, struct pt_regs *regs, struct file *file,
			  unsigned long limit);

static void dump_thread32(struct pt_regs *regs, struct user32 *dump)
{
	u32 fs, gs;

	dump->magic = CMAGIC;
	dump->start_code = 0;
	dump->start_stack = regs->sp & ~(PAGE_SIZE - 1);
	dump->u_tsize = ((unsigned long) current->mm->end_code) >> PAGE_SHIFT;
	dump->u_dsize = ((unsigned long)
			 (current->mm->brk + (PAGE_SIZE-1))) >> PAGE_SHIFT;
	dump->u_dsize -= dump->u_tsize;
	dump->u_ssize = 0;
	dump->u_debugreg[0] = current->thread.debugreg0;
	dump->u_debugreg[1] = current->thread.debugreg1;
	dump->u_debugreg[2] = current->thread.debugreg2;
	dump->u_debugreg[3] = current->thread.debugreg3;
	dump->u_debugreg[4] = 0;
	dump->u_debugreg[5] = 0;
	dump->u_debugreg[6] = current->thread.debugreg6;
	dump->u_debugreg[7] = current->thread.debugreg7;

	if (dump->start_stack < 0xc0000000) {
		unsigned long tmp;

		tmp = (unsigned long) (0xc0000000 - dump->start_stack);
		dump->u_ssize = tmp >> PAGE_SHIFT;
	}

	dump->regs.bx = regs->bx;
	dump->regs.cx = regs->cx;
	dump->regs.dx = regs->dx;
	dump->regs.si = regs->si;
	dump->regs.di = regs->di;
	dump->regs.bp = regs->bp;
	dump->regs.ax = regs->ax;
	dump->regs.ds = current->thread.ds;
	dump->regs.es = current->thread.es;
	savesegment(fs, fs);
	dump->regs.fs = fs;
	savesegment(gs, gs);
	dump->regs.gs = gs;
	dump->regs.orig_ax = regs->orig_ax;
	dump->regs.ip = regs->ip;
	dump->regs.cs = regs->cs;
	dump->regs.flags = regs->flags;
	dump->regs.sp = regs->sp;
	dump->regs.ss = regs->ss;

#if 1 
	dump->u_fpvalid = 0;
#else
	dump->u_fpvalid = dump_fpu(regs, &dump->i387);
#endif
}

#endif

static struct linux_binfmt aout_format = {
	.module		= THIS_MODULE,
	.load_binary	= load_aout_binary,
	.load_shlib	= load_aout_library,
#ifdef CORE_DUMP
	.core_dump	= aout_core_dump,
#endif
	.min_coredump	= PAGE_SIZE
};

static void set_brk(unsigned long start, unsigned long end)
{
	start = PAGE_ALIGN(start);
	end = PAGE_ALIGN(end);
	if (end <= start)
		return;
	vm_brk(start, end - start);
}

#ifdef CORE_DUMP

#include <linux/coredump.h>

#define DUMP_WRITE(addr, nr)			     \
	if (!dump_write(file, (void *)(addr), (nr))) \
		goto end_coredump;

#define DUMP_SEEK(offset)		\
	if (!dump_seek(file, offset))	\
		goto end_coredump;

#define START_DATA()	(u.u_tsize << PAGE_SHIFT)
#define START_STACK(u)	(u.start_stack)


static int aout_core_dump(long signr, struct pt_regs *regs, struct file *file,
			  unsigned long limit)
{
	mm_segment_t fs;
	int has_dumped = 0;
	unsigned long dump_start, dump_size;
	struct user32 dump;

	fs = get_fs();
	set_fs(KERNEL_DS);
	has_dumped = 1;
	current->flags |= PF_DUMPCORE;
	strncpy(dump.u_comm, current->comm, sizeof(current->comm));
	dump.u_ar0 = offsetof(struct user32, regs);
	dump.signal = signr;
	dump_thread32(regs, &dump);

	if ((dump.u_dsize + dump.u_ssize + 1) * PAGE_SIZE > limit)
		dump.u_dsize = 0;

	
	if ((dump.u_ssize + 1) * PAGE_SIZE > limit)
		dump.u_ssize = 0;

	
	set_fs(USER_DS);
	if (!access_ok(VERIFY_READ, (void *) (unsigned long)START_DATA(dump),
		       dump.u_dsize << PAGE_SHIFT))
		dump.u_dsize = 0;
	if (!access_ok(VERIFY_READ, (void *) (unsigned long)START_STACK(dump),
		       dump.u_ssize << PAGE_SHIFT))
		dump.u_ssize = 0;

	set_fs(KERNEL_DS);
	
	DUMP_WRITE(&dump, sizeof(dump));
	
	DUMP_SEEK(PAGE_SIZE);
	
	set_fs(USER_DS);
	
	if (dump.u_dsize != 0) {
		dump_start = START_DATA(dump);
		dump_size = dump.u_dsize << PAGE_SHIFT;
		DUMP_WRITE(dump_start, dump_size);
	}
	
	if (dump.u_ssize != 0) {
		dump_start = START_STACK(dump);
		dump_size = dump.u_ssize << PAGE_SHIFT;
		DUMP_WRITE(dump_start, dump_size);
	}
end_coredump:
	set_fs(fs);
	return has_dumped;
}
#endif

static u32 __user *create_aout_tables(char __user *p, struct linux_binprm *bprm)
{
	u32 __user *argv, *envp, *sp;
	int argc = bprm->argc, envc = bprm->envc;

	sp = (u32 __user *) ((-(unsigned long)sizeof(u32)) & (unsigned long) p);
	sp -= envc+1;
	envp = sp;
	sp -= argc+1;
	argv = sp;
	put_user((unsigned long) envp, --sp);
	put_user((unsigned long) argv, --sp);
	put_user(argc, --sp);
	current->mm->arg_start = (unsigned long) p;
	while (argc-- > 0) {
		char c;

		put_user((u32)(unsigned long)p, argv++);
		do {
			get_user(c, p++);
		} while (c);
	}
	put_user(0, argv);
	current->mm->arg_end = current->mm->env_start = (unsigned long) p;
	while (envc-- > 0) {
		char c;

		put_user((u32)(unsigned long)p, envp++);
		do {
			get_user(c, p++);
		} while (c);
	}
	put_user(0, envp);
	current->mm->env_end = (unsigned long) p;
	return sp;
}

static int load_aout_binary(struct linux_binprm *bprm, struct pt_regs *regs)
{
	unsigned long error, fd_offset, rlim;
	struct exec ex;
	int retval;

	ex = *((struct exec *) bprm->buf);		
	if ((N_MAGIC(ex) != ZMAGIC && N_MAGIC(ex) != OMAGIC &&
	     N_MAGIC(ex) != QMAGIC && N_MAGIC(ex) != NMAGIC) ||
	    N_TRSIZE(ex) || N_DRSIZE(ex) ||
	    i_size_read(bprm->file->f_path.dentry->d_inode) <
	    ex.a_text+ex.a_data+N_SYMSIZE(ex)+N_TXTOFF(ex)) {
		return -ENOEXEC;
	}

	fd_offset = N_TXTOFF(ex);

	rlim = rlimit(RLIMIT_DATA);
	if (rlim >= RLIM_INFINITY)
		rlim = ~0;
	if (ex.a_data + ex.a_bss > rlim)
		return -ENOMEM;

	
	retval = flush_old_exec(bprm);
	if (retval)
		return retval;

	
	set_personality(PER_LINUX);
	set_personality_ia32(false);

	setup_new_exec(bprm);

	regs->cs = __USER32_CS;
	regs->r8 = regs->r9 = regs->r10 = regs->r11 = regs->r12 =
		regs->r13 = regs->r14 = regs->r15 = 0;

	current->mm->end_code = ex.a_text +
		(current->mm->start_code = N_TXTADDR(ex));
	current->mm->end_data = ex.a_data +
		(current->mm->start_data = N_DATADDR(ex));
	current->mm->brk = ex.a_bss +
		(current->mm->start_brk = N_BSSADDR(ex));
	current->mm->free_area_cache = TASK_UNMAPPED_BASE;
	current->mm->cached_hole_size = 0;

	retval = setup_arg_pages(bprm, IA32_STACK_TOP, EXSTACK_DEFAULT);
	if (retval < 0) {
		
		send_sig(SIGKILL, current, 0);
		return retval;
	}

	install_exec_creds(bprm);

	if (N_MAGIC(ex) == OMAGIC) {
		unsigned long text_addr, map_size;
		loff_t pos;

		text_addr = N_TXTADDR(ex);

		pos = 32;
		map_size = ex.a_text+ex.a_data;

		error = vm_brk(text_addr & PAGE_MASK, map_size);

		if (error != (text_addr & PAGE_MASK)) {
			send_sig(SIGKILL, current, 0);
			return error;
		}

		error = bprm->file->f_op->read(bprm->file,
			 (char __user *)text_addr,
			  ex.a_text+ex.a_data, &pos);
		if ((signed long)error < 0) {
			send_sig(SIGKILL, current, 0);
			return error;
		}

		flush_icache_range(text_addr, text_addr+ex.a_text+ex.a_data);
	} else {
#ifdef WARN_OLD
		static unsigned long error_time, error_time2;
		if ((ex.a_text & 0xfff || ex.a_data & 0xfff) &&
		    (N_MAGIC(ex) != NMAGIC) &&
				time_after(jiffies, error_time2 + 5*HZ)) {
			printk(KERN_NOTICE "executable not page aligned\n");
			error_time2 = jiffies;
		}

		if ((fd_offset & ~PAGE_MASK) != 0 &&
			    time_after(jiffies, error_time + 5*HZ)) {
			printk(KERN_WARNING
			       "fd_offset is not page aligned. Please convert "
			       "program: %s\n",
			       bprm->file->f_path.dentry->d_name.name);
			error_time = jiffies;
		}
#endif

		if (!bprm->file->f_op->mmap || (fd_offset & ~PAGE_MASK) != 0) {
			loff_t pos = fd_offset;

			vm_brk(N_TXTADDR(ex), ex.a_text+ex.a_data);
			bprm->file->f_op->read(bprm->file,
					(char __user *)N_TXTADDR(ex),
					ex.a_text+ex.a_data, &pos);
			flush_icache_range((unsigned long) N_TXTADDR(ex),
					   (unsigned long) N_TXTADDR(ex) +
					   ex.a_text+ex.a_data);
			goto beyond_if;
		}

		error = vm_mmap(bprm->file, N_TXTADDR(ex), ex.a_text,
				PROT_READ | PROT_EXEC,
				MAP_FIXED | MAP_PRIVATE | MAP_DENYWRITE |
				MAP_EXECUTABLE | MAP_32BIT,
				fd_offset);

		if (error != N_TXTADDR(ex)) {
			send_sig(SIGKILL, current, 0);
			return error;
		}

		error = vm_mmap(bprm->file, N_DATADDR(ex), ex.a_data,
				PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_FIXED | MAP_PRIVATE | MAP_DENYWRITE |
				MAP_EXECUTABLE | MAP_32BIT,
				fd_offset + ex.a_text);
		if (error != N_DATADDR(ex)) {
			send_sig(SIGKILL, current, 0);
			return error;
		}
	}
beyond_if:
	set_binfmt(&aout_format);

	set_brk(current->mm->start_brk, current->mm->brk);

	current->mm->start_stack =
		(unsigned long)create_aout_tables((char __user *)bprm->p, bprm);
	
	loadsegment(fs, 0);
	loadsegment(ds, __USER32_DS);
	loadsegment(es, __USER32_DS);
	load_gs_index(0);
	(regs)->ip = ex.a_entry;
	(regs)->sp = current->mm->start_stack;
	(regs)->flags = 0x200;
	(regs)->cs = __USER32_CS;
	(regs)->ss = __USER32_DS;
	regs->r8 = regs->r9 = regs->r10 = regs->r11 =
	regs->r12 = regs->r13 = regs->r14 = regs->r15 = 0;
	set_fs(USER_DS);
	return 0;
}

static int load_aout_library(struct file *file)
{
	struct inode *inode;
	unsigned long bss, start_addr, len, error;
	int retval;
	struct exec ex;

	inode = file->f_path.dentry->d_inode;

	retval = -ENOEXEC;
	error = kernel_read(file, 0, (char *) &ex, sizeof(ex));
	if (error != sizeof(ex))
		goto out;

	
	if ((N_MAGIC(ex) != ZMAGIC && N_MAGIC(ex) != QMAGIC) || N_TRSIZE(ex) ||
	    N_DRSIZE(ex) || ((ex.a_entry & 0xfff) && N_MAGIC(ex) == ZMAGIC) ||
	    i_size_read(inode) <
	    ex.a_text+ex.a_data+N_SYMSIZE(ex)+N_TXTOFF(ex)) {
		goto out;
	}

	if (N_FLAGS(ex))
		goto out;


	start_addr =  ex.a_entry & 0xfffff000;

	if ((N_TXTOFF(ex) & ~PAGE_MASK) != 0) {
		loff_t pos = N_TXTOFF(ex);

#ifdef WARN_OLD
		static unsigned long error_time;
		if (time_after(jiffies, error_time + 5*HZ)) {
			printk(KERN_WARNING
			       "N_TXTOFF is not page aligned. Please convert "
			       "library: %s\n",
			       file->f_path.dentry->d_name.name);
			error_time = jiffies;
		}
#endif
		vm_brk(start_addr, ex.a_text + ex.a_data + ex.a_bss);

		file->f_op->read(file, (char __user *)start_addr,
			ex.a_text + ex.a_data, &pos);
		flush_icache_range((unsigned long) start_addr,
				   (unsigned long) start_addr + ex.a_text +
				   ex.a_data);

		retval = 0;
		goto out;
	}
	
	error = vm_mmap(file, start_addr, ex.a_text + ex.a_data,
			PROT_READ | PROT_WRITE | PROT_EXEC,
			MAP_FIXED | MAP_PRIVATE | MAP_DENYWRITE | MAP_32BIT,
			N_TXTOFF(ex));
	retval = error;
	if (error != start_addr)
		goto out;

	len = PAGE_ALIGN(ex.a_text + ex.a_data);
	bss = ex.a_text + ex.a_data + ex.a_bss;
	if (bss > len) {
		error = vm_brk(start_addr + len, bss - len);
		retval = error;
		if (error != start_addr + len)
			goto out;
	}
	retval = 0;
out:
	return retval;
}

static int __init init_aout_binfmt(void)
{
	register_binfmt(&aout_format);
	return 0;
}

static void __exit exit_aout_binfmt(void)
{
	unregister_binfmt(&aout_format);
}

module_init(init_aout_binfmt);
module_exit(exit_aout_binfmt);
MODULE_LICENSE("GPL");
