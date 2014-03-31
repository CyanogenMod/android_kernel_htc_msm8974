/*
 *  linux/kernel/acct.c
 *
 *  BSD Process Accounting for Linux
 *
 *  Author: Marco van Wieringen <mvw@planets.elm.net>
 *
 *  Some code based on ideas and code from:
 *  Thomas K. Dyas <tdyas@eden.rutgers.edu>
 *
 *  This file implements BSD-style process accounting. Whenever any
 *  process exits, an accounting record of type "struct acct" is
 *  written to the file specified with the acct() system call. It is
 *  up to user-level programs to do useful things with the accounting
 *  log. The kernel just provides the raw accounting information.
 *
 * (C) Copyright 1995 - 1997 Marco van Wieringen - ELM Consultancy B.V.
 *
 *  Plugged two leaks. 1) It didn't return acct_file into the free_filps if
 *  the file happened to be read-only. 2) If the accounting was suspended
 *  due to the lack of space it happily allowed to reopen it and completely
 *  lost the old acct_file. 3/10/98, Al Viro.
 *
 *  Now we silently close acct_file on attempt to reopen. Cleaned sys_acct().
 *  XTerms and EMACS are manifestations of pure evil. 21/10/98, AV.
 *
 *  Fixed a nasty interaction with with sys_umount(). If the accointing
 *  was suspeneded we failed to stop it on umount(). Messy.
 *  Another one: remount to readonly didn't stop accounting.
 *	Question: what should we do if we have CAP_SYS_ADMIN but not
 *  CAP_SYS_PACCT? Current code does the following: umount returns -EBUSY
 *  unless we are messing with the root. In that case we are getting a
 *  real mess with do_remount_sb(). 9/11/98, AV.
 *
 *  Fixed a bunch of races (and pair of leaks). Probably not the best way,
 *  but this one obviously doesn't introduce deadlocks. Later. BTW, found
 *  one race (and leak) in BSD implementation.
 *  OK, that's better. ANOTHER race and leak in BSD variant. There always
 *  is one more bug... 10/11/98, AV.
 *
 *	Oh, fsck... Oopsable SMP race in do_process_acct() - we must hold
 * ->mmap_sem to walk the vma list of current->mm. Nasty, since it leaks
 * a struct file opened for write. Fixed. 2/6/2000, AV.
 */

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/acct.h>
#include <linux/capability.h>
#include <linux/file.h>
#include <linux/tty.h>
#include <linux/security.h>
#include <linux/vfs.h>
#include <linux/jiffies.h>
#include <linux/times.h>
#include <linux/syscalls.h>
#include <linux/mount.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <linux/blkdev.h> 
#include <linux/pid_namespace.h>


int acct_parm[3] = {4, 2, 30};
#define RESUME		(acct_parm[0])	
#define SUSPEND		(acct_parm[1])	
#define ACCT_TIMEOUT	(acct_parm[2])	

static void do_acct_process(struct bsd_acct_struct *acct,
		struct pid_namespace *ns, struct file *);

struct bsd_acct_struct {
	int			active;
	unsigned long		needcheck;
	struct file		*file;
	struct pid_namespace	*ns;
	struct list_head	list;
};

static DEFINE_SPINLOCK(acct_lock);
static LIST_HEAD(acct_list);

static int check_free_space(struct bsd_acct_struct *acct, struct file *file)
{
	struct kstatfs sbuf;
	int res;
	int act;
	u64 resume;
	u64 suspend;

	spin_lock(&acct_lock);
	res = acct->active;
	if (!file || time_is_before_jiffies(acct->needcheck))
		goto out;
	spin_unlock(&acct_lock);

	
	if (vfs_statfs(&file->f_path, &sbuf))
		return res;
	suspend = sbuf.f_blocks * SUSPEND;
	resume = sbuf.f_blocks * RESUME;

	do_div(suspend, 100);
	do_div(resume, 100);

	if (sbuf.f_bavail <= suspend)
		act = -1;
	else if (sbuf.f_bavail >= resume)
		act = 1;
	else
		act = 0;

	spin_lock(&acct_lock);
	if (file != acct->file) {
		if (act)
			res = act>0;
		goto out;
	}

	if (acct->active) {
		if (act < 0) {
			acct->active = 0;
			printk(KERN_INFO "Process accounting paused\n");
		}
	} else {
		if (act > 0) {
			acct->active = 1;
			printk(KERN_INFO "Process accounting resumed\n");
		}
	}

	acct->needcheck = jiffies + ACCT_TIMEOUT*HZ;
	res = acct->active;
out:
	spin_unlock(&acct_lock);
	return res;
}

