/*
 * Routines for doing kexec-based kdump.
 *
 * Copyright (C) 2005, IBM Corp.
 *
 * Created by: Michael Ellerman
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#undef DEBUG

#include <linux/crash_dump.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <asm/code-patching.h>
#include <asm/kdump.h>
#include <asm/prom.h>
#include <asm/firmware.h>
#include <asm/uaccess.h>
#include <asm/rtas.h>

#ifdef DEBUG
#include <asm/udbg.h>
#define DBG(fmt...) udbg_printf(fmt)
#else
#define DBG(fmt...)
#endif

#ifndef CONFIG_NONSTATIC_KERNEL
void __init reserve_kdump_trampoline(void)
{
	memblock_reserve(0, KDUMP_RESERVE_LIMIT);
}

static void __init create_trampoline(unsigned long addr)
{
	unsigned int *p = (unsigned int *)addr;

	patch_instruction(p, PPC_INST_NOP);
	patch_branch(++p, addr + PHYSICAL_START, 0);
}

void __init setup_kdump_trampoline(void)
{
	unsigned long i;

	DBG(" -> setup_kdump_trampoline()\n");

	for (i = KDUMP_TRAMPOLINE_START; i < KDUMP_TRAMPOLINE_END; i += 8) {
		create_trampoline(i);
	}

#ifdef CONFIG_PPC_PSERIES
	create_trampoline(__pa(system_reset_fwnmi) - PHYSICAL_START);
	create_trampoline(__pa(machine_check_fwnmi) - PHYSICAL_START);
#endif 

	DBG(" <- setup_kdump_trampoline()\n");
}
#endif 

static int __init parse_savemaxmem(char *p)
{
	if (p)
		saved_max_pfn = (memparse(p, &p) >> PAGE_SHIFT) - 1;

	return 1;
}
__setup("savemaxmem=", parse_savemaxmem);


static size_t copy_oldmem_vaddr(void *vaddr, char *buf, size_t csize,
                               unsigned long offset, int userbuf)
{
	if (userbuf) {
		if (copy_to_user((char __user *)buf, (vaddr + offset), csize))
			return -EFAULT;
	} else
		memcpy(buf, (vaddr + offset), csize);

	return csize;
}

ssize_t copy_oldmem_page(unsigned long pfn, char *buf,
			size_t csize, unsigned long offset, int userbuf)
{
	void  *vaddr;

	if (!csize)
		return 0;

	csize = min_t(size_t, csize, PAGE_SIZE);

	if ((min_low_pfn < pfn) && (pfn < max_pfn)) {
		vaddr = __va(pfn << PAGE_SHIFT);
		csize = copy_oldmem_vaddr(vaddr, buf, csize, offset, userbuf);
	} else {
		vaddr = __ioremap(pfn << PAGE_SHIFT, PAGE_SIZE, 0);
		csize = copy_oldmem_vaddr(vaddr, buf, csize, offset, userbuf);
		iounmap(vaddr);
	}

	return csize;
}

#ifdef CONFIG_PPC_RTAS
void crash_free_reserved_phys_range(unsigned long begin, unsigned long end)
{
	unsigned long addr;
	const u32 *basep, *sizep;
	unsigned int rtas_start = 0, rtas_end = 0;

	basep = of_get_property(rtas.dev, "linux,rtas-base", NULL);
	sizep = of_get_property(rtas.dev, "rtas-size", NULL);

	if (basep && sizep) {
		rtas_start = *basep;
		rtas_end = *basep + *sizep;
	}

	for (addr = begin; addr < end; addr += PAGE_SIZE) {
		
		if (addr <= rtas_end && ((addr + PAGE_SIZE) > rtas_start))
			continue;

		ClearPageReserved(pfn_to_page(addr >> PAGE_SHIFT));
		init_page_count(pfn_to_page(addr >> PAGE_SHIFT));
		free_page((unsigned long)__va(addr));
		totalram_pages++;
	}
}
#endif
