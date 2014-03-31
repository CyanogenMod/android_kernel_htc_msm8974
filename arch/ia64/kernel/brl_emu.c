
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm/processor.h>

extern char ia64_set_b1, ia64_set_b2, ia64_set_b3, ia64_set_b4, ia64_set_b5;

struct illegal_op_return {
	unsigned long fkt, arg1, arg2, arg3;
};

#define unimplemented_virtual_address(va) (						\
	((va) & local_cpu_data->unimpl_va_mask) != 0 &&					\
	((va) & local_cpu_data->unimpl_va_mask) != local_cpu_data->unimpl_va_mask	\
)

#define unimplemented_physical_address(pa) (		\
	((pa) & local_cpu_data->unimpl_pa_mask) != 0	\
)


struct illegal_op_return
ia64_emulate_brl (struct pt_regs *regs, unsigned long ar_ec)
{
	unsigned long bundle[2];
	unsigned long opcode, btype, qp, offset, cpl;
	unsigned long next_ip;
	struct siginfo siginfo;
	struct illegal_op_return rv;
	long tmp_taken, unimplemented_address;

	rv.fkt = (unsigned long) -1;


	if (copy_from_user(bundle, (void *) (regs->cr_iip), sizeof(bundle)))
		return rv;

	next_ip = (unsigned long) regs->cr_iip + 16;

	
	if (ia64_psr(regs)->ri != 1) return rv;

	
	if ((bundle[0] & 0x1e) != 0x4) return rv;

	opcode = (bundle[1] >> 60);
	btype = ((bundle[1] >> 29) & 0x7);
	qp = ((bundle[1] >> 23) & 0x3f);
	offset = ((bundle[1] & 0x0800000000000000L) << 4)
		| ((bundle[1] & 0x00fffff000000000L) >> 32)
		| ((bundle[1] & 0x00000000007fffffL) << 40)
		| ((bundle[0] & 0xffff000000000000L) >> 24);

	tmp_taken = regs->pr & (1L << qp);

	switch(opcode) {

		case 0xC:
			if (btype != 0) return rv;
			rv.fkt = 0;
			if (!(tmp_taken)) {
				regs->cr_iip = next_ip;
				ia64_psr(regs)->ri = 0;
				return rv;
			}
			break;

		case 0xD:
			rv.fkt = 0;
			if (!(tmp_taken)) {
				regs->cr_iip = next_ip;
				ia64_psr(regs)->ri = 0;
				return rv;
			}

			switch(btype) {
				case 0:
					regs->b0 = next_ip;
					break;
				case 1:
					rv.fkt = (unsigned long) &ia64_set_b1;
					break;
				case 2:
					rv.fkt = (unsigned long) &ia64_set_b2;
					break;
				case 3:
					rv.fkt = (unsigned long) &ia64_set_b3;
					break;
				case 4:
					rv.fkt = (unsigned long) &ia64_set_b4;
					break;
				case 5:
					rv.fkt = (unsigned long) &ia64_set_b5;
					break;
				case 6:
					regs->b6 = next_ip;
					break;
				case 7:
					regs->b7 = next_ip;
					break;
			}
			rv.arg1 = next_ip;

			cpl = ia64_psr(regs)->cpl;
			regs->ar_pfs = ((regs->cr_ifs & 0x3fffffffff)
					| (ar_ec << 52) | (cpl << 62));

			regs->cr_ifs = ((regs->cr_ifs & 0xffffffc00000007f)
					- ((regs->cr_ifs >> 7) & 0x7f));

			break;

		default:
			return rv;

	}

	regs->cr_iip += offset;
	ia64_psr(regs)->ri = 0;

	if (ia64_psr(regs)->it == 0)
		unimplemented_address = unimplemented_physical_address(regs->cr_iip);
	else
		unimplemented_address = unimplemented_virtual_address(regs->cr_iip);

	if (unimplemented_address) {
		printk(KERN_DEBUG "Woah! Unimplemented Instruction Address Trap!\n");
		siginfo.si_signo = SIGILL;
		siginfo.si_errno = 0;
		siginfo.si_flags = 0;
		siginfo.si_isr = 0;
		siginfo.si_imm = 0;
		siginfo.si_code = ILL_BADIADDR;
		force_sig_info(SIGILL, &siginfo, current);
	} else if (ia64_psr(regs)->tb) {
		siginfo.si_signo = SIGTRAP;
		siginfo.si_errno = 0;
		siginfo.si_code = TRAP_BRANCH;
		siginfo.si_flags = 0;
		siginfo.si_isr = 0;
		siginfo.si_addr = 0;
		siginfo.si_imm = 0;
		force_sig_info(SIGTRAP, &siginfo, current);
	} else if (ia64_psr(regs)->ss) {
		siginfo.si_signo = SIGTRAP;
		siginfo.si_errno = 0;
		siginfo.si_code = TRAP_TRACE;
		siginfo.si_flags = 0;
		siginfo.si_isr = 0;
		siginfo.si_addr = 0;
		siginfo.si_imm = 0;
		force_sig_info(SIGTRAP, &siginfo, current);
	}
	return rv;
}
