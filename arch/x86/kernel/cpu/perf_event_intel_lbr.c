#include <linux/perf_event.h>
#include <linux/types.h>

#include <asm/perf_event.h>
#include <asm/msr.h>
#include <asm/insn.h>

#include "perf_event.h"

enum {
	LBR_FORMAT_32		= 0x00,
	LBR_FORMAT_LIP		= 0x01,
	LBR_FORMAT_EIP		= 0x02,
	LBR_FORMAT_EIP_FLAGS	= 0x03,
};

#define LBR_KERNEL_BIT		0 
#define LBR_USER_BIT		1 
#define LBR_JCC_BIT		2 
#define LBR_REL_CALL_BIT	3 
#define LBR_IND_CALL_BIT	4 
#define LBR_RETURN_BIT		5 
#define LBR_IND_JMP_BIT		6 
#define LBR_REL_JMP_BIT		7 
#define LBR_FAR_BIT		8 

#define LBR_KERNEL	(1 << LBR_KERNEL_BIT)
#define LBR_USER	(1 << LBR_USER_BIT)
#define LBR_JCC		(1 << LBR_JCC_BIT)
#define LBR_REL_CALL	(1 << LBR_REL_CALL_BIT)
#define LBR_IND_CALL	(1 << LBR_IND_CALL_BIT)
#define LBR_RETURN	(1 << LBR_RETURN_BIT)
#define LBR_REL_JMP	(1 << LBR_REL_JMP_BIT)
#define LBR_IND_JMP	(1 << LBR_IND_JMP_BIT)
#define LBR_FAR		(1 << LBR_FAR_BIT)

#define LBR_PLM (LBR_KERNEL | LBR_USER)

#define LBR_SEL_MASK	0x1ff	
#define LBR_NOT_SUPP	-1	
#define LBR_IGN		0	

#define LBR_ANY		 \
	(LBR_JCC	|\
	 LBR_REL_CALL	|\
	 LBR_IND_CALL	|\
	 LBR_RETURN	|\
	 LBR_REL_JMP	|\
	 LBR_IND_JMP	|\
	 LBR_FAR)

#define LBR_FROM_FLAG_MISPRED  (1ULL << 63)

#define for_each_branch_sample_type(x) \
	for ((x) = PERF_SAMPLE_BRANCH_USER; \
	     (x) < PERF_SAMPLE_BRANCH_MAX; (x) <<= 1)

enum {
	X86_BR_NONE     = 0,      

	X86_BR_USER     = 1 << 0, 
	X86_BR_KERNEL   = 1 << 1, 

	X86_BR_CALL     = 1 << 2, 
	X86_BR_RET      = 1 << 3, 
	X86_BR_SYSCALL  = 1 << 4, 
	X86_BR_SYSRET   = 1 << 5, 
	X86_BR_INT      = 1 << 6, 
	X86_BR_IRET     = 1 << 7, 
	X86_BR_JCC      = 1 << 8, 
	X86_BR_JMP      = 1 << 9, 
	X86_BR_IRQ      = 1 << 10,
	X86_BR_IND_CALL = 1 << 11,
};

#define X86_BR_PLM (X86_BR_USER | X86_BR_KERNEL)

#define X86_BR_ANY       \
	(X86_BR_CALL    |\
	 X86_BR_RET     |\
	 X86_BR_SYSCALL |\
	 X86_BR_SYSRET  |\
	 X86_BR_INT     |\
	 X86_BR_IRET    |\
	 X86_BR_JCC     |\
	 X86_BR_JMP	 |\
	 X86_BR_IRQ	 |\
	 X86_BR_IND_CALL)

#define X86_BR_ALL (X86_BR_PLM | X86_BR_ANY)

#define X86_BR_ANY_CALL		 \
	(X86_BR_CALL		|\
	 X86_BR_IND_CALL	|\
	 X86_BR_SYSCALL		|\
	 X86_BR_IRQ		|\
	 X86_BR_INT)

