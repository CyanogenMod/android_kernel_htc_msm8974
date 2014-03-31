#ifndef _UNWIND_H_
#define _UNWIND_H_

#include <linux/list.h>

struct unwind_table_entry {
	unsigned int region_start;
	unsigned int region_end;
	unsigned int Cannot_unwind:1; 
	unsigned int Millicode:1;	
	unsigned int Millicode_save_sr0:1;	
	unsigned int Region_description:2;	
	unsigned int reserved1:1;	
	unsigned int Entry_SR:1;	
	unsigned int Entry_FR:4;	
	unsigned int Entry_GR:5;	
	unsigned int Args_stored:1;	
	unsigned int Variable_Frame:1;	
	unsigned int Separate_Package_Body:1;	
	unsigned int Frame_Extension_Millicode:1;	
	unsigned int Stack_Overflow_Check:1;	
	unsigned int Two_Instruction_SP_Increment:1;	
	unsigned int Ada_Region:1;	
	unsigned int cxx_info:1;	
	unsigned int cxx_try_catch:1;	
	unsigned int sched_entry_seq:1;	
	unsigned int reserved2:1;	
	unsigned int Save_SP:1;	
	unsigned int Save_RP:1;	
	unsigned int Save_MRP_in_frame:1;	
	unsigned int extn_ptr_defined:1;	
	unsigned int Cleanup_defined:1;	
	
	unsigned int MPE_XL_interrupt_marker:1;	
	unsigned int HP_UX_interrupt_marker:1;	
	unsigned int Large_frame:1;	
	unsigned int Pseudo_SP_Set:1;	
	unsigned int reserved4:1;	
	unsigned int Total_frame_size:27;	
};

struct unwind_table {
	struct list_head list;
	const char *name;
	unsigned long gp;
	unsigned long base_addr;
	unsigned long start;
	unsigned long end;
	const struct unwind_table_entry *table;
	unsigned long length;
};

struct unwind_frame_info {
	struct task_struct *t;
	
	unsigned long sp, ip, rp, r31;
	unsigned long prev_sp, prev_ip;
};

struct unwind_table *
unwind_table_add(const char *name, unsigned long base_addr, 
		 unsigned long gp, void *start, void *end);
void
unwind_table_remove(struct unwind_table *table);

void unwind_frame_init(struct unwind_frame_info *info, struct task_struct *t, 
		       struct pt_regs *regs);
void unwind_frame_init_from_blocked_task(struct unwind_frame_info *info, struct task_struct *t);
void unwind_frame_init_running(struct unwind_frame_info *info, struct pt_regs *regs);
int unwind_once(struct unwind_frame_info *info);
int unwind_to_user(struct unwind_frame_info *info);

int unwind_init(void);

#endif
