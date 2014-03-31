/*
 * Copyright (C) 2002 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#ifndef __OS_H__
#define __OS_H__

#include <stdarg.h>
#include "irq_user.h"
#include "longjmp.h"
#include "mm_id.h"

#define CATCH_EINTR(expr) while ((errno = 0, ((expr) < 0)) && (errno == EINTR))

#define OS_TYPE_FILE 1
#define OS_TYPE_DIR 2
#define OS_TYPE_SYMLINK 3
#define OS_TYPE_CHARDEV 4
#define OS_TYPE_BLOCKDEV 5
#define OS_TYPE_FIFO 6
#define OS_TYPE_SOCK 7

#define OS_ACC_F_OK    0       
#define OS_ACC_X_OK    1       
#define OS_ACC_W_OK    2       
#define OS_ACC_R_OK    4       
#define OS_ACC_RW_OK   (OS_ACC_W_OK | OS_ACC_R_OK) 

#ifdef CONFIG_64BIT
#define OS_LIB_PATH	"/usr/lib64/"
#else
#define OS_LIB_PATH	"/usr/lib/"
#endif

struct uml_stat {
	int                ust_dev;        
	unsigned long long ust_ino;        
	int                ust_mode;       
	int                ust_nlink;      
	int                ust_uid;        
	int                ust_gid;        
	unsigned long long ust_size;       
	int                ust_blksize;    
	unsigned long long ust_blocks;     
	unsigned long      ust_atime;      
	unsigned long      ust_mtime;      
	unsigned long      ust_ctime;      
};

struct openflags {
	unsigned int r : 1;
	unsigned int w : 1;
	unsigned int s : 1;	
	unsigned int c : 1;	
	unsigned int t : 1;	
	unsigned int a : 1;	
	unsigned int e : 1;	
	unsigned int cl : 1;    
};

#define OPENFLAGS() ((struct openflags) { .r = 0, .w = 0, .s = 0, .c = 0, \
					  .t = 0, .a = 0, .e = 0, .cl = 0 })

static inline struct openflags of_read(struct openflags flags)
{
	flags.r = 1;
	return flags;
}

static inline struct openflags of_write(struct openflags flags)
{
	flags.w = 1;
	return flags;
}

static inline struct openflags of_rdwr(struct openflags flags)
{
	return of_read(of_write(flags));
}

static inline struct openflags of_set_rw(struct openflags flags, int r, int w)
{
	flags.r = r;
	flags.w = w;
	return flags;
}

static inline struct openflags of_sync(struct openflags flags)
{
	flags.s = 1;
	return flags;
}

static inline struct openflags of_create(struct openflags flags)
{
	flags.c = 1;
	return flags;
}

static inline struct openflags of_trunc(struct openflags flags)
{
	flags.t = 1;
	return flags;
}

static inline struct openflags of_append(struct openflags flags)
{
	flags.a = 1;
	return flags;
}

static inline struct openflags of_excl(struct openflags flags)
{
	flags.e = 1;
	return flags;
}

static inline struct openflags of_cloexec(struct openflags flags)
{
	flags.cl = 1;
	return flags;
}

extern int os_stat_file(const char *file_name, struct uml_stat *buf);
extern int os_stat_fd(const int fd, struct uml_stat *buf);
extern int os_access(const char *file, int mode);
extern int os_set_exec_close(int fd);
extern int os_ioctl_generic(int fd, unsigned int cmd, unsigned long arg);
extern int os_get_ifname(int fd, char *namebuf);
extern int os_set_slip(int fd);
extern int os_mode_fd(int fd, int mode);

extern int os_seek_file(int fd, unsigned long long offset);
extern int os_open_file(const char *file, struct openflags flags, int mode);
extern int os_read_file(int fd, void *buf, int len);
extern int os_write_file(int fd, const void *buf, int count);
extern int os_file_size(const char *file, unsigned long long *size_out);
extern int os_file_modtime(const char *file, unsigned long *modtime);
extern int os_pipe(int *fd, int stream, int close_on_exec);
extern int os_set_fd_async(int fd);
extern int os_clear_fd_async(int fd);
extern int os_set_fd_block(int fd, int blocking);
extern int os_accept_connection(int fd);
extern int os_create_unix_socket(const char *file, int len, int close_on_exec);
extern int os_shutdown_socket(int fd, int r, int w);
extern void os_close_file(int fd);
extern int os_rcv_fd(int fd, int *helper_pid_out);
extern int create_unix_socket(char *file, int len, int close_on_exec);
extern int os_connect_socket(const char *name);
extern int os_file_type(char *file);
extern int os_file_mode(const char *file, struct openflags *mode_out);
extern int os_lock_file(int fd, int excl);
extern void os_flush_stdout(void);
extern int os_stat_filesystem(char *path, long *bsize_out,
			      long long *blocks_out, long long *bfree_out,
			      long long *bavail_out, long long *files_out,
			      long long *ffree_out, void *fsid_out,
			      int fsid_size, long *namelen_out,
			      long *spare_out);
extern int os_change_dir(char *dir);
extern int os_fchange_dir(int fd);
extern unsigned os_major(unsigned long long dev);
extern unsigned os_minor(unsigned long long dev);
extern unsigned long long os_makedev(unsigned major, unsigned minor);

extern void os_early_checks(void);
extern void can_do_skas(void);
extern void os_check_bugs(void);
extern void check_host_supports_tls(int *supports_tls, int *tls_min);

extern int create_mem_file(unsigned long long len);

extern unsigned long os_process_pc(int pid);
extern int os_process_parent(int pid);
extern void os_stop_process(int pid);
extern void os_kill_process(int pid, int reap_child);
extern void os_kill_ptraced_process(int pid, int reap_child);
extern long os_ptrace_ldt(long pid, long addr, long data);

extern int os_getpid(void);
extern int os_getpgrp(void);

extern void init_new_thread_signals(void);
extern int run_kernel_thread(int (*fn)(void *), void *arg, jmp_buf **jmp_ptr);

extern int os_map_memory(void *virt, int fd, unsigned long long off,
			 unsigned long len, int r, int w, int x);
extern int os_protect_memory(void *addr, unsigned long len,
			     int r, int w, int x);
extern int os_unmap_memory(void *addr, int len);
extern int os_drop_memory(void *addr, int length);
extern int can_drop_memory(void);
extern void os_flush_stdout(void);

extern int execvp_noalloc(char *buf, const char *file, char *const argv[]);
extern int run_helper(void (*pre_exec)(void *), void *pre_data, char **argv);
extern int run_helper_thread(int (*proc)(void *), void *arg,
			     unsigned int flags, unsigned long *stack_out);
extern int helper_wait(int pid);


extern int umid_file_name(char *name, char *buf, int len);
extern int set_umid(char *name);
extern char *get_umid(void);

extern void timer_init(void);
extern void set_sigstack(void *sig_stack, int size);
extern void remove_sigstack(void);
extern void set_handler(int sig);
extern int change_sig(int signal, int on);
extern void block_signals(void);
extern void unblock_signals(void);
extern int get_signals(void);
extern int set_signals(int enable);

extern void stack_protections(unsigned long address);
extern int raw(int fd);
extern void setup_machinename(char *machine_out);
extern void setup_hostinfo(char *buf, int len);
extern void os_dump_core(void) __attribute__ ((noreturn));
extern void um_early_printk(const char *s, unsigned int n);

extern void idle_sleep(unsigned long long nsecs);
extern int set_interval(void);
extern int timer_one_shot(int ticks);
extern long long disable_timer(void);
extern void uml_idle_timer(void);
extern long long os_nsecs(void);

extern long run_syscall_stub(struct mm_id * mm_idp,
			     int syscall, unsigned long *args, long expected,
			     void **addr, int done);
extern long syscall_stub_data(struct mm_id * mm_idp,
			      unsigned long *data, int data_count,
			      void **addr, void **stub_addr);
extern int map(struct mm_id * mm_idp, unsigned long virt,
	       unsigned long len, int prot, int phys_fd,
	       unsigned long long offset, int done, void **data);
extern int unmap(struct mm_id * mm_idp, unsigned long addr, unsigned long len,
		 int done, void **data);
extern int protect(struct mm_id * mm_idp, unsigned long addr,
		   unsigned long len, unsigned int prot, int done, void **data);

extern int is_skas_winch(int pid, int fd, void *data);
extern int start_userspace(unsigned long stub_stack);
extern int copy_context_skas0(unsigned long stack, int pid);
extern void userspace(struct uml_pt_regs *regs);
extern int map_stub_pages(int fd, unsigned long code, unsigned long data,
			  unsigned long stack);
extern void new_thread(void *stack, jmp_buf *buf, void (*handler)(void));
extern void switch_threads(jmp_buf *me, jmp_buf *you);
extern int start_idle_thread(void *stack, jmp_buf *switch_buf);
extern void initial_thread_cb_skas(void (*proc)(void *),
				 void *arg);
extern void halt_skas(void);
extern void reboot_skas(void);

extern int os_waiting_for_events(struct irq_fd *active_fds);
extern int os_create_pollfd(int fd, int events, void *tmp_pfd, int size_tmpfds);
extern void os_free_irq_by_cb(int (*test)(struct irq_fd *, void *), void *arg,
		struct irq_fd *active_fds, struct irq_fd ***last_irq_ptr2);
extern void os_free_irq_later(struct irq_fd *active_fds,
		int irq, void *dev_id);
extern int os_get_pollfd(int i);
extern void os_set_pollfd(int i, int fd);
extern void os_set_ioignore(void);

extern int add_sigio_fd(int fd);
extern int ignore_sigio_fd(int fd);
extern void maybe_sigio_broken(int fd, int read);
extern void sigio_broken(int fd, int read);

extern int os_arch_prctl(int pid, int code, unsigned long *addr);

extern int get_pty(void);

extern unsigned long os_get_top_address(void);

#endif