static void intel_pmu_lbr_filter(struct cpu_hw_events *cpuc);


static void __intel_pmu_lbr_enable(void)
{
	u64 debugctl;
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (cpuc->lbr_sel)
		wrmsrl(MSR_LBR_SELECT, cpuc->lbr_sel->config);

	rdmsrl(MSR_IA32_DEBUGCTLMSR, debugctl);
	debugctl |= (DEBUGCTLMSR_LBR | DEBUGCTLMSR_FREEZE_LBRS_ON_PMI);
	wrmsrl(MSR_IA32_DEBUGCTLMSR, debugctl);
}

static void __intel_pmu_lbr_disable(void)
{
	u64 debugctl;

	rdmsrl(MSR_IA32_DEBUGCTLMSR, debugctl);
	debugctl &= ~(DEBUGCTLMSR_LBR | DEBUGCTLMSR_FREEZE_LBRS_ON_PMI);
	wrmsrl(MSR_IA32_DEBUGCTLMSR, debugctl);
}

static void intel_pmu_lbr_reset_32(void)
{
	int i;

	for (i = 0; i < x86_pmu.lbr_nr; i++)
		wrmsrl(x86_pmu.lbr_from + i, 0);
}

static void intel_pmu_lbr_reset_64(void)
{
	int i;

	for (i = 0; i < x86_pmu.lbr_nr; i++) {
		wrmsrl(x86_pmu.lbr_from + i, 0);
		wrmsrl(x86_pmu.lbr_to   + i, 0);
	}
}

void intel_pmu_lbr_reset(void)
{
	if (!x86_pmu.lbr_nr)
		return;

	if (x86_pmu.intel_cap.lbr_format == LBR_FORMAT_32)
		intel_pmu_lbr_reset_32();
	else
		intel_pmu_lbr_reset_64();
}

void intel_pmu_lbr_enable(struct perf_event *event)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (!x86_pmu.lbr_nr)
		return;

	if (event->ctx->task && cpuc->lbr_context != event->ctx) {
		intel_pmu_lbr_reset();
		cpuc->lbr_context = event->ctx;
	}
	cpuc->br_sel = event->hw.branch_reg.reg;

	cpuc->lbr_users++;
}

void intel_pmu_lbr_disable(struct perf_event *event)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (!x86_pmu.lbr_nr)
		return;

	cpuc->lbr_users--;
	WARN_ON_ONCE(cpuc->lbr_users < 0);

	if (cpuc->enabled && !cpuc->lbr_users) {
		__intel_pmu_lbr_disable();
		
		cpuc->lbr_context = NULL;
	}
}

void intel_pmu_lbr_enable_all(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (cpuc->lbr_users)
		__intel_pmu_lbr_enable();
}

void intel_pmu_lbr_disable_all(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (cpuc->lbr_users)
		__intel_pmu_lbr_disable();
}

static inline u64 intel_pmu_lbr_tos(void)
{
	u64 tos;

	rdmsrl(x86_pmu.lbr_tos, tos);

	return tos;
}

static void intel_pmu_lbr_read_32(struct cpu_hw_events *cpuc)
{
	unsigned long mask = x86_pmu.lbr_nr - 1;
	u64 tos = intel_pmu_lbr_tos();
	int i;

	for (i = 0; i < x86_pmu.lbr_nr; i++) {
		unsigned long lbr_idx = (tos - i) & mask;
		union {
			struct {
				u32 from;
				u32 to;
			};
			u64     lbr;
		} msr_lastbranch;

		rdmsrl(x86_pmu.lbr_from + lbr_idx, msr_lastbranch.lbr);

		cpuc->lbr_entries[i].from	= msr_lastbranch.from;
		cpuc->lbr_entries[i].to		= msr_lastbranch.to;
		cpuc->lbr_entries[i].mispred	= 0;
		cpuc->lbr_entries[i].predicted	= 0;
		cpuc->lbr_entries[i].reserved	= 0;
	}
	cpuc->lbr_stack.nr = i;
}

