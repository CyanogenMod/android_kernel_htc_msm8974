/* 
 *    Static declaration of "init" task data structure.
 *
 *    Copyright (C) 2000 Paul Bame <bame at parisc-linux.org>
 *    Copyright (C) 2000-2001 John Marvin <jsm at parisc-linux.org>
 *    Copyright (C) 2001 Helge Deller <deller @ parisc-linux.org>
 *    Copyright (C) 2002 Matthew Wilcox <willy with parisc-linux.org>
 *
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/init_task.h>
#include <linux/mqueue.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>

static struct signal_struct init_signals = INIT_SIGNALS(init_signals);
static struct sighand_struct init_sighand = INIT_SIGHAND(init_sighand);
union thread_union init_thread_union __init_task_data
	__attribute__((aligned(128))) =
		{ INIT_THREAD_INFO(init_task) };

#if PT_NLEVELS == 3
pmd_t pmd0[PTRS_PER_PMD] __attribute__ ((__section__ (".data..vm0.pmd"), aligned(PAGE_SIZE)));
#endif

pgd_t swapper_pg_dir[PTRS_PER_PGD] __attribute__ ((__section__ (".data..vm0.pgd"), aligned(PAGE_SIZE)));
pte_t pg0[PT_INITIAL * PTRS_PER_PTE] __attribute__ ((__section__ (".data..vm0.pte"), aligned(PAGE_SIZE)));

EXPORT_SYMBOL(init_task);

__asm__(".data");
struct task_struct init_task = INIT_TASK(init_task);
