/*
 * PPC EDAC common defs
 *
 * Author: Dave Jiang <djiang@mvista.com>
 *
 * 2007 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#ifndef ASM_EDAC_H
#define ASM_EDAC_H
static __inline__ void atomic_scrub(void *va, u32 size)
{
	unsigned int *virt_addr = va;
	unsigned int temp;
	unsigned int i;

	for (i = 0; i < size / sizeof(*virt_addr); i++, virt_addr++) {
		__asm__ __volatile__ ("\n\
				1:	lwarx	%0,0,%1\n\
					stwcx.	%0,0,%1\n\
					bne-	1b\n\
					isync"
					: "=&r"(temp)
					: "r"(virt_addr)
					: "cr0", "memory");
	}
}

#endif