static void intel_pmu_lbr_read_64(struct cpu_hw_events *cpuc)
{
	unsigned long mask = x86_pmu.lbr_nr - 1;
	int lbr_format = x86_pmu.intel_cap.lbr_format;
	u64 tos = intel_pmu_lbr_tos();
	int i;

	for (i = 0; i < x86_pmu.lbr_nr; i++) {
		unsigned long lbr_idx = (tos - i) & mask;
		u64 from, to, mis = 0, pred = 0;

		rdmsrl(x86_pmu.lbr_from + lbr_idx, from);
		rdmsrl(x86_pmu.lbr_to   + lbr_idx, to);

		if (lbr_format == LBR_FORMAT_EIP_FLAGS) {
			mis = !!(from & LBR_FROM_FLAG_MISPRED);
			pred = !mis;
			from = (u64)((((s64)from) << 1) >> 1);
		}

		cpuc->lbr_entries[i].from	= from;
		cpuc->lbr_entries[i].to		= to;
		cpuc->lbr_entries[i].mispred	= mis;
		cpuc->lbr_entries[i].predicted	= pred;
		cpuc->lbr_entries[i].reserved	= 0;
	}
	cpuc->lbr_stack.nr = i;
}

void intel_pmu_lbr_read(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (!cpuc->lbr_users)
		return;

	if (x86_pmu.intel_cap.lbr_format == LBR_FORMAT_32)
		intel_pmu_lbr_read_32(cpuc);
	else
		intel_pmu_lbr_read_64(cpuc);

	intel_pmu_lbr_filter(cpuc);
}

static void intel_pmu_setup_sw_lbr_filter(struct perf_event *event)
{
	u64 br_type = event->attr.branch_sample_type;
	int mask = 0;

	if (br_type & PERF_SAMPLE_BRANCH_USER)
		mask |= X86_BR_USER;

	if (br_type & PERF_SAMPLE_BRANCH_KERNEL)
		mask |= X86_BR_KERNEL;

	

	if (br_type & PERF_SAMPLE_BRANCH_ANY)
		mask |= X86_BR_ANY;

	if (br_type & PERF_SAMPLE_BRANCH_ANY_CALL)
		mask |= X86_BR_ANY_CALL;

	if (br_type & PERF_SAMPLE_BRANCH_ANY_RETURN)
		mask |= X86_BR_RET | X86_BR_IRET | X86_BR_SYSRET;

	if (br_type & PERF_SAMPLE_BRANCH_IND_CALL)
		mask |= X86_BR_IND_CALL;
	event->hw.branch_reg.reg = mask;
}

static int intel_pmu_setup_hw_lbr_filter(struct perf_event *event)
{
	struct hw_perf_event_extra *reg;
	u64 br_type = event->attr.branch_sample_type;
	u64 mask = 0, m;
	u64 v;

	for_each_branch_sample_type(m) {
		if (!(br_type & m))
			continue;

		v = x86_pmu.lbr_sel_map[m];
		if (v == LBR_NOT_SUPP)
			return -EOPNOTSUPP;

		if (v != LBR_IGN)
			mask |= v;
	}
	reg = &event->hw.branch_reg;
	reg->idx = EXTRA_REG_LBR;

	
	reg->config = ~mask & x86_pmu.lbr_sel_mask;

	return 0;
}

int intel_pmu_setup_lbr_filter(struct perf_event *event)
{
	int ret = 0;

	if (!x86_pmu.lbr_nr)
		return -EOPNOTSUPP;

	intel_pmu_setup_sw_lbr_filter(event);

	if (x86_pmu.lbr_sel_map)
		ret = intel_pmu_setup_hw_lbr_filter(event);

	return ret;
}

