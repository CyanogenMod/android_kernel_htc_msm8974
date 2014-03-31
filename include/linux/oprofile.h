/**
 * @file oprofile.h
 *
 * API for machine-specific interrupts to interface
 * to oprofile.
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <levon@movementarian.org>
 */

#ifndef OPROFILE_H
#define OPROFILE_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/atomic.h>
 
#define ESCAPE_CODE			~0UL
#define CTX_SWITCH_CODE			1
#define CPU_SWITCH_CODE			2
#define COOKIE_SWITCH_CODE		3
#define KERNEL_ENTER_SWITCH_CODE	4
#define KERNEL_EXIT_SWITCH_CODE		5
#define MODULE_LOADED_CODE		6
#define CTX_TGID_CODE			7
#define TRACE_BEGIN_CODE		8
#define TRACE_END_CODE			9
#define XEN_ENTER_SWITCH_CODE		10
#define SPU_PROFILING_CODE		11
#define SPU_CTX_SWITCH_CODE		12
#define IBS_FETCH_CODE			13
#define IBS_OP_CODE			14

struct super_block;
struct dentry;
struct file_operations;
struct pt_regs;
 
struct oprofile_operations {
	int (*create_files)(struct super_block * sb, struct dentry * root);
	
	int (*setup)(void);
	
	void (*shutdown)(void);
	
	int (*start)(void);
	
	void (*stop)(void);
	int (*sync_start)(void);
	int (*sync_stop)(void);

	
	void (*backtrace)(struct pt_regs * const regs, unsigned int depth);

	
	int (*switch_events)(void);
	
	char * cpu_type;
};

int oprofile_arch_init(struct oprofile_operations * ops);
 
void oprofile_arch_exit(void);

void oprofile_add_sample(struct pt_regs * const regs, unsigned long event);

void oprofile_add_ext_sample(unsigned long pc, struct pt_regs * const regs,
				unsigned long event, int is_kernel);

void oprofile_add_ext_hw_sample(unsigned long pc, struct pt_regs * const regs,
	unsigned long event, int is_kernel,
	struct task_struct *task);

void oprofile_add_pc(unsigned long pc, int is_kernel, unsigned long event);

void oprofile_add_trace(unsigned long eip);


int oprofilefs_create_file(struct super_block * sb, struct dentry * root,
	char const * name, const struct file_operations * fops);

int oprofilefs_create_file_perm(struct super_block * sb, struct dentry * root,
	char const * name, const struct file_operations * fops, int perm);
 
int oprofilefs_create_ulong(struct super_block * sb, struct dentry * root,
	char const * name, ulong * val);
 
int oprofilefs_create_ro_ulong(struct super_block * sb, struct dentry * root,
	char const * name, ulong * val);
 
int oprofilefs_create_ro_atomic(struct super_block * sb, struct dentry * root,
	char const * name, atomic_t * val);
 
struct dentry * oprofilefs_mkdir(struct super_block * sb, struct dentry * root,
	char const * name);

/**
 * Write the given asciz string to the given user buffer @buf, updating *offset
 * appropriately. Returns bytes written or -EFAULT.
 */
ssize_t oprofilefs_str_to_user(char const * str, char __user * buf, size_t count, loff_t * offset);

/**
 * Convert an unsigned long value into ASCII and copy it to the user buffer @buf,
 * updating *offset appropriately. Returns bytes written or -EFAULT.
 */
ssize_t oprofilefs_ulong_to_user(unsigned long val, char __user * buf, size_t count, loff_t * offset);

int oprofilefs_ulong_from_user(unsigned long * val, char const __user * buf, size_t count);

extern raw_spinlock_t oprofilefs_lock;

void oprofile_put_buff(unsigned long *buf, unsigned int start,
			unsigned int stop, unsigned int max);

unsigned long oprofile_get_cpu_buffer_size(void);
void oprofile_cpu_buffer_inc_smpl_lost(void);
 

struct op_sample;

struct op_entry {
	struct ring_buffer_event *event;
	struct op_sample *sample;
	unsigned long size;
	unsigned long *data;
};

void oprofile_write_reserve(struct op_entry *entry,
			    struct pt_regs * const regs,
			    unsigned long pc, int code, int size);
int oprofile_add_data(struct op_entry *entry, unsigned long val);
int oprofile_add_data64(struct op_entry *entry, u64 val);
int oprofile_write_commit(struct op_entry *entry);

#ifdef CONFIG_HW_PERF_EVENTS
int __init oprofile_perf_init(struct oprofile_operations *ops);
void oprofile_perf_exit(void);
char *op_name_from_perf_id(void);
#else
static inline int __init oprofile_perf_init(struct oprofile_operations *ops)
{
	pr_info("oprofile: hardware counters not available\n");
	return -ENODEV;
}
static inline void oprofile_perf_exit(void) { }
#endif 

#endif 
