/*
 *    Implements HPUX syscalls.
 *
 *    Copyright (C) 1999 Matthew Wilcox <willy with parisc-linux.org>
 *    Copyright (C) 2000 Philipp Rumpf
 *    Copyright (C) 2000 John Marvin <jsm with parisc-linux.org>
 *    Copyright (C) 2000 Michael Ang <mang with subcarrier.org>
 *    Copyright (C) 2001 Nathan Neulinger <nneul at umr.edu>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/capability.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/utsname.h>
#include <linux/vfs.h>
#include <linux/vmalloc.h>

#include <asm/errno.h>
#include <asm/pgalloc.h>
#include <asm/uaccess.h>

unsigned long hpux_brk(unsigned long addr)
{
	
	return sys_brk(addr + PAGE_SIZE);
}

int hpux_sbrk(void)
{
	return -ENOSYS;
}


int hpux_nice(int priority_change)
{
	return -ENOSYS;
}

int hpux_ptrace(void)
{
	return -ENOSYS;
}

int hpux_wait(int __user *stat_loc)
{
	return sys_waitpid(-1, stat_loc, 0);
}

int hpux_setpgrp(void)
{
	return sys_setpgid(0,0);
}

int hpux_setpgrp3(void)
{
	return hpux_setpgrp();
}

#define _SC_CPU_VERSION	10001
#define _SC_OPEN_MAX	4
#define CPU_PA_RISC1_1	0x210

int hpux_sysconf(int which)
{
	switch (which) {
	case _SC_CPU_VERSION:
		return CPU_PA_RISC1_1;
	case _SC_OPEN_MAX:
		return INT_MAX;
	default:
		return -EINVAL;
	}
}


#define HPUX_UTSLEN 9
#define HPUX_SNLEN 15

struct hpux_utsname {
	char sysname[HPUX_UTSLEN];
	char nodename[HPUX_UTSLEN];
	char release[HPUX_UTSLEN];
	char version[HPUX_UTSLEN];
	char machine[HPUX_UTSLEN];
	char idnumber[HPUX_SNLEN];
} ;

struct hpux_ustat {
	int32_t		f_tfree;	
	u_int32_t	f_tinode;	
	char		f_fname[6];	
	char		f_fpack[6];	
	u_int32_t	f_blksize;	
};


/*  This function is called from hpux_utssys(); HP-UX implements
 *  ustat() as an option to utssys().
 *
 *  Now, struct ustat on HP-UX is exactly the same as on Linux, except
 *  that it contains one addition field on the end, int32_t f_blksize.
 *  So, we could have written this function to just call the Linux
 *  sys_ustat(), (defined in linux/fs/super.c), and then just
 *  added this additional field to the user's structure.  But I figure
 *  if we're gonna be digging through filesystem structures to get
 *  this, we might as well just do the whole enchilada all in one go.
 *
 *  So, most of this function is almost identical to sys_ustat().
 *  I have placed comments at the few lines changed or added, to
 *  aid in porting forward if and when sys_ustat() is changed from
 *  its form in kernel 2.2.5.
 */
static int hpux_ustat(dev_t dev, struct hpux_ustat __user *ubuf)
{
	struct hpux_ustat tmp;  
	struct kstatfs sbuf;
	int err = vfs_ustat(dev, &sbuf);
	if (err)
		goto out;

	memset(&tmp,0,sizeof(tmp));

	tmp.f_tfree = (int32_t)sbuf.f_bfree;
	tmp.f_tinode = (u_int32_t)sbuf.f_ffree;
	tmp.f_blksize = (u_int32_t)sbuf.f_bsize;  

	err = copy_to_user(ubuf, &tmp, sizeof(tmp)) ? -EFAULT : 0;
out:
	return err;
}


typedef int32_t hpux_fsid_t[2];              
typedef uint16_t hpux_site_t;

struct hpux_statfs {
     int32_t f_type;                    
     int32_t f_bsize;                   
     int32_t f_blocks;                  
     int32_t f_bfree;                   
     int32_t f_bavail;                  
     int32_t f_files;                   
     int32_t f_ffree;                   
     hpux_fsid_t  f_fsid;                    
     int32_t f_magic;                   
     int32_t f_featurebits;             
     int32_t f_spare[4];                
     hpux_site_t  f_cnode;                   
     int16_t f_pad;
};

