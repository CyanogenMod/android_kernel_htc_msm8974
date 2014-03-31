/*
 * Access to user system call parameters and results
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.  All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v.2.
 *
 * This file is a stub providing documentation for what functions
 * asm-ARCH/syscall.h files need to define.  Most arch definitions
 * will be simple inlines.
 *
 * All of these functions expect to be called with no locks,
 * and only when the caller is sure that the task of interest
 * cannot return to user mode while we are looking at it.
 */

#ifndef _ASM_SYSCALL_H
#define _ASM_SYSCALL_H	1

struct task_struct;
struct pt_regs;

int syscall_get_nr(struct task_struct *task, struct pt_regs *regs);

void syscall_rollback(struct task_struct *task, struct pt_regs *regs);

long syscall_get_error(struct task_struct *task, struct pt_regs *regs);

long syscall_get_return_value(struct task_struct *task, struct pt_regs *regs);

void syscall_set_return_value(struct task_struct *task, struct pt_regs *regs,
			      int error, long val);

void syscall_get_arguments(struct task_struct *task, struct pt_regs *regs,
			   unsigned int i, unsigned int n, unsigned long *args);

void syscall_set_arguments(struct task_struct *task, struct pt_regs *regs,
			   unsigned int i, unsigned int n,
			   const unsigned long *args);

#endif	
