/*
 * kexec.h for kexec
 * Created by <nschichan@corp.free.fr> on Thu Oct 12 14:59:34 2006
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */

#ifndef _MIPS_KEXEC
# define _MIPS_KEXEC

#define KEXEC_SOURCE_MEMORY_LIMIT (0x20000000)
#define KEXEC_DESTINATION_MEMORY_LIMIT (0x20000000)
 
#define KEXEC_CONTROL_MEMORY_LIMIT (0x20000000)

#define KEXEC_CONTROL_PAGE_SIZE 4096

#define KEXEC_ARCH KEXEC_ARCH_MIPS

static inline void crash_setup_regs(struct pt_regs *newregs,
				    struct pt_regs *oldregs)
{
	
}

#endif 