static int do_statfs_hpux(struct kstatfs *st, struct hpux_statfs __user *p)
{
	struct hpux_statfs buf;
	memset(&buf, 0, sizeof(buf));
	buf.f_type = st->f_type;
	buf.f_bsize = st->f_bsize;
	buf.f_blocks = st->f_blocks;
	buf.f_bfree = st->f_bfree;
	buf.f_bavail = st->f_bavail;
	buf.f_files = st->f_files;
	buf.f_ffree = st->f_ffree;
	buf.f_fsid[0] = st->f_fsid.val[0];
	buf.f_fsid[1] = st->f_fsid.val[1];
	if (copy_to_user(p, &buf, sizeof(buf)))
		return -EFAULT;
	return 0;
}

asmlinkage long hpux_statfs(const char __user *pathname,
						struct hpux_statfs __user *buf)
{
	struct kstatfs st;
	int error = user_statfs(pathname, &st);
	if (!error)
		error = do_statfs_hpux(&st, buf);
	return error;
}

asmlinkage long hpux_fstatfs(unsigned int fd, struct hpux_statfs __user * buf)
{
	struct kstatfs st;
	int error = fd_statfs(fd, &st);
	if (!error)
		error = do_statfs_hpux(&st, buf);
	return error;
}


static int hpux_uname(struct hpux_utsname __user *name)
{
	int error;

	if (!name)
		return -EFAULT;
	if (!access_ok(VERIFY_WRITE,name,sizeof(struct hpux_utsname)))
		return -EFAULT;

	down_read(&uts_sem);

	error = __copy_to_user(&name->sysname, &utsname()->sysname,
			       HPUX_UTSLEN - 1);
	error |= __put_user(0, name->sysname + HPUX_UTSLEN - 1);
	error |= __copy_to_user(&name->nodename, &utsname()->nodename,
				HPUX_UTSLEN - 1);
	error |= __put_user(0, name->nodename + HPUX_UTSLEN - 1);
	error |= __copy_to_user(&name->release, &utsname()->release,
				HPUX_UTSLEN - 1);
	error |= __put_user(0, name->release + HPUX_UTSLEN - 1);
	error |= __copy_to_user(&name->version, &utsname()->version,
				HPUX_UTSLEN - 1);
	error |= __put_user(0, name->version + HPUX_UTSLEN - 1);
	error |= __copy_to_user(&name->machine, &utsname()->machine,
				HPUX_UTSLEN - 1);
	error |= __put_user(0, name->machine + HPUX_UTSLEN - 1);

	up_read(&uts_sem);

	

	
#if 0
	error |= __put_user(0,name->idnumber);
	error |= __put_user(0,name->idnumber+HPUX_SNLEN-1);
#endif

	error = error ? -EFAULT : 0;

	return error;
}

int hpux_utssys(char __user *ubuf, int n, int type)
{
	int len;
	int error;
	switch( type ) {
	case 0:
		
		return hpux_uname((struct hpux_utsname __user *)ubuf);
		break ;
	case 1:
		
		return -EFAULT ;
		break ;
	case 2:
		
		return hpux_ustat(new_decode_dev(n),
				  (struct hpux_ustat __user *)ubuf);
		break;
	case 3:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		
		if ( n <= 0 )
			return -EINVAL ;
		
		len = (n <= __NEW_UTS_LEN) ? n : __NEW_UTS_LEN ;
		return sys_sethostname(ubuf, len);
		break ;
	case 4:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		
		if ( n <= 0 )
			return -EINVAL ;
		
		len = (n <= __NEW_UTS_LEN) ? n : __NEW_UTS_LEN ;
		return sys_sethostname(ubuf, len);
		break ;
	case 5:
		
		if ( n <= 0 )
			return -EINVAL ;
		return sys_gethostname(ubuf, n);
		break ;
	case 6:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		
		if ( n <= 0 )
			return -EINVAL ;
		
		len = (n <= __NEW_UTS_LEN) ? n : __NEW_UTS_LEN ;
		
		
		down_write(&uts_sem);
		error = -EFAULT;
		if (!copy_from_user(utsname()->sysname, ubuf, len)) {
			utsname()->sysname[len] = 0;
			error = 0;
		}
		up_write(&uts_sem);
		return error;
		break ;
	case 7:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		
		if ( n <= 0 )
			return -EINVAL ;
		
		len = (n <= __NEW_UTS_LEN) ? n : __NEW_UTS_LEN ;
		
		
		down_write(&uts_sem);
		error = -EFAULT;
		if (!copy_from_user(utsname()->release, ubuf, len)) {
			utsname()->release[len] = 0;
			error = 0;
		}
		up_write(&uts_sem);
		return error;
		break ;
	default:
		return -EFAULT ;
	}
}

