#ifndef __ASM_CRIS_SWITCH_TO_H
#define __ASM_CRIS_SWITCH_TO_H


extern struct task_struct *resume(struct task_struct *prev, struct task_struct *next, int);
#define switch_to(prev,next,last) last = resume(prev,next, \
					 (int)&((struct task_struct *)0)->thread)

#endif 
