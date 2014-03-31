#ifndef __SPARC_MMU_CONTEXT_H
#define __SPARC_MMU_CONTEXT_H

#include <asm/btfixup.h>

#ifndef __ASSEMBLY__

#include <asm-generic/mm_hooks.h>

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
}

#define init_new_context(tsk, mm) (((mm)->context = NO_CONTEXT), 0)

BTFIXUPDEF_CALL(void, destroy_context, struct mm_struct *)

#define destroy_context(mm) BTFIXUP_CALL(destroy_context)(mm)

BTFIXUPDEF_CALL(void, switch_mm, struct mm_struct *, struct mm_struct *, struct task_struct *)

#define switch_mm(old_mm, mm, tsk) BTFIXUP_CALL(switch_mm)(old_mm, mm, tsk)

#define deactivate_mm(tsk,mm)	do { } while (0)

#define activate_mm(active_mm, mm) switch_mm((active_mm), (mm), NULL)

#endif 

#endif 