static void acct_file_reopen(struct bsd_acct_struct *acct, struct file *file,
		struct pid_namespace *ns)
{
	struct file *old_acct = NULL;
	struct pid_namespace *old_ns = NULL;

	if (acct->file) {
		old_acct = acct->file;
		old_ns = acct->ns;
		acct->active = 0;
		acct->file = NULL;
		acct->ns = NULL;
		list_del(&acct->list);
	}
	if (file) {
		acct->file = file;
		acct->ns = ns;
		acct->needcheck = jiffies + ACCT_TIMEOUT*HZ;
		acct->active = 1;
		list_add(&acct->list, &acct_list);
	}
	if (old_acct) {
		mnt_unpin(old_acct->f_path.mnt);
		spin_unlock(&acct_lock);
		do_acct_process(acct, old_ns, old_acct);
		filp_close(old_acct, NULL);
		spin_lock(&acct_lock);
	}
}

static int acct_on(char *name)
{
	struct file *file;
	struct vfsmount *mnt;
	struct pid_namespace *ns;
	struct bsd_acct_struct *acct = NULL;

	
	file = filp_open(name, O_WRONLY|O_APPEND|O_LARGEFILE, 0);
	if (IS_ERR(file))
		return PTR_ERR(file);

	if (!S_ISREG(file->f_path.dentry->d_inode->i_mode)) {
		filp_close(file, NULL);
		return -EACCES;
	}

	if (!file->f_op->write) {
		filp_close(file, NULL);
		return -EIO;
	}

	ns = task_active_pid_ns(current);
	if (ns->bacct == NULL) {
		acct = kzalloc(sizeof(struct bsd_acct_struct), GFP_KERNEL);
		if (acct == NULL) {
			filp_close(file, NULL);
			return -ENOMEM;
		}
	}

	spin_lock(&acct_lock);
	if (ns->bacct == NULL) {
		ns->bacct = acct;
		acct = NULL;
	}

	mnt = file->f_path.mnt;
	mnt_pin(mnt);
	acct_file_reopen(ns->bacct, file, ns);
	spin_unlock(&acct_lock);

	mntput(mnt); 
	kfree(acct);

	return 0;
}

/**
 * sys_acct - enable/disable process accounting
 * @name: file name for accounting records or NULL to shutdown accounting
 *
 * Returns 0 for success or negative errno values for failure.
 *
 * sys_acct() is the only system call needed to implement process
 * accounting. It takes the name of the file where accounting records
 * should be written. If the filename is NULL, accounting will be
 * shutdown.
 */
SYSCALL_DEFINE1(acct, const char __user *, name)
{
	int error = 0;

	if (!capable(CAP_SYS_PACCT))
		return -EPERM;

	if (name) {
		char *tmp = getname(name);
		if (IS_ERR(tmp))
			return (PTR_ERR(tmp));
		error = acct_on(tmp);
		putname(tmp);
	} else {
		struct bsd_acct_struct *acct;

		acct = task_active_pid_ns(current)->bacct;
		if (acct == NULL)
			return 0;

		spin_lock(&acct_lock);
		acct_file_reopen(acct, NULL, NULL);
		spin_unlock(&acct_lock);
	}

	return error;
}

void acct_auto_close_mnt(struct vfsmount *m)
{
	struct bsd_acct_struct *acct;

	spin_lock(&acct_lock);
restart:
	list_for_each_entry(acct, &acct_list, list)
		if (acct->file && acct->file->f_path.mnt == m) {
			acct_file_reopen(acct, NULL, NULL);
			goto restart;
		}
	spin_unlock(&acct_lock);
}

void acct_auto_close(struct super_block *sb)
{
	struct bsd_acct_struct *acct;

	spin_lock(&acct_lock);
restart:
	list_for_each_entry(acct, &acct_list, list)
		if (acct->file && acct->file->f_path.dentry->d_sb == sb) {
			acct_file_reopen(acct, NULL, NULL);
			goto restart;
		}
	spin_unlock(&acct_lock);
}