int hpux_getdomainname(char __user *name, int len)
{
 	int nlen;
 	int err = -EFAULT;
 	
 	down_read(&uts_sem);
 	
	nlen = strlen(utsname()->domainname) + 1;

	if (nlen < len)
		len = nlen;
	if(len > __NEW_UTS_LEN)
		goto done;
	if(copy_to_user(name, utsname()->domainname, len))
		goto done;
	err = 0;
done:
	up_read(&uts_sem);
	return err;
	
}

int hpux_pipe(int *kstack_fildes)
{
	return do_pipe_flags(kstack_fildes, 0);
}

int hpux_lockf(int fildes, int function, off_t size)
{
	return 0;
}

int hpux_sysfs(int opcode, unsigned long arg1, unsigned long arg2)
{
	char *fsname = NULL;
	int len = 0;
	int fstype;

	printk(KERN_DEBUG "in hpux_sysfs\n");
	printk(KERN_DEBUG "hpux_sysfs called with opcode = %d\n", opcode);
	printk(KERN_DEBUG "hpux_sysfs called with arg1='%lx'\n", arg1);

	if ( opcode == 1 ) { 	
		char __user *user_fsname = (char __user *)arg1;
		len = strlen_user(user_fsname);
		printk(KERN_DEBUG "len of arg1 = %d\n", len);
		if (len == 0)
			return 0;
		fsname = kmalloc(len, GFP_KERNEL);
		if (!fsname) {
			printk(KERN_DEBUG "failed to kmalloc fsname\n");
			return 0;
		}

		if (copy_from_user(fsname, user_fsname, len)) {
			printk(KERN_DEBUG "failed to copy_from_user fsname\n");
			kfree(fsname);
			return 0;
		}

		
		fsname[len] = '\0';

		printk(KERN_DEBUG "that is '%s' as (char *)\n", fsname);
		if ( !strcmp(fsname, "hfs") ) {
			fstype = 0;
		} else {
			fstype = 0;
		}

		kfree(fsname);

		printk(KERN_DEBUG "returning fstype=%d\n", fstype);
		return fstype; 
	}


	return 0;
}


