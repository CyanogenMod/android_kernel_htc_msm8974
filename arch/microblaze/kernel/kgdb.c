/*
 * Microblaze KGDB support
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/kgdb.h>
#include <linux/kdebug.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <asm/asm-offsets.h>
#include <asm/pvr.h>

#define GDB_REG		0
#define GDB_PC		32
#define GDB_MSR		33
#define GDB_EAR		34
#define GDB_ESR		35
#define GDB_FSR		36
#define GDB_BTR		37
#define GDB_PVR		38
#define GDB_REDR	50
#define GDB_RPID	51
#define GDB_RZPR	52
#define GDB_RTLBX	53
#define GDB_RTLBSX	54 
#define GDB_RTLBLO	55
#define GDB_RTLBHI	56

struct pvr_s pvr;

void pt_regs_to_gdb_regs(unsigned long *gdb_regs, struct pt_regs *regs)
{
	int i;
	unsigned long *pt_regb = (unsigned long *)regs;
	int temp;
	
	for (i = 0; i < (sizeof(struct pt_regs) / 4) - 1; i++)
		gdb_regs[i] = pt_regb[i];

	
	__asm__ __volatile__ ("mfs %0, rbtr;" : "=r"(temp) : );
	gdb_regs[GDB_BTR] = temp;

	
	for (i = 0; i < sizeof(struct pvr_s)/4; i++)
		gdb_regs[GDB_PVR + i] = pvr.pvr[i];

	
	__asm__ __volatile__ ("mfs %0, redr;" : "=r"(temp) : );
	gdb_regs[GDB_REDR] = temp;
	__asm__ __volatile__ ("mfs %0, rpid;" : "=r"(temp) : );
	gdb_regs[GDB_RPID] = temp;
	__asm__ __volatile__ ("mfs %0, rzpr;" : "=r"(temp) : );
	gdb_regs[GDB_RZPR] = temp;
	__asm__ __volatile__ ("mfs %0, rtlbx;" : "=r"(temp) : );
	gdb_regs[GDB_RTLBX] = temp;
	__asm__ __volatile__ ("mfs %0, rtlblo;" : "=r"(temp) : );
	gdb_regs[GDB_RTLBLO] = temp;
	__asm__ __volatile__ ("mfs %0, rtlbhi;" : "=r"(temp) : );
	gdb_regs[GDB_RTLBHI] = temp;
}

void gdb_regs_to_pt_regs(unsigned long *gdb_regs, struct pt_regs *regs)
{
	int i;
	unsigned long *pt_regb = (unsigned long *)regs;

	for (i = 1; i < (sizeof(struct pt_regs) / 4) - 1; i++)
		pt_regb[i] = gdb_regs[i];
}

void microblaze_kgdb_break(struct pt_regs *regs)
{
	if (kgdb_handle_exception(1, SIGTRAP, 0, regs) != 0)
		return;

	if (*(u32 *) (regs->pc) == *(u32 *) (&arch_kgdb_ops.gdb_bpt_instr))
		regs->pc += BREAK_INSTR_SIZE;
}

void sleeping_thread_to_gdb_regs(unsigned long *gdb_regs, struct task_struct *p)
{
	int i;
	unsigned long *pt_regb = (unsigned long *)(p->thread.regs);

	
	for (i = 0; i < (sizeof(struct pt_regs) / 4) - 1; i++)
		gdb_regs[i] = pt_regb[i];

	
	for (i = 0; i < sizeof(struct pvr_s)/4; i++)
		gdb_regs[GDB_PVR + i] = pvr.pvr[i];
}

void kgdb_arch_set_pc(struct pt_regs *regs, unsigned long ip)
{
	regs->pc = ip;
}

int kgdb_arch_handle_exception(int vector, int signo, int err_code,
			       char *remcom_in_buffer, char *remcom_out_buffer,
			       struct pt_regs *regs)
{
	char *ptr;
	unsigned long address;

	switch (remcom_in_buffer[0]) {
	case 'c':
		
		ptr = &remcom_in_buffer[1];
		if (kgdb_hex2long(&ptr, &address))
			regs->pc = address;

		return 0;
	}
	return -1; 
}

int kgdb_arch_init(void)
{
	get_pvr(&pvr); 
	return 0;
}

void kgdb_arch_exit(void)
{
	
}

struct kgdb_arch arch_kgdb_ops = {
#ifdef __MICROBLAZEEL__
	.gdb_bpt_instr = {0x18, 0x00, 0x0c, 0xba}, 
#else
	.gdb_bpt_instr = {0xba, 0x0c, 0x00, 0x18}, 
#endif
};