void acct_exit_ns(struct pid_namespace *ns)
{
	struct bsd_acct_struct *acct = ns->bacct;

	if (acct == NULL)
		return;

	spin_lock(&acct_lock);
	if (acct->file != NULL)
		acct_file_reopen(acct, NULL, NULL);
	spin_unlock(&acct_lock);

	kfree(acct);
}


#define	MANTSIZE	13			
#define	EXPSIZE		3			
#define	MAXFRACT	((1 << MANTSIZE) - 1)	

static comp_t encode_comp_t(unsigned long value)
{
	int exp, rnd;

	exp = rnd = 0;
	while (value > MAXFRACT) {
		rnd = value & (1 << (EXPSIZE - 1));	
		value >>= EXPSIZE;	
		exp++;
	}

	if (rnd && (++value > MAXFRACT)) {
		value >>= EXPSIZE;
		exp++;
	}

	exp <<= MANTSIZE;		
	exp += value;			
	return exp;
}

#if ACCT_VERSION==1 || ACCT_VERSION==2

#define MANTSIZE2       20                      
#define EXPSIZE2        5                       
#define MAXFRACT2       ((1ul << MANTSIZE2) - 1) 
#define MAXEXP2         ((1 <<EXPSIZE2) - 1)    

static comp2_t encode_comp2_t(u64 value)
{
	int exp, rnd;

	exp = (value > (MAXFRACT2>>1));
	rnd = 0;
	while (value > MAXFRACT2) {
		rnd = value & 1;
		value >>= 1;
		exp++;
	}

	if (rnd && (++value > MAXFRACT2)) {
		value >>= 1;
		exp++;
	}

	if (exp > MAXEXP2) {
		
		return (1ul << (MANTSIZE2+EXPSIZE2-1)) - 1;
	} else {
		return (value & (MAXFRACT2>>1)) | (exp << (MANTSIZE2-1));
	}
}
#endif

#if ACCT_VERSION==3
static u32 encode_float(u64 value)
{
	unsigned exp = 190;
	unsigned u;

	if (value==0) return 0;
	while ((s64)value > 0){
		value <<= 1;
		exp--;
	}
	u = (u32)(value >> 40) & 0x7fffffu;
	return u | (exp << 23);
}
#endif

/*
 *  Write an accounting entry for an exiting process
 *
 *  The acct_process() call is the workhorse of the process
 *  accounting system. The struct acct is built here and then written
 *  into the accounting file. This function should only be called from
 *  do_exit() or when switching to a different output file.
 */

static void do_acct_process(struct bsd_acct_struct *acct,
		struct pid_namespace *ns, struct file *file)
{
	struct pacct_struct *pacct = &current->signal->pacct;
	acct_t ac;
	mm_segment_t fs;
	unsigned long flim;
	u64 elapsed;
	u64 run_time;
	struct timespec uptime;
	struct tty_struct *tty;
	const struct cred *orig_cred;

	
	orig_cred = override_creds(file->f_cred);

	if (!check_free_space(acct, file))
		goto out;

	memset(&ac, 0, sizeof(acct_t));

	ac.ac_version = ACCT_VERSION | ACCT_BYTEORDER;
	strlcpy(ac.ac_comm, current->comm, sizeof(ac.ac_comm));

	
	do_posix_clock_monotonic_gettime(&uptime);
	run_time = (u64)uptime.tv_sec*NSEC_PER_SEC + uptime.tv_nsec;
	run_time -= (u64)current->group_leader->start_time.tv_sec * NSEC_PER_SEC
		       + current->group_leader->start_time.tv_nsec;
	
	elapsed = nsec_to_AHZ(run_time);
#if ACCT_VERSION==3
	ac.ac_etime = encode_float(elapsed);
#else
	ac.ac_etime = encode_comp_t(elapsed < (unsigned long) -1l ?
	                       (unsigned long) elapsed : (unsigned long) -1l);
#endif
#if ACCT_VERSION==1 || ACCT_VERSION==2
	{
		
		comp2_t etime = encode_comp2_t(elapsed);
		ac.ac_etime_hi = etime >> 16;
		ac.ac_etime_lo = (u16) etime;
	}
#endif
	do_div(elapsed, AHZ);
	ac.ac_btime = get_seconds() - elapsed;
	