static int branch_type(unsigned long from, unsigned long to)
{
	struct insn insn;
	void *addr;
	int bytes, size = MAX_INSN_SIZE;
	int ret = X86_BR_NONE;
	int ext, to_plm, from_plm;
	u8 buf[MAX_INSN_SIZE];
	int is64 = 0;

	to_plm = kernel_ip(to) ? X86_BR_KERNEL : X86_BR_USER;
	from_plm = kernel_ip(from) ? X86_BR_KERNEL : X86_BR_USER;

	if (from == 0 || to == 0)
		return X86_BR_NONE;

	if (from_plm == X86_BR_USER) {
		if (!current->mm)
			return X86_BR_NONE;

		
		bytes = copy_from_user_nmi(buf, (void __user *)from, size);
		if (bytes != size)
			return X86_BR_NONE;

		addr = buf;
	} else
		addr = (void *)from;

#ifdef CONFIG_X86_64
	is64 = kernel_ip((unsigned long)addr) || !test_thread_flag(TIF_IA32);
#endif
	insn_init(&insn, addr, is64);
	insn_get_opcode(&insn);

	switch (insn.opcode.bytes[0]) {
	case 0xf:
		switch (insn.opcode.bytes[1]) {
		case 0x05: 
		case 0x34: 
			ret = X86_BR_SYSCALL;
			break;
		case 0x07: 
		case 0x35: 
			ret = X86_BR_SYSRET;
			break;
		case 0x80 ... 0x8f: 
			ret = X86_BR_JCC;
			break;
		default:
			ret = X86_BR_NONE;
		}
		break;
	case 0x70 ... 0x7f: 
		ret = X86_BR_JCC;
		break;
	case 0xc2: 
	case 0xc3: 
	case 0xca: 
	case 0xcb: 
		ret = X86_BR_RET;
		break;
	case 0xcf: 
		ret = X86_BR_IRET;
		break;
	case 0xcc ... 0xce: 
		ret = X86_BR_INT;
		break;
	case 0xe8: 
	case 0x9a: 
		ret = X86_BR_CALL;
		break;
	case 0xe0 ... 0xe3: 
		ret = X86_BR_JCC;
		break;
	case 0xe9 ... 0xeb: 
		ret = X86_BR_JMP;
		break;
	case 0xff: 
		insn_get_modrm(&insn);
		ext = (insn.modrm.bytes[0] >> 3) & 0x7;
		switch (ext) {
		case 2: 
		case 3: 
			ret = X86_BR_IND_CALL;
			break;
		case 4:
		case 5:
			ret = X86_BR_JMP;
			break;
		}
		break;
	default:
		ret = X86_BR_NONE;
	}
	if (from_plm == X86_BR_USER && to_plm == X86_BR_KERNEL
	    && ret != X86_BR_SYSCALL && ret != X86_BR_INT)
		ret = X86_BR_IRQ;

	if (ret != X86_BR_NONE)
		ret |= to_plm;

	return ret;
}

static void
intel_pmu_lbr_filter(struct cpu_hw_events *cpuc)
{
	u64 from, to;
	int br_sel = cpuc->br_sel;
	int i, j, type;
	bool compress = false;

	
	if ((br_sel & X86_BR_ALL) == X86_BR_ALL)
		return;

	for (i = 0; i < cpuc->lbr_stack.nr; i++) {

		from = cpuc->lbr_entries[i].from;
		to = cpuc->lbr_entries[i].to;

		type = branch_type(from, to);

		
		if (type == X86_BR_NONE || (br_sel & type) != type) {
			cpuc->lbr_entries[i].from = 0;
			compress = true;
		}
	}

	if (!compress)
		return;

	
	for (i = 0; i < cpuc->lbr_stack.nr; ) {
		if (!cpuc->lbr_entries[i].from) {
			j = i;
			while (++j < cpuc->lbr_stack.nr)
				cpuc->lbr_entries[j-1] = cpuc->lbr_entries[j];
			cpuc->lbr_stack.nr--;
			if (!cpuc->lbr_entries[i].from)
				continue;
		}
		i++;
	}
}

