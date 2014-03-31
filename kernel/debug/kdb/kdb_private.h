#ifndef _KDBPRIVATE_H
#define _KDBPRIVATE_H

/*
 * Kernel Debugger Architecture Independent Private Headers
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Wind River Systems, Inc.  All Rights Reserved.
 */

#include <linux/kgdb.h>
#include "../debug_core.h"

#define KDB_CMD_GO	(-1001)
#define KDB_CMD_CPU	(-1002)
#define KDB_CMD_SS	(-1003)
#define KDB_CMD_SSB	(-1004)
#define KDB_CMD_KGDB (-1005)

#define KDB_DEBUG_FLAG_BP	0x0002	
#define KDB_DEBUG_FLAG_BB_SUMM	0x0004	
#define KDB_DEBUG_FLAG_AR	0x0008	
#define KDB_DEBUG_FLAG_ARA	0x0010	
#define KDB_DEBUG_FLAG_BB	0x0020	
#define KDB_DEBUG_FLAG_STATE	0x0040	
#define KDB_DEBUG_FLAG_MASK	0xffff	
#define KDB_DEBUG_FLAG_SHIFT	16	

#define KDB_DEBUG(flag)	(kdb_flags & \
	(KDB_DEBUG_FLAG_##flag << KDB_DEBUG_FLAG_SHIFT))
#define KDB_DEBUG_STATE(text, value) if (KDB_DEBUG(STATE)) \
		kdb_print_state(text, value)

#if BITS_PER_LONG == 32

#define KDB_PLATFORM_ENV	"BYTESPERWORD=4"

#define kdb_machreg_fmt		"0x%lx"
#define kdb_machreg_fmt0	"0x%08lx"
#define kdb_bfd_vma_fmt		"0x%lx"
#define kdb_bfd_vma_fmt0	"0x%08lx"
#define kdb_elfw_addr_fmt	"0x%x"
#define kdb_elfw_addr_fmt0	"0x%08x"
#define kdb_f_count_fmt		"%d"

#elif BITS_PER_LONG == 64

#define KDB_PLATFORM_ENV	"BYTESPERWORD=8"

#define kdb_machreg_fmt		"0x%lx"
#define kdb_machreg_fmt0	"0x%016lx"
#define kdb_bfd_vma_fmt		"0x%lx"
#define kdb_bfd_vma_fmt0	"0x%016lx"
#define kdb_elfw_addr_fmt	"0x%x"
#define kdb_elfw_addr_fmt0	"0x%016x"
#define kdb_f_count_fmt		"%ld"

#endif

#define KDB_MAXBPT	16

typedef struct __ksymtab {
		unsigned long value;	
		const char *mod_name;	
		unsigned long mod_start;
		unsigned long mod_end;
		const char *sec_name;	
		unsigned long sec_start;
		unsigned long sec_end;
		const char *sym_name;	
		unsigned long sym_start;
		unsigned long sym_end;
		} kdb_symtab_t;
extern int kallsyms_symbol_next(char *prefix_name, int flag);
extern int kallsyms_symbol_complete(char *prefix_name, int max_len);

extern int kdb_getarea_size(void *, unsigned long, size_t);
extern int kdb_putarea_size(unsigned long, void *, size_t);

#define kdb_getarea(x, addr) kdb_getarea_size(&(x), addr, sizeof((x)))
#define kdb_putarea(addr, x) kdb_putarea_size(addr, &(x), sizeof((x)))

extern int kdb_getphysword(unsigned long *word,
			unsigned long addr, size_t size);
extern int kdb_getword(unsigned long *, unsigned long, size_t);
extern int kdb_putword(unsigned long, unsigned long, size_t);

extern int kdbgetularg(const char *, unsigned long *);
extern int kdbgetu64arg(const char *, u64 *);
extern char *kdbgetenv(const char *);
extern int kdbgetaddrarg(int, const char **, int*, unsigned long *,
			 long *, char **);
extern int kdbgetsymval(const char *, kdb_symtab_t *);
extern int kdbnearsym(unsigned long, kdb_symtab_t *);
extern void kdbnearsym_cleanup(void);
extern char *kdb_strdup(const char *str, gfp_t type);
extern void kdb_symbol_print(unsigned long, const kdb_symtab_t *, unsigned int);

extern void kdb_print_state(const char *, int);

extern int kdb_state;
#define KDB_STATE_KDB		0x00000001	
#define KDB_STATE_LEAVING	0x00000002	
#define KDB_STATE_CMD		0x00000004	
#define KDB_STATE_KDB_CONTROL	0x00000008	
#define KDB_STATE_HOLD_CPU	0x00000010	
#define KDB_STATE_DOING_SS	0x00000020	
#define KDB_STATE_DOING_SSB	0x00000040	
#define KDB_STATE_SSBPT		0x00000080	
#define KDB_STATE_REENTRY	0x00000100	
#define KDB_STATE_SUPPRESS	0x00000200	
#define KDB_STATE_PAGER		0x00000400	
#define KDB_STATE_GO_SWITCH	0x00000800	
#define KDB_STATE_PRINTF_LOCK	0x00001000	
#define KDB_STATE_WAIT_IPI	0x00002000	
#define KDB_STATE_RECURSE	0x00004000	
#define KDB_STATE_IP_ADJUSTED	0x00008000	
#define KDB_STATE_GO1		0x00010000	
#define KDB_STATE_KEYBOARD	0x00020000	
#define KDB_STATE_KEXEC		0x00040000	
#define KDB_STATE_DOING_KGDB	0x00080000	
#define KDB_STATE_KGDB_TRANS	0x00200000	
#define KDB_STATE_ARCH		0xff000000	

#define KDB_STATE(flag) (kdb_state & KDB_STATE_##flag)
#define KDB_STATE_SET(flag) ((void)(kdb_state |= KDB_STATE_##flag))
#define KDB_STATE_CLEAR(flag) ((void)(kdb_state &= ~KDB_STATE_##flag))

extern int kdb_nextline; 

typedef struct _kdb_bp {
	unsigned long	bp_addr;	
	unsigned int	bp_free:1;	
	unsigned int	bp_enabled:1;	
	unsigned int	bp_type:4;	
	unsigned int	bp_installed:1;	
	unsigned int	bp_delay:1;	
	unsigned int	bp_delayed:1;	
	unsigned int	bph_length;	
} kdb_bp_t;

#ifdef CONFIG_KGDB_KDB
extern kdb_bp_t kdb_breakpoints[];

typedef struct _kdbtab {
	char    *cmd_name;		
	kdb_func_t cmd_func;		
	char    *cmd_usage;		
	char    *cmd_help;		
	short    cmd_flags;		
	short    cmd_minlen;		
	kdb_repeat_t cmd_repeat;	
} kdbtab_t;

extern int kdb_bt(int, const char **);	

extern void kdb_initbptab(void);
extern void kdb_bp_install(struct pt_regs *);
extern void kdb_bp_remove(void);

typedef enum {
	KDB_DB_BPT,	
	KDB_DB_SS,	
	KDB_DB_SSB,	
	KDB_DB_SSBPT,	
	KDB_DB_NOBPT	
} kdb_dbtrap_t;

extern int kdb_main_loop(kdb_reason_t, kdb_reason_t,
			 int, kdb_dbtrap_t, struct pt_regs *);

extern int kdb_grepping_flag;
extern char kdb_grep_string[];
extern int kdb_grep_leading;
extern int kdb_grep_trailing;
extern char *kdb_cmds[];
extern void kdb_syslog_data(char *syslog_data[]);
extern unsigned long kdb_task_state_string(const char *);
extern char kdb_task_state_char (const struct task_struct *);
extern unsigned long kdb_task_state(const struct task_struct *p,
				    unsigned long mask);
extern void kdb_ps_suppressed(void);
extern void kdb_ps1(const struct task_struct *p);
extern void kdb_print_nameval(const char *name, unsigned long val);
extern void kdb_send_sig_info(struct task_struct *p, struct siginfo *info);
extern void kdb_meminfo_proc_show(void);
extern char *kdb_getstr(char *, size_t, char *);
extern void kdb_gdb_state_pass(char *buf);

#define KDB_SP_SPACEB	0x0001		
#define KDB_SP_SPACEA	0x0002		
#define KDB_SP_PAREN	0x0004		
#define KDB_SP_VALUE	0x0008		
#define KDB_SP_SYMSIZE	0x0010		
#define KDB_SP_NEWLINE	0x0020		
#define KDB_SP_DEFAULT (KDB_SP_VALUE|KDB_SP_PAREN)

#define KDB_TSK(cpu) kgdb_info[cpu].task
#define KDB_TSKREGS(cpu) kgdb_info[cpu].debuggerinfo

extern struct task_struct *kdb_curr_task(int);

#define kdb_task_has_cpu(p) (task_curr(p))

#define	kdb_do_each_thread(g, p) do_each_thread(g, p)
#define	kdb_while_each_thread(g, p) while_each_thread(g, p)

#define GFP_KDB (in_interrupt() ? GFP_ATOMIC : GFP_KERNEL)

extern void *debug_kmalloc(size_t size, gfp_t flags);
extern void debug_kfree(void *);
extern void debug_kusage(void);

extern void kdb_set_current_task(struct task_struct *);
extern struct task_struct *kdb_current_task;

#ifdef CONFIG_KDB_KEYBOARD
extern void kdb_kbd_cleanup_state(void);
#else 
#define kdb_kbd_cleanup_state()
#endif 

#ifdef CONFIG_MODULES
extern struct list_head *kdb_modules;
#endif 

extern char kdb_prompt_str[];

#define	KDB_WORD_SIZE	((int)sizeof(unsigned long))

#endif 
#endif	