	ac.ac_uid = orig_cred->uid;
	ac.ac_gid = orig_cred->gid;
#if ACCT_VERSION==2
	ac.ac_ahz = AHZ;
#endif
#if ACCT_VERSION==1 || ACCT_VERSION==2
	
	ac.ac_uid16 = ac.ac_uid;
	ac.ac_gid16 = ac.ac_gid;
#endif
#if ACCT_VERSION==3
	ac.ac_pid = task_tgid_nr_ns(current, ns);
	rcu_read_lock();
	ac.ac_ppid = task_tgid_nr_ns(rcu_dereference(current->real_parent), ns);
	rcu_read_unlock();
#endif

	spin_lock_irq(&current->sighand->siglock);
	tty = current->signal->tty;	
	ac.ac_tty = tty ? old_encode_dev(tty_devnum(tty)) : 0;
	ac.ac_utime = encode_comp_t(jiffies_to_AHZ(cputime_to_jiffies(pacct->ac_utime)));
	ac.ac_stime = encode_comp_t(jiffies_to_AHZ(cputime_to_jiffies(pacct->ac_stime)));
	ac.ac_flag = pacct->ac_flag;
	ac.ac_mem = encode_comp_t(pacct->ac_mem);
	ac.ac_minflt = encode_comp_t(pacct->ac_minflt);
	ac.ac_majflt = encode_comp_t(pacct->ac_majflt);
	ac.ac_exitcode = pacct->ac_exitcode;
	spin_unlock_irq(&current->sighand->siglock);
	ac.ac_io = encode_comp_t(0 );	
	ac.ac_rw = encode_comp_t(ac.ac_io / 1024);
	ac.ac_swaps = encode_comp_t(0);

	fs = get_fs();
	set_fs(KERNEL_DS);
	flim = current->signal->rlim[RLIMIT_FSIZE].rlim_cur;
	current->signal->rlim[RLIMIT_FSIZE].rlim_cur = RLIM_INFINITY;
	file->f_op->write(file, (char *)&ac,
			       sizeof(acct_t), &file->f_pos);
	current->signal->rlim[RLIMIT_FSIZE].rlim_cur = flim;
	set_fs(fs);
out:
	revert_creds(orig_cred);
}

void acct_collect(long exitcode, int group_dead)
{
	struct pacct_struct *pacct = &current->signal->pacct;
	unsigned long vsize = 0;

	if (group_dead && current->mm) {
		struct vm_area_struct *vma;
		down_read(&current->mm->mmap_sem);
		vma = current->mm->mmap;
		while (vma) {
			vsize += vma->vm_end - vma->vm_start;
			vma = vma->vm_next;
		}
		up_read(&current->mm->mmap_sem);
	}

	spin_lock_irq(&current->sighand->siglock);
	if (group_dead)
		pacct->ac_mem = vsize / 1024;
	if (thread_group_leader(current)) {
		pacct->ac_exitcode = exitcode;
		if (current->flags & PF_FORKNOEXEC)
			pacct->ac_flag |= AFORK;
	}
	if (current->flags & PF_SUPERPRIV)
		pacct->ac_flag |= ASU;
	if (current->flags & PF_DUMPCORE)
		pacct->ac_flag |= ACORE;
	if (current->flags & PF_SIGNALED)
		pacct->ac_flag |= AXSIG;
	pacct->ac_utime += current->utime;
	pacct->ac_stime += current->stime;
	pacct->ac_minflt += current->min_flt;
	pacct->ac_majflt += current->maj_flt;
	spin_unlock_irq(&current->sighand->siglock);
}

static void acct_process_in_ns(struct pid_namespace *ns)
{
	struct file *file = NULL;
	struct bsd_acct_struct *acct;

	acct = ns->bacct;
	if (!acct || !acct->file)
		return;

	spin_lock(&acct_lock);
	file = acct->file;
	if (unlikely(!file)) {
		spin_unlock(&acct_lock);
		return;
	}
	get_file(file);
	spin_unlock(&acct_lock);

	do_acct_process(acct, ns, file);
	fput(file);
}

void acct_process(void)
{
	struct pid_namespace *ns;

	for (ns = task_active_pid_ns(current); ns != NULL; ns = ns->parent)
		acct_process_in_ns(ns);
}
