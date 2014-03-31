/*
 *  include/asm-m68k/bugs.h
 *
 *  Copyright (C) 1994  Linus Torvalds
 */


#ifdef CONFIG_MMU
extern void check_bugs(void);	
#else
static void check_bugs(void)
{
}
#endif
