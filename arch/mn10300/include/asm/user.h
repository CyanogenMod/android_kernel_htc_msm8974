/* MN10300 User process data
 *
 * Copyright (C) 2007 Matsushita Electric Industrial Co., Ltd.
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef _ASM_USER_H
#define _ASM_USER_H

#include <asm/page.h>
#include <linux/ptrace.h>

#ifndef __ASSEMBLY__
struct user {
	struct pt_regs regs;		

	
	unsigned long int u_tsize;	
	unsigned long int u_dsize;	
	unsigned long int u_ssize;	
	unsigned long start_code;	
	unsigned long start_stack;	
	long int signal;		
	int reserved;			
	struct user_pt_regs *u_ar0;	

	
	unsigned long magic;		
	char u_comm[32];		
};
#endif

#define NBPG PAGE_SIZE
#define UPAGES 1
#define HOST_TEXT_START_ADDR	+(u.start_code)
#define HOST_STACK_END_ADDR	+(u.start_stack + u.u_ssize * NBPG)

#endif 