static const int nhm_lbr_sel_map[PERF_SAMPLE_BRANCH_MAX] = {
	[PERF_SAMPLE_BRANCH_ANY]	= LBR_ANY,
	[PERF_SAMPLE_BRANCH_USER]	= LBR_USER,
	[PERF_SAMPLE_BRANCH_KERNEL]	= LBR_KERNEL,
	[PERF_SAMPLE_BRANCH_HV]		= LBR_IGN,
	[PERF_SAMPLE_BRANCH_ANY_RETURN]	= LBR_RETURN | LBR_REL_JMP
					| LBR_IND_JMP | LBR_FAR,
	[PERF_SAMPLE_BRANCH_ANY_CALL] =
	 LBR_REL_CALL | LBR_IND_CALL | LBR_REL_JMP | LBR_IND_JMP | LBR_FAR,
	[PERF_SAMPLE_BRANCH_IND_CALL] = LBR_IND_CALL | LBR_IND_JMP,
};

static const int snb_lbr_sel_map[PERF_SAMPLE_BRANCH_MAX] = {
	[PERF_SAMPLE_BRANCH_ANY]	= LBR_ANY,
	[PERF_SAMPLE_BRANCH_USER]	= LBR_USER,
	[PERF_SAMPLE_BRANCH_KERNEL]	= LBR_KERNEL,
	[PERF_SAMPLE_BRANCH_HV]		= LBR_IGN,
	[PERF_SAMPLE_BRANCH_ANY_RETURN]	= LBR_RETURN | LBR_FAR,
	[PERF_SAMPLE_BRANCH_ANY_CALL]	= LBR_REL_CALL | LBR_IND_CALL
					| LBR_FAR,
	[PERF_SAMPLE_BRANCH_IND_CALL]	= LBR_IND_CALL,
};

void intel_pmu_lbr_init_core(void)
{
	x86_pmu.lbr_nr     = 4;
	x86_pmu.lbr_tos    = MSR_LBR_TOS;
	x86_pmu.lbr_from   = MSR_LBR_CORE_FROM;
	x86_pmu.lbr_to     = MSR_LBR_CORE_TO;

	pr_cont("4-deep LBR, ");
}

void intel_pmu_lbr_init_nhm(void)
{
	x86_pmu.lbr_nr     = 16;
	x86_pmu.lbr_tos    = MSR_LBR_TOS;
	x86_pmu.lbr_from   = MSR_LBR_NHM_FROM;
	x86_pmu.lbr_to     = MSR_LBR_NHM_TO;

	x86_pmu.lbr_sel_mask = LBR_SEL_MASK;
	x86_pmu.lbr_sel_map  = nhm_lbr_sel_map;

	pr_cont("16-deep LBR, ");
}

void intel_pmu_lbr_init_snb(void)
{
	x86_pmu.lbr_nr	 = 16;
	x86_pmu.lbr_tos	 = MSR_LBR_TOS;
	x86_pmu.lbr_from = MSR_LBR_NHM_FROM;
	x86_pmu.lbr_to   = MSR_LBR_NHM_TO;

	x86_pmu.lbr_sel_mask = LBR_SEL_MASK;
	x86_pmu.lbr_sel_map  = snb_lbr_sel_map;

	pr_cont("16-deep LBR, ");
}

void intel_pmu_lbr_init_atom(void)
{
	if (boot_cpu_data.x86_mask < 10) {
		pr_cont("LBR disabled due to erratum");
		return;
	}

	x86_pmu.lbr_nr	   = 8;
	x86_pmu.lbr_tos    = MSR_LBR_TOS;
	x86_pmu.lbr_from   = MSR_LBR_CORE_FROM;
	x86_pmu.lbr_to     = MSR_LBR_CORE_TO;

	pr_cont("8-deep LBR, ");
}