static const char * const syscall_names[] = {
	"nosys",                  
	"exit",                  
	"fork",                  
	"read",                  
	"write",                 
	"open",                   
	"close",                 
	"wait",                  
	"creat",                 
	"link",                  
	"unlink",                 
	"execv",                 
	"chdir",                 
	"time",                  
	"mknod",                 
	"chmod",                  
	"chown",                 
	"brk",                   
	"lchmod",                
	"lseek",                 
	"getpid",                 
	"mount",                 
	"umount",                
	"setuid",                
	"getuid",                
	"stime",                  
	"ptrace",                
	"alarm",                 
	NULL,                    
	"pause",                 
	"utime",                  
	"stty",                  
	"gtty",                  
	"access",                
	"nice",                  
	"ftime",                  
	"sync",                  
	"kill",                  
	"stat",                  
	"setpgrp3",              
	"lstat",                  
	"dup",                   
	"pipe",                  
	"times",                 
	"profil",                
	"ki_call",                
	"setgid",                
	"getgid",                
	NULL,                    
	NULL,                    
	NULL,                     
	"acct",                  
	"set_userthreadid",      
	NULL,                    
	"ioctl",                 
	"reboot",                 
	"symlink",               
	"utssys",                
	"readlink",              
	"execve",                
	"umask",                  
	"chroot",                
	"fcntl",                 
	"ulimit",                
	NULL,                    
	NULL,                     
	"vfork",                 
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                     
	"mmap",                  
	NULL,                    
	"munmap",                
	"mprotect",              
	"madvise",                
	"vhangup",               
	"swapoff",               
	NULL,                    
	"getgroups",             
	"setgroups",              
	"getpgrp2",              
	"setpgid/setpgrp2",      
	"setitimer",             
	"wait3",                 
	"swapon",                 
	"getitimer",             
	NULL,                    
	NULL,                    
	NULL,                    
	"dup2",                   
	NULL,                    
	"fstat",                 
	"select",                
	NULL,                    
	"fsync",                  
	"setpriority",           
	NULL,                    
	NULL,                    
	NULL,                    
	"getpriority",            
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                     
	NULL,                    
	NULL,                    
	"sigvector",             
	"sigblock",              
	"sigsetmask",             
	"sigpause",              
	"sigstack",              
	NULL,                    
	NULL,                    
	NULL,                     
	"gettimeofday",          
	"getrusage",             
	NULL,                    
	NULL,                    
	"readv",                  
	"writev",                
	"settimeofday",          
	"fchown",                
	"fchmod",                
	NULL,                     
	"setresuid",             
	"setresgid",             
	"rename",                
	"truncate",              
	"ftruncate",              
	NULL,                    
	"sysconf",               
	NULL,                    
	NULL,                    
	NULL,                     
	"mkdir",                 
	"rmdir",                 
	NULL,                    
	"sigcleanup",            
	"setcore",                
	NULL,                    
	"gethostid",             
	"sethostid",             
	"getrlimit",             
	"setrlimit",              
	NULL,                    
	NULL,                    
	"quotactl",              
	"get_sysinfo",           
	NULL,                     
	"privgrp",               
	"rtprio",                
	"plock",                 
	NULL,                    
	"lockf",                  
	"semget",                
	NULL,                    
	"semop",                 
	"msgget",                
	NULL,                     
	"msgsnd",                
	"msgrcv",                
	"shmget",                
	NULL,                    
	"shmat",                  
	"shmdt",                 
	NULL,                    
	"csp/nsp_init",          
	"cluster",               
	"mkrnod",                 
	"test",                  
	"unsp_open",             
	NULL,                    
	"getcontext",            
	"osetcontext",            
	"bigio",                 
	"pipenode",              
	"lsync",                 
	"getmachineid",          
	"cnodeid/mysite",         
	"cnodes/sitels",         
	"swapclients",           
	"rmtprocess",            
	"dskless_stats",         
	"sigprocmask",            
	"sigpending",            
	"sigsuspend",            
	"sigaction",             
	NULL,                    
	"nfssvc",                 
	"getfh",                 
	"getdomainname",         
	"setdomainname",         
	"async_daemon",          
	"getdirentries",          
	NULL,                
	NULL,               
	"vfsmount",              
	NULL,                    
	"waitpid",                
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                     
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                     
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                     
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                     
	NULL,                    
	NULL,                    
	NULL,                    
	"sigsetreturn",          
	"sigsetstatemask",        
	"bfactl",                
	"cs",                    
	"cds",                   
	NULL,                    
	"pathconf",               
	"fpathconf",             
	NULL,                    
	NULL,                    
	"nfs_fcntl",             
	"ogetacl",                
	"ofgetacl",              
	"osetacl",               
	"ofsetacl",              
	"pstat",                 
	"getaudid",               
	"setaudid",              
	"getaudproc",            
	"setaudproc",            
	"getevent",              
	"setevent",               
	"audwrite",              
	"audswitch",             
	"audctl",                
	"ogetaccess",            
	"fsctl",                  
	"ulconnect",             
	"ulcontrol",             
	"ulcreate",              
	"uldest",                
	"ulrecv",                 
	"ulrecvcn",              
	"ulsend",                
	"ulshutdown",            
	"swapfs",                
	"fss",                    
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                     
	NULL,                    
	"tsync",                 
	"getnumfds",             
	"poll",                  
	"getmsg",                 
	"putmsg",                
	"fchdir",                
	"getmount_cnt",          
	"getmount_entry",        
	"accept",                 
	"bind",                  
	"connect",               
	"getpeername",           
	"getsockname",           
	"getsockopt",             
	"listen",                
	"recv",                  
	"recvfrom",              
	"recvmsg",               
	"send",                   
	"sendmsg",               
	"sendto",                
	"setsockopt",            
	"shutdown",              
	"socket",                 
	"socketpair",            
	"proc_open",             
	"proc_close",            
	"proc_send",             
	"proc_recv",              
	"proc_sendrecv",         
	"proc_syscall",          
	"ipccreate",             
	"ipcname",               
	"ipcnamerase",            
	"ipclookup",             
	"ipcselect",             
	"ipcconnect",            
	"ipcrecvcn",             
	"ipcsend",                
	"ipcrecv",               
	"ipcgetnodename",        
	"ipcsetnodename",        
	"ipccontrol",            
	"ipcshutdown",            
	"ipcdest",               
	"semctl",                
	"msgctl",                
	"shmctl",                
	"mpctl",                  
	"exportfs",              
	"getpmsg",               
	"putpmsg",               
	"strioctl",              
	"msync",                  
	"msleep",                
	"mwakeup",               
	"msem_init",             
	"msem_remove",           
	"adjtime",                
	"kload",                 
	"fattach",               
	"fdetach",               
	"serialize",             
	"statvfs",                
	"fstatvfs",              
	"lchown",                
	"getsid",                
	"sysfs",                 
	NULL,                     
	NULL,                    
	"sched_setparam",        
	"sched_getparam",        
	"sched_setscheduler",    
	"sched_getscheduler",     
	"sched_yield",           
	"sched_get_priority_max",
	"sched_get_priority_min",
	"sched_rr_get_interval", 
	"clock_settime",          
	"clock_gettime",         
	"clock_getres",          
	"timer_create",          
	"timer_delete",          
	"timer_settime",          
	"timer_gettime",         
	"timer_getoverrun",      
	"nanosleep",             
	"toolbox",               
	NULL,                     
	"getdents",              
	"getcontext",            
	"sysinfo",               
	"fcntl64",               
	"ftruncate64",            
	"fstat64",               
	"getdirentries64",       
	"getrlimit64",           
	"lockf64",               
	"lseek64",                
	"lstat64",               
	"mmap64",                
	"setrlimit64",           
	"stat64",                
	"truncate64",             
	"ulimit64",              
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                     
	NULL,                    
	NULL,                    
	NULL,                    
	NULL,                    
	"setcontext",             
	"sigaltstack",           
	"waitid",                
	"setpgrp",               
	"recvmsg2",              
	"sendmsg2",               
	"socket2",               
	"socketpair2",           
	"setregid",              
	"lwp_create",            
	"lwp_terminate",          
	"lwp_wait",              
	"lwp_suspend",           
	"lwp_resume",            
	"lwp_self",              
	"lwp_abort_syscall",      
	"lwp_info",              
	"lwp_kill",              
	"ksleep",                
	"kwakeup",               
	"ksleep_abort",           
	"lwp_proc_info",         
	"lwp_exit",              
	"lwp_continue",          
	"getacl",                
	"fgetacl",                
	"setacl",                
	"fsetacl",               
	"getaccess",             
	"lwp_mutex_init",        
	"lwp_mutex_lock_sys",     
	"lwp_mutex_unlock",      
	"lwp_cond_init",         
	"lwp_cond_signal",       
	"lwp_cond_broadcast",    
	"lwp_cond_wait_sys",      
	"lwp_getscheduler",      
	"lwp_setscheduler",      
	"lwp_getprivate",        
	"lwp_setprivate",        
	"lwp_detach",             
	"mlock",                 
	"munlock",               
	"mlockall",              
	"munlockall",            
	"shm_open",               
	"shm_unlink",            
	"sigqueue",              
	"sigwaitinfo",           
	"sigtimedwait",          
	"sigwait",                
	"aio_read",              
	"aio_write",             
	"lio_listio",            
	"aio_error",             
	"aio_return",             
	"aio_cancel",            
	"aio_suspend",           
	"aio_fsync",             
	"mq_open",               
	"mq_unlink",              
	"mq_send",               
	"mq_receive",            
	"mq_notify",             
	"mq_setattr",            
	"mq_getattr",             
	"ksem_open",             
	"ksem_unlink",           
	"ksem_close",            
	"ksem_destroy",          
	"lw_sem_incr",            
	"lw_sem_decr",           
	"lw_sem_read",           
	"mq_close",              
};
static const int syscall_names_max = 453;

int
hpux_unimplemented(unsigned long arg1,unsigned long arg2,unsigned long arg3,
		   unsigned long arg4,unsigned long arg5,unsigned long arg6,
		   unsigned long arg7,unsigned long sc_num)
{
	const char *name = NULL;
	if ( sc_num <= syscall_names_max && sc_num >= 0 ) {
		name = syscall_names[sc_num];
	}

	if ( name ) {
		printk(KERN_DEBUG "Unimplemented HP-UX syscall emulation. Syscall #%lu (%s)\n",
		sc_num, name);
	} else {
		printk(KERN_DEBUG "Unimplemented unknown HP-UX syscall emulation. Syscall #%lu\n",
		sc_num);
	}
	
	printk(KERN_DEBUG "  Args: %lx %lx %lx %lx %lx %lx %lx\n",
		arg1, arg2, arg3, arg4, arg5, arg6, arg7);

	return -ENOSYS;
}
