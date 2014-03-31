#ifndef _ASM_X86_DEBUGREG_H
#define _ASM_X86_DEBUGREG_H


#define DR_FIRSTADDR 0        
#define DR_LASTADDR 3         

#define DR_STATUS 6           
#define DR_CONTROL 7          


#define DR6_RESERVED	(0xFFFF0FF0)

#define DR_TRAP0	(0x1)		
#define DR_TRAP1	(0x2)		
#define DR_TRAP2	(0x4)		
#define DR_TRAP3	(0x8)		
#define DR_TRAP_BITS	(DR_TRAP0|DR_TRAP1|DR_TRAP2|DR_TRAP3)

#define DR_STEP		(0x4000)	
#define DR_SWITCH	(0x8000)	


#define DR_CONTROL_SHIFT 16 
#define DR_CONTROL_SIZE 4   

#define DR_RW_EXECUTE (0x0)   
#define DR_RW_WRITE (0x1)
#define DR_RW_READ (0x3)

#define DR_LEN_1 (0x0) 
#define DR_LEN_2 (0x4)
#define DR_LEN_4 (0xC)
#define DR_LEN_8 (0x8)


#define DR_LOCAL_ENABLE_SHIFT 0    
#define DR_GLOBAL_ENABLE_SHIFT 1   
#define DR_LOCAL_ENABLE (0x1)      
#define DR_GLOBAL_ENABLE (0x2)     
#define DR_ENABLE_SIZE 2           

#define DR_LOCAL_ENABLE_MASK (0x55)  
#define DR_GLOBAL_ENABLE_MASK (0xAA) 


#ifdef __i386__
#define DR_CONTROL_RESERVED (0xFC00) 
#else
#define DR_CONTROL_RESERVED (0xFFFFFFFF0000FC00UL) 
#endif

#define DR_LOCAL_SLOWDOWN (0x100)   
#define DR_GLOBAL_SLOWDOWN (0x200)  

#ifdef __KERNEL__

#include <linux/bug.h>

DECLARE_PER_CPU(unsigned long, cpu_dr7);

#ifndef CONFIG_PARAVIRT
#define get_debugreg(var, register)				\
	(var) = native_get_debugreg(register)
#define set_debugreg(value, register)				\
	native_set_debugreg(register, value)
#endif

static inline unsigned long native_get_debugreg(int regno)
{
	unsigned long val = 0;	

	switch (regno) {
	case 0:
		asm("mov %%db0, %0" :"=r" (val));
		break;
	case 1:
		asm("mov %%db1, %0" :"=r" (val));
		break;
	case 2:
		asm("mov %%db2, %0" :"=r" (val));
		break;
	case 3:
		asm("mov %%db3, %0" :"=r" (val));
		break;
	case 6:
		asm("mov %%db6, %0" :"=r" (val));
		break;
	case 7:
		asm("mov %%db7, %0" :"=r" (val));
		break;
	default:
		BUG();
	}
	return val;
}

static inline void native_set_debugreg(int regno, unsigned long value)
{
	switch (regno) {
	case 0:
		asm("mov %0, %%db0"	::"r" (value));
		break;
	case 1:
		asm("mov %0, %%db1"	::"r" (value));
		break;
	case 2:
		asm("mov %0, %%db2"	::"r" (value));
		break;
	case 3:
		asm("mov %0, %%db3"	::"r" (value));
		break;
	case 6:
		asm("mov %0, %%db6"	::"r" (value));
		break;
	case 7:
		asm("mov %0, %%db7"	::"r" (value));
		break;
	default:
		BUG();
	}
}

static inline void hw_breakpoint_disable(void)
{
	
	set_debugreg(0UL, 7);

	
	set_debugreg(0UL, 0);
	set_debugreg(0UL, 1);
	set_debugreg(0UL, 2);
	set_debugreg(0UL, 3);
}

static inline int hw_breakpoint_active(void)
{
	return __this_cpu_read(cpu_dr7) & DR_GLOBAL_ENABLE_MASK;
}

extern void aout_dump_debugregs(struct user *dump);

extern void hw_breakpoint_restore(void);

#ifdef CONFIG_X86_64
DECLARE_PER_CPU(int, debug_stack_usage);
static inline void debug_stack_usage_inc(void)
{
	__get_cpu_var(debug_stack_usage)++;
}
static inline void debug_stack_usage_dec(void)
{
	__get_cpu_var(debug_stack_usage)--;
}
int is_debug_stack(unsigned long addr);
void debug_stack_set_zero(void);
void debug_stack_reset(void);
#else 
static inline int is_debug_stack(unsigned long addr) { return 0; }
static inline void debug_stack_set_zero(void) { }
static inline void debug_stack_reset(void) { }
static inline void debug_stack_usage_inc(void) { }
static inline void debug_stack_usage_dec(void) { }
#endif 


#endif	

#endif 
